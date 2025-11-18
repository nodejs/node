// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-allocator.h"

#include "src/base/functional/function-ref.h"
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/allocation-result.h"
#include "src/heap/heap-allocator-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/large-page-metadata.h"
#include "src/heap/large-spaces.h"
#include "src/heap/memory-chunk-metadata.h"
#include "src/heap/page-metadata.h"
#include "src/logging/counters.h"
#include "src/objects/heap-object.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

class Heap;

HeapAllocator::HeapAllocator(LocalHeap* local_heap)
    : local_heap_(local_heap), heap_(local_heap->heap()) {}

void HeapAllocator::Setup() {
  for (int i = FIRST_SPACE; i <= LAST_SPACE; ++i) {
    spaces_[i] = heap_->space(i);
  }

  if ((heap_->new_space() || v8_flags.sticky_mark_bits) &&
      local_heap_->is_main_thread()) {
    LinearAllocationArea* const new_allocation_info =
        &heap_->isolate()->isolate_data()->new_allocation_info();
    new_space_allocator_.emplace(
        local_heap_,
        v8_flags.sticky_mark_bits
            ? static_cast<SpaceWithLinearArea*>(heap_->sticky_space())
            : static_cast<SpaceWithLinearArea*>(heap_->new_space()),
        MainAllocator::IsNewGeneration::kYes, new_allocation_info);
  }

  if (local_heap_->is_main_thread()) {
    last_young_allocation_pointer_ = reinterpret_cast<Address*>(
        heap_->isolate()->isolate_data()->last_young_allocation_address());
  } else {
    last_young_allocation_.emplace(kNullAddress);
    last_young_allocation_pointer_ = &last_young_allocation_.value();
  }

  LinearAllocationArea* const old_allocation_info =
      local_heap_->is_main_thread()
          ? &heap_->isolate()->isolate_data()->old_allocation_info()
          : nullptr;
  old_space_allocator_.emplace(local_heap_, heap_->old_space(),
                               MainAllocator::IsNewGeneration::kNo,
                               old_allocation_info);

  trusted_space_allocator_.emplace(local_heap_, heap_->trusted_space(),
                                   MainAllocator::IsNewGeneration::kNo);
  code_space_allocator_.emplace(local_heap_, heap_->code_space(),
                                MainAllocator::IsNewGeneration::kNo);

  if (heap_->isolate()->has_shared_space()) {
    shared_space_allocator_.emplace(local_heap_,
                                    heap_->shared_allocation_space(),
                                    MainAllocator::IsNewGeneration::kNo);
    shared_lo_space_ = heap_->shared_lo_allocation_space();

    shared_trusted_space_allocator_.emplace(
        local_heap_, heap_->shared_trusted_allocation_space(),
        MainAllocator::IsNewGeneration::kNo);
    shared_trusted_lo_space_ = heap_->shared_trusted_lo_allocation_space();
  }
}

void HeapAllocator::SetReadOnlySpace(ReadOnlySpace* read_only_space) {
  read_only_space_ = read_only_space;
}

AllocationResult HeapAllocator::AllocateRawLargeInternal(
    int size_in_bytes, AllocationType allocation, AllocationOrigin origin,
    AllocationAlignment alignment, AllocationHint hint) {
  DCHECK_GT(size_in_bytes, heap_->MaxRegularHeapObjectSize(allocation));
  AllocationResult allocation_result;
  switch (allocation) {
    case AllocationType::kYoung:
      allocation_result =
          new_lo_space()->AllocateRaw(local_heap_, size_in_bytes, hint);
      break;
    case AllocationType::kOld:
      allocation_result =
          lo_space()->AllocateRaw(local_heap_, size_in_bytes, hint);
      break;
    case AllocationType::kCode:
      allocation_result =
          code_lo_space()->AllocateRaw(local_heap_, size_in_bytes, hint);
      break;
    case AllocationType::kSharedOld:
      allocation_result =
          shared_lo_space()->AllocateRaw(local_heap_, size_in_bytes, hint);
      break;
    case AllocationType::kTrusted:
      allocation_result =
          trusted_lo_space()->AllocateRaw(local_heap_, size_in_bytes, hint);
      break;
    case AllocationType::kSharedTrusted:
      allocation_result = shared_trusted_lo_space()->AllocateRaw(
          local_heap_, size_in_bytes, hint);
      break;
    case AllocationType::kMap:
    case AllocationType::kReadOnly:
    case AllocationType::kSharedMap:
      UNREACHABLE();
  }
  if (!allocation_result.IsFailure()) {
    int allocated_size = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
    heap_->AddTotalAllocatedBytes(allocated_size);
  }
  return allocation_result;
}

