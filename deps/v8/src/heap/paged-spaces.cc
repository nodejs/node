// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/paged-spaces.h"

#include <atomic>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/string.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// PagedSpaceObjectIterator

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpaceBase* space)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(space->first_page(), nullptr),
      current_page_(page_range_.begin())
#if V8_COMPRESS_POINTERS
      ,
      cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
{
  heap->MakeHeapIterable();
  USE(space_);
}

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpaceBase* space,
                                                   const Page* page)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(page),
      current_page_(page_range_.begin())
#if V8_COMPRESS_POINTERS
      ,
      cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
{
  heap->MakeHeapIterable();
}

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpace* space,
                                                   const Page* page,
                                                   Address start_address)
    : cur_addr_(start_address),
      cur_end_(page->area_end()),
      space_(space),
      page_range_(page, page),
      current_page_(page_range_.begin())
#if V8_COMPRESS_POINTERS
      ,
      cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
{
  heap->MakeHeapIterable();
  DCHECK_IMPLIES(!heap->IsInlineAllocationEnabled(),
                 !page->Contains(space->top()));
  DCHECK(page->Contains(start_address));
  DCHECK(page->SweepingDone());
}

// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool PagedSpaceObjectIterator::AdvanceToNextPage() {
  DCHECK_EQ(cur_addr_, cur_end_);
  if (current_page_ == page_range_.end()) return false;
  const Page* cur_page = *(current_page_++);

  cur_addr_ = cur_page->area_start();
  cur_end_ = cur_page->area_end();
  DCHECK(cur_page->SweepingDone());
  return true;
}

Page* PagedSpaceBase::InitializePage(MemoryChunk* chunk) {
  Page* page = static_cast<Page*>(chunk);
  DCHECK_EQ(
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(page->owner_identity()),
      page->area_size());
  // Make sure that categories are initialized before freeing the area.
  page->ResetAllocationStatistics();
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->AllocateFreeListCategories();
  page->InitializeFreeListCategories();
  page->list_node().Initialize();
  page->InitializationMemoryFence();
  return page;
}

PagedSpaceBase::PagedSpaceBase(
    Heap* heap, AllocationSpace space, Executability executable,
    FreeList* free_list, AllocationCounter& allocation_counter,
    LinearAllocationArea& allocation_info_,
    LinearAreaOriginalData& linear_area_original_data,
    CompactionSpaceKind compaction_space_kind)
    : SpaceWithLinearArea(heap, space, free_list, allocation_counter,
                          allocation_info_, linear_area_original_data),
      executable_(executable),
      compaction_space_kind_(compaction_space_kind) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

void PagedSpaceBase::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                     chunk);
  }
  accounting_stats_.Clear();
}

void PagedSpaceBase::MergeCompactionSpace(CompactionSpace* other) {
  base::MutexGuard guard(mutex());

  DCHECK_NE(NEW_SPACE, identity());
  DCHECK_NE(NEW_SPACE, other->identity());
  DCHECK(identity() == other->identity());

  // Unmerged fields:
  //   area_size_
  other->FreeLinearAllocationArea();

  for (int i = static_cast<int>(AllocationOrigin::kFirstAllocationOrigin);
       i <= static_cast<int>(AllocationOrigin::kLastAllocationOrigin); i++) {
    allocations_origins_[i] += other->allocations_origins_[i];
  }

  // The linear allocation area of {other} should be destroyed now.
  DCHECK_EQ(kNullAddress, other->top());
  DCHECK_EQ(kNullAddress, other->limit());

  // Move over pages.
  for (auto it = other->begin(); it != other->end();) {
    Page* p = *(it++);

    // Ensure that pages are initialized before objects on it are discovered by
    // concurrent markers.
    p->InitializationMemoryFence();

    // Relinking requires the category to be unlinked.
    other->RemovePage(p);
    AddPage(p);
    DCHECK_IMPLIES(
        !p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
        p->AvailableInFreeList() == p->AvailableInFreeListFromAllocatedBytes());

    // TODO(leszeks): Here we should allocation step, but:
    //   1. Allocation groups are currently not handled properly by the sampling
    //      allocation profiler, and
    //   2. Observers might try to take the space lock, which isn't reentrant.
    // We'll have to come up with a better solution for allocation stepping
    // before shipping, which will likely be using LocalHeap.
  }
  for (auto p : other->GetNewPages()) {
    heap()->NotifyOldGenerationExpansion(identity(), p);
  }

  DCHECK_EQ(0u, other->Size());
  DCHECK_EQ(0u, other->Capacity());
}

