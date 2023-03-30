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
#include "src/sandbox/external-pointer-inl.h"
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
  TData value = *location();
  DCHECK(HAS_STRONG_HEAP_OBJECT_TAG(value));
  return HeapObject::cast(Object(value));
}

void FullHeapObjectSlot::StoreHeapObject(HeapObject value) const {
  *location() = value.ptr();
}

void ExternalPointerSlot::init(Isolate* isolate, Address value,
                               ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  ExternalPointerTable& table = GetExternalPointerTableForTag(isolate, tag);
  ExternalPointerHandle handle =
      table.AllocateAndInitializeEntry(isolate, value, tag);
  // Use a Release_Store to ensure that the store of the pointer into the
  // table is not reordered after the store of the handle. Otherwise, other
  // threads may access an uninitialized table entry and crash.
  Release_StoreHandle(handle);
#else
  store(isolate, value, tag);
#endif  // V8_ENABLE_SANDBOX
}

#ifdef V8_ENABLE_SANDBOX
ExternalPointerHandle ExternalPointerSlot::Relaxed_LoadHandle() const {
  return base::AsAtomic32::Relaxed_Load(location());
}

void ExternalPointerSlot::Relaxed_StoreHandle(
    ExternalPointerHandle handle) const {
  return base::AsAtomic32::Relaxed_Store(location(), handle);
}

void ExternalPointerSlot::Release_StoreHandle(
    ExternalPointerHandle handle) const {
  return base::AsAtomic32::Release_Store(location(), handle);
}
#endif  // V8_ENABLE_SANDBOX

Address ExternalPointerSlot::load(const Isolate* isolate,
                                  ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  const ExternalPointerTable& table =
      GetExternalPointerTableForTag(isolate, tag);
  ExternalPointerHandle handle = Relaxed_LoadHandle();
  return table.Get(handle, tag);
#else
  return ReadMaybeUnalignedValue<Address>(address());
#endif  // V8_ENABLE_SANDBOX
}

void ExternalPointerSlot::store(Isolate* isolate, Address value,
                                ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  DCHECK_NE(tag, kExternalPointerNullTag);
  ExternalPointerTable& table = GetExternalPointerTableForTag(isolate, tag);
  ExternalPointerHandle handle = Relaxed_LoadHandle();
  table.Set(handle, value, tag);
#else
  WriteMaybeUnalignedValue<Address>(address(), value);
#endif  // V8_ENABLE_SANDBOX
}

ExternalPointerSlot::RawContent
ExternalPointerSlot::GetAndClearContentForSerialization(
    const DisallowGarbageCollection& no_gc) {
#ifdef V8_ENABLE_SANDBOX
  ExternalPointerHandle content = Relaxed_LoadHandle();
  Relaxed_StoreHandle(kNullExternalPointerHandle);
#else
  Address content = ReadMaybeUnalignedValue<Address>(address());
  WriteMaybeUnalignedValue<Address>(address(), kNullAddress);
#endif
  return content;
}

void ExternalPointerSlot::RestoreContentAfterSerialization(
    ExternalPointerSlot::RawContent content,
    const DisallowGarbageCollection& no_gc) {
#ifdef V8_ENABLE_SANDBOX
  return Relaxed_StoreHandle(content);
#else
  return WriteMaybeUnalignedValue<Address>(address(), content);
#endif
}

#ifdef V8_ENABLE_SANDBOX
const ExternalPointerTable& ExternalPointerSlot::GetExternalPointerTableForTag(
    const Isolate* isolate, ExternalPointerTag tag) {
  return IsSharedExternalPointerType(tag)
             ? isolate->shared_external_pointer_table()
             : isolate->external_pointer_table();
}

ExternalPointerTable& ExternalPointerSlot::GetExternalPointerTableForTag(
    Isolate* isolate, ExternalPointerTag tag) {
  return IsSharedExternalPointerType(tag)
             ? isolate->shared_external_pointer_table()
             : isolate->external_pointer_table();
}
#endif  // V8_ENABLE_SANDBOX

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
  // CompressAny since many callers pass values which are not valid objects.
  Tagged_t raw_value = V8HeapCompressionScheme::CompressAny(value.ptr());
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
