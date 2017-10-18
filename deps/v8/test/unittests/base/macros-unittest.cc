// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(AlignedAddressTest, AlignedAddress) {
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFF0), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFF2), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFF2), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0xFFFF0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFFF), 16));
  EXPECT_EQ(reinterpret_cast<void*>(0x0),
            AlignedAddress(reinterpret_cast<void*>(0xFFFFF), 0x100000));
}

}  // namespace base
}  // namespace v8
