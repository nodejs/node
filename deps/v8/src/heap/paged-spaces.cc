// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/paged-spaces.h"

#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/string.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// PagedSpaceObjectIterator

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   PagedSpace* space)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(space->first_page(), nullptr),
      current_page_(page_range_.begin()) {
  space_->MakeLinearAllocationAreaIterable();
  heap->mark_compact_collector()->EnsureSweepingCompleted();
}

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   PagedSpace* space,
                                                   Page* page)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(page),
      current_page_(page_range_.begin()) {
  space_->MakeLinearAllocationAreaIterable();
  heap->mark_compact_collector()->EnsureSweepingCompleted();
#ifdef DEBUG
  AllocationSpace owner = page->owner_identity();
  DCHECK(owner == OLD_SPACE || owner == MAP_SPACE || owner == CODE_SPACE);
#endif  // DEBUG
}

// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool PagedSpaceObjectIterator::AdvanceToNextPage() {
  DCHECK_EQ(cur_addr_, cur_end_);
  if (current_page_ == page_range_.end()) return false;
  Page* cur_page = *(current_page_++);

  cur_addr_ = cur_page->area_start();
  cur_end_ = cur_page->area_end();
  DCHECK(cur_page->SweepingDone());
  return true;
}
Page* PagedSpace::InitializePage(MemoryChunk* chunk) {
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

PagedSpace::PagedSpace(Heap* heap, AllocationSpace space,
                       Executability executable, FreeList* free_list,
                       CompactionSpaceKind compaction_space_kind)
    : SpaceWithLinearArea(heap, space, free_list),
      executable_(executable),
      compaction_space_kind_(compaction_space_kind) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

void PagedSpace::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free<MemoryAllocator::kFull>(chunk);
  }
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
  size_t added = 0;

  {
    Page* p = nullptr;
    while ((p = collector->sweeper()->GetSweptPageSafe(this)) != nullptr) {
      // We regularly sweep NEVER_ALLOCATE_ON_PAGE pages. We drop the freelist
      // entries here to make them unavailable for allocations.
      if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
        p->ForAllFreeListCategories([this](FreeListCategory* category) {
          category->Reset(free_list());
        });
      }

      // Also merge old-to-new remembered sets if not scavenging because of
      // data races: One thread might iterate remembered set, while another
      // thread merges them.
      if (compaction_space_kind() !=
          CompactionSpaceKind::kCompactionSpaceForScavenge) {
        p->MergeOldToNewRememberedSets();
      }

      // Only during compaction pages can actually change ownership. This is
      // safe because there exists no other competing action on the page links
      // during compaction.
      if (is_compaction_space()) {
        DCHECK_NE(this, p->owner());
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
}

void PagedSpace::MergeCompactionSpace(CompactionSpace* other) {
  base::MutexGuard guard(mutex());

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

      p->MergeOldToNewRememberedSets();

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

size_t PagedSpace::CommittedPhysicalMemory() {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  base::MutexGuard guard(mutex());
  size_t size = 0;
  for (Page* page : *this) {
    size += page->CommittedPhysicalMemory();
  }
  return size;
}

bool PagedSpace::ContainsSlow(Address addr) const {
  Page* p = Page::FromAddress(addr);
  for (const Page* page : *this) {
    if (page == p) return true;
  }
  return false;
}

void PagedSpace::RefineAllocatedBytesAfterSweeping(Page* page) {
  CHECK(page->SweepingDone());
  auto marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
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

Page* PagedSpace::RemovePageSafe(int size_in_bytes) {
  base::MutexGuard guard(mutex());
  Page* page = free_list()->GetPageForSize(size_in_bytes);
  if (!page) return nullptr;
  RemovePage(page);
  return page;
}

size_t PagedSpace::AddPage(Page* page) {
  CHECK(page->SweepingDone());
  page->set_owner(this);
  memory_chunk_list_.PushBack(page);
  AccountCommitted(page->size());
  IncreaseCapacity(page->area_size());
  IncreaseAllocatedBytes(page->allocated_bytes(), page);
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
  return RelinkFreeListCategories(page);
}

void PagedSpace::RemovePage(Page* page) {
  CHECK(page->SweepingDone());
  memory_chunk_list_.Remove(page);
  UnlinkFreeListCategories(page);
  DecreaseAllocatedBytes(page->allocated_bytes(), page);
  DecreaseCapacity(page->area_size());
  AccountUncommitted(page->size());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
}

void PagedSpace::SetTopAndLimit(Address top, Address limit) {
  DCHECK(top == limit ||
         Page::FromAddress(top) == Page::FromAddress(limit - 1));
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(top, limit);

  base::Optional<base::SharedMutexGuard<base::kExclusive>> optional_guard;
  if (!is_compaction_space())
    optional_guard.emplace(&pending_allocation_mutex_);
  original_limit_ = limit;
  original_top_ = top;
}

size_t PagedSpace::ShrinkPageToHighWaterMark(Page* page) {
  size_t unused = page->ShrinkToHighWaterMark();
  accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
  AccountUncommitted(unused);
  return unused;
}

void PagedSpace::ResetFreeList() {
  for (Page* page : *this) {
    free_list_->EvictFreeListItems(page);
  }
  DCHECK(free_list_->IsEmpty());
}

void PagedSpace::ShrinkImmortalImmovablePages() {
  DCHECK(!heap()->deserialization_complete());
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  FreeLinearAllocationArea();
  ResetFreeList();
  for (Page* page : *this) {
    DCHECK(page->IsFlagSet(Page::NEVER_EVACUATE));
    ShrinkPageToHighWaterMark(page);
  }
}

Page* PagedSpace::AllocatePage() {
  return heap()->memory_allocator()->AllocatePage(AreaSize(), this,
                                                  executable());
}

Page* PagedSpace::Expand() {
  Page* page = AllocatePage();
  if (page == nullptr) return nullptr;
  ConcurrentAllocationMutex guard(this);
  AddPage(page);
  Free(page->area_start(), page->area_size(),
       SpaceAccountingMode::kSpaceAccounted);
  return page;
}

base::Optional<std::pair<Address, size_t>> PagedSpace::ExpandBackground(
    LocalHeap* local_heap, size_t size_in_bytes) {
  Page* page = AllocatePage();
  if (page == nullptr) return {};
  base::MutexGuard lock(&space_mutex_);
  AddPage(page);
  Address object_start = page->area_start();
  CHECK_LE(size_in_bytes, page->area_size());
  Free(page->area_start() + size_in_bytes, page->area_size() - size_in_bytes,
       SpaceAccountingMode::kSpaceAccounted);
  return std::make_pair(object_start, size_in_bytes);
}

int PagedSpace::CountTotalPages() {
  int count = 0;
  for (Page* page : *this) {
    count++;
    USE(page);
  }
  return count;
}

void PagedSpace::SetLinearAllocationArea(Address top, Address limit) {
  SetTopAndLimit(top, limit);
  if (top != kNullAddress && top != limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAllocationAreaAddress(top)->CreateBlackArea(top, limit);
  }
}

void PagedSpace::DecreaseLimit(Address new_limit) {
  Address old_limit = limit();
  DCHECK_LE(top(), new_limit);
  DCHECK_GE(old_limit, new_limit);
  if (new_limit != old_limit) {
    base::Optional<CodePageMemoryModificationScope> optional_scope;

    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(new_limit);
      optional_scope.emplace(chunk);
    }

    SetTopAndLimit(top(), new_limit);
    Free(new_limit, old_limit - new_limit,
         SpaceAccountingMode::kSpaceAccounted);
    if (heap()->incremental_marking()->black_allocation()) {
      Page::FromAllocationAreaAddress(new_limit)->DestroyBlackArea(new_limit,
                                                                   old_limit);
    }
  }
}

void PagedSpace::MarkLinearAllocationAreaBlack() {
  DCHECK(heap()->incremental_marking()->black_allocation());
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->CreateBlackArea(current_top, current_limit);
  }
}

void PagedSpace::UnmarkLinearAllocationArea() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }
}

