// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"

#include <algorithm>
#include <cinttypes>
#include <utility>

#include "src/base/bits.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/macros.h"
#include "src/base/sanitizer/msan.h"
#include "src/common/globals.h"
#include "src/heap/combined-heap.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/large-spaces.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/remembered-set.h"
#include "src/heap/slot-set.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/snapshot.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// These checks are here to ensure that the lower 32 bits of any real heap
// object can't overlap with the lower 32 bits of cleared weak reference value
// and therefore it's enough to compare only the lower 32 bits of a MaybeObject
// in order to figure out if it's a cleared weak reference or not.
STATIC_ASSERT(kClearedWeakHeapObjectLower32 > 0);
STATIC_ASSERT(kClearedWeakHeapObjectLower32 < Page::kHeaderSize);

void Page::AllocateFreeListCategories() {
  DCHECK_NULL(categories_);
  categories_ =
      new FreeListCategory*[owner()->free_list()->number_of_categories()]();
  for (int i = kFirstCategory; i <= owner()->free_list()->last_category();
       i++) {
    DCHECK_NULL(categories_[i]);
    categories_[i] = new FreeListCategory();
  }
}

void Page::InitializeFreeListCategories() {
  for (int i = kFirstCategory; i <= owner()->free_list()->last_category();
       i++) {
    categories_[i]->Initialize(static_cast<FreeListCategoryType>(i));
  }
}

void Page::ReleaseFreeListCategories() {
  if (categories_ != nullptr) {
    for (int i = kFirstCategory; i <= owner()->free_list()->last_category();
         i++) {
      if (categories_[i] != nullptr) {
        delete categories_[i];
        categories_[i] = nullptr;
      }
    }
    delete[] categories_;
    categories_ = nullptr;
  }
}

Page* Page::ConvertNewToOld(Page* old_page) {
  DCHECK(old_page);
  DCHECK(old_page->InNewSpace());
  OldSpace* old_space = old_page->heap()->old_space();
  old_page->set_owner(old_space);
  old_page->SetFlags(0, static_cast<uintptr_t>(~0));
  Page* new_page = old_space->InitializePage(old_page);
  old_space->AddPage(new_page);
  return new_page;
}

void Page::MoveOldToNewRememberedSetForSweeping() {
  CHECK_NULL(sweeping_slot_set_);
  sweeping_slot_set_ = slot_set_[OLD_TO_NEW];
  slot_set_[OLD_TO_NEW] = nullptr;
}

void Page::MergeOldToNewRememberedSets() {
  if (sweeping_slot_set_ == nullptr) return;

  if (slot_set_[OLD_TO_NEW]) {
    RememberedSet<OLD_TO_NEW>::Iterate(
        this,
        [this](MaybeObjectSlot slot) {
          Address address = slot.address();
          RememberedSetSweeping::Insert<AccessMode::NON_ATOMIC>(this, address);
          return KEEP_SLOT;
        },
        SlotSet::KEEP_EMPTY_BUCKETS);

    ReleaseSlotSet<OLD_TO_NEW>();
  }

  CHECK_NULL(slot_set_[OLD_TO_NEW]);
  slot_set_[OLD_TO_NEW] = sweeping_slot_set_;
  sweeping_slot_set_ = nullptr;
}

size_t Page::AvailableInFreeList() {
  size_t sum = 0;
  ForAllFreeListCategories([&sum](FreeListCategory* category) {
    sum += category->available();
  });
  return sum;
}

#ifdef DEBUG
namespace {
// Skips filler starting from the given filler until the end address.
// Returns the first address after the skipped fillers.
Address SkipFillers(HeapObject filler, Address end) {
  Address addr = filler.address();
  while (addr < end) {
    filler = HeapObject::FromAddress(addr);
    CHECK(filler.IsFreeSpaceOrFiller());
    addr = filler.address() + filler.Size();
  }
  return addr;
}
}  // anonymous namespace
#endif  // DEBUG

