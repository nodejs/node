// Copyright 2025 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/chunked_queue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <forward_list>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/macros.h"
#include "absl/base/throw_delegate.h"
#include "absl/container/internal/test_allocator.h"
#include "absl/strings/str_cat.h"

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::SizeIs;

// Hide in a namespace to make sure swap is found via ADL.
namespace adl_namespace {
namespace {
TEST(ChunkedQueueADLTest, Swap) {
  absl::chunked_queue<int64_t> q1;
  absl::chunked_queue<int64_t> q2;
  q1.push_back(4);
  q2.push_back(5);
  q2.push_back(6);
  swap(q1, q2);
  EXPECT_THAT(q1, ElementsAre(5, 6));
  EXPECT_THAT(q2, ElementsAre(4));
}
}  // namespace
}  // namespace adl_namespace

namespace {

template <class T>
using ChunkedQueueBlock =
    absl::container_internal::ChunkedQueueBlock<T, std::allocator<T>>;

TEST(Internal, elements_in_bytes) {
  EXPECT_EQ(size_t{1}, ChunkedQueueBlock<int>::block_size_from_bytes(0));
  EXPECT_EQ(size_t{1}, ChunkedQueueBlock<int>::block_size_from_bytes(
                           sizeof(ChunkedQueueBlock<int>)));
  EXPECT_EQ(size_t{1},
            ChunkedQueueBlock<int>::block_size_from_bytes(sizeof(int)));
  EXPECT_EQ(size_t{2}, ChunkedQueueBlock<int>::block_size_from_bytes(
                           sizeof(ChunkedQueueBlock<int>) + 2 * sizeof(int)));
}

TEST(Internal, BlockSizedDelete) {
  struct Item {
    int i;
    char c;
  };
  std::allocator<Item> allocator;
  auto* block = ChunkedQueueBlock<Item>::New(3, &allocator);
  ChunkedQueueBlock<Item>::Delete(block, &allocator);
}

template <size_t elem_size>
void BlockSizeRounding() {
  struct Elem {
    char data[elem_size];
  };
  typedef ChunkedQueueBlock<Elem> Block;
  for (size_t n = 1; n < 100; ++n) {
    SCOPED_TRACE(n);
    std::allocator<Elem> allocator;
    Block* b = Block::New(n, &allocator);
    EXPECT_GE(b->size(), n);
    Block::Delete(b, &allocator);
  }
}

TEST(Internal, BlockSizeRounding1) { BlockSizeRounding<1>(); }
TEST(Internal, BlockSizeRounding17) { BlockSizeRounding<17>(); }
TEST(Internal, BlockSizeRounding101) { BlockSizeRounding<101>(); }
TEST(Internal, BlockSizeRounding528) { BlockSizeRounding<528>(); }

TEST(ChunkedQueue, MinMaxBlockSize) {
  absl::chunked_queue<int64_t, 1, 2> q = {1, 2, 3};
  EXPECT_THAT(q, ElementsAre(1, 2, 3));
}

TEST(ChunkedQueue, Empty) {
  absl::chunked_queue<int64_t> q;
  EXPECT_TRUE(q.empty());
  q.push_back(10);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.front(), 10);
  EXPECT_EQ(q.back(), 10);
  q.pop_front();
  EXPECT_TRUE(q.empty());
  q.clear();
  EXPECT_TRUE(q.empty());
}

TEST(ChunkedQueue, CopyConstruct) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  absl::chunked_queue<int64_t> r(q);
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, CopyConstructMultipleChunks) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_back(1);
  q.push_back(2);
  q.push_back(3);
  absl::chunked_queue<int64_t, 2> r(q);
  EXPECT_THAT(r, ElementsAre(1, 2, 3));
  EXPECT_EQ(3, r.size());
}

TEST(ChunkedQueue, BeginEndConstruct) {
  std::vector<int64_t> src = {1, 2, 3, 4, 5};
  absl::chunked_queue<int64_t, 2> q(src.begin(), src.end());
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(5, q.size());
}

TEST(ChunkedQueue, InitializerListConstruct) {
  absl::chunked_queue<int64_t, 2> q = {1, 2, 3, 4, 5};
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(5, q.size());
}

TEST(ChunkedQueue, CountConstruct) {
  absl::chunked_queue<int64_t> q(3);
  EXPECT_THAT(q, ElementsAre(0, 0, 0));
  EXPECT_EQ(3, q.size());
}

TEST(ChunkedQueue, CountValueConstruct) {
  absl::chunked_queue<int64_t> q(3, 10);
  EXPECT_THAT(q, ElementsAre(10, 10, 10));
  EXPECT_EQ(3, q.size());
}

TEST(ChunkedQueue, InitializerListAssign) {
  absl::chunked_queue<int64_t, 2> q;
  q = {1, 2, 3, 4, 5};
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(5, q.size());
}

TEST(ChunkedQueue, CopyAssign) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  absl::chunked_queue<int64_t> r = q;
  EXPECT_THAT(r, ElementsAre(1));
}

TEST(ChunkedQueue, CopyAssignSelf) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  q = *&q;  // Avoid -Wself-assign.
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(1, q.size());
}

TEST(ChunkedQueue, CopyAssignDestinationBigger) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  absl::chunked_queue<int64_t> r;
  r.push_back(9);
  r.push_back(9);
  r.push_back(9);
  r = q;
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, CopyAssignSourceBiggerMultipleChunks) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_back(1);
  q.push_back(2);
  q.push_back(3);
  absl::chunked_queue<int64_t, 2> r;
  r.push_back(9);
  r = q;
  EXPECT_THAT(r, ElementsAre(1, 2, 3));
  EXPECT_EQ(3, r.size());
}

TEST(ChunkedQueue, CopyAssignDestinationBiggerMultipleChunks) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_back(1);
  absl::chunked_queue<int64_t, 2> r;
  r.push_back(9);
  r.push_back(9);
  r.push_back(9);
  r = q;
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, AssignCountValue) {
  absl::chunked_queue<int64_t> q;
  q.assign(3, 10);
  EXPECT_THAT(q, ElementsAre(10, 10, 10));
  EXPECT_EQ(3, q.size());

  q.assign(2, 20);
  EXPECT_THAT(q, ElementsAre(20, 20));
  EXPECT_EQ(2, q.size());
}