namespace {

constexpr AllocationSpace AllocationTypeToGCSpace(AllocationType type) {
  switch (type) {
    case AllocationType::kYoung:
      return NEW_SPACE;
    case AllocationType::kOld:
    case AllocationType::kCode:
    case AllocationType::kMap:
    case AllocationType::kTrusted:
    case AllocationType::kSharedMap:
    case AllocationType::kSharedOld:
      // OLD_SPACE indicates full GC.
      return OLD_SPACE;
    case AllocationType::kReadOnly:
    case AllocationType::kSharedTrusted:
      UNREACHABLE();
  }
}

}  // namespace

void HeapAllocator::CollectGarbage(
    AllocationType allocation, PerformHeapLimitCheck perform_heap_limit_check) {
  if (IsSharedAllocationType(allocation)) {
    auto* isolate = heap_->isolate();
    if (isolate->shared_space_isolate() == isolate &&
        local_heap_->is_main_thread()) {
      AllocationSpace space_to_gc = AllocationTypeToGCSpace(allocation);
      heap_->CollectGarbage(space_to_gc,
                            GarbageCollectionReason::kAllocationFailure,
                            kNoGCCallbackFlags, perform_heap_limit_check);
    } else {
      isolate->shared_space_isolate()
          ->heap()
          ->TriggerAndWaitForGCFromBackgroundThread(local_heap_,
                                                    RequestedGCKind::kMajor);
    }
  } else if (local_heap_->is_main_thread()) {
    // On the main thread we can directly start the GC.
    AllocationSpace space_to_gc = AllocationTypeToGCSpace(allocation);
    heap_->CollectGarbage(space_to_gc,
                          GarbageCollectionReason::kAllocationFailure,
                          kNoGCCallbackFlags, perform_heap_limit_check);
  } else {
    // Request GC from main thread.
    heap_->TriggerAndWaitForGCFromBackgroundThread(local_heap_,
                                                   RequestedGCKind::kMajor);
  }
}

void HeapAllocator::CollectAllAvailableGarbage(AllocationType allocation) {
  if (IsSharedAllocationType(allocation)) {
    auto* isolate = heap_->isolate();
    if (isolate->shared_space_isolate() == isolate &&
        local_heap_->is_main_thread()) {
      heap_->CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
    } else {
      isolate->shared_space_isolate()
          ->heap()
          ->TriggerAndWaitForGCFromBackgroundThread(
              local_heap_, RequestedGCKind::kLastResort);
    }
  } else if (local_heap_->is_main_thread()) {
    heap_->CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  } else {
    // Request GC from main thread.
    heap_->TriggerAndWaitForGCFromBackgroundThread(
        local_heap_, RequestedGCKind::kLastResort);
  }
}

Tagged<HeapObject> HeapAllocator::AllocateRawSlowPath(
    AllocationRetryMode retry_mode, int size, AllocationType allocation,
    AllocationOrigin origin, AllocationAlignment alignment,
    AllocationHint hint) {
  AllocationResult result;
  auto allocate = [&result, size, allocation, origin, alignment, hint, this]() {
    // Initially flags on the LocalHeap are always disabled. They are only
    // active while this method is running.
    DCHECK(!local_heap_->IsRetryOfFailedAllocation());
    local_heap_->SetRetryOfFailedAllocation(true);
    result = AllocateRaw(size, allocation, origin, alignment, hint);
    local_heap_->SetRetryOfFailedAllocation(false);
    return !result.IsFailure();
  };

  if (retry_mode == AllocationRetryMode::kLightRetry) {
    RetryCustomAllocateLight(allocate, allocation);
  } else {
    DCHECK_EQ(retry_mode, AllocationRetryMode::kRetryOrFail);
    RetryCustomAllocateOrFail(allocate, allocation);
  }

  Tagged<HeapObject> object;
  if (result.To(&object)) {
    return object;
  }
  return Tagged<HeapObject>();
}

