// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-tracer.h"

#include <cstdarg>

#include "include/v8-metrics.h"
#include "src/base/atomic-utils.h"
#include "src/base/strings.h"
#include "src/common/globals.h"
#include "src/execution/thread-id.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/metric-recorder.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/logging/counters.h"
#include "src/logging/metrics.h"
#include "src/logging/tracing-flags.h"
#include "src/tracing/tracing-category-observer.h"

namespace v8 {
namespace internal {

static size_t CountTotalHolesSize(Heap* heap) {
  size_t holes_size = 0;
  PagedSpaceIterator spaces(heap);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    DCHECK_GE(holes_size + space->Waste() + space->Available(), holes_size);
    holes_size += space->Waste() + space->Available();
  }
  return holes_size;
}

namespace {
std::atomic<CollectionEpoch> global_epoch{0};

CollectionEpoch next_epoch() {
  return global_epoch.fetch_add(1, std::memory_order_relaxed) + 1;
}
}  // namespace

#if DEBUG
void GCTracer::Scope::AssertMainThread() {
  Isolate* isolate = tracer_->heap_->isolate();
  Isolate* shared_isolate = isolate->shared_isolate();
  ThreadId thread_id = ThreadId::Current();

  // Either run on isolate's main thread or on the current main thread of the
  // shared isolate during shared GCs.
  DCHECK(isolate->thread_id() == thread_id ||
         (shared_isolate && shared_isolate->thread_id() == thread_id));
}
#endif  // DEBUG

const char* GCTracer::Scope::Name(ScopeId id) {
#define CASE(scope)  \
  case Scope::scope: \
    return "V8.GC_" #scope;
  switch (id) {
    TRACER_SCOPES(CASE)
    TRACER_BACKGROUND_SCOPES(CASE)
    case Scope::NUMBER_OF_SCOPES:
      break;
  }
#undef CASE
  UNREACHABLE();
}

bool GCTracer::Scope::NeedsYoungEpoch(ScopeId id) {
#define CASE(scope)  \
  case Scope::scope: \
    return true;
  switch (id) {
    TRACER_YOUNG_EPOCH_SCOPES(CASE)
    default:
      return false;
  }
#undef CASE
  UNREACHABLE();
}

GCTracer::Event::Event(Type type, State state,
                       GarbageCollectionReason gc_reason,
                       const char* collector_reason)
    : type(type),
      state(state),
      gc_reason(gc_reason),
      collector_reason(collector_reason),
      start_time(0.0),
      end_time(0.0),
      reduce_memory(false),
      start_object_size(0),
      end_object_size(0),
      start_memory_size(0),
      end_memory_size(0),
      start_holes_size(0),
      end_holes_size(0),
      young_object_size(0),
      survived_young_object_size(0),
      incremental_marking_bytes(0),
      incremental_marking_duration(0.0) {
  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    scopes[i] = 0;
  }
}

const char* GCTracer::Event::TypeName(bool short_name) const {
  switch (type) {
    case SCAVENGER:
      return (short_name) ? "s" : "Scavenge";
    case MARK_COMPACTOR:
    case INCREMENTAL_MARK_COMPACTOR:
      return (short_name) ? "ms" : "Mark-sweep";
    case MINOR_MARK_COMPACTOR:
      return (short_name) ? "mmc" : "Minor Mark-Compact";
    case START:
      return (short_name) ? "st" : "Start";
  }
  return "Unknown Event Type";
}

GCTracer::RecordGCPhasesInfo::RecordGCPhasesInfo(Heap* heap,
                                                 GarbageCollector collector) {
  Counters* counters = heap->isolate()->counters();
  const bool in_background = heap->isolate()->IsIsolateInBackground();
  if (Heap::IsYoungGenerationCollector(collector)) {
    mode = Mode::Scavenger;
    type_timer = type_priority_timer = nullptr;
  } else {
    DCHECK_EQ(GarbageCollector::MARK_COMPACTOR, collector);
    if (heap->incremental_marking()->IsStopped()) {
      mode = Mode::None;
      type_timer = counters->gc_compactor();
      type_priority_timer = in_background ? counters->gc_compactor_background()
                                          : counters->gc_compactor_foreground();
    } else if (heap->ShouldReduceMemory()) {
      mode = Mode::None;
      type_timer = counters->gc_finalize_reduce_memory();
      type_priority_timer =
          in_background ? counters->gc_finalize_reduce_memory_background()
                        : counters->gc_finalize_reduce_memory_foreground();
    } else {
      if (heap->incremental_marking()->IsMarking() &&
          heap->incremental_marking()
              ->local_marking_worklists()
              ->IsPerContextMode()) {
        mode = Mode::None;
        type_timer = counters->gc_finalize_measure_memory();
      } else {
        mode = Mode::Finalize;
        type_timer = counters->gc_finalize();
      }
      type_priority_timer = in_background ? counters->gc_finalize_background()
                                          : counters->gc_finalize_foreground();
    }
  }
}

GCTracer::GCTracer(Heap* heap)
    : heap_(heap),
      current_(Event::START, Event::State::NOT_RUNNING,
               GarbageCollectionReason::kUnknown, nullptr),
      previous_(current_),
      incremental_marking_bytes_(0),
      incremental_marking_duration_(0.0),
      incremental_marking_start_time_(0.0),
      recorded_incremental_marking_speed_(0.0),
      allocation_time_ms_(0.0),
      new_space_allocation_counter_bytes_(0),
      old_generation_allocation_counter_bytes_(0),
      embedder_allocation_counter_bytes_(0),
      allocation_duration_since_gc_(0.0),
      new_space_allocation_in_bytes_since_gc_(0),
      old_generation_allocation_in_bytes_since_gc_(0),
      embedder_allocation_in_bytes_since_gc_(0),
      combined_mark_compact_speed_cache_(0.0),
      start_counter_(0),
      average_mutator_duration_(0),
      average_mark_compact_duration_(0),
      current_mark_compact_mutator_utilization_(1.0),
      previous_mark_compact_end_time_(0) {
  // All accesses to incremental_marking_scope assume that incremental marking
  // scopes come first.
  STATIC_ASSERT(0 == Scope::FIRST_INCREMENTAL_SCOPE);
  // We assume that MC_INCREMENTAL is the first scope so that we can properly
  // map it to RuntimeCallStats.
  STATIC_ASSERT(0 == Scope::MC_INCREMENTAL);
  current_.end_time = MonotonicallyIncreasingTimeInMs();
  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    background_counter_[i].total_duration_ms = 0;
  }
}

void GCTracer::ResetForTesting() {
  current_ = Event(Event::START, Event::State::NOT_RUNNING,
                   GarbageCollectionReason::kTesting, nullptr);
  current_.end_time = MonotonicallyIncreasingTimeInMs();
  previous_ = current_;
  start_of_observable_pause_ = 0.0;
  notified_sweeping_completed_ = false;
  notified_full_cppgc_completed_ = false;
  notified_young_cppgc_completed_ = false;
  notified_young_cppgc_running_ = false;
  young_gc_while_full_gc_ = false;
  ResetIncrementalMarkingCounters();
  allocation_time_ms_ = 0.0;
  new_space_allocation_counter_bytes_ = 0.0;
  old_generation_allocation_counter_bytes_ = 0.0;
  allocation_duration_since_gc_ = 0.0;
  new_space_allocation_in_bytes_since_gc_ = 0.0;
  old_generation_allocation_in_bytes_since_gc_ = 0.0;
  combined_mark_compact_speed_cache_ = 0.0;
  recorded_minor_gcs_total_.Reset();
  recorded_minor_gcs_survived_.Reset();
  recorded_compactions_.Reset();
  recorded_mark_compacts_.Reset();
  recorded_incremental_mark_compacts_.Reset();
  recorded_new_generation_allocations_.Reset();
  recorded_old_generation_allocations_.Reset();
  recorded_embedder_generation_allocations_.Reset();
  recorded_survival_ratios_.Reset();
  start_counter_ = 0;
  average_mutator_duration_ = 0;
  average_mark_compact_duration_ = 0;
  current_mark_compact_mutator_utilization_ = 1.0;
  previous_mark_compact_end_time_ = 0;
  base::MutexGuard guard(&background_counter_mutex_);
  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    background_counter_[i].total_duration_ms = 0;
  }
}

void GCTracer::NotifyYoungGenerationHandling(
    YoungGenerationHandling young_generation_handling) {
  DCHECK_GE(1, start_counter_);
  DCHECK_EQ(Event::SCAVENGER, current_.type);
  heap_->isolate()->counters()->young_generation_handling()->AddSample(
      static_cast<int>(young_generation_handling));
}

void GCTracer::StartObservablePause() {
  DCHECK_EQ(0, start_counter_);
  start_counter_++;

  DCHECK(!IsInObservablePause());
  start_of_observable_pause_ = MonotonicallyIncreasingTimeInMs();
}

void GCTracer::UpdateCurrentEvent(GarbageCollectionReason gc_reason,
                                  const char* collector_reason) {
  // For incremental marking, the event has already been created and we just
  // need to update a few fields.
  DCHECK_EQ(Event::INCREMENTAL_MARK_COMPACTOR, current_.type);
  DCHECK_EQ(Event::State::ATOMIC, current_.state);
  DCHECK(IsInObservablePause());
  current_.gc_reason = gc_reason;
  current_.collector_reason = collector_reason;
  // TODO(chromium:1154636): The start_time of the current event contains
  // currently the start time of the observable pause. This should be
  // reconsidered.
  current_.start_time = start_of_observable_pause_;
  current_.reduce_memory = heap_->ShouldReduceMemory();
}

