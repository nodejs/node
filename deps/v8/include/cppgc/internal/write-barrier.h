// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_
#define INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_

#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/process-heap.h"
#include "v8config.h"  // NOLINT(build/include_directory)

#if defined(CPPGC_CAGED_HEAP)
#include "cppgc/internal/caged-heap-local-data.h"
#endif

namespace cppgc {
namespace internal {

class V8_EXPORT WriteBarrier final {
 public:
  static V8_INLINE void MarkingBarrier(const void* slot, const void* value) {
#if defined(CPPGC_CAGED_HEAP)
    const uintptr_t start =
        reinterpret_cast<uintptr_t>(value) &
        ~(api_constants::kCagedHeapReservationAlignment - 1);
    const uintptr_t slot_offset = reinterpret_cast<uintptr_t>(slot) - start;
    if (slot_offset > api_constants::kCagedHeapReservationSize) {
      // Check if slot is on stack or value is sentinel or nullptr. This relies
      // on the fact that kSentinelPointer is encoded as 0x1.
      return;
    }

    CagedHeapLocalData* local_data =
        reinterpret_cast<CagedHeapLocalData*>(start);
    if (V8_UNLIKELY(local_data->is_marking_in_progress)) {
      MarkingBarrierSlow(value);
      return;
    }
#if defined(CPPGC_YOUNG_GENERATION)
    GenerationalBarrier(local_data, slot, slot_offset,
                        reinterpret_cast<uintptr_t>(value) - start);
#endif
#else
    if (V8_LIKELY(!ProcessHeap::IsAnyIncrementalOrConcurrentMarking())) return;

    MarkingBarrierSlowWithSentinelCheck(value);
#endif  // CPPGC_CAGED_HEAP
  }

 private:
  WriteBarrier() = delete;

  static void MarkingBarrierSlow(const void* value);
  static void MarkingBarrierSlowWithSentinelCheck(const void* value);

#if defined(CPPGC_YOUNG_GENERATION)
  static V8_INLINE void GenerationalBarrier(CagedHeapLocalData* local_data,
                                            const void* slot,
                                            uintptr_t slot_offset,
                                            uintptr_t value_offset) {
    const AgeTable& age_table = local_data->age_table;

    // Bail out if the slot is in young generation.
    if (V8_LIKELY(age_table[slot_offset] == AgeTable::Age::kYoung)) return;

    GenerationalBarrierSlow(local_data, age_table, slot, value_offset);
  }

  static void GenerationalBarrierSlow(CagedHeapLocalData* local_data,
                                      const AgeTable& ageTable,
                                      const void* slot, uintptr_t value_offset);
#endif
};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_
