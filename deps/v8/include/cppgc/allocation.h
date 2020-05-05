// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_ALLOCATION_H_
#define INCLUDE_CPPGC_ALLOCATION_H_

#include <stdint.h>
#include <atomic>

#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/gc-info.h"
#include "include/cppgc/heap.h"
#include "include/cppgc/internals.h"

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

  friend class HeapObjectHeader;
};

}  // namespace internal

// Users with custom allocation needs (e.g. overriding size) should override
// MakeGarbageCollectedTrait (see below) and inherit their trait from
// MakeGarbageCollectedTraitBase to get access to low-level primitives.
template <typename T>
class MakeGarbageCollectedTraitBase
    : private internal::MakeGarbageCollectedTraitInternal {
 protected:
  // Allocates an object of |size| bytes on |heap|.
  //
  // TODO(mlippautz): Allow specifying arena for specific embedder uses.
  static void* Allocate(Heap* heap, size_t size) {
    return internal::MakeGarbageCollectedTraitInternal::Allocate(
        heap, size, internal::GCInfoTrait<T>::Index());
  }

  // Marks an object as being fully constructed, resulting in precise handling
  // by the garbage collector.
  static void MarkObjectAsFullyConstructed(const void* payload) {
    // internal::MarkObjectAsFullyConstructed(payload);
    internal::MakeGarbageCollectedTraitInternal::MarkObjectAsFullyConstructed(
        payload);
  }
};

template <typename T>
class MakeGarbageCollectedTrait : public MakeGarbageCollectedTraitBase<T> {
 public:
  template <typename... Args>
  static T* Call(Heap* heap, Args&&... args) {
    static_assert(internal::IsGarbageCollectedType<T>::value,
                  "T needs to be a garbage collected object");
    void* memory = MakeGarbageCollectedTraitBase<T>::Allocate(heap, sizeof(T));
    T* object = ::new (memory) T(std::forward<Args>(args)...);
    MakeGarbageCollectedTraitBase<T>::MarkObjectAsFullyConstructed(object);
    return object;
  }
};

// Default MakeGarbageCollected: Constructs an instance of T, which is a garbage
// collected type.
template <typename T, typename... Args>
T* MakeGarbageCollected(Heap* heap, Args&&... args) {
  return MakeGarbageCollectedTrait<T>::Call(heap, std::forward<Args>(args)...);
}

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_ALLOCATION_H_