void GCTracer::StartCycle(GarbageCollector collector,
                          GarbageCollectionReason gc_reason,
                          const char* collector_reason, MarkingType marking) {
  // We cannot start a new cycle while there's another one in its atomic pause.
  DCHECK_NE(Event::State::ATOMIC, current_.state);
  // We cannot start a new cycle while a young generation GC cycle has
  // already interrupted a full GC cycle.
  DCHECK(!young_gc_while_full_gc_);

  young_gc_while_full_gc_ = current_.state != Event::State::NOT_RUNNING;

  DCHECK_IMPLIES(young_gc_while_full_gc_,
                 Heap::IsYoungGenerationCollector(collector) &&
                     !Event::IsYoungGenerationEvent(current_.type));

  Event::Type type;
  switch (collector) {
    case GarbageCollector::SCAVENGER:
      type = Event::SCAVENGER;
      break;
    case GarbageCollector::MINOR_MARK_COMPACTOR:
      type = Event::MINOR_MARK_COMPACTOR;
      break;
    case GarbageCollector::MARK_COMPACTOR:
      type = marking == MarkingType::kIncremental
                 ? Event::INCREMENTAL_MARK_COMPACTOR
                 : Event::MARK_COMPACTOR;
      break;
  }

  DCHECK_IMPLIES(!young_gc_while_full_gc_,
                 current_.state == Event::State::NOT_RUNNING);
  DCHECK_EQ(Event::State::NOT_RUNNING, previous_.state);

  previous_ = current_;
  current_ = Event(type, Event::State::MARKING, gc_reason, collector_reason);

  switch (marking) {
    case MarkingType::kAtomic:
      DCHECK(IsInObservablePause());
      // TODO(chromium:1154636): The start_time of the current event contains
      // currently the start time of the observable pause. This should be
      // reconsidered.
      current_.start_time = start_of_observable_pause_;
      current_.reduce_memory = heap_->ShouldReduceMemory();
      break;
    case MarkingType::kIncremental:
      // The current event will be updated later.
      DCHECK(!Heap::IsYoungGenerationCollector(collector));
      DCHECK(!IsInObservablePause());
      break;
  }

  if (Heap::IsYoungGenerationCollector(collector)) {
    epoch_young_ = next_epoch();
  } else {
    epoch_full_ = next_epoch();
  }
}

void GCTracer::StartAtomicPause() {
  DCHECK_EQ(Event::State::MARKING, current_.state);
  current_.state = Event::State::ATOMIC;
}

void GCTracer::StartInSafepoint() {
  SampleAllocation(current_.start_time, heap_->NewSpaceAllocationCounter(),
                   heap_->OldGenerationAllocationCounter(),
                   heap_->EmbedderAllocationCounter());

  current_.start_object_size = heap_->SizeOfObjects();
  current_.start_memory_size = heap_->memory_allocator()->Size();
  current_.start_holes_size = CountTotalHolesSize(heap_);
  size_t new_space_size = (heap_->new_space() ? heap_->new_space()->Size() : 0);
  size_t new_lo_space_size =
      (heap_->new_lo_space() ? heap_->new_lo_space()->SizeOfObjects() : 0);
  current_.young_object_size = new_space_size + new_lo_space_size;
}

void GCTracer::ResetIncrementalMarkingCounters() {
  incremental_marking_bytes_ = 0;
  incremental_marking_duration_ = 0;
  for (int i = 0; i < Scope::NUMBER_OF_INCREMENTAL_SCOPES; i++) {
    incremental_scopes_[i].ResetCurrentCycle();
  }
}

void GCTracer::StopInSafepoint() {
  current_.end_object_size = heap_->SizeOfObjects();
  current_.end_memory_size = heap_->memory_allocator()->Size();
  current_.end_holes_size = CountTotalHolesSize(heap_);
  current_.survived_young_object_size = heap_->SurvivedYoungObjectSize();
}

void GCTracer::StopObservablePause() {
  start_counter_--;
  DCHECK_EQ(0, start_counter_);

  DCHECK(IsInObservablePause());
  start_of_observable_pause_ = 0.0;

  // TODO(chromium:1154636): The end_time of the current event contains
  // currently the end time of the observable pause. This should be
  // reconsidered.
  current_.end_time = MonotonicallyIncreasingTimeInMs();
}

void GCTracer::UpdateStatistics(GarbageCollector collector) {
  const bool is_young = Heap::IsYoungGenerationCollector(collector);
  DCHECK(IsConsistentWithCollector(collector));

  AddAllocation(current_.end_time);

  double duration = current_.end_time - current_.start_time;
  int64_t duration_us =
      static_cast<int64_t>(duration * base::Time::kMicrosecondsPerMillisecond);
  auto* long_task_stats = heap_->isolate()->GetCurrentLongTaskStats();

  if (is_young) {
    recorded_minor_gcs_total_.Push(
        MakeBytesAndDuration(current_.young_object_size, duration));
    recorded_minor_gcs_survived_.Push(
        MakeBytesAndDuration(current_.survived_young_object_size, duration));
    long_task_stats->gc_young_wall_clock_duration_us += duration_us;
  } else {
    if (current_.type == Event::INCREMENTAL_MARK_COMPACTOR) {
      RecordIncrementalMarkingSpeed(incremental_marking_bytes_,
                                    incremental_marking_duration_);
      recorded_incremental_mark_compacts_.Push(
          MakeBytesAndDuration(current_.end_object_size, duration));
    } else {
      recorded_mark_compacts_.Push(
          MakeBytesAndDuration(current_.end_object_size, duration));
    }
    RecordMutatorUtilization(current_.end_time,
                             duration + incremental_marking_duration_);
    RecordGCSumCounters();
    combined_mark_compact_speed_cache_ = 0.0;
    long_task_stats->gc_full_atomic_wall_clock_duration_us += duration_us;
  }

  heap_->UpdateTotalGCTime(duration);

  if (FLAG_trace_gc_ignore_scavenger && is_young) return;

  if (FLAG_trace_gc_nvp) {
    PrintNVP();
  } else {
    Print();
  }

  if (FLAG_trace_gc) {
    heap_->PrintShortHeapStatistics();
  }

  if (V8_UNLIKELY(TracingFlags::gc.load(std::memory_order_relaxed) &
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    std::stringstream heap_stats;
    heap_->DumpJSONHeapStatistics(heap_stats);

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "V8.GC_Heap_Stats",
                         TRACE_EVENT_SCOPE_THREAD, "stats",
                         TRACE_STR_COPY(heap_stats.str().c_str()));
  }
}

void GCTracer::FinalizeCurrentEvent() {
  const bool is_young = Event::IsYoungGenerationEvent(current_.type);

  if (is_young) {
    FetchBackgroundMinorGCCounters();
  } else {
    if (current_.type == Event::INCREMENTAL_MARK_COMPACTOR) {
      current_.incremental_marking_bytes = incremental_marking_bytes_;
      current_.incremental_marking_duration = incremental_marking_duration_;
      for (int i = 0; i < Scope::NUMBER_OF_INCREMENTAL_SCOPES; i++) {
        current_.incremental_scopes[i] = incremental_scopes_[i];
        current_.scopes[i] = incremental_scopes_[i].duration;
      }
      ResetIncrementalMarkingCounters();
    } else {
      DCHECK_EQ(0u, incremental_marking_bytes_);
      DCHECK_EQ(0.0, incremental_marking_duration_);
      DCHECK_EQ(0u, current_.incremental_marking_bytes);
      DCHECK_EQ(0.0, current_.incremental_marking_duration);
    }
    FetchBackgroundMarkCompactCounters();
  }
  FetchBackgroundGeneralCounters();
}

void GCTracer::StopAtomicPause() {
  DCHECK_EQ(Event::State::ATOMIC, current_.state);
  current_.state = Event::State::SWEEPING;
}

void GCTracer::StopCycle(GarbageCollector collector) {
  DCHECK_EQ(Event::State::SWEEPING, current_.state);
  current_.state = Event::State::NOT_RUNNING;

  DCHECK(IsConsistentWithCollector(collector));
  FinalizeCurrentEvent();

  if (Heap::IsYoungGenerationCollector(collector)) {
    ReportYoungCycleToRecorder();

    // If a young generation GC interrupted an unfinished full GC cycle, restore
    // the event corresponding to the full GC cycle.
    if (young_gc_while_full_gc_) {
      std::swap(current_, previous_);
      young_gc_while_full_gc_ = false;
    }
  } else {
    ReportFullCycleToRecorder();

    heap_->isolate()->counters()->mark_compact_reason()->AddSample(
        static_cast<int>(current_.gc_reason));

    if (FLAG_trace_gc_freelists) {
      PrintIsolate(heap_->isolate(),
                   "FreeLists statistics before collection:\n");
      heap_->PrintFreeListsStats();
    }
  }
}

void GCTracer::StopFullCycleIfNeeded() {
  if (current_.state != Event::State::SWEEPING) return;
  if (!notified_sweeping_completed_) return;
  if (heap_->cpp_heap() && !notified_full_cppgc_completed_) return;
  StopCycle(GarbageCollector::MARK_COMPACTOR);
  notified_sweeping_completed_ = false;
  notified_full_cppgc_completed_ = false;
}

void GCTracer::StopYoungCycleIfNeeded() {
  // We rely here on the fact that young GCs in V8 are atomic and by the time
  // this is called, the Scavenger or Minor MC has already finished.
  DCHECK(Event::IsYoungGenerationEvent(current_.type));
  if (current_.state != Event::State::SWEEPING) return;
  // Check if young cppgc was scheduled but hasn't completed yet.
  if (heap_->cpp_heap() && notified_young_cppgc_running_ &&
      !notified_young_cppgc_completed_)
    return;
  StopCycle(current_.type == Event::SCAVENGER
                ? GarbageCollector::SCAVENGER
                : GarbageCollector::MINOR_MARK_COMPACTOR);
  notified_young_cppgc_running_ = false;
  notified_young_cppgc_completed_ = false;
}

