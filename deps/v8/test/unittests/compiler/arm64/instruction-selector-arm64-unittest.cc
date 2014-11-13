// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/instruction-selector-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

typedef RawMachineAssembler::Label MLabel;

template <typename T>
struct MachInst {
  T constructor;
  const char* constructor_name;
  ArchOpcode arch_opcode;
  MachineType machine_type;
};

typedef MachInst<Node* (RawMachineAssembler::*)(Node*)> MachInst1;
typedef MachInst<Node* (RawMachineAssembler::*)(Node*, Node*)> MachInst2;


template <typename T>
std::ostream& operator<<(std::ostream& os, const MachInst<T>& mi) {
  return os << mi.constructor_name;
}


struct Shift {
  MachInst2 mi;
  AddressingMode mode;
};


std::ostream& operator<<(std::ostream& os, const Shift& shift) {
  return os << shift.mi;
}


// Helper to build Int32Constant or Int64Constant depending on the given
// machine type.
Node* BuildConstant(InstructionSelectorTest::StreamBuilder& m, MachineType type,
                    int64_t value) {
  switch (type) {
    case kMachInt32:
      return m.Int32Constant(value);
      break;

    case kMachInt64:
      return m.Int64Constant(value);
      break;

    default:
      UNIMPLEMENTED();
  }
  return NULL;
}


// ARM64 logical instructions.
static const MachInst2 kLogicalInstructions[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64And32, kMachInt32},
    {&RawMachineAssembler::Word64And, "Word64And", kArm64And, kMachInt64},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArm64Or32, kMachInt32},
    {&RawMachineAssembler::Word64Or, "Word64Or", kArm64Or, kMachInt64},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArm64Eor32, kMachInt32},
    {&RawMachineAssembler::Word64Xor, "Word64Xor", kArm64Eor, kMachInt64}};


// ARM64 logical immediates: contiguous set bits, rotated about a power of two
// sized block. The block is then duplicated across the word. Below is a random
// subset of the 32-bit immediates.
static const uint32_t kLogical32Immediates[] = {
    0x00000002, 0x00000003, 0x00000070, 0x00000080, 0x00000100, 0x000001c0,
    0x00000300, 0x000007e0, 0x00003ffc, 0x00007fc0, 0x0003c000, 0x0003f000,
    0x0003ffc0, 0x0003fff8, 0x0007ff00, 0x0007ffe0, 0x000e0000, 0x001e0000,
    0x001ffffc, 0x003f0000, 0x003f8000, 0x00780000, 0x007fc000, 0x00ff0000,
    0x01800000, 0x01800180, 0x01f801f8, 0x03fe0000, 0x03ffffc0, 0x03fffffc,
    0x06000000, 0x07fc0000, 0x07ffc000, 0x07ffffc0, 0x07ffffe0, 0x0ffe0ffe,
    0x0ffff800, 0x0ffffff0, 0x0fffffff, 0x18001800, 0x1f001f00, 0x1f801f80,
    0x30303030, 0x3ff03ff0, 0x3ff83ff8, 0x3fff0000, 0x3fff8000, 0x3fffffc0,
    0x70007000, 0x7f7f7f7f, 0x7fc00000, 0x7fffffc0, 0x8000001f, 0x800001ff,
    0x81818181, 0x9fff9fff, 0xc00007ff, 0xc0ffffff, 0xdddddddd, 0xe00001ff,
    0xe00003ff, 0xe007ffff, 0xefffefff, 0xf000003f, 0xf001f001, 0xf3fff3ff,
    0xf800001f, 0xf80fffff, 0xf87ff87f, 0xfbfbfbfb, 0xfc00001f, 0xfc0000ff,
    0xfc0001ff, 0xfc03fc03, 0xfe0001ff, 0xff000001, 0xff03ff03, 0xff800000,
    0xff800fff, 0xff801fff, 0xff87ffff, 0xffc0003f, 0xffc007ff, 0xffcfffcf,
    0xffe00003, 0xffe1ffff, 0xfff0001f, 0xfff07fff, 0xfff80007, 0xfff87fff,
    0xfffc00ff, 0xfffe07ff, 0xffff00ff, 0xffffc001, 0xfffff007, 0xfffff3ff,
    0xfffff807, 0xfffff9ff, 0xfffffc0f, 0xfffffeff};


// Random subset of 64-bit logical immediates.
static const uint64_t kLogical64Immediates[] = {
    0x0000000000000001, 0x0000000000000002, 0x0000000000000003,
    0x0000000000000070, 0x0000000000000080, 0x0000000000000100,
    0x00000000000001c0, 0x0000000000000300, 0x0000000000000600,
    0x00000000000007e0, 0x0000000000003ffc, 0x0000000000007fc0,
    0x0000000600000000, 0x0000003ffffffffc, 0x000000f000000000,
    0x000001f800000000, 0x0003fc0000000000, 0x0003fc000003fc00,
    0x0003ffffffc00000, 0x0003ffffffffffc0, 0x0006000000060000,
    0x003ffffffffc0000, 0x0180018001800180, 0x01f801f801f801f8,
    0x0600000000000000, 0x1000000010000000, 0x1000100010001000,
    0x1010101010101010, 0x1111111111111111, 0x1f001f001f001f00,
    0x1f1f1f1f1f1f1f1f, 0x1ffffffffffffffe, 0x3ffc3ffc3ffc3ffc,
    0x5555555555555555, 0x7f7f7f7f7f7f7f7f, 0x8000000000000000,
    0x8000001f8000001f, 0x8181818181818181, 0x9999999999999999,
    0x9fff9fff9fff9fff, 0xaaaaaaaaaaaaaaaa, 0xdddddddddddddddd,
    0xe0000000000001ff, 0xf800000000000000, 0xf8000000000001ff,
    0xf807f807f807f807, 0xfefefefefefefefe, 0xfffefffefffefffe,
    0xfffff807fffff807, 0xfffff9fffffff9ff, 0xfffffc0ffffffc0f,
    0xfffffc0fffffffff, 0xfffffefffffffeff, 0xfffffeffffffffff,
    0xffffff8000000000, 0xfffffffefffffffe, 0xffffffffefffffff,
    0xfffffffff9ffffff, 0xffffffffff800000, 0xffffffffffffc0ff,
    0xfffffffffffffffe};


