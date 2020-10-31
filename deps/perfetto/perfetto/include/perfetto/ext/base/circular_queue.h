/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_EXT_BASE_CIRCULAR_QUEUE_H_
#define INCLUDE_PERFETTO_EXT_BASE_CIRCULAR_QUEUE_H_

#include <stdint.h>
#include <iterator>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace base {

// CircularQueue is a push-back-only / pop-front-only queue with the following
// characteristics:
// - The storage is based on a flat circular buffer. Beginning and end wrap
//   as necessary, to keep pushes and pops O(1) as long as capacity expansion is
//   not required.
// - Capacity is automatically expanded like in a std::vector. Expansion has a
//   O(N) cost.
// - It allows random access, allowing in-place std::sort.
// - Iterators are not stable. Mutating the container invalidates all iterators.
// - It doesn't bother with const-correctness.
//
// Implementation details:
// Internally, |begin|, |end| and iterators use 64-bit monotonic indexes, which
// are incremented as if the queue was backed by unlimited storage.
// Even assuming that elements are inserted and removed every nanosecond, 64 bit
// is enough for 584 years.
// Wrapping happens only when addressing elements in the underlying circular
// storage. This limits the complexity and avoiding dealing with modular
// arithmetic all over the places.
template <class T>
class CircularQueue {
 public:
  class Iterator {
   public:
    using difference_type = ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator_category = std::random_access_iterator_tag;

    Iterator(CircularQueue* queue, uint64_t pos, uint32_t generation)
        : queue_(queue),
          pos_(pos)
#if PERFETTO_DCHECK_IS_ON()
          ,
          generation_(generation)
#endif
    {
      ignore_result(generation);
    }

    T* operator->() {
#if PERFETTO_DCHECK_IS_ON()
      PERFETTO_DCHECK(generation_ == queue_->generation());
#endif
      return queue_->Get(pos_);
    }

    const T* operator->() const {
      return const_cast<CircularQueue<T>::Iterator*>(this)->operator->();
    }

    T& operator*() { return *(operator->()); }
    const T& operator*() const { return *(operator->()); }

    value_type& operator[](difference_type i) { return *(*this + i); }

    const value_type& operator[](difference_type i) const {
      return const_cast<CircularQueue<T>::Iterator&>(*this)[i];
    }

    Iterator& operator++() {
      Add(1);
      return *this;
    }

    Iterator operator++(int) {
      Iterator ret = *this;
      Add(1);
      return ret;
    }

    Iterator& operator--() {
      Add(-1);
      return *this;
    }

    Iterator operator--(int) {
      Iterator ret = *this;
      Add(-1);
      return ret;
    }

    friend Iterator operator+(const Iterator& iter, difference_type offset) {
      Iterator ret = iter;
      ret.Add(offset);
      return ret;
    }

    Iterator& operator+=(difference_type offset) {
      Add(offset);
      return *this;
    }

    friend Iterator operator-(const Iterator& iter, difference_type offset) {
      Iterator ret = iter;
      ret.Add(-offset);
      return ret;
    }

    Iterator& operator-=(difference_type offset) {
      Add(-offset);
      return *this;
    }

    friend ptrdiff_t operator-(const Iterator& lhs, const Iterator& rhs) {
      return static_cast<ptrdiff_t>(lhs.pos_) -
             static_cast<ptrdiff_t>(rhs.pos_);
    }

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.pos_ == rhs.pos_;
    }

    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
      return lhs.pos_ != rhs.pos_;
    }

    friend bool operator<(const Iterator& lhs, const Iterator& rhs) {
      return lhs.pos_ < rhs.pos_;
    }

    friend bool operator<=(const Iterator& lhs, const Iterator& rhs) {
      return lhs.pos_ <= rhs.pos_;
    }

    friend bool operator>(const Iterator& lhs, const Iterator& rhs) {
      return lhs.pos_ > rhs.pos_;
    }

    friend bool operator>=(const Iterator& lhs, const Iterator& rhs) {
      return lhs.pos_ >= rhs.pos_;
    }

   private:
    inline void Add(difference_type offset) {
      pos_ = static_cast<uint64_t>(static_cast<difference_type>(pos_) + offset);
      PERFETTO_DCHECK(pos_ <= queue_->end_);
    }

    CircularQueue* queue_;
    uint64_t pos_;

#if PERFETTO_DCHECK_IS_ON()
    uint32_t generation_;