void PagedSpace::MakeLinearAllocationAreaIterable() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    base::Optional<CodePageMemoryModificationScope> optional_scope;

    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(current_top);
      optional_scope.emplace(chunk);
    }

    heap_->CreateFillerObjectAt(current_top,
                                static_cast<int>(current_limit - current_top),
                                ClearRecordedSlots::kNo);
  }
}

size_t PagedSpace::Available() {
  ConcurrentAllocationMutex guard(this);
  return free_list_->Available();
}

namespace {

UnprotectMemoryOrigin GetUnprotectMemoryOrigin(bool is_compaction_space) {
  return is_compaction_space ? UnprotectMemoryOrigin::kMaybeOffMainThread
                             : UnprotectMemoryOrigin::kMainThread;
}

}  // namespace

void PagedSpace::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  Address current_top = top();
  Address current_limit = limit();
  if (current_top == kNullAddress) {
    DCHECK_EQ(kNullAddress, current_limit);
    return;
  }

  AdvanceAllocationObservers();

  if (current_top != current_limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }

  SetTopAndLimit(kNullAddress, kNullAddress);
  DCHECK_GE(current_limit, current_top);

  // The code page of the linear allocation area needs to be unprotected
  // because we are going to write a filler into that memory area below.
  if (identity() == CODE_SPACE) {
    heap()->UnprotectAndRegisterMemoryChunk(
        MemoryChunk::FromAddress(current_top),
        GetUnprotectMemoryOrigin(is_compaction_space()));
  }

  DCHECK_IMPLIES(current_limit - current_top >= 2 * kTaggedSize,
                 heap()->incremental_marking()->marking_state()->IsWhite(
                     HeapObject::FromAddress(current_top)));
  Free(current_top, current_limit - current_top,
       SpaceAccountingMode::kSpaceAccounted);
}

