// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRUCT_INL_H_
#define V8_OBJECTS_STRUCT_INL_H_

#include "src/objects/struct.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/struct-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(Struct)

Tagged<Object> Tuple2::value1() const { return value1_.load(); }
void Tuple2::set_value1(Tagged<Object> value, WriteBarrierMode mode) {
  value1_.store(this, value, mode);
}

Tagged<Object> Tuple2::value2() const { return value2_.load(); }
void Tuple2::set_value2(Tagged<Object> value, WriteBarrierMode mode) {
  value2_.store(this, value, mode);
}

NEVER_READ_ONLY_SPACE_IMPL(AccessorPair)

Tagged<Object> AccessorPair::get(AccessorComponent component) {
  return component == ACCESSOR_GETTER ? getter() : setter();
}

void AccessorPair::set(AccessorComponent component, Tagged<Object> value) {
  if (component == ACCESSOR_GETTER) {
    set_getter(value);
  } else {
    set_setter(value);
  }
}

void AccessorPair::set(AccessorComponent component, Tagged<Object> value,
                       ReleaseStoreTag tag) {
  if (component == ACCESSOR_GETTER) {
    set_getter(value, tag);
  } else {
    set_setter(value, tag);
  }
}

Tagged<Object> AccessorPair::getter() const { return getter_.load(); }
void AccessorPair::set_getter(Tagged<Object> value, WriteBarrierMode mode) {
  getter_.store(this, value, mode);
}

Tagged<Object> AccessorPair::getter(AcquireLoadTag) const {
  return getter_.Acquire_Load();
}
void AccessorPair::set_getter(Tagged<Object> value, ReleaseStoreTag,
                              WriteBarrierMode mode) {
  getter_.Release_Store(this, value, mode);
}

Tagged<Object> AccessorPair::setter() const { return setter_.load(); }
void AccessorPair::set_setter(Tagged<Object> value, WriteBarrierMode mode) {
  setter_.store(this, value, mode);
}

Tagged<Object> AccessorPair::setter(AcquireLoadTag) const {
  return setter_.Acquire_Load();
}
void AccessorPair::set_setter(Tagged<Object> value, ReleaseStoreTag,
                              WriteBarrierMode mode) {
  setter_.Release_Store(this, value, mode);
}

void AccessorPair::SetComponents(Tagged<Object> getter, Tagged<Object> setter) {
  if (!IsNull(getter)) set_getter(getter);
  if (!IsNull(setter)) set_setter(setter);
}

bool AccessorPair::Equals(Tagged<Object> getter_value,
                          Tagged<Object> setter_value) {
  return (getter() == getter_value) && (setter() == setter_value);
}

int ClassPositions::start() const { return start_.load().value(); }
void ClassPositions::set_start(int value) {
  start_.store(this, Smi::FromInt(value));
}

int ClassPositions::end() const { return end_.load().value(); }
void ClassPositions::set_end(int value) {
  end_.store(this, Smi::FromInt(value));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRUCT_INL_H_
