// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/heap/incremental-marking.h"

#include "src/execution/isolate.h"
#include "src/heap/mark-compact-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

void IncrementalMarking::TransferColor(HeapObject from, HeapObject to) {
  if (atomic_marking_state()->IsBlack(to)) {
    DCHECK(black_allocation());
    return;
  }

  DCHECK(atomic_marking_state()->IsWhite(to));
  if (atomic_marking_state()->IsGrey(from)) {
    bool success = atomic_marking_state()->WhiteToGrey(to);
    DCHECK(success);
    USE(success);
  } else if (atomic_marking_state()->IsBlack(from)) {
    bool success = atomic_marking_state()->WhiteToBlack(to);
    DCHECK(success);
    USE(success);
  }
}

bool IncrementalMarking::WhiteToGreyAndPush(HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    local_marking_worklists()->Push(obj);
    return true;
  }
  return false;
}

void IncrementalMarking::RestartIfNotMarking() {
  if (state_ == COMPLETE) {
    state_ = MARKING;
    if (FLAG_trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Restarting (new grey objects)\n");
    }
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_INL_H_
