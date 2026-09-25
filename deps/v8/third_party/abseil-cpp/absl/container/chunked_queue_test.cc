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
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/macros.h"
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

TEST(ChunkedQueue, IteratorDefaultConstructor) {
  using ConstIter = absl::chunked_queue<int64_t>::const_iterator;
  using Iter = absl::chunked_queue<int64_t>::iterator;
  ConstIter const_iter;
  EXPECT_TRUE(const_iter == ConstIter());
  Iter iter;
  EXPECT_TRUE(iter == Iter());
}

TEST(ChunkedQueue, IteratorConversion) {
  using ConstIter = absl::chunked_queue<int64_t>::const_iterator;
  using Iter = absl::chunked_queue<int64_t>::iterator;
  EXPECT_FALSE((std::is_convertible<ConstIter, Iter>::value));
  EXPECT_TRUE((std::is_convertible<Iter, ConstIter>::value));
  absl::chunked_queue<int64_t> q;
  ConstIter it1 = q.begin();
  ConstIter it2 = q.cbegin();
  Iter it3 = q.begin();
  it1 = q.end();
  it2 = q.cend();
  it3 = q.end();
  EXPECT_FALSE((std::is_assignable<Iter, ConstIter>::value));
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
      } else {
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

TEST(ChunkedQueue, MoveObjects) {
  absl::chunked_queue<std::unique_ptr<int>> q;
  q.push_back(std::make_unique<int>(10));
  q.push_back(std::make_unique<int>(11));

  EXPECT_EQ(10, *q.front());
  q.pop_front();
  EXPECT_EQ(11, *q.front());
  q.pop_front();
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
      void* limit;
    };
    MockIterator head;
    MockIterator tail;
    size_t size;
  };
  struct MockBlock {
    void* next;
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

  const absl::chunked_queue<int> cq;
  EXPECT_DEATH_IF_SUPPORTED(cq.front(), "");
  EXPECT_DEATH_IF_SUPPORTED(cq.back(), "");
}

}  // namespace
