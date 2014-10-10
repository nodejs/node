// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/lithium.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

typedef v8::internal::compiler::Instruction TestInstr;
typedef v8::internal::compiler::InstructionSequence TestInstrSeq;

// A testing helper for the register code abstraction.
class InstructionTester : public HandleAndZoneScope {
 public:  // We're all friends here.
  InstructionTester()
      : isolate(main_isolate()),
        graph(zone()),
        schedule(zone()),
        info(static_cast<HydrogenCodeStub*>(NULL), main_isolate()),
        linkage(&info),
        common(zone()),
        code(NULL) {}

  ~InstructionTester() { delete code; }

  Isolate* isolate;
  Graph graph;
  Schedule schedule;
  CompilationInfoWithZone info;
  Linkage linkage;
  CommonOperatorBuilder common;
  MachineOperatorBuilder machine;
  TestInstrSeq* code;

  Zone* zone() { return main_zone(); }

  void allocCode() {
    if (schedule.rpo_order()->size() == 0) {
      // Compute the RPO order.
      Scheduler::ComputeSpecialRPO(&schedule);
      DCHECK(schedule.rpo_order()->size() > 0);
    }
    code = new TestInstrSeq(&linkage, &graph, &schedule);
  }

  Node* Int32Constant(int32_t val) {
    Node* node = graph.NewNode(common.Int32Constant(val));
    schedule.AddNode(schedule.start(), node);
    return node;
  }

  Node* Float64Constant(double val) {
    Node* node = graph.NewNode(common.Float64Constant(val));
    schedule.AddNode(schedule.start(), node);
    return node;
  }

  Node* Parameter(int32_t which) {
    Node* node = graph.NewNode(common.Parameter(which));
    schedule.AddNode(schedule.start(), node);
    return node;
  }

  Node* NewNode(BasicBlock* block) {
    Node* node = graph.NewNode(common.Int32Constant(111));
    schedule.AddNode(block, node);
    return node;
  }

  int NewInstr(BasicBlock* block) {
    InstructionCode opcode = static_cast<InstructionCode>(110);
    TestInstr* instr = TestInstr::New(zone(), opcode);
    return code->AddInstruction(instr, block);
  }

  UnallocatedOperand* NewUnallocated(int vreg) {
    UnallocatedOperand* unallocated =
        new (zone()) UnallocatedOperand(UnallocatedOperand::ANY);
    unallocated->set_virtual_register(vreg);
    return unallocated;
  }
};


TEST(InstructionBasic) {
  InstructionTester R;

  for (int i = 0; i < 10; i++) {
    R.Int32Constant(i);  // Add some nodes to the graph.
  }

  BasicBlock* last = R.schedule.start();
  for (int i = 0; i < 5; i++) {
    BasicBlock* block = R.schedule.NewBasicBlock();
    R.schedule.AddGoto(last, block);
    last = block;
  }

  R.allocCode();

  CHECK_EQ(R.graph.NodeCount(), R.code->ValueCount());

  BasicBlockVector* blocks = R.schedule.rpo_order();
  CHECK_EQ(static_cast<int>(blocks->size()), R.code->BasicBlockCount());

  int index = 0;
  for (BasicBlockVectorIter i = blocks->begin(); i != blocks->end();
       i++, index++) {
    BasicBlock* block = *i;
    CHECK_EQ(block, R.code->BlockAt(index));
    CHECK_EQ(-1, R.code->GetLoopEnd(block));
  }
}


