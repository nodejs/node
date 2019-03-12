// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "test/unittests/compiler/backend/instruction-selector-unittest.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

template <typename T>
struct MachInst {
  T constructor;
  const char* constructor_name;
  ArchOpcode arch_opcode;
  MachineType machine_type;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, const MachInst<T>& mi) {
  return os << mi.constructor_name;
}

typedef MachInst<Node* (RawMachineAssembler::*)(Node*)> MachInst1;
typedef MachInst<Node* (RawMachineAssembler::*)(Node*, Node*)> MachInst2;

// To avoid duplicated code IntCmp helper structure
// is created. It contains MachInst2 with two nodes and expected_size
// because different cmp instructions have different size.
struct IntCmp {
  MachInst2 mi;
  uint32_t expected_size;
};

struct FPCmp {
  MachInst2 mi;
  FlagsCondition cond;
};

const FPCmp kFPCmpInstructions[] = {
    {{&RawMachineAssembler::Float64Equal, "Float64Equal", kMipsCmpD,
      MachineType::Float64()},
     kEqual},
    {{&RawMachineAssembler::Float64LessThan, "Float64LessThan", kMipsCmpD,
      MachineType::Float64()},
     kUnsignedLessThan},
    {{&RawMachineAssembler::Float64LessThanOrEqual, "Float64LessThanOrEqual",
      kMipsCmpD, MachineType::Float64()},
     kUnsignedLessThanOrEqual},
    {{&RawMachineAssembler::Float64GreaterThan, "Float64GreaterThan", kMipsCmpD,
      MachineType::Float64()},
     kUnsignedLessThan},
    {{&RawMachineAssembler::Float64GreaterThanOrEqual,
      "Float64GreaterThanOrEqual", kMipsCmpD, MachineType::Float64()},
     kUnsignedLessThanOrEqual}};

struct Conversion {
  // The machine_type field in MachInst1 represents the destination type.
  MachInst1 mi;
  MachineType src_machine_type;
};


// ----------------------------------------------------------------------------
// Logical instructions.
// ----------------------------------------------------------------------------


const MachInst2 kLogicalInstructions[] = {
    {&RawMachineAssembler::WordAnd, "WordAnd", kMipsAnd, MachineType::Int16()},
    {&RawMachineAssembler::WordOr, "WordOr", kMipsOr, MachineType::Int16()},
    {&RawMachineAssembler::WordXor, "WordXor", kMipsXor, MachineType::Int16()},
    {&RawMachineAssembler::Word32And, "Word32And", kMipsAnd,
     MachineType::Int32()},
    {&RawMachineAssembler::Word32Or, "Word32Or", kMipsOr, MachineType::Int32()},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kMipsXor,
     MachineType::Int32()}};


// ----------------------------------------------------------------------------
// Shift instructions.
// ----------------------------------------------------------------------------


const MachInst2 kShiftInstructions[] = {
    {&RawMachineAssembler::WordShl, "WordShl", kMipsShl, MachineType::Int16()},
    {&RawMachineAssembler::WordShr, "WordShr", kMipsShr, MachineType::Int16()},
    {&RawMachineAssembler::WordSar, "WordSar", kMipsSar, MachineType::Int16()},
    {&RawMachineAssembler::WordRor, "WordRor", kMipsRor, MachineType::Int16()},
    {&RawMachineAssembler::Word32Shl, "Word32Shl", kMipsShl,
     MachineType::Int32()},
    {&RawMachineAssembler::Word32Shr, "Word32Shr", kMipsShr,
     MachineType::Int32()},
    {&RawMachineAssembler::Word32Sar, "Word32Sar", kMipsSar,
     MachineType::Int32()},
    {&RawMachineAssembler::Word32Ror, "Word32Ror", kMipsRor,
     MachineType::Int32()}};


// ----------------------------------------------------------------------------
// MUL/DIV instructions.
// ----------------------------------------------------------------------------


const MachInst2 kMulDivInstructions[] = {
    {&RawMachineAssembler::Int32Mul, "Int32Mul", kMipsMul,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32Div, "Int32Div", kMipsDiv,
     MachineType::Int32()},
    {&RawMachineAssembler::Uint32Div, "Uint32Div", kMipsDivU,
     MachineType::Uint32()},
    {&RawMachineAssembler::Float64Mul, "Float64Mul", kMipsMulD,
     MachineType::Float64()},
    {&RawMachineAssembler::Float64Div, "Float64Div", kMipsDivD,
     MachineType::Float64()}};


// ----------------------------------------------------------------------------
// MOD instructions.
// ----------------------------------------------------------------------------


const MachInst2 kModInstructions[] = {
    {&RawMachineAssembler::Int32Mod, "Int32Mod", kMipsMod,
     MachineType::Int32()},
    {&RawMachineAssembler::Uint32Mod, "Int32UMod", kMipsModU,
     MachineType::Int32()},
    {&RawMachineAssembler::Float64Mod, "Float64Mod", kMipsModD,
     MachineType::Float64()}};


// ----------------------------------------------------------------------------
// Arithmetic FPU instructions.
// ----------------------------------------------------------------------------


const MachInst2 kFPArithInstructions[] = {
    {&RawMachineAssembler::Float64Add, "Float64Add", kMipsAddD,
     MachineType::Float64()},
    {&RawMachineAssembler::Float64Sub, "Float64Sub", kMipsSubD,
     MachineType::Float64()}};


// ----------------------------------------------------------------------------
// IntArithTest instructions, two nodes.
// ----------------------------------------------------------------------------


const MachInst2 kAddSubInstructions[] = {
    {&RawMachineAssembler::Int32Add, "Int32Add", kMipsAdd,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32Sub, "Int32Sub", kMipsSub,
     MachineType::Int32()},
    {&RawMachineAssembler::Int32AddWithOverflow, "Int32AddWithOverflow",
     kMipsAddOvf, MachineType::Int32()},
    {&RawMachineAssembler::Int32SubWithOverflow, "Int32SubWithOverflow",
     kMipsSubOvf, MachineType::Int32()}};


// ----------------------------------------------------------------------------
// IntArithTest instructions, one node.
// ----------------------------------------------------------------------------


const MachInst1 kAddSubOneInstructions[] = {
    {&RawMachineAssembler::Int32Neg, "Int32Neg", kMipsSub,
     MachineType::Int32()},
    // TODO(dusmil): check this ...
    // {&RawMachineAssembler::WordEqual  , "WordEqual"  , kMipsTst,
    // MachineType::Int32()}
};


// ----------------------------------------------------------------------------
// Arithmetic compare instructions.
// ----------------------------------------------------------------------------


const IntCmp kCmpInstructions[] = {
    {{&RawMachineAssembler::WordEqual, "WordEqual", kMipsCmp,
      MachineType::Int16()},
     1U},
    {{&RawMachineAssembler::WordNotEqual, "WordNotEqual", kMipsCmp,
      MachineType::Int16()},
     1U},
    {{&RawMachineAssembler::Word32Equal, "Word32Equal", kMipsCmp,
      MachineType::Int32()},
     1U},
    {{&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kMipsCmp,
      MachineType::Int32()},
     1U},
    {{&RawMachineAssembler::Int32LessThan, "Int32LessThan", kMipsCmp,
      MachineType::Int32()},
     1U},
    {{&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
      kMipsCmp, MachineType::Int32()},
     1U},
    {{&RawMachineAssembler::Int32GreaterThan, "Int32GreaterThan", kMipsCmp,
      MachineType::Int32()},
     1U},
    {{&RawMachineAssembler::Int32GreaterThanOrEqual, "Int32GreaterThanOrEqual",
      kMipsCmp, MachineType::Int32()},
     1U},
    {{&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kMipsCmp,
      MachineType::Uint32()},
     1U},
    {{&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
      kMipsCmp, MachineType::Uint32()},
     1U}};


