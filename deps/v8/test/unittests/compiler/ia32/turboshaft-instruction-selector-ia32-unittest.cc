// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/backend/turboshaft-instruction-selector-unittest.h"

namespace v8::internal::compiler::turboshaft {

namespace {

// Immediates (random subset).
const int32_t kImmediates[] = {kMinInt, -42, -1,   0,      1,          2,
                               3,       4,   5,    6,      7,          8,
                               16,      42,  0xFF, 0xFFFF, 0x0F0F0F0F, kMaxInt};

}  // namespace

TEST_F(TurboshaftInstructionSelectorTest, Word32AddWithParameter) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Word32Add(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
}

TEST_F(TurboshaftInstructionSelectorTest, Word32AddWithImmediate) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Add(m.Parameter(0), m.Int32Constant(imm)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      if (imm != kMinInt) {
        EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
      }
      if (imm == 0) {
        ASSERT_EQ(1U, s[0]->InputCount());
      } else {
        ASSERT_EQ(2U, s[0]->InputCount());
        EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      }
    }
    {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Add(m.Int32Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      if (imm != kMinInt) {
        EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
      }
      if (imm == 0) {
        ASSERT_EQ(1U, s[0]->InputCount());
      } else {
        ASSERT_EQ(2U, s[0]->InputCount());
        EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      }
    }
  }
}

TEST_F(TurboshaftInstructionSelectorTest, Word32SubWithParameter) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Word32Sub(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Sub, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

TEST_F(TurboshaftInstructionSelectorTest, Word32SubWithImmediate) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Sub(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kIA32Sub, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
  }
}

// -----------------------------------------------------------------------------
// Conversions.

