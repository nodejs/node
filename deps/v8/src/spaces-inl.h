// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SPACES_INL_H_
#define V8_SPACES_INL_H_

#include "heap-profiler.h"
#include "isolate.h"
#include "spaces.h"
#include "v8memory.h"

namespace v8 {
namespace internal {


// -----------------------------------------------------------------------------
// Bitmap

void Bitmap::Clear(MemoryChunk* chunk) {
  Bitmap* bitmap = chunk->markbits();
  for (int i = 0; i < bitmap->CellsCount(); i++) bitmap->cells()[i] = 0;
  chunk->ResetLiveBytes();
}


// -----------------------------------------------------------------------------
// PageIterator


PageIterator::PageIterator(PagedSpace* space)
    : space_(space),
      prev_page_(&space->anchor_),
      next_page_(prev_page_->next_page()) { }


bool PageIterator::has_next() {
  return next_page_ != &space_->anchor_;
}


Page* PageIterator::next() {
  ASSERT(has_next());
  prev_page_ = next_page_;
  next_page_ = next_page_->next_page();
  return prev_page_;
}


// -----------------------------------------------------------------------------
// NewSpacePageIterator


NewSpacePageIterator::NewSpacePageIterator(NewSpace* space)
    : prev_page_(NewSpacePage::FromAddress(space->ToSpaceStart())->prev_page()),
      next_page_(NewSpacePage::FromAddress(space->ToSpaceStart())),
      last_page_(NewSpacePage::FromLimit(space->ToSpaceEnd())) { }

NewSpacePageIterator::NewSpacePageIterator(SemiSpace* space)
    : prev_page_(space->anchor()),
      next_page_(prev_page_->next_page()),
      last_page_(prev_page_->prev_page()) { }

NewSpacePageIterator::NewSpacePageIterator(Address start, Address limit)
    : prev_page_(NewSpacePage::FromAddress(start)->prev_page()),
      next_page_(NewSpacePage::FromAddress(start)),
      last_page_(NewSpacePage::FromLimit(limit)) {
  SemiSpace::AssertValidRange(start, limit);
}


bool NewSpacePageIterator::has_next() {
  return prev_page_ != last_page_;
}


NewSpacePage* NewSpacePageIterator::next() {
  ASSERT(has_next());
  prev_page_ = next_page_;
  next_page_ = next_page_->next_page();
  return prev_page_;
}


// -----------------------------------------------------------------------------
// HeapObjectIterator
HeapObject* HeapObjectIterator::FromCurrentPage() {
  while (cur_addr_ != cur_end_) {
    if (cur_addr_ == space_->top() && cur_addr_ != space_->limit()) {
      cur_addr_ = space_->limit();
      continue;
    }
    HeapObject* obj = HeapObject::FromAddress(cur_addr_);
    int obj_size = (size_func_ == NULL) ? obj->Size() : size_func_(obj);
    cur_addr_ += obj_size;
    ASSERT(cur_addr_ <= cur_end_);
    if (!obj->IsFiller()) {
      ASSERT_OBJECT_SIZE(obj_size);
      return obj;
    }
  }
  return NULL;
}


// -----------------------------------------------------------------------------
// MemoryAllocator

#ifdef ENABLE_HEAP_PROTECTION

void MemoryAllocator::Protect(Address start, size_t size) {
  OS::Protect(start, size);
}


void MemoryAllocator::Unprotect(Address start,
                                size_t size,
                                Executability executable) {
  OS::Unprotect(start, size, executable);
}


void MemoryAllocator::ProtectChunkFromPage(Page* page) {
  int id = GetChunkId(page);
  OS::Protect(chunks_[id].address(), chunks_[id].size());
}


void MemoryAllocator::UnprotectChunkFromPage(Page* page) {
  int id = GetChunkId(page);
  OS::Unprotect(chunks_[id].address(), chunks_[id].size(),
                chunks_[id].owner()->executable() == EXECUTABLE);
}

#endif


// --------------------------------------------------------------------------
// PagedSpace
Page* Page::Initialize(Heap* heap,
                       MemoryChunk* chunk,
                       Executability executable,
                       PagedSpace* owner) {
  Page* page = reinterpret_cast<Page*>(chunk);
  ASSERT(page->area_size() <= kMaxRegularHeapObjectSize);
  ASSERT(chunk->owner() == owner);
  owner->IncreaseCapacity(page->area_size());
  owner->Free(page->area_start(), page->area_size());

  heap->incremental_marking()->SetOldSpacePageFlags(chunk);

  return page;
}


bool PagedSpace::Contains(Address addr) {
  Page* p = Page::FromAddress(addr);
  if (!p->is_valid()) return false;
  return p->owner() == this;
}


void MemoryChunk::set_scan_on_scavenge(bool scan) {
  if (scan) {
    if (!scan_on_scavenge()) heap_->increment_scan_on_scavenge_pages();
    SetFlag(SCAN_ON_SCAVENGE);
  } else {
    if (scan_on_scavenge()) heap_->decrement_scan_on_scavenge_pages();
    ClearFlag(SCAN_ON_SCAVENGE);
  }
  heap_->incremental_marking()->SetOldSpacePageFlags(this);
}


MemoryChunk* MemoryChunk::FromAnyPointerAddress(Heap* heap, Address addr) {
  MemoryChunk* maybe = reinterpret_cast<MemoryChunk*>(
      OffsetFrom(addr) & ~Page::kPageAlignmentMask);
  if (maybe->owner() != NULL) return maybe;
  LargeObjectIterator iterator(heap->lo_space());
  for (HeapObject* o = iterator.Next(); o != NULL; o = iterator.Next()) {
    // Fixed arrays are the only pointer-containing objects in large object
    // space.
    if (o->IsFixedArray()) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(o->address());
      if (chunk->Contains(addr)) {
        return chunk;
      }
    }
  }
  UNREACHABLE();
  return NULL;
}


void MemoryChunk::UpdateHighWaterMark(Address mark) {
  if (mark == NULL) return;
  // Need to subtract one from the mark because when a chunk is full the
  // top points to the next address after the chunk, which effectively belongs
  // to another chunk. See the comment to Page::FromAllocationTop.
  MemoryChunk* chunk = MemoryChunk::FromAddress(mark - 1);
  int new_mark = static_cast<int>(mark - chunk->address());
  if (new_mark > chunk->high_water_mark_) {
    chunk->high_water_mark_ = new_mark;
  }
}


PointerChunkIterator::PointerChunkIterator(Heap* heap)
    : state_(kOldPointerState),
      old_pointer_iterator_(heap->old_pointer_space()),
      map_iterator_(heap->map_space()),
      lo_iterator_(heap->lo_space()) { }


Page* Page::next_page() {
  ASSERT(next_chunk()->owner() == owner());
  return static_cast<Page*>(next_chunk());
}


Page* Page::prev_page() {
  ASSERT(prev_chunk()->owner() == owner());
  return static_cast<Page*>(prev_chunk());
}


void Page::set_next_page(Page* page) {
  ASSERT(page->owner() == owner());
  set_next_chunk(page);
}


void Page::set_prev_page(Page* page) {
  ASSERT(page->owner() == owner());
  set_prev_chunk(page);
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


// Raw allocation.
AllocationResult PagedSpace::AllocateRaw(int size_in_bytes) {
  HeapObject* object = AllocateLinearly(size_in_bytes);
  if (object != NULL) {
    if (identity() == CODE_SPACE) {
      SkipList::Update(object->address(), size_in_bytes);
    }
    return object;
  }

  ASSERT(!heap()->linear_allocation() ||
         (anchor_.next_chunk() == &anchor_ &&
          anchor_.prev_chunk() == &anchor_));

  object = free_list_.Allocate(size_in_bytes);
  if (object != NULL) {
    if (identity() == CODE_SPACE) {
      SkipList::Update(object->address(), size_in_bytes);
    }
    return object;
  }

  object = SlowAllocateRaw(size_in_bytes);
  if (object != NULL) {
    if (identity() == CODE_SPACE) {
      SkipList::Update(object->address(), size_in_bytes);
    }
    return object;
  }

  return AllocationResult::Retry(identity());
}


// -----------------------------------------------------------------------------
// NewSpace


AllocationResult NewSpace::AllocateRaw(int size_in_bytes) {
  Address old_top = allocation_info_.top();
#ifdef DEBUG
  // If we are stressing compaction we waste some memory in new space
  // in order to get more frequent GCs.
  if (FLAG_stress_compaction && !heap()->linear_allocation()) {
    if (allocation_info_.limit() - old_top >= size_in_bytes * 4) {
      int filler_size = size_in_bytes * 4;
      for (int i = 0; i < filler_size; i += kPointerSize) {
        *(reinterpret_cast<Object**>(old_top + i)) =
            heap()->one_pointer_filler_map();
      }
      old_top += filler_size;
      allocation_info_.set_top(allocation_info_.top() + filler_size);
    }
  }
#endif

  if (allocation_info_.limit() - old_top < size_in_bytes) {
    return SlowAllocateRaw(size_in_bytes);
  }

  HeapObject* obj = HeapObject::FromAddress(old_top);
  allocation_info_.set_top(allocation_info_.top() + size_in_bytes);
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  return obj;
}


LargePage* LargePage::Initialize(Heap* heap, MemoryChunk* chunk) {
  heap->incremental_marking()->SetOldSpacePageFlags(chunk);
  return static_cast<LargePage*>(chunk);
}


intptr_t LargeObjectSpace::Available() {
  return ObjectSizeFor(heap()->isolate()->memory_allocator()->Available());
}


bool FreeListNode::IsFreeListNode(HeapObject* object) {
  Map* map = object->map();
  Heap* heap = object->GetHeap();
  return map == heap->raw_unchecked_free_space_map()
      || map == heap->raw_unchecked_one_pointer_filler_map()
      || map == heap->raw_unchecked_two_pointer_filler_map();
}

} }  // namespace v8::internal

#endif  // V8_SPACES_INL_H_
