// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/flags.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

const double GCIdleTimeHandler::kConservativeTimeRatio = 0.9;
const size_t GCIdleTimeHandler::kMaxMarkCompactTimeInMs = 1000;
const size_t GCIdleTimeHandler::kMaxFinalIncrementalMarkCompactTimeInMs = 1000;
const double GCIdleTimeHandler::kHighContextDisposalRate = 100;
const size_t GCIdleTimeHandler::kMinTimeForOverApproximatingWeakClosureInMs = 1;


void GCIdleTimeAction::Print() {
  switch (type) {
    case DONE:
      PrintF("done");
      break;
    case DO_NOTHING:
      PrintF("no action");
      break;
    case DO_INCREMENTAL_MARKING:
      PrintF("incremental marking with step %" V8_PTR_PREFIX "d / ms",
             parameter);
      if (additional_work) {
        PrintF("; finalized marking");
      }
      break;
    case DO_SCAVENGE:
      PrintF("scavenge");
      break;
    case DO_FULL_GC:
      PrintF("full GC");
      break;
    case DO_FINALIZE_SWEEPING:
      PrintF("finalize sweeping");
      break;
  }
}


void GCIdleTimeHandler::HeapState::Print() {
  PrintF("contexts_disposed=%d ", contexts_disposed);
  PrintF("contexts_disposal_rate=%f ", contexts_disposal_rate);
  PrintF("size_of_objects=%" V8_PTR_PREFIX "d ", size_of_objects);
  PrintF("incremental_marking_stopped=%d ", incremental_marking_stopped);
  PrintF("can_start_incremental_marking=%d ", can_start_incremental_marking);
  PrintF("sweeping_in_progress=%d ", sweeping_in_progress);
  PrintF("has_low_allocation_rate=%d", has_low_allocation_rate);
  PrintF("mark_compact_speed=%" V8_PTR_PREFIX "d ",
         mark_compact_speed_in_bytes_per_ms);
  PrintF("incremental_marking_speed=%" V8_PTR_PREFIX "d ",
         incremental_marking_speed_in_bytes_per_ms);
  PrintF("scavenge_speed=%" V8_PTR_PREFIX "d ", scavenge_speed_in_bytes_per_ms);
  PrintF("new_space_size=%" V8_PTR_PREFIX "d ", used_new_space_size);
  PrintF("new_space_capacity=%" V8_PTR_PREFIX "d ", new_space_capacity);
  PrintF("new_space_allocation_throughput=%" V8_PTR_PREFIX "d ",
         new_space_allocation_throughput_in_bytes_per_ms);
}


size_t GCIdleTimeHandler::EstimateMarkingStepSize(
    size_t idle_time_in_ms, size_t marking_speed_in_bytes_per_ms) {
  DCHECK(idle_time_in_ms > 0);

  if (marking_speed_in_bytes_per_ms == 0) {
    marking_speed_in_bytes_per_ms = kInitialConservativeMarkingSpeed;
  }

  size_t marking_step_size = marking_speed_in_bytes_per_ms * idle_time_in_ms;
  if (marking_step_size / marking_speed_in_bytes_per_ms != idle_time_in_ms) {
    // In the case of an overflow we return maximum marking step size.
    return kMaximumMarkingStepSize;
  }

  if (marking_step_size > kMaximumMarkingStepSize)
    return kMaximumMarkingStepSize;

  return static_cast<size_t>(marking_step_size * kConservativeTimeRatio);
}


size_t GCIdleTimeHandler::EstimateMarkCompactTime(
    size_t size_of_objects, size_t mark_compact_speed_in_bytes_per_ms) {
  // TODO(hpayer): Be more precise about the type of mark-compact event. It
  // makes a huge difference if compaction is happening.
  if (mark_compact_speed_in_bytes_per_ms == 0) {
    mark_compact_speed_in_bytes_per_ms = kInitialConservativeMarkCompactSpeed;
  }
  size_t result = size_of_objects / mark_compact_speed_in_bytes_per_ms;
  return Min(result, kMaxMarkCompactTimeInMs);
}