bool HeapAllocator::TryResizeLargeObject(Tagged<HeapObject> object,
                                         size_t old_object_size,
                                         size_t new_object_size) {
  if (V8_UNLIKELY(!v8_flags.resize_large_object)) {
    return false;
  }

  PageMetadata* page = PageMetadata::FromHeapObject(object);
  Space* space = page->owner();
  if (space->identity() != NEW_LO_SPACE && space->identity() != LO_SPACE) {
    return false;
  }
  DCHECK(page->is_large());
  DCHECK_EQ(page->area_size(), old_object_size);
  CHECK_GT(new_object_size, old_object_size);
  if (!heap_->memory_allocator()->ResizeLargePage(
          LargePageMetadata::cast(page), old_object_size, new_object_size)) {
    if (V8_UNLIKELY(v8_flags.trace_resize_large_object)) {
      heap_->isolate()->PrintWithTimestamp(
          "resizing large object failed: allocation could not be extended\n");
    }
    return false;
  }

  LargeObjectSpace* large_space = static_cast<LargeObjectSpace*>(page->owner());
  large_space->UpdateAccountingAfterResizingObject(old_object_size,
                                                   new_object_size);
  return true;
}

void HeapAllocator::MakeLinearAllocationAreasIterable() {
  if (new_space_allocator_) {
    new_space_allocator_->MakeLinearAllocationAreaIterable();
  }
  old_space_allocator_->MakeLinearAllocationAreaIterable();
  trusted_space_allocator_->MakeLinearAllocationAreaIterable();
  code_space_allocator_->MakeLinearAllocationAreaIterable();

  if (shared_space_allocator_) {
    shared_space_allocator_->MakeLinearAllocationAreaIterable();
  }

  if (shared_trusted_space_allocator_) {
    shared_trusted_space_allocator_->MakeLinearAllocationAreaIterable();
  }
}

#if DEBUG
void HeapAllocator::VerifyLinearAllocationAreas() const {
  if (new_space_allocator_) {
    new_space_allocator_->Verify();
  }
  old_space_allocator_->Verify();
  trusted_space_allocator_->Verify();
  code_space_allocator_->Verify();

  if (shared_space_allocator_) {
    shared_space_allocator_->Verify();
  }

  if (shared_trusted_space_allocator_) {
    shared_trusted_space_allocator_->Verify();
  }
}
#endif  // DEBUG

void HeapAllocator::MarkLinearAllocationAreasBlack() {
  DCHECK(!v8_flags.black_allocated_pages);
  old_space_allocator_->MarkLinearAllocationAreaBlack();
  trusted_space_allocator_->MarkLinearAllocationAreaBlack();
  code_space_allocator_->MarkLinearAllocationAreaBlack();
}

void HeapAllocator::UnmarkLinearAllocationsArea() {
  DCHECK(!v8_flags.black_allocated_pages);
  old_space_allocator_->UnmarkLinearAllocationArea();
  trusted_space_allocator_->UnmarkLinearAllocationArea();
  code_space_allocator_->UnmarkLinearAllocationArea();
}

void HeapAllocator::MarkSharedLinearAllocationAreasBlack() {
  DCHECK(!v8_flags.black_allocated_pages);
  if (shared_space_allocator_) {
    shared_space_allocator_->MarkLinearAllocationAreaBlack();
  }
  if (shared_trusted_space_allocator_) {
    shared_trusted_space_allocator_->MarkLinearAllocationAreaBlack();
  }
}

void HeapAllocator::UnmarkSharedLinearAllocationAreas() {
  DCHECK(!v8_flags.black_allocated_pages);
  if (shared_space_allocator_) {
    shared_space_allocator_->UnmarkLinearAllocationArea();
  }
  if (shared_trusted_space_allocator_) {
    shared_trusted_space_allocator_->UnmarkLinearAllocationArea();
  }
}

void HeapAllocator::FreeLinearAllocationAreasAndResetFreeLists() {
  DCHECK(v8_flags.black_allocated_pages);
  old_space_allocator_->FreeLinearAllocationAreaAndResetFreeList();
  trusted_space_allocator_->FreeLinearAllocationAreaAndResetFreeList();
  code_space_allocator_->FreeLinearAllocationAreaAndResetFreeList();
}

void HeapAllocator::FreeSharedLinearAllocationAreasAndResetFreeLists() {
  DCHECK(v8_flags.black_allocated_pages);
  if (shared_space_allocator_) {
    shared_space_allocator_->FreeLinearAllocationAreaAndResetFreeList();
  }
  if (shared_trusted_space_allocator_) {
    shared_trusted_space_allocator_->FreeLinearAllocationAreaAndResetFreeList();
  }
}

