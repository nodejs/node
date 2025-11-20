// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MAIN_ALLOCATOR_H_
#define V8_HEAP_MAIN_ALLOCATOR_H_

#include <optional>

#include "src/common/globals.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/allocation-result.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/linear-allocation-area.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class Heap;
class LocalHeap;
class MainAllocator;
class PagedNewSpace;
class PagedSpaceBase;
class SemiSpaceNewSpace;
class SpaceWithLinearArea;

class AllocatorPolicy {
 public:
  explicit AllocatorPolicy(MainAllocator* allocator);
  virtual ~AllocatorPolicy() = default;

  // Sets up a linear allocation area that fits the given number of bytes.
  // Returns false if there is not enough space and the caller has to retry
  // after collecting garbage.
  // Writes to `max_aligned_size` the actual number of bytes used for checking
  // that there is enough space.
  virtual bool EnsureAllocation(int size_in_bytes,
                                AllocationAlignment alignment,
                                AllocationOrigin origin) = 0;
  virtual void FreeLinearAllocationArea() = 0;

  virtual bool SupportsExtendingLAB() const { return false; }

 protected:
  Heap* space_heap() const;
  Heap* isolate_heap() const;

  MainAllocator* const allocator_;
};

class SemiSpaceNewSpaceAllocatorPolicy final : public AllocatorPolicy {
 public:
  explicit SemiSpaceNewSpaceAllocatorPolicy(SemiSpaceNewSpace* space,
                                            MainAllocator* allocator)
      : AllocatorPolicy(allocator), space_(space) {}

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment,
                        AllocationOrigin origin) final;
  void FreeLinearAllocationArea() final;

 private:
  static constexpr int kLabSizeInGC = 32 * KB;

  void FreeLinearAllocationAreaUnsynchronized();

  SemiSpaceNewSpace* const space_;
};

class PagedSpaceAllocatorPolicy final : public AllocatorPolicy {
 public:
  PagedSpaceAllocatorPolicy(PagedSpaceBase* space, MainAllocator* allocator)
      : AllocatorPolicy(allocator), space_(space) {}

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment,
                        AllocationOrigin origin) final;
  void FreeLinearAllocationArea() final;

 private:
  bool RefillLab(int size_in_bytes, AllocationOrigin origin);

  // Returns true if allocation may be possible after sweeping.
  bool ContributeToSweeping(
      uint32_t max_pages = std::numeric_limits<uint32_t>::max());

  bool TryAllocationFromFreeList(size_t size_in_bytes, AllocationOrigin origin);

  bool TryExpandAndAllocate(size_t size_in_bytes, AllocationOrigin origin);

  V8_WARN_UNUSED_RESULT bool TryExtendLAB(int size_in_bytes);

  void SetLinearAllocationArea(Address top, Address limit, Address end);

  void FreeLinearAllocationAreaUnsynchronized();

  PagedSpaceBase* const space_;

  friend class PagedNewSpaceAllocatorPolicy;
};

class PagedNewSpaceAllocatorPolicy final : public AllocatorPolicy {
 public:
  PagedNewSpaceAllocatorPolicy(PagedNewSpace* space, MainAllocator* allocator);

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment,
                        AllocationOrigin origin) final;
  void FreeLinearAllocationArea() final;

  bool SupportsExtendingLAB() const final { return true; }

 private:
  bool TryAllocatePage(int size_in_bytes, AllocationOrigin origin);
  bool WaitForSweepingForAllocation(int size_in_bytes, AllocationOrigin origin);

  PagedNewSpace* const space_;
  std::unique_ptr<PagedSpaceAllocatorPolicy> paged_space_allocator_policy_;
};

class LinearAreaOriginalData {
 public:
  // Loads top and limit fields using the synchronization protocol without
  // holding the mutex lock. Note that in the returned (top, limit) pair, top
  // and limit might not actually belong together for brief moments when
  // changing LABs. It could happen that the LAB is invalid (limit<top) or that
  // the LAB is longer than needed (when old limit <= new limit). However, this
  // is fine for concurrent marking which would just push a few more objects to
  // the "on_hold" queue then necessary.
  //
  // If top and limit need to be consistent with respect to each other use
  // GetTopAndLimitLocked() instead.
  std::pair<Address, Address> GetTopAndLimit() const {
    // The order of the two loads is important. See SetTopAndLimit().
    auto top = original_top_.load(std::memory_order_acquire);
    auto limit = original_limit_.load(std::memory_order_relaxed);
    return std::make_pair(top, limit);
  }

  // Same as GetTopAndLimit() but also locks the mutex.
  std::pair<Address, Address> GetTopAndLimitLocked() const;

  void SetTopAndLimit(Address top, Address limit);

 private:
  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks. Protected by mutex_.
  std::atomic<Address> original_top_ = 0;
  std::atomic<Address> original_limit_ = 0;

  // Protects original_top_ and original_limit_.
  mutable base::Mutex mutex_;
};

class MainAllocator {
 public:
  struct InGCTag {};
  static constexpr InGCTag kInGC{};

  enum class IsNewGeneration { kNo, kYes };

  // Use this constructor on main/background threads. `allocation_info` can be
  // used for allocation support in generated code (currently new and old
  // space).
  V8_EXPORT_PRIVATE MainAllocator(
      LocalHeap* heap, SpaceWithLinearArea* space,
      IsNewGeneration is_new_generation,
      LinearAllocationArea* allocation_info = nullptr);