// ARM64 arithmetic instructions.
struct AddSub {
  MachInst2 mi;
  ArchOpcode negate_arch_opcode;
};


std::ostream& operator<<(std::ostream& os, const AddSub& op) {
  return os << op.mi;
}


static const AddSub kAddSubInstructions[] = {
    {{&RawMachineAssembler::Int32Add, "Int32Add", kArm64Add32, kMachInt32},
     kArm64Sub32},
    {{&RawMachineAssembler::Int64Add, "Int64Add", kArm64Add, kMachInt64},
     kArm64Sub},
    {{&RawMachineAssembler::Int32Sub, "Int32Sub", kArm64Sub32, kMachInt32},
     kArm64Add32},
    {{&RawMachineAssembler::Int64Sub, "Int64Sub", kArm64Sub, kMachInt64},
     kArm64Add}};


// ARM64 Add/Sub immediates: 12-bit immediate optionally shifted by 12.
// Below is a combination of a random subset and some edge values.
static const int32_t kAddSubImmediates[] = {
    0,        1,        69,       493,      599,      701,      719,
    768,      818,      842,      945,      1246,     1286,     1429,
    1669,     2171,     2179,     2182,     2254,     2334,     2338,
    2343,     2396,     2449,     2610,     2732,     2855,     2876,
    2944,     3377,     3458,     3475,     3476,     3540,     3574,
    3601,     3813,     3871,     3917,     4095,     4096,     16384,
    364544,   462848,   970752,   1523712,  1863680,  2363392,  3219456,
    3280896,  4247552,  4526080,  4575232,  4960256,  5505024,  5894144,
    6004736,  6193152,  6385664,  6795264,  7114752,  7233536,  7348224,
    7499776,  7573504,  7729152,  8634368,  8937472,  9465856,  10354688,
    10682368, 11059200, 11460608, 13168640, 13176832, 14336000, 15028224,
    15597568, 15892480, 16773120};


// ARM64 flag setting data processing instructions.
static const MachInst2 kDPFlagSetInstructions[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64Tst32, kMachInt32},
    {&RawMachineAssembler::Int32Add, "Int32Add", kArm64Cmn32, kMachInt32},
    {&RawMachineAssembler::Int32Sub, "Int32Sub", kArm64Cmp32, kMachInt32},
    {&RawMachineAssembler::Word64And, "Word64And", kArm64Tst, kMachInt64}};


// ARM64 arithmetic with overflow instructions.
static const MachInst2 kOvfAddSubInstructions[] = {
    {&RawMachineAssembler::Int32AddWithOverflow, "Int32AddWithOverflow",
     kArm64Add32, kMachInt32},
    {&RawMachineAssembler::Int32SubWithOverflow, "Int32SubWithOverflow",
     kArm64Sub32, kMachInt32}};


// ARM64 shift instructions.
static const Shift kShiftInstructions[] = {
    {{&RawMachineAssembler::Word32Shl, "Word32Shl", kArm64Lsl32, kMachInt32},
     kMode_Operand2_R_LSL_I},
    {{&RawMachineAssembler::Word64Shl, "Word64Shl", kArm64Lsl, kMachInt64},
     kMode_Operand2_R_LSL_I},
    {{&RawMachineAssembler::Word32Shr, "Word32Shr", kArm64Lsr32, kMachInt32},
     kMode_Operand2_R_LSR_I},
    {{&RawMachineAssembler::Word64Shr, "Word64Shr", kArm64Lsr, kMachInt64},
     kMode_Operand2_R_LSR_I},
    {{&RawMachineAssembler::Word32Sar, "Word32Sar", kArm64Asr32, kMachInt32},
     kMode_Operand2_R_ASR_I},
    {{&RawMachineAssembler::Word64Sar, "Word64Sar", kArm64Asr, kMachInt64},
     kMode_Operand2_R_ASR_I},
    {{&RawMachineAssembler::Word32Ror, "Word32Ror", kArm64Ror32, kMachInt32},
     kMode_Operand2_R_ROR_I},
    {{&RawMachineAssembler::Word64Ror, "Word64Ror", kArm64Ror, kMachInt64},
     kMode_Operand2_R_ROR_I}};


