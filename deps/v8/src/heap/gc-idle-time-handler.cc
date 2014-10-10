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
const size_t GCIdleTimeHandler::kMinTimeForFinalizeSweeping = 100;
const int GCIdleTimeHandler::kMaxMarkCompactsInIdleRound = 7;
const int GCIdleTimeHandler::kIdleScavengeThreshold = 5;


void GCIdleTimeAction::Print() {
  switch (type) {
    case DONE:
      PrintF("done");
      break;
    case DO_NOTHING:
      PrintF("no action");
      break;
    case DO_INCREMENTAL_MARKING:
      PrintF("incremental marking with step %" V8_PTR_PREFIX "d", parameter);
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
  if (mark_compact_speed_in_bytes_per_ms == 0) {
    mark_compact_speed_in_bytes_per_ms = kInitialConservativeMarkCompactSpeed;
  }
  size_t result = size_of_objects / mark_compact_speed_in_bytes_per_ms;
  return Min(result, kMaxMarkCompactTimeInMs);
}


size_t GCIdleTimeHandler::EstimateScavengeTime(
    size_t new_space_size, size_t scavenge_speed_in_bytes_per_ms) {
  if (scavenge_speed_in_bytes_per_ms == 0) {
    scavenge_speed_in_bytes_per_ms = kInitialConservativeScavengeSpeed;
  }
  return new_space_size / scavenge_speed_in_bytes_per_ms;
}


bool GCIdleTimeHandler::ScavangeMayHappenSoon(
    size_t available_new_space_memory,
    size_t new_space_allocation_throughput_in_bytes_per_ms) {
  if (available_new_space_memory <=
      new_space_allocation_throughput_in_bytes_per_ms *
          kMaxFrameRenderingIdleTime) {
    return true;
  }
  return false;
}


// The following logic is implemented by the controller:
// (1) If the new space is almost full and we can effort a Scavenge, then a
// Scavenge is performed.
// (2) If there is currently no MarkCompact idle round going on, we start a
// new idle round if enough garbage was created or we received a context
// disposal event. Otherwise we do not perform garbage collection to keep
// system utilization low.
// (3) If incremental marking is done, we perform a full garbage collection
// if context was disposed or if we are allowed to still do full garbage
// collections during this idle round or if we are not allowed to start
// incremental marking. Otherwise we do not perform garbage collection to
// keep system utilization low.
// (4) If sweeping is in progress and we received a large enough idle time
// request, we finalize sweeping here.
// (5) If incremental marking is in progress, we perform a marking step. Note,
// that this currently may trigger a full garbage collection.
GCIdleTimeAction GCIdleTimeHandler::Compute(size_t idle_time_in_ms,
                                            HeapState heap_state) {
  if (idle_time_in_ms <= kMaxFrameRenderingIdleTime &&
      ScavangeMayHappenSoon(
          heap_state.available_new_space_memory,
          heap_state.new_space_allocation_throughput_in_bytes_per_ms) &&
      idle_time_in_ms >=
          EstimateScavengeTime(heap_state.new_space_capacity,
                               heap_state.scavenge_speed_in_bytes_per_ms)) {
    return GCIdleTimeAction::Scavenge();
  }
  if (IsMarkCompactIdleRoundFinished()) {
    if (EnoughGarbageSinceLastIdleRound() || heap_state.contexts_disposed > 0) {
      StartIdleRound();
    } else {
      return GCIdleTimeAction::Done();
    }
  }

  if (idle_time_in_ms == 0) {
    return GCIdleTimeAction::Nothing();
  }

  if (heap_state.incremental_marking_stopped) {
    size_t estimated_time_in_ms =
        EstimateMarkCompactTime(heap_state.size_of_objects,
                                heap_state.mark_compact_speed_in_bytes_per_ms);
    if (idle_time_in_ms >= estimated_time_in_ms ||
        (heap_state.size_of_objects < kSmallHeapSize &&
         heap_state.contexts_disposed > 0)) {
      // If there are no more than two GCs left in this idle round and we are
      // allowed to do a full GC, then make those GCs full in order to compact
      // the code space.
      // TODO(ulan): Once we enable code compaction for incremental marking, we
      // can get rid of this special case and always start incremental marking.
      int remaining_mark_sweeps =
          kMaxMarkCompactsInIdleRound - mark_compacts_since_idle_round_started_;
      if (heap_state.contexts_disposed > 0 ||
          (idle_time_in_ms > kMaxFrameRenderingIdleTime &&
           (remaining_mark_sweeps <= 2 ||
            !heap_state.can_start_incremental_marking))) {
        return GCIdleTimeAction::FullGC();
      }
    }
    if (!heap_state.can_start_incremental_marking) {
      return GCIdleTimeAction::Nothing();
    }
  }
  // TODO(hpayer): Estimate finalize sweeping time.
  if (heap_state.sweeping_in_progress &&
      idle_time_in_ms >= kMinTimeForFinalizeSweeping) {
    return GCIdleTimeAction::FinalizeSweeping();
  }

  if (heap_state.incremental_marking_stopped &&
      !heap_state.can_start_incremental_marking) {
    return GCIdleTimeAction::Nothing();
  }
  size_t step_size = EstimateMarkingStepSize(
      idle_time_in_ms, heap_state.incremental_marking_speed_in_bytes_per_ms);
  return GCIdleTimeAction::IncrementalMarking(step_size);
}
}
}