#endif
  };

  CircularQueue(size_t initial_capacity = 1024) { Grow(initial_capacity); }

  CircularQueue(CircularQueue&& other) noexcept {
    // Copy all fields using the (private) default copy assignment operator.
    *this = other;
    increment_generation();
    new (&other) CircularQueue();  // Reset the old queue so it's still usable.
  }

  CircularQueue& operator=(CircularQueue&& other) {
    this->~CircularQueue();                      // Destroy the current state.
    new (this) CircularQueue(std::move(other));  // Use the move ctor above.
    return *this;
  }

  ~CircularQueue() {
    if (!entries_) {
      PERFETTO_DCHECK(empty());
      return;
    }
    erase_front(size());  // Invoke destructors on all alive entries.
    PERFETTO_DCHECK(empty());
    free(entries_);
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    increment_generation();
    if (PERFETTO_UNLIKELY(size() >= capacity_))
      Grow();
    T* slot = Get(end_++);
    new (slot) T(std::forward<Args>(args)...);
  }

  void erase_front(size_t n) {
    increment_generation();
    for (; n && (begin_ < end_); --n) {
      Get(begin_)->~T();
      begin_++;  // This needs to be its own statement, Get() checks begin_.
    }
  }

  void pop_front() { erase_front(1); }

  T& at(size_t idx) {
    PERFETTO_DCHECK(idx < size());
    return *Get(begin_ + idx);
  }

  Iterator begin() { return Iterator(this, begin_, generation()); }
  Iterator end() { return Iterator(this, end_, generation()); }
  T& front() { return *begin(); }
  T& back() { return *(end() - 1); }

  bool empty() const { return size() == 0; }

  size_t size() const {
    PERFETTO_DCHECK(end_ - begin_ <= capacity_);
    return static_cast<size_t>(end_ - begin_);
  }

  size_t capacity() const { return capacity_; }

#if PERFETTO_DCHECK_IS_ON()
  uint32_t generation() const { return generation_; }
  void increment_generation() { ++generation_; }
#else
  uint32_t generation() const { return 0; }
  void increment_generation() {}
#endif

 private:
  CircularQueue(const CircularQueue&) = delete;
  CircularQueue& operator=(const CircularQueue&) = default;

  void Grow(size_t new_capacity = 0) {
    // Capacity must be always a power of two. This allows Get() to use a simple
    // bitwise-AND for handling the wrapping instead of a full division.
    new_capacity = new_capacity ? new_capacity : capacity_ * 2;
    PERFETTO_CHECK((new_capacity & (new_capacity - 1)) == 0);  // Must be pow2.

    // On 32-bit systems this might hit the 4GB wall and overflow. We can't do
    // anything other than crash in this case.
    PERFETTO_CHECK(new_capacity > capacity_);
    size_t malloc_size = new_capacity * sizeof(T);
    PERFETTO_CHECK(malloc_size > new_capacity);
    auto* new_vec = static_cast<T*>(malloc(malloc_size));

    // Move all elements in the expanded array.
    size_t new_size = 0;
    for (uint64_t i = begin_; i < end_; i++)
      new (&new_vec[new_size++]) T(std::move(*Get(i)));  // Placement move ctor.

    // Even if all the elements are std::move()-d and likely empty, we are still
    // required to call the dtor for them.
    for (uint64_t i = begin_; i < end_; i++)
      Get(i)->~T();
    free(entries_);  // It's fine to free(nullptr) (for the ctor call case).

    begin_ = 0;
    end_ = new_size;
    capacity_ = new_capacity;
    entries_ = new_vec;
  }

  inline T* Get(uint64_t pos) {
    PERFETTO_DCHECK(pos >= begin_ && pos < end_);
    PERFETTO_DCHECK((capacity_ & (capacity_ - 1)) == 0);  // Must be a pow2.
    auto index = static_cast<size_t>(pos & (capacity_ - 1));
    return &entries_[index];
  }

  // Underlying storage. It's raw malloc-ed rather than being a unique_ptr<T[]>
  // to allow having uninitialized entries inside it.
  T* entries_ = nullptr;
  size_t capacity_ = 0;  // Number of allocated slots (NOT bytes) in |entries_|.

  // The |begin_| and |end_| indexes are monotonic and never wrap. Modular arith
  // is used only when dereferencing entries in the vector.
  uint64_t begin_ = 0;
  uint64_t end_ = 0;

// Generation is used in debug builds only for checking iterator validity.
#if PERFETTO_DCHECK_IS_ON()
  uint32_t generation_ = 0;
#endif
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_CIRCULAR_QUEUE_H_
