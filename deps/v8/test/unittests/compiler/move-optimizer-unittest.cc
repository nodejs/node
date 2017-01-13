// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/move-optimizer.h"
#include "src/compiler/pipeline.h"
#include "test/unittests/compiler/instruction-sequence-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class MoveOptimizerTest : public InstructionSequenceTest {
 public:
  Instruction* LastInstruction() { return sequence()->instructions().back(); }

  void AddMove(Instruction* instr, TestOperand from, TestOperand to,
               Instruction::GapPosition pos = Instruction::START) {
    auto parallel_move = instr->GetOrCreateParallelMove(pos, zone());
    parallel_move->AddMove(ConvertMoveArg(from), ConvertMoveArg(to));
  }

  int NonRedundantSize(ParallelMove* moves) {
    int i = 0;
    for (auto move : *moves) {
      if (move->IsRedundant()) continue;
      i++;
    }
    return i;
  }

  bool Contains(ParallelMove* moves, TestOperand from_op, TestOperand to_op) {
    auto from = ConvertMoveArg(from_op);
    auto to = ConvertMoveArg(to_op);
    for (auto move : *moves) {
      if (move->IsRedundant()) continue;
      if (move->source().Equals(from) && move->destination().Equals(to)) {
        return true;
      }
    }
    return false;
  }

  // TODO(dcarney): add a verifier.
  void Optimize() {
    WireBlocks();
    if (FLAG_trace_turbo) {
      OFStream os(stdout);
      PrintableInstructionSequence printable = {config(), sequence()};
      os << "----- Instruction sequence before move optimization -----\n"
         << printable;
    }
    MoveOptimizer move_optimizer(zone(), sequence());
    move_optimizer.Run();
    if (FLAG_trace_turbo) {
      OFStream os(stdout);
      PrintableInstructionSequence printable = {config(), sequence()};
      os << "----- Instruction sequence after move optimization -----\n"
         << printable;
    }
  }

 private:
  InstructionOperand ConvertMoveArg(TestOperand op) {
    CHECK_EQ(kNoValue, op.vreg_.value_);
    CHECK_NE(kNoValue, op.value_);
    switch (op.type_) {
      case kConstant:
        return ConstantOperand(op.value_);
      case kFixedSlot:
        return AllocatedOperand(LocationOperand::STACK_SLOT,
                                MachineRepresentation::kWord32, op.value_);
      case kFixedRegister:
        CHECK(0 <= op.value_ && op.value_ < num_general_registers());
        return AllocatedOperand(LocationOperand::REGISTER,
                                MachineRepresentation::kWord32, op.value_);
      case kExplicit:
        CHECK(0 <= op.value_ && op.value_ < num_general_registers());
        return ExplicitOperand(LocationOperand::REGISTER,
                               MachineRepresentation::kWord32, op.value_);
      default:
        break;
    }
    CHECK(false);
    return InstructionOperand();
  }
};


TEST_F(MoveOptimizerTest, RemovesRedundant) {
  StartBlock();
  auto first_instr = EmitNop();
  AddMove(first_instr, Reg(0), Reg(1));
  auto last_instr = EmitNop();
  AddMove(last_instr, Reg(1), Reg(0));
  EndBlock(Last());

  Optimize();

  CHECK_EQ(0, NonRedundantSize(first_instr->parallel_moves()[0]));
  auto move = last_instr->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
}


TEST_F(MoveOptimizerTest, RemovesRedundantExplicit) {
  int first_reg_index =
      RegisterConfiguration::Turbofan()->GetAllocatableGeneralCode(0);
  int second_reg_index =
      RegisterConfiguration::Turbofan()->GetAllocatableGeneralCode(1);

  StartBlock();
  auto first_instr = EmitNop();
  AddMove(first_instr, Reg(first_reg_index), ExplicitReg(second_reg_index));
  auto last_instr = EmitNop();
  AddMove(last_instr, Reg(second_reg_index), Reg(first_reg_index));
  EndBlock(Last());

  Optimize();

  CHECK_EQ(0, NonRedundantSize(first_instr->parallel_moves()[0]));
  auto move = last_instr->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, Reg(first_reg_index), ExplicitReg(second_reg_index)));
}


TEST_F(MoveOptimizerTest, SplitsConstants) {
  StartBlock();
  EndBlock(Last());

  auto gap = LastInstruction();
  AddMove(gap, Const(1), Slot(0));
  AddMove(gap, Const(1), Slot(1));
  AddMove(gap, Const(1), Reg(0));
  AddMove(gap, Const(1), Slot(2));

  Optimize();

  auto move = gap->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, Const(1), Reg(0)));

  move = gap->parallel_moves()[1];
  CHECK_EQ(3, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Slot(0)));
  CHECK(Contains(move, Reg(0), Slot(1)));
  CHECK(Contains(move, Reg(0), Slot(2)));
}


TEST_F(MoveOptimizerTest, SimpleMerge) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  AddMove(LastInstruction(), Reg(0), Reg(1));

  StartBlock();
  EndBlock(Jump(1));
  AddMove(LastInstruction(), Reg(0), Reg(1));

  StartBlock();
  EndBlock(Last());

  auto last = LastInstruction();

  Optimize();

  auto move = last->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
}


