// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-tracer.h"

#include "src/counters.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

static intptr_t CountTotalHolesSize(Heap* heap) {
  intptr_t holes_size = 0;
  OldSpaces spaces(heap);
  for (OldSpace* space = spaces.next(); space != NULL; space = spaces.next()) {
    holes_size += space->Waste() + space->Available();
  }
  return holes_size;
}


GCTracer::Scope::Scope(GCTracer* tracer, ScopeId scope)
    : tracer_(tracer), scope_(scope) {
  start_time_ = tracer_->heap_->MonotonicallyIncreasingTimeInMs();
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (FLAG_runtime_call_stats) {
    RuntimeCallStats::Enter(tracer_->heap_->isolate(), &timer_,
                            &RuntimeCallStats::GC);
  }
}


GCTracer::Scope::~Scope() {
  DCHECK(scope_ < NUMBER_OF_SCOPES);  // scope_ is unsigned.
  tracer_->current_.scopes[scope_] +=
      tracer_->heap_->MonotonicallyIncreasingTimeInMs() - start_time_;
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (FLAG_runtime_call_stats) {
    RuntimeCallStats::Leave(tracer_->heap_->isolate(), &timer_);
  }
}

const char* GCTracer::Scope::Name(ScopeId id) {
#define CASE(scope)  \
  case Scope::scope: \
    return "V8.GC_" #scope;
  switch (id) {
    TRACER_SCOPES(CASE)
    case Scope::NUMBER_OF_SCOPES:
      break;
  }
#undef CASE
  return "(unknown)";
}

GCTracer::Event::Event(Type type, const char* gc_reason,
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
      cumulative_incremental_marking_steps(0),
      incremental_marking_steps(0),
      cumulative_incremental_marking_bytes(0),
      incremental_marking_bytes(0),
      cumulative_incremental_marking_duration(0.0),
      incremental_marking_duration(0.0),
      cumulative_pure_incremental_marking_duration(0.0),
      pure_incremental_marking_duration(0.0),
      longest_incremental_marking_step(0.0) {
  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    scopes[i] = 0;
  }
}


const char* GCTracer::Event::TypeName(bool short_name) const {
  switch (type) {
    case SCAVENGER:
      if (short_name) {
        return "s";
      } else {
        return "Scavenge";
      }
    case MARK_COMPACTOR:
    case INCREMENTAL_MARK_COMPACTOR:
      if (short_name) {
        return "ms";
      } else {
        return "Mark-sweep";
      }
    case START:
      if (short_name) {
        return "st";
      } else {
        return "Start";
      }
  }
  return "Unknown Event Type";
}


GCTracer::GCTracer(Heap* heap)
    : heap_(heap),
      cumulative_incremental_marking_steps_(0),
      cumulative_incremental_marking_bytes_(0),
      cumulative_incremental_marking_duration_(0.0),
      cumulative_pure_incremental_marking_duration_(0.0),
      longest_incremental_marking_step_(0.0),
      cumulative_incremental_marking_finalization_steps_(0),
      cumulative_incremental_marking_finalization_duration_(0.0),
      longest_incremental_marking_finalization_step_(0.0),
      cumulative_marking_duration_(0.0),
      cumulative_sweeping_duration_(0.0),
      allocation_time_ms_(0.0),
      new_space_allocation_counter_bytes_(0),
      old_generation_allocation_counter_bytes_(0),
      allocation_duration_since_gc_(0.0),
      new_space_allocation_in_bytes_since_gc_(0),
      old_generation_allocation_in_bytes_since_gc_(0),
      combined_mark_compact_speed_cache_(0.0),
      start_counter_(0) {
  current_ = Event(Event::START, NULL, NULL);
  current_.end_time = heap_->MonotonicallyIncreasingTimeInMs();
  previous_ = previous_incremental_mark_compactor_event_ = current_;
}


