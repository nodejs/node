// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/common/flag-utils.h"
#include "test/unittests/compiler/backend/turboshaft-instruction-selector-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace turboshaft {

#ifdef V8_TARGET_ARCH_ARM64

using ScheduledStream = TurboshaftInstructionSelectorTest::Stream;
using ScheduledBuilder = TurboshaftInstructionSelectorTest::StreamBuilder;

class InstructionSchedulerTurboshaftArm64Test
    : public TurboshaftInstructionSelectorTest {};

#ifdef V8_ENABLE_SIMD128
TEST_F(InstructionSchedulerTurboshaftArm64Test, ParallelLdrQFmulStrQ) {
  ScheduledBuilder m(this, MachineType::Simd128(), MachineType::Pointer(),
                     MachineType::Pointer());

  OpIndex load_base = m.Parameter(0);
  OpIndex store_base = m.Parameter(1);

  // 12 LDR Q
  constexpr int32_t kNumLoads = 12;
  OpIndex offsets[kNumLoads];
  OpIndex loads[kNumLoads];
  for (int i = 0; i < kNumLoads; ++i) {
    offsets[i] = m.IntPtrConstant(static_cast<int64_t>(i) * 16);
    loads[i] = m.Load(MachineType::Simd128(), load_base, offsets[i]);
  }

  // Multiply each pair of loaded values and store the result.
  auto mem_rep = MachineRepresentation::kSimd128;
  for (unsigned i = 0; i < kNumLoads; i += 2) {
    m.Store(
        mem_rep, store_base, offsets[i],
        m.Simd128Binop(loads[i], loads[i + 1], Simd128BinopOp::Kind::kF32x4Mul),
        WriteBarrierKind::kNoWriteBarrier);
  }
  m.Return(m.Int32Constant(0));

  ScheduledStream s =
      m.Build(CpuFeatures::SupportedFeatures(), kAllExceptNopInstructions,
              InstructionSelector::kAllSourcePositions,
              InstructionSelector::kEnableScheduling);

  // All the loads are the longest latency, so they will be scheduled first.
  for (unsigned i = 0; i < kNumLoads; ++i) {
    EXPECT_EQ(kArm64LdrQ, s[i]->arch_opcode());
  }

  constexpr int32_t kArithBase = kNumLoads;
  EXPECT_EQ(kArm64FMul, s[kArithBase]->arch_opcode());
  EXPECT_EQ(kArm64FMul, s[kArithBase + 1]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 2]->arch_opcode());
  EXPECT_EQ(kArm64FMul, s[kArithBase + 3]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 4]->arch_opcode());
  EXPECT_EQ(kArm64FMul, s[kArithBase + 5]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 6]->arch_opcode());
  EXPECT_EQ(kArm64FMul, s[kArithBase + 7]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 8]->arch_opcode());
  EXPECT_EQ(kArm64FMul, s[kArithBase + 9]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 10]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 11]->arch_opcode());
}

TEST_F(InstructionSchedulerTurboshaftArm64Test, LdrQIntoFMA) {
  ScheduledBuilder m(this, MachineType::Simd128(), MachineType::Pointer());

  OpIndex load_base = m.Parameter(0);

  // 11 LDR Q
  constexpr int32_t kNumLoads = 11;
  OpIndex offsets[kNumLoads];
  OpIndex loads[kNumLoads];
  for (int i = 0; i < kNumLoads; ++i) {
    offsets[i] = m.IntPtrConstant(static_cast<int64_t>(i) * 16);
    loads[i] = m.Load(MachineType::Simd128(), load_base, offsets[i]);
  }

  // The first load is used as the initial accumulator value. Then each pair
  // of loads is FMA'd.
  OpIndex acc = loads[0];
  for (unsigned i = 1; i < kNumLoads; i += 2) {
    acc = m.Simd128Ternary(loads[i], loads[i + 1], acc,
                           Simd128TernaryOp::Kind::kF32x4Qfma);
  }
  m.Return(acc);

  ScheduledStream s =
      m.Build(CpuFeatures::SupportedFeatures(), kAllExceptNopInstructions,
              InstructionSelector::kAllSourcePositions,
              InstructionSelector::kEnableScheduling);

  // First loads.
  EXPECT_EQ(kArm64LdrQ, s[0]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[1]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[2]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[3]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[4]->arch_opcode());

  // Then one fma.
  EXPECT_EQ(kArm64Ffma, s[5]->arch_opcode());

  // Then two loads.
  EXPECT_EQ(kArm64LdrQ, s[6]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[7]->arch_opcode());

  // Then one fma.
  EXPECT_EQ(kArm64Ffma, s[8]->arch_opcode());

  // Then two loads.
  EXPECT_EQ(kArm64LdrQ, s[9]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[10]->arch_opcode());

  // Then one fma.
  EXPECT_EQ(kArm64Ffma, s[11]->arch_opcode());

  // Final two loads.
  EXPECT_EQ(kArm64LdrQ, s[12]->arch_opcode());
  EXPECT_EQ(kArm64LdrQ, s[13]->arch_opcode());

  // Final two fmas.
  EXPECT_EQ(kArm64Ffma, s[14]->arch_opcode());
  EXPECT_EQ(kArm64Ffma, s[15]->arch_opcode());
}

