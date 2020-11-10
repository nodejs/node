// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-tracer.h"

#include <cstdarg>

#include "src/base/atomic-utils.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/logging/counters-inl.h"

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
WorkerThreadRuntimeCallStats* GCTracer::worker_thread_runtime_call_stats() {
  return heap_->isolate()->counters()->worker_thread_runtime_call_stats();
}

RuntimeCallCounterId GCTracer::RCSCounterFromScope(Scope::ScopeId id) {
  STATIC_ASSERT(Scope::FIRST_SCOPE == Scope::MC_INCREMENTAL);
  return static_cast<RuntimeCallCounterId>(
      static_cast<int>(RuntimeCallCounterId::kGC_MC_INCREMENTAL) +
      static_cast<int>(id));
}

RuntimeCallCounterId GCTracer::RCSCounterFromBackgroundScope(
    BackgroundScope::ScopeId id) {
  STATIC_ASSERT(Scope::FIRST_BACKGROUND_SCOPE ==
                Scope::BACKGROUND_ARRAY_BUFFER_FREE);
  STATIC_ASSERT(
      0 == static_cast<int>(BackgroundScope::BACKGROUND_ARRAY_BUFFER_FREE));
  return static_cast<RuntimeCallCounterId>(
      static_cast<int>(RCSCounterFromScope(Scope::FIRST_BACKGROUND_SCOPE)) +
      static_cast<int>(id));
}

GCTracer::Scope::Scope(GCTracer* tracer, ScopeId scope)
    : tracer_(tracer), scope_(scope) {
  start_time_ = tracer_->heap_->MonotonicallyIncreasingTimeInMs();
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;
  runtime_stats_ = tracer_->heap_->isolate()->counters()->runtime_call_stats();
  runtime_stats_->Enter(&timer_, GCTracer::RCSCounterFromScope(scope));
}

GCTracer::Scope::~Scope() {
  tracer_->AddScopeSample(
      scope_, tracer_->heap_->MonotonicallyIncreasingTimeInMs() - start_time_);
  if (V8_LIKELY(runtime_stats_ == nullptr)) return;
  runtime_stats_->Leave(&timer_);
}

GCTracer::BackgroundScope::BackgroundScope(GCTracer* tracer, ScopeId scope,
                                           RuntimeCallStats* runtime_stats)
    : tracer_(tracer), scope_(scope), runtime_stats_(runtime_stats) {
  start_time_ = tracer_->heap_->MonotonicallyIncreasingTimeInMs();
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;
  runtime_stats_->Enter(&timer_,
                        GCTracer::RCSCounterFromBackgroundScope(scope));
}

GCTracer::BackgroundScope::~BackgroundScope() {
  double duration_ms =
      tracer_->heap_->MonotonicallyIncreasingTimeInMs() - start_time_;
  tracer_->AddBackgroundScopeSample(scope_, duration_ms);
  if (V8_LIKELY(runtime_stats_ == nullptr)) return;
  runtime_stats_->Leave(&timer_);
}

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
  return nullptr;
}

const char* GCTracer::BackgroundScope::Name(ScopeId id) {
#define CASE(scope)            \
  case BackgroundScope::scope: \
    return "V8.GC_" #scope;
  switch (id) {
    TRACER_BACKGROUND_SCOPES(CASE)
    case BackgroundScope::NUMBER_OF_SCOPES:
      break;
  }
#undef CASE
  UNREACHABLE();
  return nullptr;
}

GCTracer::Event::Event(Type type, GarbageCollectionReason gc_reason,
                       const char* collector_reason)
    : type(type),
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

GCTracer::GCTracer(Heap* heap)
    : heap_(heap),
      current_(Event::START, GarbageCollectionReason::kUnknown, nullptr),
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
  current_.end_time = heap_->MonotonicallyIncreasingTimeInMs();
  for (int i = 0; i < BackgroundScope::NUMBER_OF_SCOPES; i++) {
    background_counter_[i].total_duration_ms = 0;
  }
}

