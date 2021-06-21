// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_CPPGC_H_
#define INCLUDE_V8_CPPGC_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "cppgc/common.h"
#include "cppgc/custom-space.h"
#include "cppgc/heap-statistics.h"
#include "cppgc/internal/write-barrier.h"
#include "cppgc/visitor.h"
#include "v8-internal.h"  // NOLINT(build/include_directory)
#include "v8.h"           // NOLINT(build/include_directory)

namespace cppgc {
class AllocationHandle;
class HeapHandle;
}  // namespace cppgc

namespace v8 {

namespace internal {
class CppHeap;
}  // namespace internal

/**
 * Describes how V8 wrapper objects maintain references to garbage-collected C++
 * objects.
 */
struct WrapperDescriptor final {
  /**
   * The index used on `v8::Ojbect::SetAlignedPointerFromInternalField()` and
   * related APIs to add additional data to an object which is used to identify
   * JS->C++ references.
   */
  using InternalFieldIndex = int;

  /**
   * Unknown embedder id. The value is reserved for internal usages and must not
   * be used with `CppHeap`.
   */
  static constexpr uint16_t kUnknownEmbedderId = UINT16_MAX;

  constexpr WrapperDescriptor(InternalFieldIndex wrappable_type_index,
                              InternalFieldIndex wrappable_instance_index,
                              uint16_t embedder_id_for_garbage_collected)
      : wrappable_type_index(wrappable_type_index),
        wrappable_instance_index(wrappable_instance_index),
        embedder_id_for_garbage_collected(embedder_id_for_garbage_collected) {}

  /**
   * Index of the wrappable type.
   */
  InternalFieldIndex wrappable_type_index;

  /**
   * Index of the wrappable instance.
   */
  InternalFieldIndex wrappable_instance_index;

  /**
   * Embedder id identifying instances of garbage-collected objects. It is
   * expected that the first field of the wrappable type is a uint16_t holding
   * the id. Only references to instances of wrappables types with an id of
   * `embedder_id_for_garbage_collected` will be considered by CppHeap.
   */
  uint16_t embedder_id_for_garbage_collected;
};

struct V8_EXPORT CppHeapCreateParams {
  CppHeapCreateParams(const CppHeapCreateParams&) = delete;
  CppHeapCreateParams& operator=(const CppHeapCreateParams&) = delete;

  std::vector<std::unique_ptr<cppgc::CustomSpaceBase>> custom_spaces;
  WrapperDescriptor wrapper_descriptor;
};

/**
 * A heap for allocating managed C++ objects.
 */
class V8_EXPORT CppHeap {
 public:
  static std::unique_ptr<CppHeap> Create(v8::Platform* platform,
                                         const CppHeapCreateParams& params);

  virtual ~CppHeap() = default;

  /**
   * \returns the opaque handle for allocating objects using
   * `MakeGarbageCollected()`.
   */
  cppgc::AllocationHandle& GetAllocationHandle();

  /**
   * \returns the opaque heap handle which may be used to refer to this heap in
   *   other APIs. Valid as long as the underlying `CppHeap` is alive.
   */
  cppgc::HeapHandle& GetHeapHandle();

  /**
   * Terminate clears all roots and performs multiple garbage collections to
   * reclaim potentially newly created objects in destructors.
   *
   * After this call, object allocation is prohibited.
   */
  void Terminate();

  /**
   * \param detail_level specifies whether should return detailed
   *   statistics or only brief summary statistics.
   * \returns current CppHeap statistics regarding memory consumption
   *   and utilization.
   */
  cppgc::HeapStatistics CollectStatistics(
      cppgc::HeapStatistics::DetailLevel detail_level);

  /**
   * Enables a detached mode that allows testing garbage collection using
   * `cppgc::testing` APIs. Once used, the heap cannot be attached to an
   * `Isolate` anymore.
   */
  void EnableDetachedGarbageCollectionsForTesting();

  /**
   * Performs a stop-the-world garbage collection for testing purposes.
   *
   * \param stack_state The stack state to assume for the garbage collection.
   */
  void CollectGarbageForTesting(cppgc::EmbedderStackState stack_state);

 private:
  CppHeap() = default;

  friend class internal::CppHeap;
};

class JSVisitor : public cppgc::Visitor {
 public:
  explicit JSVisitor(cppgc::Visitor::Key key) : cppgc::Visitor(key) {}

  void Trace(const TracedReferenceBase& ref) {
    if (ref.IsEmptyThreadSafe()) return;
    Visit(ref);
  }

 protected:
  using cppgc::Visitor::Visit;

  virtual void Visit(const TracedReferenceBase& ref) {}
};

/**
 * **DO NOT USE: Use the appropriate managed types.**
 *
 * Consistency helpers that aid in maintaining a consistent internal state of
 * the garbage collector.
 */
class V8_EXPORT JSHeapConsistency final {
 public:
  using WriteBarrierParams = cppgc::internal::WriteBarrier::Params;
  using WriteBarrierType = cppgc::internal::WriteBarrier::Type;

