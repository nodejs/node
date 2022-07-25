// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-compiler.h"

#include <algorithm>
#include <queue>

#include "src/api/api-inl.h"
#include "src/asmjs/asm-js.h"
#include "src/base/enum-set.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "src/base/utils/random-number-generator.h"
#include "src/compiler/wasm-compiler.h"
#include "src/handles/global-handles-inl.h"
#include "src/heap/heap-inl.h"  // For CodePageCollectionMemoryModificationScope.
#include "src/logging/counters-scopes.h"
#include "src/logging/metrics.h"
#include "src/objects/property-descriptor.h"
#include "src/tasks/task-utils.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/identity-map.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-serialization.h"

#define TRACE_COMPILE(...)                             \
  do {                                                 \
    if (FLAG_trace_wasm_compiler) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_STREAMING(...)                            \
  do {                                                  \
    if (FLAG_trace_wasm_streaming) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_LAZY(...)                                        \
  do {                                                         \
    if (FLAG_trace_wasm_lazy_compilation) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

namespace {

enum class CompileStrategy : uint8_t {
  // Compiles functions on first use. In this case, execution will block until
  // the function's baseline is reached and top tier compilation starts in
  // background (if applicable).
  // Lazy compilation can help to reduce startup time and code size at the risk
  // of blocking execution.
  kLazy,
  // Compiles baseline ahead of execution and starts top tier compilation in
  // background (if applicable).
  kEager,
  // Triggers baseline compilation on first use (just like {kLazy}) with the
  // difference that top tier compilation is started eagerly.
  // This strategy can help to reduce startup time at the risk of blocking
  // execution, but only in its early phase (until top tier compilation
  // finishes).
  kLazyBaselineEagerTopTier,
  // Marker for default strategy.
  kDefault = kEager,
};

class CompilationStateImpl;
class CompilationUnitBuilder;

class V8_NODISCARD BackgroundCompileScope {
 public:
  explicit BackgroundCompileScope(std::weak_ptr<NativeModule> native_module)
      : native_module_(native_module.lock()) {}

  NativeModule* native_module() const {
    DCHECK(native_module_);
    return native_module_.get();
  }
  inline CompilationStateImpl* compilation_state() const;

  bool cancelled() const;

 private:
  // Keep the native module alive while in this scope.
  std::shared_ptr<NativeModule> native_module_;
};

enum CompileBaselineOnly : bool {
  kBaselineOnly = true,
  kBaselineOrTopTier = false
};

// A set of work-stealing queues (vectors of units). Each background compile
// task owns one of the queues and steals from all others once its own queue
// runs empty.
class CompilationUnitQueues {
 public:
  // Public API for QueueImpl.
  struct Queue {
    bool ShouldPublish(int num_processed_units) const;
  };

  explicit CompilationUnitQueues(int num_declared_functions)
      : num_declared_functions_(num_declared_functions) {
    // Add one first queue, to add units to.
    queues_.emplace_back(std::make_unique<QueueImpl>(0));

    for (auto& atomic_counter : num_units_) {
      std::atomic_init(&atomic_counter, size_t{0});
    }

    top_tier_compiled_ =
        std::make_unique<std::atomic<bool>[]>(num_declared_functions);

    for (int i = 0; i < num_declared_functions; i++) {
      std::atomic_init(&top_tier_compiled_.get()[i], false);
    }
  }

  Queue* GetQueueForTask(int task_id) {
    int required_queues = task_id + 1;
    {
      base::SharedMutexGuard<base::kShared> queues_guard(&queues_mutex_);
      if (V8_LIKELY(static_cast<int>(queues_.size()) >= required_queues)) {
        return queues_[task_id].get();
      }
    }

    // Otherwise increase the number of queues.
    base::SharedMutexGuard<base::kExclusive> queues_guard(&queues_mutex_);
    int num_queues = static_cast<int>(queues_.size());
    while (num_queues < required_queues) {
      int steal_from = num_queues + 1;
      queues_.emplace_back(std::make_unique<QueueImpl>(steal_from));
      ++num_queues;
    }

    // Update the {publish_limit}s of all queues.

    // We want background threads to publish regularly (to avoid contention when
    // they are all publishing at the end). On the other side, each publishing
    // has some overhead (part of it for synchronizing between threads), so it
    // should not happen *too* often. Thus aim for 4-8 publishes per thread, but
    // distribute it such that publishing is likely to happen at different
    // times.
    int units_per_thread = num_declared_functions_ / num_queues;
    int min = std::max(10, units_per_thread / 8);
    int queue_id = 0;
    for (auto& queue : queues_) {
      // Set a limit between {min} and {2*min}, but not smaller than {10}.
      int limit = min + (min * queue_id / num_queues);
      queue->publish_limit.store(limit, std::memory_order_relaxed);
      ++queue_id;
    }

    return queues_[task_id].get();
  }

  base::Optional<WasmCompilationUnit> GetNextUnit(
      Queue* queue, CompileBaselineOnly baseline_only) {
    // As long as any lower-tier units are outstanding we need to steal them
    // before executing own higher-tier units.
    int max_tier = baseline_only ? kBaseline : kTopTier;
    for (int tier = GetLowestTierWithUnits(); tier <= max_tier; ++tier) {
      if (auto unit = GetNextUnitOfTier(queue, tier)) {
        size_t old_units_count =
            num_units_[tier].fetch_sub(1, std::memory_order_relaxed);
        DCHECK_LE(1, old_units_count);
        USE(old_units_count);
        return unit;
      }
    }
    return {};
  }

  void AddUnits(base::Vector<WasmCompilationUnit> baseline_units,
                base::Vector<WasmCompilationUnit> top_tier_units,
                const WasmModule* module) {
    DCHECK_LT(0, baseline_units.size() + top_tier_units.size());
    // Add to the individual queues in a round-robin fashion. No special care is
    // taken to balance them; they will be balanced by work stealing.
    QueueImpl* queue;
    {
      int queue_to_add = next_queue_to_add.load(std::memory_order_relaxed);
      base::SharedMutexGuard<base::kShared> queues_guard(&queues_mutex_);
      while (!next_queue_to_add.compare_exchange_weak(
          queue_to_add, next_task_id(queue_to_add, queues_.size()),
          std::memory_order_relaxed)) {
        // Retry with updated {queue_to_add}.
      }
      queue = queues_[queue_to_add].get();
    }

    base::MutexGuard guard(&queue->mutex);
    base::Optional<base::MutexGuard> big_units_guard;
    for (auto pair : {std::make_pair(int{kBaseline}, baseline_units),
                      std::make_pair(int{kTopTier}, top_tier_units)}) {
      int tier = pair.first;
      base::Vector<WasmCompilationUnit> units = pair.second;
      if (units.empty()) continue;
      num_units_[tier].fetch_add(units.size(), std::memory_order_relaxed);
      for (WasmCompilationUnit unit : units) {
        size_t func_size = module->functions[unit.func_index()].code.length();
        if (func_size <= kBigUnitsLimit) {
          queue->units[tier].push_back(unit);
        } else {
          if (!big_units_guard) {
            big_units_guard.emplace(&big_units_queue_.mutex);
          }
          big_units_queue_.has_units[tier].store(true,
                                                 std::memory_order_relaxed);
          big_units_queue_.units[tier].emplace(func_size, unit);
        }
      }
    }
  }

  void AddTopTierPriorityUnit(WasmCompilationUnit unit, size_t priority) {
    base::SharedMutexGuard<base::kShared> queues_guard(&queues_mutex_);
    // Add to the individual queues in a round-robin fashion. No special care is
    // taken to balance them; they will be balanced by work stealing. We use
    // the same counter for this reason.
    int queue_to_add = next_queue_to_add.load(std::memory_order_relaxed);
    while (!next_queue_to_add.compare_exchange_weak(
        queue_to_add, next_task_id(queue_to_add, queues_.size()),
        std::memory_order_relaxed)) {
      // Retry with updated {queue_to_add}.
    }

    {
      auto* queue = queues_[queue_to_add].get();
      base::MutexGuard guard(&queue->mutex);
      queue->top_tier_priority_units.emplace(priority, unit);
    }
    num_priority_units_.fetch_add(1, std::memory_order_relaxed);
    num_units_[kTopTier].fetch_add(1, std::memory_order_relaxed);
  }

  // Get the current total number of units in all queues. This is only a
  // momentary snapshot, it's not guaranteed that {GetNextUnit} returns a unit
  // if this method returns non-zero.
  size_t GetTotalSize() const {
    size_t total = 0;
    for (auto& atomic_counter : num_units_) {
      total += atomic_counter.load(std::memory_order_relaxed);
    }
    return total;
  }

 private:
  // Store tier in int so we can easily loop over it:
  static constexpr int kBaseline = 0;
  static constexpr int kTopTier = 1;
  static constexpr int kNumTiers = kTopTier + 1;

  // Functions bigger than {kBigUnitsLimit} will be compiled first, in ascending
  // order of their function body size.
  static constexpr size_t kBigUnitsLimit = 4096;

  struct BigUnit {
    BigUnit(size_t func_size, WasmCompilationUnit unit)
        : func_size{func_size}, unit(unit) {}

    size_t func_size;
    WasmCompilationUnit unit;

    bool operator<(const BigUnit& other) const {
      return func_size < other.func_size;
    }
  };

  struct TopTierPriorityUnit {
    TopTierPriorityUnit(int priority, WasmCompilationUnit unit)
        : priority(priority), unit(unit) {}

    size_t priority;
    WasmCompilationUnit unit;

    bool operator<(const TopTierPriorityUnit& other) const {
      return priority < other.priority;
    }
  };

  struct BigUnitsQueue {
    BigUnitsQueue() {
      for (auto& atomic : has_units) std::atomic_init(&atomic, false);
    }

    base::Mutex mutex;

    // Can be read concurrently to check whether any elements are in the queue.
    std::atomic<bool> has_units[kNumTiers];

    // Protected by {mutex}:
    std::priority_queue<BigUnit> units[kNumTiers];
  };

  struct QueueImpl : public Queue {
    explicit QueueImpl(int next_steal_task_id)
        : next_steal_task_id(next_steal_task_id) {}

    // Number of units after which the task processing this queue should publish
    // compilation results. Updated (reduced, using relaxed ordering) when new
    // queues are allocated. If there is only one thread running, we can delay
    // publishing arbitrarily.
    std::atomic<int> publish_limit{kMaxInt};

    base::Mutex mutex;

    // All fields below are protected by {mutex}.
    std::vector<WasmCompilationUnit> units[kNumTiers];
    std::priority_queue<TopTierPriorityUnit> top_tier_priority_units;
    int next_steal_task_id;
  };

  int next_task_id(int task_id, size_t num_queues) const {
    int next = task_id + 1;
    return next == static_cast<int>(num_queues) ? 0 : next;
  }

  int GetLowestTierWithUnits() const {
    for (int tier = 0; tier < kNumTiers; ++tier) {
      if (num_units_[tier].load(std::memory_order_relaxed) > 0) return tier;
    }
    return kNumTiers;
  }

  base::Optional<WasmCompilationUnit> GetNextUnitOfTier(Queue* public_queue,
                                                        int tier) {
    QueueImpl* queue = static_cast<QueueImpl*>(public_queue);

    // First check whether there is a priority unit. Execute that first.
    if (tier == kTopTier) {
      if (auto unit = GetTopTierPriorityUnit(queue)) {
        return unit;
      }
    }

    // Then check whether there is a big unit of that tier.
    if (auto unit = GetBigUnitOfTier(tier)) return unit;

    // Finally check whether our own queue has a unit of the wanted tier. If
    // so, return it, otherwise get the task id to steal from.
    int steal_task_id;
    {
      base::MutexGuard mutex_guard(&queue->mutex);
      if (!queue->units[tier].empty()) {
        auto unit = queue->units[tier].back();
        queue->units[tier].pop_back();
        return unit;
      }
      steal_task_id = queue->next_steal_task_id;
    }

    // Try to steal from all other queues. If this succeeds, return one of the
    // stolen units.
    {
      base::SharedMutexGuard<base::kShared> guard(&queues_mutex_);
      for (size_t steal_trials = 0; steal_trials < queues_.size();
           ++steal_trials, ++steal_task_id) {
        if (steal_task_id >= static_cast<int>(queues_.size())) {
          steal_task_id = 0;
        }
        if (auto unit = StealUnitsAndGetFirst(queue, steal_task_id, tier)) {
          return unit;
        }
      }
    }

    // If we reach here, we didn't find any unit of the requested tier.
    return {};
  }

  base::Optional<WasmCompilationUnit> GetBigUnitOfTier(int tier) {
    // Fast path without locking.
    if (!big_units_queue_.has_units[tier].load(std::memory_order_relaxed)) {
      return {};
    }
    base::MutexGuard guard(&big_units_queue_.mutex);
    if (big_units_queue_.units[tier].empty()) return {};
    WasmCompilationUnit unit = big_units_queue_.units[tier].top().unit;
    big_units_queue_.units[tier].pop();
    if (big_units_queue_.units[tier].empty()) {
      big_units_queue_.has_units[tier].store(false, std::memory_order_relaxed);
    }
    return unit;
  }

  base::Optional<WasmCompilationUnit> GetTopTierPriorityUnit(QueueImpl* queue) {
    // Fast path without locking.
    if (num_priority_units_.load(std::memory_order_relaxed) == 0) {
      return {};
    }

    int steal_task_id;
    {
      base::MutexGuard mutex_guard(&queue->mutex);
      while (!queue->top_tier_priority_units.empty()) {
        auto unit = queue->top_tier_priority_units.top().unit;
        queue->top_tier_priority_units.pop();
        num_priority_units_.fetch_sub(1, std::memory_order_relaxed);

        if (!top_tier_compiled_[unit.func_index()].exchange(
                true, std::memory_order_relaxed)) {
          return unit;
        }
        num_units_[kTopTier].fetch_sub(1, std::memory_order_relaxed);
      }
      steal_task_id = queue->next_steal_task_id;
    }

    // Try to steal from all other queues. If this succeeds, return one of the
    // stolen units.
    {
      base::SharedMutexGuard<base::kShared> guard(&queues_mutex_);
      for (size_t steal_trials = 0; steal_trials < queues_.size();
           ++steal_trials, ++steal_task_id) {
        if (steal_task_id >= static_cast<int>(queues_.size())) {
          steal_task_id = 0;
        }
        if (auto unit = StealTopTierPriorityUnit(queue, steal_task_id)) {
          return unit;
        }
      }
    }

    return {};
  }

  // Steal units of {wanted_tier} from {steal_from_task_id} to {queue}. Return
  // first stolen unit (rest put in queue of {task_id}), or {nullopt} if
  // {steal_from_task_id} had no units of {wanted_tier}.
  // Hold a shared lock on {queues_mutex_} when calling this method.
  base::Optional<WasmCompilationUnit> StealUnitsAndGetFirst(
      QueueImpl* queue, int steal_from_task_id, int wanted_tier) {
    auto* steal_queue = queues_[steal_from_task_id].get();
    // Cannot steal from own queue.
    if (steal_queue == queue) return {};
    std::vector<WasmCompilationUnit> stolen;
    base::Optional<WasmCompilationUnit> returned_unit;
    {
      base::MutexGuard guard(&steal_queue->mutex);
      auto* steal_from_vector = &steal_queue->units[wanted_tier];
      if (steal_from_vector->empty()) return {};
      size_t remaining = steal_from_vector->size() / 2;
      auto steal_begin = steal_from_vector->begin() + remaining;
      returned_unit = *steal_begin;
      stolen.assign(steal_begin + 1, steal_from_vector->end());
      steal_from_vector->erase(steal_begin, steal_from_vector->end());
    }
    base::MutexGuard guard(&queue->mutex);
    auto* target_queue = &queue->units[wanted_tier];
    target_queue->insert(target_queue->end(), stolen.begin(), stolen.end());
    queue->next_steal_task_id = steal_from_task_id + 1;
    return returned_unit;
  }

  // Steal one priority unit from {steal_from_task_id} to {task_id}. Return
  // stolen unit, or {nullopt} if {steal_from_task_id} had no priority units.
  // Hold a shared lock on {queues_mutex_} when calling this method.
  base::Optional<WasmCompilationUnit> StealTopTierPriorityUnit(
      QueueImpl* queue, int steal_from_task_id) {
    auto* steal_queue = queues_[steal_from_task_id].get();
    // Cannot steal from own queue.
    if (steal_queue == queue) return {};
    base::Optional<WasmCompilationUnit> returned_unit;
    {
      base::MutexGuard guard(&steal_queue->mutex);
      while (true) {
        if (steal_queue->top_tier_priority_units.empty()) return {};

        auto unit = steal_queue->top_tier_priority_units.top().unit;
        steal_queue->top_tier_priority_units.pop();
        num_priority_units_.fetch_sub(1, std::memory_order_relaxed);

        if (!top_tier_compiled_[unit.func_index()].exchange(
                true, std::memory_order_relaxed)) {
          returned_unit = unit;
          break;
        }
        num_units_[kTopTier].fetch_sub(1, std::memory_order_relaxed);
      }
    }
    base::MutexGuard guard(&queue->mutex);
    queue->next_steal_task_id = steal_from_task_id + 1;
    return returned_unit;
  }

  // {queues_mutex_} protectes {queues_};
  base::SharedMutex queues_mutex_;
  std::vector<std::unique_ptr<QueueImpl>> queues_;

  const int num_declared_functions_;

  BigUnitsQueue big_units_queue_;

  std::atomic<size_t> num_units_[kNumTiers];
  std::atomic<size_t> num_priority_units_{0};
  std::unique_ptr<std::atomic<bool>[]> top_tier_compiled_;
  std::atomic<int> next_queue_to_add{0};
};

bool CompilationUnitQueues::Queue::ShouldPublish(
    int num_processed_units) const {
  auto* queue = static_cast<const QueueImpl*>(this);
  return num_processed_units >=
         queue->publish_limit.load(std::memory_order_relaxed);
}

// The {CompilationStateImpl} keeps track of the compilation state of the
// owning NativeModule, i.e. which functions are left to be compiled.
// It contains a task manager to allow parallel and asynchronous background
// compilation of functions.
// Its public interface {CompilationState} lives in compilation-environment.h.
class CompilationStateImpl {
 public:
  CompilationStateImpl(const std::shared_ptr<NativeModule>& native_module,
                       std::shared_ptr<Counters> async_counters,
                       DynamicTiering dynamic_tiering);
  ~CompilationStateImpl() {
    if (compile_job_->IsValid()) compile_job_->CancelAndDetach();
  }

  // Call right after the constructor, after the {compilation_state_} field in
  // the {NativeModule} has been initialized.
  void InitCompileJob();

  // {kCancelUnconditionally}: Cancel all compilation.
  // {kCancelInitialCompilation}: Cancel all compilation if initial (baseline)
  // compilation is not finished yet.
  enum CancellationPolicy { kCancelUnconditionally, kCancelInitialCompilation };
  void CancelCompilation(CancellationPolicy);

  bool cancelled() const;

  // Initialize compilation progress. Set compilation tiers to expect for
  // baseline and top tier compilation. Must be set before
  // {CommitCompilationUnits} is invoked which triggers background compilation.
  void InitializeCompilationProgress(bool lazy_module, int num_import_wrappers,
                                     int num_export_wrappers);

  // Initialize the compilation progress after deserialization. This is needed
  // for recompilation (e.g. for tier down) to work later.
  void InitializeCompilationProgressAfterDeserialization(
      base::Vector<const int> lazy_functions,
      base::Vector<const int> liftoff_functions);

  // Initializes compilation units based on the information encoded in the
  // {compilation_progress_}.
  void InitializeCompilationUnits(
      std::unique_ptr<CompilationUnitBuilder> builder);

  // Adds compilation units for another function to the
  // {CompilationUnitBuilder}. This function is the streaming compilation
  // equivalent to {InitializeCompilationUnits}.
  void AddCompilationUnit(CompilationUnitBuilder* builder, int func_index);

  // Initialize recompilation of the whole module: Setup compilation progress
  // for recompilation and add the respective compilation units. The callback is
  // called immediately if no recompilation is needed, or called later
  // otherwise.
  void InitializeRecompilation(TieringState new_tiering_state,
                               std::unique_ptr<CompilationEventCallback>
                                   recompilation_finished_callback);

  // Add the callback to be called on compilation events. Needs to be
  // set before {CommitCompilationUnits} is run to ensure that it receives all
  // events. The callback object must support being deleted from any thread.
  void AddCallback(std::unique_ptr<CompilationEventCallback> callback);

  // Inserts new functions to compile and kicks off compilation.
  void CommitCompilationUnits(
      base::Vector<WasmCompilationUnit> baseline_units,
      base::Vector<WasmCompilationUnit> top_tier_units,
      base::Vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
          js_to_wasm_wrapper_units);
  void CommitTopTierCompilationUnit(WasmCompilationUnit);
  void AddTopTierPriorityCompilationUnit(WasmCompilationUnit, size_t);

  CompilationUnitQueues::Queue* GetQueueForCompileTask(int task_id);

  base::Optional<WasmCompilationUnit> GetNextCompilationUnit(
      CompilationUnitQueues::Queue*, CompileBaselineOnly);

  std::shared_ptr<JSToWasmWrapperCompilationUnit>
  GetNextJSToWasmWrapperCompilationUnit();
  void FinalizeJSToWasmWrappers(Isolate* isolate, const WasmModule* module,
                                Handle<FixedArray>* export_wrappers_out);

  void OnFinishedUnits(base::Vector<WasmCode*>);
  void OnFinishedJSToWasmWrapperUnits(int num);

  void OnCompilationStopped(WasmFeatures detected);
  void PublishDetectedFeatures(Isolate*);
  void SchedulePublishCompilationResults(
      std::vector<std::unique_ptr<WasmCode>> unpublished_code);

  size_t NumOutstandingCompilations() const;

  void SetError();

  void WaitForCompilationEvent(CompilationEvent event);

  void SetHighPriority() {
    // TODO(wasm): Keep a lower priority for TurboFan-only jobs.
    compile_job_->UpdatePriority(TaskPriority::kUserBlocking);
  }

  bool failed() const {
    return compile_failed_.load(std::memory_order_relaxed);
  }

  bool baseline_compilation_finished() const {
    base::MutexGuard guard(&callbacks_mutex_);
    return outstanding_baseline_units_ == 0 &&
           outstanding_export_wrappers_ == 0;
  }

  bool top_tier_compilation_finished() const {
    base::MutexGuard guard(&callbacks_mutex_);
    return outstanding_top_tier_functions_ == 0;
  }

  bool recompilation_finished() const {
    base::MutexGuard guard(&callbacks_mutex_);
    return outstanding_recompilation_functions_ == 0;
  }

  DynamicTiering dynamic_tiering() const { return dynamic_tiering_; }

  Counters* counters() const { return async_counters_.get(); }

  void SetWireBytesStorage(
      std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
    base::MutexGuard guard(&mutex_);
    wire_bytes_storage_ = std::move(wire_bytes_storage);
  }

  std::shared_ptr<WireBytesStorage> GetWireBytesStorage() const {
    base::MutexGuard guard(&mutex_);
    DCHECK_NOT_NULL(wire_bytes_storage_);
    return wire_bytes_storage_;
  }

  void set_compilation_id(int compilation_id) {
    DCHECK_EQ(compilation_id_, kInvalidCompilationID);
    compilation_id_ = compilation_id;
  }

  std::weak_ptr<NativeModule> const native_module_weak() const {
    return native_module_weak_;
  }

 private:
  uint8_t SetupCompilationProgressForFunction(
      bool lazy_function, NativeModule* module,
      const WasmFeatures& enabled_features, int func_index);

  // Returns the potentially-updated {function_progress}.
  uint8_t AddCompilationUnitInternal(CompilationUnitBuilder* builder,
                                     int function_index,
                                     uint8_t function_progress);

  // Trigger callbacks according to the internal counters below
  // (outstanding_...), plus the given events.
  // Hold the {callbacks_mutex_} when calling this method.
  void TriggerCallbacks(base::EnumSet<CompilationEvent> additional_events = {});

  void PublishCompilationResults(
      std::vector<std::unique_ptr<WasmCode>> unpublished_code);
  void PublishCode(base::Vector<std::unique_ptr<WasmCode>> codes);

  NativeModule* const native_module_;
  std::weak_ptr<NativeModule> const native_module_weak_;
  const std::shared_ptr<Counters> async_counters_;

  // Compilation error, atomically updated. This flag can be updated and read
  // using relaxed semantics.
  std::atomic<bool> compile_failed_{false};

  // True if compilation was cancelled and worker threads should return. This
  // flag can be updated and read using relaxed semantics.
  std::atomic<bool> compile_cancelled_{false};

  CompilationUnitQueues compilation_unit_queues_;

  // Number of wrappers to be compiled. Initialized once, counted down in
  // {GetNextJSToWasmWrapperCompilationUnit}.
  std::atomic<size_t> outstanding_js_to_wasm_wrappers_{0};
  // Wrapper compilation units are stored in shared_ptrs so that they are kept
  // alive by the tasks even if the NativeModule dies.
  std::vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
      js_to_wasm_wrapper_units_;

  // Cache the dynamic tiering configuration to be consistent for the whole
  // compilation.
  const DynamicTiering dynamic_tiering_;

  // This mutex protects all information of this {CompilationStateImpl} which is
  // being accessed concurrently.
  mutable base::Mutex mutex_;

  // The compile job handle, initialized right after construction of
  // {CompilationStateImpl}.
  std::unique_ptr<JobHandle> compile_job_;

  // The compilation id to identify trace events linked to this compilation.
  static constexpr int kInvalidCompilationID = -1;
  int compilation_id_ = kInvalidCompilationID;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  // Features detected to be used in this module. Features can be detected
  // as a module is being compiled.
  WasmFeatures detected_features_ = WasmFeatures::None();

  // Abstraction over the storage of the wire bytes. Held in a shared_ptr so
  // that background compilation jobs can keep the storage alive while
  // compiling.
  std::shared_ptr<WireBytesStorage> wire_bytes_storage_;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  // This mutex protects the callbacks vector, and the counters used to
  // determine which callbacks to call. The counters plus the callbacks
  // themselves need to be synchronized to ensure correct order of events.
  mutable base::Mutex callbacks_mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {callbacks_mutex_}:

  // Callbacks to be called on compilation events.
  std::vector<std::unique_ptr<CompilationEventCallback>> callbacks_;

  // Events that already happened.
  base::EnumSet<CompilationEvent> finished_events_;

  int outstanding_baseline_units_ = 0;
  int outstanding_export_wrappers_ = 0;
  int outstanding_top_tier_functions_ = 0;
  // The amount of generated top tier code since the last
  // {kFinishedCompilationChunk} event.
  size_t bytes_since_last_chunk_ = 0;
  std::vector<uint8_t> compilation_progress_;

  int outstanding_recompilation_functions_ = 0;
  TieringState tiering_state_ = kTieredUp;

  // End of fields protected by {callbacks_mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  // {publish_mutex_} protects {publish_queue_} and {publisher_running_}.
  base::Mutex publish_mutex_;
  std::vector<std::unique_ptr<WasmCode>> publish_queue_;
  bool publisher_running_ = false;

  // Encoding of fields in the {compilation_progress_} vector.
  using RequiredBaselineTierField = base::BitField8<ExecutionTier, 0, 2>;
  using RequiredTopTierField = base::BitField8<ExecutionTier, 2, 2>;
  using ReachedTierField = base::BitField8<ExecutionTier, 4, 2>;
  using MissingRecompilationField = base::BitField8<bool, 6, 1>;
};

CompilationStateImpl* Impl(CompilationState* compilation_state) {
  return reinterpret_cast<CompilationStateImpl*>(compilation_state);
}
const CompilationStateImpl* Impl(const CompilationState* compilation_state) {
  return reinterpret_cast<const CompilationStateImpl*>(compilation_state);
}

CompilationStateImpl* BackgroundCompileScope::compilation_state() const {
  DCHECK(native_module_);
  return Impl(native_module_->compilation_state());
}

bool BackgroundCompileScope::cancelled() const {
  return native_module_ == nullptr ||
         Impl(native_module_->compilation_state())->cancelled();
}

void UpdateFeatureUseCounts(Isolate* isolate, const WasmFeatures& detected) {
  using Feature = v8::Isolate::UseCounterFeature;
  constexpr static std::pair<WasmFeature, Feature> kUseCounters[] = {
      {kFeature_reftypes, Feature::kWasmRefTypes},
      {kFeature_simd, Feature::kWasmSimdOpcodes},
      {kFeature_threads, Feature::kWasmThreadOpcodes},
      {kFeature_eh, Feature::kWasmExceptionHandling}};

  for (auto& feature : kUseCounters) {
    if (detected.contains(feature.first)) isolate->CountUsage(feature.second);
  }
}

}  // namespace

//////////////////////////////////////////////////////
// PIMPL implementation of {CompilationState}.

CompilationState::~CompilationState() { Impl(this)->~CompilationStateImpl(); }

void CompilationState::InitCompileJob() { Impl(this)->InitCompileJob(); }

void CompilationState::CancelCompilation() {
  Impl(this)->CancelCompilation(CompilationStateImpl::kCancelUnconditionally);
}

void CompilationState::CancelInitialCompilation() {
  Impl(this)->CancelCompilation(
      CompilationStateImpl::kCancelInitialCompilation);
}

void CompilationState::SetError() { Impl(this)->SetError(); }

void CompilationState::SetWireBytesStorage(
    std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
  Impl(this)->SetWireBytesStorage(std::move(wire_bytes_storage));
}

std::shared_ptr<WireBytesStorage> CompilationState::GetWireBytesStorage()
    const {
  return Impl(this)->GetWireBytesStorage();
}

void CompilationState::AddCallback(
    std::unique_ptr<CompilationEventCallback> callback) {
  return Impl(this)->AddCallback(std::move(callback));
}

void CompilationState::WaitForTopTierFinished() {
  Impl(this)->WaitForCompilationEvent(
      CompilationEvent::kFinishedTopTierCompilation);
}

void CompilationState::SetHighPriority() { Impl(this)->SetHighPriority(); }

void CompilationState::InitializeAfterDeserialization(
    base::Vector<const int> lazy_functions,
    base::Vector<const int> liftoff_functions) {
  Impl(this)->InitializeCompilationProgressAfterDeserialization(
      lazy_functions, liftoff_functions);
}

bool CompilationState::failed() const { return Impl(this)->failed(); }

bool CompilationState::baseline_compilation_finished() const {
  return Impl(this)->baseline_compilation_finished();
}

bool CompilationState::top_tier_compilation_finished() const {
  return Impl(this)->top_tier_compilation_finished();
}

bool CompilationState::recompilation_finished() const {
  return Impl(this)->recompilation_finished();
}

void CompilationState::set_compilation_id(int compilation_id) {
  Impl(this)->set_compilation_id(compilation_id);
}

DynamicTiering CompilationState::dynamic_tiering() const {
  return Impl(this)->dynamic_tiering();
}

// static
std::unique_ptr<CompilationState> CompilationState::New(
    const std::shared_ptr<NativeModule>& native_module,
    std::shared_ptr<Counters> async_counters, DynamicTiering dynamic_tiering) {
  return std::unique_ptr<CompilationState>(reinterpret_cast<CompilationState*>(
      new CompilationStateImpl(std::move(native_module),
                               std::move(async_counters), dynamic_tiering)));
}

// End of PIMPL implementation of {CompilationState}.
//////////////////////////////////////////////////////

namespace {

ExecutionTier ApplyHintToExecutionTier(WasmCompilationHintTier hint,
                                       ExecutionTier default_tier) {
  switch (hint) {
    case WasmCompilationHintTier::kDefault:
      return default_tier;
    case WasmCompilationHintTier::kBaseline:
      return ExecutionTier::kLiftoff;
    case WasmCompilationHintTier::kOptimized:
      return ExecutionTier::kTurbofan;
  }
  UNREACHABLE();
}

const WasmCompilationHint* GetCompilationHint(const WasmModule* module,
                                              uint32_t func_index) {
  DCHECK_LE(module->num_imported_functions, func_index);
  uint32_t hint_index = declared_function_index(module, func_index);
  const std::vector<WasmCompilationHint>& compilation_hints =
      module->compilation_hints;
  if (hint_index < compilation_hints.size()) {
    return &compilation_hints[hint_index];
  }
  return nullptr;
}

CompileStrategy GetCompileStrategy(const WasmModule* module,
                                   const WasmFeatures& enabled_features,
                                   uint32_t func_index, bool lazy_module) {
  if (lazy_module) return CompileStrategy::kLazy;
  if (!enabled_features.has_compilation_hints()) {
    return CompileStrategy::kDefault;
  }
  auto* hint = GetCompilationHint(module, func_index);
  if (hint == nullptr) return CompileStrategy::kDefault;
  switch (hint->strategy) {
    case WasmCompilationHintStrategy::kLazy:
      return CompileStrategy::kLazy;
    case WasmCompilationHintStrategy::kEager:
      return CompileStrategy::kEager;
    case WasmCompilationHintStrategy::kLazyBaselineEagerTopTier:
      return CompileStrategy::kLazyBaselineEagerTopTier;
    case WasmCompilationHintStrategy::kDefault:
      return CompileStrategy::kDefault;
  }
}

struct ExecutionTierPair {
  ExecutionTier baseline_tier;
  ExecutionTier top_tier;
};

ExecutionTierPair GetRequestedExecutionTiers(
    NativeModule* native_module, const WasmFeatures& enabled_features,
    uint32_t func_index) {
  const WasmModule* module = native_module->module();
  ExecutionTierPair result;

  result.baseline_tier = WasmCompilationUnit::GetBaselineExecutionTier(module);

  bool dynamic_tiering =
      Impl(native_module->compilation_state())->dynamic_tiering() ==
      DynamicTiering::kEnabled;
  bool tier_up_enabled = !dynamic_tiering && FLAG_wasm_tier_up;
  if (module->origin != kWasmOrigin || !tier_up_enabled ||
      V8_UNLIKELY(FLAG_wasm_tier_up_filter >= 0 &&
                  func_index !=
                      static_cast<uint32_t>(FLAG_wasm_tier_up_filter))) {
    result.top_tier = result.baseline_tier;
    return result;
  }

  // Default tiering behaviour.
  result.top_tier = ExecutionTier::kTurbofan;

  // Check if compilation hints override default tiering behaviour.
  if (enabled_features.has_compilation_hints()) {
    const WasmCompilationHint* hint = GetCompilationHint(module, func_index);
    if (hint != nullptr) {
      result.baseline_tier =
          ApplyHintToExecutionTier(hint->baseline_tier, result.baseline_tier);
      result.top_tier =
          ApplyHintToExecutionTier(hint->top_tier, result.top_tier);
    }
  }

  // Correct top tier if necessary.
  static_assert(ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                "Assume an order on execution tiers");
  if (result.baseline_tier > result.top_tier) {
    result.top_tier = result.baseline_tier;
  }
  return result;
}

// The {CompilationUnitBuilder} builds compilation units and stores them in an
// internal buffer. The buffer is moved into the working queue of the
// {CompilationStateImpl} when {Commit} is called.
class CompilationUnitBuilder {
 public:
  explicit CompilationUnitBuilder(NativeModule* native_module)
      : native_module_(native_module) {}

  void AddUnits(uint32_t func_index) {
    if (func_index < native_module_->module()->num_imported_functions) {
      baseline_units_.emplace_back(func_index, ExecutionTier::kNone,
                                   kNoDebugging);
      return;
    }
    ExecutionTierPair tiers = GetRequestedExecutionTiers(
        native_module_, native_module_->enabled_features(), func_index);
    // Compile everything for non-debugging initially. If needed, we will tier
    // down when the module is fully compiled. Synchronization would be pretty
    // difficult otherwise.
    baseline_units_.emplace_back(func_index, tiers.baseline_tier, kNoDebugging);
    if (tiers.baseline_tier != tiers.top_tier) {
      tiering_units_.emplace_back(func_index, tiers.top_tier, kNoDebugging);
    }
  }

  void AddJSToWasmWrapperUnit(
      std::shared_ptr<JSToWasmWrapperCompilationUnit> unit) {
    js_to_wasm_wrapper_units_.emplace_back(std::move(unit));
  }

  void AddBaselineUnit(int func_index, ExecutionTier tier) {
    baseline_units_.emplace_back(func_index, tier, kNoDebugging);
  }

  void AddTopTierUnit(int func_index, ExecutionTier tier) {
    tiering_units_.emplace_back(func_index, tier, kNoDebugging);
  }

  void AddDebugUnit(int func_index) {
    baseline_units_.emplace_back(func_index, ExecutionTier::kLiftoff,
                                 kForDebugging);
  }

  void AddRecompilationUnit(int func_index, ExecutionTier tier) {
    // For recompilation, just treat all units like baseline units.
    baseline_units_.emplace_back(
        func_index, tier,
        tier == ExecutionTier::kLiftoff ? kForDebugging : kNoDebugging);
  }

  bool Commit() {
    if (baseline_units_.empty() && tiering_units_.empty() &&
        js_to_wasm_wrapper_units_.empty()) {
      return false;
    }
    compilation_state()->CommitCompilationUnits(
        base::VectorOf(baseline_units_), base::VectorOf(tiering_units_),
        base::VectorOf(js_to_wasm_wrapper_units_));
    Clear();
    return true;
  }

  void Clear() {
    baseline_units_.clear();
    tiering_units_.clear();
    js_to_wasm_wrapper_units_.clear();
  }

  const WasmModule* module() { return native_module_->module(); }

 private:
  CompilationStateImpl* compilation_state() const {
    return Impl(native_module_->compilation_state());
  }

  NativeModule* const native_module_;
  std::vector<WasmCompilationUnit> baseline_units_;
  std::vector<WasmCompilationUnit> tiering_units_;
  std::vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
      js_to_wasm_wrapper_units_;
};

void SetCompileError(ErrorThrower* thrower, ModuleWireBytes wire_bytes,
                     const WasmFunction* func, const WasmModule* module,
                     WasmError error) {
  WasmName name = wire_bytes.GetNameOrNull(func, module);
  if (name.begin() == nullptr) {
    thrower->CompileError("Compiling function #%d failed: %s @+%u",
                          func->func_index, error.message().c_str(),
                          error.offset());
  } else {
    TruncatedUserString<> truncated_name(name);
    thrower->CompileError("Compiling function #%d:\"%.*s\" failed: %s @+%u",
                          func->func_index, truncated_name.length(),
                          truncated_name.start(), error.message().c_str(),
                          error.offset());
  }
}

DecodeResult ValidateSingleFunction(const WasmModule* module, int func_index,
                                    base::Vector<const uint8_t> code,
                                    Counters* counters,
                                    AccountingAllocator* allocator,
                                    WasmFeatures enabled_features) {
  const WasmFunction* func = &module->functions[func_index];
  FunctionBody body{func->sig, func->code.offset(), code.begin(), code.end()};
  DecodeResult result;

  WasmFeatures detected;
  return VerifyWasmCode(allocator, enabled_features, module, &detected, body);
}

enum OnlyLazyFunctions : bool {
  kAllFunctions = false,
  kOnlyLazyFunctions = true,
};

void ValidateSequentially(
    const WasmModule* module, NativeModule* native_module, Counters* counters,
    AccountingAllocator* allocator, ErrorThrower* thrower, bool lazy_module,
    OnlyLazyFunctions only_lazy_functions = kAllFunctions) {
  DCHECK(!thrower->error());
  uint32_t start = module->num_imported_functions;
  uint32_t end = start + module->num_declared_functions;
  auto enabled_features = native_module->enabled_features();
  for (uint32_t func_index = start; func_index < end; func_index++) {
    // Skip non-lazy functions if requested.
    if (only_lazy_functions) {
      CompileStrategy strategy =
          GetCompileStrategy(module, enabled_features, func_index, lazy_module);
      if (strategy != CompileStrategy::kLazy &&
          strategy != CompileStrategy::kLazyBaselineEagerTopTier) {
        continue;
      }
    }

    ModuleWireBytes wire_bytes{native_module->wire_bytes()};
    const WasmFunction* func = &module->functions[func_index];
    base::Vector<const uint8_t> code = wire_bytes.GetFunctionBytes(func);
    DecodeResult result = ValidateSingleFunction(
        module, func_index, code, counters, allocator, enabled_features);
    if (result.failed()) {
      SetCompileError(thrower, wire_bytes, func, module, result.error());
    }
  }
}

bool IsLazyModule(const WasmModule* module) {
  return FLAG_wasm_lazy_compilation ||
         (FLAG_asm_wasm_lazy_compilation && is_asmjs_module(module));
}

}  // namespace

bool CompileLazy(Isolate* isolate, Handle<WasmInstanceObject> instance,
                 int func_index) {
  Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
  NativeModule* native_module = module_object->native_module();
  const WasmModule* module = native_module->module();
  auto enabled_features = native_module->enabled_features();
  Counters* counters = isolate->counters();

  // Put the timer scope around everything, including the {CodeSpaceWriteScope}
  // and its destruction, to measure complete overhead (apart from the runtime
  // function itself, which has constant overhead).
  base::Optional<TimedHistogramScope> lazy_compile_time_scope;
  if (base::TimeTicks::IsHighResolution()) {
    lazy_compile_time_scope.emplace(counters->wasm_lazy_compile_time());
  }

  DCHECK(!native_module->lazy_compile_frozen());

  TRACE_LAZY("Compiling wasm-function#%d.\n", func_index);

  base::ThreadTicks thread_ticks = base::ThreadTicks::IsSupported()
                                       ? base::ThreadTicks::Now()
                                       : base::ThreadTicks();

  CompilationStateImpl* compilation_state =
      Impl(native_module->compilation_state());
  ExecutionTierPair tiers =
      GetRequestedExecutionTiers(native_module, enabled_features, func_index);

  DCHECK_LE(native_module->num_imported_functions(), func_index);
  DCHECK_LT(func_index, native_module->num_functions());
  WasmCompilationUnit baseline_unit{func_index, tiers.baseline_tier,
                                    kNoDebugging};
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmEngine* engine = GetWasmEngine();
  WasmFeatures detected_features;
  WasmCompilationResult result = baseline_unit.ExecuteCompilation(
      &env, compilation_state->GetWireBytesStorage().get(), counters,
      &detected_features);
  compilation_state->OnCompilationStopped(detected_features);
  if (!thread_ticks.IsNull()) {
    native_module->UpdateCPUDuration(
        (base::ThreadTicks::Now() - thread_ticks).InMicroseconds(),
        tiers.baseline_tier);
  }

  // During lazy compilation, we can only get compilation errors when
  // {--wasm-lazy-validation} is enabled. Otherwise, the module was fully
  // verified before starting its execution.
  CHECK_IMPLIES(result.failed(), FLAG_wasm_lazy_validation);
  const WasmFunction* func = &module->functions[func_index];
  if (result.failed()) {
    ErrorThrower thrower(isolate, nullptr);
    base::Vector<const uint8_t> code =
        compilation_state->GetWireBytesStorage()->GetCode(func->code);
    DecodeResult decode_result =
        ValidateSingleFunction(module, func_index, code, counters,
                               engine->allocator(), enabled_features);
    CHECK(decode_result.failed());
    SetCompileError(&thrower, ModuleWireBytes(native_module->wire_bytes()),
                    func, module, decode_result.error());
    return false;
  }

  // Allocate feedback vector if needed.
  if (result.feedback_vector_slots > 0) {
    DCHECK(FLAG_wasm_speculative_inlining);
    Handle<FixedArray> vector = isolate->factory()->NewFixedArrayWithZeroes(
        result.feedback_vector_slots);
    instance->feedback_vectors().set(
        declared_function_index(module, func_index), *vector);
  }

  WasmCodeRefScope code_ref_scope;
  WasmCode* code;
  {
    CodeSpaceWriteScope code_space_write_scope(native_module);
    code = native_module->PublishCode(
        native_module->AddCompiledCode(std::move(result)));
  }
  DCHECK_EQ(func_index, code->index());

  if (WasmCode::ShouldBeLogged(isolate)) {
    DisallowGarbageCollection no_gc;
    Object url_obj = module_object->script().name();
    DCHECK(url_obj.IsString() || url_obj.IsUndefined());
    std::unique_ptr<char[]> url =
        url_obj.IsString() ? String::cast(url_obj).ToCString() : nullptr;
    code->LogCode(isolate, url.get(), module_object->script().id());
  }

  counters->wasm_lazily_compiled_functions()->Increment();

  const bool lazy_module = IsLazyModule(module);
  if (GetCompileStrategy(module, enabled_features, func_index, lazy_module) ==
          CompileStrategy::kLazy &&
      tiers.baseline_tier < tiers.top_tier) {
    WasmCompilationUnit tiering_unit{func_index, tiers.top_tier, kNoDebugging};
    compilation_state->CommitTopTierCompilationUnit(tiering_unit);
  }

  return true;
}

class TransitiveTypeFeedbackProcessor {
 public:
  TransitiveTypeFeedbackProcessor(const WasmModule* module,
                                  Handle<WasmInstanceObject> instance,
                                  int func_index)
      : instance_(instance),
        feedback_for_function_(module->type_feedback.feedback_for_function) {
    base::MutexGuard mutex_guard(&module->type_feedback.mutex);
    queue_.insert(func_index);
    while (!queue_.empty()) {
      auto next = queue_.cbegin();
      Process(*next);
      queue_.erase(next);
    }
  }

 private:
  void Process(int func_index);

  void EnqueueCallees(std::vector<CallSiteFeedback> feedback) {
    for (size_t i = 0; i < feedback.size(); i++) {
      int func = feedback[i].function_index;
      // TODO(jkummerow): Find a way to get the target function ID for
      // direct calls (which currently requires decoding the function).
      if (func == -1) continue;
      // Don't spend time on calls that have never been executed.
      if (feedback[i].absolute_call_frequency == 0) continue;
      // Don't recompute feedback that has already been processed.
      auto existing = feedback_for_function_.find(func);
      if (existing != feedback_for_function_.end() &&
          existing->second.feedback_vector.size() > 0) {
        continue;
      }
      queue_.insert(func);
    }
  }

  Handle<WasmInstanceObject> instance_;
  std::map<uint32_t, FunctionTypeFeedback>& feedback_for_function_;
  std::unordered_set<int> queue_;
};

void TransitiveTypeFeedbackProcessor::Process(int func_index) {
  int which_vector = declared_function_index(instance_->module(), func_index);
  Object maybe_feedback = instance_->feedback_vectors().get(which_vector);
  if (!maybe_feedback.IsFixedArray()) return;
  FixedArray feedback = FixedArray::cast(maybe_feedback);
  std::vector<CallSiteFeedback> result(feedback.length() / 2);
  int imported_functions =
      static_cast<int>(instance_->module()->num_imported_functions);
  for (int i = 0; i < feedback.length(); i += 2) {
    Object value = feedback.get(i);
    if (value.IsWasmInternalFunction() &&
        WasmExportedFunction::IsWasmExportedFunction(
            WasmInternalFunction::cast(value).external())) {
      // Monomorphic, and the internal function points to a wasm-generated
      // external function (WasmExportedFunction). Mark the target for inlining
      // if it's defined in the same module.
      WasmExportedFunction target = WasmExportedFunction::cast(
          WasmInternalFunction::cast(value).external());
      if (target.instance() == *instance_ &&
          target.function_index() >= imported_functions) {
        if (FLAG_trace_wasm_speculative_inlining) {
          PrintF("[Function #%d call_ref #%d inlineable (monomorphic)]\n",
                 func_index, i / 2);
        }
        int32_t count = Smi::cast(feedback.get(i + 1)).value();
        result[i / 2] = {target.function_index(), count};
        continue;
      }
    } else if (value.IsFixedArray()) {
      // Polymorphic. Pick a target for inlining if there is one that was
      // seen for most calls, and matches the requirements of the monomorphic
      // case.
      FixedArray polymorphic = FixedArray::cast(value);
      size_t total_count = 0;
      for (int j = 0; j < polymorphic.length(); j += 2) {
        total_count += Smi::cast(polymorphic.get(j + 1)).value();
      }
      int found_target = -1;
      int found_count = -1;
      double best_frequency = 0;
      for (int j = 0; j < polymorphic.length(); j += 2) {
        int32_t this_count = Smi::cast(polymorphic.get(j + 1)).value();
        double frequency = static_cast<double>(this_count) / total_count;
        if (frequency > best_frequency) best_frequency = frequency;
        if (frequency < 0.8) continue;

        // We reject this polymorphic entry if:
        // - it is not defined,
        // - it is not a wasm-defined function (WasmExportedFunction)
        // - it was not defined in this module.
        if (!polymorphic.get(j).IsWasmInternalFunction()) continue;
        WasmInternalFunction internal =
            WasmInternalFunction::cast(polymorphic.get(j));
        if (!WasmExportedFunction::IsWasmExportedFunction(
                internal.external())) {
          continue;
        }
        WasmExportedFunction target =
            WasmExportedFunction::cast(internal.external());
        if (target.instance() != *instance_ ||
            target.function_index() < imported_functions) {
          continue;
        }

        found_target = target.function_index();
        found_count = static_cast<int>(this_count);
        if (FLAG_trace_wasm_speculative_inlining) {
          PrintF("[Function #%d call_ref #%d inlineable (polymorphic %f)]\n",
                 func_index, i / 2, frequency);
        }
        break;
      }
      if (found_target >= 0) {
        result[i / 2] = {found_target, found_count};
        continue;
      } else if (FLAG_trace_wasm_speculative_inlining) {
        PrintF("[Function #%d call_ref #%d: best frequency %f]\n", func_index,
               i / 2, best_frequency);
      }
    } else if (value.IsSmi()) {
      // Direct call, just collecting call count.
      int count = Smi::cast(value).value();
      if (FLAG_trace_wasm_speculative_inlining) {
        PrintF("[Function #%d call_direct #%d: frequency %d]\n", func_index,
               i / 2, count);
      }
      result[i / 2] = {-1, count};
      continue;
    }
    // If we fall through to here, then this call isn't eligible for inlining.
    // Possible reasons: uninitialized or megamorphic feedback; or monomorphic
    // or polymorphic that didn't meet our requirements.
    if (FLAG_trace_wasm_speculative_inlining) {
      PrintF("[Function #%d call_ref #%d *not* inlineable]\n", func_index,
             i / 2);
    }
    result[i / 2] = {-1, -1};
  }
  EnqueueCallees(result);
  feedback_for_function_[func_index].feedback_vector = std::move(result);
}

void TriggerTierUp(Isolate* isolate, NativeModule* native_module,
                   int func_index, Handle<WasmInstanceObject> instance) {
  CompilationStateImpl* compilation_state =
      Impl(native_module->compilation_state());
  WasmCompilationUnit tiering_unit{func_index, ExecutionTier::kTurbofan,
                                   kNoDebugging};

  const WasmModule* module = native_module->module();
  size_t priority;
  {
    base::MutexGuard mutex_guard(&module->type_feedback.mutex);
    int saved_priority =
        module->type_feedback.feedback_for_function[func_index].tierup_priority;
    saved_priority++;
    module->type_feedback.feedback_for_function[func_index].tierup_priority =
        saved_priority;
    // Continue to creating a compilation unit if this is the first time
    // we detect this function as hot, and create a new higher-priority unit
    // if the number of tierup checks is a power of two (at least 4).
    if (saved_priority > 1 &&
        (saved_priority < 4 || (saved_priority & (saved_priority - 1)) != 0)) {
      return;
    }
    priority = saved_priority;
  }
  if (FLAG_wasm_speculative_inlining) {
    // TODO(jkummerow): we could have collisions here if different instances
    // of the same module have collected different feedback. If that ever
    // becomes a problem, figure out a solution.
    TransitiveTypeFeedbackProcessor process(module, instance, func_index);
  }

  compilation_state->AddTopTierPriorityCompilationUnit(tiering_unit, priority);
}

namespace {

void RecordStats(const Code code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(code.raw_body_size());
  counters->wasm_reloc_size()->Increment(code.relocation_info().length());
}

enum CompilationExecutionResult : int8_t { kNoMoreUnits, kYield };

CompilationExecutionResult ExecuteJSToWasmWrapperCompilationUnits(
    std::weak_ptr<NativeModule> native_module, JobDelegate* delegate) {
  std::shared_ptr<JSToWasmWrapperCompilationUnit> wrapper_unit = nullptr;
  int num_processed_wrappers = 0;

  OperationsBarrier::Token wrapper_compilation_token;
  Isolate* isolate;

  {
    BackgroundCompileScope compile_scope(native_module);
    if (compile_scope.cancelled()) return kYield;
    wrapper_unit = compile_scope.compilation_state()
                       ->GetNextJSToWasmWrapperCompilationUnit();
    if (!wrapper_unit) return kNoMoreUnits;
    isolate = wrapper_unit->isolate();
    wrapper_compilation_token =
        wasm::GetWasmEngine()->StartWrapperCompilation(isolate);
    if (!wrapper_compilation_token) return kNoMoreUnits;
  }

  TRACE_EVENT0("v8.wasm", "wasm.JSToWasmWrapperCompilation");
  while (true) {
    DCHECK_EQ(isolate, wrapper_unit->isolate());
    wrapper_unit->Execute();
    ++num_processed_wrappers;
    bool yield = delegate && delegate->ShouldYield();
    BackgroundCompileScope compile_scope(native_module);
    if (compile_scope.cancelled()) return kYield;
    if (yield ||
        !(wrapper_unit = compile_scope.compilation_state()
                             ->GetNextJSToWasmWrapperCompilationUnit())) {
      compile_scope.compilation_state()->OnFinishedJSToWasmWrapperUnits(
          num_processed_wrappers);
      return yield ? kYield : kNoMoreUnits;
    }
  }
}

namespace {
const char* GetCompilationEventName(const WasmCompilationUnit& unit,
                                    const CompilationEnv& env) {
  ExecutionTier tier = unit.tier();
  if (tier == ExecutionTier::kLiftoff) {
    return "wasm.BaselineCompilation";
  }
  if (tier == ExecutionTier::kTurbofan) {
    return "wasm.TopTierCompilation";
  }
  if (unit.func_index() <
      static_cast<int>(env.module->num_imported_functions)) {
    return "wasm.WasmToJSWrapperCompilation";
  }
  return "wasm.OtherCompilation";
}
}  // namespace

constexpr uint8_t kMainTaskId = 0;

// Run by the {BackgroundCompileJob} (on any thread).
CompilationExecutionResult ExecuteCompilationUnits(
    std::weak_ptr<NativeModule> native_module, Counters* counters,
    JobDelegate* delegate, CompileBaselineOnly baseline_only) {
  TRACE_EVENT0("v8.wasm", "wasm.ExecuteCompilationUnits");

  // Execute JS to Wasm wrapper units first, so that they are ready to be
  // finalized by the main thread when the kFinishedBaselineCompilation event is
  // triggered.
  if (ExecuteJSToWasmWrapperCompilationUnits(native_module, delegate) ==
      kYield) {
    return kYield;
  }

  // These fields are initialized in a {BackgroundCompileScope} before
  // starting compilation.
  base::Optional<CompilationEnv> env;
  std::shared_ptr<WireBytesStorage> wire_bytes;
  std::shared_ptr<const WasmModule> module;
  // Task 0 is any main thread (there might be multiple from multiple isolates),
  // worker threads start at 1 (thus the "+ 1").
  STATIC_ASSERT(kMainTaskId == 0);
  int task_id = delegate ? (int{delegate->GetTaskId()} + 1) : kMainTaskId;
  DCHECK_LE(0, task_id);
  CompilationUnitQueues::Queue* queue;
  base::Optional<WasmCompilationUnit> unit;

  WasmFeatures detected_features = WasmFeatures::None();

  base::ThreadTicks thread_ticks = base::ThreadTicks::IsSupported()
                                       ? base::ThreadTicks::Now()
                                       : base::ThreadTicks();

  // Preparation (synchronized): Initialize the fields above and get the first
  // compilation unit.
  {
    BackgroundCompileScope compile_scope(native_module);
    if (compile_scope.cancelled()) return kYield;
    env.emplace(compile_scope.native_module()->CreateCompilationEnv());
    wire_bytes = compile_scope.compilation_state()->GetWireBytesStorage();
    module = compile_scope.native_module()->shared_module();
    queue = compile_scope.compilation_state()->GetQueueForCompileTask(task_id);
    unit = compile_scope.compilation_state()->GetNextCompilationUnit(
        queue, baseline_only);
    if (!unit) return kNoMoreUnits;
  }
  TRACE_COMPILE("ExecuteCompilationUnits (task id %d)\n", task_id);

  std::vector<WasmCompilationResult> results_to_publish;
  while (true) {
    ExecutionTier current_tier = unit->tier();
    const char* event_name = GetCompilationEventName(unit.value(), env.value());
    TRACE_EVENT0("v8.wasm", event_name);
    while (unit->tier() == current_tier) {
      // (asynchronous): Execute the compilation.
      WasmCompilationResult result = unit->ExecuteCompilation(
          &env.value(), wire_bytes.get(), counters, &detected_features);
      results_to_publish.emplace_back(std::move(result));

      bool yield = delegate && delegate->ShouldYield();

      // (synchronized): Publish the compilation result and get the next unit.
      BackgroundCompileScope compile_scope(native_module);
      if (compile_scope.cancelled()) return kYield;

      if (!results_to_publish.back().succeeded()) {
        compile_scope.compilation_state()->SetError();
        return kNoMoreUnits;
      }

      if (!unit->for_debugging() && result.result_tier != current_tier) {
        compile_scope.native_module()->AddLiftoffBailout();
      }

      // Yield or get next unit.
      if (yield ||
          !(unit = compile_scope.compilation_state()->GetNextCompilationUnit(
                queue, baseline_only))) {
        if (!thread_ticks.IsNull()) {
          compile_scope.native_module()->UpdateCPUDuration(
              (base::ThreadTicks::Now() - thread_ticks).InMicroseconds(),
              current_tier);
        }
        std::vector<std::unique_ptr<WasmCode>> unpublished_code =
            compile_scope.native_module()->AddCompiledCode(
                base::VectorOf(std::move(results_to_publish)));
        results_to_publish.clear();
        compile_scope.compilation_state()->SchedulePublishCompilationResults(
            std::move(unpublished_code));
        compile_scope.compilation_state()->OnCompilationStopped(
            detected_features);
        return yield ? kYield : kNoMoreUnits;
      }

      // Publish after finishing a certain amount of units, to avoid contention
      // when all threads publish at the end.
      bool batch_full =
          queue->ShouldPublish(static_cast<int>(results_to_publish.size()));
      // Also publish each time the compilation tier changes from Liftoff to
      // TurboFan, such that we immediately publish the baseline compilation
      // results to start execution, and do not wait for a batch to fill up.
      bool liftoff_finished = unit->tier() != current_tier &&
                              unit->tier() == ExecutionTier::kTurbofan;
      if (batch_full || liftoff_finished) {
        if (!thread_ticks.IsNull()) {
          base::ThreadTicks thread_ticks_now = base::ThreadTicks::Now();
          compile_scope.native_module()->UpdateCPUDuration(
              (thread_ticks_now - thread_ticks).InMicroseconds(), current_tier);
          thread_ticks = thread_ticks_now;
        }
        std::vector<std::unique_ptr<WasmCode>> unpublished_code =
            compile_scope.native_module()->AddCompiledCode(
                base::VectorOf(std::move(results_to_publish)));
        results_to_publish.clear();
        compile_scope.compilation_state()->SchedulePublishCompilationResults(
            std::move(unpublished_code));
      }
    }
  }
  UNREACHABLE();
}

using JSToWasmWrapperKey = std::pair<bool, FunctionSig>;

// Returns the number of units added.
int AddExportWrapperUnits(Isolate* isolate, NativeModule* native_module,
                          CompilationUnitBuilder* builder) {
  std::unordered_set<JSToWasmWrapperKey, base::hash<JSToWasmWrapperKey>> keys;
  for (auto exp : native_module->module()->export_table) {
    if (exp.kind != kExternalFunction) continue;
    auto& function = native_module->module()->functions[exp.index];
    JSToWasmWrapperKey key(function.imported, *function.sig);
    if (keys.insert(key).second) {
      auto unit = std::make_shared<JSToWasmWrapperCompilationUnit>(
          isolate, function.sig, native_module->module(), function.imported,
          native_module->enabled_features(),
          JSToWasmWrapperCompilationUnit::kAllowGeneric);
      builder->AddJSToWasmWrapperUnit(std::move(unit));
    }
  }

  return static_cast<int>(keys.size());
}

// Returns the number of units added.
int AddImportWrapperUnits(NativeModule* native_module,
                          CompilationUnitBuilder* builder) {
  std::unordered_set<WasmImportWrapperCache::CacheKey,
                     WasmImportWrapperCache::CacheKeyHash>
      keys;
  int num_imported_functions = native_module->num_imported_functions();
  for (int func_index = 0; func_index < num_imported_functions; func_index++) {
    const FunctionSig* sig = native_module->module()->functions[func_index].sig;
    if (!IsJSCompatibleSignature(sig, native_module->module(),
                                 native_module->enabled_features())) {
      continue;
    }
    WasmImportWrapperCache::CacheKey key(
        compiler::kDefaultImportCallKind, sig,
        static_cast<int>(sig->parameter_count()), kNoSuspend);
    auto it = keys.insert(key);
    if (it.second) {
      // Ensure that all keys exist in the cache, so that we can populate the
      // cache later without locking.
      (*native_module->import_wrapper_cache())[key] = nullptr;
      builder->AddUnits(func_index);
    }
  }
  return static_cast<int>(keys.size());
}

void InitializeLazyCompilation(NativeModule* native_module) {
  const bool lazy_module = IsLazyModule(native_module->module());
  auto* module = native_module->module();

  uint32_t start = module->num_imported_functions;
  uint32_t end = start + module->num_declared_functions;
  base::Optional<CodeSpaceWriteScope> lazy_code_space_write_scope;
  for (uint32_t func_index = start; func_index < end; func_index++) {
    CompileStrategy strategy = GetCompileStrategy(
        module, native_module->enabled_features(), func_index, lazy_module);
    if (strategy == CompileStrategy::kLazy ||
        strategy == CompileStrategy::kLazyBaselineEagerTopTier) {
      // Open a single scope for all following calls to {UseLazyStub()}, instead
      // of flipping page permissions for each {func_index} individually.
      if (!lazy_code_space_write_scope.has_value()) {
        lazy_code_space_write_scope.emplace(native_module);
      }
      native_module->UseLazyStub(func_index);
    }
  }
}

std::unique_ptr<CompilationUnitBuilder> InitializeCompilation(
    Isolate* isolate, NativeModule* native_module) {
  InitializeLazyCompilation(native_module);
  CompilationStateImpl* compilation_state =
      Impl(native_module->compilation_state());
  const bool lazy_module = IsLazyModule(native_module->module());
  auto builder = std::make_unique<CompilationUnitBuilder>(native_module);
  int num_import_wrappers = AddImportWrapperUnits(native_module, builder.get());
  int num_export_wrappers =
      AddExportWrapperUnits(isolate, native_module, builder.get());
  compilation_state->InitializeCompilationProgress(
      lazy_module, num_import_wrappers, num_export_wrappers);
  return builder;
}

bool MayCompriseLazyFunctions(const WasmModule* module,
                              const WasmFeatures& enabled_features,
                              bool lazy_module) {
  if (lazy_module || enabled_features.has_compilation_hints()) return true;
#ifdef ENABLE_SLOW_DCHECKS
  int start = module->num_imported_functions;
  int end = start + module->num_declared_functions;
  for (int func_index = start; func_index < end; func_index++) {
    SLOW_DCHECK(GetCompileStrategy(module, enabled_features, func_index,
                                   lazy_module) != CompileStrategy::kLazy);
  }
#endif
  return false;
}

class CompilationTimeCallback : public CompilationEventCallback {
 public:
  enum CompileMode { kSynchronous, kAsync, kStreaming };
  explicit CompilationTimeCallback(
      std::shared_ptr<Counters> async_counters,
      std::shared_ptr<metrics::Recorder> metrics_recorder,
      v8::metrics::Recorder::ContextId context_id,
      std::weak_ptr<NativeModule> native_module, CompileMode compile_mode)
      : start_time_(base::TimeTicks::Now()),
        async_counters_(std::move(async_counters)),
        metrics_recorder_(std::move(metrics_recorder)),
        context_id_(context_id),
        native_module_(std::move(native_module)),
        compile_mode_(compile_mode) {}

  // Keep this callback alive to be able to record caching metrics.
  ReleaseAfterFinalEvent release_after_final_event() override {
    return CompilationEventCallback::ReleaseAfterFinalEvent::kKeep;
  }

  void call(CompilationEvent compilation_event) override {
    DCHECK(base::TimeTicks::IsHighResolution());
    std::shared_ptr<NativeModule> native_module = native_module_.lock();
    if (!native_module) return;
    auto now = base::TimeTicks::Now();
    auto duration = now - start_time_;
    if (compilation_event == CompilationEvent::kFinishedBaselineCompilation) {
      // Reset {start_time_} to measure tier-up time.
      start_time_ = now;
      if (compile_mode_ != kSynchronous) {
        TimedHistogram* histogram =
            compile_mode_ == kAsync
                ? async_counters_->wasm_async_compile_wasm_module_time()
                : async_counters_->wasm_streaming_compile_wasm_module_time();
        histogram->AddSample(static_cast<int>(duration.InMicroseconds()));
      }

      v8::metrics::WasmModuleCompiled event{
          (compile_mode_ != kSynchronous),         // async
          (compile_mode_ == kStreaming),           // streamed
          false,                                   // cached
          false,                                   // deserialized
          FLAG_wasm_lazy_compilation,              // lazy
          true,                                    // success
          native_module->liftoff_code_size(),      // code_size_in_bytes
          native_module->liftoff_bailout_count(),  // liftoff_bailout_count
          duration.InMicroseconds(),               // wall_clock_duration_in_us
          static_cast<int64_t>(                    // cpu_time_duration_in_us
              native_module->baseline_compilation_cpu_duration())};
      metrics_recorder_->DelayMainThreadEvent(event, context_id_);
    }
    if (compilation_event == CompilationEvent::kFinishedTopTierCompilation) {
      TimedHistogram* histogram = async_counters_->wasm_tier_up_module_time();
      histogram->AddSample(static_cast<int>(duration.InMicroseconds()));

      v8::metrics::WasmModuleTieredUp event{
          FLAG_wasm_lazy_compilation,           // lazy
          native_module->turbofan_code_size(),  // code_size_in_bytes
          duration.InMicroseconds(),            // wall_clock_duration_in_us
          static_cast<int64_t>(                 // cpu_time_duration_in_us
              native_module->tier_up_cpu_duration())};
      metrics_recorder_->DelayMainThreadEvent(event, context_id_);
    }
    if (compilation_event == CompilationEvent::kFailedCompilation) {
      v8::metrics::WasmModuleCompiled event{
          (compile_mode_ != kSynchronous),         // async
          (compile_mode_ == kStreaming),           // streamed
          false,                                   // cached
          false,                                   // deserialized
          FLAG_wasm_lazy_compilation,              // lazy
          false,                                   // success
          native_module->liftoff_code_size(),      // code_size_in_bytes
          native_module->liftoff_bailout_count(),  // liftoff_bailout_count
          duration.InMicroseconds(),               // wall_clock_duration_in_us
          static_cast<int64_t>(                    // cpu_time_duration_in_us
              native_module->baseline_compilation_cpu_duration())};
      metrics_recorder_->DelayMainThreadEvent(event, context_id_);
    }
  }

 private:
  base::TimeTicks start_time_;
  const std::shared_ptr<Counters> async_counters_;
  std::shared_ptr<metrics::Recorder> metrics_recorder_;
  v8::metrics::Recorder::ContextId context_id_;
  std::weak_ptr<NativeModule> native_module_;
  const CompileMode compile_mode_;
};

void CompileNativeModule(Isolate* isolate,
                         v8::metrics::Recorder::ContextId context_id,
                         ErrorThrower* thrower, const WasmModule* wasm_module,
                         std::shared_ptr<NativeModule> native_module,
                         Handle<FixedArray>* export_wrappers_out) {
  CHECK(!FLAG_jitless);
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const bool lazy_module = IsLazyModule(wasm_module);
  if (!FLAG_wasm_lazy_validation && wasm_module->origin == kWasmOrigin &&
      MayCompriseLazyFunctions(wasm_module, native_module->enabled_features(),
                               lazy_module)) {
    // Validate wasm modules for lazy compilation if requested. Never validate
    // asm.js modules as these are valid by construction (additionally a CHECK
    // will catch this during lazy compilation).
    ValidateSequentially(wasm_module, native_module.get(), isolate->counters(),
                         isolate->allocator(), thrower, lazy_module,
                         kOnlyLazyFunctions);
    // On error: Return and leave the module in an unexecutable state.
    if (thrower->error()) return;
  }

  DCHECK_GE(kMaxInt, native_module->module()->num_declared_functions);

  // The callback captures a shared ptr to the semaphore.
  auto* compilation_state = Impl(native_module->compilation_state());
  if (base::TimeTicks::IsHighResolution()) {
    compilation_state->AddCallback(std::make_unique<CompilationTimeCallback>(
        isolate->async_counters(), isolate->metrics_recorder(), context_id,
        native_module, CompilationTimeCallback::kSynchronous));
  }

  // Initialize the compilation units and kick off background compile tasks.
  std::unique_ptr<CompilationUnitBuilder> builder =
      InitializeCompilation(isolate, native_module.get());
  compilation_state->InitializeCompilationUnits(std::move(builder));

  compilation_state->WaitForCompilationEvent(
      CompilationEvent::kFinishedExportWrappers);

  if (compilation_state->failed()) {
    DCHECK_IMPLIES(lazy_module, !FLAG_wasm_lazy_validation);
    ValidateSequentially(wasm_module, native_module.get(), isolate->counters(),
                         isolate->allocator(), thrower, lazy_module);
    CHECK(thrower->error());
    return;
  }

  compilation_state->FinalizeJSToWasmWrappers(isolate, native_module->module(),
                                              export_wrappers_out);

  compilation_state->WaitForCompilationEvent(
      CompilationEvent::kFinishedBaselineCompilation);

  compilation_state->PublishDetectedFeatures(isolate);

  if (compilation_state->failed()) {
    DCHECK_IMPLIES(lazy_module, !FLAG_wasm_lazy_validation);
    ValidateSequentially(wasm_module, native_module.get(), isolate->counters(),
                         isolate->allocator(), thrower, lazy_module);
    CHECK(thrower->error());
  }
}

class BackgroundCompileJob final : public JobTask {
 public:
  explicit BackgroundCompileJob(std::weak_ptr<NativeModule> native_module,
                                std::shared_ptr<Counters> async_counters)
      : native_module_(std::move(native_module)),
        engine_barrier_(GetWasmEngine()->GetBarrierForBackgroundCompile()),
        async_counters_(std::move(async_counters)) {}

  void Run(JobDelegate* delegate) override {
    auto engine_scope = engine_barrier_->TryLock();
    if (!engine_scope) return;
    ExecuteCompilationUnits(native_module_, async_counters_.get(), delegate,
                            kBaselineOrTopTier);
  }

  size_t GetMaxConcurrency(size_t worker_count) const override {
    BackgroundCompileScope compile_scope(native_module_);
    if (compile_scope.cancelled()) return 0;
    // NumOutstandingCompilations() does not reflect the units that running
    // workers are processing, thus add the current worker count to that number.
    return std::min(
        static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
        worker_count +
            compile_scope.compilation_state()->NumOutstandingCompilations());
  }

 private:
  std::weak_ptr<NativeModule> native_module_;
  std::shared_ptr<OperationsBarrier> engine_barrier_;
  const std::shared_ptr<Counters> async_counters_;
};

}  // namespace

std::shared_ptr<NativeModule> CompileToNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    std::shared_ptr<const WasmModule> module, const ModuleWireBytes& wire_bytes,
    Handle<FixedArray>* export_wrappers_out, int compilation_id,
    v8::metrics::Recorder::ContextId context_id) {
  const WasmModule* wasm_module = module.get();
  WasmEngine* engine = GetWasmEngine();
  base::OwnedVector<uint8_t> wire_bytes_copy =
      base::OwnedVector<uint8_t>::Of(wire_bytes.module_bytes());
  // Prefer {wire_bytes_copy} to {wire_bytes.module_bytes()} for the temporary
  // cache key. When we eventually install the module in the cache, the wire
  // bytes of the temporary key and the new key have the same base pointer and
  // we can skip the full bytes comparison.
  std::shared_ptr<NativeModule> native_module = engine->MaybeGetNativeModule(
      wasm_module->origin, wire_bytes_copy.as_vector(), isolate);
  if (native_module) {
    // TODO(thibaudm): Look into sharing export wrappers.
    CompileJsToWasmWrappers(isolate, wasm_module, export_wrappers_out);
    return native_module;
  }

  TimedHistogramScope wasm_compile_module_time_scope(SELECT_WASM_COUNTER(
      isolate->counters(), wasm_module->origin, wasm_compile, module_time));

  // Embedder usage count for declared shared memories.
  if (wasm_module->has_shared_memory) {
    isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmSharedMemory);
  }

  // Create a new {NativeModule} first.
  const bool include_liftoff = module->origin == kWasmOrigin && FLAG_liftoff;
  DynamicTiering dynamic_tiering = isolate->IsWasmDynamicTieringEnabled()
                                       ? DynamicTiering::kEnabled
                                       : DynamicTiering::kDisabled;
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          module.get(), include_liftoff, dynamic_tiering);
  native_module =
      engine->NewNativeModule(isolate, enabled, module, code_size_estimate);
  native_module->SetWireBytes(std::move(wire_bytes_copy));
  native_module->compilation_state()->set_compilation_id(compilation_id);
  // Sync compilation is user blocking, so we increase the priority.
  native_module->compilation_state()->SetHighPriority();

  CompileNativeModule(isolate, context_id, thrower, wasm_module, native_module,
                      export_wrappers_out);
  bool cache_hit = !engine->UpdateNativeModuleCache(thrower->error(),
                                                    &native_module, isolate);
  if (thrower->error()) return {};

  if (cache_hit) {
    CompileJsToWasmWrappers(isolate, wasm_module, export_wrappers_out);
    return native_module;
  }

  // Ensure that the code objects are logged before returning.
  engine->LogOutstandingCodesForIsolate(isolate);

  return native_module;
}

