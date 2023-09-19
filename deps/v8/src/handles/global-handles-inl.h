// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_GLOBAL_HANDLES_INL_H_
#define V8_HANDLES_GLOBAL_HANDLES_INL_H_

#include "src/handles/global-handles.h"
#include "src/handles/handles-inl.h"
#include "src/objects/heap-object-inl.h"

namespace v8 {
namespace internal {

template <typename T>
Handle<T> GlobalHandles::Create(T value) {
  static_assert(std::is_base_of<Object, T>::value, "static type violation");
  // The compiler should only pick this method if T is not Object.
  static_assert(!std::is_same<Object, T>::value, "compiler error");
  return Handle<T>::cast(Create(Object(value)));
}

template <typename T>
T GlobalHandleVector<T>::Pop() {
  T obj = T::cast(Object(locations_.back()));
  locations_.pop_back();
  return obj;
}

template <typename T>
GlobalHandleVector<T>::GlobalHandleVector(LocalHeap* local_heap)
    : GlobalHandleVector(local_heap->AsHeap()) {}

template <typename T>
GlobalHandleVector<T>::GlobalHandleVector(Heap* heap)
    : locations_(StrongRootBlockAllocator(heap)) {}

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_GLOBAL_HANDLES_INL_H_
