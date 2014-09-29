// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/instruction-selector-tester.h"
#include "test/cctest/compiler/value-helper.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

TEST(InstructionSelectorInt32AddP) {
  InstructionSelectorTester m;
  m.Return(m.Int32Add(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kIA32Add, m.code[0]->arch_opcode());
}


TEST(InstructionSelectorInt32AddImm) {
  FOR_INT32_INPUTS(i) {
    int32_t imm = *i;
    {
      InstructionSelectorTester m;
      m.Return(m.Int32Add(m.Parameter(0), m.Int32Constant(imm)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kIA32Add, m.code[0]->arch_opcode());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
    }
    {
      InstructionSelectorTester m;
      m.Return(m.Int32Add(m.Int32Constant(imm), m.Parameter(0)));
      m.SelectInstructions();
      CHECK_EQ(1, m.code.size());
      CHECK_EQ(kIA32Add, m.code[0]->arch_opcode());
      CHECK_EQ(2, m.code[0]->InputCount());
      CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
    }
  }
}


TEST(InstructionSelectorInt32SubP) {
  InstructionSelectorTester m;
  m.Return(m.Int32Sub(m.Parameter(0), m.Parameter(1)));
  m.SelectInstructions();
  CHECK_EQ(1, m.code.size());
  CHECK_EQ(kIA32Sub, m.code[0]->arch_opcode());
  CHECK_EQ(1, m.code[0]->OutputCount());
}


TEST(InstructionSelectorInt32SubImm) {
  FOR_INT32_INPUTS(i) {
    int32_t imm = *i;
    InstructionSelectorTester m;
    m.Return(m.Int32Sub(m.Parameter(0), m.Int32Constant(imm)));
    m.SelectInstructions();
    CHECK_EQ(1, m.code.size());
    CHECK_EQ(kIA32Sub, m.code[0]->arch_opcode());
    CHECK_EQ(2, m.code[0]->InputCount());
    CHECK_EQ(imm, m.ToInt32(m.code[0]->InputAt(1)));
  }
}
