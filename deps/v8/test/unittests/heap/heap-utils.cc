// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/heap-utils.h"

#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/safepoint.h"

namespace v8 {
namespace internal {

void HeapInternalsBase::SimulateIncrementalMarking(Heap* heap,
                                                   bool force_completion) {
  constexpr double kStepSizeInMs = 100;
  CHECK(FLAG_incremental_marking);
  i::IncrementalMarking* marking = heap->incremental_marking();
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    SafepointScope scope(heap);
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }
  CHECK(marking->IsMarking() || marking->IsStopped() || marking->IsComplete());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                  i::GarbageCollectionReason::kTesting);
  }
  CHECK(marking->IsMarking() || marking->IsComplete());
  if (!force_completion) return;

  while (!marking->IsComplete()) {
    marking->Step(kStepSizeInMs,
                  i::IncrementalMarking::CompletionAction::kGCViaTask,
                  i::StepOrigin::kV8);
  }
  CHECK(marking->IsComplete());
}

void HeapInternalsBase::SimulateFullSpace(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles) {
  // If you see this check failing, disable the flag at the start of your test:
  // FLAG_stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!FLAG_stress_concurrent_allocation);
  while (FillCurrentPage(space, out_handles) || space->AddFreshPage()) {
  }
}

void HeapInternalsBase::SimulateFullSpace(v8::internal::PagedSpace* space) {
  // If you see this check failing, disable the flag at the start of your test:
  // FLAG_stress_concurrent_allocation = false;
  // Background thread allocating concurrently interferes with this function.
  CHECK(!FLAG_stress_concurrent_allocation);
  CodePageCollectionMemoryModificationScopeForTesting code_scope(space->heap());
  i::MarkCompactCollector* collector = space->heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }
  space->FreeLinearAllocationArea();
  space->ResetFreeList();
}

bool HeapInternalsBase::FillCurrentPage(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles) {
  return FillCurrentPageButNBytes(space, 0, out_handles);
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

bool HeapInternalsBase::FillCurrentPageButNBytes(
    v8::internal::NewSpace* space, int extra_bytes,
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
  if (new_linear_size == 0) return false;
  std::vector<Handle<FixedArray>> handles =
      CreatePadding(space->heap(), space_remaining, i::AllocationType::kYoung);
  if (out_handles != nullptr) {
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  }
  return true;
}

int HeapInternalsBase::FixedArrayLenFromSize(int size) {
  return std::min({(size - FixedArray::kHeaderSize) / kTaggedSize,
                   FixedArray::kMaxRegularLength});
}

std::vector<Handle<FixedArray>> HeapInternalsBase::CreatePadding(
    Heap* heap, int padding_size, AllocationType allocation, int object_size) {
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
          FLAG_single_generation);
    free_memory -= handles.back()->Size();
  }
  return handles;
}

}  // namespace internal
}  // namespace v8
