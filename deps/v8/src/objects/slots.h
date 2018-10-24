// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_H_
#define V8_OBJECTS_SLOTS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

template <typename Subclass, typename ValueType>
class SlotBase {
 public:
  Subclass& operator++() {
    ptr_ += kPointerSize;
    return *static_cast<Subclass*>(this);
  }

  bool operator<(const SlotBase& other) const { return ptr_ < other.ptr_; }
  bool operator<=(const SlotBase& other) const { return ptr_ <= other.ptr_; }
  bool operator==(const SlotBase& other) const { return ptr_ == other.ptr_; }
  bool operator!=(const SlotBase& other) const { return ptr_ != other.ptr_; }
  size_t operator-(const SlotBase& other) const {
    DCHECK_GE(ptr_, other.ptr_);
    return static_cast<size_t>((ptr_ - other.ptr_) / kPointerSize);
  }
  Subclass operator-(int i) const { return Subclass(ptr_ - i * kPointerSize); }
  Subclass operator+(int i) const { return Subclass(ptr_ + i * kPointerSize); }
  Subclass& operator+=(int i) {
    ptr_ += i * kPointerSize;
    return *static_cast<Subclass*>(this);
  }

  ValueType operator*() const {
    return *reinterpret_cast<ValueType*>(address());
  }
  void store(ValueType value) {
    *reinterpret_cast<ValueType*>(address()) = value;
  }

  void* ToVoidPtr() const { return reinterpret_cast<void*>(address()); }

  Address address() const { return ptr_; }

 protected:
  explicit SlotBase(Address ptr) : ptr_(ptr) {
    DCHECK(IsAligned(ptr, kPointerSize));
  }

 private:
  // This field usually describes an on-heap address (a slot within an object),
  // so its type should not be a pointer to another C++ wrapper class.
  // Type safety is provided by well-defined conversion operations.
  Address ptr_;
};

// An ObjectSlot instance describes a pointer-sized field ("slot") holding
// a tagged pointer (smi or heap object).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class ObjectSlot : public SlotBase<ObjectSlot, Object*> {
 public:
  ObjectSlot() : SlotBase(kNullAddress) {}
  explicit ObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit ObjectSlot(Object** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T, typename U>
  explicit ObjectSlot(SlotBase<T, U> slot) : SlotBase(slot.address()) {}

  inline Object* Relaxed_Load() const;
  inline Object* Relaxed_Load(int offset) const;
  inline void Relaxed_Store(int offset, Object* value) const;
};

// A MaybeObjectSlot instance describes a pointer-sized field ("slot") holding
// a possibly-weak tagged pointer (think: MaybeObject).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class MaybeObjectSlot : public SlotBase<MaybeObjectSlot, MaybeObject*> {
 public:
  explicit MaybeObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit MaybeObjectSlot(Object** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T, typename U>
  explicit MaybeObjectSlot(SlotBase<T, U> slot) : SlotBase(slot.address()) {}

  inline MaybeObject* Relaxed_Load() const;
  inline void Release_CompareAndSwap(MaybeObject* old,
                                     MaybeObject* target) const;
};

// A HeapObjectSlot instance describes a pointer-sized field ("slot") holding
// a weak or strong pointer to a heap object (think: HeapObjectReference).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
// In case it is known that that slot contains a strong heap object pointer,
// ToHeapObject() can be used to retrieve that heap object.
class HeapObjectSlot : public SlotBase<HeapObjectSlot, HeapObjectReference*> {
 public:
  HeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit HeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  template <typename T, typename U>
  explicit HeapObjectSlot(SlotBase<T, U> slot) : SlotBase(slot.address()) {}

  HeapObject* ToHeapObject() {
    DCHECK((*reinterpret_cast<Address*>(address()) & kHeapObjectTagMask) ==
           kHeapObjectTag);
    return *reinterpret_cast<HeapObject**>(address());
  }

  void StoreHeapObject(HeapObject* value) {
    *reinterpret_cast<HeapObject**>(address()) = value;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_H_
