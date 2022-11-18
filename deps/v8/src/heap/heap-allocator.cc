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

  shared_old_allocator_ = heap_->shared_space_allocator_.get();
  shared_map_allocator_ = heap_->shared_map_allocator_
                              ? heap_->shared_map_allocator_.get()
                              : shared_old_allocator_;
  shared_lo_space_ = heap_->shared_lo_allocation_space();
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
    case AllocationType::kSharedOld:
      return shared_lo_space()->AllocateRawBackground(
          heap_->main_thread_local_heap(), size_in_bytes);
    case AllocationType::kMap:
    case AllocationType::kReadOnly:
    case AllocationType::kSharedMap:
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
      heap_->CollectGarbageShared(heap_->main_thread_local_heap(),
                                  GarbageCollectionReason::kAllocationFailure);
    } else {
      AllocationSpace space_to_gc = AllocationTypeToGCSpace(allocation);
      if (v8_flags.minor_mc && i > 0) {
        // Repeated young gen GCs won't have any additional effect. Do a full GC
        // instead.
        space_to_gc = AllocationSpace::OLD_SPACE;
      }
      heap_->CollectGarbage(space_to_gc,
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

  if (IsSharedAllocationType(allocation)) {
    heap_->CollectGarbageShared(heap_->main_thread_local_heap(),
                                GarbageCollectionReason::kLastResort);

    // We need always_allocate() to be true both on the client- and
    // server-isolate. It is used in both code paths.
    AlwaysAllocateScope shared_scope(
        heap_->isolate()->shared_heap_isolate()->heap());
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

  V8::FatalProcessOutOfMemory(heap_->isolate(), "CALL_AND_RETRY_LAST",
                              V8::kHeapOOM);
}

#ifdef DEBUG

void HeapAllocator::IncrementObjectCounters() {
  heap_->isolate()->counters()->objs_since_last_full()->Increment();
  heap_->isolate()->counters()->objs_since_last_young()->Increment();
}

#endif  // DEBUG

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
// static
void HeapAllocator::InitializeOncePerProcess() {
  SetAllocationGcInterval(v8_flags.gc_interval);
}

// static
void HeapAllocator::SetAllocationGcInterval(int allocation_gc_interval) {
  allocation_gc_interval_.store(allocation_gc_interval,
                                std::memory_order_relaxed);
}

// static
std::atomic<int> HeapAllocator::allocation_gc_interval_{-1};

void HeapAllocator::SetAllocationTimeout(int allocation_timeout) {
  // See `allocation_timeout_` for description. We map negative values to 0 to
  // avoid underflows as allocation decrements this value as well.
  allocation_timeout_ = std::max(0, allocation_timeout);
}

void HeapAllocator::UpdateAllocationTimeout() {
  if (v8_flags.random_gc_interval > 0) {
    const int new_timeout = allocation_timeout_ <= 0
                                ? heap_->isolate()->fuzzer_rng()->NextInt(
                                      v8_flags.random_gc_interval + 1)
                                : allocation_timeout_;
    // Reset the allocation timeout, but make sure to allow at least a few
    // allocations after a collection. The reason for this is that we have a lot
    // of allocation sequences and we assume that a garbage collection will
    // allow the subsequent allocation attempts to go through.
    constexpr int kFewAllocationsHeadroom = 6;
    allocation_timeout_ = std::max(kFewAllocationsHeadroom, new_timeout);
    return;
  }

  int interval = allocation_gc_interval_.load(std::memory_order_relaxed);
  if (interval >= 0) {
    allocation_timeout_ = interval;
  }
}

#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

}  // namespace internal
}  // namespace v8