void GCTracer::Start(GarbageCollector collector, const char* gc_reason,
                     const char* collector_reason) {
  start_counter_++;
  if (start_counter_ != 1) return;

  previous_ = current_;
  double start_time = heap_->MonotonicallyIncreasingTimeInMs();
  SampleAllocation(start_time, heap_->NewSpaceAllocationCounter(),
                   heap_->OldGenerationAllocationCounter());
  if (current_.type == Event::INCREMENTAL_MARK_COMPACTOR)
    previous_incremental_mark_compactor_event_ = current_;

  if (collector == SCAVENGER) {
    current_ = Event(Event::SCAVENGER, gc_reason, collector_reason);
  } else if (collector == MARK_COMPACTOR) {
    if (heap_->incremental_marking()->WasActivated()) {
      current_ =
          Event(Event::INCREMENTAL_MARK_COMPACTOR, gc_reason, collector_reason);
    } else {
      current_ = Event(Event::MARK_COMPACTOR, gc_reason, collector_reason);
    }
  }

  current_.reduce_memory = heap_->ShouldReduceMemory();
  current_.start_time = start_time;
  current_.start_object_size = heap_->SizeOfObjects();
  current_.start_memory_size = heap_->memory_allocator()->Size();
  current_.start_holes_size = CountTotalHolesSize(heap_);
  current_.new_space_object_size =
      heap_->new_space()->top() - heap_->new_space()->bottom();

  current_.cumulative_incremental_marking_steps =
      cumulative_incremental_marking_steps_;
  current_.cumulative_incremental_marking_bytes =
      cumulative_incremental_marking_bytes_;
  current_.cumulative_incremental_marking_duration =
      cumulative_incremental_marking_duration_;
  current_.cumulative_pure_incremental_marking_duration =
      cumulative_pure_incremental_marking_duration_;
  current_.longest_incremental_marking_step = longest_incremental_marking_step_;

  for (int i = 0; i < Scope::NUMBER_OF_SCOPES; i++) {
    current_.scopes[i] = 0;
  }
  int committed_memory = static_cast<int>(heap_->CommittedMemory() / KB);
  int used_memory = static_cast<int>(current_.start_object_size / KB);
  heap_->isolate()->counters()->aggregated_memory_heap_committed()->AddSample(
      start_time, committed_memory);
  heap_->isolate()->counters()->aggregated_memory_heap_used()->AddSample(
      start_time, used_memory);
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (FLAG_runtime_call_stats) {
    RuntimeCallStats::Enter(heap_->isolate(), &timer_, &RuntimeCallStats::GC);
  }
}