size_t Page::ShrinkToHighWaterMark() {
  // Shrinking only makes sense outside of the CodeRange, where we don't care
  // about address space fragmentation.
  VirtualMemory* reservation = reserved_memory();
  if (!reservation->IsReserved()) return 0;

  // Shrink pages to high water mark. The water mark points either to a filler
  // or the area_end.
  HeapObject filler = HeapObject::FromAddress(HighWaterMark());
  if (filler.address() == area_end()) return 0;
  CHECK(filler.IsFreeSpaceOrFiller());
  // Ensure that no objects were allocated in [filler, area_end) region.
  DCHECK_EQ(area_end(), SkipFillers(filler, area_end()));
  // Ensure that no objects will be allocated on this page.
  DCHECK_EQ(0u, AvailableInFreeList());

  // Ensure that slot sets are empty. Otherwise the buckets for the shrinked
  // area would not be freed when deallocating this page.
  DCHECK_NULL(slot_set<OLD_TO_NEW>());
  DCHECK_NULL(slot_set<OLD_TO_OLD>());
  DCHECK_NULL(sweeping_slot_set());

  size_t unused = RoundDown(static_cast<size_t>(area_end() - filler.address()),
                            MemoryAllocator::GetCommitPageSize());
  if (unused > 0) {
    DCHECK_EQ(0u, unused % MemoryAllocator::GetCommitPageSize());
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Shrinking page %p: end %p -> %p\n",
                   reinterpret_cast<void*>(this),
                   reinterpret_cast<void*>(area_end()),
                   reinterpret_cast<void*>(area_end() - unused));
    }
    heap()->CreateFillerObjectAt(
        filler.address(),
        static_cast<int>(area_end() - filler.address() - unused),
        ClearRecordedSlots::kNo);
    heap()->memory_allocator()->PartialFreeMemory(
        this, address() + size() - unused, unused, area_end() - unused);
    if (filler.address() != area_end()) {
      CHECK(filler.IsFreeSpaceOrFiller());
      CHECK_EQ(filler.address() + filler.Size(), area_end());
    }
  }
  return unused;
}