TEST(InstructionGetBasicBlock) {
  InstructionTester R;

  BasicBlock* b0 = R.schedule.start();
  BasicBlock* b1 = R.schedule.NewBasicBlock();
  BasicBlock* b2 = R.schedule.NewBasicBlock();
  BasicBlock* b3 = R.schedule.end();

  R.schedule.AddGoto(b0, b1);
  R.schedule.AddGoto(b1, b2);
  R.schedule.AddGoto(b2, b3);

  R.allocCode();

  R.code->StartBlock(b0);
  int i0 = R.NewInstr(b0);
  int i1 = R.NewInstr(b0);
  R.code->EndBlock(b0);
  R.code->StartBlock(b1);
  int i2 = R.NewInstr(b1);
  int i3 = R.NewInstr(b1);
  int i4 = R.NewInstr(b1);
  int i5 = R.NewInstr(b1);
  R.code->EndBlock(b1);
  R.code->StartBlock(b2);
  int i6 = R.NewInstr(b2);
  int i7 = R.NewInstr(b2);
  int i8 = R.NewInstr(b2);
  R.code->EndBlock(b2);
  R.code->StartBlock(b3);
  R.code->EndBlock(b3);

  CHECK_EQ(b0, R.code->GetBasicBlock(i0));
  CHECK_EQ(b0, R.code->GetBasicBlock(i1));

  CHECK_EQ(b1, R.code->GetBasicBlock(i2));
  CHECK_EQ(b1, R.code->GetBasicBlock(i3));
  CHECK_EQ(b1, R.code->GetBasicBlock(i4));
  CHECK_EQ(b1, R.code->GetBasicBlock(i5));

  CHECK_EQ(b2, R.code->GetBasicBlock(i6));
  CHECK_EQ(b2, R.code->GetBasicBlock(i7));
  CHECK_EQ(b2, R.code->GetBasicBlock(i8));

  CHECK_EQ(b0, R.code->GetBasicBlock(b0->first_instruction_index()));
  CHECK_EQ(b0, R.code->GetBasicBlock(b0->last_instruction_index()));

  CHECK_EQ(b1, R.code->GetBasicBlock(b1->first_instruction_index()));
  CHECK_EQ(b1, R.code->GetBasicBlock(b1->last_instruction_index()));

  CHECK_EQ(b2, R.code->GetBasicBlock(b2->first_instruction_index()));
  CHECK_EQ(b2, R.code->GetBasicBlock(b2->last_instruction_index()));

  CHECK_EQ(b3, R.code->GetBasicBlock(b3->first_instruction_index()));
  CHECK_EQ(b3, R.code->GetBasicBlock(b3->last_instruction_index()));
}


TEST(InstructionIsGapAt) {
  InstructionTester R;

  BasicBlock* b0 = R.schedule.start();
  R.schedule.AddReturn(b0, R.Int32Constant(1));

  R.allocCode();
  TestInstr* i0 = TestInstr::New(R.zone(), 100);
  TestInstr* g = TestInstr::New(R.zone(), 103)->MarkAsControl();
  R.code->StartBlock(b0);
  R.code->AddInstruction(i0, b0);
  R.code->AddInstruction(g, b0);
  R.code->EndBlock(b0);

  CHECK_EQ(true, R.code->InstructionAt(0)->IsBlockStart());

  CHECK_EQ(true, R.code->IsGapAt(0));   // Label
  CHECK_EQ(true, R.code->IsGapAt(1));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(2));  // i0
  CHECK_EQ(true, R.code->IsGapAt(3));   // Gap
  CHECK_EQ(true, R.code->IsGapAt(4));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(5));  // g
}


TEST(InstructionIsGapAt2) {
  InstructionTester R;

  BasicBlock* b0 = R.schedule.start();
  BasicBlock* b1 = R.schedule.end();
  R.schedule.AddGoto(b0, b1);
  R.schedule.AddReturn(b1, R.Int32Constant(1));

  R.allocCode();
  TestInstr* i0 = TestInstr::New(R.zone(), 100);
  TestInstr* g = TestInstr::New(R.zone(), 103)->MarkAsControl();
  R.code->StartBlock(b0);
  R.code->AddInstruction(i0, b0);
  R.code->AddInstruction(g, b0);
  R.code->EndBlock(b0);

  TestInstr* i1 = TestInstr::New(R.zone(), 102);
  TestInstr* g1 = TestInstr::New(R.zone(), 104)->MarkAsControl();
  R.code->StartBlock(b1);
  R.code->AddInstruction(i1, b1);
  R.code->AddInstruction(g1, b1);
  R.code->EndBlock(b1);

  CHECK_EQ(true, R.code->InstructionAt(0)->IsBlockStart());

  CHECK_EQ(true, R.code->IsGapAt(0));   // Label
  CHECK_EQ(true, R.code->IsGapAt(1));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(2));  // i0
  CHECK_EQ(true, R.code->IsGapAt(3));   // Gap
  CHECK_EQ(true, R.code->IsGapAt(4));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(5));  // g

  CHECK_EQ(true, R.code->InstructionAt(6)->IsBlockStart());

  CHECK_EQ(true, R.code->IsGapAt(6));    // Label
  CHECK_EQ(true, R.code->IsGapAt(7));    // Gap
  CHECK_EQ(false, R.code->IsGapAt(8));   // i1
  CHECK_EQ(true, R.code->IsGapAt(9));    // Gap
  CHECK_EQ(true, R.code->IsGapAt(10));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(11));  // g1
}


