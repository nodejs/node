// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "test/cctest/compiler/instruction-selector-tester.h"
#include "test/cctest/compiler/value-helper.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

namespace {

typedef RawMachineAssembler::Label MLabel;

struct DPI {
  Operator* op;
  ArchOpcode arch_opcode;
  ArchOpcode reverse_arch_opcode;
  ArchOpcode test_arch_opcode;
};


// ARM data processing instructions.
class DPIs V8_FINAL : public std::list<DPI>, private HandleAndZoneScope {
 public:
  DPIs() {
    MachineOperatorBuilder machine(main_zone());
    DPI and_ = {machine.Word32And(), kArmAnd, kArmAnd, kArmTst};
    push_back(and_);
    DPI or_ = {machine.Word32Or(), kArmOrr, kArmOrr, kArmOrr};
    push_back(or_);
    DPI xor_ = {machine.Word32Xor(), kArmEor, kArmEor, kArmTeq};
    push_back(xor_);
    DPI add = {machine.Int32Add(), kArmAdd, kArmAdd, kArmCmn};
    push_back(add);
    DPI sub = {machine.Int32Sub(), kArmSub, kArmRsb, kArmCmp};
    push_back(sub);
  }
};


struct ODPI {
  Operator* op;
  ArchOpcode arch_opcode;
  ArchOpcode reverse_arch_opcode;
};


// ARM data processing instructions with overflow.
class ODPIs V8_FINAL : public std::list<ODPI>, private HandleAndZoneScope {
 public:
  ODPIs() {
    MachineOperatorBuilder machine(main_zone());
    ODPI add = {machine.Int32AddWithOverflow(), kArmAdd, kArmAdd};
    push_back(add);
    ODPI sub = {machine.Int32SubWithOverflow(), kArmSub, kArmRsb};
    push_back(sub);
  }
};


// ARM immediates.
class Immediates V8_FINAL : public std::list<int32_t> {
 public:
  Immediates() {
    for (uint32_t imm8 = 0; imm8 < 256; ++imm8) {
      for (uint32_t rot4 = 0; rot4 < 32; rot4 += 2) {
        int32_t imm = (imm8 >> rot4) | (imm8 << (32 - rot4));
        CHECK(Assembler::ImmediateFitsAddrMode1Instruction(imm));
        push_back(imm);
      }
    }
  }
};


struct Shift {
  Operator* op;
  int32_t i_low;          // lowest possible immediate
  int32_t i_high;         // highest possible immediate
  AddressingMode i_mode;  // Operand2_R_<shift>_I
  AddressingMode r_mode;  // Operand2_R_<shift>_R
};


// ARM shifts.
class Shifts V8_FINAL : public std::list<Shift>, private HandleAndZoneScope {
 public:
  Shifts() {
    MachineOperatorBuilder machine(main_zone());
    Shift sar = {machine.Word32Sar(), 1, 32, kMode_Operand2_R_ASR_I,
                 kMode_Operand2_R_ASR_R};
    Shift shl = {machine.Word32Shl(), 0, 31, kMode_Operand2_R_LSL_I,
                 kMode_Operand2_R_LSL_R};
    Shift shr = {machine.Word32Shr(), 1, 32, kMode_Operand2_R_LSR_I,
                 kMode_Operand2_R_LSR_R};
    push_back(sar);
    push_back(shl);
    push_back(shr);
  }
};

}  // namespace


TEST(InstructionSelectorDPIP) {
  DPIs dpis;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    InstructionSelectorTester m;
    m.Return(m.NewNode(dpi.op, m.Parameter(0), m.Parameter(1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(dpi.arch_opcode, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
}


TEST(InstructionSelectorDPIImm) {
  DPIs dpis;
  Immediates immediates;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    for (Immediates::const_iterator j = immediates.begin();
         j != immediates.end(); ++j) {
      int32_t imm = *j;
      {
        InstructionSelectorTester m;
        m.Return(m.NewNode(dpi.op, m.Parameter(0), m.Int32Constant(imm)));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.NewNode(dpi.op, m.Int32Constant(imm), m.Parameter(0)));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
      }
    }
  }
}


TEST(InstructionSelectorDPIAndShiftP) {
  DPIs dpis;
  Shifts shifts;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    for (Shifts::const_iterator j = shifts.begin(); j != shifts.end(); ++j) {
      Shift shift = *j;
      {
        InstructionSelectorTester m;
        m.Return(
            m.NewNode(dpi.op, m.Parameter(0),
                      m.NewNode(shift.op, m.Parameter(1), m.Parameter(2))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.NewNode(dpi.op,
                           m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)),
                           m.Parameter(2)));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
      }
    }
  }
}


TEST(InstructionSelectorDPIAndRotateRightP) {
  DPIs dpis;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    {
      InstructionSelectorTester m;
      Node* value = m.Parameter(1);
      Node* shift = m.Parameter(2);
      Node* ror = m.Word32Or(
          m.Word32Shr(value, shift),
          m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)));
      m.Return(m.NewNode(dpi.op, m.Parameter(0), ror));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      Node* value = m.Parameter(1);
      Node* shift = m.Parameter(2);
      Node* ror =
          m.Word32Or(m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)),
                     m.Word32Shr(value, shift));
      m.Return(m.NewNode(dpi.op, m.Parameter(0), ror));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      Node* value = m.Parameter(1);
      Node* shift = m.Parameter(2);
      Node* ror = m.Word32Or(
          m.Word32Shr(value, shift),
          m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)));
      m.Return(m.NewNode(dpi.op, ror, m.Parameter(0)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.reverse_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      Node* value = m.Parameter(1);
      Node* shift = m.Parameter(2);
      Node* ror =
          m.Word32Or(m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)),
                     m.Word32Shr(value, shift));
      m.Return(m.NewNode(dpi.op, ror, m.Parameter(0)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.reverse_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    }
  }
}


