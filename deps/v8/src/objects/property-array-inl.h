// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_ARRAY_INL_H_
#define V8_OBJECTS_PROPERTY_ARRAY_INL_H_

#include "src/objects/property-array.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/property-array-tq-inl.inc"

int PropertyArray::length_and_hash() const {
  return length_and_hash_.load().value();
}

int PropertyArray::length_and_hash(AcquireLoadTag) const {
  return length_and_hash_.Acquire_Load().value();
}

void PropertyArray::set_length_and_hash(int value) {
  length_and_hash_.store(this, Smi::FromInt(value));
}

void PropertyArray::set_length_and_hash(int value, ReleaseStoreTag) {
  length_and_hash_.Release_Store(this, Smi::FromInt(value));
}

Tagged<Object> PropertyArray::get(int index) const {
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  return objects()[index].Relaxed_Load();
}

Tagged<Object> PropertyArray::get(int index, SeqCstAccessTag) const {
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  return objects()[index].SeqCst_Load();
}

void PropertyArray::set(int index, Tagged<Object> value) {
  DCHECK(IsPropertyArray(this));
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  objects()[index].Relaxed_Store(this, value);
}

void PropertyArray::set(int index, Tagged<Object> value,
                        WriteBarrierMode mode) {
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  objects()[index].Relaxed_Store(this, value, mode);
}

void PropertyArray::set(int index, Tagged<Object> value, SeqCstAccessTag) {
  DCHECK(IsPropertyArray(this));
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  DCHECK(IsShared(value));
  objects()[index].SeqCst_Store(this, value);
}

Tagged<Object> PropertyArray::Swap(int index, Tagged<Object> value,
                                   SeqCstAccessTag) {
  DCHECK(IsPropertyArray(this));
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  DCHECK(IsShared(value));
  return objects()[index].SeqCst_Swap(this, value);
}

Tagged<Object> PropertyArray::CompareAndSwap(int index, Tagged<Object> expected,
                                             Tagged<Object> value,
                                             SeqCstAccessTag) {
  DCHECK(IsPropertyArray(this));
  DCHECK_LT(static_cast<uint32_t>(index), this->length(kAcquireLoad).value());
  DCHECK(IsShared(value));
  return objects()[index].SeqCst_CompareAndSwap(this, expected, value);
}

ObjectSlot PropertyArray::data_start() { return RawFieldOfElementAt(0); }

// TODO(jgruber): Remove this method entirely; callers should access
// elements directly via objects()[index] when possible.
ObjectSlot PropertyArray::RawFieldOfElementAt(int index) {
  return ObjectSlot(&objects()[index]);
}

SafeHeapObjectSize PropertyArray::length() const {
  int len = LengthField::decode(length_and_hash());
  DCHECK_GE(len, 0);
  return SafeHeapObjectSize(static_cast<uint32_t>(len));
}

void PropertyArray::initialize_length(uint32_t len) {
  DCHECK(LengthField::is_valid(len));
  set_length_and_hash(len);
}

SafeHeapObjectSize PropertyArray::length(AcquireLoadTag) const {
  int len = LengthField::decode(length_and_hash(kAcquireLoad));
  DCHECK_GE(len, 0);
  return SafeHeapObjectSize(static_cast<uint32_t>(len));
}

int PropertyArray::Hash() const { return HashField::decode(length_and_hash()); }

void PropertyArray::SetHash(int hash) {
  int value = length_and_hash();
  value = HashField::update(value, hash);
  set_length_and_hash(value, kReleaseStore);
}

// static
void PropertyArray::CopyElements(Isolate* isolate, Tagged<PropertyArray> dst,
                                 int dst_index, Tagged<PropertyArray> src,
                                 int src_index, int len,
                                 WriteBarrierMode mode) {
  if (len == 0) return;
  DisallowGarbageCollection no_gc;
  ObjectSlot dst_slot(dst->data_start() + dst_index);
  ObjectSlot src_slot(src->data_start() + src_index);
  isolate->heap()->CopyRange(dst, dst_slot, src_slot, len, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_ARRAY_INL_H_
