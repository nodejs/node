// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

const double GCIdleTimeHandler::kConservativeTimeRatio = 0.9;
const size_t GCIdleTimeHandler::kMaxMarkCompactTimeInMs = 1000;
const size_t GCIdleTimeHandler::kMaxFinalIncrementalMarkCompactTimeInMs = 1000;
const size_t GCIdleTimeHandler::kMinTimeForFinalizeSweeping = 100;
const int GCIdleTimeHandler::kMaxMarkCompactsInIdleRound = 7;
const int GCIdleTimeHandler::kIdleScavengeThreshold = 5;
const double GCIdleTimeHandler::kHighContextDisposalRate = 100;


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
  PrintF("mark_compact_speed=%" V8_PTR_PREFIX "d ",
         mark_compact_speed_in_bytes_per_ms);
  PrintF("incremental_marking_speed=%" V8_PTR_PREFIX "d ",
         incremental_marking_speed_in_bytes_per_ms);
  PrintF("scavenge_speed=%" V8_PTR_PREFIX "d ", scavenge_speed_in_bytes_per_ms);
  PrintF("new_space_size=%" V8_PTR_PREFIX "d ", used_new_space_size);
  PrintF("new_space_capacity=%" V8_PTR_PREFIX "d ", new_space_capacity);
  PrintF("new_space_allocation_throughput=%" V8_PTR_PREFIX "d",
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
      kMaxFrameRenderingIdleTime * scavenge_speed_in_bytes_per_ms;

  // If the limit is larger than the new space size, then scavenging used to be
  // really fast. We can take advantage of the whole new space.
  if (new_space_allocation_limit > new_space_size) {
    new_space_allocation_limit = new_space_size;
  }

  // We do not know the allocation throughput before the first Scavenge.
  // TODO(hpayer): Estimate allocation throughput before the first Scavenge.
  if (new_space_allocation_throughput_in_bytes_per_ms == 0) {
    new_space_allocation_limit =
        static_cast<size_t>(new_space_size * kConservativeTimeRatio);
  } else {
    // We have to trigger scavenge before we reach the end of new space.
    new_space_allocation_limit -=
        new_space_allocation_throughput_in_bytes_per_ms *
        kMaxFrameRenderingIdleTime;
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
  return idle_time_in_ms >=
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


// The following logic is implemented by the controller:
// (1) If we don't have any idle time, do nothing, unless a context was
// disposed, incremental marking is stopped, and the heap is small. Then do
// a full GC.
// (2) If the new space is almost full and we can affort a Scavenge or if the
// next Scavenge will very likely take long, then a Scavenge is performed.
// (3) If there is currently no MarkCompact idle round going on, we start a
// new idle round if enough garbage was created. Otherwise we do not perform
// garbage collection to keep system utilization low.
// (4) If incremental marking is done, we perform a full garbage collection
// if  we are allowed to still do full garbage collections during this idle
// round or if we are not allowed to start incremental marking. Otherwise we
// do not perform garbage collection to keep system utilization low.
// (5) If sweeping is in progress and we received a large enough idle time
// request, we finalize sweeping here.
// (6) If incremental marking is in progress, we perform a marking step. Note,
// that this currently may trigger a full garbage collection.
GCIdleTimeAction GCIdleTimeHandler::Compute(double idle_time_in_ms,
                                            HeapState heap_state) {
  if (static_cast<int>(idle_time_in_ms) <= 0) {
    if (heap_state.contexts_disposed > 0) {
      StartIdleRound();
    }
    if (heap_state.incremental_marking_stopped) {
      if (ShouldDoContextDisposalMarkCompact(
              heap_state.contexts_disposed,
              heap_state.contexts_disposal_rate)) {
        return GCIdleTimeAction::FullGC();
      }
    }
    return GCIdleTimeAction::Nothing();
  }

  if (ShouldDoScavenge(
          static_cast<size_t>(idle_time_in_ms), heap_state.new_space_capacity,
          heap_state.used_new_space_size,
          heap_state.scavenge_speed_in_bytes_per_ms,
          heap_state.new_space_allocation_throughput_in_bytes_per_ms)) {
    return GCIdleTimeAction::Scavenge();
  }

  if (IsMarkCompactIdleRoundFinished()) {
    if (EnoughGarbageSinceLastIdleRound()) {
      StartIdleRound();
    } else {
      return GCIdleTimeAction::Done();
    }
  }

  if (heap_state.incremental_marking_stopped) {
    if (ShouldDoMarkCompact(static_cast<size_t>(idle_time_in_ms),
                            heap_state.size_of_objects,
                            heap_state.mark_compact_speed_in_bytes_per_ms)) {
      // If there are no more than two GCs left in this idle round and we are
      // allowed to do a full GC, then make those GCs full in order to compact
      // the code space.
      // TODO(ulan): Once we enable code compaction for incremental marking, we
      // can get rid of this special case and always start incremental marking.
      int remaining_mark_sweeps =
          kMaxMarkCompactsInIdleRound - mark_compacts_since_idle_round_started_;
      if (static_cast<size_t>(idle_time_in_ms) > kMaxFrameRenderingIdleTime &&
          (remaining_mark_sweeps <= 2 ||
           !heap_state.can_start_incremental_marking)) {
        return GCIdleTimeAction::FullGC();
      }
    }
    if (!heap_state.can_start_incremental_marking) {
      return GCIdleTimeAction::Nothing();
    }
  }
  // TODO(hpayer): Estimate finalize sweeping time.
  if (heap_state.sweeping_in_progress &&
      static_cast<size_t>(idle_time_in_ms) >= kMinTimeForFinalizeSweeping) {
    return GCIdleTimeAction::FinalizeSweeping();
  }

  if (heap_state.incremental_marking_stopped &&
      !heap_state.can_start_incremental_marking) {
    return GCIdleTimeAction::Nothing();
  }
  size_t step_size = EstimateMarkingStepSize(
      static_cast<size_t>(kIncrementalMarkingStepTimeInMs),
      heap_state.incremental_marking_speed_in_bytes_per_ms);
  return GCIdleTimeAction::IncrementalMarking(step_size);
}
}
}