void HeapAllocator::FreeLinearAllocationAreas() {
  if (new_space_allocator_) {
    new_space_allocator_->FreeLinearAllocationArea();
  }
  old_space_allocator_->FreeLinearAllocationArea();
  trusted_space_allocator_->FreeLinearAllocationArea();
  code_space_allocator_->FreeLinearAllocationArea();

  if (shared_space_allocator_) {
    shared_space_allocator_->FreeLinearAllocationArea();
  }

  if (shared_trusted_space_allocator_) {
    shared_trusted_space_allocator_->FreeLinearAllocationArea();
  }
}

void HeapAllocator::PublishPendingAllocations() {
  if (new_space_allocator_) {
    new_space_allocator_->MoveOriginalTopForward();
  }

  old_space_allocator_->MoveOriginalTopForward();
  trusted_space_allocator_->MoveOriginalTopForward();
  code_space_allocator_->MoveOriginalTopForward();

  lo_space()->ResetPendingObject();
  if (new_lo_space()) new_lo_space()->ResetPendingObject();
  code_lo_space()->ResetPendingObject();
  trusted_lo_space()->ResetPendingObject();
}

void HeapAllocator::AddAllocationObserver(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  if (new_space_allocator_) {
    new_space_allocator_->AddAllocationObserver(new_space_observer);
  }
  if (new_lo_space()) {
    new_lo_space()->AddAllocationObserver(new_space_observer);
  }
  old_space_allocator_->AddAllocationObserver(observer);
  lo_space()->AddAllocationObserver(observer);
  trusted_space_allocator_->AddAllocationObserver(observer);
  trusted_lo_space()->AddAllocationObserver(observer);
  code_space_allocator_->AddAllocationObserver(observer);
  code_lo_space()->AddAllocationObserver(observer);
}

void HeapAllocator::RemoveAllocationObserver(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  if (new_space_allocator_) {
    new_space_allocator_->RemoveAllocationObserver(new_space_observer);
  }
  if (new_lo_space()) {
    new_lo_space()->RemoveAllocationObserver(new_space_observer);
  }
  old_space_allocator_->RemoveAllocationObserver(observer);
  lo_space()->RemoveAllocationObserver(observer);
  trusted_space_allocator_->RemoveAllocationObserver(observer);
  trusted_lo_space()->RemoveAllocationObserver(observer);
  code_space_allocator_->RemoveAllocationObserver(observer);
  code_lo_space()->RemoveAllocationObserver(observer);
}

void HeapAllocator::PauseAllocationObservers() {
  if (new_space_allocator_) {
    new_space_allocator_->PauseAllocationObservers();
  }
  old_space_allocator_->PauseAllocationObservers();
  trusted_space_allocator_->PauseAllocationObservers();
  code_space_allocator_->PauseAllocationObservers();
}

void HeapAllocator::ResumeAllocationObservers() {
  if (new_space_allocator_) {
    new_space_allocator_->ResumeAllocationObservers();
  }
  old_space_allocator_->ResumeAllocationObservers();
  trusted_space_allocator_->ResumeAllocationObservers();
  code_space_allocator_->ResumeAllocationObservers();
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
  if (allocation_timeout > 0) {
    allocation_timeout_ = allocation_timeout;
  } else {
    allocation_timeout_.reset();
  }
}

void HeapAllocator::UpdateAllocationTimeout() {
  if (v8_flags.random_gc_interval > 0) {
    const int new_timeout = heap_->isolate()->fuzzer_rng()->NextInt(
        v8_flags.random_gc_interval + 1);
    // Reset the allocation timeout, but make sure to allow at least a few
    // allocations after a collection. The reason for this is that we have a lot
    // of allocation sequences and we assume that a garbage collection will
    // allow the subsequent allocation attempts to go through.
    constexpr int kFewAllocationsHeadroom = 6;
    int timeout = std::max(kFewAllocationsHeadroom, new_timeout);
    SetAllocationTimeout(timeout);
    DCHECK(allocation_timeout_.has_value());
    return;
  }

  int timeout = allocation_gc_interval_.load(std::memory_order_relaxed);
  SetAllocationTimeout(timeout);
}

