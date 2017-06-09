// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone.h"

#include "src/zone/accounting-allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(Zone, 8ByteAlignment) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  for (size_t i = 0; i < 16; ++i) {
    ASSERT_EQ(reinterpret_cast<intptr_t>(zone.New(i)) % 8, 0);
  }
}

}  // namespace internal
}  // namespace v8