// ----------------------------------------------------------------------------
// Conversion instructions.
// ----------------------------------------------------------------------------

const Conversion kConversionInstructions[] = {
    // Conversion instructions are related to machine_operator.h:
    // FPU conversions:
    // Convert representation of integers between float64 and int32/uint32.
    // The precise rounding mode and handling of out of range inputs are *not*
    // defined for these operators, since they are intended only for use with
    // integers.
    // mips instruction: cvt_d_w
    {{&RawMachineAssembler::ChangeInt32ToFloat64, "ChangeInt32ToFloat64",
      kMipsCvtDW, MachineType::Float64()},
     MachineType::Int32()},

    // mips instruction: cvt_d_uw
    {{&RawMachineAssembler::ChangeUint32ToFloat64, "ChangeUint32ToFloat64",
      kMipsCvtDUw, MachineType::Float64()},
     MachineType::Int32()},

    // mips instruction: trunc_w_d
    {{&RawMachineAssembler::ChangeFloat64ToInt32, "ChangeFloat64ToInt32",
      kMipsTruncWD, MachineType::Float64()},
     MachineType::Int32()},

    // mips instruction: trunc_uw_d
    {{&RawMachineAssembler::ChangeFloat64ToUint32, "ChangeFloat64ToUint32",
      kMipsTruncUwD, MachineType::Float64()},
     MachineType::Int32()}};

const Conversion kFloat64RoundInstructions[] = {
    {{&RawMachineAssembler::Float64RoundUp, "Float64RoundUp", kMipsCeilWD,
      MachineType::Int32()},
     MachineType::Float64()},
    {{&RawMachineAssembler::Float64RoundDown, "Float64RoundDown", kMipsFloorWD,
      MachineType::Int32()},
     MachineType::Float64()},
    {{&RawMachineAssembler::Float64RoundTiesEven, "Float64RoundTiesEven",
      kMipsRoundWD, MachineType::Int32()},
     MachineType::Float64()},
    {{&RawMachineAssembler::Float64RoundTruncate, "Float64RoundTruncate",
      kMipsTruncWD, MachineType::Int32()},
     MachineType::Float64()}};

const Conversion kFloat32RoundInstructions[] = {
    {{&RawMachineAssembler::Float32RoundUp, "Float32RoundUp", kMipsCeilWS,
      MachineType::Int32()},
     MachineType::Float32()},
    {{&RawMachineAssembler::Float32RoundDown, "Float32RoundDown", kMipsFloorWS,
      MachineType::Int32()},
     MachineType::Float32()},
    {{&RawMachineAssembler::Float32RoundTiesEven, "Float32RoundTiesEven",
      kMipsRoundWS, MachineType::Int32()},
     MachineType::Float32()},
    {{&RawMachineAssembler::Float32RoundTruncate, "Float32RoundTruncate",
      kMipsTruncWS, MachineType::Int32()},
     MachineType::Float32()}};

}  // namespace


typedef InstructionSelectorTestWithParam<FPCmp> InstructionSelectorFPCmpTest;


