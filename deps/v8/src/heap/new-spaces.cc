// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/new-spaces.h"

#include "src/common/globals.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/marking-state.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

Page* SemiSpace::InitializePage(MemoryChunk* chunk) {
  bool in_to_space = (id() != kFromSpace);
  chunk->SetFlag(in_to_space ? MemoryChunk::TO_PAGE : MemoryChunk::FROM_PAGE);
  Page* page = static_cast<Page*>(chunk);
  page->SetYoungGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->list_node().Initialize();
  if (v8_flags.minor_mc) {
    heap()->non_atomic_marking_state()->ClearLiveness(page);
  }
  page->InitializationMemoryFence();
  return page;
}

bool SemiSpace::EnsureCurrentCapacity() {
  if (IsCommitted()) {
    const int expected_pages =
        static_cast<int>(target_capacity_ / Page::kPageSize);
    // `target_capacity_` is a multiple of `Page::kPageSize`.
    DCHECK_EQ(target_capacity_, expected_pages * Page::kPageSize);
    MemoryChunk* current_page = first_page();
    int actual_pages = 0;

    // First iterate through the pages list until expected pages if so many
    // pages exist.
    while (current_page != nullptr && actual_pages < expected_pages) {
      actual_pages++;
      current_page = current_page->list_node().next();
    }

    DCHECK_LE(actual_pages, expected_pages);

    // Free all overallocated pages which are behind current_page.
    while (current_page) {
      DCHECK_EQ(actual_pages, expected_pages);
      MemoryChunk* next_current = current_page->list_node().next();
      // Promoted pages contain live objects and should not be discarded.
      DCHECK(!current_page->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
      // `current_page_` contains the current allocation area. Thus, we should
      // never free the `current_page_`. Furthermore, live objects generally
      // reside before the current allocation area, so `current_page_` also
      // serves as a guard against freeing pages with live objects on them.
      DCHECK_NE(current_page, current_page_);
      AccountUncommitted(Page::kPageSize);
      DecrementCommittedPhysicalMemory(current_page->CommittedPhysicalMemory());
      memory_chunk_list_.Remove(current_page);
      // Clear new space flags to avoid this page being treated as a new
      // space page that is potentially being swept.
      current_page->ClearFlags(Page::kIsInYoungGenerationMask);
      heap()->memory_allocator()->Free(
          MemoryAllocator::FreeMode::kConcurrentlyAndPool, current_page);
      current_page = next_current;
    }

    // Add more pages if we have less than expected_pages.
    NonAtomicMarkingState* marking_state = heap()->non_atomic_marking_state();
    while (actual_pages < expected_pages) {
      actual_pages++;
      current_page = heap()->memory_allocator()->AllocatePage(
          MemoryAllocator::AllocationMode::kUsePool, this, NOT_EXECUTABLE);
      if (current_page == nullptr) return false;
      DCHECK_NOT_NULL(current_page);
      AccountCommitted(Page::kPageSize);
      IncrementCommittedPhysicalMemory(current_page->CommittedPhysicalMemory());
      memory_chunk_list_.PushBack(current_page);
      marking_state->ClearLiveness(current_page);
      current_page->SetFlags(first_page()->GetFlags());
      heap()->CreateFillerObjectAt(current_page->area_start(),
                                   static_cast<int>(current_page->area_size()));
    }
    DCHECK_EQ(expected_pages, actual_pages);
  }
  return true;
}

// -----------------------------------------------------------------------------
// SemiSpace implementation

void SemiSpace::SetUp(size_t initial_capacity, size_t maximum_capacity) {
  DCHECK_GE(maximum_capacity, static_cast<size_t>(Page::kPageSize));
  minimum_capacity_ = RoundDown(initial_capacity, Page::kPageSize);
  target_capacity_ = minimum_capacity_;
  maximum_capacity_ = RoundDown(maximum_capacity, Page::kPageSize);
}

void SemiSpace::TearDown() {
  // Properly uncommit memory to keep the allocator counters in sync.
  if (IsCommitted()) {
    Uncommit();
  }
  target_capacity_ = maximum_capacity_ = 0;
}

bool SemiSpace::Commit() {
  DCHECK(!IsCommitted());
  DCHECK_EQ(CommittedMemory(), size_t(0));
  const int num_pages = static_cast<int>(target_capacity_ / Page::kPageSize);
  DCHECK(num_pages);
  for (int pages_added = 0; pages_added < num_pages; pages_added++) {
    // Pages in the new spaces can be moved to the old space by the full
    // collector. Therefore, they must be initialized with the same FreeList as
    // old pages.
    Page* new_page = heap()->memory_allocator()->AllocatePage(
        MemoryAllocator::AllocationMode::kUsePool, this, NOT_EXECUTABLE);
    if (new_page == nullptr) {
      if (pages_added) RewindPages(pages_added);
      DCHECK(!IsCommitted());
      return false;
    }
    memory_chunk_list_.PushBack(new_page);
    IncrementCommittedPhysicalMemory(new_page->CommittedPhysicalMemory());
    heap()->CreateFillerObjectAt(new_page->area_start(),
                                 static_cast<int>(new_page->area_size()));
  }
  Reset();
  AccountCommitted(target_capacity_);
  if (age_mark_ == kNullAddress) {
    age_mark_ = first_page()->area_start();
  }
  DCHECK(IsCommitted());
  return true;
}

void SemiSpace::Uncommit() {
  DCHECK(IsCommitted());
  int actual_pages = 0;
  while (!memory_chunk_list_.Empty()) {
    actual_pages++;
    MemoryChunk* chunk = memory_chunk_list_.front();
    DecrementCommittedPhysicalMemory(chunk->CommittedPhysicalMemory());
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free(
        MemoryAllocator::FreeMode::kConcurrentlyAndPool, chunk);
  }
  current_page_ = nullptr;
  current_capacity_ = 0;
  size_t removed_page_size =
      static_cast<size_t>(actual_pages * Page::kPageSize);
  DCHECK_EQ(CommittedMemory(), removed_page_size);
  DCHECK_EQ(CommittedPhysicalMemory(), 0);
  AccountUncommitted(removed_page_size);
  DCHECK(!IsCommitted());
}

size_t SemiSpace::CommittedPhysicalMemory() const {
  if (!IsCommitted()) return 0;
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  return committed_physical_memory_;
}

bool SemiSpace::GrowTo(size_t new_capacity) {
  if (!IsCommitted()) {
    if (!Commit()) return false;
  }
  DCHECK_EQ(new_capacity & kPageAlignmentMask, 0u);
  DCHECK_LE(new_capacity, maximum_capacity_);
  DCHECK_GT(new_capacity, target_capacity_);
  const size_t delta = new_capacity - target_capacity_;
  DCHECK(IsAligned(delta, AllocatePageSize()));
  const int delta_pages = static_cast<int>(delta / Page::kPageSize);
  DCHECK(last_page());
  NonAtomicMarkingState* marking_state = heap()->non_atomic_marking_state();
  for (int pages_added = 0; pages_added < delta_pages; pages_added++) {
    Page* new_page = heap()->memory_allocator()->AllocatePage(
        MemoryAllocator::AllocationMode::kUsePool, this, NOT_EXECUTABLE);
    if (new_page == nullptr) {
      if (pages_added) RewindPages(pages_added);
      return false;
    }
    memory_chunk_list_.PushBack(new_page);
    marking_state->ClearLiveness(new_page);
    IncrementCommittedPhysicalMemory(new_page->CommittedPhysicalMemory());
    // Duplicate the flags that was set on the old page.
    new_page->SetFlags(last_page()->GetFlags(), Page::kCopyOnFlipFlagsMask);
    heap()->CreateFillerObjectAt(new_page->area_start(),
                                 static_cast<int>(new_page->area_size()));
  }
  AccountCommitted(delta);
  target_capacity_ = new_capacity;
  return true;
}

void SemiSpace::RewindPages(int num_pages) {
  DCHECK_GT(num_pages, 0);
  DCHECK(last_page());
  while (num_pages > 0) {
    MemoryChunk* last = last_page();
    memory_chunk_list_.Remove(last);
    DecrementCommittedPhysicalMemory(last->CommittedPhysicalMemory());
    heap()->memory_allocator()->Free(
        MemoryAllocator::FreeMode::kConcurrentlyAndPool, last);
    num_pages--;
  }
}

void SemiSpace::ShrinkTo(size_t new_capacity) {
  DCHECK_EQ(new_capacity & kPageAlignmentMask, 0u);
  DCHECK_GE(new_capacity, minimum_capacity_);
  DCHECK_LT(new_capacity, target_capacity_);
  if (IsCommitted()) {
    const size_t delta = target_capacity_ - new_capacity;
    DCHECK(IsAligned(delta, Page::kPageSize));
    int delta_pages = static_cast<int>(delta / Page::kPageSize);
    RewindPages(delta_pages);
    AccountUncommitted(delta);
  }
  target_capacity_ = new_capacity;
}

void SemiSpace::FixPagesFlags(Page::MainThreadFlags flags,
                              Page::MainThreadFlags mask) {
  for (Page* page : *this) {
    page->set_owner(this);
    page->SetFlags(flags, mask);
    if (id_ == kToSpace) {
      page->ClearFlag(MemoryChunk::FROM_PAGE);
      page->SetFlag(MemoryChunk::TO_PAGE);
      page->ClearFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
    } else {
      page->SetFlag(MemoryChunk::FROM_PAGE);
      page->ClearFlag(MemoryChunk::TO_PAGE);
    }
    DCHECK(page->InYoungGeneration());
  }
}

void SemiSpace::Reset() {
  DCHECK(first_page());
  DCHECK(last_page());
  current_page_ = first_page();
  current_capacity_ = Page::kPageSize;
}

void SemiSpace::RemovePage(Page* page) {
  if (current_page_ == page) {
    if (page->prev_page()) {
      current_page_ = page->prev_page();
    }
  }
  memory_chunk_list_.Remove(page);
  AccountUncommitted(Page::kPageSize);
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void SemiSpace::PrependPage(Page* page) {
  page->SetFlags(current_page()->GetFlags());
  page->set_owner(this);
  memory_chunk_list_.PushFront(page);
  current_capacity_ += Page::kPageSize;
  AccountCommitted(Page::kPageSize);
  IncrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void SemiSpace::MovePageToTheEnd(Page* page) {
  DCHECK_EQ(page->owner(), this);
  memory_chunk_list_.Remove(page);
  memory_chunk_list_.PushBack(page);
  current_page_ = page;
}

void SemiSpace::Swap(SemiSpace* from, SemiSpace* to) {
  // We won't be swapping semispaces without data in them.
  DCHECK(from->first_page());
  DCHECK(to->first_page());

  auto saved_to_space_flags = to->current_page()->GetFlags();

  // We swap all properties but id_.
  std::swap(from->target_capacity_, to->target_capacity_);
  std::swap(from->maximum_capacity_, to->maximum_capacity_);
  std::swap(from->minimum_capacity_, to->minimum_capacity_);
  std::swap(from->age_mark_, to->age_mark_);
  std::swap(from->memory_chunk_list_, to->memory_chunk_list_);
  std::swap(from->current_page_, to->current_page_);
  std::swap(from->external_backing_store_bytes_,
            to->external_backing_store_bytes_);
  std::swap(from->committed_physical_memory_, to->committed_physical_memory_);

  to->FixPagesFlags(saved_to_space_flags, Page::kCopyOnFlipFlagsMask);
  from->FixPagesFlags(Page::NO_FLAGS, Page::NO_FLAGS);
}

void SemiSpace::IncrementCommittedPhysicalMemory(size_t increment_value) {
  if (!base::OS::HasLazyCommits()) return;
  DCHECK_LE(committed_physical_memory_,
            committed_physical_memory_ + increment_value);
  committed_physical_memory_ += increment_value;
}

void SemiSpace::DecrementCommittedPhysicalMemory(size_t decrement_value) {
  if (!base::OS::HasLazyCommits()) return;
  DCHECK_LE(decrement_value, committed_physical_memory_);
  committed_physical_memory_ -= decrement_value;
}

void SemiSpace::AddRangeToActiveSystemPages(Address start, Address end) {
  Page* page = current_page();

  DCHECK_LE(page->address(), start);
  DCHECK_LT(start, end);
  DCHECK_LE(end, page->address() + Page::kPageSize);

  const size_t added_pages = page->active_system_pages()->Add(
      start - page->address(), end - page->address(),
      MemoryAllocator::GetCommitPageSizeBits());
  IncrementCommittedPhysicalMemory(added_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void SemiSpace::set_age_mark(Address mark) {
  DCHECK_EQ(Page::FromAllocationAreaAddress(mark)->owner(), this);
  age_mark_ = mark;
  // Mark all pages up to the one containing mark.
  for (Page* p : PageRange(space_start(), mark)) {
    p->SetFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
  }
}

std::unique_ptr<ObjectIterator> SemiSpace::GetObjectIterator(Heap* heap) {
  // Use the SemiSpaceNewSpace::NewObjectIterator to iterate the ToSpace.
  UNREACHABLE();
}

#ifdef DEBUG
void SemiSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void SemiSpace::VerifyPageMetadata() const {
  bool is_from_space = (id_ == kFromSpace);
  size_t external_backing_store_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_backing_store_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  int actual_pages = 0;
  size_t computed_committed_physical_memory = 0;

  for (const Page* page : *this) {
    CHECK_EQ(page->owner(), this);
    CHECK(page->InNewSpace());
    CHECK(page->IsFlagSet(is_from_space ? MemoryChunk::FROM_PAGE
                                        : MemoryChunk::TO_PAGE));
    CHECK(!page->IsFlagSet(is_from_space ? MemoryChunk::TO_PAGE
                                         : MemoryChunk::FROM_PAGE));
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
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      external_backing_store_bytes[t] += page->ExternalBackingStoreBytes(t);
    }

    computed_committed_physical_memory += page->CommittedPhysicalMemory();

    CHECK_IMPLIES(page->list_node().prev(),
                  page->list_node().prev()->list_node().next() == page);
    actual_pages++;
  }
  CHECK_EQ(actual_pages * size_t(Page::kPageSize), CommittedMemory());
  CHECK_EQ(computed_committed_physical_memory, CommittedPhysicalMemory());

  for (int i = 0; i < kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_backing_store_bytes[t], ExternalBackingStoreBytes(t));
  }
}
#endif  // VERIFY_HEAP

#ifdef DEBUG
void SemiSpace::AssertValidRange(Address start, Address end) {
  // Addresses belong to same semi-space
  Page* page = Page::FromAllocationAreaAddress(start);
  Page* end_page = Page::FromAllocationAreaAddress(end);
  SemiSpace* space = reinterpret_cast<SemiSpace*>(page->owner());
  DCHECK_EQ(space, end_page->owner());
  // Start address is before end address, either on same page,
  // or end address is on a later page in the linked list of
  // semi-space pages.
  if (page == end_page) {
    DCHECK_LE(start, end);
  } else {
    while (page != end_page) {
      page = page->next_page();
    }
    DCHECK(page);
  }
}
#endif

// -----------------------------------------------------------------------------
// NewSpace implementation

NewSpace::NewSpace(Heap* heap, LinearAllocationArea& allocation_info)
    : SpaceWithLinearArea(heap, NEW_SPACE, new NoFreeList(),
                          allocation_counter_, allocation_info,
                          linear_area_original_data_) {}

void NewSpace::MaybeFreeUnusedLab(LinearAllocationArea info) {
  if (allocation_info_.MergeIfAdjacent(info)) {
    linear_area_original_data_.set_original_top_release(allocation_info_.top());
  }

#if DEBUG
  VerifyTop();
#endif
}

#if DEBUG
void NewSpace::VerifyTop() const {
  SpaceWithLinearArea::VerifyTop();

  // Ensure that original_top_ always >= LAB start. The delta between start_
  // and top_ is still to be processed by allocation observers.
  DCHECK_GE(linear_area_original_data_.get_original_top_acquire(),
            allocation_info_.start());

  // Ensure that limit() is <= original_limit_.
  DCHECK_LE(allocation_info_.limit(),
            linear_area_original_data_.get_original_limit_relaxed());
}
#endif  // DEBUG

void NewSpace::PromotePageToOldSpace(Page* page) {
  DCHECK(!page->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION));
  DCHECK(page->InYoungGeneration());
  page->ClearWasUsedForAllocation();
  RemovePage(page);
  Page* new_page = Page::ConvertNewToOld(page);
  DCHECK(!new_page->InYoungGeneration());
  USE(new_page);
}

// -----------------------------------------------------------------------------
// SemiSpaceNewSpace implementation

SemiSpaceNewSpace::SemiSpaceNewSpace(Heap* heap,
                                     size_t initial_semispace_capacity,
                                     size_t max_semispace_capacity,
                                     LinearAllocationArea& allocation_info)
    : NewSpace(heap, allocation_info),
      to_space_(heap, kToSpace),
      from_space_(heap, kFromSpace) {
  DCHECK(initial_semispace_capacity <= max_semispace_capacity);

  to_space_.SetUp(initial_semispace_capacity, max_semispace_capacity);
  from_space_.SetUp(initial_semispace_capacity, max_semispace_capacity);
  if (!to_space_.Commit()) {
    V8::FatalProcessOutOfMemory(heap->isolate(), "New space setup");
  }
  DCHECK(!from_space_.IsCommitted());  // No need to use memory yet.
  ResetLinearAllocationArea();
}

SemiSpaceNewSpace::~SemiSpaceNewSpace() {
  // Tears down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  allocation_info_.Reset(kNullAddress, kNullAddress);

  to_space_.TearDown();
  from_space_.TearDown();
}

void SemiSpaceNewSpace::Grow() {
  heap()->safepoint()->AssertActive();
  // Double the semispace size but only up to maximum capacity.
  DCHECK(TotalCapacity() < MaximumCapacity());
  size_t new_capacity = std::min(
      MaximumCapacity(),
      static_cast<size_t>(v8_flags.semi_space_growth_factor) * TotalCapacity());
  if (to_space_.GrowTo(new_capacity)) {
    // Only grow from space if we managed to grow to-space.
    if (!from_space_.GrowTo(new_capacity)) {
      // If we managed to grow to-space but couldn't grow from-space,
      // attempt to shrink to-space.
      to_space_.ShrinkTo(from_space_.target_capacity());
    }
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}

void SemiSpaceNewSpace::Shrink() {
  size_t new_capacity = std::max(InitialTotalCapacity(), 2 * Size());
  size_t rounded_new_capacity = ::RoundUp(new_capacity, Page::kPageSize);
  if (rounded_new_capacity < TotalCapacity()) {
    to_space_.ShrinkTo(rounded_new_capacity);
    // Only shrink from-space if we managed to shrink to-space.
    if (from_space_.IsCommitted()) from_space_.Reset();
    from_space_.ShrinkTo(rounded_new_capacity);
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  if (!from_space_.IsCommitted()) return;
  from_space_.Uncommit();
}

size_t SemiSpaceNewSpace::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = to_space_.CommittedPhysicalMemory();
  if (from_space_.IsCommitted()) {
    size += from_space_.CommittedPhysicalMemory();
  }
  return size;
}

bool SemiSpaceNewSpace::EnsureCurrentCapacity() {
  // Order here is important to make use of the page pool.
  return to_space_.EnsureCurrentCapacity() &&
         from_space_.EnsureCurrentCapacity();
}

void SemiSpaceNewSpace::UpdateLinearAllocationArea(Address known_top) {
  AdvanceAllocationObservers();

  Address new_top = known_top == 0 ? to_space_.page_low() : known_top;
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(new_top, to_space_.page_high());
  // The order of the following two stores is important.
  // See the corresponding loads in ConcurrentMarking::Run.
  {
    base::SharedMutexGuard<base::kExclusive> guard(linear_area_lock());
    linear_area_original_data_.set_original_limit_relaxed(limit());
    linear_area_original_data_.set_original_top_release(top());
  }

  // The linear allocation area should reach the end of the page, so no filler
  // object is needed there to make the page iterable.
  DCHECK_EQ(limit(), to_space_.page_high());

  to_space_.AddRangeToActiveSystemPages(top(), limit());
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  UpdateInlineAllocationLimit();
}

void SemiSpaceNewSpace::ResetLinearAllocationArea() {
  to_space_.Reset();
  UpdateLinearAllocationArea();
  // Clear all mark-bits in the to-space.
  NonAtomicMarkingState* marking_state = heap()->non_atomic_marking_state();
  for (Page* p : to_space_) {
    marking_state->ClearLiveness(p);
    // Concurrent marking may have local live bytes for this page.
    heap()->concurrent_marking()->ClearMemoryChunkData(p);
  }
}

void SemiSpaceNewSpace::UpdateInlineAllocationLimitForAllocation(
    size_t min_size) {
  Address new_limit = ComputeLimit(top(), to_space_.page_high(),
                                   ALIGN_TO_ALLOCATION_ALIGNMENT(min_size));
  DCHECK_LE(top(), new_limit);
  DCHECK_LE(new_limit, to_space_.page_high());
  allocation_info_.SetLimit(new_limit);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  // Add a filler object after the linear allocation area (if there is space
  // left), to ensure that the page will be iterable.
  heap()->CreateFillerObjectAt(
      limit(), static_cast<int>(to_space_.page_high() - limit()));

#if DEBUG
  VerifyTop();
#endif
}

void SemiSpaceNewSpace::UpdateInlineAllocationLimit() {
  UpdateInlineAllocationLimitForAllocation(0);
}

bool SemiSpaceNewSpace::AddFreshPage() {
  Address top = allocation_info_.top();
  DCHECK(!OldSpace::IsAtPageStart(top));

  // Clear remainder of current page.
  Address limit = Page::FromAllocationAreaAddress(top)->area_end();
  int remaining_in_page = static_cast<int>(limit - top);
  heap()->CreateFillerObjectAt(top, remaining_in_page);

  if (!to_space_.AdvancePage()) {
    // No more pages left to advance.
    return false;
  }

  // We park unused allocation buffer space of allocations happening from the
  // mutator.
  if (v8_flags.allocation_buffer_parking &&
      heap()->gc_state() == Heap::NOT_IN_GC &&
      remaining_in_page >= kAllocationBufferParkingThreshold) {
    parked_allocation_buffers_.push_back(
        ParkedAllocationBuffer(remaining_in_page, top));
  }
  UpdateLinearAllocationArea();

  return true;
}

bool SemiSpaceNewSpace::AddParkedAllocationBuffer(
    int size_in_bytes, AllocationAlignment alignment) {
  int parked_size = 0;
  Address start = 0;
  for (auto it = parked_allocation_buffers_.begin();
       it != parked_allocation_buffers_.end();) {
    parked_size = it->first;
    start = it->second;
    int filler_size = Heap::GetFillToAlign(start, alignment);
    if (size_in_bytes + filler_size <= parked_size) {
      parked_allocation_buffers_.erase(it);
      Page* page = Page::FromAddress(start);
      // We move a page with a parked allocation to the end of the pages list
      // to maintain the invariant that the last page is the used one.
      to_space_.MovePageToTheEnd(page);
      UpdateLinearAllocationArea(start);
      return true;
    } else {
      it++;
    }
  }
  return false;
}

void SemiSpaceNewSpace::ResetParkedAllocationBuffers() {
  parked_allocation_buffers_.clear();
}

void SemiSpaceNewSpace::FreeLinearAllocationArea() {
  AdvanceAllocationObservers();
  MakeLinearAllocationAreaIterable();
  UpdateInlineAllocationLimit();
}

#if DEBUG
void SemiSpaceNewSpace::VerifyTop() const {
  NewSpace::VerifyTop();

  // original_limit_ always needs to be end of curent to space page.
  DCHECK_EQ(linear_area_original_data_.get_original_limit_relaxed(),
            to_space_.page_high());
}
#endif  // DEBUG

#ifdef VERIFY_HEAP
// We do not use the SemiSpaceObjectIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void SemiSpaceNewSpace::Verify(Isolate* isolate,
                               SpaceVerificationVisitor* visitor) const {
  // The allocation pointer should be in the space or at the very end.
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  VerifyObjects(isolate, visitor);

  // Check semi-spaces.
  CHECK_EQ(from_space_.id(), kFromSpace);
  CHECK_EQ(to_space_.id(), kToSpace);
  from_space_.VerifyPageMetadata();
  to_space_.VerifyPageMetadata();
}

// We do not use the SemiSpaceObjectIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void SemiSpaceNewSpace::VerifyObjects(Isolate* isolate,
                                      SpaceVerificationVisitor* visitor) const {
  size_t external_space_bytes[kNumTypes];
  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  PtrComprCageBase cage_base(isolate);
  for (const Page* page = to_space_.first_page(); page;
       page = page->next_page()) {
    visitor->VerifyPage(page);

    Address current_address = page->area_start();

    while (!Page::IsAlignedToPageSize(current_address)) {
      HeapObject object = HeapObject::FromAddress(current_address);

      // The first word should be a map, and we expect all map pointers to
      // be in map space or read-only space.
      int size = object.Size(cage_base);

      visitor->VerifyObject(object);

      if (object.IsExternalString(cage_base)) {
        ExternalString external_string = ExternalString::cast(object);
        size_t string_size = external_string.ExternalPayloadSize();
        external_space_bytes[ExternalBackingStoreType::kExternalString] +=
            string_size;
      }

      current_address += ALIGN_TO_ALLOCATION_ALIGNMENT(size);
    }

    visitor->VerifyPageDone(page);
  }

  for (int i = 0; i < kNumTypes; i++) {
    if (i == ExternalBackingStoreType::kArrayBuffer) continue;
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }

  if (!v8_flags.concurrent_array_buffer_sweeping) {
    size_t bytes = heap()->array_buffer_sweeper()->young().BytesSlow();
    CHECK_EQ(bytes,
             ExternalBackingStoreBytes(ExternalBackingStoreType::kArrayBuffer));
  }
}
#endif  // VERIFY_HEAP

void SemiSpaceNewSpace::MakeIterable() {
  MakeAllPagesInFromSpaceIterable();
  MakeUnusedPagesInToSpaceIterable();
}

void SemiSpaceNewSpace::MakeAllPagesInFromSpaceIterable() {
  if (!IsFromSpaceCommitted()) return;

  // Fix all pages in the "from" semispace.
  for (Page* page : from_space()) {
    heap()->CreateFillerObjectAt(page->area_start(),
                                 static_cast<int>(page->area_size()));
  }
}

void SemiSpaceNewSpace::MakeUnusedPagesInToSpaceIterable() {
  PageIterator it(to_space().current_page());

  // Fix the current page, above the LAB.
  DCHECK_NOT_NULL(*it);
  if (limit() != (*it)->area_end()) {
    DCHECK((*it)->Contains(limit()));
    heap()->CreateFillerObjectAt(limit(),
                                 static_cast<int>((*it)->area_end() - limit()));
  }

  // Fix the remaining unused pages in the "to" semispace.
  for (Page* page = *(++it); page != nullptr; page = *(++it)) {
    heap()->CreateFillerObjectAt(page->area_start(),
                                 static_cast<int>(page->area_size()));
  }
}

bool SemiSpaceNewSpace::ShouldBePromoted(Address address) const {
  Page* page = Page::FromAddress(address);
  Address current_age_mark = age_mark();
  return page->IsFlagSet(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK) &&
         (!page->ContainsLimit(current_age_mark) || address < current_age_mark);
}

std::unique_ptr<ObjectIterator> SemiSpaceNewSpace::GetObjectIterator(
    Heap* heap) {
  return std::unique_ptr<ObjectIterator>(new SemiSpaceObjectIterator(this));
}

bool SemiSpaceNewSpace::ContainsSlow(Address a) const {
  return from_space_.ContainsSlow(a) || to_space_.ContainsSlow(a);
}

size_t SemiSpaceNewSpace::AllocatedSinceLastGC() const {
  const Address age_mark = to_space_.age_mark();
  DCHECK_NE(age_mark, kNullAddress);
  DCHECK_NE(top(), kNullAddress);
  Page* const age_mark_page = Page::FromAllocationAreaAddress(age_mark);
  Page* const last_page = Page::FromAllocationAreaAddress(top());
  Page* current_page = age_mark_page;
  size_t allocated = 0;
  if (current_page != last_page) {
    DCHECK_EQ(current_page, age_mark_page);
    DCHECK_GE(age_mark_page->area_end(), age_mark);
    allocated += age_mark_page->area_end() - age_mark;
    current_page = current_page->next_page();
  } else {
    DCHECK_GE(top(), age_mark);
    return top() - age_mark;
  }
  while (current_page != last_page) {
    DCHECK_NE(current_page, age_mark_page);
    allocated += MemoryChunkLayout::AllocatableMemoryInDataPage();
    current_page = current_page->next_page();
  }
  DCHECK_GE(top(), current_page->area_start());
  allocated += top() - current_page->area_start();
  DCHECK_LE(allocated, Size());
  return allocated;
}

void SemiSpaceNewSpace::Prologue() {
  if (from_space_.IsCommitted() || from_space_.Commit()) return;

  // Committing memory to from space failed.
  // Memory is exhausted and we will die.
  heap_->FatalProcessOutOfMemory("Committing semi space failed.");
}

void SemiSpaceNewSpace::EvacuatePrologue() {
  // Flip the semispaces.  After flipping, to space is empty, from space has
  // live objects.
  SemiSpace::Swap(&from_space_, &to_space_);
  ResetLinearAllocationArea();
  DCHECK_EQ(0u, Size());
}

void SemiSpaceNewSpace::GarbageCollectionEpilogue() { set_age_mark(top()); }

void SemiSpaceNewSpace::ZapUnusedMemory() {
  if (!IsFromSpaceCommitted()) return;
  for (Page* page : PageRange(from_space().first_page(), nullptr)) {
    heap_->memory_allocator()->ZapBlock(
        page->area_start(), page->HighWaterMark() - page->area_start(),
        heap_->ZapValue());
  }
}

void SemiSpaceNewSpace::RemovePage(Page* page) {
  DCHECK(!page->IsToPage());
  DCHECK(page->IsFromPage());
  from_space().RemovePage(page);
}

void SemiSpaceNewSpace::PromotePageInNewSpace(Page* page) {
  DCHECK(page->IsFromPage());
  from_space_.RemovePage(page);
  to_space_.PrependPage(page);
  page->SetFlag(Page::PAGE_NEW_NEW_PROMOTION);
}

bool SemiSpaceNewSpace::IsPromotionCandidate(const MemoryChunk* page) const {
  return !page->Contains(age_mark());
}

void SemiSpaceNewSpace::MakeLinearAllocationAreaIterable() {
  Address to_top = top();
  Page* page = Page::FromAddress(to_top - kTaggedSize);
  if (page->Contains(to_top)) {
    int remaining_in_page = static_cast<int>(page->area_end() - to_top);
    heap_->CreateFillerObjectAt(to_top, remaining_in_page);
  }
}

// -----------------------------------------------------------------------------
// PagedSpaceForNewSpace implementation

PagedSpaceForNewSpace::PagedSpaceForNewSpace(
    Heap* heap, size_t initial_capacity, size_t max_capacity,
    AllocationCounter& allocation_counter,
    LinearAllocationArea& allocation_info,
    LinearAreaOriginalData& linear_area_original_data)
    : PagedSpaceBase(heap, NEW_SPACE, NOT_EXECUTABLE,
                     FreeList::CreateFreeListForNewSpace(), allocation_counter,
                     allocation_info, linear_area_original_data,
                     CompactionSpaceKind::kNone),
      initial_capacity_(RoundDown(initial_capacity, Page::kPageSize)),
      max_capacity_(RoundDown(max_capacity, Page::kPageSize)),
      target_capacity_(initial_capacity_) {
  DCHECK_LE(initial_capacity_, max_capacity_);

  if (!PreallocatePages()) {
    V8::FatalProcessOutOfMemory(heap->isolate(), "New space setup");
  }
}

Page* PagedSpaceForNewSpace::InitializePage(MemoryChunk* chunk) {
  DCHECK_EQ(identity(), NEW_SPACE);
  Page* page = static_cast<Page*>(chunk);
  DCHECK_EQ(
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(page->owner_identity()),
      page->area_size());
  // Make sure that categories are initialized before freeing the area.
  page->ResetAllocationStatistics();
  page->SetFlags(Page::TO_PAGE);
  page->SetYoungGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  heap()->non_atomic_marking_state()->ClearLiveness(page);
  page->AllocateFreeListCategories();
  page->InitializeFreeListCategories();
  page->list_node().Initialize();
  page->InitializationMemoryFence();
  return page;
}

void PagedSpaceForNewSpace::Grow() {
  heap()->safepoint()->AssertActive();
  // Double the space size but only up to maximum capacity.
  DCHECK(TotalCapacity() < MaximumCapacity());
  target_capacity_ =
      std::min(MaximumCapacity(),
               RoundUp(static_cast<size_t>(v8_flags.semi_space_growth_factor) *
                           TotalCapacity(),
                       Page::kPageSize));
}

bool PagedSpaceForNewSpace::StartShrinking() {
  DCHECK_GE(current_capacity_, target_capacity_);
  DCHECK(heap()->tracer()->IsInAtomicPause());
  size_t new_target_capacity =
      RoundUp(std::max(initial_capacity_, 2 * Size()), Page::kPageSize);
  if (new_target_capacity > target_capacity_) return false;
  target_capacity_ = new_target_capacity;
  return true;
}

void PagedSpaceForNewSpace::FinishShrinking() {
  DCHECK(heap()->tracer()->IsInAtomicPause());
  if (current_capacity_ > target_capacity_) {
#if DEBUG
    // If `current_capacity_` is higher than `target_capacity_`, i.e. the
    // space could not be shrunk all the way down to `target_capacity_`, it
    // must mean that all pages contain live objects.
    for (Page* page : *this) {
      DCHECK_NE(0, heap()->non_atomic_marking_state()->live_bytes(page));
    }
#endif  // DEBUG
    target_capacity_ = current_capacity_;
  }
}

void PagedSpaceForNewSpace::UpdateInlineAllocationLimit() {
  PagedSpaceBase::UpdateInlineAllocationLimit();
}

size_t PagedSpaceForNewSpace::AddPage(Page* page) {
  current_capacity_ += Page::kPageSize;
  DCHECK_IMPLIES(!force_allocation_success_,
                 UsableCapacity() <= TotalCapacity());
  return PagedSpaceBase::AddPage(page);
}

void PagedSpaceForNewSpace::RemovePage(Page* page) {
  DCHECK_LE(Page::kPageSize, current_capacity_);
  current_capacity_ -= Page::kPageSize;
  PagedSpaceBase::RemovePage(page);
}

void PagedSpaceForNewSpace::ReleasePage(Page* page) {
  DCHECK_LE(Page::kPageSize, current_capacity_);
  current_capacity_ -= Page::kPageSize;
  PagedSpaceBase::ReleasePage(page);
}

bool PagedSpaceForNewSpace::PreallocatePages() {
  while (current_capacity_ < target_capacity_) {
    if (!TryExpandImpl()) return false;
  }
  DCHECK_GE(current_capacity_, target_capacity_);
  return true;
}

bool PagedSpaceForNewSpace::EnsureCurrentCapacity() {
  // Verify that the free space map is already initialized. Otherwise, new free
  // list entries will be invalid.
  DCHECK_NE(kNullAddress,
            heap()->isolate()->root(RootIndex::kFreeSpaceMap).ptr());
  return PreallocatePages();
}

void PagedSpaceForNewSpace::FreeLinearAllocationArea() {
  size_t remaining_allocation_area_size = limit() - top();
  DCHECK_GE(allocated_linear_areas_, remaining_allocation_area_size);
  allocated_linear_areas_ -= remaining_allocation_area_size;
  PagedSpaceBase::FreeLinearAllocationArea();
}

bool PagedSpaceForNewSpace::ShouldReleaseEmptyPage() const {
  return current_capacity_ > target_capacity_;
}

void PagedSpaceForNewSpace::RefillFreeList() {
  // New space is not used for concurrent allcations or allocations during
  // evacuation.
  DCHECK(heap_->IsMainThread() ||
         (heap_->IsSharedMainThread() &&
          !heap_->isolate()->is_shared_space_isolate()));
  DCHECK(!is_compaction_space());

  Sweeper* sweeper = heap()->sweeper();

  Sweeper::SweptList swept_pages = sweeper->GetAllSweptPagesSafe(this);
  if (swept_pages.empty()) return;

  base::MutexGuard guard(mutex());
  for (Page* p : swept_pages) {
    DCHECK(!p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE));
    RefineAllocatedBytesAfterSweeping(p);
    RelinkFreeListCategories(p);
  }
}

