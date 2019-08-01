// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_ARRAY_INL_H_
#define V8_OBJECTS_PROPERTY_ARRAY_INL_H_

#include "src/objects/property-array.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(PropertyArray, HeapObject)
CAST_ACCESSOR(PropertyArray)

Object PropertyArray::get(int index) const {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length()));
  return RELAXED_READ_FIELD(*this, OffsetOfElementAt(index));
}

void PropertyArray::set(int index, Object value) {
  DCHECK(IsPropertyArray());
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length()));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  WRITE_BARRIER(*this, offset, value);
}

void PropertyArray::set(int index, Object value, WriteBarrierMode mode) {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length()));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

ObjectSlot PropertyArray::data_start() { return RawField(kHeaderSize); }

int PropertyArray::length() const {
  Object value_obj = READ_FIELD(*this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return LengthField::decode(value);
}

void PropertyArray::initialize_length(int len) {
  DCHECK_LT(static_cast<unsigned>(len),
            static_cast<unsigned>(LengthField::kMax));
  WRITE_FIELD(*this, kLengthAndHashOffset, Smi::FromInt(len));
}

int PropertyArray::synchronized_length() const {
  Object value_obj = ACQUIRE_READ_FIELD(*this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return LengthField::decode(value);
}

int PropertyArray::Hash() const {
  Object value_obj = READ_FIELD(*this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return HashField::decode(value);
}

void PropertyArray::SetHash(int hash) {
  Object value_obj = READ_FIELD(*this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  value = HashField::update(value, hash);
  WRITE_FIELD(*this, kLengthAndHashOffset, Smi::FromInt(value));
}

void PropertyArray::CopyElements(Isolate* isolate, int dst_index,
                                 PropertyArray src, int src_index, int len,
                                 WriteBarrierMode mode) {
  if (len == 0) return;
  DisallowHeapAllocation no_gc;

  ObjectSlot dst_slot(data_start() + dst_index);
  ObjectSlot src_slot(src.data_start() + src_index);
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_ARRAY_INL_H_
