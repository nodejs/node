// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/heap/heap-utils.h"

#include "src/base/platform/mutex.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/heap/free-list.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces.h"
#include "src/objects/free-space-inl.h"
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
  // v8_flags.stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!v8_flags.stress_concurrent_allocation);
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();
  heap->mark_compact_collector()->EnsureSweepingCompleted(
      MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
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
              *heap->old_space()->allocation_top_address(), free_memory);
        } else {
          heap->CreateFillerObjectAt(
              *heap->new_space()->allocation_top_address(), free_memory);
        }
        break;
      }
    }
    handles.push_back(isolate->factory()->NewFixedArray(length, allocation));
    CHECK((allocation == AllocationType::kYoung &&
           heap->new_space()->Contains(*handles.back())) ||
          (allocation == AllocationType::kOld &&
           heap->InOldSpace(*handles.back())) ||
          v8_flags.single_generation);
    free_memory -= handles.back()->Size();
  }
  return handles;
}

namespace {
void FillPageInPagedSpace(Page* page,
                          std::vector<Handle<FixedArray>>* out_handles) {
  DCHECK(page->SweepingDone());
  PagedSpaceBase* paged_space = static_cast<PagedSpaceBase*>(page->owner());
  // Make sure the LAB is empty to guarantee that all free space is accounted
  // for in the freelist.
  DCHECK_EQ(paged_space->limit(), paged_space->top());

  for (Page* p : *paged_space) {
    if (p != page) paged_space->UnlinkFreeListCategories(p);
  }

  // If min_block_size is larger than FixedArray::kHeaderSize, all blocks in the
  // free list can be used to allocate a fixed array. This guarantees that we
  // can fill the whole page.
  DCHECK_LT(FixedArray::kHeaderSize,
            paged_space->free_list()->min_block_size());

  std::vector<int> available_sizes;
  // Collect all free list block sizes
  page->ForAllFreeListCategories(
      [&available_sizes](FreeListCategory* category) {
        category->IterateNodesForTesting([&available_sizes](FreeSpace node) {
          int node_size = node.Size();
          DCHECK_LT(0, FixedArrayLenFromSize(node_size));
          available_sizes.push_back(node_size);
        });
      });

  Isolate* isolate = page->heap()->isolate();

  // Allocate as many max size arrays as possible, while making sure not to
  // leave behind a block too small to fit a FixedArray.
  const int max_array_length = FixedArrayLenFromSize(kMaxRegularHeapObjectSize);
  for (size_t i = 0; i < available_sizes.size(); ++i) {
    int available_size = available_sizes[i];
    while (available_size >
           kMaxRegularHeapObjectSize + FixedArray::kHeaderSize) {
      Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArray(
          max_array_length, AllocationType::kYoung);
      if (out_handles) out_handles->push_back(fixed_array);
      available_size -= kMaxRegularHeapObjectSize;
    }
    if (available_size > kMaxRegularHeapObjectSize) {
      // Allocate less than kMaxRegularHeapObjectSize to ensure remaining space
      // can be used to allcoate another FixedArray.
      int array_size = kMaxRegularHeapObjectSize - FixedArray::kHeaderSize;
      Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArray(
          FixedArrayLenFromSize(array_size), AllocationType::kYoung);
      if (out_handles) out_handles->push_back(fixed_array);
      available_size -= array_size;
    }
    DCHECK_LE(available_size, kMaxRegularHeapObjectSize);
    DCHECK_LT(0, FixedArrayLenFromSize(available_size));
    available_sizes[i] = available_size;
  }

  // Allocate FixedArrays in remaining free list blocks, from largest to
  // smallest.
  std::sort(available_sizes.begin(), available_sizes.end(),
            [](size_t a, size_t b) { return a > b; });
  for (size_t i = 0; i < available_sizes.size(); ++i) {
    int available_size = available_sizes[i];
    DCHECK_LE(available_size, kMaxRegularHeapObjectSize);
    int array_length = FixedArrayLenFromSize(available_size);
    DCHECK_LT(0, array_length);
    Handle<FixedArray> fixed_array =
        isolate->factory()->NewFixedArray(array_length, AllocationType::kYoung);
    if (out_handles) out_handles->push_back(fixed_array);
  }

  for (Page* p : *paged_space) {
    if (p != page) paged_space->RelinkFreeListCategories(p);
  }
}
}  // namespace

