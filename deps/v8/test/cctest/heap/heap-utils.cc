// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/heap/heap-utils.h"

#include "src/base/platform/mutex.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/safepoint.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace heap {

void InvokeScavenge(Isolate* isolate) {
  CcTest::CollectGarbage(i::NEW_SPACE, isolate);
}

void InvokeMarkSweep(Isolate* isolate) { CcTest::CollectAllGarbage(isolate); }

void SealCurrentObjects(Heap* heap) {
  // If you see this check failing, disable the flag at the start of your test:
  // FLAG_stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!FLAG_stress_concurrent_allocation);
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted();
  heap->old_space()->FreeLinearAllocationArea();
  for (Page* page : *heap->old_space()) {
    page->MarkNeverAllocateForTesting();
  }
}

int FixedArrayLenFromSize(int size) {
  return std::min({(size - FixedArray::kHeaderSize) / kTaggedSize,
                   FixedArray::kMaxRegularLength});
}

std::vector<Handle<FixedArray>> FillOldSpacePageWithFixedArrays(Heap* heap,
                                                                int remainder) {
  PauseAllocationObserversScope pause_observers(heap);
  std::vector<Handle<FixedArray>> handles;
  Isolate* isolate = heap->isolate();
  const int kArraySize = 128;
  const int kArrayLen = heap::FixedArrayLenFromSize(kArraySize);
  Handle<FixedArray> array;
  int allocated = 0;
  do {
    if (allocated + kArraySize * 2 >
        static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage())) {
      int size =
          kArraySize * 2 -
          ((allocated + kArraySize * 2) -
           static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage())) -
          remainder;
      int last_array_len = heap::FixedArrayLenFromSize(size);
      array = isolate->factory()->NewFixedArray(last_array_len,
                                                AllocationType::kOld);
      CHECK_EQ(size, array->Size());
      allocated += array->Size() + remainder;
    } else {
      array =
          isolate->factory()->NewFixedArray(kArrayLen, AllocationType::kOld);
      allocated += array->Size();
      CHECK_EQ(kArraySize, array->Size());
    }
    if (handles.empty()) {
      // Check that allocations started on a new page.
      CHECK_EQ(array->address(), Page::FromHeapObject(*array)->area_start());
    }
    handles.push_back(array);
  } while (allocated <
           static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()));
  return handles;
}

std::vector<Handle<FixedArray>> CreatePadding(Heap* heap, int padding_size,
                                              AllocationType allocation,
                                              int object_size) {
  std::vector<Handle<FixedArray>> handles;
  Isolate* isolate = heap->isolate();
  int allocate_memory;
  int length;
  int free_memory = padding_size;
  if (allocation == i::AllocationType::kOld) {
    heap->old_space()->FreeLinearAllocationArea();
    int overall_free_memory = static_cast<int>(heap->old_space()->Available());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  } else {
    int overall_free_memory = static_cast<int>(heap->new_space()->Available());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  }
  while (free_memory > 0) {
    if (free_memory > object_size) {
      allocate_memory = object_size;
      length = FixedArrayLenFromSize(allocate_memory);
    } else {
      allocate_memory = free_memory;
      length = FixedArrayLenFromSize(allocate_memory);
      if (length <= 0) {
        // Not enough room to create another FixedArray, so create a filler.
        if (allocation == i::AllocationType::kOld) {
          heap->CreateFillerObjectAt(
              *heap->old_space()->allocation_top_address(), free_memory,
              ClearRecordedSlots::kNo);
        } else {
          heap->CreateFillerObjectAt(
              *heap->new_space()->allocation_top_address(), free_memory,
              ClearRecordedSlots::kNo);
        }
        break;
      }
    }
    handles.push_back(isolate->factory()->NewFixedArray(length, allocation));
    CHECK((allocation == AllocationType::kYoung &&
           heap->new_space()->Contains(*handles.back())) ||
          (allocation == AllocationType::kOld &&
           heap->InOldSpace(*handles.back())));
    free_memory -= handles.back()->Size();
  }
  return handles;
}

bool FillCurrentPage(v8::internal::NewSpace* space,
                     std::vector<Handle<FixedArray>>* out_handles) {
  return heap::FillCurrentPageButNBytes(space, 0, out_handles);
}

