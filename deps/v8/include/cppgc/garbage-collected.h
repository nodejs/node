// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_GARBAGE_COLLECTED_H_
#define INCLUDE_CPPGC_GARBAGE_COLLECTED_H_

#include <type_traits>

#include "include/cppgc/internals.h"
#include "include/cppgc/platform.h"

namespace cppgc {
namespace internal {

template <typename T, typename = void>
struct IsGarbageCollectedType : std::false_type {
  static_assert(sizeof(T), "T must be fully defined");
};

template <typename T>
struct IsGarbageCollectedType<
    T, void_t<typename std::remove_const_t<T>::IsGarbageCollectedTypeMarker>>
    : std::true_type {
  static_assert(sizeof(T), "T must be fully defined");
};

}  // namespace internal

template <typename>
class GarbageCollected {
 public:
  using IsGarbageCollectedTypeMarker = void;

  // Must use MakeGarbageCollected.
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
  // The garbage collector is taking care of reclaiming the object. Also,
  // virtual destructor requires an unambiguous, accessible 'operator delete'.
  void operator delete(void*) {
#ifdef V8_ENABLE_CHECKS
    internal::Abort();
#endif  // V8_ENABLE_CHECKS
  }
  void operator delete[](void*) = delete;

 protected:
  GarbageCollected() = default;
};

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_GARBAGE_COLLECTED_H_
