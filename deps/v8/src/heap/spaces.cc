// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"

#include "src/base/bits.h"
#include "src/base/platform/platform.h"
#include "src/full-codegen/full-codegen.h"
#include "src/heap/slot-set.h"
#include "src/heap/slots-buffer.h"
#include "src/macro-assembler.h"
#include "src/msan.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// HeapObjectIterator

HeapObjectIterator::HeapObjectIterator(PagedSpace* space) {
  // You can't actually iterate over the anchor page.  It is not a real page,
  // just an anchor for the double linked page list.  Initialize as if we have
  // reached the end of the anchor page, then the first iteration will move on
  // to the first page.
  Initialize(space, NULL, NULL, kAllPagesInSpace);
}


HeapObjectIterator::HeapObjectIterator(Page* page) {
  Space* owner = page->owner();
  DCHECK(owner == page->heap()->old_space() ||
         owner == page->heap()->map_space() ||
         owner == page->heap()->code_space());
  Initialize(reinterpret_cast<PagedSpace*>(owner), page->area_start(),
             page->area_end(), kOnePageOnly);
  DCHECK(page->SweepingDone());
}


void HeapObjectIterator::Initialize(PagedSpace* space, Address cur, Address end,
                                    HeapObjectIterator::PageMode mode) {
  space_ = space;
  cur_addr_ = cur;
  cur_end_ = end;
  page_mode_ = mode;
}


// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool HeapObjectIterator::AdvanceToNextPage() {
  DCHECK(cur_addr_ == cur_end_);
  if (page_mode_ == kOnePageOnly) return false;
  Page* cur_page;
  if (cur_addr_ == NULL) {
    cur_page = space_->anchor();
  } else {
    cur_page = Page::FromAddress(cur_addr_ - 1);
    DCHECK(cur_addr_ == cur_page->area_end());
  }
  cur_page = cur_page->next_page();
  if (cur_page == space_->anchor()) return false;
  cur_page->heap()->mark_compact_collector()->SweepOrWaitUntilSweepingCompleted(
      cur_page);
  cur_addr_ = cur_page->area_start();
  cur_end_ = cur_page->area_end();
  DCHECK(cur_page->SweepingDone());
  return true;
}

PauseAllocationObserversScope::PauseAllocationObserversScope(Heap* heap)
    : heap_(heap) {
  AllSpaces spaces(heap_);
  for (Space* space = spaces.next(); space != NULL; space = spaces.next()) {
    space->PauseAllocationObservers();
  }
}

PauseAllocationObserversScope::~PauseAllocationObserversScope() {
  AllSpaces spaces(heap_);
  for (Space* space = spaces.next(); space != NULL; space = spaces.next()) {
    space->ResumeAllocationObservers();
  }
}

// -----------------------------------------------------------------------------
// CodeRange


CodeRange::CodeRange(Isolate* isolate)
    : isolate_(isolate),
      code_range_(NULL),
      free_list_(0),
      allocation_list_(0),
      current_allocation_block_index_(0) {}


bool CodeRange::SetUp(size_t requested) {
  DCHECK(code_range_ == NULL);

  if (requested == 0) {
    // When a target requires the code range feature, we put all code objects
    // in a kMaximalCodeRangeSize range of virtual address space, so that
    // they can call each other with near calls.
    if (kRequiresCodeRange) {
      requested = kMaximalCodeRangeSize;
    } else {
      return true;
    }
  }

  if (requested <= kMinimumCodeRangeSize) {
    requested = kMinimumCodeRangeSize;
  }

  DCHECK(!kRequiresCodeRange || requested <= kMaximalCodeRangeSize);
#ifdef V8_TARGET_ARCH_MIPS64
  // To use pseudo-relative jumps such as j/jal instructions which have 28-bit
  // encoded immediate, the addresses have to be in range of 256Mb aligned
  // region.
  code_range_ = new base::VirtualMemory(requested, kMaximalCodeRangeSize);
#else
  code_range_ = new base::VirtualMemory(requested);
#endif
  CHECK(code_range_ != NULL);
  if (!code_range_->IsReserved()) {
    delete code_range_;
    code_range_ = NULL;
    return false;
  }

  // We are sure that we have mapped a block of requested addresses.
  DCHECK(code_range_->size() == requested);
  Address base = reinterpret_cast<Address>(code_range_->address());

  // On some platforms, specifically Win64, we need to reserve some pages at
  // the beginning of an executable space.
  if (kReservedCodeRangePages) {
    if (!code_range_->Commit(
            base, kReservedCodeRangePages * base::OS::CommitPageSize(), true)) {
      delete code_range_;
      code_range_ = NULL;
      return false;
    }
    base += kReservedCodeRangePages * base::OS::CommitPageSize();
  }
  Address aligned_base = RoundUp(base, MemoryChunk::kAlignment);
  size_t size = code_range_->size() - (aligned_base - base) -
                kReservedCodeRangePages * base::OS::CommitPageSize();
  allocation_list_.Add(FreeBlock(aligned_base, size));
  current_allocation_block_index_ = 0;

  LOG(isolate_, NewEvent("CodeRange", code_range_->address(), requested));
  return true;
}


int CodeRange::CompareFreeBlockAddress(const FreeBlock* left,
                                       const FreeBlock* right) {
  // The entire point of CodeRange is that the difference between two
  // addresses in the range can be represented as a signed 32-bit int,
  // so the cast is semantically correct.
  return static_cast<int>(left->start - right->start);
}


bool CodeRange::GetNextAllocationBlock(size_t requested) {
  for (current_allocation_block_index_++;
       current_allocation_block_index_ < allocation_list_.length();
       current_allocation_block_index_++) {
    if (requested <= allocation_list_[current_allocation_block_index_].size) {
      return true;  // Found a large enough allocation block.
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
      return true;  // Found a large enough allocation block.
    }
  }
  current_allocation_block_index_ = 0;
  // Code range is full or too fragmented.
  return false;
}


Address CodeRange::AllocateRawMemory(const size_t requested_size,
                                     const size_t commit_size,
                                     size_t* allocated) {
  // request_size includes guards while committed_size does not. Make sure
  // callers know about the invariant.
  CHECK_LE(commit_size,
           requested_size - 2 * MemoryAllocator::CodePageGuardSize());
  FreeBlock current;
  if (!ReserveBlock(requested_size, &current)) {
    *allocated = 0;
    return NULL;
  }
  *allocated = current.size;
  DCHECK(*allocated <= current.size);
  DCHECK(IsAddressAligned(current.start, MemoryChunk::kAlignment));
  if (!isolate_->memory_allocator()->CommitExecutableMemory(
          code_range_, current.start, commit_size, *allocated)) {
    *allocated = 0;
    ReleaseBlock(&current);
    return NULL;
  }
  return current.start;
}


bool CodeRange::CommitRawMemory(Address start, size_t length) {
  return isolate_->memory_allocator()->CommitMemory(start, length, EXECUTABLE);
}


bool CodeRange::UncommitRawMemory(Address start, size_t length) {
  return code_range_->Uncommit(start, length);
}


void CodeRange::FreeRawMemory(Address address, size_t length) {
  DCHECK(IsAddressAligned(address, MemoryChunk::kAlignment));
  base::LockGuard<base::Mutex> guard(&code_range_mutex_);
  free_list_.Add(FreeBlock(address, length));
  code_range_->Uncommit(address, length);
}


void CodeRange::TearDown() {
  delete code_range_;  // Frees all memory in the virtual memory range.
  code_range_ = NULL;
  base::LockGuard<base::Mutex> guard(&code_range_mutex_);
  free_list_.Free();
  allocation_list_.Free();
}


bool CodeRange::ReserveBlock(const size_t requested_size, FreeBlock* block) {
  base::LockGuard<base::Mutex> guard(&code_range_mutex_);
  DCHECK(allocation_list_.length() == 0 ||
         current_allocation_block_index_ < allocation_list_.length());
  if (allocation_list_.length() == 0 ||
      requested_size > allocation_list_[current_allocation_block_index_].size) {
    // Find an allocation block large enough.
    if (!GetNextAllocationBlock(requested_size)) return false;
  }
  // Commit the requested memory at the start of the current allocation block.
  size_t aligned_requested = RoundUp(requested_size, MemoryChunk::kAlignment);
  *block = allocation_list_[current_allocation_block_index_];
  // Don't leave a small free block, useless for a large object or chunk.
  if (aligned_requested < (block->size - Page::kPageSize)) {
    block->size = aligned_requested;
  }
  DCHECK(IsAddressAligned(block->start, MemoryChunk::kAlignment));
  allocation_list_[current_allocation_block_index_].start += block->size;
  allocation_list_[current_allocation_block_index_].size -= block->size;
  return true;
}


void CodeRange::ReleaseBlock(const FreeBlock* block) {
  base::LockGuard<base::Mutex> guard(&code_range_mutex_);
  free_list_.Add(*block);
}


// -----------------------------------------------------------------------------
// MemoryAllocator
//

MemoryAllocator::MemoryAllocator(Isolate* isolate)
    : isolate_(isolate),
      capacity_(0),
      capacity_executable_(0),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(reinterpret_cast<void*>(-1)),
      highest_ever_allocated_(reinterpret_cast<void*>(0)) {}


bool MemoryAllocator::SetUp(intptr_t capacity, intptr_t capacity_executable) {
  capacity_ = RoundUp(capacity, Page::kPageSize);
  capacity_executable_ = RoundUp(capacity_executable, Page::kPageSize);
  DCHECK_GE(capacity_, capacity_executable_);

  size_ = 0;
  size_executable_ = 0;

  return true;
}


void MemoryAllocator::TearDown() {
  // Check that spaces were torn down before MemoryAllocator.
  DCHECK(size_.Value() == 0);
  // TODO(gc) this will be true again when we fix FreeMemory.
  // DCHECK(size_executable_ == 0);
  capacity_ = 0;
  capacity_executable_ = 0;
}


bool MemoryAllocator::CommitMemory(Address base, size_t size,
                                   Executability executable) {
  if (!base::VirtualMemory::CommitRegion(base, size,
                                         executable == EXECUTABLE)) {
    return false;
  }
  UpdateAllocatedSpaceLimits(base, base + size);
  return true;
}


void MemoryAllocator::FreeNewSpaceMemory(Address addr,
                                         base::VirtualMemory* reservation,
                                         Executability executable) {
  LOG(isolate_, DeleteEvent("NewSpace", addr));

  DCHECK(reservation->IsReserved());
  const intptr_t size = static_cast<intptr_t>(reservation->size());
  DCHECK(size_.Value() >= size);
  size_.Increment(-size);
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));
  FreeMemory(reservation, NOT_EXECUTABLE);
}


