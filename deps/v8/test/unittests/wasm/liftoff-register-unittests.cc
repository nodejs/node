// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-assembler-defs.h"
#if V8_TARGET_ARCH_X64
#include "src/codegen/safepoint-table.h"
#include "src/execution/x64/frame-constants-x64.h"
#include "src/wasm/baseline/liftoff-assembler-inl.h"
#include "test/common/assembler-tester.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"
#elif V8_TARGET_ARCH_IA32
#include "src/execution/ia32/frame-constants-ia32.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/execution/mips64/frame-constants-mips64.h"
#elif V8_TARGET_ARCH_LOONG64
#include "src/execution/loong64/frame-constants-loong64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/execution/arm/frame-constants-arm.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/execution/arm64/frame-constants-arm64.h"
#elif V8_TARGET_ARCH_S390X
#include "src/execution/s390/frame-constants-s390.h"
#elif V8_TARGET_ARCH_PPC64
#include "src/execution/ppc/frame-constants-ppc.h"
#elif V8_TARGET_ARCH_RISCV32 || V8_TARGET_ARCH_RISCV64
#include "src/execution/riscv/frame-constants-riscv.h"
#endif

#include "src/wasm/baseline/liftoff-register.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace wasm {

// The registers used by Liftoff and the registers spilled by the
// WasmDebugBreak builtin should match.
static_assert(kLiftoffAssemblerGpCacheRegs ==
              WasmDebugBreakFrameConstants::kPushedGpRegs);

static_assert(kLiftoffAssemblerFpCacheRegs ==
              WasmDebugBreakFrameConstants::kPushedFpRegs);

class WasmRegisterTest : public ::testing::Test {};

TEST_F(WasmRegisterTest, SpreadSetBitsToAdjacentFpRegs) {
  LiftoffRegList input(
  // GP reg selection criteria: an even and an odd register belonging to
  // separate adjacent pairs, and contained in kLiftoffAssemblerGpCacheRegs
  // for the given platform.
#if V8_TARGET_ARCH_S390X || V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_LOONG64
      LiftoffRegister::from_code(kGpReg, 4),
      LiftoffRegister::from_code(kGpReg, 7),
#elif V8_TARGET_ARCH_RISCV32 || V8_TARGET_ARCH_RISCV64
      LiftoffRegister::from_code(kGpReg, 10),
      LiftoffRegister::from_code(kGpReg, 13),
#else
      LiftoffRegister::from_code(kGpReg, 1),
      LiftoffRegister::from_code(kGpReg, 2),
#endif
      LiftoffRegister::from_code(kFpReg, 1),
      LiftoffRegister::from_code(kFpReg, 4));
  // GP regs are left alone, FP regs are spread to adjacent pairs starting
  // at an even index: 1 → (0, 1) and 4 → (4, 5).
#if V8_TARGET_ARCH_RISCV32 || V8_TARGET_ARCH_RISCV64 || V8_TARGET_ARCH_IA32 || \
    V8_TARGET_ARCH_PPC64
  // These platforms don't have code 0 in kLiftoffAssemblerFpCacheRegs
  LiftoffRegList expected =
      input | LiftoffRegList(LiftoffRegister::from_code(kFpReg, 5));
#else
  LiftoffRegList expected =
      input | LiftoffRegList(LiftoffRegister::from_code(kFpReg, 0),
                             LiftoffRegister::from_code(kFpReg, 5));
#endif
  LiftoffRegList actual = input.SpreadSetBitsToAdjacentFpRegs();
  EXPECT_EQ(expected, actual);
}

#if V8_TARGET_ARCH_X64
using LiftoffFrameAlignmentTest = TestWithIsolateAndZone;

