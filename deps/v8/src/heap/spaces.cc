// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"

#include <utility>

#include "src/base/bits.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/counters.h"
#include "src/full-codegen/full-codegen.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/slot-set.h"
#include "src/macro-assembler.h"
#include "src/msan.h"
#include "src/objects-inl.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// HeapObjectIterator

HeapObjectIterator::HeapObjectIterator(PagedSpace* space)
    : cur_addr_(nullptr),
      cur_end_(nullptr),
      space_(space),
      page_range_(space->anchor()->next_page(), space->anchor()),
      current_page_(page_range_.begin()) {}

HeapObjectIterator::HeapObjectIterator(Page* page)
    : cur_addr_(nullptr),
      cur_end_(nullptr),
      space_(reinterpret_cast<PagedSpace*>(page->owner())),
      page_range_(page),
      current_page_(page_range_.begin()) {
#ifdef DEBUG
  Space* owner = page->owner();
  DCHECK(owner == page->heap()->old_space() ||
         owner == page->heap()->map_space() ||
         owner == page->heap()->code_space());
#endif  // DEBUG
}

// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool HeapObjectIterator::AdvanceToNextPage() {
  DCHECK_EQ(cur_addr_, cur_end_);
  if (current_page_ == page_range_.end()) return false;
  Page* cur_page = *(current_page_++);
  Heap* heap = space_->heap();

  heap->mark_compact_collector()->sweeper().SweepOrWaitUntilSweepingCompleted(
      cur_page);
  if (cur_page->IsFlagSet(Page::SWEEP_TO_ITERATE))
    heap->minor_mark_compact_collector()->MakeIterable(
        cur_page, MarkingTreatmentMode::CLEAR,
        FreeSpaceTreatmentMode::IGNORE_FREE_SPACE);
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

  const size_t reserved_area =
      kReservedCodeRangePages * MemoryAllocator::GetCommitPageSize();
  if (requested < (kMaximalCodeRangeSize - reserved_area))
    requested += reserved_area;

  DCHECK(!kRequiresCodeRange || requested <= kMaximalCodeRangeSize);

  code_range_ = new base::VirtualMemory(
      requested, Max(kCodeRangeAreaAlignment,
                     static_cast<size_t>(base::OS::AllocateAlignment())));
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
  if (reserved_area > 0) {
    if (!code_range_->Commit(base, reserved_area, true)) {
      delete code_range_;
      code_range_ = NULL;
      return false;
    }
    base += reserved_area;
  }
  Address aligned_base = RoundUp(base, MemoryChunk::kAlignment);
  size_t size = code_range_->size() - (aligned_base - base) - reserved_area;
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
  if (!isolate_->heap()->memory_allocator()->CommitExecutableMemory(
          code_range_, current.start, commit_size, *allocated)) {
    *allocated = 0;
    ReleaseBlock(&current);
    return NULL;
  }
  return current.start;
}


bool CodeRange::CommitRawMemory(Address start, size_t length) {
  return isolate_->heap()->memory_allocator()->CommitMemory(start, length,
                                                            EXECUTABLE);
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
      code_range_(nullptr),
      capacity_(0),
      size_(0),
      size_executable_(0),
      lowest_ever_allocated_(reinterpret_cast<void*>(-1)),
      highest_ever_allocated_(reinterpret_cast<void*>(0)),
      unmapper_(this) {}

bool MemoryAllocator::SetUp(size_t capacity, size_t code_range_size) {
  capacity_ = RoundUp(capacity, Page::kPageSize);

  size_ = 0;
  size_executable_ = 0;

  code_range_ = new CodeRange(isolate_);
  if (!code_range_->SetUp(code_range_size)) return false;

  return true;
}


void MemoryAllocator::TearDown() {
  unmapper()->TearDown();

  // Check that spaces were torn down before MemoryAllocator.
  DCHECK_EQ(size_.Value(), 0u);
  // TODO(gc) this will be true again when we fix FreeMemory.
  // DCHECK(size_executable_ == 0);
  capacity_ = 0;

  if (last_chunk_.IsReserved()) {
    last_chunk_.Release();
  }

  delete code_range_;
  code_range_ = nullptr;
}

class MemoryAllocator::Unmapper::UnmapFreeMemoryTask : public v8::Task {
 public:
  explicit UnmapFreeMemoryTask(Unmapper* unmapper) : unmapper_(unmapper) {}

 private:
  // v8::Task overrides.
  void Run() override {
    unmapper_->PerformFreeMemoryOnQueuedChunks<FreeMode::kUncommitPooled>();
    unmapper_->pending_unmapping_tasks_semaphore_.Signal();
  }

  Unmapper* unmapper_;
  DISALLOW_COPY_AND_ASSIGN(UnmapFreeMemoryTask);
};

void MemoryAllocator::Unmapper::FreeQueuedChunks() {
  ReconsiderDelayedChunks();
  if (FLAG_concurrent_sweeping) {
    V8::GetCurrentPlatform()->CallOnBackgroundThread(
        new UnmapFreeMemoryTask(this), v8::Platform::kShortRunningTask);
    concurrent_unmapping_tasks_active_++;
  } else {
    PerformFreeMemoryOnQueuedChunks<FreeMode::kUncommitPooled>();
  }
}

bool MemoryAllocator::Unmapper::WaitUntilCompleted() {
  bool waited = false;
  while (concurrent_unmapping_tasks_active_ > 0) {
    pending_unmapping_tasks_semaphore_.Wait();
    concurrent_unmapping_tasks_active_--;
    waited = true;
  }
  return waited;
}

template <MemoryAllocator::Unmapper::FreeMode mode>
void MemoryAllocator::Unmapper::PerformFreeMemoryOnQueuedChunks() {
  MemoryChunk* chunk = nullptr;
  // Regular chunks.
  while ((chunk = GetMemoryChunkSafe<kRegular>()) != nullptr) {
    bool pooled = chunk->IsFlagSet(MemoryChunk::POOLED);
    allocator_->PerformFreeMemory(chunk);
    if (pooled) AddMemoryChunkSafe<kPooled>(chunk);
  }
  if (mode == MemoryAllocator::Unmapper::FreeMode::kReleasePooled) {
    // The previous loop uncommitted any pages marked as pooled and added them
    // to the pooled list. In case of kReleasePooled we need to free them
    // though.
    while ((chunk = GetMemoryChunkSafe<kPooled>()) != nullptr) {
      allocator_->Free<MemoryAllocator::kAlreadyPooled>(chunk);
    }
  }
  // Non-regular chunks.
  while ((chunk = GetMemoryChunkSafe<kNonRegular>()) != nullptr) {
    allocator_->PerformFreeMemory(chunk);
  }
}

void MemoryAllocator::Unmapper::TearDown() {
  WaitUntilCompleted();
  ReconsiderDelayedChunks();
  CHECK(delayed_regular_chunks_.empty());
  PerformFreeMemoryOnQueuedChunks<FreeMode::kReleasePooled>();
  for (int i = 0; i < kNumberOfChunkQueues; i++) {
    DCHECK(chunks_[i].empty());
  }
}

void MemoryAllocator::Unmapper::ReconsiderDelayedChunks() {
  std::list<MemoryChunk*> delayed_chunks(std::move(delayed_regular_chunks_));
  // Move constructed, so the permanent list should be empty.
  DCHECK(delayed_regular_chunks_.empty());
  for (auto it = delayed_chunks.begin(); it != delayed_chunks.end(); ++it) {
    AddMemoryChunkSafe<kRegular>(*it);
  }
}

bool MemoryAllocator::CanFreeMemoryChunk(MemoryChunk* chunk) {
  MarkCompactCollector* mc = isolate_->heap()->mark_compact_collector();
  // We cannot free a memory chunk in new space while the sweeper is running
  // because the memory chunk can be in the queue of a sweeper task.
  // Chunks in old generation are unmapped if they are empty.
  DCHECK(chunk->InNewSpace() || chunk->SweepingDone());
  return !chunk->InNewSpace() || mc == nullptr || !FLAG_concurrent_sweeping ||
         !mc->sweeper().sweeping_in_progress();
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


void MemoryAllocator::FreeMemory(base::VirtualMemory* reservation,
                                 Executability executable) {
  // TODO(gc) make code_range part of memory allocator?
  // Code which is part of the code-range does not have its own VirtualMemory.
  DCHECK(code_range() == NULL ||
         !code_range()->contains(static_cast<Address>(reservation->address())));
  DCHECK(executable == NOT_EXECUTABLE || !code_range()->valid() ||
         reservation->size() <= Page::kPageSize);

  reservation->Release();
}


void MemoryAllocator::FreeMemory(Address base, size_t size,
                                 Executability executable) {
  // TODO(gc) make code_range part of memory allocator?
  if (code_range() != NULL &&
      code_range()->contains(static_cast<Address>(base))) {
    DCHECK(executable == EXECUTABLE);
    code_range()->FreeRawMemory(base, size);
  } else {
    DCHECK(executable == NOT_EXECUTABLE || !code_range()->valid());
    bool result = base::VirtualMemory::ReleaseRegion(base, size);
    USE(result);
    DCHECK(result);
  }
}

Address MemoryAllocator::ReserveAlignedMemory(size_t size, size_t alignment,
                                              base::VirtualMemory* controller) {
  base::VirtualMemory reservation(size, alignment);

  if (!reservation.IsReserved()) return NULL;
  size_.Increment(reservation.size());
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
    size_.Decrement(reserve_size);
    return NULL;
  }

  controller->TakeControl(&reservation);
  return base;
}

void Page::InitializeAsAnchor(Space* space) {
  set_owner(space);
  set_next_chunk(this);
  set_prev_chunk(this);
  SetFlags(0, static_cast<uintptr_t>(~0));
  SetFlag(ANCHOR);
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
  chunk->flags_ = Flags(NO_FLAGS);
  chunk->set_owner(owner);
  chunk->InitializeReservedMemory();
  chunk->slot_set_[OLD_TO_NEW].SetValue(nullptr);
  chunk->slot_set_[OLD_TO_OLD].SetValue(nullptr);
  chunk->typed_slot_set_[OLD_TO_NEW].SetValue(nullptr);
  chunk->typed_slot_set_[OLD_TO_OLD].SetValue(nullptr);
  chunk->skip_list_ = nullptr;
  chunk->progress_bar_ = 0;
  chunk->high_water_mark_.SetValue(static_cast<intptr_t>(area_start - base));
  chunk->concurrent_sweeping_state().SetValue(kSweepingDone);
  chunk->mutex_ = new base::RecursiveMutex();
  chunk->available_in_free_list_ = 0;
  chunk->wasted_memory_ = 0;
  chunk->young_generation_bitmap_ = nullptr;
  chunk->set_next_chunk(nullptr);
  chunk->set_prev_chunk(nullptr);
  chunk->local_tracker_ = nullptr;

  MarkingState::Internal(chunk).ClearLiveness();

  DCHECK(OFFSET_OF(MemoryChunk, flags_) == kFlagsOffset);

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
      RoundUp(header_size + requested, MemoryAllocator::GetCommitPageSize());
  size_t committed_size = RoundUp(header_size + (area_end() - area_start()),
                                  MemoryAllocator::GetCommitPageSize());

  if (commit_size > committed_size) {
    // Commit size should be less or equal than the reserved size.
    DCHECK(commit_size <= size() - 2 * guard_size);
    // Append the committed area.
    Address start = address() + committed_size + guard_size;
    size_t length = commit_size - committed_size;
    if (reservation_.IsReserved()) {
      Executability executable =
          IsFlagSet(IS_EXECUTABLE) ? EXECUTABLE : NOT_EXECUTABLE;
      if (!heap()->memory_allocator()->CommitMemory(start, length,
                                                    executable)) {
        return false;
      }
    } else {
      CodeRange* code_range = heap_->memory_allocator()->code_range();
      DCHECK(code_range->valid() && IsFlagSet(IS_EXECUTABLE));
      if (!code_range->CommitRawMemory(start, length)) return false;
    }

    if (Heap::ShouldZapGarbage()) {
      heap_->memory_allocator()->ZapBlock(start, length);
    }
  } else if (commit_size < committed_size) {
    DCHECK(commit_size > 0);
    // Shrink the committed area.
    size_t length = committed_size - commit_size;
    Address start = address() + committed_size + guard_size - length;
    if (reservation_.IsReserved()) {
      if (!reservation_.Uncommit(start, length)) return false;
    } else {
      CodeRange* code_range = heap_->memory_allocator()->code_range();
      DCHECK(code_range->valid() && IsFlagSet(IS_EXECUTABLE));
      if (!code_range->UncommitRawMemory(start, length)) return false;
    }
  }

  area_end_ = area_start_ + requested;
  return true;
}

size_t MemoryChunk::CommittedPhysicalMemory() {
  if (!base::VirtualMemory::HasLazyCommits() || owner()->identity() == LO_SPACE)
    return size();
  return high_water_mark_.Value();
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

void MemoryAllocator::ShrinkChunk(MemoryChunk* chunk, size_t bytes_to_shrink) {
  DCHECK_GE(bytes_to_shrink, static_cast<size_t>(GetCommitPageSize()));
  DCHECK_EQ(0u, bytes_to_shrink % GetCommitPageSize());
  Address free_start = chunk->area_end_ - bytes_to_shrink;
  // Don't adjust the size of the page. The area is just uncomitted but not
  // released.
  chunk->area_end_ -= bytes_to_shrink;
  UncommitBlock(free_start, bytes_to_shrink);
  if (chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE)) {
    if (chunk->reservation_.IsReserved())
      chunk->reservation_.Guard(chunk->area_end_);
    else
      base::OS::Guard(chunk->area_end_, GetCommitPageSize());
  }
}

MemoryChunk* MemoryAllocator::AllocateChunk(size_t reserve_area_size,
                                            size_t commit_area_size,
                                            Executability executable,
                                            Space* owner) {
  DCHECK_LE(commit_area_size, reserve_area_size);

  size_t chunk_size;
  Heap* heap = isolate_->heap();
  Address base = nullptr;
  base::VirtualMemory reservation;
  Address area_start = nullptr;
  Address area_end = nullptr;

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
                         GetCommitPageSize()) +
                 CodePageGuardSize();

    // Size of header (not executable) plus area (executable).
    size_t commit_size = RoundUp(CodePageGuardStartOffset() + commit_area_size,
                                 GetCommitPageSize());
    // Allocate executable memory either from code range or from the
    // OS.
#ifdef V8_TARGET_ARCH_MIPS64
    // Use code range only for large object space on mips64 to keep address
    // range within 256-MB memory region.
    if (code_range()->valid() && reserve_area_size > CodePageAreaSize()) {
#else
    if (code_range()->valid()) {
#endif
      base =
          code_range()->AllocateRawMemory(chunk_size, commit_size, &chunk_size);
      DCHECK(
          IsAligned(reinterpret_cast<intptr_t>(base), MemoryChunk::kAlignment));
      if (base == NULL) return NULL;
      size_.Increment(chunk_size);
      // Update executable memory size.
      size_executable_.Increment(chunk_size);
    } else {
      base = AllocateAlignedMemory(chunk_size, commit_size,
                                   MemoryChunk::kAlignment, executable,
                                   &reservation);
      if (base == NULL) return NULL;
      // Update executable memory size.
      size_executable_.Increment(reservation.size());
    }

    if (Heap::ShouldZapGarbage()) {
      ZapBlock(base, CodePageGuardStartOffset());
      ZapBlock(base + CodePageAreaStartOffset(), commit_area_size);
    }

    area_start = base + CodePageAreaStartOffset();
    area_end = area_start + commit_area_size;
  } else {
    chunk_size = RoundUp(MemoryChunk::kObjectStartOffset + reserve_area_size,
                         GetCommitPageSize());
    size_t commit_size =
        RoundUp(MemoryChunk::kObjectStartOffset + commit_area_size,
                GetCommitPageSize());
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

  // We cannot use the last chunk in the address space because we would
  // overflow when comparing top and limit if this chunk is used for a
  // linear allocation area.
  if ((reinterpret_cast<uintptr_t>(base) + chunk_size) == 0u) {
    CHECK(!last_chunk_.IsReserved());
    last_chunk_.TakeControl(&reservation);
    UncommitBlock(reinterpret_cast<Address>(last_chunk_.address()),
                  last_chunk_.size());
    size_.Decrement(chunk_size);
    if (executable == EXECUTABLE) {
      size_executable_.Decrement(chunk_size);
    }
    CHECK(last_chunk_.IsReserved());
    return AllocateChunk(reserve_area_size, commit_area_size, executable,
                         owner);
  }

  return MemoryChunk::Initialize(heap, base, chunk_size, area_start, area_end,
                                 executable, owner, &reservation);
}


void Page::ResetFreeListStatistics() {
  wasted_memory_ = 0;
  available_in_free_list_ = 0;
}

size_t Page::AvailableInFreeList() {
  size_t sum = 0;
  ForAllFreeListCategories([&sum](FreeListCategory* category) {
    sum += category->available();
  });
  return sum;
}

size_t Page::ShrinkToHighWaterMark() {
  // Shrink pages to high water mark. The water mark points either to a filler
  // or the area_end.
  HeapObject* filler = HeapObject::FromAddress(HighWaterMark());
  if (filler->address() == area_end()) return 0;
  CHECK(filler->IsFiller());
  if (!filler->IsFreeSpace()) return 0;

#ifdef DEBUG
  // Check the the filler is indeed the last filler on the page.
  HeapObjectIterator it(this);
  HeapObject* filler2 = nullptr;
  for (HeapObject* obj = it.Next(); obj != nullptr; obj = it.Next()) {
    filler2 = HeapObject::FromAddress(obj->address() + obj->Size());
  }
  if (filler2 == nullptr || filler2->address() == area_end()) return 0;
  DCHECK(filler2->IsFiller());
  // The deserializer might leave behind fillers. In this case we need to
  // iterate even further.
  while ((filler2->address() + filler2->Size()) != area_end()) {
    filler2 = HeapObject::FromAddress(filler2->address() + filler2->Size());
    DCHECK(filler2->IsFiller());
  }
  DCHECK_EQ(filler->address(), filler2->address());
#endif  // DEBUG

  size_t unused = RoundDown(
      static_cast<size_t>(area_end() - filler->address() - FreeSpace::kSize),
      MemoryAllocator::GetCommitPageSize());
  if (unused > 0) {
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Shrinking page %p: end %p -> %p\n",
                   reinterpret_cast<void*>(this),
                   reinterpret_cast<void*>(area_end()),
                   reinterpret_cast<void*>(area_end() - unused));
    }
    heap()->CreateFillerObjectAt(
        filler->address(),
        static_cast<int>(area_end() - filler->address() - unused),
        ClearRecordedSlots::kNo);
    heap()->memory_allocator()->ShrinkChunk(this, unused);
    CHECK(filler->IsFiller());
    CHECK_EQ(filler->address() + filler->Size(), area_end());
  }
  return unused;
}