void MemoryAllocator::FreeMemory(base::VirtualMemory* reservation,
                                 Executability executable) {
  // TODO(gc) make code_range part of memory allocator?
  // Code which is part of the code-range does not have its own VirtualMemory.
  DCHECK(isolate_->code_range() == NULL ||
         !isolate_->code_range()->contains(
             static_cast<Address>(reservation->address())));
  DCHECK(executable == NOT_EXECUTABLE || isolate_->code_range() == NULL ||
         !isolate_->code_range()->valid() ||
         reservation->size() <= Page::kPageSize);

  reservation->Release();
}


void MemoryAllocator::FreeMemory(Address base, size_t size,
                                 Executability executable) {
  // TODO(gc) make code_range part of memory allocator?
  if (isolate_->code_range() != NULL &&
      isolate_->code_range()->contains(static_cast<Address>(base))) {
    DCHECK(executable == EXECUTABLE);
    isolate_->code_range()->FreeRawMemory(base, size);
  } else {
    DCHECK(executable == NOT_EXECUTABLE || isolate_->code_range() == NULL ||
           !isolate_->code_range()->valid());
    bool result = base::VirtualMemory::ReleaseRegion(base, size);
    USE(result);
    DCHECK(result);
  }
}


Address MemoryAllocator::ReserveAlignedMemory(size_t size, size_t alignment,
                                              base::VirtualMemory* controller) {
  base::VirtualMemory reservation(size, alignment);

  if (!reservation.IsReserved()) return NULL;
  size_.Increment(static_cast<intptr_t>(reservation.size()));
  Address base =
      RoundUp(static_cast<Address>(reservation.address()), alignment);
  controller->TakeControl(&reservation);
  return base;
}


Address MemoryAllocator::AllocateAlignedMemory(
    size_t reserve_size, size_t commit_size, size_t alignment,
    Executability executable, base::VirtualMemory* controller) {
  DCHECK(commit_size <= reserve_size);
  base::VirtualMemory reservation;
  Address base = ReserveAlignedMemory(reserve_size, alignment, &reservation);
  if (base == NULL) return NULL;

  if (executable == EXECUTABLE) {
    if (!CommitExecutableMemory(&reservation, base, commit_size,
                                reserve_size)) {
      base = NULL;
    }
  } else {
    if (reservation.Commit(base, commit_size, false)) {
      UpdateAllocatedSpaceLimits(base, base + commit_size);
    } else {
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


NewSpacePage* NewSpacePage::Initialize(Heap* heap, Address start,
                                       SemiSpace* semi_space) {
  Address area_start = start + NewSpacePage::kObjectStartOffset;
  Address area_end = start + Page::kPageSize;

  MemoryChunk* chunk =
      MemoryChunk::Initialize(heap, start, Page::kPageSize, area_start,
                              area_end, NOT_EXECUTABLE, semi_space, nullptr);
  bool in_to_space = (semi_space->id() != kFromSpace);
  chunk->SetFlag(in_to_space ? MemoryChunk::IN_TO_SPACE
                             : MemoryChunk::IN_FROM_SPACE);
  DCHECK(!chunk->IsFlagSet(in_to_space ? MemoryChunk::IN_FROM_SPACE
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

MemoryChunk* MemoryChunk::Initialize(Heap* heap, Address base, size_t size,
                                     Address area_start, Address area_end,
                                     Executability executable, Space* owner,
                                     base::VirtualMemory* reservation) {
  MemoryChunk* chunk = FromAddress(base);

  DCHECK(base == chunk->address());

  chunk->heap_ = heap;
  chunk->size_ = size;
  chunk->area_start_ = area_start;
  chunk->area_end_ = area_end;
  chunk->flags_ = 0;
  chunk->set_owner(owner);
  chunk->InitializeReservedMemory();
  chunk->slots_buffer_ = nullptr;
  chunk->old_to_new_slots_ = nullptr;
  chunk->old_to_old_slots_ = nullptr;
  chunk->skip_list_ = nullptr;
  chunk->write_barrier_counter_ = kWriteBarrierCounterGranularity;
  chunk->progress_bar_ = 0;
  chunk->high_water_mark_.SetValue(static_cast<intptr_t>(area_start - base));
  chunk->concurrent_sweeping_state().SetValue(kSweepingDone);
  chunk->parallel_compaction_state().SetValue(kCompactingDone);
  chunk->mutex_ = nullptr;
  chunk->available_in_free_list_ = 0;
  chunk->wasted_memory_ = 0;
  chunk->ResetLiveBytes();
  Bitmap::Clear(chunk);
  chunk->set_next_chunk(nullptr);
  chunk->set_prev_chunk(nullptr);

  DCHECK(OFFSET_OF(MemoryChunk, flags_) == kFlagsOffset);
  DCHECK(OFFSET_OF(MemoryChunk, live_byte_count_) == kLiveBytesOffset);

  if (executable == EXECUTABLE) {
    chunk->SetFlag(IS_EXECUTABLE);
  }

  if (reservation != nullptr) {
    chunk->reservation_.TakeControl(reservation);
  }

  return chunk;
}


// Commit MemoryChunk area to the requested size.
bool MemoryChunk::CommitArea(size_t requested) {
  size_t guard_size =
      IsFlagSet(IS_EXECUTABLE) ? MemoryAllocator::CodePageGuardSize() : 0;
  size_t header_size = area_start() - address() - guard_size;
  size_t commit_size =
      RoundUp(header_size + requested, base::OS::CommitPageSize());
  size_t committed_size = RoundUp(header_size + (area_end() - area_start()),
                                  base::OS::CommitPageSize());

  if (commit_size > committed_size) {
    // Commit size should be less or equal than the reserved size.
    DCHECK(commit_size <= size() - 2 * guard_size);
    // Append the committed area.
    Address start = address() + committed_size + guard_size;
    size_t length = commit_size - committed_size;
    if (reservation_.IsReserved()) {
      Executability executable =
          IsFlagSet(IS_EXECUTABLE) ? EXECUTABLE : NOT_EXECUTABLE;
      if (!heap()->isolate()->memory_allocator()->CommitMemory(start, length,
                                                               executable)) {
        return false;
      }
    } else {
      CodeRange* code_range = heap_->isolate()->code_range();
      DCHECK(code_range != NULL && code_range->valid() &&
             IsFlagSet(IS_EXECUTABLE));
      if (!code_range->CommitRawMemory(start, length)) return false;
    }

    if (Heap::ShouldZapGarbage()) {
      heap_->isolate()->memory_allocator()->ZapBlock(start, length);
    }
  } else if (commit_size < committed_size) {
    DCHECK(commit_size > 0);
    // Shrink the committed area.
    size_t length = committed_size - commit_size;
    Address start = address() + committed_size + guard_size - length;
    if (reservation_.IsReserved()) {
      if (!reservation_.Uncommit(start, length)) return false;
    } else {
      CodeRange* code_range = heap_->isolate()->code_range();
      DCHECK(code_range != NULL && code_range->valid() &&
             IsFlagSet(IS_EXECUTABLE));
      if (!code_range->UncommitRawMemory(start, length)) return false;
    }
  }

  area_end_ = area_start_ + requested;
  return true;
}


void MemoryChunk::InsertAfter(MemoryChunk* other) {
  MemoryChunk* other_next = other->next_chunk();

  set_next_chunk(other_next);
  set_prev_chunk(other);
  other_next->set_prev_chunk(this);
  other->set_next_chunk(this);
}


void MemoryChunk::Unlink() {
  MemoryChunk* next_element = next_chunk();
  MemoryChunk* prev_element = prev_chunk();
  next_element->set_prev_chunk(prev_element);
  prev_element->set_next_chunk(next_element);
  set_prev_chunk(NULL);
  set_next_chunk(NULL);
}


MemoryChunk* MemoryAllocator::AllocateChunk(intptr_t reserve_area_size,
                                            intptr_t commit_area_size,
                                            Executability executable,
                                            Space* owner) {
  DCHECK(commit_area_size <= reserve_area_size);

  size_t chunk_size;
  Heap* heap = isolate_->heap();
  Address base = NULL;
  base::VirtualMemory reservation;
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
                         base::OS::CommitPageSize()) +
                 CodePageGuardSize();

    // Check executable memory limit.
    if ((size_executable_.Value() + static_cast<intptr_t>(chunk_size)) >
        capacity_executable_) {
      LOG(isolate_, StringEvent("MemoryAllocator::AllocateRawMemory",
                                "V8 Executable Allocation capacity exceeded"));
      return NULL;
    }

    // Size of header (not executable) plus area (executable).
    size_t commit_size = RoundUp(CodePageGuardStartOffset() + commit_area_size,
                                 base::OS::CommitPageSize());
    // Allocate executable memory either from code range or from the
    // OS.
#ifdef V8_TARGET_ARCH_MIPS64
    // Use code range only for large object space on mips64 to keep address
    // range within 256-MB memory region.
    if (isolate_->code_range() != NULL && isolate_->code_range()->valid() &&
        reserve_area_size > CodePageAreaSize()) {
#else
    if (isolate_->code_range() != NULL && isolate_->code_range()->valid()) {
#endif
      base = isolate_->code_range()->AllocateRawMemory(chunk_size, commit_size,
                                                       &chunk_size);
      DCHECK(
          IsAligned(reinterpret_cast<intptr_t>(base), MemoryChunk::kAlignment));
      if (base == NULL) return NULL;
      size_.Increment(static_cast<intptr_t>(chunk_size));
      // Update executable memory size.
      size_executable_.Increment(static_cast<intptr_t>(chunk_size));
    } else {
      base = AllocateAlignedMemory(chunk_size, commit_size,
                                   MemoryChunk::kAlignment, executable,
                                   &reservation);
      if (base == NULL) return NULL;
      // Update executable memory size.
      size_executable_.Increment(static_cast<intptr_t>(reservation.size()));
    }

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(base, CodePageGuardStartOffset());
      ZapBlock(base + CodePageAreaStartOffset(), commit_area_size);
    }

    area_start = base + CodePageAreaStartOffset();
    area_end = area_start + commit_area_size;
  } else {
    chunk_size = RoundUp(MemoryChunk::kObjectStartOffset + reserve_area_size,
                         base::OS::CommitPageSize());
    size_t commit_size =
        RoundUp(MemoryChunk::kObjectStartOffset + commit_area_size,
                base::OS::CommitPageSize());
    base =
        AllocateAlignedMemory(chunk_size, commit_size, MemoryChunk::kAlignment,
                              executable, &reservation);

    if (base == NULL) return NULL;

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(base, Page::kObjectStartOffset + commit_area_size);
    }

    area_start = base + Page::kObjectStartOffset;
    area_end = area_start + commit_area_size;
  }

  // Use chunk_size for statistics and callbacks because we assume that they
  // treat reserved but not-yet committed memory regions of chunks as allocated.
  isolate_->counters()->memory_allocated()->Increment(
      static_cast<int>(chunk_size));

  LOG(isolate_, NewEvent("MemoryChunk", base, chunk_size));
  if (owner != NULL) {
    ObjectSpace space = static_cast<ObjectSpace>(1 << owner->identity());
    PerformAllocationCallback(space, kAllocationActionAllocate, chunk_size);
  }

  return MemoryChunk::Initialize(heap, base, chunk_size, area_start, area_end,
                                 executable, owner, &reservation);
}


void Page::ResetFreeListStatistics() {
  wasted_memory_ = 0;
  available_in_free_list_ = 0;
}


Page* MemoryAllocator::AllocatePage(intptr_t size, PagedSpace* owner,
                                    Executability executable) {
  MemoryChunk* chunk = AllocateChunk(size, size, executable, owner);
  if (chunk == NULL) return NULL;
  return Page::Initialize(isolate_->heap(), chunk, executable, owner);
}


LargePage* MemoryAllocator::AllocateLargePage(intptr_t object_size,
                                              Space* owner,
                                              Executability executable) {
  MemoryChunk* chunk =
      AllocateChunk(object_size, object_size, executable, owner);
  if (chunk == NULL) return NULL;
  return LargePage::Initialize(isolate_->heap(), chunk);
}


void MemoryAllocator::PreFreeMemory(MemoryChunk* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));
  if (chunk->owner() != NULL) {
    ObjectSpace space =
        static_cast<ObjectSpace>(1 << chunk->owner()->identity());
    PerformAllocationCallback(space, kAllocationActionFree, chunk->size());
  }

  isolate_->heap()->RememberUnmappedPage(reinterpret_cast<Address>(chunk),
                                         chunk->IsEvacuationCandidate());

  intptr_t size;
  base::VirtualMemory* reservation = chunk->reserved_memory();
  if (reservation->IsReserved()) {
    size = static_cast<intptr_t>(reservation->size());
  } else {
    size = static_cast<intptr_t>(chunk->size());
  }
  DCHECK(size_.Value() >= size);
  size_.Increment(-size);
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));

  if (chunk->executable() == EXECUTABLE) {
    DCHECK(size_executable_.Value() >= size);
    size_executable_.Increment(-size);
  }

  chunk->SetFlag(MemoryChunk::PRE_FREED);
}


