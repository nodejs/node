// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_ALLOCATOR_INL_H_
#define V8_HEAP_CONCURRENT_ALLOCATOR_INL_H_

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/heap/allocation-result.h"
#include "src/heap/concurrent-allocator.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/local-heap.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

AllocationResult ConcurrentAllocator::AllocateRaw(int size_in_bytes,
                                                  AllocationAlignment alignment,
                                                  AllocationOrigin origin) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
  DCHECK(!v8_flags.enable_third_party_heap);
  DCHECK_EQ(origin == AllocationOrigin::kGC, context_ == Context::kGC);
  // TODO(dinfuehr): Add support for allocation observers
#ifdef DEBUG
  if (local_heap_) local_heap_->VerifyCurrent();
#endif  // DEBUG

  if (size_in_bytes > kMaxLabObjectSize) {
    return AllocateOutsideLab(size_in_bytes, alignment, origin);
  }

  AllocationResult result;
  if (USE_ALLOCATION_ALIGNMENT_BOOL && alignment != kTaggedAligned) {
    result = AllocateInLabFastAligned(size_in_bytes, alignment);
  } else {
    result = AllocateInLabFastUnaligned(size_in_bytes);
  }
  return result.IsFailure()
             ? AllocateInLabSlow(size_in_bytes, alignment, origin)
             : result;
}

AllocationResult ConcurrentAllocator::AllocateInLabFastUnaligned(
    int size_in_bytes) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);

  if (!lab_.CanIncrementTop(size_in_bytes)) {
    return AllocationResult::Failure();
  }

  Tagged<HeapObject> object =
      HeapObject::FromAddress(lab_.IncrementTop(size_in_bytes));
  return AllocationResult::FromObject(object);
}

AllocationResult ConcurrentAllocator::AllocateInLabFastAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
  Address current_top = lab_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);
  int aligned_size = filler_size + size_in_bytes;

  if (!lab_.CanIncrementTop(aligned_size)) {
    return AllocationResult::Failure();
  }

  Tagged<HeapObject> object =
      HeapObject::FromAddress(lab_.IncrementTop(aligned_size));

  if (filler_size > 0) {
    object = owning_heap()->PrecedeWithFillerBackground(object, filler_size);
  }

  return AllocationResult::FromObject(object);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_ALLOCATOR_INL_H_