void Page::CreateBlackArea(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_NE(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  MarkingState::Internal(this).bitmap()->SetRange(AddressToMarkbitIndex(start),
                                                  AddressToMarkbitIndex(end));
  MarkingState::Internal(this).IncrementLiveBytes(
      static_cast<int>(end - start));
}

void Page::DestroyBlackArea(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_NE(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  MarkingState::Internal(this).bitmap()->ClearRange(
      AddressToMarkbitIndex(start), AddressToMarkbitIndex(end));
  MarkingState::Internal(this).IncrementLiveBytes(
      -static_cast<int>(end - start));
}

void MemoryAllocator::PartialFreeMemory(MemoryChunk* chunk,
                                        Address start_free) {
  // We do not allow partial shrink for code.
  DCHECK(chunk->executable() == NOT_EXECUTABLE);

  intptr_t size;
  base::VirtualMemory* reservation = chunk->reserved_memory();
  DCHECK(reservation->IsReserved());
  size = static_cast<intptr_t>(reservation->size());

  size_t to_free_size = size - (start_free - chunk->address());

  DCHECK(size_.Value() >= to_free_size);
  size_.Decrement(to_free_size);
  isolate_->counters()->memory_allocated()->Decrement(
      static_cast<int>(to_free_size));
  chunk->set_size(size - to_free_size);

  reservation->ReleasePartial(start_free);
}

void MemoryAllocator::PreFreeMemory(MemoryChunk* chunk) {
  DCHECK(!chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  LOG(isolate_, DeleteEvent("MemoryChunk", chunk));

  isolate_->heap()->RememberUnmappedPage(reinterpret_cast<Address>(chunk),
                                         chunk->IsEvacuationCandidate());

  base::VirtualMemory* reservation = chunk->reserved_memory();
  const size_t size =
      reservation->IsReserved() ? reservation->size() : chunk->size();
  DCHECK_GE(size_.Value(), static_cast<size_t>(size));
  size_.Decrement(size);
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));
  if (chunk->executable() == EXECUTABLE) {
    DCHECK_GE(size_executable_.Value(), size);
    size_executable_.Decrement(size);
  }

  chunk->SetFlag(MemoryChunk::PRE_FREED);
}


void MemoryAllocator::PerformFreeMemory(MemoryChunk* chunk) {
  DCHECK(chunk->IsFlagSet(MemoryChunk::PRE_FREED));
  chunk->ReleaseAllocatedMemory();

  base::VirtualMemory* reservation = chunk->reserved_memory();
  if (chunk->IsFlagSet(MemoryChunk::POOLED)) {
    UncommitBlock(reinterpret_cast<Address>(chunk), MemoryChunk::kPageSize);
  } else {
    if (reservation->IsReserved()) {
      FreeMemory(reservation, chunk->executable());
    } else {
      FreeMemory(chunk->address(), chunk->size(), chunk->executable());
    }
  }
}

template <MemoryAllocator::FreeMode mode>
void MemoryAllocator::Free(MemoryChunk* chunk) {
  switch (mode) {
    case kFull:
      PreFreeMemory(chunk);
      PerformFreeMemory(chunk);
      break;
    case kAlreadyPooled:
      // Pooled pages cannot be touched anymore as their memory is uncommitted.
      FreeMemory(chunk->address(), static_cast<size_t>(MemoryChunk::kPageSize),
                 Executability::NOT_EXECUTABLE);
      break;
    case kPooledAndQueue:
      DCHECK_EQ(chunk->size(), static_cast<size_t>(MemoryChunk::kPageSize));
      DCHECK_EQ(chunk->executable(), NOT_EXECUTABLE);
      chunk->SetFlag(MemoryChunk::POOLED);
    // Fall through to kPreFreeAndQueue.
    case kPreFreeAndQueue:
      PreFreeMemory(chunk);
      // The chunks added to this queue will be freed by a concurrent thread.
      unmapper()->AddMemoryChunkSafe(chunk);
      break;
  }
}

template void MemoryAllocator::Free<MemoryAllocator::kFull>(MemoryChunk* chunk);

template void MemoryAllocator::Free<MemoryAllocator::kAlreadyPooled>(
    MemoryChunk* chunk);

template void MemoryAllocator::Free<MemoryAllocator::kPreFreeAndQueue>(
    MemoryChunk* chunk);

template void MemoryAllocator::Free<MemoryAllocator::kPooledAndQueue>(
    MemoryChunk* chunk);

template <MemoryAllocator::AllocationMode alloc_mode, typename SpaceType>
Page* MemoryAllocator::AllocatePage(size_t size, SpaceType* owner,
                                    Executability executable) {
  MemoryChunk* chunk = nullptr;
  if (alloc_mode == kPooled) {
    DCHECK_EQ(size, static_cast<size_t>(MemoryChunk::kAllocatableMemory));
    DCHECK_EQ(executable, NOT_EXECUTABLE);
    chunk = AllocatePagePooled(owner);
  }
  if (chunk == nullptr) {
    chunk = AllocateChunk(size, size, executable, owner);
  }
  if (chunk == nullptr) return nullptr;
  return Page::Initialize(isolate_->heap(), chunk, executable, owner);
}

template Page*
MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, PagedSpace>(
    size_t size, PagedSpace* owner, Executability executable);
template Page*
MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, SemiSpace>(
    size_t size, SemiSpace* owner, Executability executable);
template Page*
MemoryAllocator::AllocatePage<MemoryAllocator::kPooled, SemiSpace>(
    size_t size, SemiSpace* owner, Executability executable);

LargePage* MemoryAllocator::AllocateLargePage(size_t size,
                                              LargeObjectSpace* owner,
                                              Executability executable) {
  MemoryChunk* chunk = AllocateChunk(size, size, executable, owner);
  if (chunk == nullptr) return nullptr;
  return LargePage::Initialize(isolate_->heap(), chunk, executable, owner);
}

template <typename SpaceType>
MemoryChunk* MemoryAllocator::AllocatePagePooled(SpaceType* owner) {
  MemoryChunk* chunk = unmapper()->TryGetPooledMemoryChunkSafe();
  if (chunk == nullptr) return nullptr;
  const int size = MemoryChunk::kPageSize;
  const Address start = reinterpret_cast<Address>(chunk);
  const Address area_start = start + MemoryChunk::kObjectStartOffset;
  const Address area_end = start + size;
  if (!CommitBlock(reinterpret_cast<Address>(chunk), size, NOT_EXECUTABLE)) {
    return nullptr;
  }
  base::VirtualMemory reservation(start, size);
  MemoryChunk::Initialize(isolate_->heap(), start, size, area_start, area_end,
                          NOT_EXECUTABLE, owner, &reservation);
  size_.Increment(size);
  return chunk;
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

#ifdef DEBUG
void MemoryAllocator::ReportStatistics() {
  size_t size = Size();
  float pct = static_cast<float>(capacity_ - size) / capacity_;
  PrintF("  capacity: %zu , used: %" PRIuS ", available: %%%d\n\n",
         capacity_, size, static_cast<int>(pct * 100));
}
#endif

size_t MemoryAllocator::CodePageGuardStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return RoundUp(Page::kObjectStartOffset, GetCommitPageSize());
}

size_t MemoryAllocator::CodePageGuardSize() {
  return static_cast<int>(GetCommitPageSize());
}

size_t MemoryAllocator::CodePageAreaStartOffset() {
  // We are guarding code pages: the first OS page after the header
  // will be protected as non-writable.
  return CodePageGuardStartOffset() + CodePageGuardSize();
}

size_t MemoryAllocator::CodePageAreaEndOffset() {
  // We are guarding code pages: the last OS page will be protected as
  // non-writable.
  return Page::kPageSize - static_cast<int>(GetCommitPageSize());
}

intptr_t MemoryAllocator::GetCommitPageSize() {
  if (FLAG_v8_os_page_size != 0) {
    DCHECK(base::bits::IsPowerOfTwo32(FLAG_v8_os_page_size));
    return FLAG_v8_os_page_size * KB;
  } else {
    return base::OS::CommitPageSize();
  }
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
  if (skip_list_ != nullptr) {
    delete skip_list_;
    skip_list_ = nullptr;
  }
  if (mutex_ != nullptr) {
    delete mutex_;
    mutex_ = nullptr;
  }
  ReleaseSlotSet<OLD_TO_NEW>();
  ReleaseSlotSet<OLD_TO_OLD>();
  ReleaseTypedSlotSet<OLD_TO_NEW>();
  ReleaseTypedSlotSet<OLD_TO_OLD>();
  if (local_tracker_ != nullptr) ReleaseLocalTracker();
  if (young_generation_bitmap_ != nullptr) ReleaseYoungGenerationBitmap();
}

static SlotSet* AllocateAndInitializeSlotSet(size_t size, Address page_start) {
  size_t pages = (size + Page::kPageSize - 1) / Page::kPageSize;
  DCHECK(pages > 0);
  SlotSet* slot_set = new SlotSet[pages];
  for (size_t i = 0; i < pages; i++) {
    slot_set[i].SetPageStart(page_start + i * Page::kPageSize);
  }
  return slot_set;
}

template SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_NEW>();
template SlotSet* MemoryChunk::AllocateSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
SlotSet* MemoryChunk::AllocateSlotSet() {
  SlotSet* slot_set = AllocateAndInitializeSlotSet(size_, address());
  if (!slot_set_[type].TrySetValue(nullptr, slot_set)) {
    delete[] slot_set;
    slot_set = slot_set_[type].Value();
    DCHECK(slot_set);
    return slot_set;
  }
  return slot_set;
}

template void MemoryChunk::ReleaseSlotSet<OLD_TO_NEW>();
template void MemoryChunk::ReleaseSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
void MemoryChunk::ReleaseSlotSet() {
  SlotSet* slot_set = slot_set_[type].Value();
  if (slot_set) {
    delete[] slot_set;
    slot_set_[type].SetValue(nullptr);
  }
}

template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_NEW>();
template TypedSlotSet* MemoryChunk::AllocateTypedSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
TypedSlotSet* MemoryChunk::AllocateTypedSlotSet() {
  TypedSlotSet* slot_set = new TypedSlotSet(address());
  if (!typed_slot_set_[type].TrySetValue(nullptr, slot_set)) {
    delete slot_set;
    slot_set = typed_slot_set_[type].Value();
    DCHECK(slot_set);
    return slot_set;
  }
  return slot_set;
}

template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_NEW>();
template void MemoryChunk::ReleaseTypedSlotSet<OLD_TO_OLD>();

template <RememberedSetType type>
void MemoryChunk::ReleaseTypedSlotSet() {
  TypedSlotSet* typed_slot_set = typed_slot_set_[type].Value();
  if (typed_slot_set) {
    delete typed_slot_set;
    typed_slot_set_[type].SetValue(nullptr);
  }
}

void MemoryChunk::AllocateLocalTracker() {
  DCHECK_NULL(local_tracker_);
  local_tracker_ = new LocalArrayBufferTracker(heap());
}

void MemoryChunk::ReleaseLocalTracker() {
  DCHECK_NOT_NULL(local_tracker_);
  delete local_tracker_;
  local_tracker_ = nullptr;
}

void MemoryChunk::AllocateYoungGenerationBitmap() {
  DCHECK_NULL(young_generation_bitmap_);
  young_generation_bitmap_ = static_cast<Bitmap*>(calloc(1, Bitmap::kSize));
}

void MemoryChunk::ReleaseYoungGenerationBitmap() {
  DCHECK_NOT_NULL(young_generation_bitmap_);
  free(young_generation_bitmap_);
  young_generation_bitmap_ = nullptr;
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

void Space::AddAllocationObserver(AllocationObserver* observer) {
  allocation_observers_->Add(observer);
}

void Space::RemoveAllocationObserver(AllocationObserver* observer) {
  bool removed = allocation_observers_->RemoveElement(observer);
  USE(removed);
  DCHECK(removed);
}

void Space::PauseAllocationObservers() { allocation_observers_paused_ = true; }

void Space::ResumeAllocationObservers() {
  allocation_observers_paused_ = false;
}

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
    : Space(heap, space, executable), anchor_(this), free_list_(this) {
  area_size_ = MemoryAllocator::PageAreaSize(space);
  accounting_stats_.Clear();

  allocation_info_.Reset(nullptr, nullptr);
}


bool PagedSpace::SetUp() { return true; }


bool PagedSpace::HasBeenSetUp() { return true; }


void PagedSpace::TearDown() {
  for (auto it = begin(); it != end();) {
    Page* page = *(it++);  // Will be erased.
    ArrayBufferTracker::FreeAll(page);
    heap()->memory_allocator()->Free<MemoryAllocator::kFull>(page);
  }
  anchor_.set_next_page(&anchor_);
  anchor_.set_prev_page(&anchor_);
  accounting_stats_.Clear();
}

void PagedSpace::RefillFreeList() {
  // Any PagedSpace might invoke RefillFreeList. We filter all but our old
  // generation spaces out.
  if (identity() != OLD_SPACE && identity() != CODE_SPACE &&
      identity() != MAP_SPACE) {
    return;
  }
  MarkCompactCollector* collector = heap()->mark_compact_collector();
  intptr_t added = 0;
  {
    Page* p = nullptr;
    while ((p = collector->sweeper().GetSweptPageSafe(this)) != nullptr) {
      // Only during compaction pages can actually change ownership. This is
      // safe because there exists no other competing action on the page links
      // during compaction.
      if (is_local() && (p->owner() != this)) {
        base::LockGuard<base::Mutex> guard(
            reinterpret_cast<PagedSpace*>(p->owner())->mutex());
        p->Unlink();
        p->set_owner(this);
        p->InsertAfter(anchor_.prev_page());
      }
      added += RelinkFreeListCategories(p);
      added += p->wasted_memory();
      if (is_local() && (added > kCompactionMemoryWanted)) break;
    }
  }
  accounting_stats_.IncreaseCapacity(added);
}

void PagedSpace::MergeCompactionSpace(CompactionSpace* other) {
  DCHECK(identity() == other->identity());
  // Unmerged fields:
  //   area_size_
  //   anchor_

  other->EmptyAllocationInfo();

  // Update and clear accounting statistics.
  accounting_stats_.Merge(other->accounting_stats_);
  other->accounting_stats_.Clear();

  // The linear allocation area of {other} should be destroyed now.
  DCHECK(other->top() == nullptr);
  DCHECK(other->limit() == nullptr);

  AccountCommitted(other->CommittedMemory());

  // Move over pages.
  for (auto it = other->begin(); it != other->end();) {
    Page* p = *(it++);

    // Relinking requires the category to be unlinked.
    other->UnlinkFreeListCategories(p);

    p->Unlink();
    p->set_owner(this);
    p->InsertAfter(anchor_.prev_page());
    RelinkFreeListCategories(p);
    DCHECK_EQ(p->AvailableInFreeList(), p->available_in_free_list());
  }
}


size_t PagedSpace::CommittedPhysicalMemory() {
  if (!base::VirtualMemory::HasLazyCommits()) return CommittedMemory();
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = 0;
  for (Page* page : *this) {
    size += page->CommittedPhysicalMemory();
  }
  return size;
}

bool PagedSpace::ContainsSlow(Address addr) {
  Page* p = Page::FromAddress(addr);
  for (Page* page : *this) {
    if (page == p) return true;
  }
  return false;
}

Page* PagedSpace::RemovePageSafe(int size_in_bytes) {
  base::LockGuard<base::Mutex> guard(mutex());

  // Check for pages that still contain free list entries. Bail out for smaller
  // categories.
  const int minimum_category =
      static_cast<int>(FreeList::SelectFreeListCategoryType(size_in_bytes));
  Page* page = free_list()->GetPageForCategoryType(kHuge);
  if (!page && static_cast<int>(kLarge) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kLarge);
  if (!page && static_cast<int>(kMedium) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kMedium);
  if (!page && static_cast<int>(kSmall) >= minimum_category)
    page = free_list()->GetPageForCategoryType(kSmall);
  if (!page) return nullptr;

  AccountUncommitted(page->size());
  accounting_stats_.DeallocateBytes(page->LiveBytesFromFreeList());
  accounting_stats_.DecreaseCapacity(page->area_size());
  page->Unlink();
  UnlinkFreeListCategories(page);
  return page;
}

void PagedSpace::AddPage(Page* page) {
  AccountCommitted(page->size());
  accounting_stats_.IncreaseCapacity(page->area_size());
  accounting_stats_.AllocateBytes(page->LiveBytesFromFreeList());
  page->set_owner(this);
  RelinkFreeListCategories(page);
  page->InsertAfter(anchor()->prev_page());
}

void PagedSpace::ShrinkImmortalImmovablePages() {
  DCHECK(!heap()->deserialization_complete());
  MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  EmptyAllocationInfo();
  ResetFreeList();

  for (Page* page : *this) {
    DCHECK(page->IsFlagSet(Page::NEVER_EVACUATE));
    size_t unused = page->ShrinkToHighWaterMark();
    accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
    // Do not account for the unused space as uncommitted because the counter
    // is kept in sync with page size which is also not adjusted for those
    // chunks.
  }
}

bool PagedSpace::Expand() {
  // Always lock against the main space as we can only adjust capacity and
  // pages concurrently for the main paged space.
  base::LockGuard<base::Mutex> guard(heap()->paged_space(identity())->mutex());

  const int size = AreaSize();

  if (!heap()->CanExpandOldGeneration(size)) return false;

  Page* p = heap()->memory_allocator()->AllocatePage(size, this, executable());
  if (p == nullptr) return false;

  AccountCommitted(p->size());

  // Pages created during bootstrapping may contain immortal immovable objects.
  if (!heap()->deserialization_complete()) p->MarkNeverEvacuate();

  DCHECK(Capacity() <= heap()->MaxOldGenerationSize());

  p->InsertAfter(anchor_.prev_page());

  return true;
}


int PagedSpace::CountTotalPages() {
  int count = 0;
  for (Page* page : *this) {
    count++;
    USE(page);
  }
  return count;
}


void PagedSpace::ResetFreeListStatistics() {
  for (Page* page : *this) {
    page->ResetFreeListStatistics();
  }
}

void PagedSpace::SetAllocationInfo(Address top, Address limit) {
  SetTopAndLimit(top, limit);
  if (top != nullptr && top != limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAllocationAreaAddress(top)->CreateBlackArea(top, limit);
  }
}

void PagedSpace::MarkAllocationInfoBlack() {
  DCHECK(heap()->incremental_marking()->black_allocation());
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != nullptr && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->CreateBlackArea(current_top, current_limit);
  }
}

void PagedSpace::UnmarkAllocationInfo() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != nullptr && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }
}