void RecompileNativeModule(NativeModule* native_module,
                           TieringState tiering_state) {
  // Install a callback to notify us once background recompilation finished.
  auto recompilation_finished_semaphore = std::make_shared<base::Semaphore>(0);
  auto* compilation_state = Impl(native_module->compilation_state());

  class RecompilationFinishedCallback : public CompilationEventCallback {
   public:
    explicit RecompilationFinishedCallback(
        std::shared_ptr<base::Semaphore> recompilation_finished_semaphore)
        : recompilation_finished_semaphore_(
              std::move(recompilation_finished_semaphore)) {}

    void call(CompilationEvent event) override {
      DCHECK_NE(CompilationEvent::kFailedCompilation, event);
      if (event == CompilationEvent::kFinishedRecompilation) {
        recompilation_finished_semaphore_->Signal();
      }
    }

   private:
    std::shared_ptr<base::Semaphore> recompilation_finished_semaphore_;
  };

  // The callback captures a shared ptr to the semaphore.
  // Initialize the compilation units and kick off background compile tasks.
  compilation_state->InitializeRecompilation(
      tiering_state, std::make_unique<RecompilationFinishedCallback>(
                         recompilation_finished_semaphore));

  constexpr JobDelegate* kNoDelegate = nullptr;
  ExecuteCompilationUnits(compilation_state->native_module_weak(),
                          compilation_state->counters(), kNoDelegate,
                          kBaselineOnly);
  recompilation_finished_semaphore->Wait();
  DCHECK(!compilation_state->failed());
}