void GCTracer::ResetForTesting() {
  current_ = Event(Event::START, GarbageCollectionReason::kTesting, nullptr);
  current_.end_time = heap_->MonotonicallyIncreasingTimeInMs();
  previous_ = current_;
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
  recorded_context_disposal_times_.Reset();
  recorded_survival_ratios_.Reset();
  start_counter_ = 0;
  average_mutator_duration_ = 0;
  average_mark_compact_duration_ = 0;
  current_mark_compact_mutator_utilization_ = 1.0;
  previous_mark_compact_end_time_ = 0;
  base::MutexGuard guard(&background_counter_mutex_);
  for (int i = 0; i < BackgroundScope::NUMBER_OF_SCOPES; i++) {
    background_counter_[i].total_duration_ms = 0;
  }
}

void GCTracer::NotifyYoungGenerationHandling(
    YoungGenerationHandling young_generation_handling) {
  DCHECK(current_.type == Event::SCAVENGER || start_counter_ > 1);
  heap_->isolate()->counters()->young_generation_handling()->AddSample(
      static_cast<int>(young_generation_handling));
}

void GCTracer::Start(GarbageCollector collector,
                     GarbageCollectionReason gc_reason,
                     const char* collector_reason) {
  start_counter_++;
  if (start_counter_ != 1) return;

  previous_ = current_;
  double start_time = heap_->MonotonicallyIncreasingTimeInMs();
  SampleAllocation(start_time, heap_->NewSpaceAllocationCounter(),
                   heap_->OldGenerationAllocationCounter(),
                   heap_->EmbedderAllocationCounter());

  switch (collector) {
    case SCAVENGER:
      current_ = Event(Event::SCAVENGER, gc_reason, collector_reason);
      break;
    case MINOR_MARK_COMPACTOR:
      current_ =
          Event(Event::MINOR_MARK_COMPACTOR, gc_reason, collector_reason);
      break;
    case MARK_COMPACTOR:
      if (heap_->incremental_marking()->WasActivated()) {
        current_ = Event(Event::INCREMENTAL_MARK_COMPACTOR, gc_reason,
                         collector_reason);
      } else {
        current_ = Event(Event::MARK_COMPACTOR, gc_reason, collector_reason);
      }
      break;
  }

  current_.reduce_memory = heap_->ShouldReduceMemory();
  current_.start_time = start_time;
  current_.start_object_size = 0;
  current_.start_memory_size = 0;
  current_.start_holes_size = 0;
  current_.young_object_size = 0;

  current_.incremental_marking_bytes = 0;
  current_.incremental_marking_duration = 0;

  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    current_.scopes[i] = 0;
  }

  Counters* counters = heap_->isolate()->counters();

  if (Heap::IsYoungGenerationCollector(collector)) {
    counters->scavenge_reason()->AddSample(static_cast<int>(gc_reason));
  } else {
    counters->mark_compact_reason()->AddSample(static_cast<int>(gc_reason));

    if (FLAG_trace_gc_freelists) {
      PrintIsolate(heap_->isolate(),
                   "FreeLists statistics before collection:\n");
      heap_->PrintFreeListsStats();
    }
  }
}

void GCTracer::StartInSafepoint() {
  current_.start_object_size = heap_->SizeOfObjects();
  current_.start_memory_size = heap_->memory_allocator()->Size();
  current_.start_holes_size = CountTotalHolesSize(heap_);
  current_.young_object_size =
      heap_->new_space()->Size() + heap_->new_lo_space()->SizeOfObjects();
}

void GCTracer::ResetIncrementalMarkingCounters() {
  incremental_marking_bytes_ = 0;
  incremental_marking_duration_ = 0;
  for (int i = 0; i < Scope::NUMBER_OF_INCREMENTAL_SCOPES; i++) {
    incremental_marking_scopes_[i].ResetCurrentCycle();
  }
}