void PagedSpace::ReleasePage(Page* page) {
  DCHECK_EQ(
      0, heap()->incremental_marking()->non_atomic_marking_state()->live_bytes(
             page));
  DCHECK_EQ(page->owner(), this);

  free_list_->EvictFreeListItems(page);

  if (Page::FromAllocationAreaAddress(allocation_info_.top()) == page) {
    SetTopAndLimit(kNullAddress, kNullAddress);
  }

  if (identity() == CODE_SPACE) {
    heap()->isolate()->RemoveCodeMemoryChunk(page);
  }

  AccountUncommitted(page->size());
  accounting_stats_.DecreaseCapacity(page->area_size());
  heap()->memory_allocator()->Free<MemoryAllocator::kPreFreeAndQueue>(page);
}

void PagedSpace::SetReadable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    CHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadable();
  }
}

void PagedSpace::SetReadAndExecutable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    CHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndExecutable();
  }
}

void PagedSpace::SetReadAndWritable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    CHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndWritable();
  }
}

std::unique_ptr<ObjectIterator> PagedSpace::GetObjectIterator(Heap* heap) {
  return std::unique_ptr<ObjectIterator>(
      new PagedSpaceObjectIterator(heap, this));
}

bool PagedSpace::TryAllocationFromFreeListMain(size_t size_in_bytes,
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
    Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }
  SetLinearAllocationArea(start, limit);

  return true;
}