void MemoryAllocator::PerformFreeMemory(MemoryChunk* chunk) {
  DCHECK(chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  chunk->ReleaseAllocatedMemory();

  base::VirtualMemory* reservation = chunk->reserved_memory();
  if (reservation->IsReserved()) {
    FreeMemory(reservation, chunk->executable());
  } else {
    FreeMemory(chunk->address(), chunk->size(), chunk->executable());
  }
}


void MemoryAllocator::Free(MemoryChunk* chunk) {
  PreFreeMemory(chunk);
  PerformFreeMemory(chunk);
}


bool MemoryAllocator::CommitBlock(Address start, size_t size,
                                  Executability executable) {
  if (!CommitMemory(start, size, executable)) return false;

  if (Heap::ShouldZapGarbage()) {
    ZapBlock(start, size);
  }

  isolate_->counters()->memory_allocated()->Increment(static_cast<int>(size));
  return true;
}


bool MemoryAllocator::UncommitBlock(Address start, size_t size) {
  if (!base::VirtualMemory::UncommitRegion(start, size)) return false;
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
    MemoryAllocationCallback callback, ObjectSpace space,
    AllocationAction action) {
  DCHECK(callback != NULL);
  MemoryAllocationCallbackRegistration registration(callback, space, action);
  DCHECK(!MemoryAllocator::MemoryAllocationCallbackRegistered(callback));
  return memory_allocation_callbacks_.Add(registration);
}


void MemoryAllocator::RemoveMemoryAllocationCallback(
    MemoryAllocationCallback callback) {
  DCHECK(callback != NULL);
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
  intptr_t size = Size();
  float pct = static_cast<float>(capacity_ - size) / capacity_;
  PrintF("  capacity: %" V8_PTR_PREFIX
         "d"
         ", used: %" V8_PTR_PREFIX
         "d"
         ", available: %%%d\n\n",
         capacity_, size, static_cast<int>(pct * 100));
}
#endif


int MemoryAllocator::CodePageGuardStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return RoundUp(Page::kObjectStartOffset, base::OS::CommitPageSize());
}


int MemoryAllocator::CodePageGuardSize() {
  return static_cast<int>(base::OS::CommitPageSize());
}


int MemoryAllocator::CodePageAreaStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return CodePageGuardStartOffset() + CodePageGuardSize();
}


int MemoryAllocator::CodePageAreaEndOffset() {
  // We are guarding code pages: the last OS page will be protected as
  // non-writable.
  return Page::kPageSize - static_cast<int>(base::OS::CommitPageSize());
}


bool MemoryAllocator::CommitExecutableMemory(base::VirtualMemory* vm,
                                             Address start, size_t commit_size,
                                             size_t reserved_size) {
  // Commit page header (not executable).
  Address header = start;
  size_t header_size = CodePageGuardStartOffset();
  if (vm->Commit(header, header_size, false)) {
    // Create guard page after the header.
    if (vm->Guard(start + CodePageGuardStartOffset())) {
      // Commit page body (executable).
      Address body = start + CodePageAreaStartOffset();
      size_t body_size = commit_size - CodePageGuardStartOffset();
      if (vm->Commit(body, body_size, true)) {
        // Create guard page before the end.
        if (vm->Guard(start + reserved_size - CodePageGuardSize())) {
          UpdateAllocatedSpaceLimits(start, start + CodePageAreaStartOffset() +
                                                commit_size -
                                                CodePageGuardStartOffset());
          return true;
        }
        vm->Uncommit(body, body_size);
      }
    }
    vm->Uncommit(header, header_size);
  }
  return false;
}


// -----------------------------------------------------------------------------
// MemoryChunk implementation

void MemoryChunk::ReleaseAllocatedMemory() {
  delete slots_buffer_;
  slots_buffer_ = nullptr;
  delete skip_list_;
  skip_list_ = nullptr;
  delete mutex_;
  mutex_ = nullptr;
  ReleaseOldToNewSlots();
  ReleaseOldToOldSlots();
}

static SlotSet* AllocateSlotSet(size_t size, Address page_start) {
  size_t pages = (size + Page::kPageSize - 1) / Page::kPageSize;
  DCHECK(pages > 0);
  SlotSet* slot_set = new SlotSet[pages];
  for (size_t i = 0; i < pages; i++) {
    slot_set[i].SetPageStart(page_start + i * Page::kPageSize);
  }
  return slot_set;
}

void MemoryChunk::AllocateOldToNewSlots() {
  DCHECK(nullptr == old_to_new_slots_);
  old_to_new_slots_ = AllocateSlotSet(size_, address());
}

void MemoryChunk::ReleaseOldToNewSlots() {
  delete[] old_to_new_slots_;
  old_to_new_slots_ = nullptr;
}

void MemoryChunk::AllocateOldToOldSlots() {
  DCHECK(nullptr == old_to_old_slots_);
  old_to_old_slots_ = AllocateSlotSet(size_, address());
}

void MemoryChunk::ReleaseOldToOldSlots() {
  delete[] old_to_old_slots_;
  old_to_old_slots_ = nullptr;
}

// -----------------------------------------------------------------------------
// PagedSpace implementation

STATIC_ASSERT(static_cast<ObjectSpace>(1 << AllocationSpace::NEW_SPACE) ==
              ObjectSpace::kObjectSpaceNewSpace);
STATIC_ASSERT(static_cast<ObjectSpace>(1 << AllocationSpace::OLD_SPACE) ==
              ObjectSpace::kObjectSpaceOldSpace);
STATIC_ASSERT(static_cast<ObjectSpace>(1 << AllocationSpace::CODE_SPACE) ==
              ObjectSpace::kObjectSpaceCodeSpace);
STATIC_ASSERT(static_cast<ObjectSpace>(1 << AllocationSpace::MAP_SPACE) ==
              ObjectSpace::kObjectSpaceMapSpace);

void Space::AllocationStep(Address soon_object, int size) {
  if (!allocation_observers_paused_) {
    for (int i = 0; i < allocation_observers_->length(); ++i) {
      AllocationObserver* o = (*allocation_observers_)[i];
      o->AllocationStep(size, soon_object, size);
    }
  }
}

PagedSpace::PagedSpace(Heap* heap, AllocationSpace space,
                       Executability executable)
    : Space(heap, space, executable), free_list_(this) {
  area_size_ = MemoryAllocator::PageAreaSize(space);
  accounting_stats_.Clear();

  allocation_info_.Reset(nullptr, nullptr);

  anchor_.InitializeAsAnchor(this);
}


bool PagedSpace::SetUp() { return true; }


bool PagedSpace::HasBeenSetUp() { return true; }


void PagedSpace::TearDown() {
  PageIterator iterator(this);
  while (iterator.has_next()) {
    heap()->isolate()->memory_allocator()->Free(iterator.next());
  }
  anchor_.set_next_page(&anchor_);
  anchor_.set_prev_page(&anchor_);
  accounting_stats_.Clear();
}


void PagedSpace::AddMemory(Address start, intptr_t size) {
  accounting_stats_.ExpandSpace(static_cast<int>(size));
  Free(start, static_cast<int>(size));
}


void PagedSpace::RefillFreeList() {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  FreeList* free_list = nullptr;
  if (this == heap()->old_space()) {
    free_list = collector->free_list_old_space().get();
  } else if (this == heap()->code_space()) {
    free_list = collector->free_list_code_space().get();
  } else if (this == heap()->map_space()) {
    free_list = collector->free_list_map_space().get();
  } else {
    // Any PagedSpace might invoke RefillFreeList. We filter all but our old
    // generation spaces out.
    return;
  }
  DCHECK(free_list != nullptr);
  intptr_t added = free_list_.Concatenate(free_list);
  accounting_stats_.IncreaseCapacity(added);
}


void CompactionSpace::RefillFreeList() {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  FreeList* free_list = nullptr;
  if (identity() == OLD_SPACE) {
    free_list = collector->free_list_old_space().get();
  } else if (identity() == CODE_SPACE) {
    free_list = collector->free_list_code_space().get();
  } else {
    // Compaction spaces only represent old or code space.
    UNREACHABLE();
  }
  DCHECK(free_list != nullptr);
  intptr_t refilled = 0;
  while (refilled < kCompactionMemoryWanted) {
    FreeSpace* node =
        free_list->TryRemoveMemory(kCompactionMemoryWanted - refilled);
    if (node == nullptr) return;
    refilled += node->size();
    AddMemory(node->address(), node->size());
  }
}

