// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "macro-assembler.h"
#include "mark-compact.h"
#include "platform.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// HeapObjectIterator

HeapObjectIterator::HeapObjectIterator(PagedSpace* space) {
  // You can't actually iterate over the anchor page.  It is not a real page,
  // just an anchor for the double linked page list.  Initialize as if we have
  // reached the end of the anchor page, then the first iteration will move on
  // to the first page.
  Initialize(space,
             NULL,
             NULL,
             kAllPagesInSpace,
             NULL);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space,
                                       HeapObjectCallback size_func) {
  // You can't actually iterate over the anchor page.  It is not a real page,
  // just an anchor for the double linked page list.  Initialize the current
  // address and end as NULL, then the first iteration will move on
  // to the first page.
  Initialize(space,
             NULL,
             NULL,
             kAllPagesInSpace,
             size_func);
}


HeapObjectIterator::HeapObjectIterator(Page* page,
                                       HeapObjectCallback size_func) {
  Space* owner = page->owner();
  ASSERT(owner == page->heap()->old_pointer_space() ||
         owner == page->heap()->old_data_space() ||
         owner == page->heap()->map_space() ||
         owner == page->heap()->cell_space() ||
         owner == page->heap()->property_cell_space() ||
         owner == page->heap()->code_space());
  Initialize(reinterpret_cast<PagedSpace*>(owner),
             page->area_start(),
             page->area_end(),
             kOnePageOnly,
             size_func);
  ASSERT(page->WasSweptPrecisely());
}


void HeapObjectIterator::Initialize(PagedSpace* space,
                                    Address cur, Address end,
                                    HeapObjectIterator::PageMode mode,
                                    HeapObjectCallback size_f) {
  // Check that we actually can iterate this space.
  ASSERT(!space->was_swept_conservatively());

  space_ = space;
  cur_addr_ = cur;
  cur_end_ = end;
  page_mode_ = mode;
  size_func_ = size_f;
}


// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool HeapObjectIterator::AdvanceToNextPage() {
  ASSERT(cur_addr_ == cur_end_);
  if (page_mode_ == kOnePageOnly) return false;
  Page* cur_page;
  if (cur_addr_ == NULL) {
    cur_page = space_->anchor();
  } else {
    cur_page = Page::FromAddress(cur_addr_ - 1);
    ASSERT(cur_addr_ == cur_page->area_end());
  }
  cur_page = cur_page->next_page();
  if (cur_page == space_->anchor()) return false;
  cur_addr_ = cur_page->area_start();
  cur_end_ = cur_page->area_end();
  ASSERT(cur_page->WasSweptPrecisely());
  return true;
}


// -----------------------------------------------------------------------------
// CodeRange


CodeRange::CodeRange(Isolate* isolate)
    : isolate_(isolate),
      code_range_(NULL),
      free_list_(0),
      allocation_list_(0),
      current_allocation_block_index_(0) {
}


bool CodeRange::SetUp(const size_t requested) {
  ASSERT(code_range_ == NULL);

  code_range_ = new VirtualMemory(requested);
  CHECK(code_range_ != NULL);
  if (!code_range_->IsReserved()) {
    delete code_range_;
    code_range_ = NULL;
    return false;
  }

  // We are sure that we have mapped a block of requested addresses.
  ASSERT(code_range_->size() == requested);
  LOG(isolate_, NewEvent("CodeRange", code_range_->address(), requested));
  Address base = reinterpret_cast<Address>(code_range_->address());
  Address aligned_base =
      RoundUp(reinterpret_cast<Address>(code_range_->address()),
              MemoryChunk::kAlignment);
  size_t size = code_range_->size() - (aligned_base - base);
  allocation_list_.Add(FreeBlock(aligned_base, size));
  current_allocation_block_index_ = 0;
  return true;
}


int CodeRange::CompareFreeBlockAddress(const FreeBlock* left,
                                       const FreeBlock* right) {
  // The entire point of CodeRange is that the difference between two
  // addresses in the range can be represented as a signed 32-bit int,
  // so the cast is semantically correct.
  return static_cast<int>(left->start - right->start);
}


void CodeRange::GetNextAllocationBlock(size_t requested) {
  for (current_allocation_block_index_++;
       current_allocation_block_index_ < allocation_list_.length();
       current_allocation_block_index_++) {
    if (requested <= allocation_list_[current_allocation_block_index_].size) {
      return;  // Found a large enough allocation block.
    }
  }

  // Sort and merge the free blocks on the free list and the allocation list.
  free_list_.AddAll(allocation_list_);
  allocation_list_.Clear();
  free_list_.Sort(&CompareFreeBlockAddress);
  for (int i = 0; i < free_list_.length();) {
    FreeBlock merged = free_list_[i];
    i++;
    // Add adjacent free blocks to the current merged block.
    while (i < free_list_.length() &&
           free_list_[i].start == merged.start + merged.size) {
      merged.size += free_list_[i].size;
      i++;
    }
    if (merged.size > 0) {
      allocation_list_.Add(merged);
    }
  }
  free_list_.Clear();

  for (current_allocation_block_index_ = 0;
       current_allocation_block_index_ < allocation_list_.length();
       current_allocation_block_index_++) {
    if (requested <= allocation_list_[current_allocation_block_index_].size) {
      return;  // Found a large enough allocation block.
    }
  }

  // Code range is full or too fragmented.
  V8::FatalProcessOutOfMemory("CodeRange::GetNextAllocationBlock");
}


Address CodeRange::AllocateRawMemory(const size_t requested_size,
                                     const size_t commit_size,
                                     size_t* allocated) {
  ASSERT(commit_size <= requested_size);
  ASSERT(current_allocation_block_index_ < allocation_list_.length());
  if (requested_size > allocation_list_[current_allocation_block_index_].size) {
    // Find an allocation block large enough.  This function call may
    // call V8::FatalProcessOutOfMemory if it cannot find a large enough block.
    GetNextAllocationBlock(requested_size);
  }
  // Commit the requested memory at the start of the current allocation block.
  size_t aligned_requested = RoundUp(requested_size, MemoryChunk::kAlignment);
  FreeBlock current = allocation_list_[current_allocation_block_index_];
  if (aligned_requested >= (current.size - Page::kPageSize)) {
    // Don't leave a small free block, useless for a large object or chunk.
    *allocated = current.size;
  } else {
    *allocated = aligned_requested;
  }
  ASSERT(*allocated <= current.size);
  ASSERT(IsAddressAligned(current.start, MemoryChunk::kAlignment));
  if (!MemoryAllocator::CommitExecutableMemory(code_range_,
                                               current.start,
                                               commit_size,
                                               *allocated)) {
    *allocated = 0;
    return NULL;
  }
  allocation_list_[current_allocation_block_index_].start += *allocated;
  allocation_list_[current_allocation_block_index_].size -= *allocated;
  if (*allocated == current.size) {
    GetNextAllocationBlock(0);  // This block is used up, get the next one.
  }
  return current.start;
}


bool CodeRange::CommitRawMemory(Address start, size_t length) {
  return code_range_->Commit(start, length, true);
}


bool CodeRange::UncommitRawMemory(Address start, size_t length) {
  return code_range_->Uncommit(start, length);
}


void CodeRange::FreeRawMemory(Address address, size_t length) {
  ASSERT(IsAddressAligned(address, MemoryChunk::kAlignment));
  free_list_.Add(FreeBlock(address, length));
  code_range_->Uncommit(address, length);
}


void CodeRange::TearDown() {
    delete code_range_;  // Frees all memory in the virtual memory range.
    code_range_ = NULL;
    free_list_.Free();
    allocation_list_.Free();
}


// -----------------------------------------------------------------------------
// MemoryAllocator
//

MemoryAllocator::MemoryAllocator(Isolate* isolate)
    : isolate_(isolate),
      capacity_(0),
      capacity_executable_(0),
      size_(0),
      size_executable_(0) {
}


bool MemoryAllocator::SetUp(intptr_t capacity, intptr_t capacity_executable) {
  capacity_ = RoundUp(capacity, Page::kPageSize);
  capacity_executable_ = RoundUp(capacity_executable, Page::kPageSize);
  ASSERT_GE(capacity_, capacity_executable_);

  size_ = 0;
  size_executable_ = 0;

  return true;
}


void MemoryAllocator::TearDown() {
  // Check that spaces were torn down before MemoryAllocator.
  ASSERT(size_ == 0);
  // TODO(gc) this will be true again when we fix FreeMemory.
  // ASSERT(size_executable_ == 0);
  capacity_ = 0;
  capacity_executable_ = 0;
}


void MemoryAllocator::FreeMemory(VirtualMemory* reservation,
                                 Executability executable) {
  // TODO(gc) make code_range part of memory allocator?
  ASSERT(reservation->IsReserved());
  size_t size = reservation->size();
  ASSERT(size_ >= size);
  size_ -= size;

  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));

  if (executable == EXECUTABLE) {
    ASSERT(size_executable_ >= size);
    size_executable_ -= size;
  }
  // Code which is part of the code-range does not have its own VirtualMemory.
  ASSERT(!isolate_->code_range()->contains(
      static_cast<Address>(reservation->address())));
  ASSERT(executable == NOT_EXECUTABLE || !isolate_->code_range()->exists());
  reservation->Release();
}


void MemoryAllocator::FreeMemory(Address base,
                                 size_t size,
                                 Executability executable) {
  // TODO(gc) make code_range part of memory allocator?
  ASSERT(size_ >= size);
  size_ -= size;

  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));

  if (executable == EXECUTABLE) {
    ASSERT(size_executable_ >= size);
    size_executable_ -= size;
  }
  if (isolate_->code_range()->contains(static_cast<Address>(base))) {
    ASSERT(executable == EXECUTABLE);
    isolate_->code_range()->FreeRawMemory(base, size);
  } else {
    ASSERT(executable == NOT_EXECUTABLE || !isolate_->code_range()->exists());
    bool result = VirtualMemory::ReleaseRegion(base, size);
    USE(result);
    ASSERT(result);
  }
}


Address MemoryAllocator::ReserveAlignedMemory(size_t size,
                                              size_t alignment,
                                              VirtualMemory* controller) {
  VirtualMemory reservation(size, alignment);

  if (!reservation.IsReserved()) return NULL;
  size_ += reservation.size();
  Address base = RoundUp(static_cast<Address>(reservation.address()),
                         alignment);
  controller->TakeControl(&reservation);
  return base;
}


Address MemoryAllocator::AllocateAlignedMemory(size_t reserve_size,
                                               size_t commit_size,
                                               size_t alignment,
                                               Executability executable,
                                               VirtualMemory* controller) {
  ASSERT(commit_size <= reserve_size);
  VirtualMemory reservation;
  Address base = ReserveAlignedMemory(reserve_size, alignment, &reservation);
  if (base == NULL) return NULL;

  if (executable == EXECUTABLE) {
    if (!CommitExecutableMemory(&reservation,
                                base,
                                commit_size,
                                reserve_size)) {
      base = NULL;
    }
  } else {
    if (!reservation.Commit(base, commit_size, false)) {
      base = NULL;
    }
  }

  if (base == NULL) {
    // Failed to commit the body. Release the mapping and any partially
    // commited regions inside it.
    reservation.Release();
    return NULL;
  }

  controller->TakeControl(&reservation);
  return base;
}


void Page::InitializeAsAnchor(PagedSpace* owner) {
  set_owner(owner);
  set_prev_page(this);
  set_next_page(this);
}


NewSpacePage* NewSpacePage::Initialize(Heap* heap,
                                       Address start,
                                       SemiSpace* semi_space) {
  Address area_start = start + NewSpacePage::kObjectStartOffset;
  Address area_end = start + Page::kPageSize;

  MemoryChunk* chunk = MemoryChunk::Initialize(heap,
                                               start,
                                               Page::kPageSize,
                                               area_start,
                                               area_end,
                                               NOT_EXECUTABLE,
                                               semi_space);
  chunk->set_next_chunk(NULL);
  chunk->set_prev_chunk(NULL);
  chunk->initialize_scan_on_scavenge(true);
  bool in_to_space = (semi_space->id() != kFromSpace);
  chunk->SetFlag(in_to_space ? MemoryChunk::IN_TO_SPACE
                             : MemoryChunk::IN_FROM_SPACE);
  ASSERT(!chunk->IsFlagSet(in_to_space ? MemoryChunk::IN_FROM_SPACE
                                       : MemoryChunk::IN_TO_SPACE));
  NewSpacePage* page = static_cast<NewSpacePage*>(chunk);
  heap->incremental_marking()->SetNewSpacePageFlags(page);
  return page;
}


