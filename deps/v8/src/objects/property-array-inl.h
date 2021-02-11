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

SMI_ACCESSORS(PropertyArray, length_and_hash, kLengthAndHashOffset)
SYNCHRONIZED_SMI_ACCESSORS(PropertyArray, length_and_hash, kLengthAndHashOffset)

Object PropertyArray::get(int index) const {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return get(isolate, index);
}

Object PropertyArray::get(IsolateRoot isolate, int index) const {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length()));
  return TaggedField<Object>::Relaxed_Load(isolate, *this,
                                           OffsetOfElementAt(index));
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
  return LengthField::decode(length_and_hash());
}

void PropertyArray::initialize_length(int len) {
  DCHECK(LengthField::is_valid(len));
  set_length_and_hash(len);
}

int PropertyArray::synchronized_length() const {
  return LengthField::decode(synchronized_length_and_hash());
}

int PropertyArray::Hash() const { return HashField::decode(length_and_hash()); }

void PropertyArray::SetHash(int hash) {
  int value = length_and_hash();
  value = HashField::update(value, hash);
  set_length_and_hash(value);
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
