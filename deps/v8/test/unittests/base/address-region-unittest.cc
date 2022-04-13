// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/address-region.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

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

}  // namespace base
}  // namespace v8