// ARM64 Mul/Div instructions.
static const MachInst2 kMulDivInstructions[] = {
    {&RawMachineAssembler::Int32Mul, "Int32Mul", kArm64Mul32, kMachInt32},
    {&RawMachineAssembler::Int64Mul, "Int64Mul", kArm64Mul, kMachInt64},
    {&RawMachineAssembler::Int32Div, "Int32Div", kArm64Idiv32, kMachInt32},
    {&RawMachineAssembler::Int64Div, "Int64Div", kArm64Idiv, kMachInt64},
    {&RawMachineAssembler::Uint32Div, "Uint32Div", kArm64Udiv32, kMachInt32},
    {&RawMachineAssembler::Uint64Div, "Uint64Div", kArm64Udiv, kMachInt64}};


// ARM64 FP arithmetic instructions.
static const MachInst2 kFPArithInstructions[] = {
    {&RawMachineAssembler::Float64Add, "Float64Add", kArm64Float64Add,
     kMachFloat64},
    {&RawMachineAssembler::Float64Sub, "Float64Sub", kArm64Float64Sub,
     kMachFloat64},
    {&RawMachineAssembler::Float64Mul, "Float64Mul", kArm64Float64Mul,
     kMachFloat64},
    {&RawMachineAssembler::Float64Div, "Float64Div", kArm64Float64Div,
     kMachFloat64}};


struct FPCmp {
  MachInst2 mi;
  FlagsCondition cond;
};


std::ostream& operator<<(std::ostream& os, const FPCmp& cmp) {
  return os << cmp.mi;
}


// ARM64 FP comparison instructions.
static const FPCmp kFPCmpInstructions[] = {
    {{&RawMachineAssembler::Float64Equal, "Float64Equal", kArm64Float64Cmp,
      kMachFloat64},
     kUnorderedEqual},
    {{&RawMachineAssembler::Float64LessThan, "Float64LessThan",
      kArm64Float64Cmp, kMachFloat64},
     kUnorderedLessThan},
    {{&RawMachineAssembler::Float64LessThanOrEqual, "Float64LessThanOrEqual",
      kArm64Float64Cmp, kMachFloat64},
     kUnorderedLessThanOrEqual}};


struct Conversion {
  // The machine_type field in MachInst1 represents the destination type.
  MachInst1 mi;
  MachineType src_machine_type;
};


std::ostream& operator<<(std::ostream& os, const Conversion& conv) {
  return os << conv.mi;
}


// ARM64 type conversion instructions.
static const Conversion kConversionInstructions[] = {
    {{&RawMachineAssembler::ChangeFloat32ToFloat64, "ChangeFloat32ToFloat64",
      kArm64Float32ToFloat64, kMachFloat64},
     kMachFloat32},
    {{&RawMachineAssembler::TruncateFloat64ToFloat32,
      "TruncateFloat64ToFloat32", kArm64Float64ToFloat32, kMachFloat32},
     kMachFloat64},
    {{&RawMachineAssembler::ChangeInt32ToInt64, "ChangeInt32ToInt64",
      kArm64Sxtw, kMachInt64},
     kMachInt32},
    {{&RawMachineAssembler::ChangeUint32ToUint64, "ChangeUint32ToUint64",
      kArm64Mov32, kMachUint64},
     kMachUint32},
    {{&RawMachineAssembler::TruncateInt64ToInt32, "TruncateInt64ToInt32",
      kArm64Mov32, kMachInt32},
     kMachInt64},
    {{&RawMachineAssembler::ChangeInt32ToFloat64, "ChangeInt32ToFloat64",
      kArm64Int32ToFloat64, kMachFloat64},
     kMachInt32},
    {{&RawMachineAssembler::ChangeUint32ToFloat64, "ChangeUint32ToFloat64",
      kArm64Uint32ToFloat64, kMachFloat64},
     kMachUint32},
    {{&RawMachineAssembler::ChangeFloat64ToInt32, "ChangeFloat64ToInt32",
      kArm64Float64ToInt32, kMachInt32},
     kMachFloat64},
    {{&RawMachineAssembler::ChangeFloat64ToUint32, "ChangeFloat64ToUint32",
      kArm64Float64ToUint32, kMachUint32},
     kMachFloat64}};

}  // namespace


// -----------------------------------------------------------------------------
// Logical instructions.


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


