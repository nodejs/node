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
#include "src/heap/base/active-system-pages.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/large-spaces.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-chunk-layout.h"
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

// -----------------------------------------------------------------------------
// PagedSpace implementation

void Space::AddAllocationObserver(AllocationObserver* observer) {
  allocation_counter_.AddAllocationObserver(observer);
}

void Space::RemoveAllocationObserver(AllocationObserver* observer) {
  allocation_counter_.RemoveAllocationObserver(observer);
}

Address SpaceWithLinearArea::ComputeLimit(Address start, Address end,
                                          size_t min_size) const {
  DCHECK_GE(end - start, min_size);

  // During GCs we always use the full LAB.
  if (heap()->IsInGC()) return end;

  if (!heap()->IsInlineAllocationEnabled()) {
    // LABs are disabled, so we fit the requested area exactly.
    return start + min_size;
  }

  // When LABs are enabled, pick the largest possible LAB size by default.
  size_t step_size = end - start;

  if (SupportsAllocationObserver() && heap()->IsAllocationObserverActive()) {
    // Ensure there are no unaccounted allocations.
    DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

    size_t step = allocation_counter_.NextBytes();
    DCHECK_NE(step, 0);
    // Generated code may allocate inline from the linear allocation area. To
    // make sure we can observe these allocations, we use a lower limit.
    size_t rounded_step = static_cast<size_t>(
        RoundSizeDownToObjectAlignment(static_cast<int>(step - 1)));
    step_size = std::min(step_size, rounded_step);
  }

  if (v8_flags.stress_marking) {
    step_size = std::min(step_size, static_cast<size_t>(64));
  }

  DCHECK_LE(start + step_size, end);
  return start + std::max(step_size, min_size);
}

void SpaceWithLinearArea::UpdateAllocationOrigins(AllocationOrigin origin) {
  DCHECK(!((origin != AllocationOrigin::kGC) &&
           (heap()->isolate()->current_vm_state() == GC)));
  allocations_origins_[static_cast<int>(origin)]++;
}

void SpaceWithLinearArea::PrintAllocationsOrigins() const {
  PrintIsolate(
      heap()->isolate(),
      "Allocations Origins for %s: GeneratedCode:%zu - Runtime:%zu - GC:%zu\n",
      ToString(identity()), allocations_origins_[0], allocations_origins_[1],
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
        static_cast<int>(allocation_info_.limit() - allocation_info_.top()));
  }
}

LocalAllocationBuffer::LocalAllocationBuffer(
    Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT
    : heap_(heap),
      allocation_info_(allocation_info) {}

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
    UpdateInlineAllocationLimit();
  } else {
    Space::AddAllocationObserver(observer);
  }
}

void SpaceWithLinearArea::RemoveAllocationObserver(
    AllocationObserver* observer) {
  if (!allocation_counter_.IsStepInProgress()) {
    AdvanceAllocationObservers();
    Space::RemoveAllocationObserver(observer);
    UpdateInlineAllocationLimit();
  } else {
    Space::RemoveAllocationObserver(observer);
  }
}

void SpaceWithLinearArea::PauseAllocationObservers() {
  AdvanceAllocationObservers();
}

void SpaceWithLinearArea::ResumeAllocationObservers() {
  MarkLabStartInitialized();
  UpdateInlineAllocationLimit();
}

void SpaceWithLinearArea::AdvanceAllocationObservers() {
  if (allocation_info_.top() &&
      allocation_info_.start() != allocation_info_.top()) {
    if (heap()->IsAllocationObserverActive()) {
      allocation_counter_.AdvanceAllocationObservers(allocation_info_.top() -
                                                     allocation_info_.start());
    }
    MarkLabStartInitialized();
  }
}

void SpaceWithLinearArea::MarkLabStartInitialized() {
  allocation_info_.ResetStart();
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

  if (!SupportsAllocationObserver() || !heap()->IsAllocationObserverActive())
    return;

  if (allocation_size >= allocation_counter_.NextBytes()) {
    // Only the first object in a LAB should reach the next step.
    DCHECK_EQ(soon_object,
              allocation_info_.start() + aligned_size_in_bytes - size_in_bytes);

    // Right now the LAB only contains that one object.
    DCHECK_EQ(allocation_info_.top() + allocation_size - aligned_size_in_bytes,
              allocation_info_.limit());

    // Ensure that there is a valid object
    heap_->CreateFillerObjectAt(soon_object, static_cast<int>(size_in_bytes));

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

  DCHECK_LT(allocation_info_.limit() - allocation_info_.start(),
            allocation_counter_.NextBytes());
}

#if DEBUG
void SpaceWithLinearArea::VerifyTop() const {
  // Ensure validity of LAB: start <= top <= limit
  DCHECK_LE(allocation_info_.start(), allocation_info_.top());
  DCHECK_LE(allocation_info_.top(), allocation_info_.limit());
}
#endif  // DEBUG

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

SpaceIterator::SpaceIterator(Heap* heap)
    : heap_(heap), current_space_(FIRST_MUTABLE_SPACE) {}

SpaceIterator::~SpaceIterator() = default;

bool SpaceIterator::HasNext() {
  while (current_space_ <= LAST_MUTABLE_SPACE) {
    Space* space = heap_->space(current_space_);
    if (space) return true;
    ++current_space_;
  }

  // No more spaces left.
  return false;
}

Space* SpaceIterator::Next() {
  DCHECK_LE(current_space_, LAST_MUTABLE_SPACE);
  Space* space = heap_->space(current_space_++);
  DCHECK_NOT_NULL(space);
  return space;
}

}  // namespace internal
}  // namespace v8
