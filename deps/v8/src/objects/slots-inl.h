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

void ObjectSlot::store(Object* value) {
  *reinterpret_cast<Address*>(address()) = value->ptr();
}

ObjectPtr ObjectSlot::Acquire_Load() const {
  return ObjectPtr(base::AsAtomicWord::Acquire_Load(location()));
}

Object* ObjectSlot::Relaxed_Load() const {
  Address object_ptr = base::AsAtomicWord::Relaxed_Load(location());
  return reinterpret_cast<Object*>(object_ptr);
}

Object* ObjectSlot::Relaxed_Load(int offset) const {
  Address object_ptr = base::AsAtomicWord::Relaxed_Load(
      reinterpret_cast<Address*>(address() + offset * kPointerSize));
  return reinterpret_cast<Object*>(object_ptr);
}

void ObjectSlot::Relaxed_Store(ObjectPtr value) const {
  base::AsAtomicWord::Relaxed_Store(location(), value->ptr());
}

void ObjectSlot::Relaxed_Store(int offset, Object* value) const {
  Address* addr = reinterpret_cast<Address*>(address() + offset * kPointerSize);
  base::AsAtomicWord::Relaxed_Store(addr, value->ptr());
}

void ObjectSlot::Release_Store(ObjectPtr value) const {
  base::AsAtomicWord::Release_Store(location(), value->ptr());
}

ObjectPtr ObjectSlot::Release_CompareAndSwap(ObjectPtr old,
                                             ObjectPtr target) const {
  Address result = base::AsAtomicWord::Release_CompareAndSwap(
      location(), old->ptr(), target->ptr());
  return ObjectPtr(result);
}

MaybeObject MaybeObjectSlot::operator*() {
  return MaybeObject(*reinterpret_cast<Address*>(address()));
}

void MaybeObjectSlot::store(MaybeObject value) {
  *reinterpret_cast<Address*>(address()) = value.ptr();
}

MaybeObject MaybeObjectSlot::Relaxed_Load() const {
  Address object_ptr = base::AsAtomicWord::Relaxed_Load(location());
  return MaybeObject(object_ptr);
}

void MaybeObjectSlot::Release_CompareAndSwap(MaybeObject old,
                                             MaybeObject target) const {
  base::AsAtomicWord::Release_CompareAndSwap(location(), old.ptr(),
                                             target.ptr());
}

HeapObjectReference HeapObjectSlot::operator*() {
  return HeapObjectReference(*reinterpret_cast<Address*>(address()));
}
void HeapObjectSlot::store(HeapObjectReference value) {
  *reinterpret_cast<Address*>(address()) = value.ptr();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_INL_H_
