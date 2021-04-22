// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_
#define INCLUDE_CPPGC_INTERNAL_WRITE_BARRIER_H_

#include "cppgc/heap-state.h"
#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/atomic-entry-flag.h"
#include "cppgc/sentinel-pointer.h"
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
    HeapHandle* heap = nullptr;
#if V8_ENABLE_CHECKS
    Type type = Type::kNone;
#endif  // !V8_ENABLE_CHECKS
#if defined(CPPGC_CAGED_HEAP)
    uintptr_t start = 0;
    CagedHeapLocalData& caged_heap() const {
      return *reinterpret_cast<CagedHeapLocalData*>(start);
    }
    uintptr_t slot_offset = 0;
    uintptr_t value_offset = 0;
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
  template <typename HeapHandleCallback>
  static V8_INLINE Type GetWriteBarrierType(const void* slot, Params& params,
                                            HeapHandleCallback callback);

  template <typename HeapHandleCallback>
  static V8_INLINE Type GetWriteBarrierTypeForExternallyReferencedObject(
      const void* value, Params& params, HeapHandleCallback callback);

  static V8_INLINE void DijkstraMarkingBarrier(const Params& params,
                                               const void* object);
  static V8_INLINE void DijkstraMarkingBarrierRange(
      const Params& params, const void* first_element, size_t element_size,
      size_t number_of_elements, TraceCallback trace_callback);
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

  // The IncrementalOrConcurrentUpdater class allows cppgc internal to update
  // |incremental_or_concurrent_marking_flag_|.
  class IncrementalOrConcurrentMarkingFlagUpdater;
  static bool IsAnyIncrementalOrConcurrentMarking() {
    return incremental_or_concurrent_marking_flag_.MightBeEntered();
  }

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
  static CagedHeapLocalData& GetLocalData(HeapHandle&);
  static void GenerationalBarrierSlow(const CagedHeapLocalData& local_data,
                                      const AgeTable& ageTable,
                                      const void* slot, uintptr_t value_offset);
#endif  // CPPGC_YOUNG_GENERATION

  static AtomicEntryFlag incremental_or_concurrent_marking_flag_;
};

template <WriteBarrier::Type type>
V8_INLINE WriteBarrier::Type SetAndReturnType(WriteBarrier::Params& params) {
  if (type == WriteBarrier::Type::kNone) return WriteBarrier::Type::kNone;
#if V8_ENABLE_CHECKS
  params.type = type;
#endif  // !V8_ENABLE_CHECKS
  return type;
}

#if defined(CPPGC_CAGED_HEAP)
class V8_EXPORT WriteBarrierTypeForCagedHeapPolicy final {
 public:
  template <WriteBarrier::ValueMode value_mode, typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type Get(const void* slot, const void* value,
                                          WriteBarrier::Params& params,
                                          HeapHandleCallback callback) {
    return ValueModeDispatch<value_mode>::Get(slot, value, params, callback);
  }

  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type GetForExternallyReferenced(
      const void* value, WriteBarrier::Params& params, HeapHandleCallback) {
    if (!TryGetCagedHeap(value, value, params)) {
      return WriteBarrier::Type::kNone;
    }
    if (V8_UNLIKELY(params.caged_heap().is_incremental_marking_in_progress)) {
      return SetAndReturnType<WriteBarrier::Type::kMarking>(params);
    }
    return SetAndReturnType<WriteBarrier::Type::kNone>(params);
  }

 private:
  WriteBarrierTypeForCagedHeapPolicy() = delete;

  template <WriteBarrier::ValueMode value_mode>
  struct ValueModeDispatch;

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

  // Returns whether marking is in progress. If marking is not in progress
  // sets the start of the cage accordingly.
  //
  // TODO(chromium:1056170): Create fast path on API.
  static bool IsMarking(const HeapHandle&, WriteBarrier::Params&);
};

template <>
struct WriteBarrierTypeForCagedHeapPolicy::ValueModeDispatch<
    WriteBarrier::ValueMode::kValuePresent> {
  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type Get(const void* slot, const void* value,
                                          WriteBarrier::Params& params,
                                          HeapHandleCallback) {
    bool within_cage = TryGetCagedHeap(slot, value, params);
    if (!within_cage) {
      return WriteBarrier::Type::kNone;
    }
    if (V8_LIKELY(!params.caged_heap().is_incremental_marking_in_progress)) {
#if defined(CPPGC_YOUNG_GENERATION)
      params.heap = reinterpret_cast<HeapHandle*>(params.start);
      params.slot_offset = reinterpret_cast<uintptr_t>(slot) - params.start;
      params.value_offset = reinterpret_cast<uintptr_t>(value) - params.start;
      return SetAndReturnType<WriteBarrier::Type::kGenerational>(params);
#else   // !CPPGC_YOUNG_GENERATION
      return SetAndReturnType<WriteBarrier::Type::kNone>(params);
#endif  // !CPPGC_YOUNG_GENERATION
    }
    params.heap = reinterpret_cast<HeapHandle*>(params.start);
    return SetAndReturnType<WriteBarrier::Type::kMarking>(params);
  }
};

