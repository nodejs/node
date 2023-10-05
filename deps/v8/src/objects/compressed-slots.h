// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_COMPRESSED_SLOTS_H_
#define V8_OBJECTS_COMPRESSED_SLOTS_H_

#include "include/v8config.h"
#include "src/objects/slots.h"

namespace v8::internal {

#ifdef V8_COMPRESS_POINTERS

class V8HeapCompressionScheme;

// A CompressedObjectSlot instance describes a kTaggedSize-sized field ("slot")
// holding a compressed tagged pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedObjectSlot : public SlotBase<CompressedObjectSlot, Tagged_t> {
 public:
  using TCompressionScheme = V8HeapCompressionScheme;
  using TObject = Tagged<Object>;
  using THeapObjectSlot = CompressedHeapObjectSlot;

  static constexpr bool kCanBeWeak = false;

  CompressedObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedObjectSlot(Address* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  inline explicit CompressedObjectSlot(Tagged<Object>* object);
  explicit CompressedObjectSlot(Tagged<Object> const* const* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  // Compares memory representation of a value stored in the slot with given
  // raw value without decompression.
  inline bool contains_map_value(Address raw_value) const;
  inline bool Relaxed_ContainsMapValue(Address raw_value) const;

  // TODO(leszeks): Consider deprecating the operator* load, and always pass the
  // Isolate.
  inline Tagged<Object> operator*() const;
  inline Tagged<Object> load(PtrComprCageBase cage_base) const;
  inline void store(Tagged<Object> value) const;
  inline void store_map(Tagged<Map> map) const;

  inline Tagged<Map> load_map() const;

  inline Tagged<Object> Acquire_Load() const;
  inline Tagged<Object> Relaxed_Load() const;
  inline Tagged<Object> Relaxed_Load(PtrComprCageBase cage_base) const;
  inline void Relaxed_Store(Tagged<Object> value) const;
  inline void Release_Store(Tagged<Object> value) const;
  inline Tagged<Object> Release_CompareAndSwap(Tagged<Object> old,
                                               Tagged<Object> target) const;
};

// A CompressedMaybeObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a possibly-weak compressed tagged pointer
// (think: MaybeObject).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class CompressedMaybeObjectSlot
    : public SlotBase<CompressedMaybeObjectSlot, Tagged_t> {
 public:
  using TCompressionScheme = V8HeapCompressionScheme;
  using TObject = MaybeObject;
  using THeapObjectSlot = CompressedHeapObjectSlot;

  static constexpr bool kCanBeWeak = true;

  CompressedMaybeObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedMaybeObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedMaybeObjectSlot(Tagged<Object>* ptr)
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
  using TCompressionScheme = V8HeapCompressionScheme;

  CompressedHeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit CompressedHeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit CompressedHeapObjectSlot(TaggedBase* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit CompressedHeapObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  inline HeapObjectReference operator*() const;
  inline HeapObjectReference load(PtrComprCageBase cage_base) const;
  inline void store(HeapObjectReference value) const;

  inline Tagged<HeapObject> ToHeapObject() const;

  inline void StoreHeapObject(Tagged<HeapObject> value) const;
};

// An OffHeapCompressedObjectSlot instance describes a kTaggedSize-sized field
// ("slot") holding a compressed tagged pointer (smi or heap object).
// Unlike CompressedObjectSlot, it does not assume that the slot is on the heap,
// and so does not provide an operator* with implicit Isolate* calculation.
// Its address() is the address of the slot.
// The slot's contents can be read and written using load() and store().
template <typename CompressionScheme>
class OffHeapCompressedObjectSlot
    : public SlotBase<OffHeapCompressedObjectSlot<CompressionScheme>,
                      Tagged_t> {
 public:
  using TSlotBase =
      SlotBase<OffHeapCompressedObjectSlot<CompressionScheme>, Tagged_t>;
  using TCompressionScheme = CompressionScheme;
  using TObject = Tagged<Object>;
  using THeapObjectSlot = OffHeapCompressedObjectSlot<CompressionScheme>;

  static constexpr bool kCanBeWeak = false;

  OffHeapCompressedObjectSlot() : TSlotBase(kNullAddress) {}
  explicit OffHeapCompressedObjectSlot(Address ptr) : TSlotBase(ptr) {}
  explicit OffHeapCompressedObjectSlot(const uint32_t* ptr)
      : TSlotBase(reinterpret_cast<Address>(ptr)) {}

  inline Tagged<Object> load(PtrComprCageBase cage_base) const;
  inline void store(Tagged<Object> value) const;

  inline Tagged<Object> Relaxed_Load(PtrComprCageBase cage_base) const;
  inline Tagged<Object> Acquire_Load(PtrComprCageBase cage_base) const;
  inline void Relaxed_Store(Tagged<Object> value) const;
  inline void Release_Store(Tagged<Object> value) const;
  inline void Release_CompareAndSwap(Tagged<Object> old,
                                     Tagged<Object> target) const;
};

#endif  // V8_COMPRESS_POINTERS

}  // namespace v8::internal

#endif  // V8_OBJECTS_COMPRESSED_SLOTS_H_
