// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/move-optimizer.h"
#include "test/unittests/compiler/instruction-sequence-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class MoveOptimizerTest : public InstructionSequenceTest {
 public:
  GapInstruction* LastGap() {
    return GapInstruction::cast(*(sequence()->instructions().rbegin() + 1));
  }

  void AddMove(GapInstruction* gap, TestOperand from, TestOperand to,
               GapInstruction::InnerPosition pos = GapInstruction::START) {
    auto parallel_move = gap->GetOrCreateParallelMove(pos, zone());
    parallel_move->AddMove(ConvertMoveArg(from), ConvertMoveArg(to), zone());
  }

  int NonRedundantSize(ParallelMove* move) {
    int i = 0;
    auto ops = move->move_operands();
    for (auto op = ops->begin(); op != ops->end(); ++op) {
      if (op->IsRedundant()) continue;
      i++;
    }
    return i;
  }

  bool Contains(ParallelMove* move, TestOperand from_op, TestOperand to_op) {
    auto from = ConvertMoveArg(from_op);
    auto to = ConvertMoveArg(to_op);
    auto ops = move->move_operands();
    for (auto op = ops->begin(); op != ops->end(); ++op) {
      if (op->IsRedundant()) continue;
      if (op->source()->Equals(from) && op->destination()->Equals(to)) {
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
  InstructionOperand* ConvertMoveArg(TestOperand op) {
    CHECK_EQ(kNoValue, op.vreg_.value_);
    CHECK_NE(kNoValue, op.value_);
    switch (op.type_) {
      case kConstant:
        return ConstantOperand::New(op.value_, zone());
      case kFixedSlot:
        return StackSlotOperand::New(op.value_, zone());
      case kFixedRegister:
        CHECK(0 <= op.value_ && op.value_ < num_general_registers());
        return RegisterOperand::New(op.value_, zone());
      default:
        break;
    }
    CHECK(false);
    return nullptr;
  }
};


TEST_F(MoveOptimizerTest, RemovesRedundant) {
  StartBlock();
  EmitNop();
  AddMove(LastGap(), Reg(0), Reg(1));
  EmitNop();
  AddMove(LastGap(), Reg(1), Reg(0));
  EndBlock(Last());

  Optimize();

  auto gap = LastGap();
  auto move = gap->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
}


TEST_F(MoveOptimizerTest, SplitsConstants) {
  StartBlock();
  EndBlock(Last());

  auto gap = LastGap();
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
  AddMove(LastGap(), Reg(0), Reg(1));

  StartBlock();
  EndBlock(Jump(1));
  AddMove(LastGap(), Reg(0), Reg(1));

  StartBlock();
  EndBlock(Last());

  Optimize();

  auto move = LastGap()->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
}


TEST_F(MoveOptimizerTest, SimpleMergeCycle) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  auto gap_0 = LastGap();
  AddMove(gap_0, Reg(0), Reg(1));
  AddMove(LastGap(), Reg(1), Reg(0));

  StartBlock();
  EndBlock(Jump(1));
  auto gap_1 = LastGap();
  AddMove(gap_1, Reg(0), Reg(1));
  AddMove(gap_1, Reg(1), Reg(0));

  StartBlock();
  EndBlock(Last());

  Optimize();

  CHECK(gap_0->IsRedundant());
  CHECK(gap_1->IsRedundant());
  auto move = LastGap()->parallel_moves()[0];
  CHECK_EQ(2, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
  CHECK(Contains(move, Reg(1), Reg(0)));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
