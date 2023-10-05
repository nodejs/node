// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/gc-idle-time-handler.h"

#include "src/flags/flags.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

void GCIdleTimeHeapState::Print() {
  PrintF("size_of_objects=%zu ", size_of_objects);
  PrintF("incremental_marking_stopped=%d ", incremental_marking_stopped);
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