void NewSpacePage::InitializeAsAnchor(SemiSpace* semi_space) {
  set_owner(semi_space);
  set_next_chunk(this);
  set_prev_chunk(this);
  // Flags marks this invalid page as not being in new-space.
  // All real new-space pages will be in new-space.
  SetFlags(0, ~0);
}


MemoryChunk* MemoryChunk::Initialize(Heap* heap,
                                     Address base,
                                     size_t size,
                                     Address area_start,
                                     Address area_end,
                                     Executability executable,
                                     Space* owner) {
  MemoryChunk* chunk = FromAddress(base);

  ASSERT(base == chunk->address());

  chunk->heap_ = heap;
  chunk->size_ = size;
  chunk->area_start_ = area_start;
  chunk->area_end_ = area_end;
  chunk->flags_ = 0;
  chunk->set_owner(owner);
  chunk->InitializeReservedMemory();
  chunk->slots_buffer_ = NULL;
  chunk->skip_list_ = NULL;
  chunk->write_barrier_counter_ = kWriteBarrierCounterGranularity;
  chunk->progress_bar_ = 0;
  chunk->high_water_mark_ = static_cast<int>(area_start - base);
  chunk->parallel_sweeping_ = 0;
  chunk->available_in_small_free_list_ = 0;
  chunk->available_in_medium_free_list_ = 0;
  chunk->available_in_large_free_list_ = 0;
  chunk->available_in_huge_free_list_ = 0;
  chunk->non_available_small_blocks_ = 0;
  chunk->ResetLiveBytes();
  Bitmap::Clear(chunk);
  chunk->initialize_scan_on_scavenge(false);
  chunk->SetFlag(WAS_SWEPT_PRECISELY);

  ASSERT(OFFSET_OF(MemoryChunk, flags_) == kFlagsOffset);
  ASSERT(OFFSET_OF(MemoryChunk, live_byte_count_) == kLiveBytesOffset);

  if (executable == EXECUTABLE) {
    chunk->SetFlag(IS_EXECUTABLE);
  }

  if (owner == heap->old_data_space()) {
    chunk->SetFlag(CONTAINS_ONLY_DATA);
  }

  return chunk;
}


// Commit MemoryChunk area to the requested size.
bool MemoryChunk::CommitArea(size_t requested) {
  size_t guard_size = IsFlagSet(IS_EXECUTABLE) ?
                      MemoryAllocator::CodePageGuardSize() : 0;
  size_t header_size = area_start() - address() - guard_size;
  size_t commit_size = RoundUp(header_size + requested, OS::CommitPageSize());
  size_t committed_size = RoundUp(header_size + (area_end() - area_start()),
                                  OS::CommitPageSize());

  if (commit_size > committed_size) {
    // Commit size should be less or equal than the reserved size.
    ASSERT(commit_size <= size() - 2 * guard_size);
    // Append the committed area.
    Address start = address() + committed_size + guard_size;
    size_t length = commit_size - committed_size;
    if (reservation_.IsReserved()) {
      if (!reservation_.Commit(start, length, IsFlagSet(IS_EXECUTABLE))) {
        return false;
      }
    } else {
      CodeRange* code_range = heap_->isolate()->code_range();
      ASSERT(code_range->exists() && IsFlagSet(IS_EXECUTABLE));
      if (!code_range->CommitRawMemory(start, length)) return false;
    }

    if (Heap::ShouldZapGarbage()) {
      heap_->isolate()->memory_allocator()->ZapBlock(start, length);
    }
  } else if (commit_size < committed_size) {
    ASSERT(commit_size > 0);
    // Shrink the committed area.
    size_t length = committed_size - commit_size;
    Address start = address() + committed_size + guard_size - length;
    if (reservation_.IsReserved()) {
      if (!reservation_.Uncommit(start, length)) return false;
    } else {
      CodeRange* code_range = heap_->isolate()->code_range();
      ASSERT(code_range->exists() && IsFlagSet(IS_EXECUTABLE));
      if (!code_range->UncommitRawMemory(start, length)) return false;
    }
  }

  area_end_ = area_start_ + requested;
  return true;
}


void MemoryChunk::InsertAfter(MemoryChunk* other) {
  next_chunk_ = other->next_chunk_;
  prev_chunk_ = other;

  // This memory barrier is needed since concurrent sweeper threads may iterate
  // over the list of pages while a new page is inserted.
  // TODO(hpayer): find a cleaner way to guarantee that the page list can be
  // expanded concurrently
  MemoryBarrier();

  // The following two write operations can take effect in arbitrary order
  // since pages are always iterated by the sweeper threads in LIFO order, i.e,
  // the inserted page becomes visible for the sweeper threads after
  // other->next_chunk_ = this;
  other->next_chunk_->prev_chunk_ = this;
  other->next_chunk_ = this;
}


void MemoryChunk::Unlink() {
  if (!InNewSpace() && IsFlagSet(SCAN_ON_SCAVENGE)) {
    heap_->decrement_scan_on_scavenge_pages();
    ClearFlag(SCAN_ON_SCAVENGE);
  }
  next_chunk_->prev_chunk_ = prev_chunk_;
  prev_chunk_->next_chunk_ = next_chunk_;
  prev_chunk_ = NULL;
  next_chunk_ = NULL;
}


MemoryChunk* MemoryAllocator::AllocateChunk(intptr_t reserve_area_size,
                                            intptr_t commit_area_size,
                                            Executability executable,
                                            Space* owner) {
  ASSERT(commit_area_size <= reserve_area_size);

  size_t chunk_size;
  Heap* heap = isolate_->heap();
  Address base = NULL;
  VirtualMemory reservation;
  Address area_start = NULL;
  Address area_end = NULL;

  //
  // MemoryChunk layout:
  //
  //             Executable
  // +----------------------------+<- base aligned with MemoryChunk::kAlignment
  // |           Header           |
  // +----------------------------+<- base + CodePageGuardStartOffset
  // |           Guard            |
  // +----------------------------+<- area_start_
  // |           Area             |
  // +----------------------------+<- area_end_ (area_start + commit_area_size)
  // |   Committed but not used   |
  // +----------------------------+<- aligned at OS page boundary
  // | Reserved but not committed |
  // +----------------------------+<- aligned at OS page boundary
  // |           Guard            |
  // +----------------------------+<- base + chunk_size
  //
  //           Non-executable
  // +----------------------------+<- base aligned with MemoryChunk::kAlignment
  // |          Header            |
  // +----------------------------+<- area_start_ (base + kObjectStartOffset)
  // |           Area             |
  // +----------------------------+<- area_end_ (area_start + commit_area_size)
  // |  Committed but not used    |
  // +----------------------------+<- aligned at OS page boundary
  // | Reserved but not committed |
  // +----------------------------+<- base + chunk_size
  //

  if (executable == EXECUTABLE) {
    chunk_size = RoundUp(CodePageAreaStartOffset() + reserve_area_size,
                         OS::CommitPageSize()) + CodePageGuardSize();

    // Check executable memory limit.
    if (size_executable_ + chunk_size > capacity_executable_) {
      LOG(isolate_,
          StringEvent("MemoryAllocator::AllocateRawMemory",
                      "V8 Executable Allocation capacity exceeded"));
      return NULL;
    }

    // Size of header (not executable) plus area (executable).
    size_t commit_size = RoundUp(CodePageGuardStartOffset() + commit_area_size,
                                 OS::CommitPageSize());
    // Allocate executable memory either from code range or from the
    // OS.
    if (isolate_->code_range()->exists()) {
      base = isolate_->code_range()->AllocateRawMemory(chunk_size,
                                                       commit_size,
                                                       &chunk_size);
      ASSERT(IsAligned(reinterpret_cast<intptr_t>(base),
                       MemoryChunk::kAlignment));
      if (base == NULL) return NULL;
      size_ += chunk_size;
      // Update executable memory size.
      size_executable_ += chunk_size;
    } else {
      base = AllocateAlignedMemory(chunk_size,
                                   commit_size,
                                   MemoryChunk::kAlignment,
                                   executable,
                                   &reservation);
      if (base == NULL) return NULL;
      // Update executable memory size.
      size_executable_ += reservation.size();
    }

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(base, CodePageGuardStartOffset());
      ZapBlock(base + CodePageAreaStartOffset(), commit_area_size);
    }

    area_start = base + CodePageAreaStartOffset();
    area_end = area_start + commit_area_size;
  } else {
    chunk_size = RoundUp(MemoryChunk::kObjectStartOffset + reserve_area_size,
                         OS::CommitPageSize());
    size_t commit_size = RoundUp(MemoryChunk::kObjectStartOffset +
                                 commit_area_size, OS::CommitPageSize());
    base = AllocateAlignedMemory(chunk_size,
                                 commit_size,
                                 MemoryChunk::kAlignment,
                                 executable,
                                 &reservation);

    if (base == NULL) return NULL;

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(base, Page::kObjectStartOffset + commit_area_size);
    }

    area_start = base + Page::kObjectStartOffset;
    area_end = area_start + commit_area_size;
  }

  // Use chunk_size for statistics and callbacks because we assume that they
  // treat reserved but not-yet committed memory regions of chunks as allocated.
  isolate_->counters()->memory_allocated()->
      Increment(static_cast<int>(chunk_size));

  LOG(isolate_, NewEvent("MemoryChunk", base, chunk_size));
  if (owner != NULL) {
    ObjectSpace space = static_cast<ObjectSpace>(1 << owner->identity());
    PerformAllocationCallback(space, kAllocationActionAllocate, chunk_size);
  }

  MemoryChunk* result = MemoryChunk::Initialize(heap,
                                                base,
                                                chunk_size,
                                                area_start,
                                                area_end,
                                                executable,
                                                owner);
  result->set_reserved_memory(&reservation);
  return result;
}


void Page::ResetFreeListStatistics() {
  non_available_small_blocks_ = 0;
  available_in_small_free_list_ = 0;
  available_in_medium_free_list_ = 0;
  available_in_large_free_list_ = 0;
  available_in_huge_free_list_ = 0;
}


Page* MemoryAllocator::AllocatePage(intptr_t size,
                                    PagedSpace* owner,
                                    Executability executable) {
  MemoryChunk* chunk = AllocateChunk(size, size, executable, owner);

  if (chunk == NULL) return NULL;

  return Page::Initialize(isolate_->heap(), chunk, executable, owner);
}


LargePage* MemoryAllocator::AllocateLargePage(intptr_t object_size,
                                              Space* owner,
                                              Executability executable) {
  MemoryChunk* chunk = AllocateChunk(object_size,
                                     object_size,
                                     executable,
                                     owner);
  if (chunk == NULL) return NULL;
  return LargePage::Initialize(isolate_->heap(), chunk);
}


void MemoryAllocator::Free(MemoryChunk* chunk) {
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));
  if (chunk->owner() != NULL) {
    ObjectSpace space =
        static_cast<ObjectSpace>(1 << chunk->owner()->identity());
    PerformAllocationCallback(space, kAllocationActionFree, chunk->size());
  }

  isolate_->heap()->RememberUnmappedPage(
      reinterpret_cast<Address>(chunk), chunk->IsEvacuationCandidate());

  delete chunk->slots_buffer();
  delete chunk->skip_list();

  VirtualMemory* reservation = chunk->reserved_memory();
  if (reservation->IsReserved()) {
    FreeMemory(reservation, chunk->executable());
  } else {
    FreeMemory(chunk->address(),
               chunk->size(),
               chunk->executable());
  }
}


bool MemoryAllocator::CommitBlock(Address start,
                                  size_t size,
                                  Executability executable) {
  if (!VirtualMemory::CommitRegion(start, size, executable)) return false;

  if (Heap::ShouldZapGarbage()) {
    ZapBlock(start, size);
  }

  isolate_->counters()->memory_allocated()->Increment(static_cast<int>(size));
  return true;
}


bool MemoryAllocator::UncommitBlock(Address start, size_t size) {
  if (!VirtualMemory::UncommitRegion(start, size)) return false;
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));
  return true;
}


void MemoryAllocator::ZapBlock(Address start, size_t size) {
  for (size_t s = 0; s + kPointerSize <= size; s += kPointerSize) {
    Memory::Address_at(start + s) = kZapValue;
  }
}


void MemoryAllocator::PerformAllocationCallback(ObjectSpace space,
                                                AllocationAction action,
                                                size_t size) {
  for (int i = 0; i < memory_allocation_callbacks_.length(); ++i) {
    MemoryAllocationCallbackRegistration registration =
      memory_allocation_callbacks_[i];
    if ((registration.space & space) == space &&
        (registration.action & action) == action)
      registration.callback(space, action, static_cast<int>(size));
  }
}


