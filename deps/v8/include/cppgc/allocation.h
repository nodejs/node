// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_ALLOCATION_H_
#define INCLUDE_CPPGC_ALLOCATION_H_

#include <stdint.h>

#include <atomic>

#include "cppgc/custom-space.h"
#include "cppgc/garbage-collected.h"
#include "cppgc/heap.h"
#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/gc-info.h"

namespace cppgc {

template <typename T>
class MakeGarbageCollectedTraitBase;

namespace internal {

class V8_EXPORT MakeGarbageCollectedTraitInternal {
 protected:
  static inline void MarkObjectAsFullyConstructed(const void* payload) {
    // See api_constants for an explanation of the constants.
    std::atomic<uint16_t>* atomic_mutable_bitfield =
        reinterpret_cast<std::atomic<uint16_t>*>(
            const_cast<uint16_t*>(reinterpret_cast<const uint16_t*>(
                reinterpret_cast<const uint8_t*>(payload) -
                api_constants::kFullyConstructedBitFieldOffsetFromPayload)));
    uint16_t value = atomic_mutable_bitfield->load(std::memory_order_relaxed);
    value = value | api_constants::kFullyConstructedBitMask;
    atomic_mutable_bitfield->store(value, std::memory_order_release);
  }

  static void* Allocate(cppgc::Heap* heap, size_t size, GCInfoIndex index);
  static void* Allocate(cppgc::Heap* heapx, size_t size, GCInfoIndex index,
                        CustomSpaceIndex space_inde);

  friend class HeapObjectHeader;
};

}  // namespace internal

/**
 * Base trait that provides utilities for advancers users that have custom
 * allocation needs (e.g., overriding size). It's expected that users override
 * MakeGarbageCollectedTrait (see below) and inherit from
 * MakeGarbageCollectedTraitBase and make use of the low-level primitives
 * offered to allocate and construct an object.
 */
template <typename T>
class MakeGarbageCollectedTraitBase
    : private internal::MakeGarbageCollectedTraitInternal {
 private:
  template <typename U, typename CustomSpace>
  struct SpacePolicy {
    static void* Allocate(Heap* heap, size_t size) {
      // Custom space.
      static_assert(std::is_base_of<CustomSpaceBase, CustomSpace>::value,
                    "Custom space must inherit from CustomSpaceBase.");
      return internal::MakeGarbageCollectedTraitInternal::Allocate(
          heap, size, internal::GCInfoTrait<T>::Index(),
          CustomSpace::kSpaceIndex);
    }
  };

  template <typename U>
  struct SpacePolicy<U, void> {
    static void* Allocate(Heap* heap, size_t size) {
      // Default space.
      return internal::MakeGarbageCollectedTraitInternal::Allocate(
          heap, size, internal::GCInfoTrait<T>::Index());
    }
  };

 protected:
  /**
   * Allocates memory for an object of type T.
   *
   * \param heap The heap to allocate this object on.
   * \param size The size that should be reserved for the object.
   * \returns the memory to construct an object of type T on.
   */
  static void* Allocate(Heap* heap, size_t size) {
    return SpacePolicy<T, typename SpaceTrait<T>::Space>::Allocate(heap, size);
  }

  /**
   * Marks an object as fully constructed, resulting in precise handling by the
   * garbage collector.
   *
   * \param payload The base pointer the object is allocated at.
   */
  static void MarkObjectAsFullyConstructed(const void* payload) {
    internal::MakeGarbageCollectedTraitInternal::MarkObjectAsFullyConstructed(
        payload);
  }
};

/**
 * Default trait class that specifies how to construct an object of type T.
 * Advanced users may override how an object is constructed using the utilities
 * that are provided through MakeGarbageCollectedTraitBase.
 *
 * Any trait overriding construction must
 * - allocate through MakeGarbageCollectedTraitBase<T>::Allocate;
 * - mark the object as fully constructed using
 *   MakeGarbageCollectedTraitBase<T>::MarkObjectAsFullyConstructed;
 */
template <typename T>
class MakeGarbageCollectedTrait : public MakeGarbageCollectedTraitBase<T> {
 public:
  template <typename... Args>
  static T* Call(Heap* heap, Args&&... args) {
    static_assert(internal::IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    static_assert(
        !internal::IsGarbageCollectedMixinType<T>::value ||
            sizeof(T) <= internal::api_constants::kLargeObjectSizeThreshold,
        "GarbageCollectedMixin may not be a large object");
    void* memory = MakeGarbageCollectedTraitBase<T>::Allocate(heap, sizeof(T));
    T* object = ::new (memory) T(std::forward<Args>(args)...);
    MakeGarbageCollectedTraitBase<T>::MarkObjectAsFullyConstructed(object);
    return object;
  }
};

/**
 * Allows users to specify a post-construction callback for specific types. The
 * callback is invoked on the instance of type T right after it has been
 * constructed. This can be useful when the callback requires a
 * fully-constructed object to be able to dispatch to virtual methods.
 */
template <typename T, typename = void>
struct PostConstructionCallbackTrait {
  static void Call(T*) {}
};

/**
 * Constructs a managed object of type T where T transitively inherits from
 * GarbageCollected.
 *
 * \param args List of arguments with which an instance of T will be
 *   constructed.
 * \returns an instance of type T.
 */
template <typename T, typename... Args>
T* MakeGarbageCollected(Heap* heap, Args&&... args) {
  T* object =
      MakeGarbageCollectedTrait<T>::Call(heap, std::forward<Args>(args)...);
  PostConstructionCallbackTrait<T>::Call(object);
  return object;
}

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_ALLOCATION_H_