size_t PagedSpaceBase::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits()) {
    DCHECK_EQ(0, committed_physical_memory());
    return CommittedMemory();
  }
  CodePageHeaderModificationScope rwx_write_scope(
      "Updating high water mark for Code pages requires write access to "
      "the Code page headers");
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  return committed_physical_memory();
}

void PagedSpaceBase::IncrementCommittedPhysicalMemory(size_t increment_value) {
  if (!base::OS::HasLazyCommits() || increment_value == 0) return;
  size_t old_value = committed_physical_memory_.fetch_add(
      increment_value, std::memory_order_relaxed);
  USE(old_value);
  DCHECK_LT(old_value, old_value + increment_value);
}

void PagedSpaceBase::DecrementCommittedPhysicalMemory(size_t decrement_value) {
  if (!base::OS::HasLazyCommits() || decrement_value == 0) return;
  size_t old_value = committed_physical_memory_.fetch_sub(
      decrement_value, std::memory_order_relaxed);
  USE(old_value);
  DCHECK_GT(old_value, old_value - decrement_value);
}

#if DEBUG
void PagedSpaceBase::VerifyCommittedPhysicalMemory() const {
  heap()->safepoint()->AssertActive();
  size_t size = 0;
  for (const Page* page : *this) {
    DCHECK(page->SweepingDone());
    size += page->CommittedPhysicalMemory();
  }
  // Ensure that the space's counter matches the sum of all page counters.
  DCHECK_EQ(size, CommittedPhysicalMemory());
}
#endif  // DEBUG

bool PagedSpaceBase::ContainsSlow(Address addr) const {
  Page* p = Page::FromAddress(addr);
  for (const Page* page : *this) {
    if (page == p) return true;
  }
  return false;
}

void PagedSpaceBase::RefineAllocatedBytesAfterSweeping(Page* page) {
  CHECK(page->SweepingDone());
  auto marking_state = heap()->non_atomic_marking_state();
  // The live_byte on the page was accounted in the space allocated
  // bytes counter. After sweeping allocated_bytes() contains the
  // accurate live byte count on the page.
  size_t old_counter = marking_state->live_bytes(page);
  size_t new_counter = page->allocated_bytes();
  DCHECK_GE(old_counter, new_counter);
  if (old_counter > new_counter) {
    DecreaseAllocatedBytes(old_counter - new_counter, page);
  }
  marking_state->SetLiveBytes(page, 0);
}

Page* PagedSpaceBase::RemovePageSafe(int size_in_bytes) {
  base::MutexGuard guard(mutex());
  Page* page = free_list()->GetPageForSize(size_in_bytes);
  if (!page) return nullptr;
  RemovePage(page);
  return page;
}

size_t PagedSpaceBase::AddPage(Page* page) {
  DCHECK_NOT_NULL(page);
  CHECK(page->SweepingDone());
  page->set_owner(this);
  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));
  DCHECK_IMPLIES(identity() != NEW_SPACE, !page->IsFlagSet(Page::TO_PAGE));
  memory_chunk_list_.PushBack(page);
  AccountCommitted(page->size());
  IncreaseCapacity(page->area_size());
  IncreaseAllocatedBytes(page->allocated_bytes(), page);
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
  IncrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  return RelinkFreeListCategories(page);
}

void PagedSpaceBase::RemovePage(Page* page) {
  CHECK(page->SweepingDone());
  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));
  memory_chunk_list_.Remove(page);
  UnlinkFreeListCategories(page);
  if (identity() == NEW_SPACE) {
    page->ReleaseFreeListCategories();
  }
  // Pages are only removed from new space when they are promoted to old space
  // during a GC. This happens after sweeping as started and the allocation
  // counters have been reset.
  DCHECK_IMPLIES(identity() == NEW_SPACE,
                 heap()->gc_state() != Heap::NOT_IN_GC);
  if (identity() != NEW_SPACE) {
    DecreaseAllocatedBytes(page->allocated_bytes(), page);
  }
  DecreaseCapacity(page->area_size());
  AccountUncommitted(page->size());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
}

