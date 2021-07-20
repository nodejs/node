// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/write-barrier.h"

#include "include/cppgc/heap-consistency.h"
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

// static
AtomicEntryFlag WriteBarrier::incremental_or_concurrent_marking_flag_;

namespace {

template <MarkerBase::WriteBarrierType type>
void ProcessMarkValue(HeapObjectHeader& header, MarkerBase* marker,
                      const void* value) {
#if defined(CPPGC_CAGED_HEAP)
  DCHECK(reinterpret_cast<CagedHeapLocalData*>(
             reinterpret_cast<uintptr_t>(value) &
             ~(kCagedHeapReservationAlignment - 1))
             ->is_incremental_marking_in_progress);
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

  marker->WriteBarrierForObject<type>(header);
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
  const auto& heap = page->heap();

  // GetWriteBarrierType() checks marking state.
  DCHECK(heap.marker());
  // No write barriers should be executed from atomic pause marking.
  DCHECK(!heap.in_atomic_pause());

  auto& header =
      const_cast<HeapObjectHeader&>(page->ObjectHeaderFromInnerAddress(value));
  if (!header.TryMarkAtomic()) return;

  ProcessMarkValue<MarkerBase::WriteBarrierType::kDijkstra>(
      header, heap.marker(), value);
}

// static
void WriteBarrier::DijkstraMarkingBarrierRangeSlow(
    HeapHandle& heap_handle, const void* first_element, size_t element_size,
    size_t number_of_elements, TraceCallback trace_callback) {
  auto& heap_base = HeapBase::From(heap_handle);

  // GetWriteBarrierType() checks marking state.
  DCHECK(heap_base.marker());
  // No write barriers should be executed from atomic pause marking.
  DCHECK(!heap_base.in_atomic_pause());

  cppgc::subtle::DisallowGarbageCollectionScope disallow_gc_scope(heap_base);
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
  const auto& heap = page->heap();

  // GetWriteBarrierType() checks marking state.
  DCHECK(heap.marker());
  // No write barriers should be executed from atomic pause marking.
  DCHECK(!heap.in_atomic_pause());

  auto& header =
      const_cast<HeapObjectHeader&>(page->ObjectHeaderFromInnerAddress(value));
  if (!header.IsMarked<AccessMode::kAtomic>()) return;

  ProcessMarkValue<MarkerBase::WriteBarrierType::kSteele>(header, heap.marker(),
                                                          value);
}

#if defined(CPPGC_YOUNG_GENERATION)
// static
void WriteBarrier::GenerationalBarrierSlow(const CagedHeapLocalData& local_data,
                                           const AgeTable& age_table,
                                           const void* slot,
                                           uintptr_t value_offset) {
  // A write during atomic pause (e.g. pre-finalizer) may trigger the slow path
  // of the barrier. This is a result of the order of bailouts where not marking
  // results in applying the generational barrier.
  if (local_data.heap_base->in_atomic_pause()) return;

  if (value_offset > 0 && age_table[value_offset] == AgeTable::Age::kOld)
    return;
  // Record slot.
  local_data.heap_base->remembered_slots().insert(const_cast<void*>(slot));
}
#endif  // CPPGC_YOUNG_GENERATION

#if V8_ENABLE_CHECKS
// static
void WriteBarrier::CheckParams(Type expected_type, const Params& params) {
  CHECK_EQ(expected_type, params.type);
}
#endif  // V8_ENABLE_CHECKS

// static
bool WriteBarrierTypeForNonCagedHeapPolicy::IsMarking(const void* object,
                                                      HeapHandle** handle) {
  // Large objects cannot have mixins, so we are guaranteed to always have
  // a pointer on the same page.
  const auto* page = BasePage::FromPayload(object);
  *handle = &page->heap();
  const MarkerBase* marker = page->heap().marker();
  return marker && marker->IsMarking();
}

// static
bool WriteBarrierTypeForNonCagedHeapPolicy::IsMarking(HeapHandle& heap_handle) {
  const auto& heap_base = internal::HeapBase::From(heap_handle);
  const MarkerBase* marker = heap_base.marker();
  return marker && marker->IsMarking();
}

#if defined(CPPGC_CAGED_HEAP)

// static
bool WriteBarrierTypeForCagedHeapPolicy::IsMarking(
    const HeapHandle& heap_handle, WriteBarrier::Params& params) {
  const auto& heap_base = internal::HeapBase::From(heap_handle);
  if (const MarkerBase* marker = heap_base.marker()) {
    return marker->IsMarking();
  }
  // Also set caged heap start here to avoid another call immediately after
  // checking IsMarking().
#if defined(CPPGC_YOUNG_GENERATION)
  params.start =
      reinterpret_cast<uintptr_t>(&heap_base.caged_heap().local_data());
#endif  // !CPPGC_YOUNG_GENERATION
  return false;
}

#endif  // CPPGC_CAGED_HEAP

}  // namespace internal
}  // namespace cppgc
