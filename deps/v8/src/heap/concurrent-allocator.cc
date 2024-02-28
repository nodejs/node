// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-allocator.h"

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/heap.h"
#include "src/heap/linear-allocation-area.h"
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
        AllocationAlignment::kTaggedAligned);
    if (!result.IsFailure()) {
      heap->CreateFillerObjectAtBackground(result.ToAddress(),
                                           kSmallObjectSize);
    } else {
      heap->CollectGarbageFromAnyThread(&local_heap);
    }

    result = local_heap.AllocateRaw(kMediumObjectSize, AllocationType::kOld,
                                    AllocationOrigin::kRuntime,
                                    AllocationAlignment::kTaggedAligned);
    if (!result.IsFailure()) {
      heap->CreateFillerObjectAtBackground(result.ToAddress(),
                                           kMediumObjectSize);
    } else {
      heap->CollectGarbageFromAnyThread(&local_heap);
    }

    result = local_heap.AllocateRaw(kLargeObjectSize, AllocationType::kOld,
                                    AllocationOrigin::kRuntime,
                                    AllocationAlignment::kTaggedAligned);
    if (!result.IsFailure()) {
      heap->CreateFillerObjectAtBackground(result.ToAddress(),
                                           kLargeObjectSize);
    } else {
      heap->CollectGarbageFromAnyThread(&local_heap);
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

ConcurrentAllocator::ConcurrentAllocator(LocalHeap* local_heap,
                                         PagedSpace* space, Context context)
    : local_heap_(local_heap),
      space_(space),
      owning_heap_(space_->heap()),
      context_(context) {
  DCHECK_IMPLIES(!local_heap_, context_ == Context::kGC);
}

void ConcurrentAllocator::FreeLinearAllocationArea() {
  if (IsLabValid() && lab_.top() != lab_.limit()) {
    base::Optional<CodePageHeaderModificationScope> optional_scope;
    if (space_->identity() == CODE_SPACE) {
      optional_scope.emplace("Clears marking bitmap in the page header.");
    }

    Page* page = Page::FromAddress(lab_.top());

    if (IsBlackAllocationEnabled()) {
      page->DestroyBlackArea(lab_.top(), lab_.limit());
    }

    // When starting incremental marking free lists are dropped for evacuation
    // candidates. So for evacuation candidates we just make the free memory
    // iterable.
    CHECK(!page->IsEvacuationCandidate());
    base::MutexGuard guard(space_->mutex());
    space_->Free(lab_.top(), lab_.limit() - lab_.top(),
                 SpaceAccountingMode::kSpaceAccounted);
  }

  ResetLab();
}

void ConcurrentAllocator::MakeLinearAllocationAreaIterable() {
  MakeLabIterable();
}

void ConcurrentAllocator::MarkLinearAllocationAreaBlack() {
  Address top = lab_.top();
  Address limit = lab_.limit();

  if (top != kNullAddress && top != limit) {
    base::Optional<CodePageHeaderModificationScope> optional_rwx_write_scope;
    if (space_->identity() == CODE_SPACE) {
      optional_rwx_write_scope.emplace(
          "Marking InstructionStream objects requires write access to the "
          "Code page header");
    }
    Page::FromAllocationAreaAddress(top)->CreateBlackArea(top, limit);
  }
}

void ConcurrentAllocator::UnmarkLinearAllocationArea() {
  Address top = lab_.top();
  Address limit = lab_.limit();

  if (top != kNullAddress && top != limit) {
    base::Optional<CodePageHeaderModificationScope> optional_rwx_write_scope;
    if (space_->identity() == CODE_SPACE) {
      optional_rwx_write_scope.emplace(
          "Marking InstructionStream objects requires write access to the "
          "Code page header");
    }
    Page::FromAllocationAreaAddress(top)->DestroyBlackArea(top, limit);
  }
}

AllocationResult ConcurrentAllocator::AllocateInLabSlow(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  if (!AllocateLab(origin)) {
    return AllocationResult::Failure();
  }
  AllocationResult allocation =
      AllocateInLabFastAligned(size_in_bytes, alignment);
  DCHECK(!allocation.IsFailure());
  return allocation;
}

bool ConcurrentAllocator::AllocateLab(AllocationOrigin origin) {
  auto result = AllocateFromSpaceFreeList(kMinLabSize, kMaxLabSize, origin);
  if (!result) return false;

  owning_heap()->StartIncrementalMarkingIfAllocationLimitIsReachedBackground();

  FreeLinearAllocationArea();

  Address lab_start = result->first;
  Address lab_end = lab_start + result->second;
  lab_ = LinearAllocationArea(lab_start, lab_end);
  DCHECK(IsLabValid());

  if (IsBlackAllocationEnabled()) {
    Address top = lab_.top();
    Address limit = lab_.limit();
    Page::FromAllocationAreaAddress(top)->CreateBlackArea(top, limit);
  }

  return true;
}

base::Optional<std::pair<Address, size_t>>
ConcurrentAllocator::AllocateFromSpaceFreeList(size_t min_size_in_bytes,
                                               size_t max_size_in_bytes,
                                               AllocationOrigin origin) {
  DCHECK(!space_->is_compaction_space());
  DCHECK(space_->identity() == OLD_SPACE || space_->identity() == CODE_SPACE ||
         space_->identity() == SHARED_SPACE ||
         space_->identity() == TRUSTED_SPACE);
  DCHECK(origin == AllocationOrigin::kRuntime ||
         origin == AllocationOrigin::kGC);
  DCHECK_IMPLIES(!local_heap_, origin == AllocationOrigin::kGC);

  base::Optional<std::pair<Address, size_t>> result =
      TryFreeListAllocation(min_size_in_bytes, max_size_in_bytes, origin);
  if (result) return result;

  uint64_t trace_flow_id = owning_heap()->sweeper()->GetTraceIdForFlowEvent(
      GCTracer::Scope::MC_BACKGROUND_SWEEPING);
  // Sweeping is still in progress.
  if (owning_heap()->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    {
      TRACE_GC_EPOCH_WITH_FLOW(
          owning_heap()->tracer(), GCTracer::Scope::MC_BACKGROUND_SWEEPING,
          ThreadKind::kBackground, trace_flow_id,
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
      space_->RefillFreeList();
    }

    // Retry the free list allocation.
    result =
        TryFreeListAllocation(min_size_in_bytes, max_size_in_bytes, origin);
    if (result) return result;

    if (owning_heap()->major_sweeping_in_progress()) {
      // Now contribute to sweeping from background thread and then try to
      // reallocate.
      int max_freed;
      {
        TRACE_GC_EPOCH_WITH_FLOW(
            owning_heap()->tracer(), GCTracer::Scope::MC_BACKGROUND_SWEEPING,
            ThreadKind::kBackground, trace_flow_id,
            TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
        const int kMaxPagesToSweep = 1;
        max_freed = owning_heap()->sweeper()->ParallelSweepSpace(
            space_->identity(), Sweeper::SweepingMode::kLazyOrConcurrent,
            static_cast<int>(min_size_in_bytes), kMaxPagesToSweep);
        space_->RefillFreeList();
      }

      if (static_cast<size_t>(max_freed) >= min_size_in_bytes) {
        result =
            TryFreeListAllocation(min_size_in_bytes, max_size_in_bytes, origin);
        if (result) return result;
      }
    }
  }

  if (owning_heap()->ShouldExpandOldGenerationOnSlowAllocation(local_heap_,
                                                               origin) &&
      owning_heap()->CanExpandOldGeneration(space_->AreaSize())) {
    result = space_->TryExpandBackground(max_size_in_bytes);
    if (result) return result;
  }

  if (owning_heap()->major_sweeping_in_progress()) {
    // Complete sweeping for this space.
    TRACE_GC_EPOCH_WITH_FLOW(
        owning_heap()->tracer(), GCTracer::Scope::MC_BACKGROUND_SWEEPING,
        ThreadKind::kBackground, trace_flow_id,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
    owning_heap()->DrainSweepingWorklistForSpace(space_->identity());

    space_->RefillFreeList();

    // Last try to acquire memory from free list.
    return TryFreeListAllocation(min_size_in_bytes, max_size_in_bytes, origin);
  }

  return {};
}

base::Optional<std::pair<Address, size_t>>
ConcurrentAllocator::TryFreeListAllocation(size_t min_size_in_bytes,
                                           size_t max_size_in_bytes,
                                           AllocationOrigin origin) {
  base::MutexGuard lock(&space_->space_mutex_);
  DCHECK_LE(min_size_in_bytes, max_size_in_bytes);
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == SHARED_SPACE || identity() == TRUSTED_SPACE);

  size_t new_node_size = 0;
  Tagged<FreeSpace> new_node =
      space_->free_list_->Allocate(min_size_in_bytes, &new_node_size, origin);
  if (new_node.is_null()) return {};
  DCHECK_GE(new_node_size, min_size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  space_->IncreaseAllocatedBytes(new_node_size, page);

  size_t used_size_in_bytes = std::min(new_node_size, max_size_in_bytes);

  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = new_node.address() + used_size_in_bytes;
  DCHECK_LE(limit, end);
  DCHECK_LE(min_size_in_bytes, limit - start);
  if (limit != end) {
    space_->Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }
  space_->AddRangeToActiveSystemPages(page, start, limit);

  return std::make_pair(start, used_size_in_bytes);
}

AllocationResult ConcurrentAllocator::AllocateOutsideLab(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  // Conservative estimate as we don't know the alignment of the allocation.
  const int requested_filler_size = Heap::GetMaximumFillToAlign(alignment);
  const int aligned_size_in_bytes = size_in_bytes + requested_filler_size;
  auto result = AllocateFromSpaceFreeList(aligned_size_in_bytes,
                                          aligned_size_in_bytes, origin);

  if (!result) return AllocationResult::Failure();

  owning_heap()->StartIncrementalMarkingIfAllocationLimitIsReachedBackground();

  DCHECK_GE(result->second, aligned_size_in_bytes);

  Tagged<HeapObject> object = HeapObject::FromAddress(result->first);
  if (requested_filler_size > 0) {
    object = owning_heap()->AlignWithFillerBackground(
        object, size_in_bytes, static_cast<int>(result->second), alignment);
  }

  if (IsBlackAllocationEnabled()) {
    owning_heap()->incremental_marking()->MarkBlackBackground(object,
                                                              size_in_bytes);
  }
  return AllocationResult::FromObject(object);
}

bool ConcurrentAllocator::IsBlackAllocationEnabled() const {
  return context_ == Context::kNotGC &&
         owning_heap()->incremental_marking()->black_allocation();
}

void ConcurrentAllocator::MakeLabIterable() {
  if (IsLabValid()) {
    owning_heap()->CreateFillerObjectAtBackground(
        lab_.top(), static_cast<int>(lab_.limit() - lab_.top()));
  }
}

AllocationSpace ConcurrentAllocator::identity() const {
  return space_->identity();
}

}  // namespace internal
}  // namespace v8
