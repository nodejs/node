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
#include "src/heap/large-spaces.h"
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

PageRange::PageRange(Address start, Address limit)
    : begin_(Page::FromAddress(start)),
      end_(Page::FromAllocationAreaAddress(limit)->next_page()) {
#ifdef DEBUG
  if (begin_->InNewSpace()) {
    SemiSpace::AssertValidRange(start, limit);
  }
#endif  // DEBUG
}

void Space::IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                               size_t amount) {
  base::CheckedIncrement(&external_backing_store_bytes_[type], amount);
  heap()->IncrementExternalBackingStoreBytes(type, amount);
}

void Space::DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                               size_t amount) {
  base::CheckedDecrement(&external_backing_store_bytes_[type], amount);
  heap()->DecrementExternalBackingStoreBytes(type, amount);
}

void Space::MoveExternalBackingStoreBytes(ExternalBackingStoreType type,
                                          Space* from, Space* to,
                                          size_t amount) {
  if (from == to) return;

  base::CheckedDecrement(&(from->external_backing_store_bytes_[type]), amount);
  base::CheckedIncrement(&(to->external_backing_store_bytes_[type]), amount);
}

void Page::MarkNeverAllocateForTesting() {
  DCHECK(this->owner_identity() != NEW_SPACE);
  DCHECK(!IsFlagSet(NEVER_ALLOCATE_ON_PAGE));
  SetFlag(NEVER_ALLOCATE_ON_PAGE);
  SetFlag(NEVER_EVACUATE);
  reinterpret_cast<PagedSpace*>(owner())->free_list()->EvictFreeListItems(this);
}

void Page::MarkEvacuationCandidate() {
  DCHECK(!IsFlagSet(NEVER_EVACUATE));
  DCHECK_NULL(slot_set<OLD_TO_OLD>());
  DCHECK_NULL(typed_slot_set<OLD_TO_OLD>());
  SetFlag(EVACUATION_CANDIDATE);
  reinterpret_cast<PagedSpace*>(owner())->free_list()->EvictFreeListItems(this);
}

void Page::ClearEvacuationCandidate() {
  if (!IsFlagSet(COMPACTION_WAS_ABORTED)) {
    DCHECK_NULL(slot_set<OLD_TO_OLD>());
    DCHECK_NULL(typed_slot_set<OLD_TO_OLD>());
  }
  ClearFlag(EVACUATION_CANDIDATE);
  InitializeFreeListCategories();
}

OldGenerationMemoryChunkIterator::OldGenerationMemoryChunkIterator(Heap* heap)
    : heap_(heap),
      state_(kOldSpaceState),
      old_iterator_(heap->old_space()->begin()),
      code_iterator_(heap->code_space()->begin()),
      map_iterator_(heap->map_space()->begin()),
      lo_iterator_(heap->lo_space()->begin()),
      code_lo_iterator_(heap->code_lo_space()->begin()) {}

MemoryChunk* OldGenerationMemoryChunkIterator::next() {
  switch (state_) {
    case kOldSpaceState: {
      if (old_iterator_ != heap_->old_space()->end()) return *(old_iterator_++);
      state_ = kMapState;
      V8_FALLTHROUGH;
    }
    case kMapState: {
      if (map_iterator_ != heap_->map_space()->end()) return *(map_iterator_++);
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
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);
  int aligned_size = filler_size + size_in_bytes;
  if (!allocation_info_.CanIncrementTop(aligned_size)) {
    return AllocationResult::Retry(NEW_SPACE);
  }
  HeapObject object =
      HeapObject::FromAddress(allocation_info_.IncrementTop(aligned_size));
  if (filler_size > 0) {
    return Heap::PrecedeWithFiller(ReadOnlyRoots(heap_), object, filler_size);
  }

  return AllocationResult(object);
}

LocalAllocationBuffer LocalAllocationBuffer::FromResult(Heap* heap,
                                                        AllocationResult result,
                                                        intptr_t size) {
  if (result.IsRetry()) return InvalidBuffer();
  HeapObject obj;
  bool ok = result.To(&obj);
  USE(ok);
  DCHECK(ok);
  Address top = HeapObject::cast(obj).address();
  return LocalAllocationBuffer(heap, LinearAllocationArea(top, top + size));
}

bool LocalAllocationBuffer::TryMerge(LocalAllocationBuffer* other) {
  return allocation_info_.MergeIfAdjacent(other->allocation_info_);
}

bool LocalAllocationBuffer::TryFreeLast(HeapObject object, int object_size) {
  if (IsValid()) {
    const Address object_address = object.address();
    return allocation_info_.DecrementTopIfAdjacent(object_address, object_size);
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_INL_H_
