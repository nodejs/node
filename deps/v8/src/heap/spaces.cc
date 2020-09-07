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
#include "src/common/globals.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/combined-heap.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/large-spaces.h"
#include "src/heap/mark-compact.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/remembered-set.h"
#include "src/heap/slot-set.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "src/sanitizer/msan.h"
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

PauseAllocationObserversScope::PauseAllocationObserversScope(Heap* heap)
    : heap_(heap) {
  DCHECK_EQ(heap->gc_state(), Heap::NOT_IN_GC);

  for (SpaceIterator it(heap_); it.HasNext();) {
    it.Next()->PauseAllocationObservers();
  }
}

PauseAllocationObserversScope::~PauseAllocationObserversScope() {
  for (SpaceIterator it(heap_); it.HasNext();) {
    it.Next()->ResumeAllocationObservers();
  }
}

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

void Page::AllocateLocalTracker() {
  DCHECK_NULL(local_tracker_);
  local_tracker_ = new LocalArrayBufferTracker(this);
}

bool Page::contains_array_buffers() {
  return local_tracker_ != nullptr && !local_tracker_->IsEmpty();
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
  allocation_observers_.push_back(observer);
  StartNextInlineAllocationStep();
}

void Space::RemoveAllocationObserver(AllocationObserver* observer) {
  auto it = std::find(allocation_observers_.begin(),
                      allocation_observers_.end(), observer);
  DCHECK(allocation_observers_.end() != it);
  allocation_observers_.erase(it);
  StartNextInlineAllocationStep();
}

void Space::PauseAllocationObservers() { allocation_observers_paused_ = true; }

void Space::ResumeAllocationObservers() {
  allocation_observers_paused_ = false;
}

void Space::AllocationStep(int bytes_since_last, Address soon_object,
                           int size) {
  if (!AllocationObserversActive()) {
    return;
  }

  DCHECK(!heap()->allocation_step_in_progress());
  heap()->set_allocation_step_in_progress(true);
  heap()->CreateFillerObjectAt(soon_object, size, ClearRecordedSlots::kNo);
  for (AllocationObserver* observer : allocation_observers_) {
    observer->AllocationStep(bytes_since_last, soon_object, size);
  }
  heap()->set_allocation_step_in_progress(false);
}

void Space::AllocationStepAfterMerge(Address first_object_in_chunk, int size) {
  if (!AllocationObserversActive()) {
    return;
  }

  DCHECK(!heap()->allocation_step_in_progress());
  heap()->set_allocation_step_in_progress(true);
  for (AllocationObserver* observer : allocation_observers_) {
    observer->AllocationStep(size, first_object_in_chunk, size);
  }
  heap()->set_allocation_step_in_progress(false);
}

intptr_t Space::GetNextInlineAllocationStepSize() {
  intptr_t next_step = 0;
  for (AllocationObserver* observer : allocation_observers_) {
    next_step = next_step ? Min(next_step, observer->bytes_to_next_step())
                          : observer->bytes_to_next_step();
  }
  DCHECK(allocation_observers_.size() == 0 || next_step > 0);
  return next_step;
}

Address SpaceWithLinearArea::ComputeLimit(Address start, Address end,
                                          size_t min_size) {
  DCHECK_GE(end - start, min_size);

  if (heap()->inline_allocation_disabled()) {
    // Fit the requested area exactly.
    return start + min_size;
  } else if (SupportsInlineAllocation() && AllocationObserversActive()) {
    // Generated code may allocate inline from the linear allocation area for.
    // To make sure we can observe these allocations, we use a lower limit.
    size_t step = GetNextInlineAllocationStepSize();
    size_t rounded_step =
        RoundSizeDownToObjectAlignment(static_cast<int>(step - 1));
    return Min(static_cast<Address>(start + min_size + rounded_step), end);
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
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearRecordedSlots::kNo);
  }
}

LocalAllocationBuffer::LocalAllocationBuffer(
    Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT
    : heap_(heap),
      allocation_info_(allocation_info) {
  if (IsValid()) {
    heap_->CreateFillerObjectAt(
        allocation_info_.top(),
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()),
        ClearRecordedSlots::kNo);
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
void SpaceWithLinearArea::StartNextInlineAllocationStep() {
  if (heap()->allocation_step_in_progress()) {
    // If we are mid-way through an existing step, don't start a new one.
    return;
  }

  if (AllocationObserversActive()) {
    top_on_previous_step_ = top();
    UpdateInlineAllocationLimit(0);
  } else {
    DCHECK_EQ(kNullAddress, top_on_previous_step_);
  }
}

void SpaceWithLinearArea::AddAllocationObserver(AllocationObserver* observer) {
  InlineAllocationStep(top(), top(), kNullAddress, 0);
  Space::AddAllocationObserver(observer);
  DCHECK_IMPLIES(top_on_previous_step_, AllocationObserversActive());
}

void SpaceWithLinearArea::RemoveAllocationObserver(
    AllocationObserver* observer) {
  Address top_for_next_step =
      allocation_observers_.size() == 1 ? kNullAddress : top();
  InlineAllocationStep(top(), top_for_next_step, kNullAddress, 0);
  Space::RemoveAllocationObserver(observer);
  DCHECK_IMPLIES(top_on_previous_step_, AllocationObserversActive());
}

void SpaceWithLinearArea::PauseAllocationObservers() {
  // Do a step to account for memory allocated so far.
  InlineAllocationStep(top(), kNullAddress, kNullAddress, 0);
  Space::PauseAllocationObservers();
  DCHECK_EQ(kNullAddress, top_on_previous_step_);
  UpdateInlineAllocationLimit(0);
}

void SpaceWithLinearArea::ResumeAllocationObservers() {
  DCHECK_EQ(kNullAddress, top_on_previous_step_);
  Space::ResumeAllocationObservers();
  StartNextInlineAllocationStep();
}

void SpaceWithLinearArea::InlineAllocationStep(Address top,
                                               Address top_for_next_step,
                                               Address soon_object,
                                               size_t size) {
  if (heap()->allocation_step_in_progress()) {
    // Avoid starting a new step if we are mid-way through an existing one.
    return;
  }

  if (top_on_previous_step_) {
    if (top < top_on_previous_step_) {
      // Generated code decreased the top pointer to do folded allocations.
      DCHECK_NE(top, kNullAddress);
      DCHECK_EQ(Page::FromAllocationAreaAddress(top),
                Page::FromAllocationAreaAddress(top_on_previous_step_));
      top_on_previous_step_ = top;
    }
    int bytes_allocated = static_cast<int>(top - top_on_previous_step_);
    AllocationStep(bytes_allocated, soon_object, static_cast<int>(size));
    top_on_previous_step_ = top_for_next_step;
  }
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