TEST_F(InstructionSchedulerTurboshaftArm64Test, LdrQIntoFixedFP) {
  ScheduledBuilder m(this, MachineType::Simd128(), MachineType::Pointer(),
                     MachineType::Pointer());
  OpIndex load_base = m.Parameter(0);
  OpIndex store_base = m.Parameter(1);

  // 6 LDR Q
  constexpr int32_t kNumLoads = 12;
  OpIndex offsets[kNumLoads];
  OpIndex loads[kNumLoads];
  for (int i = 0; i < kNumLoads; ++i) {
    offsets[i] = m.IntPtrConstant(static_cast<int64_t>(i) * 16);
    loads[i] = m.Load(MachineType::Simd128(), load_base, offsets[i]);
  }

  constexpr int32_t kNumArithOps = kNumLoads / 2;
  constexpr Simd128BinopOp::Kind binops[kNumArithOps] = {
      Simd128BinopOp::Kind::kF32x4Mul, Simd128BinopOp::Kind::kF32x4Add,
      Simd128BinopOp::Kind::kF32x4Sub, Simd128BinopOp::Kind::kF32x4Min,
      Simd128BinopOp::Kind::kF32x4Max, Simd128BinopOp::Kind::kF32x4Div,
  };

  // Then for each pair of loads, we iterate through the above list of binops
  // and insert that operation. The result is stored to memory.
  auto mem_rep = MachineRepresentation::kSimd128;
  for (unsigned i = 0; i < kNumLoads; i += 2) {
    m.Store(mem_rep, store_base, offsets[i],
            m.Simd128Binop(loads[i], loads[i + 1], binops[i / 2]),
            WriteBarrierKind::kNoWriteBarrier);
  }
  m.Return(m.Int32Constant(0));
  ScheduledStream s =
      m.Build(CpuFeatures::SupportedFeatures(), kAllExceptNopInstructions,
              InstructionSelector::kAllSourcePositions,
              InstructionSelector::kEnableScheduling);

  // The loads are the longest latency.
  for (unsigned i = 0; i < kNumLoads; ++i) {
    EXPECT_EQ(kArm64LdrQ, s[i]->arch_opcode());
  }

  constexpr int32_t kArithBase = kNumLoads;
  EXPECT_EQ(kArm64FMul, s[kArithBase]->arch_opcode());
  EXPECT_EQ(kArm64FAdd, s[kArithBase + 1]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 2]->arch_opcode());
  EXPECT_EQ(kArm64FSub, s[kArithBase + 3]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 4]->arch_opcode());
  EXPECT_EQ(kArm64FMin, s[kArithBase + 5]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 6]->arch_opcode());
  EXPECT_EQ(kArm64FMax, s[kArithBase + 7]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 8]->arch_opcode());
  EXPECT_EQ(kArm64FDiv, s[kArithBase + 9]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 10]->arch_opcode());
  EXPECT_EQ(kArm64StrQ, s[kArithBase + 11]->arch_opcode());
}

#endif  // V8_ENABLE_SIMD128

