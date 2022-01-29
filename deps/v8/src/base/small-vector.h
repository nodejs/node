// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_SMALL_VECTOR_H_
#define V8_BASE_SMALL_VECTOR_H_

#include <algorithm>
#include <type_traits>
#include <utility>

#include "src/base/bits.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// Minimal SmallVector implementation. Uses inline storage first, switches to
// dynamic storage when it overflows.
template <typename T, size_t kSize, typename Allocator = std::allocator<T>>
class SmallVector {
  // Currently only support trivially copyable and trivially destructible data
  // types, as it uses memcpy to copy elements and never calls destructors.
  ASSERT_TRIVIALLY_COPYABLE(T);
  STATIC_ASSERT(std::is_trivially_destructible<T>::value);

 public:
  static constexpr size_t kInlineSize = kSize;

  explicit SmallVector(const Allocator& allocator = Allocator())
      : allocator_(allocator) {}
  explicit SmallVector(size_t size, const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    resize_no_init(size);
  }
  SmallVector(const SmallVector& other,
              const Allocator& allocator = Allocator()) V8_NOEXCEPT
      : allocator_(allocator) {
    *this = other;
  }
  SmallVector(SmallVector&& other,
              const Allocator& allocator = Allocator()) V8_NOEXCEPT
      : allocator_(allocator) {
    *this = std::move(other);
  }
  SmallVector(std::initializer_list<T> init,
              const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    resize_no_init(init.size());
    memcpy(begin_, init.begin(), sizeof(T) * init.size());
  }

  ~SmallVector() {
    if (is_big()) FreeDynamicStorage();
  }

  SmallVector& operator=(const SmallVector& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    size_t other_size = other.size();
    if (capacity() < other_size) {
      // Create large-enough heap-allocated storage.
      if (is_big()) FreeDynamicStorage();
      begin_ = AllocateDynamicStorage(other_size);
      end_of_storage_ = begin_ + other_size;
    }
    memcpy(begin_, other.begin_, sizeof(T) * other_size);
    end_ = begin_ + other_size;
    return *this;
  }

  SmallVector& operator=(SmallVector&& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    if (other.is_big()) {
      if (is_big()) FreeDynamicStorage();
      begin_ = other.begin_;
      end_ = other.end_;
      end_of_storage_ = other.end_of_storage_;
      other.reset_to_inline_storage();
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
  const T& back() const {
    DCHECK_NE(0, size());
    return end_[-1];
  }

  T& operator[](size_t index) {
    DCHECK_GT(size(), index);
    return begin_[index];
  }

  const T& at(size_t index) const {
    DCHECK_GT(size(), index);
    return begin_[index];
  }

  const T& operator[](size_t index) const { return at(index); }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    T* end = end_;
    if (V8_UNLIKELY(end == end_of_storage_)) end = Grow();
    new (end) T(std::forward<Args>(args)...);
    end_ = end + 1;
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

  // Clear without reverting back to inline storage.
  void clear() { end_ = begin_; }

 private:
  V8_NO_UNIQUE_ADDRESS Allocator allocator_;

  T* begin_ = inline_storage_begin();
  T* end_ = begin_;
  T* end_of_storage_ = begin_ + kInlineSize;
  typename std::aligned_storage<sizeof(T) * kInlineSize, alignof(T)>::type
      inline_storage_;

  // Grows the backing store by a factor of two. Returns the new end of the used
  // storage (this reduces binary size).
  V8_NOINLINE T* Grow() { return Grow(0); }

  // Grows the backing store by a factor of two, and at least to {min_capacity}.
  V8_NOINLINE T* Grow(size_t min_capacity) {
    size_t in_use = end_ - begin_;
    size_t new_capacity =
        base::bits::RoundUpToPowerOfTwo(std::max(min_capacity, 2 * capacity()));
    T* new_storage = AllocateDynamicStorage(new_capacity);
    if (new_storage == nullptr) {
      // Should be: V8::FatalProcessOutOfMemory, but we don't include V8 from
      // base. The message is intentionally the same as FatalProcessOutOfMemory
      // since that will help fuzzers and chromecrash to categorize such
      // crashes appropriately.
      FATAL("Fatal process out of memory: base::SmallVector::Grow");
    }
    memcpy(new_storage, begin_, sizeof(T) * in_use);
    if (is_big()) FreeDynamicStorage();
    begin_ = new_storage;
    end_ = new_storage + in_use;
    end_of_storage_ = new_storage + new_capacity;
    return end_;
  }

  T* AllocateDynamicStorage(size_t number_of_elements) {
    return allocator_.allocate(number_of_elements);
  }

  void FreeDynamicStorage() {
    DCHECK(is_big());
    allocator_.deallocate(begin_, end_of_storage_ - begin_);
  }

  // Clear and go back to inline storage. Dynamic storage is *not* freed. For
  // internal use only.
  void reset_to_inline_storage() {
    begin_ = inline_storage_begin();
    end_ = begin_;
    end_of_storage_ = begin_ + kInlineSize;
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
