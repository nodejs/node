// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "test/unittests/compiler/backend/instruction-selector-unittest.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

typedef Node* (RawMachineAssembler::*Constructor)(Node*, Node*);


// Data processing instructions.
struct DPI {
  Constructor constructor;
  const char* constructor_name;
  ArchOpcode arch_opcode;
  ArchOpcode reverse_arch_opcode;
  ArchOpcode test_arch_opcode;
};


std::ostream& operator<<(std::ostream& os, const DPI& dpi) {
  return os << dpi.constructor_name;
}


const DPI kDPIs[] = {
    {&RawMachineAssembler::Word32And, "Word32And", kArmAnd, kArmAnd, kArmTst},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArmOrr, kArmOrr, kArmOrr},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArmEor, kArmEor, kArmTeq},
    {&RawMachineAssembler::Int32Add, "Int32Add", kArmAdd, kArmAdd, kArmCmn},
    {&RawMachineAssembler::Int32Sub, "Int32Sub", kArmSub, kArmRsb, kArmCmp}};


// Floating point arithmetic instructions.
struct FAI {
  Constructor constructor;
  const char* constructor_name;
  MachineType machine_type;
  ArchOpcode arch_opcode;
};


std::ostream& operator<<(std::ostream& os, const FAI& fai) {
  return os << fai.constructor_name;
}


const FAI kFAIs[] = {{&RawMachineAssembler::Float32Add, "Float32Add",
                      MachineType::Float32(), kArmVaddF32},
                     {&RawMachineAssembler::Float64Add, "Float64Add",
                      MachineType::Float64(), kArmVaddF64},
                     {&RawMachineAssembler::Float32Sub, "Float32Sub",
                      MachineType::Float32(), kArmVsubF32},
                     {&RawMachineAssembler::Float64Sub, "Float64Sub",
                      MachineType::Float64(), kArmVsubF64},
                     {&RawMachineAssembler::Float32Mul, "Float32Mul",
                      MachineType::Float32(), kArmVmulF32},
                     {&RawMachineAssembler::Float64Mul, "Float64Mul",
                      MachineType::Float64(), kArmVmulF64},
                     {&RawMachineAssembler::Float32Div, "Float32Div",
                      MachineType::Float32(), kArmVdivF32},
                     {&RawMachineAssembler::Float64Div, "Float64Div",
                      MachineType::Float64(), kArmVdivF64}};


// Data processing instructions with overflow.
struct ODPI {
  Constructor constructor;
  const char* constructor_name;
  ArchOpcode arch_opcode;
  ArchOpcode reverse_arch_opcode;
};


std::ostream& operator<<(std::ostream& os, const ODPI& odpi) {
  return os << odpi.constructor_name;
}


const ODPI kODPIs[] = {{&RawMachineAssembler::Int32AddWithOverflow,
                        "Int32AddWithOverflow", kArmAdd, kArmAdd},
                       {&RawMachineAssembler::Int32SubWithOverflow,
                        "Int32SubWithOverflow", kArmSub, kArmRsb}};


// Shifts.
struct Shift {
  Constructor constructor;
  const char* constructor_name;
  int32_t i_low;          // lowest possible immediate
  int32_t i_high;         // highest possible immediate
  AddressingMode i_mode;  // Operand2_R_<shift>_I
  AddressingMode r_mode;  // Operand2_R_<shift>_R
};


std::ostream& operator<<(std::ostream& os, const Shift& shift) {
  return os << shift.constructor_name;
}


const Shift kShifts[] = {{&RawMachineAssembler::Word32Sar, "Word32Sar", 1, 32,
                          kMode_Operand2_R_ASR_I, kMode_Operand2_R_ASR_R},
                         {&RawMachineAssembler::Word32Shl, "Word32Shl", 0, 31,
                          kMode_Operand2_R_LSL_I, kMode_Operand2_R_LSL_R},
                         {&RawMachineAssembler::Word32Shr, "Word32Shr", 1, 32,
                          kMode_Operand2_R_LSR_I, kMode_Operand2_R_LSR_R},
                         {&RawMachineAssembler::Word32Ror, "Word32Ror", 1, 31,
                          kMode_Operand2_R_ROR_I, kMode_Operand2_R_ROR_R}};


// Immediates (random subset).
const int32_t kImmediates[] = {
    std::numeric_limits<int32_t>::min(), -2147483617, -2147483606, -2113929216,
    -2080374784, -1996488704, -1879048192, -1459617792, -1358954496,
    -1342177265, -1275068414, -1073741818, -1073741777, -855638016, -805306368,
    -402653184, -268435444, -16777216, 0, 35, 61, 105, 116, 171, 245, 255, 692,
    1216, 1248, 1520, 1600, 1888, 3744, 4080, 5888, 8384, 9344, 9472, 9792,
    13312, 15040, 15360, 20736, 22272, 23296, 32000, 33536, 37120, 45824, 47872,
    56320, 59392, 65280, 72704, 101376, 147456, 161792, 164864, 167936, 173056,
    195584, 209920, 212992, 356352, 655360, 704512, 716800, 851968, 901120,
    1044480, 1523712, 2572288, 3211264, 3588096, 3833856, 3866624, 4325376,
    5177344, 6488064, 7012352, 7471104, 14090240, 16711680, 19398656, 22282240,
    28573696, 30408704, 30670848, 43253760, 54525952, 55312384, 56623104,
    68157440, 115343360, 131072000, 187695104, 188743680, 195035136, 197132288,
    203423744, 218103808, 267386880, 268435470, 285212672, 402653185, 415236096,
    595591168, 603979776, 603979778, 629145600, 1073741835, 1073741855,
    1073741861, 1073741884, 1157627904, 1476395008, 1476395010, 1610612741,
    2030043136, 2080374785, 2097152000};

}  // namespace


// -----------------------------------------------------------------------------
// Data processing instructions.


typedef InstructionSelectorTestWithParam<DPI> InstructionSelectorDPITest;