TEST_F(TurboshaftInstructionSelectorTest, ChangeFloat32ToFloat64WithParameter) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float32());
  m.Return(m.ChangeFloat32ToFloat64(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Float32ToFloat64, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

TEST_F(TurboshaftInstructionSelectorTest,
       TruncateFloat64ToFloat32WithParameter) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float64());
  m.Return(m.TruncateFloat64ToFloat32(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Float64ToFloat32, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

// -----------------------------------------------------------------------------
// Better left operand for commutative binops

#if 0

// TODO(dmercadier): evaluate whether Turboshaft should reorder additions.

TEST_F(TurboshaftInstructionSelectorTest, BetterLeftOperandTestAddBinop) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex param1 = m.Parameter(0);
  OpIndex param2 = m.Parameter(1);
  OpIndex add = m.Word32Add(param1, param2);
  m.Return(m.Word32Add(add, param1));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_TRUE(s[0]->InputAt(0)->IsUnallocated());
  EXPECT_EQ(s.ToVreg(param1), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(param2), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(2U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(param1), s.ToVreg(s[0]->InputAt(0)));
}


TEST_F(TurboshaftInstructionSelectorTest, BetterLeftOperandTestMulBinop) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex param1 = m.Parameter(0);
  OpIndex param2 = m.Parameter(1);
  OpIndex mul = m.Word32Mul(param1, param2);
  m.Return(m.Word32Mul(mul, param1));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kIA32Imul, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_TRUE(s[0]->InputAt(0)->IsUnallocated());
  EXPECT_EQ(s.ToVreg(param2), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(param1), s.ToVreg(s[0]->InputAt(1)));
}

#endif

// -----------------------------------------------------------------------------
// Conversions.

TEST_F(TurboshaftInstructionSelectorTest, ChangeUint32ToFloat64WithParameter) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Uint32());
  m.Return(m.ChangeUint32ToFloat64(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Uint32ToFloat64, s[0]->arch_opcode());
}

// -----------------------------------------------------------------------------
// Loads and stores

struct MemoryAccess {
  MachineType type;
  ArchOpcode load_opcode;
  ArchOpcode store_opcode;
};


std::ostream& operator<<(std::ostream& os, const MemoryAccess& memacc) {
  return os << memacc.type;
}


static const MemoryAccess kMemoryAccesses[] = {
    {MachineType::Int8(), kIA32Movsxbl, kIA32Movb},
    {MachineType::Uint8(), kIA32Movzxbl, kIA32Movb},
    {MachineType::Int16(), kIA32Movsxwl, kIA32Movw},
    {MachineType::Uint16(), kIA32Movzxwl, kIA32Movw},
    {MachineType::Int32(), kIA32Movl, kIA32Movl},
    {MachineType::Uint32(), kIA32Movl, kIA32Movl},
    {MachineType::Float32(), kIA32Movss, kIA32Movss},
    {MachineType::Float64(), kIA32Movsd, kIA32Movsd}};

using TurboshaftInstructionSelectorMemoryAccessTest =
    TurboshaftInstructionSelectorTestWithParam<MemoryAccess>;

TEST_P(TurboshaftInstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, MachineType::Pointer(),
                  MachineType::Int32());
  m.Return(m.Load(memacc.type, m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

TEST_P(TurboshaftInstructionSelectorMemoryAccessTest, LoadWithImmediateBase) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, base, kImmediates) {
    StreamBuilder m(this, memacc.type, MachineType::Pointer());
    m.Return(m.Load(memacc.type, m.Int32Constant(base), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
    if (base == 0) {
      ASSERT_EQ(1U, s[0]->InputCount());
    } else {
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(base, s.ToInt32(s[0]->InputAt(1)));
    }
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

TEST_P(TurboshaftInstructionSelectorMemoryAccessTest, LoadWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, kImmediates) {
    StreamBuilder m(this, memacc.type, MachineType::Pointer());
    m.Return(m.Load(m.Parameter(0), LoadOp::Kind::RawAligned(),
                    MemoryRepresentation::FromMachineType(memacc.type), index));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
    if (index == 0) {
      ASSERT_EQ(1U, s[0]->InputCount());
    } else if (index != kMinInt) {
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    }
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

TEST_P(TurboshaftInstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                  MachineType::Int32(), memacc.type);
  m.Store(memacc.type.representation(), m.Parameter(0), m.Parameter(1),
          m.Parameter(2), kNoWriteBarrier);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
  EXPECT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(0U, s[0]->OutputCount());
}

TEST_P(TurboshaftInstructionSelectorMemoryAccessTest, StoreWithImmediateBase) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, base, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    memacc.type);
    m.Store(memacc.type.representation(), m.Int32Constant(base), m.Parameter(0),
            m.Parameter(1), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
    if (base == 0) {
      ASSERT_EQ(2U, s[0]->InputCount());
    } else {
      ASSERT_EQ(3U, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(base, s.ToInt32(s[0]->InputAt(1)));
    }
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

TEST_P(TurboshaftInstructionSelectorMemoryAccessTest, StoreWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    memacc.type);
    m.Store(m.Parameter(0), m.Parameter(1), StoreOp::Kind::RawAligned(),
            MemoryRepresentation::FromMachineType(memacc.type), kNoWriteBarrier,
            index);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
    if (index == 0) {
      ASSERT_EQ(2U, s[0]->InputCount());
    } else if (index != kMinInt) {
      ASSERT_EQ(3U, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    }
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_SUITE_P(TurboshaftInstructionSelectorTest,
                         TurboshaftInstructionSelectorMemoryAccessTest,
                         ::testing::ValuesIn(kMemoryAccesses));

// -----------------------------------------------------------------------------
// AddressingMode for loads and stores.

class TurboshaftAddressingModeUnitTest
    : public TurboshaftInstructionSelectorTest {
 public:
  TurboshaftAddressingModeUnitTest() : m(nullptr) {}
  ~TurboshaftAddressingModeUnitTest() override { delete m; }

  void SetUp() override {
    TurboshaftInstructionSelectorTest::SetUp();
    Reset();
  }

  void Run(OpIndex base, OpIndex load_index, OpIndex store_index,
           AddressingMode mode, int offset, int scale) {
    OpIndex load = m->Load(base, load_index, LoadOp::Kind::RawAligned(),
                           MemoryRepresentation::Int32(), offset, scale);
    m->Store(base, store_index, load, StoreOp::Kind::RawAligned(),
             MemoryRepresentation::Int32(), kNoWriteBarrier, offset, scale);
    m->Return(m->Int32Constant(0));
    Stream s = m->Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(mode, s[0]->addressing_mode());
    EXPECT_EQ(mode, s[1]->addressing_mode());
  }

  OpIndex zero;
  OpIndex null_ptr;
  OpIndex non_zero;
  OpIndex base_reg;   // opaque value to generate base as register
  OpIndex index_reg;  // opaque value to generate index as register
  OpIndex scales[4];
  StreamBuilder* m;

  void Reset() {
    delete m;
    m = new StreamBuilder(this, MachineType::Int32(), MachineType::Int32(),
                          MachineType::Int32());
    zero = m->Int32Constant(0);
    null_ptr = m->Int32Constant(0);
    non_zero = m->Int32Constant(127);
    base_reg = m->Parameter(0);
    index_reg = m->Parameter(0);

    scales[0] = m->Int32Constant(1);
    scales[1] = m->Int32Constant(2);
    scales[2] = m->Int32Constant(4);
    scales[3] = m->Int32Constant(8);
  }
};

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MR) {
  OpIndex base = base_reg;
  int offset = 0;
  int scale = 0;
  Run(base, {}, {}, kMode_MR, offset, scale);
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MRI) {
  OpIndex base = base_reg;
  int offset = 127;
  int scale = 0;
  Run(base, {}, {}, kMode_MRI, offset, scale);
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MR1) {
  OpIndex base = base_reg;
  OpIndex index = index_reg;
  int offset = 0;
  int scale = 0;
  Run(base, index, index, kMode_MR1, offset, scale);
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MRN) {
  AddressingMode expected[] = {kMode_MR1, kMode_MR2, kMode_MR4, kMode_MR8};
  for (size_t i = 0; i < 4; ++i) {
    Reset();
    int offset = 0;
    Run(base_reg, index_reg, index_reg, expected[i], offset, i);
  }
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MR1I) {
  int scale = 0;
  Run(base_reg, index_reg, index_reg, kMode_MR1I, 127, scale);
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MRNI) {
  AddressingMode expected[] = {kMode_MR1I, kMode_MR2I, kMode_MR4I, kMode_MR8I};
  for (size_t i = 0; i < arraysize(scales); ++i) {
    Reset();
    OpIndex base = base_reg;
    int offset = 127;
    Run(base, index_reg, index_reg, expected[i], offset, i);
  }
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_M1ToMR) {
  OpIndex base = null_ptr;
  OpIndex index = index_reg;
  int offset = 0;
  int scale = 0;
  // M1 maps to MR
  Run(base, index, index, kMode_MR, offset, scale);
}

TEST_F(TurboshaftAddressingModeUnitTest, AddressingMode_MI) {
  OpIndex bases[] = {zero, non_zero};
  for (size_t j = 0; j < arraysize(bases); ++j) {
    Reset();
    int offset = 0;
    int scale = 0;
    OpIndex base = bases[j];
    Run(base, {}, {}, kMode_MI, offset, scale);
    Reset();
  }
}

// -----------------------------------------------------------------------------
// Multiplication.

struct MultParam {
  int value;
  bool lea_expected;
  AddressingMode addressing_mode;
};


std::ostream& operator<<(std::ostream& os, const MultParam& m) {
  return os << m.value << "." << m.lea_expected << "." << m.addressing_mode;
}


const MultParam kMultParams[] = {{-1, false, kMode_None},
                                 {0, false, kMode_None},
                                 {1, true, kMode_MR},
                                 {2, true, kMode_M2},
                                 {3, true, kMode_MR2},
                                 {4, true, kMode_M4},
                                 {5, true, kMode_MR4},
                                 {6, false, kMode_None},
                                 {7, false, kMode_None},
                                 {8, true, kMode_M8},
                                 {9, true, kMode_MR8},
                                 {10, false, kMode_None},
                                 {11, false, kMode_None}};

using TurboshaftInstructionSelectorMultTest =
    TurboshaftInstructionSelectorTestWithParam<MultParam>;

static unsigned InputCountForLea(AddressingMode mode) {
  switch (mode) {
    case kMode_MR1I:
    case kMode_MR2I:
    case kMode_MR4I:
    case kMode_MR8I:
      return 3U;
    case kMode_M1I:
    case kMode_M2I:
    case kMode_M4I:
    case kMode_M8I:
      return 2U;
    case kMode_MR1:
    case kMode_MR2:
    case kMode_MR4:
    case kMode_MR8:
    case kMode_MRI:
      return 2U;
    case kMode_M1:
    case kMode_M2:
    case kMode_M4:
    case kMode_M8:
    case kMode_MI:
    case kMode_MR:
      return 1U;
    default:
      UNREACHABLE();
  }
}

TEST_P(TurboshaftInstructionSelectorMultTest, Mult32) {
  const MultParam m_param = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
  OpIndex param = m.Parameter(0);
  OpIndex mult = m.Word32Mul(param, m.Int32Constant(m_param.value));
  m.Return(mult);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(m_param.addressing_mode, s[0]->addressing_mode());
  if (m_param.lea_expected) {
    EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
    ASSERT_EQ(InputCountForLea(s[0]->addressing_mode()), s[0]->InputCount());
  } else {
    EXPECT_EQ(kIA32Imul, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
  }
  EXPECT_EQ(s.ToVreg(param), s.ToVreg(s[0]->InputAt(0)));
}

#if 0

static AddressingMode AddressingModeForAddMult(int32_t imm,
                                               const MultParam& m) {
  if (imm == 0) return m.addressing_mode;
  switch (m.addressing_mode) {
    case kMode_MR1:
      return kMode_MR1I;
    case kMode_MR2:
      return kMode_MR2I;
    case kMode_MR4:
      return kMode_MR4I;
    case kMode_MR8:
      return kMode_MR8I;
    case kMode_M1:
      return kMode_M1I;
    case kMode_M2:
      return kMode_M2I;
    case kMode_M4:
      return kMode_M4I;
    case kMode_M8:
      return kMode_M8I;
    case kMode_MR:
      return kMode_MRI;
    default:
      UNREACHABLE();
  }
}

// TODO(dmercadier): Fix following test.

TEST_P(TurboshaftInstructionSelectorMultTest, MultAdd32) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    if (imm == kMinInt) continue;
    const MultParam m_param = GetParam();
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    OpIndex param = m.Parameter(0);
    OpIndex mult = m.Word32Add(m.Word32Mul(param, m.Int32Constant(m_param.value)),
                            m.Int32Constant(imm));
    m.Return(mult);
    Stream s = m.Build();
    if (m_param.lea_expected) {
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
      EXPECT_EQ(AddressingModeForAddMult(imm, m_param),
                s[0]->addressing_mode());
      unsigned input_count = InputCountForLea(s[0]->addressing_mode());
      ASSERT_EQ(input_count, s[0]->InputCount());
      if (imm != 0) {
        ASSERT_EQ(InstructionOperand::IMMEDIATE,
                  s[0]->InputAt(input_count - 1)->kind());
        EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(input_count - 1)));
      }
    } else {
      ASSERT_EQ(2U, s.size());
      EXPECT_EQ(kIA32Imul, s[0]->arch_opcode());
      EXPECT_EQ(kIA32Lea, s[1]->arch_opcode());
    }
  }
}

#endif

INSTANTIATE_TEST_SUITE_P(TurboshaftInstructionSelectorTest,
                         TurboshaftInstructionSelectorMultTest,
                         ::testing::ValuesIn(kMultParams));

TEST_F(TurboshaftInstructionSelectorTest, Int32MulOverflownBits) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  OpIndex const n = m.Int32MulOverflownBits(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32ImulHigh, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_TRUE(s.IsFixed(s[0]->InputAt(0), eax));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  EXPECT_TRUE(!s.IsUsedAtStart(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  EXPECT_TRUE(s.IsFixed(s[0]->OutputAt(0), edx));
}

// -----------------------------------------------------------------------------
// Binops with a memory operand.

TEST_F(TurboshaftInstructionSelectorTest, LoadAnd32) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  m.Return(m.Word32BitwiseAnd(
      p0, m.Load(MachineType::Int32(), p1, m.Int32Constant(127))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32And, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
}

TEST_F(TurboshaftInstructionSelectorTest, LoadImmutableAnd32) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  m.Return(m.Word32BitwiseAnd(
      p0, m.LoadImmutable(MachineType::Int32(), p1, m.Int32Constant(127))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32And, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
}

TEST_F(TurboshaftInstructionSelectorTest, LoadOr32) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  m.Return(m.Word32BitwiseOr(
      p0, m.Load(MachineType::Int32(), p1, m.Int32Constant(127))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Or, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
}

TEST_F(TurboshaftInstructionSelectorTest, LoadXor32) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  m.Return(m.Word32BitwiseXor(
      p0, m.Load(MachineType::Int32(), p1, m.Int32Constant(127))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Xor, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
}

TEST_F(TurboshaftInstructionSelectorTest, LoadAdd32) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  m.Return(
      m.Word32Add(p0, m.Load(MachineType::Int32(), p1, m.Int32Constant(127))));
  Stream s = m.Build();
  // Use lea instead of add, so memory operand is invalid.
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kIA32Movl, s[0]->arch_opcode());
  EXPECT_EQ(kIA32Lea, s[1]->arch_opcode());
}

TEST_F(TurboshaftInstructionSelectorTest, LoadSub32) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const p1 = m.Parameter(1);
  m.Return(
      m.Word32Sub(p0, m.Load(MachineType::Int32(), p1, m.Int32Constant(127))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Sub, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
}

// -----------------------------------------------------------------------------
// Floating point operations.

TEST_F(TurboshaftInstructionSelectorTest, Float32Abs) {
  {
    StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
    OpIndex const p0 = m.Parameter(0);
    OpIndex const n = m.Float32Abs(p0);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kFloat32Abs, s[0]->arch_opcode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE(s.IsSameAsFirst(s[0]->Output()));
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
  {
    StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
    OpIndex const p0 = m.Parameter(0);
    OpIndex const n = m.Float32Abs(p0);
    m.Return(n);
    Stream s = m.Build(AVX);
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kFloat32Abs, s[0]->arch_opcode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}

TEST_F(TurboshaftInstructionSelectorTest, Float64Abs) {
  {
    StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
    OpIndex const p0 = m.Parameter(0);
    OpIndex const n = m.Float64Abs(p0);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kFloat64Abs, s[0]->arch_opcode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE(s.IsSameAsFirst(s[0]->Output()));
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
  {
    StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
    OpIndex const p0 = m.Parameter(0);
    OpIndex const n = m.Float64Abs(p0);
    m.Return(n);
    Stream s = m.Build(AVX);
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kFloat64Abs, s[0]->arch_opcode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}

TEST_F(TurboshaftInstructionSelectorTest, Float64BinopArithmetic) {
  {
    StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                    MachineType::Float64());
    OpIndex add = m.Float64Add(m.Parameter(0), m.Parameter(1));
    OpIndex mul = m.Float64Mul(add, m.Parameter(1));
    OpIndex sub = m.Float64Sub(mul, add);
    OpIndex ret = m.Float64Div(mul, sub);
    m.Return(ret);
    Stream s = m.Build(AVX);
    ASSERT_EQ(4U, s.size());
    EXPECT_EQ(kFloat64Add, s[0]->arch_opcode());
    EXPECT_EQ(kFloat64Mul, s[1]->arch_opcode());
    EXPECT_EQ(kFloat64Sub, s[2]->arch_opcode());
    EXPECT_EQ(kFloat64Div, s[3]->arch_opcode());
  }
  {
    StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                    MachineType::Float64());
    OpIndex add = m.Float64Add(m.Parameter(0), m.Parameter(1));
    OpIndex mul = m.Float64Mul(add, m.Parameter(1));
    OpIndex sub = m.Float64Sub(mul, add);
    OpIndex ret = m.Float64Div(mul, sub);
    m.Return(ret);
    Stream s = m.Build();
    ASSERT_EQ(4U, s.size());
    EXPECT_EQ(kFloat64Add, s[0]->arch_opcode());
    EXPECT_EQ(kFloat64Mul, s[1]->arch_opcode());
    EXPECT_EQ(kFloat64Sub, s[2]->arch_opcode());
    EXPECT_EQ(kFloat64Div, s[3]->arch_opcode());
  }
}

// -----------------------------------------------------------------------------
// Branch-if-overflow fusion
struct OverflowBinopOp {
  TSBinop op;
  const char* constructor_name;
  ArchOpcode arch_opcode;
};

std::ostream& operator<<(std::ostream& os, const OverflowBinopOp& bop) {
  return os << bop.constructor_name;
}

const OverflowBinopOp kOverflowBinaryOperationsForBranchFusion[] = {
    {TSBinop::kInt32AddCheckOverflow, "Int32AddCheckOverflow", kIA32Add},
    {TSBinop::kInt32SubCheckOverflow, "kInt32SubCheckOverflow", kIA32Sub},
    {TSBinop::kInt32MulCheckOverflow, "Int32MulCheckOverflow", kIA32Imul}};

using TurboshaftInstructionSelectorBranchIfOverflowTest =
    TurboshaftInstructionSelectorTestWithParam<OverflowBinopOp>;

TEST_P(TurboshaftInstructionSelectorBranchIfOverflowTest,
       BranchIfZeroWithParameters) {
  const OverflowBinopOp ovf_binop = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Block *a = m.NewBlock(), *b = m.NewBlock();
  OpIndex n = m.Emit(ovf_binop.op, m.Parameter(0), m.Parameter(1));
  m.Branch(m.Word32Equal(m.Projection(n, 1), m.Int32Constant(0)), a, b);
  m.Bind(a);
  m.Return(m.Projection(n, 0));
  m.Bind(b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(ovf_binop.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kNotOverflow, s[0]->flags_condition());
}

TEST_P(TurboshaftInstructionSelectorBranchIfOverflowTest,
       BranchIfNotZeroWithParameters) {
  const OverflowBinopOp ovf_binop = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Block *a = m.NewBlock(), *b = m.NewBlock();
  OpIndex n = m.Emit(ovf_binop.op, m.Parameter(0), m.Parameter(1));
  m.Branch(m.Word32NotEqual(m.Projection(n, 1), m.Int32Constant(0)), a, b);
  m.Bind(a);
  m.Return(m.Projection(n, 0));
  m.Bind(b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(ovf_binop.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}

INSTANTIATE_TEST_SUITE_P(
    TurboshaftInstructionSelectorTest,
    TurboshaftInstructionSelectorBranchIfOverflowTest,
    ::testing::ValuesIn(kOverflowBinaryOperationsForBranchFusion));

// -----------------------------------------------------------------------------
// Miscellaneous.

TEST_F(TurboshaftInstructionSelectorTest, Word32Clz) {
  StreamBuilder m(this, MachineType::Uint32(), MachineType::Uint32());
  OpIndex const p0 = m.Parameter(0);
  OpIndex const n = m.Word32CountLeadingZeros(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Lzcnt, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

#if V8_ENABLE_WEBASSEMBLY
// SIMD.

TEST_F(TurboshaftInstructionSelectorTest, SIMDSplatZero) {
  // Test optimization for splat of contant 0.
  // {i8x16,i16x8,i32x4,i64x2}.splat(const(0)) -> v128.zero().
  // Optimizations for f32x4.splat and f64x2.splat not implemented since it
  // doesn't improve the codegen as much (same number of instructions).

#if 0
  // TODO(14108): introduce I64x2SplatI32Pair in Turboshaft for better codegen.
  {
    StreamBuilder m(this, MachineType::Simd128());
    OpIndex const splat =
        m.I64x2SplatI32Pair(m.Int32Constant(0), m.Int32Constant(0));
    m.Return(splat);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kIA32S128Zero, s[0]->arch_opcode());
    ASSERT_EQ(0U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
#endif

  {
    StreamBuilder m(this, MachineType::Simd128());
    OpIndex const splat = m.I32x4Splat(m.Int32Constant(0));
    m.Return(splat);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kIA32S128Zero, s[0]->arch_opcode());
    ASSERT_EQ(0U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Simd128());
    OpIndex const splat = m.I16x8Splat(m.Int32Constant(0));
    m.Return(splat);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kIA32S128Zero, s[0]->arch_opcode());
    ASSERT_EQ(0U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Simd128());
    OpIndex const splat = m.I8x16Splat(m.Int32Constant(0));
    m.Return(splat);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kIA32S128Zero, s[0]->arch_opcode());
    ASSERT_EQ(0U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

struct SwizzleConstants {
  uint8_t shuffle[kSimd128Size];
  bool omit_add;
};

static constexpr SwizzleConstants kSwizzleConstants[] = {
    {
        // all lanes < kSimd128Size
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        true,
    },
    {
        // lanes that are >= kSimd128Size have top bit set
        {12, 13, 14, 15, 0x90, 0x91, 0x92, 0x93, 0xA0, 0xA1, 0xA2, 0xA3, 0xFC,
         0xFD, 0xFE, 0xFF},
        true,
    },
    {
        {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
        false,
    },
};

using TurboshaftInstructionSelectorSIMDSwizzleConstantTest =
    TurboshaftInstructionSelectorTestWithParam<SwizzleConstants>;

TEST_P(TurboshaftInstructionSelectorSIMDSwizzleConstantTest,
       SimdSwizzleConstant) {
  // Test optimization of swizzle with constant indices.
  auto param = GetParam();
  StreamBuilder m(this, MachineType::Simd128(), MachineType::Simd128());
  OpIndex const c = m.Simd128Constant(param.shuffle);
  OpIndex swizzle = m.I8x16Swizzle(m.Parameter(0), c);
  m.Return(swizzle);
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  ASSERT_EQ(kIA32I8x16Swizzle, s[1]->arch_opcode());
  ASSERT_EQ(param.omit_add, s[1]->misc());
  ASSERT_EQ(1U, s[0]->OutputCount());
}

INSTANTIATE_TEST_SUITE_P(TurboshaftInstructionSelectorTest,
                         TurboshaftInstructionSelectorSIMDSwizzleConstantTest,
                         ::testing::ValuesIn(kSwizzleConstants));
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace v8::internal::compiler::turboshaft
