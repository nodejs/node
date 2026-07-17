// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_INL_H_
#define V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_INL_H_

#include "src/objects/property-descriptor-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/property-descriptor-object-tq-inl.inc"

int PropertyDescriptorObject::flags() const { return flags_.load().value(); }
void PropertyDescriptorObject::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<Object> PropertyDescriptorObject::value() const { return value_.load(); }
void PropertyDescriptorObject::set_value(Tagged<Object> value,
                                         WriteBarrierMode mode) {
  value_.store(this, value, mode);
}

Tagged<Object> PropertyDescriptorObject::get() const { return get_.load(); }
void PropertyDescriptorObject::set_get(Tagged<Object> value,
                                       WriteBarrierMode mode) {
  get_.store(this, value, mode);
}

Tagged<Object> PropertyDescriptorObject::set() const { return set_.load(); }
void PropertyDescriptorObject::set_set(Tagged<Object> value,
                                       WriteBarrierMode mode) {
  set_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_DESCRIPTOR_OBJECT_INL_H_
