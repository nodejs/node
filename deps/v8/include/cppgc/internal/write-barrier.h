// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_
#define INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_

#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/process-heap.h"
#include "cppgc/trace-trait.h"
#include "v8config.h"  // NOLINT(build/include_directory)

#if defined(CPPGC_CAGED_HEAP)
#include "cppgc/internal/caged-heap-local-data.h"
#endif

namespace cppgc {

class HeapHandle;

namespace internal {

class WriteBarrierTypeForCagedHeapPolicy;
class WriteBarrierTypeForNonCagedHeapPolicy;

class V8_EXPORT WriteBarrier final {
 public:
  enum class Type : uint8_t {
    kNone,
    kMarking,
    kGenerational,
  };

  struct Params {
#if V8_ENABLE_CHECKS
    Type type = Type::kNone;
#endif  // !V8_ENABLE_CHECKS
#if defined(CPPGC_CAGED_HEAP)
    uintptr_t start;

    CagedHeapLocalData& caged_heap() const {
      return *reinterpret_cast<CagedHeapLocalData*>(start);
    }
    uintptr_t slot_offset;
    uintptr_t value_offset;
#endif  // CPPGC_CAGED_HEAP
  };

  enum class ValueMode {
    kValuePresent,
    kNoValuePresent,
  };

  // Returns the required write barrier for a given `slot` and `value`.
  static V8_INLINE Type GetWriteBarrierType(const void* slot, const void* value,
                                            Params& params);
  // Returns the required write barrier for a given `slot`.
  static V8_INLINE Type GetWriteBarrierType(const void* slot, Params& params);

  static V8_INLINE void DijkstraMarkingBarrier(const Params& params,
                                               const void* object);
  static V8_INLINE void DijkstraMarkingBarrierRange(
      const Params& params, HeapHandle& heap, const void* first_element,
      size_t element_size, size_t number_of_elements,
      TraceCallback trace_callback);
  static V8_INLINE void SteeleMarkingBarrier(const Params& params,
                                             const void* object);
#if defined(CPPGC_YOUNG_GENERATION)
  static V8_INLINE void GenerationalBarrier(const Params& params,
                                            const void* slot);
#else   // !CPPGC_YOUNG_GENERATION
  static V8_INLINE void GenerationalBarrier(const Params& params,
                                            const void* slot) {}
#endif  // CPPGC_YOUNG_GENERATION

#if V8_ENABLE_CHECKS
  static void CheckParams(Type expected_type, const Params& params);
#else   // !V8_ENABLE_CHECKS
  static void CheckParams(Type expected_type, const Params& params) {}
#endif  // !V8_ENABLE_CHECKS

 private:
  WriteBarrier() = delete;

#if defined(CPPGC_CAGED_HEAP)
  using WriteBarrierTypePolicy = WriteBarrierTypeForCagedHeapPolicy;
#else   // !CPPGC_CAGED_HEAP
  using WriteBarrierTypePolicy = WriteBarrierTypeForNonCagedHeapPolicy;
#endif  // !CPPGC_CAGED_HEAP

  static void DijkstraMarkingBarrierSlow(const void* value);
  static void DijkstraMarkingBarrierSlowWithSentinelCheck(const void* value);
  static void DijkstraMarkingBarrierRangeSlow(HeapHandle& heap_handle,
                                              const void* first_element,
                                              size_t element_size,
                                              size_t number_of_elements,
                                              TraceCallback trace_callback);
  static void SteeleMarkingBarrierSlow(const void* value);
  static void SteeleMarkingBarrierSlowWithSentinelCheck(const void* value);

#if defined(CPPGC_YOUNG_GENERATION)
  static void GenerationalBarrierSlow(const CagedHeapLocalData& local_data,
                                      const AgeTable& ageTable,
                                      const void* slot, uintptr_t value_offset);
#endif  // CPPGC_YOUNG_GENERATION
};

#if defined(CPPGC_CAGED_HEAP)
class WriteBarrierTypeForCagedHeapPolicy final {
 public:
  template <WriteBarrier::ValueMode value_mode>
  static V8_INLINE WriteBarrier::Type Get(const void* slot, const void* value,
                                          WriteBarrier::Params& params) {
    const bool have_caged_heap =
        value_mode == WriteBarrier::ValueMode::kValuePresent
            ? TryGetCagedHeap(slot, value, params)
            : TryGetCagedHeap(slot, slot, params);
    if (!have_caged_heap) {
      return WriteBarrier::Type::kNone;
    }
    if (V8_UNLIKELY(params.caged_heap().is_marking_in_progress)) {
#if V8_ENABLE_CHECKS
      params.type = WriteBarrier::Type::kMarking;
#endif  // !V8_ENABLE_CHECKS
      return WriteBarrier::Type::kMarking;
    }
#if defined(CPPGC_YOUNG_GENERATION)
    params.slot_offset = reinterpret_cast<uintptr_t>(slot) - params.start;
    if (value_mode == WriteBarrier::ValueMode::kValuePresent) {
      params.value_offset = reinterpret_cast<uintptr_t>(value) - params.start;
    } else {
      params.value_offset = 0;
    }
#if V8_ENABLE_CHECKS
    params.type = WriteBarrier::Type::kGenerational;
#endif  // !V8_ENABLE_CHECKS
    return WriteBarrier::Type::kGenerational;
#else   // !CPPGC_YOUNG_GENERATION
    return WriteBarrier::Type::kNone;
#endif  // !CPPGC_YOUNG_GENERATION
  }