size_t GCIdleTimeHandler::EstimateFinalIncrementalMarkCompactTime(
    size_t size_of_objects,
    size_t final_incremental_mark_compact_speed_in_bytes_per_ms) {
  if (final_incremental_mark_compact_speed_in_bytes_per_ms == 0) {
    final_incremental_mark_compact_speed_in_bytes_per_ms =
        kInitialConservativeFinalIncrementalMarkCompactSpeed;
  }
  size_t result =
      size_of_objects / final_incremental_mark_compact_speed_in_bytes_per_ms;
  return Min(result, kMaxFinalIncrementalMarkCompactTimeInMs);
}


bool GCIdleTimeHandler::ShouldDoScavenge(
    size_t idle_time_in_ms, size_t new_space_size, size_t used_new_space_size,
    size_t scavenge_speed_in_bytes_per_ms,
    size_t new_space_allocation_throughput_in_bytes_per_ms) {
  size_t new_space_allocation_limit =
      kMaxScheduledIdleTime * scavenge_speed_in_bytes_per_ms;

  // If the limit is larger than the new space size, then scavenging used to be
  // really fast. We can take advantage of the whole new space.
  if (new_space_allocation_limit > new_space_size) {
    new_space_allocation_limit = new_space_size;
  }

  // We do not know the allocation throughput before the first scavenge.
  // TODO(hpayer): Estimate allocation throughput before the first scavenge.
  if (new_space_allocation_throughput_in_bytes_per_ms == 0) {
    new_space_allocation_limit =
        static_cast<size_t>(new_space_size * kConservativeTimeRatio);
  } else {
    // We have to trigger scavenge before we reach the end of new space.
    size_t adjust_limit = new_space_allocation_throughput_in_bytes_per_ms *
                          kTimeUntilNextIdleEvent;
    if (adjust_limit > new_space_allocation_limit) {
      new_space_allocation_limit = 0;
    } else {
      new_space_allocation_limit -= adjust_limit;
    }
  }

  // The allocated new space limit to trigger a scavange has to be at least
  // kMinimumNewSpaceSizeToPerformScavenge.
  if (new_space_allocation_limit < kMinimumNewSpaceSizeToPerformScavenge) {
    new_space_allocation_limit = kMinimumNewSpaceSizeToPerformScavenge;
  }

  if (scavenge_speed_in_bytes_per_ms == 0) {
    scavenge_speed_in_bytes_per_ms = kInitialConservativeScavengeSpeed;
  }

  if (new_space_allocation_limit <= used_new_space_size) {
    if (used_new_space_size / scavenge_speed_in_bytes_per_ms <=
        idle_time_in_ms) {
      return true;
    }
  }
  return false;
}


bool GCIdleTimeHandler::ShouldDoMarkCompact(
    size_t idle_time_in_ms, size_t size_of_objects,
    size_t mark_compact_speed_in_bytes_per_ms) {
  return idle_time_in_ms >= kMaxScheduledIdleTime &&
         idle_time_in_ms >=
             EstimateMarkCompactTime(size_of_objects,
                                     mark_compact_speed_in_bytes_per_ms);
}


bool GCIdleTimeHandler::ShouldDoContextDisposalMarkCompact(
    int contexts_disposed, double contexts_disposal_rate) {
  return contexts_disposed > 0 && contexts_disposal_rate > 0 &&
         contexts_disposal_rate < kHighContextDisposalRate;
}


bool GCIdleTimeHandler::ShouldDoFinalIncrementalMarkCompact(
    size_t idle_time_in_ms, size_t size_of_objects,
    size_t final_incremental_mark_compact_speed_in_bytes_per_ms) {
  return idle_time_in_ms >=
         EstimateFinalIncrementalMarkCompactTime(
             size_of_objects,
             final_incremental_mark_compact_speed_in_bytes_per_ms);
}


bool GCIdleTimeHandler::ShouldDoOverApproximateWeakClosure(
    size_t idle_time_in_ms) {
  // TODO(jochen): Estimate the time it will take to build the object groups.
  return idle_time_in_ms >= kMinTimeForOverApproximatingWeakClosureInMs;
}


