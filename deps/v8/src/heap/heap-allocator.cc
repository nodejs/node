// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-allocator.h"

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-allocator-inl.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"

namespace v8 {
namespace internal {

class Heap;

HeapAllocator::HeapAllocator(Heap* heap) : heap_(heap) {}

void HeapAllocator::Setup() {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; ++i) {
    spaces_[i] = heap_->space(i);
  }

  space_for_maps_ = spaces_[MAP_SPACE]
                        ? static_cast<PagedSpace*>(spaces_[MAP_SPACE])
                        : static_cast<PagedSpace*>(spaces_[OLD_SPACE]);

  shared_old_allocator_ = heap_->shared_old_allocator_.get();
  shared_map_allocator_ = heap_->shared_map_allocator_
                              ? heap_->shared_map_allocator_.get()
                              : shared_old_allocator_;
}

void HeapAllocator::SetReadOnlySpace(ReadOnlySpace* read_only_space) {
  read_only_space_ = read_only_space;
}

AllocationResult HeapAllocator::AllocateRawLargeInternal(
    int size_in_bytes, AllocationType allocation, AllocationOrigin origin,
    AllocationAlignment alignment) {
  DCHECK_GT(size_in_bytes, heap_->MaxRegularHeapObjectSize(allocation));
  switch (allocation) {
    case AllocationType::kYoung:
      return new_lo_space()->AllocateRaw(size_in_bytes);
    case AllocationType::kOld:
      return lo_space()->AllocateRaw(size_in_bytes);
    case AllocationType::kCode:
      return code_lo_space()->AllocateRaw(size_in_bytes);
    case AllocationType::kMap:
    case AllocationType::kReadOnly:
    case AllocationType::kSharedMap:
    case AllocationType::kSharedOld:
      UNREACHABLE();
  }
}

namespace {

constexpr AllocationSpace AllocationTypeToGCSpace(AllocationType type) {
  switch (type) {
    case AllocationType::kYoung:
      return NEW_SPACE;
    case AllocationType::kOld:
    case AllocationType::kCode:
    case AllocationType::kMap:
      // OLD_SPACE indicates full GC.
      return OLD_SPACE;
    case AllocationType::kReadOnly:
    case AllocationType::kSharedMap:
    case AllocationType::kSharedOld:
      UNREACHABLE();
  }
}

}  // namespace

AllocationResult HeapAllocator::AllocateRawWithLightRetrySlowPath(
    int size, AllocationType allocation, AllocationOrigin origin,
    AllocationAlignment alignment) {
  AllocationResult result = AllocateRaw(size, allocation, origin, alignment);
  if (!result.IsFailure()) {
    return result;
  }

  // Two GCs before returning failure.
  for (int i = 0; i < 2; i++) {
    if (IsSharedAllocationType(allocation)) {
      heap_->CollectSharedGarbage(GarbageCollectionReason::kAllocationFailure);
    } else {
      heap_->CollectGarbage(AllocationTypeToGCSpace(allocation),
                            GarbageCollectionReason::kAllocationFailure);
    }
    result = AllocateRaw(size, allocation, origin, alignment);
    if (!result.IsFailure()) {
      return result;
    }
  }
  return result;
}

AllocationResult HeapAllocator::AllocateRawWithRetryOrFailSlowPath(
    int size, AllocationType allocation, AllocationOrigin origin,
    AllocationAlignment alignment) {
  AllocationResult result =
      AllocateRawWithLightRetrySlowPath(size, allocation, origin, alignment);
  if (!result.IsFailure()) return result;

  heap_->isolate()->counters()->gc_last_resort_from_handles()->Increment();
  if (IsSharedAllocationType(allocation)) {
    heap_->CollectSharedGarbage(GarbageCollectionReason::kLastResort);

    // We need always_allocate() to be true both on the client- and
    // server-isolate. It is used in both code paths.
    AlwaysAllocateScope shared_scope(
        heap_->isolate()->shared_isolate()->heap());
    AlwaysAllocateScope client_scope(heap_);
    result = AllocateRaw(size, allocation, origin, alignment);
  } else {
    heap_->CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);

    AlwaysAllocateScope scope(heap_);
    result = AllocateRaw(size, allocation, origin, alignment);
  }

  if (!result.IsFailure()) {
    return result;
  }

  v8::internal::V8::FatalProcessOutOfMemory(heap_->isolate(),
                                            "CALL_AND_RETRY_LAST", true);
}

#ifdef DEBUG

void HeapAllocator::IncrementObjectCounters() {
  heap_->isolate()->counters()->objs_since_last_full()->Increment();
  heap_->isolate()->counters()->objs_since_last_young()->Increment();
}

#endif  // DEBUG

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT

void HeapAllocator::SetAllocationTimeout(int allocation_timeout) {
  allocation_timeout_ = allocation_timeout;
}

void HeapAllocator::UpdateAllocationTimeout() {
  if (FLAG_random_gc_interval > 0) {
    const int new_timeout = allocation_timeout_ <= 0
                                ? heap_->isolate()->fuzzer_rng()->NextInt(
                                      FLAG_random_gc_interval + 1)
                                : allocation_timeout_;
    // Reset the allocation timeout, but make sure to allow at least a few
    // allocations after a collection. The reason for this is that we have a lot
    // of allocation sequences and we assume that a garbage collection will
    // allow the subsequent allocation attempts to go through.
    constexpr int kFewAllocationsHeadroom = 6;
    allocation_timeout_ = std::max(kFewAllocationsHeadroom, new_timeout);
  } else if (FLAG_gc_interval >= 0) {
    allocation_timeout_ = FLAG_gc_interval;
  }
}

#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

}  // namespace internal
}  // namespace v8
