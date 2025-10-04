// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_ALLOCATOR_H_
#define V8_HEAP_HEAP_ALLOCATOR_H_

#include <optional>
#include <type_traits>

#include "include/v8config.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/heap/allocation-result.h"
#include "src/heap/main-allocator.h"

namespace v8 {
namespace internal {

class AllocationObserver;
class CodeLargeObjectSpace;
class LocalHeap;
class LinearAllocationArea;
class MainAllocator;
class NewSpace;
class NewLargeObjectSpace;
class OldLargeObjectSpace;
class PagedSpace;
class ReadOnlySpace;
class SharedTrustedLargeObjectSpace;
class Space;

// Allocator for the main thread. All exposed functions internally call the
// right bottleneck.
class V8_EXPORT_PRIVATE HeapAllocator final {
 public:
  explicit HeapAllocator(LocalHeap*);

  // Set up all LABs for this LocalHeap.
  void Setup();

  void SetReadOnlySpace(ReadOnlySpace*);

  // Supports all `AllocationType` types.
  //
  // Returns a failed result on an unsuccessful allocation attempt.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationType allocation,
              AllocationOrigin origin = AllocationOrigin::kRuntime,
              AllocationAlignment alignment = kTaggedAligned,
              AllocationHint hint = AllocationHint());

  // Supports all `AllocationType` types. Use when type is statically known.
  //
  // Returns a failed result on an unsuccessful allocation attempt.
  template <AllocationType type>
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult AllocateRaw(
      int size_in_bytes, AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kTaggedAligned,
      AllocationHint hint = AllocationHint());

  enum AllocationRetryMode { kLightRetry, kRetryOrFail };

  // Supports all `AllocationType` types and allows specifying retry handling.
  template <AllocationRetryMode mode>
  V8_WARN_UNUSED_RESULT V8_INLINE Tagged<HeapObject> AllocateRawWith(
      int size, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kTaggedAligned,
      AllocationHint hint = AllocationHint());

  // Returns true if a large object can be resized in-place to
  // |new_object_size|. On failure the return value is false.
  bool TryResizeLargeObject(Tagged<HeapObject> object, size_t old_object_size,
                            size_t new_object_size);

  V8_INLINE bool CanAllocateInReadOnlySpace() const;

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  void UpdateAllocationTimeout();
  // See `allocation_timeout_`.
  void SetAllocationTimeout(int allocation_timeout);

  static void SetAllocationGcInterval(int allocation_gc_interval);
  static void InitializeOncePerProcess();

  std::optional<int> get_allocation_timeout_for_testing() const {
    return allocation_timeout_;
  }
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

  // Give up all LABs. Used for e.g. full GCs.
  void FreeLinearAllocationAreas();

  // Make all LABs iterable.
  void MakeLinearAllocationAreasIterable();

#if DEBUG
  void VerifyLinearAllocationAreas() const;
#endif  // DEBUG

  // Mark/Unmark all LABs except for new and shared space. Use for black
  // allocation with sticky mark bits.
  void MarkLinearAllocationAreasBlack();
  void UnmarkLinearAllocationsArea();

  // Mark/Unmark linear allocation areas in shared heap black. Used for black
  // allocation with sticky mark bits.
  void MarkSharedLinearAllocationAreasBlack();
  void UnmarkSharedLinearAllocationAreas();

  // Free linear allocation areas and reset free-lists.
  void FreeLinearAllocationAreasAndResetFreeLists();
  void FreeSharedLinearAllocationAreasAndResetFreeLists();

  void PauseAllocationObservers();
  void ResumeAllocationObservers();

  void PublishPendingAllocations();

  void AddAllocationObserver(AllocationObserver* observer,
                             AllocationObserver* new_space_observer);
  void RemoveAllocationObserver(AllocationObserver* observer,
                                AllocationObserver* new_space_observer);

  MainAllocator* new_space_allocator() { return &new_space_allocator_.value(); }
  const MainAllocator* new_space_allocator() const {
    return &new_space_allocator_.value();
  }
  MainAllocator* old_space_allocator() { return &old_space_allocator_.value(); }
  MainAllocator* trusted_space_allocator() {
    return &trusted_space_allocator_.value();
  }
  MainAllocator* code_space_allocator() {
    return &code_space_allocator_.value();
  }
  MainAllocator* shared_space_allocator() {
    return &shared_space_allocator_.value();
  }

  template <typename AllocateFunction>
  V8_WARN_UNUSED_RESULT V8_INLINE auto CustomAllocateWithRetryOrFail(
      AllocateFunction&& Allocate, AllocationType allocation);

#if V8_VERIFY_WRITE_BARRIERS
  bool IsMostRecentYoungAllocation(Address object_address);
  void ResetMostRecentYoungAllocation();
#endif  // V8_VERIFY_WRITE_BARRIERS

