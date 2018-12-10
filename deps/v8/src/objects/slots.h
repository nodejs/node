// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_H_
#define V8_OBJECTS_SLOTS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class ObjectPtr;

template <typename Subclass, typename Data, size_t SlotDataSize>
class SlotBase {
 public:
  using TData = Data;

  // TODO(ishell): This should eventually become just sizeof(TData) once
  // pointer compression is implemented.
  static constexpr size_t kSlotDataSize = SlotDataSize;

  Subclass& operator++() {  // Prefix increment.
    ptr_ += kSlotDataSize;
    return *static_cast<Subclass*>(this);
  }
  Subclass operator++(int) {  // Postfix increment.
    Subclass result = *static_cast<Subclass*>(this);
    ptr_ += kSlotDataSize;
    return result;
  }
  Subclass& operator--() {  // Prefix decrement.
    ptr_ -= kSlotDataSize;
    return *static_cast<Subclass*>(this);
  }
  Subclass operator--(int) {  // Postfix decrement.
    Subclass result = *static_cast<Subclass*>(this);
    ptr_ -= kSlotDataSize;
    return result;
  }

  bool operator<(const SlotBase& other) const { return ptr_ < other.ptr_; }
  bool operator<=(const SlotBase& other) const { return ptr_ <= other.ptr_; }
  bool operator>(const SlotBase& other) const { return ptr_ > other.ptr_; }
  bool operator>=(const SlotBase& other) const { return ptr_ >= other.ptr_; }
  bool operator==(const SlotBase& other) const { return ptr_ == other.ptr_; }
  bool operator!=(const SlotBase& other) const { return ptr_ != other.ptr_; }
  size_t operator-(const SlotBase& other) const {
    DCHECK_GE(ptr_, other.ptr_);
    return static_cast<size_t>((ptr_ - other.ptr_) / kSlotDataSize);
  }
  Subclass operator-(int i) const { return Subclass(ptr_ - i * kSlotDataSize); }
  Subclass operator+(int i) const { return Subclass(ptr_ + i * kSlotDataSize); }
  friend Subclass operator+(int i, const Subclass& slot) {
    return Subclass(slot.ptr_ + i * kSlotDataSize);
  }
  Subclass& operator+=(int i) {
    ptr_ += i * kSlotDataSize;
    return *static_cast<Subclass*>(this);
  }
  Subclass operator-(int i) { return Subclass(ptr_ - i * kSlotDataSize); }
  Subclass& operator-=(int i) {
    ptr_ -= i * kSlotDataSize;
    return *static_cast<Subclass*>(this);
  }

  void* ToVoidPtr() const { return reinterpret_cast<void*>(address()); }

  Address address() const { return ptr_; }
  // For symmetry with Handle.
  TData* location() const { return reinterpret_cast<TData*>(ptr_); }

 protected:
  STATIC_ASSERT(IsAligned(kSlotDataSize, kTaggedSize));
  explicit SlotBase(Address ptr) : ptr_(ptr) {
    DCHECK(IsAligned(ptr, kTaggedSize));
  }

 private:
  // This field usually describes an on-heap address (a slot within an object),
  // so its type should not be a pointer to another C++ wrapper class.
  // Type safety is provided by well-defined conversion operations.
  Address ptr_;
};

// An FullObjectSlot instance describes a kSystemPointerSize-sized field
// ("slot") holding a tagged pointer (smi or strong heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class FullObjectSlot
    : public SlotBase<FullObjectSlot, Address, kSystemPointerSize> {
 public:
  using TObject = ObjectPtr;
  using THeapObjectSlot = FullHeapObjectSlot;

  // Tagged value stored in this slot is guaranteed to never be a weak pointer.
  static constexpr bool kCanBeWeak = false;

  FullObjectSlot() : SlotBase(kNullAddress) {}
  explicit FullObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit FullObjectSlot(Address* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  inline explicit FullObjectSlot(ObjectPtr* object);
  explicit FullObjectSlot(Object const* const* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit FullObjectSlot(HeapObject** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit FullObjectSlot(SlotBase<T, TData, kSlotDataSize> slot)
      : SlotBase(slot.address()) {}

  // Compares memory representation of a value stored in the slot with given
  // raw value.
  inline bool contains_value(Address raw_value) const;

  inline Object* operator*() const;
  // TODO(3770): drop this in favor of operator* once migration is complete.
  inline ObjectPtr load() const;
  inline void store(Object* value) const;
  inline void store(ObjectPtr value) const;

  inline ObjectPtr Acquire_Load() const;
  inline ObjectPtr Relaxed_Load() const;
  inline void Relaxed_Store(ObjectPtr value) const;
  inline void Release_Store(ObjectPtr value) const;
  inline ObjectPtr Release_CompareAndSwap(ObjectPtr old,
                                          ObjectPtr target) const;
  // Old-style alternative for the above, temporarily separate to allow
  // incremental transition.
  // TODO(3770): Get rid of the duplication when the migration is complete.
  inline Object* Acquire_Load1() const;
  inline void Relaxed_Store1(Object* value) const;
  inline void Release_Store1(Object* value) const;
};

// A FullMaybeObjectSlot instance describes a kSystemPointerSize-sized field
// ("slot") holding a possibly-weak tagged pointer (think: MaybeObject).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class FullMaybeObjectSlot
    : public SlotBase<FullMaybeObjectSlot, Address, kSystemPointerSize> {
 public:
  using TObject = MaybeObject;
  using THeapObjectSlot = FullHeapObjectSlot;

  // Tagged value stored in this slot can be a weak pointer.
  static constexpr bool kCanBeWeak = true;

  FullMaybeObjectSlot() : SlotBase(kNullAddress) {}
  explicit FullMaybeObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit FullMaybeObjectSlot(ObjectPtr* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit FullMaybeObjectSlot(Object** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit FullMaybeObjectSlot(HeapObject** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit FullMaybeObjectSlot(SlotBase<T, TData, kSlotDataSize> slot)
      : SlotBase(slot.address()) {}

  inline MaybeObject operator*() const;
  // TODO(3770): drop this once ObjectSlot::load() is dropped.
  inline MaybeObject load() const;
  inline void store(MaybeObject value) const;

  inline MaybeObject Relaxed_Load() const;
  inline void Relaxed_Store(MaybeObject value) const;
  inline void Release_CompareAndSwap(MaybeObject old, MaybeObject target) const;
};

// A FullHeapObjectSlot instance describes a kSystemPointerSize-sized field
// ("slot") holding a weak or strong pointer to a heap object (think:
// HeapObjectReference).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
// In case it is known that that slot contains a strong heap object pointer,
// ToHeapObject() can be used to retrieve that heap object.
class FullHeapObjectSlot
    : public SlotBase<HeapObjectSlot, Address, kSystemPointerSize> {
 public:
  FullHeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit FullHeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit FullHeapObjectSlot(ObjectPtr* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit FullHeapObjectSlot(HeapObject** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit FullHeapObjectSlot(SlotBase<T, TData, kSlotDataSize> slot)
      : SlotBase(slot.address()) {}

  inline HeapObjectReference operator*() const;
  inline void store(HeapObjectReference value) const;

  HeapObject* ToHeapObject() const {
    DCHECK((*location() & kHeapObjectTagMask) == kHeapObjectTag);
    return reinterpret_cast<HeapObject*>(*location());
  }

  void StoreHeapObject(HeapObject* value) const {
    *reinterpret_cast<HeapObject**>(address()) = value;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_H_
