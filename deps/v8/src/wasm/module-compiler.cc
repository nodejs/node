// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/module-compiler.h"

#include <algorithm>
#include <queue>

#include "src/api/api.h"
#include "src/asmjs/asm-js.h"
#include "src/base/enum-set.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "src/base/utils/random-number-generator.h"
#include "src/compiler/wasm-compiler.h"
#include "src/heap/heap-inl.h"  // For CodeSpaceMemoryModificationScope.
#include "src/logging/counters.h"
#include "src/objects/property-descriptor.h"
#include "src/tasks/task-utils.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/identity-map.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
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

enum class CompileMode : uint8_t { kRegular, kTiering };

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

// Background compile jobs hold a shared pointer to this token. The token is
// used to notify them that they should stop. As soon as they see this (after
// finishing their current compilation unit), they will stop.
// This allows to already remove the NativeModule without having to synchronize
// on background compile jobs.
class BackgroundCompileToken {
 public:
  explicit BackgroundCompileToken(
      const std::shared_ptr<NativeModule>& native_module)
      : native_module_(native_module) {}

  void Cancel() {
    base::SharedMutexGuard<base::kExclusive> mutex_guard(&mutex_);
    native_module_.reset();
  }

 private:
  friend class BackgroundCompileScope;
  base::SharedMutex mutex_;
  std::weak_ptr<NativeModule> native_module_;

  std::shared_ptr<NativeModule> StartScope() {
    mutex_.LockShared();
    return native_module_.lock();
  }

  void ExitScope() { mutex_.UnlockShared(); }
};

class CompilationStateImpl;

// Keep these scopes short, as they hold the mutex of the token, which
// sequentializes all these scopes. The mutex is also acquired from foreground
// tasks, which should not be blocked for a long time.
class BackgroundCompileScope {
 public:
  explicit BackgroundCompileScope(
      const std::shared_ptr<BackgroundCompileToken>& token)
      : token_(token.get()), native_module_(token->StartScope()) {}

  ~BackgroundCompileScope() { token_->ExitScope(); }

  bool cancelled() const { return native_module_ == nullptr; }

  NativeModule* native_module() {
    DCHECK(!cancelled());
    return native_module_.get();
  }

  inline CompilationStateImpl* compilation_state();

 private:
  BackgroundCompileToken* const token_;
  // Keep the native module alive while in this scope.
  std::shared_ptr<NativeModule> const native_module_;
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
  explicit CompilationUnitQueues(int max_tasks) : queues_(max_tasks) {
    DCHECK_LT(0, max_tasks);
    for (int task_id = 0; task_id < max_tasks; ++task_id) {
      queues_[task_id].next_steal_task_id = next_task_id(task_id);
    }
    for (auto& atomic_counter : num_units_) {
      std::atomic_init(&atomic_counter, size_t{0});
    }
  }

  base::Optional<WasmCompilationUnit> GetNextUnit(
      int task_id, CompileBaselineOnly baseline_only) {
    DCHECK_LE(0, task_id);
    DCHECK_GT(queues_.size(), task_id);

    // As long as any lower-tier units are outstanding we need to steal them
    // before executing own higher-tier units.
    int max_tier = baseline_only ? kBaseline : kTopTier;
    for (int tier = GetLowestTierWithUnits(); tier <= max_tier; ++tier) {
      if (auto unit = GetNextUnitOfTier(task_id, tier)) {
        size_t old_units_count =
            num_units_[tier].fetch_sub(1, std::memory_order_relaxed);
        DCHECK_LE(1, old_units_count);
        USE(old_units_count);
        return unit;
      }
    }
    return {};
  }