void PagedSpace::MoveOverFreeMemory(PagedSpace* other) {
  DCHECK(identity() == other->identity());
  // Destroy the linear allocation space of {other}. This is needed to
  //   (a) not waste the memory and
  //   (b) keep the rest of the chunk in an iterable state (filler is needed).
  other->EmptyAllocationInfo();

  // Move over the free list. Concatenate makes sure that the source free list
  // gets properly reset after moving over all nodes.
  intptr_t added = free_list_.Concatenate(other->free_list());

  // Moved memory is not recorded as allocated memory, but rather increases and
  // decreases capacity of the corresponding spaces.
  other->accounting_stats_.DecreaseCapacity(added);
  accounting_stats_.IncreaseCapacity(added);
}


void PagedSpace::MergeCompactionSpace(CompactionSpace* other) {
  // Unmerged fields:
  //   area_size_
  //   anchor_

  MoveOverFreeMemory(other);

  // Update and clear accounting statistics.
  accounting_stats_.Merge(other->accounting_stats_);
  other->accounting_stats_.Clear();

  // The linear allocation area of {other} should be destroyed now.
  DCHECK(other->top() == nullptr);
  DCHECK(other->limit() == nullptr);

  AccountCommitted(other->CommittedMemory());

  // Move over pages.
  PageIterator it(other);
  Page* p = nullptr;
  while (it.has_next()) {
    p = it.next();
    p->Unlink();
    p->set_owner(this);
    p->InsertAfter(anchor_.prev_page());
  }
}


size_t PagedSpace::CommittedPhysicalMemory() {
  if (!base::VirtualMemory::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = 0;
  PageIterator it(this);
  while (it.has_next()) {
    size += it.next()->CommittedPhysicalMemory();
  }
  return size;
}

bool PagedSpace::ContainsSlow(Address addr) {
  Page* p = Page::FromAddress(addr);
  PageIterator iterator(this);
  while (iterator.has_next()) {
    if (iterator.next() == p) return true;
  }
  return false;
}


Object* PagedSpace::FindObject(Address addr) {
  // Note: this function can only be called on iterable spaces.
  DCHECK(!heap()->mark_compact_collector()->in_use());

  if (!Contains(addr)) return Smi::FromInt(0);  // Signaling not found.

  Page* p = Page::FromAddress(addr);
  HeapObjectIterator it(p);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    Address cur = obj->address();
    Address next = cur + obj->Size();
    if ((cur <= addr) && (addr < next)) return obj;
  }

  UNREACHABLE();
  return Smi::FromInt(0);
}


bool PagedSpace::CanExpand(size_t size) {
  DCHECK(heap()->mark_compact_collector()->is_compacting() ||
         Capacity() <= heap()->MaxOldGenerationSize());

  // Are we going to exceed capacity for this space? At this point we can be
  // way over the maximum size because of AlwaysAllocate scopes and large
  // objects.
  if (!heap()->CanExpandOldGeneration(static_cast<int>(size))) return false;

  return true;
}