bool MemoryAllocator::MemoryAllocationCallbackRegistered(
    MemoryAllocationCallback callback) {
  for (int i = 0; i < memory_allocation_callbacks_.length(); ++i) {
    if (memory_allocation_callbacks_[i].callback == callback) return true;
  }
  return false;
}


void MemoryAllocator::AddMemoryAllocationCallback(
    MemoryAllocationCallback callback,
    ObjectSpace space,
    AllocationAction action) {
  ASSERT(callback != NULL);
  MemoryAllocationCallbackRegistration registration(callback, space, action);
  ASSERT(!MemoryAllocator::MemoryAllocationCallbackRegistered(callback));
  return memory_allocation_callbacks_.Add(registration);
}


void MemoryAllocator::RemoveMemoryAllocationCallback(
     MemoryAllocationCallback callback) {
  ASSERT(callback != NULL);
  for (int i = 0; i < memory_allocation_callbacks_.length(); ++i) {
    if (memory_allocation_callbacks_[i].callback == callback) {
      memory_allocation_callbacks_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}


#ifdef DEBUG
void MemoryAllocator::ReportStatistics() {
  float pct = static_cast<float>(capacity_ - size_) / capacity_;
  PrintF("  capacity: %" V8_PTR_PREFIX "d"
             ", used: %" V8_PTR_PREFIX "d"
             ", available: %%%d\n\n",
         capacity_, size_, static_cast<int>(pct*100));
}
#endif


int MemoryAllocator::CodePageGuardStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return RoundUp(Page::kObjectStartOffset, OS::CommitPageSize());
}


int MemoryAllocator::CodePageGuardSize() {
  return static_cast<int>(OS::CommitPageSize());
}


int MemoryAllocator::CodePageAreaStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return CodePageGuardStartOffset() + CodePageGuardSize();
}


int MemoryAllocator::CodePageAreaEndOffset() {
  // We are guarding code pages: the last OS page will be protected as
  // non-writable.
  return Page::kPageSize - static_cast<int>(OS::CommitPageSize());
}


bool MemoryAllocator::CommitExecutableMemory(VirtualMemory* vm,
                                             Address start,
                                             size_t commit_size,
                                             size_t reserved_size) {
  // Commit page header (not executable).
  if (!vm->Commit(start,
                  CodePageGuardStartOffset(),
                  false)) {
    return false;
  }

  // Create guard page after the header.
  if (!vm->Guard(start + CodePageGuardStartOffset())) {
    return false;
  }

  // Commit page body (executable).
  if (!vm->Commit(start + CodePageAreaStartOffset(),
                  commit_size - CodePageGuardStartOffset(),
                  true)) {
    return false;
  }

  // Create guard page before the end.
  if (!vm->Guard(start + reserved_size - CodePageGuardSize())) {
    return false;
  }

  return true;
}


// -----------------------------------------------------------------------------
// MemoryChunk implementation

void MemoryChunk::IncrementLiveBytesFromMutator(Address address, int by) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(address);
  if (!chunk->InNewSpace() && !static_cast<Page*>(chunk)->WasSwept()) {
    static_cast<PagedSpace*>(chunk->owner())->IncrementUnsweptFreeBytes(-by);
  }
  chunk->IncrementLiveBytes(by);
}

// -----------------------------------------------------------------------------
// PagedSpace implementation

PagedSpace::PagedSpace(Heap* heap,
                       intptr_t max_capacity,
                       AllocationSpace id,
                       Executability executable)
    : Space(heap, id, executable),
      free_list_(this),
      was_swept_conservatively_(false),
      first_unswept_page_(Page::FromAddress(NULL)),
      unswept_free_bytes_(0) {
  if (id == CODE_SPACE) {
    area_size_ = heap->isolate()->memory_allocator()->
        CodePageAreaSize();
  } else {
    area_size_ = Page::kPageSize - Page::kObjectStartOffset;
  }
  max_capacity_ = (RoundDown(max_capacity, Page::kPageSize) / Page::kPageSize)
      * AreaSize();
  accounting_stats_.Clear();

  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;

  anchor_.InitializeAsAnchor(this);
}


bool PagedSpace::SetUp() {
  return true;
}


bool PagedSpace::HasBeenSetUp() {
  return true;
}


void PagedSpace::TearDown() {
  PageIterator iterator(this);
  while (iterator.has_next()) {
    heap()->isolate()->memory_allocator()->Free(iterator.next());
  }
  anchor_.set_next_page(&anchor_);
  anchor_.set_prev_page(&anchor_);
  accounting_stats_.Clear();
}


size_t PagedSpace::CommittedPhysicalMemory() {
  if (!VirtualMemory::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top);
  size_t size = 0;
  PageIterator it(this);
  while (it.has_next()) {
    size += it.next()->CommittedPhysicalMemory();
  }
  return size;
}


MaybeObject* PagedSpace::FindObject(Address addr) {
  // Note: this function can only be called on precisely swept spaces.
  ASSERT(!heap()->mark_compact_collector()->in_use());

  if (!Contains(addr)) return Failure::Exception();

  Page* p = Page::FromAddress(addr);
  HeapObjectIterator it(p, NULL);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    Address cur = obj->address();
    Address next = cur + obj->Size();
    if ((cur <= addr) && (addr < next)) return obj;
  }

  UNREACHABLE();
  return Failure::Exception();
}

bool PagedSpace::CanExpand() {
  ASSERT(max_capacity_ % AreaSize() == 0);

  if (Capacity() == max_capacity_) return false;

  ASSERT(Capacity() < max_capacity_);

  // Are we going to exceed capacity for this space?
  if ((Capacity() + Page::kPageSize) > max_capacity_) return false;

  return true;
}


bool PagedSpace::Expand() {
  if (!CanExpand()) return false;

  intptr_t size = AreaSize();

  if (anchor_.next_page() == &anchor_) {
    size = SizeOfFirstPage();
  }

  Page* p = heap()->isolate()->memory_allocator()->AllocatePage(
      size, this, executable());
  if (p == NULL) return false;

  ASSERT(Capacity() <= max_capacity_);

  p->InsertAfter(anchor_.prev_page());

  return true;
}


intptr_t PagedSpace::SizeOfFirstPage() {
  int size = 0;
  switch (identity()) {
    case OLD_POINTER_SPACE:
      size = 64 * kPointerSize * KB;
      break;
    case OLD_DATA_SPACE:
      size = 192 * KB;
      break;
    case MAP_SPACE:
      size = 16 * kPointerSize * KB;
      break;
    case CELL_SPACE:
      size = 16 * kPointerSize * KB;
      break;
    case PROPERTY_CELL_SPACE:
      size = 8 * kPointerSize * KB;
      break;
    case CODE_SPACE:
      if (heap()->isolate()->code_range()->exists()) {
        // When code range exists, code pages are allocated in a special way
        // (from the reserved code range). That part of the code is not yet
        // upgraded to handle small pages.
        size = AreaSize();
      } else {
        size = 384 * KB;
      }
      break;
    default:
      UNREACHABLE();
  }
  return Min(size, AreaSize());
}


int PagedSpace::CountTotalPages() {
  PageIterator it(this);
  int count = 0;
  while (it.has_next()) {
    it.next();
    count++;
  }
  return count;
}


void PagedSpace::ObtainFreeListStatistics(Page* page, SizeStats* sizes) {
  sizes->huge_size_ = page->available_in_huge_free_list();
  sizes->small_size_ = page->available_in_small_free_list();
  sizes->medium_size_ = page->available_in_medium_free_list();
  sizes->large_size_ = page->available_in_large_free_list();
}


void PagedSpace::ResetFreeListStatistics() {
  PageIterator page_iterator(this);
  while (page_iterator.has_next()) {
    Page* page = page_iterator.next();
    page->ResetFreeListStatistics();
  }
}


void PagedSpace::ReleasePage(Page* page, bool unlink) {
  ASSERT(page->LiveBytes() == 0);
  ASSERT(AreaSize() == page->area_size());

  // Adjust list of unswept pages if the page is the head of the list.
  if (first_unswept_page_ == page) {
    first_unswept_page_ = page->next_page();
    if (first_unswept_page_ == anchor()) {
      first_unswept_page_ = Page::FromAddress(NULL);
    }
  }

  if (page->WasSwept()) {
    intptr_t size = free_list_.EvictFreeListItems(page);
    accounting_stats_.AllocateBytes(size);
    ASSERT_EQ(AreaSize(), static_cast<int>(size));
  } else {
    DecreaseUnsweptFreeBytes(page);
  }

  if (Page::FromAllocationTop(allocation_info_.top) == page) {
    allocation_info_.top = allocation_info_.limit = NULL;
  }

  if (unlink) {
    page->Unlink();
  }
  if (page->IsFlagSet(MemoryChunk::CONTAINS_ONLY_DATA)) {
    heap()->isolate()->memory_allocator()->Free(page);
  } else {
    heap()->QueueMemoryChunkForFree(page);
  }

  ASSERT(Capacity() > 0);
  accounting_stats_.ShrinkSpace(AreaSize());
}


#ifdef DEBUG
void PagedSpace::Print() { }
#endif

#ifdef VERIFY_HEAP
void PagedSpace::Verify(ObjectVisitor* visitor) {
  // We can only iterate over the pages if they were swept precisely.
  if (was_swept_conservatively_) return;

  bool allocation_pointer_found_in_space =
      (allocation_info_.top == allocation_info_.limit);
  PageIterator page_iterator(this);
  while (page_iterator.has_next()) {
    Page* page = page_iterator.next();
    CHECK(page->owner() == this);
    if (page == Page::FromAllocationTop(allocation_info_.top)) {
      allocation_pointer_found_in_space = true;
    }
    CHECK(page->WasSweptPrecisely());
    HeapObjectIterator it(page, NULL);
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();
    int black_size = 0;
    for (HeapObject* object = it.Next(); object != NULL; object = it.Next()) {
      CHECK(end_of_previous_object <= object->address());

      // The first word should be a map, and we expect all map pointers to
      // be in map space.
      Map* map = object->map();
      CHECK(map->IsMap());
      CHECK(heap()->map_space()->Contains(map));

      // Perform space-specific object verification.
      VerifyObject(object);

      // The object itself should look OK.
      object->Verify();

      // All the interior pointers should be contained in the heap.
      int size = object->Size();
      object->IterateBody(map->instance_type(), size, visitor);
      if (Marking::IsBlack(Marking::MarkBitFrom(object))) {
        black_size += size;
      }

      CHECK(object->address() + size <= top);
      end_of_previous_object = object->address() + size;
    }
    CHECK_LE(black_size, page->LiveBytes());
  }
  CHECK(allocation_pointer_found_in_space);
}
#endif  // VERIFY_HEAP

// -----------------------------------------------------------------------------
// NewSpace implementation


bool NewSpace::SetUp(int reserved_semispace_capacity,
                     int maximum_semispace_capacity) {
  // Set up new space based on the preallocated memory block defined by
  // start and size. The provided space is divided into two semi-spaces.
  // To support fast containment testing in the new space, the size of
  // this chunk must be a power of two and it must be aligned to its size.
  int initial_semispace_capacity = heap()->InitialSemiSpaceSize();

  size_t size = 2 * reserved_semispace_capacity;
  Address base =
      heap()->isolate()->memory_allocator()->ReserveAlignedMemory(
          size, size, &reservation_);
  if (base == NULL) return false;

  chunk_base_ = base;
  chunk_size_ = static_cast<uintptr_t>(size);
  LOG(heap()->isolate(), NewEvent("InitialChunk", chunk_base_, chunk_size_));

  ASSERT(initial_semispace_capacity <= maximum_semispace_capacity);
  ASSERT(IsPowerOf2(maximum_semispace_capacity));

  // Allocate and set up the histogram arrays if necessary.
  allocated_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
  promoted_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);

#define SET_NAME(name) allocated_histogram_[name].set_name(#name); \
                       promoted_histogram_[name].set_name(#name);
  INSTANCE_TYPE_LIST(SET_NAME)
#undef SET_NAME

  ASSERT(reserved_semispace_capacity == heap()->ReservedSemiSpaceSize());
  ASSERT(static_cast<intptr_t>(chunk_size_) >=
         2 * heap()->ReservedSemiSpaceSize());
  ASSERT(IsAddressAligned(chunk_base_, 2 * reserved_semispace_capacity, 0));

  to_space_.SetUp(chunk_base_,
                  initial_semispace_capacity,
                  maximum_semispace_capacity);
  from_space_.SetUp(chunk_base_ + reserved_semispace_capacity,
                    initial_semispace_capacity,
                    maximum_semispace_capacity);
  if (!to_space_.Commit()) {
    return false;
  }
  ASSERT(!from_space_.is_committed());  // No need to use memory yet.

  start_ = chunk_base_;
  address_mask_ = ~(2 * reserved_semispace_capacity - 1);
  object_mask_ = address_mask_ | kHeapObjectTagMask;
  object_expected_ = reinterpret_cast<uintptr_t>(start_) | kHeapObjectTag;

  ResetAllocationInfo();

  return true;
}


