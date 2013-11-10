// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HYDROGEN_UNIQUE_H_
#define V8_HYDROGEN_UNIQUE_H_

#include "handles.h"
#include "objects.h"
#include "utils.h"
#include "zone.h"

namespace v8 {
namespace internal {


template <typename T>
class UniqueSet;


// Represents a handle to an object on the heap, but with the additional
// ability of checking for equality and hashing without accessing the heap.
//
// Creating a Unique<T> requires first dereferencing the handle to obtain
// the address of the object, which is used as the hashcode and the basis for
// comparison. The object can be moved later by the GC, but comparison
// and hashing use the old address of the object, without dereferencing it.
//
// Careful! Comparison of two Uniques is only correct if both were created
// in the same "era" of GC or if at least one is a non-movable object.
template <typename T>
class Unique V8_FINAL {
 public:
  // TODO(titzer): make private and introduce a uniqueness scope.
  explicit Unique(Handle<T> handle) {
    if (handle.is_null()) {
      raw_address_ = NULL;
    } else {
      // This is a best-effort check to prevent comparing Unique<T>'s created
      // in different GC eras; we require heap allocation to be disallowed at
      // creation time.
      // NOTE: we currently consider maps to be non-movable, so no special
      // assurance is required for creating a Unique<Map>.
      // TODO(titzer): other immortable immovable objects are also fine.
      ASSERT(!AllowHeapAllocation::IsAllowed() || handle->IsMap());
      raw_address_ = reinterpret_cast<Address>(*handle);
      ASSERT_NE(raw_address_, NULL);  // Non-null should imply non-zero address.
    }
    handle_ = handle;
  }

  // TODO(titzer): this is a hack to migrate to Unique<T> incrementally.
  Unique(Address raw_address, Handle<T> handle)
    : raw_address_(raw_address), handle_(handle) { }

  // Constructor for handling automatic up casting.
  // Eg. Unique<JSFunction> can be passed when Unique<Object> is expected.
  template <class S> Unique(Unique<S> uniq) {
#ifdef DEBUG
    T* a = NULL;
    S* b = NULL;
    a = b;  // Fake assignment to enforce type checks.
    USE(a);
#endif
    raw_address_ = uniq.raw_address_;
    handle_ = uniq.handle_;
  }

  template <typename U>
  inline bool operator==(const Unique<U>& other) const {
    ASSERT(IsInitialized() && other.IsInitialized());
    return raw_address_ == other.raw_address_;
  }

  template <typename U>
  inline bool operator!=(const Unique<U>& other) const {
    ASSERT(IsInitialized() && other.IsInitialized());
    return raw_address_ != other.raw_address_;
  }

  inline intptr_t Hashcode() const {
    ASSERT(IsInitialized());
    return reinterpret_cast<intptr_t>(raw_address_);
  }

  inline bool IsNull() const {
    ASSERT(IsInitialized());
    return raw_address_ == NULL;
  }

  inline bool IsKnownGlobal(void* global) const {
    ASSERT(IsInitialized());
    return raw_address_ == reinterpret_cast<Address>(global);
  }

  inline Handle<T> handle() const {
    return handle_;
  }

  template <class S> static Unique<T> cast(Unique<S> that) {
    return Unique<T>(that.raw_address_, Handle<T>::cast(that.handle_));
  }

  inline bool IsInitialized() const {
    return raw_address_ != NULL || handle_.is_null();
  }

  // TODO(titzer): this is a hack to migrate to Unique<T> incrementally.
  static Unique<T> CreateUninitialized(Handle<T> handle) {
    return Unique<T>(reinterpret_cast<Address>(NULL), handle);
  }

  static Unique<T> CreateImmovable(Handle<T> handle) {
    return Unique<T>(reinterpret_cast<Address>(*handle), handle);
  }

  friend class UniqueSet<T>;  // Uses internal details for speed.
  template <class U>
  friend class Unique;  // For comparing raw_address values.

 private:
  Address raw_address_;
  Handle<T> handle_;
};


template <typename T>
class UniqueSet V8_FINAL : public ZoneObject {
 public:
  // Constructor. A new set will be empty.
  UniqueSet() : size_(0), capacity_(0), array_(NULL) { }

  // Add a new element to this unique set. Mutates this set. O(|this|).
  void Add(Unique<T> uniq, Zone* zone) {
    ASSERT(uniq.IsInitialized());
    // Keep the set sorted by the {raw_address} of the unique elements.
    for (int i = 0; i < size_; i++) {
      if (array_[i] == uniq) return;
      if (array_[i].raw_address_ > uniq.raw_address_) {
        // Insert in the middle.
        Grow(size_ + 1, zone);
        for (int j = size_ - 1; j >= i; j--) array_[j + 1] = array_[j];
        array_[i] = uniq;
        size_++;
        return;
      }
    }
    // Append the element to the the end.
    Grow(size_ + 1, zone);
    array_[size_++] = uniq;
  }

