// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/memory-access-information.h"

#ifdef V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT
#include <sys/user.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

TEST(MemoryAccessInformationTest, ParseInstructionWidthAndExtension) {
  struct user_regs_struct regs = {};

  // Test 8-bit zero-extending read.
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("movzxbl eax,[rbx]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(&regs.rax, info.result_reg);
    EXPECT_EQ(1, info.access_width);
    EXPECT_EQ(MemoryAccessInformation::kZeroExtend, info.extension);
  }

  // Test 16-bit sign-extending read.
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("movsxw rdx,[rsi]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(&regs.rdx, info.result_reg);
    EXPECT_EQ(2, info.access_width);
    EXPECT_EQ(MemoryAccessInformation::kSignExtend, info.extension);
  }

  // Test 32-bit zero-extending read (standard 32-bit register destination).
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("movl rax,[rax+0x8]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(&regs.rax, info.result_reg);
    EXPECT_EQ(4, info.access_width);
    EXPECT_EQ(MemoryAccessInformation::kZeroExtend, info.extension);
  }

  // Test 32-bit sign-extending read to 64-bit register.
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("movsxlq rax,[rbx]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(&regs.rax, info.result_reg);
    EXPECT_EQ(4, info.access_width);
    EXPECT_EQ(MemoryAccessInformation::kSignExtend, info.extension);
  }

  // Test sub-register alias matching in bitwise read operation.
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("orb cl,[rbx]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(&regs.rcx, info.result_reg);
    EXPECT_EQ(1, info.access_width);
    EXPECT_EQ(MemoryAccessInformation::kNoExtend, info.extension);
  }

  // Test memory write instruction.
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("movq [rbx],rax", regs);
    EXPECT_EQ(MemoryAccessInformation::kWrite, info.kind);
    EXPECT_EQ(nullptr, info.result_reg);
    EXPECT_EQ(8, info.access_width);
  }

  // Test compare instruction reading memory.
  {
    MemoryAccessInformation info = ParseMemoryAccessInformationFromInstruction(
        "cmpl [rax+0x7],0x2000000", regs);
    EXPECT_EQ(MemoryAccessInformation::kCmp, info.kind);
    EXPECT_EQ(4, info.access_width);
  }

  // Test vector (YMM) register read.
  {
    MemoryAccessInformation info =
        ParseMemoryAccessInformationFromInstruction("vmovdqu ymm1,[rsi]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(1, info.xmm_reg_index);
  }

  // Test 3-operand SIMD instruction reading memory.
  {
    MemoryAccessInformation info = ParseMemoryAccessInformationFromInstruction(
        "vpcmpeqb ymm1,ymm0,[rdi]", regs);
    EXPECT_EQ(MemoryAccessInformation::kRead, info.kind);
    EXPECT_EQ(1, info.xmm_reg_index);
    EXPECT_EQ(1, info.access_width);
  }
}

}  // namespace
}  // namespace v8
#endif  // V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT
