// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/address-region.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8::base {

using Address = AddressRegion::Address;

TEST(AddressRegionTest, Contains) {
  struct {
    Address start;
    size_t size;
  } test_cases[] = {{153, 771}, {0, 227}, {static_cast<Address>(-447), 447}};

  for (size_t i = 0; i < arraysize(test_cases); i++) {
    Address start = test_cases[i].start;
    size_t size = test_cases[i].size;
    Address end = start + size;  // exclusive

    AddressRegion region(start, size);

    // Test single-argument contains().
    CHECK(!region.contains(start - 1041));
    CHECK(!region.contains(start - 1));
    CHECK(!region.contains(end));
    CHECK(!region.contains(end + 1));
    CHECK(!region.contains(end + 113));

    CHECK(region.contains(start));
    CHECK(region.contains(start + 1));
    CHECK(region.contains(start + size / 2));
    CHECK(region.contains(end - 1));

    // Test two-arguments contains().
    CHECK(!region.contains(start - 1, size));
    CHECK(!region.contains(start, size + 1));
    CHECK(!region.contains(start - 17, 17));
    CHECK(!region.contains(start - 17, size * 2));
    CHECK(!region.contains(end, 1));
    CHECK(!region.contains(end, static_cast<size_t>(0 - end)));

    CHECK(region.contains(start, size));
    CHECK(region.contains(start, 10));
    CHECK(region.contains(start + 11, 120));
    CHECK(region.contains(end - 13, 13));
    CHECK(!region.contains(end, 0));

    // Zero-size queries.
    CHECK(!region.contains(start - 10, 0));
    CHECK(!region.contains(start - 1, 0));
    CHECK(!region.contains(end, 0));
    CHECK(!region.contains(end + 10, 0));

    CHECK(region.contains(start, 0));
    CHECK(region.contains(start + 10, 0));
    CHECK(region.contains(end - 1, 0));
  }
}

TEST(AddressRegionTest, GetOverlap) {
  constexpr AddressRegion region(100, 20);
  // No overlap.
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(0, 20)).size());
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(120, 20)).size());
  // Adjacent.
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(80, 20)).size());
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(120, 20)).size());
  // Simple overlap.
  EXPECT_EQ(AddressRegion(100, 10), region.GetOverlap(AddressRegion(90, 20)));
  EXPECT_EQ(AddressRegion(110, 10), region.GetOverlap(AddressRegion(110, 20)));
  // Full overlap.
  EXPECT_EQ(region, region.GetOverlap(AddressRegion(90, 40)));
  // Identical.
  EXPECT_EQ(region, region.GetOverlap(region));
  // Zero-sized regions.
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(0, 0)).size());
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(100, 0)).size());
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(110, 0)).size());
  EXPECT_EQ(0u, region.GetOverlap(AddressRegion(120, 0)).size());
}

TEST(AddressRegionTest, Constexpr) {
  static_assert(AddressRegion(0, 20) == AddressRegion(0, 20));
  static_assert(AddressRegion(0, 20).size() == 20);
  static_assert(AddressRegion(0, 20).GetOverlap(AddressRegion(20, 20)).size() ==
                0);
}

}  // namespace v8::base