bool HeapAllocator::ReachedAllocationTimeout() {
  DCHECK(allocation_timeout_.has_value());

  if (heap_->always_allocate() || local_heap_->IsRetryOfFailedAllocation()) {
    return false;
  }

  allocation_timeout_ = std::max(0, allocation_timeout_.value() - 1);
  return allocation_timeout_.value() <= 0;
}

#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

Heap* HeapAllocator::heap_for_allocation(AllocationType allocation) {
  if (IsSharedAllocationType(allocation)) {
    return heap_->isolate()->shared_space_isolate()->heap();
  } else {
    return heap_;
  }
}

bool HeapAllocator::RetryCustomAllocate(CustomAllocationFunction allocate,
                                        AllocationType allocation) {
  if (CollectGarbageAndRetryAllocation(allocate, allocation)) {
    return true;
  }

  // In the case of young allocations, the GCs above were minor GCs. Try "light"
  // full GCs before performing the last-resort GCs.
  if (allocation == AllocationType::kYoung) {
    if (CollectGarbageAndRetryAllocation(allocate, AllocationType::kOld)) {
      return true;
    }
  }

  // Perform last resort GC. This call will clear more caches and perform more
  // GCs. It will also enforce the heap limit if still violated.
  CollectAllAvailableGarbage(allocation);

  return allocate();
}

void HeapAllocator::RetryCustomAllocateOrFail(CustomAllocationFunction allocate,
                                              AllocationType allocation) {
  if (RetryCustomAllocate(allocate, allocation)) return;
  V8::FatalProcessOutOfMemory(heap_->isolate(), "CALL_AND_RETRY_LAST",
                              V8::kHeapOOM);
}

bool HeapAllocator::RetryCustomAllocateLight(CustomAllocationFunction allocate,
                                             AllocationType allocation) {
  DCHECK_NE(AllocationType::kYoung, allocation);

  if (auto result = CollectGarbageAndRetryAllocation(allocate, allocation)) {
    return result;
  }

  heap_for_allocation(allocation)->CheckHeapLimitReached();

  return {};
}

bool HeapAllocator::CollectGarbageAndRetryAllocation(
    CustomAllocationFunction allocate, AllocationType allocation) {
  const auto perform_heap_limit_check = v8_flags.late_heap_limit_check
                                            ? PerformHeapLimitCheck::kNo
                                            : PerformHeapLimitCheck::kYes;

  for (int i = 0; i < 2; i++) {
    if (v8_flags.ineffective_gcs_forces_last_resort &&
        allocation != AllocationType::kYoung &&
        heap_for_allocation(allocation)
            ->HasConsecutiveIneffectiveMarkCompact()) {
      return false;
    }

    // Skip the heap limit check in the GC if enabled. The heap limit needs to
    // be enforced by the caller.
    CollectGarbage(allocation, perform_heap_limit_check);

    // As long as we are at or above the heap limit, we definitely need another
    // GC.
    if (heap_for_allocation(allocation)->ReachedHeapLimit()) {
      continue;
    }

    if (allocate()) {
      return true;
    }
  }

  return false;
}

#if V8_VERIFY_WRITE_BARRIERS

bool HeapAllocator::IsMostRecentYoungAllocation(Address object_address) {
  const Address last = last_young_allocation();

  if (last == kNullAddress) {
    return false;
  }

  DCHECK(new_space_allocator_.has_value());

  if (new_space_allocator_->start() <= last &&
      last < new_space_allocator_->top()) {
    // The last young allocation was allocated from LAB. Because of allocation
    // folding we have to allow values between [last_young_allocation; LAB top[.
    return last <= object_address &&
           object_address < new_space_allocator_->top();
  } else {
    // Otherwise the last young allocation has to be a large object.
    MemoryChunkMetadata* chunk =
        MemoryChunkMetadata::FromAddress(heap_->isolate(), last);
    CHECK(chunk->is_large());
    CHECK_EQ(chunk->owner_identity(), NEW_LO_SPACE);
    // No allocation folding with large objects, so object_address has to match
    // the last young allocation exactly.
    return last == object_address;
  }
}

void HeapAllocator::ResetMostRecentYoungAllocation() {
  set_last_young_allocation(kNullAddress);
}

#endif  // V8_VERIFY_WRITE_BARRIERS

}  // namespace internal
}  // namespace v8
