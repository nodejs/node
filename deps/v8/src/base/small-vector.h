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
#include "src/base/vector.h"

namespace v8 {
namespace base {

// Minimal SmallVector implementation. Uses inline storage first, switches to
// dynamic storage when it overflows.
template <typename T, size_t kSize, typename Allocator = std::allocator<T>>
class SmallVector {
 public:
  static constexpr size_t kInlineSize = kSize;
  using value_type = T;

  SmallVector() = default;
  explicit SmallVector(const Allocator& allocator) : allocator_(allocator) {}
  explicit V8_INLINE SmallVector(size_t size,
                                 const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    resize(size);
  }
  explicit V8_INLINE SmallVector(size_t size, const T& initial_value,
                                 const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    resize(size, initial_value);
  }
  SmallVector(const SmallVector& other) V8_NOEXCEPT
      : allocator_(other.allocator_) {
    *this = other;
  }
  SmallVector(const SmallVector& other, const Allocator& allocator) V8_NOEXCEPT
      : allocator_(allocator) {
    *this = other;
  }
  SmallVector(SmallVector&& other) V8_NOEXCEPT
      : allocator_(std::move(other.allocator_)) {
    *this = std::move(other);
  }
  SmallVector(SmallVector&& other, const Allocator& allocator) V8_NOEXCEPT
      : allocator_(allocator) {
    *this = std::move(other);
  }
  V8_INLINE SmallVector(std::initializer_list<T> init,
                        const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    if (init.size() > capacity()) Grow(init.size());
    DCHECK_GE(capacity(), init.size());  // Sanity check.
    std::uninitialized_move(init.begin(), init.end(), begin_);
    end_ = begin_ + init.size();
  }
  explicit V8_INLINE SmallVector(base::Vector<const T> init,
                                 const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    if (init.size() > capacity()) Grow(init.size());
    DCHECK_GE(capacity(), init.size());  // Sanity check.
    std::uninitialized_copy(init.begin(), init.end(), begin_);
    end_ = begin_ + init.size();
  }

  ~SmallVector() { FreeStorage(); }

  SmallVector& operator=(const SmallVector& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    size_t other_size = other.size();
    if (capacity() < other_size) {
      // Create large-enough heap-allocated storage.
      FreeStorage();
      begin_ = AllocateDynamicStorage(other_size);
      end_of_storage_ = begin_ + other_size;
      std::uninitialized_copy(other.begin_, other.end_, begin_);
    } else if constexpr (kHasTrivialElement) {
      std::copy(other.begin_, other.end_, begin_);
    } else {
      ptrdiff_t to_copy =
          std::min(static_cast<ptrdiff_t>(other_size), end_ - begin_);
      std::copy(other.begin_, other.begin_ + to_copy, begin_);
      if (other.begin_ + to_copy < other.end_) {
        std::uninitialized_copy(other.begin_ + to_copy, other.end_,
                                begin_ + to_copy);
      } else {
        std::destroy_n(begin_ + to_copy, size() - to_copy);
      }
    }
    end_ = begin_ + other_size;
    return *this;
  }

  SmallVector& operator=(SmallVector&& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    if (other.is_big()) {
      FreeStorage();
      begin_ = other.begin_;
      end_ = other.end_;
      end_of_storage_ = other.end_of_storage_;
    } else {
      DCHECK_GE(capacity(), other.size());  // Sanity check.
      size_t other_size = other.size();
      if constexpr (kHasTrivialElement) {
        std::move(other.begin_, other.end_, begin_);
      } else {
        ptrdiff_t to_move =
            std::min(static_cast<ptrdiff_t>(other_size), end_ - begin_);
        std::move(other.begin_, other.begin_ + to_move, begin_);
        if (other.begin_ + to_move < other.end_) {
          std::uninitialized_move(other.begin_ + to_move, other.end_,
                                  begin_ + to_move);
        } else {
          std::destroy_n(begin_ + to_move, size() - to_move);
        }
      }
      end_ = begin_ + other_size;
    }
    other.reset_to_inline_storage();
    return *this;
  }

  T* data() { return begin_; }
  const T* data() const { return begin_; }

  T* begin() { return begin_; }
  const T* begin() const { return begin_; }

  T* end() { return end_; }
  const T* end() const { return end_; }

  auto rbegin() { return std::make_reverse_iterator(end_); }
  auto rbegin() const { return std::make_reverse_iterator(end_); }

  auto rend() { return std::make_reverse_iterator(begin_); }
  auto rend() const { return std::make_reverse_iterator(begin_); }

  size_t size() const { return end_ - begin_; }
  bool empty() const { return end_ == begin_; }
  size_t capacity() const { return end_of_storage_ - begin_; }

  T& front() {
    DCHECK_NE(0, size());
    return begin_[0];
  }
  const T& front() const {
    DCHECK_NE(0, size());
    return begin_[0];
  }

  T& back() {
    DCHECK_NE(0, size());
    return end_[-1];
  }
  const T& back() const {
    DCHECK_NE(0, size());
    return end_[-1];
  }

  T& at(size_t index) {
    DCHECK_GT(size(), index);
    return begin_[index];
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
    if (V8_UNLIKELY(end_ == end_of_storage_)) Grow();
    void* storage = end_;
    end_ += 1;
    new (storage) T(std::forward<Args>(args)...);
  }

