// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SPACES_INL_H_
#define V8_HEAP_SPACES_INL_H_

#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/isolate.h"
#include "src/msan.h"
#include "src/profiler/heap-profiler.h"
#include "src/v8memory.h"

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

NewSpacePageRange::NewSpacePageRange(Address start, Address limit)
    : range_(Page::FromAddress(start),
             Page::FromAllocationAreaAddress(limit)->next_page()) {
  SemiSpace::AssertValidRange(start, limit);
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
// MemoryAllocator

#ifdef ENABLE_HEAP_PROTECTION

void MemoryAllocator::Protect(Address start, size_t size) {
  base::OS::Protect(start, size);
}


void MemoryAllocator::Unprotect(Address start, size_t size,
                                Executability executable) {
  base::OS::Unprotect(start, size, executable);
}


void MemoryAllocator::ProtectChunkFromPage(Page* page) {
  int id = GetChunkId(page);
  base::OS::Protect(chunks_[id].address(), chunks_[id].size());
}


void MemoryAllocator::UnprotectChunkFromPage(Page* page) {
  int id = GetChunkId(page);
  base::OS::Unprotect(chunks_[id].address(), chunks_[id].size(),
                      chunks_[id].owner()->executable() == EXECUTABLE);
}

#endif

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

Page* Page::Initialize(Heap* heap, MemoryChunk* chunk, Executability executable,
                       SemiSpace* owner) {
  DCHECK_EQ(executable, Executability::NOT_EXECUTABLE);
  bool in_to_space = (owner->id() != kFromSpace);
  chunk->SetFlag(in_to_space ? MemoryChunk::IN_TO_SPACE
                             : MemoryChunk::IN_FROM_SPACE);
  DCHECK(!chunk->IsFlagSet(in_to_space ? MemoryChunk::IN_FROM_SPACE
                                       : MemoryChunk::IN_TO_SPACE));
  Page* page = static_cast<Page*>(chunk);
  heap->incremental_marking()->SetNewSpacePageFlags(page);
  page->AllocateLocalTracker();
  return page;
}

// --------------------------------------------------------------------------
// PagedSpace

template <Page::InitializationMode mode>
Page* Page::Initialize(Heap* heap, MemoryChunk* chunk, Executability executable,
                       PagedSpace* owner) {
  Page* page = reinterpret_cast<Page*>(chunk);
  DCHECK(page->area_size() <= kAllocatableMemory);
  DCHECK(chunk->owner() == owner);

  owner->IncreaseCapacity(page->area_size());
  heap->incremental_marking()->SetOldSpacePageFlags(chunk);

  // Make sure that categories are initialized before freeing the area.
  page->InitializeFreeListCategories();
  // In the case we do not free the memory, we effectively account for the whole
  // page as allocated memory that cannot be used for further allocations.
  if (mode == kFreeMemory) {
    owner->Free(page->area_start(), page->area_size());
  }

  return page;
}

Page* Page::ConvertNewToOld(Page* old_page, PagedSpace* new_owner) {
  DCHECK(old_page->InNewSpace());
  old_page->set_owner(new_owner);
  old_page->SetFlags(0, ~0);
  new_owner->AccountCommitted(old_page->size());
  Page* new_page = Page::Initialize<kDoNotFreeMemory>(
      old_page->heap(), old_page, NOT_EXECUTABLE, new_owner);
  new_page->InsertAfter(new_owner->anchor()->prev_page());
  return new_page;
}

void Page::InitializeFreeListCategories() {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i].Initialize(static_cast<FreeListCategoryType>(i));
  }
}

void MemoryChunk::IncrementLiveBytesFromGC(HeapObject* object, int by) {
  MemoryChunk::FromAddress(object->address())->IncrementLiveBytes(by);
}

void MemoryChunk::ResetLiveBytes() {
  if (FLAG_trace_live_bytes) {
    PrintIsolate(heap()->isolate(), "live-bytes: reset page=%p %d->0\n",
                 static_cast<void*>(this), live_byte_count_);
  }
  live_byte_count_ = 0;
}

void MemoryChunk::IncrementLiveBytes(int by) {
  if (FLAG_trace_live_bytes) {
    PrintIsolate(
        heap()->isolate(), "live-bytes: update page=%p delta=%d %d->%d\n",
        static_cast<void*>(this), by, live_byte_count_, live_byte_count_ + by);
  }
  live_byte_count_ += by;
  DCHECK_GE(live_byte_count_, 0);
  DCHECK_LE(static_cast<size_t>(live_byte_count_), size_);
}

void MemoryChunk::IncrementLiveBytesFromMutator(HeapObject* object, int by) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
  if (!chunk->InNewSpace() && !static_cast<Page*>(chunk)->SweepingDone()) {
    static_cast<PagedSpace*>(chunk->owner())->Allocate(by);
  }
  chunk->IncrementLiveBytes(by);
}

