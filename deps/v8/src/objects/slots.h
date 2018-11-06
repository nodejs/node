// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_H_
#define V8_OBJECTS_SLOTS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

template <typename Subclass>
class SlotBase {
 public:
  Subclass& operator++() {  // Prefix increment.
    ptr_ += kPointerSize;
    return *static_cast<Subclass*>(this);
  }
  Subclass operator++(int) {  // Postfix increment.
    Subclass result = *static_cast<Subclass*>(this);
    ptr_ += kPointerSize;
    return result;
  }
  Subclass& operator--() {  // Prefix decrement.
    ptr_ -= kPointerSize;
    return *static_cast<Subclass*>(this);
  }
  Subclass operator--(int) {  // Postfix decrement.
    Subclass result = *static_cast<Subclass*>(this);
    ptr_ -= kPointerSize;
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
    return static_cast<size_t>((ptr_ - other.ptr_) / kPointerSize);
  }
  Subclass operator-(int i) const { return Subclass(ptr_ - i * kPointerSize); }
  Subclass operator+(int i) const { return Subclass(ptr_ + i * kPointerSize); }
  friend Subclass operator+(int i, const Subclass& slot) {
    return Subclass(slot.ptr_ + i * kPointerSize);
  }
  Subclass& operator+=(int i) {
    ptr_ += i * kPointerSize;
    return *static_cast<Subclass*>(this);
  }
  Subclass operator-(int i) { return Subclass(ptr_ - i * kPointerSize); }
  Subclass& operator-=(int i) {
    ptr_ -= i * kPointerSize;
    return *static_cast<Subclass*>(this);
  }

  void* ToVoidPtr() const { return reinterpret_cast<void*>(address()); }

  Address address() const { return ptr_; }
  // For symmetry with Handle.
  Address* location() const { return reinterpret_cast<Address*>(ptr_); }

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
class ObjectSlot : public SlotBase<ObjectSlot> {
 public:
  ObjectSlot() : SlotBase(kNullAddress) {}
  explicit ObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit ObjectSlot(Address* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit ObjectSlot(Object const* const* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit ObjectSlot(SlotBase<T> slot) : SlotBase(slot.address()) {}

  Object* operator*() const { return *reinterpret_cast<Object**>(address()); }
  void store(Object* value) { *reinterpret_cast<Object**>(address()) = value; }

  inline Object* Relaxed_Load() const;
  inline Object* Relaxed_Load(int offset) const;
  inline void Relaxed_Store(int offset, Object* value) const;
  inline Object* Release_CompareAndSwap(Object* old, Object* target) const;
};

// A MaybeObjectSlot instance describes a pointer-sized field ("slot") holding
// a possibly-weak tagged pointer (think: MaybeObject).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
class MaybeObjectSlot : public SlotBase<MaybeObjectSlot> {
 public:
  explicit MaybeObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit MaybeObjectSlot(Object** ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit MaybeObjectSlot(SlotBase<T> slot) : SlotBase(slot.address()) {}

  inline MaybeObject operator*();
  inline void store(MaybeObject value);

  inline MaybeObject Relaxed_Load() const;
  inline void Release_CompareAndSwap(MaybeObject old, MaybeObject target) const;
};

// A HeapObjectSlot instance describes a pointer-sized field ("slot") holding
// a weak or strong pointer to a heap object (think: HeapObjectReference).
// Its address() is the address of the slot.
// The slot's contents can be read and written using operator* and store().
// In case it is known that that slot contains a strong heap object pointer,
// ToHeapObject() can be used to retrieve that heap object.
class HeapObjectSlot : public SlotBase<HeapObjectSlot> {
 public:
  HeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit HeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  template <typename T>
  explicit HeapObjectSlot(SlotBase<T> slot) : SlotBase(slot.address()) {}

  inline HeapObjectReference operator*();
  inline void store(HeapObjectReference value);

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
