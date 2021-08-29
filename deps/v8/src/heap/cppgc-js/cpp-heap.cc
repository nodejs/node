// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/cpp-heap.h"

#include <cstdint>
#include <memory>
#include <numeric>

#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/platform.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate-inl.h"
#include "src/flags/flags.h"
#include "src/handles/handles.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc-js/cpp-snapshot.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc-js/unified-heap-marking-verifier.h"
#include "src/heap/cppgc-js/unified-heap-marking-visitor.h"
#include "src/heap/cppgc/concurrent-marker.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/metric-recorder.h"
#include "src/heap/cppgc/object-allocator.h"
#include "src/heap/cppgc/prefinalizer-handler.h"
#include "src/heap/cppgc/raw-heap.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/sweeper.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/sweeper.h"
#include "src/init/v8.h"
#include "src/profiler/heap-profiler.h"

namespace v8 {

// static
constexpr uint16_t WrapperDescriptor::kUnknownEmbedderId;

// static
std::unique_ptr<CppHeap> CppHeap::Create(v8::Platform* platform,
                                         const CppHeapCreateParams& params) {
  return std::make_unique<internal::CppHeap>(platform, params.custom_spaces,
                                             params.wrapper_descriptor);
}

cppgc::AllocationHandle& CppHeap::GetAllocationHandle() {
  return internal::CppHeap::From(this)->object_allocator();
}

cppgc::HeapHandle& CppHeap::GetHeapHandle() {
  return *internal::CppHeap::From(this);
}

void CppHeap::Terminate() { internal::CppHeap::From(this)->Terminate(); }

cppgc::HeapStatistics CppHeap::CollectStatistics(
    cppgc::HeapStatistics::DetailLevel detail_level) {
  return internal::CppHeap::From(this)->AsBase().CollectStatistics(
      detail_level);
}

void CppHeap::CollectCustomSpaceStatisticsAtLastGC(
    std::vector<cppgc::CustomSpaceIndex> custom_spaces,
    std::unique_ptr<CustomSpaceStatisticsReceiver> receiver) {
  return internal::CppHeap::From(this)->CollectCustomSpaceStatisticsAtLastGC(
      std::move(custom_spaces), std::move(receiver));
}

void CppHeap::EnableDetachedGarbageCollectionsForTesting() {
  return internal::CppHeap::From(this)
      ->EnableDetachedGarbageCollectionsForTesting();
}

void CppHeap::CollectGarbageForTesting(cppgc::EmbedderStackState stack_state) {
  return internal::CppHeap::From(this)->CollectGarbageForTesting(stack_state);
}

void JSHeapConsistency::DijkstraMarkingBarrierSlow(
    cppgc::HeapHandle& heap_handle, const TracedReferenceBase& ref) {
  auto& heap_base = cppgc::internal::HeapBase::From(heap_handle);
  static_cast<JSVisitor*>(&heap_base.marker()->Visitor())->Trace(ref);
}

void JSHeapConsistency::CheckWrapper(v8::Local<v8::Object>& wrapper,
                                     int wrapper_index, const void* wrappable) {
  CHECK_EQ(wrappable,
           wrapper->GetAlignedPointerFromInternalField(wrapper_index));
}

namespace internal {

namespace {

class CppgcPlatformAdapter final : public cppgc::Platform {
 public:
  explicit CppgcPlatformAdapter(v8::Platform* platform) : platform_(platform) {}

  CppgcPlatformAdapter(const CppgcPlatformAdapter&) = delete;
  CppgcPlatformAdapter& operator=(const CppgcPlatformAdapter&) = delete;

  PageAllocator* GetPageAllocator() final {
    return platform_->GetPageAllocator();
  }

