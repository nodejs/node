// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/write-barrier.h"

#include "include/cppgc/internal/pointer-policies.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-visitor.h"

#if defined(CPPGC_CAGED_HEAP)
#include "include/cppgc/internal/caged-heap-local-data.h"
#endif

namespace cppgc {
namespace internal {

namespace {

void ProcessMarkValue(HeapObjectHeader& header, MarkerBase* marker,
                      const void* value) {
#if defined(CPPGC_CAGED_HEAP)
  DCHECK(reinterpret_cast<CagedHeapLocalData*>(
             reinterpret_cast<uintptr_t>(value) &
             ~(kCagedHeapReservationAlignment - 1))
             ->is_marking_in_progress);
#endif
  DCHECK(header.IsMarked<AccessMode::kAtomic>());
  DCHECK(marker);

  if (V8_UNLIKELY(header.IsInConstruction<AccessMode::kNonAtomic>())) {
    // In construction objects are traced only if they are unmarked. If marking
    // reaches this object again when it is fully constructed, it will re-mark
    // it and tracing it as a previously not fully constructed object would know
    // to bail out.
    header.Unmark<AccessMode::kAtomic>();
    marker->WriteBarrierForInConstructionObject(header);
    return;
  }

  marker->WriteBarrierForObject(header);
}

}  // namespace

// static
void WriteBarrier::DijkstraMarkingBarrierSlowWithSentinelCheck(
    const void* value) {
  if (!value || value == kSentinelPointer) return;

  DijkstraMarkingBarrierSlow(value);
}

// static
void WriteBarrier::DijkstraMarkingBarrierSlow(const void* value) {
  const BasePage* page = BasePage::FromPayload(value);
  const auto* heap = page->heap();

  // Marker being not set up means that no incremental/concurrent marking is in
  // progress.
  if (!heap->marker()) return;

  auto& header =
      const_cast<HeapObjectHeader&>(page->ObjectHeaderFromInnerAddress(value));
  if (!header.TryMarkAtomic()) return;

  ProcessMarkValue(header, heap->marker(), value);
}

// static
void WriteBarrier::DijkstraMarkingBarrierRangeSlow(
    HeapHandle& heap_handle, const void* first_element, size_t element_size,
    size_t number_of_elements, TraceCallback trace_callback) {
  auto& heap_base = HeapBase::From(heap_handle);
  MarkerBase* marker = heap_base.marker();
  if (!marker) {
    return;
  }

  ObjectAllocator::NoAllocationScope no_allocation(
      heap_base.object_allocator());
  const char* array = static_cast<const char*>(first_element);
  while (number_of_elements-- > 0) {
    trace_callback(&heap_base.marker()->Visitor(), array);
    array += element_size;
  }
}

// static
void WriteBarrier::SteeleMarkingBarrierSlowWithSentinelCheck(
    const void* value) {
  if (!value || value == kSentinelPointer) return;

  SteeleMarkingBarrierSlow(value);
}

// static
void WriteBarrier::SteeleMarkingBarrierSlow(const void* value) {
  const BasePage* page = BasePage::FromPayload(value);
  const auto* heap = page->heap();

  // Marker being not set up means that no incremental/concurrent marking is in
  // progress.
  if (!heap->marker()) return;

  auto& header =
      const_cast<HeapObjectHeader&>(page->ObjectHeaderFromInnerAddress(value));
  if (!header.IsMarked<AccessMode::kAtomic>()) return;

  ProcessMarkValue(header, heap->marker(), value);
}

#if defined(CPPGC_YOUNG_GENERATION)
// static
void WriteBarrier::GenerationalBarrierSlow(const CagedHeapLocalData& local_data,
                                           const AgeTable& age_table,
                                           const void* slot,
                                           uintptr_t value_offset) {
  if (value_offset > 0 && age_table[value_offset] == AgeTable::Age::kOld)
    return;
  // Record slot.
  local_data.heap_base->remembered_slots().insert(const_cast<void*>(slot));
}
#endif

#if V8_ENABLE_CHECKS
// static
void WriteBarrier::CheckParams(Type expected_type, const Params& params) {
  CHECK_EQ(expected_type, params.type);
}
#endif  // V8_ENABLE_CHECKS

}  // namespace internal
}  // namespace cppgc