void PagedSpaceBase::SetTopAndLimit(Address top, Address limit, Address end) {
  DCHECK_GE(end, limit);
  DCHECK(top == limit ||
         Page::FromAddress(top) == Page::FromAddress(limit - 1));
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(top, limit);

  base::Optional<base::SharedMutexGuard<base::kExclusive>> optional_guard;
  if (!is_compaction_space()) optional_guard.emplace(linear_area_lock());
  linear_area_original_data_.set_original_limit_relaxed(end);
  linear_area_original_data_.set_original_top_release(top);
}

void PagedSpaceBase::SetLimit(Address limit) {
  DCHECK(SupportsExtendingLAB());
  DCHECK_LE(limit, original_limit_relaxed());
  allocation_info_.SetLimit(limit);
}

size_t PagedSpaceBase::ShrinkPageToHighWaterMark(Page* page) {
  size_t unused = page->ShrinkToHighWaterMark();
  accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
  AccountUncommitted(unused);
  return unused;
}

void PagedSpaceBase::ResetFreeList() {
  for (Page* page : *this) {
    free_list_->EvictFreeListItems(page);
  }
  DCHECK(free_list_->IsEmpty());
  DCHECK_EQ(0, free_list_->Available());
}

void PagedSpaceBase::ShrinkImmortalImmovablePages() {
  DCHECK(!heap()->deserialization_complete());
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  FreeLinearAllocationArea();
  ResetFreeList();
  for (Page* page : *this) {
    DCHECK(page->IsFlagSet(Page::NEVER_EVACUATE));
    ShrinkPageToHighWaterMark(page);
  }
}

Page* PagedSpaceBase::TryExpandImpl() {
  Page* page = heap()->memory_allocator()->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, this, executable());
  if (page == nullptr) return nullptr;
  ConcurrentAllocationMutex guard(this);
  AddPage(page);
  Free(page->area_start(), page->area_size(),
       SpaceAccountingMode::kSpaceAccounted);
  return page;
}

base::Optional<std::pair<Address, size_t>> PagedSpaceBase::TryExpandBackground(
    size_t size_in_bytes) {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = heap()->memory_allocator()->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, this, executable());
  if (page == nullptr) return {};
  base::MutexGuard lock(&space_mutex_);
  AddPage(page);
  if (identity() == CODE_SPACE || identity() == CODE_LO_SPACE) {
    heap()->isolate()->AddCodeMemoryChunk(page);
  }
  Address object_start = page->area_start();
  CHECK_LE(size_in_bytes, page->area_size());
  Free(page->area_start() + size_in_bytes, page->area_size() - size_in_bytes,
       SpaceAccountingMode::kSpaceAccounted);
  AddRangeToActiveSystemPages(page, object_start, object_start + size_in_bytes);
  return std::make_pair(object_start, size_in_bytes);
}

int PagedSpaceBase::CountTotalPages() const {
  int count = 0;
  for (const Page* page : *this) {
    count++;
    USE(page);
  }
  return count;
}

void PagedSpaceBase::SetLinearAllocationArea(Address top, Address limit,
                                             Address end) {
  SetTopAndLimit(top, limit, end);
  if (top != kNullAddress && top != limit) {
    Page* page = Page::FromAllocationAreaAddress(top);
    if (identity() == NEW_SPACE) {
      page->MarkWasUsedForAllocation();
    } else if (heap()->incremental_marking()->black_allocation()) {
      DCHECK_NE(NEW_SPACE, identity());
      page->CreateBlackArea(top, limit);
    }
  }
}