bool PagedSpace::Contains(Address addr) {
  Page* p = Page::FromAddress(addr);
  if (!Page::IsValid(p)) return false;
  return p->owner() == this;
}

bool PagedSpace::Contains(Object* o) {
  if (!o->IsHeapObject()) return false;
  Page* p = Page::FromAddress(HeapObject::cast(o)->address());
  if (!Page::IsValid(p)) return false;
  return p->owner() == this;
}

void PagedSpace::UnlinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  page->ForAllFreeListCategories([this](FreeListCategory* category) {
    DCHECK_EQ(free_list(), category->owner());
    free_list()->RemoveCategory(category);
  });
}

intptr_t PagedSpace::RelinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  intptr_t added = 0;
  page->ForAllFreeListCategories([&added](FreeListCategory* category) {
    added += category->available();
    category->Relink();
  });
  return added;
}

MemoryChunk* MemoryChunk::FromAnyPointerAddress(Heap* heap, Address addr) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(addr);
  uintptr_t offset = addr - chunk->address();
  if (offset < MemoryChunk::kHeaderSize || !chunk->HasPageHeader()) {
    chunk = heap->lo_space()->FindPage(addr);
  }
  return chunk;
}

Page* Page::FromAnyPointerAddress(Heap* heap, Address addr) {
  return static_cast<Page*>(MemoryChunk::FromAnyPointerAddress(heap, addr));
}

void Page::MarkNeverAllocateForTesting() {
  DCHECK(this->owner()->identity() != NEW_SPACE);
  DCHECK(!IsFlagSet(NEVER_ALLOCATE_ON_PAGE));
  SetFlag(NEVER_ALLOCATE_ON_PAGE);
  reinterpret_cast<PagedSpace*>(owner())->free_list()->EvictFreeListItems(this);
}

void Page::MarkEvacuationCandidate() {
  DCHECK(!IsFlagSet(NEVER_EVACUATE));
  DCHECK_NULL(old_to_old_slots_);
  DCHECK_NULL(typed_old_to_old_slots_);
  SetFlag(EVACUATION_CANDIDATE);
  reinterpret_cast<PagedSpace*>(owner())->free_list()->EvictFreeListItems(this);
}

void Page::ClearEvacuationCandidate() {
  if (!IsFlagSet(COMPACTION_WAS_ABORTED)) {
    DCHECK_NULL(old_to_old_slots_);
    DCHECK_NULL(typed_old_to_old_slots_);
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
      // Fall through.
    }
    case kMapState: {
      if (map_iterator_ != heap_->map_space()->end()) return *(map_iterator_++);
      state_ = kCodeState;
      // Fall through.
    }
    case kCodeState: {
      if (code_iterator_ != heap_->code_space()->end())
        return *(code_iterator_++);
      state_ = kLargeObjectState;
      // Fall through.
    }
    case kLargeObjectState: {
      if (lo_iterator_ != heap_->lo_space()->end()) return *(lo_iterator_++);
      state_ = kFinishedState;
      // Fall through;
    }
    case kFinishedState:
      return nullptr;
    default:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

Page* FreeListCategory::page() {
  return Page::FromAddress(reinterpret_cast<Address>(this));
}

FreeList* FreeListCategory::owner() {
  return reinterpret_cast<PagedSpace*>(
             Page::FromAddress(reinterpret_cast<Address>(this))->owner())
      ->free_list();
}

bool FreeListCategory::is_linked() {
  return prev_ != nullptr || next_ != nullptr || owner()->top(type_) == this;
}

// Try linear allocation in the page of alloc_info's allocation top.  Does
// not contain slow case logic (e.g. move to the next page or try free list
// allocation) so it can be used by all the allocation functions and for all
// the paged spaces.
HeapObject* PagedSpace::AllocateLinearly(int size_in_bytes) {
  Address current_top = allocation_info_.top();
  Address new_top = current_top + size_in_bytes;
  if (new_top > allocation_info_.limit()) return NULL;

  allocation_info_.set_top(new_top);
  return HeapObject::FromAddress(current_top);
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


HeapObject* PagedSpace::AllocateLinearlyAligned(int* size_in_bytes,
                                                AllocationAlignment alignment) {
  Address current_top = allocation_info_.top();
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + *size_in_bytes;
  if (new_top > allocation_info_.limit()) return NULL;

  allocation_info_.set_top(new_top);
  if (filler_size > 0) {
    *size_in_bytes += filler_size;
    return heap()->PrecedeWithFiller(HeapObject::FromAddress(current_top),
                                     filler_size);
  }

  return HeapObject::FromAddress(current_top);
}


// Raw allocation.
AllocationResult PagedSpace::AllocateRawUnaligned(
    int size_in_bytes, UpdateSkipList update_skip_list) {
  HeapObject* object = AllocateLinearly(size_in_bytes);

  if (object == NULL) {
    object = free_list_.Allocate(size_in_bytes);
    if (object == NULL) {
      object = SlowAllocateRaw(size_in_bytes);
    }
    if (object != NULL) {
      if (heap()->incremental_marking()->black_allocation()) {
        Marking::MarkBlack(ObjectMarking::MarkBitFrom(object));
        MemoryChunk::IncrementLiveBytesFromGC(object, size_in_bytes);
      }
    }
  }

  if (object != NULL) {
    if (update_skip_list == UPDATE_SKIP_LIST && identity() == CODE_SPACE) {
      SkipList::Update(object->address(), size_in_bytes);
    }
    MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), size_in_bytes);
    return object;
  }

  return AllocationResult::Retry(identity());
}