  void AddUnits(Vector<WasmCompilationUnit> baseline_units,
                Vector<WasmCompilationUnit> top_tier_units,
                const WasmModule* module) {
    DCHECK_LT(0, baseline_units.size() + top_tier_units.size());
    // Add to the individual queues in a round-robin fashion. No special care is
    // taken to balance them; they will be balanced by work stealing.
    int queue_to_add = next_queue_to_add.load(std::memory_order_relaxed);
    while (!next_queue_to_add.compare_exchange_weak(
        queue_to_add, next_task_id(queue_to_add), std::memory_order_relaxed)) {
      // Retry with updated {queue_to_add}.
    }

    Queue* queue = &queues_[queue_to_add];
    base::MutexGuard guard(&queue->mutex);
    base::Optional<base::MutexGuard> big_units_guard;
    for (auto pair : {std::make_pair(int{kBaseline}, baseline_units),
                      std::make_pair(int{kTopTier}, top_tier_units)}) {
      int tier = pair.first;
      Vector<WasmCompilationUnit> units = pair.second;
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

  struct Queue {
    base::Mutex mutex;

    // Protected by {mutex}:
    std::vector<WasmCompilationUnit> units[kNumTiers];
    int next_steal_task_id;
    // End of fields protected by {mutex}.
  };

  struct BigUnit {
    BigUnit(size_t func_size, WasmCompilationUnit unit)
        : func_size{func_size}, unit(unit) {}

    size_t func_size;
    WasmCompilationUnit unit;

    bool operator<(const BigUnit& other) const {
      return func_size < other.func_size;
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

  std::vector<Queue> queues_;
  BigUnitsQueue big_units_queue_;

  std::atomic<size_t> num_units_[kNumTiers];
  std::atomic<int> next_queue_to_add{0};

  int next_task_id(int task_id) const {
    int next = task_id + 1;
    return next == static_cast<int>(queues_.size()) ? 0 : next;
  }

  int GetLowestTierWithUnits() const {
    for (int tier = 0; tier < kNumTiers; ++tier) {
      if (num_units_[tier].load(std::memory_order_relaxed) > 0) return tier;
    }
    return kNumTiers;
  }

  base::Optional<WasmCompilationUnit> GetNextUnitOfTier(int task_id, int tier) {
    Queue* queue = &queues_[task_id];
    // First check whether there is a big unit of that tier. Execute that first.
    if (auto unit = GetBigUnitOfTier(tier)) return unit;

    // Then check whether our own queue has a unit of the wanted tier. If
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
    size_t steal_trials = queues_.size();
    for (; steal_trials > 0;
         --steal_trials, steal_task_id = next_task_id(steal_task_id)) {
      if (steal_task_id == task_id) continue;
      if (auto unit = StealUnitsAndGetFirst(task_id, steal_task_id, tier)) {
        return unit;
      }
    }

    // If we reach here, we didn't find any unit of the requested tier.
    return {};
  }

  base::Optional<WasmCompilationUnit> GetBigUnitOfTier(int tier) {
    // Fast-path without locking.
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

  // Steal units of {wanted_tier} from {steal_from_task_id} to {task_id}. Return
  // first stolen unit (rest put in queue of {task_id}), or {nullopt} if
  // {steal_from_task_id} had no units of {wanted_tier}.
  base::Optional<WasmCompilationUnit> StealUnitsAndGetFirst(
      int task_id, int steal_from_task_id, int wanted_tier) {
    DCHECK_NE(task_id, steal_from_task_id);
    std::vector<WasmCompilationUnit> stolen;
    base::Optional<WasmCompilationUnit> returned_unit;
    {
      Queue* steal_queue = &queues_[steal_from_task_id];
      base::MutexGuard guard(&steal_queue->mutex);
      auto* steal_from_vector = &steal_queue->units[wanted_tier];
      if (steal_from_vector->empty()) return {};
      size_t remaining = steal_from_vector->size() / 2;
      auto steal_begin = steal_from_vector->begin() + remaining;
      returned_unit = *steal_begin;
      stolen.assign(steal_begin + 1, steal_from_vector->end());
      steal_from_vector->erase(steal_begin, steal_from_vector->end());
    }
    Queue* queue = &queues_[task_id];
    base::MutexGuard guard(&queue->mutex);
    auto* target_queue = &queue->units[wanted_tier];
    target_queue->insert(target_queue->end(), stolen.begin(), stolen.end());
    queue->next_steal_task_id = next_task_id(steal_from_task_id);
    return returned_unit;
  }
};

// The {CompilationStateImpl} keeps track of the compilation state of the
// owning NativeModule, i.e. which functions are left to be compiled.
// It contains a task manager to allow parallel and asynchronous background
// compilation of functions.
// Its public interface {CompilationState} lives in compilation-environment.h.
class CompilationStateImpl {
 public:
  CompilationStateImpl(const std::shared_ptr<NativeModule>& native_module,
                       std::shared_ptr<Counters> async_counters);

  // Cancel all background compilation and wait for all tasks to finish. Call
  // this before destructing this object.
  void AbortCompilation();

  // Initialize compilation progress. Set compilation tiers to expect for
  // baseline and top tier compilation. Must be set before {AddCompilationUnits}
  // is invoked which triggers background compilation.
  void InitializeCompilationProgress(bool lazy_module, int num_wrappers);

  // Initialize recompilation of the whole module: Setup compilation progress
  // for recompilation and add the respective compilation units. The callback is
  // called immediately if no recompilation is needed, or called later
  // otherwise.
  void InitializeRecompilation(
      ExecutionTier tier,
      CompilationState::callback_t recompilation_finished_callback);

  // Add the callback function to be called on compilation events. Needs to be
  // set before {AddCompilationUnits} is run to ensure that it receives all
  // events. The callback object must support being deleted from any thread.
  void AddCallback(CompilationState::callback_t);

  // Inserts new functions to compile and kicks off compilation.
  void AddCompilationUnits(
      Vector<WasmCompilationUnit> baseline_units,
      Vector<WasmCompilationUnit> top_tier_units,
      Vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
          js_to_wasm_wrapper_units);
  void AddTopTierCompilationUnit(WasmCompilationUnit);
  base::Optional<WasmCompilationUnit> GetNextCompilationUnit(
      int task_id, CompileBaselineOnly baseline_only);

  std::shared_ptr<JSToWasmWrapperCompilationUnit>
  GetNextJSToWasmWrapperCompilationUnit();
  void FinalizeJSToWasmWrappers(Isolate* isolate, const WasmModule* module,
                                Handle<FixedArray>* export_wrappers_out);

  void OnFinishedUnits(Vector<WasmCode*>, Vector<WasmCompilationResult>);
  void OnFinishedJSToWasmWrapperUnits(int num);

  void OnBackgroundTaskStopped(int task_id, const WasmFeatures& detected);
  void UpdateDetectedFeatures(const WasmFeatures& detected);
  void PublishDetectedFeatures(Isolate*);
  void RestartBackgroundTasks();

  void SetError();

  bool failed() const {
    return compile_failed_.load(std::memory_order_relaxed);
  }

  bool baseline_compilation_finished() const {
    base::MutexGuard guard(&callbacks_mutex_);
    return outstanding_baseline_units_ == 0;
  }

  bool top_tier_compilation_finished() const {
    base::MutexGuard guard(&callbacks_mutex_);
    return outstanding_top_tier_functions_ == 0;
  }

  bool recompilation_finished() const {
    base::MutexGuard guard(&callbacks_mutex_);
    return outstanding_recompilation_functions_ == 0;
  }

  CompileMode compile_mode() const { return compile_mode_; }
  Counters* counters() const { return async_counters_.get(); }
  WasmFeatures* detected_features() { return &detected_features_; }

  void SetWireBytesStorage(
      std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
    base::MutexGuard guard(&mutex_);
    wire_bytes_storage_ = wire_bytes_storage;
  }

  std::shared_ptr<WireBytesStorage> GetWireBytesStorage() const {
    base::MutexGuard guard(&mutex_);
    DCHECK_NOT_NULL(wire_bytes_storage_);
    return wire_bytes_storage_;
  }

  const std::shared_ptr<BackgroundCompileToken>& background_compile_token()
      const {
    return background_compile_token_;
  }

  double GetCompilationDeadline(double now) {
    // Execute for at least 50ms. Try to distribute deadlines of different tasks
    // such that every 5ms one task stops. No task should execute longer than
    // 200ms though.
    constexpr double kMinLimit = 50. / base::Time::kMillisecondsPerSecond;
    constexpr double kMaxLimit = 200. / base::Time::kMillisecondsPerSecond;
    constexpr double kGapBetweenTasks = 5. / base::Time::kMillisecondsPerSecond;
    double min_deadline = now + kMinLimit;
    double max_deadline = now + kMaxLimit;
    double next_deadline =
        next_compilation_deadline_.load(std::memory_order_relaxed);
    while (true) {
      double deadline =
          std::max(min_deadline, std::min(max_deadline, next_deadline));
      if (next_compilation_deadline_.compare_exchange_weak(
              next_deadline, deadline + kGapBetweenTasks,
              std::memory_order_relaxed)) {
        return deadline;
      }
      // Otherwise, retry with the updated {next_deadline}.
    }
  }

 private:
  // Trigger callbacks according to the internal counters below
  // (outstanding_...), plus the given events.
  // Hold the {callbacks_mutex_} when calling this method.
  void TriggerCallbacks(base::EnumSet<CompilationEvent> additional_events = {});

  NativeModule* const native_module_;
  const std::shared_ptr<BackgroundCompileToken> background_compile_token_;
  const CompileMode compile_mode_;
  const std::shared_ptr<Counters> async_counters_;

  // Compilation error, atomically updated. This flag can be updated and read
  // using relaxed semantics.
  std::atomic<bool> compile_failed_{false};

  const int max_background_tasks_ = 0;

  CompilationUnitQueues compilation_unit_queues_;

  // Each compilation task executes until a certain deadline. The
  // {CompilationStateImpl} orchestrates the deadlines such that they are
  // evenly distributed and not all tasks stop at the same time. This removes
  // contention during publishing of compilation results and also gives other
  // tasks a fair chance to utilize the worker threads on a regular basis.
  std::atomic<double> next_compilation_deadline_{0};

  // Index of the next wrapper to compile in {js_to_wasm_wrapper_units_}.
  std::atomic<int> js_to_wasm_wrapper_id_{0};
  // Wrapper compilation units are stored in shared_ptrs so that they are kept
  // alive by the tasks even if the NativeModule dies.
  std::vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
      js_to_wasm_wrapper_units_;

  // This mutex protects all information of this {CompilationStateImpl} which is
  // being accessed concurrently.
  mutable base::Mutex mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  // Set of unused task ids; <= {max_background_tasks_} many.
  std::vector<int> available_task_ids_;

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

  // Callback functions to be called on compilation events.
  std::vector<CompilationState::callback_t> callbacks_;

  // Events that already happened.
  base::EnumSet<CompilationEvent> finished_events_;

  int outstanding_baseline_units_ = 0;
  int outstanding_top_tier_functions_ = 0;
  std::vector<uint8_t> compilation_progress_;

  int outstanding_recompilation_functions_ = 0;
  ExecutionTier recompilation_tier_;
  // End of fields protected by {callbacks_mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  // Encoding of fields in the {compilation_progress_} vector.
  using RequiredBaselineTierField = base::BitField8<ExecutionTier, 0, 2>;
  using RequiredTopTierField = base::BitField8<ExecutionTier, 2, 2>;
  using ReachedTierField = base::BitField8<ExecutionTier, 4, 2>;
  using ReachedRecompilationTierField = base::BitField8<ExecutionTier, 6, 2>;
};

CompilationStateImpl* Impl(CompilationState* compilation_state) {
  return reinterpret_cast<CompilationStateImpl*>(compilation_state);
}
const CompilationStateImpl* Impl(const CompilationState* compilation_state) {
  return reinterpret_cast<const CompilationStateImpl*>(compilation_state);
}

CompilationStateImpl* BackgroundCompileScope::compilation_state() {
  return Impl(native_module()->compilation_state());
}

void UpdateFeatureUseCounts(Isolate* isolate, const WasmFeatures& detected) {
  if (detected.has_threads()) {
    isolate->CountUsage(v8::Isolate::UseCounterFeature::kWasmThreadOpcodes);
  }
}

}  // namespace

//////////////////////////////////////////////////////
// PIMPL implementation of {CompilationState}.

CompilationState::~CompilationState() { Impl(this)->~CompilationStateImpl(); }

void CompilationState::AbortCompilation() { Impl(this)->AbortCompilation(); }

void CompilationState::SetError() { Impl(this)->SetError(); }

void CompilationState::SetWireBytesStorage(
    std::shared_ptr<WireBytesStorage> wire_bytes_storage) {
  Impl(this)->SetWireBytesStorage(std::move(wire_bytes_storage));
}

std::shared_ptr<WireBytesStorage> CompilationState::GetWireBytesStorage()
    const {
  return Impl(this)->GetWireBytesStorage();
}

void CompilationState::AddCallback(CompilationState::callback_t callback) {
  return Impl(this)->AddCallback(std::move(callback));
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

// static
std::unique_ptr<CompilationState> CompilationState::New(
    const std::shared_ptr<NativeModule>& native_module,
    std::shared_ptr<Counters> async_counters) {
  return std::unique_ptr<CompilationState>(reinterpret_cast<CompilationState*>(
      new CompilationStateImpl(native_module, std::move(async_counters))));
}

// End of PIMPL implementation of {CompilationState}.
//////////////////////////////////////////////////////

namespace {

ExecutionTier ApplyHintToExecutionTier(WasmCompilationHintTier hint,
                                       ExecutionTier default_tier) {
  switch (hint) {
    case WasmCompilationHintTier::kDefault:
      return default_tier;
    case WasmCompilationHintTier::kInterpreter:
      return ExecutionTier::kInterpreter;
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
    const WasmModule* module, CompileMode compile_mode,
    const WasmFeatures& enabled_features, uint32_t func_index) {
  ExecutionTierPair result;

  result.baseline_tier = WasmCompilationUnit::GetBaselineExecutionTier(module);
  switch (compile_mode) {
    case CompileMode::kRegular:
      result.top_tier = result.baseline_tier;
      return result;

    case CompileMode::kTiering:

      // Default tiering behaviour.
      result.top_tier = ExecutionTier::kTurbofan;

      // Check if compilation hints override default tiering behaviour.
      if (enabled_features.has_compilation_hints()) {
        const WasmCompilationHint* hint =
            GetCompilationHint(module, func_index);
        if (hint != nullptr) {
          result.baseline_tier = ApplyHintToExecutionTier(hint->baseline_tier,
                                                          result.baseline_tier);
          result.top_tier =
              ApplyHintToExecutionTier(hint->top_tier, result.top_tier);
        }
      }

      // Correct top tier if necessary.
      static_assert(ExecutionTier::kInterpreter < ExecutionTier::kLiftoff &&
                        ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                    "Assume an order on execution tiers");
      if (result.baseline_tier > result.top_tier) {
        result.top_tier = result.baseline_tier;
      }
      return result;
  }
  UNREACHABLE();
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
      baseline_units_.emplace_back(func_index, ExecutionTier::kNone);
      return;
    }
    ExecutionTierPair tiers = GetRequestedExecutionTiers(
        native_module_->module(), compilation_state()->compile_mode(),
        native_module_->enabled_features(), func_index);
    baseline_units_.emplace_back(func_index, tiers.baseline_tier);
    if (tiers.baseline_tier != tiers.top_tier) {
      tiering_units_.emplace_back(func_index, tiers.top_tier);
    }
  }

  void AddJSToWasmWrapperUnit(
      std::shared_ptr<JSToWasmWrapperCompilationUnit> unit) {
    js_to_wasm_wrapper_units_.emplace_back(std::move(unit));
  }

  void AddTopTierUnit(int func_index) {
    ExecutionTierPair tiers = GetRequestedExecutionTiers(
        native_module_->module(), compilation_state()->compile_mode(),
        native_module_->enabled_features(), func_index);
    // In this case, the baseline is lazily compiled, if at all. The compilation
    // unit is added even if the baseline tier is the same.
#ifdef DEBUG
    auto* module = native_module_->module();
    DCHECK_EQ(kWasmOrigin, module->origin);
    const bool lazy_module = false;
    DCHECK_EQ(CompileStrategy::kLazyBaselineEagerTopTier,
              GetCompileStrategy(module, native_module_->enabled_features(),
                                 func_index, lazy_module));
#endif
    tiering_units_.emplace_back(func_index, tiers.top_tier);
  }

  void AddRecompilationUnit(int func_index, ExecutionTier tier) {
    // For recompilation, just treat all units like baseline units.
    baseline_units_.emplace_back(func_index, tier);
  }

  bool Commit() {
    if (baseline_units_.empty() && tiering_units_.empty() &&
        js_to_wasm_wrapper_units_.empty()) {
      return false;
    }
    compilation_state()->AddCompilationUnits(
        VectorOf(baseline_units_), VectorOf(tiering_units_),
        VectorOf(js_to_wasm_wrapper_units_));
    Clear();
    return true;
  }

  void Clear() {
    baseline_units_.clear();
    tiering_units_.clear();
    js_to_wasm_wrapper_units_.clear();
  }

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
                                    Vector<const uint8_t> code,
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
    Vector<const uint8_t> code = wire_bytes.GetFunctionBytes(func);
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

bool CompileLazy(Isolate* isolate, NativeModule* native_module,
                 int func_index) {
  const WasmModule* module = native_module->module();
  auto enabled_features = native_module->enabled_features();
  Counters* counters = isolate->counters();

  DCHECK(!native_module->lazy_compile_frozen());
  NativeModuleModificationScope native_module_modification_scope(native_module);

  TRACE_LAZY("Compiling wasm-function#%d.\n", func_index);

  CompilationStateImpl* compilation_state =
      Impl(native_module->compilation_state());
  ExecutionTierPair tiers = GetRequestedExecutionTiers(
      module, compilation_state->compile_mode(), enabled_features, func_index);

  DCHECK_LE(native_module->num_imported_functions(), func_index);
  DCHECK_LT(func_index, native_module->num_functions());
  WasmCompilationUnit baseline_unit(func_index, tiers.baseline_tier);
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = baseline_unit.ExecuteCompilation(
      isolate->wasm_engine(), &env, compilation_state->GetWireBytesStorage(),
      counters, compilation_state->detected_features());

  // During lazy compilation, we can only get compilation errors when
  // {--wasm-lazy-validation} is enabled. Otherwise, the module was fully
  // verified before starting its execution.
  CHECK_IMPLIES(result.failed(), FLAG_wasm_lazy_validation);
  const WasmFunction* func = &module->functions[func_index];
  if (result.failed()) {
    ErrorThrower thrower(isolate, nullptr);
    Vector<const uint8_t> code =
        compilation_state->GetWireBytesStorage()->GetCode(func->code);
    DecodeResult decode_result = ValidateSingleFunction(
        module, func_index, code, counters, isolate->wasm_engine()->allocator(),
        enabled_features);
    CHECK(decode_result.failed());
    SetCompileError(&thrower, ModuleWireBytes(native_module->wire_bytes()),
                    func, module, decode_result.error());
    return false;
  }

  WasmCodeRefScope code_ref_scope;
  WasmCode* code = native_module->AddCompiledCode(std::move(result));
  DCHECK_EQ(func_index, code->index());

  if (WasmCode::ShouldBeLogged(isolate)) code->LogCode(isolate);

  counters->wasm_lazily_compiled_functions()->Increment();

  const bool lazy_module = IsLazyModule(module);
  if (GetCompileStrategy(module, enabled_features, func_index, lazy_module) ==
          CompileStrategy::kLazy &&
      tiers.baseline_tier < tiers.top_tier) {
    WasmCompilationUnit tiering_unit{func_index, tiers.top_tier};
    compilation_state->AddTopTierCompilationUnit(tiering_unit);
  }

  return true;
}

namespace {

void RecordStats(const Code code, Counters* counters) {
  counters->wasm_generated_code_size()->Increment(code.body_size());
  counters->wasm_reloc_size()->Increment(code.relocation_info().length());
}

constexpr int kMainThreadTaskId = -1;

bool ExecuteJSToWasmWrapperCompilationUnits(
    const std::shared_ptr<BackgroundCompileToken>& token) {
  std::shared_ptr<JSToWasmWrapperCompilationUnit> wrapper_unit = nullptr;
  int num_processed_wrappers = 0;
  do {
    // TODO(thibaudm): Reschedule the compilation task if it takes too long, so
    // that the background thread is not blocked.
    {
      BackgroundCompileScope compile_scope(token);
      if (compile_scope.cancelled()) return false;
      wrapper_unit = compile_scope.compilation_state()
                         ->GetNextJSToWasmWrapperCompilationUnit();
    }
    if (wrapper_unit) {
      wrapper_unit->Execute();
      ++num_processed_wrappers;
    }
  } while (wrapper_unit);
  if (num_processed_wrappers > 0) {
    BackgroundCompileScope compile_scope(token);
    if (compile_scope.cancelled()) return false;
    compile_scope.compilation_state()->OnFinishedJSToWasmWrapperUnits(
        num_processed_wrappers);
  }
  return true;
}

bool NeedsDeterministicCompile() {
  return FLAG_trace_wasm_decoder || FLAG_wasm_num_compilation_tasks <= 1;
}

// Run by the main thread and background tasks to take part in compilation.
// Returns whether any units were executed.
bool ExecuteCompilationUnits(
    const std::shared_ptr<BackgroundCompileToken>& token, Counters* counters,
    int task_id, CompileBaselineOnly baseline_only) {
  TRACE_COMPILE("Compiling (task %d)...\n", task_id);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "ExecuteCompilationUnits");

  // Execute JS to Wasm wrapper units first, so that they are ready to be
  // finalized by the main thread when the kFinishedBaselineCompilation event is
  // triggered.
  if (!ExecuteJSToWasmWrapperCompilationUnits(token)) {
    return false;
  }

  const bool is_foreground = task_id == kMainThreadTaskId;
  // The main thread uses task id 0, which might collide with one of the
  // background tasks. This is fine, as it will only cause some contention on
  // the one queue, but work otherwise.
  if (is_foreground) task_id = 0;

  Platform* platform = V8::GetCurrentPlatform();
  double compilation_start = platform->MonotonicallyIncreasingTime();

  // These fields are initialized in a {BackgroundCompileScope} before
  // starting compilation.
  double deadline = 0;
  const bool deterministic = NeedsDeterministicCompile();
  base::Optional<CompilationEnv> env;
  std::shared_ptr<WireBytesStorage> wire_bytes;
  std::shared_ptr<const WasmModule> module;
  WasmEngine* wasm_engine = nullptr;
  base::Optional<WasmCompilationUnit> unit;
  WasmFeatures detected_features = WasmFeatures::None();

  auto stop = [is_foreground, task_id,
               &detected_features](BackgroundCompileScope& compile_scope) {
    if (is_foreground) {
      compile_scope.compilation_state()->UpdateDetectedFeatures(
          detected_features);
    } else {
      compile_scope.compilation_state()->OnBackgroundTaskStopped(
          task_id, detected_features);
    }
  };

  // Preparation (synchronized): Initialize the fields above and get the first
  // compilation unit.
  {
    BackgroundCompileScope compile_scope(token);
    if (compile_scope.cancelled()) return false;
    auto* compilation_state = compile_scope.compilation_state();
    deadline = compilation_state->GetCompilationDeadline(compilation_start);
    env.emplace(compile_scope.native_module()->CreateCompilationEnv());
    wire_bytes = compilation_state->GetWireBytesStorage();
    module = compile_scope.native_module()->shared_module();
    wasm_engine = compile_scope.native_module()->engine();
    unit = compilation_state->GetNextCompilationUnit(task_id, baseline_only);
    if (!unit) {
      stop(compile_scope);
      return false;
    }
  }

  std::vector<WasmCompilationResult> results_to_publish;

  auto publish_results = [&results_to_publish](
                             BackgroundCompileScope* compile_scope) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "PublishResults",
                 "num_results", results_to_publish.size());
    if (results_to_publish.empty()) return;
    WasmCodeRefScope code_ref_scope;
    std::vector<WasmCode*> code_vector =
        compile_scope->native_module()->AddCompiledCode(
            VectorOf(results_to_publish));

    // For import wrapper compilation units, add result to the cache.
    const NativeModule* native_module = compile_scope->native_module();
    int num_imported_functions = native_module->num_imported_functions();
    DCHECK_EQ(code_vector.size(), results_to_publish.size());
    WasmImportWrapperCache* cache = native_module->import_wrapper_cache();
    for (WasmCode* code : code_vector) {
      int func_index = code->index();
      DCHECK_LE(0, func_index);
      DCHECK_LT(func_index, native_module->num_functions());
      if (func_index < num_imported_functions) {
        const FunctionSig* sig =
            native_module->module()->functions[func_index].sig;
        WasmImportWrapperCache::CacheKey key(compiler::kDefaultImportCallKind,
                                             sig);
        // If two imported functions have the same key, only one of them should
        // have been added as a compilation unit. So it is always the first time
        // we compile a wrapper for this key here.
        DCHECK_NULL((*cache)[key]);
        (*cache)[key] = code;
        code->IncRef();
      }
    }

    native_module->engine()->LogCode(VectorOf(code_vector));

    compile_scope->compilation_state()->OnFinishedUnits(
        VectorOf(code_vector), VectorOf(results_to_publish));
    results_to_publish.clear();
  };

  bool compilation_failed = false;
  while (true) {
    // (asynchronous): Execute the compilation.
    WasmCompilationResult result = unit->ExecuteCompilation(
        wasm_engine, &env.value(), wire_bytes, counters, &detected_features);
    results_to_publish.emplace_back(std::move(result));

    // (synchronized): Publish the compilation result and get the next unit.
    {
      BackgroundCompileScope compile_scope(token);
      if (compile_scope.cancelled()) return true;
      if (!results_to_publish.back().succeeded()) {
        // Compile error.
        compile_scope.compilation_state()->SetError();
        stop(compile_scope);
        compilation_failed = true;
        break;
      }

      // Get next unit.
      if (deterministic || deadline < platform->MonotonicallyIncreasingTime()) {
        unit = {};
      } else {
        unit = compile_scope.compilation_state()->GetNextCompilationUnit(
            task_id, baseline_only);
      }

      if (!unit) {
        publish_results(&compile_scope);
        stop(compile_scope);
        return true;
      } else if (unit->tier() == ExecutionTier::kTurbofan) {
        // Before executing a TurboFan unit, ensure to publish all previous
        // units. If we compiled Liftoff before, we need to publish them anyway
        // to ensure fast completion of baseline compilation, if we compiled
        // TurboFan before, we publish to reduce peak memory consumption.
        publish_results(&compile_scope);
      }
    }
  }
  // We only get here if compilation failed. Other exits return directly.
  DCHECK(compilation_failed);
  USE(compilation_failed);
  token->Cancel();
  return true;
}

using JSToWasmWrapperKey = std::pair<bool, FunctionSig>;

// Returns the number of units added.
int AddExportWrapperUnits(Isolate* isolate, WasmEngine* wasm_engine,
                          NativeModule* native_module,
                          CompilationUnitBuilder* builder,
                          const WasmFeatures& enabled_features) {
  std::unordered_set<JSToWasmWrapperKey, base::hash<JSToWasmWrapperKey>> keys;
  for (auto exp : native_module->module()->export_table) {
    if (exp.kind != kExternalFunction) continue;
    auto& function = native_module->module()->functions[exp.index];
    JSToWasmWrapperKey key(function.imported, *function.sig);
    if (keys.insert(key).second) {
      auto unit = std::make_shared<JSToWasmWrapperCompilationUnit>(
          isolate, wasm_engine, function.sig, function.imported,
          enabled_features);
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
    if (!IsJSCompatibleSignature(sig, native_module->enabled_features())) {
      continue;
    }
    WasmImportWrapperCache::CacheKey key(compiler::kDefaultImportCallKind, sig);
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

void InitializeCompilationUnits(Isolate* isolate, NativeModule* native_module) {
  CompilationStateImpl* compilation_state =
      Impl(native_module->compilation_state());
  const bool lazy_module = IsLazyModule(native_module->module());
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  CompilationUnitBuilder builder(native_module);
  auto* module = native_module->module();
  const bool prefer_liftoff = native_module->IsTieredDown();

  uint32_t start = module->num_imported_functions;
  uint32_t end = start + module->num_declared_functions;
  for (uint32_t func_index = start; func_index < end; func_index++) {
    if (prefer_liftoff) {
      builder.AddRecompilationUnit(func_index, ExecutionTier::kLiftoff);
      continue;
    }
    CompileStrategy strategy = GetCompileStrategy(
        module, native_module->enabled_features(), func_index, lazy_module);
    if (strategy == CompileStrategy::kLazy) {
      native_module->UseLazyStub(func_index);
    } else if (strategy == CompileStrategy::kLazyBaselineEagerTopTier) {
      builder.AddTopTierUnit(func_index);
      native_module->UseLazyStub(func_index);
    } else {
      DCHECK_EQ(strategy, CompileStrategy::kEager);
      builder.AddUnits(func_index);
    }
  }
  int num_import_wrappers = AddImportWrapperUnits(native_module, &builder);
  int num_export_wrappers =
      AddExportWrapperUnits(isolate, isolate->wasm_engine(), native_module,
                            &builder, WasmFeatures::FromIsolate(isolate));
  compilation_state->InitializeCompilationProgress(
      lazy_module, num_import_wrappers + num_export_wrappers);
  builder.Commit();
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

class CompilationTimeCallback {
 public:
  enum CompileMode { kSynchronous, kAsync, kStreaming };
  explicit CompilationTimeCallback(std::shared_ptr<Counters> async_counters,
                                   CompileMode compile_mode)
      : start_time_(base::TimeTicks::Now()),
        async_counters_(std::move(async_counters)),
        compile_mode_(compile_mode) {}

  void operator()(CompilationEvent event) {
    DCHECK(base::TimeTicks::IsHighResolution());
    if (event == CompilationEvent::kFinishedBaselineCompilation) {
      auto now = base::TimeTicks::Now();
      auto duration = now - start_time_;
      // Reset {start_time_} to measure tier-up time.
      start_time_ = now;
      if (compile_mode_ != kSynchronous) {
        TimedHistogram* histogram =
            compile_mode_ == kAsync
                ? async_counters_->wasm_async_compile_wasm_module_time()
                : async_counters_->wasm_streaming_compile_wasm_module_time();
        histogram->AddSample(static_cast<int>(duration.InMicroseconds()));
      }
    }
    if (event == CompilationEvent::kFinishedTopTierCompilation) {
      auto duration = base::TimeTicks::Now() - start_time_;
      TimedHistogram* histogram = async_counters_->wasm_tier_up_module_time();
      histogram->AddSample(static_cast<int>(duration.InMicroseconds()));
    }
  }

 private:
  base::TimeTicks start_time_;
  const std::shared_ptr<Counters> async_counters_;
  const CompileMode compile_mode_;
};

void CompileNativeModule(Isolate* isolate, ErrorThrower* thrower,
                         const WasmModule* wasm_module,
                         NativeModule* native_module) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  const bool lazy_module = IsLazyModule(wasm_module);
  if (!FLAG_wasm_lazy_validation && wasm_module->origin == kWasmOrigin &&
      MayCompriseLazyFunctions(wasm_module, native_module->enabled_features(),
                               lazy_module)) {
    // Validate wasm modules for lazy compilation if requested. Never validate
    // asm.js modules as these are valid by construction (additionally a CHECK
    // will catch this during lazy compilation).
    ValidateSequentially(wasm_module, native_module, isolate->counters(),
                         isolate->allocator(), thrower, lazy_module,
                         kOnlyLazyFunctions);
    // On error: Return and leave the module in an unexecutable state.
    if (thrower->error()) return;
  }

  DCHECK_GE(kMaxInt, native_module->module()->num_declared_functions);

  // Install a callback to notify us once background compilation finished, or
  // compilation failed.
  auto baseline_finished_semaphore = std::make_shared<base::Semaphore>(0);
  // The callback captures a shared ptr to the semaphore.
  auto* compilation_state = Impl(native_module->compilation_state());
  compilation_state->AddCallback(
      [baseline_finished_semaphore](CompilationEvent event) {
        if (event == CompilationEvent::kFinishedBaselineCompilation ||
            event == CompilationEvent::kFailedCompilation) {
          baseline_finished_semaphore->Signal();
        }
      });
  if (base::TimeTicks::IsHighResolution()) {
    compilation_state->AddCallback(CompilationTimeCallback{
        isolate->async_counters(), CompilationTimeCallback::kSynchronous});
  }

  // Initialize the compilation units and kick off background compile tasks.
  InitializeCompilationUnits(isolate, native_module);

  // If tiering is disabled, the main thread can execute any unit (all of them
  // are part of initial compilation). Otherwise, just execute baseline units.
  bool is_tiering = compilation_state->compile_mode() == CompileMode::kTiering;
  auto baseline_only = is_tiering ? kBaselineOnly : kBaselineOrTopTier;
  // The main threads contributes to the compilation, except if we need
  // deterministic compilation; in that case, the single background task will
  // execute all compilation.
  if (!NeedsDeterministicCompile()) {
    while (ExecuteCompilationUnits(
        compilation_state->background_compile_token(), isolate->counters(),
        kMainThreadTaskId, baseline_only)) {
      // Continue executing compilation units.
    }
  }

  // Now wait until baseline compilation finished.
  baseline_finished_semaphore->Wait();

  compilation_state->PublishDetectedFeatures(isolate);

  if (compilation_state->failed()) {
    DCHECK_IMPLIES(lazy_module, !FLAG_wasm_lazy_validation);
    ValidateSequentially(wasm_module, native_module, isolate->counters(),
                         isolate->allocator(), thrower, lazy_module);
    CHECK(thrower->error());
  }
}

// The runnable task that performs compilations in the background.
class BackgroundCompileTask : public CancelableTask {
 public:
  explicit BackgroundCompileTask(CancelableTaskManager* manager,
                                 std::shared_ptr<BackgroundCompileToken> token,
                                 std::shared_ptr<Counters> async_counters,
                                 int task_id)
      : CancelableTask(manager),
        token_(std::move(token)),
        async_counters_(std::move(async_counters)),
        task_id_(task_id) {}

  void RunInternal() override {
    ExecuteCompilationUnits(token_, async_counters_.get(), task_id_,
                            kBaselineOrTopTier);
  }

 private:
  const std::shared_ptr<BackgroundCompileToken> token_;
  const std::shared_ptr<Counters> async_counters_;
  const int task_id_;
};

}  // namespace

std::shared_ptr<NativeModule> CompileToNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, ErrorThrower* thrower,
    std::shared_ptr<const WasmModule> module, const ModuleWireBytes& wire_bytes,
    Handle<FixedArray>* export_wrappers_out) {
  const WasmModule* wasm_module = module.get();
  std::shared_ptr<NativeModule> native_module =
      isolate->wasm_engine()->MaybeGetNativeModule(
          wasm_module->origin, wire_bytes.module_bytes(), isolate);
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
  OwnedVector<uint8_t> wire_bytes_copy =
      OwnedVector<uint8_t>::Of(wire_bytes.module_bytes());

  // Create a new {NativeModule} first.
  const bool uses_liftoff = module->origin == kWasmOrigin && FLAG_liftoff;
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module.get(),
                                                          uses_liftoff);
  native_module = isolate->wasm_engine()->NewNativeModule(
      isolate, enabled, module, code_size_estimate);
  native_module->SetWireBytes(std::move(wire_bytes_copy));

  CompileNativeModule(isolate, thrower, wasm_module, native_module.get());
  bool cache_hit = !isolate->wasm_engine()->UpdateNativeModuleCache(
      thrower->error(), &native_module, isolate);
  if (thrower->error()) return {};

  if (cache_hit) {
    CompileJsToWasmWrappers(isolate, wasm_module, export_wrappers_out);
    return native_module;
  }
  Impl(native_module->compilation_state())
      ->FinalizeJSToWasmWrappers(isolate, native_module->module(),
                                 export_wrappers_out);

  // Ensure that the code objects are logged before returning.
  isolate->wasm_engine()->LogOutstandingCodesForIsolate(isolate);

  return native_module;
}

void RecompileNativeModule(Isolate* isolate, NativeModule* native_module,
                           ExecutionTier tier) {
  // Install a callback to notify us once background recompilation finished.
  auto recompilation_finished_semaphore = std::make_shared<base::Semaphore>(0);
  auto* compilation_state = Impl(native_module->compilation_state());
  DCHECK(tier == ExecutionTier::kTurbofan || tier == ExecutionTier::kLiftoff);
  // The callback captures a shared ptr to the semaphore.
  // Initialize the compilation units and kick off background compile tasks.
  compilation_state->InitializeRecompilation(
      tier, [recompilation_finished_semaphore](CompilationEvent event) {
        if (event == CompilationEvent::kFinishedRecompilation) {
          recompilation_finished_semaphore->Signal();
        }
      });

  // For tier down only.
  if (tier == ExecutionTier::kLiftoff) {
    // The main thread contributes to the compilation, except if we need
    // deterministic compilation; in that case, the single background task will
    // execute all compilation.
    if (!NeedsDeterministicCompile()) {
      while (ExecuteCompilationUnits(
          compilation_state->background_compile_token(), isolate->counters(),
          kMainThreadTaskId, kBaselineOnly)) {
        // Continue executing compilation units.
      }
    }

    // Now wait until baseline recompilation finished.
    recompilation_finished_semaphore->Wait();
    DCHECK(!compilation_state->failed());
  }
}

AsyncCompileJob::AsyncCompileJob(
    Isolate* isolate, const WasmFeatures& enabled,
    std::unique_ptr<byte[]> bytes_copy, size_t length, Handle<Context> context,
    const char* api_method_name,
    std::shared_ptr<CompilationResultResolver> resolver)
    : isolate_(isolate),
      api_method_name_(api_method_name),
      enabled_features_(enabled),
      wasm_lazy_compilation_(FLAG_wasm_lazy_compilation),
      start_time_(base::TimeTicks::Now()),
      bytes_copy_(std::move(bytes_copy)),
      wire_bytes_(bytes_copy_.get(), bytes_copy_.get() + length),
      resolver_(std::move(resolver)) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "new AsyncCompileJob");
  CHECK(!FLAG_jitless);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Platform* platform = V8::GetCurrentPlatform();
  foreground_task_runner_ = platform->GetForegroundTaskRunner(v8_isolate);
  native_context_ =
      isolate->global_handles()->Create(context->native_context());
  DCHECK(native_context_->IsNativeContext());
}

void AsyncCompileJob::Start() {
  DoAsync<DecodeModule>(isolate_->counters());  // --
}

void AsyncCompileJob::Abort() {
  // Removing this job will trigger the destructor, which will cancel all
  // compilation.
  isolate_->wasm_engine()->RemoveCompileJob(this);
}

class AsyncStreamingProcessor final : public StreamingProcessor {
 public:
  explicit AsyncStreamingProcessor(AsyncCompileJob* job,
                                   std::shared_ptr<Counters> counters,
                                   AccountingAllocator* allocator);

  ~AsyncStreamingProcessor();

  bool ProcessModuleHeader(Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  bool ProcessSection(SectionCode section_code, Vector<const uint8_t> bytes,
                      uint32_t offset) override;

  bool ProcessCodeSectionHeader(int num_functions, uint32_t offset,
                                std::shared_ptr<WireBytesStorage>,
                                int code_section_length) override;

  bool ProcessFunctionBody(Vector<const uint8_t> bytes,
                           uint32_t offset) override;

  void OnFinishedChunk() override;

  void OnFinishedStream(OwnedVector<uint8_t> bytes) override;

  void OnError(const WasmError&) override;

  void OnAbort() override;

  bool Deserialize(Vector<const uint8_t> wire_bytes,
                   Vector<const uint8_t> module_bytes) override;

 private:
  // Finishes the AsyncCompileJob with an error.
  void FinishAsyncCompileJobWithError(const WasmError&);

  void CommitCompilationUnits();

  ModuleDecoder decoder_;
  AsyncCompileJob* job_;
  WasmEngine* wasm_engine_;
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
  stream_.reset(new StreamingDecoder(std::make_unique<AsyncStreamingProcessor>(
      this, isolate_->async_counters(), isolate_->allocator())));
  return stream_;
}

AsyncCompileJob::~AsyncCompileJob() {
  // Note: This destructor always runs on the foreground thread of the isolate.
  background_task_manager_.CancelAndWait();
  // If the runtime objects were not created yet, then initial compilation did
  // not finish yet. In this case we can abort compilation.
  if (native_module_ && module_object_.is_null()) {
    Impl(native_module_->compilation_state())->AbortCompilation();
  }
  // Tell the streaming decoder that the AsyncCompileJob is not available
  // anymore.
  // TODO(ahaas): Is this notification really necessary? Check
  // https://crbug.com/888170.
  if (stream_) stream_->NotifyCompilationEnded();
  CancelPendingForegroundTask();
  isolate_->global_handles()->Destroy(native_context_.location());
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

  native_module_ = isolate_->wasm_engine()->NewNativeModule(
      isolate_, enabled_features_, std::move(module), code_size_estimate);
  native_module_->SetWireBytes({std::move(bytes_copy_), wire_bytes_.length()});
}

bool AsyncCompileJob::GetOrCreateNativeModule(
    std::shared_ptr<const WasmModule> module, size_t code_size_estimate) {
  native_module_ = isolate_->wasm_engine()->MaybeGetNativeModule(
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
  const WasmModule* module = native_module_->module();
  auto source_url = stream_ ? stream_->url() : Vector<const char>();
  Handle<Script> script = CreateWasmScript(
      isolate_, native_module_->wire_bytes(), VectorOf(module->source_map_url),
      module->name, source_url);

  Handle<WasmModuleObject> module_object =
      WasmModuleObject::New(isolate_, native_module_, script);

  module_object_ = isolate_->global_handles()->Create(*module_object);
}

// This function assumes that it is executed in a HandleScope, and that a
// context is set on the isolate.
void AsyncCompileJob::FinishCompile(bool is_after_cache_hit) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "AsyncCompileJob::FinishCompile");
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
  }

  DCHECK(!isolate_->context().is_null());
  // Finish the wasm script now and make it public to the debugger.
  Handle<Script> script(module_object_->script(), isolate_);
  if (script->type() == Script::TYPE_WASM &&
      module_object_->module()->source_map_url.size() != 0) {
    MaybeHandle<String> src_map_str = isolate_->factory()->NewStringFromUtf8(
        CStrVector(module_object_->module()->source_map_url.c_str()),
        AllocationType::kOld);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "Debug::OnAfterCompile");
    isolate_->debug()->OnAfterCompile(script);
  }