  double MonotonicallyIncreasingTime() final {
    return platform_->MonotonicallyIncreasingTime();
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner() final {
    // If no Isolate has been set, there's no task runner to leverage for
    // foreground tasks. In detached mode the original platform handles the
    // task runner retrieval.
    if (!isolate_ && !is_in_detached_mode_) return nullptr;

    return platform_->GetForegroundTaskRunner(isolate_);
  }

  std::unique_ptr<JobHandle> PostJob(TaskPriority priority,
                                     std::unique_ptr<JobTask> job_task) final {
    return platform_->PostJob(priority, std::move(job_task));
  }

  TracingController* GetTracingController() override {
    return platform_->GetTracingController();
  }

  void SetIsolate(v8::Isolate* isolate) { isolate_ = isolate; }
  void EnableDetachedModeForTesting() { is_in_detached_mode_ = true; }

 private:
  v8::Platform* platform_;
  v8::Isolate* isolate_ = nullptr;
  bool is_in_detached_mode_ = false;
};

class UnifiedHeapConcurrentMarker
    : public cppgc::internal::ConcurrentMarkerBase {
 public:
  UnifiedHeapConcurrentMarker(
      cppgc::internal::HeapBase& heap,
      cppgc::internal::MarkingWorklists& marking_worklists,
      cppgc::internal::IncrementalMarkingSchedule& incremental_marking_schedule,
      cppgc::Platform* platform,
      UnifiedHeapMarkingState& unified_heap_marking_state)
      : cppgc::internal::ConcurrentMarkerBase(
            heap, marking_worklists, incremental_marking_schedule, platform),
        unified_heap_marking_state_(unified_heap_marking_state) {}

  std::unique_ptr<cppgc::Visitor> CreateConcurrentMarkingVisitor(
      cppgc::internal::ConcurrentMarkingState&) const final;

 private:
  UnifiedHeapMarkingState& unified_heap_marking_state_;
};

std::unique_ptr<cppgc::Visitor>
UnifiedHeapConcurrentMarker::CreateConcurrentMarkingVisitor(
    cppgc::internal::ConcurrentMarkingState& marking_state) const {
  return std::make_unique<ConcurrentUnifiedHeapMarkingVisitor>(
      heap(), marking_state, unified_heap_marking_state_);
}

class UnifiedHeapMarker final : public cppgc::internal::MarkerBase {
 public:
  UnifiedHeapMarker(Key, Heap* v8_heap, cppgc::internal::HeapBase& cpp_heap,
                    cppgc::Platform* platform, MarkingConfig config);

  ~UnifiedHeapMarker() final = default;

  void AddObject(void*);

 protected:
  cppgc::Visitor& visitor() final { return marking_visitor_; }
  cppgc::internal::ConservativeTracingVisitor& conservative_visitor() final {
    return conservative_marking_visitor_;
  }
  ::heap::base::StackVisitor& stack_visitor() final {
    return conservative_marking_visitor_;
  }