AsyncCompileJob::AsyncCompileJob(
    Isolate* isolate, const WasmFeatures& enabled,
    std::unique_ptr<byte[]> bytes_copy, size_t length, Handle<Context> context,
    Handle<Context> incumbent_context, const char* api_method_name,
    std::shared_ptr<CompilationResultResolver> resolver, int compilation_id)
    : isolate_(isolate),
      api_method_name_(api_method_name),
      enabled_features_(enabled),
      dynamic_tiering_(isolate_->IsWasmDynamicTieringEnabled()
                           ? DynamicTiering::kEnabled
                           : DynamicTiering::kDisabled),
      wasm_lazy_compilation_(FLAG_wasm_lazy_compilation),
      start_time_(base::TimeTicks::Now()),
      bytes_copy_(std::move(bytes_copy)),
      wire_bytes_(bytes_copy_.get(), bytes_copy_.get() + length),
      resolver_(std::move(resolver)),
      compilation_id_(compilation_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.AsyncCompileJob");
  CHECK(FLAG_wasm_async_compilation);
  CHECK(!FLAG_jitless);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
  native_context_ =
      isolate->global_handles()->Create(context->native_context());
  incumbent_context_ = isolate->global_handles()->Create(*incumbent_context);
  DCHECK(native_context_->IsNativeContext());
  context_id_ = isolate->GetOrRegisterRecorderContextId(native_context_);
  metrics_event_.async = true;
}