  // Use this constructor for GC LABs/allocations.
  V8_EXPORT_PRIVATE MainAllocator(Heap* heap, SpaceWithLinearArea* space,
                                  InGCTag);

  // Returns the allocation pointer in this space.
  Address start() const { return allocation_info_->start(); }
  Address top() const { return allocation_info_->top(); }
  Address limit() const { return allocation_info_->limit(); }

  // The allocation top address.
  Address* allocation_top_address() const {
    return allocation_info_->top_address();
  }

  // The allocation limit address.
  Address* allocation_limit_address() const {
    return allocation_info_->limit_address();
  }

  std::pair<Address, Address> GetOriginalTopAndLimit() const {
    return linear_area_original_data().GetTopAndLimit();
  }

  void MoveOriginalTopForward();
  V8_EXPORT_PRIVATE void ResetLab(Address start, Address end,
                                  Address extended_end);
  V8_EXPORT_PRIVATE bool IsPendingAllocation(Address object_address);

  LinearAllocationArea& allocation_info() { return *allocation_info_; }

  const LinearAllocationArea& allocation_info() const {
    return *allocation_info_;
  }

  AllocationCounter& allocation_counter() {
    return allocation_counter_.value();
  }

  const AllocationCounter& allocation_counter() const {
    return allocation_counter_.value();
  }

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationAlignment alignment,
              AllocationOrigin origin, AllocationHint hint);

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

  V8_EXPORT_PRIVATE void MakeLinearAllocationAreaIterable();

  V8_EXPORT_PRIVATE void MarkLinearAllocationAreaBlack();
  V8_EXPORT_PRIVATE void UnmarkLinearAllocationArea();
  V8_EXPORT_PRIVATE void FreeLinearAllocationAreaAndResetFreeList();

  V8_EXPORT_PRIVATE Address AlignTopForTesting(AllocationAlignment alignment,
                                               int offset);

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
  V8_INLINE bool IsLabValid() const {
    return allocation_info_->top() != kNullAddress;
  }

  V8_EXPORT_PRIVATE void FreeLinearAllocationArea();

  void ExtendLAB(Address limit);

  V8_EXPORT_PRIVATE bool EnsureAllocationForTesting(
      int size_in_bytes, AllocationAlignment alignment,
      AllocationOrigin origin);

 private:
  enum class BlackAllocation {
    kAlwaysEnabled,
    kAlwaysDisabled,
    kEnabledOnMarking
  };

  static constexpr BlackAllocation ComputeBlackAllocation(IsNewGeneration);

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
                        AllocationOrigin origin);

  void MarkLabStartInitialized();

  bool IsBlackAllocationEnabled() const;

  LinearAreaOriginalData& linear_area_original_data() {
    return linear_area_original_data_.value();
  }

  const LinearAreaOriginalData& linear_area_original_data() const {
    return linear_area_original_data_.value();
  }

  int ObjectAlignment() const;

  AllocationSpace identity() const;

  bool SupportsAllocationObserver() const {
    return allocation_counter_.has_value();
  }

  bool SupportsPendingAllocation() const {
    return linear_area_original_data_.has_value();
  }

  // Returns true when this LAB is used during GC.
  bool in_gc() const { return local_heap_ == nullptr; }

  // Returns true when this LAB is used during GC and the space is in the heap
  // that is currently collected. This is needed because a GC can directly
  // promote new space objects into shared space (which might not be currently
  // collected in worker isolates).
  bool in_gc_for_space() const;

  Address extended_limit() const { return extended_limit_; }

  bool supports_extending_lab() const { return supports_extending_lab_; }

  V8_EXPORT_PRIVATE bool is_main_thread() const;

  LocalHeap* local_heap() const { return local_heap_; }

  // The heap for the current thread (respectively LocalHeap). See comment for
  // `space_heap()` as well.
  Heap* isolate_heap() const { return isolate_heap_; }

  // Returns the space's heap. Note that this might differ from `isolate_heap()`
  // for shared space in worker isolates.
  V8_EXPORT_PRIVATE Heap* space_heap() const;

  // The current main or background thread's LocalHeap. nullptr for GC threads.
  LocalHeap* const local_heap_;
  Heap* const isolate_heap_;
  SpaceWithLinearArea* const space_;

  std::optional<AllocationCounter> allocation_counter_;
  LinearAllocationArea* const allocation_info_;
  // This memory is used if no LinearAllocationArea& is passed in as argument.
  LinearAllocationArea owned_allocation_info_;
  // Some spaces support "extending" of LABs (see supports_extending_lab()).
  // This helps to avoid fragmentation on pages due to LAB allocation. It is
  // also a fast path for allocation that avoids free list allocation.
  Address extended_limit_;

  std::optional<LinearAreaOriginalData> linear_area_original_data_;
  std::unique_ptr<AllocatorPolicy> allocator_policy_;

  const bool supports_extending_lab_;
  const BlackAllocation black_allocation_;

  friend class AllocatorPolicy;
  friend class PagedSpaceAllocatorPolicy;
  friend class SemiSpaceNewSpaceAllocatorPolicy;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MAIN_ALLOCATOR_H_
