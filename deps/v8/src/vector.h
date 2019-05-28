// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VECTOR_H_
#define V8_VECTOR_H_

#include <algorithm>
#include <cstring>
#include <iterator>

#include "src/allocation.h"
#include "src/checks.h"
#include "src/globals.h"

namespace v8 {
namespace internal {


template <typename T>
class Vector {
 public:
  constexpr Vector() : start_(nullptr), length_(0) {}

  Vector(T* data, size_t length) : start_(data), length_(length) {
    DCHECK(length == 0 || data != nullptr);
  }

  template <int N>
  explicit constexpr Vector(T (&arr)[N]) : start_(arr), length_(N) {}

  static Vector<T> New(int length) {
    return Vector<T>(NewArray<T>(length), length);
  }

  // Returns a vector using the same backing storage as this one,
  // spanning from and including 'from', to but not including 'to'.
  Vector<T> SubVector(size_t from, size_t to) const {
    DCHECK_LE(from, to);
    DCHECK_LE(to, length_);
    return Vector<T>(start() + from, to - from);
  }

  // Returns the length of the vector.
  int length() const {
    DCHECK(length_ <= static_cast<size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(length_);
  }

  // Returns the length of the vector as a size_t.
  constexpr size_t size() const { return length_; }

  // Returns whether or not the vector is empty.
  constexpr bool empty() const { return length_ == 0; }

  // Returns the pointer to the start of the data in the vector.
  constexpr T* start() const { return start_; }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](size_t index) const {
    DCHECK_LT(index, length_);
    return start_[index];
  }

  const T& at(size_t index) const { return operator[](index); }

  T& first() { return start_[0]; }

  T& last() {
    DCHECK_LT(0, length_);
    return start_[length_ - 1];
  }

  typedef T* iterator;
  constexpr iterator begin() const { return start_; }
  constexpr iterator end() const { return start_ + length_; }

  // Returns a clone of this vector with a new backing store.
  Vector<T> Clone() const {
    T* result = NewArray<T>(length_);
    for (size_t i = 0; i < length_; i++) result[i] = start_[i];
    return Vector<T>(result, length_);
  }

  template <typename CompareFunction>
  void Sort(CompareFunction cmp, size_t s, size_t l) {
    std::sort(start() + s, start() + s + l, RawComparer<CompareFunction>(cmp));
  }

  template <typename CompareFunction>
  void Sort(CompareFunction cmp) {
    std::sort(start(), start() + length(), RawComparer<CompareFunction>(cmp));
  }

  void Sort() {
    std::sort(start(), start() + length());
  }

  template <typename CompareFunction>
  void StableSort(CompareFunction cmp, size_t s, size_t l) {
    std::stable_sort(start() + s, start() + s + l,
                     RawComparer<CompareFunction>(cmp));
  }

  template <typename CompareFunction>
  void StableSort(CompareFunction cmp) {
    std::stable_sort(start(), start() + length(),
                     RawComparer<CompareFunction>(cmp));
  }

  void StableSort() { std::stable_sort(start(), start() + length()); }

  void Truncate(size_t length) {
    DCHECK(length <= length_);
    length_ = length;
  }

  // Releases the array underlying this vector. Once disposed the
  // vector is empty.
  void Dispose() {
    DeleteArray(start_);
    start_ = nullptr;
    length_ = 0;
  }

  Vector<T> operator+(size_t offset) {
    DCHECK_LE(offset, length_);
    return Vector<T>(start_ + offset, length_ - offset);
  }

  Vector<T> operator+=(size_t offset) {
    DCHECK_LE(offset, length_);
    start_ += offset;
    length_ -= offset;
    return *this;
  }

  // Implicit conversion from Vector<T> to Vector<const T>.
  inline operator Vector<const T>() const {
    return Vector<const T>::cast(*this);
  }

  template <typename S>
  static constexpr Vector<T> cast(Vector<S> input) {
    return Vector<T>(reinterpret_cast<T*>(input.start()),
                     input.length() * sizeof(S) / sizeof(T));
  }

  bool operator==(const Vector<const T> other) const {
    if (length_ != other.length_) return false;
    if (start_ == other.start_) return true;
    for (size_t i = 0; i < length_; ++i) {
      if (start_[i] != other.start_[i]) {
        return false;
      }
    }
    return true;
  }

 private:
  T* start_;
  size_t length_;

  template <typename CookedComparer>
  class RawComparer {
   public:
    explicit RawComparer(CookedComparer cmp) : cmp_(cmp) {}
    bool operator()(const T& a, const T& b) {
      return cmp_(&a, &b) < 0;
    }

