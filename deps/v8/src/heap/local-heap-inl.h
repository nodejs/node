// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_INL_H_
#define V8_HEAP_LOCAL_HEAP_INL_H_

#include <atomic>

#include "src/common/assert-scope.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

AllocationResult LocalHeap::AllocateRaw(int size_in_bytes, AllocationType type,
                                        AllocationOrigin origin,
                                        AllocationAlignment alignment) {
  DCHECK(!FLAG_enable_third_party_heap);
#if DEBUG
  VerifyCurrent();
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK_IMPLIES(type == AllocationType::kCode || type == AllocationType::kMap,
                 alignment == AllocationAlignment::kTaggedAligned);
  Heap::HeapState state = heap()->gc_state();
  DCHECK(state == Heap::TEAR_DOWN || state == Heap::NOT_IN_GC);
  DCHECK(IsRunning());
#endif

  // Each allocation is supposed to be a safepoint.
  Safepoint();

  bool large_object = size_in_bytes > heap_->MaxRegularHeapObjectSize(type);

  if (type == AllocationType::kCode) {
    AllocationResult alloc;
    if (large_object) {
      alloc =
          heap()->code_lo_space()->AllocateRawBackground(this, size_in_bytes);
    } else {
      alloc =
          code_space_allocator()->AllocateRaw(size_in_bytes, alignment, origin);
    }
    HeapObject object;
    if (alloc.To(&object) && !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
      heap()->UnprotectAndRegisterMemoryChunk(
          object, UnprotectMemoryOrigin::kMaybeOffMainThread);
      heap()->ZapCodeObject(object.address(), size_in_bytes);
    }
    return alloc;
  }

  if (type == AllocationType::kOld) {
    if (large_object)
      return heap()->lo_space()->AllocateRawBackground(this, size_in_bytes);
    else
      return old_space_allocator()->AllocateRaw(size_in_bytes, alignment,
                                                origin);
  }

  DCHECK_EQ(type, AllocationType::kSharedOld);
  return shared_old_space_allocator()->AllocateRaw(size_in_bytes, alignment,
                                                   origin);
}

Address LocalHeap::AllocateRawOrFail(int object_size, AllocationType type,
                                     AllocationOrigin origin,
                                     AllocationAlignment alignment) {
  DCHECK(!FLAG_enable_third_party_heap);
  AllocationResult result = AllocateRaw(object_size, type, origin, alignment);
  HeapObject object;
  if (result.To(&object)) return object.address();
  return PerformCollectionAndAllocateAgain(object_size, type, origin,
                                           alignment);
}

void LocalHeap::CreateFillerObjectAt(Address addr, int size,
                                     ClearRecordedSlots clear_slots_mode) {
  DCHECK_EQ(clear_slots_mode, ClearRecordedSlots::kNo);
  heap()->CreateFillerObjectAtBackground(
      addr, size, ClearFreedMemoryMode::kDontClearFreedMemory);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_HEAP_INL_H_
