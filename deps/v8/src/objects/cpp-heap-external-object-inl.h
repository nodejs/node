// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CPP_HEAP_EXTERNAL_OBJECT_INL_H_
#define V8_OBJECTS_CPP_HEAP_EXTERNAL_OBJECT_INL_H_

#include "src/objects/cpp-heap-external-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/cpp-heap-external-object-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(CppHeapExternalObject)

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CPP_HEAP_EXTERNAL_OBJECT_INL_H_
