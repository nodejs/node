// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGED_SPACES_INL_H_
#define V8_HEAP_PAGED_SPACES_INL_H_

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

bool PagedSpace::EnsureLinearAllocationArea(int size_in_bytes,
                                            AllocationOrigin origin) {
  if (allocation_info_.top() + size_in_bytes <= allocation_info_.limit()) {
    return true;
  }
  return SlowRefillLinearAllocationArea(size_in_bytes, origin);
}

HeapObject PagedSpace::AllocateLinearly(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  DCHECK_LE(new_top, allocation_info_.limit());
  allocation_info_.set_top(new_top);
  return HeapObject::FromAddress(current_top);
}

HeapObject PagedSpace::TryAllocateLinearlyAligned(
    int* size_in_bytes, AllocationAlignment alignment) {
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + *size_in_bytes;
  if (new_top > allocation_info_.limit()) return HeapObject();

  allocation_info_.set_top(new_top);
  if (filler_size > 0) {
    *size_in_bytes += filler_size;
    return Heap::PrecedeWithFiller(ReadOnlyRoots(heap()),
                                   HeapObject::FromAddress(current_top),
                                   filler_size);
  }

  return HeapObject::FromAddress(current_top);
}

AllocationResult PagedSpace::AllocateRawUnaligned(int size_in_bytes,
                                                  AllocationOrigin origin) {
  if (!EnsureLinearAllocationArea(size_in_bytes, origin)) {
    return AllocationResult::Retry(identity());
  }
  HeapObject object = AllocateLinearly(size_in_bytes);
  DCHECK(!object.is_null());
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object.address(), size_in_bytes);

  if (FLAG_trace_allocations_origins) {
    UpdateAllocationOrigins(origin);
  }

  return object;
}

AllocationResult PagedSpace::AllocateRawAligned(int size_in_bytes,
                                                AllocationAlignment alignment,
                                                AllocationOrigin origin) {
  DCHECK_EQ(identity(), OLD_SPACE);
  int allocation_size = size_in_bytes;
  HeapObject object = TryAllocateLinearlyAligned(&allocation_size, alignment);
  if (object.is_null()) {
    // We don't know exactly how much filler we need to align until space is
    // allocated, so assume the worst case.
    int filler_size = Heap::GetMaximumFillToAlign(alignment);
    allocation_size += filler_size;
    if (!EnsureLinearAllocationArea(allocation_size, origin)) {
      return AllocationResult::Retry(identity());
    }
    allocation_size = size_in_bytes;
    object = TryAllocateLinearlyAligned(&allocation_size, alignment);
    DCHECK(!object.is_null());
  }
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object.address(), size_in_bytes);

  if (FLAG_trace_allocations_origins) {
    UpdateAllocationOrigins(origin);
  }

  return object;
}

AllocationResult PagedSpace::AllocateRaw(int size_in_bytes,
                                         AllocationAlignment alignment,
                                         AllocationOrigin origin) {
  if (top_on_previous_step_ && top() < top_on_previous_step_ &&
      SupportsInlineAllocation()) {
    // Generated code decreased the top() pointer to do folded allocations.
    // The top_on_previous_step_ can be one byte beyond the current page.
    DCHECK_NE(top(), kNullAddress);
    DCHECK_EQ(Page::FromAllocationAreaAddress(top()),
              Page::FromAllocationAreaAddress(top_on_previous_step_ - 1));
    top_on_previous_step_ = top();
  }
  size_t bytes_since_last =
      top_on_previous_step_ ? top() - top_on_previous_step_ : 0;

  DCHECK_IMPLIES(!SupportsInlineAllocation(), bytes_since_last == 0);
#ifdef V8_HOST_ARCH_32_BIT
  AllocationResult result =
      alignment != kWordAligned
          ? AllocateRawAligned(size_in_bytes, alignment, origin)
          : AllocateRawUnaligned(size_in_bytes, origin);
#else
  AllocationResult result = AllocateRawUnaligned(size_in_bytes, origin);
#endif
  HeapObject heap_obj;
  if (!result.IsRetry() && result.To(&heap_obj) && !is_local_space()) {
    AllocationStep(static_cast<int>(size_in_bytes + bytes_since_last),
                   heap_obj.address(), size_in_bytes);
    StartNextInlineAllocationStep();
    DCHECK_IMPLIES(
        heap()->incremental_marking()->black_allocation(),
        heap()->incremental_marking()->marking_state()->IsBlack(heap_obj));
  }
  return result;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGED_SPACES_INL_H_