void GCTracer::StopInSafepoint() {
  current_.end_object_size = heap_->SizeOfObjects();
  current_.end_memory_size = heap_->memory_allocator()->Size();
  current_.end_holes_size = CountTotalHolesSize(heap_);
  current_.survived_young_object_size = heap_->SurvivedYoungObjectSize();
}

void GCTracer::Stop(GarbageCollector collector) {
  start_counter_--;
  if (start_counter_ != 0) {
    if (FLAG_trace_gc_verbose) {
      heap_->isolate()->PrintWithTimestamp(
          "[Finished reentrant %s during %s.]\n",
          Heap::CollectorName(collector), current_.TypeName(false));
    }
    return;
  }

  DCHECK_LE(0, start_counter_);
  DCHECK((collector == SCAVENGER && current_.type == Event::SCAVENGER) ||
         (collector == MINOR_MARK_COMPACTOR &&
          current_.type == Event::MINOR_MARK_COMPACTOR) ||
         (collector == MARK_COMPACTOR &&
          (current_.type == Event::MARK_COMPACTOR ||
           current_.type == Event::INCREMENTAL_MARK_COMPACTOR)));

  current_.end_time = heap_->MonotonicallyIncreasingTimeInMs();

  AddAllocation(current_.end_time);

  double duration = current_.end_time - current_.start_time;

  switch (current_.type) {
    case Event::SCAVENGER:
    case Event::MINOR_MARK_COMPACTOR:
      recorded_minor_gcs_total_.Push(
          MakeBytesAndDuration(current_.young_object_size, duration));
      recorded_minor_gcs_survived_.Push(
          MakeBytesAndDuration(current_.survived_young_object_size, duration));
      FetchBackgroundMinorGCCounters();
      break;
    case Event::INCREMENTAL_MARK_COMPACTOR:
      current_.incremental_marking_bytes = incremental_marking_bytes_;
      current_.incremental_marking_duration = incremental_marking_duration_;
      for (int i = 0; i < Scope::NUMBER_OF_INCREMENTAL_SCOPES; i++) {
        current_.incremental_marking_scopes[i] = incremental_marking_scopes_[i];
        current_.scopes[i] = incremental_marking_scopes_[i].duration;
      }

      RecordMutatorUtilization(
          current_.end_time, duration + current_.incremental_marking_duration);
      RecordIncrementalMarkingSpeed(current_.incremental_marking_bytes,
                                    current_.incremental_marking_duration);
      recorded_incremental_mark_compacts_.Push(
          MakeBytesAndDuration(current_.end_object_size, duration));
      RecordGCSumCounters(duration);
      ResetIncrementalMarkingCounters();
      combined_mark_compact_speed_cache_ = 0.0;
      FetchBackgroundMarkCompactCounters();
      break;
    case Event::MARK_COMPACTOR:
      DCHECK_EQ(0u, current_.incremental_marking_bytes);
      DCHECK_EQ(0, current_.incremental_marking_duration);
      RecordMutatorUtilization(
          current_.end_time, duration + current_.incremental_marking_duration);
      recorded_mark_compacts_.Push(
          MakeBytesAndDuration(current_.end_object_size, duration));
      RecordGCSumCounters(duration);
      ResetIncrementalMarkingCounters();
      combined_mark_compact_speed_cache_ = 0.0;
      FetchBackgroundMarkCompactCounters();
      break;
    case Event::START:
      UNREACHABLE();
  }
  FetchBackgroundGeneralCounters();

  heap_->UpdateTotalGCTime(duration);

  if ((current_.type == Event::SCAVENGER ||
       current_.type == Event::MINOR_MARK_COMPACTOR) &&
      FLAG_trace_gc_ignore_scavenger)
    return;

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

void GCTracer::NotifySweepingCompleted() {
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


void GCTracer::AddContextDisposalTime(double time) {
  recorded_context_disposal_times_.Push(time);
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
  Vector<char> buffer(raw_buffer, kBufferSize);
  va_list arguments2;
  va_start(arguments2, format);
  VSNPrintF(buffer, format, arguments2);
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
        current_.scopes[Scope::MC_INCREMENTAL],
        current_.incremental_marking_scopes[Scope::MC_INCREMENTAL].steps,
        current_.incremental_marking_scopes[Scope::MC_INCREMENTAL].longest_step,
        current_.end_time - incremental_marking_start_time_);
  }

  // Avoid PrintF as Output also appends the string to the tracing ring buffer
  // that gets printed on OOM failures.
  Output(
      "[%d:%p] "
      "%8.0f ms: "
      "%s%s %.1f (%.1f) -> %.1f (%.1f) MB, "
      "%.1f / %.1f ms %s (average mu = %.3f, current mu = %.3f) %s %s\n",
      base::OS::GetCurrentProcessId(),
      reinterpret_cast<void*>(heap_->isolate()),
      heap_->isolate()->time_millis_since_init(), current_.TypeName(false),
      current_.reduce_memory ? " (reduce)" : "",
      static_cast<double>(current_.start_object_size) / MB,
      static_cast<double>(current_.start_memory_size) / MB,
      static_cast<double>(current_.end_object_size) / MB,
      static_cast<double>(current_.end_memory_size) / MB, duration,
      TotalExternalTime(), incremental_buffer,
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

  switch (current_.type) {
    case Event::SCAVENGER:
      heap_->isolate()->PrintWithTimestamp(
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
          "heap.prologue=%.2f "
          "heap.epilogue=%.2f "
          "heap.epilogue.reduce_new_space=%.2f "
          "heap.external.prologue=%.2f "
          "heap.external.epilogue=%.2f "
          "heap.external_weak_global_handles=%.2f "
          "fast_promote=%.2f "
          "complete.sweep_array_buffers=%.2f "
          "scavenge=%.2f "
          "scavenge.process_array_buffers=%.2f "
          "scavenge.free_remembered_set=%.2f "
          "scavenge.roots=%.2f "
          "scavenge.weak=%.2f "
          "scavenge.weak_global_handles.identify=%.2f "
          "scavenge.weak_global_handles.process=%.2f "
          "scavenge.parallel=%.2f "
          "scavenge.update_refs=%.2f "
          "scavenge.sweep_array_buffers=%.2f "
          "background.scavenge.parallel=%.2f "
          "background.array_buffer_free=%.2f "
          "background.store_buffer=%.2f "
          "background.unmapper=%.2f "
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
          "unmapper_chunks=%d "
          "context_disposal_rate=%.1f\n",
          duration, spent_in_mutator, current_.TypeName(true),
          current_.reduce_memory, current_.scopes[Scope::HEAP_PROLOGUE],
          current_.scopes[Scope::HEAP_EPILOGUE],
          current_.scopes[Scope::HEAP_EPILOGUE_REDUCE_NEW_SPACE],
          current_.scopes[Scope::HEAP_EXTERNAL_PROLOGUE],
          current_.scopes[Scope::HEAP_EXTERNAL_EPILOGUE],
          current_.scopes[Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES],
          current_.scopes[Scope::SCAVENGER_SWEEP_ARRAY_BUFFERS],
          current_.scopes[Scope::SCAVENGER_FAST_PROMOTE],
          current_.scopes[Scope::SCAVENGER_SCAVENGE],
          current_.scopes[Scope::SCAVENGER_PROCESS_ARRAY_BUFFERS],
          current_.scopes[Scope::SCAVENGER_FREE_REMEMBERED_SET],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_ROOTS],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_WEAK],
          current_
              .scopes[Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY],
          current_
              .scopes[Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_PARALLEL],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_UPDATE_REFS],
          current_.scopes[Scope::SCAVENGER_SWEEP_ARRAY_BUFFERS],
          current_.scopes[Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL],
          current_.scopes[Scope::BACKGROUND_ARRAY_BUFFER_FREE],
          current_.scopes[Scope::BACKGROUND_STORE_BUFFER],
          current_.scopes[Scope::BACKGROUND_UNMAPPER],
          current_.incremental_marking_scopes[GCTracer::Scope::MC_INCREMENTAL]
              .steps,
          current_.scopes[Scope::MC_INCREMENTAL],
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
          heap_->memory_allocator()->unmapper()->NumberOfChunks(),
          ContextDisposalRateInMilliseconds());
      break;
    case Event::MINOR_MARK_COMPACTOR:
      heap_->isolate()->PrintWithTimestamp(
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
          "minor_mc=%.2f "
          "finish_sweeping=%.2f "
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
          "background.array_buffer_free=%.2f "
          "background.store_buffer=%.2f "
          "background.unmapper=%.2f "
          "update_marking_deque=%.2f "
          "reset_liveness=%.2f\n",
          duration, spent_in_mutator, "mmc", current_.reduce_memory,
          current_.scopes[Scope::MINOR_MC],
          current_.scopes[Scope::MINOR_MC_SWEEPING],
          current_.scopes[Scope::MINOR_MC_MARK],
          current_.scopes[Scope::MINOR_MC_MARK_SEED],
          current_.scopes[Scope::MINOR_MC_MARK_ROOTS],
          current_.scopes[Scope::MINOR_MC_MARK_WEAK],
          current_.scopes[Scope::MINOR_MC_MARK_GLOBAL_HANDLES],
          current_.scopes[Scope::MINOR_MC_CLEAR],
          current_.scopes[Scope::MINOR_MC_CLEAR_STRING_TABLE],
          current_.scopes[Scope::MINOR_MC_CLEAR_WEAK_LISTS],
          current_.scopes[Scope::MINOR_MC_EVACUATE],
          current_.scopes[Scope::MINOR_MC_EVACUATE_COPY],
          current_.scopes[Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS],
          current_
              .scopes[Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS],
          current_.scopes[Scope::MINOR_MC_EVACUATE_UPDATE_POINTERS_SLOTS],
          current_.scopes[Scope::MINOR_MC_BACKGROUND_MARKING],
          current_.scopes[Scope::MINOR_MC_BACKGROUND_EVACUATE_COPY],
          current_.scopes[Scope::MINOR_MC_BACKGROUND_EVACUATE_UPDATE_POINTERS],
          current_.scopes[Scope::BACKGROUND_ARRAY_BUFFER_FREE],
          current_.scopes[Scope::BACKGROUND_STORE_BUFFER],
          current_.scopes[Scope::BACKGROUND_UNMAPPER],
          current_.scopes[Scope::MINOR_MC_MARKING_DEQUE],
          current_.scopes[Scope::MINOR_MC_RESET_LIVENESS]);
      break;
    case Event::MARK_COMPACTOR:
    case Event::INCREMENTAL_MARK_COMPACTOR:
      heap_->isolate()->PrintWithTimestamp(
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
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
          "clear.store_buffer=%.1f "
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
          "evacuate.update_pointers.slots.map_space=%.1f "
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
          "incremental.start=%.1f "
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
          "background.array_buffer_free=%.2f "
          "background.store_buffer=%.2f "
          "background.unmapper=%.1f "
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
          "context_disposal_rate=%.1f "
          "compaction_speed=%.f\n",
          duration, spent_in_mutator, current_.TypeName(true),
          current_.reduce_memory, current_.scopes[Scope::HEAP_PROLOGUE],
          current_.scopes[Scope::HEAP_EMBEDDER_TRACING_EPILOGUE],
          current_.scopes[Scope::HEAP_EPILOGUE],
          current_.scopes[Scope::HEAP_EPILOGUE_REDUCE_NEW_SPACE],
          current_.scopes[Scope::HEAP_EXTERNAL_PROLOGUE],
          current_.scopes[Scope::HEAP_EXTERNAL_EPILOGUE],
          current_.scopes[Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES],
          current_.scopes[Scope::MC_CLEAR],
          current_.scopes[Scope::MC_CLEAR_DEPENDENT_CODE],
          current_.scopes[Scope::MC_CLEAR_MAPS],
          current_.scopes[Scope::MC_CLEAR_SLOTS_BUFFER],
          current_.scopes[Scope::MC_CLEAR_STORE_BUFFER],
          current_.scopes[Scope::MC_CLEAR_STRING_TABLE],
          current_.scopes[Scope::MC_CLEAR_WEAK_COLLECTIONS],
          current_.scopes[Scope::MC_CLEAR_WEAK_LISTS],
          current_.scopes[Scope::MC_CLEAR_WEAK_REFERENCES],
          current_.scopes[Scope::MC_COMPLETE_SWEEP_ARRAY_BUFFERS],
          current_.scopes[Scope::MC_EPILOGUE],
          current_.scopes[Scope::MC_EVACUATE],
          current_.scopes[Scope::MC_EVACUATE_CANDIDATES],
          current_.scopes[Scope::MC_EVACUATE_CLEAN_UP],
          current_.scopes[Scope::MC_EVACUATE_COPY],
          current_.scopes[Scope::MC_EVACUATE_PROLOGUE],
          current_.scopes[Scope::MC_EVACUATE_EPILOGUE],
          current_.scopes[Scope::MC_EVACUATE_REBALANCE],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW_ROOTS],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAIN],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_SLOTS_MAP_SPACE],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_WEAK],
          current_.scopes[Scope::MC_FINISH],
          current_.scopes[Scope::MC_FINISH_SWEEP_ARRAY_BUFFERS],
          current_.scopes[Scope::MC_MARK],
          current_.scopes[Scope::MC_MARK_FINISH_INCREMENTAL],
          current_.scopes[Scope::MC_MARK_ROOTS],
          current_.scopes[Scope::MC_MARK_MAIN],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_MARKING],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_EPHEMERON_LINEAR],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_HARMONY],
          current_.scopes[Scope::MC_MARK_EMBEDDER_PROLOGUE],
          current_.scopes[Scope::MC_MARK_EMBEDDER_TRACING],
          current_.scopes[Scope::MC_PROLOGUE], current_.scopes[Scope::MC_SWEEP],
          current_.scopes[Scope::MC_SWEEP_CODE],
          current_.scopes[Scope::MC_SWEEP_MAP],
          current_.scopes[Scope::MC_SWEEP_OLD],
          current_.scopes[Scope::MC_INCREMENTAL],
          current_.scopes[Scope::MC_INCREMENTAL_FINALIZE],
          current_.scopes[Scope::MC_INCREMENTAL_FINALIZE_BODY],
          current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_LAYOUT_CHANGE],
          current_.scopes[Scope::MC_INCREMENTAL_START],
          current_.scopes[Scope::MC_INCREMENTAL_SWEEP_ARRAY_BUFFERS],
          current_.scopes[Scope::MC_INCREMENTAL_SWEEPING],
          current_.scopes[Scope::MC_INCREMENTAL_EMBEDDER_PROLOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_EMBEDDER_TRACING],
          current_
              .incremental_marking_scopes
                  [Scope::MC_INCREMENTAL_EMBEDDER_TRACING]
              .longest_step,
          current_
              .incremental_marking_scopes[Scope::MC_INCREMENTAL_FINALIZE_BODY]
              .longest_step,
          current_
              .incremental_marking_scopes[Scope::MC_INCREMENTAL_FINALIZE_BODY]
              .steps,
          current_.incremental_marking_scopes[Scope::MC_INCREMENTAL]
              .longest_step,
          current_.incremental_marking_scopes[Scope::MC_INCREMENTAL].steps,
          IncrementalMarkingSpeedInBytesPerMillisecond(),
          incremental_walltime_duration,
          current_.scopes[Scope::MC_BACKGROUND_MARKING],
          current_.scopes[Scope::MC_BACKGROUND_SWEEPING],
          current_.scopes[Scope::MC_BACKGROUND_EVACUATE_COPY],
          current_.scopes[Scope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS],
          current_.scopes[Scope::BACKGROUND_ARRAY_BUFFER_FREE],
          current_.scopes[Scope::BACKGROUND_STORE_BUFFER],
          current_.scopes[Scope::BACKGROUND_UNMAPPER],
          current_.start_object_size, current_.end_object_size,
          current_.start_holes_size, current_.end_holes_size,
          allocated_since_last_gc, heap_->promoted_objects_size(),
          heap_->semi_space_copied_object_size(),
          heap_->nodes_died_in_new_space_, heap_->nodes_copied_in_new_space_,
          heap_->nodes_promoted_, heap_->promotion_ratio_,
          AverageSurvivalRatio(), heap_->promotion_rate_,
          heap_->semi_space_copied_rate_,
          NewSpaceAllocationThroughputInBytesPerMillisecond(),
          heap_->memory_allocator()->unmapper()->NumberOfChunks(),
          ContextDisposalRateInMilliseconds(),
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

