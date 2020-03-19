// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_H_
#define V8_OBJECTS_SLOTS_H_

#include "src/base/memory.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

class Object;

template <typename Subclass, typename Data,
          size_t SlotDataAlignment = sizeof(Data)>
class SlotBase {
 public:
  using TData = Data;

  static constexpr size_t kSlotDataSize = sizeof(Data);
  static constexpr size_t kSlotDataAlignment = SlotDataAlignment;

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
  explicit SlotBase(Address ptr) : ptr_(ptr) {
    DCHECK(IsAligned(ptr, kSlotDataAlignment));
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
class FullObjectSlot : public SlotBase<FullObjectSlot, Address> {
 public:
  using TObject = Object;
  using THeapObjectSlot = FullHeapObjectSlot;

  // Tagged value stored in this slot is guaranteed to never be a weak pointer.
  static constexpr bool kCanBeWeak = false;

  FullObjectSlot() : SlotBase(kNullAddress) {}
  explicit FullObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit FullObjectSlot(const Address* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  inline explicit FullObjectSlot(Object* object);
  template <typename T>
  explicit FullObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  // Compares memory representation of a value stored in the slot with given
  // raw value.
  inline bool contains_value(Address raw_value) const;

  inline const Object operator*() const;
  inline void store(Object value) const;

  inline Object Acquire_Load() const;
  inline Object Relaxed_Load() const;
  inline void Relaxed_Store(Object value) const;
  inline void Release_Store(Object value) const;
  inline Object Relaxed_CompareAndSwap(Object old, Object target) const;
  inline Object Release_CompareAndSwap(Object old, Object target) const;
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
  explicit FullMaybeObjectSlot(Object* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  explicit FullMaybeObjectSlot(MaybeObject* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit FullMaybeObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  inline const MaybeObject operator*() const;
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
class FullHeapObjectSlot : public SlotBase<FullHeapObjectSlot, Address> {
 public:
  FullHeapObjectSlot() : SlotBase(kNullAddress) {}
  explicit FullHeapObjectSlot(Address ptr) : SlotBase(ptr) {}
  explicit FullHeapObjectSlot(Object* ptr)
      : SlotBase(reinterpret_cast<Address>(ptr)) {}
  template <typename T>
  explicit FullHeapObjectSlot(SlotBase<T, TData, kSlotDataAlignment> slot)
      : SlotBase(slot.address()) {}

  inline const HeapObjectReference operator*() const;
  inline void store(HeapObjectReference value) const;

  inline HeapObject ToHeapObject() const;

  inline void StoreHeapObject(HeapObject value) const;
};

// TODO(ishell, v8:8875): When pointer compression is enabled the [u]intptr_t
// and double fields are only kTaggedSize aligned so in order to avoid undefined
// behavior in C++ code we use this iterator adaptor when using STL algorithms
// with unaligned pointers.
// It will be removed once all v8:8875 is fixed and all the full pointer and
// double values in compressed V8 heap are properly aligned.
template <typename T>
class UnalignedSlot : public SlotBase<UnalignedSlot<T>, T, 1> {
 public:
  // This class is a stand-in for "T&" that uses custom read/write operations
  // for the actual memory accesses.
  class Reference {
   public:
    explicit Reference(Address address) : address_(address) {}
    Reference(const Reference&) V8_NOEXCEPT = default;

    Reference& operator=(const Reference& other) V8_NOEXCEPT {
      base::WriteUnalignedValue<T>(address_, other.value());
      return *this;
    }
    Reference& operator=(T value) {
      base::WriteUnalignedValue<T>(address_, value);
      return *this;
    }

    // Values of type UnalignedSlot::reference must be implicitly convertible
    // to UnalignedSlot::value_type.
    operator T() const { return value(); }

    void swap(Reference& other) {
      T tmp = value();
      base::WriteUnalignedValue<T>(address_, other.value());
      base::WriteUnalignedValue<T>(other.address_, tmp);
    }

    bool operator<(const Reference& other) const {
      return value() < other.value();
    }

    bool operator==(const Reference& other) const {
      return value() == other.value();
    }

   private:
    T value() const { return base::ReadUnalignedValue<T>(address_); }

    Address address_;
  };

  // The rest of this class follows C++'s "RandomAccessIterator" requirements.
  // Most of the heavy lifting is inherited from SlotBase.
  using difference_type = int;
  using value_type = T;
  using reference = Reference;
  using pointer = T*;
  using iterator_category = std::random_access_iterator_tag;

  UnalignedSlot() : SlotBase<UnalignedSlot<T>, T, 1>(kNullAddress) {}
  explicit UnalignedSlot(Address address)
      : SlotBase<UnalignedSlot<T>, T, 1>(address) {}
  explicit UnalignedSlot(T* address)
      : SlotBase<UnalignedSlot<T>, T, 1>(reinterpret_cast<Address>(address)) {}

  Reference operator*() const {
    return Reference(SlotBase<UnalignedSlot<T>, T, 1>::address());
  }
  Reference operator[](difference_type i) const {
    return Reference(SlotBase<UnalignedSlot<T>, T, 1>::address() +
                     i * sizeof(T));
  }

  friend void swap(Reference lhs, Reference rhs) { lhs.swap(rhs); }

  friend difference_type operator-(UnalignedSlot a, UnalignedSlot b) {
    return static_cast<int>(a.address() - b.address()) / sizeof(T);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_H_
