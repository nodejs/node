// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGED_SPACES_INL_H_
#define V8_HEAP_PAGED_SPACES_INL_H_

#include "src/common/globals.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/paged-spaces.h"
#include "src/objects/code-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// PagedSpaceObjectIterator

HeapObject PagedSpaceObjectIterator::Next() {
  do {
    HeapObject next_obj = FromCurrentPage();
    if (!next_obj.is_null()) return next_obj;
  } while (AdvanceToNextPage());
  return HeapObject();
}

HeapObject PagedSpaceObjectIterator::FromCurrentPage() {
  while (cur_addr_ != cur_end_) {
    HeapObject obj = HeapObject::FromAddress(cur_addr_);
    const int obj_size = obj.Size();
    cur_addr_ += obj_size;
    DCHECK_LE(cur_addr_, cur_end_);
    if (!obj.IsFreeSpaceOrFiller()) {
      if (obj.IsCode()) {
        DCHECK_EQ(space_->identity(), CODE_SPACE);
        DCHECK_CODEOBJECT_SIZE(obj_size, space_);
      } else {
        DCHECK_OBJECT_SIZE(obj_size);
      }
      return obj;
    }
  }
  return HeapObject();
}

bool PagedSpace::Contains(Address addr) const {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  return Page::FromAddress(addr)->owner() == this;
}

bool PagedSpace::Contains(Object o) const {
  if (!o.IsHeapObject()) return false;
  return Page::FromAddress(o.ptr())->owner() == this;
}

void PagedSpace::UnlinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  page->ForAllFreeListCategories([this](FreeListCategory* category) {
    free_list()->RemoveCategory(category);
  });
}

size_t PagedSpace::RelinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  size_t added = 0;
  page->ForAllFreeListCategories([this, &added](FreeListCategory* category) {
    added += category->available();
    category->Relink(free_list());
  });

  DCHECK_IMPLIES(!page->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
                 page->AvailableInFreeList() ==
                     page->AvailableInFreeListFromAllocatedBytes());
  return added;
}

bool PagedSpace::TryFreeLast(HeapObject object, int object_size) {
  if (allocation_info_.top() != kNullAddress) {
    const Address object_address = object.address();
    if ((allocation_info_.top() - object_size) == object_address) {
      allocation_info_.set_top(object_address);
      return true;
    }
  }
  return false;
}

bool PagedSpace::EnsureLabMain(int size_in_bytes, AllocationOrigin origin) {
  if (allocation_info_.top() + size_in_bytes <= allocation_info_.limit()) {
    return true;
  }
  return RefillLabMain(size_in_bytes, origin);
}

AllocationResult PagedSpace::AllocateFastUnaligned(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  if (new_top > allocation_info_.limit())
    return AllocationResult::Retry(identity());
  DCHECK_LE(new_top, allocation_info_.limit());
  allocation_info_.set_top(new_top);

  return AllocationResult(HeapObject::FromAddress(current_top));
}

AllocationResult PagedSpace::AllocateFastAligned(
    int size_in_bytes, int* aligned_size_in_bytes,
    AllocationAlignment alignment) {
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + size_in_bytes;
  if (new_top > allocation_info_.limit())
    return AllocationResult::Retry(identity());

  allocation_info_.set_top(new_top);
  if (aligned_size_in_bytes)
    *aligned_size_in_bytes = filler_size + size_in_bytes;
  if (filler_size > 0) {
    Heap::PrecedeWithFiller(ReadOnlyRoots(heap()),
                            HeapObject::FromAddress(current_top), filler_size);
  }

  return AllocationResult(HeapObject::FromAddress(current_top + filler_size));
}

AllocationResult PagedSpace::AllocateRawUnaligned(int size_in_bytes,
                                                  AllocationOrigin origin) {
  DCHECK(!FLAG_enable_third_party_heap);
  if (!EnsureLabMain(size_in_bytes, origin)) {
    return AllocationResult::Retry(identity());
  }

  AllocationResult result = AllocateFastUnaligned(size_in_bytes);
  DCHECK(!result.IsRetry());
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(result.ToObjectChecked().address(),
                                      size_in_bytes);

  if (FLAG_trace_allocations_origins) {
    UpdateAllocationOrigins(origin);
  }

  InvokeAllocationObservers(result.ToAddress(), size_in_bytes, size_in_bytes,
                            size_in_bytes);

  return result;
}

AllocationResult PagedSpace::AllocateRawAligned(int size_in_bytes,
                                                AllocationAlignment alignment,
                                                AllocationOrigin origin) {
  DCHECK(!FLAG_enable_third_party_heap);
  DCHECK_EQ(identity(), OLD_SPACE);
  int allocation_size = size_in_bytes;
  // We don't know exactly how much filler we need to align until space is
  // allocated, so assume the worst case.
  int filler_size = Heap::GetMaximumFillToAlign(alignment);
  allocation_size += filler_size;
  if (!EnsureLabMain(allocation_size, origin)) {
    return AllocationResult::Retry(identity());
  }
  int aligned_size_in_bytes;
  AllocationResult result =
      AllocateFastAligned(size_in_bytes, &aligned_size_in_bytes, alignment);
  DCHECK(!result.IsRetry());
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(result.ToObjectChecked().address(),
                                      size_in_bytes);

  if (FLAG_trace_allocations_origins) {
    UpdateAllocationOrigins(origin);
  }

  InvokeAllocationObservers(result.ToAddress(), size_in_bytes,
                            aligned_size_in_bytes, allocation_size);

  return result;
}

AllocationResult PagedSpace::AllocateRaw(int size_in_bytes,
                                         AllocationAlignment alignment,
                                         AllocationOrigin origin) {
  DCHECK(!FLAG_enable_third_party_heap);
  AllocationResult result;

  if (alignment != kWordAligned) {
    result = AllocateFastAligned(size_in_bytes, nullptr, alignment);
  } else {
    result = AllocateFastUnaligned(size_in_bytes);
  }

  if (!result.IsRetry()) {
    return result;
  } else {
    return AllocateRawSlow(size_in_bytes, alignment, origin);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGED_SPACES_INL_H_