TEST_P(InstructionSelectorLogicalTest, Immediate) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  // TODO(all): Add support for testing 64-bit immediates.
  if (type == kMachInt32) {
    // Immediate on the right.
    TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
      StreamBuilder m(this, type, type);
      m.Return((m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }

    // Immediate on the left; all logical ops should commute.
    TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
      StreamBuilder m(this, type, type);
      m.Return((m.*dpi.constructor)(m.Int32Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_P(InstructionSelectorLogicalTest, ShiftByImmediate) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test 64-bit shifted operands with 64-bit instructions.
    if (shift.mi.machine_type != type) continue;

    TRACED_FORRANGE(int, imm, 0, ((type == kMachInt32) ? 31 : 63)) {
      StreamBuilder m(this, type, type, type);
      m.Return((m.*dpi.constructor)(
          m.Parameter(0),
          (m.*shift.mi.constructor)(m.Parameter(1),
                                    BuildConstant(m, type, imm))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }

    TRACED_FORRANGE(int, imm, 0, ((type == kMachInt32) ? 31 : 63)) {
      StreamBuilder m(this, type, type, type);
      m.Return((m.*dpi.constructor)(
          (m.*shift.mi.constructor)(m.Parameter(1),
                                    BuildConstant(m, type, imm)),
          m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorLogicalTest,
                        ::testing::ValuesIn(kLogicalInstructions));


// -----------------------------------------------------------------------------
// Add and Sub instructions.

typedef InstructionSelectorTestWithParam<AddSub> InstructionSelectorAddSubTest;


TEST_P(InstructionSelectorAddSubTest, Parameter) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*dpi.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorAddSubTest, ImmediateOnRight) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    m.Return(
        (m.*dpi.mi.constructor)(m.Parameter(0), BuildConstant(m, type, imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorAddSubTest, NegImmediateOnRight) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    if (imm == 0) continue;
    StreamBuilder m(this, type, type);
    m.Return(
        (m.*dpi.mi.constructor)(m.Parameter(0), BuildConstant(m, type, -imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.negate_arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorAddSubTest, ShiftByImmediateOnRight) {
  const AddSub dpi = GetParam();
  const MachineType type = dpi.mi.machine_type;
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test 64-bit shifted operands with 64-bit instructions.
    if (shift.mi.machine_type != type) continue;

    if ((shift.mi.arch_opcode == kArm64Ror32) ||
        (shift.mi.arch_opcode == kArm64Ror)) {
      // Not supported by add/sub instructions.
      continue;
    }

    TRACED_FORRANGE(int, imm, 0, ((type == kMachInt32) ? 31 : 63)) {
      StreamBuilder m(this, type, type, type);
      m.Return((m.*dpi.mi.constructor)(
          m.Parameter(0),
          (m.*shift.mi.constructor)(m.Parameter(1),
                                    BuildConstant(m, type, imm))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.mi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorAddSubTest,
                        ::testing::ValuesIn(kAddSubInstructions));


TEST_F(InstructionSelectorTest, AddImmediateOnLeft) {
  {
    // 32-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Int32Add(m.Int32Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
  {
    // 64-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      StreamBuilder m(this, kMachInt64, kMachInt64);
      m.Return(m.Int64Add(m.Int64Constant(imm), m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_F(InstructionSelectorTest, SubZeroOnLeft) {
  // Subtraction with zero on the left maps to Neg.
  {
    // 32-bit subtract.
    StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
    m.Return(m.Int32Sub(m.Int32Constant(0), m.Parameter(0)));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Neg32, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    // 64-bit subtract.
    StreamBuilder m(this, kMachInt64, kMachInt64, kMachInt64);
    m.Return(m.Int64Sub(m.Int64Constant(0), m.Parameter(0)));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Neg, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, AddNegImmediateOnLeft) {
  {
    // 32-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      if (imm == 0) continue;
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Int32Add(m.Int32Constant(-imm), m.Parameter(0)));
      Stream s = m.Build();

      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Sub32, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
  {
    // 64-bit add.
    TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
      if (imm == 0) continue;
      StreamBuilder m(this, kMachInt64, kMachInt64);
      m.Return(m.Int64Add(m.Int64Constant(-imm), m.Parameter(0)));
      Stream s = m.Build();

      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Sub, s[0]->arch_opcode());
      ASSERT_EQ(2U, s[0]->InputCount());
      ASSERT_TRUE(s[0]->InputAt(1)->IsImmediate());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_F(InstructionSelectorTest, AddShiftByImmediateOnLeft) {
  // 32-bit add.
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test relevant shifted operands.
    if (shift.mi.machine_type != kMachInt32) continue;
    if (shift.mi.arch_opcode == kArm64Ror32) continue;

    TRACED_FORRANGE(int, imm, 0, 31) {
      StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
      m.Return((m.Int32Add)(
          (m.*shift.mi.constructor)(m.Parameter(1), m.Int32Constant(imm)),
          m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }

  // 64-bit add.
  TRACED_FOREACH(Shift, shift, kShiftInstructions) {
    // Only test relevant shifted operands.
    if (shift.mi.machine_type != kMachInt64) continue;
    if (shift.mi.arch_opcode == kArm64Ror) continue;

    TRACED_FORRANGE(int, imm, 0, 63) {
      StreamBuilder m(this, kMachInt64, kMachInt64, kMachInt64);
      m.Return((m.Int64Add)(
          (m.*shift.mi.constructor)(m.Parameter(1), m.Int64Constant(imm)),
          m.Parameter(0)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Add, s[0]->arch_opcode());
      EXPECT_EQ(shift.mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


// -----------------------------------------------------------------------------
// Data processing controlled branches.


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorDPFlagSetTest;


TEST_P(InstructionSelectorDPFlagSetTest, BranchWithParameters) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  MLabel a, b;
  m.Branch((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kNotEqual, s[0]->flags_condition());
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorDPFlagSetTest,
                        ::testing::ValuesIn(kDPFlagSetInstructions));


TEST_F(InstructionSelectorTest, Word32AndBranchWithImmediateOnRight) {
  TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation32(imm) == 1) continue;

    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Word32And(m.Parameter(0), m.Int32Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithImmediateOnRight) {
  TRACED_FOREACH(int64_t, imm, kLogical64Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation64(imm) == 1) continue;

    StreamBuilder m(this, kMachInt64, kMachInt64);
    MLabel a, b;
    m.Branch(m.Word64And(m.Parameter(0), m.Int64Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, AddBranchWithImmediateOnRight) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Int32Add(m.Parameter(0), m.Int32Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, SubBranchWithImmediateOnRight) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Int32Sub(m.Parameter(0), m.Int32Constant(imm)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmp32, s[0]->arch_opcode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word32AndBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kLogical32Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation32(imm) == 1) continue;

    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Word32And(m.Int32Constant(imm), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    ASSERT_LE(1U, s[0]->InputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int64_t, imm, kLogical64Immediates) {
    // Skip the cases where the instruction selector would use tbz/tbnz.
    if (base::bits::CountPopulation64(imm) == 1) continue;

    StreamBuilder m(this, kMachInt64, kMachInt64);
    MLabel a, b;
    m.Branch(m.Word64And(m.Int64Constant(imm), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    ASSERT_LE(1U, s[0]->InputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, AddBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Int32Add(m.Int32Constant(imm), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Cmn32, s[0]->arch_opcode());
    ASSERT_LE(1U, s[0]->InputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word32AndBranchWithOneBitMaskOnRight) {
  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Word32And(m.Parameter(0), m.Int32Constant(mask)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbnz32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }

  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(
        m.Word32BinaryNot(m.Word32And(m.Parameter(0), m.Int32Constant(mask))),
        &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbz32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }
}


TEST_F(InstructionSelectorTest, Word32AndBranchWithOneBitMaskOnLeft) {
  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(m.Word32And(m.Int32Constant(mask), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbnz32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }

  TRACED_FORRANGE(int, bit, 0, 31) {
    uint32_t mask = 1 << bit;
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    m.Branch(
        m.Word32BinaryNot(m.Word32And(m.Int32Constant(mask), m.Parameter(0))),
        &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbz32, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt32(s[0]->InputAt(1)));
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithOneBitMaskOnRight) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = 1L << bit;
    StreamBuilder m(this, kMachInt64, kMachInt64);
    MLabel a, b;
    m.Branch(m.Word64And(m.Parameter(0), m.Int64Constant(mask)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbnz, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }

  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = 1L << bit;
    StreamBuilder m(this, kMachInt64, kMachInt64);
    MLabel a, b;
    m.Branch(
        m.Word64BinaryNot(m.Word64And(m.Parameter(0), m.Int64Constant(mask))),
        &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbz, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }
}


TEST_F(InstructionSelectorTest, Word64AndBranchWithOneBitMaskOnLeft) {
  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = 1L << bit;
    StreamBuilder m(this, kMachInt64, kMachInt64);
    MLabel a, b;
    m.Branch(m.Word64And(m.Int64Constant(mask), m.Parameter(0)), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbnz, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }

  TRACED_FORRANGE(int, bit, 0, 63) {
    uint64_t mask = 1L << bit;
    StreamBuilder m(this, kMachInt64, kMachInt64);
    MLabel a, b;
    m.Branch(
        m.Word64BinaryNot(m.Word64And(m.Int64Constant(mask), m.Parameter(0))),
        &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tbz, s[0]->arch_opcode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(bit, s.ToInt64(s[0]->InputAt(1)));
  }
}


// -----------------------------------------------------------------------------
// Add and subtract instructions with overflow.


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorOvfAddSubTest;


TEST_P(InstructionSelectorOvfAddSubTest, OvfParameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return(
      m.Projection(1, (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_LE(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorOvfAddSubTest, OvfImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    m.Return(m.Projection(
        1, (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorOvfAddSubTest, ValParameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return(
      m.Projection(0, (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_LE(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}


TEST_P(InstructionSelectorOvfAddSubTest, ValImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    m.Return(m.Projection(
        0, (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_P(InstructionSelectorOvfAddSubTest, BothParameter) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
  Stream s = m.Build();
  ASSERT_LE(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(2U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorOvfAddSubTest, BothImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();
    ASSERT_LE(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorOvfAddSubTest, BranchWithParameters) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  StreamBuilder m(this, type, type, type);
  MLabel a, b;
  Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Branch(m.Projection(1, n), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(0));
  m.Bind(&b);
  m.Return(m.Projection(0, n));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorOvfAddSubTest, BranchWithImmediateOnRight) {
  const MachInst2 dpi = GetParam();
  const MachineType type = dpi.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, type, type);
    MLabel a, b;
    Node* n = (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm));
    m.Branch(m.Projection(1, n), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(0));
    m.Bind(&b);
    m.Return(m.Projection(0, n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorOvfAddSubTest,
                        ::testing::ValuesIn(kOvfAddSubInstructions));


TEST_F(InstructionSelectorTest, OvfFlagAddImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Projection(
        1, m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0))));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, OvfValAddImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Projection(
        0, m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0))));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_F(InstructionSelectorTest, OvfBothAddImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    Node* n = m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();

    ASSERT_LE(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, OvfBranchWithImmediateOnLeft) {
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    MLabel a, b;
    Node* n = m.Int32AddWithOverflow(m.Int32Constant(imm), m.Parameter(0));
    m.Branch(m.Projection(1, n), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(0));
    m.Bind(&b);
    m.Return(m.Projection(0, n));
    Stream s = m.Build();

    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Add32, s[0]->arch_opcode());
    ASSERT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


// -----------------------------------------------------------------------------
// Shift instructions.


typedef InstructionSelectorTestWithParam<Shift> InstructionSelectorShiftTest;


TEST_P(InstructionSelectorShiftTest, Parameter) {
  const Shift shift = GetParam();
  const MachineType type = shift.mi.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*shift.mi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(shift.mi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorShiftTest, Immediate) {
  const Shift shift = GetParam();
  const MachineType type = shift.mi.machine_type;
  TRACED_FORRANGE(int32_t, imm, 0, (ElementSizeOf(type) * 8) - 1) {
    StreamBuilder m(this, type, type);
    m.Return((m.*shift.mi.constructor)(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(shift.mi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorShiftTest,
                        ::testing::ValuesIn(kShiftInstructions));


// -----------------------------------------------------------------------------
// Mul and Div instructions.


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


namespace {

struct MulDPInst {
  const char* mul_constructor_name;
  Node* (RawMachineAssembler::*mul_constructor)(Node*, Node*);
  Node* (RawMachineAssembler::*add_constructor)(Node*, Node*);
  Node* (RawMachineAssembler::*sub_constructor)(Node*, Node*);
  ArchOpcode add_arch_opcode;
  ArchOpcode sub_arch_opcode;
  ArchOpcode neg_arch_opcode;
  MachineType machine_type;
};


std::ostream& operator<<(std::ostream& os, const MulDPInst& inst) {
  return os << inst.mul_constructor_name;
}

}  // namespace


static const MulDPInst kMulDPInstructions[] = {
    {"Int32Mul", &RawMachineAssembler::Int32Mul, &RawMachineAssembler::Int32Add,
     &RawMachineAssembler::Int32Sub, kArm64Madd32, kArm64Msub32, kArm64Mneg32,
     kMachInt32},
    {"Int64Mul", &RawMachineAssembler::Int64Mul, &RawMachineAssembler::Int64Add,
     &RawMachineAssembler::Int64Sub, kArm64Madd, kArm64Msub, kArm64Mneg,
     kMachInt64}};


typedef InstructionSelectorTestWithParam<MulDPInst>
    InstructionSelectorIntDPWithIntMulTest;


TEST_P(InstructionSelectorIntDPWithIntMulTest, AddWithMul) {
  const MulDPInst mdpi = GetParam();
  const MachineType type = mdpi.machine_type;
  {
    StreamBuilder m(this, type, type, type, type);
    Node* n = (m.*mdpi.mul_constructor)(m.Parameter(1), m.Parameter(2));
    m.Return((m.*mdpi.add_constructor)(m.Parameter(0), n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.add_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type, type);
    Node* n = (m.*mdpi.mul_constructor)(m.Parameter(0), m.Parameter(1));
    m.Return((m.*mdpi.add_constructor)(n, m.Parameter(2)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.add_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorIntDPWithIntMulTest, SubWithMul) {
  const MulDPInst mdpi = GetParam();
  const MachineType type = mdpi.machine_type;
  {
    StreamBuilder m(this, type, type, type, type);
    Node* n = (m.*mdpi.mul_constructor)(m.Parameter(1), m.Parameter(2));
    m.Return((m.*mdpi.sub_constructor)(m.Parameter(0), n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.sub_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorIntDPWithIntMulTest, NegativeMul) {
  const MulDPInst mdpi = GetParam();
  const MachineType type = mdpi.machine_type;
  {
    StreamBuilder m(this, type, type, type);
    Node* n =
        (m.*mdpi.sub_constructor)(BuildConstant(m, type, 0), m.Parameter(0));
    m.Return((m.*mdpi.mul_constructor)(n, m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.neg_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type);
    Node* n =
        (m.*mdpi.sub_constructor)(BuildConstant(m, type, 0), m.Parameter(1));
    m.Return((m.*mdpi.mul_constructor)(m.Parameter(0), n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(mdpi.neg_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorIntDPWithIntMulTest,
                        ::testing::ValuesIn(kMulDPInstructions));


// -----------------------------------------------------------------------------
// Floating point instructions.

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


// -----------------------------------------------------------------------------
// Conversions.

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


// -----------------------------------------------------------------------------
// Memory access instructions.


namespace {

struct MemoryAccess {
  MachineType type;
  ArchOpcode ldr_opcode;
  ArchOpcode str_opcode;
  const int32_t immediates[20];
};


std::ostream& operator<<(std::ostream& os, const MemoryAccess& memacc) {
  return os << memacc.type;
}

}  // namespace


static const MemoryAccess kMemoryAccesses[] = {
    {kMachInt8,
     kArm64Ldrsb,
     kArm64Strb,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 257, 258, 1000, 1001, 2121,
      2442, 4093, 4094, 4095}},
    {kMachUint8,
     kArm64Ldrb,
     kArm64Strb,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 257, 258, 1000, 1001, 2121,
      2442, 4093, 4094, 4095}},
    {kMachInt16,
     kArm64Ldrsh,
     kArm64Strh,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 258, 260, 4096, 4098, 4100,
      4242, 6786, 8188, 8190}},
    {kMachUint16,
     kArm64Ldrh,
     kArm64Strh,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 258, 260, 4096, 4098, 4100,
      4242, 6786, 8188, 8190}},
    {kMachInt32,
     kArm64LdrW,
     kArm64StrW,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 260, 4096, 4100, 8192, 8196,
      3276, 3280, 16376, 16380}},
    {kMachUint32,
     kArm64LdrW,
     kArm64StrW,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 260, 4096, 4100, 8192, 8196,
      3276, 3280, 16376, 16380}},
    {kMachInt64,
     kArm64Ldr,
     kArm64Str,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 264, 4096, 4104, 8192, 8200,
      16384, 16392, 32752, 32760}},
    {kMachUint64,
     kArm64Ldr,
     kArm64Str,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 264, 4096, 4104, 8192, 8200,
      16384, 16392, 32752, 32760}},
    {kMachFloat32,
     kArm64LdrS,
     kArm64StrS,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 260, 4096, 4100, 8192, 8196,
      3276, 3280, 16376, 16380}},
    {kMachFloat64,
     kArm64LdrD,
     kArm64StrD,
     {-256, -255, -3, -2, -1, 0, 1, 2, 3, 255, 256, 264, 4096, 4104, 8192, 8200,
      16384, 16392, 32752, 32760}}};


typedef InstructionSelectorTestWithParam<MemoryAccess>
    InstructionSelectorMemoryAccessTest;


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, kMachPtr, kMachInt32);
  m.Return(m.Load(memacc.type, m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, memacc.type, kMachPtr);
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, kMachInt32, kMachPtr, kMachInt32, memacc.type);
  m.Store(memacc.type, m.Parameter(0), m.Parameter(1), m.Parameter(2));
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_MRR, s[0]->addressing_mode());
  EXPECT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(0U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, kMachInt32, kMachPtr, memacc.type);
    m.Store(memacc.type, m.Parameter(0), m.Int32Constant(index),
            m.Parameter(1));
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_MRI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorMemoryAccessTest,
                        ::testing::ValuesIn(kMemoryAccesses));


// -----------------------------------------------------------------------------
// Comparison instructions.

static const MachInst2 kComparisonInstructions[] = {
    {&RawMachineAssembler::Word32Equal, "Word32Equal", kArm64Cmp32, kMachInt32},
    {&RawMachineAssembler::Word64Equal, "Word64Equal", kArm64Cmp, kMachInt64},
};


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorComparisonTest;


TEST_P(InstructionSelectorComparisonTest, WithParameters) {
  const MachInst2 cmp = GetParam();
  const MachineType type = cmp.machine_type;
  StreamBuilder m(this, type, type, type);
  m.Return((m.*cmp.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(cmp.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kEqual, s[0]->flags_condition());
}


TEST_P(InstructionSelectorComparisonTest, WithImmediate) {
  const MachInst2 cmp = GetParam();
  const MachineType type = cmp.machine_type;
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    // Compare with 0 are turned into tst instruction.
    if (imm == 0) continue;
    StreamBuilder m(this, type, type);
    m.Return((m.*cmp.constructor)(m.Parameter(0), BuildConstant(m, type, imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(cmp.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kAddSubImmediates) {
    // Compare with 0 are turned into tst instruction.
    if (imm == 0) continue;
    StreamBuilder m(this, type, type);
    m.Return((m.*cmp.constructor)(BuildConstant(m, type, imm), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(cmp.arch_opcode, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(imm, s.ToInt64(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}

INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorComparisonTest,
                        ::testing::ValuesIn(kComparisonInstructions));


TEST_F(InstructionSelectorTest, Word32EqualWithZero) {
  {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Word32Equal(m.Parameter(0), m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Word32Equal(m.Int32Constant(0), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst32, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word64EqualWithZero) {
  {
    StreamBuilder m(this, kMachInt64, kMachInt64);
    m.Return(m.Word64Equal(m.Parameter(0), m.Int64Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, kMachInt64, kMachInt64);
    m.Return(m.Word64Equal(m.Int64Constant(0), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Tst, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


// -----------------------------------------------------------------------------
// Miscellaneous


static const MachInst2 kLogicalWithNotRHSs[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArm64Bic32, kMachInt32},
    {&RawMachineAssembler::Word64And, "Word64And", kArm64Bic, kMachInt64},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArm64Orn32, kMachInt32},
    {&RawMachineAssembler::Word64Or, "Word64Or", kArm64Orn, kMachInt64},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArm64Eon32, kMachInt32},
    {&RawMachineAssembler::Word64Xor, "Word64Xor", kArm64Eon, kMachInt64}};


typedef InstructionSelectorTestWithParam<MachInst2>
    InstructionSelectorLogicalWithNotRHSTest;


TEST_P(InstructionSelectorLogicalWithNotRHSTest, Parameter) {
  const MachInst2 inst = GetParam();
  const MachineType type = inst.machine_type;
  // Test cases where RHS is Xor(x, -1).
  {
    StreamBuilder m(this, type, type, type);
    if (type == kMachInt32) {
      m.Return((m.*inst.constructor)(
          m.Parameter(0), m.Word32Xor(m.Parameter(1), m.Int32Constant(-1))));
    } else {
      ASSERT_EQ(kMachInt64, type);
      m.Return((m.*inst.constructor)(
          m.Parameter(0), m.Word64Xor(m.Parameter(1), m.Int64Constant(-1))));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type);
    if (type == kMachInt32) {
      m.Return((m.*inst.constructor)(
          m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)), m.Parameter(1)));
    } else {
      ASSERT_EQ(kMachInt64, type);
      m.Return((m.*inst.constructor)(
          m.Word64Xor(m.Parameter(0), m.Int64Constant(-1)), m.Parameter(1)));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // Test cases where RHS is Not(x).
  {
    StreamBuilder m(this, type, type, type);
    if (type == kMachInt32) {
      m.Return(
          (m.*inst.constructor)(m.Parameter(0), m.Word32Not(m.Parameter(1))));
    } else {
      ASSERT_EQ(kMachInt64, type);
      m.Return(
          (m.*inst.constructor)(m.Parameter(0), m.Word64Not(m.Parameter(1))));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, type, type, type);
    if (type == kMachInt32) {
      m.Return(
          (m.*inst.constructor)(m.Word32Not(m.Parameter(0)), m.Parameter(1)));
    } else {
      ASSERT_EQ(kMachInt64, type);
      m.Return(
          (m.*inst.constructor)(m.Word64Not(m.Parameter(0)), m.Parameter(1)));
    }
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest,
                        InstructionSelectorLogicalWithNotRHSTest,
                        ::testing::ValuesIn(kLogicalWithNotRHSs));


TEST_F(InstructionSelectorTest, Word32NotWithParameter) {
  StreamBuilder m(this, kMachInt32, kMachInt32);
  m.Return(m.Word32Not(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Not32, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Word64NotWithParameter) {
  StreamBuilder m(this, kMachInt64, kMachInt64);
  m.Return(m.Word64Not(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArm64Not, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Word32XorMinusOneWithParameter) {
  {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not32, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, kMachInt32, kMachInt32);
    m.Return(m.Word32Xor(m.Int32Constant(-1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not32, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word64XorMinusOneWithParameter) {
  {
    StreamBuilder m(this, kMachInt64, kMachInt64);
    m.Return(m.Word64Xor(m.Parameter(0), m.Int64Constant(-1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, kMachInt64, kMachInt64);
    m.Return(m.Word64Xor(m.Int64Constant(-1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArm64Not, s[0]->arch_opcode());
    EXPECT_EQ(1U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32ShrWithWord32AndWithImmediate) {
  TRACED_FORRANGE(int32_t, lsb, 1, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t jnk = rng()->NextInt();
      jnk >>= 32 - lsb;
      uint32_t msk = ((0xffffffffu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Word32Shr(m.Word32And(m.Parameter(0), m.Int32Constant(msk)),
                           m.Int32Constant(lsb)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, lsb, 1, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t jnk = rng()->NextInt();
      jnk >>= 32 - lsb;
      uint32_t msk = ((0xffffffffu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Word32Shr(m.Word32And(m.Int32Constant(msk), m.Parameter(0)),
                           m.Int32Constant(lsb)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word64ShrWithWord64AndWithImmediate) {
  TRACED_FORRANGE(int32_t, lsb, 1, 63) {
    TRACED_FORRANGE(int32_t, width, 1, 64 - lsb) {
      uint64_t jnk = rng()->NextInt64();
      jnk >>= 64 - lsb;
      uint64_t msk =
          ((V8_UINT64_C(0xffffffffffffffff) >> (64 - width)) << lsb) | jnk;
      StreamBuilder m(this, kMachInt64, kMachInt64);
      m.Return(m.Word64Shr(m.Word64And(m.Parameter(0), m.Int64Constant(msk)),
                           m.Int64Constant(lsb)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, lsb, 1, 63) {
    TRACED_FORRANGE(int32_t, width, 1, 64 - lsb) {
      uint64_t jnk = rng()->NextInt64();
      jnk >>= 64 - lsb;
      uint64_t msk =
          ((V8_UINT64_C(0xffffffffffffffff) >> (64 - width)) << lsb) | jnk;
      StreamBuilder m(this, kMachInt64, kMachInt64);
      m.Return(m.Word64Shr(m.Word64And(m.Int64Constant(msk), m.Parameter(0)),
                           m.Int64Constant(lsb)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word32AndWithImmediateWithWord32Shr) {
  TRACED_FORRANGE(int32_t, lsb, 1, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 31) {
      uint32_t msk = (1 << width) - 1;
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Word32And(m.Word32Shr(m.Parameter(0), m.Int32Constant(lsb)),
                           m.Int32Constant(msk)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      int32_t actual_width = (lsb + width > 32) ? (32 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, lsb, 1, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 31) {
      uint32_t msk = (1 << width) - 1;
      StreamBuilder m(this, kMachInt32, kMachInt32);
      m.Return(m.Word32And(m.Int32Constant(msk),
                           m.Word32Shr(m.Parameter(0), m.Int32Constant(lsb))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx32, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      int32_t actual_width = (lsb + width > 32) ? (32 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word64AndWithImmediateWithWord64Shr) {
  TRACED_FORRANGE(int64_t, lsb, 1, 63) {
    TRACED_FORRANGE(int64_t, width, 1, 63) {
      uint64_t msk = (V8_UINT64_C(1) << width) - 1;
      StreamBuilder m(this, kMachInt64, kMachInt64);
      m.Return(m.Word64And(m.Word64Shr(m.Parameter(0), m.Int64Constant(lsb)),
                           m.Int64Constant(msk)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      int64_t actual_width = (lsb + width > 64) ? (64 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int64_t, lsb, 1, 63) {
    TRACED_FORRANGE(int64_t, width, 1, 63) {
      uint64_t msk = (V8_UINT64_C(1) << width) - 1;
      StreamBuilder m(this, kMachInt64, kMachInt64);
      m.Return(m.Word64And(m.Int64Constant(msk),
                           m.Word64Shr(m.Parameter(0), m.Int64Constant(lsb))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArm64Ubfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt64(s[0]->InputAt(1)));
      int64_t actual_width = (lsb + width > 64) ? (64 - lsb) : width;
      EXPECT_EQ(actual_width, s.ToInt64(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Int32MulHighWithParameters) {
  StreamBuilder m(this, kMachInt32, kMachInt32, kMachInt32);
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Int32MulHigh(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArm64Smull, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArm64Asr, s[1]->arch_opcode());
  ASSERT_EQ(2U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(32, s.ToInt64(s[1]->InputAt(1)));
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[1]->Output()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
