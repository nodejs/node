// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/cpu.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(CPUTest, FeatureImplications) {
  CPU cpu;

  // ia32 and x64 features
  EXPECT_TRUE(!cpu.has_sse() || cpu.has_mmx());
  EXPECT_TRUE(!cpu.has_sse2() || cpu.has_sse());
  EXPECT_TRUE(!cpu.has_sse3() || cpu.has_sse2());
  EXPECT_TRUE(!cpu.has_ssse3() || cpu.has_sse3());
  EXPECT_TRUE(!cpu.has_sse41() || cpu.has_sse3());
  EXPECT_TRUE(!cpu.has_sse42() || cpu.has_sse41());

  // arm features
  EXPECT_TRUE(!cpu.has_vfp3_d32() || cpu.has_vfp3());
}


TEST(CPUTest, RequiredFeatures) {
  CPU cpu;

#if V8_HOST_ARCH_ARM
  EXPECT_TRUE(cpu.has_fpu());
#endif

#if V8_HOST_ARCH_IA32
  EXPECT_TRUE(cpu.has_fpu());
  EXPECT_TRUE(cpu.has_sahf());
#endif

#if V8_HOST_ARCH_X64
  EXPECT_TRUE(cpu.has_fpu());
  EXPECT_TRUE(cpu.has_cmov());
  EXPECT_TRUE(cpu.has_mmx());
  EXPECT_TRUE(cpu.has_sse());
  EXPECT_TRUE(cpu.has_sse2());
#endif
}

}  // namespace base
}  // namespace v8