double GCTracer::ContextDisposalRateInMilliseconds() const {
  if (recorded_context_disposal_times_.Count() <
      recorded_context_disposal_times_.kSize)
    return 0.0;
  double begin = heap_->MonotonicallyIncreasingTimeInMs();
  double end = recorded_context_disposal_times_.Sum(
      [](double a, double b) { return b; }, 0.0);
  return (begin - end) / recorded_context_disposal_times_.Count();
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
  incremental_marking_start_time_ = heap_->MonotonicallyIncreasingTimeInMs();
}

void GCTracer::FetchBackgroundMarkCompactCounters() {
  FetchBackgroundCounters(Scope::FIRST_MC_BACKGROUND_SCOPE,
                          Scope::LAST_MC_BACKGROUND_SCOPE,
                          BackgroundScope::FIRST_MC_BACKGROUND_SCOPE,
                          BackgroundScope::LAST_MC_BACKGROUND_SCOPE);
  heap_->isolate()->counters()->background_marking()->AddSample(
      static_cast<int>(current_.scopes[Scope::MC_BACKGROUND_MARKING]));
  heap_->isolate()->counters()->background_sweeping()->AddSample(
      static_cast<int>(current_.scopes[Scope::MC_BACKGROUND_SWEEPING]));
}

void GCTracer::FetchBackgroundMinorGCCounters() {
  FetchBackgroundCounters(Scope::FIRST_MINOR_GC_BACKGROUND_SCOPE,
                          Scope::LAST_MINOR_GC_BACKGROUND_SCOPE,
                          BackgroundScope::FIRST_MINOR_GC_BACKGROUND_SCOPE,
                          BackgroundScope::LAST_MINOR_GC_BACKGROUND_SCOPE);
  heap_->isolate()->counters()->background_scavenger()->AddSample(
      static_cast<int>(
          current_.scopes[Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL]));
}

