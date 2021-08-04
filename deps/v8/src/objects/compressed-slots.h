// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_COMPRESSED_SLOTS_H_
#define V8_OBJECTS_COMPRESSED_SLOTS_H_

#include "include/v8config.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

#ifdef V8_COMPRESS_POINTERS
// A CompressedObjectSlot instance describes a kTaggedSize-sized field ("slot")
// holding a compressed tagged pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedObjectSlot : public SlotBase<CompressedObjectSlot, Tagged_t> {
 public:
  using TObject = Object;
  using THeapObjectSlot = CompressedHeapObjectSlot;

  static constexpr bool kCanBeWeak = false;

  CompressedObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedObjectSlot(Address* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  inline explicit CompressedObjectSlot(Object* object);
  explicit CompressedObjectSlot(Object const* const* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  // Compares memory representation of a value stored in the slot with given
  // raw value without decompression.
  inline bool contains_value(Address raw_value) const;
  inline bool contains_map_value(Address raw_value) const;

  // TODO(leszeks): Consider deprecating the operator* load, and always pass the
  // Isolate.
  inline Object operator*() const;
  inline Object load(PtrComprCageBase cage_base) const;
  inline void store(Object value) const;
  inline void store_map(Map map) const;

  inline Map load_map() const;

  inline Object Acquire_Load() const;
  inline Object Relaxed_Load() const;
  inline Object Relaxed_Load(PtrComprCageBase cage_base) const;
  inline void Relaxed_Store(Object value) const;
  inline void Release_Store(Object value) const;
  inline Object Release_CompareAndSwap(Object old, Object target) const;
};

// A CompressedMaybeObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a possibly-weak compressed tagged pointer
// (think: MaybeObject).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedMaybeObjectSlot
    : public SlotBase<CompressedMaybeObjectSlot, Tagged_t> {
 public:
  using TObject = MaybeObject;
  using THeapObjectSlot = CompressedHeapObjectSlot;

  static constexpr bool kCanBeWeak = true;

  CompressedMaybeObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedMaybeObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedMaybeObjectSlot(Object* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit CompressedMaybeObjectSlot(MaybeObject* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedMaybeObjectSlot(
      SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  inline MaybeObject operator*() const;
  inline MaybeObject load(PtrComprCageBase cage_base) const;
  inline void store(MaybeObject value) const;

  inline MaybeObject Relaxed_Load() const;
  inline MaybeObject Relaxed_Load(PtrComprCageBase cage_base) const;
  inline void Relaxed_Store(MaybeObject value) const;
  inline void Release_CompareAndSwap(MaybeObject old, MaybeObject target) const;
};

// A CompressedHeapObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a weak or strong compressed pointer to a heap object (think:
// HeapObjectReference).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
// In case it is known that that slot contains a strong heap object pointer,
// ToHeapObject() can be used to retrieve that heap object.
class CompressedHeapObjectSlot
    : public SlotBase<CompressedHeapObjectSlot, Tagged_t> {
 public:
  CompressedHeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedHeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedHeapObjectSlot(Object* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedHeapObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  inline HeapObjectReference operator*() const;
  inline HeapObjectReference load(PtrComprCageBase cage_base) const;
  inline void store(HeapObjectReference value) const;

  inline HeapObject ToHeapObject() const;

  inline void StoreHeapObject(HeapObject value) const;
};

// An OffHeapCompressedObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a compressed tagged pointer (smi or heap object).
// Unlike CompressedObjectSlot, it does not assume that the slot is on the heap,
// and so does not provide an operator* with implicit Isolate* calculation.
// Its address() is the address of the slot.
// The slot's contents can be read and written using load() and store().
class OffHeapCompressedObjectSlot
    : public SlotBase<OffHeapCompressedObjectSlot, Tagged_t> {
 public:
  using TObject = Object;
  using THeapObjectSlot = OffHeapCompressedObjectSlot;

  static constexpr bool kCanBeWeak = false;

  OffHeapCompressedObjectSlot() : SlotBase(kNullAddress) {}
  explicit OffHeapCompressedObjectSlot(const uint32_t* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}

  inline Object load(PtrComprCageBase cage_base) const;
  inline void store(Object value) const;

  inline Object Relaxed_Load(PtrComprCageBase cage_base) const;
  inline Object Acquire_Load(PtrComprCageBase cage_base) const;
  inline void Relaxed_Store(Object value) const;
  inline void Release_Store(Object value) const;
  inline void Release_CompareAndSwap(Object old, Object target) const;
};

#endif  // V8_COMPRESS_POINTERS

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_COMPRESSED_SLOTS_H_
