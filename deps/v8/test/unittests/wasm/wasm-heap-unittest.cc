// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "src/wasm/wasm-heap.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace wasm_heap_unittest {

class DisjointAllocationPoolTest : public ::testing::Test {
 public:
  Address A(size_t n) { return reinterpret_cast<Address>(n); }
  void CheckLooksLike(const DisjointAllocationPool& mem,
                      std::vector<std::pair<size_t, size_t>> expectation);
  DisjointAllocationPool Make(std::vector<std::pair<size_t, size_t>> model);
};

void DisjointAllocationPoolTest::CheckLooksLike(
    const DisjointAllocationPool& mem,
    std::vector<std::pair<size_t, size_t>> expectation) {
  const auto& ranges = mem.ranges();
  CHECK_EQ(ranges.size(), expectation.size());
  auto iter = expectation.begin();
  for (auto it = ranges.begin(), e = ranges.end(); it != e; ++it, ++iter) {
    CHECK_EQ(it->first, A(iter->first));
    CHECK_EQ(it->second, A(iter->second));
  }
}

DisjointAllocationPool DisjointAllocationPoolTest::Make(
    std::vector<std::pair<size_t, size_t>> model) {
  DisjointAllocationPool ret;
  for (auto& pair : model) {
    ret.Merge(DisjointAllocationPool(A(pair.first), A(pair.second)));
  }
  return ret;
}

TEST_F(DisjointAllocationPoolTest, Construct) {
  DisjointAllocationPool a;
  CHECK(a.IsEmpty());
  CHECK_EQ(a.ranges().size(), 0);
  DisjointAllocationPool b = Make({{1, 5}});
  CHECK(!b.IsEmpty());
  CHECK_EQ(b.ranges().size(), 1);
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}});
  DisjointAllocationPool c;
  a.Merge(std::move(c));
  CheckLooksLike(a, {{1, 5}});
  DisjointAllocationPool e, f;
  e.Merge(std::move(f));
  CHECK(e.IsEmpty());
}

TEST_F(DisjointAllocationPoolTest, SimpleExtract) {
  DisjointAllocationPool a = Make({{1, 5}});
  DisjointAllocationPool b = a.AllocatePool(2);
  CheckLooksLike(a, {{3, 5}});
  CheckLooksLike(b, {{1, 3}});
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}});
  CHECK_EQ(a.ranges().size(), 1);
  CHECK_EQ(a.ranges().front().first, A(1));
  CHECK_EQ(a.ranges().front().second, A(5));
}

TEST_F(DisjointAllocationPoolTest, ExtractAll) {
  DisjointAllocationPool a(A(1), A(5));
  DisjointAllocationPool b = a.AllocatePool(4);
  CheckLooksLike(b, {{1, 5}});
  CHECK(a.IsEmpty());
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}});
}

TEST_F(DisjointAllocationPoolTest, ExtractAccross) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 20}});
  DisjointAllocationPool b = a.AllocatePool(5);
  CheckLooksLike(a, {{11, 20}});
  CheckLooksLike(b, {{1, 5}, {10, 11}});
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}, {10, 20}});
}

TEST_F(DisjointAllocationPoolTest, ReassembleOutOfOrder) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 15}});
  DisjointAllocationPool b = Make({{7, 8}, {20, 22}});
  a.Merge(std::move(b));
  CheckLooksLike(a, {{1, 5}, {7, 8}, {10, 15}, {20, 22}});

  DisjointAllocationPool c = Make({{1, 5}, {10, 15}});
  DisjointAllocationPool d = Make({{7, 8}, {20, 22}});
  d.Merge(std::move(c));
  CheckLooksLike(d, {{1, 5}, {7, 8}, {10, 15}, {20, 22}});
}

TEST_F(DisjointAllocationPoolTest, FailToExtract) {
  DisjointAllocationPool a = Make({{1, 5}});
  DisjointAllocationPool b = a.AllocatePool(5);
  CheckLooksLike(a, {{1, 5}});
  CHECK(b.IsEmpty());
}

TEST_F(DisjointAllocationPoolTest, FailToExtractExact) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 14}});
  DisjointAllocationPool b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}, {10, 14}});
  CHECK(b.IsEmpty());
}

TEST_F(DisjointAllocationPoolTest, ExtractExact) {
  DisjointAllocationPool a = Make({{1, 5}, {10, 15}});
  DisjointAllocationPool b = a.Allocate(5);
  CheckLooksLike(a, {{1, 5}});
  CheckLooksLike(b, {{10, 15}});
}

TEST_F(DisjointAllocationPoolTest, Merging) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}});
  a.Merge(Make({{15, 20}}));
  CheckLooksLike(a, {{10, 25}});
}

TEST_F(DisjointAllocationPoolTest, MergingMore) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{15, 20}, {25, 30}}));
  CheckLooksLike(a, {{10, 35}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkip) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{25, 30}}));
  CheckLooksLike(a, {{10, 15}, {20, 35}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrc) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{25, 30}, {35, 40}}));
  CheckLooksLike(a, {{10, 15}, {20, 40}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrcWithGap) {
  DisjointAllocationPool a = Make({{10, 15}, {20, 25}, {30, 35}});
  a.Merge(Make({{25, 30}, {36, 40}}));
  CheckLooksLike(a, {{10, 15}, {20, 35}, {36, 40}});
}

}  // namespace wasm_heap_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
