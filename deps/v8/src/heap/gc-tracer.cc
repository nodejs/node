// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-tracer.h"

#include <cstdarg>

#include "src/base/atomic-utils.h"
#include "src/counters.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

static size_t CountTotalHolesSize(Heap* heap) {
  size_t holes_size = 0;
  PagedSpaces spaces(heap);
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    DCHECK_GE(holes_size + space->Waste() + space->Available(), holes_size);
    holes_size += space->Waste() + space->Available();
  }
  return holes_size;
}

RuntimeCallCounterId GCTracer::RCSCounterFromScope(Scope::ScopeId id) {
  STATIC_ASSERT(Scope::FIRST_SCOPE == Scope::MC_INCREMENTAL);
  return static_cast<RuntimeCallCounterId>(
      static_cast<int>(RuntimeCallCounterId::kGC_MC_INCREMENTAL) +
      static_cast<int>(id));
}

GCTracer::Scope::Scope(GCTracer* tracer, ScopeId scope)
    : tracer_(tracer), scope_(scope) {
  start_time_ = tracer_->heap_->MonotonicallyIncreasingTimeInMs();
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (V8_LIKELY(!FLAG_runtime_stats)) return;
  runtime_stats_ = tracer_->heap_->isolate()->counters()->runtime_call_stats();
  runtime_stats_->Enter(&timer_, GCTracer::RCSCounterFromScope(scope));
}

GCTracer::Scope::~Scope() {
  tracer_->AddScopeSample(
      scope_, tracer_->heap_->MonotonicallyIncreasingTimeInMs() - start_time_);
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (V8_LIKELY(runtime_stats_ == nullptr)) return;
  runtime_stats_->Leave(&timer_);
}

GCTracer::BackgroundScope::BackgroundScope(GCTracer* tracer, ScopeId scope)
    : tracer_(tracer), scope_(scope), runtime_stats_enabled_(false) {
  start_time_ = tracer_->heap_->MonotonicallyIncreasingTimeInMs();
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (V8_LIKELY(!base::AsAtomic32::Relaxed_Load(&FLAG_runtime_stats))) return;
  timer_.Start(&counter_, nullptr);
  runtime_stats_enabled_ = true;
}

