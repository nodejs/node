// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_SMALL_VECTOR_H_
#define V8_BASE_SMALL_VECTOR_H_

#include <type_traits>

#include "src/base/bits.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// Minimal SmallVector implementation. Uses inline storage first, switches to
// malloc when it overflows.
template <typename T, size_t kSize>
class SmallVector {
  // Currently only support trivially copyable and trivially destructible data
  // types, as it uses memcpy to copy elements and never calls destructors.
  ASSERT_TRIVIALLY_COPYABLE(T);
  STATIC_ASSERT(std::is_trivially_destructible<T>::value);

 public:
  static constexpr size_t kInlineSize = kSize;

  SmallVector() = default;
  explicit SmallVector(size_t size) { resize_no_init(size); }
  SmallVector(const SmallVector& other) V8_NOEXCEPT { *this = other; }
  SmallVector(SmallVector&& other) V8_NOEXCEPT { *this = std::move(other); }

  ~SmallVector() {
    if (is_big()) free(begin_);
  }

  SmallVector& operator=(const SmallVector& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    size_t other_size = other.size();
    if (capacity() < other_size) {
      // Create large-enough heap-allocated storage.
      if (is_big()) free(begin_);
      begin_ = reinterpret_cast<T*>(malloc(sizeof(T) * other_size));
      end_of_storage_ = begin_ + other_size;
    }
    memcpy(begin_, other.begin_, sizeof(T) * other_size);
    end_ = begin_ + other_size;
    return *this;
  }

  SmallVector& operator=(SmallVector&& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    if (other.is_big()) {
      if (is_big()) free(begin_);
      begin_ = other.begin_;
      end_ = other.end_;
      end_of_storage_ = other.end_of_storage_;
      other.reset();
    } else {
      DCHECK_GE(capacity(), other.size());  // Sanity check.
      size_t other_size = other.size();
      memcpy(begin_, other.begin_, sizeof(T) * other_size);
      end_ = begin_ + other_size;
    }
    return *this;
  }

  T* data() { return begin_; }
  const T* data() const { return begin_; }

  T* begin() { return begin_; }
  const T* begin() const { return begin_; }

  T* end() { return end_; }
  const T* end() const { return end_; }

  size_t size() const { return end_ - begin_; }
  bool empty() const { return end_ == begin_; }
  size_t capacity() const { return end_of_storage_ - begin_; }

  T& back() {
    DCHECK_NE(0, size());
    return end_[-1];
  }

  T& operator[](size_t index) {
    DCHECK_GT(size(), index);
    return begin_[index];
  }

  const T& operator[](size_t index) const {
    DCHECK_GT(size(), index);
    return begin_[index];
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    if (V8_UNLIKELY(end_ == end_of_storage_)) Grow();
    new (end_) T(std::forward<Args>(args)...);
    ++end_;
  }

  void pop_back(size_t count = 1) {
    DCHECK_GE(size(), count);
    end_ -= count;
  }

  void resize_no_init(size_t new_size) {
    // Resizing without initialization is safe if T is trivially copyable.
    ASSERT_TRIVIALLY_COPYABLE(T);
    if (new_size > capacity()) Grow(new_size);
    end_ = begin_ + new_size;
  }

  // Clear without freeing any storage.
  void clear() { end_ = begin_; }

  // Clear and go back to inline storage.
  void reset() {
    begin_ = inline_storage_begin();
    end_ = begin_;
    end_of_storage_ = begin_ + kInlineSize;
  }

 private:
  T* begin_ = inline_storage_begin();
  T* end_ = begin_;
  T* end_of_storage_ = begin_ + kInlineSize;
  typename std::aligned_storage<sizeof(T) * kInlineSize, alignof(T)>::type
      inline_storage_;

  void Grow(size_t min_capacity = 0) {
    size_t in_use = end_ - begin_;
    size_t new_capacity =
        base::bits::RoundUpToPowerOfTwo(std::max(min_capacity, 2 * capacity()));
    T* new_storage = reinterpret_cast<T*>(malloc(sizeof(T) * new_capacity));
    memcpy(new_storage, begin_, sizeof(T) * in_use);
    if (is_big()) free(begin_);
    begin_ = new_storage;
    end_ = new_storage + in_use;
    end_of_storage_ = new_storage + new_capacity;
  }

  bool is_big() const { return begin_ != inline_storage_begin(); }

  T* inline_storage_begin() { return reinterpret_cast<T*>(&inline_storage_); }
  const T* inline_storage_begin() const {
    return reinterpret_cast<const T*>(&inline_storage_);
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_SMALL_VECTOR_H_