base::Optional<std::pair<Address, size_t>> PagedSpace::RawRefillLabBackground(
    LocalHeap* local_heap, size_t min_size_in_bytes, size_t max_size_in_bytes,
    AllocationAlignment alignment, AllocationOrigin origin) {
  DCHECK(!is_compaction_space());
  DCHECK(identity() == OLD_SPACE || identity() == MAP_SPACE);
  DCHECK_EQ(origin, AllocationOrigin::kRuntime);

  auto result = TryAllocationFromFreeListBackground(
      local_heap, min_size_in_bytes, max_size_in_bytes, alignment, origin);
  if (result) return result;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList();

    // Retry the free list allocation.
    auto result = TryAllocationFromFreeListBackground(
        local_heap, min_size_in_bytes, max_size_in_bytes, alignment, origin);
    if (result) return result;

    // Now contribute to sweeping from background thread and then try to
    // reallocate.
    Sweeper::FreeSpaceMayContainInvalidatedSlots
        invalidated_slots_in_free_space =
            Sweeper::FreeSpaceMayContainInvalidatedSlots::kNo;

    const int kMaxPagesToSweep = 1;
    int max_freed = collector->sweeper()->ParallelSweepSpace(
        identity(), static_cast<int>(min_size_in_bytes), kMaxPagesToSweep,
        invalidated_slots_in_free_space);

    RefillFreeList();

    if (static_cast<size_t>(max_freed) >= min_size_in_bytes) {
      auto result = TryAllocationFromFreeListBackground(
          local_heap, min_size_in_bytes, max_size_in_bytes, alignment, origin);
      if (result) return result;
    }
  }

  if (heap()->ShouldExpandOldGenerationOnSlowAllocation(local_heap) &&
      heap()->CanExpandOldGenerationBackground(local_heap, AreaSize())) {
    auto result = ExpandBackground(local_heap, max_size_in_bytes);
    if (result) {
      DCHECK_EQ(Heap::GetFillToAlign(result->first, alignment), 0);
      return result;
    }
  }

  if (collector->sweeping_in_progress()) {
    // Complete sweeping for this space.
    collector->DrainSweepingWorklistForSpace(identity());

    RefillFreeList();

    // Last try to acquire memory from free list.
    return TryAllocationFromFreeListBackground(
        local_heap, min_size_in_bytes, max_size_in_bytes, alignment, origin);
  }

  return {};
}

base::Optional<std::pair<Address, size_t>>
PagedSpace::TryAllocationFromFreeListBackground(LocalHeap* local_heap,
                                                size_t min_size_in_bytes,
                                                size_t max_size_in_bytes,
                                                AllocationAlignment alignment,
                                                AllocationOrigin origin) {
  base::MutexGuard lock(&space_mutex_);
  DCHECK_LE(min_size_in_bytes, max_size_in_bytes);
  DCHECK(identity() == OLD_SPACE || identity() == MAP_SPACE);

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

  heap()->StartIncrementalMarkingIfAllocationLimitIsReachedBackground();

  size_t used_size_in_bytes = std::min(new_node_size, max_size_in_bytes);

  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = new_node.address() + used_size_in_bytes;
  DCHECK_LE(limit, end);
  DCHECK_LE(min_size_in_bytes, limit - start);
  if (limit != end) {
    Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }

  return std::make_pair(start, used_size_in_bytes);
}

#ifdef DEBUG
void PagedSpace::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpace::Verify(Isolate* isolate, ObjectVisitor* visitor) {
  bool allocation_pointer_found_in_space =
      (allocation_info_.top() == allocation_info_.limit());
  size_t external_space_bytes[kNumTypes];
  size_t external_page_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  for (Page* page : *this) {
    CHECK_EQ(page->owner(), this);

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

      // The first word should be a map, and we expect all map pointers to
      // be in map space.
      Map map = object.map();
      CHECK(map.IsMap());
      CHECK(ReadOnlyHeap::Contains(map) ||
            isolate->heap()->map_space()->Contains(map));

      // Perform space-specific object verification.
      VerifyObject(object);

      // The object itself should look OK.
      object.ObjectVerify(isolate);

      if (identity() != RO_SPACE && !FLAG_verify_heap_skip_remembered_set) {
        isolate->heap()->VerifyRememberedSetFor(object);
      }

      // All the interior pointers should be contained in the heap.
      int size = object.Size();
      object.IterateBody(map, size, visitor);
      CHECK(object.address() + size <= top);
      end_of_previous_object = object.address() + size;

      if (object.IsExternalString()) {
        ExternalString external_string = ExternalString::cast(object);
        size_t size = external_string.ExternalPayloadSize();
        external_page_bytes[ExternalBackingStoreType::kExternalString] += size;
      }
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      CHECK_EQ(external_page_bytes[t], page->ExternalBackingStoreBytes(t));
      external_space_bytes[t] += external_page_bytes[t];
    }
  }
  for (int i = 0; i < kNumTypes; i++) {
    if (i == ExternalBackingStoreType::kArrayBuffer) continue;
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }
  CHECK(allocation_pointer_found_in_space);

  if (identity() == OLD_SPACE && !FLAG_concurrent_array_buffer_sweeping) {
    size_t bytes = heap()->array_buffer_sweeper()->old().BytesSlow();
    CHECK_EQ(bytes,
             ExternalBackingStoreBytes(ExternalBackingStoreType::kArrayBuffer));
  }

