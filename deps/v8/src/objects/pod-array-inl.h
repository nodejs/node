// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_POD_ARRAY_INL_H_
#define V8_OBJECTS_POD_ARRAY_INL_H_

#include "src/objects/pod-array.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/numerics/checked_math.h"
#include "src/heap/factory.h"
#include "src/heap/local-factory.h"
#include "src/objects/fixed-primitive-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

template <class T, class Super>
SafeHeapObjectSize PodArrayBase<T, Super>::length() const {
  return SafeHeapObjectSize(Super::length().value() / sizeof(T));
}

// static
template <class T>
Handle<PodArray<T>> PodArray<T>::New(Isolate* isolate, uint32_t length,
                                     AllocationType allocation) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return Cast<PodArray<T>>(
      isolate->factory()->NewByteArray(byte_length, allocation));
}

// static
template <class T>
Handle<PodArray<T>> PodArray<T>::New(LocalIsolate* isolate, uint32_t length,
                                     AllocationType allocation) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return Cast<PodArray<T>>(
      isolate->factory()->NewByteArray(byte_length, allocation));
}

// static
template <class T>
DirectHandle<TrustedPodArray<T>> TrustedPodArray<T>::New(
    Isolate* isolate, uint32_t length, AllocationType allocation_type) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return TrustedCast<TrustedPodArray<T>>(
      isolate->factory()->NewTrustedByteArray(byte_length, allocation_type));
}

// static
template <class T>
DirectHandle<TrustedPodArray<T>> TrustedPodArray<T>::New(
    LocalIsolate* isolate, uint32_t length, AllocationType allocation_type) {
  uint32_t byte_length;
  base::internal::CheckedNumeric<uint32_t> checked_byte_length = length;
  checked_byte_length *= sizeof(T);
  CHECK(checked_byte_length.AssignIfValid(&byte_length));
  return TrustedCast<TrustedPodArray<T>>(
      isolate->factory()->NewTrustedByteArray(byte_length, allocation_type));
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_POD_ARRAY_INL_H_