void NewSpace::TearDown() {
  if (allocated_histogram_) {
    DeleteArray(allocated_histogram_);
    allocated_histogram_ = NULL;
  }
  if (promoted_histogram_) {
    DeleteArray(promoted_histogram_);
    promoted_histogram_ = NULL;
  }

  start_ = NULL;
  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;

  to_space_.TearDown();
  from_space_.TearDown();

  LOG(heap()->isolate(), DeleteEvent("InitialChunk", chunk_base_));

  ASSERT(reservation_.IsReserved());
  heap()->isolate()->memory_allocator()->FreeMemory(&reservation_,
                                                    NOT_EXECUTABLE);
  chunk_base_ = NULL;
  chunk_size_ = 0;
}


void NewSpace::Flip() {
  SemiSpace::Swap(&from_space_, &to_space_);
}


void NewSpace::Grow() {
  // Double the semispace size but only up to maximum capacity.
  ASSERT(Capacity() < MaximumCapacity());
  int new_capacity = Min(MaximumCapacity(), 2 * static_cast<int>(Capacity()));
  if (to_space_.GrowTo(new_capacity)) {
    // Only grow from space if we managed to grow to-space.
    if (!from_space_.GrowTo(new_capacity)) {
      // If we managed to grow to-space but couldn't grow from-space,
      // attempt to shrink to-space.
      if (!to_space_.ShrinkTo(from_space_.Capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        V8::FatalProcessOutOfMemory("Failed to grow new space.");
      }
    }
  }
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::Shrink() {
  int new_capacity = Max(InitialCapacity(), 2 * SizeAsInt());
  int rounded_new_capacity = RoundUp(new_capacity, Page::kPageSize);
  if (rounded_new_capacity < Capacity() &&
      to_space_.ShrinkTo(rounded_new_capacity))  {
    // Only shrink from-space if we managed to shrink to-space.
    from_space_.Reset();
    if (!from_space_.ShrinkTo(rounded_new_capacity)) {
      // If we managed to shrink to-space but couldn't shrink from
      // space, attempt to grow to-space again.
      if (!to_space_.GrowTo(from_space_.Capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        V8::FatalProcessOutOfMemory("Failed to shrink new space.");
      }
    }
  }
  allocation_info_.limit = to_space_.page_high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::UpdateAllocationInfo() {
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top);
  allocation_info_.top = to_space_.page_low();
  allocation_info_.limit = to_space_.page_high();

  // Lower limit during incremental marking.
  if (heap()->incremental_marking()->IsMarking() &&
      inline_allocation_limit_step() != 0) {
    Address new_limit =
        allocation_info_.top + inline_allocation_limit_step();
    allocation_info_.limit = Min(new_limit, allocation_info_.limit);
  }
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::ResetAllocationInfo() {
  to_space_.Reset();
  UpdateAllocationInfo();
  pages_used_ = 0;
  // Clear all mark-bits in the to-space.
  NewSpacePageIterator it(&to_space_);
  while (it.has_next()) {
    Bitmap::Clear(it.next());
  }
}


bool NewSpace::AddFreshPage() {
  Address top = allocation_info_.top;
  if (NewSpacePage::IsAtStart(top)) {
    // The current page is already empty. Don't try to make another.

    // We should only get here if someone asks to allocate more
    // than what can be stored in a single page.
    // TODO(gc): Change the limit on new-space allocation to prevent this
    // from happening (all such allocations should go directly to LOSpace).
    return false;
  }
  if (!to_space_.AdvancePage()) {
    // Failed to get a new page in to-space.
    return false;
  }

  // Clear remainder of current page.
  Address limit = NewSpacePage::FromLimit(top)->area_end();
  if (heap()->gc_state() == Heap::SCAVENGE) {
    heap()->promotion_queue()->SetNewLimit(limit);
    heap()->promotion_queue()->ActivateGuardIfOnTheSamePage();
  }

  int remaining_in_page = static_cast<int>(limit - top);
  heap()->CreateFillerObjectAt(top, remaining_in_page);
  pages_used_++;
  UpdateAllocationInfo();

  return true;
}


MaybeObject* NewSpace::SlowAllocateRaw(int size_in_bytes) {
  Address old_top = allocation_info_.top;
  Address new_top = old_top + size_in_bytes;
  Address high = to_space_.page_high();
  if (allocation_info_.limit < high) {
    // Incremental marking has lowered the limit to get a
    // chance to do a step.
    allocation_info_.limit = Min(
        allocation_info_.limit + inline_allocation_limit_step_,
        high);
    int bytes_allocated = static_cast<int>(new_top - top_on_previous_step_);
    heap()->incremental_marking()->Step(
        bytes_allocated, IncrementalMarking::GC_VIA_STACK_GUARD);
    top_on_previous_step_ = new_top;
    return AllocateRaw(size_in_bytes);
  } else if (AddFreshPage()) {
    // Switched to new page. Try allocating again.
    int bytes_allocated = static_cast<int>(old_top - top_on_previous_step_);
    heap()->incremental_marking()->Step(
        bytes_allocated, IncrementalMarking::GC_VIA_STACK_GUARD);
    top_on_previous_step_ = to_space_.page_low();
    return AllocateRaw(size_in_bytes);
  } else {
    return Failure::RetryAfterGC();
  }
}


#ifdef VERIFY_HEAP
// We do not use the SemiSpaceIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void NewSpace::Verify() {
  // The allocation pointer should be in the space or at the very end.
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  // There should be objects packed in from the low address up to the
  // allocation pointer.
  Address current = to_space_.first_page()->area_start();
  CHECK_EQ(current, to_space_.space_start());

  while (current != top()) {
    if (!NewSpacePage::IsAtEnd(current)) {
      // The allocation pointer should not be in the middle of an object.
      CHECK(!NewSpacePage::FromLimit(current)->ContainsLimit(top()) ||
            current < top());

      HeapObject* object = HeapObject::FromAddress(current);

      // The first word should be a map, and we expect all map pointers to
      // be in map space.
      Map* map = object->map();
      CHECK(map->IsMap());
      CHECK(heap()->map_space()->Contains(map));

      // The object should not be code or a map.
      CHECK(!object->IsMap());
      CHECK(!object->IsCode());

      // The object itself should look OK.
      object->Verify();

      // All the interior pointers should be contained in the heap.
      VerifyPointersVisitor visitor;
      int size = object->Size();
      object->IterateBody(map->instance_type(), size, &visitor);

      current += size;
    } else {
      // At end of page, switch to next page.
      NewSpacePage* page = NewSpacePage::FromLimit(current)->next_page();
      // Next page should be valid.
      CHECK(!page->is_anchor());
      current = page->area_start();
    }
  }

  // Check semi-spaces.
  CHECK_EQ(from_space_.id(), kFromSpace);
  CHECK_EQ(to_space_.id(), kToSpace);
  from_space_.Verify();
  to_space_.Verify();
}
#endif

// -----------------------------------------------------------------------------
// SemiSpace implementation

void SemiSpace::SetUp(Address start,
                      int initial_capacity,
                      int maximum_capacity) {
  // Creates a space in the young generation. The constructor does not
  // allocate memory from the OS.  A SemiSpace is given a contiguous chunk of
  // memory of size 'capacity' when set up, and does not grow or shrink
  // otherwise.  In the mark-compact collector, the memory region of the from
  // space is used as the marking stack. It requires contiguous memory
  // addresses.
  ASSERT(maximum_capacity >= Page::kPageSize);
  initial_capacity_ = RoundDown(initial_capacity, Page::kPageSize);
  capacity_ = initial_capacity;
  maximum_capacity_ = RoundDown(maximum_capacity, Page::kPageSize);
  committed_ = false;
  start_ = start;
  address_mask_ = ~(maximum_capacity - 1);
  object_mask_ = address_mask_ | kHeapObjectTagMask;
  object_expected_ = reinterpret_cast<uintptr_t>(start) | kHeapObjectTag;
  age_mark_ = start_;
}


void SemiSpace::TearDown() {
  start_ = NULL;
  capacity_ = 0;
}


bool SemiSpace::Commit() {
  ASSERT(!is_committed());
  int pages = capacity_ / Page::kPageSize;
  Address end = start_ + maximum_capacity_;
  Address start = end - pages * Page::kPageSize;
  if (!heap()->isolate()->memory_allocator()->CommitBlock(start,
                                                          capacity_,
                                                          executable())) {
    return false;
  }

  NewSpacePage* page = anchor();
  for (int i = 1; i <= pages; i++) {
    NewSpacePage* new_page =
      NewSpacePage::Initialize(heap(), end - i * Page::kPageSize, this);
    new_page->InsertAfter(page);
    page = new_page;
  }

  committed_ = true;
  Reset();
  return true;
}


bool SemiSpace::Uncommit() {
  ASSERT(is_committed());
  Address start = start_ + maximum_capacity_ - capacity_;
  if (!heap()->isolate()->memory_allocator()->UncommitBlock(start, capacity_)) {
    return false;
  }
  anchor()->set_next_page(anchor());
  anchor()->set_prev_page(anchor());

  committed_ = false;
  return true;
}


size_t SemiSpace::CommittedPhysicalMemory() {
  if (!is_committed()) return 0;
  size_t size = 0;
  NewSpacePageIterator it(this);
  while (it.has_next()) {
    size += it.next()->CommittedPhysicalMemory();
  }
  return size;
}


bool SemiSpace::GrowTo(int new_capacity) {
  if (!is_committed()) {
    if (!Commit()) return false;
  }
  ASSERT((new_capacity & Page::kPageAlignmentMask) == 0);
  ASSERT(new_capacity <= maximum_capacity_);
  ASSERT(new_capacity > capacity_);
  int pages_before = capacity_ / Page::kPageSize;
  int pages_after = new_capacity / Page::kPageSize;

  Address end = start_ + maximum_capacity_;
  Address start = end - new_capacity;
  size_t delta = new_capacity - capacity_;

  ASSERT(IsAligned(delta, OS::AllocateAlignment()));
  if (!heap()->isolate()->memory_allocator()->CommitBlock(
      start, delta, executable())) {
    return false;
  }
  capacity_ = new_capacity;
  NewSpacePage* last_page = anchor()->prev_page();
  ASSERT(last_page != anchor());
  for (int i = pages_before + 1; i <= pages_after; i++) {
    Address page_address = end - i * Page::kPageSize;
    NewSpacePage* new_page = NewSpacePage::Initialize(heap(),
                                                      page_address,
                                                      this);
    new_page->InsertAfter(last_page);
    Bitmap::Clear(new_page);
    // Duplicate the flags that was set on the old page.
    new_page->SetFlags(last_page->GetFlags(),
                       NewSpacePage::kCopyOnFlipFlagsMask);
    last_page = new_page;
  }
  return true;
}


bool SemiSpace::ShrinkTo(int new_capacity) {
  ASSERT((new_capacity & Page::kPageAlignmentMask) == 0);
  ASSERT(new_capacity >= initial_capacity_);
  ASSERT(new_capacity < capacity_);
  if (is_committed()) {
    // Semispaces grow backwards from the end of their allocated capacity,
    // so we find the before and after start addresses relative to the
    // end of the space.
    Address space_end = start_ + maximum_capacity_;
    Address old_start = space_end - capacity_;
    size_t delta = capacity_ - new_capacity;
    ASSERT(IsAligned(delta, OS::AllocateAlignment()));

    MemoryAllocator* allocator = heap()->isolate()->memory_allocator();
    if (!allocator->UncommitBlock(old_start, delta)) {
      return false;
    }

    int pages_after = new_capacity / Page::kPageSize;
    NewSpacePage* new_last_page =
        NewSpacePage::FromAddress(space_end - pages_after * Page::kPageSize);
    new_last_page->set_next_page(anchor());
    anchor()->set_prev_page(new_last_page);
    ASSERT((current_page_ <= first_page()) && (current_page_ >= new_last_page));
  }

  capacity_ = new_capacity;

  return true;
}


void SemiSpace::FlipPages(intptr_t flags, intptr_t mask) {
  anchor_.set_owner(this);
  // Fixup back-pointers to anchor. Address of anchor changes
  // when we swap.
  anchor_.prev_page()->set_next_page(&anchor_);
  anchor_.next_page()->set_prev_page(&anchor_);

  bool becomes_to_space = (id_ == kFromSpace);
  id_ = becomes_to_space ? kToSpace : kFromSpace;
  NewSpacePage* page = anchor_.next_page();
  while (page != &anchor_) {
    page->set_owner(this);
    page->SetFlags(flags, mask);
    if (becomes_to_space) {
      page->ClearFlag(MemoryChunk::IN_FROM_SPACE);
      page->SetFlag(MemoryChunk::IN_TO_SPACE);
      page->ClearFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
      page->ResetLiveBytes();
    } else {
      page->SetFlag(MemoryChunk::IN_FROM_SPACE);
      page->ClearFlag(MemoryChunk::IN_TO_SPACE);
    }
    ASSERT(page->IsFlagSet(MemoryChunk::SCAN_ON_SCAVENGE));
    ASSERT(page->IsFlagSet(MemoryChunk::IN_TO_SPACE) ||
           page->IsFlagSet(MemoryChunk::IN_FROM_SPACE));
    page = page->next_page();
  }
}


void SemiSpace::Reset() {
  ASSERT(anchor_.next_page() != &anchor_);
  current_page_ = anchor_.next_page();
}


void SemiSpace::Swap(SemiSpace* from, SemiSpace* to) {
  // We won't be swapping semispaces without data in them.
  ASSERT(from->anchor_.next_page() != &from->anchor_);
  ASSERT(to->anchor_.next_page() != &to->anchor_);

  // Swap bits.
  SemiSpace tmp = *from;
  *from = *to;
  *to = tmp;

  // Fixup back-pointers to the page list anchor now that its address
  // has changed.
  // Swap to/from-space bits on pages.
  // Copy GC flags from old active space (from-space) to new (to-space).
  intptr_t flags = from->current_page()->GetFlags();
  to->FlipPages(flags, NewSpacePage::kCopyOnFlipFlagsMask);

  from->FlipPages(0, 0);
}


void SemiSpace::set_age_mark(Address mark) {
  ASSERT(NewSpacePage::FromLimit(mark)->semi_space() == this);
  age_mark_ = mark;
  // Mark all pages up to the one containing mark.
  NewSpacePageIterator it(space_start(), mark);
  while (it.has_next()) {
    it.next()->SetFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
  }
}


#ifdef DEBUG
void SemiSpace::Print() { }
#endif

#ifdef VERIFY_HEAP
void SemiSpace::Verify() {
  bool is_from_space = (id_ == kFromSpace);
  NewSpacePage* page = anchor_.next_page();
  CHECK(anchor_.semi_space() == this);
  while (page != &anchor_) {
    CHECK(page->semi_space() == this);
    CHECK(page->InNewSpace());
    CHECK(page->IsFlagSet(is_from_space ? MemoryChunk::IN_FROM_SPACE
                                        : MemoryChunk::IN_TO_SPACE));
    CHECK(!page->IsFlagSet(is_from_space ? MemoryChunk::IN_TO_SPACE
                                         : MemoryChunk::IN_FROM_SPACE));
    CHECK(page->IsFlagSet(MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING));
    if (!is_from_space) {
      // The pointers-from-here-are-interesting flag isn't updated dynamically
      // on from-space pages, so it might be out of sync with the marking state.
      if (page->heap()->incremental_marking()->IsMarking()) {
        CHECK(page->IsFlagSet(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING));
      } else {
        CHECK(!page->IsFlagSet(
            MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING));
      }
      // TODO(gc): Check that the live_bytes_count_ field matches the
      // black marking on the page (if we make it match in new-space).
    }
    CHECK(page->IsFlagSet(MemoryChunk::SCAN_ON_SCAVENGE));
    CHECK(page->prev_page()->next_page() == page);
    page = page->next_page();
  }
}
#endif

#ifdef DEBUG
void SemiSpace::AssertValidRange(Address start, Address end) {
  // Addresses belong to same semi-space
  NewSpacePage* page = NewSpacePage::FromLimit(start);
  NewSpacePage* end_page = NewSpacePage::FromLimit(end);
  SemiSpace* space = page->semi_space();
  CHECK_EQ(space, end_page->semi_space());
  // Start address is before end address, either on same page,
  // or end address is on a later page in the linked list of
  // semi-space pages.
  if (page == end_page) {
    CHECK(start <= end);
  } else {
    while (page != end_page) {
      page = page->next_page();
      CHECK_NE(page, space->anchor());
    }
  }
}
#endif


// -----------------------------------------------------------------------------
// SemiSpaceIterator implementation.
SemiSpaceIterator::SemiSpaceIterator(NewSpace* space) {
  Initialize(space->bottom(), space->top(), NULL);
}


SemiSpaceIterator::SemiSpaceIterator(NewSpace* space,
                                     HeapObjectCallback size_func) {
  Initialize(space->bottom(), space->top(), size_func);
}


SemiSpaceIterator::SemiSpaceIterator(NewSpace* space, Address start) {
  Initialize(start, space->top(), NULL);
}


SemiSpaceIterator::SemiSpaceIterator(Address from, Address to) {
  Initialize(from, to, NULL);
}


void SemiSpaceIterator::Initialize(Address start,
                                   Address end,
                                   HeapObjectCallback size_func) {
  SemiSpace::AssertValidRange(start, end);
  current_ = start;
  limit_ = end;
  size_func_ = size_func;
}


#ifdef DEBUG
// heap_histograms is shared, always clear it before using it.
static void ClearHistograms() {
  Isolate* isolate = Isolate::Current();
  // We reset the name each time, though it hasn't changed.
#define DEF_TYPE_NAME(name) isolate->heap_histograms()[name].set_name(#name);
  INSTANCE_TYPE_LIST(DEF_TYPE_NAME)
#undef DEF_TYPE_NAME

#define CLEAR_HISTOGRAM(name) isolate->heap_histograms()[name].clear();
  INSTANCE_TYPE_LIST(CLEAR_HISTOGRAM)
#undef CLEAR_HISTOGRAM

  isolate->js_spill_information()->Clear();
}


static void ClearCodeKindStatistics(int* code_kind_statistics) {
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    code_kind_statistics[i] = 0;
  }
}


static void ReportCodeKindStatistics(int* code_kind_statistics) {
  PrintF("\n   Code kind histograms: \n");
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    if (code_kind_statistics[i] > 0) {
      PrintF("     %-20s: %10d bytes\n",
             Code::Kind2String(static_cast<Code::Kind>(i)),
             code_kind_statistics[i]);
    }
  }
  PrintF("\n");
}


static int CollectHistogramInfo(HeapObject* obj) {
  Isolate* isolate = obj->GetIsolate();
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  ASSERT(isolate->heap_histograms()[type].name() != NULL);
  isolate->heap_histograms()[type].increment_number(1);
  isolate->heap_histograms()[type].increment_bytes(obj->Size());

  if (FLAG_collect_heap_spill_statistics && obj->IsJSObject()) {
    JSObject::cast(obj)->IncrementSpillStatistics(
        isolate->js_spill_information());
  }

  return obj->Size();
}


static void ReportHistogram(bool print_spill) {
  Isolate* isolate = Isolate::Current();
  PrintF("\n  Object Histogram:\n");
  for (int i = 0; i <= LAST_TYPE; i++) {
    if (isolate->heap_histograms()[i].number() > 0) {
      PrintF("    %-34s%10d (%10d bytes)\n",
             isolate->heap_histograms()[i].name(),
             isolate->heap_histograms()[i].number(),
             isolate->heap_histograms()[i].bytes());
    }
  }
  PrintF("\n");

  // Summarize string types.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name)      \
    string_number += isolate->heap_histograms()[type].number(); \
    string_bytes += isolate->heap_histograms()[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    PrintF("    %-34s%10d (%10d bytes)\n\n", "STRING_TYPE", string_number,
           string_bytes);
  }

  if (FLAG_collect_heap_spill_statistics && print_spill) {
    isolate->js_spill_information()->Print();
  }
}
#endif  // DEBUG


// Support for statistics gathering for --heap-stats and --log-gc.
void NewSpace::ClearHistograms() {
  for (int i = 0; i <= LAST_TYPE; i++) {
    allocated_histogram_[i].clear();
    promoted_histogram_[i].clear();
  }
}

// Because the copying collector does not touch garbage objects, we iterate
// the new space before a collection to get a histogram of allocated objects.
// This only happens when --log-gc flag is set.
void NewSpace::CollectStatistics() {
  ClearHistograms();
  SemiSpaceIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next())
    RecordAllocation(obj);
}