#ifdef DEBUG
  VerifyCountersAfterSweeping(isolate->heap());
#endif
}

void PagedSpace::VerifyLiveBytes() {
  IncrementalMarking::MarkingState* marking_state =
      heap()->incremental_marking()->marking_state();
  for (Page* page : *this) {
    CHECK(page->SweepingDone());
    PagedSpaceObjectIterator it(heap(), this, page);
    int black_size = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      // All the interior pointers should be contained in the heap.
      if (marking_state->IsBlack(object)) {
        black_size += object.Size();
      }
    }
    CHECK_LE(black_size, marking_state->live_bytes(page));
  }
}
#endif  // VERIFY_HEAP

#ifdef DEBUG
void PagedSpace::VerifyCountersAfterSweeping(Heap* heap) {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  for (Page* page : *this) {
    DCHECK(page->SweepingDone());
    total_capacity += page->area_size();
    PagedSpaceObjectIterator it(heap, this, page);
    size_t real_allocated = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      if (!object.IsFreeSpaceOrFiller()) {
        real_allocated += object.Size();
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

void PagedSpace::VerifyCountersBeforeConcurrentSweeping() {
  // We need to refine the counters on pages that are already swept and have
  // not been moved over to the actual space. Otherwise, the AccountingStats
  // are just an over approximation.
  RefillFreeList();

  size_t total_capacity = 0;
  size_t total_allocated = 0;
  auto marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (Page* page : *this) {
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

void PagedSpace::UpdateInlineAllocationLimit(size_t min_size) {
  // Ensure there are no unaccounted allocations.
  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

  Address new_limit = ComputeLimit(top(), limit(), min_size);
  DCHECK_LE(top(), new_limit);
  DCHECK_LE(new_limit, limit());
  DecreaseLimit(new_limit);
}

// -----------------------------------------------------------------------------
// OldSpace implementation

void PagedSpace::PrepareForMarkCompact() {
  // We don't have a linear allocation area while sweeping.  It will be restored
  // on the first allocation after the sweep.
  FreeLinearAllocationArea();

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_->Reset();
}

bool PagedSpace::RefillLabMain(int size_in_bytes, AllocationOrigin origin) {
  VMState<GC> state(heap()->isolate());
  RCS_SCOPE(heap()->isolate(),
            RuntimeCallCounterId::kGC_Custom_SlowAllocateRaw);
  return RawRefillLabMain(size_in_bytes, origin);
}

Page* CompactionSpace::Expand() {
  Page* page = PagedSpace::Expand();
  new_pages_.push_back(page);
  return page;
}

bool CompactionSpace::RefillLabMain(int size_in_bytes,
                                    AllocationOrigin origin) {
  return RawRefillLabMain(size_in_bytes, origin);
}

bool PagedSpace::TryExpand(int size_in_bytes, AllocationOrigin origin) {
  Page* page = Expand();
  if (!page) return false;
  if (!is_compaction_space()) {
    heap()->NotifyOldGenerationExpansion(identity(), page);
  }
  DCHECK((CountTotalPages() > 1) ||
         (static_cast<size_t>(size_in_bytes) <= free_list_->Available()));
  return TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                       origin);
}

bool PagedSpace::RawRefillLabMain(int size_in_bytes, AllocationOrigin origin) {
  // Allocation in this space has failed.
  DCHECK_GE(size_in_bytes, 0);
  const int kMaxPagesToSweep = 1;

  if (TryAllocationFromFreeListMain(size_in_bytes, origin)) return true;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList();

    // Retry the free list allocation.
    if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                      origin))
      return true;

    if (ContributeToSweepingMain(size_in_bytes, kMaxPagesToSweep, size_in_bytes,
                                 origin))
      return true;
  }

  if (is_compaction_space()) {
    // The main thread may have acquired all swept pages. Try to steal from
    // it. This can only happen during young generation evacuation.
    PagedSpace* main_space = heap()->paged_space(identity());
    Page* page = main_space->RemovePageSafe(size_in_bytes);
    if (page != nullptr) {
      AddPage(page);
      if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                        origin))
        return true;
    }
  }

  if (heap()->ShouldExpandOldGenerationOnSlowAllocation() &&
      heap()->CanExpandOldGeneration(AreaSize())) {
    if (TryExpand(size_in_bytes, origin)) {
      return true;
    }
  }

  // Try sweeping all pages.
  if (ContributeToSweepingMain(0, 0, size_in_bytes, origin)) {
    return true;
  }

  if (heap()->gc_state() != Heap::NOT_IN_GC && !heap()->force_oom()) {
    // Avoid OOM crash in the GC in order to invoke NearHeapLimitCallback after
    // GC and give it a chance to increase the heap limit.
    return TryExpand(size_in_bytes, origin);
  }
  return false;
}

