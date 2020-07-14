// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace wasm_heap_unittest {

class DisjointAllocationPoolTest : public ::testing::Test {
 public:
  void CheckPool(const DisjointAllocationPool& mem,
                 std::initializer_list<base::AddressRegion> expected_regions);
  void CheckRange(base::AddressRegion region1, base::AddressRegion region2);
  DisjointAllocationPool Make(
      std::initializer_list<base::AddressRegion> regions);
};

void DisjointAllocationPoolTest::CheckPool(
    const DisjointAllocationPool& mem,
    std::initializer_list<base::AddressRegion> expected_regions) {
  const auto& regions = mem.regions();
  CHECK_EQ(regions.size(), expected_regions.size());
  auto iter = expected_regions.begin();
  for (auto it = regions.begin(), e = regions.end(); it != e; ++it, ++iter) {
    CHECK_EQ(*it, *iter);
  }
}

void DisjointAllocationPoolTest::CheckRange(base::AddressRegion region1,
                                            base::AddressRegion region2) {
  CHECK_EQ(region1, region2);
}

DisjointAllocationPool DisjointAllocationPoolTest::Make(
    std::initializer_list<base::AddressRegion> regions) {
  DisjointAllocationPool ret;
  for (auto& region : regions) {
    ret.Merge(region);
  }
  return ret;
}

TEST_F(DisjointAllocationPoolTest, ConstructEmpty) {
  DisjointAllocationPool a;
  CHECK(a.IsEmpty());
  CheckPool(a, {});
  a.Merge({1, 4});
  CheckPool(a, {{1, 4}});
}

TEST_F(DisjointAllocationPoolTest, ConstructWithRange) {
  DisjointAllocationPool a({1, 4});
  CHECK(!a.IsEmpty());
  CheckPool(a, {{1, 4}});
}

TEST_F(DisjointAllocationPoolTest, SimpleExtract) {
  DisjointAllocationPool a = Make({{1, 4}});
  base::AddressRegion b = a.Allocate(2);
  CheckPool(a, {{3, 2}});
  CheckRange(b, {1, 2});
  a.Merge(b);
  CheckPool(a, {{1, 4}});
  CHECK_EQ(a.regions().size(), 1);
  CHECK_EQ(a.regions().begin()->begin(), 1);
  CHECK_EQ(a.regions().begin()->end(), 5);
}

TEST_F(DisjointAllocationPoolTest, ExtractAll) {
  DisjointAllocationPool a({1, 4});
  base::AddressRegion b = a.Allocate(4);
  CheckRange(b, {1, 4});
  CHECK(a.IsEmpty());
  a.Merge(b);
  CheckPool(a, {{1, 4}});
}

TEST_F(DisjointAllocationPoolTest, FailToExtract) {
  DisjointAllocationPool a = Make({{1, 4}});
  base::AddressRegion b = a.Allocate(5);
  CheckPool(a, {{1, 4}});
  CHECK(b.is_empty());
}

TEST_F(DisjointAllocationPoolTest, FailToExtractExact) {
  DisjointAllocationPool a = Make({{1, 4}, {10, 4}});
  base::AddressRegion b = a.Allocate(5);
  CheckPool(a, {{1, 4}, {10, 4}});
  CHECK(b.is_empty());
}

TEST_F(DisjointAllocationPoolTest, ExtractExact) {
  DisjointAllocationPool a = Make({{1, 4}, {10, 5}});
  base::AddressRegion b = a.Allocate(5);
  CheckPool(a, {{1, 4}});
  CheckRange(b, {10, 5});
}

TEST_F(DisjointAllocationPoolTest, Merging) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}});
  a.Merge({15, 5});
  CheckPool(a, {{10, 15}});
}

TEST_F(DisjointAllocationPoolTest, MergingFirst) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}});
  a.Merge({5, 5});
  CheckPool(a, {{5, 10}, {20, 5}});
}

TEST_F(DisjointAllocationPoolTest, MergingAbove) {
  DisjointAllocationPool a = Make({{10, 5}, {25, 5}});
  a.Merge({20, 5});
  CheckPool(a, {{10, 5}, {20, 10}});
}

TEST_F(DisjointAllocationPoolTest, MergingMore) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({15, 5});
  a.Merge({25, 5});
  CheckPool(a, {{10, 25}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkip) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({25, 5});
  CheckPool(a, {{10, 5}, {20, 15}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrc) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({25, 5});
  a.Merge({35, 5});
  CheckPool(a, {{10, 5}, {20, 20}});
}

TEST_F(DisjointAllocationPoolTest, MergingSkipLargerSrcWithGap) {
  DisjointAllocationPool a = Make({{10, 5}, {20, 5}, {30, 5}});
  a.Merge({25, 5});
  a.Merge({36, 4});
  CheckPool(a, {{10, 5}, {20, 15}, {36, 4}});
}

}  // namespace wasm_heap_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8
