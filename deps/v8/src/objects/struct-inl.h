// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_INL_H_
#define V8_OBJECTS_STRUCT_INL_H_

#include "src/objects/struct.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/roots/roots-inl.h"
#include "torque-generated/class-definitions-tq-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(Struct)
TQ_OBJECT_CONSTRUCTORS_IMPL(Tuple2)
TQ_OBJECT_CONSTRUCTORS_IMPL(Tuple3)
OBJECT_CONSTRUCTORS_IMPL(AccessorPair, Struct)

TQ_OBJECT_CONSTRUCTORS_IMPL(ClassPositions)

CAST_ACCESSOR(AccessorPair)

void Struct::InitializeBody(int object_size) {
  Object value = GetReadOnlyRoots().undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kTaggedSize) {
    WRITE_FIELD(*this, offset, value);
  }
}

ACCESSORS(AccessorPair, getter, Object, kGetterOffset)
ACCESSORS(AccessorPair, setter, Object, kSetterOffset)

TQ_SMI_ACCESSORS(ClassPositions, start)
TQ_SMI_ACCESSORS(ClassPositions, end)

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
  if (!getter.IsNull()) set_getter(getter);
  if (!setter.IsNull()) set_setter(setter);
}

bool AccessorPair::Equals(Object getter_value, Object setter_value) {
  return (getter() == getter_value) && (setter() == setter_value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_INL_H_