void GCTracer::NotifySweepingCompleted() {
#ifdef VERIFY_HEAP
  // If heap verification is enabled, sweeping finalization can also be
  // triggered from inside a full GC cycle's atomic pause.
  DCHECK((current_.type == Event::MARK_COMPACTOR ||
          current_.type == Event::INCREMENTAL_MARK_COMPACTOR) &&
         (current_.state == Event::State::SWEEPING ||
          (FLAG_verify_heap && current_.state == Event::State::ATOMIC)));
#else
  DCHECK(IsSweepingInProgress());
#endif

  // Stop a full GC cycle only when both v8 and cppgc (if available) GCs have
  // finished sweeping. This method is invoked by v8.
  if (FLAG_trace_gc_freelists) {
    PrintIsolate(heap_->isolate(),
                 "FreeLists statistics after sweeping completed:\n");
    heap_->PrintFreeListsStats();
  }
  if (FLAG_trace_allocations_origins) {
    heap_->new_space()->PrintAllocationsOrigins();
    heap_->old_space()->PrintAllocationsOrigins();
    heap_->code_space()->PrintAllocationsOrigins();
    heap_->map_space()->PrintAllocationsOrigins();
  }
  DCHECK(!notified_sweeping_completed_);
  notified_sweeping_completed_ = true;
  StopFullCycleIfNeeded();
}

void GCTracer::NotifyFullCppGCCompleted() {
  // Stop a full GC cycle only when both v8 and cppgc (if available) GCs have
  // finished sweeping. This method is invoked by cppgc.
  DCHECK(heap_->cpp_heap());
  const auto* metric_recorder =
      CppHeap::From(heap_->cpp_heap())->GetMetricRecorder();
  USE(metric_recorder);
  DCHECK(metric_recorder->FullGCMetricsReportPending());
  DCHECK(!notified_full_cppgc_completed_);
  notified_full_cppgc_completed_ = true;
  StopFullCycleIfNeeded();
}

void GCTracer::NotifyYoungCppGCCompleted() {
  // Stop a young GC cycle only when both v8 and cppgc (if available) GCs have
  // finished sweeping. This method is invoked by cppgc.
  DCHECK(heap_->cpp_heap());
  DCHECK(notified_young_cppgc_running_);
  const auto* metric_recorder =
      CppHeap::From(heap_->cpp_heap())->GetMetricRecorder();
  USE(metric_recorder);
  DCHECK(metric_recorder->YoungGCMetricsReportPending());
  DCHECK(!notified_young_cppgc_completed_);
  notified_young_cppgc_completed_ = true;
  StopYoungCycleIfNeeded();
}

void GCTracer::NotifyYoungCppGCRunning() {
  DCHECK(!notified_young_cppgc_running_);
  notified_young_cppgc_running_ = true;
}

void GCTracer::SampleAllocation(double current_ms,
                                size_t new_space_counter_bytes,
                                size_t old_generation_counter_bytes,
                                size_t embedder_counter_bytes) {
  if (allocation_time_ms_ == 0) {
    // It is the first sample.
    allocation_time_ms_ = current_ms;
    new_space_allocation_counter_bytes_ = new_space_counter_bytes;
    old_generation_allocation_counter_bytes_ = old_generation_counter_bytes;
    embedder_allocation_counter_bytes_ = embedder_counter_bytes;
    return;
  }
  // This assumes that counters are unsigned integers so that the subtraction
  // below works even if the new counter is less than the old counter.
  size_t new_space_allocated_bytes =
      new_space_counter_bytes - new_space_allocation_counter_bytes_;
  size_t old_generation_allocated_bytes =
      old_generation_counter_bytes - old_generation_allocation_counter_bytes_;
  size_t embedder_allocated_bytes =
      embedder_counter_bytes - embedder_allocation_counter_bytes_;
  double duration = current_ms - allocation_time_ms_;
  allocation_time_ms_ = current_ms;
  new_space_allocation_counter_bytes_ = new_space_counter_bytes;
  old_generation_allocation_counter_bytes_ = old_generation_counter_bytes;
  embedder_allocation_counter_bytes_ = embedder_counter_bytes;
  allocation_duration_since_gc_ += duration;
  new_space_allocation_in_bytes_since_gc_ += new_space_allocated_bytes;
  old_generation_allocation_in_bytes_since_gc_ +=
      old_generation_allocated_bytes;
  embedder_allocation_in_bytes_since_gc_ += embedder_allocated_bytes;
}

void GCTracer::AddAllocation(double current_ms) {
  allocation_time_ms_ = current_ms;
  if (allocation_duration_since_gc_ > 0) {
    recorded_new_generation_allocations_.Push(
        MakeBytesAndDuration(new_space_allocation_in_bytes_since_gc_,
                             allocation_duration_since_gc_));
    recorded_old_generation_allocations_.Push(
        MakeBytesAndDuration(old_generation_allocation_in_bytes_since_gc_,
                             allocation_duration_since_gc_));
    recorded_embedder_generation_allocations_.Push(MakeBytesAndDuration(
        embedder_allocation_in_bytes_since_gc_, allocation_duration_since_gc_));
  }
  allocation_duration_since_gc_ = 0;
  new_space_allocation_in_bytes_since_gc_ = 0;
  old_generation_allocation_in_bytes_since_gc_ = 0;
  embedder_allocation_in_bytes_since_gc_ = 0;
}

void GCTracer::AddCompactionEvent(double duration,
                                  size_t live_bytes_compacted) {
  recorded_compactions_.Push(
      MakeBytesAndDuration(live_bytes_compacted, duration));
}

void GCTracer::AddSurvivalRatio(double promotion_ratio) {
  recorded_survival_ratios_.Push(promotion_ratio);
}

void GCTracer::AddIncrementalMarkingStep(double duration, size_t bytes) {
  if (bytes > 0) {
    incremental_marking_bytes_ += bytes;
    incremental_marking_duration_ += duration;
  }
  ReportIncrementalMarkingStepToRecorder(duration);
}

void GCTracer::AddIncrementalSweepingStep(double duration) {
  ReportIncrementalSweepingStepToRecorder(duration);
}

void GCTracer::Output(const char* format, ...) const {
  if (FLAG_trace_gc) {
    va_list arguments;
    va_start(arguments, format);
    base::OS::VPrint(format, arguments);
    va_end(arguments);
  }

  const int kBufferSize = 256;
  char raw_buffer[kBufferSize];
  base::Vector<char> buffer(raw_buffer, kBufferSize);
  va_list arguments2;
  va_start(arguments2, format);
  base::VSNPrintF(buffer, format, arguments2);
  va_end(arguments2);

  heap_->AddToRingBuffer(buffer.begin());
}

void GCTracer::Print() const {
  double duration = current_.end_time - current_.start_time;
  const size_t kIncrementalStatsSize = 128;
  char incremental_buffer[kIncrementalStatsSize] = {0};

  if (current_.type == Event::INCREMENTAL_MARK_COMPACTOR) {
    base::OS::SNPrintF(
        incremental_buffer, kIncrementalStatsSize,
        " (+ %.1f ms in %d steps since start of marking, "
        "biggest step %.1f ms, walltime since start of marking %.f ms)",
        current_scope(Scope::MC_INCREMENTAL),
        incremental_scope(Scope::MC_INCREMENTAL).steps,
        incremental_scope(Scope::MC_INCREMENTAL).longest_step,
        current_.end_time - incremental_marking_start_time_);
  }

  const double total_external_time =
      current_scope(Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES) +
      current_scope(Scope::HEAP_EXTERNAL_EPILOGUE) +
      current_scope(Scope::HEAP_EXTERNAL_PROLOGUE) +
      current_scope(Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE) +
      current_scope(Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE);

  // Avoid PrintF as Output also appends the string to the tracing ring buffer
  // that gets printed on OOM failures.
  Output(
      "[%d:%p] "
      "%8.0f ms: "
      "%s%s%s %.1f (%.1f) -> %.1f (%.1f) MB, "
      "%.1f / %.1f ms %s (average mu = %.3f, current mu = %.3f) %s; %s\n",
      base::OS::GetCurrentProcessId(),
      reinterpret_cast<void*>(heap_->isolate()),
      heap_->isolate()->time_millis_since_init(),
      heap_->IsShared() ? "Shared " : "", current_.TypeName(false),
      current_.reduce_memory ? " (reduce)" : "",
      static_cast<double>(current_.start_object_size) / MB,
      static_cast<double>(current_.start_memory_size) / MB,
      static_cast<double>(current_.end_object_size) / MB,
      static_cast<double>(current_.end_memory_size) / MB, duration,
      total_external_time, incremental_buffer,
      AverageMarkCompactMutatorUtilization(),
      CurrentMarkCompactMutatorUtilization(),
      Heap::GarbageCollectionReasonToString(current_.gc_reason),
      current_.collector_reason != nullptr ? current_.collector_reason : "");
}

