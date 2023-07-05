// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-idle-time-handler.h"

#include "src/flags/flags.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

const double GCIdleTimeHandler::kConservativeTimeRatio = 0.9;

void GCIdleTimeHeapState::Print() {
  PrintF("size_of_objects=%zu ", size_of_objects);
  PrintF("incremental_marking_stopped=%d ", incremental_marking_stopped);
}

size_t GCIdleTimeHandler::EstimateMarkingStepSize(
    double idle_time_in_ms, double marking_speed_in_bytes_per_ms) {
  DCHECK_LT(0, idle_time_in_ms);

  if (marking_speed_in_bytes_per_ms == 0) {
    marking_speed_in_bytes_per_ms = kInitialConservativeMarkingSpeed;
  }

  double marking_step_size = marking_speed_in_bytes_per_ms * idle_time_in_ms;
  if (marking_step_size >= kMaximumMarkingStepSize) {
    return kMaximumMarkingStepSize;
  }
  return static_cast<size_t>(marking_step_size * kConservativeTimeRatio);
}

// The following logic is implemented by the controller:
// (1) If we don't have any idle time, do nothing, unless a context was
// disposed, incremental marking is stopped, and the heap is small. Then do
// a full GC.
// (2) If the context disposal rate is high and we cannot perform a full GC,
// we do nothing until the context disposal rate becomes lower.
// (3) If incremental marking is in progress, we perform a marking step.
GCIdleTimeAction GCIdleTimeHandler::Compute(double idle_time_in_ms,
                                            GCIdleTimeHeapState heap_state) {
  if (static_cast<int>(idle_time_in_ms) <= 0) {
    return GCIdleTimeAction::kDone;
  }

  if (v8_flags.incremental_marking && !heap_state.incremental_marking_stopped) {
    return GCIdleTimeAction::kIncrementalStep;
  }

  return GCIdleTimeAction::kDone;
}

bool GCIdleTimeHandler::Enabled() { return v8_flags.incremental_marking; }

}  // namespace internal
}  // namespace v8
