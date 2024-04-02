// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/cpp-heap.h"

#include <cstdint>
#include <memory>
#include <numeric>

#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/platform.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-platform.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate-inl.h"
#include "src/flags/flags.h"
#include "src/handles/global-handles.h"
#include "src/handles/handles.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc-js/cpp-marking-state.h"
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
#include "src/heap/cppgc/unmarker.h"
#include "src/heap/embedder-tracing-inl.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/sweeper.h"
#include "src/init/v8.h"
#include "src/profiler/heap-profiler.h"

namespace v8 {

namespace {

class V8ToCppGCReferencesVisitor final
    : public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  V8ToCppGCReferencesVisitor(
      cppgc::internal::MutatorMarkingState& marking_state,
      v8::internal::Isolate* isolate,
      const v8::WrapperDescriptor& wrapper_descriptor)
      : marking_state_(marking_state),
        isolate_(isolate),
        wrapper_descriptor_(wrapper_descriptor) {}

  void VisitTracedReference(const v8::TracedReference<v8::Value>& value) final {
    VisitHandle(value, value.WrapperClassId());
  }

 private:
  void VisitHandle(const v8::TracedReference<v8::Value>& value,
                   uint16_t class_id) {
    DCHECK(!value.IsEmpty());

    const internal::JSObject js_object =
        *reinterpret_cast<const internal::JSObject* const&>(value);
    if (!js_object.ptr() || js_object.IsSmi() ||
        !js_object.MayHaveEmbedderFields())
      return;

    internal::LocalEmbedderHeapTracer::WrapperInfo info;
    if (!internal::LocalEmbedderHeapTracer::ExtractWrappableInfo(
            isolate_, js_object, wrapper_descriptor_, &info))
      return;

    marking_state_.MarkAndPush(
        cppgc::internal::HeapObjectHeader::FromObject(info.second));
  }

  cppgc::internal::MutatorMarkingState& marking_state_;
  v8::internal::Isolate* isolate_;
  const v8::WrapperDescriptor& wrapper_descriptor_;
};

void TraceV8ToCppGCReferences(
    v8::internal::Isolate* isolate,
    cppgc::internal::MutatorMarkingState& marking_state,
    const v8::WrapperDescriptor& wrapper_descriptor) {
  DCHECK(isolate);
  V8ToCppGCReferencesVisitor forwarding_visitor(marking_state, isolate,
                                                wrapper_descriptor);
  isolate->global_handles()->IterateTracedNodes(&forwarding_visitor);
}

}  // namespace

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
  return internal::CppHeap::From(this)->CollectGarbageForTesting(
      internal::CppHeap::CollectionType::kMajor, stack_state);
}

void CppHeap::CollectGarbageInYoungGenerationForTesting(
    cppgc::EmbedderStackState stack_state) {
  return internal::CppHeap::From(this)->CollectGarbageForTesting(
      internal::CppHeap::CollectionType::kMinor, stack_state);
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
      cppgc::internal::HeapBase& heap, Heap* v8_heap,
      cppgc::internal::MarkingWorklists& marking_worklists,
      cppgc::internal::IncrementalMarkingSchedule& incremental_marking_schedule,
      cppgc::Platform* platform,
      UnifiedHeapMarkingState& unified_heap_marking_state)
      : cppgc::internal::ConcurrentMarkerBase(
            heap, marking_worklists, incremental_marking_schedule, platform),
        v8_heap_(v8_heap) {}

  std::unique_ptr<cppgc::Visitor> CreateConcurrentMarkingVisitor(
      cppgc::internal::ConcurrentMarkingState&) const final;

 private:
  Heap* const v8_heap_;
};

std::unique_ptr<cppgc::Visitor>
UnifiedHeapConcurrentMarker::CreateConcurrentMarkingVisitor(
    cppgc::internal::ConcurrentMarkingState& marking_state) const {
  return std::make_unique<ConcurrentUnifiedHeapMarkingVisitor>(heap(), v8_heap_,
                                                               marking_state);
}