void GCTracer::PrintNVP() const {
  double duration = current_.end_time - current_.start_time;
  double spent_in_mutator = current_.start_time - previous_.end_time;
  size_t allocated_since_last_gc =
      current_.start_object_size - previous_.end_object_size;

  double incremental_walltime_duration = 0;

  if (current_.type == Event::INCREMENTAL_MARK_COMPACTOR) {
    incremental_walltime_duration =
        current_.end_time - incremental_marking_start_time_;
  }

  // Avoid data races when printing the background scopes.
  base::MutexGuard guard(&background_counter_mutex_);

  switch (current_.type) {
    case Event::SCAVENGER:
      heap_->isolate()->PrintWithTimestamp(
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
          "time_to_safepoint=%.2f "
          "heap.prologue=%.2f "
          "heap.epilogue=%.2f "
          "heap.epilogue.reduce_new_space=%.2f "
          "heap.external.prologue=%.2f "
          "heap.external.epilogue=%.2f "
          "heap.external_weak_global_handles=%.2f "
          "fast_promote=%.2f "
          "complete.sweep_array_buffers=%.2f "
          "scavenge=%.2f "
          "scavenge.free_remembered_set=%.2f "
          "scavenge.roots=%.2f "
          "scavenge.weak=%.2f "
          "scavenge.weak_global_handles.identify=%.2f "
          "scavenge.weak_global_handles.process=%.2f "
          "scavenge.parallel=%.2f "
          "scavenge.update_refs=%.2f "
          "scavenge.sweep_array_buffers=%.2f "
          "background.scavenge.parallel=%.2f "
          "background.unmapper=%.2f "
          "unmapper=%.2f "
          "incremental.steps_count=%d "
          "incremental.steps_took=%.1f "
          "scavenge_throughput=%.f "
          "total_size_before=%zu "
          "total_size_after=%zu "
          "holes_size_before=%zu "
          "holes_size_after=%zu "
          "allocated=%zu "
          "promoted=%zu "
          "semi_space_copied=%zu "
          "nodes_died_in_new=%d "
          "nodes_copied_in_new=%d "
          "nodes_promoted=%d "
          "promotion_ratio=%.1f%% "
          "average_survival_ratio=%.1f%% "
          "promotion_rate=%.1f%% "
          "semi_space_copy_rate=%.1f%% "
          "new_space_allocation_throughput=%.1f "
          "unmapper_chunks=%d\n",
          duration, spent_in_mutator, current_.TypeName(true),
          current_.reduce_memory, current_.scopes[Scope::TIME_TO_SAFEPOINT],
          current_scope(Scope::HEAP_PROLOGUE),
          current_scope(Scope::HEAP_EPILOGUE),
          current_scope(Scope::HEAP_EPILOGUE_REDUCE_NEW_SPACE),
          current_scope(Scope::HEAP_EXTERNAL_PROLOGUE),
          current_scope(Scope::HEAP_EXTERNAL_EPILOGUE),
          current_scope(Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES),
          current_scope(Scope::SCAVENGER_FAST_PROMOTE),
          current_scope(Scope::SCAVENGER_COMPLETE_SWEEP_ARRAY_BUFFERS),
          current_scope(Scope::SCAVENGER_SCAVENGE),
          current_scope(Scope::SCAVENGER_FREE_REMEMBERED_SET),
          current_scope(Scope::SCAVENGER_SCAVENGE_ROOTS),
          current_scope(Scope::SCAVENGER_SCAVENGE_WEAK),
          current_scope(Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY),
          current_scope(Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS),
          current_scope(Scope::SCAVENGER_SCAVENGE_PARALLEL),
          current_scope(Scope::SCAVENGER_SCAVENGE_UPDATE_REFS),
          current_scope(Scope::SCAVENGER_SWEEP_ARRAY_BUFFERS),
          current_scope(Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL),
          current_scope(Scope::BACKGROUND_UNMAPPER),
          current_scope(Scope::UNMAPPER),
          incremental_scope(GCTracer::Scope::MC_INCREMENTAL).steps,
          current_scope(Scope::MC_INCREMENTAL),
          ScavengeSpeedInBytesPerMillisecond(), current_.start_object_size,
          current_.end_object_size, current_.start_holes_size,
          current_.end_holes_size, allocated_since_last_gc,
          heap_->promoted_objects_size(),
          heap_->semi_space_copied_object_size(),
          heap_->nodes_died_in_new_space_, heap_->nodes_copied_in_new_space_,
          heap_->nodes_promoted_, heap_->promotion_ratio_,
          AverageSurvivalRatio(), heap_->promotion_rate_,
          heap_->semi_space_copied_rate_,
          NewSpaceAllocationThroughputInBytesPerMillisecond(),
          heap_->memory_allocator()->unmapper()->NumberOfChunks());
      break;
    case Event::MINOR_MARK_COMPACTOR:
      heap_->isolate()->PrintWithTimestamp(
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
          "minor_mc=%.2f "
          "finish_sweeping=%.2f "
          "time_to_safepoint=%.2f "
          "mark=%.2f "
          "mark.seed=%.2f "
          "mark.roots=%.2f "
          "mark.weak=%.2f "
          "mark.global_handles=%.2f "
          "clear=%.2f "
          "clear.string_table=%.2f "
          "clear.weak_lists=%.2f "
          "evacuate=%.2f "
          "evacuate.copy=%.2f "
          "evacuate.update_pointers=%.2f "
          "evacuate.update_pointers.to_new_roots=%.2f "
          "evacuate.update_pointers.slots=%.2f "
          "background.mark=%.2f "
          "background.evacuate.copy=%.2f "
          "background.evacuate.update_pointers=%.2f "
          "background.unmapper=%.2f "
          "unmapper=%.2f "
          "update_marking_deque=%.2f "
          "reset_liveness=%.2f\n",
          duration, spent_in_mutator, "mmc", current_.reduce_memory,
          current_scope(Scope::MINOR_MC),
          current_scope(Scope::MINOR_MC_SWEEPING),
          current_scope(Scope::TIME_TO_SAFEPOINT),
          current_scope(Scope::MINOR_MC_MARK),
          current_scope(Scope::MINOR_MC_MARK_SEED),
          current_scope(Scope::MINOR_MC_MARK_ROOTS),
          current_scope(Scope::MINOR_MC_MARK_WEAK),
          current_scope(Scope::MINOR_MC_MARK_GLOBAL_HANDLES),
          current_scope(Scope::MINOR_MC_CLEAR),
          current_scope(Scope::MINOR_MC_CLEAR_STRING_TABLE),
          current_scope(Scope::MINOR_MC_CLEAR_WEAK_LISTS),
          current_scope(Scope::MINOR_MC_EVACUATE),
          current_scope(Scope::MINOR_MC_EVACUATE_COPY),
          current_scope(Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS),
          current_scope(Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS),
          current_scope(Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_SLOTS),
          current_scope(Scope::MINOR_MC_BACKGROUND_MARKING),
          current_scope(Scope::MINOR_MC_BACKGROUND_EVACUATE_COPY),
          current_scope(Scope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS),
          current_scope(Scope::BACKGROUND_UNMAPPER),
          current_scope(Scope::UNMAPPER),
          current_scope(Scope::MINOR_MC_MARKING_DEQUE),
          current_scope(Scope::MINOR_MC_RESET_LIVENESS));
      break;
    case Event::MARK_COMPACTOR:
    case Event::INCREMENTAL_MARK_COMPACTOR:
      heap_->isolate()->PrintWithTimestamp(
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
          "time_to_safepoint=%.2f "
          "heap.prologue=%.2f "
          "heap.embedder_tracing_epilogue=%.2f "
          "heap.epilogue=%.2f "
          "heap.epilogue.reduce_new_space=%.2f "
          "heap.external.prologue=%.1f "
          "heap.external.epilogue=%.1f "
          "heap.external.weak_global_handles=%.1f "
          "clear=%1.f "
          "clear.dependent_code=%.1f "
          "clear.maps=%.1f "
          "clear.slots_buffer=%.1f "
          "clear.string_table=%.1f "
          "clear.weak_collections=%.1f "
          "clear.weak_lists=%.1f "
          "clear.weak_references=%.1f "
          "complete.sweep_array_buffers=%.1f "
          "epilogue=%.1f "
          "evacuate=%.1f "
          "evacuate.candidates=%.1f "
          "evacuate.clean_up=%.1f "
          "evacuate.copy=%.1f "
          "evacuate.prologue=%.1f "
          "evacuate.epilogue=%.1f "
          "evacuate.rebalance=%.1f "
          "evacuate.update_pointers=%.1f "
          "evacuate.update_pointers.to_new_roots=%.1f "
          "evacuate.update_pointers.slots.main=%.1f "
          "evacuate.update_pointers.weak=%.1f "
          "finish=%.1f "
          "finish.sweep_array_buffers=%.1f "
          "mark=%.1f "
          "mark.finish_incremental=%.1f "
          "mark.roots=%.1f "
          "mark.main=%.1f "
          "mark.weak_closure=%.1f "
          "mark.weak_closure.ephemeron=%.1f "
          "mark.weak_closure.ephemeron.marking=%.1f "
          "mark.weak_closure.ephemeron.linear=%.1f "
          "mark.weak_closure.weak_handles=%.1f "
          "mark.weak_closure.weak_roots=%.1f "
          "mark.weak_closure.harmony=%.1f "
          "mark.embedder_prologue=%.1f "
          "mark.embedder_tracing=%.1f "
          "prologue=%.1f "
          "sweep=%.1f "
          "sweep.code=%.1f "
          "sweep.map=%.1f "
          "sweep.old=%.1f "
          "incremental=%.1f "
          "incremental.finalize=%.1f "
          "incremental.finalize.body=%.1f "
          "incremental.finalize.external.prologue=%.1f "
          "incremental.finalize.external.epilogue=%.1f "
          "incremental.layout_change=%.1f "
          "incremental.sweep_array_buffers=%.1f "
          "incremental.sweeping=%.1f "
          "incremental.embedder_prologue=%.1f "
          "incremental.embedder_tracing=%.1f "
          "incremental_wrapper_tracing_longest_step=%.1f "
          "incremental_finalize_longest_step=%.1f "
          "incremental_finalize_steps_count=%d "
          "incremental_longest_step=%.1f "
          "incremental_steps_count=%d "
          "incremental_marking_throughput=%.f "
          "incremental_walltime_duration=%.f "
          "background.mark=%.1f "
          "background.sweep=%.1f "
          "background.evacuate.copy=%.1f "
          "background.evacuate.update_pointers=%.1f "
          "background.unmapper=%.1f "
          "unmapper=%.1f "
          "total_size_before=%zu "
          "total_size_after=%zu "
          "holes_size_before=%zu "
          "holes_size_after=%zu "
          "allocated=%zu "
          "promoted=%zu "
          "semi_space_copied=%zu "
          "nodes_died_in_new=%d "
          "nodes_copied_in_new=%d "
          "nodes_promoted=%d "
          "promotion_ratio=%.1f%% "
          "average_survival_ratio=%.1f%% "
          "promotion_rate=%.1f%% "
          "semi_space_copy_rate=%.1f%% "
          "new_space_allocation_throughput=%.1f "
          "unmapper_chunks=%d "
          "compaction_speed=%.f\n",
          duration, spent_in_mutator, current_.TypeName(true),
          current_.reduce_memory, current_scope(Scope::TIME_TO_SAFEPOINT),
          current_scope(Scope::HEAP_PROLOGUE),
          current_scope(Scope::HEAP_EMBEDDER_TRACING_EPILOGUE),
          current_scope(Scope::HEAP_EPILOGUE),
          current_scope(Scope::HEAP_EPILOGUE_REDUCE_NEW_SPACE),
          current_scope(Scope::HEAP_EXTERNAL_PROLOGUE),
          current_scope(Scope::HEAP_EXTERNAL_EPILOGUE),
          current_scope(Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES),
          current_scope(Scope::MC_CLEAR),
          current_scope(Scope::MC_CLEAR_DEPENDENT_CODE),
          current_scope(Scope::MC_CLEAR_MAPS),
          current_scope(Scope::MC_CLEAR_SLOTS_BUFFER),
          current_scope(Scope::MC_CLEAR_STRING_TABLE),
          current_scope(Scope::MC_CLEAR_WEAK_COLLECTIONS),
          current_scope(Scope::MC_CLEAR_WEAK_LISTS),
          current_scope(Scope::MC_CLEAR_WEAK_REFERENCES),
          current_scope(Scope::MC_COMPLETE_SWEEP_ARRAY_BUFFERS),
          current_scope(Scope::MC_EPILOGUE), current_scope(Scope::MC_EVACUATE),
          current_scope(Scope::MC_EVACUATE_CANDIDATES),
          current_scope(Scope::MC_EVACUATE_CLEAN_UP),
          current_scope(Scope::MC_EVACUATE_COPY),
          current_scope(Scope::MC_EVACUATE_PROLOGUE),
          current_scope(Scope::MC_EVACUATE_EPILOGUE),
          current_scope(Scope::MC_EVACUATE_REBALANCE),
          current_scope(Scope::MC_EVACUATE_UPDATE_POINTERS),
          current_scope(Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS),
          current_scope(Scope::MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAIN),
          current_scope(Scope::MC_EVACUATE_UPDATE_POINTERS_WEAK),
          current_scope(Scope::MC_FINISH),
          current_scope(Scope::MC_FINISH_SWEEP_ARRAY_BUFFERS),
          current_scope(Scope::MC_MARK),
          current_scope(Scope::MC_MARK_FINISH_INCREMENTAL),
          current_scope(Scope::MC_MARK_ROOTS),
          current_scope(Scope::MC_MARK_MAIN),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_MARKING),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_LINEAR),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS),
          current_scope(Scope::MC_MARK_WEAK_CLOSURE_HARMONY),
          current_scope(Scope::MC_MARK_EMBEDDER_PROLOGUE),
          current_scope(Scope::MC_MARK_EMBEDDER_TRACING),
          current_scope(Scope::MC_PROLOGUE), current_scope(Scope::MC_SWEEP),
          current_scope(Scope::MC_SWEEP_CODE),
          current_scope(Scope::MC_SWEEP_MAP),
          current_scope(Scope::MC_SWEEP_OLD),
          current_scope(Scope::MC_INCREMENTAL),
          current_scope(Scope::MC_INCREMENTAL_FINALIZE),
          current_scope(Scope::MC_INCREMENTAL_FINALIZE_BODY),
          current_scope(Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE),
          current_scope(Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE),
          current_scope(Scope::MC_INCREMENTAL_LAYOUT_CHANGE),
          current_scope(Scope::MC_INCREMENTAL_START),
          current_scope(Scope::MC_INCREMENTAL_SWEEPING),
          current_scope(Scope::MC_INCREMENTAL_EMBEDDER_PROLOGUE),
          current_scope(Scope::MC_INCREMENTAL_EMBEDDER_TRACING),
          incremental_scope(Scope::MC_INCREMENTAL_EMBEDDER_TRACING)
              .longest_step,
          incremental_scope(Scope::MC_INCREMENTAL_FINALIZE_BODY).longest_step,
          incremental_scope(Scope::MC_INCREMENTAL_FINALIZE_BODY).steps,
          incremental_scope(Scope::MC_INCREMENTAL).longest_step,
          incremental_scope(Scope::MC_INCREMENTAL).steps,
          IncrementalMarkingSpeedInBytesPerMillisecond(),
          incremental_walltime_duration,
          current_scope(Scope::MC_BACKGROUND_MARKING),
          current_scope(Scope::MC_BACKGROUND_SWEEPING),
          current_scope(Scope::MC_BACKGROUND_EVACUATE_COPY),
          current_scope(Scope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS),
          current_scope(Scope::BACKGROUND_UNMAPPER),
          current_scope(Scope::UNMAPPER), current_.start_object_size,
          current_.end_object_size, current_.start_holes_size,
          current_.end_holes_size, allocated_since_last_gc,
          heap_->promoted_objects_size(),
          heap_->semi_space_copied_object_size(),
          heap_->nodes_died_in_new_space_, heap_->nodes_copied_in_new_space_,
          heap_->nodes_promoted_, heap_->promotion_ratio_,
          AverageSurvivalRatio(), heap_->promotion_rate_,
          heap_->semi_space_copied_rate_,
          NewSpaceAllocationThroughputInBytesPerMillisecond(),
          heap_->memory_allocator()->unmapper()->NumberOfChunks(),
          CompactionSpeedInBytesPerMillisecond());
      break;
    case Event::START:
      break;
    default:
      UNREACHABLE();
  }
}