static void DoReportStatistics(Isolate* isolate,
                               HistogramInfo* info, const char* description) {
  LOG(isolate, HeapSampleBeginEvent("NewSpace", description));
  // Lump all the string types together.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name)       \
    string_number += info[type].number();             \
    string_bytes += info[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    LOG(isolate,
        HeapSampleItemEvent("STRING_TYPE", string_number, string_bytes));
  }

  // Then do the other types.
  for (int i = FIRST_NONSTRING_TYPE; i <= LAST_TYPE; ++i) {
    if (info[i].number() > 0) {
      LOG(isolate,
          HeapSampleItemEvent(info[i].name(), info[i].number(),
                              info[i].bytes()));
    }
  }
  LOG(isolate, HeapSampleEndEvent("NewSpace", description));
}


void NewSpace::ReportStatistics() {
#ifdef DEBUG
  if (FLAG_heap_stats) {
    float pct = static_cast<float>(Available()) / Capacity();
    PrintF("  capacity: %" V8_PTR_PREFIX "d"
               ", available: %" V8_PTR_PREFIX "d, %%%d\n",
           Capacity(), Available(), static_cast<int>(pct*100));
    PrintF("\n  Object Histogram:\n");
    for (int i = 0; i <= LAST_TYPE; i++) {
      if (allocated_histogram_[i].number() > 0) {
        PrintF("    %-34s%10d (%10d bytes)\n",
               allocated_histogram_[i].name(),
               allocated_histogram_[i].number(),
               allocated_histogram_[i].bytes());
      }
    }
    PrintF("\n");
  }
#endif  // DEBUG

  if (FLAG_log_gc) {
    Isolate* isolate = ISOLATE;
    DoReportStatistics(isolate, allocated_histogram_, "allocated");
    DoReportStatistics(isolate, promoted_histogram_, "promoted");
  }
}


void NewSpace::RecordAllocation(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  allocated_histogram_[type].increment_number(1);
  allocated_histogram_[type].increment_bytes(obj->Size());
}


void NewSpace::RecordPromotion(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  promoted_histogram_[type].increment_number(1);
  promoted_histogram_[type].increment_bytes(obj->Size());
}


size_t NewSpace::CommittedPhysicalMemory() {
  if (!VirtualMemory::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top);
  size_t size = to_space_.CommittedPhysicalMemory();
  if (from_space_.is_committed()) {
    size += from_space_.CommittedPhysicalMemory();
  }
  return size;
}

// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation

void FreeListNode::set_size(Heap* heap, int size_in_bytes) {
  ASSERT(size_in_bytes > 0);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  // We write a map and possibly size information to the block.  If the block
  // is big enough to be a FreeSpace with at least one extra word (the next
  // pointer), we set its map to be the free space map and its size to an
  // appropriate array length for the desired size from HeapObject::Size().
  // If the block is too small (eg, one or two words), to hold both a size
  // field and a next pointer, we give it a filler map that gives it the
  // correct size.
  if (size_in_bytes > FreeSpace::kHeaderSize) {
    set_map_no_write_barrier(heap->raw_unchecked_free_space_map());
    // Can't use FreeSpace::cast because it fails during deserialization.
    FreeSpace* this_as_free_space = reinterpret_cast<FreeSpace*>(this);
    this_as_free_space->set_size(size_in_bytes);
  } else if (size_in_bytes == kPointerSize) {
    set_map_no_write_barrier(heap->raw_unchecked_one_pointer_filler_map());
  } else if (size_in_bytes == 2 * kPointerSize) {
    set_map_no_write_barrier(heap->raw_unchecked_two_pointer_filler_map());
  } else {
    UNREACHABLE();
  }
  // We would like to ASSERT(Size() == size_in_bytes) but this would fail during
  // deserialization because the free space map is not done yet.
}


FreeListNode* FreeListNode::next() {
  ASSERT(IsFreeListNode(this));
  if (map() == GetHeap()->raw_unchecked_free_space_map()) {
    ASSERT(map() == NULL || Size() >= kNextOffset + kPointerSize);
    return reinterpret_cast<FreeListNode*>(
        Memory::Address_at(address() + kNextOffset));
  } else {
    return reinterpret_cast<FreeListNode*>(
        Memory::Address_at(address() + kPointerSize));
  }
}


FreeListNode** FreeListNode::next_address() {
  ASSERT(IsFreeListNode(this));
  if (map() == GetHeap()->raw_unchecked_free_space_map()) {
    ASSERT(Size() >= kNextOffset + kPointerSize);
    return reinterpret_cast<FreeListNode**>(address() + kNextOffset);
  } else {
    return reinterpret_cast<FreeListNode**>(address() + kPointerSize);
  }
}


