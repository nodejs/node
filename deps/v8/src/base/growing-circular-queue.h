// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_GROWING_CIRCULAR_QUEUE_H_
#define V8_BASE_GROWING_CIRCULAR_QUEUE_H_

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "include/v8config.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8::base {

// A dynamically growing FIFO circular queue backed by an `Allocator`-allocated
// buffer and two pointers (`head_` and `tail_`).
//
// Invariants:
// - When empty: `head_ == nullptr` (`tail_` is `nullptr` if unallocated, or in
//   `[buffer_, buffer_end_)` if allocated).
// - When non-empty: `head_` points to the next element to be dequeued, and
//   `tail_` points to the next slot to be written on enqueue.
// - When full (or unallocated): `head_ == tail_`.
template <typename T, typename Allocator = std::allocator<T>>
class GrowingCircularQueue final {
 public:
  static constexpr size_t kInitialCapacity = 4;

  GrowingCircularQueue() = default;
  explicit GrowingCircularQueue(const Allocator& allocator)
      : allocator_(allocator) {}

  GrowingCircularQueue(const GrowingCircularQueue&) = delete;
  GrowingCircularQueue& operator=(const GrowingCircularQueue&) = delete;

  GrowingCircularQueue(GrowingCircularQueue&& other) V8_NOEXCEPT
      : allocator_(std::move(other.allocator_)),
        buffer_(std::exchange(other.buffer_, nullptr)),
        buffer_end_(std::exchange(other.buffer_end_, nullptr)),
        head_(std::exchange(other.head_, nullptr)),
        tail_(std::exchange(other.tail_, nullptr)) {}

  GrowingCircularQueue& operator=(GrowingCircularQueue&& other) V8_NOEXCEPT {
    if (this == &other) return *this;
    if (buffer_ != nullptr) FreeStorage();
    allocator_ = std::move(other.allocator_);
    buffer_ = std::exchange(other.buffer_, nullptr);
    buffer_end_ = std::exchange(other.buffer_end_, nullptr);
    head_ = std::exchange(other.head_, nullptr);
    tail_ = std::exchange(other.tail_, nullptr);
    return *this;
  }

  ~GrowingCircularQueue() {
    if (buffer_ != nullptr) FreeStorage();
  }

  constexpr bool empty() const { return head_ == nullptr; }

  constexpr size_t size() const {
    if (empty()) return 0;
    if (tail_ > head_) return tail_ - head_;
    return capacity() - (head_ - tail_);
  }

  constexpr size_t capacity() const { return buffer_end_ - buffer_; }

  void Enqueue(T value) {
    if (V8_UNLIKELY(head_ == tail_)) {
      return GrowAndEnqueue(std::move(value));
    }
    EnqueueUnchecked(std::move(value));
  }

  T Dequeue() {
    DCHECK(!empty());
    T result = std::move(*head_);
    std::destroy_at(head_);
    if (++head_ == buffer_end_) {
      head_ = buffer_;
    }
    if (head_ == tail_) {
      head_ = nullptr;
    }
    return result;
  }

  void Clear() {
    DestroyElements();
    head_ = nullptr;
    tail_ = buffer_;
  }

 private:
  V8_INLINE void EnqueueUnchecked(T value) {
    DCHECK_NE(head_, tail_);
    if (head_ == nullptr) {
      head_ = tail_;
    }
    std::construct_at(tail_, std::move(value));
    if (++tail_ == buffer_end_) {
      tail_ = buffer_;
    }
  }

  V8_NOINLINE V8_PRESERVE_MOST void GrowAndEnqueue(T value) {
    Grow();
    EnqueueUnchecked(std::move(value));
  }

  void Grow() {
    DCHECK_EQ(head_, tail_);
    if (buffer_ == nullptr) {
      buffer_ = allocator_.allocate(kInitialCapacity);
      buffer_end_ = buffer_ + kInitialCapacity;
      tail_ = buffer_;
      return;
    }
    size_t old_capacity = capacity();
    CHECK_LE(old_capacity, std::numeric_limits<size_t>::max() / 2);
    size_t new_capacity = 2 * old_capacity;
    T* new_buffer = allocator_.allocate(new_capacity);
    T* mid = std::uninitialized_move(head_, buffer_end_, new_buffer);
    tail_ = std::uninitialized_move(buffer_, head_, mid);
    std::destroy(buffer_, buffer_end_);
    allocator_.deallocate(buffer_, old_capacity);
    buffer_ = new_buffer;
    buffer_end_ = new_buffer + new_capacity;
    head_ = new_buffer;
  }

  void DestroyElements() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (!empty()) {
        if (tail_ > head_) {
          std::destroy(head_, tail_);
        } else {
          std::destroy(head_, buffer_end_);
          std::destroy(buffer_, tail_);
        }
      }
    }
  }

  V8_NOINLINE V8_PRESERVE_MOST void FreeStorage() {
    DestroyElements();
    allocator_.deallocate(buffer_, capacity());
  }

  V8_NO_UNIQUE_ADDRESS Allocator allocator_;
  T* buffer_ = nullptr;
  T* buffer_end_ = nullptr;
  T* head_ = nullptr;
  T* tail_ = nullptr;
};

}  // namespace v8::base

#endif  // V8_BASE_GROWING_CIRCULAR_QUEUE_H_