 private:
  UnifiedHeapMarkingState unified_heap_marking_state_;
  MutatorUnifiedHeapMarkingVisitor marking_visitor_;
  cppgc::internal::ConservativeMarkingVisitor conservative_marking_visitor_;
};

UnifiedHeapMarker::UnifiedHeapMarker(Key key, Heap* v8_heap,
                                     cppgc::internal::HeapBase& heap,
                                     cppgc::Platform* platform,
                                     MarkingConfig config)
    : cppgc::internal::MarkerBase(key, heap, platform, config),
      unified_heap_marking_state_(v8_heap),
      marking_visitor_(heap, mutator_marking_state_,
                       unified_heap_marking_state_),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    marking_visitor_) {
  concurrent_marker_ = std::make_unique<UnifiedHeapConcurrentMarker>(
      heap_, marking_worklists_, schedule_, platform_,
      unified_heap_marking_state_);
}

void UnifiedHeapMarker::AddObject(void* object) {
  mutator_marking_state_.MarkAndPush(
      cppgc::internal::HeapObjectHeader::FromObject(object));
}

}  // namespace

void CppHeap::MetricRecorderAdapter::AddMainThreadEvent(
    const FullCycle& cppgc_event) {
  last_full_gc_event_ = cppgc_event;
  GetIsolate()->heap()->tracer()->NotifyGCCompleted();
}

void CppHeap::MetricRecorderAdapter::AddMainThreadEvent(
    const MainThreadIncrementalMark& cppgc_event) {
  // Incremental marking steps might be nested in V8 marking steps. In such
  // cases, stash the relevant values and delegate to V8 to report them. For
  // non-nested steps, report to the Recorder directly.
  if (cpp_heap_.is_in_v8_marking_step_) {
    last_incremental_mark_event_ = cppgc_event;
    return;
  }
  // This is a standalone incremental marking step.
  const std::shared_ptr<metrics::Recorder>& recorder =
      GetIsolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!recorder->HasEmbedderRecorder()) return;
  incremental_mark_batched_events_.events.emplace_back();
  incremental_mark_batched_events_.events.back().cpp_wall_clock_duration_in_us =
      cppgc_event.duration_us;
  // TODO(chromium:1154636): Populate event.wall_clock_duration_in_us.
  if (incremental_mark_batched_events_.events.size() == kMaxBatchedEvents) {
    recorder->AddMainThreadEvent(std::move(incremental_mark_batched_events_),
                                 GetContextId());
  }
}

void CppHeap::MetricRecorderAdapter::AddMainThreadEvent(
    const MainThreadIncrementalSweep& cppgc_event) {
  // Incremental sweeping steps are never nested inside V8 sweeping steps, so
  // report to the Recorder directly.
  const std::shared_ptr<metrics::Recorder>& recorder =
      GetIsolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!recorder->HasEmbedderRecorder()) return;
  incremental_sweep_batched_events_.events.emplace_back();
  incremental_sweep_batched_events_.events.back()
      .cpp_wall_clock_duration_in_us = cppgc_event.duration_us;
  // TODO(chromium:1154636): Populate event.wall_clock_duration_in_us.
  if (incremental_sweep_batched_events_.events.size() == kMaxBatchedEvents) {
    recorder->AddMainThreadEvent(std::move(incremental_sweep_batched_events_),
                                 GetContextId());
  }
}

void CppHeap::MetricRecorderAdapter::FlushBatchedIncrementalEvents() {
  const std::shared_ptr<metrics::Recorder>& recorder =
      GetIsolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!incremental_mark_batched_events_.events.empty()) {
    recorder->AddMainThreadEvent(std::move(incremental_mark_batched_events_),
                                 GetContextId());
  }
  if (!incremental_sweep_batched_events_.events.empty()) {
    recorder->AddMainThreadEvent(std::move(incremental_sweep_batched_events_),
                                 GetContextId());
  }
}

bool CppHeap::MetricRecorderAdapter::MetricsReportPending() const {
  return last_full_gc_event_.has_value();
}

const base::Optional<cppgc::internal::MetricRecorder::FullCycle>
CppHeap::MetricRecorderAdapter::ExtractLastFullGcEvent() {
  return std::move(last_full_gc_event_);
}

const base::Optional<cppgc::internal::MetricRecorder::MainThreadIncrementalMark>
CppHeap::MetricRecorderAdapter::ExtractLastIncrementalMarkEvent() {
  return std::move(last_incremental_mark_event_);
}

Isolate* CppHeap::MetricRecorderAdapter::GetIsolate() const {
  DCHECK_NOT_NULL(cpp_heap_.isolate());
  return reinterpret_cast<Isolate*>(cpp_heap_.isolate());
}

v8::metrics::Recorder::ContextId CppHeap::MetricRecorderAdapter::GetContextId()
    const {
  DCHECK_NOT_NULL(GetIsolate());
  if (GetIsolate()->context().is_null())
    return v8::metrics::Recorder::ContextId::Empty();
  HandleScope scope(GetIsolate());
  return GetIsolate()->GetOrRegisterRecorderContextId(
      GetIsolate()->native_context());
}