  /**
   * Gets the required write barrier type for a specific write.
   *
   * Note: Handling for C++ to JS references.
   *
   * \param ref The reference being written to.
   * \param params Parameters that may be used for actual write barrier calls.
   *   Only filled if return value indicates that a write barrier is needed. The
   *   contents of the `params` are an implementation detail.
   * \param callback Callback returning the corresponding heap handle. The
   *   callback is only invoked if the heap cannot otherwise be figured out. The
   *   callback must not allocate.
   * \returns whether a write barrier is needed and which barrier to invoke.
   */
  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrierType
  GetWriteBarrierType(const TracedReferenceBase& ref,
                      WriteBarrierParams& params, HeapHandleCallback callback) {
    if (ref.IsEmpty()) return WriteBarrierType::kNone;

    if (V8_LIKELY(!cppgc::internal::WriteBarrier::
                      IsAnyIncrementalOrConcurrentMarking())) {
      return cppgc::internal::WriteBarrier::Type::kNone;
    }
    cppgc::HeapHandle& handle = callback();
    if (!cppgc::subtle::HeapState::IsMarking(handle)) {
      return cppgc::internal::WriteBarrier::Type::kNone;
    }
    params.heap = &handle;
#if V8_ENABLE_CHECKS
    params.type = cppgc::internal::WriteBarrier::Type::kMarking;
#endif  // !V8_ENABLE_CHECKS
    return cppgc::internal::WriteBarrier::Type::kMarking;
  }

  /**
   * Gets the required write barrier type for a specific write.
   *
   * Note: Handling for JS to C++ references.
   *
   * \param wrapper The wrapper that has been written into.
   * \param wrapper_index The wrapper index in `wrapper` that has been written
   *   into.
   * \param wrappable The value that was written.
   * \param params Parameters that may be used for actual write barrier calls.
   *   Only filled if return value indicates that a write barrier is needed. The
   *   contents of the `params` are an implementation detail.
   * \param callback Callback returning the corresponding heap handle. The
   *   callback is only invoked if the heap cannot otherwise be figured out. The
   *   callback must not allocate.
   * \returns whether a write barrier is needed and which barrier to invoke.
   */
  template <typename HeapHandleCallback>
  static V8_INLINE WriteBarrierType GetWriteBarrierType(
      v8::Local<v8::Object>& wrapper, int wrapper_index, const void* wrappable,
      WriteBarrierParams& params, HeapHandleCallback callback) {
#if V8_ENABLE_CHECKS
    CheckWrapper(wrapper, wrapper_index, wrappable);
#endif  // V8_ENABLE_CHECKS
    return cppgc::internal::WriteBarrier::
        GetWriteBarrierTypeForExternallyReferencedObject(wrappable, params,
                                                         callback);
  }

  /**
   * Conservative Dijkstra-style write barrier that processes an object if it
   * has not yet been processed.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param ref The reference being written to.
   */
  static V8_INLINE void DijkstraMarkingBarrier(const WriteBarrierParams& params,
                                               cppgc::HeapHandle& heap_handle,
                                               const TracedReferenceBase& ref) {
    cppgc::internal::WriteBarrier::CheckParams(WriteBarrierType::kMarking,
                                               params);
    DijkstraMarkingBarrierSlow(heap_handle, ref);
  }

  /**
   * Conservative Dijkstra-style write barrier that processes an object if it
   * has not yet been processed.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param object The pointer to the object. May be an interior pointer to a
   *   an interface of the actual object.
   */
  static V8_INLINE void DijkstraMarkingBarrier(const WriteBarrierParams& params,
                                               cppgc::HeapHandle& heap_handle,
                                               const void* object) {
    cppgc::internal::WriteBarrier::DijkstraMarkingBarrier(params, object);
  }

  /**
   * Generational barrier for maintaining consistency when running with multiple
   * generations.
   *
   * \param params The parameters retrieved from `GetWriteBarrierType()`.
   * \param ref The reference being written to.
   */
  static V8_INLINE void GenerationalBarrier(const WriteBarrierParams& params,
                                            const TracedReferenceBase& ref) {}

 private:
  JSHeapConsistency() = delete;

  static void CheckWrapper(v8::Local<v8::Object>&, int, const void*);

  static void DijkstraMarkingBarrierSlow(cppgc::HeapHandle&,
                                         const TracedReferenceBase& ref);
};

}  // namespace v8

namespace cppgc {

template <typename T>
struct TraceTrait<v8::TracedReference<T>> {
  static void Trace(Visitor* visitor, const v8::TracedReference<T>* self) {
    static_cast<v8::JSVisitor*>(visitor)->Trace(*self);
  }
};

}  // namespace cppgc

#endif  // INCLUDE_V8_CPPGC_H_