void FatalOutOfMemoryHandlerImpl(const std::string& reason,
                                 const SourceLocation&, HeapBase* heap) {
  FatalProcessOutOfMemory(static_cast<v8::internal::CppHeap*>(heap)->isolate(),
                          reason.c_str());
}

}  // namespace

class UnifiedHeapMarker final : public cppgc::internal::MarkerBase {
 public:
  UnifiedHeapMarker(Heap* v8_heap, cppgc::internal::HeapBase& cpp_heap,
                    cppgc::Platform* platform, MarkingConfig config);

  ~UnifiedHeapMarker() final = default;

  void AddObject(void*);

  cppgc::internal::MarkingWorklists& GetMarkingWorklists() {
    return marking_worklists_;
  }

  cppgc::internal::MutatorMarkingState& GetMutatorMarkingState() {
    return static_cast<cppgc::internal::MutatorMarkingState&>(
        marking_visitor_->marking_state_);
  }

  UnifiedHeapMarkingState& GetMutatorUnifiedHeapMarkingState() {
    return mutator_unified_heap_marking_state_;
  }

 protected:
  cppgc::Visitor& visitor() final { return *marking_visitor_; }
  cppgc::internal::ConservativeTracingVisitor& conservative_visitor() final {
    return conservative_marking_visitor_;
  }
  ::heap::base::StackVisitor& stack_visitor() final {
    return conservative_marking_visitor_;
  }

 private:
  UnifiedHeapMarkingState mutator_unified_heap_marking_state_;
  std::unique_ptr<MutatorUnifiedHeapMarkingVisitor> marking_visitor_;
  cppgc::internal::ConservativeMarkingVisitor conservative_marking_visitor_;
};

UnifiedHeapMarker::UnifiedHeapMarker(Heap* v8_heap,
                                     cppgc::internal::HeapBase& heap,
                                     cppgc::Platform* platform,
                                     MarkingConfig config)
    : cppgc::internal::MarkerBase(heap, platform, config),
      mutator_unified_heap_marking_state_(v8_heap, nullptr),
      marking_visitor_(config.collection_type == CppHeap::CollectionType::kMajor
                           ? std::make_unique<MutatorUnifiedHeapMarkingVisitor>(
                                 heap, mutator_marking_state_,
                                 mutator_unified_heap_marking_state_)
                           : std::make_unique<MutatorMinorGCMarkingVisitor>(
                                 heap, mutator_marking_state_,
                                 mutator_unified_heap_marking_state_)),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    *marking_visitor_) {
  concurrent_marker_ = std::make_unique<UnifiedHeapConcurrentMarker>(
      heap_, v8_heap, marking_worklists_, schedule_, platform_,
      mutator_unified_heap_marking_state_);
}

void UnifiedHeapMarker::AddObject(void* object) {
  mutator_marking_state_.MarkAndPush(
      cppgc::internal::HeapObjectHeader::FromObject(object));
}

void CppHeap::MetricRecorderAdapter::AddMainThreadEvent(
    const GCCycle& cppgc_event) {
  auto* tracer = GetIsolate()->heap()->tracer();
  if (cppgc_event.type == MetricRecorder::GCCycle::Type::kMinor) {
    DCHECK(!last_young_gc_event_);
    last_young_gc_event_ = cppgc_event;
    tracer->NotifyYoungCppGCCompleted();
  } else {
    DCHECK(!last_full_gc_event_);
    last_full_gc_event_ = cppgc_event;
    tracer->NotifyFullCppGCCompleted();
  }
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
  if (incremental_mark_batched_events_.events.size() == kMaxBatchedEvents) {
    recorder->AddMainThreadEvent(std::move(incremental_mark_batched_events_),
                                 GetContextId());
    incremental_mark_batched_events_ = {};
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
  if (incremental_sweep_batched_events_.events.size() == kMaxBatchedEvents) {
    recorder->AddMainThreadEvent(std::move(incremental_sweep_batched_events_),
                                 GetContextId());
    incremental_sweep_batched_events_ = {};
  }
}

void CppHeap::MetricRecorderAdapter::FlushBatchedIncrementalEvents() {
  const std::shared_ptr<metrics::Recorder>& recorder =
      GetIsolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!incremental_mark_batched_events_.events.empty()) {
    recorder->AddMainThreadEvent(std::move(incremental_mark_batched_events_),
                                 GetContextId());
    incremental_mark_batched_events_ = {};
  }
  if (!incremental_sweep_batched_events_.events.empty()) {
    recorder->AddMainThreadEvent(std::move(incremental_sweep_batched_events_),
                                 GetContextId());
    incremental_sweep_batched_events_ = {};
  }
}