double GCTracer::AverageSpeed(const base::RingBuffer<BytesAndDuration>& buffer,
                              const BytesAndDuration& initial, double time_ms) {
  BytesAndDuration sum = buffer.Sum(
      [time_ms](BytesAndDuration a, BytesAndDuration b) {
        if (time_ms != 0 && a.second >= time_ms) return a;
        return std::make_pair(a.first + b.first, a.second + b.second);
      },
      initial);
  uint64_t bytes = sum.first;
  double durations = sum.second;
  if (durations == 0.0) return 0;
  double speed = bytes / durations;
  const int max_speed = 1024 * MB;
  const int min_speed = 1;
  if (speed >= max_speed) return max_speed;
  if (speed <= min_speed) return min_speed;
  return speed;
}

double GCTracer::AverageSpeed(
    const base::RingBuffer<BytesAndDuration>& buffer) {
  return AverageSpeed(buffer, MakeBytesAndDuration(0, 0), 0);
}

void GCTracer::RecordIncrementalMarkingSpeed(size_t bytes, double duration) {
  if (duration == 0 || bytes == 0) return;
  double current_speed = bytes / duration;
  if (recorded_incremental_marking_speed_ == 0) {
    recorded_incremental_marking_speed_ = current_speed;
  } else {
    recorded_incremental_marking_speed_ =
        (recorded_incremental_marking_speed_ + current_speed) / 2;
  }
}

void GCTracer::RecordTimeToIncrementalMarkingTask(double time_to_task) {
  if (average_time_to_incremental_marking_task_ == 0.0) {
    average_time_to_incremental_marking_task_ = time_to_task;
  } else {
    average_time_to_incremental_marking_task_ =
        (average_time_to_incremental_marking_task_ + time_to_task) / 2;
  }
}

double GCTracer::AverageTimeToIncrementalMarkingTask() const {
  return average_time_to_incremental_marking_task_;
}

void GCTracer::RecordEmbedderSpeed(size_t bytes, double duration) {
  if (duration == 0 || bytes == 0) return;
  double current_speed = bytes / duration;
  if (recorded_embedder_speed_ == 0.0) {
    recorded_embedder_speed_ = current_speed;
  } else {
    recorded_embedder_speed_ = (recorded_embedder_speed_ + current_speed) / 2;
  }
}

void GCTracer::RecordMutatorUtilization(double mark_compact_end_time,
                                        double mark_compact_duration) {
  if (previous_mark_compact_end_time_ == 0) {
    // The first event only contributes to previous_mark_compact_end_time_,
    // because we cannot compute the mutator duration.
    previous_mark_compact_end_time_ = mark_compact_end_time;
  } else {
    double total_duration =
        mark_compact_end_time - previous_mark_compact_end_time_;
    double mutator_duration = total_duration - mark_compact_duration;
    if (average_mark_compact_duration_ == 0 && average_mutator_duration_ == 0) {
      // This is the first event with mutator and mark-compact durations.
      average_mark_compact_duration_ = mark_compact_duration;
      average_mutator_duration_ = mutator_duration;
    } else {
      average_mark_compact_duration_ =
          (average_mark_compact_duration_ + mark_compact_duration) / 2;
      average_mutator_duration_ =
          (average_mutator_duration_ + mutator_duration) / 2;
    }
    current_mark_compact_mutator_utilization_ =
        total_duration ? mutator_duration / total_duration : 0;
    previous_mark_compact_end_time_ = mark_compact_end_time;
  }
}

double GCTracer::AverageMarkCompactMutatorUtilization() const {
  double average_total_duration =
      average_mark_compact_duration_ + average_mutator_duration_;
  if (average_total_duration == 0) return 1.0;
  return average_mutator_duration_ / average_total_duration;
}

