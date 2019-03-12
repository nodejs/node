// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_INL_H_
#define V8_OBJECTS_SLOTS_INL_H_

#include "src/objects/slots.h"

#include "src/base/atomic-utils.h"
#include "src/memcopy.h"
#include "src/objects.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/maybe-object.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/ptr-compr-inl.h"
#endif

namespace v8 {
namespace internal {

//
// FullObjectSlot implementation.
//

FullObjectSlot::FullObjectSlot(Object* object)
    : SlotBase(reinterpret_cast<Address>(&object->ptr_)) {}

bool FullObjectSlot::contains_value(Address raw_value) const {
  return base::AsAtomicPointer::Relaxed_Load(location()) == raw_value;
}

Object FullObjectSlot::operator*() const { return Object(*location()); }

void FullObjectSlot::store(Object value) const { *location() = value->ptr(); }

Object FullObjectSlot::Acquire_Load() const {
  return Object(base::AsAtomicPointer::Acquire_Load(location()));
}

Object FullObjectSlot::Relaxed_Load() const {
  return Object(base::AsAtomicPointer::Relaxed_Load(location()));
}

void FullObjectSlot::Relaxed_Store(Object value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value->ptr());
}

void FullObjectSlot::Release_Store(Object value) const {
  base::AsAtomicPointer::Release_Store(location(), value->ptr());
}

Object FullObjectSlot::Release_CompareAndSwap(Object old, Object target) const {
  Address result = base::AsAtomicPointer::Release_CompareAndSwap(
      location(), old->ptr(), target->ptr());
  return Object(result);
}

//
// FullMaybeObjectSlot implementation.
//

MaybeObject FullMaybeObjectSlot::operator*() const {
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

HeapObject FullHeapObjectSlot::ToHeapObject() const {
  DCHECK((*location() & kHeapObjectTagMask) == kHeapObjectTag);
  return HeapObject::cast(Object(*location()));
}

void FullHeapObjectSlot::StoreHeapObject(HeapObject value) const {
  *location() = value->ptr();
}

//
// Utils.
//

// Sets |counter| number of kTaggedSize-sized values starting at |start| slot.
inline void MemsetTagged(ObjectSlot start, Object value, size_t counter) {
  // TODO(ishell): revisit this implementation, maybe use "rep stosl"
  STATIC_ASSERT(kTaggedSize == kSystemPointerSize);
  MemsetPointer(start.location(), value.ptr(), counter);
}

// Sets |counter| number of kSystemPointerSize-sized values starting at |start|
// slot.
inline void MemsetPointer(FullObjectSlot start, Object value, size_t counter) {
  MemsetPointer(start.location(), value.ptr(), counter);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_INL_H_