void Page::CreateBlackArea(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_LT(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  IncrementalMarking::MarkingState* marking_state =
      heap()->incremental_marking()->marking_state();
  marking_state->bitmap(this)->SetRange(AddressToMarkbitIndex(start),
                                        AddressToMarkbitIndex(end));
  marking_state->IncrementLiveBytes(this, static_cast<intptr_t>(end - start));
}

void Page::CreateBlackAreaBackground(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_LT(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  IncrementalMarking::AtomicMarkingState* marking_state =
      heap()->incremental_marking()->atomic_marking_state();
  marking_state->bitmap(this)->SetRange(AddressToMarkbitIndex(start),
                                        AddressToMarkbitIndex(end));
  heap()->incremental_marking()->IncrementLiveBytesBackground(
      this, static_cast<intptr_t>(end - start));
}

void Page::DestroyBlackArea(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_LT(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  IncrementalMarking::MarkingState* marking_state =
      heap()->incremental_marking()->marking_state();
  marking_state->bitmap(this)->ClearRange(AddressToMarkbitIndex(start),
                                          AddressToMarkbitIndex(end));
  marking_state->IncrementLiveBytes(this, -static_cast<intptr_t>(end - start));
}

void Page::DestroyBlackAreaBackground(Address start, Address end) {
  DCHECK(heap()->incremental_marking()->black_allocation());
  DCHECK_EQ(Page::FromAddress(start), this);
  DCHECK_LT(start, end);
  DCHECK_EQ(Page::FromAddress(end - 1), this);
  IncrementalMarking::AtomicMarkingState* marking_state =
      heap()->incremental_marking()->atomic_marking_state();
  marking_state->bitmap(this)->ClearRange(AddressToMarkbitIndex(start),
                                          AddressToMarkbitIndex(end));
  heap()->incremental_marking()->IncrementLiveBytesBackground(
      this, -static_cast<intptr_t>(end - start));
}

// -----------------------------------------------------------------------------
// PagedSpace implementation

void Space::AddAllocationObserver(AllocationObserver* observer) {
  allocation_counter_.AddAllocationObserver(observer);
}

void Space::RemoveAllocationObserver(AllocationObserver* observer) {
  allocation_counter_.RemoveAllocationObserver(observer);
}

void Space::PauseAllocationObservers() { allocation_counter_.Pause(); }

void Space::ResumeAllocationObservers() { allocation_counter_.Resume(); }

Address SpaceWithLinearArea::ComputeLimit(Address start, Address end,
                                          size_t min_size) {
  DCHECK_GE(end - start, min_size);

  if (heap()->inline_allocation_disabled()) {
    // Fit the requested area exactly.
    return start + min_size;
  } else if (SupportsAllocationObserver() && allocation_counter_.IsActive()) {
    // Ensure there are no unaccounted allocations.
    DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

    // Generated code may allocate inline from the linear allocation area for.
    // To make sure we can observe these allocations, we use a lower ©limit.
    size_t step = allocation_counter_.NextBytes();
    DCHECK_NE(step, 0);
    size_t rounded_step =
        RoundSizeDownToObjectAlignment(static_cast<int>(step - 1));
    // Use uint64_t to avoid overflow on 32-bit
    uint64_t step_end =
        static_cast<uint64_t>(start) + std::max(min_size, rounded_step);
    uint64_t new_end = std::min(step_end, static_cast<uint64_t>(end));
    return static_cast<Address>(new_end);
  } else {
    // The entire node can be used as the linear allocation area.
    return end;
  }
}

void SpaceWithLinearArea::UpdateAllocationOrigins(AllocationOrigin origin) {
  DCHECK(!((origin != AllocationOrigin::kGC) &&
           (heap()->isolate()->current_vm_state() == GC)));
  allocations_origins_[static_cast<int>(origin)]++;
}

void SpaceWithLinearArea::PrintAllocationsOrigins() {
  PrintIsolate(
      heap()->isolate(),
      "Allocations Origins for %s: GeneratedCode:%zu - Runtime:%zu - GC:%zu\n",
      name(), allocations_origins_[0], allocations_origins_[1],
      allocations_origins_[2]);
}

LinearAllocationArea LocalAllocationBuffer::CloseAndMakeIterable() {
  if (IsValid()) {
    MakeIterable();
    const LinearAllocationArea old_info = allocation_info_;
    allocation_info_ = LinearAllocationArea(kNullAddress, kNullAddress);
    return old_info;
  }
  return LinearAllocationArea(kNullAddress, kNullAddress);
}

void LocalAllocationBuffer::MakeIterable() {
  if (IsValid()) {
    heap_->CreateFillerObjectAtBackground(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearFreedMemoryMode::kDontClearFreedMemory);
  }
}

LocalAllocationBuffer::LocalAllocationBuffer(
    Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT
    : heap_(heap),
      allocation_info_(allocation_info) {
  if (IsValid()) {
    heap_->CreateFillerObjectAtBackground(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearFreedMemoryMode::kDontClearFreedMemory);
  }
}

LocalAllocationBuffer::LocalAllocationBuffer(LocalAllocationBuffer&& other)
    V8_NOEXCEPT {
  *this = std::move(other);
}

LocalAllocationBuffer& LocalAllocationBuffer::operator=(
    LocalAllocationBuffer&& other) V8_NOEXCEPT {
  heap_ = other.heap_;
  allocation_info_ = other.allocation_info_;

  other.allocation_info_.Reset(kNullAddress, kNullAddress);
  return *this;
}

void SpaceWithLinearArea::AddAllocationObserver(AllocationObserver* observer) {
  if (!allocation_counter_.IsStepInProgress()) {
    AdvanceAllocationObservers();
    Space::AddAllocationObserver(observer);
    UpdateInlineAllocationLimit(0);
  } else {
    Space::AddAllocationObserver(observer);
  }
}

void SpaceWithLinearArea::RemoveAllocationObserver(
    AllocationObserver* observer) {
  if (!allocation_counter_.IsStepInProgress()) {
    AdvanceAllocationObservers();
    Space::RemoveAllocationObserver(observer);
    UpdateInlineAllocationLimit(0);
  } else {
    Space::RemoveAllocationObserver(observer);
  }
}

void SpaceWithLinearArea::PauseAllocationObservers() {
  AdvanceAllocationObservers();
  Space::PauseAllocationObservers();
}

void SpaceWithLinearArea::ResumeAllocationObservers() {
  Space::ResumeAllocationObservers();
  MarkLabStartInitialized();
  UpdateInlineAllocationLimit(0);
}

void SpaceWithLinearArea::AdvanceAllocationObservers() {
  if (allocation_info_.top() &&
      allocation_info_.start() != allocation_info_.top()) {
    allocation_counter_.AdvanceAllocationObservers(allocation_info_.top() -
                                                   allocation_info_.start());
    MarkLabStartInitialized();
  }
}

void SpaceWithLinearArea::MarkLabStartInitialized() {
  allocation_info_.MoveStartToTop();
  if (identity() == NEW_SPACE) {
    heap()->new_space()->MoveOriginalTopForward();

#if DEBUG
    heap()->VerifyNewSpaceTop();
#endif
  }
}

// Perform an allocation step when the step is reached. size_in_bytes is the
// actual size needed for the object (required for InvokeAllocationObservers).
// aligned_size_in_bytes is the size of the object including the filler right
// before it to reach the right alignment (required to DCHECK the start of the
// object). allocation_size is the size of the actual allocation which needs to
// be used for the accounting. It can be different from aligned_size_in_bytes in
// PagedSpace::AllocateRawAligned, where we have to overallocate in order to be
// able to align the allocation afterwards.
void SpaceWithLinearArea::InvokeAllocationObservers(
    Address soon_object, size_t size_in_bytes, size_t aligned_size_in_bytes,
    size_t allocation_size) {
  DCHECK_LE(size_in_bytes, aligned_size_in_bytes);
  DCHECK_LE(aligned_size_in_bytes, allocation_size);
  DCHECK(size_in_bytes == aligned_size_in_bytes ||
         aligned_size_in_bytes == allocation_size);

  if (!SupportsAllocationObserver() || !allocation_counter_.IsActive()) return;

  if (allocation_size >= allocation_counter_.NextBytes()) {
    // Only the first object in a LAB should reach the next step.
    DCHECK_EQ(soon_object,
              allocation_info_.start() + aligned_size_in_bytes - size_in_bytes);

    // Right now the LAB only contains that one object.
    DCHECK_EQ(allocation_info_.top() + allocation_size - aligned_size_in_bytes,
              allocation_info_.limit());

    // Ensure that there is a valid object
    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(soon_object);
      heap()->UnprotectAndRegisterMemoryChunk(chunk);
    }
    heap_->CreateFillerObjectAt(soon_object, static_cast<int>(size_in_bytes),
                                ClearRecordedSlots::kNo);

#if DEBUG
    // Ensure that allocation_info_ isn't modified during one of the
    // AllocationObserver::Step methods.
    LinearAllocationArea saved_allocation_info = allocation_info_;
#endif

    // Run AllocationObserver::Step through the AllocationCounter.
    allocation_counter_.InvokeAllocationObservers(soon_object, size_in_bytes,
                                                  allocation_size);

    // Ensure that start/top/limit didn't change.
    DCHECK_EQ(saved_allocation_info.start(), allocation_info_.start());
    DCHECK_EQ(saved_allocation_info.top(), allocation_info_.top());
    DCHECK_EQ(saved_allocation_info.limit(), allocation_info_.limit());
  }

  DCHECK_IMPLIES(allocation_counter_.IsActive(),
                 (allocation_info_.limit() - allocation_info_.start()) <
                     allocation_counter_.NextBytes());
}

int MemoryChunk::FreeListsLength() {
  int length = 0;
  for (int cat = kFirstCategory; cat <= owner()->free_list()->last_category();
       cat++) {
    if (categories_[cat] != nullptr) {
      length += categories_[cat]->FreeListLength();
    }
  }
  return length;
}

}  // namespace internal
}  // namespace v8