void GCTracer::FetchBackgroundGeneralCounters() {
  FetchBackgroundCounters(Scope::FIRST_GENERAL_BACKGROUND_SCOPE,
                          Scope::LAST_GENERAL_BACKGROUND_SCOPE,
                          BackgroundScope::FIRST_GENERAL_BACKGROUND_SCOPE,
                          BackgroundScope::LAST_GENERAL_BACKGROUND_SCOPE);
}

void GCTracer::FetchBackgroundCounters(int first_global_scope,
                                       int last_global_scope,
                                       int first_background_scope,
                                       int last_background_scope) {
  DCHECK_EQ(last_global_scope - first_global_scope,
            last_background_scope - first_background_scope);
  base::MutexGuard guard(&background_counter_mutex_);
  int background_mc_scopes = last_background_scope - first_background_scope + 1;
  for (int i = 0; i < background_mc_scopes; i++) {
    current_.scopes[first_global_scope + i] +=
        background_counter_[first_background_scope + i].total_duration_ms;
    background_counter_[first_background_scope + i].total_duration_ms = 0;
  }
}

void GCTracer::AddBackgroundScopeSample(BackgroundScope::ScopeId scope,
                                        double duration) {
  base::MutexGuard guard(&background_counter_mutex_);
  BackgroundCounter& counter = background_counter_[scope];
  counter.total_duration_ms += duration;
}

