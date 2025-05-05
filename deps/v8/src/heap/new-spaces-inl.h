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
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/tagged-impl.h"
#include "src/objects/tagged.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// SemiSpace

bool SemiSpace::Contains(Tagged<HeapObject> o) const {
  MemoryChunk* memory_chunk = MemoryChunk::FromHeapObject(o);
  if (memory_chunk->IsLargePage()) return false;
  return id_ == kToSpace ? memory_chunk->IsToPage()
                         : memory_chunk->IsFromPage();
}

bool SemiSpace::Contains(Tagged<Object> o) const {
  return IsHeapObject(o) && Contains(Cast<HeapObject>(o));
}

template <typename T>
inline bool SemiSpace::Contains(Tagged<T> o) const {
  static_assert(kTaggedCanConvertToRawObjects);
  return Contains(*o);
}

bool SemiSpace::ContainsSlow(Address a) const {
  for (const PageMetadata* p : *this) {
    if (p == MemoryChunkMetadata::FromAddress(a)) return true;
  }
  return false;
}

// --------------------------------------------------------------------------
// NewSpace

bool NewSpace::Contains(Tagged<Object> o) const {
  return IsHeapObject(o) && Contains(Cast<HeapObject>(o));
}

bool NewSpace::Contains(Tagged<HeapObject> o) const {
  return MemoryChunk::FromHeapObject(o)->InNewSpace();
}

// -----------------------------------------------------------------------------
// SemiSpaceObjectIterator

SemiSpaceObjectIterator::SemiSpaceObjectIterator(const SemiSpaceNewSpace* space)
    : current_(space->first_allocatable_address()) {}

Tagged<HeapObject> SemiSpaceObjectIterator::Next() {
  while (true) {
    if (PageMetadata::IsAlignedToPageSize(current_)) {
      PageMetadata* page = PageMetadata::FromAllocationAreaAddress(current_);
      page = page->next_page();
      if (page == nullptr) return Tagged<HeapObject>();
      current_ = page->area_start();
    }
    Tagged<HeapObject> object = HeapObject::FromAddress(current_);
    current_ += ALIGN_TO_ALLOCATION_ALIGNMENT(object->Size());
    if (!IsFreeSpaceOrFiller(object)) return object;
  }
}

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
  DCHECK(chunk->InYoungGeneration() ||
         chunk->IsFlagSet(MemoryChunk::PAGE_NEW_OLD_PROMOTION));
  DCHECK(!chunk->IsLargePage());

  if (!chunk->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK)) {
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
