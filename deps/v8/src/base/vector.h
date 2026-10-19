// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_VECTOR_H_
#define V8_BASE_VECTOR_H_

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>

#include "include/v8config.h"
#include "src/base/algorithm.h"
#include "src/base/hashing.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8::base {

template <typename T>
class Vector;

}  // namespace v8::base

// Mark `Vector` as satisfying the `view` and `borrowed_range` concepts.
// This should be done before the definition of `Vector`, so that any
// inlined calls to range functionality use the correct specializations.
template <typename T>
inline constexpr bool std::ranges::enable_view<v8::base::Vector<T>> = true;
template <typename T>
inline constexpr bool std::ranges::enable_borrowed_range<v8::base::Vector<T>> =
    true;

namespace v8::base {

template <typename T>
class Vector final {
 public:
  using value_type = T;
  using iterator = T*;
  using const_iterator = const T*;

  constexpr Vector() = default;

  constexpr Vector(T* data V8_LIFETIME_BOUND, size_t length)
      : span_(data, length) {
    DCHECK(length == 0 || data != nullptr);
  }

  template <typename U, size_t n>
    requires std::is_convertible_v<std::span<U, n>, std::span<T>>
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr Vector(std::span<U, n> span) : span_(span) {}

  template <typename U, size_t n>
    requires std::is_convertible_v<std::span<U, n>, std::span<T>>
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr Vector(std::array<U, n>& arr V8_LIFETIME_BOUND) : span_(arr) {}

  template <typename U, size_t n>
    requires std::is_convertible_v<std::span<const U, n>, std::span<T>>
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr Vector(const std::array<U, n>& arr V8_LIFETIME_BOUND)
      : span_(arr) {}

  // Returns a vector using the same backing storage as this one,
  // spanning from and including 'from', to but not including 'to'.
  constexpr Vector<T> SubVector(size_t from, size_t to) const {
    DCHECK_LE(from, to);
    return subspan(from, to - from);
  }
  constexpr Vector<T> SubVectorFrom(size_t from) const {
    return subspan(from);
  }

  template <class U>
  void OverwriteWith(Vector<U> other) {
    DCHECK_EQ(size(), other.size());
    base::Copy(other.begin(), other.end(), begin());
  }

  template <class U, size_t n>
  void OverwriteWith(const std::array<U, n>& other) {
    DCHECK_EQ(size(), other.size());
    base::Copy(other.begin(), other.end(), begin());
  }

  // Returns the length of the vector. Only use this if you really need an
  // integer return value. Use {size()} otherwise.
  int length() const {
    CHECK_GE(std::numeric_limits<int>::max(), size());
    return static_cast<int>(size());
  }

  // Returns the length of the vector as a size_t.
  constexpr size_t size() const { return span_.size(); }

  // Returns whether or not the vector is empty.
  constexpr bool empty() const { return span_.empty(); }

  // Access individual vector elements - checks bounds in debug mode.
  T& operator[](size_t index) const {
    DCHECK_LT(index, size());
    return span_[index];
  }

  const T& at(size_t index) const { return operator[](index); }

  constexpr T& front() const {
    DCHECK_LT(0, size());
    return span_.front();
  }

  constexpr T& back() const {
    DCHECK_LT(0, size());
    return span_.back();
  }

  constexpr Vector<T> first(size_t count) const {
    DCHECK_LE(count, size());
    return Vector<T>(span_.first(count));
  }

  constexpr Vector<T> last(size_t count) const {
    DCHECK_LE(count, size());
    return Vector<T>(span_.last(count));
  }

  constexpr Vector<T> subspan(size_t offset,
                              size_t count = std::dynamic_extent) const {
    DCHECK_LE(offset, size());
    DCHECK(count == std::dynamic_extent || count <= size() - offset);
    return Vector<T>(span_.subspan(offset, count));
  }

  // Returns a pointer to the start of the data in the vector.
  constexpr T* begin() const { return span_.data(); }
  constexpr const T* cbegin() const { return span_.data(); }

  // For consistency with other containers, do also provide a {data} accessor.
  constexpr T* data() const { return span_.data(); }

  // Returns a pointer past the end of the data in the vector.
  constexpr T* end() const { return span_.data() + span_.size(); }
  constexpr const T* cend() const { return span_.data() + span_.size(); }

  constexpr std::reverse_iterator<T*> rbegin() const {
    return std::make_reverse_iterator(end());
  }
  constexpr std::reverse_iterator<T*> rend() const {
    return std::make_reverse_iterator(begin());
  }

  void Truncate(size_t length) {
    DCHECK_LE(length, size());
    span_ = span_.first(length);
  }