void FreeListNode::set_next(FreeListNode* next) {
  ASSERT(IsFreeListNode(this));
  // While we are booting the VM the free space map will actually be null.  So
  // we have to make sure that we don't try to use it for anything at that
  // stage.
  if (map() == GetHeap()->raw_unchecked_free_space_map()) {
    ASSERT(map() == NULL || Size() >= kNextOffset + kPointerSize);
    Memory::Address_at(address() + kNextOffset) =
        reinterpret_cast<Address>(next);
  } else {
    Memory::Address_at(address() + kPointerSize) =
        reinterpret_cast<Address>(next);
  }
}


intptr_t FreeListCategory::Concatenate(FreeListCategory* category) {
  intptr_t free_bytes = 0;
  if (category->top_ != NULL) {
    ASSERT(category->end_ != NULL);
    // This is safe (not going to deadlock) since Concatenate operations
    // are never performed on the same free lists at the same time in
    // reverse order.
    ScopedLock lock_target(mutex_);
    ScopedLock lock_source(category->mutex());
    free_bytes = category->available();
    if (end_ == NULL) {
      end_ = category->end();
    } else {
      category->end()->set_next(top_);
    }
    top_ = category->top();
    available_ += category->available();
    category->Reset();
  }
  return free_bytes;
}


void FreeListCategory::Reset() {
  top_ = NULL;
  end_ = NULL;
  available_ = 0;
}


intptr_t FreeListCategory::EvictFreeListItemsInList(Page* p) {
  int sum = 0;
  FreeListNode** n = &top_;
  while (*n != NULL) {
    if (Page::FromAddress((*n)->address()) == p) {
      FreeSpace* free_space = reinterpret_cast<FreeSpace*>(*n);
      sum += free_space->Size();
      *n = (*n)->next();
    } else {
      n = (*n)->next_address();
    }
  }
  if (top_ == NULL) {
    end_ = NULL;
  }
  available_ -= sum;
  return sum;
}


FreeListNode* FreeListCategory::PickNodeFromList(int *node_size) {
  FreeListNode* node = top_;

  if (node == NULL) return NULL;

  while (node != NULL &&
         Page::FromAddress(node->address())->IsEvacuationCandidate()) {
    available_ -= reinterpret_cast<FreeSpace*>(node)->Size();
    node = node->next();
  }

  if (node != NULL) {
    set_top(node->next());
    *node_size = reinterpret_cast<FreeSpace*>(node)->Size();
    available_ -= *node_size;
  } else {
    set_top(NULL);
  }

  if (top() == NULL) {
    set_end(NULL);
  }

  return node;
}


FreeListNode* FreeListCategory::PickNodeFromList(int size_in_bytes,
                                                 int *node_size) {
  FreeListNode* node = PickNodeFromList(node_size);
  if (node != NULL && *node_size < size_in_bytes) {
    Free(node, *node_size);
    *node_size = 0;
    return NULL;
  }
  return node;
}


void FreeListCategory::Free(FreeListNode* node, int size_in_bytes) {
  node->set_next(top_);
  top_ = node;
  if (end_ == NULL) {
    end_ = node;
  }
  available_ += size_in_bytes;
}


void FreeListCategory::RepairFreeList(Heap* heap) {
  FreeListNode* n = top_;
  while (n != NULL) {
    Map** map_location = reinterpret_cast<Map**>(n->address());
    if (*map_location == NULL) {
      *map_location = heap->free_space_map();
    } else {
      ASSERT(*map_location == heap->free_space_map());
    }
    n = n->next();
  }
}


FreeList::FreeList(PagedSpace* owner)
    : owner_(owner), heap_(owner->heap()) {
  Reset();
}


intptr_t FreeList::Concatenate(FreeList* free_list) {
  intptr_t free_bytes = 0;
  free_bytes += small_list_.Concatenate(free_list->small_list());
  free_bytes += medium_list_.Concatenate(free_list->medium_list());
  free_bytes += large_list_.Concatenate(free_list->large_list());
  free_bytes += huge_list_.Concatenate(free_list->huge_list());
  return free_bytes;
}


void FreeList::Reset() {
  small_list_.Reset();
  medium_list_.Reset();
  large_list_.Reset();
  huge_list_.Reset();
}


int FreeList::Free(Address start, int size_in_bytes) {
  if (size_in_bytes == 0) return 0;

  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(heap_, size_in_bytes);
  Page* page = Page::FromAddress(start);

  // Early return to drop too-small blocks on the floor.
  if (size_in_bytes < kSmallListMin) {
    page->add_non_available_small_blocks(size_in_bytes);
    return size_in_bytes;
  }

  // Insert other blocks at the head of a free list of the appropriate
  // magnitude.
  if (size_in_bytes <= kSmallListMax) {
    small_list_.Free(node, size_in_bytes);
    page->add_available_in_small_free_list(size_in_bytes);
  } else if (size_in_bytes <= kMediumListMax) {
    medium_list_.Free(node, size_in_bytes);
    page->add_available_in_medium_free_list(size_in_bytes);
  } else if (size_in_bytes <= kLargeListMax) {
    large_list_.Free(node, size_in_bytes);
    page->add_available_in_large_free_list(size_in_bytes);
  } else {
    huge_list_.Free(node, size_in_bytes);
    page->add_available_in_huge_free_list(size_in_bytes);
  }

  ASSERT(IsVeryLong() || available() == SumFreeLists());
  return 0;
}


FreeListNode* FreeList::FindNodeFor(int size_in_bytes, int* node_size) {
  FreeListNode* node = NULL;
  Page* page = NULL;

  if (size_in_bytes <= kSmallAllocationMax) {
    node = small_list_.PickNodeFromList(node_size);
    if (node != NULL) {
      ASSERT(size_in_bytes <= *node_size);
      page = Page::FromAddress(node->address());
      page->add_available_in_small_free_list(-(*node_size));
      ASSERT(IsVeryLong() || available() == SumFreeLists());
      return node;
    }
  }

  if (size_in_bytes <= kMediumAllocationMax) {
    node = medium_list_.PickNodeFromList(node_size);
    if (node != NULL) {
      ASSERT(size_in_bytes <= *node_size);
      page = Page::FromAddress(node->address());
      page->add_available_in_medium_free_list(-(*node_size));
      ASSERT(IsVeryLong() || available() == SumFreeLists());
      return node;
    }
  }

  if (size_in_bytes <= kLargeAllocationMax) {
    node = large_list_.PickNodeFromList(node_size);
    if (node != NULL) {
      ASSERT(size_in_bytes <= *node_size);
      page = Page::FromAddress(node->address());
      page->add_available_in_large_free_list(-(*node_size));
      ASSERT(IsVeryLong() || available() == SumFreeLists());
      return node;
    }
  }

  int huge_list_available = huge_list_.available();
  for (FreeListNode** cur = huge_list_.GetTopAddress();
       *cur != NULL;
       cur = (*cur)->next_address()) {
    FreeListNode* cur_node = *cur;
    while (cur_node != NULL &&
           Page::FromAddress(cur_node->address())->IsEvacuationCandidate()) {
      int size = reinterpret_cast<FreeSpace*>(cur_node)->Size();
      huge_list_available -= size;
      page = Page::FromAddress(cur_node->address());
      page->add_available_in_huge_free_list(-size);
      cur_node = cur_node->next();
    }

    *cur = cur_node;
    if (cur_node == NULL) {
      huge_list_.set_end(NULL);
      break;
    }

    ASSERT((*cur)->map() == heap_->raw_unchecked_free_space_map());
    FreeSpace* cur_as_free_space = reinterpret_cast<FreeSpace*>(*cur);
    int size = cur_as_free_space->Size();
    if (size >= size_in_bytes) {
      // Large enough node found.  Unlink it from the list.
      node = *cur;
      *cur = node->next();
      *node_size = size;
      huge_list_available -= size;
      page = Page::FromAddress(node->address());
      page->add_available_in_huge_free_list(-size);
      break;
    }
  }

  if (huge_list_.top() == NULL) {
    huge_list_.set_end(NULL);
  }
  huge_list_.set_available(huge_list_available);

  if (node != NULL) {
    ASSERT(IsVeryLong() || available() == SumFreeLists());
    return node;
  }

  if (size_in_bytes <= kSmallListMax) {
    node = small_list_.PickNodeFromList(size_in_bytes, node_size);
    if (node != NULL) {
      ASSERT(size_in_bytes <= *node_size);
      page = Page::FromAddress(node->address());
      page->add_available_in_small_free_list(-(*node_size));
    }
  } else if (size_in_bytes <= kMediumListMax) {
    node = medium_list_.PickNodeFromList(size_in_bytes, node_size);
    if (node != NULL) {
      ASSERT(size_in_bytes <= *node_size);
      page = Page::FromAddress(node->address());
      page->add_available_in_medium_free_list(-(*node_size));
    }
  } else if (size_in_bytes <= kLargeListMax) {
    node = large_list_.PickNodeFromList(size_in_bytes, node_size);
    if (node != NULL) {
      ASSERT(size_in_bytes <= *node_size);
      page = Page::FromAddress(node->address());
      page->add_available_in_large_free_list(-(*node_size));
    }
  }

  ASSERT(IsVeryLong() || available() == SumFreeLists());
  return node;
}


// Allocation on the old space free list.  If it succeeds then a new linear
// allocation space has been set up with the top and limit of the space.  If
// the allocation fails then NULL is returned, and the caller can perform a GC
// or allocate a new page before retrying.
HeapObject* FreeList::Allocate(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  ASSERT(size_in_bytes <= kMaxBlockSize);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));
  // Don't free list allocate if there is linear space available.
  ASSERT(owner_->limit() - owner_->top() < size_in_bytes);

  int old_linear_size = static_cast<int>(owner_->limit() - owner_->top());
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  owner_->Free(owner_->top(), old_linear_size);

  owner_->heap()->incremental_marking()->OldSpaceStep(
      size_in_bytes - old_linear_size);

  int new_node_size = 0;
  FreeListNode* new_node = FindNodeFor(size_in_bytes, &new_node_size);
  if (new_node == NULL) {
    owner_->SetTop(NULL, NULL);
    return NULL;
  }

  int bytes_left = new_node_size - size_in_bytes;
  ASSERT(bytes_left >= 0);

#ifdef DEBUG
  for (int i = 0; i < size_in_bytes / kPointerSize; i++) {
    reinterpret_cast<Object**>(new_node->address())[i] =
        Smi::FromInt(kCodeZapValue);
  }
#endif

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  ASSERT(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  const int kThreshold = IncrementalMarking::kAllocatedThreshold;

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  owner_->Allocate(new_node_size);

  if (bytes_left > kThreshold &&
      owner_->heap()->incremental_marking()->IsMarkingIncomplete() &&
      FLAG_incremental_marking_steps) {
    int linear_size = owner_->RoundSizeDownToObjectAlignment(kThreshold);
    // We don't want to give too large linear areas to the allocator while
    // incremental marking is going on, because we won't check again whether
    // we want to do another increment until the linear area is used up.
    owner_->Free(new_node->address() + size_in_bytes + linear_size,
                 new_node_size - size_in_bytes - linear_size);
    owner_->SetTop(new_node->address() + size_in_bytes,
                   new_node->address() + size_in_bytes + linear_size);
  } else if (bytes_left > 0) {
    // Normally we give the rest of the node to the allocator as its new
    // linear allocation area.
    owner_->SetTop(new_node->address() + size_in_bytes,
                   new_node->address() + new_node_size);
  } else {
    // TODO(gc) Try not freeing linear allocation region when bytes_left
    // are zero.
    owner_->SetTop(NULL, NULL);
  }

  return new_node;
}


intptr_t FreeList::EvictFreeListItems(Page* p) {
  intptr_t sum = huge_list_.EvictFreeListItemsInList(p);
  p->set_available_in_huge_free_list(0);

  if (sum < p->area_size()) {
    sum += small_list_.EvictFreeListItemsInList(p) +
        medium_list_.EvictFreeListItemsInList(p) +
        large_list_.EvictFreeListItemsInList(p);
    p->set_available_in_small_free_list(0);
    p->set_available_in_medium_free_list(0);
    p->set_available_in_large_free_list(0);
  }

  return sum;
}


void FreeList::RepairLists(Heap* heap) {
  small_list_.RepairFreeList(heap);
  medium_list_.RepairFreeList(heap);
  large_list_.RepairFreeList(heap);
  huge_list_.RepairFreeList(heap);
}


