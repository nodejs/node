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

bool IncrementalMarking::BaseRecordWrite(HeapObject obj, HeapObject value) {
  DCHECK(!marking_state()->IsImpossible(value));
  DCHECK(!marking_state()->IsImpossible(obj));
  // The write barrier stub generated with V8_CONCURRENT_MARKING does not
  // check the color of the source object.
  const bool need_recording =
      V8_CONCURRENT_MARKING_BOOL || marking_state()->IsBlack(obj);

  if (need_recording && WhiteToGreyAndPush(value)) {
    RestartIfNotMarking();
  }
  return is_compacting_ && need_recording;
}

template <typename TSlot>
void IncrementalMarking::RecordWrite(HeapObject obj, TSlot slot,
                                     typename TSlot::TObject value) {
  static_assert(std::is_same<TSlot, ObjectSlot>::value ||
                    std::is_same<TSlot, MaybeObjectSlot>::value,
                "Only ObjectSlot and MaybeObjectSlot are expected here");
  DCHECK_NE(slot.address(), kNullAddress);
  DCHECK_IMPLIES(!TSlot::kCanBeWeak, !HAS_WEAK_HEAP_OBJECT_TAG((*slot).ptr()));
  DCHECK_IMPLIES(!TSlot::kCanBeWeak, !HAS_WEAK_HEAP_OBJECT_TAG(value.ptr()));
  // When writing a weak reference, treat it as strong for the purposes of the
  // marking barrier.
  HeapObject value_heap_object;
  if (IsMarking() && value.GetHeapObject(&value_heap_object)) {
    RecordWriteSlow(obj, HeapObjectSlot(slot), value_heap_object);
  }
}

bool IncrementalMarking::WhiteToGreyAndPush(HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    marking_worklist()->Push(obj);
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