bool PagedSpace::ContributeToSweepingMain(int required_freed_bytes,
                                          int max_pages, int size_in_bytes,
                                          AllocationOrigin origin) {
  // Cleanup invalidated old-to-new refs for compaction space in the
  // final atomic pause.
  Sweeper::FreeSpaceMayContainInvalidatedSlots invalidated_slots_in_free_space =
      is_compaction_space() ? Sweeper::FreeSpaceMayContainInvalidatedSlots::kYes
                            : Sweeper::FreeSpaceMayContainInvalidatedSlots::kNo;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->sweeper()->ParallelSweepSpace(identity(), required_freed_bytes,
                                             max_pages,
                                             invalidated_slots_in_free_space);
    RefillFreeList();
    return TryAllocationFromFreeListMain(size_in_bytes, origin);
  }
  return false;
}

AllocationResult PagedSpace::AllocateRawSlow(int size_in_bytes,
                                             AllocationAlignment alignment,
                                             AllocationOrigin origin) {
  if (!is_compaction_space()) {
    // Start incremental marking before the actual allocation, this allows the
    // allocation function to mark the object black when incremental marking is
    // running.
    heap()->StartIncrementalMarkingIfAllocationLimitIsReached(
        heap()->GCFlagsForIncrementalMarking(),
        kGCCallbackScheduleIdleGarbageCollection);
  }

#ifdef V8_HOST_ARCH_32_BIT
  AllocationResult result =
      alignment != kWordAligned
          ? AllocateRawAligned(size_in_bytes, alignment, origin)
          : AllocateRawUnaligned(size_in_bytes, origin);
#else
  AllocationResult result = AllocateRawUnaligned(size_in_bytes, origin);
#endif
  return result;
}

// -----------------------------------------------------------------------------
// MapSpace implementation

// TODO(dmercadier): use a heap instead of sorting like that.
// Using a heap will have multiple benefits:
//   - for now, SortFreeList is only called after sweeping, which is somewhat
//   late. Using a heap, sorting could be done online: FreeListCategories would
//   be inserted in a heap (ie, in a sorted manner).
//   - SortFreeList is a bit fragile: any change to FreeListMap (or to
//   MapSpace::free_list_) could break it.
void MapSpace::SortFreeList() {
  using LiveBytesPagePair = std::pair<size_t, Page*>;
  std::vector<LiveBytesPagePair> pages;
  pages.reserve(CountTotalPages());

  for (Page* p : *this) {
    free_list()->RemoveCategory(p->free_list_category(kFirstCategory));
    pages.push_back(std::make_pair(p->allocated_bytes(), p));
  }

  // Sorting by least-allocated-bytes first.
  std::sort(pages.begin(), pages.end(),
            [](const LiveBytesPagePair& a, const LiveBytesPagePair& b) {
              return a.first < b.first;
            });

  for (LiveBytesPagePair const& p : pages) {
    // Since AddCategory inserts in head position, it reverts the order produced
    // by the sort above: least-allocated-bytes will be Added first, and will
    // therefore be the last element (and the first one will be
    // most-allocated-bytes).
    free_list()->AddCategory(p.second->free_list_category(kFirstCategory));
  }
}

#ifdef VERIFY_HEAP
void MapSpace::VerifyObject(HeapObject object) { CHECK(object.IsMap()); }
#endif

}  // namespace internal
}  // namespace v8
