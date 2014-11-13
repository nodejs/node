// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "test/unittests/compiler/instruction-selector-unittest.h"

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
      kMachFloat64},
     kUnorderedEqual},
    {{&RawMachineAssembler::Float64LessThan, "Float64LessThan", kMipsCmpD,
      kMachFloat64},
     kUnorderedLessThan},
    {{&RawMachineAssembler::Float64LessThanOrEqual, "Float64LessThanOrEqual",
      kMipsCmpD, kMachFloat64},
     kUnorderedLessThanOrEqual},
    {{&RawMachineAssembler::Float64GreaterThan, "Float64GreaterThan", kMipsCmpD,
      kMachFloat64},
     kUnorderedLessThan},
    {{&RawMachineAssembler::Float64GreaterThanOrEqual,
      "Float64GreaterThanOrEqual", kMipsCmpD, kMachFloat64},
     kUnorderedLessThanOrEqual}};

struct Conversion {
  // The machine_type field in MachInst1 represents the destination type.
  MachInst1 mi;
  MachineType src_machine_type;
};


// ----------------------------------------------------------------------------
// Logical instructions.
// ----------------------------------------------------------------------------


const MachInst2 kLogicalInstructions[] = {
    {&RawMachineAssembler::WordAnd, "WordAnd", kMipsAnd, kMachInt16},
    {&RawMachineAssembler::WordOr, "WordOr", kMipsOr, kMachInt16},
    {&RawMachineAssembler::WordXor, "WordXor", kMipsXor, kMachInt16},
    {&RawMachineAssembler::Word32And, "Word32And", kMipsAnd, kMachInt32},
    {&RawMachineAssembler::Word32Or, "Word32Or", kMipsOr, kMachInt32},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kMipsXor, kMachInt32}};


// ----------------------------------------------------------------------------
// Shift instructions.
// ----------------------------------------------------------------------------


const MachInst2 kShiftInstructions[] = {
    {&RawMachineAssembler::WordShl, "WordShl", kMipsShl, kMachInt16},
    {&RawMachineAssembler::WordShr, "WordShr", kMipsShr, kMachInt16},
    {&RawMachineAssembler::WordSar, "WordSar", kMipsSar, kMachInt16},
    {&RawMachineAssembler::WordRor, "WordRor", kMipsRor, kMachInt16},
    {&RawMachineAssembler::Word32Shl, "Word32Shl", kMipsShl, kMachInt32},
    {&RawMachineAssembler::Word32Shr, "Word32Shr", kMipsShr, kMachInt32},
    {&RawMachineAssembler::Word32Sar, "Word32Sar", kMipsSar, kMachInt32},
    {&RawMachineAssembler::Word32Ror, "Word32Ror", kMipsRor, kMachInt32}};


// ----------------------------------------------------------------------------
// MUL/DIV instructions.
// ----------------------------------------------------------------------------


const MachInst2 kMulDivInstructions[] = {
    {&RawMachineAssembler::Int32Mul, "Int32Mul", kMipsMul, kMachInt32},
    {&RawMachineAssembler::Int32Div, "Int32Div", kMipsDiv, kMachInt32},
    {&RawMachineAssembler::Uint32Div, "Uint32Div", kMipsDivU, kMachUint32},
    {&RawMachineAssembler::Float64Mul, "Float64Mul", kMipsMulD, kMachFloat64},
    {&RawMachineAssembler::Float64Div, "Float64Div", kMipsDivD, kMachFloat64}};


// ----------------------------------------------------------------------------
// MOD instructions.
// ----------------------------------------------------------------------------


const MachInst2 kModInstructions[] = {
    {&RawMachineAssembler::Int32Mod, "Int32Mod", kMipsMod, kMachInt32},
    {&RawMachineAssembler::Uint32Mod, "Int32UMod", kMipsModU, kMachInt32},
    {&RawMachineAssembler::Float64Mod, "Float64Mod", kMipsModD, kMachFloat64}};


