// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/heap/heap-utils.h"

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace heap {

void InvokeScavenge() { CcTest::CollectGarbage(i::NEW_SPACE); }

void InvokeMarkSweep() { CcTest::CollectAllGarbage(); }

void SealCurrentObjects(Heap* heap) {
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted();
  heap->old_space()->FreeLinearAllocationArea();
  for (Page* page : *heap->old_space()) {
    page->MarkNeverAllocateForTesting();
  }
}

int FixedArrayLenFromSize(int size) {
  return Min((size - FixedArray::kHeaderSize) / kTaggedSize,
             FixedArray::kMaxRegularLength);
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
    int overall_free_memory =
        static_cast<int>(*heap->new_space()->allocation_limit_address() -
                         *heap->new_space()->allocation_top_address());
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
        // Not enough room to create another fixed array. Let's create a filler.
        if (free_memory > (2 * kTaggedSize)) {
          heap->CreateFillerObjectAt(
              *heap->old_space()->allocation_top_address(), free_memory,
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

void AllocateAllButNBytes(v8::internal::NewSpace* space, int extra_bytes,
                          std::vector<Handle<FixedArray>>* out_handles) {
  PauseAllocationObserversScope pause_observers(space->heap());
  int space_remaining = static_cast<int>(*space->allocation_limit_address() -
                                         *space->allocation_top_address());
  CHECK(space_remaining >= extra_bytes);
  int new_linear_size = space_remaining - extra_bytes;
  if (new_linear_size == 0) return;
  std::vector<Handle<FixedArray>> handles = heap::CreatePadding(
      space->heap(), new_linear_size, i::AllocationType::kYoung);
  if (out_handles != nullptr)
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
}

void FillCurrentPage(v8::internal::NewSpace* space,
                     std::vector<Handle<FixedArray>>* out_handles) {
  heap::AllocateAllButNBytes(space, 0, out_handles);
}

bool FillUpOnePage(v8::internal::NewSpace* space,
                   std::vector<Handle<FixedArray>>* out_handles) {
  PauseAllocationObserversScope pause_observers(space->heap());
  int space_remaining = static_cast<int>(*space->allocation_limit_address() -
                                         *space->allocation_top_address());
  if (space_remaining == 0) return false;
  std::vector<Handle<FixedArray>> handles = heap::CreatePadding(
      space->heap(), space_remaining, i::AllocationType::kYoung);
  if (out_handles != nullptr)
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  return true;
}

void SimulateFullSpace(v8::internal::NewSpace* space,
                       std::vector<Handle<FixedArray>>* out_handles) {
  heap::FillCurrentPage(space, out_handles);
  while (heap::FillUpOnePage(space, out_handles) || space->AddFreshPage()) {
  }
}

void SimulateIncrementalMarking(i::Heap* heap, bool force_completion) {
  const double kStepSizeInMs = 100;
  CHECK(FLAG_incremental_marking);
  i::IncrementalMarking* marking = heap->incremental_marking();
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  if (marking->IsSweeping()) {
    marking->FinalizeSweeping();
  }
  CHECK(marking->IsMarking() || marking->IsStopped() || marking->IsComplete());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking() || marking->IsComplete());
  if (!force_completion) return;

  while (!marking->IsComplete()) {
    marking->V8Step(kStepSizeInMs, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                    i::StepOrigin::kV8);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      marking->FinalizeIncrementally();
    }
  }
  CHECK(marking->IsComplete());
}

void SimulateFullSpace(v8::internal::PagedSpace* space) {
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
    heap->mark_compact_collector()->EnsureSweepingCompleted();
  }
}

void ForceEvacuationCandidate(Page* page) {
  CHECK(FLAG_manual_evacuation_candidates_selection);
  page->SetFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
  PagedSpace* space = static_cast<PagedSpace*>(page->owner());
  Address top = space->top();
  Address limit = space->limit();
  if (top < limit && Page::FromAllocationAreaAddress(top) == page) {
    // Create filler object to keep page iterable if it was iterable.
    int remaining = static_cast<int>(limit - top);
    space->heap()->CreateFillerObjectAt(top, remaining,
                                        ClearRecordedSlots::kNo);
    space->FreeLinearAllocationArea();
  }
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