void PagedSpaceBase::DecreaseLimit(Address new_limit) {
  Address old_limit = limit();
  DCHECK_LE(top(), new_limit);
  DCHECK_GE(old_limit, new_limit);
  if (new_limit != old_limit) {
    base::Optional<CodePageMemoryModificationScope> optional_scope;

    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(new_limit);
      optional_scope.emplace(chunk);
    }

    ConcurrentAllocationMutex guard(this);
    Address old_max_limit = original_limit_relaxed();
    if (!SupportsExtendingLAB()) {
      DCHECK_EQ(old_max_limit, old_limit);
      SetTopAndLimit(top(), new_limit, new_limit);
      Free(new_limit, old_max_limit - new_limit,
           SpaceAccountingMode::kSpaceAccounted);
    } else {
      SetLimit(new_limit);
      heap()->CreateFillerObjectAt(new_limit,
                                   static_cast<int>(old_max_limit - new_limit));
    }
    if (heap()->incremental_marking()->black_allocation() &&
        identity() != NEW_SPACE) {
      Page::FromAllocationAreaAddress(new_limit)->DestroyBlackArea(new_limit,
                                                                   old_limit);
    }
  }
}

void PagedSpaceBase::MarkLinearAllocationAreaBlack() {
  DCHECK(heap()->incremental_marking()->black_allocation());
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->CreateBlackArea(current_top, current_limit);
  }
}

void PagedSpaceBase::UnmarkLinearAllocationArea() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }
}

void PagedSpaceBase::MakeLinearAllocationAreaIterable() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    base::Optional<CodePageMemoryModificationScope> optional_scope;

    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(current_top);
      optional_scope.emplace(chunk);
    }

    heap_->CreateFillerObjectAt(current_top,
                                static_cast<int>(current_limit - current_top));
  }
}

size_t PagedSpaceBase::Available() const {
  ConcurrentAllocationMutex guard(this);
  return free_list_->Available();
}

namespace {

UnprotectMemoryOrigin GetUnprotectMemoryOrigin(bool is_compaction_space) {
  return is_compaction_space ? UnprotectMemoryOrigin::kMaybeOffMainThread
                             : UnprotectMemoryOrigin::kMainThread;
}

}  // namespace

void PagedSpaceBase::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  Address current_top = top();
  Address current_limit = limit();
  if (current_top == kNullAddress) {
    DCHECK_EQ(kNullAddress, current_limit);
    return;
  }
  Address current_max_limit = original_limit_relaxed();
  DCHECK_IMPLIES(!SupportsExtendingLAB(), current_max_limit == current_limit);

  AdvanceAllocationObservers();

  base::Optional<CodePageMemoryModificationScope> optional_scope;

  if (identity() == CODE_SPACE) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(allocation_info_.top());
    optional_scope.emplace(chunk);
  }

  if (identity() != NEW_SPACE && current_top != current_limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }

  SetTopAndLimit(kNullAddress, kNullAddress, kNullAddress);
  DCHECK_GE(current_limit, current_top);

  // The code page of the linear allocation area needs to be unprotected
  // because we are going to write a filler into that memory area below.
  if (identity() == CODE_SPACE) {
    heap()->UnprotectAndRegisterMemoryChunk(
        MemoryChunk::FromAddress(current_top),
        GetUnprotectMemoryOrigin(is_compaction_space()));
  }

  DCHECK_IMPLIES(current_limit - current_top >= 2 * kTaggedSize,
                 heap()->marking_state()->IsUnmarked(
                     HeapObject::FromAddress(current_top)));
  Free(current_top, current_max_limit - current_top,
       SpaceAccountingMode::kSpaceAccounted);
}

void PagedSpaceBase::ReleasePage(Page* page) {
  DCHECK(page->SweepingDone());
  DCHECK_EQ(0, heap()->non_atomic_marking_state()->live_bytes(page));
  DCHECK_EQ(page->owner(), this);

  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));

  memory_chunk_list().Remove(page);

  free_list_->EvictFreeListItems(page);

  if (Page::FromAllocationAreaAddress(allocation_info_.top()) == page) {
    SetTopAndLimit(kNullAddress, kNullAddress, kNullAddress);
  }

  if (identity() == CODE_SPACE) {
    heap()->isolate()->RemoveCodeMemoryChunk(page);
  }

  AccountUncommitted(page->size());
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  accounting_stats_.DecreaseCapacity(page->area_size());
  heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kConcurrently,
                                   page);
}

void PagedSpaceBase::SetReadable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadable();
  }
}

void PagedSpaceBase::SetReadAndExecutable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndExecutable();
  }
}

void PagedSpaceBase::SetCodeModificationPermissions() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetCodeModificationPermissions();
  }
}