TEST_F(InstructionSchedulerTurboshaftArm64Test, LdrSIntoFixedFP) {
  ScheduledBuilder m(this, MachineType::Float32(), MachineType::Pointer(),
                     MachineType::Pointer());
  OpIndex load_base = m.Parameter(0);
  OpIndex store_base = m.Parameter(1);

  // 12 LDR S
  constexpr int32_t kNumLoads = 12;
  OpIndex offsets[kNumLoads];
  OpIndex loads[kNumLoads];
  for (int i = 0; i < kNumLoads; ++i) {
    offsets[i] = m.IntPtrConstant(static_cast<int64_t>(i) * 16);
    loads[i] = m.Load(MachineType::Float32(), load_base, offsets[i]);
  }

  constexpr int32_t kNumArithOps = kNumLoads / 2;
  constexpr FloatBinopOp::Kind binops[kNumArithOps] = {
      FloatBinopOp::Kind::kAdd, FloatBinopOp::Kind::kMul,
      FloatBinopOp::Kind::kSub, FloatBinopOp::Kind::kMin,
      FloatBinopOp::Kind::kMax, FloatBinopOp::Kind::kDiv,
  };

  // Use the above binops to operate on a pair of loads and store the result.
  auto mem_rep = MachineRepresentation::kFloat32;
  for (unsigned i = 0; i < kNumLoads; i += 2) {
    m.Store(mem_rep, store_base, offsets[i],
            m.Float32Binop(loads[i], loads[i + 1], binops[i / 2]),
            WriteBarrierKind::kNoWriteBarrier);
  }
  m.Return(m.Int32Constant(0));
  ScheduledStream s =
      m.Build(CpuFeatures::SupportedFeatures(), kAllExceptNopInstructions,
              InstructionSelector::kAllSourcePositions,
              InstructionSelector::kEnableScheduling);

  // All the loads happen first.
  for (unsigned i = 0; i < kNumLoads; ++i) {
    EXPECT_EQ(kArm64LdrS, s[i]->arch_opcode());
  }

  constexpr int32_t kArithBase = kNumLoads;
  // The divide has been scheduled earlier than the other arith ops.
  EXPECT_EQ(kArm64Float32Div, s[kArithBase]->arch_opcode());
  EXPECT_EQ(kArm64Float32Add, s[kArithBase + 1]->arch_opcode());
  EXPECT_EQ(kArm64Float32Sub, s[kArithBase + 2]->arch_opcode());
  EXPECT_EQ(kArm64Float32Mul, s[kArithBase + 3]->arch_opcode());
  EXPECT_EQ(kArm64Float32Min, s[kArithBase + 4]->arch_opcode());
  EXPECT_EQ(kArm64StrS, s[kArithBase + 5]->arch_opcode());
  EXPECT_EQ(kArm64StrS, s[kArithBase + 6]->arch_opcode());
  EXPECT_EQ(kArm64Float32Max, s[kArithBase + 7]->arch_opcode());
  EXPECT_EQ(kArm64StrS, s[kArithBase + 8]->arch_opcode());
  EXPECT_EQ(kArm64StrS, s[kArithBase + 9]->arch_opcode());
  EXPECT_EQ(kArm64StrS, s[kArithBase + 10]->arch_opcode());
  EXPECT_EQ(kArm64StrS, s[kArithBase + 11]->arch_opcode());
}

TEST_F(InstructionSchedulerTurboshaftArm64Test, ScheduleBackwards) {
  ScheduledBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                     MachineType::Pointer(), MachineType::Pointer(),
                     MachineType::Uint64());
  OpIndex store_base = m.Parameter(0);
  OpIndex address_input0 = m.Parameter(1);
  OpIndex address_input1 = m.Parameter(2);
  OpIndex value = m.Parameter(3);

  OpIndex offset0 = m.IntPtrConstant(0);

  OpIndex address0 = m.WordPtrAdd(address_input0, address_input1);
  OpIndex address1 = m.WordPtrSub(address0, address_input1);
  OpIndex address2 = m.WordPtrSub(store_base, address_input1);
  OpIndex loaded_address = m.Load(MachineType::Pointer(), address1, address2);
  USE(m.Load(loaded_address, offset0, LoadOp::Kind::Trapping(),
             MemoryRepresentation::Int32(), RegisterRepresentation::Word32()));

  OpIndex divided = m.Uint64Div(value, value);
  m.Store(MachineRepresentation::kWord64, loaded_address, offset0, divided,
          WriteBarrierKind::kNoWriteBarrier);
  m.Return(m.Int32Constant(0));

  ScheduledStream s =
      m.Build(CpuFeatures::SupportedFeatures(), kAllExceptNopInstructions,
              InstructionSelector::kAllSourcePositions,
              InstructionSelector::kEnableScheduling);

  // The address-calculation chain is scheduled before the independent divide
  // feeding the store. Backward scheduling should pull the divide/store chain
  // first, saving cycles, but we need to be using 'data' predecessors to
  // calculate the latency accurately.
  ASSERT_GE(s.size(), 7U);
  EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
  EXPECT_EQ(kArm64Sub, s[1]->arch_opcode());
  EXPECT_EQ(kArm64Sub, s[2]->arch_opcode());
  EXPECT_EQ(kArm64Ldr, s[3]->arch_opcode());
  EXPECT_EQ(kArm64Udiv, s[4]->arch_opcode());
  EXPECT_EQ(kArm64LdrW, s[5]->arch_opcode());
  EXPECT_EQ(kArm64Str, s[6]->arch_opcode());
}

#endif  // V8_TARGET_ARCH_ARM64

}  // namespace turboshaft
}  // namespace compiler
}  // namespace internal
}  // namespace v8