double GCTracer::CurrentMarkCompactMutatorUtilization() const {
  return current_mark_compact_mutator_utilization_;
}

double GCTracer::IncrementalMarkingSpeedInBytesPerMillisecond() const {
  if (recorded_incremental_marking_speed_ != 0) {
    return recorded_incremental_marking_speed_;
  }
  if (incremental_marking_duration_ != 0.0) {
    return incremental_marking_bytes_ / incremental_marking_duration_;
  }
  return kConservativeSpeedInBytesPerMillisecond;
}

double GCTracer::EmbedderSpeedInBytesPerMillisecond() const {
  // Note: Returning 0 is ok here as callers check for whether embedder speeds
  // have been recorded at all.
  return recorded_embedder_speed_;
}

double GCTracer::ScavengeSpeedInBytesPerMillisecond(
    ScavengeSpeedMode mode) const {
  if (mode == kForAllObjects) {
    return AverageSpeed(recorded_minor_gcs_total_);
  } else {
    return AverageSpeed(recorded_minor_gcs_survived_);
  }
}

double GCTracer::CompactionSpeedInBytesPerMillisecond() const {
  return AverageSpeed(recorded_compactions_);
}

double GCTracer::MarkCompactSpeedInBytesPerMillisecond() const {
  return AverageSpeed(recorded_mark_compacts_);
}

double GCTracer::FinalIncrementalMarkCompactSpeedInBytesPerMillisecond() const {
  return AverageSpeed(recorded_incremental_mark_compacts_);
}

double GCTracer::CombinedMarkCompactSpeedInBytesPerMillisecond() {
  const double kMinimumMarkingSpeed = 0.5;
  if (combined_mark_compact_speed_cache_ > 0)
    return combined_mark_compact_speed_cache_;
  // MarkCompact speed is more stable than incremental marking speed, because
  // there might not be many incremental marking steps because of concurrent
  // marking.
  combined_mark_compact_speed_cache_ = MarkCompactSpeedInBytesPerMillisecond();
  if (combined_mark_compact_speed_cache_ > 0)
    return combined_mark_compact_speed_cache_;
  double speed1 = IncrementalMarkingSpeedInBytesPerMillisecond();
  double speed2 = FinalIncrementalMarkCompactSpeedInBytesPerMillisecond();
  if (speed1 < kMinimumMarkingSpeed || speed2 < kMinimumMarkingSpeed) {
    // No data for the incremental marking speed.
    // Return the non-incremental mark-compact speed.
    combined_mark_compact_speed_cache_ =
        MarkCompactSpeedInBytesPerMillisecond();
  } else {
    // Combine the speed of incremental step and the speed of the final step.
    // 1 / (1 / speed1 + 1 / speed2) = speed1 * speed2 / (speed1 + speed2).
    combined_mark_compact_speed_cache_ = speed1 * speed2 / (speed1 + speed2);
  }
  return combined_mark_compact_speed_cache_;
}

double GCTracer::CombineSpeedsInBytesPerMillisecond(double default_speed,
                                                    double optional_speed) {
  constexpr double kMinimumSpeed = 0.5;
  if (optional_speed < kMinimumSpeed) {
    return default_speed;
  }
  return default_speed * optional_speed / (default_speed + optional_speed);
}

double GCTracer::NewSpaceAllocationThroughputInBytesPerMillisecond(
    double time_ms) const {
  size_t bytes = new_space_allocation_in_bytes_since_gc_;
  double durations = allocation_duration_since_gc_;
  return AverageSpeed(recorded_new_generation_allocations_,
                      MakeBytesAndDuration(bytes, durations), time_ms);
}

double GCTracer::OldGenerationAllocationThroughputInBytesPerMillisecond(
    double time_ms) const {
  size_t bytes = old_generation_allocation_in_bytes_since_gc_;
  double durations = allocation_duration_since_gc_;
  return AverageSpeed(recorded_old_generation_allocations_,
                      MakeBytesAndDuration(bytes, durations), time_ms);
}

double GCTracer::EmbedderAllocationThroughputInBytesPerMillisecond(
    double time_ms) const {
  size_t bytes = embedder_allocation_in_bytes_since_gc_;
  double durations = allocation_duration_since_gc_;
  return AverageSpeed(recorded_embedder_generation_allocations_,
                      MakeBytesAndDuration(bytes, durations), time_ms);
}

double GCTracer::AllocationThroughputInBytesPerMillisecond(
    double time_ms) const {
  return NewSpaceAllocationThroughputInBytesPerMillisecond(time_ms) +
         OldGenerationAllocationThroughputInBytesPerMillisecond(time_ms);
}

double GCTracer::CurrentAllocationThroughputInBytesPerMillisecond() const {
  return AllocationThroughputInBytesPerMillisecond(kThroughputTimeFrameMs);
}

double GCTracer::CurrentOldGenerationAllocationThroughputInBytesPerMillisecond()
    const {
  return OldGenerationAllocationThroughputInBytesPerMillisecond(
      kThroughputTimeFrameMs);
}

double GCTracer::CurrentEmbedderAllocationThroughputInBytesPerMillisecond()
    const {
  return EmbedderAllocationThroughputInBytesPerMillisecond(
      kThroughputTimeFrameMs);
}

double GCTracer::AverageSurvivalRatio() const {
  if (recorded_survival_ratios_.Count() == 0) return 0.0;
  double sum = recorded_survival_ratios_.Sum(
      [](double a, double b) { return a + b; }, 0.0);
  return sum / recorded_survival_ratios_.Count();
}

bool GCTracer::SurvivalEventsRecorded() const {
  return recorded_survival_ratios_.Count() > 0;
}

void GCTracer::ResetSurvivalEvents() { recorded_survival_ratios_.Reset(); }

void GCTracer::NotifyIncrementalMarkingStart() {
  incremental_marking_start_time_ = MonotonicallyIncreasingTimeInMs();
}

void GCTracer::FetchBackgroundMarkCompactCounters() {
  FetchBackgroundCounters(Scope::FIRST_MC_BACKGROUND_SCOPE,
                          Scope::LAST_MC_BACKGROUND_SCOPE);
  heap_->isolate()->counters()->background_marking()->AddSample(
      static_cast<int>(current_.scopes[Scope::MC_BACKGROUND_MARKING]));
  heap_->isolate()->counters()->background_sweeping()->AddSample(
      static_cast<int>(current_.scopes[Scope::MC_BACKGROUND_SWEEPING]));
}

void GCTracer::FetchBackgroundMinorGCCounters() {
  FetchBackgroundCounters(Scope::FIRST_MINOR_GC_BACKGROUND_SCOPE,
                          Scope::LAST_MINOR_GC_BACKGROUND_SCOPE);
}

void GCTracer::FetchBackgroundGeneralCounters() {
  FetchBackgroundCounters(Scope::FIRST_GENERAL_BACKGROUND_SCOPE,
                          Scope::LAST_GENERAL_BACKGROUND_SCOPE);
}

void GCTracer::FetchBackgroundCounters(int first_scope, int last_scope) {
  base::MutexGuard guard(&background_counter_mutex_);
  for (int i = first_scope; i <= last_scope; i++) {
    current_.scopes[i] += background_counter_[i].total_duration_ms;
    background_counter_[i].total_duration_ms = 0;
  }
}

void GCTracer::RecordGCPhasesHistograms(RecordGCPhasesInfo::Mode mode) {
  Counters* counters = heap_->isolate()->counters();
  if (mode == RecordGCPhasesInfo::Mode::Finalize) {
    DCHECK_EQ(Scope::FIRST_TOP_MC_SCOPE, Scope::MC_CLEAR);
    counters->gc_finalize_clear()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_CLEAR]));
    counters->gc_finalize_epilogue()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_EPILOGUE]));
    counters->gc_finalize_evacuate()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_EVACUATE]));
    counters->gc_finalize_finish()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_FINISH]));
    counters->gc_finalize_mark()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_MARK]));
    counters->gc_finalize_prologue()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_PROLOGUE]));
    counters->gc_finalize_sweep()->AddSample(
        static_cast<int>(current_.scopes[Scope::MC_SWEEP]));
    if (incremental_marking_duration_ > 0) {
      heap_->isolate()->counters()->incremental_marking_sum()->AddSample(
          static_cast<int>(incremental_marking_duration_));
    }
    const double overall_marking_time =
        incremental_marking_duration_ + current_.scopes[Scope::MC_MARK];
    heap_->isolate()->counters()->gc_marking_sum()->AddSample(
        static_cast<int>(overall_marking_time));

    // Filter out samples where
    // - we don't have high-resolution timers;
    // - size of marked objects is very small;
    // - marking time is rounded to 0;
    constexpr size_t kMinObjectSizeForReportingThroughput = 1024 * 1024;
    if (base::TimeTicks::IsHighResolution() &&
        heap_->SizeOfObjects() > kMinObjectSizeForReportingThroughput &&
        overall_marking_time > 0) {
      const double overall_v8_marking_time =
          overall_marking_time -
          current_.scopes[Scope::MC_MARK_EMBEDDER_TRACING];
      if (overall_v8_marking_time > 0) {
        const int main_thread_marking_throughput_mb_per_s =
            static_cast<int>(static_cast<double>(heap_->SizeOfObjects()) /
                             overall_v8_marking_time * 1000 / 1024 / 1024);
        heap_->isolate()
            ->counters()
            ->gc_main_thread_marking_throughput()
            ->AddSample(
                static_cast<int>(main_thread_marking_throughput_mb_per_s));
      }
    }

    DCHECK_EQ(Scope::LAST_TOP_MC_SCOPE, Scope::MC_SWEEP);
  } else if (mode == RecordGCPhasesInfo::Mode::Scavenger) {
    counters->gc_scavenger_scavenge_main()->AddSample(
        static_cast<int>(current_.scopes[Scope::SCAVENGER_SCAVENGE_PARALLEL]));
    counters->gc_scavenger_scavenge_roots()->AddSample(
        static_cast<int>(current_.scopes[Scope::SCAVENGER_SCAVENGE_ROOTS]));
  }
}