bool PagedSpace::Expand() {
  intptr_t size = AreaSize();
  if (snapshotable() && !HasPages()) {
    size = Snapshot::SizeOfFirstPage(heap()->isolate(), identity());
  }

  if (!CanExpand(size)) return false;

  Page* p = heap()->isolate()->memory_allocator()->AllocatePage(size, this,
                                                                executable());
  if (p == NULL) return false;

  AccountCommitted(static_cast<intptr_t>(p->size()));

  // Pages created during bootstrapping may contain immortal immovable objects.
  if (!heap()->deserialization_complete()) p->MarkNeverEvacuate();

  DCHECK(Capacity() <= heap()->MaxOldGenerationSize());

  p->InsertAfter(anchor_.prev_page());

  return true;
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


void PagedSpace::ResetFreeListStatistics() {
  PageIterator page_iterator(this);
  while (page_iterator.has_next()) {
    Page* page = page_iterator.next();
    page->ResetFreeListStatistics();
  }
}


void PagedSpace::IncreaseCapacity(int size) {
  accounting_stats_.ExpandSpace(size);
}


void PagedSpace::ReleasePage(Page* page, bool evict_free_list_items) {
  DCHECK(page->LiveBytes() == 0);
  DCHECK(AreaSize() == page->area_size());

  if (evict_free_list_items) {
    intptr_t size = free_list_.EvictFreeListItems(page);
    accounting_stats_.AllocateBytes(size);
    DCHECK_EQ(AreaSize(), static_cast<int>(size));
  }

  DCHECK(!free_list_.ContainsPageFreeListItems(page));

  if (Page::FromAllocationTop(allocation_info_.top()) == page) {
    allocation_info_.Reset(nullptr, nullptr);
  }

  // If page is still in a list, unlink it from that list.
  if (page->next_chunk() != NULL) {
    DCHECK(page->prev_chunk() != NULL);
    page->Unlink();
  }

  AccountUncommitted(static_cast<intptr_t>(page->size()));
  heap()->QueueMemoryChunkForFree(page);

  DCHECK(Capacity() > 0);
  accounting_stats_.ShrinkSpace(AreaSize());
}


#ifdef DEBUG
void PagedSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpace::Verify(ObjectVisitor* visitor) {
  bool allocation_pointer_found_in_space =
      (allocation_info_.top() == allocation_info_.limit());
  PageIterator page_iterator(this);
  while (page_iterator.has_next()) {
    Page* page = page_iterator.next();
    CHECK(page->owner() == this);
    if (page == Page::FromAllocationTop(allocation_info_.top())) {
      allocation_pointer_found_in_space = true;
    }
    CHECK(page->SweepingDone());
    HeapObjectIterator it(page);
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
      object->ObjectVerify();

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
  Address base = heap()->isolate()->memory_allocator()->ReserveAlignedMemory(
      size, size, &reservation_);
  if (base == NULL) return false;

  chunk_base_ = base;
  chunk_size_ = static_cast<uintptr_t>(size);
  LOG(heap()->isolate(), NewEvent("InitialChunk", chunk_base_, chunk_size_));

  DCHECK(initial_semispace_capacity <= maximum_semispace_capacity);
  DCHECK(base::bits::IsPowerOfTwo32(maximum_semispace_capacity));

  // Allocate and set up the histogram arrays if necessary.
  allocated_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
  promoted_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);

#define SET_NAME(name)                        \
  allocated_histogram_[name].set_name(#name); \
  promoted_histogram_[name].set_name(#name);
  INSTANCE_TYPE_LIST(SET_NAME)
#undef SET_NAME

  DCHECK(reserved_semispace_capacity == heap()->ReservedSemiSpaceSize());
  DCHECK(static_cast<intptr_t>(chunk_size_) >=
         2 * heap()->ReservedSemiSpaceSize());
  DCHECK(IsAddressAligned(chunk_base_, 2 * reserved_semispace_capacity, 0));

  to_space_.SetUp(chunk_base_, initial_semispace_capacity,
                  maximum_semispace_capacity);
  from_space_.SetUp(chunk_base_ + reserved_semispace_capacity,
                    initial_semispace_capacity, maximum_semispace_capacity);
  if (!to_space_.Commit()) {
    return false;
  }
  DCHECK(!from_space_.is_committed());  // No need to use memory yet.

  start_ = chunk_base_;

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
  allocation_info_.Reset(nullptr, nullptr);


  to_space_.TearDown();
  from_space_.TearDown();

  heap()->isolate()->memory_allocator()->FreeNewSpaceMemory(
      chunk_base_, &reservation_, NOT_EXECUTABLE);

  chunk_base_ = NULL;
  chunk_size_ = 0;
}


void NewSpace::Flip() { SemiSpace::Swap(&from_space_, &to_space_); }


void NewSpace::Grow() {
  // Double the semispace size but only up to maximum capacity.
  DCHECK(TotalCapacity() < MaximumCapacity());
  int new_capacity =
      Min(MaximumCapacity(),
          FLAG_semi_space_growth_factor * static_cast<int>(TotalCapacity()));
  if (to_space_.GrowTo(new_capacity)) {
    // Only grow from space if we managed to grow to-space.
    if (!from_space_.GrowTo(new_capacity)) {
      // If we managed to grow to-space but couldn't grow from-space,
      // attempt to shrink to-space.
      if (!to_space_.ShrinkTo(from_space_.current_capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        CHECK(false);
      }
    }
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::Shrink() {
  int new_capacity = Max(InitialTotalCapacity(), 2 * SizeAsInt());
  int rounded_new_capacity = RoundUp(new_capacity, Page::kPageSize);
  if (rounded_new_capacity < TotalCapacity() &&
      to_space_.ShrinkTo(rounded_new_capacity)) {
    // Only shrink from-space if we managed to shrink to-space.
    from_space_.Reset();
    if (!from_space_.ShrinkTo(rounded_new_capacity)) {
      // If we managed to shrink to-space but couldn't shrink from
      // space, attempt to grow to-space again.
      if (!to_space_.GrowTo(from_space_.current_capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        CHECK(false);
      }
    }
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void LocalAllocationBuffer::Close() {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()));
  }
}


LocalAllocationBuffer::LocalAllocationBuffer(Heap* heap,
                                             AllocationInfo allocation_info)
    : heap_(heap), allocation_info_(allocation_info) {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()));
  }
}


LocalAllocationBuffer::LocalAllocationBuffer(
    const LocalAllocationBuffer& other) {
  *this = other;
}


LocalAllocationBuffer& LocalAllocationBuffer::operator=(
    const LocalAllocationBuffer& other) {
  Close();
  heap_ = other.heap_;
  allocation_info_ = other.allocation_info_;

  // This is needed since we (a) cannot yet use move-semantics, and (b) want
  // to make the use of the class easy by it as value and (c) implicitly call
  // {Close} upon copy.
  const_cast<LocalAllocationBuffer&>(other)
      .allocation_info_.Reset(nullptr, nullptr);
  return *this;
}


void NewSpace::UpdateAllocationInfo() {
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(to_space_.page_low(), to_space_.page_high());
  UpdateInlineAllocationLimit(0);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::ResetAllocationInfo() {
  Address old_top = allocation_info_.top();
  to_space_.Reset();
  UpdateAllocationInfo();
  pages_used_ = 0;
  // Clear all mark-bits in the to-space.
  NewSpacePageIterator it(&to_space_);
  while (it.has_next()) {
    Bitmap::Clear(it.next());
  }
  InlineAllocationStep(old_top, allocation_info_.top(), nullptr, 0);
}


void NewSpace::UpdateInlineAllocationLimit(int size_in_bytes) {
  if (heap()->inline_allocation_disabled()) {
    // Lowest limit when linear allocation was disabled.
    Address high = to_space_.page_high();
    Address new_top = allocation_info_.top() + size_in_bytes;
    allocation_info_.set_limit(Min(new_top, high));
  } else if (allocation_observers_paused_ || top_on_previous_step_ == 0) {
    // Normal limit is the end of the current page.
    allocation_info_.set_limit(to_space_.page_high());
  } else {
    // Lower limit during incremental marking.
    Address high = to_space_.page_high();
    Address new_top = allocation_info_.top() + size_in_bytes;
    Address new_limit = new_top + GetNextInlineAllocationStepSize() - 1;
    allocation_info_.set_limit(Min(new_limit, high));
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


bool NewSpace::AddFreshPage() {
  Address top = allocation_info_.top();
  DCHECK(!NewSpacePage::IsAtStart(top));
  if (!to_space_.AdvancePage()) {
    // No more pages left to advance.
    return false;
  }

  // Clear remainder of current page.
  Address limit = NewSpacePage::FromLimit(top)->area_end();
  if (heap()->gc_state() == Heap::SCAVENGE) {
    heap()->promotion_queue()->SetNewLimit(limit);
  }

  int remaining_in_page = static_cast<int>(limit - top);
  heap()->CreateFillerObjectAt(top, remaining_in_page);
  pages_used_++;
  UpdateAllocationInfo();

  return true;
}


bool NewSpace::AddFreshPageSynchronized() {
  base::LockGuard<base::Mutex> guard(&mutex_);
  return AddFreshPage();
}


bool NewSpace::EnsureAllocation(int size_in_bytes,
                                AllocationAlignment alignment) {
  Address old_top = allocation_info_.top();
  Address high = to_space_.page_high();
  int filler_size = Heap::GetFillToAlign(old_top, alignment);
  int aligned_size_in_bytes = size_in_bytes + filler_size;

  if (old_top + aligned_size_in_bytes >= high) {
    // Not enough room in the page, try to allocate a new one.
    if (!AddFreshPage()) {
      return false;
    }

    InlineAllocationStep(old_top, allocation_info_.top(), nullptr, 0);

    old_top = allocation_info_.top();
    high = to_space_.page_high();
    filler_size = Heap::GetFillToAlign(old_top, alignment);
    aligned_size_in_bytes = size_in_bytes + filler_size;
  }

  DCHECK(old_top + aligned_size_in_bytes < high);

  if (allocation_info_.limit() < high) {
    // Either the limit has been lowered because linear allocation was disabled
    // or because incremental marking wants to get a chance to do a step,
    // or because idle scavenge job wants to get a chance to post a task.
    // Set the new limit accordingly.
    Address new_top = old_top + aligned_size_in_bytes;
    Address soon_object = old_top + filler_size;
    InlineAllocationStep(new_top, new_top, soon_object, size_in_bytes);
    UpdateInlineAllocationLimit(aligned_size_in_bytes);
  }
  return true;
}


void NewSpace::StartNextInlineAllocationStep() {
  if (!allocation_observers_paused_) {
    top_on_previous_step_ =
        allocation_observers_->length() ? allocation_info_.top() : 0;
    UpdateInlineAllocationLimit(0);
  }
}


intptr_t NewSpace::GetNextInlineAllocationStepSize() {
  intptr_t next_step = 0;
  for (int i = 0; i < allocation_observers_->length(); ++i) {
    AllocationObserver* o = (*allocation_observers_)[i];
    next_step = next_step ? Min(next_step, o->bytes_to_next_step())
                          : o->bytes_to_next_step();
  }
  DCHECK(allocation_observers_->length() == 0 || next_step != 0);
  return next_step;
}

void NewSpace::AddAllocationObserver(AllocationObserver* observer) {
  Space::AddAllocationObserver(observer);
  StartNextInlineAllocationStep();
}

void NewSpace::RemoveAllocationObserver(AllocationObserver* observer) {
  Space::RemoveAllocationObserver(observer);
  StartNextInlineAllocationStep();
}

void NewSpace::PauseAllocationObservers() {
  // Do a step to account for memory allocated so far.
  InlineAllocationStep(top(), top(), nullptr, 0);
  Space::PauseAllocationObservers();
  top_on_previous_step_ = 0;
  UpdateInlineAllocationLimit(0);
}

void NewSpace::ResumeAllocationObservers() {
  DCHECK(top_on_previous_step_ == 0);
  Space::ResumeAllocationObservers();
  StartNextInlineAllocationStep();
}


void NewSpace::InlineAllocationStep(Address top, Address new_top,
                                    Address soon_object, size_t size) {
  if (top_on_previous_step_) {
    int bytes_allocated = static_cast<int>(top - top_on_previous_step_);
    for (int i = 0; i < allocation_observers_->length(); ++i) {
      (*allocation_observers_)[i]->AllocationStep(bytes_allocated, soon_object,
                                                  size);
    }
    top_on_previous_step_ = new_top;
  }
}

#ifdef VERIFY_HEAP
// We do not use the SemiSpaceIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void NewSpace::Verify() {
  // The allocation pointer should be in the space or at the very end.
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

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
      object->ObjectVerify();

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

void SemiSpace::SetUp(Address start, int initial_capacity,
                      int maximum_capacity) {
  DCHECK_GE(maximum_capacity, Page::kPageSize);
  minimum_capacity_ = RoundDown(initial_capacity, Page::kPageSize);
  current_capacity_ = minimum_capacity_;
  maximum_capacity_ = RoundDown(maximum_capacity, Page::kPageSize);
  committed_ = false;
  start_ = start;
  age_mark_ = start_ + NewSpacePage::kObjectStartOffset;
}


void SemiSpace::TearDown() {
  start_ = nullptr;
  current_capacity_ = 0;
}


bool SemiSpace::Commit() {
  DCHECK(!is_committed());
  if (!heap()->isolate()->memory_allocator()->CommitBlock(
          start_, current_capacity_, executable())) {
    return false;
  }
  AccountCommitted(current_capacity_);

  NewSpacePage* current = anchor();
  const int num_pages = current_capacity_ / Page::kPageSize;
  for (int i = 0; i < num_pages; i++) {
    NewSpacePage* new_page =
        NewSpacePage::Initialize(heap(), start_ + i * Page::kPageSize, this);
    new_page->InsertAfter(current);
    current = new_page;
  }
  Reset();

  set_current_capacity(current_capacity_);
  committed_ = true;
  return true;
}


bool SemiSpace::Uncommit() {
  DCHECK(is_committed());
  Address start = start_ + maximum_capacity_ - current_capacity_;
  if (!heap()->isolate()->memory_allocator()->UncommitBlock(
          start, current_capacity_)) {
    return false;
  }
  AccountUncommitted(current_capacity_);

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
  DCHECK_EQ(new_capacity & Page::kPageAlignmentMask, 0);
  DCHECK_LE(new_capacity, maximum_capacity_);
  DCHECK_GT(new_capacity, current_capacity_);
  int pages_before = current_capacity_ / Page::kPageSize;
  int pages_after = new_capacity / Page::kPageSize;

  size_t delta = new_capacity - current_capacity_;

  DCHECK(IsAligned(delta, base::OS::AllocateAlignment()));
  if (!heap()->isolate()->memory_allocator()->CommitBlock(
          start_ + current_capacity_, delta, executable())) {
    return false;
  }
  AccountCommitted(static_cast<intptr_t>(delta));
  set_current_capacity(new_capacity);
  NewSpacePage* last_page = anchor()->prev_page();
  DCHECK_NE(last_page, anchor());
  for (int i = pages_before; i < pages_after; i++) {
    Address page_address = start_ + i * Page::kPageSize;
    NewSpacePage* new_page =
        NewSpacePage::Initialize(heap(), page_address, this);
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
  DCHECK_EQ(new_capacity & Page::kPageAlignmentMask, 0);
  DCHECK_GE(new_capacity, minimum_capacity_);
  DCHECK_LT(new_capacity, current_capacity_);
  if (is_committed()) {
    size_t delta = current_capacity_ - new_capacity;
    DCHECK(IsAligned(delta, base::OS::AllocateAlignment()));

    MemoryAllocator* allocator = heap()->isolate()->memory_allocator();
    if (!allocator->UncommitBlock(start_ + new_capacity, delta)) {
      return false;
    }
    AccountUncommitted(static_cast<intptr_t>(delta));

    int pages_after = new_capacity / Page::kPageSize;
    NewSpacePage* new_last_page =
        NewSpacePage::FromAddress(start_ + (pages_after - 1) * Page::kPageSize);
    new_last_page->set_next_page(anchor());
    anchor()->set_prev_page(new_last_page);
    DCHECK((current_page_ >= first_page()) && (current_page_ <= new_last_page));
  }

  set_current_capacity(new_capacity);

  return true;
}

void SemiSpace::FixPagesFlags(intptr_t flags, intptr_t mask) {
  anchor_.set_owner(this);
  // Fixup back-pointers to anchor. Address of anchor changes when we swap.
  anchor_.prev_page()->set_next_page(&anchor_);
  anchor_.next_page()->set_prev_page(&anchor_);

  NewSpacePageIterator it(this);
  while (it.has_next()) {
    NewSpacePage* page = it.next();
    page->set_owner(this);
    page->SetFlags(flags, mask);
    if (id_ == kToSpace) {
      page->ClearFlag(MemoryChunk::IN_FROM_SPACE);
      page->SetFlag(MemoryChunk::IN_TO_SPACE);
      page->ClearFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
      page->ResetLiveBytes();
    } else {
      page->SetFlag(MemoryChunk::IN_FROM_SPACE);
      page->ClearFlag(MemoryChunk::IN_TO_SPACE);
    }
    DCHECK(page->IsFlagSet(MemoryChunk::IN_TO_SPACE) ||
           page->IsFlagSet(MemoryChunk::IN_FROM_SPACE));
  }
}


void SemiSpace::Reset() {
  DCHECK_NE(anchor_.next_page(), &anchor_);
  current_page_ = anchor_.next_page();
}


void SemiSpace::Swap(SemiSpace* from, SemiSpace* to) {
  // We won't be swapping semispaces without data in them.
  DCHECK_NE(from->anchor_.next_page(), &from->anchor_);
  DCHECK_NE(to->anchor_.next_page(), &to->anchor_);

  intptr_t saved_to_space_flags = to->current_page()->GetFlags();

  // We swap all properties but id_.
  std::swap(from->current_capacity_, to->current_capacity_);
  std::swap(from->maximum_capacity_, to->maximum_capacity_);
  std::swap(from->minimum_capacity_, to->minimum_capacity_);
  std::swap(from->start_, to->start_);
  std::swap(from->age_mark_, to->age_mark_);
  std::swap(from->committed_, to->committed_);
  std::swap(from->anchor_, to->anchor_);
  std::swap(from->current_page_, to->current_page_);

  to->FixPagesFlags(saved_to_space_flags, NewSpacePage::kCopyOnFlipFlagsMask);
  from->FixPagesFlags(0, 0);
}


void SemiSpace::set_age_mark(Address mark) {
  DCHECK_EQ(NewSpacePage::FromLimit(mark)->semi_space(), this);
  age_mark_ = mark;
  // Mark all pages up to the one containing mark.
  NewSpacePageIterator it(space_start(), mark);
  while (it.has_next()) {
    it.next()->SetFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
  }
}


#ifdef DEBUG
void SemiSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void SemiSpace::Verify() {
  bool is_from_space = (id_ == kFromSpace);
  NewSpacePage* page = anchor_.next_page();
  CHECK(anchor_.semi_space() == this);
  while (page != &anchor_) {
    CHECK_EQ(page->semi_space(), this);
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
        CHECK(
            !page->IsFlagSet(MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING));
      }
      // TODO(gc): Check that the live_bytes_count_ field matches the
      // black marking on the page (if we make it match in new-space).
    }
    CHECK_EQ(page->prev_page()->next_page(), page);
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
    CHECK_LE(start, end);
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
  Initialize(space->bottom(), space->top());
}


void SemiSpaceIterator::Initialize(Address start, Address end) {
  SemiSpace::AssertValidRange(start, end);
  current_ = start;
  limit_ = end;
}


#ifdef DEBUG
// heap_histograms is shared, always clear it before using it.
static void ClearHistograms(Isolate* isolate) {
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
  DCHECK(0 <= type && type <= LAST_TYPE);
  DCHECK(isolate->heap_histograms()[type].name() != NULL);
  isolate->heap_histograms()[type].increment_number(1);
  isolate->heap_histograms()[type].increment_bytes(obj->Size());

  if (FLAG_collect_heap_spill_statistics && obj->IsJSObject()) {
    JSObject::cast(obj)
        ->IncrementSpillStatistics(isolate->js_spill_information());
  }

  return obj->Size();
}


static void ReportHistogram(Isolate* isolate, bool print_spill) {
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
#define INCREMENT(type, size, name, camel_name)               \
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


static void DoReportStatistics(Isolate* isolate, HistogramInfo* info,
                               const char* description) {
  LOG(isolate, HeapSampleBeginEvent("NewSpace", description));
  // Lump all the string types together.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name) \
  string_number += info[type].number();         \
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
      LOG(isolate, HeapSampleItemEvent(info[i].name(), info[i].number(),
                                       info[i].bytes()));
    }
  }
  LOG(isolate, HeapSampleEndEvent("NewSpace", description));
}


void NewSpace::ReportStatistics() {
#ifdef DEBUG
  if (FLAG_heap_stats) {
    float pct = static_cast<float>(Available()) / TotalCapacity();
    PrintF("  capacity: %" V8_PTR_PREFIX
           "d"
           ", available: %" V8_PTR_PREFIX "d, %%%d\n",
           TotalCapacity(), Available(), static_cast<int>(pct * 100));
    PrintF("\n  Object Histogram:\n");
    for (int i = 0; i <= LAST_TYPE; i++) {
      if (allocated_histogram_[i].number() > 0) {
        PrintF("    %-34s%10d (%10d bytes)\n", allocated_histogram_[i].name(),
               allocated_histogram_[i].number(),
               allocated_histogram_[i].bytes());
      }
    }
    PrintF("\n");
  }
#endif  // DEBUG

  if (FLAG_log_gc) {
    Isolate* isolate = heap()->isolate();
    DoReportStatistics(isolate, allocated_histogram_, "allocated");
    DoReportStatistics(isolate, promoted_histogram_, "promoted");
  }
}


void NewSpace::RecordAllocation(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  DCHECK(0 <= type && type <= LAST_TYPE);
  allocated_histogram_[type].increment_number(1);
  allocated_histogram_[type].increment_bytes(obj->Size());
}


void NewSpace::RecordPromotion(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  DCHECK(0 <= type && type <= LAST_TYPE);
  promoted_histogram_[type].increment_number(1);
  promoted_histogram_[type].increment_bytes(obj->Size());
}


size_t NewSpace::CommittedPhysicalMemory() {
  if (!base::VirtualMemory::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = to_space_.CommittedPhysicalMemory();
  if (from_space_.is_committed()) {
    size += from_space_.CommittedPhysicalMemory();
  }
  return size;
}


// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation

intptr_t FreeListCategory::Concatenate(FreeListCategory* category) {
  intptr_t free_bytes = 0;
  if (category->top() != NULL) {
    DCHECK(category->end_ != NULL);
    free_bytes = category->available();
    if (end_ == NULL) {
      end_ = category->end();
    } else {
      category->end()->set_next(top());
    }
    set_top(category->top());
    available_ += category->available();
    category->Reset();
  }
  return free_bytes;
}


void FreeListCategory::Reset() {
  set_top(nullptr);
  set_end(nullptr);
  available_ = 0;
}


intptr_t FreeListCategory::EvictFreeListItemsInList(Page* p) {
  intptr_t sum = 0;
  FreeSpace* prev_node = nullptr;
  for (FreeSpace* cur_node = top(); cur_node != nullptr;
       cur_node = cur_node->next()) {
    Page* page_for_node = Page::FromAddress(cur_node->address());
    if (page_for_node == p) {
      // FreeSpace node on eviction page found, unlink it.
      int size = cur_node->size();
      sum += size;
      DCHECK((prev_node != nullptr) || (top() == cur_node));
      if (cur_node == top()) {
        set_top(cur_node->next());
      }
      if (cur_node == end()) {
        set_end(prev_node);
      }
      if (prev_node != nullptr) {
        prev_node->set_next(cur_node->next());
      }
      continue;
    }
    prev_node = cur_node;
  }
  p->add_available_in_free_list(-sum);
  available_ -= sum;
  return sum;
}


bool FreeListCategory::ContainsPageFreeListItemsInList(Page* p) {
  FreeSpace* node = top();
  while (node != NULL) {
    if (Page::FromAddress(node->address()) == p) return true;
    node = node->next();
  }
  return false;
}


FreeSpace* FreeListCategory::PickNodeFromList(int* node_size) {
  FreeSpace* node = top();
  if (node == nullptr) return nullptr;

  Page* page = Page::FromAddress(node->address());
  while ((node != nullptr) && !page->CanAllocate()) {
    available_ -= node->size();
    page->add_available_in_free_list(-(node->Size()));
    node = node->next();
  }

  if (node != nullptr) {
    set_top(node->next());
    *node_size = node->Size();
    available_ -= *node_size;
  } else {
    set_top(nullptr);
  }

  if (top() == nullptr) {
    set_end(nullptr);
  }

  return node;
}


FreeSpace* FreeListCategory::PickNodeFromList(int size_in_bytes,
                                              int* node_size) {
  FreeSpace* node = PickNodeFromList(node_size);
  if ((node != nullptr) && (*node_size < size_in_bytes)) {
    Free(node, *node_size);
    *node_size = 0;
    return nullptr;
  }
  return node;
}


FreeSpace* FreeListCategory::SearchForNodeInList(int size_in_bytes,
                                                 int* node_size) {
  FreeSpace* prev_non_evac_node = nullptr;
  for (FreeSpace* cur_node = top(); cur_node != nullptr;
       cur_node = cur_node->next()) {
    int size = cur_node->size();
    Page* page_for_node = Page::FromAddress(cur_node->address());

    if ((size >= size_in_bytes) || !page_for_node->CanAllocate()) {
      // The node is either large enough or contained in an evacuation
      // candidate. In both cases we need to unlink it from the list.
      available_ -= size;
      if (cur_node == top()) {
        set_top(cur_node->next());
      }
      if (cur_node == end()) {
        set_end(prev_non_evac_node);
      }
      if (prev_non_evac_node != nullptr) {
        prev_non_evac_node->set_next(cur_node->next());
      }
      // For evacuation candidates we continue.
      if (!page_for_node->CanAllocate()) {
        page_for_node->add_available_in_free_list(-size);
        continue;
      }
      // Otherwise we have a large enough node and can return.
      *node_size = size;
      return cur_node;
    }

    prev_non_evac_node = cur_node;
  }
  return nullptr;
}


void FreeListCategory::Free(FreeSpace* free_space, int size_in_bytes) {
  free_space->set_next(top());
  set_top(free_space);
  if (end_ == NULL) {
    end_ = free_space;
  }
  available_ += size_in_bytes;
}


void FreeListCategory::RepairFreeList(Heap* heap) {
  FreeSpace* n = top();
  while (n != NULL) {
    Map** map_location = reinterpret_cast<Map**>(n->address());
    if (*map_location == NULL) {
      *map_location = heap->free_space_map();
    } else {
      DCHECK(*map_location == heap->free_space_map());
    }
    n = n->next();
  }
}

FreeList::FreeList(PagedSpace* owner) : owner_(owner), wasted_bytes_(0) {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    category_[i].Initialize(this, static_cast<FreeListCategoryType>(i));
  }
  Reset();
}


intptr_t FreeList::Concatenate(FreeList* other) {
  intptr_t usable_bytes = 0;
  intptr_t wasted_bytes = 0;

  // This is safe (not going to deadlock) since Concatenate operations
  // are never performed on the same free lists at the same time in
  // reverse order. Furthermore, we only lock if the PagedSpace containing
  // the free list is know to be globally available, i.e., not local.
  if (!owner()->is_local()) mutex_.Lock();
  if (!other->owner()->is_local()) other->mutex()->Lock();

  wasted_bytes = other->wasted_bytes_;
  wasted_bytes_ += wasted_bytes;
  other->wasted_bytes_ = 0;

  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    usable_bytes += category_[i].Concatenate(
        other->GetFreeListCategory(static_cast<FreeListCategoryType>(i)));
  }

  if (!other->owner()->is_local()) other->mutex()->Unlock();
  if (!owner()->is_local()) mutex_.Unlock();
  return usable_bytes + wasted_bytes;
}


void FreeList::Reset() {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    category_[i].Reset();
  }
  ResetStats();
}


int FreeList::Free(Address start, int size_in_bytes) {
  if (size_in_bytes == 0) return 0;

  owner()->heap()->CreateFillerObjectAt(start, size_in_bytes);

  Page* page = Page::FromAddress(start);

  // Early return to drop too-small blocks on the floor.
  if (size_in_bytes <= kSmallListMin) {
    page->add_wasted_memory(size_in_bytes);
    wasted_bytes_ += size_in_bytes;
    return size_in_bytes;
  }

  FreeSpace* free_space = FreeSpace::cast(HeapObject::FromAddress(start));
  // Insert other blocks at the head of a free list of the appropriate
  // magnitude.
  FreeListCategoryType type = SelectFreeListCategoryType(size_in_bytes);
  category_[type].Free(free_space, size_in_bytes);
  page->add_available_in_free_list(size_in_bytes);

  DCHECK(IsVeryLong() || Available() == SumFreeLists());
  return 0;
}


FreeSpace* FreeList::FindNodeIn(FreeListCategoryType category, int* node_size) {
  FreeSpace* node = GetFreeListCategory(category)->PickNodeFromList(node_size);
  if (node != nullptr) {
    Page::FromAddress(node->address())
        ->add_available_in_free_list(-(*node_size));
    DCHECK(IsVeryLong() || Available() == SumFreeLists());
  }
  return node;
}


FreeSpace* FreeList::FindNodeFor(int size_in_bytes, int* node_size) {
  FreeSpace* node = nullptr;
  Page* page = nullptr;

  // First try the allocation fast path: try to allocate the minimum element
  // size of a free list category. This operation is constant time.
  FreeListCategoryType type =
      SelectFastAllocationFreeListCategoryType(size_in_bytes);
  for (int i = type; i < kHuge; i++) {
    node = FindNodeIn(static_cast<FreeListCategoryType>(i), node_size);
    if (node != nullptr) return node;
  }

  // Next search the huge list for free list nodes. This takes linear time in
  // the number of huge elements.
  node = category_[kHuge].SearchForNodeInList(size_in_bytes, node_size);
  if (node != nullptr) {
    page = Page::FromAddress(node->address());
    page->add_available_in_free_list(-(*node_size));
    DCHECK(IsVeryLong() || Available() == SumFreeLists());
    return node;
  }

  // We need a huge block of memory, but we didn't find anything in the huge
  // list.
  if (type == kHuge) return nullptr;

  // Now search the best fitting free list for a node that has at least the
  // requested size. This takes linear time in the number of elements.
  type = SelectFreeListCategoryType(size_in_bytes);
  node = category_[type].PickNodeFromList(size_in_bytes, node_size);
  if (node != nullptr) {
    DCHECK(size_in_bytes <= *node_size);
    page = Page::FromAddress(node->address());
    page->add_available_in_free_list(-(*node_size));
  }

  DCHECK(IsVeryLong() || Available() == SumFreeLists());
  return node;
}


FreeSpace* FreeList::TryRemoveMemory(intptr_t hint_size_in_bytes) {
  hint_size_in_bytes = RoundDown(hint_size_in_bytes, kPointerSize);
  base::LockGuard<base::Mutex> guard(&mutex_);
  FreeSpace* node = nullptr;
  int node_size = 0;
  // Try to find a node that fits exactly.
  node = FindNodeFor(static_cast<int>(hint_size_in_bytes), &node_size);
  // If no node could be found get as much memory as possible.
  if (node == nullptr) node = FindNodeIn(kHuge, &node_size);
  if (node == nullptr) node = FindNodeIn(kLarge, &node_size);
  if (node != nullptr) {
    // We round up the size to (kSmallListMin + kPointerSize) to (a) have a
    // size larger then the minimum size required for FreeSpace, and (b) to get
    // a block that can actually be freed into some FreeList later on.
    if (hint_size_in_bytes <= kSmallListMin) {
      hint_size_in_bytes = kSmallListMin + kPointerSize;
    }
    // Give back left overs that were not required by {size_in_bytes}.
    intptr_t left_over = node_size - hint_size_in_bytes;

    // Do not bother to return anything below {kSmallListMin} as it would be
    // immediately discarded anyways.
    if (left_over > kSmallListMin) {
      Free(node->address() + hint_size_in_bytes, static_cast<int>(left_over));
      node->set_size(static_cast<int>(hint_size_in_bytes));
    }
  }
  return node;
}


// Allocation on the old space free list.  If it succeeds then a new linear
// allocation space has been set up with the top and limit of the space.  If
// the allocation fails then NULL is returned, and the caller can perform a GC
// or allocate a new page before retrying.
HeapObject* FreeList::Allocate(int size_in_bytes) {
  DCHECK(0 < size_in_bytes);
  DCHECK(size_in_bytes <= kMaxBlockSize);
  DCHECK(IsAligned(size_in_bytes, kPointerSize));
  // Don't free list allocate if there is linear space available.
  DCHECK(owner_->limit() - owner_->top() < size_in_bytes);

  int old_linear_size = static_cast<int>(owner_->limit() - owner_->top());
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  owner_->Free(owner_->top(), old_linear_size);
  owner_->SetTopAndLimit(nullptr, nullptr);

  owner_->heap()->incremental_marking()->OldSpaceStep(size_in_bytes -
                                                      old_linear_size);

  int new_node_size = 0;
  FreeSpace* new_node = FindNodeFor(size_in_bytes, &new_node_size);
  if (new_node == nullptr) return nullptr;
  owner_->AllocationStep(new_node->address(), size_in_bytes);

  int bytes_left = new_node_size - size_in_bytes;
  DCHECK(bytes_left >= 0);

#ifdef DEBUG
  for (int i = 0; i < size_in_bytes / kPointerSize; i++) {
    reinterpret_cast<Object**>(new_node->address())[i] =
        Smi::FromInt(kCodeZapValue);
  }
#endif

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  const int kThreshold = IncrementalMarking::kAllocatedThreshold;

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  owner_->Allocate(new_node_size);

  if (owner_->heap()->inline_allocation_disabled()) {
    // Keep the linear allocation area empty if requested to do so, just
    // return area back to the free list instead.
    owner_->Free(new_node->address() + size_in_bytes, bytes_left);
    DCHECK(owner_->top() == NULL && owner_->limit() == NULL);
  } else if (bytes_left > kThreshold &&
             owner_->heap()->incremental_marking()->IsMarkingIncomplete() &&
             FLAG_incremental_marking) {
    int linear_size = owner_->RoundSizeDownToObjectAlignment(kThreshold);
    // We don't want to give too large linear areas to the allocator while
    // incremental marking is going on, because we won't check again whether
    // we want to do another increment until the linear area is used up.
    owner_->Free(new_node->address() + size_in_bytes + linear_size,
                 new_node_size - size_in_bytes - linear_size);
    owner_->SetTopAndLimit(new_node->address() + size_in_bytes,
                           new_node->address() + size_in_bytes + linear_size);
  } else if (bytes_left > 0) {
    // Normally we give the rest of the node to the allocator as its new
    // linear allocation area.
    owner_->SetTopAndLimit(new_node->address() + size_in_bytes,
                           new_node->address() + new_node_size);
  }

  return new_node;
}


intptr_t FreeList::EvictFreeListItems(Page* p) {
  intptr_t sum = category_[kHuge].EvictFreeListItemsInList(p);
  if (sum < p->area_size()) {
    for (int i = kFirstCategory; i <= kLarge; i++) {
      sum += category_[i].EvictFreeListItemsInList(p);
    }
  }
  return sum;
}


bool FreeList::ContainsPageFreeListItems(Page* p) {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    if (category_[i].EvictFreeListItemsInList(p)) {
      return true;
    }
  }
  return false;
}


void FreeList::RepairLists(Heap* heap) {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    category_[i].RepairFreeList(heap);
  }
}


#ifdef DEBUG
intptr_t FreeListCategory::SumFreeList() {
  intptr_t sum = 0;
  FreeSpace* cur = top();
  while (cur != NULL) {
    DCHECK(cur->map() == cur->GetHeap()->root(Heap::kFreeSpaceMapRootIndex));
    sum += cur->nobarrier_size();
    cur = cur->next();
  }
  return sum;
}


int FreeListCategory::FreeListLength() {
  int length = 0;
  FreeSpace* cur = top();
  while (cur != NULL) {
    length++;
    cur = cur->next();
    if (length == kVeryLongFreeList) return length;
  }
  return length;
}


bool FreeListCategory::IsVeryLong() {
  return FreeListLength() == kVeryLongFreeList;
}


bool FreeList::IsVeryLong() {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    if (category_[i].IsVeryLong()) {
      return true;
    }
  }
  return false;
}