void AsyncCompileJob::Start() {
  DoAsync<DecodeModule>(isolate_->counters(),
                        isolate_->metrics_recorder());  // --
}

void AsyncCompileJob::Abort() {
  // Removing this job will trigger the destructor, which will cancel all
  // compilation.
  GetWasmEngine()->RemoveCompileJob(this);
}

class AsyncStreamingProcessor final : public StreamingProcessor {
 public:
  explicit AsyncStreamingProcessor(AsyncCompileJob* job,
                                   std::shared_ptr<Counters> counters,
                                   AccountingAllocator* allocator);

  ~AsyncStreamingProcessor() override;

  bool ProcessModuleHeader(base::Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  bool ProcessSection(SectionCode section_code,
                      base::Vector<const uint8_t> bytes,
                      uint32_t offset) override;

  bool ProcessCodeSectionHeader(int num_functions,
                                uint32_t functions_mismatch_error_offset,
                                std::shared_ptr<WireBytesStorage>,
                                int code_section_start,
                                int code_section_length) override;

  bool ProcessFunctionBody(base::Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  void OnFinishedChunk() override;

  void OnFinishedStream(base::OwnedVector<uint8_t> bytes) override;

  void OnError(const WasmError&) override;

  void OnAbort() override;

  bool Deserialize(base::Vector<const uint8_t> wire_bytes,
                   base::Vector<const uint8_t> module_bytes) override;

 private:
  // Finishes the AsyncCompileJob with an error.
  void FinishAsyncCompileJobWithError(const WasmError&);

  void CommitCompilationUnits();

  ModuleDecoder decoder_;
  AsyncCompileJob* job_;
  std::unique_ptr<CompilationUnitBuilder> compilation_unit_builder_;
  int num_functions_ = 0;
  bool prefix_cache_hit_ = false;
  bool before_code_section_ = true;
  std::shared_ptr<Counters> async_counters_;
  AccountingAllocator* allocator_;

  // Running hash of the wire bytes up to code section size, but excluding the
  // code section itself. Used by the {NativeModuleCache} to detect potential
  // duplicate modules.
  size_t prefix_hash_;
};

std::shared_ptr<StreamingDecoder> AsyncCompileJob::CreateStreamingDecoder() {
  DCHECK_NULL(stream_);
  stream_ = StreamingDecoder::CreateAsyncStreamingDecoder(
      std::make_unique<AsyncStreamingProcessor>(
          this, isolate_->async_counters(), isolate_->allocator()));
  return stream_;
}

AsyncCompileJob::~AsyncCompileJob() {
  // Note: This destructor always runs on the foreground thread of the isolate.
  background_task_manager_.CancelAndWait();
  // If initial compilation did not finish yet we can abort it.
  if (native_module_) {
    Impl(native_module_->compilation_state())
        ->CancelCompilation(CompilationStateImpl::kCancelInitialCompilation);
  }
  // Tell the streaming decoder that the AsyncCompileJob is not available
  // anymore.
  // TODO(ahaas): Is this notification really necessary? Check
  // https://crbug.com/888170.
  if (stream_) stream_->NotifyCompilationEnded();
  CancelPendingForegroundTask();
  isolate_->global_handles()->Destroy(native_context_.location());
  isolate_->global_handles()->Destroy(incumbent_context_.location());
  if (!module_object_.is_null()) {
    isolate_->global_handles()->Destroy(module_object_.location());
  }
}

void AsyncCompileJob::CreateNativeModule(
    std::shared_ptr<const WasmModule> module, size_t code_size_estimate) {
  // Embedder usage count for declared shared memories.
  if (module->has_shared_memory) {
    isolate_->CountUsage(v8::Isolate::UseCounterFeature::kWasmSharedMemory);
  }

  // TODO(wasm): Improve efficiency of storing module wire bytes. Only store
  // relevant sections, not function bodies

  // Create the module object and populate with compiled functions and
  // information needed at instantiation time.

  native_module_ = GetWasmEngine()->NewNativeModule(
      isolate_, enabled_features_, std::move(module), code_size_estimate);
  native_module_->SetWireBytes({std::move(bytes_copy_), wire_bytes_.length()});
  native_module_->compilation_state()->set_compilation_id(compilation_id_);
}

bool AsyncCompileJob::GetOrCreateNativeModule(
    std::shared_ptr<const WasmModule> module, size_t code_size_estimate) {
  native_module_ = GetWasmEngine()->MaybeGetNativeModule(
      module->origin, wire_bytes_.module_bytes(), isolate_);
  if (native_module_ == nullptr) {
    CreateNativeModule(std::move(module), code_size_estimate);
    return false;
  }
  return true;
}

void AsyncCompileJob::PrepareRuntimeObjects() {
  // Create heap objects for script and module bytes to be stored in the
  // module object. Asm.js is not compiled asynchronously.
  DCHECK(module_object_.is_null());
  auto source_url = stream_ ? stream_->url() : base::Vector<const char>();
  auto script =
      GetWasmEngine()->GetOrCreateScript(isolate_, native_module_, source_url);
  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate_, native_module_, script);

  module_object_ = isolate_->global_handles()->Create(*module_object);
}

// This function assumes that it is executed in a HandleScope, and that a
// context is set on the isolate.
void AsyncCompileJob::FinishCompile(bool is_after_cache_hit) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.FinishAsyncCompile");
  bool is_after_deserialization = !module_object_.is_null();
  auto compilation_state = Impl(native_module_->compilation_state());
  if (!is_after_deserialization) {
    if (stream_) {
      stream_->NotifyNativeModuleCreated(native_module_);
    }
    PrepareRuntimeObjects();
  }