TEST_P(InstructionSelectorDPITest, Parameters) {
  const DPI dpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorDPITest, Immediate) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return((m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return((m.*dpi.constructor)(m.Int32Constant(imm), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorDPITest, ShiftByParameter) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return((m.*dpi.constructor)(
        m.Parameter(0),
        (m.*shift.constructor)(m.Parameter(1), m.Parameter(2))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return((m.*dpi.constructor)(
        (m.*shift.constructor)(m.Parameter(0), m.Parameter(1)),
        m.Parameter(2)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorDPITest, ShiftByImmediate) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return((m.*dpi.constructor)(
          m.Parameter(0),
          (m.*shift.constructor)(m.Parameter(1), m.Int32Constant(imm))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return((m.*dpi.constructor)(
          (m.*shift.constructor)(m.Parameter(0), m.Int32Constant(imm)),
          m.Parameter(1)));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.reverse_arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_EQ(1U, s[0]->OutputCount());
    }
  }
}


TEST_P(InstructionSelectorDPITest, BranchWithParameters) {
  const DPI dpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  RawMachineLabel a, b;
  m.Branch((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kNotEqual, s[0]->flags_condition());
}


TEST_P(InstructionSelectorDPITest, BranchWithImmediate) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch((m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)), &a,
             &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch((m.*dpi.constructor)(m.Int32Constant(imm), m.Parameter(0)), &a,
             &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorDPITest, BranchWithShiftByParameter) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch((m.*dpi.constructor)(
                 m.Parameter(0),
                 (m.*shift.constructor)(m.Parameter(1), m.Parameter(2))),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch((m.*dpi.constructor)(
                 (m.*shift.constructor)(m.Parameter(0), m.Parameter(1)),
                 m.Parameter(2)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorDPITest, BranchWithShiftByImmediate) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      RawMachineLabel a, b;
      m.Branch((m.*dpi.constructor)(m.Parameter(0),
                                    (m.*shift.constructor)(
                                        m.Parameter(1), m.Int32Constant(imm))),
               &a, &b);
      m.Bind(&a);
      m.Return(m.Int32Constant(1));
      m.Bind(&b);
      m.Return(m.Int32Constant(0));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(5U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
      EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    }
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      RawMachineLabel a, b;
      m.Branch((m.*dpi.constructor)(
                   (m.*shift.constructor)(m.Parameter(0), m.Int32Constant(imm)),
                   m.Parameter(1)),
               &a, &b);
      m.Bind(&a);
      m.Return(m.Int32Constant(1));
      m.Bind(&b);
      m.Return(m.Int32Constant(0));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(5U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
      EXPECT_EQ(kNotEqual, s[0]->flags_condition());
    }
  }
}


TEST_P(InstructionSelectorDPITest, BranchIfZeroWithParameters) {
  const DPI dpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  RawMachineLabel a, b;
  m.Branch(m.Word32Equal((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)),
                         m.Int32Constant(0)),
           &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kEqual, s[0]->flags_condition());
}


TEST_P(InstructionSelectorDPITest, BranchIfNotZeroWithParameters) {
  const DPI dpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  RawMachineLabel a, b;
  m.Branch(
      m.Word32NotEqual((m.*dpi.constructor)(m.Parameter(0), m.Parameter(1)),
                       m.Int32Constant(0)),
      &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(1));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kNotEqual, s[0]->flags_condition());
}


TEST_P(InstructionSelectorDPITest, BranchIfZeroWithImmediate) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32Equal(
                 (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)),
                 m.Int32Constant(0)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32Equal(
                 (m.*dpi.constructor)(m.Int32Constant(imm), m.Parameter(0)),
                 m.Int32Constant(0)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorDPITest, BranchIfNotZeroWithImmediate) {
  const DPI dpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32NotEqual(
                 (m.*dpi.constructor)(m.Parameter(0), m.Int32Constant(imm)),
                 m.Int32Constant(0)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    m.Branch(m.Word32NotEqual(
                 (m.*dpi.constructor)(m.Int32Constant(imm), m.Parameter(0)),
                 m.Int32Constant(0)),
             &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(dpi.test_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kNotEqual, s[0]->flags_condition());
  }
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest, InstructionSelectorDPITest,
                         ::testing::ValuesIn(kDPIs));

// -----------------------------------------------------------------------------
// Data processing instructions with overflow.


typedef InstructionSelectorTestWithParam<ODPI> InstructionSelectorODPITest;


TEST_P(InstructionSelectorODPITest, OvfWithParameters) {
  const ODPI odpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(
      m.Projection(1, (m.*odpi.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_LE(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorODPITest, OvfWithImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        1, (m.*odpi.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        1, (m.*odpi.constructor)(m.Int32Constant(imm), m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorODPITest, OvfWithShiftByParameter) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        1, (m.*odpi.constructor)(
               m.Parameter(0),
               (m.*shift.constructor)(m.Parameter(1), m.Parameter(2)))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        1, (m.*odpi.constructor)(
               (m.*shift.constructor)(m.Parameter(0), m.Parameter(1)),
               m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorODPITest, OvfWithShiftByImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return(m.Projection(
          1, (m.*odpi.constructor)(m.Parameter(0),
                                   (m.*shift.constructor)(
                                       m.Parameter(1), m.Int32Constant(imm)))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_LE(1U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(kOverflow, s[0]->flags_condition());
    }
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return(m.Projection(
          1, (m.*odpi.constructor)(
                 (m.*shift.constructor)(m.Parameter(1), m.Int32Constant(imm)),
                 m.Parameter(0))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_LE(1U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(kOverflow, s[0]->flags_condition());
    }
  }
}


TEST_P(InstructionSelectorODPITest, ValWithParameters) {
  const ODPI odpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(
      m.Projection(0, (m.*odpi.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_LE(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}


TEST_P(InstructionSelectorODPITest, ValWithImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        0, (m.*odpi.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        0, (m.*odpi.constructor)(m.Int32Constant(imm), m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_P(InstructionSelectorODPITest, ValWithShiftByParameter) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        0, (m.*odpi.constructor)(
               m.Parameter(0),
               (m.*shift.constructor)(m.Parameter(1), m.Parameter(2)))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return(m.Projection(
        0, (m.*odpi.constructor)(
               (m.*shift.constructor)(m.Parameter(0), m.Parameter(1)),
               m.Parameter(0))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_LE(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_P(InstructionSelectorODPITest, ValWithShiftByImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return(m.Projection(
          0, (m.*odpi.constructor)(m.Parameter(0),
                                   (m.*shift.constructor)(
                                       m.Parameter(1), m.Int32Constant(imm)))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_LE(1U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_none, s[0]->flags_mode());
    }
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      m.Return(m.Projection(
          0, (m.*odpi.constructor)(
                 (m.*shift.constructor)(m.Parameter(1), m.Int32Constant(imm)),
                 m.Parameter(0))));
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_LE(1U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_none, s[0]->flags_mode());
    }
  }
}


TEST_P(InstructionSelectorODPITest, BothWithParameters) {
  const ODPI odpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Node* n = (m.*odpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
  Stream s = m.Build();
  ASSERT_LE(1U, s.size());
  EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(2U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorODPITest, BothWithImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* n = (m.*odpi.constructor)(m.Parameter(0), m.Int32Constant(imm));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();
    ASSERT_LE(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* n = (m.*odpi.constructor)(m.Int32Constant(imm), m.Parameter(0));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();
    ASSERT_LE(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorODPITest, BothWithShiftByParameter) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    Node* n = (m.*odpi.constructor)(
        m.Parameter(0), (m.*shift.constructor)(m.Parameter(1), m.Parameter(2)));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();
    ASSERT_LE(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    Node* n = (m.*odpi.constructor)(
        (m.*shift.constructor)(m.Parameter(0), m.Parameter(1)), m.Parameter(2));
    m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
    Stream s = m.Build();
    ASSERT_LE(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorODPITest, BothWithShiftByImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      Node* n = (m.*odpi.constructor)(
          m.Parameter(0),
          (m.*shift.constructor)(m.Parameter(1), m.Int32Constant(imm)));
      m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
      Stream s = m.Build();
      ASSERT_LE(1U, s.size());
      EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_EQ(2U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(kOverflow, s[0]->flags_condition());
    }
  }
  TRACED_FOREACH(Shift, shift, kShifts) {
    TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      Node* n = (m.*odpi.constructor)(
          (m.*shift.constructor)(m.Parameter(0), m.Int32Constant(imm)),
          m.Parameter(1));
      m.Return(m.Word32Equal(m.Projection(0, n), m.Projection(1, n)));
      Stream s = m.Build();
      ASSERT_LE(1U, s.size());
      EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
      EXPECT_EQ(2U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(kOverflow, s[0]->flags_condition());
    }
  }
}


TEST_P(InstructionSelectorODPITest, BranchWithParameters) {
  const ODPI odpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  RawMachineLabel a, b;
  Node* n = (m.*odpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Branch(m.Projection(1, n), &a, &b);
  m.Bind(&a);
  m.Return(m.Int32Constant(0));
  m.Bind(&b);
  m.Return(m.Projection(0, n));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorODPITest, BranchWithImmediate) {
  const ODPI odpi = GetParam();
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* n = (m.*odpi.constructor)(m.Parameter(0), m.Int32Constant(imm));
    m.Branch(m.Projection(1, n), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(0));
    m.Bind(&b);
    m.Return(m.Projection(0, n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* n = (m.*odpi.constructor)(m.Int32Constant(imm), m.Parameter(0));
    m.Branch(m.Projection(1, n), &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(0));
    m.Bind(&b);
    m.Return(m.Projection(0, n));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(odpi.reverse_arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kOverflow, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorODPITest, BranchIfZeroWithParameters) {
  const ODPI odpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  RawMachineLabel a, b;
  Node* n = (m.*odpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Branch(m.Word32Equal(m.Projection(1, n), m.Int32Constant(0)), &a, &b);
  m.Bind(&a);
  m.Return(m.Projection(0, n));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kNotOverflow, s[0]->flags_condition());
}


TEST_P(InstructionSelectorODPITest, BranchIfNotZeroWithParameters) {
  const ODPI odpi = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  RawMachineLabel a, b;
  Node* n = (m.*odpi.constructor)(m.Parameter(0), m.Parameter(1));
  m.Branch(m.Word32NotEqual(m.Projection(1, n), m.Int32Constant(0)), &a, &b);
  m.Bind(&a);
  m.Return(m.Projection(0, n));
  m.Bind(&b);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(odpi.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(4U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
  EXPECT_EQ(kOverflow, s[0]->flags_condition());
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest, InstructionSelectorODPITest,
                         ::testing::ValuesIn(kODPIs));

// -----------------------------------------------------------------------------
// Shifts.


typedef InstructionSelectorTestWithParam<Shift> InstructionSelectorShiftTest;


TEST_P(InstructionSelectorShiftTest, Parameters) {
  const Shift shift = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return((m.*shift.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmMov, s[0]->arch_opcode());
  EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorShiftTest, Immediate) {
  const Shift shift = GetParam();
  TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return((m.*shift.constructor)(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmMov, s[0]->arch_opcode());
    EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_P(InstructionSelectorShiftTest, Word32EqualWithParameter) {
  const Shift shift = GetParam();
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return(
        m.Word32Equal(m.Parameter(0),
                      (m.*shift.constructor)(m.Parameter(1), m.Parameter(2))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    m.Return(
        m.Word32Equal((m.*shift.constructor)(m.Parameter(1), m.Parameter(2)),
                      m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorShiftTest, Word32EqualWithParameterAndImmediate) {
  const Shift shift = GetParam();
  TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Word32Equal(
        (m.*shift.constructor)(m.Parameter(1), m.Int32Constant(imm)),
        m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Word32Equal(
        m.Parameter(0),
        (m.*shift.constructor)(m.Parameter(1), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_P(InstructionSelectorShiftTest, Word32EqualToZeroWithParameters) {
  const Shift shift = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(
      m.Word32Equal(m.Int32Constant(0),
                    (m.*shift.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmMov, s[0]->arch_opcode());
  EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(2U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kEqual, s[0]->flags_condition());
}


TEST_P(InstructionSelectorShiftTest, Word32EqualToZeroWithImmediate) {
  const Shift shift = GetParam();
  TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Word32Equal(
        m.Int32Constant(0),
        (m.*shift.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmMov, s[0]->arch_opcode());
    EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(2U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorShiftTest, Word32BitwiseNotWithParameters) {
  const Shift shift = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Word32BitwiseNot(
      (m.*shift.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmMvn, s[0]->arch_opcode());
  EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

TEST_P(InstructionSelectorShiftTest, Word32BitwiseNotWithImmediate) {
  const Shift shift = GetParam();
  TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32BitwiseNot(
        (m.*shift.constructor)(m.Parameter(0), m.Int32Constant(imm))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmMvn, s[0]->arch_opcode());
    EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

TEST_P(InstructionSelectorShiftTest,
       Word32AndWithWord32BitwiseNotWithParameters) {
  const Shift shift = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32(), MachineType::Int32());
  m.Return(
      m.Word32And(m.Parameter(0), m.Word32BitwiseNot((m.*shift.constructor)(
                                      m.Parameter(1), m.Parameter(2)))));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmBic, s[0]->arch_opcode());
  EXPECT_EQ(shift.r_mode, s[0]->addressing_mode());
  EXPECT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}

TEST_P(InstructionSelectorShiftTest,
       Word32AndWithWord32BitwiseNotWithImmediate) {
  const Shift shift = GetParam();
  TRACED_FORRANGE(int32_t, imm, shift.i_low, shift.i_high) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Word32And(m.Parameter(0),
                         m.Word32BitwiseNot((m.*shift.constructor)(
                             m.Parameter(1), m.Int32Constant(imm)))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmBic, s[0]->arch_opcode());
    EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest, InstructionSelectorShiftTest,
                         ::testing::ValuesIn(kShifts));

// -----------------------------------------------------------------------------
// Memory access instructions.


namespace {

struct MemoryAccess {
  MachineType type;
  ArchOpcode ldr_opcode;
  ArchOpcode str_opcode;
  bool (InstructionSelectorTest::Stream::*val_predicate)(
      const InstructionOperand*) const;
  const int32_t immediates[40];
};


std::ostream& operator<<(std::ostream& os, const MemoryAccess& memacc) {
  return os << memacc.type;
}


const MemoryAccess kMemoryAccesses[] = {
    {MachineType::Int8(),
     kArmLdrsb,
     kArmStrb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3340, -3231, -3224, -3088, -1758, -1203, -123, -117, -91, -89,
      -87, -86, -82, -44, -23, -3, 0, 7, 10, 39, 52, 69, 71, 91, 92, 107, 109,
      115, 124, 286, 655, 1362, 1569, 2587, 3067, 3096, 3462, 3510, 4095}},
    {MachineType::Uint8(),
     kArmLdrb,
     kArmStrb,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -3914, -3536, -3234, -3185, -3169, -1073, -990, -859, -720, -434,
      -127, -124, -122, -105, -91, -86, -64, -55, -53, -30, -10, -3, 0, 20, 28,
      39, 58, 64, 73, 75, 100, 108, 121, 686, 963, 1363, 2759, 3449, 4095}},
    {MachineType::Int16(),
     kArmLdrsh,
     kArmStrh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-255, -251, -232, -220, -144, -138, -130, -126, -116, -115, -102, -101,
      -98, -69, -59, -56, -39, -35, -23, -19, -7, 0, 22, 26, 37, 68, 83, 87, 98,
      102, 108, 111, 117, 171, 195, 203, 204, 245, 246, 255}},
    {MachineType::Uint16(),
     kArmLdrh,
     kArmStrh,
     &InstructionSelectorTest::Stream::IsInteger,
     {-255, -230, -201, -172, -125, -119, -118, -105, -98, -79, -54, -42, -41,
      -32, -12, -11, -5, -4, 0, 5, 9, 25, 28, 51, 58, 60, 89, 104, 108, 109,
      114, 116, 120, 138, 150, 161, 166, 172, 228, 255}},
    {MachineType::Int32(),
     kArmLdr,
     kArmStr,
     &InstructionSelectorTest::Stream::IsInteger,
     {-4095, -1898, -1685, -1562, -1408, -1313, -344, -128, -116, -100, -92,
      -80, -72, -71, -56, -25, -21, -11, -9, 0, 3, 5, 27, 28, 42, 52, 63, 88,
      93, 97, 125, 846, 1037, 2102, 2403, 2597, 2632, 2997, 3935, 4095}},
    {MachineType::Float32(),
     kArmVldrF32,
     kArmVstrF32,
     &InstructionSelectorTest::Stream::IsDouble,
     {-1020, -928, -896, -772, -728, -680, -660, -488, -372, -112, -100, -92,
      -84, -80, -72, -64, -60, -56, -52, -48, -36, -32, -20, -8, -4, 0, 8, 20,
      24, 40, 64, 112, 204, 388, 516, 852, 856, 976, 988, 1020}},
    {MachineType::Float64(),
     kArmVldrF64,
     kArmVstrF64,
     &InstructionSelectorTest::Stream::IsDouble,
     {-1020, -948, -796, -696, -612, -364, -320, -308, -128, -112, -108, -104,
      -96, -84, -80, -56, -48, -40, -20, 0, 24, 28, 36, 48, 64, 84, 96, 100,
      108, 116, 120, 140, 156, 408, 432, 444, 772, 832, 940, 1020}}};

}  // namespace


typedef InstructionSelectorTestWithParam<MemoryAccess>
    InstructionSelectorMemoryAccessTest;


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, memacc.type, MachineType::Pointer(),
                  MachineType::Int32());
  m.Return(m.Load(memacc.type, m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Offset_RR, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_TRUE((s.*memacc.val_predicate)(s[0]->Output()));
}


TEST_P(InstructionSelectorMemoryAccessTest, LoadWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, memacc.type, MachineType::Pointer());
    m.Return(m.Load(memacc.type, m.Parameter(0), m.Int32Constant(index)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.ldr_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Offset_RI, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(1)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE((s.*memacc.val_predicate)(s[0]->Output()));
  }
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithParameters) {
  const MemoryAccess memacc = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                  MachineType::Int32(), memacc.type);
  m.Store(memacc.type.representation(), m.Parameter(0), m.Parameter(1),
          m.Parameter(2), kNoWriteBarrier);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Offset_RR, s[0]->addressing_mode());
  EXPECT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(0U, s[0]->OutputCount());
}


TEST_P(InstructionSelectorMemoryAccessTest, StoreWithImmediateIndex) {
  const MemoryAccess memacc = GetParam();
  TRACED_FOREACH(int32_t, index, memacc.immediates) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    memacc.type);
    m.Store(memacc.type.representation(), m.Parameter(0),
            m.Int32Constant(index), m.Parameter(1), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(memacc.str_opcode, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Offset_RI, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    ASSERT_EQ(InstructionOperand::IMMEDIATE, s[0]->InputAt(2)->kind());
    EXPECT_EQ(index, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest,
                         InstructionSelectorMemoryAccessTest,
                         ::testing::ValuesIn(kMemoryAccesses));

TEST_F(InstructionSelectorMemoryAccessTest, LoadWithShiftedIndex) {
  TRACED_FORRANGE(int, immediate_shift, 1, 31) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    MachineType::Int32());
    Node* const index =
        m.Word32Shl(m.Parameter(1), m.Int32Constant(immediate_shift));
    m.Return(m.Load(MachineType::Int32(), m.Parameter(0), index));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmLdr, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    EXPECT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}

TEST_F(InstructionSelectorMemoryAccessTest, StoreWithShiftedIndex) {
  TRACED_FORRANGE(int, immediate_shift, 1, 31) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer(),
                    MachineType::Int32(), MachineType::Int32());
    Node* const index =
        m.Word32Shl(m.Parameter(1), m.Int32Constant(immediate_shift));
    m.Store(MachineRepresentation::kWord32, m.Parameter(0), index,
            m.Parameter(2), kNoWriteBarrier);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmStr, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    EXPECT_EQ(4U, s[0]->InputCount());
    EXPECT_EQ(0U, s[0]->OutputCount());
  }
}

// -----------------------------------------------------------------------------
// Conversions.


TEST_F(InstructionSelectorTest, ChangeFloat32ToFloat64WithParameter) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float32());
  m.Return(m.ChangeFloat32ToFloat64(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcvtF64F32, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, TruncateFloat64ToFloat32WithParameter) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float64());
  m.Return(m.TruncateFloat64ToFloat32(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcvtF32F64, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


// -----------------------------------------------------------------------------
// Comparisons.


namespace {

struct Comparison {
  Constructor constructor;
  const char* constructor_name;
  FlagsCondition flags_condition;
  FlagsCondition negated_flags_condition;
  FlagsCondition commuted_flags_condition;
};


std::ostream& operator<<(std::ostream& os, const Comparison& cmp) {
  return os << cmp.constructor_name;
}


const Comparison kComparisons[] = {
    {&RawMachineAssembler::Word32Equal, "Word32Equal", kEqual, kNotEqual,
     kEqual},
    {&RawMachineAssembler::Int32LessThan, "Int32LessThan", kSignedLessThan,
     kSignedGreaterThanOrEqual, kSignedGreaterThan},
    {&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
     kSignedLessThanOrEqual, kSignedGreaterThan, kSignedGreaterThanOrEqual},
    {&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kUnsignedLessThan,
     kUnsignedGreaterThanOrEqual, kUnsignedGreaterThan},
    {&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
     kUnsignedLessThanOrEqual, kUnsignedGreaterThan,
     kUnsignedGreaterThanOrEqual}};

}  // namespace


typedef InstructionSelectorTestWithParam<Comparison>
    InstructionSelectorComparisonTest;


TEST_P(InstructionSelectorComparisonTest, Parameters) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const r = (m.*cmp.constructor)(p0, p1);
  m.Return(r);
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->OutputAt(0)));
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorComparisonTest, Word32EqualWithZero) {
  {
    const Comparison& cmp = GetParam();
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r =
        m.Word32Equal((m.*cmp.constructor)(p0, p1), m.Int32Constant(0));
    m.Return(r);
    Stream const s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->OutputAt(0)));
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.negated_flags_condition, s[0]->flags_condition());
  }
  {
    const Comparison& cmp = GetParam();
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r =
        m.Word32Equal(m.Int32Constant(0), (m.*cmp.constructor)(p0, p1));
    m.Return(r);
    Stream const s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->OutputAt(0)));
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(cmp.negated_flags_condition, s[0]->flags_condition());
  }
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest,
                         InstructionSelectorComparisonTest,
                         ::testing::ValuesIn(kComparisons));

// -----------------------------------------------------------------------------
// Floating point comparisons.


namespace {

const Comparison kF32Comparisons[] = {
    {&RawMachineAssembler::Float32Equal, "Float32Equal", kEqual, kNotEqual,
     kEqual},
    {&RawMachineAssembler::Float32LessThan, "Float32LessThan",
     kFloatLessThan, kFloatGreaterThanOrEqualOrUnordered, kFloatGreaterThan},
    {&RawMachineAssembler::Float32LessThanOrEqual, "Float32LessThanOrEqual",
     kFloatLessThanOrEqual, kFloatGreaterThanOrUnordered,
     kFloatGreaterThanOrEqual}};

}  // namespace

typedef InstructionSelectorTestWithParam<Comparison>
    InstructionSelectorF32ComparisonTest;


TEST_P(InstructionSelectorF32ComparisonTest, WithParameters) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float32(),
                  MachineType::Float32());
  m.Return((m.*cmp.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF32, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorF32ComparisonTest, NegatedWithParameters) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float32(),
                  MachineType::Float32());
  m.Return(
      m.Word32BinaryNot((m.*cmp.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF32, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.negated_flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorF32ComparisonTest, WithImmediateZeroOnRight) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float32());
  m.Return((m.*cmp.constructor)(m.Parameter(0), m.Float32Constant(0.0)));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF32, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorF32ComparisonTest, WithImmediateZeroOnLeft) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float32());
  m.Return((m.*cmp.constructor)(m.Float32Constant(0.0f), m.Parameter(0)));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF32, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.commuted_flags_condition, s[0]->flags_condition());
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest,
                         InstructionSelectorF32ComparisonTest,
                         ::testing::ValuesIn(kF32Comparisons));

namespace {

const Comparison kF64Comparisons[] = {
    {&RawMachineAssembler::Float64Equal, "Float64Equal", kEqual, kNotEqual,
     kEqual},
    {&RawMachineAssembler::Float64LessThan, "Float64LessThan",
     kFloatLessThan, kFloatGreaterThanOrEqualOrUnordered, kFloatGreaterThan},
    {&RawMachineAssembler::Float64LessThanOrEqual, "Float64LessThanOrEqual",
     kFloatLessThanOrEqual, kFloatGreaterThanOrUnordered,
     kFloatGreaterThanOrEqual}};

}  // namespace

typedef InstructionSelectorTestWithParam<Comparison>
    InstructionSelectorF64ComparisonTest;


TEST_P(InstructionSelectorF64ComparisonTest, WithParameters) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64(),
                  MachineType::Float64());
  m.Return((m.*cmp.constructor)(m.Parameter(0), m.Parameter(1)));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF64, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorF64ComparisonTest, NegatedWithParameters) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64(),
                  MachineType::Float64());
  m.Return(
      m.Word32BinaryNot((m.*cmp.constructor)(m.Parameter(0), m.Parameter(1))));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF64, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.negated_flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorF64ComparisonTest, WithImmediateZeroOnRight) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64());
  m.Return((m.*cmp.constructor)(m.Parameter(0), m.Float64Constant(0.0)));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF64, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
}


TEST_P(InstructionSelectorF64ComparisonTest, WithImmediateZeroOnLeft) {
  const Comparison& cmp = GetParam();
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64());
  m.Return((m.*cmp.constructor)(m.Float64Constant(0.0), m.Parameter(0)));
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVcmpF64, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_TRUE(s[0]->InputAt(1)->IsImmediate());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(cmp.commuted_flags_condition, s[0]->flags_condition());
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest,
                         InstructionSelectorF64ComparisonTest,
                         ::testing::ValuesIn(kF64Comparisons));

// -----------------------------------------------------------------------------
// Floating point arithmetic.


typedef InstructionSelectorTestWithParam<FAI> InstructionSelectorFAITest;


TEST_P(InstructionSelectorFAITest, Parameters) {
  const FAI& fai = GetParam();
  StreamBuilder m(this, fai.machine_type, fai.machine_type, fai.machine_type);
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const r = (m.*fai.constructor)(p0, p1);
  m.Return(r);
  Stream const s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(fai.arch_opcode, s[0]->arch_opcode());
  EXPECT_EQ(kMode_None, s[0]->addressing_mode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->OutputAt(0)));
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest, InstructionSelectorFAITest,
                         ::testing::ValuesIn(kFAIs));

TEST_F(InstructionSelectorTest, Float32Abs) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float32Abs(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVabsF32, s[0]->arch_opcode());
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
  EXPECT_EQ(kArmVabsF64, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Float32AddWithFloat32Mul) {
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
    EXPECT_EQ(kArmVmlaF32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE(
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
    EXPECT_EQ(kArmVmlaF32, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_F(InstructionSelectorTest, Float64AddWithFloat64Mul) {
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
    EXPECT_EQ(kArmVmlaF64, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE(
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
    EXPECT_EQ(kArmVmlaF64, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_TRUE(
        UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
    EXPECT_EQ(kFlags_none, s[0]->flags_mode());
  }
}


TEST_F(InstructionSelectorTest, Float32SubWithFloat32Mul) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32(),
                  MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const p2 = m.Parameter(2);
  Node* const n = m.Float32Sub(p0, m.Float32Mul(p1, p2));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVmlsF32, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_TRUE(UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}


TEST_F(InstructionSelectorTest, Float64SubWithFloat64Mul) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64(), MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const p2 = m.Parameter(2);
  Node* const n = m.Float64Sub(p0, m.Float64Mul(p1, p2));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVmlsF64, s[0]->arch_opcode());
  ASSERT_EQ(3U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(2)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_TRUE(UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}


TEST_F(InstructionSelectorTest, Float32Sqrt) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float32Sqrt(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVsqrtF32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}


TEST_F(InstructionSelectorTest, Float64Sqrt) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Float64Sqrt(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVsqrtF64, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  EXPECT_EQ(kFlags_none, s[0]->flags_mode());
}

// -----------------------------------------------------------------------------
// Flag-setting instructions.

const Comparison kBinopCmpZeroRightInstructions[] = {
    {&RawMachineAssembler::Word32Equal, "Word32Equal", kEqual, kNotEqual,
     kEqual},
    {&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kNotEqual, kEqual,
     kNotEqual},
    {&RawMachineAssembler::Int32LessThan, "Int32LessThan", kNegative,
     kPositiveOrZero, kNegative},
    {&RawMachineAssembler::Int32GreaterThanOrEqual, "Int32GreaterThanOrEqual",
     kPositiveOrZero, kNegative, kPositiveOrZero},
    {&RawMachineAssembler::Uint32LessThanOrEqual, "Uint32LessThanOrEqual",
     kEqual, kNotEqual, kEqual},
    {&RawMachineAssembler::Uint32GreaterThan, "Uint32GreaterThan", kNotEqual,
     kEqual, kNotEqual}};

const Comparison kBinopCmpZeroLeftInstructions[] = {
    {&RawMachineAssembler::Word32Equal, "Word32Equal", kEqual, kNotEqual,
     kEqual},
    {&RawMachineAssembler::Word32NotEqual, "Word32NotEqual", kNotEqual, kEqual,
     kNotEqual},
    {&RawMachineAssembler::Int32GreaterThan, "Int32GreaterThan", kNegative,
     kPositiveOrZero, kNegative},
    {&RawMachineAssembler::Int32LessThanOrEqual, "Int32LessThanOrEqual",
     kPositiveOrZero, kNegative, kPositiveOrZero},
    {&RawMachineAssembler::Uint32GreaterThanOrEqual, "Uint32GreaterThanOrEqual",
     kEqual, kNotEqual, kEqual},
    {&RawMachineAssembler::Uint32LessThan, "Uint32LessThan", kNotEqual, kEqual,
     kNotEqual}};

struct FlagSettingInst {
  Constructor constructor;
  const char* constructor_name;
  ArchOpcode arch_opcode;
  ArchOpcode no_output_opcode;
};

std::ostream& operator<<(std::ostream& os, const FlagSettingInst& inst) {
  return os << inst.constructor_name;
}

const FlagSettingInst kFlagSettingInstructions[] = {
    {&RawMachineAssembler::Int32Add, "Int32Add", kArmAdd, kArmCmn},
    {&RawMachineAssembler::Word32And, "Word32And", kArmAnd, kArmTst},
    {&RawMachineAssembler::Word32Or, "Word32Or", kArmOrr, kArmOrr},
    {&RawMachineAssembler::Word32Xor, "Word32Xor", kArmEor, kArmTeq}};

typedef InstructionSelectorTestWithParam<FlagSettingInst>
    InstructionSelectorFlagSettingTest;

TEST_P(InstructionSelectorFlagSettingTest, CmpZeroRight) {
  const FlagSettingInst inst = GetParam();
  // Binop with single user : a cmp instruction.
  TRACED_FOREACH(Comparison, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* binop = (m.*inst.constructor)(m.Parameter(0), m.Parameter(1));
    Node* comp = (m.*cmp.constructor)(binop, m.Int32Constant(0));
    m.Branch(comp, &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(4U, s[0]->InputCount());  // The labels are also inputs.
    EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CmpZeroLeft) {
  const FlagSettingInst inst = GetParam();
  // Test a cmp with zero on the left-hand side.
  TRACED_FOREACH(Comparison, cmp, kBinopCmpZeroLeftInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* binop = (m.*inst.constructor)(m.Parameter(0), m.Parameter(1));
    Node* comp = (m.*cmp.constructor)(m.Int32Constant(0), binop);
    m.Branch(comp, &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(4U, s[0]->InputCount());  // The labels are also inputs.
    EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CmpZeroOnlyUserInBasicBlock) {
  const FlagSettingInst inst = GetParam();
  // Binop with additional users, but in a different basic block.
  TRACED_FOREACH(Comparison, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* binop = (m.*inst.constructor)(m.Parameter(0), m.Parameter(1));
    Node* comp = (m.*cmp.constructor)(binop, m.Int32Constant(0));
    m.Branch(comp, &a, &b);
    m.Bind(&a);
    m.Return(binop);
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(4U, s[0]->InputCount());  // The labels are also inputs.
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, ShiftedOperand) {
  const FlagSettingInst inst = GetParam();
  // Like the test above, but with a shifted input to the binary operator.
  TRACED_FOREACH(Comparison, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* imm = m.Int32Constant(5);
    Node* shift = m.Word32Shl(m.Parameter(1), imm);
    Node* binop = (m.*inst.constructor)(m.Parameter(0), shift);
    Node* comp = (m.*cmp.constructor)(binop, m.Int32Constant(0));
    m.Branch(comp, &a, &b);
    m.Bind(&a);
    m.Return(binop);
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(5U, s[0]->InputCount());  // The labels are also inputs.
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(m.Parameter(1)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(5, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, UsersInSameBasicBlock) {
  const FlagSettingInst inst = GetParam();
  // Binop with additional users, in the same basic block. We need to make sure
  // we don't try to optimise this case.
  TRACED_FOREACH(Comparison, cmp, kComparisons) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    RawMachineLabel a, b;
    Node* binop = (m.*inst.constructor)(m.Parameter(0), m.Parameter(1));
    Node* mul = m.Int32Mul(m.Parameter(0), binop);
    Node* comp = (m.*cmp.constructor)(binop, m.Int32Constant(0));
    m.Branch(comp, &a, &b);
    m.Bind(&a);
    m.Return(mul);
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(3U, s.size());
    EXPECT_EQ(inst.arch_opcode, s[0]->arch_opcode());
    EXPECT_NE(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(kArmMul, s[1]->arch_opcode());
    EXPECT_EQ(kArmCmp, s[2]->arch_opcode());
    EXPECT_EQ(kFlags_branch, s[2]->flags_mode());
    EXPECT_EQ(cmp.flags_condition, s[2]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CommuteImmediate) {
  const FlagSettingInst inst = GetParam();
  // Immediate on left hand side of the binary operator.
  TRACED_FOREACH(Comparison, cmp, kBinopCmpZeroRightInstructions) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    RawMachineLabel a, b;
    Node* imm = m.Int32Constant(3);
    Node* binop = (m.*inst.constructor)(imm, m.Parameter(0));
    Node* comp = (m.*cmp.constructor)(binop, m.Int32Constant(0));
    m.Branch(comp, &a, &b);
    m.Bind(&a);
    m.Return(m.Int32Constant(1));
    m.Bind(&b);
    m.Return(m.Int32Constant(0));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    ASSERT_EQ(4U, s[0]->InputCount());  // The labels are also inputs.
    EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(3, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(kFlags_branch, s[0]->flags_mode());
    EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
  }
}

TEST_P(InstructionSelectorFlagSettingTest, CommuteShift) {
  const FlagSettingInst inst = GetParam();
  // Left-hand side operand shifted by immediate.
  TRACED_FOREACH(Comparison, cmp, kBinopCmpZeroRightInstructions) {
    TRACED_FOREACH(Shift, shift, kShifts) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                      MachineType::Int32());
      Node* imm = m.Int32Constant(5);
      Node* shifted_operand = (m.*shift.constructor)(m.Parameter(0), imm);
      Node* binop = (m.*inst.constructor)(shifted_operand, m.Parameter(1));
      Node* comp = (m.*cmp.constructor)(binop, m.Int32Constant(0));
      m.Return(comp);
      Stream s = m.Build();
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(inst.no_output_opcode, s[0]->arch_opcode());
      EXPECT_EQ(shift.i_mode, s[0]->addressing_mode());
      EXPECT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(5, s.ToInt64(s[0]->InputAt(2)));
      EXPECT_EQ(inst.arch_opcode == kArmOrr ? 2U : 1U, s[0]->OutputCount());
      EXPECT_EQ(kFlags_set, s[0]->flags_mode());
      EXPECT_EQ(cmp.flags_condition, s[0]->flags_condition());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(InstructionSelectorTest,
                         InstructionSelectorFlagSettingTest,
                         ::testing::ValuesIn(kFlagSettingInstructions));

// -----------------------------------------------------------------------------
// Miscellaneous.


TEST_F(InstructionSelectorTest, Int32AddWithInt32Mul) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Int32Add(p0, m.Int32Mul(p1, p2));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmMla, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Int32Add(m.Int32Mul(p1, p2), p0);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmMla, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Int32AddWithInt32MulHigh) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Int32Add(p0, m.Int32MulHigh(p1, p2));
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSmmla, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const p2 = m.Parameter(2);
    Node* const n = m.Int32Add(m.Int32MulHigh(p1, p2), p0);
    m.Return(n);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSmmla, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p2), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Int32AddWithWord32And) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(m.Word32And(p0, m.Int32Constant(0xFF)), p1);
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxtab, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(p1, m.Word32And(p0, m.Int32Constant(0xFF)));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxtab, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(m.Word32And(p0, m.Int32Constant(0xFFFF)), p1);
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxtah, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(p1, m.Word32And(p0, m.Int32Constant(0xFFFF)));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxtah, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Int32AddWithWord32SarWithWord32Shl) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(
        m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(24)), m.Int32Constant(24)),
        p1);
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSxtab, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(
        p1,
        m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(24)), m.Int32Constant(24)));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSxtab, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(
        m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(16)), m.Int32Constant(16)),
        p1);
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSxtah, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const p1 = m.Parameter(1);
    Node* const r = m.Int32Add(
        p1,
        m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(16)), m.Int32Constant(16)));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSxtah, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(2)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Int32SubWithInt32Mul) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32(), MachineType::Int32());
  m.Return(
      m.Int32Sub(m.Parameter(0), m.Int32Mul(m.Parameter(1), m.Parameter(2))));
  Stream s = m.Build();
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArmMul, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArmSub, s[1]->arch_opcode());
  ASSERT_EQ(2U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(1)));
}


TEST_F(InstructionSelectorTest, Int32SubWithInt32MulForMLS) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32(), MachineType::Int32());
  m.Return(
      m.Int32Sub(m.Parameter(0), m.Int32Mul(m.Parameter(1), m.Parameter(2))));
  Stream s = m.Build(ARMv7);
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmMls, s[0]->arch_opcode());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(3U, s[0]->InputCount());
}


TEST_F(InstructionSelectorTest, Int32DivWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Int32Div(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(4U, s.size());
  EXPECT_EQ(kArmVcvtF64S32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArmVcvtF64S32, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArmVdivF64, s[2]->arch_opcode());
  ASSERT_EQ(2U, s[2]->InputCount());
  ASSERT_EQ(1U, s[2]->OutputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(1)));
  EXPECT_EQ(kArmVcvtS32F64, s[3]->arch_opcode());
  ASSERT_EQ(1U, s[3]->InputCount());
  EXPECT_EQ(s.ToVreg(s[2]->Output()), s.ToVreg(s[3]->InputAt(0)));
}


TEST_F(InstructionSelectorTest, Int32DivWithParametersForSUDIV) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Int32Div(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build(SUDIV);
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmSdiv, s[0]->arch_opcode());
}


TEST_F(InstructionSelectorTest, Int32ModWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Int32Mod(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(6U, s.size());
  EXPECT_EQ(kArmVcvtF64S32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArmVcvtF64S32, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArmVdivF64, s[2]->arch_opcode());
  ASSERT_EQ(2U, s[2]->InputCount());
  ASSERT_EQ(1U, s[2]->OutputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(1)));
  EXPECT_EQ(kArmVcvtS32F64, s[3]->arch_opcode());
  ASSERT_EQ(1U, s[3]->InputCount());
  EXPECT_EQ(s.ToVreg(s[2]->Output()), s.ToVreg(s[3]->InputAt(0)));
  EXPECT_EQ(kArmMul, s[4]->arch_opcode());
  ASSERT_EQ(1U, s[4]->OutputCount());
  ASSERT_EQ(2U, s[4]->InputCount());
  EXPECT_EQ(s.ToVreg(s[3]->Output()), s.ToVreg(s[4]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->InputAt(0)), s.ToVreg(s[4]->InputAt(1)));
  EXPECT_EQ(kArmSub, s[5]->arch_opcode());
  ASSERT_EQ(1U, s[5]->OutputCount());
  ASSERT_EQ(2U, s[5]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[5]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[4]->Output()), s.ToVreg(s[5]->InputAt(1)));
}


TEST_F(InstructionSelectorTest, Int32ModWithParametersForSUDIV) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Int32Mod(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build(SUDIV);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArmSdiv, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(kArmMul, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  ASSERT_EQ(2U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[1]->InputAt(1)));
  EXPECT_EQ(kArmSub, s[2]->arch_opcode());
  ASSERT_EQ(1U, s[2]->OutputCount());
  ASSERT_EQ(2U, s[2]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(1)));
}


TEST_F(InstructionSelectorTest, Int32ModWithParametersForSUDIVAndMLS) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Int32Mod(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build(ARMv7, SUDIV);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArmSdiv, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(kArmMls, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  ASSERT_EQ(3U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[1]->InputAt(1)));
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[1]->InputAt(2)));
}


TEST_F(InstructionSelectorTest, Int32MulWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Int32Mul(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmMul, s[0]->arch_opcode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Int32MulWithImmediate) {
  // x * (2^k + 1) -> x + (x >> k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Int32Mul(m.Parameter(0), m.Int32Constant((1 << k) + 1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmAdd, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // x * (2^k - 1) -> -x + (x >> k)
  TRACED_FORRANGE(int32_t, k, 3, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Int32Mul(m.Parameter(0), m.Int32Constant((1 << k) - 1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmRsb, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // (2^k + 1) * x -> x + (x >> k)
  TRACED_FORRANGE(int32_t, k, 1, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Int32Mul(m.Int32Constant((1 << k) + 1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmAdd, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  // x * (2^k - 1) -> -x + (x >> k)
  TRACED_FORRANGE(int32_t, k, 3, 30) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Int32Mul(m.Int32Constant((1 << k) - 1), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmRsb, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R_LSL_I, s[0]->addressing_mode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[0]->InputAt(1)));
    EXPECT_EQ(k, s.ToInt32(s[0]->InputAt(2)));
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Int32MulHighWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Int32MulHigh(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmSmmul, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}


TEST_F(InstructionSelectorTest, Uint32MulHighWithParameters) {
  StreamBuilder m(this, MachineType::Uint32(), MachineType::Uint32(),
                  MachineType::Uint32());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Uint32MulHigh(p0, p1);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmUmull, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(2U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->OutputAt(1)));
}


TEST_F(InstructionSelectorTest, Uint32DivWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Uint32Div(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(4U, s.size());
  EXPECT_EQ(kArmVcvtF64U32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArmVcvtF64U32, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArmVdivF64, s[2]->arch_opcode());
  ASSERT_EQ(2U, s[2]->InputCount());
  ASSERT_EQ(1U, s[2]->OutputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(1)));
  EXPECT_EQ(kArmVcvtU32F64, s[3]->arch_opcode());
  ASSERT_EQ(1U, s[3]->InputCount());
  EXPECT_EQ(s.ToVreg(s[2]->Output()), s.ToVreg(s[3]->InputAt(0)));
}


TEST_F(InstructionSelectorTest, Uint32DivWithParametersForSUDIV) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Uint32Div(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build(SUDIV);
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmUdiv, s[0]->arch_opcode());
}


TEST_F(InstructionSelectorTest, Uint32ModWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Uint32Mod(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(6U, s.size());
  EXPECT_EQ(kArmVcvtF64U32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArmVcvtF64U32, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArmVdivF64, s[2]->arch_opcode());
  ASSERT_EQ(2U, s[2]->InputCount());
  ASSERT_EQ(1U, s[2]->OutputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(1)));
  EXPECT_EQ(kArmVcvtU32F64, s[3]->arch_opcode());
  ASSERT_EQ(1U, s[3]->InputCount());
  EXPECT_EQ(s.ToVreg(s[2]->Output()), s.ToVreg(s[3]->InputAt(0)));
  EXPECT_EQ(kArmMul, s[4]->arch_opcode());
  ASSERT_EQ(1U, s[4]->OutputCount());
  ASSERT_EQ(2U, s[4]->InputCount());
  EXPECT_EQ(s.ToVreg(s[3]->Output()), s.ToVreg(s[4]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->InputAt(0)), s.ToVreg(s[4]->InputAt(1)));
  EXPECT_EQ(kArmSub, s[5]->arch_opcode());
  ASSERT_EQ(1U, s[5]->OutputCount());
  ASSERT_EQ(2U, s[5]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[5]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[4]->Output()), s.ToVreg(s[5]->InputAt(1)));
}


TEST_F(InstructionSelectorTest, Uint32ModWithParametersForSUDIV) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Uint32Mod(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build(SUDIV);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArmUdiv, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(kArmMul, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  ASSERT_EQ(2U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[1]->InputAt(1)));
  EXPECT_EQ(kArmSub, s[2]->arch_opcode());
  ASSERT_EQ(1U, s[2]->OutputCount());
  ASSERT_EQ(2U, s[2]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[2]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[1]->Output()), s.ToVreg(s[2]->InputAt(1)));
}


TEST_F(InstructionSelectorTest, Uint32ModWithParametersForSUDIVAndMLS) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Uint32Mod(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build(ARMv7, SUDIV);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArmUdiv, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(kArmMls, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->OutputCount());
  ASSERT_EQ(3U, s[1]->InputCount());
  EXPECT_EQ(s.ToVreg(s[0]->Output()), s.ToVreg(s[1]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(1)), s.ToVreg(s[1]->InputAt(1)));
  EXPECT_EQ(s.ToVreg(s[0]->InputAt(0)), s.ToVreg(s[1]->InputAt(2)));
}


TEST_F(InstructionSelectorTest, Word32ShlWord32SarForSbfx) {
  TRACED_FORRANGE(int32_t, shl, 1, 31) {
    TRACED_FORRANGE(int32_t, sar, shl, 31) {
      if ((shl == sar) && (sar == 16)) continue;  // Sxth.
      if ((shl == sar) && (sar == 24)) continue;  // Sxtb.
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Sar(m.Word32Shl(m.Parameter(0), m.Int32Constant(shl)),
                           m.Int32Constant(sar)));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmSbfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(sar - shl, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(32 - sar, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}


TEST_F(InstructionSelectorTest, Word32AndWithUbfxImmediateForARMv7) {
  TRACED_FORRANGE(int32_t, width, 9, 23) {
    if (width == 16) continue;  // Uxth.
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32And(m.Parameter(0),
                         m.Int32Constant(0xFFFFFFFFu >> (32 - width))));
    Stream s = m.Build(ARMv7);
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUbfx, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
  }
  TRACED_FORRANGE(int32_t, width, 9, 23) {
    if (width == 16) continue;  // Uxth.
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32And(m.Int32Constant(0xFFFFFFFFu >> (32 - width)),
                         m.Parameter(0)));
    Stream s = m.Build(ARMv7);
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUbfx, s[0]->arch_opcode());
    ASSERT_EQ(3U, s[0]->InputCount());
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
  }
}


TEST_F(InstructionSelectorTest, Word32AndWithBfcImmediateForARMv7) {
  TRACED_FORRANGE(int32_t, lsb, 0, 31) {
    TRACED_FORRANGE(int32_t, width, 9, (24 - lsb) - 1) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32And(
          m.Parameter(0),
          m.Int32Constant(~((0xFFFFFFFFu >> (32 - width)) << lsb))));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmBfc, s[0]->arch_opcode());
      ASSERT_EQ(1U, s[0]->OutputCount());
      EXPECT_TRUE(
          UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, lsb, 0, 31) {
    TRACED_FORRANGE(int32_t, width, 9, (24 - lsb) - 1) {
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(
          m.Word32And(m.Int32Constant(~((0xFFFFFFFFu >> (32 - width)) << lsb)),
                      m.Parameter(0)));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmBfc, s[0]->arch_opcode());
      ASSERT_EQ(1U, s[0]->OutputCount());
      EXPECT_TRUE(
          UnallocatedOperand::cast(s[0]->Output())->HasSameAsInputPolicy());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}

TEST_F(InstructionSelectorTest, Word32AndWith0xFFFF) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32And(p0, m.Int32Constant(0xFFFF));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxth, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32And(m.Int32Constant(0xFFFF), p0);
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxth, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word32SarWithWord32Shl) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r =
        m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(24)), m.Int32Constant(24));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSxtb, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r =
        m.Word32Sar(m.Word32Shl(p0, m.Int32Constant(16)), m.Int32Constant(16));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmSxth, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    ASSERT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(s.ToVreg(r), s.ToVreg(s[0]->Output()));
  }
}


TEST_F(InstructionSelectorTest, Word32ShrWithWord32AndWithImmediateForARMv7) {
  TRACED_FORRANGE(int32_t, lsb, 0, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t max = 1 << lsb;
      if (max > static_cast<uint32_t>(kMaxInt)) max -= 1;
      uint32_t jnk = rng()->NextInt(max);
      uint32_t msk = ((0xFFFFFFFFu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Shr(m.Word32And(m.Parameter(0), m.Int32Constant(msk)),
                           m.Int32Constant(lsb)));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmUbfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, lsb, 0, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      uint32_t max = 1 << lsb;
      if (max > static_cast<uint32_t>(kMaxInt)) max -= 1;
      uint32_t jnk = rng()->NextInt(max);
      uint32_t msk = ((0xFFFFFFFFu >> (32 - width)) << lsb) | jnk;
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32Shr(m.Word32And(m.Int32Constant(msk), m.Parameter(0)),
                           m.Int32Constant(lsb)));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmUbfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}

TEST_F(InstructionSelectorTest, Word32AndWithWord32BitwiseNot) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Word32And(m.Parameter(0), m.Word32BitwiseNot(m.Parameter(1))));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmBic, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                    MachineType::Int32());
    m.Return(m.Word32And(m.Word32BitwiseNot(m.Parameter(0)), m.Parameter(1)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmBic, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
    EXPECT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(1U, s[0]->OutputCount());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithParameters) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32(),
                  MachineType::Int32());
  m.Return(m.Word32Equal(m.Parameter(0), m.Parameter(1)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kFlags_set, s[0]->flags_mode());
  EXPECT_EQ(kEqual, s[0]->flags_condition());
}


TEST_F(InstructionSelectorTest, Word32EqualWithImmediate) {
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    if (imm == 0) continue;
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Parameter(0), m.Int32Constant(imm)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  TRACED_FOREACH(int32_t, imm, kImmediates) {
    if (imm == 0) continue;
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Int32Constant(imm), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(imm, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}


TEST_F(InstructionSelectorTest, Word32EqualWithZero) {
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Parameter(0), m.Int32Constant(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
  {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    m.Return(m.Word32Equal(m.Int32Constant(0), m.Parameter(0)));
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmCmp, s[0]->arch_opcode());
    EXPECT_EQ(kMode_Operand2_I, s[0]->addressing_mode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(s.ToVreg(m.Parameter(0)), s.ToVreg(s[0]->InputAt(0)));
    EXPECT_EQ(0, s.ToInt32(s[0]->InputAt(1)));
    EXPECT_EQ(1U, s[0]->OutputCount());
    EXPECT_EQ(kFlags_set, s[0]->flags_mode());
    EXPECT_EQ(kEqual, s[0]->flags_condition());
  }
}

TEST_F(InstructionSelectorTest, Word32BitwiseNotWithParameter) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
  m.Return(m.Word32BitwiseNot(m.Parameter(0)));
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmMvn, s[0]->arch_opcode());
  EXPECT_EQ(kMode_Operand2_R, s[0]->addressing_mode());
  EXPECT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(1U, s[0]->OutputCount());
}


TEST_F(InstructionSelectorTest, Word32AndWithWord32ShrWithImmediateForARMv7) {
  TRACED_FORRANGE(int32_t, lsb, 1, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      if (((width == 8) || (width == 16)) &&
          ((lsb == 8) || (lsb == 16) || (lsb == 24)))
        continue;  // Uxtb/h ror.
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32And(m.Word32Shr(m.Parameter(0), m.Int32Constant(lsb)),
                           m.Int32Constant(0xFFFFFFFFu >> (32 - width))));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmUbfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
  TRACED_FORRANGE(int32_t, lsb, 1, 31) {
    TRACED_FORRANGE(int32_t, width, 1, 32 - lsb) {
      if (((width == 8) || (width == 16)) &&
          ((lsb == 8) || (lsb == 16) || (lsb == 24)))
        continue;  // Uxtb/h ror.
      StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
      m.Return(m.Word32And(m.Int32Constant(0xFFFFFFFFu >> (32 - width)),
                           m.Word32Shr(m.Parameter(0), m.Int32Constant(lsb))));
      Stream s = m.Build(ARMv7);
      ASSERT_EQ(1U, s.size());
      EXPECT_EQ(kArmUbfx, s[0]->arch_opcode());
      ASSERT_EQ(3U, s[0]->InputCount());
      EXPECT_EQ(lsb, s.ToInt32(s[0]->InputAt(1)));
      EXPECT_EQ(width, s.ToInt32(s[0]->InputAt(2)));
    }
  }
}

TEST_F(InstructionSelectorTest, Word32AndWithWord32ShrAnd0xFF) {
  TRACED_FORRANGE(int32_t, shr, 1, 3) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32And(m.Word32Shr(p0, m.Int32Constant(shr * 8)),
                                m.Int32Constant(0xFF));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxtb, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(shr * 8, s.ToInt32(s[0]->InputAt(1)));
  }
  TRACED_FORRANGE(int32_t, shr, 1, 3) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32And(m.Int32Constant(0xFF),
                                m.Word32Shr(p0, m.Int32Constant(shr * 8)));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxtb, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(shr * 8, s.ToInt32(s[0]->InputAt(1)));
  }
}

TEST_F(InstructionSelectorTest, Word32AndWithWord32ShrAnd0xFFFF) {
  TRACED_FORRANGE(int32_t, shr, 1, 2) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32And(m.Word32Shr(p0, m.Int32Constant(shr * 8)),
                                m.Int32Constant(0xFFFF));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxth, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(shr * 8, s.ToInt32(s[0]->InputAt(1)));
  }
  TRACED_FORRANGE(int32_t, shr, 1, 2) {
    StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
    Node* const p0 = m.Parameter(0);
    Node* const r = m.Word32And(m.Int32Constant(0xFFFF),
                                m.Word32Shr(p0, m.Int32Constant(shr * 8)));
    m.Return(r);
    Stream s = m.Build();
    ASSERT_EQ(1U, s.size());
    EXPECT_EQ(kArmUxth, s[0]->arch_opcode());
    ASSERT_EQ(2U, s[0]->InputCount());
    EXPECT_EQ(shr * 8, s.ToInt32(s[0]->InputAt(1)));
  }
}


TEST_F(InstructionSelectorTest, Word32Clz) {
  StreamBuilder m(this, MachineType::Uint32(), MachineType::Uint32());
  Node* const p0 = m.Parameter(0);
  Node* const n = m.Word32Clz(p0);
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmClz, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Float64Max) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64(),
                  MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  Node* const p1 = m.Parameter(1);
  Node* const n = m.Float64Max(p0, p1);
  m.Return(n);
  Stream s = m.Build(ARMv8);
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmFloat64Max, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
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
  Stream s = m.Build(ARMv8);
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmFloat64Min, s[0]->arch_opcode());
  ASSERT_EQ(2U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  EXPECT_EQ(s.ToVreg(p1), s.ToVreg(s[0]->InputAt(1)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Float32Neg) {
  StreamBuilder m(this, MachineType::Float32(), MachineType::Float32());
  Node* const p0 = m.Parameter(0);
  // Don't use m.Float32Neg() as that generates an explicit sub.
  Node* const n = m.AddNode(m.machine()->Float32Neg(), m.Parameter(0));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVnegF32, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, Float64Neg) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
  Node* const p0 = m.Parameter(0);
  // Don't use m.Float64Neg() as that generates an explicit sub.
  Node* const n = m.AddNode(m.machine()->Float64Neg(), m.Parameter(0));
  m.Return(n);
  Stream s = m.Build();
  ASSERT_EQ(1U, s.size());
  EXPECT_EQ(kArmVnegF64, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->InputCount());
  EXPECT_EQ(s.ToVreg(p0), s.ToVreg(s[0]->InputAt(0)));
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(s.ToVreg(n), s.ToVreg(s[0]->Output()));
}

TEST_F(InstructionSelectorTest, StackCheck0) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
  Node* const sp = m.LoadStackPointer();
  Node* const stack_limit = m.Load(MachineType::Int32(), m.Parameter(0));
  Node* const interrupt = m.UintPtrLessThan(sp, stack_limit);

  RawMachineLabel if_true, if_false;
  m.Branch(interrupt, &if_true, &if_false);

  m.Bind(&if_true);
  m.Return(m.Int32Constant(1));

  m.Bind(&if_false);
  m.Return(m.Int32Constant(0));

  Stream s = m.Build();

  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArmLdr, s[0]->arch_opcode());
  EXPECT_EQ(kArmCmp, s[1]->arch_opcode());
  EXPECT_EQ(4U, s[1]->InputCount());
  EXPECT_EQ(0U, s[1]->OutputCount());
}

TEST_F(InstructionSelectorTest, StackCheck1) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Pointer());
  Node* const sp = m.LoadStackPointer();
  Node* const stack_limit = m.Load(MachineType::Int32(), m.Parameter(0));
  Node* const sp_within_limit = m.UintPtrLessThan(stack_limit, sp);

  RawMachineLabel if_true, if_false;
  m.Branch(sp_within_limit, &if_true, &if_false);

  m.Bind(&if_true);
  m.Return(m.Int32Constant(1));

  m.Bind(&if_false);
  m.Return(m.Int32Constant(0));

  Stream s = m.Build();

  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArmLdr, s[0]->arch_opcode());
  EXPECT_EQ(kArmCmp, s[1]->arch_opcode());
  EXPECT_EQ(4U, s[1]->InputCount());
  EXPECT_EQ(0U, s[1]->OutputCount());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