GCTracer::BackgroundScope::~BackgroundScope() {
  double duration_ms =
      tracer_->heap_->MonotonicallyIncreasingTimeInMs() - start_time_;
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (V8_LIKELY(!runtime_stats_enabled_)) {
    tracer_->AddBackgroundScopeSample(scope_, duration_ms, nullptr);
  } else {
    timer_.Stop();
    tracer_->AddBackgroundScopeSample(scope_, duration_ms, &counter_);
  }
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
      new_space_object_size(0),
      survived_new_space_object_size(0),
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
      allocation_duration_since_gc_(0.0),
      new_space_allocation_in_bytes_since_gc_(0),
      old_generation_allocation_in_bytes_since_gc_(0),
      combined_mark_compact_speed_cache_(0.0),
      start_counter_(0) {
  // All accesses to incremental_marking_scope assume that incremental marking
  // scopes come first.
  STATIC_ASSERT(0 == Scope::FIRST_INCREMENTAL_SCOPE);
  // We assume that MC_INCREMENTAL is the first scope so that we can properly
  // map it to RuntimeCallStats.
  STATIC_ASSERT(0 == Scope::MC_INCREMENTAL);
  current_.end_time = heap_->MonotonicallyIncreasingTimeInMs();
  for (int i = 0; i < BackgroundScope::NUMBER_OF_SCOPES; i++) {
    background_counter_[i].total_duration_ms = 0;
    background_counter_[i].runtime_call_counter = RuntimeCallCounter(nullptr);
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
  recorded_context_disposal_times_.Reset();
  recorded_survival_ratios_.Reset();
  start_counter_ = 0;
  base::LockGuard<base::Mutex> guard(&background_counter_mutex_);
  for (int i = 0; i < BackgroundScope::NUMBER_OF_SCOPES; i++) {
    background_counter_[i].total_duration_ms = 0;
    background_counter_[i].runtime_call_counter.Reset();
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
                   heap_->OldGenerationAllocationCounter());

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
  current_.start_object_size = heap_->SizeOfObjects();
  current_.start_memory_size = heap_->memory_allocator()->Size();
  current_.start_holes_size = CountTotalHolesSize(heap_);
  current_.new_space_object_size = heap_->new_space()->Size();

  current_.incremental_marking_bytes = 0;
  current_.incremental_marking_duration = 0;

  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    current_.scopes[i] = 0;
  }

  size_t committed_memory = heap_->CommittedMemory() / KB;
  size_t used_memory = current_.start_object_size / KB;

  Counters* counters = heap_->isolate()->counters();

  if (Heap::IsYoungGenerationCollector(collector)) {
    counters->scavenge_reason()->AddSample(static_cast<int>(gc_reason));
  } else {
    counters->mark_compact_reason()->AddSample(static_cast<int>(gc_reason));
  }
  counters->aggregated_memory_heap_committed()->AddSample(start_time,
                                                          committed_memory);
  counters->aggregated_memory_heap_used()->AddSample(start_time, used_memory);
}

void GCTracer::ResetIncrementalMarkingCounters() {
  if (incremental_marking_duration_ > 0) {
    heap_->isolate()->counters()->incremental_marking_sum()->AddSample(
        static_cast<int>(incremental_marking_duration_));
  }
  incremental_marking_bytes_ = 0;
  incremental_marking_duration_ = 0;
  for (int i = 0; i < Scope::NUMBER_OF_INCREMENTAL_SCOPES; i++) {
    incremental_marking_scopes_[i].ResetCurrentCycle();
  }
}

void GCTracer::Stop(GarbageCollector collector) {
  start_counter_--;
  if (start_counter_ != 0) {
    heap_->isolate()->PrintWithTimestamp("[Finished reentrant %s during %s.]\n",
                                         Heap::CollectorName(collector),
                                         current_.TypeName(false));
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
  current_.end_object_size = heap_->SizeOfObjects();
  current_.end_memory_size = heap_->memory_allocator()->Size();
  current_.end_holes_size = CountTotalHolesSize(heap_);
  current_.survived_new_space_object_size = heap_->SurvivedNewSpaceObjectSize();

  AddAllocation(current_.end_time);

  size_t committed_memory = heap_->CommittedMemory() / KB;
  size_t used_memory = current_.end_object_size / KB;
  heap_->isolate()->counters()->aggregated_memory_heap_committed()->AddSample(
      current_.end_time, committed_memory);
  heap_->isolate()->counters()->aggregated_memory_heap_used()->AddSample(
      current_.end_time, used_memory);

  double duration = current_.end_time - current_.start_time;

  switch (current_.type) {
    case Event::SCAVENGER:
    case Event::MINOR_MARK_COMPACTOR:
      recorded_minor_gcs_total_.Push(
          MakeBytesAndDuration(current_.new_space_object_size, duration));
      recorded_minor_gcs_survived_.Push(MakeBytesAndDuration(
          current_.survived_new_space_object_size, duration));
      FetchBackgroundMinorGCCounters();
      break;
    case Event::INCREMENTAL_MARK_COMPACTOR:
      current_.incremental_marking_bytes = incremental_marking_bytes_;
      current_.incremental_marking_duration = incremental_marking_duration_;
      for (int i = 0; i < Scope::NUMBER_OF_INCREMENTAL_SCOPES; i++) {
        current_.incremental_marking_scopes[i] = incremental_marking_scopes_[i];
        current_.scopes[i] = incremental_marking_scopes_[i].duration;
      }
      RecordIncrementalMarkingSpeed(current_.incremental_marking_bytes,
                                    current_.incremental_marking_duration);
      recorded_incremental_mark_compacts_.Push(
          MakeBytesAndDuration(current_.start_object_size, duration));
      ResetIncrementalMarkingCounters();
      combined_mark_compact_speed_cache_ = 0.0;
      FetchBackgroundMarkCompactCounters();
      break;
    case Event::MARK_COMPACTOR:
      DCHECK_EQ(0u, current_.incremental_marking_bytes);
      DCHECK_EQ(0, current_.incremental_marking_duration);
      recorded_mark_compacts_.Push(
          MakeBytesAndDuration(current_.start_object_size, duration));
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
}


void GCTracer::SampleAllocation(double current_ms,
                                size_t new_space_counter_bytes,
                                size_t old_generation_counter_bytes) {
  if (allocation_time_ms_ == 0) {
    // It is the first sample.
    allocation_time_ms_ = current_ms;
    new_space_allocation_counter_bytes_ = new_space_counter_bytes;
    old_generation_allocation_counter_bytes_ = old_generation_counter_bytes;
    return;
  }
  // This assumes that counters are unsigned integers so that the subtraction
  // below works even if the new counter is less than the old counter.
  size_t new_space_allocated_bytes =
      new_space_counter_bytes - new_space_allocation_counter_bytes_;
  size_t old_generation_allocated_bytes =
      old_generation_counter_bytes - old_generation_allocation_counter_bytes_;
  double duration = current_ms - allocation_time_ms_;
  allocation_time_ms_ = current_ms;
  new_space_allocation_counter_bytes_ = new_space_counter_bytes;
  old_generation_allocation_counter_bytes_ = old_generation_counter_bytes;
  allocation_duration_since_gc_ += duration;
  new_space_allocation_in_bytes_since_gc_ += new_space_allocated_bytes;
  old_generation_allocation_in_bytes_since_gc_ +=
      old_generation_allocated_bytes;
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
  }
  allocation_duration_since_gc_ = 0;
  new_space_allocation_in_bytes_since_gc_ = 0;
  old_generation_allocation_in_bytes_since_gc_ = 0;
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

  heap_->AddToRingBuffer(buffer.start());
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
      "%s %.1f (%.1f) -> %.1f (%.1f) MB, "
      "%.1f / %.1f ms %s %s %s\n",
      base::OS::GetCurrentProcessId(),
      reinterpret_cast<void*>(heap_->isolate()),
      heap_->isolate()->time_millis_since_init(), current_.TypeName(false),
      static_cast<double>(current_.start_object_size) / MB,
      static_cast<double>(current_.start_memory_size) / MB,
      static_cast<double>(current_.end_object_size) / MB,
      static_cast<double>(current_.end_memory_size) / MB, duration,
      TotalExternalTime(), incremental_buffer,
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
          "scavenge=%.2f "
          "scavenge.process_array_buffers=%.2f "
          "scavenge.roots=%.2f "
          "scavenge.weak=%.2f "
          "scavenge.weak_global_handles.identify=%.2f "
          "scavenge.weak_global_handles.process=%.2f "
          "scavenge.parallel=%.2f "
          "background.scavenge.parallel=%.2f "
          "background.array_buffer_free=%.2f "
          "background.store_buffer=%.2f "
          "background.unmapper=%.2f "
          "incremental.steps_count=%d "
          "incremental.steps_took=%.1f "
          "scavenge_throughput=%.f "
          "total_size_before=%" PRIuS
          " "
          "total_size_after=%" PRIuS
          " "
          "holes_size_before=%" PRIuS
          " "
          "holes_size_after=%" PRIuS
          " "
          "allocated=%" PRIuS
          " "
          "promoted=%" PRIuS
          " "
          "semi_space_copied=%" PRIuS
          " "
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
          current_.scopes[Scope::SCAVENGER_FAST_PROMOTE],
          current_.scopes[Scope::SCAVENGER_SCAVENGE],
          current_.scopes[Scope::SCAVENGER_PROCESS_ARRAY_BUFFERS],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_ROOTS],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_WEAK],
          current_
              .scopes[Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY],
          current_
              .scopes[Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS],
          current_.scopes[Scope::SCAVENGER_SCAVENGE_PARALLEL],
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
          "clear.weak_cells=%.1f "
          "clear.weak_collections=%.1f "
          "clear.weak_lists=%.1f "
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
          "mark=%.1f "
          "mark.finish_incremental=%.1f "
          "mark.roots=%.1f "
          "mark.main=%.1f "
          "mark.weak_closure=%.1f "
          "mark.weak_closure.ephemeral=%.1f "
          "mark.weak_closure.weak_handles=%.1f "
          "mark.weak_closure.weak_roots=%.1f "
          "mark.weak_closure.harmony=%.1f "
          "mark.wrapper_prologue=%.1f "
          "mark.wrapper_epilogue=%.1f "
          "mark.wrapper_tracing=%.1f "
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
          "incremental.sweeping=%.1f "
          "incremental.wrapper_prologue=%.1f "
          "incremental.wrapper_tracing=%.1f "
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
          "total_size_before=%" PRIuS
          " "
          "total_size_after=%" PRIuS
          " "
          "holes_size_before=%" PRIuS
          " "
          "holes_size_after=%" PRIuS
          " "
          "allocated=%" PRIuS
          " "
          "promoted=%" PRIuS
          " "
          "semi_space_copied=%" PRIuS
          " "
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
          current_.scopes[Scope::MC_CLEAR_WEAK_CELLS],
          current_.scopes[Scope::MC_CLEAR_WEAK_COLLECTIONS],
          current_.scopes[Scope::MC_CLEAR_WEAK_LISTS],
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
          current_.scopes[Scope::MC_FINISH], current_.scopes[Scope::MC_MARK],
          current_.scopes[Scope::MC_MARK_FINISH_INCREMENTAL],
          current_.scopes[Scope::MC_MARK_ROOTS],
          current_.scopes[Scope::MC_MARK_MAIN],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_EPHEMERAL],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_HARMONY],
          current_.scopes[Scope::MC_MARK_WRAPPER_PROLOGUE],
          current_.scopes[Scope::MC_MARK_WRAPPER_EPILOGUE],
          current_.scopes[Scope::MC_MARK_WRAPPER_TRACING],
          current_.scopes[Scope::MC_PROLOGUE], current_.scopes[Scope::MC_SWEEP],
          current_.scopes[Scope::MC_SWEEP_CODE],
          current_.scopes[Scope::MC_SWEEP_MAP],
          current_.scopes[Scope::MC_SWEEP_OLD],
          current_.scopes[Scope::MC_INCREMENTAL],
          current_.scopes[Scope::MC_INCREMENTAL_FINALIZE],
          current_.scopes[Scope::MC_INCREMENTAL_FINALIZE_BODY],
          current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_SWEEPING],
          current_.scopes[Scope::MC_INCREMENTAL_WRAPPER_PROLOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_WRAPPER_TRACING],
          current_
              .incremental_marking_scopes[Scope::MC_INCREMENTAL_WRAPPER_TRACING]
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

