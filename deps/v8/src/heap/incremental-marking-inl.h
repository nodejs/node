// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/heap/incremental-marking.h"
#include "src/isolate.h"
#include "src/objects/maybe-object.h"

namespace v8 {
namespace internal {


void IncrementalMarking::RecordWrite(HeapObject* obj, Object** slot,
                                     Object* value) {
  DCHECK_IMPLIES(slot != nullptr, !HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  RecordMaybeWeakWrite(obj, reinterpret_cast<MaybeObject**>(slot),
                       reinterpret_cast<MaybeObject*>(value));
}

void IncrementalMarking::RecordMaybeWeakWrite(HeapObject* obj,
                                              MaybeObject** slot,
                                              MaybeObject* value) {
  // When writing a weak reference, treat it as strong for the purposes of the
  // marking barrier.
  HeapObject* heap_object;
  if (IsMarking() && value->ToStrongOrWeakHeapObject(&heap_object)) {
    RecordWriteSlow(obj, reinterpret_cast<HeapObjectReference**>(slot),
                    heap_object);
  }
}

void IncrementalMarking::RecordWrites(HeapObject* obj) {
  if (IsMarking()) {
    if (FLAG_concurrent_marking || marking_state()->IsBlack(obj)) {
      RevisitObject(obj);
    }
  }
}

void IncrementalMarking::RecordWriteIntoCode(Code* host, RelocInfo* rinfo,
                                             Object* value) {
  if (IsMarking() && value->IsHeapObject()) {
    RecordWriteIntoCodeSlow(host, rinfo, value);
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
