// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_HEAP_CONSISTENCY_H_
#define INCLUDE_CPPGC_HEAP_CONSISTENCY_H_

#include <cstddef>

#include "cppgc/internal/write-barrier.h"
#include "cppgc/trace-trait.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

class HeapHandle;

namespace subtle {

/**
 * **DO NOT USE: Use the appropriate managed types.**
 *
 * Consistency helpers that aid in maintaining a consistent internal state of
 * the garbage collector.
 */
class HeapConsistency final {
 public:
  using WriteBarrierParams = internal::WriteBarrier::Params;
  using WriteBarrierType = internal::WriteBarrier::Type;

  /**
   * Gets the required write barrier type for a specific write.
   *
   * \param slot Slot containing the pointer to the object. The slot itself
   *   must reside in an object that has been allocated using
   *   `MakeGarbageCollected()`.
   * \param value The pointer to the object. May be an interior pointer to an
   *   interface of the actual object.
   * \param params Parameters that may be used for actual write barrier calls.
   *   Only filled if return value indicates that a write barrier is needed. The
   *   contents of the `params` are an implementation detail.
   * \returns whether a write barrier is needed and which barrier to invoke.
   */
  static V8_INLINE WriteBarrierType GetWriteBarrierType(
      const void* slot, const void* value, WriteBarrierParams& params) {
    return internal::WriteBarrier::GetWriteBarrierType(slot, value, params);
  }

  /**
   * Gets the required write barrier type for a specific write.
   *
   * \param slot Slot containing the pointer to some part of an object object
   *   that has been allocated using `MakeGarbageCollected()`. Does not consider
   *   the value of `slot`.
   * \param params Parameters that may be used for actual write barrier calls.
   *   Only filled if return value indicates that a write barrier is needed. The
   *   contents of the `params` are an implementation detail.
   * \returns whether a write barrier is needed and which barrier to invoke.
   */
  static V8_INLINE WriteBarrierType
  GetWriteBarrierType(const void* slot, WriteBarrierParams& params) {
    return internal::WriteBarrier::GetWriteBarrierType(slot, params);
  }

  /**
   * Conservative Dijkstra-style write barrier that processes an object if it
   * has not yet been processed.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param object The pointer to the object. May be an interior pointer to a
   *   an interface of the actual object.
   */
  static V8_INLINE void DijkstraWriteBarrier(const WriteBarrierParams& params,
                                             const void* object) {
    internal::WriteBarrier::DijkstraMarkingBarrier(params, object);
  }

  /**
   * Conservative Dijkstra-style write barrier that processes a range of
   * elements if they have not yet been processed.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param heap The corresponding heap.
   * \param first_element Pointer to the first element that should be processed.
   *   The slot itself must reside in an object that has been allocated using
   *   `MakeGarbageCollected()`.
   * \param element_size Size of the element in bytes.
   * \param number_of_elements Number of elements that should be processed,
   *   starting with `first_element`.
   * \param trace_callback The trace callback that should be invoked for each
   *   element if necessary.
   */
  static V8_INLINE void DijkstraWriteBarrierRange(
      const WriteBarrierParams& params, HeapHandle& heap,
      const void* first_element, size_t element_size, size_t number_of_elements,
      TraceCallback trace_callback) {
    internal::WriteBarrier::DijkstraMarkingBarrierRange(
        params, heap, first_element, element_size, number_of_elements,
        trace_callback);
  }

  /**
   * Steele-style write barrier that re-processes an object if it has already
   * been processed.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param object The pointer to the object which must point to an object that
   *   has been allocated using `MakeGarbageCollected()`. Interior pointers are
   *   not supported.
   */
  static V8_INLINE void SteeleWriteBarrier(const WriteBarrierParams& params,
                                           const void* object) {
    internal::WriteBarrier::SteeleMarkingBarrier(params, object);
  }

  /**
   * Generational barrier for maintaining consistency when running with multiple
   * generations.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param slot Slot containing the pointer to the object. The slot itself
   *   must reside in an object that has been allocated using
   *   `MakeGarbageCollected()`.
   */
  static V8_INLINE void GenerationalBarrier(const WriteBarrierParams& params,
                                            const void* slot) {
    internal::WriteBarrier::GenerationalBarrier(params, slot);
  }

 private:
  HeapConsistency() = delete;
};

}  // namespace subtle
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_HEAP_CONSISTENCY_H_