TEST_F(LiftoffFrameAlignmentTest, AlignFrameSizeAndPatchPrepareStackFrame) {
  auto check_frame_alignment = [&](bool feedback_vector_slot,
                                   int used_spill_offset) {
    auto buffer = AllocateAssemblerBuffer();
    LiftoffAssembler assm(zone(), buffer->CreateView());
    assm.set_root_array_available(false);
    SafepointTableBuilder safepoint_table_builder(zone());

    // Set up the fixed header slots below rbp the same way Liftoff does:
    // - Without feedback_vector_slot: EnterFrame(StackFrame::WASM) pushes 2
    //   slots (frame type marker and instance data).
    // - With feedback_vector_slot: WasmLiftoffFrameSetup pushes 3 slots (frame
    //   type marker, instance data, and feedback vector), plus a zero into the
    //   4th slot (the OSR target slot at rbp - kOSRTargetOffset) when
    //   V8_X64_16BYTE_STACK_ALIGNMENT_BOOL is enabled.
    assm.EnterFrame(StackFrame::WASM);
    if (feedback_vector_slot) {
      assm.Push(Immediate(0));
      if (V8_X64_16BYTE_STACK_ALIGNMENT_BOOL) {
        assm.Push(Immediate(0));
      }
    }

    int prepare_offset = assm.PrepareStackFrame();
    assm.RecordUsedSpillOffset(used_spill_offset);
    assm.AlignFrameSize();
    assm.PatchPrepareStackFrame(prepare_offset, &safepoint_table_builder,
                                feedback_vector_slot, /*stack_param_slots=*/0,
                                /*stack_return_slots=*/0);

    assm.movq(rax, rbp);
    assm.subq(rax, rsp);
    assm.movq(rcx, rsp);
    assm.andq(rcx, Immediate(0xF));
    assm.shlq(rcx, Immediate(32));
    assm.orq(rax, rcx);
    assm.LeaveFrame(StackFrame::WASM);
    assm.ret(0);

    CodeDesc desc;
    assm.GetCode(isolate(), &desc);
    buffer->MakeExecutable();
    auto fn = GeneratedCode<int64_t>::FromBuffer(isolate(), buffer->start());
    int64_t result = fn.Call();
    int32_t allocated_bytes_below_rbp =
        static_cast<int32_t>(result & 0xFFFFFFFF);
    int32_t rsp_misalignment = static_cast<int32_t>(result >> 32);

    EXPECT_EQ(allocated_bytes_below_rbp, assm.GetTotalFrameSize());
    if (V8_X64_16BYTE_STACK_ALIGNMENT_BOOL) {
      int expected_bytes = RoundUp(used_spill_offset, 2 * kSystemPointerSize);
      EXPECT_EQ(expected_bytes, assm.GetTotalFrameSize());
      EXPECT_EQ(0, allocated_bytes_below_rbp % (2 * kSystemPointerSize));
      EXPECT_EQ(0, rsp_misalignment);
    } else {
      int expected_bytes = RoundUp(used_spill_offset, kSystemPointerSize);
      EXPECT_EQ(expected_bytes, assm.GetTotalFrameSize());
    }
  };

  // Test spill offsets spanning even, odd, and sub-slot (4-byte) boundaries:
  // 32 bytes (4 slots), 36 bytes (4.5 slots), 40 bytes (5 slots, odd),
  // 44 bytes (5.5 slots), 48 bytes (6 slots, even).
  for (bool feedback_vector_slot : {false, true}) {
    for (int spill_offset : {32, 36, 40, 44, 48}) {
      check_frame_alignment(feedback_vector_slot, spill_offset);
    }
  }
}

