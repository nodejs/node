// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_ALLOCATOR_INL_H_
#define V8_HEAP_CONCURRENT_ALLOCATOR_INL_H_

#include "src/common/globals.h"
#include "src/heap/concurrent-allocator.h"

#include "src/heap/heap.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

AllocationResult ConcurrentAllocator::Allocate(int object_size,
                                               AllocationAlignment alignment,
                                               AllocationOrigin origin) {
  // TODO(dinfuehr): Add support for allocation observers
  CHECK(FLAG_concurrent_allocation);
  if (object_size > kMaxLabObjectSize) {
    auto result = space_->SlowGetLinearAllocationAreaBackground(
        object_size, object_size, alignment, origin);

    if (result) {
      HeapObject object = HeapObject::FromAddress(result->first);
      return AllocationResult(object);
    } else {
      return AllocationResult::Retry(OLD_SPACE);
    }
  }

  return AllocateInLab(object_size, alignment, origin);
}

AllocationResult ConcurrentAllocator::AllocateInLab(
    int object_size, AllocationAlignment alignment, AllocationOrigin origin) {
  AllocationResult allocation;
  if (!lab_.IsValid() && !EnsureLab(origin)) {
    return AllocationResult::Retry(space_->identity());
  }
  allocation = lab_.AllocateRawAligned(object_size, alignment);
  if (allocation.IsRetry()) {
    if (!EnsureLab(origin)) {
      return AllocationResult::Retry(space_->identity());
    } else {
      allocation = lab_.AllocateRawAligned(object_size, alignment);
      CHECK(!allocation.IsRetry());
    }
  }
  return allocation;
}

bool ConcurrentAllocator::EnsureLab(AllocationOrigin origin) {
  LocalAllocationBuffer saved_lab = lab_;
  auto result = space_->SlowGetLinearAllocationAreaBackground(
      kLabSize, kMaxLabSize, kWordAligned, origin);

  if (!result) return false;

  HeapObject object = HeapObject::FromAddress(result->first);
  lab_ = LocalAllocationBuffer::FromResult(
      local_heap_->heap(), AllocationResult(object), result->second);
  if (lab_.IsValid()) {
    lab_.TryMerge(&saved_lab);
    return true;
  }
  lab_ = saved_lab;
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_ALLOCATOR_INL_H_
