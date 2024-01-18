// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/main-allocator-inl.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

MainAllocator::MainAllocator(Heap* heap, SpaceWithLinearArea* space,
                             CompactionSpaceKind compaction_space_kind,
                             SupportsExtendingLAB supports_extending_lab,
                             LinearAllocationArea& allocation_info)
    : heap_(heap),
      space_(space),
      compaction_space_kind_(compaction_space_kind),
      supports_extending_lab_(supports_extending_lab),
      allocation_info_(allocation_info) {}

MainAllocator::MainAllocator(Heap* heap, SpaceWithLinearArea* space,
                             CompactionSpaceKind compaction_space_kind,
                             SupportsExtendingLAB supports_extending_lab)
    : heap_(heap),
      space_(space),
      compaction_space_kind_(compaction_space_kind),
      supports_extending_lab_(supports_extending_lab),
      allocation_info_(owned_allocation_info_) {}

AllocationResult MainAllocator::AllocateRawForceAlignmentForTesting(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);

  AllocationResult result =
      AllocateFastAligned(size_in_bytes, nullptr, alignment, origin);

  return V8_UNLIKELY(result.IsFailure())
             ? AllocateRawSlowAligned(size_in_bytes, alignment, origin)
             : result;
}

void MainAllocator::AddAllocationObserver(AllocationObserver* observer) {
  if (!allocation_counter().IsStepInProgress()) {
    AdvanceAllocationObservers();
    allocation_counter().AddAllocationObserver(observer);
    UpdateInlineAllocationLimit();
  } else {
    allocation_counter().AddAllocationObserver(observer);
  }
}

void MainAllocator::RemoveAllocationObserver(AllocationObserver* observer) {
  if (!allocation_counter().IsStepInProgress()) {
    AdvanceAllocationObservers();
    allocation_counter().RemoveAllocationObserver(observer);
    UpdateInlineAllocationLimit();
  } else {
    allocation_counter().RemoveAllocationObserver(observer);
  }
}

void MainAllocator::PauseAllocationObservers() { AdvanceAllocationObservers(); }

void MainAllocator::ResumeAllocationObservers() {
  MarkLabStartInitialized();
  UpdateInlineAllocationLimit();
}

void MainAllocator::AdvanceAllocationObservers() {
  if (allocation_info().top() &&
      allocation_info().start() != allocation_info().top()) {
    if (heap()->IsAllocationObserverActive()) {
      allocation_counter().AdvanceAllocationObservers(
          allocation_info().top() - allocation_info().start());
    }
    MarkLabStartInitialized();
  }
}

