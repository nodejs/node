// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/heap/incremental-marking.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {


void IncrementalMarking::RecordWrite(HeapObject* obj, Object** slot,
                                     Object* value) {
  if (IsMarking() && value->IsHeapObject()) {
    RecordWriteSlow(obj, slot, value);
  }
}


void IncrementalMarking::RecordWriteOfCodeEntry(JSFunction* host, Object** slot,
                                                Code* value) {
  if (IsMarking()) {
    RecordWriteOfCodeEntrySlow(host, slot, value);
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
