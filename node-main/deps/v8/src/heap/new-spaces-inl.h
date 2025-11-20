// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_NEW_SPACES_INL_H_
#define V8_HEAP_NEW_SPACES_INL_H_

#include "src/heap/new-spaces.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/page-metadata.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/tagged-impl.h"
#include "src/objects/tagged.h"

namespace v8 {
namespace internal {

SemiSpaceObjectIterator::SemiSpaceObjectIterator(const SemiSpaceNewSpace* space)
    : current_page_(space->first_page()),
      current_object_(current_page_ ? current_page_->area_start()
                                    : kNullAddress) {}

Tagged<HeapObject> SemiSpaceObjectIterator::Next() {
  if (!current_page_) return {};

  DCHECK(current_page_->ContainsLimit(current_object_));

  while (true) {
    while (current_object_ < current_page_->area_end()) {
      Tagged<HeapObject> object = HeapObject::FromAddress(current_object_);
      SafeHeapObjectSize object_size = object->SafeSize();
      DCHECK_LE(object_size.value(), PageMetadata::kPageSize);
      Address next_object =
          current_object_ + ALIGN_TO_ALLOCATION_ALIGNMENT(object_size.value());
      DCHECK_GT(next_object, current_object_);
      current_object_ = next_object;
      if (!IsFreeSpaceOrFiller(object)) return object;
    }
    current_page_ = current_page_->next_page();
    if (current_page_ == nullptr) return {};
    current_object_ = current_page_->area_start();
  }
}

// -----------------------------------------------------------------------------
// SemiSpaceNewSpace

void SemiSpaceNewSpace::IncrementAllocationTop(Address new_top) {
  DCHECK_LE(allocation_top_, new_top);
  DCHECK_EQ(PageMetadata::FromAllocationAreaAddress(allocation_top_),
            PageMetadata::FromAllocationAreaAddress(new_top));
  allocation_top_ = new_top;
}

void SemiSpaceNewSpace::DecrementAllocationTop(Address new_top) {
  DCHECK_LE(new_top, allocation_top_);
  DCHECK_EQ(PageMetadata::FromAllocationAreaAddress(allocation_top_),
            PageMetadata::FromAllocationAreaAddress(new_top));
  allocation_top_ = new_top;
}

bool SemiSpaceNewSpace::IsAddressBelowAgeMark(Address address) const {
  // Note that we use MemoryChunk here on purpose to avoid the page metadata
  // table lookup for performance reasons.
  MemoryChunk* chunk = MemoryChunk::FromAddress(address);

  // This method is only ever used on non-large pages in the young generation.
  // However, on page promotion (new to old) during a full GC the page flags are
  // already updated to old space before using this method.
#ifdef DEBUG
  auto* metadata = chunk->Metadata(heap()->isolate());
  DCHECK_IMPLIES(!chunk->InYoungGeneration(), metadata->will_be_promoted());
  DCHECK(!metadata->is_large());
#endif  // DEBUG

  if (!chunk->InNewSpaceBelowAgeMark()) {
    return false;
  }

  const Address age_mark = age_mark_;
  const bool on_age_mark_page =
      chunk->address() < age_mark &&
      age_mark <= chunk->address() + PageMetadata::kPageSize;
  DCHECK_EQ(chunk->Metadata()->ContainsLimit(age_mark), on_age_mark_page);
  return !on_age_mark_page || address < age_mark;
}

bool SemiSpaceNewSpace::ShouldBePromoted(Address object) const {
  return IsAddressBelowAgeMark(object);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_NEW_SPACES_INL_H_