GCIdleTimeAction GCIdleTimeHandler::NothingOrDone() {
  if (idle_times_which_made_no_progress_per_mode_ >=
      kMaxNoProgressIdleTimesPerMode) {
    return GCIdleTimeAction::Done();
  } else {
    idle_times_which_made_no_progress_per_mode_++;
    return GCIdleTimeAction::Nothing();
  }
}


// The idle time handler has three modes and transitions between them
// as shown in the diagram:
//
//  kReduceLatency -----> kReduceMemory -----> kDone
//      ^    ^                  |                |
//      |    |                  |                |
//      |    +------------------+                |
//      |                                        |
//      +----------------------------------------+
//
// In kReduceLatency mode the handler only starts incremental marking
// if can_start_incremental_marking is false.
// In kReduceMemory mode the handler can force a new GC cycle by starting
// incremental marking even if can_start_incremental_marking is false. It can
// cause at most X idle GCs.
// In kDone mode the idle time handler does nothing.
//
// The initial mode is kReduceLatency.
//
// kReduceLatency => kReduceMemory transition happens if there were Y
// consecutive long idle notifications without any mutator GC. This is our
// notion of "mutator is idle".
//
// kReduceMemory => kDone transition happens after X idle GCs.
//
// kReduceMemory => kReduceLatency transition happens if N mutator GCs
// were performed meaning that the mutator is active.
//
// kDone => kReduceLatency transition happens if there were M mutator GCs or
// context was disposed.
//
// X = kMaxIdleMarkCompacts
// Y = kLongIdleNotificationsBeforeMutatorIsIdle
// N = #(idle GCs)
// M = kGCsBeforeMutatorIsActive
GCIdleTimeAction GCIdleTimeHandler::Compute(double idle_time_in_ms,
                                            HeapState heap_state) {
  Mode next_mode = NextMode(heap_state);

  if (next_mode != mode_) {
    mode_ = next_mode;
    ResetCounters();
  }

  UpdateCounters(idle_time_in_ms);

  if (mode_ == kDone) {
    return GCIdleTimeAction::Done();
  } else {
    return Action(idle_time_in_ms, heap_state, mode_ == kReduceMemory);
  }
}


// The following logic is implemented by the controller:
// (1) If we don't have any idle time, do nothing, unless a context was
// disposed, incremental marking is stopped, and the heap is small. Then do
// a full GC.
// (2) If the context disposal rate is high and we cannot perform a full GC,
// we do nothing until the context disposal rate becomes lower.
// (3) If the new space is almost full and we can affort a scavenge or if the
// next scavenge will very likely take long, then a scavenge is performed.
// (4) If there is currently no MarkCompact idle round going on, we start a
// new idle round if enough garbage was created. Otherwise we do not perform
// garbage collection to keep system utilization low.
// (5) If incremental marking is done, we perform a full garbage collection
// if  we are allowed to still do full garbage collections during this idle
// round or if we are not allowed to start incremental marking. Otherwise we
// do not perform garbage collection to keep system utilization low.
// (6) If sweeping is in progress and we received a large enough idle time
// request, we finalize sweeping here.
// (7) If incremental marking is in progress, we perform a marking step. Note,
// that this currently may trigger a full garbage collection.
GCIdleTimeAction GCIdleTimeHandler::Action(double idle_time_in_ms,
                                           const HeapState& heap_state,
                                           bool reduce_memory) {
  if (static_cast<int>(idle_time_in_ms) <= 0) {
    if (heap_state.incremental_marking_stopped) {
      if (ShouldDoContextDisposalMarkCompact(
              heap_state.contexts_disposed,
              heap_state.contexts_disposal_rate)) {
        return GCIdleTimeAction::FullGC(false);
      }
    }
    return GCIdleTimeAction::Nothing();
  }

  // We are in a context disposal GC scenario. Don't do anything if we do not
  // get the right idle signal.
  if (ShouldDoContextDisposalMarkCompact(heap_state.contexts_disposed,
                                         heap_state.contexts_disposal_rate)) {
    return NothingOrDone();
  }

  if (ShouldDoScavenge(
          static_cast<size_t>(idle_time_in_ms), heap_state.new_space_capacity,
          heap_state.used_new_space_size,
          heap_state.scavenge_speed_in_bytes_per_ms,
          heap_state.new_space_allocation_throughput_in_bytes_per_ms)) {
    return GCIdleTimeAction::Scavenge();
  }

  if (heap_state.incremental_marking_stopped && reduce_memory) {
    if (ShouldDoMarkCompact(static_cast<size_t>(idle_time_in_ms),
                            heap_state.size_of_objects,
                            heap_state.mark_compact_speed_in_bytes_per_ms)) {
      return GCIdleTimeAction::FullGC(reduce_memory);
    }
  }

  if (heap_state.sweeping_in_progress) {
    if (heap_state.sweeping_completed) {
      return GCIdleTimeAction::FinalizeSweeping();
    } else {
      return NothingOrDone();
    }
  }

  if (!FLAG_incremental_marking ||
      (heap_state.incremental_marking_stopped &&
       !heap_state.can_start_incremental_marking && !reduce_memory)) {
    return NothingOrDone();
  }

  size_t step_size = EstimateMarkingStepSize(
      static_cast<size_t>(kIncrementalMarkingStepTimeInMs),
      heap_state.incremental_marking_speed_in_bytes_per_ms);
  return GCIdleTimeAction::IncrementalMarking(step_size, reduce_memory);
}