bool CppHeap::MetricRecorderAdapter::FullGCMetricsReportPending() const {
  return last_full_gc_event_.has_value();
}

bool CppHeap::MetricRecorderAdapter::YoungGCMetricsReportPending() const {
  return last_young_gc_event_.has_value();
}

const base::Optional<cppgc::internal::MetricRecorder::GCCycle>
CppHeap::MetricRecorderAdapter::ExtractLastFullGcEvent() {
  auto res = std::move(last_full_gc_event_);
  last_full_gc_event_.reset();
  return res;
}

const base::Optional<cppgc::internal::MetricRecorder::GCCycle>
CppHeap::MetricRecorderAdapter::ExtractLastYoungGcEvent() {
  auto res = std::move(last_young_gc_event_);
  last_young_gc_event_.reset();
  return res;
}

const base::Optional<cppgc::internal::MetricRecorder::MainThreadIncrementalMark>
CppHeap::MetricRecorderAdapter::ExtractLastIncrementalMarkEvent() {
  auto res = std::move(last_incremental_mark_event_);
  last_incremental_mark_event_.reset();
  return res;
}

void CppHeap::MetricRecorderAdapter::ClearCachedEvents() {
  incremental_mark_batched_events_.events.clear();
  incremental_sweep_batched_events_.events.clear();
  last_incremental_mark_event_.reset();
  last_full_gc_event_.reset();
  last_young_gc_event_.reset();
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
              kSupportsConservativeStackScan,
          FLAG_single_threaded_gc ? MarkingType::kIncremental
                                  : MarkingType::kIncrementalAndConcurrent,
          FLAG_single_threaded_gc ? SweepingType::kIncremental
                                  : SweepingType::kIncrementalAndConcurrent),
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
  SetMetricRecorder(std::make_unique<MetricRecorderAdapter>(*this));
  isolate_->global_handles()->SetStackStart(base::Stack::GetStackStart());
  oom_handler().SetCustomHandler(&FatalOutOfMemoryHandlerImpl);
  no_gc_scope_--;
}

void CppHeap::DetachIsolate() {
  // TODO(chromium:1056170): Investigate whether this can be enforced with a
  // CHECK across all relevant embedders and setups.
  if (!isolate_) return;

  // Delegate to existing EmbedderHeapTracer API to finish any ongoing garbage
  // collection.
  if (isolate_->heap()->incremental_marking()->IsMarking()) {
    isolate_->heap()->FinalizeIncrementalMarkingAtomically(
        i::GarbageCollectionReason::kExternalFinalize);
  }
  sweeper_.FinishIfRunning();

  auto* heap_profiler = isolate_->heap_profiler();
  if (heap_profiler) {
    heap_profiler->RemoveBuildEmbedderGraphCallback(&CppGraphBuilder::Run,
                                                    this);
  }
  SetMetricRecorder(nullptr);
  isolate_ = nullptr;
  // Any future garbage collections will ignore the V8->C++ references.
  oom_handler().SetCustomHandler(nullptr);
  // Enter no GC scope.
  no_gc_scope_++;
}

namespace {

bool IsMemoryReducingGC(CppHeap::GarbageCollectionFlags flags) {
  return flags & CppHeap::GarbageCollectionFlagValues::kReduceMemory;
}

bool IsForceGC(CppHeap::GarbageCollectionFlags flags) {
  return flags & CppHeap::GarbageCollectionFlagValues::kForced;
}

bool ShouldReduceMemory(CppHeap::GarbageCollectionFlags flags) {
  return IsMemoryReducingGC(flags) || IsForceGC(flags);
}

}  // namespace