void GCTracer::Stop(GarbageCollector collector) {
  start_counter_--;
  if (start_counter_ != 0) {
    Output("[Finished reentrant %s during %s.]\n",
           collector == SCAVENGER ? "Scavenge" : "Mark-sweep",
           current_.TypeName(false));
    return;
  }

  DCHECK(start_counter_ >= 0);
  DCHECK((collector == SCAVENGER && current_.type == Event::SCAVENGER) ||
         (collector == MARK_COMPACTOR &&
          (current_.type == Event::MARK_COMPACTOR ||
           current_.type == Event::INCREMENTAL_MARK_COMPACTOR)));

  current_.end_time = heap_->MonotonicallyIncreasingTimeInMs();
  current_.end_object_size = heap_->SizeOfObjects();
  current_.end_memory_size = heap_->memory_allocator()->Size();
  current_.end_holes_size = CountTotalHolesSize(heap_);
  current_.survived_new_space_object_size = heap_->SurvivedNewSpaceObjectSize();

  AddAllocation(current_.end_time);

  int committed_memory = static_cast<int>(heap_->CommittedMemory() / KB);
  int used_memory = static_cast<int>(current_.end_object_size / KB);
  heap_->isolate()->counters()->aggregated_memory_heap_committed()->AddSample(
      current_.end_time, committed_memory);
  heap_->isolate()->counters()->aggregated_memory_heap_used()->AddSample(
      current_.end_time, used_memory);

  double duration = current_.end_time - current_.start_time;
  if (current_.type == Event::SCAVENGER) {
    current_.incremental_marking_steps =
        current_.cumulative_incremental_marking_steps -
        previous_.cumulative_incremental_marking_steps;
    current_.incremental_marking_bytes =
        current_.cumulative_incremental_marking_bytes -
        previous_.cumulative_incremental_marking_bytes;
    current_.incremental_marking_duration =
        current_.cumulative_incremental_marking_duration -
        previous_.cumulative_incremental_marking_duration;
    current_.pure_incremental_marking_duration =
        current_.cumulative_pure_incremental_marking_duration -
        previous_.cumulative_pure_incremental_marking_duration;
    recorded_scavenges_total_.Push(
        MakeBytesAndDuration(current_.new_space_object_size, duration));
    recorded_scavenges_survived_.Push(MakeBytesAndDuration(
        current_.survived_new_space_object_size, duration));
  } else if (current_.type == Event::INCREMENTAL_MARK_COMPACTOR) {
    current_.incremental_marking_steps =
        current_.cumulative_incremental_marking_steps -
        previous_incremental_mark_compactor_event_
            .cumulative_incremental_marking_steps;
    current_.incremental_marking_bytes =
        current_.cumulative_incremental_marking_bytes -
        previous_incremental_mark_compactor_event_
            .cumulative_incremental_marking_bytes;
    current_.incremental_marking_duration =
        current_.cumulative_incremental_marking_duration -
        previous_incremental_mark_compactor_event_
            .cumulative_incremental_marking_duration;
    current_.pure_incremental_marking_duration =
        current_.cumulative_pure_incremental_marking_duration -
        previous_incremental_mark_compactor_event_
            .cumulative_pure_incremental_marking_duration;
    longest_incremental_marking_step_ = 0.0;
    recorded_incremental_marking_steps_.Push(
        MakeBytesAndDuration(current_.incremental_marking_bytes,
                             current_.pure_incremental_marking_duration));
    recorded_incremental_mark_compacts_.Push(
        MakeBytesAndDuration(current_.start_object_size, duration));
    combined_mark_compact_speed_cache_ = 0.0;
  } else {
    DCHECK(current_.incremental_marking_bytes == 0);
    DCHECK(current_.incremental_marking_duration == 0);
    DCHECK(current_.pure_incremental_marking_duration == 0);
    longest_incremental_marking_step_ = 0.0;
    recorded_mark_compacts_.Push(
        MakeBytesAndDuration(current_.start_object_size, duration));
    combined_mark_compact_speed_cache_ = 0.0;
  }

  // TODO(ernstm): move the code below out of GCTracer.

  double spent_in_mutator = Max(current_.start_time - previous_.end_time, 0.0);

  heap_->UpdateCumulativeGCStatistics(duration, spent_in_mutator,
                                      current_.scopes[Scope::MC_MARK]);

  if (current_.type == Event::SCAVENGER && FLAG_trace_gc_ignore_scavenger)
    return;

  if (FLAG_trace_gc_nvp)
    PrintNVP();
  else
    Print();

  if (FLAG_trace_gc) {
    heap_->PrintShortHeapStatistics();
  }

  longest_incremental_marking_finalization_step_ = 0.0;
  cumulative_incremental_marking_finalization_steps_ = 0;
  cumulative_incremental_marking_finalization_duration_ = 0.0;
  // TODO(cbruni): remove once we fully moved to a trace-based system.
  if (FLAG_runtime_call_stats) {
    RuntimeCallStats::Leave(heap_->isolate(), &timer_);
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
  // below works even if the new counter is less then the old counter.
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
                                  intptr_t live_bytes_compacted) {
  recorded_compactions_.Push(
      MakeBytesAndDuration(live_bytes_compacted, duration));
}


void GCTracer::AddSurvivalRatio(double promotion_ratio) {
  recorded_survival_ratios_.Push(promotion_ratio);
}


void GCTracer::AddIncrementalMarkingStep(double duration, intptr_t bytes) {
  cumulative_incremental_marking_steps_++;
  cumulative_incremental_marking_bytes_ += bytes;
  cumulative_incremental_marking_duration_ += duration;
  longest_incremental_marking_step_ =
      Max(longest_incremental_marking_step_, duration);
  cumulative_marking_duration_ += duration;
  if (bytes > 0) {
    cumulative_pure_incremental_marking_duration_ += duration;
  }
}


void GCTracer::AddIncrementalMarkingFinalizationStep(double duration) {
  cumulative_incremental_marking_finalization_steps_++;
  cumulative_incremental_marking_finalization_duration_ += duration;
  longest_incremental_marking_finalization_step_ =
      Max(longest_incremental_marking_finalization_step_, duration);
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
  if (FLAG_trace_gc) {
    PrintIsolate(heap_->isolate(), "%s", "");
  }
  Output("%8.0f ms: ", heap_->isolate()->time_millis_since_init());

  Output("%s %.1f (%.1f) -> %.1f (%.1f) MB, ", current_.TypeName(false),
         static_cast<double>(current_.start_object_size) / MB,
         static_cast<double>(current_.start_memory_size) / MB,
         static_cast<double>(current_.end_object_size) / MB,
         static_cast<double>(current_.end_memory_size) / MB);

  double duration = current_.end_time - current_.start_time;
  Output("%.1f / %.1f ms", duration, TotalExternalTime());

  if (current_.type == Event::SCAVENGER) {
    if (current_.incremental_marking_steps > 0) {
      Output(" (+ %.1f ms in %d steps since last GC)",
             current_.incremental_marking_duration,
             current_.incremental_marking_steps);
    }
  } else {
    if (current_.incremental_marking_steps > 0) {
      Output(
          " (+ %.1f ms in %d steps since start of marking, "
          "biggest step %.1f ms)",
          current_.incremental_marking_duration,
          current_.incremental_marking_steps,
          current_.longest_incremental_marking_step);
    }
  }

  if (current_.gc_reason != NULL) {
    Output(" [%s]", current_.gc_reason);
  }

  if (current_.collector_reason != NULL) {
    Output(" [%s]", current_.collector_reason);
  }

  Output(".\n");
}


void GCTracer::PrintNVP() const {
  double duration = current_.end_time - current_.start_time;
  double spent_in_mutator = current_.start_time - previous_.end_time;
  intptr_t allocated_since_last_gc =
      current_.start_object_size - previous_.end_object_size;

  switch (current_.type) {
    case Event::SCAVENGER:
      PrintIsolate(heap_->isolate(),
                   "%8.0f ms: "
                   "pause=%.1f "
                   "mutator=%.1f "
                   "gc=%s "
                   "reduce_memory=%d "
                   "scavenge=%.2f "
                   "old_new=%.2f "
                   "weak=%.2f "
                   "roots=%.2f "
                   "code=%.2f "
                   "semispace=%.2f "
                   "object_groups=%.2f "
                   "external_prologue=%.2f "
                   "external_epilogue=%.2f "
                   "external_weak_global_handles=%.2f "
                   "steps_count=%d "
                   "steps_took=%.1f "
                   "scavenge_throughput=%.f "
                   "total_size_before=%" V8PRIdPTR
                   " "
                   "total_size_after=%" V8PRIdPTR
                   " "
                   "holes_size_before=%" V8PRIdPTR
                   " "
                   "holes_size_after=%" V8PRIdPTR
                   " "
                   "allocated=%" V8PRIdPTR
                   " "
                   "promoted=%" V8PRIdPTR
                   " "
                   "semi_space_copied=%" V8PRIdPTR
                   " "
                   "nodes_died_in_new=%d "
                   "nodes_copied_in_new=%d "
                   "nodes_promoted=%d "
                   "promotion_ratio=%.1f%% "
                   "average_survival_ratio=%.1f%% "
                   "promotion_rate=%.1f%% "
                   "semi_space_copy_rate=%.1f%% "
                   "new_space_allocation_throughput=%.1f "
                   "context_disposal_rate=%.1f\n",
                   heap_->isolate()->time_millis_since_init(), duration,
                   spent_in_mutator, current_.TypeName(true),
                   current_.reduce_memory,
                   current_.scopes[Scope::SCAVENGER_SCAVENGE],
                   current_.scopes[Scope::SCAVENGER_OLD_TO_NEW_POINTERS],
                   current_.scopes[Scope::SCAVENGER_WEAK],
                   current_.scopes[Scope::SCAVENGER_ROOTS],
                   current_.scopes[Scope::SCAVENGER_CODE_FLUSH_CANDIDATES],
                   current_.scopes[Scope::SCAVENGER_SEMISPACE],
                   current_.scopes[Scope::SCAVENGER_OBJECT_GROUPS],
                   current_.scopes[Scope::SCAVENGER_EXTERNAL_PROLOGUE],
                   current_.scopes[Scope::SCAVENGER_EXTERNAL_EPILOGUE],
                   current_.scopes[Scope::EXTERNAL_WEAK_GLOBAL_HANDLES],
                   current_.incremental_marking_steps,
                   current_.incremental_marking_duration,
                   ScavengeSpeedInBytesPerMillisecond(),
                   current_.start_object_size, current_.end_object_size,
                   current_.start_holes_size, current_.end_holes_size,
                   allocated_since_last_gc, heap_->promoted_objects_size(),
                   heap_->semi_space_copied_object_size(),
                   heap_->nodes_died_in_new_space_,
                   heap_->nodes_copied_in_new_space_, heap_->nodes_promoted_,
                   heap_->promotion_ratio_, AverageSurvivalRatio(),
                   heap_->promotion_rate_, heap_->semi_space_copied_rate_,
                   NewSpaceAllocationThroughputInBytesPerMillisecond(),
                   ContextDisposalRateInMilliseconds());
      break;
    case Event::MARK_COMPACTOR:
    case Event::INCREMENTAL_MARK_COMPACTOR:
      PrintIsolate(
          heap_->isolate(),
          "%8.0f ms: "
          "pause=%.1f "
          "mutator=%.1f "
          "gc=%s "
          "reduce_memory=%d "
          "clear=%1.f "
          "clear.code_flush=%.1f "
          "clear.dependent_code=%.1f "
          "clear.global_handles=%.1f "
          "clear.maps=%.1f "
          "clear.slots_buffer=%.1f "
          "clear.store_buffer=%.1f "
          "clear.string_table=%.1f "
          "clear.weak_cells=%.1f "
          "clear.weak_collections=%.1f "
          "clear.weak_lists=%.1f "
          "evacuate=%.1f "
          "evacuate.candidates=%.1f "
          "evacuate.clean_up=%.1f "
          "evacuate.copy=%.1f "
          "evacuate.update_pointers=%.1f "
          "evacuate.update_pointers.to_evacuated=%.1f "
          "evacuate.update_pointers.to_new=%.1f "
          "evacuate.update_pointers.weak=%.1f "
          "external.mc_prologue=%.1f "
          "external.mc_epilogue=%.1f "
          "external.mc_incremental_prologue=%.1f "
          "external.mc_incremental_epilogue=%.1f "
          "external.weak_global_handles=%.1f "
          "finish=%.1f "
          "mark=%.1f "
          "mark.finish_incremental=%.1f "
          "mark.prepare_code_flush=%.1f "
          "mark.roots=%.1f "
          "mark.weak_closure=%.1f "
          "mark.weak_closure.ephemeral=%.1f "
          "mark.weak_closure.weak_handles=%.1f "
          "mark.weak_closure.weak_roots=%.1f "
          "mark.weak_closure.harmony=%.1f "
          "sweep=%.1f "
          "sweep.code=%.1f "
          "sweep.map=%.1f "
          "sweep.old=%.1f "
          "incremental_finalize=%.1f "
          "steps_count=%d "
          "steps_took=%.1f "
          "longest_step=%.1f "
          "finalization_steps_count=%d "
          "finalization_steps_took=%.1f "
          "finalization_longest_step=%.1f "
          "incremental_marking_throughput=%.f "
          "total_size_before=%" V8PRIdPTR
          " "
          "total_size_after=%" V8PRIdPTR
          " "
          "holes_size_before=%" V8PRIdPTR
          " "
          "holes_size_after=%" V8PRIdPTR
          " "
          "allocated=%" V8PRIdPTR
          " "
          "promoted=%" V8PRIdPTR
          " "
          "semi_space_copied=%" V8PRIdPTR
          " "
          "nodes_died_in_new=%d "
          "nodes_copied_in_new=%d "
          "nodes_promoted=%d "
          "promotion_ratio=%.1f%% "
          "average_survival_ratio=%.1f%% "
          "promotion_rate=%.1f%% "
          "semi_space_copy_rate=%.1f%% "
          "new_space_allocation_throughput=%.1f "
          "context_disposal_rate=%.1f "
          "compaction_speed=%.f\n",
          heap_->isolate()->time_millis_since_init(), duration,
          spent_in_mutator, current_.TypeName(true), current_.reduce_memory,
          current_.scopes[Scope::MC_CLEAR],
          current_.scopes[Scope::MC_CLEAR_CODE_FLUSH],
          current_.scopes[Scope::MC_CLEAR_DEPENDENT_CODE],
          current_.scopes[Scope::MC_CLEAR_GLOBAL_HANDLES],
          current_.scopes[Scope::MC_CLEAR_MAPS],
          current_.scopes[Scope::MC_CLEAR_SLOTS_BUFFER],
          current_.scopes[Scope::MC_CLEAR_STORE_BUFFER],
          current_.scopes[Scope::MC_CLEAR_STRING_TABLE],
          current_.scopes[Scope::MC_CLEAR_WEAK_CELLS],
          current_.scopes[Scope::MC_CLEAR_WEAK_COLLECTIONS],
          current_.scopes[Scope::MC_CLEAR_WEAK_LISTS],
          current_.scopes[Scope::MC_EVACUATE],
          current_.scopes[Scope::MC_EVACUATE_CANDIDATES],
          current_.scopes[Scope::MC_EVACUATE_CLEAN_UP],
          current_.scopes[Scope::MC_EVACUATE_COPY],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_TO_EVACUATED],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_TO_NEW],
          current_.scopes[Scope::MC_EVACUATE_UPDATE_POINTERS_WEAK],
          current_.scopes[Scope::MC_EXTERNAL_PROLOGUE],
          current_.scopes[Scope::MC_EXTERNAL_EPILOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE],
          current_.scopes[Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE],
          current_.scopes[Scope::EXTERNAL_WEAK_GLOBAL_HANDLES],
          current_.scopes[Scope::MC_FINISH], current_.scopes[Scope::MC_MARK],
          current_.scopes[Scope::MC_MARK_FINISH_INCREMENTAL],
          current_.scopes[Scope::MC_MARK_PREPARE_CODE_FLUSH],
          current_.scopes[Scope::MC_MARK_ROOTS],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_EPHEMERAL],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_WEAK_HANDLES],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_WEAK_ROOTS],
          current_.scopes[Scope::MC_MARK_WEAK_CLOSURE_HARMONY],
          current_.scopes[Scope::MC_SWEEP],
          current_.scopes[Scope::MC_SWEEP_CODE],
          current_.scopes[Scope::MC_SWEEP_MAP],
          current_.scopes[Scope::MC_SWEEP_OLD],
          current_.scopes[Scope::MC_INCREMENTAL_FINALIZE],
          current_.incremental_marking_steps,
          current_.incremental_marking_duration,
          current_.longest_incremental_marking_step,
          cumulative_incremental_marking_finalization_steps_,
          cumulative_incremental_marking_finalization_duration_,
          longest_incremental_marking_finalization_step_,
          IncrementalMarkingSpeedInBytesPerMillisecond(),
          current_.start_object_size, current_.end_object_size,
          current_.start_holes_size, current_.end_holes_size,
          allocated_since_last_gc, heap_->promoted_objects_size(),
          heap_->semi_space_copied_object_size(),
          heap_->nodes_died_in_new_space_, heap_->nodes_copied_in_new_space_,
          heap_->nodes_promoted_, heap_->promotion_ratio_,
          AverageSurvivalRatio(), heap_->promotion_rate_,
          heap_->semi_space_copied_rate_,
          NewSpaceAllocationThroughputInBytesPerMillisecond(),
          ContextDisposalRateInMilliseconds(),
          CompactionSpeedInBytesPerMillisecond());
      break;
    case Event::START:
      break;
    default:
      UNREACHABLE();
  }
}

double GCTracer::AverageSpeed(const RingBuffer<BytesAndDuration>& buffer,
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

double GCTracer::AverageSpeed(const RingBuffer<BytesAndDuration>& buffer) {
  return AverageSpeed(buffer, MakeBytesAndDuration(0, 0), 0);
}

double GCTracer::IncrementalMarkingSpeedInBytesPerMillisecond() const {
  if (cumulative_incremental_marking_duration_ == 0.0) return 0;
  // We haven't completed an entire round of incremental marking, yet.
  // Use data from GCTracer instead of data from event buffers.
  if (recorded_incremental_marking_steps_.Count() == 0) {
    return cumulative_incremental_marking_bytes_ /
           cumulative_pure_incremental_marking_duration_;
  }
  return AverageSpeed(recorded_incremental_marking_steps_);
}

double GCTracer::ScavengeSpeedInBytesPerMillisecond(
    ScavengeSpeedMode mode) const {
  if (mode == kForAllObjects) {
    return AverageSpeed(recorded_scavenges_total_);
  } else {
    return AverageSpeed(recorded_scavenges_survived_);
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
}  // namespace internal
}  // namespace v8
