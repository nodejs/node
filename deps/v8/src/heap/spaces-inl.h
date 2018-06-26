// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SPACES_INL_H_
#define V8_HEAP_SPACES_INL_H_

#include "src/base/v8-fallthrough.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/msan.h"
#include "src/objects/code-inl.h"

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

// -----------------------------------------------------------------------------
// SemiSpaceIterator

HeapObject* SemiSpaceIterator::Next() {
  while (current_ != limit_) {
    if (Page::IsAlignedToPageSize(current_)) {
      Page* page = Page::FromAllocationAreaAddress(current_);
      page = page->next_page();
      DCHECK(!page->is_anchor());
      current_ = page->area_start();
      if (current_ == limit_) return nullptr;
    }
    HeapObject* object = HeapObject::FromAddress(current_);
    current_ += object->Size();
    if (!object->IsFiller()) {
      return object;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
// HeapObjectIterator

HeapObject* HeapObjectIterator::Next() {
  do {
    HeapObject* next_obj = FromCurrentPage();
    if (next_obj != nullptr) return next_obj;
  } while (AdvanceToNextPage());
  return nullptr;
}

HeapObject* HeapObjectIterator::FromCurrentPage() {
  while (cur_addr_ != cur_end_) {
    if (cur_addr_ == space_->top() && cur_addr_ != space_->limit()) {
      cur_addr_ = space_->limit();
      continue;
    }
    HeapObject* obj = HeapObject::FromAddress(cur_addr_);
    const int obj_size = obj->Size();
    cur_addr_ += obj_size;
    DCHECK_LE(cur_addr_, cur_end_);
    if (!obj->IsFiller()) {
      if (obj->IsCode()) {
        DCHECK_EQ(space_, space_->heap()->code_space());
        DCHECK_CODEOBJECT_SIZE(obj_size, space_);
      } else {
        DCHECK_OBJECT_SIZE(obj_size);
      }
      return obj;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
// SemiSpace

bool SemiSpace::Contains(HeapObject* o) {
  return id_ == kToSpace
             ? MemoryChunk::FromAddress(o->address())->InToSpace()
             : MemoryChunk::FromAddress(o->address())->InFromSpace();
}

bool SemiSpace::Contains(Object* o) {
  return o->IsHeapObject() && Contains(HeapObject::cast(o));
}

bool SemiSpace::ContainsSlow(Address a) {
  for (Page* p : *this) {
    if (p == MemoryChunk::FromAddress(a)) return true;
  }
  return false;
}

// --------------------------------------------------------------------------
// NewSpace

bool NewSpace::Contains(HeapObject* o) {
  return MemoryChunk::FromAddress(o->address())->InNewSpace();
}

bool NewSpace::Contains(Object* o) {
  return o->IsHeapObject() && Contains(HeapObject::cast(o));
}

bool NewSpace::ContainsSlow(Address a) {
  return from_space_.ContainsSlow(a) || to_space_.ContainsSlow(a);
}

bool NewSpace::ToSpaceContainsSlow(Address a) {
  return to_space_.ContainsSlow(a);
}

bool NewSpace::FromSpaceContainsSlow(Address a) {
  return from_space_.ContainsSlow(a);
}

bool NewSpace::ToSpaceContains(Object* o) { return to_space_.Contains(o); }
bool NewSpace::FromSpaceContains(Object* o) { return from_space_.Contains(o); }

bool PagedSpace::Contains(Address addr) {
  if (heap_->lo_space()->FindPage(addr)) return false;
  return MemoryChunk::FromAnyPointerAddress(heap(), addr)->owner() == this;
}

bool PagedSpace::Contains(Object* o) {
  if (!o->IsHeapObject()) return false;
  return Page::FromAddress(HeapObject::cast(o)->address())->owner() == this;
}

void PagedSpace::UnlinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  page->ForAllFreeListCategories([this](FreeListCategory* category) {
    DCHECK_EQ(free_list(), category->owner());
    category->set_free_list(nullptr);
    free_list()->RemoveCategory(category);
  });
}

size_t PagedSpace::RelinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  size_t added = 0;
  page->ForAllFreeListCategories([this, &added](FreeListCategory* category) {
    category->set_free_list(&free_list_);
    added += category->available();
    category->Relink();
  });
  DCHECK_EQ(page->AvailableInFreeList(),
            page->AvailableInFreeListFromAllocatedBytes());
  return added;
}

bool PagedSpace::TryFreeLast(HeapObject* object, int object_size) {
  if (allocation_info_.top() != nullptr) {
    const Address object_address = object->address();
    if ((allocation_info_.top() - object_size) == object_address) {
      allocation_info_.set_top(object_address);
      return true;
    }
  }
  return false;
}

MemoryChunk* MemoryChunk::FromAnyPointerAddress(Heap* heap, Address addr) {
  MemoryChunk* chunk = heap->lo_space()->FindPage(addr);
  if (chunk == nullptr) {
    chunk = MemoryChunk::FromAddress(addr);
  }
  return chunk;
}

void Page::MarkNeverAllocateForTesting() {
  DCHECK(this->owner()->identity() != NEW_SPACE);
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

MemoryChunkIterator::MemoryChunkIterator(Heap* heap)
    : heap_(heap),
      state_(kOldSpaceState),
      old_iterator_(heap->old_space()->begin()),
      code_iterator_(heap->code_space()->begin()),
      map_iterator_(heap->map_space()->begin()),
      lo_iterator_(heap->lo_space()->begin()) {}

MemoryChunk* MemoryChunkIterator::next() {
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

Page* FreeList::GetPageForCategoryType(FreeListCategoryType type) {
  return top(type) ? top(type)->page() : nullptr;
}

FreeList* FreeListCategory::owner() { return free_list_; }

bool FreeListCategory::is_linked() {
  return prev_ != nullptr || next_ != nullptr;
}

AllocationResult LocalAllocationBuffer::AllocateRawAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + size_in_bytes;
  if (new_top > allocation_info_.limit()) return AllocationResult::Retry();

  allocation_info_.set_top(new_top);
  if (filler_size > 0) {
    return heap_->PrecedeWithFiller(HeapObject::FromAddress(current_top),
                                    filler_size);
  }

  return AllocationResult(HeapObject::FromAddress(current_top));
}

bool PagedSpace::EnsureLinearAllocationArea(int size_in_bytes) {
  if (allocation_info_.top() + size_in_bytes <= allocation_info_.limit()) {
    return true;
  }
  return SlowRefillLinearAllocationArea(size_in_bytes);
}

HeapObject* PagedSpace::AllocateLinearly(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  DCHECK_LE(new_top, allocation_info_.limit());
  allocation_info_.set_top(new_top);
  return HeapObject::FromAddress(current_top);
}

HeapObject* PagedSpace::TryAllocateLinearlyAligned(
    int* size_in_bytes, AllocationAlignment alignment) {
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + *size_in_bytes;
  if (new_top > allocation_info_.limit()) return nullptr;

  allocation_info_.set_top(new_top);
  if (filler_size > 0) {
    *size_in_bytes += filler_size;
    return heap()->PrecedeWithFiller(HeapObject::FromAddress(current_top),
                                     filler_size);
  }

  return HeapObject::FromAddress(current_top);
}

AllocationResult PagedSpace::AllocateRawUnaligned(
    int size_in_bytes, UpdateSkipList update_skip_list) {
  if (!EnsureLinearAllocationArea(size_in_bytes)) {
    return AllocationResult::Retry(identity());
  }
  HeapObject* object = AllocateLinearly(size_in_bytes);
  DCHECK_NOT_NULL(object);
  if (update_skip_list == UPDATE_SKIP_LIST && identity() == CODE_SPACE) {
    SkipList::Update(object->address(), size_in_bytes);
  }
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
  return object;
}


AllocationResult PagedSpace::AllocateRawAligned(int size_in_bytes,
                                                AllocationAlignment alignment) {
  DCHECK(identity() == OLD_SPACE);
  int allocation_size = size_in_bytes;
  HeapObject* object = TryAllocateLinearlyAligned(&allocation_size, alignment);
  if (object == nullptr) {
    // We don't know exactly how much filler we need to align until space is
    // allocated, so assume the worst case.
    int filler_size = Heap::GetMaximumFillToAlign(alignment);
    allocation_size += filler_size;
    if (!EnsureLinearAllocationArea(allocation_size)) {
      return AllocationResult::Retry(identity());
    }
    allocation_size = size_in_bytes;
    object = TryAllocateLinearlyAligned(&allocation_size, alignment);
    DCHECK_NOT_NULL(object);
  }
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
  return object;
}


AllocationResult PagedSpace::AllocateRaw(int size_in_bytes,
                                         AllocationAlignment alignment) {
  if (top_on_previous_step_ && top() < top_on_previous_step_ &&
      SupportsInlineAllocation()) {
    // Generated code decreased the top() pointer to do folded allocations.
    // The top_on_previous_step_ can be one byte beyond the current page.
    DCHECK_NOT_NULL(top());
    DCHECK_EQ(Page::FromAllocationAreaAddress(top()),
              Page::FromAllocationAreaAddress(top_on_previous_step_ - 1));
    top_on_previous_step_ = top();
  }
  size_t bytes_since_last =
      top_on_previous_step_ ? top() - top_on_previous_step_ : 0;

  DCHECK_IMPLIES(!SupportsInlineAllocation(), bytes_since_last == 0);
#ifdef V8_HOST_ARCH_32_BIT
  AllocationResult result =
      alignment == kDoubleAligned
          ? AllocateRawAligned(size_in_bytes, kDoubleAligned)
          : AllocateRawUnaligned(size_in_bytes);
#else
  AllocationResult result = AllocateRawUnaligned(size_in_bytes);
#endif
  HeapObject* heap_obj = nullptr;
  if (!result.IsRetry() && result.To(&heap_obj) && !is_local()) {
    DCHECK_IMPLIES(
        heap()->incremental_marking()->black_allocation(),
        heap()->incremental_marking()->marking_state()->IsBlack(heap_obj));
    AllocationStep(static_cast<int>(size_in_bytes + bytes_since_last),
                   heap_obj->address(), size_in_bytes);
    StartNextInlineAllocationStep();
  }
  return result;
}


// -----------------------------------------------------------------------------
// NewSpace


AllocationResult NewSpace::AllocateRawAligned(int size_in_bytes,
                                              AllocationAlignment alignment) {
  Address top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(top, alignment);
  int aligned_size_in_bytes = size_in_bytes + filler_size;

  if (allocation_info_.limit() - top < aligned_size_in_bytes) {
    // See if we can create room.
    if (!EnsureAllocation(size_in_bytes, alignment)) {
      return AllocationResult::Retry();
    }

    top = allocation_info_.top();
    filler_size = Heap::GetFillToAlign(top, alignment);
    aligned_size_in_bytes = size_in_bytes + filler_size;
  }

  HeapObject* obj = HeapObject::FromAddress(top);
  allocation_info_.set_top(top + aligned_size_in_bytes);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  if (filler_size > 0) {
    obj = heap()->PrecedeWithFiller(obj, filler_size);
  }

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(obj->address(), size_in_bytes);

  return obj;
}


AllocationResult NewSpace::AllocateRawUnaligned(int size_in_bytes) {
  Address top = allocation_info_.top();
  if (allocation_info_.limit() < top + size_in_bytes) {
    // See if we can create room.
    if (!EnsureAllocation(size_in_bytes, kWordAligned)) {
      return AllocationResult::Retry();
    }

    top = allocation_info_.top();
  }

  HeapObject* obj = HeapObject::FromAddress(top);
  allocation_info_.set_top(top + size_in_bytes);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(obj->address(), size_in_bytes);

  return obj;
}


AllocationResult NewSpace::AllocateRaw(int size_in_bytes,
                                       AllocationAlignment alignment) {
  if (top() < top_on_previous_step_) {
    // Generated code decreased the top() pointer to do folded allocations
    DCHECK_EQ(Page::FromAllocationAreaAddress(top()),
              Page::FromAllocationAreaAddress(top_on_previous_step_));
    top_on_previous_step_ = top();
  }
#ifdef V8_HOST_ARCH_32_BIT
  return alignment == kDoubleAligned
             ? AllocateRawAligned(size_in_bytes, kDoubleAligned)
             : AllocateRawUnaligned(size_in_bytes);
#else
  return AllocateRawUnaligned(size_in_bytes);
#endif
}

V8_WARN_UNUSED_RESULT inline AllocationResult NewSpace::AllocateRawSynchronized(
    int size_in_bytes, AllocationAlignment alignment) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  return AllocateRaw(size_in_bytes, alignment);
}

size_t LargeObjectSpace::Available() {
  return ObjectSizeFor(heap()->memory_allocator()->Available());
}


LocalAllocationBuffer LocalAllocationBuffer::InvalidBuffer() {
  return LocalAllocationBuffer(nullptr, LinearAllocationArea(nullptr, nullptr));
}


LocalAllocationBuffer LocalAllocationBuffer::FromResult(Heap* heap,
                                                        AllocationResult result,
                                                        intptr_t size) {
  if (result.IsRetry()) return InvalidBuffer();
  HeapObject* obj = nullptr;
  bool ok = result.To(&obj);
  USE(ok);
  DCHECK(ok);
  Address top = HeapObject::cast(obj)->address();
  return LocalAllocationBuffer(heap, LinearAllocationArea(top, top + size));
}


bool LocalAllocationBuffer::TryMerge(LocalAllocationBuffer* other) {
  if (allocation_info_.top() == other->allocation_info_.limit()) {
    allocation_info_.set_top(other->allocation_info_.top());
    other->allocation_info_.Reset(nullptr, nullptr);
    return true;
  }
  return false;
}

bool LocalAllocationBuffer::TryFreeLast(HeapObject* object, int object_size) {
  if (IsValid()) {
    const Address object_address = object->address();
    if ((allocation_info_.top() - object_size) == object_address) {
      allocation_info_.set_top(object_address);
      return true;
    }
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_INL_H_
