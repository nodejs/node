// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/instruction-selector-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// Immediates (random subset).
static const int32_t kImmediates[] = {
    kMinInt, -42, -1, 0,  1,  2,    3,      4,          5,
    6,       7,   8,  16, 42, 0xff, 0xffff, 0x0f0f0f0f, kMaxInt};

}  // namespace


TEST_F(InstructionSelectorTest, Int32AddWithParameter) {
  StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
  m.Return(m.Int32Add(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Add, s[0]->arch_opcode());
}


TEST_F(InstructionSelectorTest, Int32AddWithImmediate) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    {
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Int32Add(m.Parameter(0), m.Int32Constant(imm)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kIA32Add, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    }
    {
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Int32Add(m.Int32Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kIA32Add, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    }
  }
}


TEST_F(InstructionSelectorTest, Int32SubWithParameter) {
  StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
  m.Return(m.Int32Sub(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kIA32Sub, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Int32SubWithImmediate) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Int32Sub(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kIA32Sub, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
  }
}


// -----------------------------------------------------------------------------
// Conversions.


TEST_F(InstructionSelectorTest, ChangeFloat32ToFloat64WithParameter) {
  StreamBuilder m(this, kMachFloat32, kMachFloat64);
  m.Return(m.ChangeFloat32ToFloat64(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kSSECvtss2sd, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, TruncateFloat64ToFloat32WithParameter) {
  StreamBuilder m(this, kMachFloat64, kMachFloat32);
  m.Return(m.TruncateFloat64ToFloat32(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kSSECvtsd2ss, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


// -----------------------------------------------------------------------------
// Better left operand for commutative binops


TEST_F(InstructionSelectorTest, BetterLeftOperandTestAddBinop) {
  StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
  Node* param1 = m.Parameter(0);
  Node* param2 = m.Parameter(1);
  Node* add = m.Int32Add(param1, param2);
  m.Return(m.Int32Add(add, param1));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kIA32Add, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_TRUE(s[0]->InputAt(0)->IsUnallocated());
  EXPECT_EQ(s.ToVreg(param2), s.ToVreg(s[0]->InputAt(0)));
}


TEST_F(InstructionSelectorTest, BetterLeftOperandTestMulBinop) {
  StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
  Node* param1 = m.Parameter(0);
  Node* param2 = m.Parameter(1);
  Node* mul = m.Int32Mul(param1, param2);
  m.Return(m.Int32Mul(mul, param1));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kIA32Imul, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_TRUE(s[0]->InputAt(0)->IsUnallocated());
  EXPECT_EQ(s.ToVreg(param2), s.ToVreg(s[0]->InputAt(0)));
}


// -----------------------------------------------------------------------------
// Conversions.


TEST_F(InstructionSelectorTest, ChangeUint32ToFloat64WithParameter) {
  StreamBuilder m(this, kMachFloat64, kMachUint32);
  m.Return(m.ChangeUint32ToFloat64(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kSSEUint32ToFloat64, s[0]->arch_opcode());
}


// -----------------------------------------------------------------------------
// Loads and stores


namespace {

struct MemoryAccess {
  MachineType type;
  ArchOpcode load_opcode;
  ArchOpcode store_opcode;
};


std::ostream& operator<<(std::ostream& os, const MemoryAccess& memacc) {
  return os << memacc.type;
}


static const MemoryAccess kMemoryAccesses[] = {
    {kMachInt8, kIA32Movsxbl, kIA32Movb},
    {kMachUint8, kIA32Movzxbl, kIA32Movb},
    {kMachInt16, kIA32Movsxwl, kIA32Movw},
    {kMachUint16, kIA32Movzxwl, kIA32Movw},
    {kMachInt32, kIA32Movl, kIA32Movl},
    {kMachUint32, kIA32Movl, kIA32Movl},
    {kMachFloat32, kIA32Movss, kIA32Movss},
    {kMachFloat64, kIA32Movsd, kIA32Movsd}};

}  // namespace


typedef InstructionSelectorTestWithParam<MemoryAccess>
    InstructionSelectorMemoryAccessTest;


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, kMachPtr, kMachInt32);
  m.Return(m.Load(memacc.type, m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithImmediateBase) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, base, kImmediates) {
    StreamBuilder m(this, memacc.type, kMachPtr);
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


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, kImmediates) {
    StreamBuilder m(this, memacc.type, kMachPtr);
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
    if (index == 0) {
      ASSERT_EQ(1U, s[0]->InputCount());
    } else {
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    }
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, kMachInt32, kMachPtr, kMachInt32, memacc.type);
  m.Store(memacc.type, m.Parameter(0), m.Parameter(1), m.Parameter(2));
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
  EXPECT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(0U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithImmediateBase) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, base, kImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32, memacc.type);
    m.Store(memacc.type, m.Int32Constant(base), m.Parameter(0), m.Parameter(1));
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


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, kImmediates) {
    StreamBuilder m(this, kMachInt32, kMachPtr, memacc.type);
    m.Store(memacc.type, m.Parameter(0), m.Int32Constant(index),
            m.Parameter(1));
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
    if (index == 0) {
      ASSERT_EQ(2U, s[0]->InputCount());
    } else {
      ASSERT_EQ(3U, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
      EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    }
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessTest,
                        ::testing::ValuesIn(kMemoryAccesses));


// -----------------------------------------------------------------------------
// AddressingMode for loads and stores.


class AddressingModeUnitTest : public InstructionSelectorTest {
 public:
  AddressingModeUnitTest() : m(NULL) { Reset(); }
  ~AddressingModeUnitTest() { delete m; }

  void Run(Node* base, Node* index, AddressingMode mode) {
    Node* load = m->Load(kMachInt32, base, index);
    m->Store(kMachInt32, base, index, load);
    m->Return(m->Int32Constant(0));
    Stream s = m->Build();
    ASSERT_EQ(2U, s.size());
    EXPECT_EQ(mode, s[0]->addressing_mode());
    EXPECT_EQ(mode, s[1]->addressing_mode());
  }

  Node* zero;
  Node* null_ptr;
  Node* non_zero;
  Node* base_reg;   // opaque value to generate base as register
  Node* index_reg;  // opaque value to generate index as register
  Node* scales[4];
  StreamBuilder* m;

  void Reset() {
    delete m;
    m = new StreamBuilder(this, kMachInt32, kMachInt32, kMachInt32);
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


TEST_F(AddressingModeUnitTest, AddressingMode_MR) {
  Node* base = base_reg;
  Node* index = zero;
  Run(base, index, kMode_MR);
}


TEST_F(AddressingModeUnitTest, AddressingMode_MRI) {
  Node* base = base_reg;
  Node* index = non_zero;
  Run(base, index, kMode_MRI);
}


TEST_F(AddressingModeUnitTest, AddressingMode_MR1) {
  Node* base = base_reg;
  Node* index = index_reg;
  Run(base, index, kMode_MR1);
}


TEST_F(AddressingModeUnitTest, AddressingMode_MRN) {
  AddressingMode expected[] = {kMode_MR1, kMode_MR2, kMode_MR4, kMode_MR8};
  for (size_t i = 0; i < arraysize(scales); ++i) {
    Reset();
    Node* base = base_reg;
    Node* index = m->Int32Mul(index_reg, scales[i]);
    Run(base, index, expected[i]);
  }
}


TEST_F(AddressingModeUnitTest, AddressingMode_MR1I) {
  Node* base = base_reg;
  Node* index = m->Int32Add(index_reg, non_zero);
  Run(base, index, kMode_MR1I);
}


TEST_F(AddressingModeUnitTest, AddressingMode_MRNI) {
  AddressingMode expected[] = {kMode_MR1I, kMode_MR2I, kMode_MR4I, kMode_MR8I};
  for (size_t i = 0; i < arraysize(scales); ++i) {
    Reset();
    Node* base = base_reg;
    Node* index = m->Int32Add(m->Int32Mul(index_reg, scales[i]), non_zero);
    Run(base, index, expected[i]);
  }
}


TEST_F(AddressingModeUnitTest, AddressingMode_M1) {
  Node* base = null_ptr;
  Node* index = index_reg;
  Run(base, index, kMode_M1);
}


TEST_F(AddressingModeUnitTest, AddressingMode_MN) {
  AddressingMode expected[] = {kMode_M1, kMode_M2, kMode_M4, kMode_M8};
  for (size_t i = 0; i < arraysize(scales); ++i) {
    Reset();
    Node* base = null_ptr;
    Node* index = m->Int32Mul(index_reg, scales[i]);
    Run(base, index, expected[i]);
  }
}


TEST_F(AddressingModeUnitTest, AddressingMode_M1I) {
  Node* base = null_ptr;
  Node* index = m->Int32Add(index_reg, non_zero);
  Run(base, index, kMode_M1I);
}


TEST_F(AddressingModeUnitTest, AddressingMode_MNI) {
  AddressingMode expected[] = {kMode_M1I, kMode_M2I, kMode_M4I, kMode_M8I};
  for (size_t i = 0; i < arraysize(scales); ++i) {
    Reset();
    Node* base = null_ptr;
    Node* index = m->Int32Add(m->Int32Mul(index_reg, scales[i]), non_zero);
    Run(base, index, expected[i]);
  }
}


TEST_F(AddressingModeUnitTest, AddressingMode_MI) {
  Node* bases[] = {null_ptr, non_zero};
  Node* indices[] = {zero, non_zero};
  for (size_t i = 0; i < arraysize(bases); ++i) {
    for (size_t j = 0; j < arraysize(indices); ++j) {
      Reset();
      Node* base = bases[i];
      Node* index = indices[j];
      Run(base, index, kMode_MI);
    }
  }
}


// -----------------------------------------------------------------------------
// Multiplication.


namespace {

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
                                 {1, true, kMode_M1},
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

}  // namespace


typedef InstructionSelectorTestWithParam<MultParam> InstructionSelectorMultTest;


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
      return 2U;
    case kMode_M1:
    case kMode_M2:
    case kMode_M4:
    case kMode_M8:
      return 1U;
    default:
      UNREACHABLE();
      return 0U;
  }
}


static AddressingMode AddressingModeForAddMult(const MultParam& m) {
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
    default:
      UNREACHABLE();
      return kMode_None;
  }
}


TEST_P(InstructionSelectorMultTest, Mult32) {
  const MultParam m_param = GetParam();
  StreamBuilder m(this, kMachInt32, kMachInt32);
  Node* param = m.Parameter(0);
  Node* mult = m.Int32Mul(param, m.Int32Constant(m_param.value));
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


TEST_P(InstructionSelectorMultTest, MultAdd32) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    const MultParam m_param = GetParam();
    StreamBuilder m(this, kMachInt32, kMachInt32);
    Node* param = m.Parameter(0);
    Node* mult = m.Int32Add(m.Int32Mul(param, m.Int32Constant(m_param.value)),
                            m.Int32Constant(imm));
    m.Return(mult);
    Stream s = m.Build();
    if (m_param.lea_expected) {
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kIA32Lea, s[0]->arch_opcode());
      EXPECT_EQ(AddressingModeForAddMult(m_param), s[0]->addressing_mode());
      unsigned input_count = InputCountForLea(s[0]->addressing_mode());
      ASSERT_EQ(input_count, s[0]->InputCount());
      ASSERT_EQ(InstructionOperand::IMMEDIATE,
                s[0]->InputAt(input_count - 1)->kind());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(input_count - 1)));
    } else {
      ASSERT_EQ(2U, s.size());
      EXPECT_EQ(kIA32Imul, s[0]->arch_opcode());
      EXPECT_EQ(kIA32Add, s[1]->arch_opcode());
    }
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorMultTest,
                        ::testing::ValuesIn(kMultParams));


TEST_F(InstructionSelectorTest, Int32MulHigh) {
  StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Int32MulHigh(p0, p1);
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