// Empty space allocation info, returning unused area to free list.
void PagedSpace::EmptyAllocationInfo() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  Address current_top = top();
  Address current_limit = limit();
  if (current_top == nullptr) {
    DCHECK(current_limit == nullptr);
    return;
  }

  if (heap()->incremental_marking()->black_allocation()) {
    Page* page = Page::FromAllocationAreaAddress(current_top);

    // Clear the bits in the unused black area.
    if (current_top != current_limit) {
      MarkingState::Internal(page).bitmap()->ClearRange(
          page->AddressToMarkbitIndex(current_top),
          page->AddressToMarkbitIndex(current_limit));
      MarkingState::Internal(page).IncrementLiveBytes(
          -static_cast<int>(current_limit - current_top));
    }
  }

  SetTopAndLimit(NULL, NULL);
  DCHECK_GE(current_limit, current_top);
  Free(current_top, current_limit - current_top);
}

void PagedSpace::IncreaseCapacity(size_t bytes) {
  accounting_stats_.ExpandSpace(bytes);
}

void PagedSpace::ReleasePage(Page* page) {
  DCHECK_EQ(0, MarkingState::Internal(page).live_bytes());
  DCHECK_EQ(page->owner(), this);

  free_list_.EvictFreeListItems(page);
  DCHECK(!free_list_.ContainsPageFreeListItems(page));

  if (Page::FromAllocationAreaAddress(allocation_info_.top()) == page) {
    allocation_info_.Reset(nullptr, nullptr);
  }

  // If page is still in a list, unlink it from that list.
  if (page->next_chunk() != NULL) {
    DCHECK(page->prev_chunk() != NULL);
    page->Unlink();
  }

  AccountUncommitted(page->size());
  accounting_stats_.ShrinkSpace(page->area_size());
  heap()->memory_allocator()->Free<MemoryAllocator::kPreFreeAndQueue>(page);
}

