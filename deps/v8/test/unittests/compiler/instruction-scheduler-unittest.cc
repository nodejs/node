// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-scheduler.h"

#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/backend/instruction.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler {

namespace instruction_scheduler_unittest {

// Create InstructionBlocks with a single block.
InstructionBlocks* CreateSingleBlock(Zone* zone) {
  InstructionBlock* block = zone->New<InstructionBlock>(
      zone, RpoNumber::FromInt(0), RpoNumber::Invalid(), RpoNumber::Invalid(),
      RpoNumber::Invalid(), false, false);
  InstructionBlocks* blocks = zone->AllocateArray<InstructionBlocks>(1);
  new (blocks) InstructionBlocks(1, block, zone);
  return blocks;
}

}  // namespace instruction_scheduler_unittest

// Wrapper around the InstructionScheduler. Kept in v8::internal::compiler to
// match `friend class InstructionSchedulerTester` in InstructionScheduler.
class InstructionSchedulerTester {
 public:
  InstructionSchedulerTester(Isolate* isolate, Zone* zone)
      : zone_(zone),
        blocks_(instruction_scheduler_unittest::CreateSingleBlock(zone)),
        sequence_(isolate, zone, blocks_),
        scheduler_(zone, &sequence_) {}

  void StartBlock() { scheduler_.StartBlock(RpoNumber::FromInt(0)); }
  void EndBlock(Instruction* instr) {
    scheduler_.EndBlock(RpoNumber::FromInt(0), instr);
  }
  void AddInstruction(Instruction* instr) { scheduler_.AddInstruction(instr); }

  void CheckHasSideEffect(Instruction* instr) {
    int flags = scheduler_.GetInstructionFlags(instr);
    CHECK(scheduler_.HasSideEffect(flags));
  }
  void CheckIsDeopt(Instruction* instr) { CHECK(instr->IsDeoptimizeCall()); }

  void CheckInSuccessors(Instruction* instr, Instruction* successor) {
    InstructionScheduler::ScheduleGraphNode* node = GetNode(instr);
    InstructionScheduler::ScheduleGraphNode* succ_node = GetNode(successor);

    InstructionScheduler::ScheduleGraphNode::SuccessorList& successors =
        node->successors();
    CHECK_NE(std::find(successors.begin(), successors.end(), succ_node),
             successors.end());
  }

  Zone* zone() { return zone_; }

 private:
  InstructionScheduler::ScheduleGraphNode* GetNode(Instruction* instr) {
    for (auto node : scheduler_.graph_) {
      if (node->instruction() == instr) return node;
    }
    return nullptr;
  }

  Zone* zone_;
  InstructionBlocks* blocks_;
  InstructionSequence sequence_;
  InstructionScheduler scheduler_;
};

namespace instruction_scheduler_unittest {

using InstructionSchedulerTest = TestWithIsolateAndZone;

TEST_F(InstructionSchedulerTest, DeoptInMiddleOfBasicBlock) {
  InstructionSchedulerTester tester(i_isolate(), zone());

  tester.StartBlock();
  InstructionCode jmp_opcode = kArchJmp;
  FlagsContinuation cont = FlagsContinuation::ForDeoptimizeForTesting(
      kEqual, DeoptimizeReason::kUnknown, 0, FeedbackSource{});
  jmp_opcode = cont.Encode(jmp_opcode);
  Instruction* jmp_inst = Instruction::New(zone(), jmp_opcode);
  tester.CheckIsDeopt(jmp_inst);
  tester.AddInstruction(jmp_inst);
  Instruction* side_effect_inst =
      Instruction::New(zone(), kArchPrepareTailCall);
  tester.CheckHasSideEffect(side_effect_inst);
  tester.AddInstruction(side_effect_inst);
  Instruction* other_jmp_inst = Instruction::New(zone(), jmp_opcode);
  tester.CheckIsDeopt(other_jmp_inst);
  tester.AddInstruction(other_jmp_inst);

  // Check that an instruction with a side effect is a successor of the deopt.
  tester.CheckInSuccessors(jmp_inst, side_effect_inst);
  // Check that the second deopt is a successor of the first deopt.
  tester.CheckInSuccessors(jmp_inst, other_jmp_inst);
  // Check that the second deopt is a successor of the side-effect instruction.
  tester.CheckInSuccessors(side_effect_inst, other_jmp_inst);

  // Schedule block.
  Instruction* ret_inst = Instruction::New(zone(), kArchRet);
  tester.EndBlock(ret_inst);
}

}  // namespace instruction_scheduler_unittest
}  // namespace v8::internal::compiler
