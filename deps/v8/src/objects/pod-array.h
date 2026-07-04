// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_POD_ARRAY_H_
#define V8_OBJECTS_POD_ARRAY_H_

#include "src/common/globals.h"
#include "src/objects/fixed-primitive-array.h"
#include "src/utils/memcopy.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

V8_OBJECT
template <class T, class Super>
class PodArrayBase : public Super {
 public:
  void copy_out(uint32_t index, T* result, uint32_t length) {
    MemCopy(result, &this->values()[index * sizeof(T)], length * sizeof(T));
  }

  void copy_in(uint32_t index, const T* buffer, uint32_t length) {
    MemCopy(&this->values()[index * sizeof(T)], buffer, length * sizeof(T));
  }

  bool matches(const T* buffer, uint32_t length) {
    DCHECK_LE(length, this->length().value());
    return memcmp(this->begin(), buffer, length * sizeof(T)) == 0;
  }

  bool matches(uint32_t offset, const T* buffer, uint32_t length) {
    DCHECK_LE(offset, this->length().value());
    DCHECK_LE(offset + length, this->length().value());
    return memcmp(this->begin() + sizeof(T) * offset, buffer,
                  length * sizeof(T)) == 0;
  }

  T get(uint32_t index) {
    DCHECK_LT(index, this->length().value());
    T result;
    copy_out(index, &result, 1);
    return result;
  }

  void set(uint32_t index, const T& value) {
    DCHECK_LT(index, this->length().value());
    copy_in(index, &value, 1);
  }

  inline SafeHeapObjectSize length() const;
} V8_OBJECT_END;

// Wrapper class for ByteArray which can store arbitrary C++ classes, as long
// as they can be copied with memcpy.
V8_OBJECT
template <class T>
class PodArray : public PodArrayBase<T, ByteArray> {
 public:
  static Handle<PodArray<T>> New(
      Isolate* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kYoung);
  static Handle<PodArray<T>> New(
      LocalIsolate* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kOld);
} V8_OBJECT_END;

V8_OBJECT
template <class T>
class TrustedPodArray : public PodArrayBase<T, TrustedByteArray> {
 public:
  static DirectHandle<TrustedPodArray<T>> New(
      Isolate* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kTrusted);
  static DirectHandle<TrustedPodArray<T>> New(
      LocalIsolate* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kTrusted);
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_POD_ARRAY_H_