CppHeap::CppHeap(
    v8::Platform* platform,
    const std::vector<std::unique_ptr<cppgc::CustomSpaceBase>>& custom_spaces,
    const v8::WrapperDescriptor& wrapper_descriptor)
    : cppgc::internal::HeapBase(
          std::make_shared<CppgcPlatformAdapter>(platform), custom_spaces,
          cppgc::internal::HeapBase::StackSupport::
              kSupportsConservativeStackScan),
      wrapper_descriptor_(wrapper_descriptor) {
  CHECK_NE(WrapperDescriptor::kUnknownEmbedderId,
           wrapper_descriptor_.embedder_id_for_garbage_collected);
  // Enter no GC scope. `AttachIsolate()` removes this and allows triggering
  // garbage collections.
  no_gc_scope_++;
  stats_collector()->RegisterObserver(this);
}

CppHeap::~CppHeap() {
  if (isolate_) {
    isolate_->heap()->DetachCppHeap();
  }
}

void CppHeap::Terminate() {
  // Must not be attached to a heap when invoking termination GCs.
  CHECK(!isolate_);
  // Gracefully terminate the C++ heap invoking destructors.
  HeapBase::Terminate();
}

void CppHeap::AttachIsolate(Isolate* isolate) {
  CHECK(!in_detached_testing_mode_);
  CHECK_NULL(isolate_);
  isolate_ = isolate;
  static_cast<CppgcPlatformAdapter*>(platform())
      ->SetIsolate(reinterpret_cast<v8::Isolate*>(isolate_));
  if (isolate_->heap_profiler()) {
    isolate_->heap_profiler()->AddBuildEmbedderGraphCallback(
        &CppGraphBuilder::Run, this);
  }
  isolate_->heap()->SetEmbedderHeapTracer(this);
  isolate_->heap()->local_embedder_heap_tracer()->SetWrapperDescriptor(
      wrapper_descriptor_);
  SetMetricRecorder(std::make_unique<MetricRecorderAdapter>(*this));
  SetStackStart(base::Stack::GetStackStart());
  no_gc_scope_--;
}

void CppHeap::DetachIsolate() {
  // TODO(chromium:1056170): Investigate whether this can be enforced with a
  // CHECK across all relevant embedders and setups.
  if (!isolate_) return;

  // Delegate to existing EmbedderHeapTracer API to finish any ongoing garbage
  // collection.
  FinalizeTracing();
  sweeper_.FinishIfRunning();

  if (isolate_->heap_profiler()) {
    isolate_->heap_profiler()->RemoveBuildEmbedderGraphCallback(
        &CppGraphBuilder::Run, this);
  }
  SetMetricRecorder(nullptr);
  isolate_ = nullptr;
  // Any future garbage collections will ignore the V8->C++ references.
  isolate()->SetEmbedderHeapTracer(nullptr);
  // Enter no GC scope.
  no_gc_scope_++;
}

void CppHeap::RegisterV8References(
    const std::vector<std::pair<void*, void*> >& embedder_fields) {
  DCHECK(marker_);
  for (auto& tuple : embedder_fields) {
    // First field points to type.
    // Second field points to object.
    static_cast<UnifiedHeapMarker*>(marker_.get())->AddObject(tuple.second);
  }
  marking_done_ = false;
}

namespace {

bool ShouldReduceMemory(CppHeap::TraceFlags flags) {
  return (flags == CppHeap::TraceFlags::kReduceMemory) ||
         (flags == CppHeap::TraceFlags::kForced);
}

}  // namespace