AllocationResult PagedSpace::AllocateRawUnalignedSynchronized(
    int size_in_bytes) {
  base::LockGuard<base::Mutex> lock_guard(&space_mutex_);
  return AllocateRawUnaligned(size_in_bytes);
}


// Raw allocation.
AllocationResult PagedSpace::AllocateRawAligned(int size_in_bytes,
                                                AllocationAlignment alignment) {
  DCHECK(identity() == OLD_SPACE);
  int allocation_size = size_in_bytes;
  HeapObject* object = AllocateLinearlyAligned(&allocation_size, alignment);

  if (object == NULL) {
    // We don't know exactly how much filler we need to align until space is
    // allocated, so assume the worst case.
    int filler_size = Heap::GetMaximumFillToAlign(alignment);
    allocation_size += filler_size;
    object = free_list_.Allocate(allocation_size);
    if (object == NULL) {
      object = SlowAllocateRaw(allocation_size);
    }
    if (object != NULL && filler_size != 0) {
      object = heap()->AlignWithFiller(object, size_in_bytes, allocation_size,
                                       alignment);
      // Filler objects are initialized, so mark only the aligned object memory
      // as uninitialized.
      allocation_size = size_in_bytes;
    }
  }

  if (object != NULL) {
    MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), allocation_size);
    return object;
  }

  return AllocationResult::Retry(identity());
}


AllocationResult PagedSpace::AllocateRaw(int size_in_bytes,
                                         AllocationAlignment alignment) {
#ifdef V8_HOST_ARCH_32_BIT
  AllocationResult result =
      alignment == kDoubleAligned
          ? AllocateRawAligned(size_in_bytes, kDoubleAligned)
          : AllocateRawUnaligned(size_in_bytes);
#else
  AllocationResult result = AllocateRawUnaligned(size_in_bytes);
#endif
  HeapObject* heap_obj = nullptr;
  if (!result.IsRetry() && result.To(&heap_obj)) {
    AllocationStep(heap_obj->address(), size_in_bytes);
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
#ifdef V8_HOST_ARCH_32_BIT
  return alignment == kDoubleAligned
             ? AllocateRawAligned(size_in_bytes, kDoubleAligned)
             : AllocateRawUnaligned(size_in_bytes);
#else
  return AllocateRawUnaligned(size_in_bytes);
#endif
}


MUST_USE_RESULT inline AllocationResult NewSpace::AllocateRawSynchronized(
    int size_in_bytes, AllocationAlignment alignment) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  return AllocateRaw(size_in_bytes, alignment);
}

LargePage* LargePage::Initialize(Heap* heap, MemoryChunk* chunk,
                                 Executability executable, Space* owner) {
  if (executable && chunk->size() > LargePage::kMaxCodePageSize) {
    STATIC_ASSERT(LargePage::kMaxCodePageSize <= TypedSlotSet::kMaxOffset);
    FATAL("Code page is too large.");
  }
  heap->incremental_marking()->SetOldSpacePageFlags(chunk);
  return static_cast<LargePage*>(chunk);
}


intptr_t LargeObjectSpace::Available() {
  return ObjectSizeFor(heap()->memory_allocator()->Available());
}


LocalAllocationBuffer LocalAllocationBuffer::InvalidBuffer() {
  return LocalAllocationBuffer(nullptr, AllocationInfo(nullptr, nullptr));
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
  return LocalAllocationBuffer(heap, AllocationInfo(top, top + size));
}


bool LocalAllocationBuffer::TryMerge(LocalAllocationBuffer* other) {
  if (allocation_info_.top() == other->allocation_info_.limit()) {
    allocation_info_.set_top(other->allocation_info_.top());
    other->allocation_info_.Reset(nullptr, nullptr);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_INL_H_
