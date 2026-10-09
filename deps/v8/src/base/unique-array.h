// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_UNIQUE_ARRAY_H_
#define V8_BASE_UNIQUE_ARRAY_H_

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "include/v8config.h"
#include "src/base/algorithm.h"
#include "src/base/logging.h"
#include "src/base/vector.h"

namespace v8::base {

// A dynamically allocated `unique_ptr<T[]>` keeping track of the size. This is
// analogous to std::dynarray or base::HeapArray in Chromium.
template <typename T>
class UniqueArray final {
 public:
  // Allocates a new array of the specified size via the default allocator.
  // Elements in the new array are value-initialized.
  static UniqueArray<T> New(size_t size) {
    if (size == 0) return {};
    return UniqueArray<T>(std::make_unique<T[]>(size), size);
  }

  // Allocates a new array of the specified size via the default allocator and
  // initializes all elements by assigning from `init`.
  template <typename U>
  static UniqueArray<T> New(size_t size, U init) {
    if (size == 0) return {};
    UniqueArray<std::remove_const_t<T>> arr =
        UniqueArray<std::remove_const_t<T>>::NewForOverwrite(size);
    std::fill_n(arr.begin(), size, init);
    return arr;
  }

  // Allocates a new array of the specified size via the default allocator.
  // Elements in the new array are default-initialized.
  static UniqueArray<T> NewForOverwrite(size_t size) {
    if (size == 0) return {};
    return UniqueArray<T>(std::make_unique_for_overwrite<T[]>(size), size);
  }

  // Allocates a new array containing the specified collection of values.
  static UniqueArray<T> CopiedFrom(const std::remove_const_t<T>* data,
                                   size_t size) {
    auto result = UniqueArray<std::remove_const_t<T>>::NewForOverwrite(size);
    base::Copy(data, data + size, result.begin());
    return result;
  }

  static UniqueArray<T> CopiedFrom(
      std::span<const std::remove_const_t<T>> that) {
    return CopiedFrom(that.data(), that.size());
  }

  UniqueArray() = default;

  // Disallow copying.
  UniqueArray(const UniqueArray&) = delete;
  UniqueArray& operator=(const UniqueArray&) = delete;

  // Move construction and move assignment from `UniqueArray<U>` to
  // `UniqueArray<T>`, instantiable if `std::unique_ptr<U>` can be converted to
  // `std::unique_ptr<T>`. Can also be used to convert `UniqueArray<T>` to
  // `UniqueArray<const T>`.
  // These also function as the standard move construction/assignment operator.
  // `other` is left as an empty array.
  template <typename U>
    requires std::is_convertible_v<std::unique_ptr<U>, std::unique_ptr<T>>
  UniqueArray(UniqueArray<U>&& other) V8_NOEXCEPT {
    *this = std::move(other);
  }

  template <typename U>
    requires std::is_convertible_v<std::unique_ptr<U>, std::unique_ptr<T>>
  UniqueArray& operator=(UniqueArray<U>&& other) V8_NOEXCEPT {
    static_assert(sizeof(U) == sizeof(T));
    data_ = std::move(other.data_);
    length_ = other.length_;
    DCHECK_NULL(other.data_);
    other.length_ = 0;
    return *this;
  }

  // Returns the length of the array as a size_t.
  constexpr size_t size() const { return length_; }

  // Returns whether or not the array is empty.
  constexpr bool empty() const { return length_ == 0; }

  constexpr T* begin() const V8_LIFETIME_BOUND {
    DCHECK_IMPLIES(length_ > 0, data_ != nullptr);
    return data_.get();
  }

  constexpr T* end() const V8_LIFETIME_BOUND { return begin() + length_; }

  // In addition to `begin`, do provide a `data()` accessor for API
  // compatibility with other sequential containers.
  constexpr T* data() const V8_LIFETIME_BOUND { return begin(); }

  constexpr std::reverse_iterator<T*> rbegin() const V8_LIFETIME_BOUND {
    return std::make_reverse_iterator(end());
  }
  constexpr std::reverse_iterator<T*> rend() const V8_LIFETIME_BOUND {
    return std::make_reverse_iterator(begin());
  }

  // Access individual array elements - checks bounds in debug mode.
  T& operator[](size_t index) const V8_LIFETIME_BOUND {
    DCHECK_LT(index, length_);
    return data_[index];
  }

  // Returns a `Vector<T>` view of the data in this array.
  Vector<T> as_vector() const V8_LIFETIME_BOUND { return {begin(), size()}; }

  // Returns a `std::span<T>` view of the data in this array.
  std::span<T> as_span() const V8_LIFETIME_BOUND { return {begin(), size()}; }

  // Releases the backing data from this array and transfers ownership to the
  // caller. This array will be empty afterwards.
  std::unique_ptr<T[]> ReleaseData() {
    length_ = 0;
    return std::move(data_);
  }

  bool operator==(std::nullptr_t) const { return data_ == nullptr; }

 private:
  template <typename U>
  friend class UniqueArray;

  UniqueArray(std::unique_ptr<T[]> data, size_t length)
      : data_(std::move(data)), length_(length) {
    DCHECK_IMPLIES(length_ > 0, data_ != nullptr);
  }

  std::unique_ptr<T[]> data_;
  size_t length_ = 0;
};

// Construct a UniqueArray from a start pointer and a size.
// The data will be copied.
template <typename T>
inline UniqueArray<T> UniqueCopyOf(const T* data, size_t size) {
  return UniqueArray<T>::CopiedFrom(data, size);
}

// Construct a UniqueArray from anything compatible with std::data and
// std::size (e.g. an array, or a container providing a `data()` and `size()`
// accessor). The data will be copied.
template <typename Container>
inline auto UniqueCopyOf(const Container& c)
    -> decltype(UniqueCopyOf(std::data(c), std::size(c))) {
  return UniqueCopyOf(std::data(c), std::size(c));
}

}  // namespace v8::base

#endif  // V8_BASE_UNIQUE_ARRAY_H_