std::unique_ptr<ObjectIterator> PagedSpace::GetObjectIterator() {
  return std::unique_ptr<ObjectIterator>(new HeapObjectIterator(this));
}

#ifdef DEBUG
void PagedSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpace::Verify(ObjectVisitor* visitor) {
  bool allocation_pointer_found_in_space =
      (allocation_info_.top() == allocation_info_.limit());
  for (Page* page : *this) {
    CHECK(page->owner() == this);
    if (page == Page::FromAllocationAreaAddress(allocation_info_.top())) {
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

      if (!FLAG_verify_heap_skip_remembered_set) {
        heap()->VerifyRememberedSetFor(object);
      }

      // All the interior pointers should be contained in the heap.
      int size = object->Size();
      object->IterateBody(map->instance_type(), size, visitor);
      if (ObjectMarking::IsBlack(object, MarkingState::Internal(object))) {
        black_size += size;
      }

      CHECK(object->address() + size <= top);
      end_of_previous_object = object->address() + size;
    }
    CHECK_LE(black_size, MarkingState::Internal(page).live_bytes());
  }
  CHECK(allocation_pointer_found_in_space);
}
#endif  // VERIFY_HEAP

// -----------------------------------------------------------------------------
// NewSpace implementation

bool NewSpace::SetUp(size_t initial_semispace_capacity,
                     size_t maximum_semispace_capacity) {
  DCHECK(initial_semispace_capacity <= maximum_semispace_capacity);
  DCHECK(base::bits::IsPowerOfTwo32(
      static_cast<uint32_t>(maximum_semispace_capacity)));

  to_space_.SetUp(initial_semispace_capacity, maximum_semispace_capacity);
  from_space_.SetUp(initial_semispace_capacity, maximum_semispace_capacity);
  if (!to_space_.Commit()) {
    return false;
  }
  DCHECK(!from_space_.is_committed());  // No need to use memory yet.
  ResetAllocationInfo();

  // Allocate and set up the histogram arrays if necessary.
  allocated_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
  promoted_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
#define SET_NAME(name)                        \
  allocated_histogram_[name].set_name(#name); \
  promoted_histogram_[name].set_name(#name);
  INSTANCE_TYPE_LIST(SET_NAME)
#undef SET_NAME

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

  allocation_info_.Reset(nullptr, nullptr);

  to_space_.TearDown();
  from_space_.TearDown();
}

void NewSpace::Flip() { SemiSpace::Swap(&from_space_, &to_space_); }


void NewSpace::Grow() {
  // Double the semispace size but only up to maximum capacity.
  DCHECK(TotalCapacity() < MaximumCapacity());
  size_t new_capacity =
      Min(MaximumCapacity(),
          static_cast<size_t>(FLAG_semi_space_growth_factor) * TotalCapacity());
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
  size_t new_capacity = Max(InitialTotalCapacity(), 2 * Size());
  size_t rounded_new_capacity = RoundUp(new_capacity, Page::kPageSize);
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

bool NewSpace::Rebalance() {
  CHECK(heap()->promotion_queue()->is_empty());
  // Order here is important to make use of the page pool.
  return to_space_.EnsureCurrentCapacity() &&
         from_space_.EnsureCurrentCapacity();
}

bool SemiSpace::EnsureCurrentCapacity() {
  if (is_committed()) {
    const int expected_pages =
        static_cast<int>(current_capacity_ / Page::kPageSize);
    int actual_pages = 0;
    Page* current_page = anchor()->next_page();
    while (current_page != anchor()) {
      actual_pages++;
      current_page = current_page->next_page();
      if (actual_pages > expected_pages) {
        Page* to_remove = current_page->prev_page();
        // Make sure we don't overtake the actual top pointer.
        CHECK_NE(to_remove, current_page_);
        to_remove->Unlink();
        // Clear new space flags to avoid this page being treated as a new
        // space page that is potentially being swept.
        to_remove->SetFlags(0, Page::kIsInNewSpaceMask);
        heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(
            to_remove);
      }
    }
    while (actual_pages < expected_pages) {
      actual_pages++;
      current_page =
          heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
              Page::kAllocatableMemory, this, executable());
      if (current_page == nullptr) return false;
      DCHECK_NOT_NULL(current_page);
      current_page->InsertAfter(anchor());
      MarkingState::Internal(current_page).ClearLiveness();
      current_page->SetFlags(anchor()->prev_page()->GetFlags(),
                             static_cast<uintptr_t>(Page::kCopyAllFlags));
      heap()->CreateFillerObjectAt(current_page->area_start(),
                                   static_cast<int>(current_page->area_size()),
                                   ClearRecordedSlots::kNo);
    }
  }
  return true;
}

AllocationInfo LocalAllocationBuffer::Close() {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearRecordedSlots::kNo);
    const AllocationInfo old_info = allocation_info_;
    allocation_info_ = AllocationInfo(nullptr, nullptr);
    return old_info;
  }
  return AllocationInfo(nullptr, nullptr);
}


