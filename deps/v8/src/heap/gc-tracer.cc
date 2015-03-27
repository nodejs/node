// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/heap/gc-tracer.h"

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


GCTracer::AllocationEvent::AllocationEvent(double duration,
                                           intptr_t allocation_in_bytes) {
  duration_ = duration;
  allocation_in_bytes_ = allocation_in_bytes;
}


GCTracer::ContextDisposalEvent::ContextDisposalEvent(double time) {
  time_ = time;
}


GCTracer::SurvivalEvent::SurvivalEvent(double promotion_ratio) {
  promotion_ratio_ = promotion_ratio;
}


GCTracer::Event::Event(Type type, const char* gc_reason,
                       const char* collector_reason)
    : type(type),
      gc_reason(gc_reason),
      collector_reason(collector_reason),
      start_time(0.0),
      end_time(0.0),
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
      cumulative_marking_duration_(0.0),
      cumulative_sweeping_duration_(0.0),
      new_space_top_after_gc_(0),
      start_counter_(0) {
  current_ = Event(Event::START, NULL, NULL);
  current_.end_time = base::OS::TimeCurrentMillis();
  previous_ = previous_incremental_mark_compactor_event_ = current_;
}


void GCTracer::Start(GarbageCollector collector, const char* gc_reason,
                     const char* collector_reason) {
  start_counter_++;
  if (start_counter_ != 1) return;

  previous_ = current_;
  double start_time = base::OS::TimeCurrentMillis();
  if (new_space_top_after_gc_ != 0) {
    AddNewSpaceAllocationTime(
        start_time - previous_.end_time,
        reinterpret_cast<intptr_t>((heap_->new_space()->top()) -
                                   new_space_top_after_gc_));
  }
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

  current_.start_time = start_time;
  current_.start_object_size = heap_->SizeOfObjects();
  current_.start_memory_size = heap_->isolate()->memory_allocator()->Size();
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
}


void GCTracer::Stop(GarbageCollector collector) {
  start_counter_--;
  if (start_counter_ != 0) {
    if (FLAG_trace_gc) {
      PrintF("[Finished reentrant %s during %s.]\n",
             collector == SCAVENGER ? "Scavenge" : "Mark-sweep",
             current_.TypeName(false));
    }
    return;
  }

  DCHECK(start_counter_ >= 0);
  DCHECK((collector == SCAVENGER && current_.type == Event::SCAVENGER) ||
         (collector == MARK_COMPACTOR &&
          (current_.type == Event::MARK_COMPACTOR ||
           current_.type == Event::INCREMENTAL_MARK_COMPACTOR)));

  current_.end_time = base::OS::TimeCurrentMillis();
  current_.end_object_size = heap_->SizeOfObjects();
  current_.end_memory_size = heap_->isolate()->memory_allocator()->Size();
  current_.end_holes_size = CountTotalHolesSize(heap_);
  new_space_top_after_gc_ =
      reinterpret_cast<intptr_t>(heap_->new_space()->top());

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
    scavenger_events_.push_front(current_);
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
    incremental_mark_compactor_events_.push_front(current_);
  } else {
    DCHECK(current_.incremental_marking_bytes == 0);
    DCHECK(current_.incremental_marking_duration == 0);
    DCHECK(current_.pure_incremental_marking_duration == 0);
    DCHECK(longest_incremental_marking_step_ == 0.0);
    mark_compactor_events_.push_front(current_);
  }

  // TODO(ernstm): move the code below out of GCTracer.

  if (!FLAG_trace_gc && !FLAG_print_cumulative_gc_stat) return;

  double duration = current_.end_time - current_.start_time;
  double spent_in_mutator = Max(current_.start_time - previous_.end_time, 0.0);

  heap_->UpdateCumulativeGCStatistics(duration, spent_in_mutator,
                                      current_.scopes[Scope::MC_MARK]);

  if (current_.type == Event::SCAVENGER && FLAG_trace_gc_ignore_scavenger)
    return;

  if (FLAG_trace_gc) {
    if (FLAG_trace_gc_nvp)
      PrintNVP();
    else
      Print();

    heap_->PrintShortHeapStatistics();
  }
}


void GCTracer::AddNewSpaceAllocationTime(double duration,
                                         intptr_t allocation_in_bytes) {
  allocation_events_.push_front(AllocationEvent(duration, allocation_in_bytes));
}


void GCTracer::AddContextDisposalTime(double time) {
  context_disposal_events_.push_front(ContextDisposalEvent(time));
}


