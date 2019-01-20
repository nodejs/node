// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_INL_H_
#define V8_OBJECTS_SLOTS_INL_H_

#include "src/objects/slots.h"

#include "src/base/atomic-utils.h"
#include "src/objects.h"
#include "src/objects/heap-object.h"
#include "src/objects/maybe-object.h"

namespace v8 {
namespace internal {

ObjectSlot::ObjectSlot(ObjectPtr* object)
    : SlotBase(reinterpret_cast<Address>(&object->ptr_)) {}

void ObjectSlot::store(Object* value) const { *location() = value->ptr(); }

ObjectPtr ObjectSlot::Acquire_Load() const {
  return ObjectPtr(AsAtomicTagged::Acquire_Load(location()));
}

Object* ObjectSlot::Acquire_Load1() const {
  return reinterpret_cast<Object*>(AsAtomicTagged::Acquire_Load(location()));
}

ObjectPtr ObjectSlot::Relaxed_Load() const {
  return ObjectPtr(AsAtomicTagged::Relaxed_Load(location()));
}

void ObjectSlot::Relaxed_Store(ObjectPtr value) const {
  AsAtomicTagged::Relaxed_Store(location(), value->ptr());
}

void ObjectSlot::Relaxed_Store1(Object* value) const {
  AsAtomicTagged::Relaxed_Store(location(), value->ptr());
}

void ObjectSlot::Release_Store1(Object* value) const {
  AsAtomicTagged::Release_Store(location(), value->ptr());
}

void ObjectSlot::Release_Store(ObjectPtr value) const {
  AsAtomicTagged::Release_Store(location(), value->ptr());
}

ObjectPtr ObjectSlot::Release_CompareAndSwap(ObjectPtr old,
                                             ObjectPtr target) const {
  Address result = AsAtomicTagged::Release_CompareAndSwap(
      location(), old->ptr(), target->ptr());
  return ObjectPtr(result);
}

MaybeObject MaybeObjectSlot::operator*() const {
  return MaybeObject(*location());
}

void MaybeObjectSlot::store(MaybeObject value) const {
  *location() = value.ptr();
}

MaybeObject MaybeObjectSlot::Relaxed_Load() const {
  return MaybeObject(AsAtomicTagged::Relaxed_Load(location()));
}

void MaybeObjectSlot::Relaxed_Store(MaybeObject value) const {
  AsAtomicTagged::Relaxed_Store(location(), value->ptr());
}

void MaybeObjectSlot::Release_CompareAndSwap(MaybeObject old,
                                             MaybeObject target) const {
  AsAtomicTagged::Release_CompareAndSwap(location(), old.ptr(), target.ptr());
}

HeapObjectReference HeapObjectSlot::operator*() const {
  return HeapObjectReference(*location());
}

void HeapObjectSlot::store(HeapObjectReference value) const {
  *location() = value.ptr();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_INL_H_