  const Vector<T> operator+(size_t offset) const {
    return SubVectorFrom(offset);
  }

  Vector<T> operator+=(size_t offset) {
    DCHECK_LE(offset, size());
    span_ = span_.subspan(offset);
    return *this;
  }

  // Implicit conversion from Vector<T> to Vector<const U> if
  // - T* is convertible to const U*, and
  // - U and T have the same size.
  // Note that this conversion is only safe for `*const* U`; writes would
  // violate covariance.
  template <typename U>
    requires std::is_convertible_v<T*, const U*> && (sizeof(U) == sizeof(T))
  constexpr operator Vector<const U>() const {
    return {span_.data(), span_.size()};
  }

  explicit constexpr operator std::span<T>() const noexcept { return span_; }

  template <typename S>
  static Vector<T> cast(Vector<S> input) {
    // Casting is potentially dangerous, so be really restrictive here. This
    // might be lifted once we have use cases for that.
    static_assert(std::is_trivial_v<S> && std::is_standard_layout_v<S>);
    static_assert(std::is_trivial_v<T> && std::is_standard_layout_v<T>);
    DCHECK_EQ(0, (input.size() * sizeof(S)) % sizeof(T));
    DCHECK_EQ(0, reinterpret_cast<uintptr_t>(input.begin()) % alignof(T));
    return Vector<T>(reinterpret_cast<T*>(input.begin()),
                     input.size() * sizeof(S) / sizeof(T));
  }

  bool operator==(const Vector<T>& other) const {
    return std::equal(begin(), end(), other.begin(), other.end());
  }

  template <typename TT = T>
    requires(!std::is_const_v<TT>)
  bool operator==(const Vector<const T>& other) const {
    return std::equal(begin(), end(), other.begin(), other.end());
  }

 private:
  std::span<T> span_;
};

template <typename T>
V8_INLINE size_t hash_value(base::Vector<T> v) {
  return hash_range(v.begin(), v.end());
}

// The vectors returned by {StaticCharVector}, {CStrVector}, or {OneByteVector}
// do not contain a null-termination byte. If you want the null byte, use
// {ArrayVector}.

// Known length, constexpr.
template <size_t N>
constexpr Vector<const char> StaticCharVector(
    const char (&array V8_LIFETIME_BOUND)[N]) {
  return {array, N - 1};
}

// Unknown length, not constexpr.
inline Vector<const char> CStrVector(const char* data V8_LIFETIME_BOUND) {
  return {data, strlen(data)};
}

inline Vector<const char> StrVector(std::string_view str V8_LIFETIME_BOUND) {
  return {str.data(), str.size()};
}

// OneByteVector is never constexpr because the data pointer is
// {reinterpret_cast}ed.
inline Vector<const uint8_t> OneByteVector(const char* data V8_LIFETIME_BOUND,
                                           size_t length) {
  return {reinterpret_cast<const uint8_t*>(data), length};
}

inline Vector<const uint8_t> OneByteVector(const char* data V8_LIFETIME_BOUND) {
  return OneByteVector(data, strlen(data));
}

template <size_t N>
Vector<const uint8_t> StaticOneByteVector(
    const char (&array V8_LIFETIME_BOUND)[N]) {
  return OneByteVector(array, N - 1);
}

// For string literals, ArrayVector("foo") returns a vector ['f', 'o', 'o', \0]
// with length 4 and null-termination.
// If you want ['f', 'o', 'o'], use CStrVector("foo").
template <typename T, size_t N>
inline constexpr Vector<T> ArrayVector(T (&arr V8_LIFETIME_BOUND)[N]) {
  return {arr, N};
}

// Construct a Vector from a start pointer and a size.
template <typename T>
inline constexpr Vector<T> VectorOf(T* start V8_LIFETIME_BOUND, size_t size) {
  return {start, size};
}

// Construct a Vector from anything compatible with std::data and std::size (ie,
// an array, or a container providing a {data()} and {size()} accessor).
template <typename Container>
inline constexpr auto VectorOf(Container&& c V8_LIFETIME_BOUND)
    -> decltype(VectorOf(std::data(c), std::size(c))) {
  return VectorOf(std::data(c), std::size(c));
}

// Construct a Vector from an initializer list. The vector can obviously only be
// used as long as the initializer list is live. Valid uses include direct use
// in parameter lists: F(VectorOf({1, 2, 3}));
template <typename T>
inline constexpr Vector<const T> VectorOf(
    std::initializer_list<T> list V8_LIFETIME_BOUND) {
  return VectorOf(list.begin(), list.size());
}

}  // namespace v8::base

#endif  // V8_BASE_VECTOR_H_