TEST(InstructionAddGapMove) {
  InstructionTester R;

  BasicBlock* b0 = R.schedule.start();
  R.schedule.AddReturn(b0, R.Int32Constant(1));

  R.allocCode();
  TestInstr* i0 = TestInstr::New(R.zone(), 100);
  TestInstr* g = TestInstr::New(R.zone(), 103)->MarkAsControl();
  R.code->StartBlock(b0);
  R.code->AddInstruction(i0, b0);
  R.code->AddInstruction(g, b0);
  R.code->EndBlock(b0);

  CHECK_EQ(true, R.code->InstructionAt(0)->IsBlockStart());

  CHECK_EQ(true, R.code->IsGapAt(0));   // Label
  CHECK_EQ(true, R.code->IsGapAt(1));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(2));  // i0
  CHECK_EQ(true, R.code->IsGapAt(3));   // Gap
  CHECK_EQ(true, R.code->IsGapAt(4));   // Gap
  CHECK_EQ(false, R.code->IsGapAt(5));  // g

  int indexes[] = {0, 1, 3, 4, -1};
  for (int i = 0; indexes[i] >= 0; i++) {
    int index = indexes[i];

    UnallocatedOperand* op1 = R.NewUnallocated(index + 6);
    UnallocatedOperand* op2 = R.NewUnallocated(index + 12);

    R.code->AddGapMove(index, op1, op2);
    GapInstruction* gap = R.code->GapAt(index);
    ParallelMove* move = gap->GetParallelMove(GapInstruction::START);
    CHECK_NE(NULL, move);
    const ZoneList<MoveOperands>* move_operands = move->move_operands();
    CHECK_EQ(1, move_operands->length());
    MoveOperands* cur = &move_operands->at(0);
    CHECK_EQ(op1, cur->source());
    CHECK_EQ(op2, cur->destination());
  }
}


TEST(InstructionOperands) {
  Zone zone(CcTest::InitIsolateOnce());

  {
    TestInstr* i = TestInstr::New(&zone, 101);
    CHECK_EQ(0, static_cast<int>(i->OutputCount()));
    CHECK_EQ(0, static_cast<int>(i->InputCount()));
    CHECK_EQ(0, static_cast<int>(i->TempCount()));
  }

  InstructionOperand* outputs[] = {
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER)};

  InstructionOperand* inputs[] = {
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER)};

  InstructionOperand* temps[] = {
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER),
      new (&zone) UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER)};

  for (size_t i = 0; i < arraysize(outputs); i++) {
    for (size_t j = 0; j < arraysize(inputs); j++) {
      for (size_t k = 0; k < arraysize(temps); k++) {
        TestInstr* m =
            TestInstr::New(&zone, 101, i, outputs, j, inputs, k, temps);
        CHECK(i == m->OutputCount());
        CHECK(j == m->InputCount());
        CHECK(k == m->TempCount());

        for (size_t z = 0; z < i; z++) {
          CHECK_EQ(outputs[z], m->OutputAt(z));
        }

        for (size_t z = 0; z < j; z++) {
          CHECK_EQ(inputs[z], m->InputAt(z));
        }

        for (size_t z = 0; z < k; z++) {
          CHECK_EQ(temps[z], m->TempAt(z));
        }
      }
    }
  }
}