 private:
  WriteBarrierTypeForCagedHeapPolicy() = delete;

  static V8_INLINE bool TryGetCagedHeap(const void* slot, const void* value,
                                        WriteBarrier::Params& params) {
    params.start = reinterpret_cast<uintptr_t>(value) &
                   ~(api_constants::kCagedHeapReservationAlignment - 1);
    const uintptr_t slot_offset =
        reinterpret_cast<uintptr_t>(slot) - params.start;
    if (slot_offset > api_constants::kCagedHeapReservationSize) {
      // Check if slot is on stack or value is sentinel or nullptr. This relies
      // on the fact that kSentinelPointer is encoded as 0x1.
      return false;
    }
    return true;
  }
};
#endif  // CPPGC_CAGED_HEAP

class WriteBarrierTypeForNonCagedHeapPolicy final {
 public:
  template <WriteBarrier::ValueMode value_mode>
  static V8_INLINE WriteBarrier::Type Get(const void* slot, const void* value,
                                          WriteBarrier::Params& params) {
    WriteBarrier::Type type =
        V8_LIKELY(!ProcessHeap::IsAnyIncrementalOrConcurrentMarking())
            ? WriteBarrier::Type::kNone
            : WriteBarrier::Type::kMarking;
#if V8_ENABLE_CHECKS
    params.type = type;
#endif  // !V8_ENABLE_CHECKS
    return type;
  }

 private:
  WriteBarrierTypeForNonCagedHeapPolicy() = delete;
};

// static
WriteBarrier::Type WriteBarrier::GetWriteBarrierType(
    const void* slot, const void* value, WriteBarrier::Params& params) {
  return WriteBarrierTypePolicy::Get<ValueMode::kValuePresent>(slot, value,
                                                               params);
}

// static
WriteBarrier::Type WriteBarrier::GetWriteBarrierType(
    const void* slot, WriteBarrier::Params& params) {
  return WriteBarrierTypePolicy::Get<ValueMode::kNoValuePresent>(slot, nullptr,
                                                                 params);
}

// static
void WriteBarrier::DijkstraMarkingBarrier(const Params& params,
                                          const void* object) {
  CheckParams(Type::kMarking, params);
#if defined(CPPGC_CAGED_HEAP)
  // Caged heap already filters out sentinels.
  DijkstraMarkingBarrierSlow(object);
#else   // !CPPGC_CAGED_HEAP
  DijkstraMarkingBarrierSlowWithSentinelCheck(object);
#endif  // !CPPGC_CAGED_HEAP
}

// static
void WriteBarrier::DijkstraMarkingBarrierRange(const Params& params,
                                               HeapHandle& heap,
                                               const void* first_element,
                                               size_t element_size,
                                               size_t number_of_elements,
                                               TraceCallback trace_callback) {
  CheckParams(Type::kMarking, params);
  DijkstraMarkingBarrierRangeSlow(heap, first_element, element_size,
                                  number_of_elements, trace_callback);
}

// static
void WriteBarrier::SteeleMarkingBarrier(const Params& params,
                                        const void* object) {
  CheckParams(Type::kMarking, params);
#if defined(CPPGC_CAGED_HEAP)
  // Caged heap already filters out sentinels.
  SteeleMarkingBarrierSlow(object);
#else   // !CPPGC_CAGED_HEAP
  SteeleMarkingBarrierSlowWithSentinelCheck(object);
#endif  // !CPPGC_CAGED_HEAP
}

#if defined(CPPGC_YOUNG_GENERATION)
// static
void WriteBarrier::GenerationalBarrier(const Params& params, const void* slot) {
  CheckParams(Type::kGenerational, params);

  const CagedHeapLocalData& local_data = params.caged_heap();
  const AgeTable& age_table = local_data.age_table;

  // Bail out if the slot is in young generation.
  if (V8_LIKELY(age_table[params.slot_offset] == AgeTable::Age::kYoung)) return;

  GenerationalBarrierSlow(local_data, age_table, slot, params.value_offset);
}

#endif  // !CPPGC_YOUNG_GENERATION

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_
