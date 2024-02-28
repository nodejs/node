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

HeapObjectRange::iterator::iterator() : cage_base_(kNullAddress) {}

HeapObjectRange::iterator::iterator(const Page* page)
    : cage_base_(page->heap()->isolate()),
      cur_addr_(page->area_start()),
      cur_end_(page->area_end()) {
  AdvanceToNextObject();
}

HeapObjectRange::iterator& HeapObjectRange::iterator::operator++() {
  DCHECK_GT(cur_size_, 0);
  cur_addr_ += cur_size_;
  AdvanceToNextObject();
  return *this;
}

HeapObjectRange::iterator HeapObjectRange::iterator::operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

void HeapObjectRange::iterator::AdvanceToNextObject() {
  DCHECK_NE(cur_addr_, kNullAddress);
  while (cur_addr_ != cur_end_) {
    DCHECK_LT(cur_addr_, cur_end_);
    Tagged<HeapObject> obj = HeapObject::FromAddress(cur_addr_);
    cur_size_ = ALIGN_TO_ALLOCATION_ALIGNMENT(obj->Size(cage_base()));
    DCHECK_LE(cur_addr_ + cur_size_, cur_end_);
    if (IsFreeSpaceOrFiller(obj, cage_base())) {
      cur_addr_ += cur_size_;
    } else {
      if (IsInstructionStream(obj, cage_base())) {
        DCHECK_EQ(Page::FromHeapObject(obj)->owner_identity(), CODE_SPACE);
        DCHECK_CODEOBJECT_SIZE(cur_size_);
      } else {
        DCHECK_OBJECT_SIZE(cur_size_);
      }
      return;
    }
  }
  cur_addr_ = kNullAddress;
}

HeapObjectRange::iterator HeapObjectRange::begin() { return iterator(page_); }

HeapObjectRange::iterator HeapObjectRange::end() { return iterator(); }

Tagged<HeapObject> PagedSpaceObjectIterator::Next() {
  do {
    if (cur_ != end_) {
      return *cur_++;
    }
  } while (AdvanceToNextPage());
  return Tagged<HeapObject>();
}

bool PagedSpaceBase::Contains(Address addr) const {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  return Page::FromAddress(addr)->owner() == this;
}

bool PagedSpaceBase::Contains(Tagged<Object> o) const {
  if (!IsHeapObject(o)) return false;
  return Page::FromAddress(o.ptr())->owner() == this;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGED_SPACES_INL_H_