TEST(ChunkedQueue, MoveConstruct) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  absl::chunked_queue<int64_t> r(std::move(q));
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, MoveAssign) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  absl::chunked_queue<int64_t> r;
  r = std::move(q);
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, MoveAssignImmovable) {
  struct Immovable {
    Immovable() = default;

    Immovable(const Immovable&) = delete;
    Immovable& operator=(const Immovable&) = delete;
    Immovable(Immovable&&) = delete;
    Immovable& operator=(Immovable&&) = delete;
  };
  absl::chunked_queue<Immovable> q;
  q.emplace_back();
  absl::chunked_queue<Immovable> r;
  r = std::move(q);
  EXPECT_THAT(r, SizeIs(1));
}

TEST(ChunkedQueue, MoveAssignSelf) {
  absl::chunked_queue<int64_t> q;
  absl::chunked_queue<int64_t>& q2 = q;
  q.push_back(1);
  q = std::move(q2);
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(1, q.size());
}

TEST(ChunkedQueue, MoveAssignDestinationBigger) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  absl::chunked_queue<int64_t> r;
  r.push_back(9);
  r.push_back(9);
  r.push_back(9);
  r = std::move(q);
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, MoveAssignDestinationBiggerMultipleChunks) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_back(1);
  absl::chunked_queue<int64_t, 2> r;
  r.push_back(9);
  r.push_back(9);
  r.push_back(9);
  r = std::move(q);
  EXPECT_THAT(r, ElementsAre(1));
  EXPECT_EQ(1, r.size());
}

TEST(ChunkedQueue, ConstFrontBack) {
  absl::chunked_queue<int64_t> q;
  q.push_back(10);
  EXPECT_EQ(q.front(), 10);
  EXPECT_EQ(q.back(), 10);
  q.front() = 12;
  EXPECT_EQ(q.front(), 12);
  EXPECT_EQ(q.back(), 12);

  const absl::chunked_queue<int64_t>& qref = q;
  EXPECT_EQ(qref.front(), 12);
  EXPECT_EQ(qref.back(), 12);

  q.pop_front();

  // Test at block bloundary and beyond
  for (int i = 0; i < 64; ++i) q.push_back(i + 10);
  EXPECT_EQ(q.front(), 10);
  EXPECT_EQ(q.back(), 73);

  for (int i = 64; i < 128; ++i) q.push_back(i + 10);
  EXPECT_EQ(q.front(), 10);
  EXPECT_EQ(q.back(), 137);
  q.clear();
  EXPECT_TRUE(q.empty());
}

TEST(ChunkedQueue, PushAndPop) {
  absl::chunked_queue<int64_t> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());
  for (int i = 0; i < 10000; i++) {
    q.push_back(i);
    EXPECT_EQ(q.front(), 0) << ": iteration " << i;
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(i + 1, q.size());
  }
  for (int i = 0; i < 10000; i++) {
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(10000 - i, q.size());
    EXPECT_EQ(q.front(), i);
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());
}

TEST(ChunkedQueue, PushFrontAndPop) {
  absl::chunked_queue<int64_t> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());
  // Push enough elements to exercise exponential block growth from
  // `kBlockSizeMin` (1) up to `kBlockSizeMax` (128) across multiple max-sized
  // head blocks (`1 + 2 + 4 + 8 + 16 + 32 + 64 + 128 = 255`).
  constexpr int kNumElements = 500;
  for (int i = 0; i < kNumElements; i++) {
    q.push_front(i);
    EXPECT_EQ(q.front(), i) << ": iteration " << i;
    EXPECT_EQ(q.back(), 0) << ": iteration " << i;
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(i + 1, q.size());
  }
  for (int i = 0; i < kNumElements; i++) {
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(kNumElements - i, q.size());
    EXPECT_EQ(q.front(), kNumElements - 1 - i);
    EXPECT_EQ(q.back(), 0);
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());
}

TEST(ChunkedQueue, PushFrontAndPushBack) {
  absl::chunked_queue<int64_t, 4> q;
  q.push_front(2);
  q.push_back(3);
  q.push_front(1);
  q.push_back(4);
  q.push_front(0);
  q.push_back(5);
  EXPECT_THAT(q, ElementsAre(0, 1, 2, 3, 4, 5));
  EXPECT_EQ(q.front(), 0);
  EXPECT_EQ(q.back(), 5);

  q.pop_front();
  q.push_front(-1);
  EXPECT_THAT(q, ElementsAre(-1, 1, 2, 3, 4, 5));

  q.clear();
  EXPECT_TRUE(q.empty());
  q.push_front(10);
  EXPECT_THAT(q, ElementsAre(10));
  EXPECT_EQ(q.front(), 10);
  EXPECT_EQ(q.back(), 10);
  q.push_front(9);
  q.push_back(11);
  EXPECT_THAT(q, ElementsAre(9, 10, 11));
}

TEST(ChunkedQueue, PushBackAndPopBack) {
  absl::chunked_queue<int64_t, 2> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());

  // Pop from a single-element queue then push to it, and vice versa.
  q.push_back(1);
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 1);
  q.pop_back();
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());

  q.push_back(2);
  EXPECT_THAT(q, ElementsAre(2));
  EXPECT_EQ(q.front(), 2);
  EXPECT_EQ(q.back(), 2);

  // Fill the first block (size 2), pop one element, then push again.
  q.push_back(3);
  EXPECT_THAT(q, ElementsAre(2, 3));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2));
  q.push_back(4);
  EXPECT_THAT(q, ElementsAre(2, 4));

  // Push a 3rd element to allocate a new tail block with a single element,
  // pop that only element in the tail block, then push to the queue again.
  q.push_back(5);
  EXPECT_THAT(q, ElementsAre(2, 4, 5));
  EXPECT_EQ(q.front(), 2);
  EXPECT_EQ(q.back(), 5);
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2, 4));
  EXPECT_EQ(q.front(), 2);
  EXPECT_EQ(q.back(), 4);

  q.push_back(6);
  EXPECT_THAT(q, ElementsAre(2, 4, 6));
  EXPECT_EQ(q.back(), 6);

  // Fill the second block, pop one element (leaving one in the tail block),
  // then push to it again.
  q.push_back(7);
  EXPECT_THAT(q, ElementsAre(2, 4, 6, 7));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2, 4, 6));
  q.push_back(8);
  EXPECT_THAT(q, ElementsAre(2, 4, 6, 8));

  // Pop all elements across block boundaries back to empty, then push again.
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2, 4, 6));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2, 4));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2));
  q.pop_back();
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());

  q.push_back(9);
  EXPECT_THAT(q, ElementsAre(9));
}

