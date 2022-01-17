// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-allocator.h"

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/parked-scope.h"

namespace v8 {
namespace internal {

void StressConcurrentAllocatorTask::RunInternal() {
  Heap* heap = isolate_->heap();
  LocalHeap local_heap(heap, ThreadKind::kBackground);
  UnparkedScope unparked_scope(&local_heap);

  const int kNumIterations = 2000;
  const int kSmallObjectSize = 10 * kTaggedSize;
  const int kMediumObjectSize = 8 * KB;
  const int kLargeObjectSize =
      static_cast<int>(MemoryChunk::kPageSize -
                       MemoryChunkLayout::ObjectStartOffsetInDataPage());

  for (int i = 0; i < kNumIterations; i++) {
    // Isolate tear down started, stop allocation...
    if (heap->gc_state() == Heap::TEAR_DOWN) return;

    AllocationResult result = local_heap.AllocateRaw(
        kSmallObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
        AllocationAlignment::kWordAligned);
    if (!result.IsRetry()) {
      heap->CreateFillerObjectAtBackground(
          result.ToAddress(), kSmallObjectSize,
          ClearFreedMemoryMode::kDontClearFreedMemory);
    } else {
      local_heap.TryPerformCollection();
    }

    result = local_heap.AllocateRaw(kMediumObjectSize, AllocationType::kOld,
                                    AllocationOrigin::kRuntime,
                                    AllocationAlignment::kWordAligned);
    if (!result.IsRetry()) {
      heap->CreateFillerObjectAtBackground(
          result.ToAddress(), kMediumObjectSize,
          ClearFreedMemoryMode::kDontClearFreedMemory);
    } else {
      local_heap.TryPerformCollection();
    }

    result = local_heap.AllocateRaw(kLargeObjectSize, AllocationType::kOld,
                                    AllocationOrigin::kRuntime,
                                    AllocationAlignment::kWordAligned);
    if (!result.IsRetry()) {
      heap->CreateFillerObjectAtBackground(
          result.ToAddress(), kLargeObjectSize,
          ClearFreedMemoryMode::kDontClearFreedMemory);
    } else {
      local_heap.TryPerformCollection();
    }
    local_heap.Safepoint();
  }

  Schedule(isolate_);
}

// static
void StressConcurrentAllocatorTask::Schedule(Isolate* isolate) {
  auto task = std::make_unique<StressConcurrentAllocatorTask>(isolate);
  const double kDelayInSeconds = 0.1;
  V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(std::move(task),
                                                      kDelayInSeconds);
}

void ConcurrentAllocator::FreeLinearAllocationArea() {
  lab_.CloseAndMakeIterable();
}

void ConcurrentAllocator::MakeLinearAllocationAreaIterable() {
  lab_.MakeIterable();
}

void ConcurrentAllocator::MarkLinearAllocationAreaBlack() {
  Address top = lab_.top();
  Address limit = lab_.limit();

  if (top != kNullAddress && top != limit) {
    Page::FromAllocationAreaAddress(top)->CreateBlackAreaBackground(top, limit);
  }
}

void ConcurrentAllocator::UnmarkLinearAllocationArea() {
  Address top = lab_.top();
  Address limit = lab_.limit();

  if (top != kNullAddress && top != limit) {
    Page::FromAllocationAreaAddress(top)->DestroyBlackAreaBackground(top,
                                                                     limit);
  }
}

AllocationResult ConcurrentAllocator::AllocateInLabSlow(
    int object_size, AllocationAlignment alignment, AllocationOrigin origin) {
  if (!EnsureLab(origin)) {
    return AllocationResult::Retry(OLD_SPACE);
  }

  AllocationResult allocation = lab_.AllocateRawAligned(object_size, alignment);
  DCHECK(!allocation.IsRetry());

  return allocation;
}

bool ConcurrentAllocator::EnsureLab(AllocationOrigin origin) {
  auto result = space_->RawRefillLabBackground(
      local_heap_, kLabSize, kMaxLabSize, kWordAligned, origin);
  if (!result) return false;

  if (local_heap_->heap()->incremental_marking()->black_allocation()) {
    Address top = result->first;
    Address limit = top + result->second;
    Page::FromAllocationAreaAddress(top)->CreateBlackAreaBackground(top, limit);
  }

  HeapObject object = HeapObject::FromAddress(result->first);
  LocalAllocationBuffer saved_lab = std::move(lab_);
  lab_ = LocalAllocationBuffer::FromResult(
      local_heap_->heap(), AllocationResult(object), result->second);
  DCHECK(lab_.IsValid());
  if (!lab_.TryMerge(&saved_lab)) {
    saved_lab.CloseAndMakeIterable();
  }
  return true;
}

AllocationResult ConcurrentAllocator::AllocateOutsideLab(
    int object_size, AllocationAlignment alignment, AllocationOrigin origin) {
  auto result = space_->RawRefillLabBackground(local_heap_, object_size,
                                               object_size, alignment, origin);
  if (!result) return AllocationResult::Retry(OLD_SPACE);

  HeapObject object = HeapObject::FromAddress(result->first);

  if (local_heap_->heap()->incremental_marking()->black_allocation()) {
    local_heap_->heap()->incremental_marking()->MarkBlackBackground(
        object, object_size);
  }

  return AllocationResult(object);
}

}  // namespace internal
}  // namespace v8