void CppHeap::TracePrologue(TraceFlags flags) {
  // Finish sweeping in case it is still running.
  sweeper_.FinishIfRunning();

  current_flags_ = flags;
  const UnifiedHeapMarker::MarkingConfig marking_config{
      UnifiedHeapMarker::MarkingConfig::CollectionType::kMajor,
      cppgc::Heap::StackState::kNoHeapPointers,
      ((current_flags_ & TraceFlags::kForced) &&
       !force_incremental_marking_for_testing_)
          ? UnifiedHeapMarker::MarkingConfig::MarkingType::kAtomic
          : UnifiedHeapMarker::MarkingConfig::MarkingType::
                kIncrementalAndConcurrent,
      flags & TraceFlags::kForced
          ? UnifiedHeapMarker::MarkingConfig::IsForcedGC::kForced
          : UnifiedHeapMarker::MarkingConfig::IsForcedGC::kNotForced};
  DCHECK_IMPLIES(!isolate_, (cppgc::Heap::MarkingType::kAtomic ==
                             marking_config.marking_type) ||
                                force_incremental_marking_for_testing_);
  if (ShouldReduceMemory(flags)) {
    // Only enable compaction when in a memory reduction garbage collection as
    // it may significantly increase the final garbage collection pause.
    compactor_.InitializeIfShouldCompact(marking_config.marking_type,
                                         marking_config.stack_state);
  }
  marker_ =
      cppgc::internal::MarkerFactory::CreateAndStartMarking<UnifiedHeapMarker>(
          isolate_ ? isolate_->heap() : nullptr, AsBase(), platform_.get(),
          marking_config);
  marking_done_ = false;
}

bool CppHeap::AdvanceTracing(double deadline_in_ms) {
  is_in_v8_marking_step_ = true;
  cppgc::internal::StatsCollector::EnabledScope stats_scope(
      stats_collector(),
      in_atomic_pause_ ? cppgc::internal::StatsCollector::kAtomicMark
                       : cppgc::internal::StatsCollector::kIncrementalMark);
  const v8::base::TimeDelta deadline =
      in_atomic_pause_ ? v8::base::TimeDelta::Max()
                       : v8::base::TimeDelta::FromMillisecondsD(deadline_in_ms);
  const size_t marked_bytes_limit = in_atomic_pause_ ? SIZE_MAX : 0;
  // TODO(chromium:1056170): Replace when unified heap transitions to
  // bytes-based deadline.
  marking_done_ =
      marker_->AdvanceMarkingWithLimits(deadline, marked_bytes_limit);
  DCHECK_IMPLIES(in_atomic_pause_, marking_done_);
  is_in_v8_marking_step_ = false;
  return marking_done_;
}

bool CppHeap::IsTracingDone() { return marking_done_; }

void CppHeap::EnterFinalPause(EmbedderStackState stack_state) {
  CHECK(!in_disallow_gc_scope());
  in_atomic_pause_ = true;
  if (override_stack_state_) {
    stack_state = *override_stack_state_;
  }
  marker_->EnterAtomicPause(stack_state);
  if (compactor_.CancelIfShouldNotCompact(cppgc::Heap::MarkingType::kAtomic,
                                          stack_state)) {
    marker_->NotifyCompactionCancelled();
  }
}

void CppHeap::TraceEpilogue(TraceSummary* trace_summary) {
  CHECK(in_atomic_pause_);
  CHECK(marking_done_);
  {
    cppgc::subtle::DisallowGarbageCollectionScope disallow_gc_scope(*this);
    marker_->LeaveAtomicPause();
  }
  marker_.reset();
  if (isolate_) {
    auto* tracer = isolate_->heap()->local_embedder_heap_tracer();
    DCHECK_NOT_NULL(tracer);
    tracer->UpdateRemoteStats(
        stats_collector_->marked_bytes(),
        stats_collector_->marking_time().InMillisecondsF());
  }
  ExecutePreFinalizers();
  // TODO(chromium:1056170): replace build flag with dedicated flag.
#if DEBUG
  UnifiedHeapMarkingVerifier verifier(*this);
  verifier.Run(stack_state_of_prev_gc(), stack_end_of_current_gc(),
               stats_collector()->marked_bytes());
#endif

  {
    cppgc::subtle::NoGarbageCollectionScope no_gc(*this);
    cppgc::internal::Sweeper::SweepingConfig::CompactableSpaceHandling
        compactable_space_handling = compactor_.CompactSpacesIfEnabled();
    const cppgc::internal::Sweeper::SweepingConfig sweeping_config{
        // In case the GC was forced, also finalize sweeping right away.
        current_flags_ & TraceFlags::kForced
            ? cppgc::internal::Sweeper::SweepingConfig::SweepingType::kAtomic
            : cppgc::internal::Sweeper::SweepingConfig::SweepingType::
                  kIncrementalAndConcurrent,
        compactable_space_handling,
        ShouldReduceMemory(current_flags_)
            ? cppgc::internal::Sweeper::SweepingConfig::FreeMemoryHandling::
                  kDiscardWherePossible
            : cppgc::internal::Sweeper::SweepingConfig::FreeMemoryHandling::
                  kDoNotDiscard};
    DCHECK_IMPLIES(
        !isolate_,
        cppgc::internal::Sweeper::SweepingConfig::SweepingType::kAtomic ==
            sweeping_config.sweeping_type);
    sweeper().Start(sweeping_config);
  }
  DCHECK_NOT_NULL(trace_summary);
  trace_summary->allocated_size = SIZE_MAX;
  trace_summary->time = 0;
  in_atomic_pause_ = false;
  sweeper().NotifyDoneIfNeeded();
}