TEST(ChunkedQueue, PushFrontAndPopBack) {
  absl::chunked_queue<int64_t, 2> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(0, q.size());

  // Push front to an empty queue (places element at the end of the block),
  // pop back to empty, then push front / push back again.
  q.push_front(1);
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(q.front(), 1);
  EXPECT_EQ(q.back(), 1);
  q.pop_back();
  EXPECT_TRUE(q.empty());

  q.push_front(2);
  EXPECT_THAT(q, ElementsAre(2));
  q.pop_back();
  EXPECT_TRUE(q.empty());

  q.push_back(3);
  EXPECT_THAT(q, ElementsAre(3));
  q.pop_back();
  EXPECT_TRUE(q.empty());

  // Populate multiple blocks via push_front: head block has [_, 2], tail block
  // has [3, 4].
  q.push_front(4);
  q.push_front(3);
  q.push_front(2);
  EXPECT_THAT(q, ElementsAre(2, 3, 4));

  // Pop from the tail block leaving one element [3], then push_back into the
  // freed tail slot and pop_back again.
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2, 3));
  EXPECT_EQ(q.back(), 3);
  q.push_back(5);
  EXPECT_THAT(q, ElementsAre(2, 3, 5));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2, 3));

  // Pop the only remaining element in the tail block, repointing tail_ to the
  // partially filled head block [_, 2].
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(2));
  EXPECT_EQ(q.front(), 2);
  EXPECT_EQ(q.back(), 2);

  // Prepend into the remaining front slot of that block ([1, 2]), then pop all.
  q.push_front(1);
  EXPECT_THAT(q, ElementsAre(1, 2));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(1));
  q.pop_back();
  EXPECT_TRUE(q.empty());
}

TEST(ChunkedQueue, MixedPushAndPopBothEnds) {
  absl::chunked_queue<int64_t, 3> q;
  q.push_front(2);
  q.push_back(3);
  q.push_front(1);
  q.push_back(4);
  q.push_front(0);
  q.push_back(5);
  EXPECT_THAT(q, ElementsAre(0, 1, 2, 3, 4, 5));

  q.pop_back();
  EXPECT_THAT(q, ElementsAre(0, 1, 2, 3, 4));
  q.pop_front();
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4));

  // Alternate push_back and pop_back across block boundary.
  q.push_back(40);
  q.push_back(50);
  q.push_back(60);
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 40, 50, 60));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 40, 50));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 40));
  q.pop_back();
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4));
  q.push_back(400);
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 400));

  while (!q.empty()) {
    q.pop_back();
  }
  EXPECT_TRUE(q.empty());
}

TEST(ChunkedQueue, Swap) {
  absl::chunked_queue<int64_t> q1;
  absl::chunked_queue<int64_t> q2;
  q1.push_back(4);
  q2.push_back(5);
  q2.push_back(6);
  q2.swap(q1);
  EXPECT_EQ(2, q1.size());
  EXPECT_EQ(5, q1.front());
  EXPECT_EQ(1, q2.size());
  EXPECT_EQ(4, q2.front());
  q1.pop_front();
  q1.swap(q2);
  EXPECT_EQ(1, q1.size());
  EXPECT_EQ(4, q1.front());
  EXPECT_EQ(1, q2.size());
  EXPECT_EQ(6, q2.front());
  q1.pop_front();
  q1.swap(q2);
  EXPECT_EQ(1, q1.size());
  EXPECT_EQ(6, q1.front());
  EXPECT_EQ(0, q2.size());
  q1.clear();
  EXPECT_TRUE(q1.empty());
}

TEST(ChunkedQueue, ShrinkToFit) {
  absl::chunked_queue<int64_t> q;
  q.shrink_to_fit();  // Should work on empty
  EXPECT_TRUE(q.empty());

  q.push_back(1);
  q.shrink_to_fit();  // Should work on non-empty
  EXPECT_THAT(q, ElementsAre(1));

  q.clear();
  // We know clear leaves a block and shrink_to_fit should remove it.
  // Hard to test internal memory state without mocks or inspection.
  // But at least we verify it doesn't crash or corrupt.
  q.shrink_to_fit();
  EXPECT_TRUE(q.empty());
}

TEST(ChunkedQueue, ResizeExtends) {
  absl::chunked_queue<int64_t> q;
  q.resize(2);
  EXPECT_THAT(q, ElementsAre(0, 0));
  EXPECT_EQ(2, q.size());
}

TEST(ChunkedQueue, ResizeShrinks) {
  absl::chunked_queue<int64_t> q;
  q.push_back(1);
  q.push_back(2);
  q.resize(1);
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(1, q.size());
}

TEST(ChunkedQueue, ResizeExtendsMultipleBlocks) {
  absl::chunked_queue<int64_t, 2> q;
  q.resize(3);
  EXPECT_THAT(q, ElementsAre(0, 0, 0));
  EXPECT_EQ(3, q.size());
}

TEST(ChunkedQueue, ResizeShrinksMultipleBlocks) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_back(1);
  q.push_back(2);
  q.push_back(3);
  q.resize(1);
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(1, q.size());
}

