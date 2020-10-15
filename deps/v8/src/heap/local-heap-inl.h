// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_INL_H_
#define V8_HEAP_LOCAL_HEAP_INL_H_

#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

AllocationResult LocalHeap::AllocateRaw(int size_in_bytes, AllocationType type,
                                        AllocationOrigin origin,
                                        AllocationAlignment alignment) {
  DCHECK_EQ(LocalHeap::Current(), this);
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK_IMPLIES(type == AllocationType::kCode,
                 alignment == AllocationAlignment::kCodeAligned);
  DCHECK(heap()->gc_state() == Heap::TEAR_DOWN ||
         heap()->gc_state() == Heap::NOT_IN_GC);

  bool large_object = size_in_bytes > kMaxRegularHeapObjectSize;
  CHECK_EQ(type, AllocationType::kOld);

  if (large_object)
    return heap()->lo_space()->AllocateRawBackground(this, size_in_bytes);
  else
    return old_space_allocator()->AllocateRaw(size_in_bytes, alignment, origin);
}

Address LocalHeap::AllocateRawOrFail(int object_size, AllocationType type,
                                     AllocationOrigin origin,
                                     AllocationAlignment alignment) {
  AllocationResult result = AllocateRaw(object_size, type, origin, alignment);
  if (!result.IsRetry()) return result.ToObject().address();
  return PerformCollectionAndAllocateAgain(object_size, type, origin,
                                           alignment);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_HEAP_INL_H_