   private:
    CookedComparer cmp_;
  };
};


template <typename T>
class ScopedVector : public Vector<T> {
 public:
  explicit ScopedVector(int length) : Vector<T>(NewArray<T>(length), length) { }
  ~ScopedVector() {
    DeleteArray(this->start());
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedVector);
};

template <typename T>
class OwnedVector {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(OwnedVector);
  OwnedVector(std::unique_ptr<T[]> data, size_t length)
      : data_(std::move(data)), length_(length) {
    DCHECK_IMPLIES(length_ > 0, data_ != nullptr);
  }
  // Implicit conversion from {OwnedVector<U>} to {OwnedVector<T>}, instantiable
  // if {std::unique_ptr<U>} can be converted to {std::unique_ptr<T>}.
  // Can be used to convert {OwnedVector<T>} to {OwnedVector<const T>}.
  template <typename U,
            typename = typename std::enable_if<std::is_convertible<
                std::unique_ptr<U>, std::unique_ptr<T>>::value>::type>
  OwnedVector(OwnedVector<U>&& other)
      : data_(std::move(other.data_)), length_(other.length_) {
    STATIC_ASSERT(sizeof(U) == sizeof(T));
    other.length_ = 0;
  }

  // Returns the length of the vector as a size_t.
  constexpr size_t size() const { return length_; }

  // Returns whether or not the vector is empty.
  constexpr bool empty() const { return length_ == 0; }

  // Returns the pointer to the start of the data in the vector.
  T* start() const {
    DCHECK_IMPLIES(length_ > 0, data_ != nullptr);
    return data_.get();
  }

  // Returns a {Vector<T>} view of the data in this vector.
  Vector<T> as_vector() const { return Vector<T>(start(), size()); }

  // Releases the backing data from this vector and transfers ownership to the
  // caller. This vector will be empty afterwards.
  std::unique_ptr<T[]> ReleaseData() {
    length_ = 0;
    return std::move(data_);
  }

  // Allocates a new vector of the specified size via the default allocator.
  static OwnedVector<T> New(size_t size) {
    if (size == 0) return {};
    return OwnedVector<T>(std::unique_ptr<T[]>(new T[size]), size);
  }

  // Allocates a new vector containing the specified collection of values.
  // {Iterator} is the common type of {std::begin} and {std::end} called on a
  // {const U&}. This function is only instantiable if that type exists.
  template <typename U, typename Iterator = typename std::common_type<
                            decltype(std::begin(std::declval<const U&>())),
                            decltype(std::end(std::declval<const U&>()))>::type>
  static OwnedVector<T> Of(const U& collection) {
    Iterator begin = std::begin(collection);
    Iterator end = std::end(collection);
    OwnedVector<T> vec = New(std::distance(begin, end));
    std::copy(begin, end, vec.start());
    return vec;
  }

  bool operator==(std::nullptr_t) const { return data_ == nullptr; }
  bool operator!=(std::nullptr_t) const { return data_ != nullptr; }

 private:
  template <typename U>
  friend class OwnedVector;

  std::unique_ptr<T[]> data_;
  size_t length_ = 0;
};

inline int StrLength(const char* string) {
  size_t length = strlen(string);
  DCHECK(length == static_cast<size_t>(static_cast<int>(length)));
  return static_cast<int>(length);
}

template <size_t N>
constexpr Vector<const uint8_t> StaticCharVector(const char (&array)[N]) {
  return Vector<const uint8_t>::cast(Vector<const char>(array, N - 1));
}

inline Vector<const char> CStrVector(const char* data) {
  return Vector<const char>(data, StrLength(data));
}

inline Vector<const uint8_t> OneByteVector(const char* data, int length) {
  return Vector<const uint8_t>(reinterpret_cast<const uint8_t*>(data), length);
}

inline Vector<const uint8_t> OneByteVector(const char* data) {
  return OneByteVector(data, StrLength(data));
}

inline Vector<char> MutableCStrVector(char* data) {
  return Vector<char>(data, StrLength(data));
}

inline Vector<char> MutableCStrVector(char* data, int max) {
  int length = StrLength(data);
  return Vector<char>(data, (length < max) ? length : max);
}

template <typename T, int N>
inline constexpr Vector<T> ArrayVector(T (&arr)[N]) {
  return Vector<T>(arr);
}

// Construct a Vector from a start pointer and a size.
template <typename T>
inline constexpr Vector<T> VectorOf(T* start, size_t size) {
  return Vector<T>(start, size);
}

// Construct a Vector from anything providing a {data()} and {size()} accessor.
template <typename Container>
inline constexpr auto VectorOf(Container&& c)
    -> decltype(VectorOf(c.data(), c.size())) {
  return VectorOf(c.data(), c.size());
}

template <typename T, int kSize>
class EmbeddedVector : public Vector<T> {
 public:
  EmbeddedVector() : Vector<T>(buffer_, kSize) {}

  explicit EmbeddedVector(T initial_value) : Vector<T>(buffer_, kSize) {
    for (int i = 0; i < kSize; ++i) {
      buffer_[i] = initial_value;
    }
  }

  // When copying, make underlying Vector to reference our buffer.
  EmbeddedVector(const EmbeddedVector& rhs) V8_NOEXCEPT : Vector<T>(rhs) {
    MemCopy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
  }

  EmbeddedVector& operator=(const EmbeddedVector& rhs) V8_NOEXCEPT {
    if (this == &rhs) return *this;
    Vector<T>::operator=(rhs);
    MemCopy(buffer_, rhs.buffer_, sizeof(T) * kSize);
    this->set_start(buffer_);
    return *this;
  }

 private:
  T buffer_[kSize];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_VECTOR_H_