TEST(InstructionSelectorDPIAndShiftImm) {
  DPIs dpis;
  Shifts shifts;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    for (Shifts::const_iterator j = shifts.begin(); j != shifts.end(); ++j) {
      Shift shift = *j;
      for (int32_t imm = shift.i_low; imm <= shift.i_high; ++imm) {
        {
          InstructionSelectorTester m;
          m.Return(m.NewNode(
              dpi.op, m.Parameter(0),
              m.NewNode(shift.op, m.Parameter(1), m.Int32Constant(imm))));
          m.SelectInstructions();
          CHECK_EQ(1, m.code.size());
          CHECK_EQ(dpi.arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
        }
        {
          InstructionSelectorTester m;
          m.Return(m.NewNode(
              dpi.op, m.NewNode(shift.op, m.Parameter(0), m.Int32Constant(imm)),
              m.Parameter(1)));
          m.SelectInstructions();
          CHECK_EQ(1, m.code.size());
          CHECK_EQ(dpi.reverse_arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
        }
      }
    }
  }
}


TEST(InstructionSelectorODPIP) {
  ODPIs odpis;
  for (ODPIs::const_iterator i = odpis.begin(); i != odpis.end(); ++i) {
    ODPI odpi = *i;
    {
      InstructionSelectorTester m;
      m.Return(
          m.Projection(1, m.NewNode(odpi.op, m.Parameter(0), m.Parameter(1))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kOverflow, m.code[0]->flags_condition());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_LE(1, m.code[0]->OutputCount());
    }
    {
      InstructionSelectorTester m;
      m.Return(
          m.Projection(0, m.NewNode(odpi.op, m.Parameter(0), m.Parameter(1))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_LE(1, m.code[0]->OutputCount());
    }
    {
      InstructionSelectorTester m;
      Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Parameter(1));
      m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
      m.SelectInstructions();
      CHECK_LE(1, m.code.size());
      CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kOverflow, m.code[0]->flags_condition());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_EQ(2, m.code[0]->OutputCount());
    }
  }
}


TEST(InstructionSelectorODPIImm) {
  ODPIs odpis;
  Immediates immediates;
  for (ODPIs::const_iterator i = odpis.begin(); i != odpis.end(); ++i) {
    ODPI odpi = *i;
    for (Immediates::const_iterator j = immediates.begin();
         j != immediates.end(); ++j) {
      int32_t imm = *j;
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            1, m.NewNode(odpi.op, m.Parameter(0), m.Int32Constant(imm))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            1, m.NewNode(odpi.op, m.Int32Constant(imm), m.Parameter(0))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            0, m.NewNode(odpi.op, m.Parameter(0), m.Int32Constant(imm))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
        CHECK_EQ(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            0, m.NewNode(odpi.op, m.Int32Constant(imm), m.Parameter(0))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
        CHECK_EQ(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Int32Constant(imm));
        m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
        m.SelectInstructions();
        CHECK_LE(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_EQ(2, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        Node* node = m.NewNode(odpi.op, m.Int32Constant(imm), m.Parameter(0));
        m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
        m.SelectInstructions();
        CHECK_LE(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_EQ(2, m.code[0]->OutputCount());
      }
    }
  }
}


TEST(InstructionSelectorODPIAndShiftP) {
  ODPIs odpis;
  Shifts shifts;
  for (ODPIs::const_iterator i = odpis.begin(); i != odpis.end(); ++i) {
    ODPI odpi = *i;
    for (Shifts::const_iterator j = shifts.begin(); j != shifts.end(); ++j) {
      Shift shift = *j;
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            1, m.NewNode(odpi.op, m.Parameter(0),
                         m.NewNode(shift.op, m.Parameter(1), m.Parameter(2)))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            1, m.NewNode(odpi.op,
                         m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)),
                         m.Parameter(2))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            0, m.NewNode(odpi.op, m.Parameter(0),
                         m.NewNode(shift.op, m.Parameter(1), m.Parameter(2)))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Projection(
            0, m.NewNode(odpi.op,
                         m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)),
                         m.Parameter(2))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_LE(1, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        Node* node =
            m.NewNode(odpi.op, m.Parameter(0),
                      m.NewNode(shift.op, m.Parameter(1), m.Parameter(2)));
        m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
        m.SelectInstructions();
        CHECK_LE(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_EQ(2, m.code[0]->OutputCount());
      }
      {
        InstructionSelectorTester m;
        Node* node = m.NewNode(
            odpi.op, m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)),
            m.Parameter(2));
        m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
        m.SelectInstructions();
        CHECK_LE(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_EQ(2, m.code[0]->OutputCount());
      }
    }
  }
}


TEST(InstructionSelectorODPIAndShiftImm) {
  ODPIs odpis;
  Shifts shifts;
  for (ODPIs::const_iterator i = odpis.begin(); i != odpis.end(); ++i) {
    ODPI odpi = *i;
    for (Shifts::const_iterator j = shifts.begin(); j != shifts.end(); ++j) {
      Shift shift = *j;
      for (int32_t imm = shift.i_low; imm <= shift.i_high; ++imm) {
        {
          InstructionSelectorTester m;
          m.Return(m.Projection(1, m.NewNode(odpi.op, m.Parameter(0),
                                             m.NewNode(shift.op, m.Parameter(1),
                                                       m.Int32Constant(imm)))));
          m.SelectInstructions();
          CHECK_EQ(1, m.code.size());
          CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
          CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
          CHECK_EQ(kOverflow, m.code[0]->flags_condition());
          CHECK_EQ(3, m.code[0]->InputCount());
          CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(2)));
          CHECK_LE(1, m.code[0]->OutputCount());
        }
        {
          InstructionSelectorTester m;
          m.Return(m.Projection(
              1, m.NewNode(odpi.op, m.NewNode(shift.op, m.Parameter(0),
                                              m.Int32Constant(imm)),
                           m.Parameter(1))));
          m.SelectInstructions();
          CHECK_EQ(1, m.code.size());
          CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
          CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
          CHECK_EQ(kOverflow, m.code[0]->flags_condition());
          CHECK_EQ(3, m.code[0]->InputCount());
          CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(2)));
          CHECK_LE(1, m.code[0]->OutputCount());
        }
        {
          InstructionSelectorTester m;
          m.Return(m.Projection(0, m.NewNode(odpi.op, m.Parameter(0),
                                             m.NewNode(shift.op, m.Parameter(1),
                                                       m.Int32Constant(imm)))));
          m.SelectInstructions();
          CHECK_EQ(1, m.code.size());
          CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
          CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
          CHECK_EQ(3, m.code[0]->InputCount());
          CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(2)));
          CHECK_LE(1, m.code[0]->OutputCount());
        }
        {
          InstructionSelectorTester m;
          m.Return(m.Projection(
              0, m.NewNode(odpi.op, m.NewNode(shift.op, m.Parameter(0),
                                              m.Int32Constant(imm)),
                           m.Parameter(1))));
          m.SelectInstructions();
          CHECK_EQ(1, m.code.size());
          CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
          CHECK_EQ(kFlags_none, m.code[0]->flags_mode());
          CHECK_EQ(3, m.code[0]->InputCount());
          CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(2)));
          CHECK_LE(1, m.code[0]->OutputCount());
        }
        {
          InstructionSelectorTester m;
          Node* node = m.NewNode(
              odpi.op, m.Parameter(0),
              m.NewNode(shift.op, m.Parameter(1), m.Int32Constant(imm)));
          m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
          m.SelectInstructions();
          CHECK_LE(1, m.code.size());
          CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
          CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
          CHECK_EQ(kOverflow, m.code[0]->flags_condition());
          CHECK_EQ(3, m.code[0]->InputCount());
          CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(2)));
          CHECK_EQ(2, m.code[0]->OutputCount());
        }
        {
          InstructionSelectorTester m;
          Node* node = m.NewNode(odpi.op, m.NewNode(shift.op, m.Parameter(0),
                                                    m.Int32Constant(imm)),
                                 m.Parameter(1));
          m.Return(m.Word32Equal(m.Projection(0, node), m.Projection(1, node)));
          m.SelectInstructions();
          CHECK_LE(1, m.code.size());
          CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
          CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
          CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
          CHECK_EQ(kOverflow, m.code[0]->flags_condition());
          CHECK_EQ(3, m.code[0]->InputCount());
          CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(2)));
          CHECK_EQ(2, m.code[0]->OutputCount());
        }
      }
    }
  }
}