LocalAllocationBuffer::LocalAllocationBuffer(Heap* heap,
                                             AllocationInfo allocation_info)
    : heap_(heap), allocation_info_(allocation_info) {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearRecordedSlots::kNo);
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
  // Clear all mark-bits in the to-space.
  for (Page* p : to_space_) {
    MarkingState::Internal(p).ClearLiveness();
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
  DCHECK(!Page::IsAtObjectStart(top));
  if (!to_space_.AdvancePage()) {
    // No more pages left to advance.
    return false;
  }

  // Clear remainder of current page.
  Address limit = Page::FromAllocationAreaAddress(top)->area_end();
  if (heap()->gc_state() == Heap::SCAVENGE) {
    heap()->promotion_queue()->SetNewLimit(limit);
  }

  int remaining_in_page = static_cast<int>(limit - top);
  heap()->CreateFillerObjectAt(top, remaining_in_page, ClearRecordedSlots::kNo);
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

  if (old_top + aligned_size_in_bytes > high) {
    // Not enough room in the page, try to allocate a new one.
    if (!AddFreshPage()) {
      return false;
    }

    InlineAllocationStep(old_top, allocation_info_.top(), nullptr, 0);

    old_top = allocation_info_.top();
    high = to_space_.page_high();
    filler_size = Heap::GetFillToAlign(old_top, alignment);
  }

  DCHECK(old_top + aligned_size_in_bytes <= high);

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

std::unique_ptr<ObjectIterator> NewSpace::GetObjectIterator() {
  return std::unique_ptr<ObjectIterator>(new SemiSpaceIterator(this));
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
    if (!Page::IsAlignedToPageSize(current)) {
      // The allocation pointer should not be in the middle of an object.
      CHECK(!Page::FromAllocationAreaAddress(current)->ContainsLimit(top()) ||
            current < top());

      HeapObject* object = HeapObject::FromAddress(current);

      // The first word should be a map, and we expect all map pointers to
      // be in map space.
      Map* map = object->map();
      CHECK(map->IsMap());
      CHECK(heap()->map_space()->Contains(map));

      // The object should not be code or a map.
      CHECK(!object->IsMap());
      CHECK(!object->IsAbstractCode());

      // The object itself should look OK.
      object->ObjectVerify();

      // All the interior pointers should be contained in the heap.
      VerifyPointersVisitor visitor;
      int size = object->Size();
      object->IterateBody(map->instance_type(), size, &visitor);

      current += size;
    } else {
      // At end of page, switch to next page.
      Page* page = Page::FromAllocationAreaAddress(current)->next_page();
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

void SemiSpace::SetUp(size_t initial_capacity, size_t maximum_capacity) {
  DCHECK_GE(maximum_capacity, static_cast<size_t>(Page::kPageSize));
  minimum_capacity_ = RoundDown(initial_capacity, Page::kPageSize);
  current_capacity_ = minimum_capacity_;
  maximum_capacity_ = RoundDown(maximum_capacity, Page::kPageSize);
  committed_ = false;
}


void SemiSpace::TearDown() {
  // Properly uncommit memory to keep the allocator counters in sync.
  if (is_committed()) {
    for (Page* p : *this) {
      ArrayBufferTracker::FreeAll(p);
    }
    Uncommit();
  }
  current_capacity_ = maximum_capacity_ = 0;
}


bool SemiSpace::Commit() {
  DCHECK(!is_committed());
  Page* current = anchor();
  const int num_pages = static_cast<int>(current_capacity_ / Page::kPageSize);
  for (int pages_added = 0; pages_added < num_pages; pages_added++) {
    Page* new_page =
        heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
            Page::kAllocatableMemory, this, executable());
    if (new_page == nullptr) {
      RewindPages(current, pages_added);
      return false;
    }
    new_page->InsertAfter(current);
    current = new_page;
  }
  Reset();
  AccountCommitted(current_capacity_);
  if (age_mark_ == nullptr) {
    age_mark_ = first_page()->area_start();
  }
  committed_ = true;
  return true;
}


bool SemiSpace::Uncommit() {
  DCHECK(is_committed());
  for (auto it = begin(); it != end();) {
    Page* p = *(it++);
    heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(p);
  }
  anchor()->set_next_page(anchor());
  anchor()->set_prev_page(anchor());
  AccountUncommitted(current_capacity_);
  committed_ = false;
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  return true;
}


size_t SemiSpace::CommittedPhysicalMemory() {
  if (!is_committed()) return 0;
  size_t size = 0;
  for (Page* p : *this) {
    size += p->CommittedPhysicalMemory();
  }
  return size;
}

bool SemiSpace::GrowTo(size_t new_capacity) {
  if (!is_committed()) {
    if (!Commit()) return false;
  }
  DCHECK_EQ(new_capacity & Page::kPageAlignmentMask, 0u);
  DCHECK_LE(new_capacity, maximum_capacity_);
  DCHECK_GT(new_capacity, current_capacity_);
  const size_t delta = new_capacity - current_capacity_;
  DCHECK(IsAligned(delta, base::OS::AllocateAlignment()));
  const int delta_pages = static_cast<int>(delta / Page::kPageSize);
  Page* last_page = anchor()->prev_page();
  DCHECK_NE(last_page, anchor());
  for (int pages_added = 0; pages_added < delta_pages; pages_added++) {
    Page* new_page =
        heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
            Page::kAllocatableMemory, this, executable());
    if (new_page == nullptr) {
      RewindPages(last_page, pages_added);
      return false;
    }
    new_page->InsertAfter(last_page);
    MarkingState::Internal(new_page).ClearLiveness();
    // Duplicate the flags that was set on the old page.
    new_page->SetFlags(last_page->GetFlags(), Page::kCopyOnFlipFlagsMask);
    last_page = new_page;
  }
  AccountCommitted(delta);
  current_capacity_ = new_capacity;
  return true;
}

void SemiSpace::RewindPages(Page* start, int num_pages) {
  Page* new_last_page = nullptr;
  Page* last_page = start;
  while (num_pages > 0) {
    DCHECK_NE(last_page, anchor());
    new_last_page = last_page->prev_page();
    last_page->prev_page()->set_next_page(last_page->next_page());
    last_page->next_page()->set_prev_page(last_page->prev_page());
    last_page = new_last_page;
    num_pages--;
  }
}

bool SemiSpace::ShrinkTo(size_t new_capacity) {
  DCHECK_EQ(new_capacity & Page::kPageAlignmentMask, 0u);
  DCHECK_GE(new_capacity, minimum_capacity_);
  DCHECK_LT(new_capacity, current_capacity_);
  if (is_committed()) {
    const size_t delta = current_capacity_ - new_capacity;
    DCHECK(IsAligned(delta, base::OS::AllocateAlignment()));
    int delta_pages = static_cast<int>(delta / Page::kPageSize);
    Page* new_last_page;
    Page* last_page;
    while (delta_pages > 0) {
      last_page = anchor()->prev_page();
      new_last_page = last_page->prev_page();
      new_last_page->set_next_page(anchor());
      anchor()->set_prev_page(new_last_page);
      heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(
          last_page);
      delta_pages--;
    }
    AccountUncommitted(delta);
    heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  }
  current_capacity_ = new_capacity;
  return true;
}

void SemiSpace::FixPagesFlags(intptr_t flags, intptr_t mask) {
  anchor_.set_owner(this);
  anchor_.prev_page()->set_next_page(&anchor_);
  anchor_.next_page()->set_prev_page(&anchor_);

  for (Page* page : *this) {
    page->set_owner(this);
    page->SetFlags(flags, mask);
    if (id_ == kToSpace) {
      page->ClearFlag(MemoryChunk::IN_FROM_SPACE);
      page->SetFlag(MemoryChunk::IN_TO_SPACE);
      page->ClearFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
      MarkingState::Internal(page).SetLiveBytes(0);
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
  pages_used_ = 0;
}

void SemiSpace::RemovePage(Page* page) {
  if (current_page_ == page) {
    current_page_ = page->prev_page();
  }
  page->Unlink();
}

void SemiSpace::PrependPage(Page* page) {
  page->SetFlags(current_page()->GetFlags(),
                 static_cast<uintptr_t>(Page::kCopyAllFlags));
  page->set_owner(this);
  page->InsertAfter(anchor());
  pages_used_++;
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
  std::swap(from->age_mark_, to->age_mark_);
  std::swap(from->committed_, to->committed_);
  std::swap(from->anchor_, to->anchor_);
  std::swap(from->current_page_, to->current_page_);

  to->FixPagesFlags(saved_to_space_flags, Page::kCopyOnFlipFlagsMask);
  from->FixPagesFlags(0, 0);
}

void SemiSpace::set_age_mark(Address mark) {
  DCHECK_EQ(Page::FromAllocationAreaAddress(mark)->owner(), this);
  age_mark_ = mark;
  // Mark all pages up to the one containing mark.
  for (Page* p : PageRange(space_start(), mark)) {
    p->SetFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
  }
}

std::unique_ptr<ObjectIterator> SemiSpace::GetObjectIterator() {
  // Use the NewSpace::NewObjectIterator to iterate the ToSpace.
  UNREACHABLE();
  return std::unique_ptr<ObjectIterator>();
}

#ifdef DEBUG
void SemiSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void SemiSpace::Verify() {
  bool is_from_space = (id_ == kFromSpace);
  Page* page = anchor_.next_page();
  CHECK(anchor_.owner() == this);
  while (page != &anchor_) {
    CHECK_EQ(page->owner(), this);
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
  Page* page = Page::FromAllocationAreaAddress(start);
  Page* end_page = Page::FromAllocationAreaAddress(end);
  SemiSpace* space = reinterpret_cast<SemiSpace*>(page->owner());
  CHECK_EQ(space, end_page->owner());
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
    PrintF("  capacity: %" PRIuS ", available: %" PRIuS ", %%%d\n",
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


void FreeListCategory::Reset() {
  set_top(nullptr);
  set_prev(nullptr);
  set_next(nullptr);
  available_ = 0;
}

FreeSpace* FreeListCategory::PickNodeFromList(size_t* node_size) {
  DCHECK(page()->CanAllocate());

  FreeSpace* node = top();
  if (node == nullptr) return nullptr;
  set_top(node->next());
  *node_size = node->Size();
  available_ -= *node_size;
  return node;
}

FreeSpace* FreeListCategory::TryPickNodeFromList(size_t minimum_size,
                                                 size_t* node_size) {
  DCHECK(page()->CanAllocate());

  FreeSpace* node = PickNodeFromList(node_size);
  if ((node != nullptr) && (*node_size < minimum_size)) {
    Free(node, *node_size, kLinkCategory);
    *node_size = 0;
    return nullptr;
  }
  return node;
}

FreeSpace* FreeListCategory::SearchForNodeInList(size_t minimum_size,
                                                 size_t* node_size) {
  DCHECK(page()->CanAllocate());

  FreeSpace* prev_non_evac_node = nullptr;
  for (FreeSpace* cur_node = top(); cur_node != nullptr;
       cur_node = cur_node->next()) {
    size_t size = cur_node->size();
    if (size >= minimum_size) {
      DCHECK_GE(available_, size);
      available_ -= size;
      if (cur_node == top()) {
        set_top(cur_node->next());
      }
      if (prev_non_evac_node != nullptr) {
        prev_non_evac_node->set_next(cur_node->next());
      }
      *node_size = size;
      return cur_node;
    }

    prev_non_evac_node = cur_node;
  }
  return nullptr;
}

bool FreeListCategory::Free(FreeSpace* free_space, size_t size_in_bytes,
                            FreeMode mode) {
  if (!page()->CanAllocate()) return false;

  free_space->set_next(top());
  set_top(free_space);
  available_ += size_in_bytes;
  if ((mode == kLinkCategory) && (prev() == nullptr) && (next() == nullptr)) {
    owner()->AddCategory(this);
  }
  return true;
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

void FreeListCategory::Relink() {
  DCHECK(!is_linked());
  owner()->AddCategory(this);
}

void FreeListCategory::Invalidate() {
  page()->remove_available_in_free_list(available());
  Reset();
  type_ = kInvalidCategory;
}

FreeList::FreeList(PagedSpace* owner) : owner_(owner), wasted_bytes_(0) {
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = nullptr;
  }
  Reset();
}


void FreeList::Reset() {
  ForAllFreeListCategories(
      [](FreeListCategory* category) { category->Reset(); });
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    categories_[i] = nullptr;
  }
  ResetStats();
}

size_t FreeList::Free(Address start, size_t size_in_bytes, FreeMode mode) {
  if (size_in_bytes == 0) return 0;

  owner()->heap()->CreateFillerObjectAt(start, static_cast<int>(size_in_bytes),
                                        ClearRecordedSlots::kNo);

  Page* page = Page::FromAddress(start);

  // Blocks have to be a minimum size to hold free list items.
  if (size_in_bytes < kMinBlockSize) {
    page->add_wasted_memory(size_in_bytes);
    wasted_bytes_.Increment(size_in_bytes);
    return size_in_bytes;
  }

  FreeSpace* free_space = FreeSpace::cast(HeapObject::FromAddress(start));
  // Insert other blocks at the head of a free list of the appropriate
  // magnitude.
  FreeListCategoryType type = SelectFreeListCategoryType(size_in_bytes);
  if (page->free_list_category(type)->Free(free_space, size_in_bytes, mode)) {
    page->add_available_in_free_list(size_in_bytes);
  }
  DCHECK_EQ(page->AvailableInFreeList(), page->available_in_free_list());
  return 0;
}

FreeSpace* FreeList::FindNodeIn(FreeListCategoryType type, size_t* node_size) {
  FreeListCategoryIterator it(this, type);
  FreeSpace* node = nullptr;
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    node = current->PickNodeFromList(node_size);
    if (node != nullptr) {
      Page::FromAddress(node->address())
          ->remove_available_in_free_list(*node_size);
      DCHECK(IsVeryLong() || Available() == SumFreeLists());
      return node;
    }
    RemoveCategory(current);
  }
  return node;
}

FreeSpace* FreeList::TryFindNodeIn(FreeListCategoryType type, size_t* node_size,
                                   size_t minimum_size) {
  if (categories_[type] == nullptr) return nullptr;
  FreeSpace* node =
      categories_[type]->TryPickNodeFromList(minimum_size, node_size);
  if (node != nullptr) {
    Page::FromAddress(node->address())
        ->remove_available_in_free_list(*node_size);
    DCHECK(IsVeryLong() || Available() == SumFreeLists());
  }
  return node;
}

FreeSpace* FreeList::SearchForNodeInList(FreeListCategoryType type,
                                         size_t* node_size,
                                         size_t minimum_size) {
  FreeListCategoryIterator it(this, type);
  FreeSpace* node = nullptr;
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    node = current->SearchForNodeInList(minimum_size, node_size);
    if (node != nullptr) {
      Page::FromAddress(node->address())
          ->remove_available_in_free_list(*node_size);
      DCHECK(IsVeryLong() || Available() == SumFreeLists());
      return node;
    }
    if (current->is_empty()) {
      RemoveCategory(current);
    }
  }
  return node;
}

FreeSpace* FreeList::FindNodeFor(size_t size_in_bytes, size_t* node_size) {
  FreeSpace* node = nullptr;

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
  node = SearchForNodeInList(kHuge, node_size, size_in_bytes);
  if (node != nullptr) {
    DCHECK(IsVeryLong() || Available() == SumFreeLists());
    return node;
  }

  // We need a huge block of memory, but we didn't find anything in the huge
  // list.
  if (type == kHuge) return nullptr;

  // Now search the best fitting free list for a node that has at least the
  // requested size.
  type = SelectFreeListCategoryType(size_in_bytes);
  node = TryFindNodeIn(type, node_size, size_in_bytes);

  DCHECK(IsVeryLong() || Available() == SumFreeLists());
  return node;
}

// Allocation on the old space free list.  If it succeeds then a new linear
// allocation space has been set up with the top and limit of the space.  If
// the allocation fails then NULL is returned, and the caller can perform a GC
// or allocate a new page before retrying.
HeapObject* FreeList::Allocate(size_t size_in_bytes) {
  DCHECK(size_in_bytes <= kMaxBlockSize);
  DCHECK(IsAligned(size_in_bytes, kPointerSize));
  DCHECK_LE(owner_->top(), owner_->limit());
#ifdef DEBUG
  if (owner_->top() != owner_->limit()) {
    DCHECK_EQ(Page::FromAddress(owner_->top()),
              Page::FromAddress(owner_->limit() - 1));
  }
#endif
  // Don't free list allocate if there is linear space available.
  DCHECK_LT(static_cast<size_t>(owner_->limit() - owner_->top()),
            size_in_bytes);

  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  owner_->EmptyAllocationInfo();

  owner_->heap()->StartIncrementalMarkingIfAllocationLimitIsReached(
      Heap::kNoGCFlags, kNoGCCallbackFlags);

  size_t new_node_size = 0;
  FreeSpace* new_node = FindNodeFor(size_in_bytes, &new_node_size);
  if (new_node == nullptr) return nullptr;

  DCHECK_GE(new_node_size, size_in_bytes);
  size_t bytes_left = new_node_size - size_in_bytes;

#ifdef DEBUG
  for (size_t i = 0; i < size_in_bytes / kPointerSize; i++) {
    reinterpret_cast<Object**>(new_node->address())[i] =
        Smi::FromInt(kCodeZapValue);
  }
#endif

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  const size_t kThreshold = IncrementalMarking::kAllocatedThreshold;

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  owner_->AccountAllocatedBytes(new_node_size);

  if (owner_->heap()->inline_allocation_disabled()) {
    // Keep the linear allocation area empty if requested to do so, just
    // return area back to the free list instead.
    owner_->Free(new_node->address() + size_in_bytes, bytes_left);
    owner_->SetAllocationInfo(new_node->address() + size_in_bytes,
                              new_node->address() + size_in_bytes);
  } else if (bytes_left > kThreshold &&
             owner_->heap()->incremental_marking()->IsMarkingIncomplete() &&
             FLAG_incremental_marking) {
    size_t linear_size = owner_->RoundSizeDownToObjectAlignment(kThreshold);
    // We don't want to give too large linear areas to the allocator while
    // incremental marking is going on, because we won't check again whether
    // we want to do another increment until the linear area is used up.
    DCHECK_GE(new_node_size, size_in_bytes + linear_size);
    owner_->Free(new_node->address() + size_in_bytes + linear_size,
                 new_node_size - size_in_bytes - linear_size);
    owner_->SetAllocationInfo(
        new_node->address() + size_in_bytes,
        new_node->address() + size_in_bytes + linear_size);
  } else {
    // Normally we give the rest of the node to the allocator as its new
    // linear allocation area.
    owner_->SetAllocationInfo(new_node->address() + size_in_bytes,
                              new_node->address() + new_node_size);
  }

  return new_node;
}

size_t FreeList::EvictFreeListItems(Page* page) {
  size_t sum = 0;
  page->ForAllFreeListCategories(
      [this, &sum](FreeListCategory* category) {
        DCHECK_EQ(this, category->owner());
        sum += category->available();
        RemoveCategory(category);
        category->Invalidate();
      });
  return sum;
}

bool FreeList::ContainsPageFreeListItems(Page* page) {
  bool contained = false;
  page->ForAllFreeListCategories(
      [this, &contained](FreeListCategory* category) {
        if (category->owner() == this && category->is_linked()) {
          contained = true;
        }
      });
  return contained;
}

void FreeList::RepairLists(Heap* heap) {
  ForAllFreeListCategories(
      [heap](FreeListCategory* category) { category->RepairFreeList(heap); });
}

bool FreeList::AddCategory(FreeListCategory* category) {
  FreeListCategoryType type = category->type_;
  FreeListCategory* top = categories_[type];

  if (category->is_empty()) return false;
  if (top == category) return false;

  // Common double-linked list insertion.
  if (top != nullptr) {
    top->set_prev(category);
  }
  category->set_next(top);
  categories_[type] = category;
  return true;
}

void FreeList::RemoveCategory(FreeListCategory* category) {
  FreeListCategoryType type = category->type_;
  FreeListCategory* top = categories_[type];

  // Common double-linked list removal.
  if (top == category) {
    categories_[type] = category->next();
  }
  if (category->prev() != nullptr) {
    category->prev()->set_next(category->next());
  }
  if (category->next() != nullptr) {
    category->next()->set_prev(category->prev());
  }
  category->set_next(nullptr);
  category->set_prev(nullptr);
}

void FreeList::PrintCategories(FreeListCategoryType type) {
  FreeListCategoryIterator it(this, type);
  PrintF("FreeList[%p, top=%p, %d] ", static_cast<void*>(this),
         static_cast<void*>(categories_[type]), type);
  while (it.HasNext()) {
    FreeListCategory* current = it.Next();
    PrintF("%p -> ", static_cast<void*>(current));
  }
  PrintF("null\n");
}


#ifdef DEBUG
size_t FreeListCategory::SumFreeList() {
  size_t sum = 0;
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

bool FreeList::IsVeryLong() {
  int len = 0;
  for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
    FreeListCategoryIterator it(this, static_cast<FreeListCategoryType>(i));
    while (it.HasNext()) {
      len += it.Next()->FreeListLength();
      if (len >= FreeListCategory::kVeryLongFreeList) return true;
    }
  }
  return false;
}


// This can take a very long time because it is linear in the number of entries
// on the free list, so it should not be called if FreeListLength returns
// kVeryLongFreeList.
size_t FreeList::SumFreeLists() {
  size_t sum = 0;
  ForAllFreeListCategories(
      [&sum](FreeListCategory* category) { sum += category->SumFreeList(); });
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

size_t PagedSpace::SizeOfObjects() {
  CHECK_GE(limit(), top());
  DCHECK_GE(Size(), static_cast<size_t>(limit() - top()));
  return Size() - (limit() - top());
}


// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally NULL), so we need to
// fix them.
void PagedSpace::RepairFreeListsAfterDeserialization() {
  free_list_.RepairLists(heap());
  // Each page may have a small free space that is not tracked by a free list.
  // Those free spaces still contain null as their map pointer.
  // Overwrite them with new fillers.
  for (Page* page : *this) {
    int size = static_cast<int>(page->wasted_memory());
    if (size == 0) {
      // If there is no wasted memory then all free space is in the free list.
      continue;
    }
    Address start = page->HighWaterMark();
    Address end = page->area_end();
    CHECK_EQ(size, static_cast<int>(end - start));
    heap()->CreateFillerObjectAt(start, size, ClearRecordedSlots::kNo);
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
  VMState<GC> state(heap()->isolate());
  RuntimeCallTimerScope runtime_timer(heap()->isolate(),
                                      &RuntimeCallStats::GC_SlowAllocateRaw);
  return RawSlowAllocateRaw(size_in_bytes);
}

HeapObject* CompactionSpace::SlowAllocateRaw(int size_in_bytes) {
  return RawSlowAllocateRaw(size_in_bytes);
}

HeapObject* PagedSpace::RawSlowAllocateRaw(int size_in_bytes) {
  // Allocation in this space has failed.
  DCHECK_GE(size_in_bytes, 0);
  const int kMaxPagesToSweep = 1;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    if (FLAG_concurrent_sweeping && !is_local() &&
        !collector->sweeper().AreSweeperTasksRunning()) {
      collector->EnsureSweepingCompleted();
    }

    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList();

    // Retry the free list allocation.
    HeapObject* object =
        free_list_.Allocate(static_cast<size_t>(size_in_bytes));
    if (object != NULL) return object;

    // If sweeping is still in progress try to sweep pages on the main thread.
    int max_freed = collector->sweeper().ParallelSweepSpace(
        identity(), size_in_bytes, kMaxPagesToSweep);
    RefillFreeList();
    if (max_freed >= size_in_bytes) {
      object = free_list_.Allocate(static_cast<size_t>(size_in_bytes));
      if (object != nullptr) return object;
    }
  } else if (is_local()) {
    // Sweeping not in progress and we are on a {CompactionSpace}. This can
    // only happen when we are evacuating for the young generation.
    PagedSpace* main_space = heap()->paged_space(identity());
    Page* page = main_space->RemovePageSafe(size_in_bytes);
    if (page != nullptr) {
      AddPage(page);
      HeapObject* object =
          free_list_.Allocate(static_cast<size_t>(size_in_bytes));
      if (object != nullptr) return object;
    }
  }

  if (heap()->ShouldExpandOldGenerationOnSlowAllocation() && Expand()) {
    DCHECK((CountTotalPages() > 1) ||
           (static_cast<size_t>(size_in_bytes) <= free_list_.Available()));
    return free_list_.Allocate(static_cast<size_t>(size_in_bytes));
  }

  // If sweeper threads are active, wait for them at that point and steal
  // elements form their free-lists. Allocation may still fail their which
  // would indicate that there is not enough memory for the given allocation.
  return SweepAndRetryAllocation(size_in_bytes);
}

#ifdef DEBUG
void PagedSpace::ReportStatistics() {
  int pct = static_cast<int>(Available() * 100 / Capacity());
  PrintF("  capacity: %" PRIuS ", waste: %" PRIuS
         ", available: %" PRIuS ", %%%d\n",
         Capacity(), Waste(), Available(), pct);

  heap()->mark_compact_collector()->EnsureSweepingCompleted();
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

Address LargePage::GetAddressToShrink() {
  HeapObject* object = GetObject();
  if (executable() == EXECUTABLE) {
    return 0;
  }
  size_t used_size = RoundUp((object->address() - address()) + object->Size(),
                             MemoryAllocator::GetCommitPageSize());
  if (used_size < CommittedPhysicalMemory()) {
    return address() + used_size;
  }
  return 0;
}

void LargePage::ClearOutOfLiveRangeSlots(Address free_start) {
  RememberedSet<OLD_TO_NEW>::RemoveRange(this, free_start, area_end(),
                                         SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_OLD>::RemoveRange(this, free_start, area_end(),
                                         SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_NEW>::RemoveRangeTyped(this, free_start, area_end());
  RememberedSet<OLD_TO_OLD>::RemoveRangeTyped(this, free_start, area_end());
}

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
      chunk_map_(1024) {}

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
    heap()->memory_allocator()->Free<MemoryAllocator::kFull>(page);
  }
  SetUp();
}


AllocationResult LargeObjectSpace::AllocateRaw(int object_size,
                                               Executability executable) {
  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->CanExpandOldGeneration(object_size) ||
      !heap()->ShouldExpandOldGenerationOnSlowAllocation()) {
    return AllocationResult::Retry(identity());
  }

  LargePage* page = heap()->memory_allocator()->AllocateLargePage(
      object_size, this, executable);
  if (page == NULL) return AllocationResult::Retry(identity());
  DCHECK_GE(page->area_size(), static_cast<size_t>(object_size));

  size_ += static_cast<int>(page->size());
  AccountCommitted(page->size());
  objects_size_ += object_size;
  page_count_++;
  page->set_next_page(first_page_);
  first_page_ = page;

  InsertChunkMapEntries(page);

  HeapObject* object = page->GetObject();

  if (Heap::ShouldZapGarbage()) {
    // Make the object consistent so the heap can be verified in OldSpaceStep.
    // We only need to do this in debug builds or if verify_heap is on.
    reinterpret_cast<Object**>(object->address())[0] =
        heap()->fixed_array_map();
    reinterpret_cast<Object**>(object->address())[1] = Smi::kZero;
  }

  heap()->StartIncrementalMarkingIfAllocationLimitIsReached(Heap::kNoGCFlags,
                                                            kNoGCCallbackFlags);
  AllocationStep(object->address(), object_size);

  heap()->CreateFillerObjectAt(object->address(), object_size,
                               ClearRecordedSlots::kNo);

  if (heap()->incremental_marking()->black_allocation()) {
    ObjectMarking::WhiteToBlack(object, MarkingState::Internal(object));
  }
  return object;
}


size_t LargeObjectSpace::CommittedPhysicalMemory() {
  // On a platform that provides lazy committing of memory, we over-account
  // the actually committed memory. There is no easy way right now to support
  // precise accounting of committed memory in large object space.
  return CommittedMemory();
}


// GC support
Object* LargeObjectSpace::FindObject(Address a) {
  LargePage* page = FindPage(a);
  if (page != NULL) {
    return page->GetObject();
  }
  return Smi::kZero;  // Signaling not found.
}

LargePage* LargeObjectSpace::FindPageThreadSafe(Address a) {
  base::LockGuard<base::Mutex> guard(&chunk_map_mutex_);
  return FindPage(a);
}

LargePage* LargeObjectSpace::FindPage(Address a) {
  uintptr_t key = reinterpret_cast<uintptr_t>(a) / MemoryChunk::kAlignment;
  base::HashMap::Entry* e = chunk_map_.Lookup(reinterpret_cast<void*>(key),
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
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    if (ObjectMarking::IsBlackOrGrey(obj, MarkingState::Internal(obj))) {
      Marking::MarkWhite(
          ObjectMarking::MarkBitFrom(obj, MarkingState::Internal(obj)));
      MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
      chunk->ResetProgressBar();
      MarkingState::Internal(chunk).SetLiveBytes(0);
    }
    DCHECK(ObjectMarking::IsWhite(obj, MarkingState::Internal(obj)));
  }
}

void LargeObjectSpace::InsertChunkMapEntries(LargePage* page) {
  // Register all MemoryChunk::kAlignment-aligned chunks covered by
  // this large page in the chunk map.
  uintptr_t start = reinterpret_cast<uintptr_t>(page) / MemoryChunk::kAlignment;
  uintptr_t limit = (reinterpret_cast<uintptr_t>(page) + (page->size() - 1)) /
                    MemoryChunk::kAlignment;
  // There may be concurrent access on the chunk map. We have to take the lock
  // here.
  base::LockGuard<base::Mutex> guard(&chunk_map_mutex_);
  for (uintptr_t key = start; key <= limit; key++) {
    base::HashMap::Entry* entry = chunk_map_.InsertNew(
        reinterpret_cast<void*>(key), static_cast<uint32_t>(key));
    DCHECK(entry != NULL);
    entry->value = page;
  }
}

void LargeObjectSpace::RemoveChunkMapEntries(LargePage* page) {
  RemoveChunkMapEntries(page, page->address());
}

void LargeObjectSpace::RemoveChunkMapEntries(LargePage* page,
                                             Address free_start) {
  uintptr_t start = RoundUp(reinterpret_cast<uintptr_t>(free_start),
                            MemoryChunk::kAlignment) /
                    MemoryChunk::kAlignment;
  uintptr_t limit = (reinterpret_cast<uintptr_t>(page) + (page->size() - 1)) /
                    MemoryChunk::kAlignment;
  for (uintptr_t key = start; key <= limit; key++) {
    chunk_map_.Remove(reinterpret_cast<void*>(key), static_cast<uint32_t>(key));
  }
}

void LargeObjectSpace::FreeUnmarkedObjects() {
  LargePage* previous = NULL;
  LargePage* current = first_page_;
  while (current != NULL) {
    HeapObject* object = current->GetObject();
    DCHECK(!ObjectMarking::IsGrey(object, MarkingState::Internal(object)));
    if (ObjectMarking::IsBlack(object, MarkingState::Internal(object))) {
      Address free_start;
      if ((free_start = current->GetAddressToShrink()) != 0) {
        // TODO(hpayer): Perform partial free concurrently.
        current->ClearOutOfLiveRangeSlots(free_start);
        RemoveChunkMapEntries(current, free_start);
        heap()->memory_allocator()->PartialFreeMemory(current, free_start);
      }
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
      size_ -= static_cast<int>(page->size());
      AccountUncommitted(page->size());
      objects_size_ -= object->Size();
      page_count_--;

      RemoveChunkMapEntries(page);
      heap()->memory_allocator()->Free<MemoryAllocator::kPreFreeAndQueue>(page);
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

std::unique_ptr<ObjectIterator> LargeObjectSpace::GetObjectIterator() {
  return std::unique_ptr<ObjectIterator>(new LargeObjectIterator(this));
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
    // strings), thin strings (sequential strings that have been
    // morphed into thin strings), fixed arrays, fixed double arrays,
    // byte arrays, and free space (right after allocation) in the
    // large object space.
    CHECK(object->IsAbstractCode() || object->IsSeqString() ||
          object->IsExternalString() || object->IsThinString() ||
          object->IsFixedArray() || object->IsFixedDoubleArray() ||
          object->IsByteArray() || object->IsFreeSpace());

    // The object itself should look OK.
    object->ObjectVerify();

    if (!FLAG_verify_heap_skip_remembered_set) {
      heap()->VerifyRememberedSetFor(object);
    }

    // Byte arrays and strings don't have interior pointers.
    if (object->IsAbstractCode()) {
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
  PrintF("  size: %" PRIuS "\n", size_);
  int num_objects = 0;
  ClearHistograms(heap()->isolate());
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.Next(); obj != NULL; obj = it.Next()) {
    num_objects++;
    CollectHistogramInfo(obj);
  }

  PrintF(
      "  number of objects %d, "
      "size of objects %" PRIuS "\n",
      num_objects, objects_size_);
  if (num_objects > 0) ReportHistogram(heap()->isolate(), false);
}


void Page::Print() {
  // Make a best-effort to print the objects in the page.
  PrintF("Page@%p in %s\n", static_cast<void*>(this->address()),
         AllocationSpaceName(this->owner()->identity()));
  printf(" --------------------------------------\n");
  HeapObjectIterator objects(this);
  unsigned mark_size = 0;
  for (HeapObject* object = objects.Next(); object != NULL;
       object = objects.Next()) {
    bool is_marked =
        ObjectMarking::IsBlackOrGrey(object, MarkingState::Internal(object));
    PrintF(" %c ", (is_marked ? '!' : ' '));  // Indent a little.
    if (is_marked) {
      mark_size += object->Size();
    }
    object->ShortPrint();
    PrintF("\n");
  }
  printf(" --------------------------------------\n");
  printf(" Marked: %x, LiveCount: %" V8PRIdPTR "\n", mark_size,
         MarkingState::Internal(this).live_bytes());
}

#endif  // DEBUG
}  // namespace internal
}  // namespace v8