TEST(ChunkedQueue, ResizeValue) {
  absl::chunked_queue<int64_t> q;
  q.resize(3, 10);
  EXPECT_THAT(q, ElementsAre(10, 10, 10));
  EXPECT_EQ(3, q.size());

  q.resize(5, 20);
  EXPECT_THAT(q, ElementsAre(10, 10, 10, 20, 20));
  EXPECT_EQ(5, q.size());

  q.resize(2, 30);
  EXPECT_THAT(q, ElementsAre(10, 10));
  EXPECT_EQ(2, q.size());
}

template <class T>
struct LimitedAllocator {
  using value_type = T;
  int* alloc_count;
  int max_allocs;

  explicit LimitedAllocator(int* count, int max)
      : alloc_count(count), max_allocs(max) {}
  template <class U>
  LimitedAllocator(const LimitedAllocator<U>& other)
      : alloc_count(other.alloc_count), max_allocs(other.max_allocs) {}

  T* allocate(size_t n) {
    if (*alloc_count >= max_allocs) {
      absl::ThrowStdBadAlloc();
    }
    ++*alloc_count;
    return std::allocator<T>().allocate(n);
  }

  void deallocate(T* p, size_t n) {
    std::allocator<T>().deallocate(p, n);
  }

  template <class U>
  bool operator==(const LimitedAllocator<U>& other) const {
    return alloc_count == other.alloc_count;
  }
  template <class U>
  bool operator!=(const LimitedAllocator<U>& other) const {
    return !(*this == other);
  }
};

TEST(ChunkedQueue, ResizeOverflowSafe) {
  int alloc_count = 0;
  absl::chunked_queue<int64_t, 0, 0, LimitedAllocator<int64_t>> q(
      LimitedAllocator<int64_t>(&alloc_count, 5));
#ifdef ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(q.resize(std::numeric_limits<size_t>::max()), std::bad_alloc);
#else
  EXPECT_DEATH_IF_SUPPORTED(q.resize(std::numeric_limits<size_t>::max()), "");
#endif
}

TEST(ChunkedQueue, MaxSize) {
  absl::chunked_queue<int64_t> q;
  EXPECT_GE(q.max_size(),
            size_t{1} << (sizeof(size_t) * 8 - sizeof(int64_t) - 4));
}

TEST(ChunkedQueue, AssignExtends) {
  absl::chunked_queue<int64_t, 2> q;
  std::vector<int64_t> v = {1, 2, 3, 4, 5};
  q.assign(v.begin(), v.end());
  EXPECT_THAT(q, ElementsAre(1, 2, 3, 4, 5));
  EXPECT_EQ(5, q.size());
}

TEST(ChunkedQueue, AssignShrinks) {
  absl::chunked_queue<int64_t, 2> q = {1, 2, 3, 4, 5};
  std::vector<int64_t> v = {1};
  q.assign(v.begin(), v.end());
  EXPECT_THAT(q, ElementsAre(1));
  EXPECT_EQ(1, q.size());
}

TEST(ChunkedQueue, AssignBoundaryCondition) {
  // Create a queue with fixed block size of 4.
  // 3 blocks: [1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12]
  absl::chunked_queue<int, 4> q = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  // Assign a range that fills exactly the first block (4 elements).
  // This triggers the boundary condition where the assignment loop ends
  // exactly at the limit of the first block.
  std::vector<int> v = {101, 102, 103, 104};
  q.assign(v.begin(), v.end());

  EXPECT_EQ(q.size(), 4);
  EXPECT_EQ(q.front(), 101);
  // Verify back() is valid. If tail_ was incorrectly pointing to the start
  // of the (now deleted) second block, this might access invalid memory
  // or fail assertions.
  EXPECT_EQ(q.back(), 104);

  // Verify we can continue to push elements correctly.
  q.push_back(105);
  EXPECT_EQ(q.size(), 5);
  EXPECT_EQ(q.back(), 105);
}

TEST(ChunkedQueue, Iterator) {
  absl::chunked_queue<int64_t> q;
  EXPECT_TRUE(q.begin() == q.end());

  q.push_back(1);
  absl::chunked_queue<int64_t>::const_iterator iter = q.begin();
  ASSERT_FALSE(iter == q.end());
  ASSERT_EQ(*iter, 1);
  ++iter;
  ASSERT_TRUE(iter == q.end());

  q.push_back(2);
  iter = q.begin();
  ASSERT_EQ(*iter, 1);
  ++iter;
  absl::chunked_queue<int64_t>::const_iterator copy_iter = iter;
  ASSERT_FALSE(copy_iter == q.end());
  ASSERT_EQ(*copy_iter, 2);
  ++copy_iter;
  ASSERT_TRUE(copy_iter == q.end());

  copy_iter = iter;
  ASSERT_FALSE(iter == q.end());
  ASSERT_EQ(*iter, 2);
  ++iter;
  ASSERT_TRUE(iter == q.end());

  ASSERT_FALSE(copy_iter == q.end());
  ASSERT_EQ(*copy_iter, 2);
  ++copy_iter;
  ASSERT_TRUE(copy_iter == q.end());
}

TEST(ChunkedQueue, ReverseIterator) {
  absl::chunked_queue<int64_t> q;
  EXPECT_TRUE(q.rbegin() == q.rend());

  q.push_back(1);
  absl::chunked_queue<int64_t>::const_reverse_iterator iter = q.rbegin();
  ASSERT_FALSE(iter == q.rend());
  ASSERT_EQ(*iter, 1);
  ++iter;
  ASSERT_TRUE(iter == q.rend());

  q.push_back(2);
  iter = q.rbegin();
  ASSERT_EQ(*iter, 2);
  ++iter;
  absl::chunked_queue<int64_t>::const_reverse_iterator copy_iter = iter;
  ASSERT_FALSE(copy_iter == q.rend());
  ASSERT_EQ(*copy_iter, 1);
  ++copy_iter;
  ASSERT_TRUE(copy_iter == q.rend());

  copy_iter = iter;
  ASSERT_FALSE(iter == q.rend());
  ASSERT_EQ(*iter, 1);
  ++iter;
  ASSERT_TRUE(iter == q.rend());

  ASSERT_FALSE(copy_iter == q.rend());
  ASSERT_EQ(*copy_iter, 1);
  ++copy_iter;
  ASSERT_TRUE(copy_iter == q.rend());
}

