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

#include "torque-generated/src/objects/property-array-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(PropertyArray)

SMI_ACCESSORS(PropertyArray, length_and_hash, kLengthAndHashOffset)
RELEASE_ACQUIRE_SMI_ACCESSORS(PropertyArray, length_and_hash,
                              kLengthAndHashOffset)

Object PropertyArray::get(int index) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index);
}

Object PropertyArray::get(PtrComprCageBase cage_base, int index) const {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length(kAcquireLoad)));
  return TaggedField<Object>::Relaxed_Load(cage_base, *this,
                                           OffsetOfElementAt(index));
}

Object PropertyArray::get(int index, SeqCstAccessTag tag) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index, tag);
}

Object PropertyArray::get(PtrComprCageBase cage_base, int index,
                          SeqCstAccessTag tag) const {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length(kAcquireLoad)));
  return TaggedField<Object>::SeqCst_Load(cage_base, *this,
                                          OffsetOfElementAt(index));
}

void PropertyArray::set(int index, Object value) {
  DCHECK(IsPropertyArray());
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length(kAcquireLoad)));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  WRITE_BARRIER(*this, offset, value);
}

void PropertyArray::set(int index, Object value, WriteBarrierMode mode) {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length(kAcquireLoad)));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

void PropertyArray::set(int index, Object value, SeqCstAccessTag tag) {
  DCHECK(IsPropertyArray());
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length(kAcquireLoad)));
  DCHECK(value.IsShared());
  int offset = OffsetOfElementAt(index);
  SEQ_CST_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, UPDATE_WRITE_BARRIER);
}

Object PropertyArray::Swap(int index, Object value, SeqCstAccessTag tag) {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return Swap(cage_base, index, value, tag);
}

Object PropertyArray::Swap(PtrComprCageBase cage_base, int index, Object value,
                           SeqCstAccessTag tag) {
  DCHECK(IsPropertyArray());
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(this->length(kAcquireLoad)));
  DCHECK(value.IsShared());
  Object result = TaggedField<Object>::SeqCst_Swap(
      cage_base, *this, OffsetOfElementAt(index), value);
  CONDITIONAL_WRITE_BARRIER(*this, OffsetOfElementAt(index), value,
                            UPDATE_WRITE_BARRIER);
  return result;
}

ObjectSlot PropertyArray::data_start() { return RawField(kHeaderSize); }

int PropertyArray::length() const {
  return LengthField::decode(length_and_hash());
}

void PropertyArray::initialize_length(int len) {
  DCHECK(LengthField::is_valid(len));
  set_length_and_hash(len);
}

int PropertyArray::length(AcquireLoadTag) const {
  return LengthField::decode(length_and_hash(kAcquireLoad));
}

int PropertyArray::Hash() const { return HashField::decode(length_and_hash()); }

void PropertyArray::SetHash(int hash) {
  int value = length_and_hash();
  value = HashField::update(value, hash);
  set_length_and_hash(value, kReleaseStore);
}

void PropertyArray::CopyElements(Isolate* isolate, int dst_index,
                                 PropertyArray src, int src_index, int len,
                                 WriteBarrierMode mode) {
  if (len == 0) return;
  DisallowGarbageCollection no_gc;

  ObjectSlot dst_slot(data_start() + dst_index);
  ObjectSlot src_slot(src.data_start() + src_index);
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_ARRAY_INL_H_
