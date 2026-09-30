//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of vector container.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_VECTOR_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_VECTOR_H

#include "hdr/func/free.h"
#include "hdr/func/malloc.h"
#include "hdr/func/realloc.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/new.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/CPP/utility.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"

#include <stddef.h> // For max_align_t, ptrdiff_t

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <typename T> class vector {
  static_assert(alignof(T) <= alignof(max_align_t),
                "Overaligned types are not supported by cpp::vector");

public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;
  using iterator = T *;
  using const_iterator = const T *;

  [[nodiscard]] LIBC_INLINE constexpr size_type max_size() const noexcept {
    return static_cast<size_type>(-1) / sizeof(T);
  }

private:
  T *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;

  static constexpr size_t DEFAULT_INITIAL_CAPACITY = 16;

  LIBC_INLINE void destroy_elements(size_t from, size_t to) {
    if constexpr (!is_trivially_destructible_v<T>) {
      while (to > from) {
        --to;
        data_[to].~T();
      }
    }
  }

  LIBC_INLINE void deallocate() {
    if (data_) {
      destroy_elements(0, size_);
      ::free(data_);
      data_ = nullptr;
      size_ = 0;
      capacity_ = 0;
    }
  }

  [[nodiscard]] LIBC_INLINE bool reallocate(size_t new_cap) {
    if constexpr (is_trivially_copyable_v<T>) {
      void *new_data = ::realloc(data_, new_cap * sizeof(T));
      if (!new_data)
        return false;
      data_ = static_cast<T *>(new_data);
      capacity_ = new_cap;
      return true;
    } else {
      void *new_raw = ::malloc(new_cap * sizeof(T));
      if (!new_raw)
        return false;
      T *new_data = static_cast<T *>(new_raw);
      for (size_t i = 0; i < size_; ++i) {
        new (new_data + i) T(cpp::move(data_[i]));
        data_[i].~T();
      }
      ::free(data_);
      data_ = new_data;
      capacity_ = new_cap;
      return true;
    }
  }

public:
  LIBC_INLINE constexpr vector() = default;

  LIBC_INLINE ~vector() { deallocate(); }

  // Move constructor
  LIBC_INLINE vector(vector &&other) noexcept
      : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
  }

  // Move assignment
  LIBC_INLINE vector &operator=(vector &&other) noexcept {
    if (this != &other) {
      deallocate();
      data_ = other.data_;
      size_ = other.size_;
      capacity_ = other.capacity_;
      other.data_ = nullptr;
      other.size_ = 0;
      other.capacity_ = 0;
    }
    return *this;
  }

  // Copy operations are deleted to prevent accidental implicit allocations.
  vector(const vector &) = delete;
  vector &operator=(const vector &) = delete;

  // Element access
  [[nodiscard]] LIBC_INLINE reference operator[](size_t i) {
    LIBC_ASSERT(i < size_);
    return data_[i];
  }

  [[nodiscard]] LIBC_INLINE const_reference operator[](size_t i) const {
    LIBC_ASSERT(i < size_);
    return data_[i];
  }

  [[nodiscard]] LIBC_INLINE pointer data() { return data_; }
  [[nodiscard]] LIBC_INLINE const_pointer data() const { return data_; }

  [[nodiscard]] LIBC_INLINE reference front() {
    LIBC_ASSERT(size_ > 0);
    return data_[0];
  }

  [[nodiscard]] LIBC_INLINE const_reference front() const {
    LIBC_ASSERT(size_ > 0);
    return data_[0];
  }

  [[nodiscard]] LIBC_INLINE reference back() {
    LIBC_ASSERT(size_ > 0);
    return data_[size_ - 1];
  }

  [[nodiscard]] LIBC_INLINE const_reference back() const {
    LIBC_ASSERT(size_ > 0);
    return data_[size_ - 1];
  }

  // Iterators
  [[nodiscard]] LIBC_INLINE iterator begin() { return data_; }
  [[nodiscard]] LIBC_INLINE const_iterator begin() const { return data_; }
  [[nodiscard]] LIBC_INLINE const_iterator cbegin() const { return data_; }
  [[nodiscard]] LIBC_INLINE iterator end() { return data_ + size_; }
  [[nodiscard]] LIBC_INLINE const_iterator end() const { return data_ + size_; }
  [[nodiscard]] LIBC_INLINE const_iterator cend() const {
    return data_ + size_;
  }

  // Capacity
  [[nodiscard]] LIBC_INLINE bool empty() const { return size_ == 0; }
  [[nodiscard]] LIBC_INLINE size_t size() const { return size_; }
  [[nodiscard]] LIBC_INLINE size_t capacity() const { return capacity_; }

  // Reserves at least new_cap elements. Returns true on success, false on OOM.
  [[nodiscard]] LIBC_INLINE bool reserve(size_t new_cap) {
    if (new_cap <= capacity_)
      return true;
    if (new_cap > max_size())
      return false;
    return reallocate(new_cap);
  }

  LIBC_INLINE void shrink_to_fit() {
    if (size_ == capacity_)
      return;
    if (size_ == 0) {
      reset();
      return;
    }
    static_cast<void>(reallocate(size_));
  }

  // Modifiers
  template <typename... Args>
  [[nodiscard]] LIBC_INLINE bool emplace_back(Args &&...args) {
    if (size_ < capacity_) {
      new (data_ + size_) T(cpp::forward<Args>(args)...);
      ++size_;
      return true;
    }

    size_t new_cap = DEFAULT_INITIAL_CAPACITY;
    if (capacity_ > 0) {
      if (capacity_ > max_size() / 2)
        return false;
      new_cap = capacity_ * 2;
    }

    void *new_raw = ::malloc(new_cap * sizeof(T));
    if (!new_raw)
      return false;
    T *new_data = static_cast<T *>(new_raw);

    // Construct the new element before moving/freeing old storage so that
    // arguments referencing the vector's own buffer remain valid.
    new (new_data + size_) T(cpp::forward<Args>(args)...);

    for (size_t i = 0; i < size_; ++i) {
      new (new_data + i) T(cpp::move(data_[i]));
      data_[i].~T();
    }
    ::free(data_);
    data_ = new_data;
    capacity_ = new_cap;
    ++size_;
    return true;
  }

  [[nodiscard]] LIBC_INLINE bool push_back(const T &val) {
    return emplace_back(val);
  }

  [[nodiscard]] LIBC_INLINE bool push_back(T &&val) {
    return emplace_back(cpp::move(val));
  }

  LIBC_INLINE void pop_back() {
    LIBC_ASSERT(size_ > 0);
    --size_;
    if constexpr (!is_trivially_destructible_v<T>)
      data_[size_].~T();
  }

  LIBC_INLINE void clear() {
    destroy_elements(0, size_);
    size_ = 0;
  }

  // Deallocates backing memory and resets size and capacity to 0.
  LIBC_INLINE void reset() { deallocate(); }

  [[nodiscard]] LIBC_INLINE bool resize(size_t new_size) {
    if (new_size < size_) {
      destroy_elements(new_size, size_);
      size_ = new_size;
      return true;
    }
    if (new_size > size_) {
      if (!reserve(new_size))
        return false;
      for (size_t i = size_; i < new_size; ++i)
        new (data_ + i) T();
      size_ = new_size;
    }
    return true;
  }

  [[nodiscard]] LIBC_INLINE bool resize(size_t new_size, const T &value) {
    if (new_size < size_) {
      destroy_elements(new_size, size_);
      size_ = new_size;
      return true;
    }
    if (new_size > size_) {
      // Copy value before reserve in case value references this vector's data.
      T val_copy(value);
      if (!reserve(new_size))
        return false;
      for (size_t i = size_; i < new_size; ++i)
        new (data_ + i) T(val_copy);
      size_ = new_size;
    }
    return true;
  }

  LIBC_INLINE void swap(vector &other) noexcept {
    cpp::swap(data_, other.data_);
    cpp::swap(size_, other.size_);
    cpp::swap(capacity_, other.capacity_);
  }
};

template <typename T>
LIBC_INLINE void swap(vector<T> &lhs, vector<T> &rhs) noexcept {
  lhs.swap(rhs);
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_VECTOR_H