// This can take a very long time because it is linear in the number of entries
// on the free list, so it should not be called if FreeListLength returns
// kVeryLongFreeList.
intptr_t FreeList::SumFreeLists() {
  intptr_t sum = 0;
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    sum += category_[i].SumFreeList();
  }
  return sum;
}
#endif


// -----------------------------------------------------------------------------
// OldSpace implementation

void PagedSpace::PrepareForMarkCompact() {
  // We don't have a linear allocation area while sweeping.  It will be restored
  // on the first allocation after the sweep.
  EmptyAllocationInfo();

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_.Reset();
}


intptr_t PagedSpace::SizeOfObjects() {
  const intptr_t size = Size() - (limit() - top());
  CHECK_GE(limit(), top());
  CHECK_GE(size, 0);
  USE(size);
  return size;
}


// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally NULL), so we need to
// fix them.
void PagedSpace::RepairFreeListsAfterDeserialization() {
  free_list_.RepairLists(heap());
  // Each page may have a small free space that is not tracked by a free list.
  // Update the maps for those free space objects.
  PageIterator iterator(this);
  while (iterator.has_next()) {
    Page* page = iterator.next();
    int size = static_cast<int>(page->wasted_memory());
    if (size == 0) continue;
    Address address = page->OffsetToAddress(Page::kPageSize - size);
    heap()->CreateFillerObjectAt(address, size);
  }
}