#ifdef DEBUG
intptr_t FreeListCategory::SumFreeList() {
  intptr_t sum = 0;
  FreeListNode* cur = top_;
  while (cur != NULL) {
    ASSERT(cur->map() == cur->GetHeap()->raw_unchecked_free_space_map());
    FreeSpace* cur_as_free_space = reinterpret_cast<FreeSpace*>(cur);
    sum += cur_as_free_space->Size();
    cur = cur->next();
  }
  return sum;
}


static const int kVeryLongFreeList = 500;


int FreeListCategory::FreeListLength() {
  int length = 0;
  FreeListNode* cur = top_;
  while (cur != NULL) {
    length++;
    cur = cur->next();
    if (length == kVeryLongFreeList) return length;
  }
  return length;
}


bool FreeList::IsVeryLong() {
  if (small_list_.FreeListLength() == kVeryLongFreeList) return  true;
  if (medium_list_.FreeListLength() == kVeryLongFreeList) return  true;
  if (large_list_.FreeListLength() == kVeryLongFreeList) return  true;
  if (huge_list_.FreeListLength() == kVeryLongFreeList) return  true;
  return false;
}


// This can take a very long time because it is linear in the number of entries
// on the free list, so it should not be called if FreeListLength returns
// kVeryLongFreeList.
intptr_t FreeList::SumFreeLists() {
  intptr_t sum = small_list_.SumFreeList();
  sum += medium_list_.SumFreeList();
  sum += large_list_.SumFreeList();
  sum += huge_list_.SumFreeList();
  return sum;
}
#endif


// -----------------------------------------------------------------------------
// OldSpace implementation

bool NewSpace::ReserveSpace(int bytes) {
  // We can't reliably unpack a partial snapshot that needs more new space
  // space than the minimum NewSpace size.  The limit can be set lower than
  // the end of new space either because there is more space on the next page
  // or because we have lowered the limit in order to get periodic incremental
  // marking.  The most reliable way to ensure that there is linear space is
  // to do the allocation, then rewind the limit.
  ASSERT(bytes <= InitialCapacity());
  MaybeObject* maybe = AllocateRaw(bytes);
  Object* object = NULL;
  if (!maybe->ToObject(&object)) return false;
  HeapObject* allocation = HeapObject::cast(object);
  Address top = allocation_info_.top;
  if ((top - bytes) == allocation->address()) {
    allocation_info_.top = allocation->address();
    return true;
  }
  // There may be a borderline case here where the allocation succeeded, but
  // the limit and top have moved on to a new page.  In that case we try again.
  return ReserveSpace(bytes);
}


void PagedSpace::PrepareForMarkCompact() {
  // We don't have a linear allocation area while sweeping.  It will be restored
  // on the first allocation after the sweep.
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  int old_linear_size = static_cast<int>(limit() - top());
  Free(top(), old_linear_size);
  SetTop(NULL, NULL);

  // Stop lazy sweeping and clear marking bits for unswept pages.
  if (first_unswept_page_ != NULL) {
    Page* p = first_unswept_page_;
    do {
      // Do not use ShouldBeSweptLazily predicate here.
      // New evacuation candidates were selected but they still have
      // to be swept before collection starts.
      if (!p->WasSwept()) {
        Bitmap::Clear(p);
        if (FLAG_gc_verbose) {
          PrintF("Sweeping 0x%" V8PRIxPTR " lazily abandoned.\n",
                 reinterpret_cast<intptr_t>(p));
        }
      }
      p = p->next_page();
    } while (p != anchor());
  }
  first_unswept_page_ = Page::FromAddress(NULL);
  unswept_free_bytes_ = 0;

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_.Reset();
}


bool PagedSpace::ReserveSpace(int size_in_bytes) {
  ASSERT(size_in_bytes <= AreaSize());
  ASSERT(size_in_bytes == RoundSizeDownToObjectAlignment(size_in_bytes));
  Address current_top = allocation_info_.top;
  Address new_top = current_top + size_in_bytes;
  if (new_top <= allocation_info_.limit) return true;

  HeapObject* new_area = free_list_.Allocate(size_in_bytes);
  if (new_area == NULL) new_area = SlowAllocateRaw(size_in_bytes);
  if (new_area == NULL) return false;

  int old_linear_size = static_cast<int>(limit() - top());
  // Mark the old linear allocation area with a free space so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  Free(top(), old_linear_size);

  SetTop(new_area->address(), new_area->address() + size_in_bytes);
  return true;
}


intptr_t PagedSpace::SizeOfObjects() {
  ASSERT(!heap()->IsSweepingComplete() || (unswept_free_bytes_ == 0));
  return Size() - unswept_free_bytes_ - (limit() - top());
}


// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally NULL), so we need to
// fix them.
void PagedSpace::RepairFreeListsAfterBoot() {
  free_list_.RepairLists(heap());
}


// You have to call this last, since the implementation from PagedSpace
// doesn't know that memory was 'promised' to large object space.
bool LargeObjectSpace::ReserveSpace(int bytes) {
  return heap()->OldGenerationCapacityAvailable() >= bytes &&
         (!heap()->incremental_marking()->IsStopped() ||
           heap()->OldGenerationSpaceAvailable() >= bytes);
}


bool PagedSpace::AdvanceSweeper(intptr_t bytes_to_sweep) {
  if (IsLazySweepingComplete()) return true;

  intptr_t freed_bytes = 0;
  Page* p = first_unswept_page_;
  do {
    Page* next_page = p->next_page();
    if (ShouldBeSweptLazily(p)) {
      if (FLAG_gc_verbose) {
        PrintF("Sweeping 0x%" V8PRIxPTR " lazily advanced.\n",
               reinterpret_cast<intptr_t>(p));
      }
      DecreaseUnsweptFreeBytes(p);
      freed_bytes +=
          MarkCompactCollector::
              SweepConservatively<MarkCompactCollector::SWEEP_SEQUENTIALLY>(
                  this, NULL, p);
    }
    p = next_page;
  } while (p != anchor() && freed_bytes < bytes_to_sweep);

  if (p == anchor()) {
    first_unswept_page_ = Page::FromAddress(NULL);
  } else {
    first_unswept_page_ = p;
  }

  heap()->FreeQueuedChunks();

  return IsLazySweepingComplete();
}


void PagedSpace::EvictEvacuationCandidatesFromFreeLists() {
  if (allocation_info_.top >= allocation_info_.limit) return;

  if (Page::FromAllocationTop(allocation_info_.top)->IsEvacuationCandidate()) {
    // Create filler object to keep page iterable if it was iterable.
    int remaining =
        static_cast<int>(allocation_info_.limit - allocation_info_.top);
    heap()->CreateFillerObjectAt(allocation_info_.top, remaining);

    allocation_info_.top = NULL;
    allocation_info_.limit = NULL;
  }
}


bool PagedSpace::EnsureSweeperProgress(intptr_t size_in_bytes) {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (collector->AreSweeperThreadsActivated()) {
    if (collector->IsConcurrentSweepingInProgress()) {
      if (collector->StealMemoryFromSweeperThreads(this) < size_in_bytes) {
        if (!collector->sequential_sweeping()) {
          collector->WaitUntilSweepingCompleted();
          return true;
        }
      }
      return false;
    }
    return true;
  } else {
    return AdvanceSweeper(size_in_bytes);
  }
}


HeapObject* PagedSpace::SlowAllocateRaw(int size_in_bytes) {
  // Allocation in this space has failed.

  // If there are unswept pages advance lazy sweeper a bounded number of times
  // until we find a size_in_bytes contiguous piece of memory
  const int kMaxSweepingTries = 5;
  bool sweeping_complete = false;

  for (int i = 0; i < kMaxSweepingTries && !sweeping_complete; i++) {
    sweeping_complete = EnsureSweeperProgress(size_in_bytes);

    // Retry the free list allocation.
    HeapObject* object = free_list_.Allocate(size_in_bytes);
    if (object != NULL) return object;
  }

  // Free list allocation failed and there is no next page.  Fail if we have
  // hit the old generation size limit that should cause a garbage
  // collection.
  if (!heap()->always_allocate() &&
      heap()->OldGenerationAllocationLimitReached()) {
    return NULL;
  }

  // Try to expand the space and allocate in the new next page.
  if (Expand()) {
    return free_list_.Allocate(size_in_bytes);
  }

  // Last ditch, sweep all the remaining pages to try to find space.  This may
  // cause a pause.
  if (!IsLazySweepingComplete()) {
    EnsureSweeperProgress(kMaxInt);

    // Retry the free list allocation.
    HeapObject* object = free_list_.Allocate(size_in_bytes);
    if (object != NULL) return object;
  }

  // Finally, fail.
  return NULL;
}


#ifdef DEBUG
void PagedSpace::ReportCodeStatistics() {
  Isolate* isolate = Isolate::Current();
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  ReportCodeKindStatistics(isolate->code_kind_statistics());
  PrintF("Code comment statistics (\"   [ comment-txt   :    size/   "
         "count  (average)\"):\n");
  for (int i = 0; i <= CommentStatistic::kMaxComments; i++) {
    const CommentStatistic& cs = comments_statistics[i];
    if (cs.size > 0) {
      PrintF("   %-30s: %10d/%6d     (%d)\n", cs.comment, cs.size, cs.count,
             cs.size/cs.count);
    }
  }
  PrintF("\n");
}


void PagedSpace::ResetCodeStatistics() {
  Isolate* isolate = Isolate::Current();
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  ClearCodeKindStatistics(isolate->code_kind_statistics());
  for (int i = 0; i < CommentStatistic::kMaxComments; i++) {
    comments_statistics[i].Clear();
  }
  comments_statistics[CommentStatistic::kMaxComments].comment = "Unknown";
  comments_statistics[CommentStatistic::kMaxComments].size = 0;
  comments_statistics[CommentStatistic::kMaxComments].count = 0;
}


// Adds comment to 'comment_statistics' table. Performance OK as long as
// 'kMaxComments' is small
static void EnterComment(Isolate* isolate, const char* comment, int delta) {
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  // Do not count empty comments
  if (delta <= 0) return;
  CommentStatistic* cs = &comments_statistics[CommentStatistic::kMaxComments];
  // Search for a free or matching entry in 'comments_statistics': 'cs'
  // points to result.
  for (int i = 0; i < CommentStatistic::kMaxComments; i++) {
    if (comments_statistics[i].comment == NULL) {
      cs = &comments_statistics[i];
      cs->comment = comment;
      break;
    } else if (strcmp(comments_statistics[i].comment, comment) == 0) {
      cs = &comments_statistics[i];
      break;
    }
  }
  // Update entry for 'comment'
  cs->size += delta;
  cs->count += 1;
}


// Call for each nested comment start (start marked with '[ xxx', end marked
// with ']'.  RelocIterator 'it' must point to a comment reloc info.
static void CollectCommentStatistics(Isolate* isolate, RelocIterator* it) {
  ASSERT(!it->done());
  ASSERT(it->rinfo()->rmode() == RelocInfo::COMMENT);
  const char* tmp = reinterpret_cast<const char*>(it->rinfo()->data());
  if (tmp[0] != '[') {
    // Not a nested comment; skip
    return;
  }

  // Search for end of nested comment or a new nested comment
  const char* const comment_txt =
      reinterpret_cast<const char*>(it->rinfo()->data());
  const byte* prev_pc = it->rinfo()->pc();
  int flat_delta = 0;
  it->next();
  while (true) {
    // All nested comments must be terminated properly, and therefore exit
    // from loop.
    ASSERT(!it->done());
    if (it->rinfo()->rmode() == RelocInfo::COMMENT) {
      const char* const txt =
          reinterpret_cast<const char*>(it->rinfo()->data());
      flat_delta += static_cast<int>(it->rinfo()->pc() - prev_pc);
      if (txt[0] == ']') break;  // End of nested  comment
      // A new comment
      CollectCommentStatistics(isolate, it);
      // Skip code that was covered with previous comment
      prev_pc = it->rinfo()->pc();
    }
    it->next();
  }
  EnterComment(isolate, comment_txt, flat_delta);
}


// Collects code size statistics:
// - by code kind
// - by code comment
void PagedSpace::CollectCodeStatistics() {
  Isolate* isolate = heap()->isolate();
  HeapObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.Next(); obj != NULL; obj = obj_it.Next()) {
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      isolate->code_kind_statistics()[code->kind()] += code->Size();
      RelocIterator it(code);
      int delta = 0;
      const byte* prev_pc = code->instruction_start();
      while (!it.done()) {
        if (it.rinfo()->rmode() == RelocInfo::COMMENT) {
          delta += static_cast<int>(it.rinfo()->pc() - prev_pc);
          CollectCommentStatistics(isolate, &it);
          prev_pc = it.rinfo()->pc();
        }
        it.next();
      }

      ASSERT(code->instruction_start() <= prev_pc &&
             prev_pc <= code->instruction_end());
      delta += static_cast<int>(code->instruction_end() - prev_pc);
      EnterComment(isolate, "NoComment", delta);
    }
  }
}


