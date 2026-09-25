// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone.h"

#include "src/zone/accounting-allocator.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class ZoneTest : public TestWithPlatform {};

// This struct is just a type tag for Zone::Allocate<T>(size_t) call.
struct ZoneTestTag {};

TEST_F(ZoneTest, 8ByteAlignment) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  for (size_t i = 0; i < 16; ++i) {
    ASSERT_EQ(reinterpret_cast<intptr_t>(zone.Allocate<ZoneTestTag>(i)) % 8, 0);
  }
}

}  // namespace internal
}  // namespace v8
