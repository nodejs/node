// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/growing-circular-queue.h"

#include <memory>

#include "testing/gtest-support.h"

namespace v8::base {

TEST(GrowingCircularQueueTest, EmptyAndEnqueueDequeue) {
  GrowingCircularQueue<int> queue;
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(0u, queue.size());
  EXPECT_EQ(0u, queue.capacity());

  queue.Enqueue(10);
  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(1u, queue.size());
  EXPECT_EQ(GrowingCircularQueue<int>::kInitialCapacity, queue.capacity());
  EXPECT_EQ(10, queue.Dequeue());
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(0u, queue.size());
  // Capacity is retained after draining.
  EXPECT_EQ(GrowingCircularQueue<int>::kInitialCapacity, queue.capacity());

  // Enqueuing again reuses the existing buffer without growing.
  queue.Enqueue(20);
  queue.Enqueue(30);
  EXPECT_EQ(2u, queue.size());
  EXPECT_EQ(GrowingCircularQueue<int>::kInitialCapacity, queue.capacity());
  EXPECT_EQ(20, queue.Dequeue());
  EXPECT_EQ(30, queue.Dequeue());
  EXPECT_TRUE(queue.empty());
}

TEST(GrowingCircularQueueTest, WrapAroundWithoutGrowing) {
  GrowingCircularQueue<int> queue;
  constexpr size_t kCap = GrowingCircularQueue<int>::kInitialCapacity;

  // Fill to full capacity and drain repeatedly to exercise wrap-around.
  for (int round = 0; round < 5; ++round) {
    for (size_t i = 0; i < kCap; ++i) {
      queue.Enqueue(static_cast<int>(round * 100 + i));
    }
    EXPECT_EQ(kCap, queue.size());
    EXPECT_EQ(kCap, queue.capacity());

    // Dequeue two, enqueue two (wrapping tail around the end of the buffer).
    EXPECT_EQ(round * 100 + 0, queue.Dequeue());
    EXPECT_EQ(round * 100 + 1, queue.Dequeue());
    EXPECT_EQ(kCap - 2, queue.size());

    queue.Enqueue(round * 100 + 50);
    queue.Enqueue(round * 100 + 51);
    EXPECT_EQ(kCap, queue.size());
    EXPECT_EQ(kCap, queue.capacity());

    for (size_t i = 2; i < kCap; ++i) {
      EXPECT_EQ(static_cast<int>(round * 100 + i), queue.Dequeue());
    }
    EXPECT_EQ(round * 100 + 50, queue.Dequeue());
    EXPECT_EQ(round * 100 + 51, queue.Dequeue());
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(kCap, queue.capacity());
  }
}

TEST(GrowingCircularQueueTest, GrowContiguousAndWrapped) {
  // 1. Grow when head_ is at buffer_.begin() (contiguous).
  {
    GrowingCircularQueue<int> queue;
    for (int i = 0; i < 20; ++i) {
      queue.Enqueue(i);
      EXPECT_EQ(static_cast<size_t>(i + 1), queue.size());
    }
    EXPECT_GE(queue.capacity(), 20u);
    for (int i = 0; i < 20; ++i) {
      EXPECT_EQ(i, queue.Dequeue());
    }
    EXPECT_TRUE(queue.empty());
  }

  // 2. Grow when head_ > tail_ (wrapped around).
  {
    GrowingCircularQueue<int> queue;
    constexpr int kCap =
        static_cast<int>(GrowingCircularQueue<int>::kInitialCapacity);
    // Enqueue kCap - 1 elements and dequeue them so head_ advances to index
    // kCap - 1.
    for (int i = 0; i < kCap - 1; ++i) {
      queue.Enqueue(-1);
      EXPECT_EQ(-1, queue.Dequeue());
    }
    EXPECT_TRUE(queue.empty());

    // Now enqueue 20 elements; the first growth happens while wrapped around.
    for (int i = 0; i < 20; ++i) {
      queue.Enqueue(100 + i);
      EXPECT_EQ(static_cast<size_t>(i + 1), queue.size());
    }
    for (int i = 0; i < 20; ++i) {
      EXPECT_EQ(100 + i, queue.Dequeue());
    }
    EXPECT_TRUE(queue.empty());
  }
}

TEST(GrowingCircularQueueTest, MoveOnlyElementsAndClear) {
  GrowingCircularQueue<std::unique_ptr<int>> queue;
  for (int i = 0; i < 10; ++i) {
    queue.Enqueue(std::make_unique<int>(i));
  }
  EXPECT_EQ(10u, queue.size());

  GrowingCircularQueue<std::unique_ptr<int>> moved_queue = std::move(queue);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(0u, queue.size());
  EXPECT_EQ(0u, queue.capacity());
  EXPECT_EQ(10u, moved_queue.size());

  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<int> val = moved_queue.Dequeue();
    ASSERT_NE(nullptr, val);
    EXPECT_EQ(i, *val);
  }
  EXPECT_EQ(5u, moved_queue.size());

  moved_queue.Clear();
  EXPECT_TRUE(moved_queue.empty());
  EXPECT_EQ(0u, moved_queue.size());

  // Reusable after Clear().
  moved_queue.Enqueue(std::make_unique<int>(42));
  EXPECT_EQ(1u, moved_queue.size());
  EXPECT_EQ(42, *moved_queue.Dequeue());
  EXPECT_TRUE(moved_queue.empty());
}

namespace {

template <typename T>
class CountingAllocator {
 public:
  using value_type = T;

  explicit CountingAllocator(size_t* active_bytes)
      : active_bytes_(active_bytes) {}

  T* allocate(size_t n) {
    *active_bytes_ += n * sizeof(T);
    return std::allocator<T>{}.allocate(n);
  }

  void deallocate(T* p, size_t n) noexcept {
    *active_bytes_ -= n * sizeof(T);
    std::allocator<T>{}.deallocate(p, n);
  }

 private:
  size_t* active_bytes_;
};

}  // namespace

TEST(GrowingCircularQueueTest, CustomAllocator) {
  size_t active_bytes = 0;
  {
    GrowingCircularQueue<int, CountingAllocator<int>> queue{
        CountingAllocator<int>(&active_bytes)};
    EXPECT_EQ(0u, active_bytes);

    for (int i = 0; i < 10; ++i) {
      queue.Enqueue(i);
    }
    EXPECT_EQ(queue.capacity() * sizeof(int), active_bytes);

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(i, queue.Dequeue());
    }
  }
  EXPECT_EQ(0u, active_bytes);
}

}  // namespace v8::base