std::unique_ptr<ObjectIterator> PagedSpaceBase::GetObjectIterator(Heap* heap) {
  return std::unique_ptr<ObjectIterator>(
      new PagedSpaceObjectIterator(heap, this));
}

bool PagedSpaceBase::TryAllocationFromFreeListMain(size_t size_in_bytes,
                                                   AllocationOrigin origin) {
  ConcurrentAllocationMutex guard(this);
  DCHECK(IsAligned(size_in_bytes, kTaggedSize));
  DCHECK_LE(top(), limit());
#ifdef DEBUG
  if (top() != limit()) {
    DCHECK_EQ(Page::FromAddress(top()), Page::FromAddress(limit() - 1));
  }
#endif
  // Don't free list allocate if there is linear space available.
  DCHECK_LT(static_cast<size_t>(limit() - top()), size_in_bytes);

  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  FreeLinearAllocationArea();

  size_t new_node_size = 0;
  FreeSpace new_node =
      free_list_->Allocate(size_in_bytes, &new_node_size, origin);
  if (new_node.is_null()) return false;
  DCHECK_GE(new_node_size, size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  IncreaseAllocatedBytes(new_node_size, page);

  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());
  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = ComputeLimit(start, end, size_in_bytes);
  DCHECK_LE(limit, end);
  DCHECK_LE(size_in_bytes, limit - start);
  if (limit != end) {
    if (identity() == CODE_SPACE) {
      heap()->UnprotectAndRegisterMemoryChunk(
          page, GetUnprotectMemoryOrigin(is_compaction_space()));
    }
    if (!SupportsExtendingLAB()) {
      Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
      end = limit;
    } else {
      DCHECK(heap()->IsMainThread());
      heap()->CreateFillerObjectAt(limit, static_cast<int>(end - limit));
    }
  }
  SetLinearAllocationArea(start, limit, end);
  AddRangeToActiveSystemPages(page, start, limit);

  return true;
}

base::Optional<std::pair<Address, size_t>>
PagedSpaceBase::TryAllocationFromFreeListBackground(size_t min_size_in_bytes,
                                                    size_t max_size_in_bytes,
                                                    AllocationOrigin origin) {
  base::MutexGuard lock(&space_mutex_);
  DCHECK_LE(min_size_in_bytes, max_size_in_bytes);
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == SHARED_SPACE);

  size_t new_node_size = 0;
  FreeSpace new_node =
      free_list_->Allocate(min_size_in_bytes, &new_node_size, origin);
  if (new_node.is_null()) return {};
  DCHECK_GE(new_node_size, min_size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  IncreaseAllocatedBytes(new_node_size, page);

  size_t used_size_in_bytes = std::min(new_node_size, max_size_in_bytes);

  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = new_node.address() + used_size_in_bytes;
  DCHECK_LE(limit, end);
  DCHECK_LE(min_size_in_bytes, limit - start);
  if (limit != end) {
    if (identity() == CODE_SPACE) {
      heap()->UnprotectAndRegisterMemoryChunk(
          page, UnprotectMemoryOrigin::kMaybeOffMainThread);
    }
    Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }
  AddRangeToActiveSystemPages(page, start, limit);

  return std::make_pair(start, used_size_in_bytes);
}