void GCTracer::RecordGCSumCounters() {
  base::MutexGuard guard(&background_counter_mutex_);

  const double atomic_pause_duration = current_.scopes[Scope::MARK_COMPACTOR];
  const double incremental_marking =
      incremental_scopes_[Scope::MC_INCREMENTAL_LAYOUT_CHANGE].duration +
      incremental_scopes_[Scope::MC_INCREMENTAL_START].duration +
      incremental_marking_duration_ +
      incremental_scopes_[Scope::MC_INCREMENTAL_FINALIZE].duration;
  const double incremental_sweeping =
      incremental_scopes_[Scope::MC_INCREMENTAL_SWEEPING].duration;
  const double overall_duration =
      atomic_pause_duration + incremental_marking + incremental_sweeping;
  const double background_duration =
      background_counter_[Scope::MC_BACKGROUND_EVACUATE_COPY]
          .total_duration_ms +
      background_counter_[Scope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS]
          .total_duration_ms +
      background_counter_[Scope::MC_BACKGROUND_MARKING].total_duration_ms +
      background_counter_[Scope::MC_BACKGROUND_SWEEPING].total_duration_ms;
  const double atomic_marking_duration =
      current_.scopes[Scope::MC_PROLOGUE] + current_.scopes[Scope::MC_MARK];
  const double marking_duration = atomic_marking_duration + incremental_marking;
  const double marking_background_duration =
      background_counter_[Scope::MC_BACKGROUND_MARKING].total_duration_ms;

  // Emit trace event counters.
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                       "V8.GCMarkCompactorSummary", TRACE_EVENT_SCOPE_THREAD,
                       "duration", overall_duration, "background_duration",
                       background_duration);
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                       "V8.GCMarkCompactorMarkingSummary",
                       TRACE_EVENT_SCOPE_THREAD, "duration", marking_duration,
                       "background_duration", marking_background_duration);
}

namespace {

void CopyTimeMetrics(
    ::v8::metrics::GarbageCollectionPhases& metrics,
    const cppgc::internal::MetricRecorder::GCCycle::IncrementalPhases&
        cppgc_metrics) {
  DCHECK_NE(-1, cppgc_metrics.mark_duration_us);
  metrics.mark_wall_clock_duration_in_us = cppgc_metrics.mark_duration_us;
  DCHECK_NE(-1, cppgc_metrics.sweep_duration_us);
  metrics.sweep_wall_clock_duration_in_us = cppgc_metrics.sweep_duration_us;
  metrics.total_wall_clock_duration_in_us =
      metrics.mark_wall_clock_duration_in_us +
      metrics.sweep_wall_clock_duration_in_us;
}

void CopyTimeMetrics(
    ::v8::metrics::GarbageCollectionPhases& metrics,
    const cppgc::internal::MetricRecorder::GCCycle::Phases& cppgc_metrics) {
  DCHECK_NE(-1, cppgc_metrics.compact_duration_us);
  metrics.compact_wall_clock_duration_in_us = cppgc_metrics.compact_duration_us;
  DCHECK_NE(-1, cppgc_metrics.mark_duration_us);
  metrics.mark_wall_clock_duration_in_us = cppgc_metrics.mark_duration_us;
  DCHECK_NE(-1, cppgc_metrics.sweep_duration_us);
  metrics.sweep_wall_clock_duration_in_us = cppgc_metrics.sweep_duration_us;
  DCHECK_NE(-1, cppgc_metrics.weak_duration_us);
  metrics.weak_wall_clock_duration_in_us = cppgc_metrics.weak_duration_us;
  metrics.total_wall_clock_duration_in_us =
      metrics.compact_wall_clock_duration_in_us +
      metrics.mark_wall_clock_duration_in_us +
      metrics.sweep_wall_clock_duration_in_us +
      metrics.weak_wall_clock_duration_in_us;
}

void CopySizeMetrics(
    ::v8::metrics::GarbageCollectionSizes& metrics,
    const cppgc::internal::MetricRecorder::GCCycle::Sizes& cppgc_metrics) {
  DCHECK_NE(-1, cppgc_metrics.after_bytes);
  metrics.bytes_after = cppgc_metrics.after_bytes;
  DCHECK_NE(-1, cppgc_metrics.before_bytes);
  metrics.bytes_before = cppgc_metrics.before_bytes;
  DCHECK_NE(-1, cppgc_metrics.freed_bytes);
  metrics.bytes_freed = cppgc_metrics.freed_bytes;
}

::v8::metrics::Recorder::ContextId GetContextId(
    v8::internal::Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  if (isolate->context().is_null())
    return v8::metrics::Recorder::ContextId::Empty();
  HandleScope scope(isolate);
  return isolate->GetOrRegisterRecorderContextId(isolate->native_context());
}

template <typename EventType>
void FlushBatchedEvents(
    v8::metrics::GarbageCollectionBatchedEvents<EventType>& batched_events,
    Isolate* isolate) {
  DCHECK_NOT_NULL(isolate->metrics_recorder());
  DCHECK(!batched_events.events.empty());
  isolate->metrics_recorder()->AddMainThreadEvent(std::move(batched_events),
                                                  GetContextId(isolate));
  batched_events = {};
}

}  // namespace

