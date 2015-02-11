// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/compiler-unittests/instruction-selector-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

InstructionSelectorTest::Stream InstructionSelectorTest::StreamBuilder::Build(
    InstructionSelector::Features features,
    InstructionSelectorTest::StreamBuilderMode mode) {
  Schedule* schedule = Export();
  EXPECT_NE(0, graph()->NodeCount());
  CompilationInfo info(test_->isolate(), test_->zone());
  Linkage linkage(&info, call_descriptor());
  InstructionSequence sequence(&linkage, graph(), schedule);
  SourcePositionTable source_position_table(graph());
  InstructionSelector selector(&sequence, &source_position_table, features);
  selector.SelectInstructions();
  if (FLAG_trace_turbo) {
    OFStream out(stdout);
    out << "--- Code sequence after instruction selection ---" << endl
        << sequence;
  }
  Stream s;
  for (InstructionSequence::const_iterator i = sequence.begin();
       i != sequence.end(); ++i) {
    Instruction* instr = *i;
    if (instr->opcode() < 0) continue;
    if (mode == kTargetInstructions) {
      switch (instr->arch_opcode()) {
#define CASE(Name) \
  case k##Name:    \
    break;
        TARGET_ARCH_OPCODE_LIST(CASE)
#undef CASE
        default:
          continue;
      }
    }
    for (size_t i = 0; i < instr->OutputCount(); ++i) {
      InstructionOperand* output = instr->OutputAt(i);
      EXPECT_NE(InstructionOperand::IMMEDIATE, output->kind());
      if (output->IsConstant()) {
        s.constants_.insert(std::make_pair(
            output->index(), sequence.GetConstant(output->index())));
      }
    }
    for (size_t i = 0; i < instr->InputCount(); ++i) {
      InstructionOperand* input = instr->InputAt(i);
      EXPECT_NE(InstructionOperand::CONSTANT, input->kind());
      if (input->IsImmediate()) {
        s.immediates_.insert(std::make_pair(
            input->index(), sequence.GetImmediate(input->index())));
      }
    }
    s.instructions_.push_back(instr);
  }
  return s;
}


COMPILER_TEST_F(InstructionSelectorTest, ReturnP) {
  StreamBuilder m(this, kMachineWord32, kMachineWord32);
  m.Return(m.Parameter(0));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
}


COMPILER_TEST_F(InstructionSelectorTest, ReturnImm) {
  StreamBuilder m(this, kMachineWord32);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(InstructionOperand::CONSTANT, s[0]->OutputAt(0)->kind());
  EXPECT_EQ(0, s.ToInt32(s[0]->OutputAt(0)));
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