TEST_F(LiftoffFrameAlignmentTest, PushAndPopRegistersAlignment) {
  auto check_push_pop = [&](LiftoffRegList regs) {
    auto buffer = AllocateAssemblerBuffer();
    LiftoffAssembler assm(zone(), buffer->CreateView());
    assm.set_root_array_available(false);
    SafepointTableBuilder safepoint_table_builder(zone());

    assm.EnterFrame(StackFrame::WASM);
    assm.movq(r8, rsp);
    assm.PushRegisters(regs);
    SafepointTableBuilder::Safepoint safepoint =
        safepoint_table_builder.DefineSafepoint(&assm);
    assm.RecordSpillsInSafepoint(safepoint, regs, LiftoffRegList{}, 0);
    // Measure how many bytes PushRegisters allocated on the stack and check
    // {rsp & 15 == 0} while registers are pushed.
    assm.movq(r9, r8);
    assm.subq(r9, rsp);
    assm.movq(r11, rsp);
    assm.andq(r11, Immediate(0xF));
    assm.PopRegisters(regs);
    // Verify PopRegisters restored rsp back to r8.
    assm.subq(r8, rsp);
    assm.shlq(r8, Immediate(16));
    assm.orq(r8, r11);
    assm.shlq(r8, Immediate(32));
    assm.orq(r9, r8);
    assm.movq(rax, r9);
    assm.LeaveFrame(StackFrame::WASM);
    assm.ret(0);

    CodeDesc desc;
    assm.GetCode(isolate(), &desc);
    buffer->MakeExecutable();
    auto fn = GeneratedCode<int64_t>::FromBuffer(isolate(), buffer->start());
    int64_t result = fn.Call();
    int32_t pushed_bytes = static_cast<int32_t>(result & 0xFFFFFFFF);
    int32_t rsp_misalignment = static_cast<int32_t>((result >> 32) & 0xFFFF);
    int32_t leftover_bytes = static_cast<int32_t>(result >> 48);

    EXPECT_EQ(0, leftover_bytes);
    int expected_gp_slots = (regs & kGpCacheRegList).GetNumRegsSet();
    if (V8_X64_16BYTE_STACK_ALIGNMENT_BOOL && (expected_gp_slots & 1) != 0) {
      ++expected_gp_slots;
    }
    int expected_fp_bytes =
        (regs & kFpCacheRegList).GetNumRegsSet() * kSimd128Size;
    int expected_bytes =
        expected_gp_slots * kSystemPointerSize + expected_fp_bytes;
    EXPECT_EQ(expected_bytes, pushed_bytes);
    EXPECT_EQ(expected_bytes / kSystemPointerSize, assm.OolSpillCount());
    if (V8_X64_16BYTE_STACK_ALIGNMENT_BOOL) {
      EXPECT_EQ(0, pushed_bytes % (2 * kSystemPointerSize));
      EXPECT_EQ(0, rsp_misalignment);
    }
  };

  LiftoffRegister gp0 = LiftoffRegister(rax);
  LiftoffRegister gp1 = LiftoffRegister(rcx);
  LiftoffRegister gp2 = LiftoffRegister(rdx);
  LiftoffRegister fp0 = LiftoffRegister(xmm0);

  check_push_pop(LiftoffRegList{gp0});
  check_push_pop(LiftoffRegList{gp0, gp1});
  check_push_pop(LiftoffRegList{gp0, gp1, gp2});
  check_push_pop(LiftoffRegList{gp0, fp0});
  check_push_pop(LiftoffRegList{gp0, gp1, fp0});
}

TEST_F(LiftoffFrameAlignmentTest, AllocateAndDeallocateStackSlotAlignment) {
  for (uint32_t size : {8u, 16u, 40u}) {
    auto buffer = AllocateAssemblerBuffer();
    LiftoffAssembler assm(zone(), buffer->CreateView());
    assm.set_root_array_available(false);

    assm.EnterFrame(StackFrame::WASM);
    assm.movq(r8, rsp);
    assm.AllocateStackSlot(rcx, size);
    assm.movq(rax, r8);
    assm.subq(rax, rsp);
    assm.movq(r9, rsp);
    assm.andq(r9, Immediate(0xF));
    assm.DeallocateStackSlot(size);
    assm.subq(r8, rsp);
    assm.shlq(r8, Immediate(16));
    assm.orq(r8, r9);
    assm.shlq(r8, Immediate(32));
    assm.orq(rax, r8);
    assm.LeaveFrame(StackFrame::WASM);
    assm.ret(0);

    CodeDesc desc;
    assm.GetCode(isolate(), &desc);
    buffer->MakeExecutable();
    auto fn = GeneratedCode<int64_t>::FromBuffer(isolate(), buffer->start());
    int64_t result = fn.Call();
    int32_t allocated_bytes = static_cast<int32_t>(result & 0xFFFFFFFF);
    int32_t rsp_misalignment = static_cast<int32_t>((result >> 32) & 0xFFFF);
    int32_t leftover_bytes = static_cast<int32_t>(result >> 48);

    EXPECT_EQ(0, leftover_bytes);
    if (V8_X64_16BYTE_STACK_ALIGNMENT_BOOL) {
      int expected_bytes = RoundUp(size, 2 * kSystemPointerSize);
      EXPECT_EQ(expected_bytes, allocated_bytes);
      EXPECT_EQ(0, allocated_bytes % (2 * kSystemPointerSize));
      EXPECT_EQ(0, rsp_misalignment);
    } else {
      EXPECT_EQ(static_cast<int32_t>(size), allocated_bytes);
    }
  }
}
#endif  // V8_TARGET_ARCH_X64

}  // namespace wasm
}  // namespace internal
}  // namespace v8
