// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/sys-info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if V8_OS_NACL
#define DISABLE_ON_NACL(Name) DISABLED_##Name
#else
#define DISABLE_ON_NACL(Name) Name
#endif

namespace v8 {
namespace base {

TEST(SysInfoTest, NumberOfProcessors) {
  EXPECT_LT(0, SysInfo::NumberOfProcessors());
}


TEST(SysInfoTest, DISABLE_ON_NACL(AmountOfPhysicalMemory)) {
  EXPECT_LT(0, SysInfo::AmountOfPhysicalMemory());
}


TEST(SysInfoTest, AmountOfVirtualMemory) {
  EXPECT_LE(0, SysInfo::AmountOfVirtualMemory());
}

}  // namespace base
}  // namespace v8