void GCTracer::RecordGCPhasesHistograms(TimedHistogram* gc_timer) {
  Counters* counters = heap_->isolate()->counters();
  if (gc_timer == counters->gc_finalize()) {
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

    constexpr size_t kMinObjectSizeForReportingThroughput = 1024 * 1024;
    if (base::TimeTicks::IsHighResolution() &&
        heap_->SizeOfObjects() > kMinObjectSizeForReportingThroughput) {
      DCHECK_GT(overall_marking_time, 0.0);
      const double overall_v8_marking_time =
          overall_marking_time -
          current_.scopes[Scope::MC_MARK_EMBEDDER_TRACING];
      DCHECK_GT(overall_v8_marking_time, 0.0);
      const int main_thread_marking_throughput_mb_per_s =
          static_cast<int>(static_cast<double>(heap_->SizeOfObjects()) /
                           overall_v8_marking_time * 1000 / 1024 / 1024);
      heap_->isolate()
          ->counters()
          ->gc_main_thread_marking_throughput()
          ->AddSample(
              static_cast<int>(main_thread_marking_throughput_mb_per_s));
    }

    DCHECK_EQ(Scope::LAST_TOP_MC_SCOPE, Scope::MC_SWEEP);
  } else if (gc_timer == counters->gc_scavenger()) {
    counters->gc_scavenger_scavenge_main()->AddSample(
        static_cast<int>(current_.scopes[Scope::SCAVENGER_SCAVENGE_PARALLEL]));
    counters->gc_scavenger_scavenge_roots()->AddSample(
        static_cast<int>(current_.scopes[Scope::SCAVENGER_SCAVENGE_ROOTS]));
  }
}