CppHeap::MarkingType CppHeap::SelectMarkingType() const {
  // For now, force atomic marking for minor collections.
  if (*collection_type_ == CollectionType::kMinor) return MarkingType::kAtomic;

  if (IsForceGC(current_gc_flags_) && !force_incremental_marking_for_testing_)
    return MarkingType::kAtomic;

  return marking_support();
}

CppHeap::SweepingType CppHeap::SelectSweepingType() const {
  if (IsForceGC(current_gc_flags_)) return SweepingType::kAtomic;

  return sweeping_support();
}

void CppHeap::InitializeTracing(CollectionType collection_type,
                                GarbageCollectionFlags gc_flags) {
  CHECK(!sweeper_.IsSweepingInProgress());

  // Check that previous cycle metrics for the same collection type have been
  // reported.
  if (GetMetricRecorder()) {
    if (collection_type == CollectionType::kMajor)
      DCHECK(!GetMetricRecorder()->FullGCMetricsReportPending());
    else
      DCHECK(!GetMetricRecorder()->YoungGCMetricsReportPending());
  }

  DCHECK(!collection_type_);
  collection_type_ = collection_type;

#if defined(CPPGC_YOUNG_GENERATION)
  if (*collection_type_ == CollectionType::kMajor)
    cppgc::internal::SequentialUnmarker unmarker(raw_heap());
#endif  // defined(CPPGC_YOUNG_GENERATION)

  current_gc_flags_ = gc_flags;

  const UnifiedHeapMarker::MarkingConfig marking_config{
      *collection_type_, StackState::kNoHeapPointers, SelectMarkingType(),
      IsForceGC(current_gc_flags_)
          ? UnifiedHeapMarker::MarkingConfig::IsForcedGC::kForced
          : UnifiedHeapMarker::MarkingConfig::IsForcedGC::kNotForced};
  DCHECK_IMPLIES(!isolate_,
                 (MarkingType::kAtomic == marking_config.marking_type) ||
                     force_incremental_marking_for_testing_);
  if (ShouldReduceMemory(current_gc_flags_)) {
    // Only enable compaction when in a memory reduction garbage collection as
    // it may significantly increase the final garbage collection pause.
    compactor_.InitializeIfShouldCompact(marking_config.marking_type,
                                         marking_config.stack_state);
  }
  marker_ = std::make_unique<UnifiedHeapMarker>(
      isolate_ ? isolate()->heap() : nullptr, AsBase(), platform_.get(),
      marking_config);
}

void CppHeap::StartTracing() {
  if (isolate_) {
    // Reuse the same local worklist for the mutator marking state which results
    // in directly processing the objects by the JS logic. Also avoids
    // publishing local objects.
    static_cast<UnifiedHeapMarker*>(marker_.get())
        ->GetMutatorUnifiedHeapMarkingState()
        .Update(isolate_->heap()
                    ->mark_compact_collector()
                    ->local_marking_worklists());
  }
  marker_->StartMarking();
  marking_done_ = false;
}

bool CppHeap::AdvanceTracing(double max_duration) {
  is_in_v8_marking_step_ = true;
  cppgc::internal::StatsCollector::EnabledScope stats_scope(
      stats_collector(),
      in_atomic_pause_ ? cppgc::internal::StatsCollector::kAtomicMark
                       : cppgc::internal::StatsCollector::kIncrementalMark);
  const v8::base::TimeDelta deadline =
      in_atomic_pause_ ? v8::base::TimeDelta::Max()
                       : v8::base::TimeDelta::FromMillisecondsD(max_duration);
  const size_t marked_bytes_limit = in_atomic_pause_ ? SIZE_MAX : 0;
  DCHECK_NOT_NULL(marker_);
  // TODO(chromium:1056170): Replace when unified heap transitions to
  // bytes-based deadline.
  marking_done_ =
      marker_->AdvanceMarkingWithLimits(deadline, marked_bytes_limit);
  DCHECK_IMPLIES(in_atomic_pause_, marking_done_);
  is_in_v8_marking_step_ = false;
  return marking_done_;
}