void GCIdleTimeHandler::UpdateCounters(double idle_time_in_ms) {
  if (mode_ == kReduceLatency) {
    int gcs = scavenges_ + mark_compacts_;
    if (gcs > 0) {
      // There was a GC since the last notification.
      long_idle_notifications_ = 0;
      background_idle_notifications_ = 0;
    }
    idle_mark_compacts_ = 0;
    mark_compacts_ = 0;
    scavenges_ = 0;
    if (idle_time_in_ms >= kMinBackgroundIdleTime) {
      background_idle_notifications_++;
    } else if (idle_time_in_ms >= kMinLongIdleTime) {
      long_idle_notifications_++;
    }
  }
}


void GCIdleTimeHandler::ResetCounters() {
  long_idle_notifications_ = 0;
  background_idle_notifications_ = 0;
  idle_mark_compacts_ = 0;
  mark_compacts_ = 0;
  scavenges_ = 0;
  idle_times_which_made_no_progress_per_mode_ = 0;
}


bool GCIdleTimeHandler::IsMutatorActive(int contexts_disposed,
                                        int mark_compacts) {
  return contexts_disposed > 0 ||
         mark_compacts >= kMarkCompactsBeforeMutatorIsActive;
}


bool GCIdleTimeHandler::IsMutatorIdle(int long_idle_notifications,
                                      int background_idle_notifications,
                                      int mutator_gcs) {
  return mutator_gcs == 0 &&
         (long_idle_notifications >=
              kLongIdleNotificationsBeforeMutatorIsIdle ||
          background_idle_notifications >=
              kBackgroundIdleNotificationsBeforeMutatorIsIdle);
}


GCIdleTimeHandler::Mode GCIdleTimeHandler::NextMode(
    const HeapState& heap_state) {
  DCHECK(mark_compacts_ >= idle_mark_compacts_);
  int mutator_gcs = scavenges_ + mark_compacts_ - idle_mark_compacts_;
  switch (mode_) {
    case kDone:
      DCHECK(idle_mark_compacts_ == 0);
      if (IsMutatorActive(heap_state.contexts_disposed, mark_compacts_)) {
        return kReduceLatency;
      }
      break;
    case kReduceLatency:
      if (IsMutatorIdle(long_idle_notifications_,
                        background_idle_notifications_, mutator_gcs)) {
        return kReduceMemory;
      }
      break;
    case kReduceMemory:
      if (idle_mark_compacts_ >= kMaxIdleMarkCompacts ||
          (idle_mark_compacts_ > 0 && !next_gc_likely_to_collect_more_)) {
        return kDone;
      }
      if (mutator_gcs > idle_mark_compacts_) {
        return kReduceLatency;
      }
      break;
  }
  return mode_;
}
}
}