bool PagedSpaceForNewSpace::AddPageBeyondCapacity(int size_in_bytes,
                                                  AllocationOrigin origin) {
  DCHECK(heap()->sweeper()->IsSweepingDoneForSpace(NEW_SPACE));
  if (!force_allocation_success_ &&
      ((UsableCapacity() >= TotalCapacity()) ||
       (TotalCapacity() - UsableCapacity() < Page::kPageSize)))
    return false;
  if (!heap()->CanExpandOldGeneration(Size() + heap()->new_lo_space()->Size() +
                                      Page::kPageSize)) {
    // Assuming all of new space if alive, doing a full GC and promoting all
    // objects should still succeed. Don't let new space grow if it means it
    // will exceed the available size of old space.
    return false;
  }
  DCHECK_IMPLIES(heap()->incremental_marking()->IsMarking(),
                 heap()->incremental_marking()->IsMajorMarking());
  if (!TryExpandImpl()) return false;
  return TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                       origin);
}

// -----------------------------------------------------------------------------
// PagedNewSpace implementation

PagedNewSpace::PagedNewSpace(Heap* heap, size_t initial_capacity,
                             size_t max_capacity,
                             LinearAllocationArea& allocation_info)
    : NewSpace(heap, allocation_info),
      paged_space_(heap, initial_capacity, max_capacity, allocation_counter_,
                   allocation_info_, linear_area_original_data_) {}

PagedNewSpace::~PagedNewSpace() {
  // Tears down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  allocation_info_.Reset(kNullAddress, kNullAddress);

  paged_space_.TearDown();
}

}  // namespace internal
}  // namespace v8
