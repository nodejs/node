// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_HEAP_EXTERNAL_H_
#define INCLUDE_V8_HEAP_EXTERNAL_H_

#include "cppgc/type-traits.h"  // NOLINT(build/include_directory)
#include "v8-sandbox.h"         // NOLINT(build/include_directory)
#include "v8-value.h"           // NOLINT(build/include_directory)
#include "v8config.h"           // NOLINT(build/include_directory)

namespace v8 {

class Isolate;

/**
 * A JavaScript value that wraps a `cppgc::GarbageCollected<T>` object allocated
 * on the managed C++ heap (CppHeap). This type of value is mainly used to
 * associate C++ data structures which aren't exposed to JavaScript with
 * JavaScript objects.
 */
class V8_EXPORT CppHeapExternal : public Data {
 public:
  template <typename T>
  static Local<CppHeapExternal> New(Isolate* isolate, T* value,
                                    CppHeapPointerTag tag) {
    static_assert(cppgc::IsGarbageCollectedTypeV<T>,
                  "Object must be of type GarbageCollected.");
    return NewImpl(isolate, value, tag);
  }

  V8_INLINE static CppHeapExternal* Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(data);
#endif
    return static_cast<CppHeapExternal*>(data);
  }

  template <typename T>
  T* Value(Isolate* isolate, CppHeapPointerTagRange tag_range) const {
    static_assert(cppgc::IsGarbageCollectedTypeV<T>,
                  "Object must be of type GarbageCollected.");
    return static_cast<T*>(ValueImpl(isolate, tag_range));
  }

 private:
  static void CheckCast(v8::Data* obj);
  static Local<CppHeapExternal> NewImpl(Isolate* isolate, void* value,
                                        CppHeapPointerTag tag);
  void* ValueImpl(Isolate*, CppHeapPointerTagRange tag_range) const;
};

}  // namespace v8

#endif  // INCLUDE_V8_HEAP_EXTERNAL_H_