void FillCurrentPage(v8::internal::NewSpace* space,
                     std::vector<Handle<FixedArray>>* out_handles) {
  if (v8_flags.minor_mc) {
    PauseAllocationObserversScope pause_observers(space->heap());
    if (space->top() == kNullAddress) return;
    Page* page = Page::FromAllocationAreaAddress(space->top());
    space->FreeLinearAllocationArea();
    FillPageInPagedSpace(page, out_handles);
  } else {
    FillCurrentPageButNBytes(space, 0, out_handles);
  }
}

namespace {
int GetSpaceRemainingOnCurrentPage(v8::internal::NewSpace* space) {
  Address top = space->top();
  if ((top & kPageAlignmentMask) == 0) {
    // `top` points to the start of a page signifies that there is not room in
    // the current page.
    return 0;
  }
  return static_cast<int>(Page::FromAddress(space->top())->area_end() - top);
}
}  // namespace

void FillCurrentPageButNBytes(v8::internal::NewSpace* space, int extra_bytes,
                              std::vector<Handle<FixedArray>>* out_handles) {
  PauseAllocationObserversScope pause_observers(space->heap());
  // We cannot rely on `space->limit()` to point to the end of the current page
  // in the case where inline allocations are disabled, it actually points to
  // the current allocation pointer.
  DCHECK_IMPLIES(!space->IsInlineAllocationEnabled(),
                 space->limit() == space->top());
  int space_remaining = GetSpaceRemainingOnCurrentPage(space);
  CHECK(space_remaining >= extra_bytes);
  int new_linear_size = space_remaining - extra_bytes;
  if (new_linear_size == 0) return;
  std::vector<Handle<FixedArray>> handles = heap::CreatePadding(
      space->heap(), space_remaining, i::AllocationType::kYoung);
  if (out_handles != nullptr) {
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  }
}

void SimulateIncrementalMarking(i::Heap* heap, bool force_completion) {
  const double kStepSizeInMs = 100;
  CHECK(v8_flags.incremental_marking);
  i::IncrementalMarking* marking = heap->incremental_marking();
  i::MarkCompactCollector* collector = heap->mark_compact_collector();

  if (collector->sweeping_in_progress()) {
    SafepointScope scope(heap);
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }

  if (marking->IsMinorMarking()) {
    // If minor incremental marking is running, we need to finalize it first
    // because of the AdvanceForTesting call in this function which is currently
    // only possible for MajorMC.
    heap->CollectGarbage(NEW_SPACE, GarbageCollectionReason::kFinalizeMinorMC);
  }

  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking());
  if (!force_completion) return;

  SafepointScope scope(heap);
  MarkingBarrier::PublishAll(heap);
  marking->MarkRootsForTesting();

  while (!marking->IsMajorMarkingComplete()) {
    marking->AdvanceForTesting(kStepSizeInMs);
  }
}

void SimulateFullSpace(v8::internal::PagedSpace* space) {
  // If you see this check failing, disable the flag at the start of your test:
  // v8_flags.stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!v8_flags.stress_concurrent_allocation);
  CodePageCollectionMemoryModificationScopeForTesting code_scope(space->heap());
  i::MarkCompactCollector* collector = space->heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
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
    heap->mark_compact_collector()->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }
}

void ForceEvacuationCandidate(Page* page) {
  CHECK(v8_flags.manual_evacuation_candidates_selection);
  page->SetFlag(MemoryChunk::FORCE_EVACUATION_CANDIDATE_FOR_TESTING);
  PagedSpace* space = static_cast<PagedSpace*>(page->owner());
  DCHECK_NOT_NULL(space);
  Address top = space->top();
  Address limit = space->limit();
  if (top < limit && Page::FromAllocationAreaAddress(top) == page) {
    // Create filler object to keep page iterable if it was iterable.
    int remaining = static_cast<int>(limit - top);
    space->heap()->CreateFillerObjectAt(top, remaining);
    base::MutexGuard guard(space->mutex());
    space->FreeLinearAllocationArea();
  }
}

bool InCorrectGeneration(HeapObject object) {
  return v8_flags.single_generation ? !i::Heap::InYoungGeneration(object)
                                    : i::Heap::InYoungGeneration(object);
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
