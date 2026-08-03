// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CPP_HEAP_OBJECT_WRAPPER_H_
#define V8_OBJECTS_CPP_HEAP_OBJECT_WRAPPER_H_

#include "src/objects/cpp-heap-external-object.h"
#include "src/objects/js-objects.h"
#include "src/objects/union.h"
#include "src/sandbox/isolate.h"

namespace v8::internal {

// Object types that can have a cpp heap object pointer field.
using CppHeapPointerWrapperObjectT = UnionOf<JSObject, CppHeapExternalObject>;

// Helper union that doesn't actually exist as type. Used to get and set the cpp
// heap pointer for objects that are associated with one. Use by value.
class CppHeapObjectWrapper {
 public:
  V8_INLINE static CppHeapObjectWrapper From(
      Tagged<CppHeapPointerWrapperObjectT> object);

  V8_INLINE explicit CppHeapObjectWrapper(Tagged<JSObject> object);
  V8_INLINE explicit CppHeapObjectWrapper(Tagged<CppHeapExternalObject> object);

  V8_INLINE void InitializeCppHeapWrapper();

  template <CppHeapPointerTag tag>
  V8_INLINE void SetCppHeapWrappable(IsolateForPointerCompression isolate,
                                     void*);
  V8_INLINE void SetCppHeapWrappable(IsolateForPointerCompression isolate,
                                     void*, CppHeapPointerTag tag);
  template <CppHeapPointerTag lower_bound, CppHeapPointerTag upper_bound>
  V8_INLINE void* GetCppHeapWrappable(
      IsolateForPointerCompression isolate) const;
  V8_INLINE void* GetCppHeapWrappable(IsolateForPointerCompression isolate,
                                      CppHeapPointerTagRange tag_range) const;

 private:
  static_assert(JSAPIObjectWithEmbedderSlots::kCppHeapWrappableOffset ==
                JSSpecialObject::kCppHeapWrappableOffset);

  Tagged<CppHeapPointerWrapperObjectT> object_;
  int offset_;
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_CPP_HEAP_OBJECT_WRAPPER_H_