// ----------------------------------------------------------------------------
// Arithmetic FPU instructions.
// ----------------------------------------------------------------------------


const MachInst2 kFPArithInstructions[] = {
    {&RawMachineAssembler::Float64Add, "Float64Add", kMipsAddD, kMachFloat64},
    {&RawMachineAssembler::Float64Sub, "Float64Sub", kMipsSubD, kMachFloat64}};


// ----------------------------------------------------------------------------
// IntArithTest instructions, two nodes.
// ----------------------------------------------------------------------------


const MachInst2 kAddSubInstructions[] = {
    {&RawMachineAssembler::Int32Add, "Int32Add", kMipsAdd, kMachInt32},
    {&RawMachineAssembler::Int32Sub, "Int32Sub", kMipsSub, kMachInt32},
    {&RawMachineAssembler::Int32AddWithOverflow, "Int32AddWithOverflow",
     kMipsAddOvf, kMachInt32},
    {&RawMachineAssembler::Int32SubWithOverflow, "Int32SubWithOverflow",
     kMipsSubOvf, kMachInt32}};


// ----------------------------------------------------------------------------
// IntArithTest instructions, one node.
// ----------------------------------------------------------------------------


const MachInst1 kAddSubOneInstructions[] = {
    {&RawMachineAssembler::Int32Neg, "Int32Neg", kMipsSub, kMachInt32},
    // TODO(dusmil): check this ...
    // {&RawMachineAssembler::WordEqual  , "WordEqual"  , kMipsTst, kMachInt32}
};


// ----------------------------------------------------------------------------
// Arithmetic compare instructions.
// ----------------------------------------------------------------------------


const IntCmp kCmpInstructions[] = {
    {{&RawMachineAssembler::WordEqual, "WordEqual", kMipsCmp, kMachInt16}, 1U},
    {{&RawMachineAssembler::WordNotEqual, "WordNotEqual", kMipsCmp, kMachInt16},
     1U},
    {{&RawMachineAssembler::Word32Equal, "Word32Equal", kMipsCmp, kMachInt32},
     1U},
    {{&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kMipsCmp,
      kMachInt32},
     1U},
    {{&RawMachineAssembler::Int32LessThan, "Int32LessThan", kMipsCmp,
      kMachInt32},
     1U},
    {{&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
      kMipsCmp, kMachInt32},
     1U},
    {{&RawMachineAssembler::Int32GreaterThan, "Int32GreaterThan", kMipsCmp,
      kMachInt32},
     1U},
    {{&RawMachineAssembler::Int32GreaterThanOrEqual, "Int32GreaterThanOrEqual",
      kMipsCmp, kMachInt32},
     1U},
    {{&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kMipsCmp,
      kMachUint32},
     1U},
    {{&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
      kMipsCmp, kMachUint32},
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
      kMipsCvtDW, kMachFloat64},
     kMachInt32},

    // mips instruction: cvt_d_uw
    {{&RawMachineAssembler::ChangeUint32ToFloat64, "ChangeUint32ToFloat64",
      kMipsCvtDUw, kMachFloat64},
     kMachInt32},

    // mips instruction: trunc_w_d
    {{&RawMachineAssembler::ChangeFloat64ToInt32, "ChangeFloat64ToInt32",
      kMipsTruncWD, kMachFloat64},
     kMachInt32},

    // mips instruction: trunc_uw_d
    {{&RawMachineAssembler::ChangeFloat64ToUint32, "ChangeFloat64ToUint32",
      kMipsTruncUwD, kMachFloat64},
     kMachInt32}};

}  // namespace


typedef InstructionSelectorTestWithParam<FPCmp> InstructionSelectorFPCmpTest;