  void set_last_young_allocation(Address value) {
    *last_young_allocation_pointer_ = value;
  }

  Address last_young_allocation() { return *last_young_allocation_pointer_; }

 private:
  V8_INLINE PagedSpace* code_space() const;
  V8_INLINE CodeLargeObjectSpace* code_lo_space() const;
  V8_INLINE NewSpace* new_space() const;
  V8_INLINE NewLargeObjectSpace* new_lo_space() const;
  V8_INLINE OldLargeObjectSpace* lo_space() const;
  V8_INLINE OldLargeObjectSpace* shared_lo_space() const;
  V8_INLINE OldLargeObjectSpace* shared_trusted_lo_space() const;
  V8_INLINE PagedSpace* old_space() const;
  V8_INLINE ReadOnlySpace* read_only_space() const;
  V8_INLINE PagedSpace* trusted_space() const;
  V8_INLINE OldLargeObjectSpace* trusted_lo_space() const;

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRawLargeInternal(
      int size_in_bytes, AllocationType allocation, AllocationOrigin origin,
      AllocationAlignment alignment, AllocationHint hint);

  template <typename AllocateFunction>
  V8_WARN_UNUSED_RESULT inline std::invoke_result_t<AllocateFunction>
  AllocateRawWithRetryOrFailSlowPath(AllocateFunction&& Allocate,
                                     AllocationType allocation);

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRawWithRetryOrFailSlowPath(
      int size, AllocationType allocation, AllocationOrigin origin,
      AllocationAlignment alignment, AllocationHint hint);

  template <typename AllocateFunction>
  V8_WARN_UNUSED_RESULT inline std::invoke_result_t<AllocateFunction>
  AllocateRawWithLightRetrySlowPath(AllocateFunction&& Allocate,
                                    AllocationType allocation);

  V8_WARN_UNUSED_RESULT AllocationResult AllocateRawWithLightRetrySlowPath(
      int size, AllocationType allocation, AllocationOrigin origin,
      AllocationAlignment alignment, AllocationHint hint);

  void CollectGarbage(AllocationType allocation,
                      PerformHeapLimitCheck perform_heap_limit_check =
                          PerformHeapLimitCheck::kYes);
  void CollectAllAvailableGarbage(AllocationType allocation);

  // Performs a GC and retries the allocation in a loop. The caller of this
  // method needs to perform the heap limit check.
  template <typename AllocateFunction>
  V8_WARN_UNUSED_RESULT std::invoke_result_t<AllocateFunction>
  CollectGarbageAndRetryAllocation(AllocateFunction&& Allocate,
                                   AllocationType allocation);

  // Performs the allocation but also sets additional flags to mark the
  // allocation as a "retry" of a failed allocation to make the allocation more
  // likely to succeed.
  template <typename AllocateFunction>
  V8_WARN_UNUSED_RESULT std::invoke_result_t<AllocateFunction> RetryAllocation(
      AllocateFunction&& Allocate);

  bool ReachedAllocationTimeout();

  Heap* heap_for_allocation(AllocationType allocation);

#ifdef DEBUG
  void IncrementObjectCounters();
#endif  // DEBUG

  LocalHeap* local_heap_;
  Heap* const heap_;
  Space* spaces_[LAST_SPACE + 1];
  ReadOnlySpace* read_only_space_;

  std::optional<MainAllocator> new_space_allocator_;
  std::optional<MainAllocator> old_space_allocator_;
  std::optional<MainAllocator> trusted_space_allocator_;
  std::optional<MainAllocator> code_space_allocator_;

  // Allocators for the shared spaces.
  std::optional<MainAllocator> shared_space_allocator_;
  std::optional<MainAllocator> shared_trusted_space_allocator_;
  OldLargeObjectSpace* shared_lo_space_;
  SharedTrustedLargeObjectSpace* shared_trusted_lo_space_;

  std::optional<Address> last_young_allocation_;
  Address* last_young_allocation_pointer_ = nullptr;

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  // Specifies how many allocations should be performed until returning
  // allocation failure (which will eventually lead to garbage collection).
  // Allocation will fail for any values <=0. See `UpdateAllocationTimeout()`
  // for how the new timeout is computed.
  std::optional<int> allocation_timeout_;

  // The configured GC interval, initialized from --gc-interval during
  // `InitializeOncePerProcess` and potentially dynamically updated by
  // `%SetAllocationTimeout()`.
  static std::atomic<int> allocation_gc_interval_;
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_ALLOCATOR_H_