bool CppHeap::IsTracingDone() { return marking_done_; }

void CppHeap::EnterFinalPause(cppgc::EmbedderStackState stack_state) {
  CHECK(!in_disallow_gc_scope());
  in_atomic_pause_ = true;
  marker_->EnterAtomicPause(stack_state);
  if (isolate_ && *collection_type_ == CollectionType::kMinor) {
    // Visit V8 -> cppgc references.
    TraceV8ToCppGCReferences(isolate_,
                             static_cast<UnifiedHeapMarker*>(marker_.get())
                                 ->GetMutatorMarkingState(),
                             wrapper_descriptor_);
  }
  compactor_.CancelIfShouldNotCompact(MarkingType::kAtomic, stack_state);
}

bool CppHeap::FinishConcurrentMarkingIfNeeded() {
  return marker_->JoinConcurrentMarkingIfNeeded();
}

void CppHeap::TraceEpilogue() {
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
  // The allocated bytes counter in v8 was reset to the current marked bytes, so
  // any pending allocated bytes updates should be discarded.
  buffered_allocated_bytes_ = 0;
  const size_t bytes_allocated_in_prefinalizers = ExecutePreFinalizers();
#if CPPGC_VERIFY_HEAP
  UnifiedHeapMarkingVerifier verifier(*this, *collection_type_);
  verifier.Run(stack_state_of_prev_gc(), stack_end_of_current_gc(),
               stats_collector()->marked_bytes_on_current_cycle() +
                   bytes_allocated_in_prefinalizers);
#endif  // CPPGC_VERIFY_HEAP
  USE(bytes_allocated_in_prefinalizers);

#if defined(CPPGC_YOUNG_GENERATION)
  ResetRememberedSet();
#endif  // defined(CPPGC_YOUNG_GENERATION)

  {
    cppgc::subtle::NoGarbageCollectionScope no_gc(*this);
    cppgc::internal::Sweeper::SweepingConfig::CompactableSpaceHandling
        compactable_space_handling = compactor_.CompactSpacesIfEnabled();
    const cppgc::internal::Sweeper::SweepingConfig sweeping_config{
        SelectSweepingType(), compactable_space_handling,
        ShouldReduceMemory(current_gc_flags_)
            ? cppgc::internal::Sweeper::SweepingConfig::FreeMemoryHandling::
                  kDiscardWherePossible
            : cppgc::internal::Sweeper::SweepingConfig::FreeMemoryHandling::
                  kDoNotDiscard};
    DCHECK_IMPLIES(!isolate_,
                   SweepingType::kAtomic == sweeping_config.sweeping_type);
    sweeper().Start(sweeping_config);
  }
  in_atomic_pause_ = false;
  collection_type_.reset();
  sweeper().NotifyDoneIfNeeded();
}

