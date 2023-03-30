// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-state-inl.h"
#include "src/objects/descriptor-array.h"

namespace v8 {
namespace internal {

void IncrementalMarking::TransferColor(HeapObject from, HeapObject to) {
  if (atomic_marking_state()->IsMarked(to)) {
    DCHECK(black_allocation());
    return;
  }

  DCHECK(atomic_marking_state()->IsUnmarked(to));
  if (atomic_marking_state()->IsGrey(from)) {
    bool success = atomic_marking_state()->TryMark(to);
    DCHECK(success);
    USE(success);
  } else if (atomic_marking_state()->IsMarked(from)) {
    bool success = atomic_marking_state()->TryMark(to);
    DCHECK(success);
    USE(success);
    success = atomic_marking_state()->GreyToBlack(to);
    DCHECK(success);
    USE(success);
    if (!to.IsDescriptorArray() ||
        (DescriptorArrayMarkingState::Marked::decode(
             DescriptorArray::cast(to).raw_gc_state(kRelaxedLoad)) != 0)) {
      atomic_marking_state()->IncrementLiveBytes(
          MemoryChunk::cast(BasicMemoryChunk::FromHeapObject(to)),
          ALIGN_TO_ALLOCATION_ALIGNMENT(to.Size()));
    }
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_INL_H_