void MainAllocator::MarkLabStartInitialized() {
  allocation_info().ResetStart();
  if (identity() == NEW_SPACE) {
    MoveOriginalTopForward();

#if DEBUG
    Verify();
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
void MainAllocator::InvokeAllocationObservers(Address soon_object,
                                              size_t size_in_bytes,
                                              size_t aligned_size_in_bytes,
                                              size_t allocation_size) {
  DCHECK_LE(size_in_bytes, aligned_size_in_bytes);
  DCHECK_LE(aligned_size_in_bytes, allocation_size);
  DCHECK(size_in_bytes == aligned_size_in_bytes ||
         aligned_size_in_bytes == allocation_size);

  if (!SupportsAllocationObserver() || !heap()->IsAllocationObserverActive())
    return;

  if (allocation_size >= allocation_counter().NextBytes()) {
    // Only the first object in a LAB should reach the next step.
    DCHECK_EQ(soon_object, allocation_info().start() + aligned_size_in_bytes -
                               size_in_bytes);

    // Right now the LAB only contains that one object.
    DCHECK_EQ(allocation_info().top() + allocation_size - aligned_size_in_bytes,
              allocation_info().limit());

    // Ensure that there is a valid object
    heap_->CreateFillerObjectAt(soon_object, static_cast<int>(size_in_bytes));

#if DEBUG
    // Ensure that allocation_info_ isn't modified during one of the
    // AllocationObserver::Step methods.
    LinearAllocationArea saved_allocation_info = allocation_info();
#endif

    // Run AllocationObserver::Step through the AllocationCounter.
    allocation_counter().InvokeAllocationObservers(soon_object, size_in_bytes,
                                                   allocation_size);

    // Ensure that start/top/limit didn't change.
    DCHECK_EQ(saved_allocation_info.start(), allocation_info().start());
    DCHECK_EQ(saved_allocation_info.top(), allocation_info().top());
    DCHECK_EQ(saved_allocation_info.limit(), allocation_info().limit());
  }

  DCHECK_LT(allocation_info().limit() - allocation_info().start(),
            allocation_counter().NextBytes());
}

AllocationResult MainAllocator::AllocateRawSlow(int size_in_bytes,
                                                AllocationAlignment alignment,
                                                AllocationOrigin origin) {
  AllocationResult result =
      USE_ALLOCATION_ALIGNMENT_BOOL && alignment != kTaggedAligned
          ? AllocateRawSlowAligned(size_in_bytes, alignment, origin)
          : AllocateRawSlowUnaligned(size_in_bytes, origin);
  return result;
}

AllocationResult MainAllocator::AllocateRawSlowUnaligned(
    int size_in_bytes, AllocationOrigin origin) {
  DCHECK(!v8_flags.enable_third_party_heap);
  int max_aligned_size;
  if (!EnsureAllocation(size_in_bytes, kTaggedAligned, origin,
                        &max_aligned_size)) {
    return AllocationResult::Failure();
  }

  DCHECK_EQ(max_aligned_size, size_in_bytes);
  DCHECK_LE(allocation_info().start(), allocation_info().top());

  AllocationResult result = AllocateFastUnaligned(size_in_bytes, origin);
  DCHECK(!result.IsFailure());

  InvokeAllocationObservers(result.ToAddress(), size_in_bytes, size_in_bytes,
                            size_in_bytes);

  return result;
}

AllocationResult MainAllocator::AllocateRawSlowAligned(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  DCHECK(!v8_flags.enable_third_party_heap);
  int max_aligned_size;
  if (!EnsureAllocation(size_in_bytes, alignment, origin, &max_aligned_size)) {
    return AllocationResult::Failure();
  }

  DCHECK_GE(max_aligned_size, size_in_bytes);
  DCHECK_LE(allocation_info().start(), allocation_info().top());

  int aligned_size_in_bytes;

  AllocationResult result = AllocateFastAligned(
      size_in_bytes, &aligned_size_in_bytes, alignment, origin);
  DCHECK_GE(max_aligned_size, aligned_size_in_bytes);
  DCHECK(!result.IsFailure());

  InvokeAllocationObservers(result.ToAddress(), size_in_bytes,
                            aligned_size_in_bytes, max_aligned_size);

  return result;
}

void MainAllocator::MakeLinearAllocationAreaIterable() {
  Address current_top = top();
  Address current_limit = original_limit_relaxed();
  DCHECK_GE(current_limit, limit());
  if (current_top != kNullAddress && current_top != current_limit) {
    heap_->CreateFillerObjectAt(current_top,
                                static_cast<int>(current_limit - current_top));
  }
}

void MainAllocator::MarkLinearAllocationAreaBlack() {
  DCHECK(heap()->incremental_marking()->black_allocation());
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->CreateBlackArea(current_top, current_limit);
  }
}

void MainAllocator::UnmarkLinearAllocationArea() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }
}

void MainAllocator::MoveOriginalTopForward() {
  DCHECK(!is_compaction_space());
  base::SharedMutexGuard<base::kExclusive> guard(
      linear_area_original_data_.linear_area_lock());
  DCHECK_GE(top(), linear_area_original_data_.get_original_top_acquire());
  DCHECK_LE(top(), linear_area_original_data_.get_original_limit_relaxed());
  linear_area_original_data_.set_original_top_release(top());
}

