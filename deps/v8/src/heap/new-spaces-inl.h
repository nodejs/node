// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NEW_SPACES_INL_H_
#define V8_HEAP_NEW_SPACES_INL_H_

#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces-inl.h"
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

V8_WARN_UNUSED_RESULT inline AllocationResult NewSpace::AllocateRawSynchronized(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  base::MutexGuard guard(&mutex_);
  return AllocateRaw(size_in_bytes, alignment, origin);
}

// -----------------------------------------------------------------------------
// SemiSpaceNewSpace

V8_INLINE bool SemiSpaceNewSpace::EnsureAllocation(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin,
    int* out_max_aligned_size) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
#if DEBUG
  VerifyTop();
#endif  // DEBUG

  AdvanceAllocationObservers();

  Address old_top = allocation_info_.top();
  Address high = to_space_.page_high();
  int filler_size = Heap::GetFillToAlign(old_top, alignment);
  int aligned_size_in_bytes = size_in_bytes + filler_size;

  if (old_top + aligned_size_in_bytes > high) {
    // Not enough room in the page, try to allocate a new one.
    if (!AddFreshPage()) {
      // When we cannot grow NewSpace anymore we query for parked allocations.
      if (!v8_flags.allocation_buffer_parking ||
          !AddParkedAllocationBuffer(size_in_bytes, alignment))
        return false;
    }

    old_top = allocation_info_.top();
    high = to_space_.page_high();
    filler_size = Heap::GetFillToAlign(old_top, alignment);
    aligned_size_in_bytes = size_in_bytes + filler_size;
  }

  if (out_max_aligned_size) {
    *out_max_aligned_size = aligned_size_in_bytes;
  }

  DCHECK(old_top + aligned_size_in_bytes <= high);
  UpdateInlineAllocationLimitForAllocation(aligned_size_in_bytes);
  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  return true;
}

// -----------------------------------------------------------------------------
// PagedSpaceForNewSpace

V8_INLINE bool PagedSpaceForNewSpace::EnsureAllocation(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin,
    int* out_max_aligned_size) {
  if (!PagedSpaceBase::EnsureAllocation(size_in_bytes, alignment, origin,
                                        out_max_aligned_size)) {
    if (!AddPageBeyondCapacity(size_in_bytes, origin)) {
      return false;
    }
  }

  allocated_linear_areas_ += limit() - top();
  return true;
}

// -----------------------------------------------------------------------------
// SemiSpaceObjectIterator

SemiSpaceObjectIterator::SemiSpaceObjectIterator(const SemiSpaceNewSpace* space)
    : current_(space->first_allocatable_address()) {}

HeapObject SemiSpaceObjectIterator::Next() {
  while (true) {
    if (Page::IsAlignedToPageSize(current_)) {
      Page* page = Page::FromAllocationAreaAddress(current_);
      page = page->next_page();
      if (page == nullptr) return HeapObject();
      current_ = page->area_start();
    }
    HeapObject object = HeapObject::FromAddress(current_);
    current_ += ALIGN_TO_ALLOCATION_ALIGNMENT(object.Size());
    if (!object.IsFreeSpaceOrFiller()) return object;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_NEW_SPACES_INL_H_