void GCTracer::RecordGCSumCounters(double atomic_pause_duration) {
  base::MutexGuard guard(&background_counter_mutex_);

  const double overall_duration =
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_LAYOUT_CHANGE]
          .duration +
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_START]
          .duration +
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_SWEEPING]
          .duration +
      incremental_marking_duration_ +
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_FINALIZE]
          .duration +
      atomic_pause_duration;
  const double background_duration =
      background_counter_[BackgroundScope::MC_BACKGROUND_EVACUATE_COPY]
          .total_duration_ms +
      background_counter_
          [BackgroundScope::MC_BACKGROUND_EVACUATE_UPDATE_POINTERS]
              .total_duration_ms +
      background_counter_[BackgroundScope::MC_BACKGROUND_MARKING]
          .total_duration_ms +
      background_counter_[BackgroundScope::MC_BACKGROUND_SWEEPING]
          .total_duration_ms;

  const double marking_duration =
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_LAYOUT_CHANGE]
          .duration +
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_START]
          .duration +
      incremental_marking_duration_ +
      current_.incremental_marking_scopes[Scope::MC_INCREMENTAL_FINALIZE]
          .duration +
      current_.scopes[Scope::MC_MARK];
  const double marking_background_duration =
      background_counter_[BackgroundScope::MC_BACKGROUND_MARKING]
          .total_duration_ms;

  // UMA.
  heap_->isolate()->counters()->gc_mark_compactor()->AddSample(
      static_cast<int>(overall_duration));

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

}  // namespace internal
}  // namespace v8
