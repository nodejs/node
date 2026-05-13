// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CPP_HEAP_EXTERNAL_OBJECT_H_
#define V8_OBJECTS_CPP_HEAP_EXTERNAL_OBJECT_H_

#include "src/objects/heap-object.h"
#include "src/sandbox/cppheap-pointer.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/cpp-heap-external-object-tq.inc"

V8_OBJECT class CppHeapExternalObject : public HeapObject {
 public:
  DECL_PRINTER(CppHeapExternalObject)
  DECL_VERIFIER(CppHeapExternalObject)

  class BodyDescriptor;

  static const int kHeaderSize;

 public:
  CppHeapPointerMember cpp_heap_wrappable_;
} V8_OBJECT_END;

inline constexpr int CppHeapExternalObject::kHeaderSize =
    sizeof(CppHeapExternalObject);

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CPP_HEAP_EXTERNAL_OBJECT_H_