double GCTracer::IncrementalMarkingSpeedInBytesPerMillisecond() const {
  const int kConservativeSpeedInBytesPerMillisecond = 128 * KB;
  if (recorded_incremental_marking_speed_ != 0) {
    return recorded_incremental_marking_speed_;
  }
  if (incremental_marking_duration_ != 0.0) {
    return incremental_marking_bytes_ / incremental_marking_duration_;
  }
  return kConservativeSpeedInBytesPerMillisecond;
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
  if (combined_mark_compact_speed_cache_ > 0)
    return combined_mark_compact_speed_cache_;
  const double kMinimumMarkingSpeed = 0.5;
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
  base::LockGuard<base::Mutex> guard(&background_counter_mutex_);
  int background_mc_scopes = last_background_scope - first_background_scope + 1;
  for (int i = 0; i < background_mc_scopes; i++) {
    current_.scopes[first_global_scope + i] +=
        background_counter_[first_background_scope + i].total_duration_ms;
    background_counter_[first_background_scope + i].total_duration_ms = 0;
  }
  if (V8_LIKELY(!FLAG_runtime_stats)) return;
  RuntimeCallStats* runtime_stats =
      heap_->isolate()->counters()->runtime_call_stats();
  if (!runtime_stats) return;
  for (int i = 0; i < background_mc_scopes; i++) {
    runtime_stats
        ->GetCounter(GCTracer::RCSCounterFromScope(
            static_cast<Scope::ScopeId>(first_global_scope + i)))
        ->Add(&background_counter_[first_background_scope + i]
                   .runtime_call_counter);
    background_counter_[first_background_scope + i]
        .runtime_call_counter.Reset();
  }
}

void GCTracer::AddBackgroundScopeSample(
    BackgroundScope::ScopeId scope, double duration,
    RuntimeCallCounter* runtime_call_counter) {
  base::LockGuard<base::Mutex> guard(&background_counter_mutex_);
  BackgroundCounter& counter = background_counter_[scope];
  counter.total_duration_ms += duration;
  if (runtime_call_counter) {
    counter.runtime_call_counter.Add(runtime_call_counter);
  }
}

}  // namespace internal
}  // namespace v8