void PagedSpace::EvictEvacuationCandidatesFromLinearAllocationArea() {
  if (allocation_info_.top() >= allocation_info_.limit()) return;

  if (!Page::FromAllocationTop(allocation_info_.top())->CanAllocate()) {
    // Create filler object to keep page iterable if it was iterable.
    int remaining =
        static_cast<int>(allocation_info_.limit() - allocation_info_.top());
    heap()->CreateFillerObjectAt(allocation_info_.top(), remaining);
    allocation_info_.Reset(nullptr, nullptr);
  }
}


HeapObject* PagedSpace::SweepAndRetryAllocation(int size_in_bytes) {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    // Wait for the sweeper threads here and complete the sweeping phase.
    collector->EnsureSweepingCompleted();

    // After waiting for the sweeper threads, there may be new free-list
    // entries.
    return free_list_.Allocate(size_in_bytes);
  }
  return nullptr;
}


HeapObject* CompactionSpace::SweepAndRetryAllocation(int size_in_bytes) {
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->SweepAndRefill(this);
    return free_list_.Allocate(size_in_bytes);
  }
  return nullptr;
}


HeapObject* PagedSpace::SlowAllocateRaw(int size_in_bytes) {
  const int kMaxPagesToSweep = 1;

  // Allocation in this space has failed.

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList();

    // Retry the free list allocation.
    HeapObject* object = free_list_.Allocate(size_in_bytes);
    if (object != NULL) return object;

    // If sweeping is still in progress try to sweep pages on the main thread.
    int max_freed = collector->SweepInParallel(heap()->paged_space(identity()),
                                               size_in_bytes, kMaxPagesToSweep);
    RefillFreeList();
    if (max_freed >= size_in_bytes) {
      object = free_list_.Allocate(size_in_bytes);
      if (object != nullptr) return object;
    }
  }

  // Free list allocation failed and there is no next page.  Fail if we have
  // hit the old generation size limit that should cause a garbage
  // collection.
  if (!heap()->always_allocate() &&
      heap()->OldGenerationAllocationLimitReached()) {
    // If sweeper threads are active, wait for them at that point and steal
    // elements form their free-lists.
    HeapObject* object = SweepAndRetryAllocation(size_in_bytes);
    return object;
  }

  // Try to expand the space and allocate in the new next page.
  if (Expand()) {
    DCHECK((CountTotalPages() > 1) ||
           (size_in_bytes <= free_list_.Available()));
    return free_list_.Allocate(size_in_bytes);
  }

  // If sweeper threads are active, wait for them at that point and steal
  // elements form their free-lists. Allocation may still fail their which
  // would indicate that there is not enough memory for the given allocation.
  return SweepAndRetryAllocation(size_in_bytes);
}