TEST(ChunkedQueue, IteratorDefaultConstructor) {
  using ConstIter = absl::chunked_queue<int64_t>::const_iterator;
  using Iter = absl::chunked_queue<int64_t>::iterator;
  using ConstRevIter = absl::chunked_queue<int64_t>::const_reverse_iterator;
  using RevIter = absl::chunked_queue<int64_t>::reverse_iterator;
  ConstIter const_iter;
  EXPECT_TRUE(const_iter == ConstIter());
  Iter iter;
  EXPECT_TRUE(iter == Iter());
  ConstRevIter const_rev_iter;
  EXPECT_TRUE(const_rev_iter == ConstRevIter());
  RevIter rev_iter;
  EXPECT_TRUE(rev_iter == RevIter());
}

TEST(ChunkedQueue, IteratorConversion) {
  using ConstIter = absl::chunked_queue<int64_t>::const_iterator;
  using Iter = absl::chunked_queue<int64_t>::iterator;
  using ConstRevIter = absl::chunked_queue<int64_t>::const_reverse_iterator;
  using RevIter = absl::chunked_queue<int64_t>::reverse_iterator;
  EXPECT_FALSE((std::is_convertible_v<ConstIter, Iter>));
  EXPECT_TRUE((std::is_convertible_v<Iter, ConstIter>));
  EXPECT_TRUE((std::is_convertible_v<RevIter, ConstRevIter>));
  absl::chunked_queue<int64_t> q;
  ConstIter it1 = q.begin();
  ConstIter it2 = q.cbegin();
  Iter it3 = q.begin();
  it1 = q.end();
  it2 = q.cend();
  it3 = q.end();
  EXPECT_FALSE((std::is_assignable_v<Iter, ConstIter>));

  ConstRevIter rit1 = q.rbegin();
  ConstRevIter rit2 = q.crbegin();
  RevIter rit3 = q.rbegin();
  rit1 = q.rend();
  rit2 = q.crend();
  rit3 = q.rend();
}

struct TestEntry {
  int x, y;
};

TEST(ChunkedQueue, Iterator2) {
  absl::chunked_queue<TestEntry> q;
  TestEntry e;
  e.x = 1;
  e.y = 2;
  q.push_back(e);
  e.x = 3;
  e.y = 4;
  q.push_back(e);

  absl::chunked_queue<TestEntry>::const_iterator iter = q.begin();
  EXPECT_EQ(iter->x, 1);
  EXPECT_EQ(iter->y, 2);
  ++iter;
  EXPECT_EQ(iter->x, 3);
  EXPECT_EQ(iter->y, 4);
  ++iter;
  EXPECT_TRUE(iter == q.end());
}

TEST(ChunkedQueue, ReverseIterator2) {
  absl::chunked_queue<TestEntry> q;
  TestEntry e;
  e.x = 1;
  e.y = 2;
  q.push_back(e);
  e.x = 3;
  e.y = 4;
  q.push_back(e);

  absl::chunked_queue<TestEntry>::const_reverse_iterator iter = q.rbegin();
  EXPECT_EQ(iter->x, 3);
  EXPECT_EQ(iter->y, 4);
  ++iter;
  EXPECT_EQ(iter->x, 1);
  EXPECT_EQ(iter->y, 2);
  ++iter;
  EXPECT_TRUE(iter == q.rend());
}

TEST(ChunkedQueue, Iterator_MultipleBlocks) {
  absl::chunked_queue<int64_t> q;
  for (int i = 0; i < 130; ++i) {
    absl::chunked_queue<int64_t>::const_iterator iter = q.begin();
    for (int j = 0; j < i; ++j) {
      ASSERT_FALSE(iter == q.end());
      EXPECT_EQ(*iter, j);
      ++iter;
    }
    ASSERT_TRUE(iter == q.end());
    q.push_back(i);
  }

  for (int i = 0; i < 130; ++i) {
    absl::chunked_queue<int64_t>::const_iterator iter = q.begin();
    for (int j = i; j < 130; ++j) {
      ASSERT_FALSE(iter == q.end());
      EXPECT_EQ(*iter, j);
      ++iter;
    }
    q.pop_front();
  }
  EXPECT_TRUE(q.empty());
  EXPECT_TRUE(q.begin() == q.end());
}

TEST(ChunkedQueue, ReverseIterator_MultipleBlocks) {
  absl::chunked_queue<int64_t> q;
  for (int i = 0; i < 130; ++i) {
    absl::chunked_queue<int64_t>::const_reverse_iterator iter = q.rbegin();
    for (int j = i - 1; j >= 0; --j) {
      ASSERT_FALSE(iter == q.rend());
      EXPECT_EQ(*iter, j);
      ++iter;
    }
    ASSERT_TRUE(iter == q.rend());
    q.push_back(i);
  }

  for (int i = 129; i >= 0; --i) {
    absl::chunked_queue<int64_t>::const_reverse_iterator iter = q.rbegin();
    for (int j = i; j >= 0; --j) {
      ASSERT_FALSE(iter == q.rend());
      EXPECT_EQ(*iter, j);
      ++iter;
    }
    ASSERT_TRUE(iter == q.rend());
    q.pop_back();
  }
  EXPECT_TRUE(q.empty());
  EXPECT_TRUE(q.rbegin() == q.rend());
}

TEST(ChunkedQueue, Iterator_PopFrontInvalidate) {
  absl::chunked_queue<int64_t> q;
  for (int i = 0; i < 130; ++i) {
    q.push_back(i);
  }

  auto iter = q.begin();
  for (int i = 0; i < 130; ++i) {
    auto prev = iter++;
    ASSERT_FALSE(prev == q.end());
    EXPECT_EQ(*prev, i);
    q.pop_front();
  }
  ASSERT_TRUE(q.empty());
}

TEST(ChunkedQueue, Iterator_PushBackInvalidate) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_back(0);
  auto i = q.begin();
  EXPECT_EQ(*i, 0);
  q.push_back(1);
  EXPECT_EQ(*++i, 1);
  q.push_back(2);
  EXPECT_EQ(*++i, 2);
}

