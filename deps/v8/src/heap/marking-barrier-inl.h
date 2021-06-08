// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MARKING_BARRIER_INL_H_
#define V8_HEAP_MARKING_BARRIER_INL_H_

#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-barrier.h"

namespace v8 {
namespace internal {

bool MarkingBarrier::MarkValue(HeapObject host, HeapObject value) {
  DCHECK(IsCurrentMarkingBarrier());
  DCHECK(is_activated_);
  DCHECK(!marking_state_.IsImpossible(value));
  // Host may have an impossible markbit pattern if manual allocation folding
  // is performed and host happens to be the last word of an allocated region.
  // In that case host has only one markbit and the second markbit belongs to
  // another object. We can detect that case by checking if value is a one word
  // filler map.
  DCHECK(!marking_state_.IsImpossible(host) ||
         value == ReadOnlyRoots(heap_->isolate()).one_pointer_filler_map());
  if (!V8_CONCURRENT_MARKING_BOOL && !marking_state_.IsBlack(host)) {
    // The value will be marked and the slot will be recorded when the marker
    // visits the host object.
    return false;
  }
  if (WhiteToGreyAndPush(value) && is_main_thread_barrier_) {
    incremental_marking_->RestartIfNotMarking();
  }
  return true;
}

bool MarkingBarrier::WhiteToGreyAndPush(HeapObject obj) {
  if (marking_state_.WhiteToGrey(obj)) {
    worklist_.Push(obj);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_BARRIER_INL_H_
