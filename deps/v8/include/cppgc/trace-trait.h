// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_TRACE_TRAIT_H_
#define INCLUDE_CPPGC_TRACE_TRAIT_H_

#include <type_traits>
#include "cppgc/type-traits.h"

namespace cppgc {

class Visitor;

namespace internal {

template <typename T,
          bool =
              IsGarbageCollectedMixinTypeV<typename std::remove_const<T>::type>>
struct TraceTraitImpl;

}  // namespace internal

using TraceCallback = void (*)(Visitor*, const void*);

// TraceDescriptor is used to describe how to trace an object.
struct TraceDescriptor {
  // The adjusted base pointer of the object that should be traced.
  const void* base_object_payload;
  // A callback for tracing the object.
  TraceCallback callback;
};

template <typename T>
struct TraceTrait {
  static_assert(internal::IsTraceableV<T>, "T must have a Trace() method");

  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return internal::TraceTraitImpl<T>::GetTraceDescriptor(
        static_cast<const T*>(self));
  }

  static void Trace(Visitor* visitor, const void* self) {
    static_cast<const T*>(self)->Trace(visitor);
  }
};

namespace internal {

template <typename T>
struct TraceTraitImpl<T, false> {
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, TraceTrait<T>::Trace};
  }
};

template <typename T>
struct TraceTraitImpl<T, true> {
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return static_cast<const T*>(self)->GetTraceDescriptor();
  }
};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_TRACE_TRAIT_H_
