// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_INL_H_
#define V8_OBJECTS_STRUCT_INL_H_

#include "src/objects/struct.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/oddball.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Struct, HeapObject)
// TODO(jkummerow): Fix IsTuple2() and IsTuple3() to be subclassing-aware,
// or rethink this more generally (see crbug.com/v8/8516).
Tuple2::Tuple2(Address ptr) : Struct(ptr) {}
Tuple3::Tuple3(Address ptr) : Tuple2(ptr) {}
OBJECT_CONSTRUCTORS_IMPL(AccessorPair, Struct)

CAST_ACCESSOR(AccessorPair)
CAST_ACCESSOR(Struct)
CAST_ACCESSOR(Tuple2)
CAST_ACCESSOR(Tuple3)

void Struct::InitializeBody(int object_size) {
  Object value = GetReadOnlyRoots().undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}

ACCESSORS(Tuple2, value1, Object, kValue1Offset)
ACCESSORS(Tuple2, value2, Object, kValue2Offset)
ACCESSORS(Tuple3, value3, Object, kValue3Offset)

ACCESSORS(AccessorPair, getter, Object, kGetterOffset)
ACCESSORS(AccessorPair, setter, Object, kSetterOffset)

Object AccessorPair::get(AccessorComponent component) {
  return component == ACCESSOR_GETTER ? getter() : setter();
}

void AccessorPair::set(AccessorComponent component, Object value) {
  if (component == ACCESSOR_GETTER) {
    set_getter(value);
  } else {
    set_setter(value);
  }
}

void AccessorPair::SetComponents(Object getter, Object setter) {
  if (!getter->IsNull()) set_getter(getter);
  if (!setter->IsNull()) set_setter(setter);
}

bool AccessorPair::Equals(Object getter_value, Object setter_value) {
  return (getter() == getter_value) && (setter() == setter_value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_INL_H_
