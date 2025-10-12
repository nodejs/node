// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_ALLOCATOR_INL_H_
#define V8_HEAP_HEAP_ALLOCATOR_INL_H_

#include "src/heap/heap-allocator.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/logging.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/heap/large-spaces.h"
#include "src/heap/local-heap.h"
#include "src/heap/main-allocator-inl.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/zapping.h"

namespace v8 {
namespace internal {

PagedSpace* HeapAllocator::code_space() const {
  return static_cast<PagedSpace*>(spaces_[CODE_SPACE]);
}

CodeLargeObjectSpace* HeapAllocator::code_lo_space() const {
  return static_cast<CodeLargeObjectSpace*>(spaces_[CODE_LO_SPACE]);
}

OldLargeObjectSpace* HeapAllocator::lo_space() const {
  return static_cast<OldLargeObjectSpace*>(spaces_[LO_SPACE]);
}

OldLargeObjectSpace* HeapAllocator::shared_lo_space() const {
  return shared_lo_space_;
}

NewSpace* HeapAllocator::new_space() const {
  return static_cast<NewSpace*>(spaces_[NEW_SPACE]);
}

NewLargeObjectSpace* HeapAllocator::new_lo_space() const {
  return static_cast<NewLargeObjectSpace*>(spaces_[NEW_LO_SPACE]);
}

PagedSpace* HeapAllocator::old_space() const {
  return static_cast<PagedSpace*>(spaces_[OLD_SPACE]);
}

ReadOnlySpace* HeapAllocator::read_only_space() const {
  return read_only_space_;
}

PagedSpace* HeapAllocator::trusted_space() const {
  return static_cast<PagedSpace*>(spaces_[TRUSTED_SPACE]);
}

OldLargeObjectSpace* HeapAllocator::trusted_lo_space() const {
  return static_cast<OldLargeObjectSpace*>(spaces_[TRUSTED_LO_SPACE]);
}

OldLargeObjectSpace* HeapAllocator::shared_trusted_lo_space() const {
  return shared_trusted_lo_space_;
}

bool HeapAllocator::CanAllocateInReadOnlySpace() const {
  return read_only_space()->writable();
}

template <AllocationType type>
V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
HeapAllocator::AllocateRaw(int size_in_bytes, AllocationOrigin origin,
                           AllocationAlignment alignment, AllocationHint hint) {
  DCHECK(!heap_->IsInGC());
  DCHECK(AllowHandleAllocation::IsAllowed());
  DCHECK(AllowHeapAllocation::IsAllowed());
  CHECK(AllowHeapAllocationInRelease::IsAllowed());
  DCHECK(local_heap_->IsRunning());
  // We need to have entered the isolate before allocating.
  DCHECK_EQ(heap_->isolate(), Isolate::TryGetCurrent());
#if DEBUG
  local_heap_->VerifyCurrent();
#endif  // DEBUG

#if V8_VERIFY_WRITE_BARRIERS
  local_heap_->AssertNoWriteBarrierModeScope();
#endif  // V8_VERIFY_WRITE_BARRIERS

  if (v8_flags.single_generation.value() && type == AllocationType::kYoung) {
    return AllocateRaw(size_in_bytes, AllocationType::kOld, origin, alignment);
  }

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  if (V8_UNLIKELY(allocation_timeout_.has_value()) &&
      ReachedAllocationTimeout()) {
    return AllocationResult::Failure();
  }
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

#ifdef DEBUG
  IncrementObjectCounters();
#endif  // DEBUG

  if (heap_->CanSafepoint()) {
    local_heap_->Safepoint();
  }

  const size_t large_object_threshold = heap_->MaxRegularHeapObjectSize(type);
  const bool large_object =
      static_cast<size_t>(size_in_bytes) > large_object_threshold;

  Tagged<HeapObject> object;
  AllocationResult allocation;

  if (V8_UNLIKELY(large_object)) {
    allocation =
        AllocateRawLargeInternal(size_in_bytes, type, origin, alignment, hint);
  } else {
    switch (type) {
      case AllocationType::kYoung:
        allocation = new_space_allocator_->AllocateRaw(size_in_bytes, alignment,
                                                       origin, hint);
        break;
      case AllocationType::kMap:
      case AllocationType::kOld:
        allocation = old_space_allocator_->AllocateRaw(size_in_bytes, alignment,
                                                       origin, hint);
        DCHECK_IMPLIES(v8_flags.sticky_mark_bits && !allocation.IsFailure(),
                       heap_->marking_state()->IsMarked(allocation.ToObject()));
        break;
      case AllocationType::kCode: {
        DCHECK_EQ(alignment, AllocationAlignment::kTaggedAligned);
        DCHECK(AllowCodeAllocation::IsAllowed());
        allocation = code_space_allocator_->AllocateRaw(
            size_in_bytes, AllocationAlignment::kTaggedAligned, origin, hint);
        break;
      }
      case AllocationType::kReadOnly:
        DCHECK(read_only_space()->writable());
        DCHECK_EQ(AllocationOrigin::kRuntime, origin);
        allocation = read_only_space()->AllocateRaw(size_in_bytes, alignment);
        break;
      case AllocationType::kSharedMap:
      case AllocationType::kSharedOld:
        allocation = shared_space_allocator_->AllocateRaw(
            size_in_bytes, alignment, origin, hint);
        break;
      case AllocationType::kTrusted:
        allocation = trusted_space_allocator_->AllocateRaw(
            size_in_bytes, alignment, origin, hint);
        break;
      case AllocationType::kSharedTrusted:
        allocation = shared_trusted_space_allocator_->AllocateRaw(
            size_in_bytes, alignment, origin, hint);
        break;
    }
  }

#if V8_VERIFY_WRITE_BARRIERS
  if (type == AllocationType::kYoung && !allocation.IsFailure()) {
    set_last_young_allocation(allocation.ToAddress());
  } else {
    set_last_young_allocation(kNullAddress);
  }
#endif  // V8_VERIFY_WRITE_BARRIERS

  if (allocation.To(&object)) {
    if (heap::ShouldZapGarbage() && AllocationType::kCode == type) {
      heap::ZapCodeBlock(object.address(), size_in_bytes);
    }

    if (local_heap_->is_main_thread()) {
      for (auto& tracker : heap_->allocation_trackers_) {
        tracker->AllocationEvent(object.address(), size_in_bytes);
      }
    }
  }

  return allocation;
}

AllocationResult HeapAllocator::AllocateRaw(int size_in_bytes,
                                            AllocationType type,
                                            AllocationOrigin origin,
                                            AllocationAlignment alignment,
                                            AllocationHint hint) {
  switch (type) {
    case AllocationType::kYoung:
      return AllocateRaw<AllocationType::kYoung>(size_in_bytes, origin,
                                                 alignment, hint);
    case AllocationType::kOld:
      return AllocateRaw<AllocationType::kOld>(size_in_bytes, origin, alignment,
                                               hint);
    case AllocationType::kCode:
      return AllocateRaw<AllocationType::kCode>(size_in_bytes, origin,
                                                alignment, hint);
    case AllocationType::kMap:
      return AllocateRaw<AllocationType::kMap>(size_in_bytes, origin, alignment,
                                               hint);
    case AllocationType::kReadOnly:
      return AllocateRaw<AllocationType::kReadOnly>(size_in_bytes, origin,
                                                    alignment, hint);
    case AllocationType::kSharedMap:
      return AllocateRaw<AllocationType::kSharedMap>(size_in_bytes, origin,
                                                     alignment, hint);
    case AllocationType::kSharedOld:
      return AllocateRaw<AllocationType::kSharedOld>(size_in_bytes, origin,
                                                     alignment, hint);
    case AllocationType::kTrusted:
      return AllocateRaw<AllocationType::kTrusted>(size_in_bytes, origin,
                                                   alignment, hint);
    case AllocationType::kSharedTrusted:
      return AllocateRaw<AllocationType::kSharedTrusted>(size_in_bytes, origin,
                                                         alignment, hint);
  }
  UNREACHABLE();
}

template <HeapAllocator::AllocationRetryMode mode>
V8_WARN_UNUSED_RESULT V8_INLINE Tagged<HeapObject>
HeapAllocator::AllocateRawWith(int size, AllocationType allocation,
                               AllocationOrigin origin,
                               AllocationAlignment alignment,
                               AllocationHint hint) {
  AllocationResult result;
  Tagged<HeapObject> object;
  size = ALIGN_TO_ALLOCATION_ALIGNMENT(size);
  if (allocation == AllocationType::kYoung) {
    result = AllocateRaw<AllocationType::kYoung>(size, origin, alignment, hint);
    if (result.To(&object)) {
      return object;
    }
  } else if (allocation == AllocationType::kOld) {
    result = AllocateRaw<AllocationType::kOld>(size, origin, alignment, hint);
    if (result.To(&object)) {
      return object;
    }
  }
  switch (mode) {
    case kLightRetry:
      result = AllocateRawWithLightRetrySlowPath(size, allocation, origin,
                                                 alignment, hint);
      break;
    case kRetryOrFail:
      result = AllocateRawWithRetryOrFailSlowPath(size, allocation, origin,
                                                  alignment, hint);
      break;
  }
  if (result.To(&object)) {
    return object;
  }
  return HeapObject();
}

template <typename AllocateFunction>
V8_WARN_UNUSED_RESULT std::invoke_result_t<AllocateFunction>
HeapAllocator::AllocateRawWithLightRetrySlowPath(AllocateFunction&& Allocate,
                                                 AllocationType allocation) {
  DCHECK_NE(AllocationType::kYoung, allocation);

  if (auto result = Allocate()) [[likely]] {
    return result;
  }

  if (auto result = CollectGarbageAndRetryAllocation(Allocate, allocation)) {
    return result;
  }

  heap_for_allocation(allocation)->CheckHeapLimitReached();

  return {};
}

template <typename AllocateFunction>
std::invoke_result_t<AllocateFunction>
HeapAllocator::AllocateRawWithRetryOrFailSlowPath(AllocateFunction&& Allocate,
                                                  AllocationType allocation) {
  if (auto result = Allocate()) [[likely]] {
    return result;
  }

  if (auto result = CollectGarbageAndRetryAllocation(Allocate, allocation)) {
    return result;
  }

  // In the case of young allocations, the GCs above were minor GCs. Try "light"
  // full GCs before performing the last-resort GCs.
  if (allocation == AllocationType::kYoung) {
    if (auto result =
            CollectGarbageAndRetryAllocation(Allocate, AllocationType::kOld)) {
      return result;
    }
  }

  // Perform last resort GC. This call will clear more caches and perform more
  // GCs. It will also enforce the heap limit if still violated.
  CollectAllAvailableGarbage(allocation);

  if (auto result = RetryAllocation(Allocate)) {
    return result;
  }

  V8::FatalProcessOutOfMemory(heap_->isolate(), "CALL_AND_RETRY_LAST",
                              V8::kHeapOOM);
}

template <typename AllocateFunction>
std::invoke_result_t<AllocateFunction>
HeapAllocator::CollectGarbageAndRetryAllocation(AllocateFunction&& Allocate,
                                                AllocationType allocation) {
  const auto perform_heap_limit_check = v8_flags.late_heap_limit_check
                                            ? PerformHeapLimitCheck::kNo
                                            : PerformHeapLimitCheck::kYes;

  for (int i = 0; i < 2; i++) {
    // Skip the heap limit check in the GC if enabled. The heap limit needs to
    // be enforced by the caller.
    CollectGarbage(allocation, perform_heap_limit_check);

    // As long as we are at or above the heap limit, we definitely need another
    // GC.
    if (heap_for_allocation(allocation)->ReachedHeapLimit()) {
      continue;
    }

    if (auto result = RetryAllocation(Allocate)) {
      return result;
    }
  }

  return {};
}

template <typename AllocateFunction>
std::invoke_result_t<AllocateFunction> HeapAllocator::RetryAllocation(
    AllocateFunction&& Allocate) {
  // Initially flags on the LocalHeap are always disabled. They are only
  // active while this method is running.
  DCHECK(!local_heap_->IsRetryOfFailedAllocation());
  local_heap_->SetRetryOfFailedAllocation(true);
  auto result = Allocate();
  local_heap_->SetRetryOfFailedAllocation(false);
  return result;
}

template <typename AllocateFunction>
V8_WARN_UNUSED_RESULT auto HeapAllocator::CustomAllocateWithRetryOrFail(
    AllocateFunction&& Allocate, AllocationType allocation) {
  return *AllocateRawWithRetryOrFailSlowPath(Allocate, allocation);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_ALLOCATOR_INL_H_