  // Remove an element from this set. Mutates this set. O(|this|)
  void Remove(Unique<T> uniq) {
    for (int i = 0; i < size_; i++) {
      if (array_[i] == uniq) {
        while (++i < size_) array_[i - 1] = array_[i];
        size_--;
        return;
      }
    }
  }

  // Compare this set against another set. O(|this|).
  bool Equals(UniqueSet<T>* that) const {
    if (that->size_ != this->size_) return false;
    for (int i = 0; i < this->size_; i++) {
      if (this->array_[i] != that->array_[i]) return false;
    }
    return true;
  }

  // Check whether this set contains the given element. O(|this|)
  // TODO(titzer): use binary search for large sets to make this O(log|this|)
  template <typename U>
  bool Contains(Unique<U> elem) const {
    for (int i = 0; i < size_; i++) {
      if (this->array_[i] == elem) return true;
    }
    return false;
  }

  // Check if this set is a subset of the given set. O(|this| + |that|).
  bool IsSubset(UniqueSet<T>* that) const {
    if (that->size_ < this->size_) return false;
    int j = 0;
    for (int i = 0; i < this->size_; i++) {
      Unique<T> sought = this->array_[i];
      while (true) {
        if (sought == that->array_[j++]) break;
        // Fail whenever there are more elements in {this} than {that}.
        if ((this->size_ - i) > (that->size_ - j)) return false;
      }
    }
    return true;
  }

  // Returns a new set representing the intersection of this set and the other.
  // O(|this| + |that|).
  UniqueSet<T>* Intersect(UniqueSet<T>* that, Zone* zone) const {
    if (that->size_ == 0 || this->size_ == 0) return new(zone) UniqueSet<T>();

    UniqueSet<T>* out = new(zone) UniqueSet<T>();
    out->Grow(Min(this->size_, that->size_), zone);

    int i = 0, j = 0, k = 0;
    while (i < this->size_ && j < that->size_) {
      Unique<T> a = this->array_[i];
      Unique<T> b = that->array_[j];
      if (a == b) {
        out->array_[k++] = a;
        i++;
        j++;
      } else if (a.raw_address_ < b.raw_address_) {
        i++;
      } else {
        j++;
      }
    }

    out->size_ = k;
    return out;
  }

  // Returns a new set representing the union of this set and the other.
  // O(|this| + |that|).
  UniqueSet<T>* Union(UniqueSet<T>* that, Zone* zone) const {
    if (that->size_ == 0) return this->Copy(zone);
    if (this->size_ == 0) return that->Copy(zone);

    UniqueSet<T>* out = new(zone) UniqueSet<T>();
    out->Grow(this->size_ + that->size_, zone);

    int i = 0, j = 0, k = 0;
    while (i < this->size_ && j < that->size_) {
      Unique<T> a = this->array_[i];
      Unique<T> b = that->array_[j];
      if (a == b) {
        out->array_[k++] = a;
        i++;
        j++;
      } else if (a.raw_address_ < b.raw_address_) {
        out->array_[k++] = a;
        i++;
      } else {
        out->array_[k++] = b;
        j++;
      }
    }

    while (i < this->size_) out->array_[k++] = this->array_[i++];
    while (j < that->size_) out->array_[k++] = that->array_[j++];

    out->size_ = k;
    return out;
  }

  // Makes an exact copy of this set. O(|this| + |that|).
  UniqueSet<T>* Copy(Zone* zone) const {
    UniqueSet<T>* copy = new(zone) UniqueSet<T>();
    copy->size_ = this->size_;
    copy->capacity_ = this->size_;
    copy->array_ = zone->NewArray<Unique<T> >(this->size_);
    memcpy(copy->array_, this->array_, this->size_ * sizeof(Unique<T>));
    return copy;
  }

  void Clear() {
    size_ = 0;
  }

  inline int size() const {
    return size_;
  }

  inline Unique<T> at(int index) const {
    ASSERT(index >= 0 && index < size_);
    return array_[index];
  }

 private:
  // These sets should be small, since operations are implemented with simple
  // linear algorithms. Enforce a maximum size.
  static const int kMaxCapacity = 65535;

  uint16_t size_;
  uint16_t capacity_;
  Unique<T>* array_;

  // Grow the size of internal storage to be at least {size} elements.
  void Grow(int size, Zone* zone) {
    CHECK(size < kMaxCapacity);  // Enforce maximum size.
    if (capacity_ < size) {
      int new_capacity = 2 * capacity_ + size;
      if (new_capacity > kMaxCapacity) new_capacity = kMaxCapacity;
      Unique<T>* new_array = zone->NewArray<Unique<T> >(new_capacity);
      if (size_ > 0) {
        memcpy(new_array, array_, size_ * sizeof(Unique<T>));
      }
      capacity_ = new_capacity;
      array_ = new_array;
    }
  }
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_UNIQUE_H_