template <>
struct WriteBarrierTypeForCagedHeapPolicy::ValueModeDispatch<
    WriteBarrier::ValueMode::kNoValuePresent> {
  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type Get(const void* slot, const void*,
                                          WriteBarrier::Params& params,
                                          HeapHandleCallback callback) {
#if defined(CPPGC_YOUNG_GENERATION)
    HeapHandle& handle = callback();
    if (V8_LIKELY(!IsMarking(handle, params))) {
      // params.start is populated by IsMarking().
      params.heap = &handle;
      params.slot_offset = reinterpret_cast<uintptr_t>(slot) - params.start;
      // params.value_offset stays 0.
      if (params.slot_offset > api_constants::kCagedHeapReservationSize) {
        // Check if slot is on stack.
        return SetAndReturnType<WriteBarrier::Type::kNone>(params);
      }
      return SetAndReturnType<WriteBarrier::Type::kGenerational>(params);
    }
#else   // !CPPGC_YOUNG_GENERATION
    if (V8_LIKELY(!WriteBarrier::IsAnyIncrementalOrConcurrentMarking())) {
      return SetAndReturnType<WriteBarrier::Type::kNone>(params);
    }
    HeapHandle& handle = callback();
    if (V8_UNLIKELY(!subtle::HeapState::IsMarking(handle))) {
      return SetAndReturnType<WriteBarrier::Type::kNone>(params);
    }
#endif  // !CPPGC_YOUNG_GENERATION
    params.heap = &handle;
    return SetAndReturnType<WriteBarrier::Type::kMarking>(params);
  }
};

#endif  // CPPGC_CAGED_HEAP

class V8_EXPORT WriteBarrierTypeForNonCagedHeapPolicy final {
 public:
  template <WriteBarrier::ValueMode value_mode, typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type Get(const void* slot, const void* value,
                                          WriteBarrier::Params& params,
                                          HeapHandleCallback callback) {
    return ValueModeDispatch<value_mode>::Get(slot, value, params, callback);
  }

  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type GetForExternallyReferenced(
      const void* value, WriteBarrier::Params& params,
      HeapHandleCallback callback) {
    // The slot will never be used in `Get()` below.
    return Get<WriteBarrier::ValueMode::kValuePresent>(nullptr, value, params,
                                                       callback);
  }

 private:
  template <WriteBarrier::ValueMode value_mode>
  struct ValueModeDispatch;

  // TODO(chromium:1056170): Create fast path on API.
  static bool IsMarking(const void*, HeapHandle**);
  // TODO(chromium:1056170): Create fast path on API.
  static bool IsMarking(HeapHandle&);

  WriteBarrierTypeForNonCagedHeapPolicy() = delete;
};

template <>
struct WriteBarrierTypeForNonCagedHeapPolicy::ValueModeDispatch<
    WriteBarrier::ValueMode::kValuePresent> {
  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type Get(const void*, const void* object,
                                          WriteBarrier::Params& params,
                                          HeapHandleCallback callback) {
    // The following check covers nullptr as well as sentinel pointer.
    if (object <= static_cast<void*>(kSentinelPointer)) {
      return WriteBarrier::Type::kNone;
    }
    if (IsMarking(object, &params.heap)) {
      return SetAndReturnType<WriteBarrier::Type::kMarking>(params);
    }
    return SetAndReturnType<WriteBarrier::Type::kNone>(params);
  }
};

template <>
struct WriteBarrierTypeForNonCagedHeapPolicy::ValueModeDispatch<
    WriteBarrier::ValueMode::kNoValuePresent> {
  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrier::Type Get(const void*, const void*,
                                          WriteBarrier::Params& params,
                                          HeapHandleCallback callback) {
    if (V8_UNLIKELY(WriteBarrier::IsAnyIncrementalOrConcurrentMarking())) {
      HeapHandle& handle = callback();
      if (IsMarking(handle)) {
        params.heap = &handle;
        return SetAndReturnType<WriteBarrier::Type::kMarking>(params);
      }
    }
    return WriteBarrier::Type::kNone;
  }
};

// static
WriteBarrier::Type WriteBarrier::GetWriteBarrierType(
    const void* slot, const void* value, WriteBarrier::Params& params) {
  return WriteBarrierTypePolicy::Get<ValueMode::kValuePresent>(slot, value,
                                                               params, []() {});
}

// static
template <typename HeapHandleCallback>
WriteBarrier::Type WriteBarrier::GetWriteBarrierType(
    const void* slot, WriteBarrier::Params& params,
    HeapHandleCallback callback) {
  return WriteBarrierTypePolicy::Get<ValueMode::kNoValuePresent>(
      slot, nullptr, params, callback);
}

// static
template <typename HeapHandleCallback>
WriteBarrier::Type
WriteBarrier::GetWriteBarrierTypeForExternallyReferencedObject(
    const void* value, Params& params, HeapHandleCallback callback) {
  return WriteBarrierTypePolicy::GetForExternallyReferenced(value, params,
                                                            callback);
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
                                               const void* first_element,
                                               size_t element_size,
                                               size_t number_of_elements,
                                               TraceCallback trace_callback) {
  CheckParams(Type::kMarking, params);
  DijkstraMarkingBarrierRangeSlow(*params.heap, first_element, element_size,
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