TEST_F(MoveOptimizerTest, SimpleMergeCycle) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  auto gap_0 = LastInstruction();
  AddMove(gap_0, Reg(0), Reg(1));
  AddMove(LastInstruction(), Reg(1), Reg(0));

  StartBlock();
  EndBlock(Jump(1));
  auto gap_1 = LastInstruction();
  AddMove(gap_1, Reg(0), Reg(1));
  AddMove(gap_1, Reg(1), Reg(0));

  StartBlock();
  EndBlock(Last());

  auto last = LastInstruction();

  Optimize();

  CHECK(gap_0->AreMovesRedundant());
  CHECK(gap_1->AreMovesRedundant());
  auto move = last->parallel_moves()[0];
  CHECK_EQ(2, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
  CHECK(Contains(move, Reg(1), Reg(0)));
}


TEST_F(MoveOptimizerTest, GapsCanMoveOverInstruction) {
  StartBlock();
  int const_index = 1;
  DefineConstant(const_index);
  Instruction* ctant_def = LastInstruction();
  AddMove(ctant_def, Reg(1), Reg(0));

  Instruction* last = EmitNop();
  AddMove(last, Const(const_index), Reg(0));
  AddMove(last, Reg(0), Reg(1));
  EndBlock(Last());
  Optimize();

  ParallelMove* inst1_start =
      ctant_def->GetParallelMove(Instruction::GapPosition::START);
  ParallelMove* inst1_end =
      ctant_def->GetParallelMove(Instruction::GapPosition::END);
  ParallelMove* last_start =
      last->GetParallelMove(Instruction::GapPosition::START);
  CHECK(inst1_start == nullptr || NonRedundantSize(inst1_start) == 0);
  CHECK(inst1_end == nullptr || NonRedundantSize(inst1_end) == 0);
  CHECK(last_start->size() == 2);
  int redundants = 0;
  int assignment = 0;
  for (MoveOperands* move : *last_start) {
    if (move->IsRedundant()) {
      ++redundants;
    } else {
      ++assignment;
      CHECK(move->destination().IsRegister());
      CHECK(move->source().IsConstant());
    }
  }
  CHECK_EQ(1, redundants);
  CHECK_EQ(1, assignment);
}


TEST_F(MoveOptimizerTest, SubsetMovesMerge) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  Instruction* last_move_b1 = LastInstruction();
  AddMove(last_move_b1, Reg(0), Reg(1));
  AddMove(last_move_b1, Reg(2), Reg(3));

  StartBlock();
  EndBlock(Jump(1));
  Instruction* last_move_b2 = LastInstruction();
  AddMove(last_move_b2, Reg(0), Reg(1));
  AddMove(last_move_b2, Reg(4), Reg(5));

  StartBlock();
  EndBlock(Last());

  Instruction* last = LastInstruction();

  Optimize();

  ParallelMove* last_move = last->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(last_move));
  CHECK(Contains(last_move, Reg(0), Reg(1)));

  ParallelMove* b1_move = last_move_b1->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(b1_move));
  CHECK(Contains(b1_move, Reg(2), Reg(3)));

  ParallelMove* b2_move = last_move_b2->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(b2_move));
  CHECK(Contains(b2_move, Reg(4), Reg(5)));
}


TEST_F(MoveOptimizerTest, GapConflictSubsetMovesDoNotMerge) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  Instruction* last_move_b1 = LastInstruction();
  AddMove(last_move_b1, Reg(0), Reg(1));
  AddMove(last_move_b1, Reg(2), Reg(0));
  AddMove(last_move_b1, Reg(4), Reg(5));

  StartBlock();
  EndBlock(Jump(1));
  Instruction* last_move_b2 = LastInstruction();
  AddMove(last_move_b2, Reg(0), Reg(1));
  AddMove(last_move_b2, Reg(4), Reg(5));

  StartBlock();
  EndBlock(Last());

  Instruction* last = LastInstruction();

  Optimize();

  ParallelMove* last_move = last->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(last_move));
  CHECK(Contains(last_move, Reg(4), Reg(5)));

  ParallelMove* b1_move = last_move_b1->parallel_moves()[0];
  CHECK_EQ(2, NonRedundantSize(b1_move));
  CHECK(Contains(b1_move, Reg(0), Reg(1)));
  CHECK(Contains(b1_move, Reg(2), Reg(0)));

  ParallelMove* b2_move = last_move_b2->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(b2_move));
  CHECK(Contains(b1_move, Reg(0), Reg(1)));
}

TEST_F(MoveOptimizerTest, ClobberedDestinationsAreEliminated) {
  StartBlock();
  EmitNop();
  Instruction* first_instr = LastInstruction();
  AddMove(first_instr, Reg(0), Reg(1));
  EmitOI(Reg(1), 0, nullptr);
  Instruction* last_instr = LastInstruction();
  EndBlock();
  Optimize();

  ParallelMove* first_move = first_instr->parallel_moves()[0];
  CHECK_EQ(0, NonRedundantSize(first_move));

  ParallelMove* last_move = last_instr->parallel_moves()[0];
  CHECK_EQ(0, NonRedundantSize(last_move));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
