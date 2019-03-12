// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/heap/incremental-marking.h"

#include "src/heap/mark-compact-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/maybe-object.h"

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

void IncrementalMarking::RecordWrite(HeapObject obj, ObjectSlot slot,
                                     Object value) {
  DCHECK_IMPLIES(slot.address() != kNullAddress, !HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (IsMarking() && value->IsHeapObject()) {
    RecordWriteSlow(obj, HeapObjectSlot(slot), HeapObject::cast(value));
  }
}

void IncrementalMarking::RecordMaybeWeakWrite(HeapObject obj,
                                              MaybeObjectSlot slot,
                                              MaybeObject value) {
  // When writing a weak reference, treat it as strong for the purposes of the
  // marking barrier.
  HeapObject heap_object;
  if (IsMarking() && value->GetHeapObject(&heap_object)) {
    RecordWriteSlow(obj, HeapObjectSlot(slot), heap_object);
  }
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