bool FillCurrentPageButNBytes(v8::internal::NewSpace* space, int extra_bytes,
                              std::vector<Handle<FixedArray>>* out_handles) {
  PauseAllocationObserversScope pause_observers(space->heap());
  // We cannot rely on `space->limit()` to point to the end of the current page
  // in the case where inline allocations are disabled, it actually points to
  // the current allocation pointer.
  DCHECK_IMPLIES(space->heap()->inline_allocation_disabled(),
                 space->limit() == space->top());
  int space_remaining =
      static_cast<int>(space->to_space().page_high() - space->top());
  CHECK(space_remaining >= extra_bytes);
  int new_linear_size = space_remaining - extra_bytes;
  if (new_linear_size == 0) return false;
  std::vector<Handle<FixedArray>> handles = heap::CreatePadding(
      space->heap(), space_remaining, i::AllocationType::kYoung);
  if (out_handles != nullptr) {
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  }
  return true;
}

void SimulateFullSpace(v8::internal::NewSpace* space,
                       std::vector<Handle<FixedArray>>* out_handles) {
  // If you see this check failing, disable the flag at the start of your test:
  // FLAG_stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!FLAG_stress_concurrent_allocation);
  while (heap::FillCurrentPage(space, out_handles) || space->AddFreshPage()) {
  }
}

void SimulateIncrementalMarking(i::Heap* heap, bool force_completion) {
  const double kStepSizeInMs = 100;
  CHECK(FLAG_incremental_marking);
  i::IncrementalMarking* marking = heap->incremental_marking();
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    SafepointScope scope(heap);
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped() || marking->IsComplete());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking() || marking->IsComplete());
  if (!force_completion) return;

  while (!marking->IsComplete()) {
    marking->Step(kStepSizeInMs, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                  i::StepOrigin::kV8);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      SafepointScope scope(heap);
      marking->FinalizeIncrementally();
    }
  }
  CHECK(marking->IsComplete());
}

void SimulateFullSpace(v8::internal::PagedSpace* space) {
  // If you see this check failing, disable the flag at the start of your test:
  // FLAG_stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!FLAG_stress_concurrent_allocation);
  CodeSpaceMemoryModificationScope modification_scope(space->heap());
  i::MarkCompactCollector* collector = space->heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  space->FreeLinearAllocationArea();
  space->ResetFreeList();
}

void AbandonCurrentlyFreeMemory(PagedSpace* space) {
  space->FreeLinearAllocationArea();
  for (Page* page : *space) {
    page->MarkNeverAllocateForTesting();
  }
}

void GcAndSweep(Heap* heap, AllocationSpace space) {
  heap->CollectGarbage(space, GarbageCollectionReason::kTesting);
  if (heap->mark_compact_collector()->sweeping_in_progress()) {
    SafepointScope scope(heap);
    heap->mark_compact_collector()->EnsureSweepingCompleted();
  }
}

void ForceEvacuationCandidate(Page* page) {
  CHECK(FLAG_manual_evacuation_candidates_selection);
  page->SetFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
  PagedSpace* space = static_cast<PagedSpace*>(page->owner());
  DCHECK_NOT_NULL(space);
  Address top = space->top();
  Address limit = space->limit();
  if (top < limit && Page::FromAllocationAreaAddress(top) == page) {
    // Create filler object to keep page iterable if it was iterable.
    int remaining = static_cast<int>(limit - top);
    space->heap()->CreateFillerObjectAt(top, remaining,
                                        ClearRecordedSlots::kNo);
    base::MutexGuard guard(space->mutex());
    space->FreeLinearAllocationArea();
  }
}

bool InCorrectGeneration(HeapObject object) {
  return FLAG_single_generation ? !i::Heap::InYoungGeneration(object)
                                : i::Heap::InYoungGeneration(object);
}

void EnsureFlagLocalHeapsEnabled() {
  // Avoid data race in concurrent thread by only setting the flag to true if
  // not already enabled.
  if (!FLAG_local_heaps) FLAG_local_heaps = true;
}

void GrowNewSpace(Heap* heap) {
  SafepointScope scope(heap);
  if (!heap->new_space()->IsAtMaximumCapacity()) {
    heap->new_space()->Grow();
  }
}

void GrowNewSpaceToMaximumCapacity(Heap* heap) {
  SafepointScope scope(heap);
  while (!heap->new_space()->IsAtMaximumCapacity()) {
    heap->new_space()->Grow();
  }
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