void CppHeap::AllocatedObjectSizeIncreased(size_t bytes) {
  buffered_allocated_bytes_ += static_cast<int64_t>(bytes);
  ReportBufferedAllocationSizeIfPossible();
}

void CppHeap::AllocatedObjectSizeDecreased(size_t bytes) {
  buffered_allocated_bytes_ -= static_cast<int64_t>(bytes);
  ReportBufferedAllocationSizeIfPossible();
}

void CppHeap::ReportBufferedAllocationSizeIfPossible() {
  // Avoid reporting to V8 in the following conditions as that may trigger GC
  // finalizations where not allowed.
  // - Recursive sweeping.
  // - GC forbidden scope.
  if (sweeper().IsSweepingOnMutatorThread() || in_no_gc_scope()) {
    return;
  }

  // The calls below may trigger full GCs that are synchronous and also execute
  // epilogue callbacks. Since such callbacks may allocate, the counter must
  // already be zeroed by that time.
  const int64_t bytes_to_report = buffered_allocated_bytes_;
  buffered_allocated_bytes_ = 0;

  if (bytes_to_report < 0) {
    DecreaseAllocatedSize(static_cast<size_t>(-bytes_to_report));
  } else {
    IncreaseAllocatedSize(static_cast<size_t>(bytes_to_report));
  }
}

void CppHeap::CollectGarbageForTesting(
    cppgc::internal::GarbageCollector::Config::StackState stack_state) {
  if (in_no_gc_scope()) return;

  // Finish sweeping in case it is still running.
  sweeper().FinishIfRunning();

  SetStackEndOfCurrentGC(v8::base::Stack::GetCurrentStackPosition());

  if (isolate_) {
    // Go through EmbedderHeapTracer API and perform a unified heap collection.
    GarbageCollectionForTesting(stack_state);
  } else {
    // Perform an atomic GC, with starting incremental/concurrent marking and
    // immediately finalizing the garbage collection.
    if (!IsMarking()) TracePrologue(TraceFlags::kForced);
    EnterFinalPause(stack_state);
    AdvanceTracing(std::numeric_limits<double>::infinity());
    TraceSummary trace_summary;
    TraceEpilogue(&trace_summary);
    DCHECK_EQ(SIZE_MAX, trace_summary.allocated_size);
  }
}

void CppHeap::EnableDetachedGarbageCollectionsForTesting() {
  CHECK(!in_detached_testing_mode_);
  CHECK_NULL(isolate_);
  no_gc_scope_--;
  in_detached_testing_mode_ = true;
  static_cast<CppgcPlatformAdapter*>(platform())
      ->EnableDetachedModeForTesting();
}

void CppHeap::StartIncrementalGarbageCollectionForTesting() {
  DCHECK(!in_no_gc_scope());
  DCHECK_NULL(isolate_);
  if (IsMarking()) return;
  force_incremental_marking_for_testing_ = true;
  TracePrologue(TraceFlags::kForced);
  force_incremental_marking_for_testing_ = false;
}