void CppHeap::RunMinorGC(StackState stack_state) {
  DCHECK(!sweeper_.IsSweepingInProgress());

  if (in_no_gc_scope()) return;
  // Minor GC does not support nesting in full GCs.
  if (IsMarking()) return;
  // Minor GCs with the stack are currently not supported.
  if (stack_state == StackState::kMayContainHeapPointers) return;

  // Notify GC tracer that CppGC started young GC cycle.
  isolate_->heap()->tracer()->NotifyYoungCppGCRunning();

  SetStackEndOfCurrentGC(v8::base::Stack::GetCurrentStackPosition());

  // Perform an atomic GC, with starting incremental/concurrent marking and
  // immediately finalizing the garbage collection.
  InitializeTracing(CollectionType::kMinor,
                    GarbageCollectionFlagValues::kNoFlags);
  StartTracing();
  // TODO(chromium:1029379): Should be safe to run without stack.
  EnterFinalPause(cppgc::EmbedderStackState::kMayContainHeapPointers);
  CHECK(AdvanceTracing(std::numeric_limits<double>::infinity()));
  if (FinishConcurrentMarkingIfNeeded()) {
    CHECK(AdvanceTracing(std::numeric_limits<double>::infinity()));
  }
  TraceEpilogue();
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
  if (sweeper().IsSweepingOnMutatorThread() || in_no_gc_scope() || !isolate_) {
    return;
  }

  // The calls below may trigger full GCs that are synchronous and also execute
  // epilogue callbacks. Since such callbacks may allocate, the counter must
  // already be zeroed by that time.
  const int64_t bytes_to_report = buffered_allocated_bytes_;
  buffered_allocated_bytes_ = 0;

  auto* const tracer = isolate_->heap()->local_embedder_heap_tracer();
  DCHECK_NOT_NULL(tracer);
  if (bytes_to_report < 0) {
    tracer->DecreaseAllocatedSize(static_cast<size_t>(-bytes_to_report));
  } else {
    tracer->IncreaseAllocatedSize(static_cast<size_t>(bytes_to_report));
  }
}

void CppHeap::CollectGarbageForTesting(CollectionType collection_type,
                                       StackState stack_state) {
  if (in_no_gc_scope()) return;

  // Finish sweeping in case it is still running.
  sweeper().FinishIfRunning();

  SetStackEndOfCurrentGC(v8::base::Stack::GetCurrentStackPosition());

  if (isolate_) {
    reinterpret_cast<v8::Isolate*>(isolate_)
        ->RequestGarbageCollectionForTesting(
            v8::Isolate::kFullGarbageCollection, stack_state);
  } else {
    // Perform an atomic GC, with starting incremental/concurrent marking and
    // immediately finalizing the garbage collection.
    if (!IsMarking()) {
      InitializeTracing(collection_type, GarbageCollectionFlagValues::kForced);
      StartTracing();
    }
    EnterFinalPause(stack_state);
    CHECK(AdvanceTracing(std::numeric_limits<double>::infinity()));
    if (FinishConcurrentMarkingIfNeeded()) {
      CHECK(AdvanceTracing(std::numeric_limits<double>::infinity()));
    }
    TraceEpilogue();
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
  InitializeTracing(CollectionType::kMajor,
                    GarbageCollectionFlagValues::kForced);
  StartTracing();
  force_incremental_marking_for_testing_ = false;
}

void CppHeap::FinalizeIncrementalGarbageCollectionForTesting(
    cppgc::EmbedderStackState stack_state) {
  DCHECK(!in_no_gc_scope());
  DCHECK_NULL(isolate_);
  DCHECK(IsMarking());
  if (IsMarking()) {
    CollectGarbageForTesting(CollectionType::kMajor, stack_state);
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

void CppHeap::FinishSweepingIfRunning() { sweeper_.FinishIfRunning(); }

void CppHeap::FinishSweepingIfOutOfWork() { sweeper_.FinishIfOutOfWork(); }

std::unique_ptr<CppMarkingState> CppHeap::CreateCppMarkingState() {
  DCHECK(IsMarking());
  return std::make_unique<CppMarkingState>(
      isolate(), wrapper_descriptor_,
      std::make_unique<cppgc::internal::MarkingStateBase>(
          AsBase(),
          static_cast<UnifiedHeapMarker*>(marker())->GetMarkingWorklists()));
}

std::unique_ptr<CppMarkingState>
CppHeap::CreateCppMarkingStateForMutatorThread() {
  DCHECK(IsMarking());
  return std::make_unique<CppMarkingState>(
      isolate(), wrapper_descriptor_,
      static_cast<UnifiedHeapMarker*>(marker())->GetMutatorMarkingState());
}

CppHeap::PauseConcurrentMarkingScope::PauseConcurrentMarkingScope(
    CppHeap* cpp_heap) {
  if (cpp_heap && cpp_heap->marker()) {
    pause_scope_.emplace(*cpp_heap->marker());
  }
}

}  // namespace internal
}  // namespace v8
