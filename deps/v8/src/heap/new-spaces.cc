// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/new-spaces.h"

#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
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
#ifdef ENABLE_MINOR_MC
  if (FLAG_minor_mc) {
    page->AllocateYoungGenerationBitmap();
    heap()
        ->minor_mark_compact_collector()
        ->non_atomic_marking_state()
        ->ClearLiveness(page);
  }
#endif  // ENABLE_MINOR_MC
  page->InitializationMemoryFence();
  return page;
}

bool SemiSpace::EnsureCurrentCapacity() {
  if (IsCommitted()) {
    const int expected_pages =
        static_cast<int>(target_capacity_ / Page::kPageSize);
    MemoryChunk* current_page = first_page();
    int actual_pages = 0;

    // First iterate through the pages list until expected pages if so many
    // pages exist.
    while (current_page != nullptr && actual_pages < expected_pages) {
      actual_pages++;
      current_page = current_page->list_node().next();
    }

    // Free all overallocated pages which are behind current_page.
    while (current_page) {
      MemoryChunk* next_current = current_page->list_node().next();
      memory_chunk_list_.Remove(current_page);
      // Clear new space flags to avoid this page being treated as a new
      // space page that is potentially being swept.
      current_page->SetFlags(0, Page::kIsInYoungGenerationMask);
      heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(
          current_page);
      current_page = next_current;
    }

    // Add more pages if we have less than expected_pages.
    IncrementalMarking::NonAtomicMarkingState* marking_state =
        heap()->incremental_marking()->non_atomic_marking_state();
    while (actual_pages < expected_pages) {
      actual_pages++;
      current_page =
          heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
              MemoryChunkLayout::AllocatableMemoryInDataPage(), this,
              NOT_EXECUTABLE);
      if (current_page == nullptr) return false;
      DCHECK_NOT_NULL(current_page);
      memory_chunk_list_.PushBack(current_page);
      marking_state->ClearLiveness(current_page);
      current_page->SetFlags(first_page()->GetFlags(),
                             static_cast<uintptr_t>(Page::kCopyAllFlags));
      heap()->CreateFillerObjectAt(current_page->area_start(),
                                   static_cast<int>(current_page->area_size()),
                                   ClearRecordedSlots::kNo);
    }
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
  const int num_pages = static_cast<int>(target_capacity_ / Page::kPageSize);
  DCHECK(num_pages);
  for (int pages_added = 0; pages_added < num_pages; pages_added++) {
    // Pages in the new spaces can be moved to the old space by the full
    // collector. Therefore, they must be initialized with the same FreeList as
    // old pages.
    Page* new_page =
        heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
            MemoryChunkLayout::AllocatableMemoryInDataPage(), this,
            NOT_EXECUTABLE);
    if (new_page == nullptr) {
      if (pages_added) RewindPages(pages_added);
      DCHECK(!IsCommitted());
      return false;
    }
    memory_chunk_list_.PushBack(new_page);
  }
  Reset();
  AccountCommitted(target_capacity_);
  if (age_mark_ == kNullAddress) {
    age_mark_ = first_page()->area_start();
  }
  DCHECK(IsCommitted());
  return true;
}

bool SemiSpace::Uncommit() {
  DCHECK(IsCommitted());
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(chunk);
  }
  current_page_ = nullptr;
  current_capacity_ = 0;
  AccountUncommitted(target_capacity_);
  heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  DCHECK(!IsCommitted());
  return true;
}

size_t SemiSpace::CommittedPhysicalMemory() {
  if (!IsCommitted()) return 0;
  size_t size = 0;
  for (Page* p : *this) {
    size += p->CommittedPhysicalMemory();
  }
  return size;
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
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (int pages_added = 0; pages_added < delta_pages; pages_added++) {
    Page* new_page =
        heap()->memory_allocator()->AllocatePage<MemoryAllocator::kPooled>(
            MemoryChunkLayout::AllocatableMemoryInDataPage(), this,
            NOT_EXECUTABLE);
    if (new_page == nullptr) {
      if (pages_added) RewindPages(pages_added);
      return false;
    }
    memory_chunk_list_.PushBack(new_page);
    marking_state->ClearLiveness(new_page);
    // Duplicate the flags that was set on the old page.
    new_page->SetFlags(last_page()->GetFlags(), Page::kCopyOnFlipFlagsMask);
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
    heap()->memory_allocator()->Free<MemoryAllocator::kPooledAndQueue>(last);
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
    heap()->memory_allocator()->unmapper()->FreeQueuedChunks();
  }
  target_capacity_ = new_capacity;
}