#ifdef DEBUG
void PagedSpaceBase::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpaceBase::Verify(Isolate* isolate,
                            SpaceVerificationVisitor* visitor) const {
  bool allocation_pointer_found_in_space =
      (allocation_info_.top() == allocation_info_.limit());
  size_t external_space_bytes[kNumTypes];
  size_t external_page_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  PtrComprCageBase cage_base(isolate);
  for (const Page* page : *this) {
    CHECK_EQ(page->owner(), this);
    CHECK_IMPLIES(identity() != NEW_SPACE, !page->WasUsedForAllocation());
    visitor->VerifyPage(page);

    for (int i = 0; i < kNumTypes; i++) {
      external_page_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
    }

    if (page == Page::FromAllocationAreaAddress(allocation_info_.top())) {
      allocation_pointer_found_in_space = true;
    }
    CHECK(page->SweepingDone());
    PagedSpaceObjectIterator it(isolate->heap(), this, page);
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();

    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      CHECK(end_of_previous_object <= object.address());

      // Invoke verification method for each object.
      visitor->VerifyObject(object);

      // All the interior pointers should be contained in the heap.
      int size = object.Size(cage_base);
      CHECK(object.address() + size <= top);
      end_of_previous_object = object.address() + size;

      if (object.IsExternalString(cage_base)) {
        ExternalString external_string = ExternalString::cast(object);
        size_t payload_size = external_string.ExternalPayloadSize();
        external_page_bytes[ExternalBackingStoreType::kExternalString] +=
            payload_size;
      }
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      CHECK_EQ(external_page_bytes[t], page->ExternalBackingStoreBytes(t));
      external_space_bytes[t] += external_page_bytes[t];
    }

    visitor->VerifyPageDone(page);
  }
  for (int i = 0; i < kNumTypes; i++) {
    if (i == ExternalBackingStoreType::kArrayBuffer) continue;
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }
  CHECK(allocation_pointer_found_in_space);

  if (!v8_flags.concurrent_array_buffer_sweeping) {
    if (identity() == OLD_SPACE) {
      size_t bytes = heap()->array_buffer_sweeper()->old().BytesSlow();
      CHECK_EQ(bytes, ExternalBackingStoreBytes(
                          ExternalBackingStoreType::kArrayBuffer));
    } else if (identity() == NEW_SPACE) {
      DCHECK(v8_flags.minor_mc);
      size_t bytes = heap()->array_buffer_sweeper()->young().BytesSlow();
      CHECK_EQ(bytes, ExternalBackingStoreBytes(
                          ExternalBackingStoreType::kArrayBuffer));
    }
  }

#ifdef DEBUG
  VerifyCountersAfterSweeping(isolate->heap());
#endif
}

void PagedSpaceBase::VerifyLiveBytes() const {
  MarkingState* marking_state = heap()->marking_state();
  PtrComprCageBase cage_base(heap()->isolate());
  for (const Page* page : *this) {
    CHECK(page->SweepingDone());
    PagedSpaceObjectIterator it(heap(), this, page);
    int black_size = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      // All the interior pointers should be contained in the heap.
      if (marking_state->IsMarked(object)) {
        black_size += object.Size(cage_base);
      }
    }
    CHECK_LE(black_size, marking_state->live_bytes(page));
  }
}
#endif  // VERIFY_HEAP

#ifdef DEBUG
void PagedSpaceBase::VerifyCountersAfterSweeping(Heap* heap) const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  PtrComprCageBase cage_base(heap->isolate());
  for (const Page* page : *this) {
    DCHECK(page->SweepingDone());
    total_capacity += page->area_size();
    PagedSpaceObjectIterator it(heap, this, page);
    size_t real_allocated = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      if (!object.IsFreeSpaceOrFiller()) {
        real_allocated += ALIGN_TO_ALLOCATION_ALIGNMENT(object.Size(cage_base));
      }
    }
    total_allocated += page->allocated_bytes();
    // The real size can be smaller than the accounted size if array trimming,
    // object slack tracking happened after sweeping.
    DCHECK_LE(real_allocated, accounting_stats_.AllocatedOnPage(page));
    DCHECK_EQ(page->allocated_bytes(), accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}

void PagedSpaceBase::VerifyCountersBeforeConcurrentSweeping() const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  auto marking_state = heap()->non_atomic_marking_state();
  for (const Page* page : *this) {
    size_t page_allocated =
        page->SweepingDone()
            ? page->allocated_bytes()
            : static_cast<size_t>(marking_state->live_bytes(page));
    total_capacity += page->area_size();
    total_allocated += page_allocated;
    DCHECK_EQ(page_allocated, accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}
#endif

void PagedSpaceBase::UpdateInlineAllocationLimit() {
  // Ensure there are no unaccounted allocations.
  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

  Address new_limit = ComputeLimit(top(), limit(), 0);
  DCHECK_LE(top(), new_limit);
  DCHECK_LE(new_limit, limit());
  DecreaseLimit(new_limit);
}

// -----------------------------------------------------------------------------
// OldSpace implementation

bool PagedSpaceBase::RefillLabMain(int size_in_bytes, AllocationOrigin origin) {
  VMState<GC> state(heap()->isolate());
  RCS_SCOPE(heap()->isolate(),
            RuntimeCallCounterId::kGC_Custom_SlowAllocateRaw);
  return RawRefillLabMain(size_in_bytes, origin);
}

Page* CompactionSpace::TryExpandImpl() {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = PagedSpaceBase::TryExpandImpl();
  new_pages_.push_back(page);
  return page;
}

bool CompactionSpace::RefillLabMain(int size_in_bytes,
                                    AllocationOrigin origin) {
  return RawRefillLabMain(size_in_bytes, origin);
}

bool PagedSpaceBase::TryExpand(int size_in_bytes, AllocationOrigin origin) {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = TryExpandImpl();
  if (!page) return false;
  if (!is_compaction_space() && identity() != NEW_SPACE) {
    heap()->NotifyOldGenerationExpansion(identity(), page);
  }
  return TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                       origin);
}