  // TODO(bbudge) Allow deserialization without wrapper compilation, so we can
  // just compile wrappers here.
  if (!is_after_deserialization) {
    Handle<FixedArray> export_wrappers;
    if (is_after_cache_hit) {
      // TODO(thibaudm): Look into sharing wrappers.
      CompileJsToWasmWrappers(isolate_, module_object_->module(),
                              &export_wrappers);
    } else {
      compilation_state->FinalizeJSToWasmWrappers(
          isolate_, module_object_->module(), &export_wrappers);
    }
    module_object_->set_export_wrappers(*export_wrappers);
  }
  // We can only update the feature counts once the entire compile is done.
  compilation_state->PublishDetectedFeatures(isolate_);

  FinishModule();
}

void AsyncCompileJob::DecodeFailed(const WasmError& error) {
  ErrorThrower thrower(isolate_, api_method_name_);
  thrower.CompileFailed(error);
  // {job} keeps the {this} pointer alive.
  std::shared_ptr<AsyncCompileJob> job =
      isolate_->wasm_engine()->RemoveCompileJob(this);
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
      isolate_->wasm_engine()->RemoveCompileJob(this);
  resolver_->OnCompilationFailed(thrower.Reify());
}

void AsyncCompileJob::AsyncCompileSucceeded(Handle<WasmModuleObject> result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "CompilationResultResolver::OnCompilationSucceeded");
  resolver_->OnCompilationSucceeded(result);
}