void SemiSpace::FixPagesFlags(intptr_t flags, intptr_t mask) {
  for (Page* page : *this) {
    page->set_owner(this);
    page->SetFlags(flags, mask);
    if (id_ == kToSpace) {
      page->ClearFlag(MemoryChunk::FROM_PAGE);
      page->SetFlag(MemoryChunk::TO_PAGE);
      page->ClearFlag(MemoryChunk::NEW_SPACE_BELOW_AGE_MARK);
      heap()->incremental_marking()->non_atomic_marking_state()->SetLiveBytes(
          page, 0);
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
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void SemiSpace::PrependPage(Page* page) {
  page->SetFlags(current_page()->GetFlags(),
                 static_cast<uintptr_t>(Page::kCopyAllFlags));
  page->set_owner(this);
  memory_chunk_list_.PushFront(page);
  current_capacity_ += Page::kPageSize;
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

  intptr_t saved_to_space_flags = to->current_page()->GetFlags();

  // We swap all properties but id_.
  std::swap(from->target_capacity_, to->target_capacity_);
  std::swap(from->maximum_capacity_, to->maximum_capacity_);
  std::swap(from->minimum_capacity_, to->minimum_capacity_);
  std::swap(from->age_mark_, to->age_mark_);
  std::swap(from->memory_chunk_list_, to->memory_chunk_list_);
  std::swap(from->current_page_, to->current_page_);
  std::swap(from->external_backing_store_bytes_,
            to->external_backing_store_bytes_);

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

std::unique_ptr<ObjectIterator> SemiSpace::GetObjectIterator(Heap* heap) {
  // Use the NewSpace::NewObjectIterator to iterate the ToSpace.
  UNREACHABLE();
}

#ifdef DEBUG
void SemiSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void SemiSpace::Verify() {
  bool is_from_space = (id_ == kFromSpace);
  size_t external_backing_store_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_backing_store_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  for (Page* page : *this) {
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

    CHECK_IMPLIES(page->list_node().prev(),
                  page->list_node().prev()->list_node().next() == page);
  }
  for (int i = 0; i < kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_backing_store_bytes[t], ExternalBackingStoreBytes(t));
  }
}
#endif

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
// SemiSpaceObjectIterator implementation.

SemiSpaceObjectIterator::SemiSpaceObjectIterator(NewSpace* space) {
  Initialize(space->first_allocatable_address(), space->top());
}

void SemiSpaceObjectIterator::Initialize(Address start, Address end) {
  SemiSpace::AssertValidRange(start, end);
  current_ = start;
  limit_ = end;
}

size_t NewSpace::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  size_t size = to_space_.CommittedPhysicalMemory();
  if (from_space_.IsCommitted()) {
    size += from_space_.CommittedPhysicalMemory();
  }
  return size;
}

// -----------------------------------------------------------------------------
// NewSpace implementation

NewSpace::NewSpace(Heap* heap, v8::PageAllocator* page_allocator,
                   size_t initial_semispace_capacity,
                   size_t max_semispace_capacity)
    : SpaceWithLinearArea(heap, NEW_SPACE, new NoFreeList()),
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

void NewSpace::TearDown() {
  allocation_info_.Reset(kNullAddress, kNullAddress);

  to_space_.TearDown();
  from_space_.TearDown();
}

void NewSpace::ResetParkedAllocationBuffers() {
  parked_allocation_buffers_.clear();
}

void NewSpace::Flip() { SemiSpace::Swap(&from_space_, &to_space_); }

void NewSpace::Grow() {
  DCHECK_IMPLIES(FLAG_local_heaps, heap()->safepoint()->IsActive());
  // Double the semispace size but only up to maximum capacity.
  DCHECK(TotalCapacity() < MaximumCapacity());
  size_t new_capacity = std::min(
      MaximumCapacity(),
      static_cast<size_t>(FLAG_semi_space_growth_factor) * TotalCapacity());
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

void NewSpace::Shrink() {
  size_t new_capacity = std::max(InitialTotalCapacity(), 2 * Size());
  size_t rounded_new_capacity = ::RoundUp(new_capacity, Page::kPageSize);
  if (rounded_new_capacity < TotalCapacity()) {
    to_space_.ShrinkTo(rounded_new_capacity);
    // Only shrink from-space if we managed to shrink to-space.
    if (from_space_.IsCommitted()) from_space_.Reset();
    from_space_.ShrinkTo(rounded_new_capacity);
  }
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}

bool NewSpace::Rebalance() {
  // Order here is important to make use of the page pool.
  return to_space_.EnsureCurrentCapacity() &&
         from_space_.EnsureCurrentCapacity();
}

void NewSpace::UpdateLinearAllocationArea(Address known_top) {
  AdvanceAllocationObservers();

  Address new_top = known_top == 0 ? to_space_.page_low() : known_top;
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(new_top, to_space_.page_high());
  // The order of the following two stores is important.
  // See the corresponding loads in ConcurrentMarking::Run.
  original_limit_.store(limit(), std::memory_order_relaxed);
  original_top_.store(top(), std::memory_order_release);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  UpdateInlineAllocationLimit(0);
}

void NewSpace::ResetLinearAllocationArea() {
  to_space_.Reset();
  UpdateLinearAllocationArea();
  // Clear all mark-bits in the to-space.
  IncrementalMarking::NonAtomicMarkingState* marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (Page* p : to_space_) {
    marking_state->ClearLiveness(p);
    // Concurrent marking may have local live bytes for this page.
    heap()->concurrent_marking()->ClearMemoryChunkData(p);
  }
}

void NewSpace::UpdateInlineAllocationLimit(size_t min_size) {
  Address new_limit = ComputeLimit(top(), to_space_.page_high(), min_size);
  DCHECK_LE(top(), new_limit);
  DCHECK_LE(new_limit, to_space_.page_high());
  allocation_info_.set_limit(new_limit);
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

#if DEBUG
  VerifyTop();
#endif
}

bool NewSpace::AddFreshPage() {
  Address top = allocation_info_.top();
  DCHECK(!OldSpace::IsAtPageStart(top));

  // Clear remainder of current page.
  Address limit = Page::FromAllocationAreaAddress(top)->area_end();
  int remaining_in_page = static_cast<int>(limit - top);
  heap()->CreateFillerObjectAt(top, remaining_in_page, ClearRecordedSlots::kNo);

  if (!to_space_.AdvancePage()) {
    // No more pages left to advance.
    return false;
  }

  // We park unused allocation buffer space of allocations happenting from the
  // mutator.
  if (FLAG_allocation_buffer_parking && heap()->gc_state() == Heap::NOT_IN_GC &&
      remaining_in_page >= kAllocationBufferParkingThreshold) {
    parked_allocation_buffers_.push_back(
        ParkedAllocationBuffer(remaining_in_page, top));
  }
  UpdateLinearAllocationArea();

  return true;
}

bool NewSpace::AddFreshPageSynchronized() {
  base::MutexGuard guard(&mutex_);
  return AddFreshPage();
}

bool NewSpace::AddParkedAllocationBuffer(int size_in_bytes,
                                         AllocationAlignment alignment) {
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
      // We move a page with a parked allocaiton to the end of the pages list
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

bool NewSpace::EnsureAllocation(int size_in_bytes,
                                AllocationAlignment alignment) {
  AdvanceAllocationObservers();

  Address old_top = allocation_info_.top();
  Address high = to_space_.page_high();
  int filler_size = Heap::GetFillToAlign(old_top, alignment);
  int aligned_size_in_bytes = size_in_bytes + filler_size;

  if (old_top + aligned_size_in_bytes <= high) {
    UpdateInlineAllocationLimit(aligned_size_in_bytes);
    return true;
  }

  // Not enough room in the page, try to allocate a new one.
  if (!AddFreshPage()) {
    // When we cannot grow NewSpace anymore we query for parked allocations.
    if (!FLAG_allocation_buffer_parking ||
        !AddParkedAllocationBuffer(size_in_bytes, alignment))
      return false;
  }

  old_top = allocation_info_.top();
  high = to_space_.page_high();
  filler_size = Heap::GetFillToAlign(old_top, alignment);
  aligned_size_in_bytes = size_in_bytes + filler_size;

  DCHECK(old_top + aligned_size_in_bytes <= high);
  UpdateInlineAllocationLimit(aligned_size_in_bytes);
  return true;
}

void NewSpace::MaybeFreeUnusedLab(LinearAllocationArea info) {
  if (info.limit() != kNullAddress && info.limit() == top()) {
    DCHECK_NE(info.top(), kNullAddress);
    allocation_info_.set_top(info.top());
    allocation_info_.MoveStartToTop();
    original_top_.store(info.top(), std::memory_order_release);
  }

#if DEBUG
  VerifyTop();
#endif
}

std::unique_ptr<ObjectIterator> NewSpace::GetObjectIterator(Heap* heap) {
  return std::unique_ptr<ObjectIterator>(new SemiSpaceObjectIterator(this));
}

AllocationResult NewSpace::AllocateRawSlow(int size_in_bytes,
                                           AllocationAlignment alignment,
                                           AllocationOrigin origin) {
#ifdef V8_HOST_ARCH_32_BIT
  return alignment != kWordAligned
             ? AllocateRawAligned(size_in_bytes, alignment, origin)
             : AllocateRawUnaligned(size_in_bytes, origin);
#else
#ifdef V8_COMPRESS_POINTERS
  // TODO(ishell, v8:8875): Consider using aligned allocations once the
  // allocation alignment inconsistency is fixed. For now we keep using
  // unaligned access since both x64 and arm64 architectures (where pointer
  // compression is supported) allow unaligned access to doubles and full words.
#endif  // V8_COMPRESS_POINTERS
  return AllocateRawUnaligned(size_in_bytes, origin);
#endif
}

AllocationResult NewSpace::AllocateRawUnaligned(int size_in_bytes,
                                                AllocationOrigin origin) {
  if (!EnsureAllocation(size_in_bytes, kWordAligned)) {
    return AllocationResult::Retry();
  }

  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

  AllocationResult result = AllocateFastUnaligned(size_in_bytes, origin);
  DCHECK(!result.IsRetry());

  InvokeAllocationObservers(result.ToAddress(), size_in_bytes, size_in_bytes,
                            size_in_bytes);

  return result;
}

AllocationResult NewSpace::AllocateRawAligned(int size_in_bytes,
                                              AllocationAlignment alignment,
                                              AllocationOrigin origin) {
  if (!EnsureAllocation(size_in_bytes, alignment)) {
    return AllocationResult::Retry();
  }

  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

  int aligned_size_in_bytes;

  AllocationResult result = AllocateFastAligned(
      size_in_bytes, &aligned_size_in_bytes, alignment, origin);
  DCHECK(!result.IsRetry());

  InvokeAllocationObservers(result.ToAddress(), size_in_bytes,
                            aligned_size_in_bytes, aligned_size_in_bytes);

  return result;
}

void NewSpace::VerifyTop() {
  // Ensure validity of LAB: start <= top <= limit
  DCHECK_LE(allocation_info_.start(), allocation_info_.top());
  DCHECK_LE(allocation_info_.top(), allocation_info_.limit());

  // Ensure that original_top_ always equals LAB start.
  DCHECK_EQ(original_top_, allocation_info_.start());

  // Ensure that limit() is <= original_limit_, original_limit_ always needs
  // to be end of curent to space page.
  DCHECK_LE(allocation_info_.limit(), original_limit_);
  DCHECK_EQ(original_limit_, to_space_.page_high());
}

#ifdef VERIFY_HEAP
// We do not use the SemiSpaceObjectIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void NewSpace::Verify(Isolate* isolate) {
  // The allocation pointer should be in the space or at the very end.
  DCHECK_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  // There should be objects packed in from the low address up to the
  // allocation pointer.
  Address current = to_space_.first_page()->area_start();
  CHECK_EQ(current, to_space_.space_start());

  size_t external_space_bytes[kNumTypes];
  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  while (current != top()) {
    if (!Page::IsAlignedToPageSize(current)) {
      // The allocation pointer should not be in the middle of an object.
      CHECK(!Page::FromAllocationAreaAddress(current)->ContainsLimit(top()) ||
            current < top());

      HeapObject object = HeapObject::FromAddress(current);

      // The first word should be a map, and we expect all map pointers to
      // be in map space or read-only space.
      Map map = object.map();
      CHECK(map.IsMap());
      CHECK(ReadOnlyHeap::Contains(map) || heap()->map_space()->Contains(map));

      // The object should not be code or a map.
      CHECK(!object.IsMap());
      CHECK(!object.IsAbstractCode());

      // The object itself should look OK.
      object.ObjectVerify(isolate);

      // All the interior pointers should be contained in the heap.
      VerifyPointersVisitor visitor(heap());
      int size = object.Size();
      object.IterateBody(map, size, &visitor);

      if (object.IsExternalString()) {
        ExternalString external_string = ExternalString::cast(object);
        size_t size = external_string.ExternalPayloadSize();
        external_space_bytes[ExternalBackingStoreType::kExternalString] += size;
      }

      current += size;
    } else {
      // At end of page, switch to next page.
      Page* page = Page::FromAllocationAreaAddress(current)->next_page();
      current = page->area_start();
    }
  }

  for (int i = 0; i < kNumTypes; i++) {
    if (i == ExternalBackingStoreType::kArrayBuffer) continue;
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }

    size_t bytes = heap()->array_buffer_sweeper()->young().BytesSlow();
    CHECK_EQ(bytes,
             ExternalBackingStoreBytes(ExternalBackingStoreType::kArrayBuffer));

  // Check semi-spaces.
  CHECK_EQ(from_space_.id(), kFromSpace);
  CHECK_EQ(to_space_.id(), kToSpace);
  from_space_.Verify();
  to_space_.Verify();
}
#endif

}  // namespace internal
}  // namespace v8
