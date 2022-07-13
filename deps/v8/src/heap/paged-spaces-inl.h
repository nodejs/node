// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGED_SPACES_INL_H_
#define V8_HEAP_PAGED_SPACES_INL_H_

#include "src/common/globals.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/paged-spaces.h"
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
    const int obj_size = obj.Size(cage_base());
    cur_addr_ += obj_size;
    DCHECK_LE(cur_addr_, cur_end_);
    if (!obj.IsFreeSpaceOrFiller(cage_base())) {
      if (obj.IsCode(cage_base())) {
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

bool PagedSpace::TryFreeLast(Address object_address, int object_size) {
  if (allocation_info_->top() != kNullAddress) {
    return allocation_info_->DecrementTopIfAdjacent(object_address,
                                                    object_size);
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGED_SPACES_INL_H_
