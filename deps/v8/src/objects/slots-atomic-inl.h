// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SLOTS_ATOMIC_INL_H_
#define V8_OBJECTS_SLOTS_ATOMIC_INL_H_

#include "src/base/atomic-utils.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

// This class is intended to be used as a wrapper for elements of an array
// that is passed in to STL functions such as std::sort. It ensures that
// elements accesses are atomic.
// Usage example:
//   FixedArray array;
//   AtomicSlot start(array->GetFirstElementAddress());
//   std::sort(start, start + given_length,
//             [](Tagged_t a, Tagged_t b) {
//               // Decompress a and b if necessary.
//               return my_comparison(a, b);
//             });
// Note how the comparator operates on Address values, representing the raw
// data found at the given heap location, so you probably want to construct
// an Object from it.
class AtomicSlot : public SlotBase<AtomicSlot, Tagged_t, kTaggedSize> {
 public:
  // This class is a stand-in for "Address&" that uses custom atomic
  // read/write operations for the actual memory accesses.
  class Reference {
   public:
    explicit Reference(Tagged_t* address) : address_(address) {}
    Reference(const Reference&) V8_NOEXCEPT = default;

    Reference& operator=(const Reference& other) V8_NOEXCEPT {
      AsAtomicTagged::Relaxed_Store(
          address_, AsAtomicTagged::Relaxed_Load(other.address_));
      return *this;
    }
    Reference& operator=(Tagged_t value) {
      AsAtomicTagged::Relaxed_Store(address_, value);
      return *this;
    }

    // Values of type AtomicSlot::reference must be implicitly convertible
    // to AtomicSlot::value_type.
    operator Tagged_t() const { return AsAtomicTagged::Relaxed_Load(address_); }

    void swap(Reference& other) {
      Address tmp = value();
      AsAtomicTagged::Relaxed_Store(address_, other.value());
      AsAtomicTagged::Relaxed_Store(other.address_, tmp);
    }

    bool operator<(const Reference& other) const {
      return value() < other.value();
    }

    bool operator==(const Reference& other) const {
      return value() == other.value();
    }

   private:
    Address value() const { return AsAtomicTagged::Relaxed_Load(address_); }

    Tagged_t* address_;
  };

  // The rest of this class follows C++'s "RandomAccessIterator" requirements.
  // Most of the heavy lifting is inherited from SlotBase.
  typedef int difference_type;
  typedef Tagged_t value_type;
  typedef Reference reference;
  typedef void* pointer;  // Must be present, but should not be used.
  typedef std::random_access_iterator_tag iterator_category;

  AtomicSlot() : SlotBase(kNullAddress) {}
  explicit AtomicSlot(Address address) : SlotBase(address) {}
  explicit AtomicSlot(ObjectSlot slot) : SlotBase(slot.address()) {}

  Reference operator*() const {
    return Reference(reinterpret_cast<Tagged_t*>(address()));
  }
  Reference operator[](difference_type i) const {
    return Reference(reinterpret_cast<Tagged_t*>(address() + i * kTaggedSize));
  }

  friend void swap(Reference lhs, Reference rhs) { lhs.swap(rhs); }

  friend difference_type operator-(AtomicSlot a, AtomicSlot b) {
    return static_cast<int>(a.address() - b.address()) / kTaggedSize;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SLOTS_ATOMIC_INL_H_