bool PagedSpaceBase::TryExtendLAB(int size_in_bytes) {
  Address current_top = top();
  if (current_top == kNullAddress) return false;
  Address current_limit = limit();
  Address max_limit = original_limit_relaxed();
  if (current_top + size_in_bytes > max_limit) {
    return false;
  }
  DCHECK(SupportsExtendingLAB());
  AdvanceAllocationObservers();
  Address new_limit = ComputeLimit(current_top, max_limit, size_in_bytes);
  SetLimit(new_limit);
  DCHECK(heap()->IsMainThread());
  heap()->CreateFillerObjectAt(new_limit,
                               static_cast<int>(max_limit - new_limit));
  Page* page = Page::FromAddress(current_top);
  // No need to create a black allocation area since new space doesn't use
  // black allocation.
  DCHECK_EQ(NEW_SPACE, identity());
  AddRangeToActiveSystemPages(page, current_limit, new_limit);
  return true;
}

bool PagedSpaceBase::RawRefillLabMain(int size_in_bytes,
                                      AllocationOrigin origin) {
  // Allocation in this space has failed.
  DCHECK_GE(size_in_bytes, 0);

  if (TryExtendLAB(size_in_bytes)) return true;

  static constexpr int kMaxPagesToSweep = 1;

  if (TryAllocationFromFreeListMain(size_in_bytes, origin)) return true;

  const bool is_main_thread =
      heap()->IsMainThread() || heap()->IsSharedMainThread();
  const auto sweeping_scope_kind =
      is_main_thread ? ThreadKind::kMain : ThreadKind::kBackground;
  const auto sweeping_scope_id =
      heap()->sweeper()->GetTracingScope(identity(), is_main_thread);
  // Sweeping is still in progress.
  if (heap()->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    if (heap()->sweeper()->ShouldRefillFreelistForSpace(identity())) {
      {
        TRACE_GC_EPOCH(heap()->tracer(), sweeping_scope_id,
                       sweeping_scope_kind);
        RefillFreeList();
      }

      // Retry the free list allocation.
      if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                        origin))
        return true;
    }

    if (ContributeToSweepingMain(size_in_bytes, kMaxPagesToSweep, size_in_bytes,
                                 origin, sweeping_scope_id,
                                 sweeping_scope_kind))
      return true;
  }

  if (is_compaction_space()) {
    DCHECK_NE(NEW_SPACE, identity());
    // The main thread may have acquired all swept pages. Try to steal from
    // it. This can only happen during young generation evacuation.
    PagedSpaceBase* main_space = heap()->paged_space(identity());
    Page* page = main_space->RemovePageSafe(size_in_bytes);
    if (page != nullptr) {
      AddPage(page);
      if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                        origin))
        return true;
    }
  }

  if (identity() != NEW_SPACE &&
      heap()->ShouldExpandOldGenerationOnSlowAllocation(
          heap()->main_thread_local_heap(), origin) &&
      heap()->CanExpandOldGeneration(AreaSize())) {
    if (TryExpand(size_in_bytes, origin)) {
      return true;
    }
  }

  // Try sweeping all pages.
  if (ContributeToSweepingMain(0, 0, size_in_bytes, origin, sweeping_scope_id,
                               sweeping_scope_kind))
    return true;

  if (identity() != NEW_SPACE && heap()->gc_state() != Heap::NOT_IN_GC &&
      !heap()->force_oom()) {
    // Avoid OOM crash in the GC in order to invoke NearHeapLimitCallback after
    // GC and give it a chance to increase the heap limit.
    return TryExpand(size_in_bytes, origin);
  }
  return false;
}