// Verifies that `push_front` on a non-empty queue does not invalidate
// iterators to existing elements, both when prepending into remaining space in
// the current head block (`1`) and when allocating a new head block (`0`), and
// that advancing `begin()` across the block boundary reaches the existing
// iterator `i`.
TEST(ChunkedQueue, Iterator_PushFrontInvalidate) {
  absl::chunked_queue<int64_t, 2> q;
  q.push_front(2);
  auto i = q.begin();
  EXPECT_EQ(*i, 2);
  q.push_front(1);
  EXPECT_EQ(*i, 2);
  q.push_front(0);
  EXPECT_EQ(*i, 2);
  auto j = q.begin();
  EXPECT_EQ(*j, 0);
  EXPECT_EQ(*++j, 1);
  EXPECT_EQ(*++j, 2);
  EXPECT_TRUE(j == i);
  EXPECT_TRUE(++j == q.end());
}

// Verifies that `pop_back` only invalidates iterators to the removed element
// and `end()`, leaving an iterator that was decremented to the preceding
// element valid across multiple blocks.
TEST(ChunkedQueue, Iterator_PopBackInvalidate) {
  absl::chunked_queue<int64_t> q;
  for (int i = 0; i < 130; ++i) {
    q.push_back(i);
  }

  auto iter = --q.end();
  for (int i = 129; i > 0; --i) {
    auto curr = iter--;
    ASSERT_NE(curr, q.end());
    EXPECT_EQ(*curr, i);
    q.pop_back();
  }
  ASSERT_EQ(iter, q.begin());
  EXPECT_EQ(*iter, 0);
  q.pop_back();
  ASSERT_TRUE(q.empty());
}

TEST(ChunkedQueue, BidirectionalAndReverseIterator) {
  absl::chunked_queue<int64_t, 3> q;
  EXPECT_EQ(q.rbegin(), q.rend());
  EXPECT_EQ(q.crbegin(), q.crend());

  // Test decrementing from `end()` to `begin()` in a single-block queue (where
  // `block->prev() == nullptr` and `begin()` is at `block->start()`).
  {
    absl::chunked_queue<int64_t, 4> single_block_q = {10, 20, 30};
    auto it = single_block_q.end();
    --it;
    EXPECT_EQ(*it, 30);
    EXPECT_EQ(*it--, 30);
    EXPECT_EQ(*it, 20);
    --it;
    EXPECT_EQ(*it, 10);
    EXPECT_EQ(it, single_block_q.begin());
  }

  // Populate from both front and back across multiple blocks.
  for (int i = 4; i >= 0; --i) {
    q.push_front(i);
  }
  for (int i = 5; i < 10; ++i) {
    q.push_back(i);
  }

  // Test decrementing from `end()` to `begin()` via `--it`.
  auto it = q.end();
  for (int expected = 9; expected >= 0; --expected) {
    ASSERT_NE(it, q.begin());
    --it;
    EXPECT_EQ(*it, expected);
  }
  EXPECT_EQ(it, q.begin());

  // Test decrementing from `end()` to `begin()` via `it--`.
  it = q.end();
  for (int expected = 9; expected >= 0; --expected) {
    ASSERT_NE(it, q.begin());
    auto prev_it = it--;
    EXPECT_EQ(prev_it, std::next(it));
    EXPECT_EQ(*it, expected);
  }
  EXPECT_EQ(it, q.begin());

  // Test `rbegin()` / `rend()` and `crbegin()` / `crend()`.
  std::vector<int64_t> rev(q.rbegin(), q.rend());
  EXPECT_THAT(rev, ElementsAre(9, 8, 7, 6, 5, 4, 3, 2, 1, 0));

  const auto& cq = q;
  std::vector<int64_t> crev(cq.crbegin(), cq.crend());
  EXPECT_THAT(crev, ElementsAre(9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
}

struct MyType {
  static int constructor_calls;
  static int destructor_calls;

  explicit MyType(int x) : val(x) { constructor_calls++; }
  MyType(const MyType& t) : val(t.val) { constructor_calls++; }
  ~MyType() { destructor_calls++; }

  int val;
};

int MyType::constructor_calls = 0;
int MyType::destructor_calls = 0;

TEST(ChunkedQueue, ConstructorDestructorCalls) {
  for (int i = 0; i < 100; i++) {
    std::vector<MyType> vals;
    for (int j = 0; j < i; j++) {
      vals.push_back(MyType(j));
    }
    MyType::constructor_calls = 0;
    MyType::destructor_calls = 0;
    {
      absl::chunked_queue<MyType> q;
      for (int j = 0; j < i; j++) {
        q.push_back(vals[j]);
      }
      if (i % 10 == 0) {
        q.clear();
      } else if (i % 2 == 0) {
        for (int j = 0; j < i; j++) {
          EXPECT_EQ(q.front().val, j);
          q.pop_front();
        }
      }
    }
    EXPECT_EQ(MyType::constructor_calls, i);
    EXPECT_EQ(MyType::destructor_calls, i);
  }
}

TEST(ChunkedQueue, ConstructorDestructorCallsPopBack) {
  for (int i = 0; i < 100; i++) {
    std::vector<MyType> vals;
    for (int j = 0; j < i; j++) {
      vals.push_back(MyType(j));
    }
    MyType::constructor_calls = 0;
    MyType::destructor_calls = 0;
    {
      absl::chunked_queue<MyType> q;
      for (int j = 0; j < i; j++) {
        q.push_back(vals[j]);
      }
      for (int j = i - 1; j >= 0; j--) {
        EXPECT_EQ(q.back().val, j);
        q.pop_back();
      }
    }
    EXPECT_EQ(MyType::constructor_calls, i);
    EXPECT_EQ(MyType::destructor_calls, i);
  }
}

TEST(ChunkedQueue, MoveObjects) {
  absl::chunked_queue<std::unique_ptr<int>> q;
  q.push_back(std::make_unique<int>(10));
  q.push_back(std::make_unique<int>(11));
  q.push_front(std::make_unique<int>(9));

  EXPECT_EQ(9, *q.front());
  q.pop_front();
  EXPECT_EQ(10, *q.front());
  q.pop_front();
  EXPECT_EQ(11, *q.front());
  q.pop_front();

  q.push_back(std::make_unique<int>(10));
  q.push_back(std::make_unique<int>(11));
  q.push_front(std::make_unique<int>(9));

  EXPECT_EQ(11, *q.back());
  q.pop_back();
  EXPECT_EQ(10, *q.back());
  q.pop_back();
  EXPECT_EQ(9, *q.back());
  q.pop_back();
}

// Verifies that `emplace_front` constructs elements in-place at the front of
// the queue, returns a reference to the newly inserted element, and preserves
// existing elements at the back.
TEST(ChunkedQueue, EmplaceFrontUpdatesFrontAndReturnsReference) {
  absl::chunked_queue<std::pair<int, int>> q;
  auto& v1 = q.emplace_front(1, 2);
  EXPECT_THAT(v1, Pair(1, 2));
  EXPECT_THAT(q.front(), Pair(1, 2));
  EXPECT_EQ(&v1, &q.front());
  EXPECT_EQ(&v1, &q.back());

  auto& v2 = q.emplace_front(3, 4);
  EXPECT_THAT(v2, Pair(3, 4));
  EXPECT_THAT(q.front(), Pair(3, 4));
  EXPECT_THAT(q.back(), Pair(1, 2));
  EXPECT_EQ(&v2, &q.front());
  EXPECT_EQ(&v1, &q.back());
}

// Verifies that `emplace_front` forwards move-only arguments to construct the
// element in-place.
TEST(ChunkedQueue, EmplaceFrontMoveOnlyType) {
  absl::chunked_queue<std::pair<std::unique_ptr<int>, std::string>> q;
  auto& v = q.emplace_front(std::make_unique<int>(11), "val12");
  EXPECT_THAT(v, Pair(Pointee(11), "val12"));
  EXPECT_THAT(q.front(), Pair(Pointee(11), "val12"));
}

TEST(ChunkedQueue, OveralignmentEmplaceFront) {
  struct alignas(64) Overaligned {
    int x;
    int y;
  };
  absl::chunked_queue<Overaligned, 1, 8> q;
  for (int i = 0; i < 10; ++i) {
    auto& v = q.emplace_front(Overaligned{i, i});
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&v) % 64, 0);
  }
}

