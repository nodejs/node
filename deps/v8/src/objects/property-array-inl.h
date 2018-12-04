// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_ARRAY_INL_H_
#define V8_OBJECTS_PROPERTY_ARRAY_INL_H_

#include "src/objects/property-array.h"

#include "src/heap/heap-write-barrier-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(PropertyArray)

Object* PropertyArray::get(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LE(index, this->length());
  return RELAXED_READ_FIELD(this, kHeaderSize + index * kPointerSize);
}

void PropertyArray::set(int index, Object* value) {
  DCHECK(IsPropertyArray());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kPointerSize;
  RELAXED_WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(this, offset, value);
}

void PropertyArray::set(int index, Object* value, WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kPointerSize;
  RELAXED_WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(this, offset, value, mode);
}

Object** PropertyArray::data_start() {
  return HeapObject::RawField(this, kHeaderSize);
}

int PropertyArray::length() const {
  Object* value_obj = READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return LengthField::decode(value);
}

void PropertyArray::initialize_length(int len) {
  SLOW_DCHECK(len >= 0);
  SLOW_DCHECK(len < LengthField::kMax);
  WRITE_FIELD(this, kLengthAndHashOffset, Smi::FromInt(len));
}

int PropertyArray::synchronized_length() const {
  Object* value_obj = ACQUIRE_READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return LengthField::decode(value);
}

int PropertyArray::Hash() const {
  Object* value_obj = READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return HashField::decode(value);
}

void PropertyArray::SetHash(int hash) {
  Object* value_obj = READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  value = HashField::update(value, hash);
  WRITE_FIELD(this, kLengthAndHashOffset, Smi::FromInt(value));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_ARRAY_INL_H_