bool PagedSpaceBase::ContributeToSweepingMain(
    int required_freed_bytes, int max_pages, int size_in_bytes,
    AllocationOrigin origin, GCTracer::Scope::ScopeId sweeping_scope_id,
    ThreadKind sweeping_scope_kind) {
  if (!heap()->sweeping_in_progress()) return false;
  if (!heap()->sweeper()->AreSweeperTasksRunning() &&
      heap()->sweeper()->IsSweepingDoneForSpace(identity()))
    return false;

  TRACE_GC_EPOCH(heap()->tracer(), sweeping_scope_id, sweeping_scope_kind);
  // Cleanup invalidated old-to-new refs for compaction space in the
  // final atomic pause.
  Sweeper::SweepingMode sweeping_mode =
      is_compaction_space() ? Sweeper::SweepingMode::kEagerDuringGC
                            : Sweeper::SweepingMode::kLazyOrConcurrent;

  heap()->sweeper()->ParallelSweepSpace(identity(), sweeping_mode,
                                        required_freed_bytes, max_pages);
  RefillFreeList();
  return TryAllocationFromFreeListMain(size_in_bytes, origin);
}

void PagedSpaceBase::AddRangeToActiveSystemPages(Page* page, Address start,
                                                 Address end) {
  DCHECK_LE(page->address(), start);
  DCHECK_LT(start, end);
  DCHECK_LE(end, page->address() + Page::kPageSize);

  const size_t added_pages = page->active_system_pages()->Add(
      start - page->address(), end - page->address(),
      MemoryAllocator::GetCommitPageSizeBits());

  IncrementCommittedPhysicalMemory(added_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void PagedSpaceBase::ReduceActiveSystemPages(
    Page* page, ActiveSystemPages active_system_pages) {
  const size_t reduced_pages =
      page->active_system_pages()->Reduce(active_system_pages);
  DecrementCommittedPhysicalMemory(reduced_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void PagedSpaceBase::UnlinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  page->ForAllFreeListCategories([this](FreeListCategory* category) {
    free_list()->RemoveCategory(category);
  });
}

size_t PagedSpaceBase::RelinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  size_t added = 0;
  page->ForAllFreeListCategories([this, &added](FreeListCategory* category) {
    added += category->available();
    category->Relink(free_list());
  });

  DCHECK_IMPLIES(!page->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
                 page->AvailableInFreeList() ==
                     page->AvailableInFreeListFromAllocatedBytes());
  return added;
}

void PagedSpace::RefillFreeList() {
  // Any PagedSpace might invoke RefillFreeList.
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == SHARED_SPACE);

  Sweeper* sweeper = heap()->sweeper();

  size_t added = 0;

  Page* p = nullptr;
  while ((p = sweeper->GetSweptPageSafe(this)) != nullptr) {
    // We regularly sweep NEVER_ALLOCATE_ON_PAGE pages. We drop the freelist
    // entries here to make them unavailable for allocations.
    if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
      p->ForAllFreeListCategories(
          [this](FreeListCategory* category) { category->Reset(free_list()); });
    }

    // Only during compaction pages can actually change ownership. This is
    // safe because there exists no other competing action on the page links
    // during compaction.
    if (is_compaction_space()) {
      DCHECK_NE(this, p->owner());
      DCHECK_NE(NEW_SPACE, identity());
      PagedSpace* owner = reinterpret_cast<PagedSpace*>(p->owner());
      base::MutexGuard guard(owner->mutex());
      owner->RefineAllocatedBytesAfterSweeping(p);
      owner->RemovePage(p);
      added += AddPage(p);
      added += p->wasted_memory();
    } else {
      base::MutexGuard guard(mutex());
      DCHECK_EQ(this, p->owner());
      RefineAllocatedBytesAfterSweeping(p);
      added += RelinkFreeListCategories(p);
      added += p->wasted_memory();
    }
    if (is_compaction_space() && (added > kCompactionMemoryWanted)) break;
  }
}

}  // namespace internal
}  // namespace v8