#ifdef DEBUG
void PagedSpace::ReportCodeStatistics(Isolate* isolate) {
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  ReportCodeKindStatistics(isolate->code_kind_statistics());
  PrintF(
      "Code comment statistics (\"   [ comment-txt   :    size/   "
      "count  (average)\"):\n");
  for (int i = 0; i <= CommentStatistic::kMaxComments; i++) {
    const CommentStatistic& cs = comments_statistics[i];
    if (cs.size > 0) {
      PrintF("   %-30s: %10d/%6d     (%d)\n", cs.comment, cs.size, cs.count,
             cs.size / cs.count);
    }
  }
  PrintF("\n");
}


void PagedSpace::ResetCodeStatistics(Isolate* isolate) {
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
  DCHECK(!it->done());
  DCHECK(it->rinfo()->rmode() == RelocInfo::COMMENT);
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
    DCHECK(!it->done());
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

      DCHECK(code->instruction_start() <= prev_pc &&
             prev_pc <= code->instruction_end());
      delta += static_cast<int>(code->instruction_end() - prev_pc);
      EnterComment(isolate, "NoComment", delta);
    }
  }
}


void PagedSpace::ReportStatistics() {
  int pct = static_cast<int>(Available() * 100 / Capacity());
  PrintF("  capacity: %" V8_PTR_PREFIX
         "d"
         ", waste: %" V8_PTR_PREFIX
         "d"
         ", available: %" V8_PTR_PREFIX "d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  if (heap()->mark_compact_collector()->sweeping_in_progress()) {
    heap()->mark_compact_collector()->EnsureSweepingCompleted();
  }
  ClearHistograms(heap()->isolate());
  HeapObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.Next(); obj != NULL; obj = obj_it.Next())
    CollectHistogramInfo(obj);
  ReportHistogram(heap()->isolate(), true);
}
#endif


// -----------------------------------------------------------------------------
// MapSpace implementation

#ifdef VERIFY_HEAP
void MapSpace::VerifyObject(HeapObject* object) { CHECK(object->IsMap()); }
#endif


// -----------------------------------------------------------------------------
// LargeObjectIterator

LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space) {
  current_ = space->first_page_;
}


HeapObject* LargeObjectIterator::Next() {
  if (current_ == NULL) return NULL;

  HeapObject* object = current_->GetObject();
  current_ = current_->next_page();
  return object;
}


// -----------------------------------------------------------------------------
// LargeObjectSpace


LargeObjectSpace::LargeObjectSpace(Heap* heap, AllocationSpace id)
    : Space(heap, id, NOT_EXECUTABLE),  // Managed on a per-allocation basis
      first_page_(NULL),
      size_(0),
      page_count_(0),
      objects_size_(0),
      chunk_map_(HashMap::PointersMatch, 1024) {}


LargeObjectSpace::~LargeObjectSpace() {}


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


AllocationResult LargeObjectSpace::AllocateRaw(int object_size,
                                               Executability executable) {
  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->CanExpandOldGeneration(object_size)) {
    return AllocationResult::Retry(identity());
  }

  LargePage* page = heap()->isolate()->memory_allocator()->AllocateLargePage(
      object_size, this, executable);
  if (page == NULL) return AllocationResult::Retry(identity());
  DCHECK(page->area_size() >= object_size);

  size_ += static_cast<int>(page->size());
  AccountCommitted(static_cast<intptr_t>(page->size()));
  objects_size_ += object_size;
  page_count_++;
  page->set_next_page(first_page_);
  first_page_ = page;

  // Register all MemoryChunk::kAlignment-aligned chunks covered by
  // this large page in the chunk map.
  uintptr_t base = reinterpret_cast<uintptr_t>(page) / MemoryChunk::kAlignment;
  uintptr_t limit = base + (page->size() - 1) / MemoryChunk::kAlignment;
  for (uintptr_t key = base; key <= limit; key++) {
    HashMap::Entry* entry = chunk_map_.LookupOrInsert(
        reinterpret_cast<void*>(key), static_cast<uint32_t>(key));
    DCHECK(entry != NULL);
    entry->value = page;
  }

  HeapObject* object = page->GetObject();

  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object->address(), object_size);

  if (Heap::ShouldZapGarbage()) {
    // Make the object consistent so the heap can be verified in OldSpaceStep.
    // We only need to do this in debug builds or if verify_heap is on.
    reinterpret_cast<Object**>(object->address())[0] =
        heap()->fixed_array_map();
    reinterpret_cast<Object**>(object->address())[1] = Smi::FromInt(0);
  }

  heap()->incremental_marking()->OldSpaceStep(object_size);
  AllocationStep(object->address(), object_size);
  return object;
}


size_t LargeObjectSpace::CommittedPhysicalMemory() {
  if (!base::VirtualMemory::HasLazyCommits()) return CommittedMemory();
  size_t size = 0;
  LargePage* current = first_page_;
  while (current != NULL) {
    size += current->CommittedPhysicalMemory();
    current = current->next_page();
  }
  return size;
}


// GC support
Object* LargeObjectSpace::FindObject(Address a) {
  LargePage* page = FindPage(a);
  if (page != NULL) {
    return page->GetObject();
  }
  return Smi::FromInt(0);  // Signaling not found.
}


LargePage* LargeObjectSpace::FindPage(Address a) {
  uintptr_t key = reinterpret_cast<uintptr_t>(a) / MemoryChunk::kAlignment;
  HashMap::Entry* e = chunk_map_.Lookup(reinterpret_cast<void*>(key),
                                        static_cast<uint32_t>(key));
  if (e != NULL) {
    DCHECK(e->value != NULL);
    LargePage* page = reinterpret_cast<LargePage*>(e->value);
    DCHECK(LargePage::IsValid(page));
    if (page->Contains(a)) {
      return page;
    }
  }
  return NULL;
}


void LargeObjectSpace::ClearMarkingStateOfLiveObjects() {
  LargePage* current = first_page_;
  while (current != NULL) {
    HeapObject* object = current->GetObject();
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    DCHECK(Marking::IsBlack(mark_bit));
    Marking::BlackToWhite(mark_bit);
    Page::FromAddress(object->address())->ResetProgressBar();
    Page::FromAddress(object->address())->ResetLiveBytes();
    current = current->next_page();
  }
}


void LargeObjectSpace::FreeUnmarkedObjects() {
  LargePage* previous = NULL;
  LargePage* current = first_page_;
  while (current != NULL) {
    HeapObject* object = current->GetObject();
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    DCHECK(!Marking::IsGrey(mark_bit));
    if (Marking::IsBlack(mark_bit)) {
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
      heap()->mark_compact_collector()->ReportDeleteIfNeeded(object,
                                                             heap()->isolate());
      size_ -= static_cast<int>(page->size());
      AccountUncommitted(static_cast<intptr_t>(page->size()));
      objects_size_ -= object->Size();
      page_count_--;

      // Remove entries belonging to this page.
      // Use variable alignment to help pass length check (<= 80 characters)
      // of single line in tools/presubmit.py.
      const intptr_t alignment = MemoryChunk::kAlignment;
      uintptr_t base = reinterpret_cast<uintptr_t>(page) / alignment;
      uintptr_t limit = base + (page->size() - 1) / alignment;
      for (uintptr_t key = base; key <= limit; key++) {
        chunk_map_.Remove(reinterpret_cast<void*>(key),
                          static_cast<uint32_t>(key));
      }

      heap()->QueueMemoryChunkForFree(page);
    }
  }
}


bool LargeObjectSpace::Contains(HeapObject* object) {
  Address address = object->address();
  MemoryChunk* chunk = MemoryChunk::FromAddress(address);

  bool owned = (chunk->owner() == this);

  SLOW_DCHECK(!owned || FindObject(address)->IsHeapObject());

  return owned;
}


#ifdef VERIFY_HEAP
// We do not assume that the large object iterator works, because it depends
// on the invariants we are checking during verification.
void LargeObjectSpace::Verify() {
  for (LargePage* chunk = first_page_; chunk != NULL;
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
    // strings), fixed arrays, byte arrays, and constant pool arrays in the
    // large object space.
    CHECK(object->IsCode() || object->IsSeqString() ||
          object->IsExternalString() || object->IsFixedArray() ||
          object->IsFixedDoubleArray() || object->IsByteArray());

    // The object itself should look OK.
    object->ObjectVerify();

    // Byte arrays and strings don't have interior pointers.
    if (object->IsCode()) {
      VerifyPointersVisitor code_visitor;
      object->IterateBody(map->instance_type(), object->Size(), &code_visitor);
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
  OFStream os(stdout);
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    obj->Print(os);
  }
}


void LargeObjectSpace::ReportStatistics() {
  PrintF("  size: %" V8_PTR_PREFIX "d\n", size_);
  int num_objects = 0;
  ClearHistograms(heap()->isolate());
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    num_objects++;
    CollectHistogramInfo(obj);
  }

  PrintF(
      "  number of objects %d, "
      "size of objects %" V8_PTR_PREFIX "d\n",
      num_objects, objects_size_);
  if (num_objects > 0) ReportHistogram(heap()->isolate(), false);
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
  PrintF("Page@%p in %s\n", this->address(),
         AllocationSpaceName(this->owner()->identity()));
  printf(" --------------------------------------\n");
  HeapObjectIterator objects(this);
  unsigned mark_size = 0;
  for (HeapObject* object = objects.Next(); object != NULL;
       object = objects.Next()) {
    bool is_marked = Marking::IsBlackOrGrey(Marking::MarkBitFrom(object));
    PrintF(" %c ", (is_marked ? '!' : ' '));  // Indent a little.
    if (is_marked) {
      mark_size += object->Size();
    }
    object->ShortPrint();
    PrintF("\n");
  }
  printf(" --------------------------------------\n");
  printf(" Marked: %x, LiveCount: %x\n", mark_size, LiveBytes());
}

#endif  // DEBUG
}  // namespace internal
}  // namespace v8