void GCTracer::AddSurvivalRatio(double promotion_ratio) {
  survival_events_.push_front(SurvivalEvent(promotion_ratio));
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


void GCTracer::Print() const {
  PrintPID("%8.0f ms: ", heap_->isolate()->time_millis_since_init());

  PrintF("%s %.1f (%.1f) -> %.1f (%.1f) MB, ", current_.TypeName(false),
         static_cast<double>(current_.start_object_size) / MB,
         static_cast<double>(current_.start_memory_size) / MB,
         static_cast<double>(current_.end_object_size) / MB,
         static_cast<double>(current_.end_memory_size) / MB);

  int external_time = static_cast<int>(current_.scopes[Scope::EXTERNAL]);
  if (external_time > 0) PrintF("%d / ", external_time);

  double duration = current_.end_time - current_.start_time;
  PrintF("%.1f ms", duration);
  if (current_.type == Event::SCAVENGER) {
    if (current_.incremental_marking_steps > 0) {
      PrintF(" (+ %.1f ms in %d steps since last GC)",
             current_.incremental_marking_duration,
             current_.incremental_marking_steps);
    }
  } else {
    if (current_.incremental_marking_steps > 0) {
      PrintF(
          " (+ %.1f ms in %d steps since start of marking, "
          "biggest step %.1f ms)",
          current_.incremental_marking_duration,
          current_.incremental_marking_steps,
          current_.longest_incremental_marking_step);
    }
  }

  if (current_.gc_reason != NULL) {
    PrintF(" [%s]", current_.gc_reason);
  }

  if (current_.collector_reason != NULL) {
    PrintF(" [%s]", current_.collector_reason);
  }

  PrintF(".\n");
}


void GCTracer::PrintNVP() const {
  PrintPID("%8.0f ms: ", heap_->isolate()->time_millis_since_init());

  double duration = current_.end_time - current_.start_time;
  double spent_in_mutator = current_.start_time - previous_.end_time;

  PrintF("pause=%.1f ", duration);
  PrintF("mutator=%.1f ", spent_in_mutator);
  PrintF("gc=%s ", current_.TypeName(true));

  PrintF("external=%.1f ", current_.scopes[Scope::EXTERNAL]);
  PrintF("mark=%.1f ", current_.scopes[Scope::MC_MARK]);
  PrintF("sweep=%.2f ", current_.scopes[Scope::MC_SWEEP]);
  PrintF("sweepns=%.2f ", current_.scopes[Scope::MC_SWEEP_NEWSPACE]);
  PrintF("sweepos=%.2f ", current_.scopes[Scope::MC_SWEEP_OLDSPACE]);
  PrintF("sweepcode=%.2f ", current_.scopes[Scope::MC_SWEEP_CODE]);
  PrintF("sweepcell=%.2f ", current_.scopes[Scope::MC_SWEEP_CELL]);
  PrintF("sweepmap=%.2f ", current_.scopes[Scope::MC_SWEEP_MAP]);
  PrintF("evacuate=%.1f ", current_.scopes[Scope::MC_EVACUATE_PAGES]);
  PrintF("new_new=%.1f ",
         current_.scopes[Scope::MC_UPDATE_NEW_TO_NEW_POINTERS]);
  PrintF("root_new=%.1f ",
         current_.scopes[Scope::MC_UPDATE_ROOT_TO_NEW_POINTERS]);
  PrintF("old_new=%.1f ",
         current_.scopes[Scope::MC_UPDATE_OLD_TO_NEW_POINTERS]);
  PrintF("compaction_ptrs=%.1f ",
         current_.scopes[Scope::MC_UPDATE_POINTERS_TO_EVACUATED]);
  PrintF("intracompaction_ptrs=%.1f ",
         current_.scopes[Scope::MC_UPDATE_POINTERS_BETWEEN_EVACUATED]);
  PrintF("misc_compaction=%.1f ",
         current_.scopes[Scope::MC_UPDATE_MISC_POINTERS]);
  PrintF("weak_closure=%.1f ", current_.scopes[Scope::MC_WEAKCLOSURE]);
  PrintF("inc_weak_closure=%.1f ",
         current_.scopes[Scope::MC_INCREMENTAL_WEAKCLOSURE]);
  PrintF("weakcollection_process=%.1f ",
         current_.scopes[Scope::MC_WEAKCOLLECTION_PROCESS]);
  PrintF("weakcollection_clear=%.1f ",
         current_.scopes[Scope::MC_WEAKCOLLECTION_CLEAR]);
  PrintF("weakcollection_abort=%.1f ",
         current_.scopes[Scope::MC_WEAKCOLLECTION_ABORT]);

  PrintF("total_size_before=%" V8_PTR_PREFIX "d ", current_.start_object_size);
  PrintF("total_size_after=%" V8_PTR_PREFIX "d ", current_.end_object_size);
  PrintF("holes_size_before=%" V8_PTR_PREFIX "d ", current_.start_holes_size);
  PrintF("holes_size_after=%" V8_PTR_PREFIX "d ", current_.end_holes_size);

  intptr_t allocated_since_last_gc =
      current_.start_object_size - previous_.end_object_size;
  PrintF("allocated=%" V8_PTR_PREFIX "d ", allocated_since_last_gc);
  PrintF("promoted=%" V8_PTR_PREFIX "d ", heap_->promoted_objects_size_);
  PrintF("semi_space_copied=%" V8_PTR_PREFIX "d ",
         heap_->semi_space_copied_object_size_);
  PrintF("nodes_died_in_new=%d ", heap_->nodes_died_in_new_space_);
  PrintF("nodes_copied_in_new=%d ", heap_->nodes_copied_in_new_space_);
  PrintF("nodes_promoted=%d ", heap_->nodes_promoted_);
  PrintF("promotion_ratio=%.1f%% ", heap_->promotion_ratio_);
  PrintF("average_survival_ratio=%.1f%% ", AverageSurvivalRatio());
  PrintF("promotion_rate=%.1f%% ", heap_->promotion_rate_);
  PrintF("semi_space_copy_rate=%.1f%% ", heap_->semi_space_copied_rate_);
  PrintF("new_space_allocation_throughput=%" V8_PTR_PREFIX "d ",
         NewSpaceAllocationThroughputInBytesPerMillisecond());
  PrintF("context_disposal_rate=%.1f ", ContextDisposalRateInMilliseconds());

  if (current_.type == Event::SCAVENGER) {
    PrintF("steps_count=%d ", current_.incremental_marking_steps);
    PrintF("steps_took=%.1f ", current_.incremental_marking_duration);
    PrintF("scavenge_throughput=%" V8_PTR_PREFIX "d ",
           ScavengeSpeedInBytesPerMillisecond());
  } else {
    PrintF("steps_count=%d ", current_.incremental_marking_steps);
    PrintF("steps_took=%.1f ", current_.incremental_marking_duration);
    PrintF("longest_step=%.1f ", current_.longest_incremental_marking_step);
    PrintF("incremental_marking_throughput=%" V8_PTR_PREFIX "d ",
           IncrementalMarkingSpeedInBytesPerMillisecond());
  }

  PrintF("\n");
}


double GCTracer::MeanDuration(const EventBuffer& events) const {
  if (events.empty()) return 0.0;

  double mean = 0.0;
  EventBuffer::const_iterator iter = events.begin();
  while (iter != events.end()) {
    mean += iter->end_time - iter->start_time;
    ++iter;
  }

  return mean / events.size();
}


double GCTracer::MaxDuration(const EventBuffer& events) const {
  if (events.empty()) return 0.0;

  double maximum = 0.0f;
  EventBuffer::const_iterator iter = events.begin();
  while (iter != events.end()) {
    maximum = Max(iter->end_time - iter->start_time, maximum);
    ++iter;
  }

  return maximum;
}


double GCTracer::MeanIncrementalMarkingDuration() const {
  if (cumulative_incremental_marking_steps_ == 0) return 0.0;

  // We haven't completed an entire round of incremental marking, yet.
  // Use data from GCTracer instead of data from event buffers.
  if (incremental_mark_compactor_events_.empty()) {
    return cumulative_incremental_marking_duration_ /
           cumulative_incremental_marking_steps_;
  }

  int steps = 0;
  double durations = 0.0;
  EventBuffer::const_iterator iter = incremental_mark_compactor_events_.begin();
  while (iter != incremental_mark_compactor_events_.end()) {
    steps += iter->incremental_marking_steps;
    durations += iter->incremental_marking_duration;
    ++iter;
  }

  if (steps == 0) return 0.0;

  return durations / steps;
}


double GCTracer::MaxIncrementalMarkingDuration() const {
  // We haven't completed an entire round of incremental marking, yet.
  // Use data from GCTracer instead of data from event buffers.
  if (incremental_mark_compactor_events_.empty())
    return longest_incremental_marking_step_;

  double max_duration = 0.0;
  EventBuffer::const_iterator iter = incremental_mark_compactor_events_.begin();
  while (iter != incremental_mark_compactor_events_.end())
    max_duration = Max(iter->longest_incremental_marking_step, max_duration);

  return max_duration;
}


intptr_t GCTracer::IncrementalMarkingSpeedInBytesPerMillisecond() const {
  if (cumulative_incremental_marking_duration_ == 0.0) return 0;

  // We haven't completed an entire round of incremental marking, yet.
  // Use data from GCTracer instead of data from event buffers.
  if (incremental_mark_compactor_events_.empty()) {
    return static_cast<intptr_t>(cumulative_incremental_marking_bytes_ /
                                 cumulative_pure_incremental_marking_duration_);
  }

  intptr_t bytes = 0;
  double durations = 0.0;
  EventBuffer::const_iterator iter = incremental_mark_compactor_events_.begin();
  while (iter != incremental_mark_compactor_events_.end()) {
    bytes += iter->incremental_marking_bytes;
    durations += iter->pure_incremental_marking_duration;
    ++iter;
  }

  if (durations == 0.0) return 0;

  return static_cast<intptr_t>(bytes / durations);
}


intptr_t GCTracer::ScavengeSpeedInBytesPerMillisecond() const {
  intptr_t bytes = 0;
  double durations = 0.0;
  EventBuffer::const_iterator iter = scavenger_events_.begin();
  while (iter != scavenger_events_.end()) {
    bytes += iter->new_space_object_size;
    durations += iter->end_time - iter->start_time;
    ++iter;
  }

  if (durations == 0.0) return 0;

  return static_cast<intptr_t>(bytes / durations);
}


intptr_t GCTracer::MarkCompactSpeedInBytesPerMillisecond() const {
  intptr_t bytes = 0;
  double durations = 0.0;
  EventBuffer::const_iterator iter = mark_compactor_events_.begin();
  while (iter != mark_compactor_events_.end()) {
    bytes += iter->start_object_size;
    durations += iter->end_time - iter->start_time;
    ++iter;
  }

  if (durations == 0.0) return 0;

  return static_cast<intptr_t>(bytes / durations);
}


intptr_t GCTracer::FinalIncrementalMarkCompactSpeedInBytesPerMillisecond()
    const {
  intptr_t bytes = 0;
  double durations = 0.0;
  EventBuffer::const_iterator iter = incremental_mark_compactor_events_.begin();
  while (iter != incremental_mark_compactor_events_.end()) {
    bytes += iter->start_object_size;
    durations += iter->end_time - iter->start_time;
    ++iter;
  }

  if (durations == 0.0) return 0;

  return static_cast<intptr_t>(bytes / durations);
}


intptr_t GCTracer::NewSpaceAllocationThroughputInBytesPerMillisecond() const {
  intptr_t bytes = 0;
  double durations = 0.0;
  AllocationEventBuffer::const_iterator iter = allocation_events_.begin();
  while (iter != allocation_events_.end()) {
    bytes += iter->allocation_in_bytes_;
    durations += iter->duration_;
    ++iter;
  }

  if (durations == 0.0) return 0;

  return static_cast<intptr_t>(bytes / durations);
}


double GCTracer::ContextDisposalRateInMilliseconds() const {
  if (context_disposal_events_.size() < kRingBufferMaxSize) return 0.0;

  double begin = base::OS::TimeCurrentMillis();
  double end = 0.0;
  ContextDisposalEventBuffer::const_iterator iter =
      context_disposal_events_.begin();
  while (iter != context_disposal_events_.end()) {
    end = iter->time_;
    ++iter;
  }

  return (begin - end) / context_disposal_events_.size();
}


double GCTracer::AverageSurvivalRatio() const {
  if (survival_events_.size() == 0) return 0.0;

  double sum_of_rates = 0.0;
  SurvivalEventBuffer::const_iterator iter = survival_events_.begin();
  while (iter != survival_events_.end()) {
    sum_of_rates += iter->promotion_ratio_;
    ++iter;
  }

  return sum_of_rates / static_cast<double>(survival_events_.size());
}


bool GCTracer::SurvivalEventsRecorded() const {
  return survival_events_.size() > 0;
}


void GCTracer::ResetSurvivalEvents() { survival_events_.reset(); }
}
}  // namespace v8::internal
