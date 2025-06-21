// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-scheduler.h"
#include "src/compiler/backend/instruction-selector-impl.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/backend/instruction.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

using FlagsContinuation = FlagsContinuationT;

// Create InstructionBlocks with a single block.
InstructionBlocks* CreateSingleBlock(Zone* zone) {
  InstructionBlock* block = zone->New<InstructionBlock>(
      zone, RpoNumber::FromInt(0), RpoNumber::Invalid(), RpoNumber::Invalid(),
      RpoNumber::Invalid(), false, false);
  InstructionBlocks* blocks = zone->AllocateArray<InstructionBlocks>(1);
  new (blocks) InstructionBlocks(1, block, zone);
  return blocks;
}

// Wrapper around the InstructionScheduler.
class InstructionSchedulerTester {
 public:
  InstructionSchedulerTester()
      : scope_(kCompressGraphZone),
        blocks_(CreateSingleBlock(scope_.main_zone())),
        sequence_(scope_.main_isolate(), scope_.main_zone(), blocks_),
        scheduler_(scope_.main_zone(), &sequence_) {}

  void StartBlock() { scheduler_.StartBlock(RpoNumber::FromInt(0)); }
  void EndBlock() { scheduler_.EndBlock(RpoNumber::FromInt(0)); }
  void AddInstruction(Instruction* instr) { scheduler_.AddInstruction(instr); }
  void AddTerminator(Instruction* instr) { scheduler_.AddTerminator(instr); }

  void CheckHasSideEffect(Instruction* instr) {
    CHECK(scheduler_.HasSideEffect(instr));
  }
  void CheckIsDeopt(Instruction* instr) { CHECK(instr->IsDeoptimizeCall()); }

  void CheckInSuccessors(Instruction* instr, Instruction* successor) {
    InstructionScheduler::ScheduleGraphNode* node = GetNode(instr);
    InstructionScheduler::ScheduleGraphNode* succ_node = GetNode(successor);

    ZoneDeque<InstructionScheduler::ScheduleGraphNode*>& successors =
        node->successors();
    CHECK_NE(std::find(successors.begin(), successors.end(), succ_node),
             successors.end());
  }

  Zone* zone() { return scope_.main_zone(); }

 private:
  InstructionScheduler::ScheduleGraphNode* GetNode(Instruction* instr) {
    for (auto node : scheduler_.graph_) {
      if (node->instruction() == instr) return node;
    }
    return nullptr;
  }

  HandleAndZoneScope scope_;
  InstructionBlocks* blocks_;
  InstructionSequence sequence_;
  InstructionScheduler scheduler_;
};

// TODO(391750831, nicohartmann): Currently broken with Turboshaft backend.
// Needs to be ported.
#if 0
TEST(DeoptInMiddleOfBasicBlock) {
  InstructionSchedulerTester tester;
  Zone* zone = tester.zone();

  tester.StartBlock();
  InstructionCode jmp_opcode = kArchJmp;
  Node* dummy_frame_state = Node::New(zone, 0, nullptr, 0, nullptr, false);
  FlagsContinuation cont = FlagsContinuation::ForDeoptimizeForTesting(
      kEqual, DeoptimizeReason::kUnknown, dummy_frame_state->id(),
      FeedbackSource{}, dummy_frame_state);
  jmp_opcode = cont.Encode(jmp_opcode);
  Instruction* jmp_inst = Instruction::New(zone, jmp_opcode);
  tester.CheckIsDeopt(jmp_inst);
  tester.AddInstruction(jmp_inst);
  Instruction* side_effect_inst = Instruction::New(zone, kArchPrepareTailCall);
  tester.CheckHasSideEffect(side_effect_inst);
  tester.AddInstruction(side_effect_inst);
  Instruction* other_jmp_inst = Instruction::New(zone, jmp_opcode);
  tester.CheckIsDeopt(other_jmp_inst);
  tester.AddInstruction(other_jmp_inst);
  Instruction* ret_inst = Instruction::New(zone, kArchRet);
  tester.AddTerminator(ret_inst);

  // Check that an instruction with a side effect is a successor of the deopt.
  tester.CheckInSuccessors(jmp_inst, side_effect_inst);
  // Check that the second deopt is a successor of the first deopt.
  tester.CheckInSuccessors(jmp_inst, other_jmp_inst);
  // Check that the second deopt is a successor of the side-effect instruction.
  tester.CheckInSuccessors(side_effect_inst, other_jmp_inst);
  // Check that the block terminator is a successor of all other instructions.
  tester.CheckInSuccessors(jmp_inst, ret_inst);
  tester.CheckInSuccessors(side_effect_inst, ret_inst);
  tester.CheckInSuccessors(other_jmp_inst, ret_inst);

  // Schedule block.
  tester.EndBlock();
}
#endif

}  // namespace compiler
}  // namespace internal
}  // namespace v8