class AsyncCompileJob::CompilationStateCallback {
 public:
  explicit CompilationStateCallback(AsyncCompileJob* job) : job_(job) {}

  void operator()(CompilationEvent event) {
    // This callback is only being called from a foreground task.
    switch (event) {
      case CompilationEvent::kFinishedBaselineCompilation:
        DCHECK(!last_event_.has_value());
        if (job_->DecrementAndCheckFinisherCount()) {
          // Install the native module in the cache, or reuse a conflicting one.
          // If we get a conflicting module, wait until we are back in the
          // main thread to update {job_->native_module_} to avoid a data race.
          std::shared_ptr<NativeModule> native_module = job_->native_module_;
          bool cache_hit =
              !job_->isolate_->wasm_engine()->UpdateNativeModuleCache(
                  false, &native_module, job_->isolate_);
          DCHECK_EQ(cache_hit, native_module != job_->native_module_);
          job_->DoSync<CompileFinished>(cache_hit ? std::move(native_module)
                                                  : nullptr);
        }
        break;
      case CompilationEvent::kFinishedTopTierCompilation:
        DCHECK_EQ(CompilationEvent::kFinishedBaselineCompilation, last_event_);
        // At this point, the job will already be gone, thus do not access it
        // here.
        break;
      case CompilationEvent::kFailedCompilation:
        DCHECK(!last_event_.has_value());
        if (job_->DecrementAndCheckFinisherCount()) {
          job_->isolate_->wasm_engine()->UpdateNativeModuleCache(
              true, &job_->native_module_, job_->isolate_);
          job_->DoSync<CompileFailed>();
        }
        break;
      case CompilationEvent::kFinishedRecompilation:
        // This event can happen either before or after
        // {kFinishedTopTierCompilation}, hence don't remember this in
        // {last_event_}.
        return;
      default:
        UNREACHABLE();
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
  explicit DecodeModule(Counters* counters) : counters_(counters) {}

  void RunInBackground(AsyncCompileJob* job) override {
    ModuleResult result;
    {
      DisallowHandleAllocation no_handle;
      DisallowHeapAllocation no_allocation;
      // Decode the module bytes.
      TRACE_COMPILE("(1) Decoding module...\n");
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
                   "AsyncCompileJob::DecodeModule");
      auto enabled_features = job->enabled_features_;
      result = DecodeWasmModule(enabled_features, job->wire_bytes_.start(),
                                job->wire_bytes_.end(), false, kWasmOrigin,
                                counters_,
                                job->isolate()->wasm_engine()->allocator());

      // Validate lazy functions here if requested.
      if (!FLAG_wasm_lazy_validation && result.ok()) {
        const WasmModule* module = result.value().get();
        DCHECK_EQ(module->origin, kWasmOrigin);
        const bool lazy_module = job->wasm_lazy_compilation_;
        if (MayCompriseLazyFunctions(module, enabled_features, lazy_module)) {
          auto allocator = job->isolate()->wasm_engine()->allocator();
          int start = module->num_imported_functions;
          int end = start + module->num_declared_functions;

          for (int func_index = start; func_index < end; func_index++) {
            const WasmFunction* func = &module->functions[func_index];
            Vector<const uint8_t> code =
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
      const bool kUsesLiftoff = false;
      size_t code_size_estimate =
          wasm::WasmCodeManager::EstimateNativeModuleCodeSize(module.get(),
                                                              kUsesLiftoff);
      job->DoSync<PrepareAndStartCompile>(std::move(module), true,
                                          code_size_estimate);
    }
  }

 private:
  Counters* const counters_;
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
  const std::shared_ptr<const WasmModule> module_;
  const bool start_compilation_;
  const size_t code_size_estimate_;

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
    compilation_state->AddCallback(CompilationStateCallback{job});
    if (base::TimeTicks::IsHighResolution()) {
      auto compile_mode = job->stream_ == nullptr
                              ? CompilationTimeCallback::kAsync
                              : CompilationTimeCallback::kStreaming;
      compilation_state->AddCallback(CompilationTimeCallback{
          job->isolate_->async_counters(), compile_mode});
    }

    if (start_compilation_) {
      // TODO(ahaas): Try to remove the {start_compilation_} check when
      // streaming decoding is done in the background. If
      // InitializeCompilationUnits always returns 0 for streaming compilation,
      // then DoAsync would do the same as NextStep already.

      // Add compilation units and kick off compilation.
      InitializeCompilationUnits(job->isolate(), job->native_module_.get());
    }
  }
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
class SampleTopTierCodeSizeCallback {
 public:
  explicit SampleTopTierCodeSizeCallback(
      std::weak_ptr<NativeModule> native_module)
      : native_module_(std::move(native_module)) {}

  void operator()(CompilationEvent event) {
    if (event != CompilationEvent::kFinishedTopTierCompilation) return;
    if (std::shared_ptr<NativeModule> native_module = native_module_.lock()) {
      native_module->engine()->SampleTopTierCodeSizeInAllIsolates(
          native_module);
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
          SampleTopTierCodeSizeCallback{job->native_module_});
    }
    // Then finalize and publish the generated module.
    job->FinishCompile(cached_native_module_ != nullptr);
  }

  std::shared_ptr<NativeModule> cached_native_module_;
};

void AsyncCompileJob::FinishModule() {
  TRACE_COMPILE("(4) Finish module...\n");
  AsyncCompileSucceeded(module_object_);
  isolate_->wasm_engine()->RemoveCompileJob(this);
}

AsyncStreamingProcessor::AsyncStreamingProcessor(
    AsyncCompileJob* job, std::shared_ptr<Counters> async_counters,
    AccountingAllocator* allocator)
    : decoder_(job->enabled_features_),
      job_(job),
      wasm_engine_(job_->isolate_->wasm_engine()),
      compilation_unit_builder_(nullptr),
      async_counters_(async_counters),
      allocator_(allocator) {}

AsyncStreamingProcessor::~AsyncStreamingProcessor() {
  if (job_->native_module_ && job_->native_module_->wire_bytes().empty()) {
    // Clean up the temporary cache entry.
    job_->isolate_->wasm_engine()->StreamingCompilationFailed(prefix_hash_);
  }
}

void AsyncStreamingProcessor::FinishAsyncCompileJobWithError(
    const WasmError& error) {
  DCHECK(error.has_error());
  // Make sure all background tasks stopped executing before we change the state
  // of the AsyncCompileJob to DecodeFail.
  job_->background_task_manager_.CancelAndWait();

  // Check if there is already a CompiledModule, in which case we have to clean
  // up the CompilationStateImpl as well.
  if (job_->native_module_) {
    Impl(job_->native_module_->compilation_state())->AbortCompilation();

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
bool AsyncStreamingProcessor::ProcessModuleHeader(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
  TRACE_STREAMING("Process module header...\n");
  decoder_.StartDecoding(job_->isolate()->counters(),
                         job_->isolate()->wasm_engine()->allocator());
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
                                             Vector<const uint8_t> bytes,
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
    int num_functions, uint32_t offset,
    std::shared_ptr<WireBytesStorage> wire_bytes_storage,
    int code_section_length) {
  DCHECK_LE(0, code_section_length);
  before_code_section_ = false;
  TRACE_STREAMING("Start the code section with %d functions...\n",
                  num_functions);
  if (!decoder_.CheckFunctionsCount(static_cast<uint32_t>(num_functions),
                                    offset)) {
    FinishAsyncCompileJobWithError(decoder_.FinishDecoding(false).error());
    return false;
  }

  prefix_hash_ = base::hash_combine(prefix_hash_,
                                    static_cast<uint32_t>(code_section_length));
  if (!wasm_engine_->GetStreamingCompilationOwnership(prefix_hash_)) {
    // Known prefix, wait until the end of the stream and check the cache.
    prefix_cache_hit_ = true;
    return true;
  }

  // Execute the PrepareAndStartCompile step immediately and not in a separate
  // task.
  int num_imported_functions =
      static_cast<int>(decoder_.module()->num_imported_functions);
  DCHECK_EQ(kWasmOrigin, decoder_.module()->origin);
  const bool uses_liftoff = FLAG_liftoff;
  size_t code_size_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          num_functions, num_imported_functions, code_section_length,
          uses_liftoff);
  job_->DoImmediately<AsyncCompileJob::PrepareAndStartCompile>(
      decoder_.shared_module(), false, code_size_estimate);

  decoder_.set_code_section(offset, static_cast<uint32_t>(code_section_length));
  auto* compilation_state = Impl(job_->native_module_->compilation_state());
  compilation_state->SetWireBytesStorage(std::move(wire_bytes_storage));
  DCHECK_EQ(job_->native_module_->module()->origin, kWasmOrigin);
  const bool lazy_module = job_->wasm_lazy_compilation_;

  // Set outstanding_finishers_ to 2, because both the AsyncCompileJob and the
  // AsyncStreamingProcessor have to finish.
  job_->outstanding_finishers_.store(2);
  compilation_unit_builder_.reset(
      new CompilationUnitBuilder(job_->native_module_.get()));

  NativeModule* native_module = job_->native_module_.get();

  int num_import_wrappers =
      AddImportWrapperUnits(native_module, compilation_unit_builder_.get());
  int num_export_wrappers = AddExportWrapperUnits(
      job_->isolate_, wasm_engine_, native_module,
      compilation_unit_builder_.get(), job_->enabled_features_);
  compilation_state->InitializeCompilationProgress(
      lazy_module, num_import_wrappers + num_export_wrappers);
  return true;
}

// Process a function body.
bool AsyncStreamingProcessor::ProcessFunctionBody(Vector<const uint8_t> bytes,
                                                  uint32_t offset) {
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

  NativeModule* native_module = job_->native_module_.get();
  if (strategy == CompileStrategy::kLazy) {
    native_module->UseLazyStub(func_index);
  } else if (strategy == CompileStrategy::kLazyBaselineEagerTopTier) {
    compilation_unit_builder_->AddTopTierUnit(func_index);
    native_module->UseLazyStub(func_index);
  } else {
    DCHECK_EQ(strategy, CompileStrategy::kEager);
    compilation_unit_builder_->AddUnits(func_index);
  }

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
void AsyncStreamingProcessor::OnFinishedStream(OwnedVector<uint8_t> bytes) {
  TRACE_STREAMING("Finish stream...\n");
  DCHECK_EQ(NativeModuleCache::PrefixHash(bytes.as_vector()), prefix_hash_);
  ModuleResult result = decoder_.FinishDecoding(false);
  if (result.failed()) {
    FinishAsyncCompileJobWithError(result.error());
    return;
  }

  job_->wire_bytes_ = ModuleWireBytes(bytes.as_vector());
  job_->bytes_copy_ = bytes.ReleaseData();

  if (prefix_cache_hit_) {
    // Restart as an asynchronous, non-streaming compilation. Most likely
    // {PrepareAndStartCompile} will get the native module from the cache.
    size_t code_size_estimate =
        wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
            result.value().get(), FLAG_liftoff);
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
      cache_hit = !job_->isolate_->wasm_engine()->UpdateNativeModuleCache(
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

bool AsyncStreamingProcessor::Deserialize(Vector<const uint8_t> module_bytes,
                                          Vector<const uint8_t> wire_bytes) {
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

int GetMaxBackgroundTasks() {
  if (NeedsDeterministicCompile()) return 0;
  int num_worker_threads = V8::GetCurrentPlatform()->NumberOfWorkerThreads();
  return std::min(FLAG_wasm_num_compilation_tasks, num_worker_threads);
}

CompilationStateImpl::CompilationStateImpl(
    const std::shared_ptr<NativeModule>& native_module,
    std::shared_ptr<Counters> async_counters)
    : native_module_(native_module.get()),
      background_compile_token_(
          std::make_shared<BackgroundCompileToken>(native_module)),
      compile_mode_(FLAG_wasm_tier_up &&
                            native_module->module()->origin == kWasmOrigin
                        ? CompileMode::kTiering
                        : CompileMode::kRegular),
      async_counters_(std::move(async_counters)),
      max_background_tasks_(std::max(GetMaxBackgroundTasks(), 1)),
      compilation_unit_queues_(max_background_tasks_),
      available_task_ids_(max_background_tasks_) {
  for (int i = 0; i < max_background_tasks_; ++i) {
    // Ids are popped on task creation, so reverse this list. This ensures that
    // the first background task gets id 0.
    available_task_ids_[i] = max_background_tasks_ - 1 - i;
  }
}

void CompilationStateImpl::AbortCompilation() {
  background_compile_token_->Cancel();
  // No more callbacks after abort.
  base::MutexGuard callbacks_guard(&callbacks_mutex_);
  callbacks_.clear();
}

void CompilationStateImpl::InitializeCompilationProgress(bool lazy_module,
                                                         int num_wrappers) {
  DCHECK(!failed());
  auto enabled_features = native_module_->enabled_features();
  auto* module = native_module_->module();

  base::MutexGuard guard(&callbacks_mutex_);
  DCHECK_EQ(0, outstanding_baseline_units_);
  DCHECK_EQ(0, outstanding_top_tier_functions_);
  compilation_progress_.reserve(module->num_declared_functions);
  int start = module->num_imported_functions;
  int end = start + module->num_declared_functions;

  const bool prefer_liftoff = native_module_->IsTieredDown();
  for (int func_index = start; func_index < end; func_index++) {
    if (prefer_liftoff) {
      constexpr uint8_t kLiftoffOnlyFunctionProgress =
          RequiredTopTierField::update(
              RequiredBaselineTierField::update(
                  ReachedTierField::encode(ExecutionTier::kNone),
                  ExecutionTier::kLiftoff),
              ExecutionTier::kLiftoff);
      compilation_progress_.push_back(kLiftoffOnlyFunctionProgress);
      outstanding_baseline_units_++;
      outstanding_top_tier_functions_++;
      continue;
    }
    ExecutionTierPair requested_tiers = GetRequestedExecutionTiers(
        module, compile_mode(), enabled_features, func_index);
    CompileStrategy strategy =
        GetCompileStrategy(module, enabled_features, func_index, lazy_module);

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
    uint8_t function_progress = ReachedTierField::encode(ExecutionTier::kNone);
    function_progress = RequiredBaselineTierField::update(
        function_progress, required_baseline_tier);
    function_progress =
        RequiredTopTierField::update(function_progress, required_top_tier);
    compilation_progress_.push_back(function_progress);
  }
  DCHECK_IMPLIES(lazy_module, outstanding_baseline_units_ == 0);
  DCHECK_IMPLIES(lazy_module, outstanding_top_tier_functions_ == 0);
  DCHECK_LE(0, outstanding_baseline_units_);
  DCHECK_LE(outstanding_baseline_units_, outstanding_top_tier_functions_);
  outstanding_baseline_units_ += num_wrappers;

  // Trigger callbacks if module needs no baseline or top tier compilation. This
  // can be the case for an empty or fully lazy module.
  if (outstanding_baseline_units_ == 0) {
    for (auto& callback : callbacks_) {
      callback(CompilationEvent::kFinishedBaselineCompilation);
    }
    if (outstanding_top_tier_functions_ == 0) {
      for (auto& callback : callbacks_) {
        callback(CompilationEvent::kFinishedTopTierCompilation);
      }
      // Clear the callbacks because no more events will be delivered.
      callbacks_.clear();
    }
  }
}

void CompilationStateImpl::InitializeRecompilation(
    ExecutionTier tier,
    CompilationState::callback_t recompilation_finished_callback) {
  DCHECK(!failed());

  // Generate necessary compilation units on the fly.
  CompilationUnitBuilder builder(native_module_);

  {
    base::MutexGuard guard(&callbacks_mutex_);

    // Restart recompilation if another recompilation is already happening.
    outstanding_recompilation_functions_ = 0;
    // If compilation hasn't started yet then code would be keep as tiered-down
    // and don't need to recompile.
    if (compilation_progress_.size() > 0) {
      int start = native_module_->module()->num_imported_functions;
      int end = start + native_module_->module()->num_declared_functions;
      for (int function_index = start; function_index < end; function_index++) {
        int slot_index = function_index - start;
        DCHECK_LT(slot_index, compilation_progress_.size());
        ExecutionTier reached_tier =
            ReachedTierField::decode(compilation_progress_[slot_index]);
        bool has_correct_tier =
            reached_tier == tier &&
            native_module_->HasCodeWithTier(function_index, tier);
        compilation_progress_[slot_index] =
            ReachedRecompilationTierField::update(
                compilation_progress_[slot_index],
                has_correct_tier ? tier : ExecutionTier::kNone);
        if (!has_correct_tier) {
          outstanding_recompilation_functions_++;
          builder.AddRecompilationUnit(function_index, tier);
        }
      }
    }

    // Trigger callback if module needs no recompilation. Add to the list of
    // callbacks (to be called later) otherwise.
    if (outstanding_recompilation_functions_ == 0) {
      recompilation_finished_callback(CompilationEvent::kFinishedRecompilation);
    } else {
      callbacks_.emplace_back(std::move(recompilation_finished_callback));
      recompilation_tier_ = tier;
    }
  }

  builder.Commit();
}

void CompilationStateImpl::AddCallback(CompilationState::callback_t callback) {
  base::MutexGuard callbacks_guard(&callbacks_mutex_);
  // Immediately trigger events that already happened.
  for (auto event : {CompilationEvent::kFinishedBaselineCompilation,
                     CompilationEvent::kFinishedTopTierCompilation,
                     CompilationEvent::kFailedCompilation}) {
    if (finished_events_.contains(event)) {
      callback(event);
    }
  }
  constexpr base::EnumSet<CompilationEvent> kFinalEvents{
      CompilationEvent::kFinishedTopTierCompilation,
      CompilationEvent::kFailedCompilation};
  if (!finished_events_.contains_any(kFinalEvents)) {
    callbacks_.emplace_back(std::move(callback));
  }
}

void CompilationStateImpl::AddCompilationUnits(
    Vector<WasmCompilationUnit> baseline_units,
    Vector<WasmCompilationUnit> top_tier_units,
    Vector<std::shared_ptr<JSToWasmWrapperCompilationUnit>>
        js_to_wasm_wrapper_units) {
  if (!baseline_units.empty() || !top_tier_units.empty()) {
    compilation_unit_queues_.AddUnits(baseline_units, top_tier_units,
                                      native_module_->module());
  }
  js_to_wasm_wrapper_units_.insert(js_to_wasm_wrapper_units_.end(),
                                   js_to_wasm_wrapper_units.begin(),
                                   js_to_wasm_wrapper_units.end());

  RestartBackgroundTasks();
}

void CompilationStateImpl::AddTopTierCompilationUnit(WasmCompilationUnit unit) {
  AddCompilationUnits({}, {&unit, 1}, {});
}

std::shared_ptr<JSToWasmWrapperCompilationUnit>
CompilationStateImpl::GetNextJSToWasmWrapperCompilationUnit() {
  int wrapper_id =
      js_to_wasm_wrapper_id_.fetch_add(1, std::memory_order_relaxed);
  if (wrapper_id < static_cast<int>(js_to_wasm_wrapper_units_.size())) {
    return js_to_wasm_wrapper_units_[wrapper_id];
  }
  return nullptr;
}

void CompilationStateImpl::FinalizeJSToWasmWrappers(
    Isolate* isolate, const WasmModule* module,
    Handle<FixedArray>* export_wrappers_out) {
  *export_wrappers_out = isolate->factory()->NewFixedArray(
      MaxNumExportWrappers(module), AllocationType::kOld);
  // TODO(6792): Wrappers below are allocated with {Factory::NewCode}. As an
  // optimization we keep the code space unlocked to avoid repeated unlocking
  // because many such wrapper are allocated in sequence below.
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "FinalizeJSToWasmWrappers",
               "num_wrappers", js_to_wasm_wrapper_units_.size());
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  for (auto& unit : js_to_wasm_wrapper_units_) {
    Handle<Code> code = unit->Finalize(isolate);
    int wrapper_index =
        GetExportWrapperIndex(module, unit->sig(), unit->is_import());
    (*export_wrappers_out)->set(wrapper_index, *code);
    RecordStats(*code, isolate->counters());
  }
}

base::Optional<WasmCompilationUnit>
CompilationStateImpl::GetNextCompilationUnit(
    int task_id, CompileBaselineOnly baseline_only) {
  return compilation_unit_queues_.GetNextUnit(task_id, baseline_only);
}

void CompilationStateImpl::OnFinishedUnits(
    Vector<WasmCode*> code_vector, Vector<WasmCompilationResult> results) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), "OnFinishedUnits",
               "num_units", code_vector.size());

  base::MutexGuard guard(&callbacks_mutex_);

  // In case of no outstanding compilation units we can return early.
  // This is especially important for lazy modules that were deserialized.
  // Compilation progress was not set up in these cases.
  if (outstanding_baseline_units_ == 0 &&
      outstanding_top_tier_functions_ == 0 &&
      outstanding_recompilation_functions_ == 0) {
    return;
  }

  // Assume an order of execution tiers that represents the quality of their
  // generated code.
  static_assert(ExecutionTier::kNone < ExecutionTier::kInterpreter &&
                    ExecutionTier::kInterpreter < ExecutionTier::kLiftoff &&
                    ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                "Assume an order on execution tiers");

  DCHECK_EQ(compilation_progress_.size(),
            native_module_->module()->num_declared_functions);

  base::EnumSet<CompilationEvent> triggered_events;

  for (size_t i = 0; i < code_vector.size(); i++) {
    WasmCode* code = code_vector[i];
    DCHECK_NOT_NULL(code);
    DCHECK_LT(code->index(), native_module_->num_functions());

    if (code->index() < native_module_->num_imported_functions()) {
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
      if (reached_tier < required_top_tier &&
          required_top_tier <= code->tier()) {
        DCHECK_GT(outstanding_top_tier_functions_, 0);
        outstanding_top_tier_functions_--;
      }

      // If there is recompilation in progress, we would only count the
      // functions which are not Liftoff already, and would only decrement the
      // counter once a function reaches Liftoff.
      if (outstanding_recompilation_functions_ > 0) {
        // TODO(duongn): extend this logic for tier up.
        ExecutionTier recompilation_tier =
            ReachedRecompilationTierField::decode(function_progress);
        if (results[i].requested_tier == recompilation_tier_ &&
            recompilation_tier == ExecutionTier::kNone) {
          DCHECK(code->tier() >= recompilation_tier_);
          outstanding_recompilation_functions_--;
          // Update function's recompilation progress.
          compilation_progress_[slot_index] =
              ReachedRecompilationTierField::update(
                  compilation_progress_[slot_index], code->tier());
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
  DCHECK_GE(outstanding_baseline_units_, num);
  outstanding_baseline_units_ -= num;
  TriggerCallbacks();
}

void CompilationStateImpl::TriggerCallbacks(
    base::EnumSet<CompilationEvent> triggered_events) {
  DCHECK(!callbacks_mutex_.TryLock());

  if (outstanding_baseline_units_ == 0) {
    triggered_events.Add(CompilationEvent::kFinishedBaselineCompilation);
    if (outstanding_top_tier_functions_ == 0) {
      triggered_events.Add(CompilationEvent::kFinishedTopTierCompilation);
    }
  }

  if (triggered_events.empty()) return;

  // Don't trigger past events again.
  triggered_events -= finished_events_;
  // Recompilation can happen multiple times, thus do not store this.
  finished_events_ |=
      triggered_events - CompilationEvent::kFinishedRecompilation;

  for (auto event :
       {std::make_pair(CompilationEvent::kFinishedBaselineCompilation,
                       "BaselineFinished"),
        std::make_pair(CompilationEvent::kFinishedTopTierCompilation,
                       "TopTierFinished"),
        std::make_pair(CompilationEvent::kFinishedRecompilation,
                       "RecompilationFinished")}) {
    if (!triggered_events.contains(event.first)) continue;
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"), event.second);
    for (auto& callback : callbacks_) {
      callback(event.first);
    }
  }

  if (outstanding_baseline_units_ == 0 &&
      outstanding_top_tier_functions_ == 0 &&
      outstanding_recompilation_functions_ == 0) {
    // Clear the callbacks because no more events will be delivered.
    callbacks_.clear();
  }
}

void CompilationStateImpl::OnBackgroundTaskStopped(
    int task_id, const WasmFeatures& detected) {
  {
    base::MutexGuard guard(&mutex_);
    DCHECK_EQ(0, std::count(available_task_ids_.begin(),
                            available_task_ids_.end(), task_id));
    DCHECK_GT(max_background_tasks_, available_task_ids_.size());
    available_task_ids_.push_back(task_id);
    detected_features_.Add(detected);
  }

  // The background task could have stopped while we were adding new units, or
  // because it reached its deadline. In both cases we need to restart tasks to
  // avoid a potential deadlock.
  RestartBackgroundTasks();
}

void CompilationStateImpl::UpdateDetectedFeatures(
    const WasmFeatures& detected) {
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

void CompilationStateImpl::RestartBackgroundTasks() {
  // Create new tasks, but only spawn them after releasing the mutex, because
  // some platforms (e.g. the predictable platform) might execute tasks right
  // away.
  std::vector<std::unique_ptr<Task>> new_tasks;
  {
    base::MutexGuard guard(&mutex_);
    // Explicit fast path (quite common): If no more task ids are available
    // (i.e. {max_background_tasks_} tasks are already running), spawn nothing.
    if (available_task_ids_.empty()) return;
    // No need to restart tasks if compilation already failed.
    if (failed()) return;

    size_t max_num_restart = compilation_unit_queues_.GetTotalSize();
    if (js_to_wasm_wrapper_id_ <
        static_cast<int>(js_to_wasm_wrapper_units_.size())) {
      max_num_restart +=
          js_to_wasm_wrapper_units_.size() - js_to_wasm_wrapper_id_;
    }

    while (!available_task_ids_.empty() && max_num_restart-- > 0) {
      int task_id = available_task_ids_.back();
      available_task_ids_.pop_back();
      new_tasks.emplace_back(
          native_module_->engine()
              ->NewBackgroundCompileTask<BackgroundCompileTask>(
                  background_compile_token_, async_counters_, task_id));
    }
  }

  if (baseline_compilation_finished() && recompilation_finished()) {
    for (auto& task : new_tasks) {
      V8::GetCurrentPlatform()->CallLowPriorityTaskOnWorkerThread(
          std::move(task));
    }
  } else {
    for (auto& task : new_tasks) {
      V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    }
  }
}

void CompilationStateImpl::SetError() {
  bool expected = false;
  if (!compile_failed_.compare_exchange_strong(expected, true,
                                               std::memory_order_relaxed)) {
    return;  // Already failed before.
  }

  base::MutexGuard callbacks_guard(&callbacks_mutex_);
  for (auto& callback : callbacks_) {
    callback(CompilationEvent::kFailedCompilation);
  }
  // No more callbacks after an error.
  callbacks_.clear();
}

namespace {
using JSToWasmWrapperQueue =
    WrapperQueue<JSToWasmWrapperKey, base::hash<JSToWasmWrapperKey>>;
using JSToWasmWrapperUnitMap =
    std::unordered_map<JSToWasmWrapperKey,
                       std::unique_ptr<JSToWasmWrapperCompilationUnit>,
                       base::hash<JSToWasmWrapperKey>>;

class CompileJSToWasmWrapperTask final : public CancelableTask {
 public:
  CompileJSToWasmWrapperTask(CancelableTaskManager* task_manager,
                             JSToWasmWrapperQueue* queue,
                             JSToWasmWrapperUnitMap* compilation_units)
      : CancelableTask(task_manager),
        queue_(queue),
        compilation_units_(compilation_units) {}

  void RunInternal() override {
    while (base::Optional<JSToWasmWrapperKey> key = queue_->pop()) {
      JSToWasmWrapperCompilationUnit* unit = (*compilation_units_)[*key].get();
      unit->Execute();
    }
  }

 private:
  JSToWasmWrapperQueue* const queue_;
  JSToWasmWrapperUnitMap* const compilation_units_;
};
}  // namespace

void CompileJsToWasmWrappers(Isolate* isolate, const WasmModule* module,
                             Handle<FixedArray>* export_wrappers_out) {
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
          isolate, isolate->wasm_engine(), function.sig, function.imported,
          enabled_features);
      compilation_units.emplace(key, std::move(unit));
    }
  }

  // Execute compilation jobs in the background.
  CancelableTaskManager task_manager;
  const int max_background_tasks = GetMaxBackgroundTasks();
  for (int i = 0; i < max_background_tasks; ++i) {
    auto task = std::make_unique<CompileJSToWasmWrapperTask>(
        &task_manager, &queue, &compilation_units);
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
  }

  // Work in the main thread too.
  while (base::Optional<JSToWasmWrapperKey> key = queue.pop()) {
    JSToWasmWrapperCompilationUnit* unit = compilation_units[*key].get();
    unit->Execute();
  }
  task_manager.CancelAndWait();

  // Finalize compilation jobs in the main thread.
  // TODO(6792): Wrappers below are allocated with {Factory::NewCode}. As an
  // optimization we keep the code space unlocked to avoid repeated unlocking
  // because many such wrapper are allocated in sequence below.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  for (auto& pair : compilation_units) {
    JSToWasmWrapperKey key = pair.first;
    JSToWasmWrapperCompilationUnit* unit = pair.second.get();
    Handle<Code> code = unit->Finalize(isolate);
    int wrapper_index = GetExportWrapperIndex(module, &key.second, key.first);
    (*export_wrappers_out)->set(wrapper_index, *code);
    RecordStats(*code, isolate->counters());
  }
}

WasmCode* CompileImportWrapper(
    WasmEngine* wasm_engine, NativeModule* native_module, Counters* counters,
    compiler::WasmImportCallKind kind, const FunctionSig* sig,
    WasmImportWrapperCache::ModificationScope* cache_scope) {
  // Entry should exist, so that we don't insert a new one and invalidate
  // other threads' iterators/references, but it should not have been compiled
  // yet.
  WasmImportWrapperCache::CacheKey key(kind, sig);
  DCHECK_NULL((*cache_scope)[key]);
  bool source_positions = is_asmjs_module(native_module->module());
  // Keep the {WasmCode} alive until we explicitly call {IncRef}.
  WasmCodeRefScope code_ref_scope;
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = compiler::CompileWasmImportCallWrapper(
      wasm_engine, &env, kind, sig, source_positions);
  std::unique_ptr<WasmCode> wasm_code = native_module->AddCode(
      result.func_index, result.code_desc, result.frame_slot_count,
      result.tagged_parameter_slots,
      result.protected_instructions_data.as_vector(),
      result.source_positions.as_vector(), GetCodeKind(result),
      ExecutionTier::kNone);
  WasmCode* published_code = native_module->PublishCode(std::move(wasm_code));
  (*cache_scope)[key] = published_code;
  published_code->IncRef();
  counters->wasm_generated_code_size()->Increment(
      published_code->instructions().length());
  counters->wasm_reloc_size()->Increment(published_code->reloc_info().length());
  return published_code;
}

Handle<Script> CreateWasmScript(Isolate* isolate,
                                Vector<const uint8_t> wire_bytes,
                                Vector<const char> source_map_url,
                                WireBytesRef name,
                                Vector<const char> source_url) {
  Handle<Script> script =
      isolate->factory()->NewScript(isolate->factory()->empty_string());
  script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);
  script->set_context_data(isolate->native_context()->debug_context_id());
  script->set_type(Script::TYPE_WASM);

  int hash = StringHasher::HashSequentialString(
      reinterpret_cast<const char*>(wire_bytes.begin()), wire_bytes.length(),
      kZeroHashSeed);

  const int kBufferSize = 32;
  char buffer[kBufferSize];

  // Script name is "<module_name>-hash" if name is available and "hash"
  // otherwise.
  Handle<String> name_str;
  if (name.is_set()) {
    int name_chars = SNPrintF(ArrayVector(buffer), "-%08x", hash);
    DCHECK(name_chars >= 0 && name_chars < kBufferSize);
    Handle<String> name_hash =
        isolate->factory()
            ->NewStringFromOneByte(
                VectorOf(reinterpret_cast<uint8_t*>(buffer), name_chars),
                AllocationType::kOld)
            .ToHandleChecked();
    Handle<String> module_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, wire_bytes, name, kNoInternalize);
    name_str = isolate->factory()
                   ->NewConsString(module_name, name_hash)
                   .ToHandleChecked();
  } else {
    int name_chars = SNPrintF(ArrayVector(buffer), "%08x", hash);
    DCHECK(name_chars >= 0 && name_chars < kBufferSize);
    name_str = isolate->factory()
                   ->NewStringFromOneByte(
                       VectorOf(reinterpret_cast<uint8_t*>(buffer), name_chars),
                       AllocationType::kOld)
                   .ToHandleChecked();
  }
  script->set_name(*name_str);
  MaybeHandle<String> url_str;
  if (!source_url.empty()) {
    url_str =
        isolate->factory()->NewStringFromUtf8(source_url, AllocationType::kOld);
  } else {
    Handle<String> url_prefix =
        isolate->factory()->InternalizeString(StaticCharVector("wasm://wasm/"));
    url_str = isolate->factory()->NewConsString(url_prefix, name_str);
  }
  script->set_source_url(*url_str.ToHandleChecked());

  if (!source_map_url.empty()) {
    MaybeHandle<String> src_map_str = isolate->factory()->NewStringFromUtf8(
        source_map_url, AllocationType::kOld);
    script->set_source_mapping_url(*src_map_str.ToHandleChecked());
  }
  return script;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef TRACE_COMPILE
#undef TRACE_STREAMING
#undef TRACE_LAZY