void CppHeap::FinalizeIncrementalGarbageCollectionForTesting(
    EmbedderStackState stack_state) {
  DCHECK(!in_no_gc_scope());
  DCHECK_NULL(isolate_);
  DCHECK(IsMarking());
  if (IsMarking()) {
    CollectGarbageForTesting(stack_state);
  }
  sweeper_.FinishIfRunning();
}

namespace {

void ReportCustomSpaceStatistics(
    cppgc::internal::RawHeap& raw_heap,
    std::vector<cppgc::CustomSpaceIndex> custom_spaces,
    std::unique_ptr<CustomSpaceStatisticsReceiver> receiver) {
  for (auto custom_space_index : custom_spaces) {
    const cppgc::internal::BaseSpace* space =
        raw_heap.CustomSpace(custom_space_index);
    size_t allocated_bytes = std::accumulate(
        space->begin(), space->end(), 0, [](size_t sum, auto* page) {
          return sum + page->AllocatedBytesAtLastGC();
        });
    receiver->AllocatedBytes(custom_space_index, allocated_bytes);
  }
}

class CollectCustomSpaceStatisticsAtLastGCTask final : public v8::Task {
 public:
  static constexpr v8::base::TimeDelta kTaskDelayMs =
      v8::base::TimeDelta::FromMilliseconds(10);

  CollectCustomSpaceStatisticsAtLastGCTask(
      cppgc::internal::HeapBase& heap,
      std::vector<cppgc::CustomSpaceIndex> custom_spaces,
      std::unique_ptr<CustomSpaceStatisticsReceiver> receiver)
      : heap_(heap),
        custom_spaces_(std::move(custom_spaces)),
        receiver_(std::move(receiver)) {}

  void Run() final {
    cppgc::internal::Sweeper& sweeper = heap_.sweeper();
    if (sweeper.PerformSweepOnMutatorThread(
            heap_.platform()->MonotonicallyIncreasingTime() +
            kStepSizeMs.InSecondsF())) {
      // Sweeping is done.
      DCHECK(!sweeper.IsSweepingInProgress());
      ReportCustomSpaceStatistics(heap_.raw_heap(), std::move(custom_spaces_),
                                  std::move(receiver_));
    } else {
      heap_.platform()->GetForegroundTaskRunner()->PostDelayedTask(
          std::make_unique<CollectCustomSpaceStatisticsAtLastGCTask>(
              heap_, std::move(custom_spaces_), std::move(receiver_)),
          kTaskDelayMs.InSecondsF());
    }
  }

 private:
  static constexpr v8::base::TimeDelta kStepSizeMs =
      v8::base::TimeDelta::FromMilliseconds(5);

  cppgc::internal::HeapBase& heap_;
  std::vector<cppgc::CustomSpaceIndex> custom_spaces_;
  std::unique_ptr<CustomSpaceStatisticsReceiver> receiver_;
};

constexpr v8::base::TimeDelta
    CollectCustomSpaceStatisticsAtLastGCTask::kTaskDelayMs;
constexpr v8::base::TimeDelta
    CollectCustomSpaceStatisticsAtLastGCTask::kStepSizeMs;

}  // namespace

void CppHeap::CollectCustomSpaceStatisticsAtLastGC(
    std::vector<cppgc::CustomSpaceIndex> custom_spaces,
    std::unique_ptr<CustomSpaceStatisticsReceiver> receiver) {
  if (sweeper().IsSweepingInProgress()) {
    platform()->GetForegroundTaskRunner()->PostDelayedTask(
        std::make_unique<CollectCustomSpaceStatisticsAtLastGCTask>(
            AsBase(), std::move(custom_spaces), std::move(receiver)),
        CollectCustomSpaceStatisticsAtLastGCTask::kTaskDelayMs.InSecondsF());
    return;
  }
  ReportCustomSpaceStatistics(raw_heap(), std::move(custom_spaces),
                              std::move(receiver));
}

CppHeap::MetricRecorderAdapter* CppHeap::GetMetricRecorder() const {
  return static_cast<MetricRecorderAdapter*>(
      stats_collector_->GetMetricRecorder());
}

}  // namespace internal
}  // namespace v8