  void push_back(T x) { emplace_back(std::move(x)); }

  void pop_back(size_t count = 1) {
    DCHECK_GE(size(), count);
    end_ -= count;
    std::destroy_n(end_, count);
  }

  T* insert(T* pos, const T& value) {
    return insert(pos, static_cast<size_t>(1), value);
  }
  T* insert(T* pos, size_t count, const T& value) {
    DCHECK_LE(pos, end_);
    size_t offset = pos - begin_;
    size_t old_size = size();
    resize(old_size + count);
    pos = begin_ + offset;
    T* old_end = begin_ + old_size;
    DCHECK_LE(old_end, end_);
    std::move_backward(pos, old_end, end_);
    std::fill_n(pos, count, value);
    return pos;
  }
  template <typename It>
  T* insert(T* pos, It begin, It end) {
    DCHECK_LE(pos, end_);
    size_t offset = pos - begin_;
    size_t count = std::distance(begin, end);
    size_t old_size = size();
    resize(old_size + count);
    pos = begin_ + offset;
    T* old_end = begin_ + old_size;
    DCHECK_LE(old_end, end_);
    std::move_backward(pos, old_end, end_);
    std::copy(begin, end, pos);
    return pos;
  }

  T* insert(T* pos, std::initializer_list<T> values) {
    return insert(pos, values.begin(), values.end());
  }

  void erase(T* erase_start) {
    DCHECK_GE(erase_start, begin_);
    DCHECK_LE(erase_start, end_);
    ptrdiff_t count = end_ - erase_start;
    end_ = erase_start;
    std::destroy_n(end_, count);
  }

  void resize(size_t new_size) {
    if (new_size > capacity()) Grow(new_size);
    T* new_end = begin_ + new_size;
    if constexpr (!kHasTrivialElement) {
      if (new_end > end_) {
        std::uninitialized_default_construct(end_, new_end);
      } else {
        std::destroy_n(new_end, end_ - new_end);
      }
    }
    end_ = new_end;
  }

  void resize(size_t new_size, const T& initial_value) {
    if (new_size > capacity()) Grow(new_size);
    T* new_end = begin_ + new_size;
    if (new_end > end_) {
      std::uninitialized_fill(end_, new_end, initial_value);
    } else {
      std::destroy_n(new_end, end_ - new_end);
    }
    end_ = new_end;
  }

  void reserve(size_t new_capacity) {
    if (new_capacity > capacity()) Grow(new_capacity);
  }

  // Clear without reverting back to inline storage.
  void clear() {
    std::destroy_n(begin_, end_ - begin_);
    end_ = begin_;
  }

  Allocator get_allocator() const { return allocator_; }

 private:
  // Grows the backing store by a factor of two. Returns the new end of the used
  // storage (this reduces binary size).
  V8_NOINLINE V8_PRESERVE_MOST void Grow() { Grow(0); }

  // Grows the backing store by a factor of two, and at least to {min_capacity}.
  V8_NOINLINE V8_PRESERVE_MOST void Grow(size_t min_capacity) {
    size_t in_use = end_ - begin_;
    size_t new_capacity =
        base::bits::RoundUpToPowerOfTwo(std::max(min_capacity, 2 * capacity()));
    T* new_storage = AllocateDynamicStorage(new_capacity);
    if (new_storage == nullptr) {
      FatalOOM(OOMType::kProcess, "base::SmallVector::Grow");
    }
    std::uninitialized_move(begin_, end_, new_storage);
    FreeStorage();
    begin_ = new_storage;
    end_ = new_storage + in_use;
    end_of_storage_ = new_storage + new_capacity;
  }

  T* AllocateDynamicStorage(size_t number_of_elements) {
    return allocator_.allocate(number_of_elements);
  }

  V8_NOINLINE V8_PRESERVE_MOST void FreeStorage() {
    std::destroy_n(begin_, end_ - begin_);
    if (is_big()) allocator_.deallocate(begin_, end_of_storage_ - begin_);
  }

  // Clear and go back to inline storage. Dynamic storage is *not* freed. For
  // internal use only.
  void reset_to_inline_storage() {
    if constexpr (!kHasTrivialElement) {
      if (!is_big()) std::destroy_n(begin_, end_ - begin_);
    }
    begin_ = inline_storage_begin();
    end_ = begin_;
    end_of_storage_ = begin_ + kInlineSize;
  }

  bool is_big() const { return begin_ != inline_storage_begin(); }

  T* inline_storage_begin() { return reinterpret_cast<T*>(inline_storage_); }
  const T* inline_storage_begin() const {
    return reinterpret_cast<const T*>(inline_storage_);
  }

  V8_NO_UNIQUE_ADDRESS Allocator allocator_;

  // Invariants:
  // 1. The elements in the range between `begin_` (included) and `end_` (not
  //    included) will be initialized at all times.
  // 2. All other elements outside the range, both in the inline storage and in
  //    the dynamic storage (if it exists), will be uninitialized at all times.

  T* begin_ = inline_storage_begin();
  T* end_ = begin_;
  T* end_of_storage_ = begin_ + kInlineSize;
  alignas(T) char inline_storage_[sizeof(T) * kInlineSize];

  static constexpr bool kHasTrivialElement =
      is_trivially_copyable<T>::value && is_trivially_destructible<T>::value;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_SMALL_VECTOR_H_
