// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EVACUATION_ALLOCATOR_INL_H_
#define V8_HEAP_EVACUATION_ALLOCATOR_INL_H_

#include "src/common/globals.h"
#include "src/heap/evacuation-allocator.h"
#include "src/heap/spaces-inl.h"

namespace v8 {
namespace internal {

AllocationResult EvacuationAllocator::Allocate(AllocationSpace space,
                                               int object_size,
                                               AllocationOrigin origin,
                                               AllocationAlignment alignment) {
  object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  switch (space) {
    case NEW_SPACE:
      return AllocateInNewSpace(object_size, origin, alignment);
    case OLD_SPACE:
      return compaction_spaces_.Get(OLD_SPACE)->AllocateRaw(object_size,
                                                            alignment, origin);
    case CODE_SPACE:
      return compaction_spaces_.Get(CODE_SPACE)
          ->AllocateRaw(object_size, alignment, origin);
    case SHARED_SPACE:
      return compaction_spaces_.Get(SHARED_SPACE)
          ->AllocateRaw(object_size, alignment, origin);
    default:
      UNREACHABLE();
  }
}

void EvacuationAllocator::FreeLast(AllocationSpace space,
                                   Tagged<HeapObject> object, int object_size) {
  object_size = ALIGN_TO_ALLOCATION_ALIGNMENT(object_size);
  switch (space) {
    case NEW_SPACE:
      FreeLastInNewSpace(object, object_size);
      return;
    case OLD_SPACE:
      FreeLastInCompactionSpace(OLD_SPACE, object, object_size);
      return;
    case SHARED_SPACE:
      FreeLastInCompactionSpace(SHARED_SPACE, object, object_size);
      return;
    default:
      // Only new and old space supported.
      UNREACHABLE();
  }
}

void EvacuationAllocator::FreeLastInNewSpace(Tagged<HeapObject> object,
                                             int object_size) {
  if (!new_space_lab_.TryFreeLast(object, object_size)) {
    // We couldn't free the last object so we have to write a proper filler.
    heap_->CreateFillerObjectAt(object.address(), object_size);
  }
}

void EvacuationAllocator::FreeLastInCompactionSpace(AllocationSpace space,
                                                    Tagged<HeapObject> object,
                                                    int object_size) {
  if (!compaction_spaces_.Get(space)->TryFreeLast(object.address(),
                                                  object_size)) {
    // We couldn't free the last object so we have to write a proper filler.
    heap_->CreateFillerObjectAt(object.address(), object_size);
  }
}

AllocationResult EvacuationAllocator::AllocateInLAB(
    int object_size, AllocationAlignment alignment) {
  AllocationResult allocation;
  if (!new_space_lab_.IsValid() && !NewLocalAllocationBuffer()) {
    return AllocationResult::Failure();
  }
  allocation = new_space_lab_.AllocateRawAligned(object_size, alignment);
  if (allocation.IsFailure()) {
    if (!NewLocalAllocationBuffer()) {
      return AllocationResult::Failure();
    } else {
      allocation = new_space_lab_.AllocateRawAligned(object_size, alignment);
      CHECK(!allocation.IsFailure());
    }
  }
  return allocation;
}

bool EvacuationAllocator::NewLocalAllocationBuffer() {
  if (lab_allocation_will_fail_) return false;
  AllocationResult result =
      new_space_->AllocateRawSynchronized(kLabSize, kTaggedAligned);
  if (result.IsFailure()) {
    lab_allocation_will_fail_ = true;
    return false;
  }
  LocalAllocationBuffer saved_lab = std::move(new_space_lab_);
  new_space_lab_ = LocalAllocationBuffer::FromResult(heap_, result, kLabSize);
  DCHECK(new_space_lab_.IsValid());
  if (!new_space_lab_.TryMerge(&saved_lab)) {
    saved_lab.CloseAndMakeIterable();
  }
  return true;
}

AllocationResult EvacuationAllocator::AllocateInNewSpace(
    int object_size, AllocationOrigin origin, AllocationAlignment alignment) {
  if (object_size > kMaxLabObjectSize) {
    return new_space_->AllocateRawSynchronized(object_size, alignment, origin);
  }
  return AllocateInLAB(object_size, alignment);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EVACUATION_ALLOCATOR_INL_H_