  // Measure duration of baseline compilation or deserialization from cache.
  if (base::TimeTicks::IsHighResolution()) {
    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    int duration_usecs = static_cast<int>(duration.InMicroseconds());
    isolate_->counters()->wasm_streaming_finish_wasm_module_time()->AddSample(
        duration_usecs);

    if (is_after_cache_hit || is_after_deserialization) {
      v8::metrics::WasmModuleCompiled event{
          true,                                     // async
          true,                                     // streamed
          is_after_cache_hit,                       // cached
          is_after_deserialization,                 // deserialized
          wasm_lazy_compilation_,                   // lazy
          !compilation_state->failed(),             // success
          native_module_->turbofan_code_size(),     // code_size_in_bytes
          native_module_->liftoff_bailout_count(),  // liftoff_bailout_count
          duration.InMicroseconds(),                // wall_clock_duration_in_us
          static_cast<int64_t>(                     // cpu_time_duration_in_us
              native_module_->baseline_compilation_cpu_duration())};
      isolate_->metrics_recorder()->DelayMainThreadEvent(event, context_id_);
    }
  }

  DCHECK(!isolate_->context().is_null());
  // Finish the wasm script now and make it public to the debugger.
  Handle<Script> script(module_object_->script(), isolate_);
  const WasmModule* module = module_object_->module();
  if (script->type() == Script::TYPE_WASM &&
      module->debug_symbols.type == WasmDebugSymbols::Type::SourceMap &&
      !module->debug_symbols.external_url.is_empty()) {
    ModuleWireBytes wire_bytes(module_object_->native_module()->wire_bytes());
    MaybeHandle<String> src_map_str = isolate_->factory()->NewStringFromUtf8(
        wire_bytes.GetNameOrNull(module->debug_symbols.external_url),
        AllocationType::kOld);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
                 "wasm.Debug.OnAfterCompile");
    isolate_->debug()->OnAfterCompile(script);
  }

  // TODO(bbudge) Allow deserialization without wrapper compilation, so we can
  // just compile wrappers here.
  if (!is_after_deserialization) {
    Handle<FixedArray> export_wrappers;
    if (is_after_cache_hit) {
      // TODO(thibaudm): Look into sharing wrappers.
      CompileJsToWasmWrappers(isolate_, module, &export_wrappers);
    } else {
      compilation_state->FinalizeJSToWasmWrappers(isolate_, module,
                                                  &export_wrappers);
    }
    module_object_->set_export_wrappers(*export_wrappers);
  }
  // We can only update the feature counts once the entire compile is done.
  compilation_state->PublishDetectedFeatures(isolate_);

  // We might need to recompile the module for debugging, if the debugger was
  // enabled while streaming compilation was running. Since handling this while
  // compiling via streaming is tricky, we just tier down now, before publishing
  // the module.
  if (native_module_->IsTieredDown()) native_module_->RecompileForTiering();

  // Finally, log all generated code (it does not matter if this happens
  // repeatedly in case the script is shared).
  native_module_->LogWasmCodes(isolate_, module_object_->script());

  FinishModule();
}

void AsyncCompileJob::DecodeFailed(const WasmError& error) {
  ErrorThrower thrower(isolate_, api_method_name_);
  thrower.CompileFailed(error);
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      GetWasmEngine()->RemoveCompileJob(this);
  resolver_->OnCompilationFailed(thrower.Reify());
}

void AsyncCompileJob::AsyncCompileFailed() {
  ErrorThrower thrower(isolate_, api_method_name_);
  DCHECK_EQ(native_module_->module()->origin, kWasmOrigin);
  const bool lazy_module = wasm_lazy_compilation_;
  ValidateSequentially(native_module_->module(), native_module_.get(),
                       isolate_->counters(), isolate_->allocator(), &thrower,
                       lazy_module);
  DCHECK(thrower.error());
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      GetWasmEngine()->RemoveCompileJob(this);
  resolver_->OnCompilationFailed(thrower.Reify());
}

void AsyncCompileJob::AsyncCompileSucceeded(Handle<WasmModuleObject> result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.OnCompilationSucceeded");
  // We have to make sure that an "incumbent context" is available in case
  // the module's start function calls out to Blink.
  Local<v8::Context> backup_incumbent_context =
      Utils::ToLocal(incumbent_context_);
  v8::Context::BackupIncumbentScope incumbent(backup_incumbent_context);
  resolver_->OnCompilationSucceeded(result);
}

class AsyncCompileJob::CompilationStateCallback
    : public CompilationEventCallback {
 public:
  explicit CompilationStateCallback(AsyncCompileJob* job) : job_(job) {}

  void call(CompilationEvent event) override {
    // This callback is only being called from a foreground task.
    switch (event) {
      case CompilationEvent::kFinishedExportWrappers:
        // Even if baseline compilation units finish first, we trigger the
        // "kFinishedExportWrappers" event first.
        DCHECK(!last_event_.has_value());
        break;
      case CompilationEvent::kFinishedBaselineCompilation:
        DCHECK_EQ(CompilationEvent::kFinishedExportWrappers, last_event_);
        if (job_->DecrementAndCheckFinisherCount()) {
          // Install the native module in the cache, or reuse a conflicting one.
          // If we get a conflicting module, wait until we are back in the
          // main thread to update {job_->native_module_} to avoid a data race.
          std::shared_ptr<NativeModule> native_module = job_->native_module_;
          bool cache_hit = !GetWasmEngine()->UpdateNativeModuleCache(
              false, &native_module, job_->isolate_);
          DCHECK_EQ(cache_hit, native_module != job_->native_module_);
          job_->DoSync<CompileFinished>(cache_hit ? std::move(native_module)
                                                  : nullptr);
        }
        break;
      case CompilationEvent::kFinishedCompilationChunk:
        DCHECK(CompilationEvent::kFinishedBaselineCompilation == last_event_ ||
               CompilationEvent::kFinishedCompilationChunk == last_event_);
        break;
      case CompilationEvent::kFinishedTopTierCompilation:
        DCHECK(CompilationEvent::kFinishedBaselineCompilation == last_event_);
        // At this point, the job will already be gone, thus do not access it
        // here.
        break;
      case CompilationEvent::kFailedCompilation:
        DCHECK(!last_event_.has_value() ||
               last_event_ == CompilationEvent::kFinishedExportWrappers);
        if (job_->DecrementAndCheckFinisherCount()) {
          // Don't update {job_->native_module_} to avoid data races with other
          // compilation threads. Use a copy of the shared pointer instead.
          std::shared_ptr<NativeModule> native_module = job_->native_module_;
          GetWasmEngine()->UpdateNativeModuleCache(true, &native_module,
                                                   job_->isolate_);
          job_->DoSync<CompileFailed>();
        }
        break;
      case CompilationEvent::kFinishedRecompilation:
        // This event can happen either before or after
        // {kFinishedTopTierCompilation}, hence don't remember this in
        // {last_event_}.
        return;
    }
#ifdef DEBUG
    last_event_ = event;
#endif
  }

 private:
  AsyncCompileJob* job_;
#ifdef DEBUG
  // This will be modified by different threads, but they externally
  // synchronize, so no explicit synchronization (currently) needed here.
  base::Optional<CompilationEvent> last_event_;
#endif
};

// A closure to run a compilation step (either as foreground or background
// task) and schedule the next step(s), if any.
class AsyncCompileJob::CompileStep {
 public:
  virtual ~CompileStep() = default;

  void Run(AsyncCompileJob* job, bool on_foreground) {
    if (on_foreground) {
      HandleScope scope(job->isolate_);
      SaveAndSwitchContext saved_context(job->isolate_, *job->native_context_);
      RunInForeground(job);
    } else {
      RunInBackground(job);
    }
  }

  virtual void RunInForeground(AsyncCompileJob*) { UNREACHABLE(); }
  virtual void RunInBackground(AsyncCompileJob*) { UNREACHABLE(); }
};

class AsyncCompileJob::CompileTask : public CancelableTask {
 public:
  CompileTask(AsyncCompileJob* job, bool on_foreground)
      // We only manage the background tasks with the {CancelableTaskManager} of
      // the {AsyncCompileJob}. Foreground tasks are managed by the system's
      // {CancelableTaskManager}. Background tasks cannot spawn tasks managed by
      // their own task manager.
      : CancelableTask(on_foreground ? job->isolate_->cancelable_task_manager()
                                     : &job->background_task_manager_),
        job_(job),
        on_foreground_(on_foreground) {}

  ~CompileTask() override {
    if (job_ != nullptr && on_foreground_) ResetPendingForegroundTask();
  }

  void RunInternal() final {
    if (!job_) return;
    if (on_foreground_) ResetPendingForegroundTask();
    job_->step_->Run(job_, on_foreground_);
    // After execution, reset {job_} such that we don't try to reset the pending
    // foreground task when the task is deleted.
    job_ = nullptr;
  }

  void Cancel() {
    DCHECK_NOT_NULL(job_);
    job_ = nullptr;
  }

 private:
  // {job_} will be cleared to cancel a pending task.
  AsyncCompileJob* job_;
  bool on_foreground_;

  void ResetPendingForegroundTask() const {
    DCHECK_EQ(this, job_->pending_foreground_task_);
    job_->pending_foreground_task_ = nullptr;
  }
};

void AsyncCompileJob::StartForegroundTask() {
  DCHECK_NULL(pending_foreground_task_);

  auto new_task = std::make_unique<CompileTask>(this, true);
  pending_foreground_task_ = new_task.get();
  foreground_task_runner_->PostTask(std::move(new_task));
}

void AsyncCompileJob::ExecuteForegroundTaskImmediately() {
  DCHECK_NULL(pending_foreground_task_);

  auto new_task = std::make_unique<CompileTask>(this, true);
  pending_foreground_task_ = new_task.get();
  new_task->Run();
}

void AsyncCompileJob::CancelPendingForegroundTask() {
  if (!pending_foreground_task_) return;
  pending_foreground_task_->Cancel();
  pending_foreground_task_ = nullptr;
}

void AsyncCompileJob::StartBackgroundTask() {
  auto task = std::make_unique<CompileTask>(this, false);

  // If --wasm-num-compilation-tasks=0 is passed, do only spawn foreground
  // tasks. This is used to make timing deterministic.
  if (FLAG_wasm_num_compilation_tasks > 0) {
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  } else {
    foreground_task_runner_->PostTask(std::move(task));
  }
}

template <typename Step,
          AsyncCompileJob::UseExistingForegroundTask use_existing_fg_task,
          typename... Args>
void AsyncCompileJob::DoSync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  if (use_existing_fg_task && pending_foreground_task_ != nullptr) return;
  StartForegroundTask();
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoImmediately(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  ExecuteForegroundTaskImmediately();
}

template <typename Step, typename... Args>
void AsyncCompileJob::DoAsync(Args&&... args) {
  NextStep<Step>(std::forward<Args>(args)...);
  StartBackgroundTask();
}

template <typename Step, typename... Args>
void AsyncCompileJob::NextStep(Args&&... args) {
  step_.reset(new Step(std::forward<Args>(args)...));
}

//==========================================================================
// Step 1: (async) Decode the module.
//==========================================================================
class AsyncCompileJob::DecodeModule : public AsyncCompileJob::CompileStep {
 public:
  explicit DecodeModule(Counters* counters,
                        std::shared_ptr<metrics::Recorder> metrics_recorder)
      : counters_(counters), metrics_recorder_(std::move(metrics_recorder)) {}

  void RunInBackground(AsyncCompileJob* job) override {
    ModuleResult result;
    {
      DisallowHandleAllocation no_handle;
      DisallowGarbageCollection no_gc;
      // Decode the module bytes.
      TRACE_COMPILE("(1) Decoding module...\n");
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
                   "wasm.DecodeModule");
      auto enabled_features = job->enabled_features_;
      result = DecodeWasmModule(
          enabled_features, job->wire_bytes_.start(), job->wire_bytes_.end(),
          false, kWasmOrigin, counters_, metrics_recorder_, job->context_id(),
          DecodingMethod::kAsync, GetWasmEngine()->allocator());

      // Validate lazy functions here if requested.
      if (!FLAG_wasm_lazy_validation && result.ok()) {
        const WasmModule* module = result.value().get();
        DCHECK_EQ(module->origin, kWasmOrigin);
        const bool lazy_module = job->wasm_lazy_compilation_;
        if (MayCompriseLazyFunctions(module, enabled_features, lazy_module)) {
          auto allocator = GetWasmEngine()->allocator();
          int start = module->num_imported_functions;
          int end = start + module->num_declared_functions;

          for (int func_index = start; func_index < end; func_index++) {
            const WasmFunction* func = &module->functions[func_index];
            base::Vector<const uint8_t> code =
                job->wire_bytes_.GetFunctionBytes(func);

            CompileStrategy strategy = GetCompileStrategy(
                module, enabled_features, func_index, lazy_module);
            bool validate_lazily_compiled_function =
                strategy == CompileStrategy::kLazy ||
                strategy == CompileStrategy::kLazyBaselineEagerTopTier;
            if (validate_lazily_compiled_function) {
              DecodeResult function_result =
                  ValidateSingleFunction(module, func_index, code, counters_,
                                         allocator, enabled_features);
              if (function_result.failed()) {
                result = ModuleResult(function_result.error());
                break;
              }
            }
          }
        }
      }
    }
    if (result.failed()) {
      // Decoding failure; reject the promise and clean up.
      job->DoSync<DecodeFail>(std::move(result).error());
    } else {
      // Decode passed.
      std::shared_ptr<WasmModule> module = std::move(result).value();
      const bool include_liftoff = FLAG_liftoff;
      size_t code_size_estimate =
          wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
              module.get(), include_liftoff, job->dynamic_tiering_);
      job->DoSync<PrepareAndStartCompile>(std::move(module), true,
                                          code_size_estimate);
    }
  }

 private:
  Counters* const counters_;
  std::shared_ptr<metrics::Recorder> metrics_recorder_;
};

