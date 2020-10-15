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

void MarkValue(const BasePage* page, MarkerBase* marker, const void* value) {
#if defined(CPPGC_CAGED_HEAP)
  DCHECK(reinterpret_cast<CagedHeapLocalData*>(
             reinterpret_cast<uintptr_t>(value) &
             ~(kCagedHeapReservationAlignment - 1))
             ->is_marking_in_progress);
#endif
  auto& header =
      const_cast<HeapObjectHeader&>(page->ObjectHeaderFromInnerAddress(value));
  if (!header.TryMarkAtomic()) return;

  DCHECK(marker);

  if (V8_UNLIKELY(
          header
              .IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>())) {
    marker->WriteBarrierForInConstructionObject(header);
    return;
  }

  marker->WriteBarrierForObject(header);
}

}  // namespace

void WriteBarrier::MarkingBarrierSlowWithSentinelCheck(const void* value) {
  if (!value || value == kSentinelPointer) return;

  MarkingBarrierSlow(value);
}

void WriteBarrier::MarkingBarrierSlow(const void* value) {
  const BasePage* page = BasePage::FromPayload(value);
  const auto* heap = page->heap();

  // Marker being not set up means that no incremental/concurrent marking is in
  // progress.
  if (!heap->marker()) return;

  MarkValue(page, heap->marker(), value);
}

#if defined(CPPGC_YOUNG_GENERATION)
void WriteBarrier::GenerationalBarrierSlow(CagedHeapLocalData* local_data,
                                           const AgeTable& age_table,
                                           const void* slot,
                                           uintptr_t value_offset) {
  if (age_table[value_offset] == AgeTable::Age::kOld) return;
  // Record slot.
  local_data->heap_base->remembered_slots().insert(const_cast<void*>(slot));
}
#endif

}  // namespace internal
}  // namespace cppgc
