// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_INL_H_
#define V8_OBJECTS_SLOTS_INL_H_

#include "src/objects/slots.h"

#include "src/base/atomic-utils.h"
#include "src/memcopy.h"
#include "src/objects.h"
#include "src/objects/heap-object.h"
#include "src/objects/maybe-object.h"

namespace v8 {
namespace internal {

//
// FullObjectSlot implementation.
//

FullObjectSlot::FullObjectSlot(ObjectPtr* object)
    : SlotBase(reinterpret_cast<Address>(&object->ptr_)) {}

bool FullObjectSlot::contains_value(Address raw_value) const {
  return base::AsAtomicPointer::Relaxed_Load(location()) == raw_value;
}

Object* FullObjectSlot::operator*() const {
  return reinterpret_cast<Object*>(*location());
}

ObjectPtr FullObjectSlot::load() const { return ObjectPtr(*location()); }

void FullObjectSlot::store(Object* value) const { *location() = value->ptr(); }

void FullObjectSlot::store(ObjectPtr value) const { *location() = value.ptr(); }

ObjectPtr FullObjectSlot::Acquire_Load() const {
  return ObjectPtr(base::AsAtomicPointer::Acquire_Load(location()));
}

Object* FullObjectSlot::Acquire_Load1() const {
  return reinterpret_cast<Object*>(
      base::AsAtomicPointer::Acquire_Load(location()));
}

ObjectPtr FullObjectSlot::Relaxed_Load() const {
  return ObjectPtr(base::AsAtomicPointer::Relaxed_Load(location()));
}

void FullObjectSlot::Relaxed_Store(ObjectPtr value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value->ptr());
}

void FullObjectSlot::Relaxed_Store1(Object* value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value->ptr());
}

void FullObjectSlot::Release_Store1(Object* value) const {
  base::AsAtomicPointer::Release_Store(location(), value->ptr());
}

void FullObjectSlot::Release_Store(ObjectPtr value) const {
  base::AsAtomicPointer::Release_Store(location(), value->ptr());
}

ObjectPtr FullObjectSlot::Release_CompareAndSwap(ObjectPtr old,
                                                 ObjectPtr target) const {
  Address result = base::AsAtomicPointer::Release_CompareAndSwap(
      location(), old->ptr(), target->ptr());
  return ObjectPtr(result);
}

//
// FullMaybeObjectSlot implementation.
//

MaybeObject FullMaybeObjectSlot::operator*() const {
  return MaybeObject(*location());
}

MaybeObject FullMaybeObjectSlot::load() const {
  return MaybeObject(*location());
}

void FullMaybeObjectSlot::store(MaybeObject value) const {
  *location() = value.ptr();
}

MaybeObject FullMaybeObjectSlot::Relaxed_Load() const {
  return MaybeObject(AsAtomicTagged::Relaxed_Load(location()));
}

void FullMaybeObjectSlot::Relaxed_Store(MaybeObject value) const {
  AsAtomicTagged::Relaxed_Store(location(), value->ptr());
}

void FullMaybeObjectSlot::Release_CompareAndSwap(MaybeObject old,
                                                 MaybeObject target) const {
  AsAtomicTagged::Release_CompareAndSwap(location(), old.ptr(), target.ptr());
}

//
// FullHeapObjectSlot implementation.
//

HeapObjectReference FullHeapObjectSlot::operator*() const {
  return HeapObjectReference(*location());
}

void FullHeapObjectSlot::store(HeapObjectReference value) const {
  *location() = value.ptr();
}

//
// Utils.
//

// Sets |counter| number of kTaggedSize-sized values starting at |start| slot.
inline void MemsetTagged(ObjectSlot start, Object* value, size_t counter) {
  // TODO(ishell): revisit this implementation, maybe use "rep stosl"
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  MemsetPointer(start.location(), reinterpret_cast<Address>(value), counter);
}

// Sets |counter| number of kSystemPointerSize-sized values starting at |start|
// slot.
inline void MemsetPointer(FullObjectSlot start, Object* value, size_t counter) {
  MemsetPointer(start.location(), reinterpret_cast<Address>(value), counter);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_INL_H_