//==========================================================================
// Step 1b: (sync) Fail decoding the module.
//==========================================================================
class AsyncCompileJob::DecodeFail : public CompileStep {
 public:
  explicit DecodeFail(WasmError error) : error_(std::move(error)) {}

 private:
  WasmError error_;

  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(1b) Decoding failed.\n");
    // {job_} is deleted in DecodeFailed, therefore the {return}.
    return job->DecodeFailed(error_);
  }
};

//==========================================================================
// Step 2 (sync): Create heap-allocated data and start compile.
//==========================================================================
class AsyncCompileJob::PrepareAndStartCompile : public CompileStep {
 public:
  PrepareAndStartCompile(std::shared_ptr<const WasmModule> module,
                         bool start_compilation, size_t code_size_estimate)
      : module_(std::move(module)),
        start_compilation_(start_compilation),
        code_size_estimate_(code_size_estimate) {}

 private:
  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(2) Prepare and start compile...\n");

    const bool streaming = job->wire_bytes_.length() == 0;
    if (streaming) {
      // Streaming compilation already checked for cache hits.
      job->CreateNativeModule(module_, code_size_estimate_);
    } else if (job->GetOrCreateNativeModule(std::move(module_),
                                            code_size_estimate_)) {
      job->FinishCompile(true);
      return;
    }

    // Make sure all compilation tasks stopped running. Decoding (async step)
    // is done.
    job->background_task_manager_.CancelAndWait();

    CompilationStateImpl* compilation_state =
        Impl(job->native_module_->compilation_state());
    compilation_state->AddCallback(
        std::make_unique<CompilationStateCallback>(job));
    if (base::TimeTicks::IsHighResolution()) {
      auto compile_mode = job->stream_ == nullptr
                              ? CompilationTimeCallback::kAsync
                              : CompilationTimeCallback::kStreaming;
      compilation_state->AddCallback(std::make_unique<CompilationTimeCallback>(
          job->isolate_->async_counters(), job->isolate_->metrics_recorder(),
          job->context_id_, job->native_module_, compile_mode));
    }

    if (start_compilation_) {
      std::unique_ptr<CompilationUnitBuilder> builder =
          InitializeCompilation(job->isolate(), job->native_module_.get());
      compilation_state->InitializeCompilationUnits(std::move(builder));
      // We are in single-threaded mode, so there are no worker tasks that will
      // do the compilation. We call {WaitForCompilationEvent} here so that the
      // main thread paticipates and finishes the compilation.
      if (FLAG_wasm_num_compilation_tasks == 0) {
        compilation_state->WaitForCompilationEvent(
            CompilationEvent::kFinishedBaselineCompilation);
      }
    }
  }

  const std::shared_ptr<const WasmModule> module_;
  const bool start_compilation_;
  const size_t code_size_estimate_;
};

//==========================================================================
// Step 3a (sync): Compilation failed.
//==========================================================================
class AsyncCompileJob::CompileFailed : public CompileStep {
 private:
  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(3a) Compilation failed\n");
    DCHECK(job->native_module_->compilation_state()->failed());

    // {job_} is deleted in AsyncCompileFailed, therefore the {return}.
    return job->AsyncCompileFailed();
  }
};

namespace {
class SampleTopTierCodeSizeCallback : public CompilationEventCallback {
 public:
  explicit SampleTopTierCodeSizeCallback(
      std::weak_ptr<NativeModule> native_module)
      : native_module_(std::move(native_module)) {}

  void call(CompilationEvent event) override {
    if (event != CompilationEvent::kFinishedTopTierCompilation) return;
    if (std::shared_ptr<NativeModule> native_module = native_module_.lock()) {
      GetWasmEngine()->SampleTopTierCodeSizeInAllIsolates(native_module);
    }
  }

 private:
  std::weak_ptr<NativeModule> native_module_;
};
}  // namespace

//==========================================================================
// Step 3b (sync): Compilation finished.
//==========================================================================
class AsyncCompileJob::CompileFinished : public CompileStep {
 public:
  explicit CompileFinished(std::shared_ptr<NativeModule> cached_native_module)
      : cached_native_module_(std::move(cached_native_module)) {}

 private:
  void RunInForeground(AsyncCompileJob* job) override {
    TRACE_COMPILE("(3b) Compilation finished\n");
    if (cached_native_module_) {
      job->native_module_ = cached_native_module_;
    } else {
      DCHECK(!job->native_module_->compilation_state()->failed());
      // Sample the generated code size when baseline compilation finished.
      job->native_module_->SampleCodeSize(job->isolate_->counters(),
                                          NativeModule::kAfterBaseline);
      // Also, set a callback to sample the code size after top-tier compilation
      // finished. This callback will *not* keep the NativeModule alive.
      job->native_module_->compilation_state()->AddCallback(
          std::make_unique<SampleTopTierCodeSizeCallback>(job->native_module_));
    }
    // Then finalize and publish the generated module.
    job->FinishCompile(cached_native_module_ != nullptr);
  }

  std::shared_ptr<NativeModule> cached_native_module_;
};

void AsyncCompileJob::FinishModule() {
  TRACE_COMPILE("(4) Finish module...\n");
  AsyncCompileSucceeded(module_object_);
  GetWasmEngine()->RemoveCompileJob(this);
}

AsyncStreamingProcessor::AsyncStreamingProcessor(
    AsyncCompileJob* job, std::shared_ptr<Counters> async_counters,
    AccountingAllocator* allocator)
    : decoder_(job->enabled_features_),
      job_(job),
      compilation_unit_builder_(nullptr),
      async_counters_(async_counters),
      allocator_(allocator) {}

AsyncStreamingProcessor::~AsyncStreamingProcessor() {
  if (job_->native_module_ && job_->native_module_->wire_bytes().empty()) {
    // Clean up the temporary cache entry.
    GetWasmEngine()->StreamingCompilationFailed(prefix_hash_);
  }
}

void AsyncStreamingProcessor::FinishAsyncCompileJobWithError(
    const WasmError& error) {
  DCHECK(error.has_error());
  // Make sure all background tasks stopped executing before we change the state
  // of the AsyncCompileJob to DecodeFail.
  job_->background_task_manager_.CancelAndWait();

  // Record event metrics.
  auto duration = base::TimeTicks::Now() - job_->start_time_;
  job_->metrics_event_.success = false;
  job_->metrics_event_.streamed = true;
  job_->metrics_event_.module_size_in_bytes = job_->wire_bytes_.length();
  job_->metrics_event_.function_count = num_functions_;
  job_->metrics_event_.wall_clock_duration_in_us = duration.InMicroseconds();
  job_->isolate_->metrics_recorder()->DelayMainThreadEvent(job_->metrics_event_,
                                                           job_->context_id_);

  // Check if there is already a CompiledModule, in which case we have to clean
  // up the CompilationStateImpl as well.
  if (job_->native_module_) {
    Impl(job_->native_module_->compilation_state())
        ->CancelCompilation(CompilationStateImpl::kCancelUnconditionally);

    job_->DoSync<AsyncCompileJob::DecodeFail,
                 AsyncCompileJob::kUseExistingForegroundTask>(error);

    // Clear the {compilation_unit_builder_} if it exists. This is needed
    // because there is a check in the destructor of the
    // {CompilationUnitBuilder} that it is empty.
    if (compilation_unit_builder_) compilation_unit_builder_->Clear();
  } else {
    job_->DoSync<AsyncCompileJob::DecodeFail>(error);
  }
}

// Process the module header.
bool AsyncStreamingProcessor::ProcessModuleHeader(
    base::Vector<const uint8_t> bytes, uint32_t offset) {
  TRACE_STREAMING("Process module header...\n");
  decoder_.StartDecoding(job_->isolate()->counters(),
                         job_->isolate()->metrics_recorder(),
                         job_->context_id(), GetWasmEngine()->allocator());
  decoder_.DecodeModuleHeader(bytes, offset);
  if (!decoder_.ok()) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
    return false;
  }
  prefix_hash_ = NativeModuleCache::WireBytesHash(bytes);
  return true;
}

// Process all sections except for the code section.
bool AsyncStreamingProcessor::ProcessSection(SectionCode section_code,
                                             base::Vector<const uint8_t> bytes,
                                             uint32_t offset) {
  TRACE_STREAMING("Process section %d ...\n", section_code);
  if (compilation_unit_builder_) {
    // We reached a section after the code section, we do not need the
    // compilation_unit_builder_ anymore.
    CommitCompilationUnits();
    compilation_unit_builder_.reset();
  }
  if (before_code_section_) {
    // Combine section hashes until code section.
    prefix_hash_ = base::hash_combine(prefix_hash_,
                                      NativeModuleCache::WireBytesHash(bytes));
  }
  if (section_code == SectionCode::kUnknownSectionCode) {
    size_t bytes_consumed = ModuleDecoder::IdentifyUnknownSection(
        &decoder_, bytes, offset, &section_code);
    if (!decoder_.ok()) {
      FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
      return false;
    }
    if (section_code == SectionCode::kUnknownSectionCode) {
      // Skip unknown sections that we do not know how to handle.
      return true;
    }
    // Remove the unknown section tag from the payload bytes.
    offset += bytes_consumed;
    bytes = bytes.SubVector(bytes_consumed, bytes.size());
  }
  constexpr bool verify_functions = false;
  decoder_.DecodeSection(section_code, bytes, offset, verify_functions);
  if (!decoder_.ok()) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
    return false;
  }
  return true;
}

// Start the code section.
bool AsyncStreamingProcessor::ProcessCodeSectionHeader(
    int num_functions, uint32_t functions_mismatch_error_offset,
    std::shared_ptr<WireBytesStorage> wire_bytes_storage,
    int code_section_start, int code_section_length) {
  DCHECK_LE(0, code_section_length);
  before_code_section_ = false;
  TRACE_STREAMING("Start the code section with %d functions...\n",
                  num_functions);
  decoder_.StartCodeSection();
  if (!decoder_.CheckFunctionsCount(static_cast<uint32_t>(num_functions),
                                    functions_mismatch_error_offset)) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
    return false;
  }

  decoder_.set_code_section(code_section_start,
                            static_cast<uint32_t>(code_section_length));

  prefix_hash_ = base::hash_combine(prefix_hash_,
                                    static_cast<uint32_t>(code_section_length));
  if (!GetWasmEngine()->GetStreamingCompilationOwnership(prefix_hash_)) {
    // Known prefix, wait until the end of the stream and check the cache.
    prefix_cache_hit_ = true;
    return true;
  }

  // Execute the PrepareAndStartCompile step immediately and not in a separate
  // task.
  int num_imported_functions =
      static_cast<int>(decoder_.module()->num_imported_functions);
  DCHECK_EQ(kWasmOrigin, decoder_.module()->origin);
  const bool include_liftoff = FLAG_liftoff;
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          num_functions, num_imported_functions, code_section_length,
          include_liftoff, job_->dynamic_tiering_);
  job_->DoImmediately<AsyncCompileJob::PrepareAndStartCompile>(
      decoder_.shared_module(), false, code_size_estimate);

  auto* compilation_state = Impl(job_->native_module_->compilation_state());
  compilation_state->SetWireBytesStorage(std::move(wire_bytes_storage));
  DCHECK_EQ(job_->native_module_->module()->origin, kWasmOrigin);

  // Set outstanding_finishers_ to 2, because both the AsyncCompileJob and the
  // AsyncStreamingProcessor have to finish.
  job_->outstanding_finishers_.store(2);
  compilation_unit_builder_ =
      InitializeCompilation(job_->isolate(), job_->native_module_.get());
  return true;
}

// Process a function body.
bool AsyncStreamingProcessor::ProcessFunctionBody(
    base::Vector<const uint8_t> bytes, uint32_t offset) {
  TRACE_STREAMING("Process function body %d ...\n", num_functions_);

  decoder_.DecodeFunctionBody(
      num_functions_, static_cast<uint32_t>(bytes.length()), offset, false);

  const WasmModule* module = decoder_.module();
  auto enabled_features = job_->enabled_features_;
  uint32_t func_index =
      num_functions_ + decoder_.module()->num_imported_functions;
  DCHECK_EQ(module->origin, kWasmOrigin);
  const bool lazy_module = job_->wasm_lazy_compilation_;
  CompileStrategy strategy =
      GetCompileStrategy(module, enabled_features, func_index, lazy_module);
  bool validate_lazily_compiled_function =
      !FLAG_wasm_lazy_validation &&
      (strategy == CompileStrategy::kLazy ||
       strategy == CompileStrategy::kLazyBaselineEagerTopTier);
  if (validate_lazily_compiled_function) {
    // The native module does not own the wire bytes until {SetWireBytes} is
    // called in {OnFinishedStream}. Validation must use {bytes} parameter.
    DecodeResult result =
        ValidateSingleFunction(module, func_index, bytes, async_counters_.get(),
                               allocator_, enabled_features);

    if (result.failed()) {
      FinishAsyncCompileJobWithError(result.error());
      return false;
    }
  }

  // Don't compile yet if we might have a cache hit.
  if (prefix_cache_hit_) {
    num_functions_++;
    return true;
  }

  auto* compilation_state = Impl(job_->native_module_->compilation_state());
  compilation_state->AddCompilationUnit(compilation_unit_builder_.get(),
                                        func_index);
  ++num_functions_;

  return true;
}

void AsyncStreamingProcessor::CommitCompilationUnits() {
  DCHECK(compilation_unit_builder_);
  compilation_unit_builder_->Commit();
}

void AsyncStreamingProcessor::OnFinishedChunk() {
  TRACE_STREAMING("FinishChunk...\n");
  if (compilation_unit_builder_) CommitCompilationUnits();
}

// Finish the processing of the stream.
void AsyncStreamingProcessor::OnFinishedStream(
    base::OwnedVector<uint8_t> bytes) {
  TRACE_STREAMING("Finish stream...\n");
  DCHECK_EQ(NativeModuleCache::PrefixHash(bytes.as_vector()), prefix_hash_);
  ModuleResult result = decoder_.FinishDecoding(false);
  if (result.failed()) {
    FinishAsyncCompileJobWithError(result.error());
    return;
  }

  job_->wire_bytes_ = ModuleWireBytes(bytes.as_vector());
  job_->bytes_copy_ = bytes.ReleaseData();

  // Record event metrics.
  auto duration = base::TimeTicks::Now() - job_->start_time_;
  job_->metrics_event_.success = true;
  job_->metrics_event_.streamed = true;
  job_->metrics_event_.module_size_in_bytes = job_->wire_bytes_.length();
  job_->metrics_event_.function_count = num_functions_;
  job_->metrics_event_.wall_clock_duration_in_us = duration.InMicroseconds();
  job_->isolate_->metrics_recorder()->DelayMainThreadEvent(job_->metrics_event_,
                                                           job_->context_id_);

  if (prefix_cache_hit_) {
    // Restart as an asynchronous, non-streaming compilation. Most likely
    // {PrepareAndStartCompile} will get the native module from the cache.
    const bool include_liftoff = FLAG_liftoff;
    size_t code_size_estimate =
        wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
            result.value().get(), include_liftoff, job_->dynamic_tiering_);
    job_->DoSync<AsyncCompileJob::PrepareAndStartCompile>(
        std::move(result).value(), true, code_size_estimate);
    return;
  }

  // We have to open a HandleScope and prepare the Context for
  // CreateNativeModule, PrepareRuntimeObjects and FinishCompile as this is a
  // callback from the embedder.
  HandleScope scope(job_->isolate_);
  SaveAndSwitchContext saved_context(job_->isolate_, *job_->native_context_);

  // Record the size of the wire bytes. In synchronous and asynchronous
  // (non-streaming) compilation, this happens in {DecodeWasmModule}.
  auto* histogram = job_->isolate_->counters()->wasm_wasm_module_size_bytes();
  histogram->AddSample(job_->wire_bytes_.module_bytes().length());

  const bool has_code_section = job_->native_module_ != nullptr;
  bool cache_hit = false;
  if (!has_code_section) {
    // We are processing a WebAssembly module without code section. Create the
    // native module now (would otherwise happen in {PrepareAndStartCompile} or
    // {ProcessCodeSectionHeader}).
    constexpr size_t kCodeSizeEstimate = 0;
    cache_hit = job_->GetOrCreateNativeModule(std::move(result).value(),
                                              kCodeSizeEstimate);
  } else {
    job_->native_module_->SetWireBytes(
        {std::move(job_->bytes_copy_), job_->wire_bytes_.length()});
  }
  const bool needs_finish = job_->DecrementAndCheckFinisherCount();
  DCHECK_IMPLIES(!has_code_section, needs_finish);
  if (needs_finish) {
    const bool failed = job_->native_module_->compilation_state()->failed();
    if (!cache_hit) {
      cache_hit = !GetWasmEngine()->UpdateNativeModuleCache(
          failed, &job_->native_module_, job_->isolate_);
    }
    if (failed) {
      job_->AsyncCompileFailed();
    } else {
      job_->FinishCompile(cache_hit);
    }
  }
}

// Report an error detected in the StreamingDecoder.
void AsyncStreamingProcessor::OnError(const WasmError& error) {
  TRACE_STREAMING("Stream error...\n");
  FinishAsyncCompileJobWithError(error);
}

void AsyncStreamingProcessor::OnAbort() {
  TRACE_STREAMING("Abort stream...\n");
  job_->Abort();
}

bool AsyncStreamingProcessor::Deserialize(
    base::Vector<const uint8_t> module_bytes,
    base::Vector<const uint8_t> wire_bytes) {
  TRACE_EVENT0("v8.wasm", "wasm.Deserialize");
  TimedHistogramScope time_scope(
      job_->isolate()->counters()->wasm_deserialization_time(),
      job_->isolate());
  // DeserializeNativeModule and FinishCompile assume that they are executed in
  // a HandleScope, and that a context is set on the isolate.
  HandleScope scope(job_->isolate_);
  SaveAndSwitchContext saved_context(job_->isolate_, *job_->native_context_);

  MaybeHandle<WasmModuleObject> result = DeserializeNativeModule(
      job_->isolate_, module_bytes, wire_bytes, job_->stream_->url());

  if (result.is_null()) return false;

  job_->module_object_ =
      job_->isolate_->global_handles()->Create(*result.ToHandleChecked());
  job_->native_module_ = job_->module_object_->shared_native_module();
  job_->wire_bytes_ = ModuleWireBytes(job_->native_module_->wire_bytes());
  job_->FinishCompile(false);
  return true;
}

CompilationStateImpl::CompilationStateImpl(
    const std::shared_ptr<NativeModule>& native_module,
    std::shared_ptr<Counters> async_counters, DynamicTiering dynamic_tiering)
    : native_module_(native_module.get()),
      native_module_weak_(std::move(native_module)),
      async_counters_(std::move(async_counters)),
      compilation_unit_queues_(native_module->num_functions()),
      dynamic_tiering_(dynamic_tiering) {}

void CompilationStateImpl::InitCompileJob() {
  DCHECK_NULL(compile_job_);
  compile_job_ = V8::GetCurrentPlatform()->PostJob(
      TaskPriority::kUserVisible, std::make_unique<BackgroundCompileJob>(
                                      native_module_weak_, async_counters_));
}

