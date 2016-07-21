// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEAP_UTILS_H_
#define HEAP_UTILS_H_

#include "src/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/isolate.h"


namespace v8 {
namespace internal {

static int LenFromSize(int size) {
  return (size - FixedArray::kHeaderSize) / kPointerSize;
}


static inline std::vector<Handle<FixedArray>> CreatePadding(
    Heap* heap, int padding_size, PretenureFlag tenure,
    int object_size = Page::kMaxRegularHeapObjectSize) {
  std::vector<Handle<FixedArray>> handles;
  Isolate* isolate = heap->isolate();
  int allocate_memory;
  int length;
  int free_memory = padding_size;
  if (tenure == i::TENURED) {
    heap->old_space()->EmptyAllocationInfo();
    int overall_free_memory = static_cast<int>(heap->old_space()->Available());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  } else {
    heap->new_space()->DisableInlineAllocationSteps();
    int overall_free_memory =
        static_cast<int>(*heap->new_space()->allocation_limit_address() -
                         *heap->new_space()->allocation_top_address());
    CHECK(padding_size <= overall_free_memory || overall_free_memory == 0);
  }
  while (free_memory > 0) {
    if (free_memory > object_size) {
      allocate_memory = object_size;
      length = LenFromSize(allocate_memory);
    } else {
      allocate_memory = free_memory;
      length = LenFromSize(allocate_memory);
      if (length <= 0) {
        // Not enough room to create another fixed array. Let's create a filler.
        heap->CreateFillerObjectAt(*heap->old_space()->allocation_top_address(),
                                   free_memory, ClearRecordedSlots::kNo);
        break;
      }
    }
    handles.push_back(isolate->factory()->NewFixedArray(length, tenure));
    CHECK((tenure == NOT_TENURED && heap->InNewSpace(*handles.back())) ||
          (tenure == TENURED && heap->InOldSpace(*handles.back())));
    free_memory -= allocate_memory;
  }
  return handles;
}


// Helper function that simulates a full new-space in the heap.
static inline bool FillUpOnePage(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles = nullptr) {
  space->DisableInlineAllocationSteps();
  int space_remaining = static_cast<int>(*space->allocation_limit_address() -
                                         *space->allocation_top_address());
  if (space_remaining == 0) return false;
  std::vector<Handle<FixedArray>> handles =
      CreatePadding(space->heap(), space_remaining, i::NOT_TENURED);
  if (out_handles != nullptr)
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
  return true;
}


// Helper function that simulates a fill new-space in the heap.
static inline void AllocateAllButNBytes(
    v8::internal::NewSpace* space, int extra_bytes,
    std::vector<Handle<FixedArray>>* out_handles = nullptr) {
  space->DisableInlineAllocationSteps();
  int space_remaining = static_cast<int>(*space->allocation_limit_address() -
                                         *space->allocation_top_address());
  CHECK(space_remaining >= extra_bytes);
  int new_linear_size = space_remaining - extra_bytes;
  if (new_linear_size == 0) return;
  std::vector<Handle<FixedArray>> handles =
      CreatePadding(space->heap(), new_linear_size, i::NOT_TENURED);
  if (out_handles != nullptr)
    out_handles->insert(out_handles->end(), handles.begin(), handles.end());
}

static inline void FillCurrentPage(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles = nullptr) {
  AllocateAllButNBytes(space, 0, out_handles);
}

static inline void SimulateFullSpace(
    v8::internal::NewSpace* space,
    std::vector<Handle<FixedArray>>* out_handles = nullptr) {
  FillCurrentPage(space, out_handles);
  while (FillUpOnePage(space, out_handles) || space->AddFreshPage()) {
  }
}


// Helper function that simulates a full old-space in the heap.
static inline void SimulateFullSpace(v8::internal::PagedSpace* space) {
  space->EmptyAllocationInfo();
  space->ResetFreeList();
  space->ClearStats();
}


// Helper function that simulates many incremental marking steps until
// marking is completed.
static inline void SimulateIncrementalMarking(i::Heap* heap,
                                              bool force_completion = true) {
  i::MarkCompactCollector* collector = heap->mark_compact_collector();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking();
  }
  CHECK(marking->IsMarking());
  if (!force_completion) return;

  while (!marking->IsComplete()) {
    marking->Step(i::MB, i::IncrementalMarking::NO_GC_VIA_STACK_GUARD);
    if (marking->IsReadyToOverApproximateWeakClosure()) {
      marking->FinalizeIncrementally();
    }
  }
  CHECK(marking->IsComplete());
}

}  // namespace internal
}  // namespace v8

#endif  // HEAP_UTILS_H_