TEST_P(InstructionSelectorFPCmpTest, Parameter) {
  const FPCmp cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), cmp.mi.machine_type,
                  cmp.mi.machine_type);
  m.Return((m.*cmp.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(cmp.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.cond, s[0]->flags_condition());
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorFPCmpTest,
                        ::testing::ValuesIn(kFPCmpInstructions));


// ----------------------------------------------------------------------------
// Arithmetic compare instructions integers.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<IntCmp> InstructionSelectorCmpTest;


TEST_P(InstructionSelectorCmpTest, Parameter) {
  const IntCmp cmp = GetParam();
  const MachineType type = cmp.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*cmp.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(cmp.expected_size, s.size());
  EXPECT_EQ(cmp.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorCmpTest,
                        ::testing::ValuesIn(kCmpInstructions));


// ----------------------------------------------------------------------------
// Shift instructions.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorShiftTest;


TEST_P(InstructionSelectorShiftTest, Immediate) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FORRANGE(int32_t, imm, 0,
                  ((1 << ElementSizeLog2Of(type.representation())) * 8) - 1) {
    StreamBuilder m(this, type, type);
    m.Return((m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorShiftTest,
                        ::testing::ValuesIn(kShiftInstructions));


TEST_F(InstructionSelectorTest, Word32ShrWithWord32AndWithImmediate) {
  // The available shift operand range is `0 <= imm < 32`, but we also test
  // that immediates outside this range are handled properly (modulo-32).
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t jnk = rng()->NextInt();
      jnk = (lsb > 0) ? (jnk >> (32 - lsb)) : 0;
      uint32_t msk = ((0xFFFFFFFFu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Shr(m.Word32And(m.Parameter(0), m.Int32Constant(msk)),
                           m.Int32Constant(shift)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kMipsExt, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t jnk = rng()->NextInt();
      jnk = (lsb > 0) ? (jnk >> (32 - lsb)) : 0;
      uint32_t msk = ((0xFFFFFFFFu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Shr(m.Word32And(m.Int32Constant(msk), m.Parameter(0)),
                           m.Int32Constant(shift)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kMipsExt, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word32ShlWithWord32And) {
  TRACED_FORRANGE(int32_t, shift, 0, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r =
        m.Word32Shl(m.Word32And(p0, m.Int32Constant((1 << (31 - shift)) - 1)),
                    m.Int32Constant(shift + 1));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsShl, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}

TEST_F(InstructionSelectorTest, Word32SarWithWord32Shl) {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      Node* const p0 = m.Parameter(0);
      Node* const r = m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(24)),
                                  m.Int32Constant(24));
      m.Return(r);
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kMipsSeb, s[0]->arch_opcode());
      ASSERT_EQ(1U, s[0]->InputCount());
      EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
      ASSERT_EQ(1U, s[0]->OutputCount());
      EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
    }
    {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      Node* const p0 = m.Parameter(0);
      Node* const r = m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(16)),
                                  m.Int32Constant(16));
      m.Return(r);
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kMipsSeh, s[0]->arch_opcode());
      ASSERT_EQ(1U, s[0]->InputCount());
      EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
      ASSERT_EQ(1U, s[0]->OutputCount());
      EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
    }
  }
}

// ----------------------------------------------------------------------------
// Logical instructions.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorLogicalTest;


TEST_P(InstructionSelectorLogicalTest, Parameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorLogicalTest,
                        ::testing::ValuesIn(kLogicalInstructions));


TEST_F(InstructionSelectorTest, Word32XorMinusOneWithParameter) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsNor, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Xor(m.Int32Constant(-1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsNor, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32XorMinusOneWithWord32Or) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Xor(m.Word32Or(m.Parameter(0), m.Parameter(0)),
                         m.Int32Constant(-1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsNor, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Xor(m.Int32Constant(-1),
                         m.Word32Or(m.Parameter(0), m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsNor, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32AndWithImmediateWithWord32Shr) {
  // The available shift operand range is `0 <= imm < 32`, but we also test
  // that immediates outside this range are handled properly (modulo-32).
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 31) {
      uint32_t msk = (1 << width) - 1;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32And(m.Word32Shr(m.Parameter(0), m.Int32Constant(shift)),
                           m.Int32Constant(msk)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kMipsExt, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      int32_t actual_width = (lsb + width > 32) ? (32 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, shift, -32, 63) {
    int32_t lsb = shift & 0x1F;
    TRACED_FORRANGE(int32_t, width, 1, 31) {
      uint32_t msk = (1 << width) - 1;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(
          m.Word32And(m.Int32Constant(msk),
                      m.Word32Shr(m.Parameter(0), m.Int32Constant(shift))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kMipsExt, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      int32_t actual_width = (lsb + width > 32) ? (32 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word32AndToClearBits) {
  TRACED_FORRANGE(int32_t, shift, 1, 31) {
    int32_t mask = ~((1 << shift) - 1);
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32And(m.Parameter(0), m.Int32Constant(mask)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsIns, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(shift, s.ToInt32(s[0]->InputAt(2)));
  }
  TRACED_FORRANGE(int32_t, shift, 1, 31) {
    int32_t mask = ~((1 << shift) - 1);
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32And(m.Int32Constant(mask), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsIns, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(shift, s.ToInt32(s[0]->InputAt(2)));
  }
}


// ----------------------------------------------------------------------------
// MUL/DIV instructions.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorMulDivTest;


TEST_P(InstructionSelectorMulDivTest, Parameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorMulDivTest,
                        ::testing::ValuesIn(kMulDivInstructions));


// ----------------------------------------------------------------------------
// MOD instructions.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst2> InstructionSelectorModTest;


TEST_P(InstructionSelectorModTest, Parameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorModTest,
                        ::testing::ValuesIn(kModInstructions));


// ----------------------------------------------------------------------------
// Floating point instructions.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorFPArithTest;


TEST_P(InstructionSelectorFPArithTest, Parameter) {
  const MachInst2 fpa = GetParam();
  StreamBuilder m(this, fpa.machine_type, fpa.machine_type, fpa.machine_type);
  m.Return((m.*fpa.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(fpa.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorFPArithTest,
                        ::testing::ValuesIn(kFPArithInstructions));


// ----------------------------------------------------------------------------
// Integer arithmetic.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorIntArithTwoTest;


TEST_P(InstructionSelectorIntArithTwoTest, Parameter) {
  const MachInst2 intpa = GetParam();
  StreamBuilder m(this, intpa.machine_type, intpa.machine_type,
                  intpa.machine_type);
  m.Return((m.*intpa.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(intpa.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorIntArithTwoTest,
                        ::testing::ValuesIn(kAddSubInstructions));


// ----------------------------------------------------------------------------
// One node.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MachInst1>
    InstructionSelectorIntArithOneTest;


TEST_P(InstructionSelectorIntArithOneTest, Parameter) {
  const MachInst1 intpa = GetParam();
  StreamBuilder m(this, intpa.machine_type, intpa.machine_type,
                  intpa.machine_type);
  m.Return((m.*intpa.constructor)(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(intpa.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorIntArithOneTest,
                        ::testing::ValuesIn(kAddSubOneInstructions));


// ----------------------------------------------------------------------------
// Conversions.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<Conversion>
    InstructionSelectorConversionTest;


TEST_P(InstructionSelectorConversionTest, Parameter) {
  const Conversion conv = GetParam();
  StreamBuilder m(this, conv.mi.machine_type, conv.src_machine_type);
  m.Return((m.*conv.mi.constructor)(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(conv.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorConversionTest,
                        ::testing::ValuesIn(kConversionInstructions));


typedef InstructionSelectorTestWithParam<Conversion>
    CombineChangeFloat64ToInt32WithRoundFloat64;

TEST_P(CombineChangeFloat64ToInt32WithRoundFloat64, Parameter) {
  {
    const Conversion conv = GetParam();
    StreamBuilder m(this, conv.mi.machine_type, conv.src_machine_type);
    m.Return(m.ChangeFloat64ToInt32((m.*conv.mi.constructor)(m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(conv.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        CombineChangeFloat64ToInt32WithRoundFloat64,
                        ::testing::ValuesIn(kFloat64RoundInstructions));


typedef InstructionSelectorTestWithParam<Conversion>
    CombineChangeFloat32ToInt32WithRoundFloat32;

TEST_P(CombineChangeFloat32ToInt32WithRoundFloat32, Parameter) {
  {
    const Conversion conv = GetParam();
    StreamBuilder m(this, conv.mi.machine_type, conv.src_machine_type);
    m.Return(m.ChangeFloat64ToInt32(
        m.ChangeFloat32ToFloat64((m.*conv.mi.constructor)(m.Parameter(0)))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(conv.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        CombineChangeFloat32ToInt32WithRoundFloat32,
                        ::testing::ValuesIn(kFloat32RoundInstructions));


TEST_F(InstructionSelectorTest, ChangeFloat64ToInt32OfChangeFloat32ToFloat64) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Float32());
    m.Return(m.ChangeFloat64ToInt32(m.ChangeFloat32ToFloat64(m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsTruncWS, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest,
       TruncateFloat64ToFloat32OfChangeInt32ToFloat64) {
  {
    StreamBuilder m(this, MachineType::Float32(), MachineType::Int32());
    m.Return(
        m.TruncateFloat64ToFloat32(m.ChangeInt32ToFloat64(m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsCvtSW, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    ASSERT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


// ----------------------------------------------------------------------------
// Loads and stores.
// ----------------------------------------------------------------------------

namespace {

struct MemoryAccess {
  MachineType type;
  ArchOpcode load_opcode;
  ArchOpcode store_opcode;
};


static const MemoryAccess kMemoryAccesses[] = {
    {MachineType::Int8(), kMipsLb, kMipsSb},
    {MachineType::Uint8(), kMipsLbu, kMipsSb},
    {MachineType::Int16(), kMipsLh, kMipsSh},
    {MachineType::Uint16(), kMipsLhu, kMipsSh},
    {MachineType::Int32(), kMipsLw, kMipsSw},
    {MachineType::Float32(), kMipsLwc1, kMipsSwc1},
    {MachineType::Float64(), kMipsLdc1, kMipsSdc1}};


struct MemoryAccessImm {
  MachineType type;
  ArchOpcode load_opcode;
  ArchOpcode store_opcode;
  bool (InstructionSelectorTest::Stream::*val_predicate)(
      const InstructionOperand*) const;
  const int32_t immediates[40];
};


std::ostream& operator<<(std::ostream& os, const MemoryAccessImm& acc) {
  return os << acc.type;
}


struct MemoryAccessImm1 {
  MachineType type;
  ArchOpcode load_opcode;
  ArchOpcode store_opcode;
  bool (InstructionSelectorTest::Stream::*val_predicate)(
      const InstructionOperand*) const;
  const int32_t immediates[5];
};


std::ostream& operator<<(std::ostream& os, const MemoryAccessImm1& acc) {
  return os << acc.type;
}

struct MemoryAccessImm2 {
  MachineType type;
  ArchOpcode store_opcode;
  ArchOpcode store_opcode_unaligned;
  bool (InstructionSelectorTest::Stream::*val_predicate)(
      const InstructionOperand*) const;
  const int32_t immediates[40];
};

std::ostream& operator<<(std::ostream& os, const MemoryAccessImm2& acc) {
  return os << acc.type;
}

// ----------------------------------------------------------------------------
// Loads and stores immediate values.
// ----------------------------------------------------------------------------


const MemoryAccessImm kMemoryAccessesImm[] = {
    {MachineType::Int8(),
     kMipsLb,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Uint8(),
     kMipsLbu,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Int16(),
     kMipsLh,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Uint16(),
     kMipsLhu,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Int32(),
     kMipsLw,
     kMipsSw,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Float32(),
     kMipsLwc1,
     kMipsSwc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Float64(),
     kMipsLdc1,
     kMipsSdc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}}};

const MemoryAccessImm1 kMemoryAccessImmMoreThan16bit[] = {
    {MachineType::Int8(),
     kMipsLb,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {MachineType::Uint8(),
     kMipsLbu,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {MachineType::Int16(),
     kMipsLh,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {MachineType::Uint16(),
     kMipsLhu,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {MachineType::Int32(),
     kMipsLw,
     kMipsSw,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {MachineType::Float32(),
     kMipsLwc1,
     kMipsSwc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-65000, -55000, 32777, 55000, 65000}},
    {MachineType::Float64(),
     kMipsLdc1,
     kMipsSdc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-65000, -55000, 32777, 55000, 65000}}};

const MemoryAccessImm2 kMemoryAccessesImmUnaligned[] = {
    {MachineType::Int16(),
     kMipsUsh,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91,
      -89,   -87,   -86,   -82,   -44,   -23,   -3,    0,    7,    10,
      39,    52,    69,    71,    91,    92,    107,   109,  115,  124,
      286,   655,   1362,  1569,  2587,  3067,  3096,  3462, 3510, 4095}},
    {MachineType::Int32(),
     kMipsUsw,
     kMipsSw,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91,
      -89,   -87,   -86,   -82,   -44,   -23,   -3,    0,    7,    10,
      39,    52,    69,    71,    91,    92,    107,   109,  115,  124,
      286,   655,   1362,  1569,  2587,  3067,  3096,  3462, 3510, 4095}},
    {MachineType::Float32(),
     kMipsUswc1,
     kMipsSwc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91,
      -89,   -87,   -86,   -82,   -44,   -23,   -3,    0,    7,    10,
      39,    52,    69,    71,    91,    92,    107,   109,  115,  124,
      286,   655,   1362,  1569,  2587,  3067,  3096,  3462, 3510, 4095}},
    {MachineType::Float64(),
     kMipsUsdc1,
     kMipsSdc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91,
      -89,   -87,   -86,   -82,   -44,   -23,   -3,    0,    7,    10,
      39,    52,    69,    71,    91,    92,    107,   109,  115,  124,
      286,   655,   1362,  1569,  2587,  3067,  3096,  3462, 3510, 4095}}};

}  // namespace


typedef InstructionSelectorTestWithParam<MemoryAccess>
    InstructionSelectorMemoryAccessTest;


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, MachineType::Pointer(),
                  MachineType::Int32());
  m.Return(m.Load(memacc.type, m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                  MachineType::Int32(), memacc.type);
  m.Store(memacc.type.representation(), m.Parameter(0), m.Parameter(1),
          kNoWriteBarrier);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessTest,
                        ::testing::ValuesIn(kMemoryAccesses));


// ----------------------------------------------------------------------------
// Load immediate.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MemoryAccessImm>
    InstructionSelectorMemoryAccessImmTest;


TEST_P(InstructionSelectorMemoryAccessImmTest, LoadWithImmediateIndex) {
  const MemoryAccessImm memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, memacc.type, MachineType::Pointer());
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE((s.*memacc.val_predicate)(s[0]->Output()));
  }
}


// ----------------------------------------------------------------------------
// Store immediate.
// ----------------------------------------------------------------------------


TEST_P(InstructionSelectorMemoryAccessImmTest, StoreWithImmediateIndex) {
  const MemoryAccessImm memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    memacc.type);
    m.Store(memacc.type.representation(), m.Parameter(0),
            m.Int32Constant(index), m.Parameter(1), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

TEST_P(InstructionSelectorMemoryAccessImmTest, StoreZero) {
  const MemoryAccessImm memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
    m.Store(memacc.type.representation(), m.Parameter(0),
            m.Int32Constant(index), m.Int32Constant(0), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(2)->kind());
    EXPECT_EQ(0, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessImmTest,
                        ::testing::ValuesIn(kMemoryAccessesImm));

typedef InstructionSelectorTestWithParam<MemoryAccessImm2>
    InstructionSelectorMemoryAccessUnalignedImmTest;

TEST_P(InstructionSelectorMemoryAccessUnalignedImmTest, StoreZero) {
  const MemoryAccessImm2 memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
    bool unaligned_store_supported =
        m.machine()->UnalignedStoreSupported(memacc.type.representation());
    m.UnalignedStore(memacc.type.representation(), m.Parameter(0),
                     m.Int32Constant(index), m.Int32Constant(0));
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(unaligned_store_supported ? memacc.store_opcode_unaligned
                                        : memacc.store_opcode,
              s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(2)->kind());
    EXPECT_EQ(0, s.ToInt64(s[0]->InputAt(2)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessUnalignedImmTest,
                        ::testing::ValuesIn(kMemoryAccessesImmUnaligned));

// ----------------------------------------------------------------------------
// Load/store offsets more than 16 bits.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MemoryAccessImm1>
    InstructionSelectorMemoryAccessImmMoreThan16bitTest;


TEST_P(InstructionSelectorMemoryAccessImmMoreThan16bitTest,
       LoadWithImmediateIndex) {
  const MemoryAccessImm1 memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, memacc.type, MachineType::Pointer());
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorMemoryAccessImmMoreThan16bitTest,
       StoreWithImmediateIndex) {
  const MemoryAccessImm1 memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    memacc.type);
    m.Store(memacc.type.representation(), m.Parameter(0),
            m.Int32Constant(index), m.Parameter(1), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.store_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessImmMoreThan16bitTest,
                        ::testing::ValuesIn(kMemoryAccessImmMoreThan16bit));


// ----------------------------------------------------------------------------
// kMipsTst testing.
// ----------------------------------------------------------------------------


TEST_F(InstructionSelectorTest, Word32EqualWithZero) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Parameter(0), m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Int32Constant(0), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word32Clz) {
  StreamBuilder m(this, MachineType::Uint32(), MachineType::Uint32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Word32Clz(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kMipsClz, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float32Abs) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float32Abs(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kMipsAbsS, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float64Abs) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float64Abs(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kMipsAbsD, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Float32AddWithFloat32Mul) {
  if (!IsMipsArchVariant(kMips32r2)) {
    return;
  }
  {
    StreamBuilder m(this, MachineType::Float32(), MachineType::Float32(),
                    MachineType::Float32(), MachineType::Float32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Float32Add(m.Float32Mul(p0, p1), p2);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsMaddS, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_FALSE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
  {
    StreamBuilder m(this, MachineType::Float32(), MachineType::Float32(),
                    MachineType::Float32(), MachineType::Float32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Float32Add(p0, m.Float32Mul(p1, p2));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsMaddS, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_FALSE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}

TEST_F(InstructionSelectorTest, Float64AddWithFloat64Mul) {
  if (!IsMipsArchVariant(kMips32r2)) {
    return;
  }
  {
    StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                    MachineType::Float64(), MachineType::Float64());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Float64Add(m.Float64Mul(p0, p1), p2);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsMaddD, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_FALSE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
  {
    StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                    MachineType::Float64(), MachineType::Float64());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Float64Add(p0, m.Float64Mul(p1, p2));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsMaddD, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_FALSE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}

TEST_F(InstructionSelectorTest, Float32SubWithFloat32Mul) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32(),
                  MachineType::Float32(), MachineType::Float32());
  if (!IsMipsArchVariant(kMips32r2)) {
    return;
  }
  {
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* n = nullptr;

    n = m.Float32Sub(m.Float32Mul(p1, p2), p0);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsMsubS, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_FALSE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}

TEST_F(InstructionSelectorTest, Float64SubWithFloat64Mul) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64(), MachineType::Float64());
  if (!IsMipsArchVariant(kMips32r2)) {
    return;
  }
  {
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* n = nullptr;

    n = m.Float64Sub(m.Float64Mul(p1, p2), p0);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsMsubD, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_FALSE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}

TEST_F(InstructionSelectorTest, Float64Max) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Float64Max(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kMipsFloat64Max, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float64Min) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Float64Min(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kMipsFloat64Min, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Word32ReverseBytes) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32ReverseBytes(m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kMipsByteSwap32, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