void CompilationStateImpl::CancelCompilation(
    CompilationStateImpl::CancellationPolicy cancellation_policy) {
  base::MutexGuard callbacks_guard(&callbacks_mutex_);

  if (cancellation_policy == kCancelInitialCompilation &&
      finished_events_.contains(
          CompilationEvent::kFinishedBaselineCompilation)) {
    // Initial compilation already finished; cannot be cancelled.
    return;
  }

  // std::memory_order_relaxed is sufficient because no other state is
  // synchronized with |compile_cancelled_|.
  compile_cancelled_.store(true, std::memory_order_relaxed);

  // No more callbacks after abort.
  callbacks_.clear();
}

bool CompilationStateImpl::cancelled() const {
  return compile_cancelled_.load(std::memory_order_relaxed);
}

uint8_t CompilationStateImpl::SetupCompilationProgressForFunction(
    bool lazy_function, NativeModule* native_module,
    const WasmFeatures& enabled_features, int func_index) {
  ExecutionTierPair requested_tiers =
      GetRequestedExecutionTiers(native_module, enabled_features, func_index);
  CompileStrategy strategy = GetCompileStrategy(
      native_module->module(), enabled_features, func_index, lazy_function);

  bool required_for_baseline = strategy == CompileStrategy::kEager;
  bool required_for_top_tier = strategy != CompileStrategy::kLazy;
  DCHECK_EQ(required_for_top_tier,
            strategy == CompileStrategy::kEager ||
                strategy == CompileStrategy::kLazyBaselineEagerTopTier);

  // Count functions to complete baseline and top tier compilation.
  if (required_for_baseline) outstanding_baseline_units_++;
  if (required_for_top_tier) outstanding_top_tier_functions_++;

  // Initialize function's compilation progress.
  ExecutionTier required_baseline_tier = required_for_baseline
                                             ? requested_tiers.baseline_tier
                                             : ExecutionTier::kNone;
  ExecutionTier required_top_tier =
      required_for_top_tier ? requested_tiers.top_tier : ExecutionTier::kNone;
  uint8_t function_progress =
      ReachedTierField::encode(ExecutionTier::kNone) |
      RequiredBaselineTierField::encode(required_baseline_tier) |
      RequiredTopTierField::encode(required_top_tier);

  return function_progress;
}

void CompilationStateImpl::InitializeCompilationProgress(
    bool lazy_module, int num_import_wrappers, int num_export_wrappers) {
  DCHECK(!failed());
  auto enabled_features = native_module_->enabled_features();
  auto* module = native_module_->module();

  base::MutexGuard guard(&callbacks_mutex_);
  DCHECK_EQ(0, outstanding_baseline_units_);
  DCHECK_EQ(0, outstanding_export_wrappers_);
  DCHECK_EQ(0, outstanding_top_tier_functions_);
  compilation_progress_.reserve(module->num_declared_functions);
  int start = module->num_imported_functions;
  int end = start + module->num_declared_functions;

  const bool prefer_liftoff = native_module_->IsTieredDown();
  for (int func_index = start; func_index < end; func_index++) {
    if (prefer_liftoff) {
      constexpr uint8_t kLiftoffOnlyFunctionProgress =
          RequiredTopTierField::encode(ExecutionTier::kLiftoff) |
          RequiredBaselineTierField::encode(ExecutionTier::kLiftoff) |
          ReachedTierField::encode(ExecutionTier::kNone);
      compilation_progress_.push_back(kLiftoffOnlyFunctionProgress);
      outstanding_baseline_units_++;
      outstanding_top_tier_functions_++;
      continue;
    }
    uint8_t function_progress = SetupCompilationProgressForFunction(
        lazy_module, native_module_, enabled_features, func_index);
    compilation_progress_.push_back(function_progress);
  }
  DCHECK_IMPLIES(lazy_module, outstanding_baseline_units_ == 0);
  DCHECK_IMPLIES(lazy_module, outstanding_top_tier_functions_ == 0);
  DCHECK_LE(0, outstanding_baseline_units_);
  DCHECK_LE(outstanding_baseline_units_, outstanding_top_tier_functions_);
  outstanding_baseline_units_ += num_import_wrappers;
  outstanding_export_wrappers_ = num_export_wrappers;

  // Trigger callbacks if module needs no baseline or top tier compilation. This
  // can be the case for an empty or fully lazy module.
  TriggerCallbacks();
}

uint8_t CompilationStateImpl::AddCompilationUnitInternal(
    CompilationUnitBuilder* builder, int function_index,
    uint8_t function_progress) {
  ExecutionTier required_baseline_tier =
      CompilationStateImpl::RequiredBaselineTierField::decode(
          function_progress);
  ExecutionTier required_top_tier =
      CompilationStateImpl::RequiredTopTierField::decode(function_progress);
  ExecutionTier reached_tier =
      CompilationStateImpl::ReachedTierField::decode(function_progress);

  if (FLAG_experimental_wasm_gc) {
    // The Turbofan optimizations we enable for WasmGC code can (for now)
    // take a very long time, so skip Turbofan compilation for super-large
    // functions.
    // Besides, module serialization currently requires that all functions
    // have been TF-compiled. By enabling this limit only for WasmGC, we
    // make sure that non-experimental modules can be serialize as usual.
    // TODO(jkummerow): This is a stop-gap solution to avoid excessive
    // compile times. We would like to replace this hard threshold with
    // a better solution (TBD) eventually.
    constexpr uint32_t kMaxWasmFunctionSizeForTurbofan = 500 * KB;
    uint32_t size = builder->module()->functions[function_index].code.length();
    if (size > kMaxWasmFunctionSizeForTurbofan) {
      required_baseline_tier = ExecutionTier::kLiftoff;
      if (required_top_tier == ExecutionTier::kTurbofan) {
        required_top_tier = ExecutionTier::kLiftoff;
        outstanding_top_tier_functions_--;
      }
    }
  }

  if (reached_tier < required_baseline_tier) {
    builder->AddBaselineUnit(function_index, required_baseline_tier);
  }
  if (reached_tier < required_top_tier &&
      required_baseline_tier != required_top_tier) {
    builder->AddTopTierUnit(function_index, required_top_tier);
  }
  return CompilationStateImpl::RequiredBaselineTierField::encode(
             required_baseline_tier) |
         CompilationStateImpl::RequiredTopTierField::encode(required_top_tier) |
         CompilationStateImpl::ReachedTierField::encode(reached_tier);
}

void CompilationStateImpl::InitializeCompilationUnits(
    std::unique_ptr<CompilationUnitBuilder> builder) {
  int offset = native_module_->module()->num_imported_functions;
  if (native_module_->IsTieredDown()) {
    for (size_t i = 0; i < compilation_progress_.size(); ++i) {
      int func_index = offset + static_cast<int>(i);
      builder->AddDebugUnit(func_index);
    }
  } else {
    base::MutexGuard guard(&callbacks_mutex_);

    for (size_t i = 0; i < compilation_progress_.size(); ++i) {
      uint8_t function_progress = compilation_progress_[i];
      int func_index = offset + static_cast<int>(i);
      compilation_progress_[i] = AddCompilationUnitInternal(
          builder.get(), func_index, function_progress);
    }
  }
  builder->Commit();
}

void CompilationStateImpl::AddCompilationUnit(CompilationUnitBuilder* builder,
                                              int func_index) {
  if (native_module_->IsTieredDown()) {
    builder->AddDebugUnit(func_index);
    return;
  }
  int offset = native_module_->module()->num_imported_functions;
  int progress_index = func_index - offset;
  uint8_t function_progress;
  {
    // TODO(ahaas): This lock may cause overhead. If so, we could get rid of the
    // lock as follows:
    // 1) Make compilation_progress_ an array of atomic<uint8_t>, and access it
    // lock-free.
    // 2) Have a copy of compilation_progress_ that we use for initialization.
    // 3) Just re-calculate the content of compilation_progress_.
    base::MutexGuard guard(&callbacks_mutex_);
    function_progress = compilation_progress_[progress_index];
  }
  uint8_t updated_function_progress =
      AddCompilationUnitInternal(builder, func_index, function_progress);
  if (updated_function_progress != function_progress) {
    // This should happen very rarely (only for super-large functions), so we're
    // not worried about overhead.
    base::MutexGuard guard(&callbacks_mutex_);
    compilation_progress_[progress_index] = updated_function_progress;
  }
}

void CompilationStateImpl::InitializeCompilationProgressAfterDeserialization(
    base::Vector<const int> lazy_functions,
    base::Vector<const int> liftoff_functions) {
  TRACE_EVENT2("v8.wasm", "wasm.CompilationAfterDeserialization",
               "num_lazy_functions", lazy_functions.size(),
               "num_liftoff_functions", liftoff_functions.size());
  TimedHistogramScope lazy_compile_time_scope(
      counters()->wasm_compile_after_deserialize());

  auto* module = native_module_->module();
  auto enabled_features = native_module_->enabled_features();
  const bool lazy_module = IsLazyModule(module);
  base::Optional<CodeSpaceWriteScope> lazy_code_space_write_scope;
  if (lazy_module || !lazy_functions.empty()) {
    lazy_code_space_write_scope.emplace(native_module_);
  }
  {
    base::MutexGuard guard(&callbacks_mutex_);
    DCHECK(compilation_progress_.empty());
    constexpr uint8_t kProgressAfterTurbofanDeserialization =
        RequiredBaselineTierField::encode(ExecutionTier::kTurbofan) |
        RequiredTopTierField::encode(ExecutionTier::kTurbofan) |
        ReachedTierField::encode(ExecutionTier::kTurbofan);
    finished_events_.Add(CompilationEvent::kFinishedExportWrappers);
    if (liftoff_functions.empty() || lazy_module) {
      // We have to trigger the compilation events to finish compilation.
      // Typically the events get triggered when a CompilationUnit finishes, but
      // with lazy compilation there are no compilation units.
      // The {kFinishedBaselineCompilation} event is needed for module
      // compilation to finish.
      finished_events_.Add(CompilationEvent::kFinishedBaselineCompilation);
      if (liftoff_functions.empty() && lazy_functions.empty()) {
        // All functions exist now as TurboFan functions, so we can trigger the
        // {kFinishedTopTierCompilation} event.
        // The {kFinishedTopTierCompilation} event is needed for the C-API so
        // that {serialize()} works after {deserialize()}.
        finished_events_.Add(CompilationEvent::kFinishedTopTierCompilation);
      }
    }
    compilation_progress_.assign(module->num_declared_functions,
                                 kProgressAfterTurbofanDeserialization);
    for (auto func_index : lazy_functions) {
      native_module_->UseLazyStub(func_index);

      compilation_progress_[declared_function_index(module, func_index)] =
          SetupCompilationProgressForFunction(/*lazy_function =*/true,
                                              native_module_, enabled_features,
                                              func_index);
    }
    for (auto func_index : liftoff_functions) {
      if (lazy_module) {
        native_module_->UseLazyStub(func_index);
      }
      // Check that {func_index} is not contained in {lazy_functions}.
      DCHECK_EQ(
          compilation_progress_[declared_function_index(module, func_index)],
          kProgressAfterTurbofanDeserialization);
      compilation_progress_[declared_function_index(module, func_index)] =
          SetupCompilationProgressForFunction(lazy_module, native_module_,
                                              enabled_features, func_index);
    }
  }
  auto builder = std::make_unique<CompilationUnitBuilder>(native_module_);
  InitializeCompilationUnits(std::move(builder));
  WaitForCompilationEvent(CompilationEvent::kFinishedBaselineCompilation);
}

void CompilationStateImpl::InitializeRecompilation(
    TieringState new_tiering_state,
    std::unique_ptr<CompilationEventCallback> recompilation_finished_callback) {
  DCHECK(!failed());

  // Hold the mutex as long as possible, to synchronize between multiple
  // recompilations that are triggered at the same time (e.g. when the profiler
  // is disabled).
  base::Optional<base::MutexGuard> guard(&callbacks_mutex_);

  // As long as there are outstanding recompilation functions, take part in
  // compilation. This is to avoid recompiling for the same tier or for
  // different tiers concurrently. Note that the compilation unit queues can run
  // empty before {outstanding_recompilation_functions_} drops to zero. In this
  // case, we do not wait for the last running compilation threads to finish
  // their units, but just start our own recompilation already.
  while (outstanding_recompilation_functions_ > 0 &&
         compilation_unit_queues_.GetTotalSize() > 0) {
    guard.reset();
    constexpr JobDelegate* kNoDelegate = nullptr;
    ExecuteCompilationUnits(native_module_weak_, async_counters_.get(),
                            kNoDelegate, kBaselineOrTopTier);
    guard.emplace(&callbacks_mutex_);
  }

  // Information about compilation progress is shared between this class and the
  // NativeModule. Before updating information here, consult the NativeModule to
  // find all functions that need recompilation.
  // Since the current tiering state is updated on the NativeModule before
  // triggering recompilation, it's OK if the information is slightly outdated.
  // If we compile functions twice, the NativeModule will ignore all redundant
  // code (or code compiled for the wrong tier).
  std::vector<int> recompile_function_indexes =
      native_module_->FindFunctionsToRecompile(new_tiering_state);

  callbacks_.emplace_back(std::move(recompilation_finished_callback));
  tiering_state_ = new_tiering_state;

  // If compilation progress is not initialized yet, then compilation didn't
  // start yet, and new code will be kept tiered-down from the start. For
  // streaming compilation, there is a special path to tier down later, when
  // the module is complete. In any case, we don't need to recompile here.
  base::Optional<CompilationUnitBuilder> builder;
  if (compilation_progress_.size() > 0) {
    builder.emplace(native_module_);
    const WasmModule* module = native_module_->module();
    DCHECK_EQ(module->num_declared_functions, compilation_progress_.size());
    DCHECK_GE(module->num_declared_functions,
              recompile_function_indexes.size());
    outstanding_recompilation_functions_ =
        static_cast<int>(recompile_function_indexes.size());
    // Restart recompilation if another recompilation is already happening.
    for (auto& progress : compilation_progress_) {
      progress = MissingRecompilationField::update(progress, false);
    }
    auto new_tier = new_tiering_state == kTieredDown ? ExecutionTier::kLiftoff
                                                     : ExecutionTier::kTurbofan;
    int imported = module->num_imported_functions;
    // Generate necessary compilation units on the fly.
    for (int function_index : recompile_function_indexes) {
      DCHECK_LE(imported, function_index);
      int slot_index = function_index - imported;
      auto& progress = compilation_progress_[slot_index];
      progress = MissingRecompilationField::update(progress, true);
      builder->AddRecompilationUnit(function_index, new_tier);
    }
  }

  // Trigger callback if module needs no recompilation.
  if (outstanding_recompilation_functions_ == 0) {
    TriggerCallbacks(base::EnumSet<CompilationEvent>(
        {CompilationEvent::kFinishedRecompilation}));
  }

  if (builder.has_value()) {
    // Avoid holding lock while scheduling a compile job.
    guard.reset();
    builder->Commit();
  }
}

void CompilationStateImpl::AddCallback(
    std::unique_ptr<CompilationEventCallback> callback) {
  base::MutexGuard callbacks_guard(&callbacks_mutex_);
  // Immediately trigger events that already happened.
  for (auto event : {CompilationEvent::kFinishedExportWrappers,
                     CompilationEvent::kFinishedBaselineCompilation,
                     CompilationEvent::kFinishedTopTierCompilation,
                     CompilationEvent::kFailedCompilation}) {
    if (finished_events_.contains(event)) {
      callback->call(event);
    }
  }
  constexpr base::EnumSet<CompilationEvent> kFinalEvents{
      CompilationEvent::kFinishedTopTierCompilation,
      CompilationEvent::kFailedCompilation};
  if (!finished_events_.contains_any(kFinalEvents)) {
    callbacks_.emplace_back(std::move(callback));
  }
}

void CompilationStateImpl::CommitCompilationUnits(
    base::Vector<WasmCompilationUnit> baseline_units,
    base::Vector<WasmCompilationUnit> top_tier_units,
    base::Vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
        js_to_wasm_wrapper_units) {
  if (!js_to_wasm_wrapper_units.empty()) {
    // |js_to_wasm_wrapper_units_| will only be initialized once.
    DCHECK_EQ(0, outstanding_js_to_wasm_wrappers_.load());
    js_to_wasm_wrapper_units_.insert(js_to_wasm_wrapper_units_.end(),
                                     js_to_wasm_wrapper_units.begin(),
                                     js_to_wasm_wrapper_units.end());
    // Use release semantics such that updates to {js_to_wasm_wrapper_units_}
    // are available to other threads doing an acquire load.
    outstanding_js_to_wasm_wrappers_.store(js_to_wasm_wrapper_units.size(),
                                           std::memory_order_release);
  }
  if (!baseline_units.empty() || !top_tier_units.empty()) {
    compilation_unit_queues_.AddUnits(baseline_units, top_tier_units,
                                      native_module_->module());
  }
  compile_job_->NotifyConcurrencyIncrease();
}

void CompilationStateImpl::CommitTopTierCompilationUnit(
    WasmCompilationUnit unit) {
  CommitCompilationUnits({}, {&unit, 1}, {});
}

void CompilationStateImpl::AddTopTierPriorityCompilationUnit(
    WasmCompilationUnit unit, size_t priority) {
  compilation_unit_queues_.AddTopTierPriorityUnit(unit, priority);
  {
    base::MutexGuard guard(&callbacks_mutex_);
    outstanding_top_tier_functions_++;
  }
  compile_job_->NotifyConcurrencyIncrease();
}

std::shared_ptr<JSToWasmWrapperCompilationUnit>
CompilationStateImpl::GetNextJSToWasmWrapperCompilationUnit() {
  size_t outstanding_units =
      outstanding_js_to_wasm_wrappers_.load(std::memory_order_relaxed);
  // Use acquire semantics such that initialization of
  // {js_to_wasm_wrapper_units_} is available.
  while (outstanding_units &&
         !outstanding_js_to_wasm_wrappers_.compare_exchange_weak(
             outstanding_units, outstanding_units - 1,
             std::memory_order_acquire)) {
    // Retry with updated {outstanding_units}.
  }
  if (outstanding_units == 0) return nullptr;
  return js_to_wasm_wrapper_units_[outstanding_units - 1];
}

void CompilationStateImpl::FinalizeJSToWasmWrappers(
    Isolate* isolate, const WasmModule* module,
    Handle<FixedArray>* export_wrappers_out) {
  *export_wrappers_out = isolate->factory()->NewFixedArray(
      MaxNumExportWrappers(module), AllocationType::kOld);
  // TODO(6792): Wrappers below are allocated with {Factory::NewCode}. As an
  // optimization we create a code memory modification scope that avoids
  // changing the page permissions back-and-forth between RWX and RX, because
  // many such wrapper are allocated in sequence below.
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.FinalizeJSToWasmWrappers", "wrappers",
               js_to_wasm_wrapper_units_.size());
  CodePageCollectionMemoryModificationScope modification_scope(isolate->heap());
  for (auto& unit : js_to_wasm_wrapper_units_) {
    DCHECK_EQ(isolate, unit->isolate());
    Handle<Code> code = unit->Finalize();
    int wrapper_index =
        GetExportWrapperIndex(module, unit->sig(), unit->is_import());
    (*export_wrappers_out)->set(wrapper_index, ToCodeT(*code));
    RecordStats(*code, isolate->counters());
  }
}

CompilationUnitQueues::Queue* CompilationStateImpl::GetQueueForCompileTask(
    int task_id) {
  return compilation_unit_queues_.GetQueueForTask(task_id);
}