TEST(InstructionSelectorWord32AndAndWord32XorWithMinus1P) {
  {
    InstructionSelectorTester m;
    m.Return(m.Word32And(m.Parameter(0),
                         m.Word32Xor(m.Int32Constant(-1), m.Parameter(1))));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
  {
    InstructionSelectorTester m;
    m.Return(m.Word32And(m.Parameter(0),
                         m.Word32Xor(m.Parameter(1), m.Int32Constant(-1))));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
  {
    InstructionSelectorTester m;
    m.Return(m.Word32And(m.Word32Xor(m.Int32Constant(-1), m.Parameter(0)),
                         m.Parameter(1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
  {
    InstructionSelectorTester m;
    m.Return(m.Word32And(m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)),
                         m.Parameter(1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
}


TEST(InstructionSelectorWord32AndAndWord32XorWithMinus1AndShiftP) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    {
      InstructionSelectorTester m;
      m.Return(m.Word32And(
          m.Parameter(0),
          m.Word32Xor(m.Int32Constant(-1),
                      m.NewNode(shift.op, m.Parameter(1), m.Parameter(2)))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Word32And(
          m.Parameter(0),
          m.Word32Xor(m.NewNode(shift.op, m.Parameter(1), m.Parameter(2)),
                      m.Int32Constant(-1))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Word32And(
          m.Word32Xor(m.Int32Constant(-1),
                      m.NewNode(shift.op, m.Parameter(0), m.Parameter(1))),
          m.Parameter(2)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Word32And(
          m.Word32Xor(m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)),
                      m.Int32Constant(-1)),
          m.Parameter(2)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmBic, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    }
  }
}


TEST(InstructionSelectorWord32XorWithMinus1P) {
  {
    InstructionSelectorTester m;
    m.Return(m.Word32Xor(m.Int32Constant(-1), m.Parameter(0)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmMvn, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
  {
    InstructionSelectorTester m;
    m.Return(m.Word32Xor(m.Parameter(0), m.Int32Constant(-1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmMvn, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  }
}


TEST(InstructionSelectorWord32XorWithMinus1AndShiftP) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    {
      InstructionSelectorTester m;
      m.Return(
          m.Word32Xor(m.Int32Constant(-1),
                      m.NewNode(shift.op, m.Parameter(0), m.Parameter(1))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmMvn, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Word32Xor(m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)),
                           m.Int32Constant(-1)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmMvn, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    }
  }
}


TEST(InstructionSelectorShiftP) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    InstructionSelectorTester m;
    m.Return(m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmMov, m.code[0]->arch_opcode());
    CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
    CHECK_EQ(2, m.code[0]->InputCount());
  }
}


TEST(InstructionSelectorShiftImm) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    for (int32_t imm = shift.i_low; imm <= shift.i_high; ++imm) {
      InstructionSelectorTester m;
      m.Return(m.NewNode(shift.op, m.Parameter(0), m.Int32Constant(imm)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmMov, m.code[0]->arch_opcode());
      CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
    }
  }
}


TEST(InstructionSelectorRotateRightP) {
  {
    InstructionSelectorTester m;
    Node* value = m.Parameter(0);
    Node* shift = m.Parameter(1);
    m.Return(
        m.Word32Or(m.Word32Shr(value, shift),
                   m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift))));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmMov, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    CHECK_EQ(2, m.code[0]->InputCount());
  }
  {
    InstructionSelectorTester m;
    Node* value = m.Parameter(0);
    Node* shift = m.Parameter(1);
    m.Return(
        m.Word32Or(m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)),
                   m.Word32Shr(value, shift)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmMov, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    CHECK_EQ(2, m.code[0]->InputCount());
  }
}


TEST(InstructionSelectorRotateRightImm) {
  FOR_INPUTS(uint32_t, ror, i) {
    uint32_t shift = *i;
    {
      InstructionSelectorTester m;
      Node* value = m.Parameter(0);
      m.Return(m.Word32Or(m.Word32Shr(value, m.Int32Constant(shift)),
                          m.Word32Shl(value, m.Int32Constant(32 - shift))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmMov, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_I, m.code[0]->addressing_mode());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_EQ(shift, m.ToInt32(m.code[0]->InputAt(1)));
    }
    {
      InstructionSelectorTester m;
      Node* value = m.Parameter(0);
      m.Return(m.Word32Or(m.Word32Shl(value, m.Int32Constant(32 - shift)),
                          m.Word32Shr(value, m.Int32Constant(shift))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmMov, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_I, m.code[0]->addressing_mode());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_EQ(shift, m.ToInt32(m.code[0]->InputAt(1)));
    }
  }
}


TEST(InstructionSelectorInt32MulP) {
  InstructionSelectorTester m;
  m.Return(m.Int32Mul(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kArmMul, m.code[0]->arch_opcode());
}


TEST(InstructionSelectorInt32MulImm) {
  // x * (2^k + 1) -> (x >> k) + x
  for (int k = 1; k < 31; ++k) {
    InstructionSelectorTester m;
    m.Return(m.Int32Mul(m.Parameter(0), m.Int32Constant((1 << k) + 1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmAdd, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_LSL_I, m.code[0]->addressing_mode());
  }
  // (2^k + 1) * x -> (x >> k) + x
  for (int k = 1; k < 31; ++k) {
    InstructionSelectorTester m;
    m.Return(m.Int32Mul(m.Int32Constant((1 << k) + 1), m.Parameter(0)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmAdd, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_LSL_I, m.code[0]->addressing_mode());
  }
  // x * (2^k - 1) -> (x >> k) - x
  for (int k = 3; k < 31; ++k) {
    InstructionSelectorTester m;
    m.Return(m.Int32Mul(m.Parameter(0), m.Int32Constant((1 << k) - 1)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmRsb, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_LSL_I, m.code[0]->addressing_mode());
  }
  // (2^k - 1) * x -> (x >> k) - x
  for (int k = 3; k < 31; ++k) {
    InstructionSelectorTester m;
    m.Return(m.Int32Mul(m.Int32Constant((1 << k) - 1), m.Parameter(0)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmRsb, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_LSL_I, m.code[0]->addressing_mode());
  }
}


TEST(InstructionSelectorWord32AndImm_ARMv7) {
  for (uint32_t width = 1; width <= 32; ++width) {
    InstructionSelectorTester m;
    m.Return(m.Word32And(m.Parameter(0),
                         m.Int32Constant(0xffffffffu >> (32 - width))));
    m.SelectInstructions(ARMv7);
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmUbfx, m.code[0]->arch_opcode());
    CHECK_EQ(3, m.code[0]->InputCount());
    CHECK_EQ(0, m.ToInt32(m.code[0]->InputAt(1)));
    CHECK_EQ(width, m.ToInt32(m.code[0]->InputAt(2)));
  }
  for (uint32_t lsb = 0; lsb <= 31; ++lsb) {
    for (uint32_t width = 1; width < 32 - lsb; ++width) {
      uint32_t msk = ~((0xffffffffu >> (32 - width)) << lsb);
      InstructionSelectorTester m;
      m.Return(m.Word32And(m.Parameter(0), m.Int32Constant(msk)));
      m.SelectInstructions(ARMv7);
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmBfc, m.code[0]->arch_opcode());
      CHECK_EQ(1, m.code[0]->OutputCount());
      CHECK(UnallocatedOperand::cast(m.code[0]->Output())
                ->HasSameAsInputPolicy());
      CHECK_EQ(3, m.code[0]->InputCount());
      CHECK_EQ(lsb, m.ToInt32(m.code[0]->InputAt(1)));
      CHECK_EQ(width, m.ToInt32(m.code[0]->InputAt(2)));
    }
  }
}


TEST(InstructionSelectorWord32AndAndWord32ShrImm_ARMv7) {
  for (uint32_t lsb = 0; lsb <= 31; ++lsb) {
    for (uint32_t width = 1; width <= 32 - lsb; ++width) {
      {
        InstructionSelectorTester m;
        m.Return(m.Word32And(m.Word32Shr(m.Parameter(0), m.Int32Constant(lsb)),
                             m.Int32Constant(0xffffffffu >> (32 - width))));
        m.SelectInstructions(ARMv7);
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(kArmUbfx, m.code[0]->arch_opcode());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_EQ(lsb, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_EQ(width, m.ToInt32(m.code[0]->InputAt(2)));
      }
      {
        InstructionSelectorTester m;
        m.Return(
            m.Word32And(m.Int32Constant(0xffffffffu >> (32 - width)),
                        m.Word32Shr(m.Parameter(0), m.Int32Constant(lsb))));
        m.SelectInstructions(ARMv7);
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(kArmUbfx, m.code[0]->arch_opcode());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_EQ(lsb, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_EQ(width, m.ToInt32(m.code[0]->InputAt(2)));
      }
    }
  }
}


TEST(InstructionSelectorWord32ShrAndWord32AndImm_ARMv7) {
  for (uint32_t lsb = 0; lsb <= 31; ++lsb) {
    for (uint32_t width = 1; width <= 32 - lsb; ++width) {
      uint32_t max = 1 << lsb;
      if (max > static_cast<uint32_t>(kMaxInt)) max -= 1;
      uint32_t jnk = CcTest::random_number_generator()->NextInt(max);
      uint32_t msk = ((0xffffffffu >> (32 - width)) << lsb) | jnk;
      {
        InstructionSelectorTester m;
        m.Return(m.Word32Shr(m.Word32And(m.Parameter(0), m.Int32Constant(msk)),
                             m.Int32Constant(lsb)));
        m.SelectInstructions(ARMv7);
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(kArmUbfx, m.code[0]->arch_opcode());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_EQ(lsb, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_EQ(width, m.ToInt32(m.code[0]->InputAt(2)));
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Word32Shr(m.Word32And(m.Int32Constant(msk), m.Parameter(0)),
                             m.Int32Constant(lsb)));
        m.SelectInstructions(ARMv7);
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(kArmUbfx, m.code[0]->arch_opcode());
        CHECK_EQ(3, m.code[0]->InputCount());
        CHECK_EQ(lsb, m.ToInt32(m.code[0]->InputAt(1)));
        CHECK_EQ(width, m.ToInt32(m.code[0]->InputAt(2)));
      }
    }
  }
}


TEST(InstructionSelectorInt32SubAndInt32MulP) {
  InstructionSelectorTester m;
  m.Return(
      m.Int32Sub(m.Parameter(0), m.Int32Mul(m.Parameter(1), m.Parameter(2))));
  m.SelectInstructions();
  CHECK_EQ(2, m.code.size());
  CHECK_EQ(kArmMul, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(kArmSub, m.code[1]->arch_opcode());
  CHECK_EQ(2, m.code[1]->InputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[1]->InputAt(1));
}


TEST(InstructionSelectorInt32SubAndInt32MulP_MLS) {
  InstructionSelectorTester m;
  m.Return(
      m.Int32Sub(m.Parameter(0), m.Int32Mul(m.Parameter(1), m.Parameter(2))));
  m.SelectInstructions(MLS);
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kArmMls, m.code[0]->arch_opcode());
}


TEST(InstructionSelectorInt32DivP) {
  InstructionSelectorTester m;
  m.Return(m.Int32Div(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(4, m.code.size());
  CHECK_EQ(kArmVcvtF64S32, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(kArmVcvtF64S32, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(kArmVdivF64, m.code[2]->arch_opcode());
  CHECK_EQ(2, m.code[2]->InputCount());
  CHECK_EQ(1, m.code[2]->OutputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[2]->InputAt(0));
  CheckSameVreg(m.code[1]->Output(), m.code[2]->InputAt(1));
  CHECK_EQ(kArmVcvtS32F64, m.code[3]->arch_opcode());
  CHECK_EQ(1, m.code[3]->InputCount());
  CheckSameVreg(m.code[2]->Output(), m.code[3]->InputAt(0));
}


TEST(InstructionSelectorInt32DivP_SUDIV) {
  InstructionSelectorTester m;
  m.Return(m.Int32Div(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions(SUDIV);
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kArmSdiv, m.code[0]->arch_opcode());
}


TEST(InstructionSelectorInt32UDivP) {
  InstructionSelectorTester m;
  m.Return(m.Int32UDiv(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(4, m.code.size());
  CHECK_EQ(kArmVcvtF64U32, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(kArmVcvtF64U32, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(kArmVdivF64, m.code[2]->arch_opcode());
  CHECK_EQ(2, m.code[2]->InputCount());
  CHECK_EQ(1, m.code[2]->OutputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[2]->InputAt(0));
  CheckSameVreg(m.code[1]->Output(), m.code[2]->InputAt(1));
  CHECK_EQ(kArmVcvtU32F64, m.code[3]->arch_opcode());
  CHECK_EQ(1, m.code[3]->InputCount());
  CheckSameVreg(m.code[2]->Output(), m.code[3]->InputAt(0));
}


TEST(InstructionSelectorInt32UDivP_SUDIV) {
  InstructionSelectorTester m;
  m.Return(m.Int32UDiv(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions(SUDIV);
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kArmUdiv, m.code[0]->arch_opcode());
}


TEST(InstructionSelectorInt32ModP) {
  InstructionSelectorTester m;
  m.Return(m.Int32Mod(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(6, m.code.size());
  CHECK_EQ(kArmVcvtF64S32, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(kArmVcvtF64S32, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(kArmVdivF64, m.code[2]->arch_opcode());
  CHECK_EQ(2, m.code[2]->InputCount());
  CHECK_EQ(1, m.code[2]->OutputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[2]->InputAt(0));
  CheckSameVreg(m.code[1]->Output(), m.code[2]->InputAt(1));
  CHECK_EQ(kArmVcvtS32F64, m.code[3]->arch_opcode());
  CHECK_EQ(1, m.code[3]->InputCount());
  CheckSameVreg(m.code[2]->Output(), m.code[3]->InputAt(0));
  CHECK_EQ(kArmMul, m.code[4]->arch_opcode());
  CHECK_EQ(1, m.code[4]->OutputCount());
  CHECK_EQ(2, m.code[4]->InputCount());
  CheckSameVreg(m.code[3]->Output(), m.code[4]->InputAt(0));
  CheckSameVreg(m.code[1]->InputAt(0), m.code[4]->InputAt(1));
  CHECK_EQ(kArmSub, m.code[5]->arch_opcode());
  CHECK_EQ(1, m.code[5]->OutputCount());
  CHECK_EQ(2, m.code[5]->InputCount());
  CheckSameVreg(m.code[0]->InputAt(0), m.code[5]->InputAt(0));
  CheckSameVreg(m.code[4]->Output(), m.code[5]->InputAt(1));
}


TEST(InstructionSelectorInt32ModP_SUDIV) {
  InstructionSelectorTester m;
  m.Return(m.Int32Mod(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions(SUDIV);
  CHECK_EQ(3, m.code.size());
  CHECK_EQ(kArmSdiv, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(2, m.code[0]->InputCount());
  CHECK_EQ(kArmMul, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(2, m.code[1]->InputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[1]->InputAt(0));
  CheckSameVreg(m.code[0]->InputAt(1), m.code[1]->InputAt(1));
  CHECK_EQ(kArmSub, m.code[2]->arch_opcode());
  CHECK_EQ(1, m.code[2]->OutputCount());
  CHECK_EQ(2, m.code[2]->InputCount());
  CheckSameVreg(m.code[0]->InputAt(0), m.code[2]->InputAt(0));
  CheckSameVreg(m.code[1]->Output(), m.code[2]->InputAt(1));
}


TEST(InstructionSelectorInt32ModP_MLS_SUDIV) {
  InstructionSelectorTester m;
  m.Return(m.Int32Mod(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions(MLS, SUDIV);
  CHECK_EQ(2, m.code.size());
  CHECK_EQ(kArmSdiv, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(2, m.code[0]->InputCount());
  CHECK_EQ(kArmMls, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(3, m.code[1]->InputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[1]->InputAt(0));
  CheckSameVreg(m.code[0]->InputAt(1), m.code[1]->InputAt(1));
  CheckSameVreg(m.code[0]->InputAt(0), m.code[1]->InputAt(2));
}


TEST(InstructionSelectorInt32UModP) {
  InstructionSelectorTester m;
  m.Return(m.Int32UMod(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(6, m.code.size());
  CHECK_EQ(kArmVcvtF64U32, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(kArmVcvtF64U32, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(kArmVdivF64, m.code[2]->arch_opcode());
  CHECK_EQ(2, m.code[2]->InputCount());
  CHECK_EQ(1, m.code[2]->OutputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[2]->InputAt(0));
  CheckSameVreg(m.code[1]->Output(), m.code[2]->InputAt(1));
  CHECK_EQ(kArmVcvtU32F64, m.code[3]->arch_opcode());
  CHECK_EQ(1, m.code[3]->InputCount());
  CheckSameVreg(m.code[2]->Output(), m.code[3]->InputAt(0));
  CHECK_EQ(kArmMul, m.code[4]->arch_opcode());
  CHECK_EQ(1, m.code[4]->OutputCount());
  CHECK_EQ(2, m.code[4]->InputCount());
  CheckSameVreg(m.code[3]->Output(), m.code[4]->InputAt(0));
  CheckSameVreg(m.code[1]->InputAt(0), m.code[4]->InputAt(1));
  CHECK_EQ(kArmSub, m.code[5]->arch_opcode());
  CHECK_EQ(1, m.code[5]->OutputCount());
  CHECK_EQ(2, m.code[5]->InputCount());
  CheckSameVreg(m.code[0]->InputAt(0), m.code[5]->InputAt(0));
  CheckSameVreg(m.code[4]->Output(), m.code[5]->InputAt(1));
}


TEST(InstructionSelectorInt32UModP_SUDIV) {
  InstructionSelectorTester m;
  m.Return(m.Int32UMod(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions(SUDIV);
  CHECK_EQ(3, m.code.size());
  CHECK_EQ(kArmUdiv, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(2, m.code[0]->InputCount());
  CHECK_EQ(kArmMul, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(2, m.code[1]->InputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[1]->InputAt(0));
  CheckSameVreg(m.code[0]->InputAt(1), m.code[1]->InputAt(1));
  CHECK_EQ(kArmSub, m.code[2]->arch_opcode());
  CHECK_EQ(1, m.code[2]->OutputCount());
  CHECK_EQ(2, m.code[2]->InputCount());
  CheckSameVreg(m.code[0]->InputAt(0), m.code[2]->InputAt(0));
  CheckSameVreg(m.code[1]->Output(), m.code[2]->InputAt(1));
}


TEST(InstructionSelectorInt32UModP_MLS_SUDIV) {
  InstructionSelectorTester m;
  m.Return(m.Int32UMod(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions(MLS, SUDIV);
  CHECK_EQ(2, m.code.size());
  CHECK_EQ(kArmUdiv, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
  CHECK_EQ(2, m.code[0]->InputCount());
  CHECK_EQ(kArmMls, m.code[1]->arch_opcode());
  CHECK_EQ(1, m.code[1]->OutputCount());
  CHECK_EQ(3, m.code[1]->InputCount());
  CheckSameVreg(m.code[0]->Output(), m.code[1]->InputAt(0));
  CheckSameVreg(m.code[0]->InputAt(1), m.code[1]->InputAt(1));
  CheckSameVreg(m.code[0]->InputAt(0), m.code[1]->InputAt(2));
}


TEST(InstructionSelectorWord32EqualP) {
  InstructionSelectorTester m;
  m.Return(m.Word32Equal(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
  CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
  CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
  CHECK_EQ(kEqual, m.code[0]->flags_condition());
}


TEST(InstructionSelectorWord32EqualImm) {
  Immediates immediates;
  for (Immediates::const_iterator i = immediates.begin(); i != immediates.end();
       ++i) {
    int32_t imm = *i;
    {
      InstructionSelectorTester m;
      m.Return(m.Word32Equal(m.Parameter(0), m.Int32Constant(imm)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      if (imm == 0) {
        CHECK_EQ(kArmTst, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
        CHECK_EQ(2, m.code[0]->InputCount());
        CheckSameVreg(m.code[0]->InputAt(0), m.code[0]->InputAt(1));
      } else {
        CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
      }
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Word32Equal(m.Int32Constant(imm), m.Parameter(0)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      if (imm == 0) {
        CHECK_EQ(kArmTst, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
        CHECK_EQ(2, m.code[0]->InputCount());
        CheckSameVreg(m.code[0]->InputAt(0), m.code[0]->InputAt(1));
      } else {
        CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
      }
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
  }
}


TEST(InstructionSelectorWord32EqualAndDPIP) {
  DPIs dpis;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    {
      InstructionSelectorTester m;
      m.Return(m.Word32Equal(m.NewNode(dpi.op, m.Parameter(0), m.Parameter(1)),
                             m.Int32Constant(0)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      m.Return(
          m.Word32Equal(m.Int32Constant(0),
                        m.NewNode(dpi.op, m.Parameter(0), m.Parameter(1))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
  }
}


TEST(InstructionSelectorWord32EqualAndDPIImm) {
  DPIs dpis;
  Immediates immediates;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    for (Immediates::const_iterator j = immediates.begin();
         j != immediates.end(); ++j) {
      int32_t imm = *j;
      {
        InstructionSelectorTester m;
        m.Return(m.Word32Equal(
            m.NewNode(dpi.op, m.Parameter(0), m.Int32Constant(imm)),
            m.Int32Constant(0)));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kEqual, m.code[0]->flags_condition());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Word32Equal(
            m.NewNode(dpi.op, m.Int32Constant(imm), m.Parameter(0)),
            m.Int32Constant(0)));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kEqual, m.code[0]->flags_condition());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Word32Equal(
            m.Int32Constant(0),
            m.NewNode(dpi.op, m.Parameter(0), m.Int32Constant(imm))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kEqual, m.code[0]->flags_condition());
      }
      {
        InstructionSelectorTester m;
        m.Return(m.Word32Equal(
            m.Int32Constant(0),
            m.NewNode(dpi.op, m.Int32Constant(imm), m.Parameter(0))));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
        CHECK_EQ(kEqual, m.code[0]->flags_condition());
      }
    }
  }
}


TEST(InstructionSelectorWord32EqualAndShiftP) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    {
      InstructionSelectorTester m;
      m.Return(m.Word32Equal(
          m.Parameter(0), m.NewNode(shift.op, m.Parameter(1), m.Parameter(2))));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Word32Equal(
          m.NewNode(shift.op, m.Parameter(0), m.Parameter(1)), m.Parameter(2)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_set, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
  }
}


TEST(InstructionSelectorBranchWithWord32EqualAndShiftP) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      m.Branch(m.Word32Equal(m.Parameter(0), m.NewNode(shift.op, m.Parameter(1),
                                                       m.Parameter(2))),
               &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      m.Branch(
          m.Word32Equal(m.NewNode(shift.op, m.Parameter(1), m.Parameter(2)),
                        m.Parameter(0)),
          &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(shift.r_mode, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
  }
}


TEST(InstructionSelectorBranchWithWord32EqualAndShiftImm) {
  Shifts shifts;
  for (Shifts::const_iterator i = shifts.begin(); i != shifts.end(); ++i) {
    Shift shift = *i;
    for (int32_t imm = shift.i_low; imm <= shift.i_high; ++imm) {
      {
        InstructionSelectorTester m;
        MLabel blocka, blockb;
        m.Branch(
            m.Word32Equal(m.Parameter(0), m.NewNode(shift.op, m.Parameter(1),
                                                    m.Int32Constant(imm))),
            &blocka, &blockb);
        m.Bind(&blocka);
        m.Return(m.Int32Constant(1));
        m.Bind(&blockb);
        m.Return(m.Int32Constant(0));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
        CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
        CHECK_EQ(kEqual, m.code[0]->flags_condition());
      }
      {
        InstructionSelectorTester m;
        MLabel blocka, blockb;
        m.Branch(m.Word32Equal(
                     m.NewNode(shift.op, m.Parameter(1), m.Int32Constant(imm)),
                     m.Parameter(0)),
                 &blocka, &blockb);
        m.Bind(&blocka);
        m.Return(m.Int32Constant(1));
        m.Bind(&blockb);
        m.Return(m.Int32Constant(0));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
        CHECK_EQ(shift.i_mode, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
        CHECK_EQ(kEqual, m.code[0]->flags_condition());
      }
    }
  }
}


TEST(InstructionSelectorBranchWithWord32EqualAndRotateRightP) {
  {
    InstructionSelectorTester m;
    MLabel blocka, blockb;
    Node* input = m.Parameter(0);
    Node* value = m.Parameter(1);
    Node* shift = m.Parameter(2);
    Node* ror =
        m.Word32Or(m.Word32Shr(value, shift),
                   m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)));
    m.Branch(m.Word32Equal(input, ror), &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(1));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(0));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
    CHECK_EQ(kEqual, m.code[0]->flags_condition());
  }
  {
    InstructionSelectorTester m;
    MLabel blocka, blockb;
    Node* input = m.Parameter(0);
    Node* value = m.Parameter(1);
    Node* shift = m.Parameter(2);
    Node* ror =
        m.Word32Or(m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)),
                   m.Word32Shr(value, shift));
    m.Branch(m.Word32Equal(input, ror), &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(1));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(0));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
    CHECK_EQ(kEqual, m.code[0]->flags_condition());
  }
  {
    InstructionSelectorTester m;
    MLabel blocka, blockb;
    Node* input = m.Parameter(0);
    Node* value = m.Parameter(1);
    Node* shift = m.Parameter(2);
    Node* ror =
        m.Word32Or(m.Word32Shr(value, shift),
                   m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)));
    m.Branch(m.Word32Equal(ror, input), &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(1));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(0));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
    CHECK_EQ(kEqual, m.code[0]->flags_condition());
  }
  {
    InstructionSelectorTester m;
    MLabel blocka, blockb;
    Node* input = m.Parameter(0);
    Node* value = m.Parameter(1);
    Node* shift = m.Parameter(2);
    Node* ror =
        m.Word32Or(m.Word32Shl(value, m.Int32Sub(m.Int32Constant(32), shift)),
                   m.Word32Shr(value, shift));
    m.Branch(m.Word32Equal(ror, input), &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(1));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(0));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
    CHECK_EQ(kMode_Operand2_R_ROR_R, m.code[0]->addressing_mode());
    CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
    CHECK_EQ(kEqual, m.code[0]->flags_condition());
  }
}


TEST(InstructionSelectorBranchWithWord32EqualAndRotateRightImm) {
  FOR_INPUTS(uint32_t, ror, i) {
    uint32_t shift = *i;
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* input = m.Parameter(0);
      Node* value = m.Parameter(1);
      Node* ror = m.Word32Or(m.Word32Shr(value, m.Int32Constant(shift)),
                             m.Word32Shl(value, m.Int32Constant(32 - shift)));
      m.Branch(m.Word32Equal(input, ror), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_I, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
      CHECK_LE(3, m.code[0]->InputCount());
      CHECK_EQ(shift, m.ToInt32(m.code[0]->InputAt(2)));
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* input = m.Parameter(0);
      Node* value = m.Parameter(1);
      Node* ror = m.Word32Or(m.Word32Shl(value, m.Int32Constant(32 - shift)),
                             m.Word32Shr(value, m.Int32Constant(shift)));
      m.Branch(m.Word32Equal(input, ror), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_I, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
      CHECK_LE(3, m.code[0]->InputCount());
      CHECK_EQ(shift, m.ToInt32(m.code[0]->InputAt(2)));
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* input = m.Parameter(0);
      Node* value = m.Parameter(1);
      Node* ror = m.Word32Or(m.Word32Shr(value, m.Int32Constant(shift)),
                             m.Word32Shl(value, m.Int32Constant(32 - shift)));
      m.Branch(m.Word32Equal(ror, input), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_I, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
      CHECK_LE(3, m.code[0]->InputCount());
      CHECK_EQ(shift, m.ToInt32(m.code[0]->InputAt(2)));
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* input = m.Parameter(0);
      Node* value = m.Parameter(1);
      Node* ror = m.Word32Or(m.Word32Shl(value, m.Int32Constant(32 - shift)),
                             m.Word32Shr(value, m.Int32Constant(shift)));
      m.Branch(m.Word32Equal(ror, input), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kArmCmp, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R_ROR_I, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
      CHECK_LE(3, m.code[0]->InputCount());
      CHECK_EQ(shift, m.ToInt32(m.code[0]->InputAt(2)));
    }
  }
}


TEST(InstructionSelectorBranchWithDPIP) {
  DPIs dpis;
  for (DPIs::const_iterator i = dpis.begin(); i != dpis.end(); ++i) {
    DPI dpi = *i;
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      m.Branch(m.NewNode(dpi.op, m.Parameter(0), m.Parameter(1)), &blocka,
               &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kNotEqual, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      m.Branch(m.Word32Equal(m.Int32Constant(0),
                             m.NewNode(dpi.op, m.Parameter(0), m.Parameter(1))),
               &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      m.Branch(m.Word32Equal(m.NewNode(dpi.op, m.Parameter(0), m.Parameter(1)),
                             m.Int32Constant(0)),
               &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(1));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(0));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(dpi.test_arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kEqual, m.code[0]->flags_condition());
    }
  }
}


TEST(InstructionSelectorBranchWithODPIP) {
  ODPIs odpis;
  for (ODPIs::const_iterator i = odpis.begin(); i != odpis.end(); ++i) {
    ODPI odpi = *i;
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Parameter(1));
      m.Branch(m.Projection(1, node), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(0));
      m.Bind(&blockb);
      m.Return(m.Projection(0, node));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kOverflow, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Parameter(1));
      m.Branch(m.Word32Equal(m.Projection(1, node), m.Int32Constant(0)),
               &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(0));
      m.Bind(&blockb);
      m.Return(m.Projection(0, node));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kNotOverflow, m.code[0]->flags_condition());
    }
    {
      InstructionSelectorTester m;
      MLabel blocka, blockb;
      Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Parameter(1));
      m.Branch(m.Word32Equal(m.Int32Constant(0), m.Projection(1, node)),
               &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(0));
      m.Bind(&blockb);
      m.Return(m.Projection(0, node));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
      CHECK_EQ(kMode_Operand2_R, m.code[0]->addressing_mode());
      CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
      CHECK_EQ(kNotOverflow, m.code[0]->flags_condition());
    }
  }
}


TEST(InstructionSelectorBranchWithODPIImm) {
  ODPIs odpis;
  Immediates immediates;
  for (ODPIs::const_iterator i = odpis.begin(); i != odpis.end(); ++i) {
    ODPI odpi = *i;
    for (Immediates::const_iterator j = immediates.begin();
         j != immediates.end(); ++j) {
      int32_t imm = *j;
      {
        InstructionSelectorTester m;
        MLabel blocka, blockb;
        Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Int32Constant(imm));
        m.Branch(m.Projection(1, node), &blocka, &blockb);
        m.Bind(&blocka);
        m.Return(m.Int32Constant(0));
        m.Bind(&blockb);
        m.Return(m.Projection(0, node));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_LE(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
      }
      {
        InstructionSelectorTester m;
        MLabel blocka, blockb;
        Node* node = m.NewNode(odpi.op, m.Int32Constant(imm), m.Parameter(0));
        m.Branch(m.Projection(1, node), &blocka, &blockb);
        m.Bind(&blocka);
        m.Return(m.Int32Constant(0));
        m.Bind(&blockb);
        m.Return(m.Projection(0, node));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
        CHECK_EQ(kOverflow, m.code[0]->flags_condition());
        CHECK_LE(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
      }
      {
        InstructionSelectorTester m;
        MLabel blocka, blockb;
        Node* node = m.NewNode(odpi.op, m.Parameter(0), m.Int32Constant(imm));
        m.Branch(m.Word32Equal(m.Projection(1, node), m.Int32Constant(0)),
                 &blocka, &blockb);
        m.Bind(&blocka);
        m.Return(m.Int32Constant(0));
        m.Bind(&blockb);
        m.Return(m.Projection(0, node));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
        CHECK_EQ(kNotOverflow, m.code[0]->flags_condition());
        CHECK_LE(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
      }
      {
        InstructionSelectorTester m;
        MLabel blocka, blockb;
        Node* node = m.NewNode(odpi.op, m.Int32Constant(imm), m.Parameter(0));
        m.Branch(m.Word32Equal(m.Projection(1, node), m.Int32Constant(0)),
                 &blocka, &blockb);
        m.Bind(&blocka);
        m.Return(m.Int32Constant(0));
        m.Bind(&blockb);
        m.Return(m.Projection(0, node));
        m.SelectInstructions();
        CHECK_EQ(1, m.code.size());
        CHECK_EQ(odpi.reverse_arch_opcode, m.code[0]->arch_opcode());
        CHECK_EQ(kMode_Operand2_I, m.code[0]->addressing_mode());
        CHECK_EQ(kFlags_branch, m.code[0]->flags_mode());
        CHECK_EQ(kNotOverflow, m.code[0]->flags_condition());
        CHECK_LE(2, m.code[0]->InputCount());
        CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
      }
    }
  }
}
