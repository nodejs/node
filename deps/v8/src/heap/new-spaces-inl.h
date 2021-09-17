// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NEW_SPACES_INL_H_
#define V8_HEAP_NEW_SPACES_INL_H_

#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/new-spaces.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/tagged-impl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// SemiSpace

bool SemiSpace::Contains(HeapObject o) const {
  BasicMemoryChunk* memory_chunk = BasicMemoryChunk::FromHeapObject(o);
  if (memory_chunk->IsLargePage()) return false;
  return id_ == kToSpace ? memory_chunk->IsToPage()
                         : memory_chunk->IsFromPage();
}

bool SemiSpace::Contains(Object o) const {
  return o.IsHeapObject() && Contains(HeapObject::cast(o));
}

bool SemiSpace::ContainsSlow(Address a) const {
  for (const Page* p : *this) {
    if (p == BasicMemoryChunk::FromAddress(a)) return true;
  }
  return false;
}

// --------------------------------------------------------------------------
// NewSpace

bool NewSpace::Contains(Object o) const {
  return o.IsHeapObject() && Contains(HeapObject::cast(o));
}

bool NewSpace::Contains(HeapObject o) const {
  return BasicMemoryChunk::FromHeapObject(o)->InNewSpace();
}

bool NewSpace::ContainsSlow(Address a) const {
  return from_space_.ContainsSlow(a) || to_space_.ContainsSlow(a);
}

bool NewSpace::ToSpaceContainsSlow(Address a) const {
  return to_space_.ContainsSlow(a);
}

bool NewSpace::ToSpaceContains(Object o) const { return to_space_.Contains(o); }
bool NewSpace::FromSpaceContains(Object o) const {
  return from_space_.Contains(o);
}

// -----------------------------------------------------------------------------
// SemiSpaceObjectIterator

HeapObject SemiSpaceObjectIterator::Next() {
  while (current_ != limit_) {
    if (Page::IsAlignedToPageSize(current_)) {
      Page* page = Page::FromAllocationAreaAddress(current_);
      page = page->next_page();
      DCHECK(page);
      current_ = page->area_start();
      if (current_ == limit_) return HeapObject();
    }
    HeapObject object = HeapObject::FromAddress(current_);
    current_ += object.Size();
    if (!object.IsFreeSpaceOrFiller()) {
      return object;
    }
  }
  return HeapObject();
}

// -----------------------------------------------------------------------------
// NewSpace

AllocationResult NewSpace::AllocateRaw(int size_in_bytes,
                                       AllocationAlignment alignment,
                                       AllocationOrigin origin) {
  DCHECK(!FLAG_single_generation);
  DCHECK(!FLAG_enable_third_party_heap);
#if DEBUG
  VerifyTop();
#endif

  AllocationResult result;

  if (alignment != kWordAligned) {
    result = AllocateFastAligned(size_in_bytes, nullptr, alignment, origin);
  } else {
    result = AllocateFastUnaligned(size_in_bytes, origin);
  }

  if (!result.IsRetry()) {
    return result;
  } else {
    return AllocateRawSlow(size_in_bytes, alignment, origin);
  }
}

AllocationResult NewSpace::AllocateFastUnaligned(int size_in_bytes,
                                                 AllocationOrigin origin) {
  if (!allocation_info_.CanIncrementTop(size_in_bytes)) {
    return AllocationResult::Retry(NEW_SPACE);
  }
  HeapObject obj =
      HeapObject::FromAddress(allocation_info_.IncrementTop(size_in_bytes));
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(obj.address(), size_in_bytes);

  if (FLAG_trace_allocations_origins) {
    UpdateAllocationOrigins(origin);
  }

  return obj;
}

AllocationResult NewSpace::AllocateFastAligned(
    int size_in_bytes, int* result_aligned_size_in_bytes,
    AllocationAlignment alignment, AllocationOrigin origin) {
  Address top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(top, alignment);
  int aligned_size_in_bytes = size_in_bytes + filler_size;

  if (!allocation_info_.CanIncrementTop(aligned_size_in_bytes)) {
    return AllocationResult::Retry(NEW_SPACE);
  }
  HeapObject obj = HeapObject::FromAddress(
      allocation_info_.IncrementTop(aligned_size_in_bytes));
  if (result_aligned_size_in_bytes)
    *result_aligned_size_in_bytes = aligned_size_in_bytes;
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  if (filler_size > 0) {
    obj = Heap::PrecedeWithFiller(ReadOnlyRoots(heap()), obj, filler_size);
  }

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(obj.address(), size_in_bytes);

  if (FLAG_trace_allocations_origins) {
    UpdateAllocationOrigins(origin);
  }

  return obj;
}

V8_WARN_UNUSED_RESULT inline AllocationResult NewSpace::AllocateRawSynchronized(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  base::MutexGuard guard(&mutex_);
  return AllocateRaw(size_in_bytes, alignment, origin);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_NEW_SPACES_INL_H_
