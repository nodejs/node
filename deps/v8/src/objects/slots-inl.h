// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_INL_H_
#define V8_OBJECTS_SLOTS_INL_H_

#include "src/base/atomic-utils.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/heap-object.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/utils/memcopy.h"

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

bool FullObjectSlot::contains_map_value(Address raw_value) const {
  return load_map().ptr() == raw_value;
}

Object FullObjectSlot::operator*() const { return Object(*location()); }

Object FullObjectSlot::load(PtrComprCageBase cage_base) const { return **this; }

void FullObjectSlot::store(Object value) const { *location() = value.ptr(); }

void FullObjectSlot::store_map(Map map) const {
#ifdef V8_MAP_PACKING
  *location() = MapWord::Pack(map.ptr());
#else
  store(map);
#endif
}

Map FullObjectSlot::load_map() const {
#ifdef V8_MAP_PACKING
  return Map::unchecked_cast(Object(MapWord::Unpack(*location())));
#else
  return Map::unchecked_cast(Object(*location()));
#endif
}

Object FullObjectSlot::Acquire_Load() const {
  return Object(base::AsAtomicPointer::Acquire_Load(location()));
}

Object FullObjectSlot::Acquire_Load(PtrComprCageBase cage_base) const {
  return Acquire_Load();
}

Object FullObjectSlot::Relaxed_Load() const {
  return Object(base::AsAtomicPointer::Relaxed_Load(location()));
}

Object FullObjectSlot::Relaxed_Load(PtrComprCageBase cage_base) const {
  return Relaxed_Load();
}

void FullObjectSlot::Relaxed_Store(Object value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value.ptr());
}

void FullObjectSlot::Release_Store(Object value) const {
  base::AsAtomicPointer::Release_Store(location(), value.ptr());
}

Object FullObjectSlot::Relaxed_CompareAndSwap(Object old, Object target) const {
  Address result = base::AsAtomicPointer::Relaxed_CompareAndSwap(
      location(), old.ptr(), target.ptr());
  return Object(result);
}

Object FullObjectSlot::Release_CompareAndSwap(Object old, Object target) const {
  Address result = base::AsAtomicPointer::Release_CompareAndSwap(
      location(), old.ptr(), target.ptr());
  return Object(result);
}

//
// FullMaybeObjectSlot implementation.
//

MaybeObject FullMaybeObjectSlot::operator*() const {
  return MaybeObject(*location());
}

MaybeObject FullMaybeObjectSlot::load(PtrComprCageBase cage_base) const {
  return **this;
}

void FullMaybeObjectSlot::store(MaybeObject value) const {
  *location() = value.ptr();
}

MaybeObject FullMaybeObjectSlot::Relaxed_Load() const {
  return MaybeObject(base::AsAtomicPointer::Relaxed_Load(location()));
}

MaybeObject FullMaybeObjectSlot::Relaxed_Load(
    PtrComprCageBase cage_base) const {
  return Relaxed_Load();
}

void FullMaybeObjectSlot::Relaxed_Store(MaybeObject value) const {
  base::AsAtomicPointer::Relaxed_Store(location(), value->ptr());
}

void FullMaybeObjectSlot::Release_CompareAndSwap(MaybeObject old,
                                                 MaybeObject target) const {
  base::AsAtomicPointer::Release_CompareAndSwap(location(), old.ptr(),
                                                target.ptr());
}

//
// FullHeapObjectSlot implementation.
//

HeapObjectReference FullHeapObjectSlot::operator*() const {
  return HeapObjectReference(*location());
}

HeapObjectReference FullHeapObjectSlot::load(PtrComprCageBase cage_base) const {
  return **this;
}

void FullHeapObjectSlot::store(HeapObjectReference value) const {
  *location() = value.ptr();
}

HeapObject FullHeapObjectSlot::ToHeapObject() const {
  DCHECK((*location() & kHeapObjectTagMask) == kHeapObjectTag);
  return HeapObject::cast(Object(*location()));
}

void FullHeapObjectSlot::StoreHeapObject(HeapObject value) const {
  *location() = value.ptr();
}

//
// Utils.
//

// Copies tagged words from |src| to |dst|. The data spans must not overlap.
// |src| and |dst| must be kTaggedSize-aligned.
inline void CopyTagged(Address dst, const Address src, size_t num_tagged) {
  static const size_t kBlockCopyLimit = 16;
  CopyImpl<kBlockCopyLimit>(reinterpret_cast<Tagged_t*>(dst),
                            reinterpret_cast<const Tagged_t*>(src), num_tagged);
}

// Sets |counter| number of kTaggedSize-sized values starting at |start| slot.
inline void MemsetTagged(Tagged_t* start, Object value, size_t counter) {
#ifdef V8_COMPRESS_POINTERS
  Tagged_t raw_value = CompressTagged(value.ptr());
  MemsetUint32(start, raw_value, counter);
#else
  Address raw_value = value.ptr();
  MemsetPointer(start, raw_value, counter);
#endif
}

// Sets |counter| number of kTaggedSize-sized values starting at |start| slot.
template <typename T>
inline void MemsetTagged(SlotBase<T, Tagged_t> start, Object value,
                         size_t counter) {
  MemsetTagged(start.location(), value, counter);
}

// Sets |counter| number of kSystemPointerSize-sized values starting at |start|
// slot.
inline void MemsetPointer(FullObjectSlot start, Object value, size_t counter) {
  MemsetPointer(start.location(), value.ptr(), counter);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_INL_H_
