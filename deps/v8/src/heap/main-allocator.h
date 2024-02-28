// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MAIN_ALLOCATOR_H_
#define V8_HEAP_MAIN_ALLOCATOR_H_

#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/allocation-result.h"
#include "src/heap/linear-allocation-area.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class Heap;
class SpaceWithLinearArea;

class LinearAreaOriginalData {
 public:
  Address get_original_top_acquire() const {
    return original_top_.load(std::memory_order_acquire);
  }
  Address get_original_limit_relaxed() const {
    return original_limit_.load(std::memory_order_relaxed);
  }

  void set_original_top_release(Address top) {
    original_top_.store(top, std::memory_order_release);
  }
  void set_original_limit_relaxed(Address limit) {
    original_limit_.store(limit, std::memory_order_relaxed);
  }

  base::SharedMutex* linear_area_lock() { return &linear_area_lock_; }

 private:
  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks. Protected by
  // pending_allocation_mutex_.
  std::atomic<Address> original_top_ = 0;
  std::atomic<Address> original_limit_ = 0;

  // Protects original_top_ and original_limit_.
  base::SharedMutex linear_area_lock_;
};

class MainAllocator {
 public:
  enum SupportsExtendingLAB { kYes, kNo };

  MainAllocator(Heap* heap, SpaceWithLinearArea* space,
                CompactionSpaceKind compaction_space_kind,
                SupportsExtendingLAB supports_extending_lab);

  // This constructor allows to pass in the address of a LinearAllocationArea.
  MainAllocator(Heap* heap, SpaceWithLinearArea* space,
                CompactionSpaceKind compaction_space_kind,
                SupportsExtendingLAB supports_extending_lab,
                LinearAllocationArea& allocation_info);

  // Returns the allocation pointer in this space.
  Address start() const { return allocation_info_.start(); }
  Address top() const { return allocation_info_.top(); }
  Address limit() const { return allocation_info_.limit(); }

  // The allocation top address.
  Address* allocation_top_address() const {
    return allocation_info_.top_address();
  }

  // The allocation limit address.
  Address* allocation_limit_address() const {
    return allocation_info_.limit_address();
  }

  Address original_top_acquire() const {
    return linear_area_original_data_.get_original_top_acquire();
  }

  Address original_limit_relaxed() const {
    return linear_area_original_data_.get_original_limit_relaxed();
  }

  void MoveOriginalTopForward();
  void ResetLab(Address start, Address end, Address extended_end);
  V8_EXPORT_PRIVATE bool IsPendingAllocation(Address object_address);
  void MaybeFreeUnusedLab(LinearAllocationArea lab);

  LinearAllocationArea& allocation_info() { return allocation_info_; }

  const LinearAllocationArea& allocation_info() const {
    return allocation_info_;
  }

  AllocationCounter& allocation_counter() { return allocation_counter_; }

  const AllocationCounter& allocation_counter() const {
    return allocation_counter_;
  }

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationAlignment alignment,
              AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT V8_EXPORT_PRIVATE AllocationResult
  AllocateRawForceAlignmentForTesting(int size_in_bytes,
                                      AllocationAlignment alignment,
                                      AllocationOrigin);

  V8_EXPORT_PRIVATE void AddAllocationObserver(AllocationObserver* observer);
  V8_EXPORT_PRIVATE void RemoveAllocationObserver(AllocationObserver* observer);
  void PauseAllocationObservers();
  void ResumeAllocationObservers();

  V8_EXPORT_PRIVATE void AdvanceAllocationObservers();
  V8_EXPORT_PRIVATE void InvokeAllocationObservers(Address soon_object,
                                                   size_t size_in_bytes,
                                                   size_t aligned_size_in_bytes,
                                                   size_t allocation_size);

  void MarkLabStartInitialized();

  void MakeLinearAllocationAreaIterable();

  void MarkLinearAllocationAreaBlack();
  void UnmarkLinearAllocationArea();

  V8_INLINE bool TryFreeLast(Address object_address, int object_size);

  // When allocation observers are active we may use a lower limit to allow the
  // observers to 'interrupt' earlier than the natural limit. Given a linear
  // area bounded by [start, end), this function computes the limit to use to
  // allow proper observation based on existing observers. min_size specifies
  // the minimum size that the limited area should have.
  Address ComputeLimit(Address start, Address end, size_t min_size) const;

#if DEBUG
  void Verify() const;
#endif  // DEBUG

  // Checks whether the LAB is currently in use.
  V8_INLINE bool IsLabValid() { return allocation_info_.top() != kNullAddress; }

  void UpdateInlineAllocationLimit();

  V8_EXPORT_PRIVATE void FreeLinearAllocationArea();

  void ExtendLAB(Address limit);

  bool supports_extending_lab() const {
    return supports_extending_lab_ == SupportsExtendingLAB::kYes;
  }

 private:
  // Allocates an object from the linear allocation area. Assumes that the
  // linear allocation area is large enough to fit the object.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateFastUnaligned(int size_in_bytes, AllocationOrigin origin);

  // Tries to allocate an aligned object from the linear allocation area.
  // Returns nullptr if the linear allocation area does not fit the object.
  // Otherwise, returns the object pointer and writes the allocation size
  // (object size + alignment filler size) to the result_aligned_size_in_bytes.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateFastAligned(int size_in_bytes, int* result_aligned_size_in_bytes,
                      AllocationAlignment alignment, AllocationOrigin origin);

  // Slow path of allocation function
  V8_WARN_UNUSED_RESULT V8_EXPORT_PRIVATE AllocationResult
  AllocateRawSlow(int size_in_bytes, AllocationAlignment alignment,
                  AllocationOrigin origin);

  // Allocate the requested number of bytes in the space if possible, return a
  // failure object if not.
  V8_WARN_UNUSED_RESULT AllocationResult AllocateRawSlowUnaligned(
      int size_in_bytes, AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Allocate the requested number of bytes in the space double aligned if
  // possible, return a failure object if not.
  V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRawSlowAligned(int size_in_bytes, AllocationAlignment alignment,
                         AllocationOrigin origin = AllocationOrigin::kRuntime);

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment,
                        AllocationOrigin origin, int* out_max_aligned_size);

  LinearAreaOriginalData& linear_area_original_data() {
    return linear_area_original_data_;
  }

  const LinearAreaOriginalData& linear_area_original_data() const {
    return linear_area_original_data_;
  }

  int ObjectAlignment() const;

  AllocationSpace identity() const;

  bool SupportsAllocationObserver() const { return !is_compaction_space(); }

  bool is_compaction_space() const {
    return compaction_space_kind_ != CompactionSpaceKind::kNone;
  }

  Heap* heap() const { return heap_; }

  Heap* heap_;
  SpaceWithLinearArea* space_;
  CompactionSpaceKind compaction_space_kind_;
  const SupportsExtendingLAB supports_extending_lab_;

  AllocationCounter allocation_counter_;
  LinearAllocationArea& allocation_info_;
  // This memory is used if no LinearAllocationArea& is passed in as argument.
  LinearAllocationArea owned_allocation_info_;
  LinearAreaOriginalData linear_area_original_data_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MAIN_ALLOCATOR_H_
