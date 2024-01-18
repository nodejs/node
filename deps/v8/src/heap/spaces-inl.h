// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SPACES_INL_H_
#define V8_HEAP_SPACES_INL_H_

#include "src/base/atomic-utils.h"
#include "src/base/v8-fallthrough.h"
#include "src/common/globals.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/large-page.h"
#include "src/heap/large-spaces.h"
#include "src/heap/main-allocator-inl.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

template <class PAGE_TYPE>
PageIteratorImpl<PAGE_TYPE>& PageIteratorImpl<PAGE_TYPE>::operator++() {
  p_ = p_->next_page();
  return *this;
}

template <class PAGE_TYPE>
PageIteratorImpl<PAGE_TYPE> PageIteratorImpl<PAGE_TYPE>::operator++(int) {
  PageIteratorImpl<PAGE_TYPE> tmp(*this);
  operator++();
  return tmp;
}

void Space::IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                               size_t amount) {
  base::CheckedIncrement(&external_backing_store_bytes_[static_cast<int>(type)],
                         amount);
  heap()->IncrementExternalBackingStoreBytes(type, amount);
}

void Space::DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                               size_t amount) {
  base::CheckedDecrement(&external_backing_store_bytes_[static_cast<int>(type)],
                         amount);
  heap()->DecrementExternalBackingStoreBytes(type, amount);
}

void Space::MoveExternalBackingStoreBytes(ExternalBackingStoreType type,
                                          Space* from, Space* to,
                                          size_t amount) {
  if (from == to) return;

  base::CheckedDecrement(
      &(from->external_backing_store_bytes_[static_cast<int>(type)]), amount);
  base::CheckedIncrement(
      &(to->external_backing_store_bytes_[static_cast<int>(type)]), amount);
}

PageRange::PageRange(Page* page) : PageRange(page, page->next_page()) {}
ConstPageRange::ConstPageRange(const Page* page)
    : ConstPageRange(page, page->next_page()) {}

OldGenerationMemoryChunkIterator::OldGenerationMemoryChunkIterator(Heap* heap)
    : heap_(heap),
      state_(kOldSpaceState),
      old_iterator_(heap->old_space()->begin()),
      code_iterator_(heap->code_space()->begin()),
      lo_iterator_(heap->lo_space()->begin()),
      code_lo_iterator_(heap->code_lo_space()->begin()) {}

MemoryChunk* OldGenerationMemoryChunkIterator::next() {
  switch (state_) {
    case kOldSpaceState: {
      if (old_iterator_ != heap_->old_space()->end()) return *(old_iterator_++);
      state_ = kCodeState;
      V8_FALLTHROUGH;
    }
    case kCodeState: {
      if (code_iterator_ != heap_->code_space()->end())
        return *(code_iterator_++);
      state_ = kLargeObjectState;
      V8_FALLTHROUGH;
    }
    case kLargeObjectState: {
      if (lo_iterator_ != heap_->lo_space()->end()) return *(lo_iterator_++);
      state_ = kCodeLargeObjectState;
      V8_FALLTHROUGH;
    }
    case kCodeLargeObjectState: {
      if (code_lo_iterator_ != heap_->code_lo_space()->end())
        return *(code_lo_iterator_++);
      state_ = kFinishedState;
      V8_FALLTHROUGH;
    }
    case kFinishedState:
      return nullptr;
    default:
      break;
  }
  UNREACHABLE();
}

AllocationResult LocalAllocationBuffer::AllocateRawAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);
  int aligned_size = filler_size + size_in_bytes;
  if (!allocation_info_.CanIncrementTop(aligned_size)) {
    return AllocationResult::Failure();
  }
  Tagged<HeapObject> object =
      HeapObject::FromAddress(allocation_info_.IncrementTop(aligned_size));
  return filler_size > 0 ? AllocationResult::FromObject(
                               heap_->PrecedeWithFiller(object, filler_size))
                         : AllocationResult::FromObject(object);
}

AllocationResult LocalAllocationBuffer::AllocateRawUnaligned(
    int size_in_bytes) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
  return allocation_info_.CanIncrementTop(size_in_bytes)
             ? AllocationResult::FromObject(HeapObject::FromAddress(
                   allocation_info_.IncrementTop(size_in_bytes)))
             : AllocationResult::Failure();
}

LocalAllocationBuffer LocalAllocationBuffer::FromResult(Heap* heap,
                                                        AllocationResult result,
                                                        intptr_t size) {
  if (result.IsFailure()) return InvalidBuffer();
  Tagged<HeapObject> obj;
  bool ok = result.To(&obj);
  USE(ok);
  DCHECK(ok);
  Address top = HeapObject::cast(obj).address();
  return LocalAllocationBuffer(heap, LinearAllocationArea(top, top + size));
}

bool LocalAllocationBuffer::TryMerge(LocalAllocationBuffer* other) {
  return allocation_info_.MergeIfAdjacent(other->allocation_info_);
}

bool LocalAllocationBuffer::TryFreeLast(Tagged<HeapObject> object,
                                        int object_size) {
  if (IsValid()) {
    const Address object_address = object.address();
    return allocation_info_.DecrementTopIfAdjacent(object_address, object_size);
  }
  return false;
}

bool MemoryChunkIterator::HasNext() {
  if (current_chunk_) return true;

  while (space_iterator_.HasNext()) {
    Space* space = space_iterator_.Next();
    current_chunk_ = space->first_page();
    if (current_chunk_) return true;
  }

  return false;
}

MemoryChunk* MemoryChunkIterator::Next() {
  MemoryChunk* chunk = current_chunk_;
  current_chunk_ = chunk->list_node().next();
  return chunk;
}

AllocationResult SpaceWithLinearArea::AllocateRaw(int size_in_bytes,
                                                  AllocationAlignment alignment,
                                                  AllocationOrigin origin) {
  return allocator_->AllocateRaw(size_in_bytes, alignment, origin);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_INL_H_