void PagedSpace::ReportStatistics() {
  int pct = static_cast<int>(Available() * 100 / Capacity());
  PrintF("  capacity: %" V8_PTR_PREFIX "d"
             ", waste: %" V8_PTR_PREFIX "d"
             ", available: %" V8_PTR_PREFIX "d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  if (was_swept_conservatively_) return;
  ClearHistograms();
  HeapObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.Next(); obj != NULL; obj = obj_it.Next())
    CollectHistogramInfo(obj);
  ReportHistogram(true);
}
#endif

// -----------------------------------------------------------------------------
// FixedSpace implementation

void FixedSpace::PrepareForMarkCompact() {
  // Call prepare of the super class.
  PagedSpace::PrepareForMarkCompact();

  // During a non-compacting collection, everything below the linear
  // allocation pointer except wasted top-of-page blocks is considered
  // allocated and we will rediscover available bytes during the
  // collection.
  accounting_stats_.AllocateBytes(free_list_.available());

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_.Reset();
}


// -----------------------------------------------------------------------------
// MapSpace implementation
// TODO(mvstanton): this is weird...the compiler can't make a vtable unless
// there is at least one non-inlined virtual function. I would prefer to hide
// the VerifyObject definition behind VERIFY_HEAP.

void MapSpace::VerifyObject(HeapObject* object) {
  // The object should be a map or a free-list node.
  CHECK(object->IsMap() || object->IsFreeSpace());
}


// -----------------------------------------------------------------------------
// CellSpace and PropertyCellSpace implementation
// TODO(mvstanton): this is weird...the compiler can't make a vtable unless
// there is at least one non-inlined virtual function. I would prefer to hide
// the VerifyObject definition behind VERIFY_HEAP.

void CellSpace::VerifyObject(HeapObject* object) {
  // The object should be a global object property cell or a free-list node.
  CHECK(object->IsCell() ||
         object->map() == heap()->two_pointer_filler_map());
}


void PropertyCellSpace::VerifyObject(HeapObject* object) {
  // The object should be a global object property cell or a free-list node.
  CHECK(object->IsPropertyCell() ||
         object->map() == heap()->two_pointer_filler_map());
}


// -----------------------------------------------------------------------------
// LargeObjectIterator

LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space) {
  current_ = space->first_page_;
  size_func_ = NULL;
}


LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space,
                                         HeapObjectCallback size_func) {
  current_ = space->first_page_;
  size_func_ = size_func;
}


HeapObject* LargeObjectIterator::Next() {
  if (current_ == NULL) return NULL;

  HeapObject* object = current_->GetObject();
  current_ = current_->next_page();
  return object;
}


// -----------------------------------------------------------------------------
// LargeObjectSpace
static bool ComparePointers(void* key1, void* key2) {
    return key1 == key2;
}


LargeObjectSpace::LargeObjectSpace(Heap* heap,
                                   intptr_t max_capacity,
                                   AllocationSpace id)
    : Space(heap, id, NOT_EXECUTABLE),  // Managed on a per-allocation basis
      max_capacity_(max_capacity),
      first_page_(NULL),
      size_(0),
      page_count_(0),
      objects_size_(0),
      chunk_map_(ComparePointers, 1024) {}


bool LargeObjectSpace::SetUp() {
  first_page_ = NULL;
  size_ = 0;
  page_count_ = 0;
  objects_size_ = 0;
  chunk_map_.Clear();
  return true;
}


void LargeObjectSpace::TearDown() {
  while (first_page_ != NULL) {
    LargePage* page = first_page_;
    first_page_ = first_page_->next_page();
    LOG(heap()->isolate(), DeleteEvent("LargeObjectChunk", page->address()));

    ObjectSpace space = static_cast<ObjectSpace>(1 << identity());
    heap()->isolate()->memory_allocator()->PerformAllocationCallback(
        space, kAllocationActionFree, page->size());
    heap()->isolate()->memory_allocator()->Free(page);
  }
  SetUp();
}


MaybeObject* LargeObjectSpace::AllocateRaw(int object_size,
                                           Executability executable) {
  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->always_allocate() &&
      heap()->OldGenerationAllocationLimitReached()) {
    return Failure::RetryAfterGC(identity());
  }

  if (Size() + object_size > max_capacity_) {
    return Failure::RetryAfterGC(identity());
  }

  LargePage* page = heap()->isolate()->memory_allocator()->
      AllocateLargePage(object_size, this, executable);
  if (page == NULL) return Failure::RetryAfterGC(identity());
  ASSERT(page->area_size() >= object_size);

  size_ += static_cast<int>(page->size());
  objects_size_ += object_size;
  page_count_++;
  page->set_next_page(first_page_);
  first_page_ = page;

  // Register all MemoryChunk::kAlignment-aligned chunks covered by
  // this large page in the chunk map.
  uintptr_t base = reinterpret_cast<uintptr_t>(page) / MemoryChunk::kAlignment;
  uintptr_t limit = base + (page->size() - 1) / MemoryChunk::kAlignment;
  for (uintptr_t key = base; key <= limit; key++) {
    HashMap::Entry* entry = chunk_map_.Lookup(reinterpret_cast<void*>(key),
                                              static_cast<uint32_t>(key),
                                              true);
    ASSERT(entry != NULL);
    entry->value = page;
  }

  HeapObject* object = page->GetObject();

  if (Heap::ShouldZapGarbage()) {
    // Make the object consistent so the heap can be verified in OldSpaceStep.
    // We only need to do this in debug builds or if verify_heap is on.
    reinterpret_cast<Object**>(object->address())[0] =
        heap()->fixed_array_map();
    reinterpret_cast<Object**>(object->address())[1] = Smi::FromInt(0);
  }

  heap()->incremental_marking()->OldSpaceStep(object_size);
  return object;
}


size_t LargeObjectSpace::CommittedPhysicalMemory() {
  if (!VirtualMemory::HasLazyCommits()) return CommittedMemory();
  size_t size = 0;
  LargePage* current = first_page_;
  while (current != NULL) {
    size += current->CommittedPhysicalMemory();
    current = current->next_page();
  }
  return size;
}


// GC support
MaybeObject* LargeObjectSpace::FindObject(Address a) {
  LargePage* page = FindPage(a);
  if (page != NULL) {
    return page->GetObject();
  }
  return Failure::Exception();
}


LargePage* LargeObjectSpace::FindPage(Address a) {
  uintptr_t key = reinterpret_cast<uintptr_t>(a) / MemoryChunk::kAlignment;
  HashMap::Entry* e = chunk_map_.Lookup(reinterpret_cast<void*>(key),
                                        static_cast<uint32_t>(key),
                                        false);
  if (e != NULL) {
    ASSERT(e->value != NULL);
    LargePage* page = reinterpret_cast<LargePage*>(e->value);
    ASSERT(page->is_valid());
    if (page->Contains(a)) {
      return page;
    }
  }
  return NULL;
}


void LargeObjectSpace::FreeUnmarkedObjects() {
  LargePage* previous = NULL;
  LargePage* current = first_page_;
  while (current != NULL) {
    HeapObject* object = current->GetObject();
    // Can this large page contain pointers to non-trivial objects.  No other
    // pointer object is this big.
    bool is_pointer_object = object->IsFixedArray();
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    if (mark_bit.Get()) {
      mark_bit.Clear();
      Page::FromAddress(object->address())->ResetProgressBar();
      Page::FromAddress(object->address())->ResetLiveBytes();
      previous = current;
      current = current->next_page();
    } else {
      LargePage* page = current;
      // Cut the chunk out from the chunk list.
      current = current->next_page();
      if (previous == NULL) {
        first_page_ = current;
      } else {
        previous->set_next_page(current);
      }

      // Free the chunk.
      heap()->mark_compact_collector()->ReportDeleteIfNeeded(
          object, heap()->isolate());
      size_ -= static_cast<int>(page->size());
      objects_size_ -= object->Size();
      page_count_--;

      // Remove entries belonging to this page.
      // Use variable alignment to help pass length check (<= 80 characters)
      // of single line in tools/presubmit.py.
      const intptr_t alignment = MemoryChunk::kAlignment;
      uintptr_t base = reinterpret_cast<uintptr_t>(page)/alignment;
      uintptr_t limit = base + (page->size()-1)/alignment;
      for (uintptr_t key = base; key <= limit; key++) {
        chunk_map_.Remove(reinterpret_cast<void*>(key),
                          static_cast<uint32_t>(key));
      }

      if (is_pointer_object) {
        heap()->QueueMemoryChunkForFree(page);
      } else {
        heap()->isolate()->memory_allocator()->Free(page);
      }
    }
  }
  heap()->FreeQueuedChunks();
}


bool LargeObjectSpace::Contains(HeapObject* object) {
  Address address = object->address();
  MemoryChunk* chunk = MemoryChunk::FromAddress(address);

  bool owned = (chunk->owner() == this);

  SLOW_ASSERT(!owned || !FindObject(address)->IsFailure());

  return owned;
}


#ifdef VERIFY_HEAP
// We do not assume that the large object iterator works, because it depends
// on the invariants we are checking during verification.
void LargeObjectSpace::Verify() {
  for (LargePage* chunk = first_page_;
       chunk != NULL;
       chunk = chunk->next_page()) {
    // Each chunk contains an object that starts at the large object page's
    // object area start.
    HeapObject* object = chunk->GetObject();
    Page* page = Page::FromAddress(object->address());
    CHECK(object->address() == page->area_start());

    // The first word should be a map, and we expect all map pointers to be
    // in map space.
    Map* map = object->map();
    CHECK(map->IsMap());
    CHECK(heap()->map_space()->Contains(map));

    // We have only code, sequential strings, external strings
    // (sequential strings that have been morphed into external
    // strings), fixed arrays, and byte arrays in large object space.
    CHECK(object->IsCode() || object->IsSeqString() ||
           object->IsExternalString() || object->IsFixedArray() ||
           object->IsFixedDoubleArray() || object->IsByteArray());

    // The object itself should look OK.
    object->Verify();

    // Byte arrays and strings don't have interior pointers.
    if (object->IsCode()) {
      VerifyPointersVisitor code_visitor;
      object->IterateBody(map->instance_type(),
                          object->Size(),
                          &code_visitor);
    } else if (object->IsFixedArray()) {
      FixedArray* array = FixedArray::cast(object);
      for (int j = 0; j < array->length(); j++) {
        Object* element = array->get(j);
        if (element->IsHeapObject()) {
          HeapObject* element_object = HeapObject::cast(element);
          CHECK(heap()->Contains(element_object));
          CHECK(element_object->map()->IsMap());
        }
      }
    }
  }
}
#endif


#ifdef DEBUG
void LargeObjectSpace::Print() {
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    obj->Print();
  }
}


void LargeObjectSpace::ReportStatistics() {
  PrintF("  size: %" V8_PTR_PREFIX "d\n", size_);
  int num_objects = 0;
  ClearHistograms();
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    num_objects++;
    CollectHistogramInfo(obj);
  }

  PrintF("  number of objects %d, "
         "size of objects %" V8_PTR_PREFIX "d\n", num_objects, objects_size_);
  if (num_objects > 0) ReportHistogram(false);
}


void LargeObjectSpace::CollectCodeStatistics() {
  Isolate* isolate = heap()->isolate();
  LargeObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.Next(); obj != NULL; obj = obj_it.Next()) {
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      isolate->code_kind_statistics()[code->kind()] += code->Size();
    }
  }
}


void Page::Print() {
  // Make a best-effort to print the objects in the page.
  PrintF("Page@%p in %s\n",
         this->address(),
         AllocationSpaceName(this->owner()->identity()));
  printf(" --------------------------------------\n");
  HeapObjectIterator objects(this, heap()->GcSafeSizeOfOldObjectFunction());
  unsigned mark_size = 0;
  for (HeapObject* object = objects.Next();
       object != NULL;
       object = objects.Next()) {
    bool is_marked = Marking::MarkBitFrom(object).Get();
    PrintF(" %c ", (is_marked ? '!' : ' '));  // Indent a little.
    if (is_marked) {
      mark_size += heap()->GcSafeSizeOfOldObjectFunction()(object);
    }
    object->ShortPrint();
    PrintF("\n");
  }
  printf(" --------------------------------------\n");
  printf(" Marked: %x, LiveCount: %x\n", mark_size, LiveBytes());
}

#endif  // DEBUG

} }  // namespace v8::internal