void MainAllocator::ResetLab(Address start, Address end, Address extended_end) {
  DCHECK_LE(start, end);
  DCHECK_LE(end, extended_end);

  allocation_info().Reset(start, end);

  base::Optional<base::SharedMutexGuard<base::kExclusive>> guard;
  if (!is_compaction_space())
    guard.emplace(linear_area_original_data_.linear_area_lock());
  linear_area_original_data().set_original_limit_relaxed(extended_end);
  linear_area_original_data().set_original_top_release(start);
}

bool MainAllocator::IsPendingAllocation(Address object_address) {
  DCHECK(!is_compaction_space());
  base::SharedMutexGuard<base::kShared> guard(
      linear_area_original_data_.linear_area_lock());
  Address top = original_top_acquire();
  Address limit = original_limit_relaxed();
  DCHECK_LE(top, limit);
  return top && top <= object_address && object_address < limit;
}

void MainAllocator::MaybeFreeUnusedLab(LinearAllocationArea lab) {
  DCHECK(!is_compaction_space());

  if (allocation_info().MergeIfAdjacent(lab)) {
    base::SharedMutexGuard<base::kExclusive> guard(
        linear_area_original_data_.linear_area_lock());
    linear_area_original_data().set_original_top_release(
        allocation_info().top());
  }

#if DEBUG
  Verify();
#endif
}

bool MainAllocator::EnsureAllocation(int size_in_bytes,
                                     AllocationAlignment alignment,
                                     AllocationOrigin origin,
                                     int* out_max_aligned_size) {
  return space_->EnsureAllocation(size_in_bytes, alignment, origin,
                                  out_max_aligned_size);
}

void MainAllocator::UpdateInlineAllocationLimit() {
  return space_->UpdateInlineAllocationLimit();
}

void MainAllocator::FreeLinearAllocationArea() {
  space_->FreeLinearAllocationArea();
}

void MainAllocator::ExtendLAB(Address limit) {
  DCHECK(supports_extending_lab());
  DCHECK_LE(limit, original_limit_relaxed());
  allocation_info().SetLimit(limit);
}

Address MainAllocator::ComputeLimit(Address start, Address end,
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
    DCHECK_EQ(allocation_info().start(), allocation_info().top());

    size_t step = allocation_counter().NextBytes();
    DCHECK_NE(step, 0);
    // Generated code may allocate inline from the linear allocation area. To
    // make sure we can observe these allocations, we use a lower limit.
    size_t rounded_step = static_cast<size_t>(
        RoundDown(static_cast<int>(step - 1), ObjectAlignment()));
    step_size = std::min(step_size, rounded_step);
  }

  if (v8_flags.stress_marking) {
    step_size = std::min(step_size, static_cast<size_t>(64));
  }

  DCHECK_LE(start + step_size, end);
  return start + std::max(step_size, min_size);
}

#if DEBUG
void MainAllocator::Verify() const {
  // Ensure validity of LAB: start <= top <= limit
  DCHECK_LE(allocation_info().start(), allocation_info().top());
  DCHECK_LE(allocation_info().top(), allocation_info().limit());

  // Ensure that original_top_ always >= LAB start. The delta between start_
  // and top_ is still to be processed by allocation observers.
  DCHECK_GE(linear_area_original_data().get_original_top_acquire(),
            allocation_info().start());

  // Ensure that limit() is <= original_limit_.
  DCHECK_LE(allocation_info().limit(),
            linear_area_original_data().get_original_limit_relaxed());
}
#endif  // DEBUG

int MainAllocator::ObjectAlignment() const {
  if (identity() == CODE_SPACE) {
    return kCodeAlignment;
  } else if (V8_COMPRESS_POINTERS_8GB_BOOL) {
    return kObjectAlignment8GbHeap;
  } else {
    return kTaggedSize;
  }
}

AllocationSpace MainAllocator::identity() const { return space_->identity(); }

}  // namespace internal
}  // namespace v8