TEST_P(InstructionSelectorFPCmpTest, Parameter) {
  const FPCmp cmp = GetParam();
  StreamBuilder m(this, kMachInt32, cmp.mi.machine_type, cmp.mi.machine_type);
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
  TRACED_FORRANGE(int32_t, imm, 0, (ElementSizeOf(type) * 8) - 1) {
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
    {kMachInt8, kMipsLb, kMipsSb},
    {kMachUint8, kMipsLbu, kMipsSb},
    {kMachInt16, kMipsLh, kMipsSh},
    {kMachUint16, kMipsLhu, kMipsSh},
    {kMachInt32, kMipsLw, kMipsSw},
    {kRepFloat32, kMipsLwc1, kMipsSwc1},
    {kRepFloat64, kMipsLdc1, kMipsSdc1}};


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


// ----------------------------------------------------------------------------
// Loads and stores immediate values.
// ----------------------------------------------------------------------------


const MemoryAccessImm kMemoryAccessesImm[] = {
    {kMachInt8,
     kMipsLb,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {kMachUint8,
     kMipsLbu,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {kMachInt16,
     kMipsLh,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {kMachUint16,
     kMipsLhu,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {kMachInt32,
     kMipsLw,
     kMipsSw,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {kMachFloat32,
     kMipsLwc1,
     kMipsSwc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {kMachFloat64,
     kMipsLdc1,
     kMipsSdc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}}};


const MemoryAccessImm1 kMemoryAccessImmMoreThan16bit[] = {
    {kMachInt8,
     kMipsLb,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {kMachInt8,
     kMipsLbu,
     kMipsSb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {kMachInt16,
     kMipsLh,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {kMachInt16,
     kMipsLhu,
     kMipsSh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {kMachInt32,
     kMipsLw,
     kMipsSw,
     &InstructionSelectorTest::Stream::IsInteger,
     {-65000, -55000, 32777, 55000, 65000}},
    {kMachFloat32,
     kMipsLwc1,
     kMipsSwc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-65000, -55000, 32777, 55000, 65000}},
    {kMachFloat64,
     kMipsLdc1,
     kMipsSdc1,
     &InstructionSelectorTest::Stream::IsDouble,
     {-65000, -55000, 32777, 55000, 65000}}};

}  // namespace


typedef InstructionSelectorTestWithParam<MemoryAccess>
    InstructionSelectorMemoryAccessTest;


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, kMachPtr, kMachInt32);
  m.Return(m.Load(memacc.type, m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.load_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, kMachInt32, kMachPtr, kMachInt32, memacc.type);
  m.Store(memacc.type, m.Parameter(0), m.Parameter(1));
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
    StreamBuilder m(this, memacc.type, kMachPtr);
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
    StreamBuilder m(this, kMachInt32, kMachPtr, memacc.type);
    m.Store(memacc.type, m.Parameter(0), m.Int32Constant(index),
            m.Parameter(1));
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


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessImmTest,
                        ::testing::ValuesIn(kMemoryAccessesImm));


// ----------------------------------------------------------------------------
// Load/store offsets more than 16 bits.
// ----------------------------------------------------------------------------


typedef InstructionSelectorTestWithParam<MemoryAccessImm1>
    InstructionSelectorMemoryAccessImmMoreThan16bitTest;


TEST_P(InstructionSelectorMemoryAccessImmMoreThan16bitTest,
       LoadWithImmediateIndex) {
  const MemoryAccessImm1 memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, memacc.type, kMachPtr);
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    // kMipsAdd is expected opcode.
    // size more than 16 bits wide.
    EXPECT_EQ(kMipsAdd, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorMemoryAccessImmMoreThan16bitTest,
       StoreWithImmediateIndex) {
  const MemoryAccessImm1 memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, kMachInt32, kMachPtr, memacc.type);
    m.Store(memacc.type, m.Parameter(0), m.Int32Constant(index),
            m.Parameter(1));
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(2U, s.size());
    // kMipsAdd is expected opcode
    // size more than 16 bits wide
    EXPECT_EQ(kMipsAdd, s[0]->arch_opcode());
    EXPECT_EQ(kMode_None, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
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
    StreamBuilder m(this, kMachInt32, kMachInt32);
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
    StreamBuilder m(this, kMachInt32, kMachInt32);
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