TEST(ChunkedQueue, EmplaceBack1) {
  absl::chunked_queue<std::pair<int, int>> q;
  auto& v = q.emplace_back(1, 2);
  EXPECT_THAT(v, Pair(1, 2));
  EXPECT_THAT(q.front(), Pair(1, 2));
  EXPECT_EQ(&v, &q.back());
}

TEST(ChunkedQueue, EmplaceBack2) {
  absl::chunked_queue<std::pair<std::unique_ptr<int>, std::string>> q;
  auto& v = q.emplace_back(std::make_unique<int>(11), "val12");
  EXPECT_THAT(v, Pair(Pointee(11), "val12"));
  EXPECT_THAT(q.front(), Pair(Pointee(11), "val12"));
}

TEST(ChunkedQueue, OveralignmentEmplaceBack) {
  struct alignas(64) Overaligned {
    int x;
    int y;
  };
  absl::chunked_queue<Overaligned, 1, 8> q;
  for (int i = 0; i < 10; ++i) {
    auto& v = q.emplace_back(Overaligned{i, i});
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&v) % 64, 0);
  }
}

TEST(ChunkedQueue, StatelessAllocatorDoesntAffectObjectSizes) {
  // When a stateless allocator type is used -- such as when no explicit
  // allocator type is given, and the stateless default is used -- it does not
  // increase the object sizes from what they used to be before allocator
  // support was added.  (In practice this verifies that allocator support makes
  // use of the empty base-class optimization.)
  //
  // These "Mock*" structs model the data members of absl::chunked_queue<> and
  // its internal ChunkedQueueBlock<> type, without any extra storage for
  // allocator state.  (We use these to generate expected stateless-allocator
  // object sizes in a portable way.)
  struct MockQueue {
    struct MockIterator {
      void* block;
      void* ptr;
    };
    MockIterator head;
    MockIterator tail;
    size_t size;
  };
  struct MockBlock {
    void* next;
    void* prev;
    void* limit;
  };
  using TestQueueType = absl::chunked_queue<int64_t, 1, 16>;
  EXPECT_EQ(sizeof(TestQueueType), sizeof(MockQueue));
  EXPECT_EQ(sizeof(absl::container_internal::ChunkedQueueBlock<
                   TestQueueType::value_type, TestQueueType::allocator_type>),
            sizeof(MockBlock));
}

TEST(ChunkedQueue, DoesNotRoundBlockSizesUpWithNonDefaultAllocator) {
  using OneByte = uint8_t;
  using CustomAllocator = absl::container_internal::CountingAllocator<OneByte>;
  using Block =
      absl::container_internal::ChunkedQueueBlock<OneByte, CustomAllocator>;
  int64_t allocator_live_bytes = 0;
  CustomAllocator allocator(&allocator_live_bytes);
  // Create a Block big enough to accomodate at least 1 OneByte.
  Block* b = Block::New(1, &allocator);
  ASSERT_TRUE(b != nullptr);
  // With a non-default allocator in play, the resulting block should have
  // capacity for exactly 1 element -- the implementation should not round the
  // allocation size up, which may be inappropriate for non-default allocators.
  //
  // (Note that we don't always round up even with the default allocator in use,
  // e.g. when compiling for ASAN analysis.)
  EXPECT_EQ(b->size(), 1);
  Block::Delete(b, &allocator);
}

TEST(ChunkedQueue, Hardening) {
  bool hardened = false;
  ABSL_HARDENING_ASSERT([&hardened]() {
    hardened = true;
    return true;
  }());
  if (!hardened) {
    GTEST_SKIP() << "Not a hardened build";
  }

  absl::chunked_queue<int> q;
  EXPECT_DEATH_IF_SUPPORTED(q.front(), "");
  EXPECT_DEATH_IF_SUPPORTED(q.back(), "");
  EXPECT_DEATH_IF_SUPPORTED(q.pop_front(), "");
  EXPECT_DEATH_IF_SUPPORTED(q.pop_back(), "");

  const absl::chunked_queue<int> cq;
  EXPECT_DEATH_IF_SUPPORTED(cq.front(), "");
  EXPECT_DEATH_IF_SUPPORTED(cq.back(), "");
}