void GCTracer::ReportFullCycleToRecorder() {
  DCHECK(!Event::IsYoungGenerationEvent(current_.type));
  DCHECK_EQ(Event::State::NOT_RUNNING, current_.state);
  auto* cpp_heap = v8::internal::CppHeap::From(heap_->cpp_heap());
  DCHECK_IMPLIES(cpp_heap,
                 cpp_heap->GetMetricRecorder()->FullGCMetricsReportPending());
  const std::shared_ptr<metrics::Recorder>& recorder =
      heap_->isolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!recorder->HasEmbedderRecorder()) {
    incremental_mark_batched_events_ = {};
    incremental_sweep_batched_events_ = {};
    if (cpp_heap) {
      cpp_heap->GetMetricRecorder()->ClearCachedEvents();
    }
    return;
  }
  if (!incremental_mark_batched_events_.events.empty()) {
    FlushBatchedEvents(incremental_mark_batched_events_, heap_->isolate());
  }
  if (!incremental_sweep_batched_events_.events.empty()) {
    FlushBatchedEvents(incremental_sweep_batched_events_, heap_->isolate());
  }

  v8::metrics::GarbageCollectionFullCycle event;
  event.reason = static_cast<int>(current_.gc_reason);

  // Managed C++ heap statistics:
  if (cpp_heap) {
    cpp_heap->GetMetricRecorder()->FlushBatchedIncrementalEvents();
    const base::Optional<cppgc::internal::MetricRecorder::GCCycle>
        optional_cppgc_event =
            cpp_heap->GetMetricRecorder()->ExtractLastFullGcEvent();
    DCHECK(optional_cppgc_event.has_value());
    DCHECK(!cpp_heap->GetMetricRecorder()->FullGCMetricsReportPending());
    const cppgc::internal::MetricRecorder::GCCycle& cppgc_event =
        optional_cppgc_event.value();
    DCHECK_EQ(cppgc_event.type,
              cppgc::internal::MetricRecorder::GCCycle::Type::kMajor);
    CopyTimeMetrics(event.total_cpp, cppgc_event.total);
    CopyTimeMetrics(event.main_thread_cpp, cppgc_event.main_thread);
    CopyTimeMetrics(event.main_thread_atomic_cpp,
                    cppgc_event.main_thread_atomic);
    CopyTimeMetrics(event.main_thread_incremental_cpp,
                    cppgc_event.main_thread_incremental);
    CopySizeMetrics(event.objects_cpp, cppgc_event.objects);
    CopySizeMetrics(event.memory_cpp, cppgc_event.memory);
    DCHECK_NE(-1, cppgc_event.collection_rate_in_percent);
    event.collection_rate_cpp_in_percent =
        cppgc_event.collection_rate_in_percent;
    DCHECK_NE(-1, cppgc_event.efficiency_in_bytes_per_us);
    event.efficiency_cpp_in_bytes_per_us =
        cppgc_event.efficiency_in_bytes_per_us;
    DCHECK_NE(-1, cppgc_event.main_thread_efficiency_in_bytes_per_us);
    event.main_thread_efficiency_cpp_in_bytes_per_us =
        cppgc_event.main_thread_efficiency_in_bytes_per_us;
  }

  // Unified heap statistics:
  const double atomic_pause_duration = current_.scopes[Scope::MARK_COMPACTOR];
  const double incremental_marking =
      current_.incremental_scopes[Scope::MC_INCREMENTAL_LAYOUT_CHANGE]
          .duration +
      current_.incremental_scopes[Scope::MC_INCREMENTAL_START].duration +
      current_.incremental_marking_duration +
      current_.incremental_scopes[Scope::MC_INCREMENTAL_FINALIZE].duration;
  const double incremental_sweeping =
      current_.incremental_scopes[Scope::MC_INCREMENTAL_SWEEPING].duration;
  const double overall_duration =
      atomic_pause_duration + incremental_marking + incremental_sweeping;
  const double marking_background_duration =
      current_.scopes[Scope::MC_BACKGROUND_MARKING];
  const double sweeping_background_duration =
      current_.scopes[Scope::MC_BACKGROUND_SWEEPING];
  const double compact_background_duration =
      current_.scopes[Scope::MC_BACKGROUND_EVACUATE_COPY] +
      current_.scopes[Scope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS];
  const double background_duration = marking_background_duration +
                                     sweeping_background_duration +
                                     compact_background_duration;
  const double atomic_marking_duration =
      current_.scopes[Scope::MC_PROLOGUE] + current_.scopes[Scope::MC_MARK];
  const double marking_duration = atomic_marking_duration + incremental_marking;
  const double weak_duration = current_.scopes[Scope::MC_CLEAR];
  const double compact_duration = current_.scopes[Scope::MC_EVACUATE] +
                                  current_.scopes[Scope::MC_FINISH] +
                                  current_.scopes[Scope::MC_EPILOGUE];
  const double atomic_sweeping_duration = current_.scopes[Scope::MC_SWEEP];
  const double sweeping_duration =
      atomic_sweeping_duration + incremental_sweeping;

  event.main_thread_atomic.total_wall_clock_duration_in_us =
      static_cast<int64_t>(atomic_pause_duration *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread.total_wall_clock_duration_in_us = static_cast<int64_t>(
      overall_duration * base::Time::kMicrosecondsPerMillisecond);
  event.total.total_wall_clock_duration_in_us =
      static_cast<int64_t>((overall_duration + background_duration) *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread_atomic.mark_wall_clock_duration_in_us =
      static_cast<int64_t>(atomic_marking_duration *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread.mark_wall_clock_duration_in_us = static_cast<int64_t>(
      marking_duration * base::Time::kMicrosecondsPerMillisecond);
  event.total.mark_wall_clock_duration_in_us =
      static_cast<int64_t>((marking_duration + marking_background_duration) *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread_atomic.weak_wall_clock_duration_in_us =
      event.main_thread.weak_wall_clock_duration_in_us =
          event.total.weak_wall_clock_duration_in_us = static_cast<int64_t>(
              weak_duration * base::Time::kMicrosecondsPerMillisecond);
  event.main_thread_atomic.compact_wall_clock_duration_in_us =
      event.main_thread.compact_wall_clock_duration_in_us =
          static_cast<int64_t>(compact_duration *
                               base::Time::kMicrosecondsPerMillisecond);
  event.total.compact_wall_clock_duration_in_us =
      static_cast<int64_t>((compact_duration + compact_background_duration) *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread_atomic.sweep_wall_clock_duration_in_us =
      static_cast<int64_t>(atomic_sweeping_duration *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread.sweep_wall_clock_duration_in_us = static_cast<int64_t>(
      sweeping_duration * base::Time::kMicrosecondsPerMillisecond);
  event.total.sweep_wall_clock_duration_in_us =
      static_cast<int64_t>((sweeping_duration + sweeping_background_duration) *
                           base::Time::kMicrosecondsPerMillisecond);
  event.main_thread_incremental.mark_wall_clock_duration_in_us =
      incremental_marking;
  event.main_thread_incremental.sweep_wall_clock_duration_in_us =
      incremental_sweeping;

  // TODO(chromium:1154636): Populate the following:
  // - event.objects
  // - event.memory
  // - event.collection_rate_in_percent
  // - event.efficiency_in_bytes_per_us
  // - event.main_thread_efficiency_in_bytes_per_us

  recorder->AddMainThreadEvent(event, GetContextId(heap_->isolate()));
}

void GCTracer::ReportIncrementalMarkingStepToRecorder(double v8_duration) {
  DCHECK_EQ(Event::Type::INCREMENTAL_MARK_COMPACTOR, current_.type);
  static constexpr int kMaxBatchedEvents =
      CppHeap::MetricRecorderAdapter::kMaxBatchedEvents;
  const std::shared_ptr<metrics::Recorder>& recorder =
      heap_->isolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!recorder->HasEmbedderRecorder()) return;
  incremental_mark_batched_events_.events.emplace_back();
  if (heap_->cpp_heap()) {
    const base::Optional<
        cppgc::internal::MetricRecorder::MainThreadIncrementalMark>
        cppgc_event = v8::internal::CppHeap::From(heap_->cpp_heap())
                          ->GetMetricRecorder()
                          ->ExtractLastIncrementalMarkEvent();
    if (cppgc_event.has_value()) {
      DCHECK_NE(-1, cppgc_event.value().duration_us);
      incremental_mark_batched_events_.events.back()
          .cpp_wall_clock_duration_in_us = cppgc_event.value().duration_us;
    }
  }
  incremental_mark_batched_events_.events.back().wall_clock_duration_in_us =
      static_cast<int64_t>(v8_duration *
                           base::Time::kMicrosecondsPerMillisecond);
  if (incremental_mark_batched_events_.events.size() == kMaxBatchedEvents) {
    FlushBatchedEvents(incremental_mark_batched_events_, heap_->isolate());
  }
}

void GCTracer::ReportIncrementalSweepingStepToRecorder(double v8_duration) {
  static constexpr int kMaxBatchedEvents =
      CppHeap::MetricRecorderAdapter::kMaxBatchedEvents;
  const std::shared_ptr<metrics::Recorder>& recorder =
      heap_->isolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!recorder->HasEmbedderRecorder()) return;
  incremental_sweep_batched_events_.events.emplace_back();
  incremental_sweep_batched_events_.events.back().wall_clock_duration_in_us =
      static_cast<int64_t>(v8_duration *
                           base::Time::kMicrosecondsPerMillisecond);
  if (incremental_sweep_batched_events_.events.size() == kMaxBatchedEvents) {
    FlushBatchedEvents(incremental_sweep_batched_events_, heap_->isolate());
  }
}

void GCTracer::ReportYoungCycleToRecorder() {
  DCHECK(Event::IsYoungGenerationEvent(current_.type));
  DCHECK_EQ(Event::State::NOT_RUNNING, current_.state);
  const std::shared_ptr<metrics::Recorder>& recorder =
      heap_->isolate()->metrics_recorder();
  DCHECK_NOT_NULL(recorder);
  if (!recorder->HasEmbedderRecorder()) return;

  v8::metrics::GarbageCollectionYoungCycle event;
  // Reason:
  event.reason = static_cast<int>(current_.gc_reason);
#if defined(CPPGC_YOUNG_GENERATION)
  // Managed C++ heap statistics:
  auto* cpp_heap = v8::internal::CppHeap::From(heap_->cpp_heap());
  if (cpp_heap) {
    auto* metric_recorder = cpp_heap->GetMetricRecorder();
    const base::Optional<cppgc::internal::MetricRecorder::GCCycle>
        optional_cppgc_event = metric_recorder->ExtractLastYoungGcEvent();
    // We bail out from Oilpan's young GC if the full GC is already in progress.
    // Check here if the young generation event was reported.
    if (optional_cppgc_event) {
      DCHECK(!metric_recorder->YoungGCMetricsReportPending());
      const cppgc::internal::MetricRecorder::GCCycle& cppgc_event =
          optional_cppgc_event.value();
      DCHECK_EQ(cppgc_event.type,
                cppgc::internal::MetricRecorder::GCCycle::Type::kMinor);
      CopyTimeMetrics(event.total_cpp, cppgc_event.total);
      CopySizeMetrics(event.objects_cpp, cppgc_event.objects);
      CopySizeMetrics(event.memory_cpp, cppgc_event.memory);
      DCHECK_NE(-1, cppgc_event.collection_rate_in_percent);
      event.collection_rate_cpp_in_percent =
          cppgc_event.collection_rate_in_percent;
      DCHECK_NE(-1, cppgc_event.efficiency_in_bytes_per_us);
      event.efficiency_cpp_in_bytes_per_us =
          cppgc_event.efficiency_in_bytes_per_us;
      DCHECK_NE(-1, cppgc_event.main_thread_efficiency_in_bytes_per_us);
      event.main_thread_efficiency_cpp_in_bytes_per_us =
          cppgc_event.main_thread_efficiency_in_bytes_per_us;
    }
  }
#endif  // defined(CPPGC_YOUNG_GENERATION)

  // Total:
  const double total_wall_clock_duration_in_us =
      (current_.scopes[Scope::SCAVENGER] +
       current_.scopes[Scope::MINOR_MARK_COMPACTOR] +
       current_.scopes[Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL] +
       current_.scopes[Scope::MINOR_MC_BACKGROUND_EVACUATE_COPY] +
       current_.scopes[Scope::MINOR_MC_BACKGROUND_MARKING] +
       current_.scopes[Scope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS]) *
      base::Time::kMicrosecondsPerMillisecond;
  // TODO(chromium:1154636): Consider adding BACKGROUND_YOUNG_ARRAY_BUFFER_SWEEP
  // (both for the case of the scavenger and the minor mark-compactor), and
  // BACKGROUND_UNMAPPER (for the case of the minor mark-compactor).
  event.total_wall_clock_duration_in_us =
      static_cast<int64_t>(total_wall_clock_duration_in_us);
  // MainThread:
  const double main_thread_wall_clock_duration_in_us =
      (current_.scopes[Scope::SCAVENGER] +
       current_.scopes[Scope::MINOR_MARK_COMPACTOR]) *
      base::Time::kMicrosecondsPerMillisecond;
  event.main_thread_wall_clock_duration_in_us =
      static_cast<int64_t>(main_thread_wall_clock_duration_in_us);
  // Collection Rate:
  event.collection_rate_in_percent =
      static_cast<double>(current_.survived_young_object_size) /
      current_.young_object_size;
  // Efficiency:
  auto freed_bytes =
      current_.young_object_size - current_.survived_young_object_size;
  event.efficiency_in_bytes_per_us =
      freed_bytes / total_wall_clock_duration_in_us;
  event.main_thread_efficiency_in_bytes_per_us =
      freed_bytes / main_thread_wall_clock_duration_in_us;

  recorder->AddMainThreadEvent(event, GetContextId(heap_->isolate()));
}

}  // namespace internal
}  // namespace v8