base::Optional<WasmCompilationUnit>
CompilationStateImpl::GetNextCompilationUnit(
    CompilationUnitQueues::Queue* queue, CompileBaselineOnly baseline_only) {
  return compilation_unit_queues_.GetNextUnit(queue, baseline_only);
}

void CompilationStateImpl::OnFinishedUnits(
    base::Vector<WasmCode*> code_vector) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.OnFinishedUnits", "units", code_vector.size());

  base::MutexGuard guard(&callbacks_mutex_);

  // In case of no outstanding compilation units we can return early.
  // This is especially important for lazy modules that were deserialized.
  // Compilation progress was not set up in these cases.
  if (outstanding_baseline_units_ == 0 && outstanding_export_wrappers_ == 0 &&
      outstanding_top_tier_functions_ == 0 &&
      outstanding_recompilation_functions_ == 0) {
    return;
  }

  // Assume an order of execution tiers that represents the quality of their
  // generated code.
  static_assert(ExecutionTier::kNone < ExecutionTier::kLiftoff &&
                    ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                "Assume an order on execution tiers");

  DCHECK_EQ(compilation_progress_.size(),
            native_module_->module()->num_declared_functions);

  base::EnumSet<CompilationEvent> triggered_events;

  for (size_t i = 0; i < code_vector.size(); i++) {
    WasmCode* code = code_vector[i];
    DCHECK_NOT_NULL(code);
    DCHECK_LT(code->index(), native_module_->num_functions());

    if (code->index() <
        static_cast<int>(native_module_->num_imported_functions())) {
      // Import wrapper.
      DCHECK_EQ(code->tier(), ExecutionTier::kTurbofan);
      outstanding_baseline_units_--;
    } else {
      // Function.
      DCHECK_NE(code->tier(), ExecutionTier::kNone);

      // Read function's compilation progress.
      // This view on the compilation progress may differ from the actually
      // compiled code. Any lazily compiled function does not contribute to the
      // compilation progress but may publish code to the code manager.
      int slot_index =
          declared_function_index(native_module_->module(), code->index());
      uint8_t function_progress = compilation_progress_[slot_index];
      ExecutionTier required_baseline_tier =
          RequiredBaselineTierField::decode(function_progress);
      ExecutionTier required_top_tier =
          RequiredTopTierField::decode(function_progress);
      ExecutionTier reached_tier = ReachedTierField::decode(function_progress);

      // Check whether required baseline or top tier are reached.
      if (reached_tier < required_baseline_tier &&
          required_baseline_tier <= code->tier()) {
        DCHECK_GT(outstanding_baseline_units_, 0);
        outstanding_baseline_units_--;
      }
      if (code->tier() == ExecutionTier::kTurbofan) {
        bytes_since_last_chunk_ += code->instructions().size();
      }
      if (reached_tier < required_top_tier &&
          required_top_tier <= code->tier()) {
        DCHECK_GT(outstanding_top_tier_functions_, 0);
        outstanding_top_tier_functions_--;
      }

      if (V8_UNLIKELY(MissingRecompilationField::decode(function_progress))) {
        DCHECK_LT(0, outstanding_recompilation_functions_);
        // If tiering up, accept any TurboFan code. For tiering down, look at
        // the {for_debugging} flag. The tier can be Liftoff or TurboFan and is
        // irrelevant here. In particular, we want to ignore any outstanding
        // non-debugging units.
        bool matches = tiering_state_ == kTieredDown
                           ? code->for_debugging()
                           : code->tier() == ExecutionTier::kTurbofan;
        if (matches) {
          outstanding_recompilation_functions_--;
          compilation_progress_[slot_index] = MissingRecompilationField::update(
              compilation_progress_[slot_index], false);
          if (outstanding_recompilation_functions_ == 0) {
            triggered_events.Add(CompilationEvent::kFinishedRecompilation);
          }
        }
      }

      // Update function's compilation progress.
      if (code->tier() > reached_tier) {
        compilation_progress_[slot_index] = ReachedTierField::update(
            compilation_progress_[slot_index], code->tier());
      }
      DCHECK_LE(0, outstanding_baseline_units_);
    }
  }

  TriggerCallbacks(triggered_events);
}

void CompilationStateImpl::OnFinishedJSToWasmWrapperUnits(int num) {
  if (num == 0) return;
  base::MutexGuard guard(&callbacks_mutex_);
  DCHECK_GE(outstanding_export_wrappers_, num);
  outstanding_export_wrappers_ -= num;
  TriggerCallbacks();
}

void CompilationStateImpl::TriggerCallbacks(
    base::EnumSet<CompilationEvent> triggered_events) {
  DCHECK(!callbacks_mutex_.TryLock());

  if (outstanding_export_wrappers_ == 0) {
    triggered_events.Add(CompilationEvent::kFinishedExportWrappers);
    if (outstanding_baseline_units_ == 0) {
      triggered_events.Add(CompilationEvent::kFinishedBaselineCompilation);
      if (dynamic_tiering_ == DynamicTiering::kDisabled &&
          outstanding_top_tier_functions_ == 0) {
        triggered_events.Add(CompilationEvent::kFinishedTopTierCompilation);
      }
    }
  }

  if (dynamic_tiering_ == DynamicTiering::kEnabled &&
      static_cast<size_t>(FLAG_wasm_caching_threshold) <
          bytes_since_last_chunk_) {
    triggered_events.Add(CompilationEvent::kFinishedCompilationChunk);
    bytes_since_last_chunk_ = 0;
  }
  if (compile_failed_.load(std::memory_order_relaxed)) {
    // *Only* trigger the "failed" event.
    triggered_events =
        base::EnumSet<CompilationEvent>({CompilationEvent::kFailedCompilation});
  }

  if (triggered_events.empty()) return;

  // Don't trigger past events again.
  triggered_events -= finished_events_;
  // Recompilation can happen multiple times, thus do not store this. There can
  // also be multiple compilation chunks.
  finished_events_ |= triggered_events -
                      CompilationEvent::kFinishedRecompilation -
                      CompilationEvent::kFinishedCompilationChunk;

  for (auto event :
       {std::make_pair(CompilationEvent::kFailedCompilation,
                       "wasm.CompilationFailed"),
        std::make_pair(CompilationEvent::kFinishedExportWrappers,
                       "wasm.ExportWrappersFinished"),
        std::make_pair(CompilationEvent::kFinishedBaselineCompilation,
                       "wasm.BaselineFinished"),
        std::make_pair(CompilationEvent::kFinishedTopTierCompilation,
                       "wasm.TopTierFinished"),
        std::make_pair(CompilationEvent::kFinishedCompilationChunk,
                       "wasm.CompilationChunkFinished"),
        std::make_pair(CompilationEvent::kFinishedRecompilation,
                       "wasm.RecompilationFinished")}) {
    if (!triggered_events.contains(event.first)) continue;
    DCHECK_NE(compilation_id_, kInvalidCompilationID);
    TRACE_EVENT1("v8.wasm", event.second, "id", compilation_id_);
    for (auto& callback : callbacks_) {
      callback->call(event.first);
    }
  }

  if (outstanding_baseline_units_ == 0 && outstanding_export_wrappers_ == 0 &&
      outstanding_top_tier_functions_ == 0 &&
      outstanding_recompilation_functions_ == 0) {
    callbacks_.erase(
        std::remove_if(
            callbacks_.begin(), callbacks_.end(),
            [](std::unique_ptr<CompilationEventCallback>& event) {
              return event->release_after_final_event() ==
                     CompilationEventCallback::ReleaseAfterFinalEvent::kRelease;
            }),
        callbacks_.end());
  }
}

void CompilationStateImpl::OnCompilationStopped(WasmFeatures detected) {
  base::MutexGuard guard(&mutex_);
  detected_features_.Add(detected);
}

void CompilationStateImpl::PublishDetectedFeatures(Isolate* isolate) {
  // Notifying the isolate of the feature counts must take place under
  // the mutex, because even if we have finished baseline compilation,
  // tiering compilations may still occur in the background.
  base::MutexGuard guard(&mutex_);
  UpdateFeatureUseCounts(isolate, detected_features_);
}

void CompilationStateImpl::PublishCompilationResults(
    std::vector<std::unique_ptr<WasmCode>> unpublished_code) {
  if (unpublished_code.empty()) return;

  // For import wrapper compilation units, add result to the cache.
  int num_imported_functions = native_module_->num_imported_functions();
  WasmImportWrapperCache* cache = native_module_->import_wrapper_cache();
  for (const auto& code : unpublished_code) {
    int func_index = code->index();
    DCHECK_LE(0, func_index);
    DCHECK_LT(func_index, native_module_->num_functions());
    if (func_index < num_imported_functions) {
      const FunctionSig* sig =
          native_module_->module()->functions[func_index].sig;
      WasmImportWrapperCache::CacheKey key(
          compiler::kDefaultImportCallKind, sig,
          static_cast<int>(sig->parameter_count()), kNoSuspend);
      // If two imported functions have the same key, only one of them should
      // have been added as a compilation unit. So it is always the first time
      // we compile a wrapper for this key here.
      DCHECK_NULL((*cache)[key]);
      (*cache)[key] = code.get();
      code->IncRef();
    }
  }
  PublishCode(base::VectorOf(unpublished_code));
}

void CompilationStateImpl::PublishCode(
    base::Vector<std::unique_ptr<WasmCode>> code) {
  WasmCodeRefScope code_ref_scope;
  std::vector<WasmCode*> published_code =
      native_module_->PublishCode(std::move(code));
  // Defer logging code in case wire bytes were not fully received yet.
  if (native_module_->HasWireBytes()) {
    GetWasmEngine()->LogCode(base::VectorOf(published_code));
  }

  OnFinishedUnits(base::VectorOf(std::move(published_code)));
}

void CompilationStateImpl::SchedulePublishCompilationResults(
    std::vector<std::unique_ptr<WasmCode>> unpublished_code) {
  {
    base::MutexGuard guard(&publish_mutex_);
    if (publisher_running_) {
      // Add new code to the queue and return.
      publish_queue_.reserve(publish_queue_.size() + unpublished_code.size());
      for (auto& c : unpublished_code) {
        publish_queue_.emplace_back(std::move(c));
      }
      return;
    }
    publisher_running_ = true;
  }
  CodeSpaceWriteScope code_space_write_scope(native_module_);
  while (true) {
    PublishCompilationResults(std::move(unpublished_code));
    unpublished_code.clear();

    // Keep publishing new code that came in.
    base::MutexGuard guard(&publish_mutex_);
    DCHECK(publisher_running_);
    if (publish_queue_.empty()) {
      publisher_running_ = false;
      return;
    }
    unpublished_code.swap(publish_queue_);
  }
}

size_t CompilationStateImpl::NumOutstandingCompilations() const {
  size_t outstanding_wrappers =
      outstanding_js_to_wasm_wrappers_.load(std::memory_order_relaxed);
  size_t outstanding_functions = compilation_unit_queues_.GetTotalSize();
  return outstanding_wrappers + outstanding_functions;
}

void CompilationStateImpl::SetError() {
  compile_cancelled_.store(true, std::memory_order_relaxed);
  if (compile_failed_.exchange(true, std::memory_order_relaxed)) {
    return;  // Already failed before.
  }

  base::MutexGuard callbacks_guard(&callbacks_mutex_);
  TriggerCallbacks();
  callbacks_.clear();
}

void CompilationStateImpl::WaitForCompilationEvent(
    CompilationEvent expect_event) {
  class WaitForCompilationEventCallback : public CompilationEventCallback {
   public:
    WaitForCompilationEventCallback(std::shared_ptr<base::Semaphore> semaphore,
                                    std::shared_ptr<std::atomic<bool>> done,
                                    base::EnumSet<CompilationEvent> events)
        : semaphore_(std::move(semaphore)),
          done_(std::move(done)),
          events_(events) {}

    void call(CompilationEvent event) override {
      if (!events_.contains(event)) return;
      done_->store(true, std::memory_order_relaxed);
      semaphore_->Signal();
    }

   private:
    std::shared_ptr<base::Semaphore> semaphore_;
    std::shared_ptr<std::atomic<bool>> done_;
    base::EnumSet<CompilationEvent> events_;
  };

  auto semaphore = std::make_shared<base::Semaphore>(0);
  auto done = std::make_shared<std::atomic<bool>>(false);
  base::EnumSet<CompilationEvent> events{expect_event,
                                         CompilationEvent::kFailedCompilation};
  {
    base::MutexGuard callbacks_guard(&callbacks_mutex_);
    if (finished_events_.contains_any(events)) return;
    callbacks_.emplace_back(std::make_unique<WaitForCompilationEventCallback>(
        semaphore, done, events));
  }

  class WaitForEventDelegate final : public JobDelegate {
   public:
    explicit WaitForEventDelegate(std::shared_ptr<std::atomic<bool>> done)
        : done_(std::move(done)) {}

    bool ShouldYield() override {
      return done_->load(std::memory_order_relaxed);
    }

    bool IsJoiningThread() const override { return true; }

    void NotifyConcurrencyIncrease() override { UNIMPLEMENTED(); }

    uint8_t GetTaskId() override { return kMainTaskId; }

   private:
    std::shared_ptr<std::atomic<bool>> done_;
  };

  WaitForEventDelegate delegate{done};
  // Everything except for top-tier units will be processed with kBaselineOnly
  // (including wrappers). Hence we choose this for any event except
  // {kFinishedTopTierCompilation}.
  auto compile_tiers =
      expect_event == CompilationEvent::kFinishedTopTierCompilation
          ? kBaselineOrTopTier
          : kBaselineOnly;
  ExecuteCompilationUnits(native_module_weak_, async_counters_.get(), &delegate,
                          compile_tiers);
  semaphore->Wait();
}

namespace {
using JSToWasmWrapperQueue =
    WrapperQueue<JSToWasmWrapperKey, base::hash<JSToWasmWrapperKey>>;
using JSToWasmWrapperUnitMap =
    std::unordered_map<JSToWasmWrapperKey,
                       std::unique_ptr<JSToWasmWrapperCompilationUnit>,
                       base::hash<JSToWasmWrapperKey>>;

class CompileJSToWasmWrapperJob final : public JobTask {
 public:
  CompileJSToWasmWrapperJob(JSToWasmWrapperQueue* queue,
                            JSToWasmWrapperUnitMap* compilation_units)
      : queue_(queue),
        compilation_units_(compilation_units),
        outstanding_units_(queue->size()) {}

  void Run(JobDelegate* delegate) override {
    while (base::Optional<JSToWasmWrapperKey> key = queue_->pop()) {
      JSToWasmWrapperCompilationUnit* unit = (*compilation_units_)[*key].get();
      unit->Execute();
      outstanding_units_.fetch_sub(1, std::memory_order_relaxed);
      if (delegate && delegate->ShouldYield()) return;
    }
  }

  size_t GetMaxConcurrency(size_t /* worker_count */) const override {
    DCHECK_GE(FLAG_wasm_num_compilation_tasks, 1);
    // {outstanding_units_} includes the units that other workers are currently
    // working on, so we can safely ignore the {worker_count} and just return
    // the current number of outstanding units.
    return std::min(static_cast<size_t>(FLAG_wasm_num_compilation_tasks),
                    outstanding_units_.load(std::memory_order_relaxed));
  }

 private:
  JSToWasmWrapperQueue* const queue_;
  JSToWasmWrapperUnitMap* const compilation_units_;
  std::atomic<size_t> outstanding_units_;
};
}  // namespace

void CompileJsToWasmWrappers(Isolate* isolate, const WasmModule* module,
                             Handle<FixedArray>* export_wrappers_out) {
  TRACE_EVENT0("v8.wasm", "wasm.CompileJsToWasmWrappers");
  *export_wrappers_out = isolate->factory()->NewFixedArray(
      MaxNumExportWrappers(module), AllocationType::kOld);

  JSToWasmWrapperQueue queue;
  JSToWasmWrapperUnitMap compilation_units;
  WasmFeatures enabled_features = WasmFeatures::FromIsolate(isolate);

  // Prepare compilation units in the main thread.
  for (auto exp : module->export_table) {
    if (exp.kind != kExternalFunction) continue;
    auto& function = module->functions[exp.index];
    JSToWasmWrapperKey key(function.imported, *function.sig);
    if (queue.insert(key)) {
      auto unit = std::make_unique<JSToWasmWrapperCompilationUnit>(
          isolate, function.sig, module, function.imported, enabled_features,
          JSToWasmWrapperCompilationUnit::kAllowGeneric);
      compilation_units.emplace(key, std::move(unit));
    }
  }

  {
    // This is nested inside the event above, so the name can be less
    // descriptive. It's mainly to log the number of wrappers.
    TRACE_EVENT1("v8.wasm", "wasm.JsToWasmWrapperCompilation", "num_wrappers",
                 compilation_units.size());
    auto job =
        std::make_unique<CompileJSToWasmWrapperJob>(&queue, &compilation_units);
    if (FLAG_wasm_num_compilation_tasks > 0) {
      auto job_handle = V8::GetCurrentPlatform()->PostJob(
          TaskPriority::kUserVisible, std::move(job));

      // Wait for completion, while contributing to the work.
      job_handle->Join();
    } else {
      job->Run(nullptr);
    }
  }

  // Finalize compilation jobs in the main thread.
  // TODO(6792): Wrappers below are allocated with {Factory::NewCode}. As an
  // optimization we create a code memory modification scope that avoids
  // changing the page permissions back-and-forth between RWX and RX, because
  // many such wrapper are allocated in sequence below.
  CodePageCollectionMemoryModificationScope modification_scope(isolate->heap());
  for (auto& pair : compilation_units) {
    JSToWasmWrapperKey key = pair.first;
    JSToWasmWrapperCompilationUnit* unit = pair.second.get();
    DCHECK_EQ(isolate, unit->isolate());
    Handle<Code> code = unit->Finalize();
    int wrapper_index = GetExportWrapperIndex(module, &key.second, key.first);
    (*export_wrappers_out)->set(wrapper_index, ToCodeT(*code));
    RecordStats(*code, isolate->counters());
  }
}

WasmCode* CompileImportWrapper(
    NativeModule* native_module, Counters* counters,
    compiler::WasmImportCallKind kind, const FunctionSig* sig,
    int expected_arity, Suspend suspend,
    WasmImportWrapperCache::ModificationScope* cache_scope) {
  // Entry should exist, so that we don't insert a new one and invalidate
  // other threads' iterators/references, but it should not have been compiled
  // yet.
  WasmImportWrapperCache::CacheKey key(kind, sig, expected_arity, suspend);
  DCHECK_NULL((*cache_scope)[key]);
  bool source_positions = is_asmjs_module(native_module->module());
  // Keep the {WasmCode} alive until we explicitly call {IncRef}.
  WasmCodeRefScope code_ref_scope;
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = compiler::CompileWasmImportCallWrapper(
      &env, kind, sig, source_positions, expected_arity, suspend);
  WasmCode* published_code;
  {
    CodeSpaceWriteScope code_space_write_scope(native_module);
    std::unique_ptr<WasmCode> wasm_code = native_module->AddCode(
        result.func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), GetCodeKind(result),
        ExecutionTier::kNone, kNoDebugging);
    published_code = native_module->PublishCode(std::move(wasm_code));
  }
  (*cache_scope)[key] = published_code;
  published_code->IncRef();
  counters->wasm_generated_code_size()->Increment(
      published_code->instructions().length());
  counters->wasm_reloc_size()->Increment(published_code->reloc_info().length());
  return published_code;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE_COMPILE
#undef TRACE_STREAMING
#undef TRACE_LAZY