#ifdef ABSL_HAVE_EXCEPTIONS
struct ThrowingCtor {
  int* ctor_count;
  int* dtor_count;
  int value;

  explicit ThrowingCtor(int v, bool should_throw, int* c_count, int* d_count)
      : ctor_count(c_count), dtor_count(d_count), value(v) {
    if (should_throw) {
      throw 0;
    }
    ++*ctor_count;
  }
  ThrowingCtor(const ThrowingCtor& other)
      : ctor_count(other.ctor_count),
        dtor_count(other.dtor_count),
        value(other.value) {
    ++*ctor_count;
  }
  ThrowingCtor(ThrowingCtor&& other) noexcept
      : ctor_count(other.ctor_count),
        dtor_count(other.dtor_count),
        value(other.value) {
    ++*ctor_count;
  }
  ~ThrowingCtor() { ++*dtor_count; }
};

TEST(ChunkedQueue, StrongExceptionSafetyEmplaceBack) {
  int ctor_count = 0;
  int dtor_count = 0;
  absl::chunked_queue<ThrowingCtor> q;
  q.emplace_back(10, false, &ctor_count, &dtor_count);
  q.emplace_back(20, false, &ctor_count, &dtor_count);
  ASSERT_EQ(q.size(), 2);
  ASSERT_EQ(ctor_count, 2);
  ASSERT_EQ(dtor_count, 0);

  try {
    q.emplace_back(30, true, &ctor_count, &dtor_count);
  } catch (...) {
  }

  EXPECT_EQ(q.size(), 2);
  EXPECT_EQ(ctor_count, 2);
  EXPECT_EQ(dtor_count, 0);

  q.clear();
  EXPECT_EQ(q.size(), 0);
  EXPECT_EQ(dtor_count, 2);
}
#endif

TEST(ChunkedQueue, StrongExceptionSafetyEmplaceFrontEmptyQueue) {
#ifndef ABSL_HAVE_EXCEPTIONS
  GTEST_SKIP() << "Exceptions are not enabled.";
#else
  int ctor_count = 0;
  int dtor_count = 0;
  absl::chunked_queue<ThrowingCtor> q;

  try {
    q.emplace_front(5, true, &ctor_count, &dtor_count);
  } catch (...) {
  }

  EXPECT_TRUE(q.empty());
  EXPECT_EQ(ctor_count, 0);
  EXPECT_EQ(dtor_count, 0);
#endif
}

TEST(ChunkedQueue, StrongExceptionSafetyEmplaceFrontNewHeadBlock) {
#ifndef ABSL_HAVE_EXCEPTIONS
  GTEST_SKIP() << "Exceptions are not enabled.";
#else
  int ctor_count = 0;
  int dtor_count = 0;
  absl::chunked_queue<ThrowingCtor> q;
  q.emplace_front(10, false, &ctor_count, &dtor_count);
  ASSERT_EQ(q.size(), 1);
  ASSERT_EQ(ctor_count, 1);
  ASSERT_EQ(dtor_count, 0);

  // The first block has size 1, so the next emplace_front allocates a new head
  // block and must delete it if element construction throws.
  try {
    q.emplace_front(15, true, &ctor_count, &dtor_count);
  } catch (...) {
  }

  EXPECT_EQ(q.size(), 1);
  EXPECT_EQ(ctor_count, 1);
  EXPECT_EQ(dtor_count, 0);
  EXPECT_EQ(q.front().value, 10);
  EXPECT_EQ(q.back().value, 10);

  q.clear();
  EXPECT_EQ(q.size(), 0);
  EXPECT_EQ(dtor_count, 1);
#endif
}

TEST(ChunkedQueue, StrongExceptionSafetyEmplaceFrontExistingHeadBlock) {
#ifndef ABSL_HAVE_EXCEPTIONS
  GTEST_SKIP() << "Exceptions are not enabled.";
#else
  int ctor_count = 0;
  int dtor_count = 0;
  absl::chunked_queue<ThrowingCtor, 2> q;
  q.emplace_front(10, false, &ctor_count, &dtor_count);
  ASSERT_EQ(q.size(), 1);
  ASSERT_EQ(ctor_count, 1);
  ASSERT_EQ(dtor_count, 0);

  // The head block has capacity 2 with 1 element, so the next emplace_front
  // constructs into the remaining slot in the existing head block.
  try {
    q.emplace_front(20, true, &ctor_count, &dtor_count);
  } catch (...) {
  }

  EXPECT_EQ(q.size(), 1);
  EXPECT_EQ(ctor_count, 1);
  EXPECT_EQ(dtor_count, 0);
  EXPECT_EQ(q.front().value, 10);
  EXPECT_EQ(q.back().value, 10);

  q.clear();
  EXPECT_EQ(q.size(), 0);
  EXPECT_EQ(dtor_count, 1);
#endif
}

TEST(ChunkedQueue, StrongExceptionSafetyEmplaceFrontClearedQueue) {
#ifndef ABSL_HAVE_EXCEPTIONS
  GTEST_SKIP() << "Exceptions are not enabled.";
#else
  int ctor_count = 0;
  int dtor_count = 0;
  absl::chunked_queue<ThrowingCtor> q;
  q.emplace_front(10, false, &ctor_count, &dtor_count);
  q.clear();
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(ctor_count, 1);
  ASSERT_EQ(dtor_count, 1);

  // `clear()` leaves one empty block allocated; throwing while constructing
  // into that retained block must leave the queue empty and valid.
  try {
    q.emplace_front(20, true, &ctor_count, &dtor_count);
  } catch (...) {
  }

  EXPECT_TRUE(q.empty());
  EXPECT_EQ(ctor_count, 1);
  EXPECT_EQ(dtor_count, 1);
#endif
}

}  // namespace
