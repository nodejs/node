// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/move-optimizer.h"

#include "src/compiler/backend/register-allocator-verifier.h"
#include "src/utils/ostreams.h"
#include "test/unittests/compiler/backend/instruction-sequence-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class MoveOptimizerTest : public InstructionSequenceTest {
 public:
  // FP register indices which don't interfere under simple or complex aliasing.
  static const int kF64_1 = 0;
  static const int kF64_2 = 1;
  static const int kF32_1 = 4;
  static const int kF32_2 = 5;
  static const int kS128_1 = 2;
  static const int kS128_2 = 3;

  Instruction* LastInstruction() { return sequence()->instructions().back(); }

  // Unlike Const(), which encodes the virtual register only, this registers
  // an actual constant value for it, as required by paths that inspect the
  // value (such as the one-instruction-constant heuristic on arm64). Unlike
  // InstructionSequenceTest::DefineConstant(), it emits no instruction.
  TestOperand ConstWithValue(int32_t value) {
    int virtual_register = sequence()->NextVirtualRegister();
    sequence()->AddConstant(virtual_register, Constant(value));
    return Const(virtual_register);
  }

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
    if (v8_flags.trace_turbo) {
      StdoutStream{}
          << "----- Instruction sequence before move optimization -----\n"
          << *sequence();
    }
    MoveOptimizer move_optimizer(zone(), sequence());
    move_optimizer.Run();
    if (v8_flags.trace_turbo) {
      StdoutStream{}
          << "----- Instruction sequence after move optimization -----\n"
          << *sequence();
    }
  }

 private:
  bool DoesRegisterAllocation() const override { return false; }

  InstructionOperand ConvertMoveArg(TestOperand op) {
    CHECK_EQ(kNoValue, op.vreg_.value_);
    CHECK_NE(kNoValue, op.value_);
    switch (op.type_) {
      case kConstant:
        return ConstantOperand(op.value_);
      case kFixedSlot: {
        MachineRepresentation rep =
            op.rep_ == kNoRep ? MachineRepresentation::kWord32 : op.rep_;
        return AllocatedOperand(LocationOperand::STACK_SLOT, rep, op.value_);
      }
      case kFixedRegister: {
        MachineRepresentation rep = GetCanonicalRep(op);
        CHECK(0 <= op.value_ && op.value_ < GetNumRegs(rep));
        return AllocatedOperand(LocationOperand::REGISTER, rep, op.value_);
      }
      default:
        break;
    }
    UNREACHABLE();
  }
};

TEST_F(MoveOptimizerTest, RemovesRedundant) {
  StartBlock();
  auto first_instr = EmitNop();
  auto last_instr = EmitNop();

  AddMove(first_instr, Reg(0), Reg(1));
  AddMove(last_instr, Reg(1), Reg(0));

  AddMove(first_instr, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128));
  AddMove(last_instr, FPReg(kS128_2, kSimd128), FPReg(kS128_1, kSimd128));
  AddMove(first_instr, FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64));
  AddMove(last_instr, FPReg(kF64_2, kFloat64), FPReg(kF64_1, kFloat64));
  AddMove(first_instr, FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32));
  AddMove(last_instr, FPReg(kF32_2, kFloat32), FPReg(kF32_1, kFloat32));

  EndBlock(Last());

  Optimize();

  CHECK_EQ(0, NonRedundantSize(first_instr->parallel_moves()[0]));
  auto move = last_instr->parallel_moves()[0];
  CHECK_EQ(4, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
  CHECK(Contains(move, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128)));
  CHECK(Contains(move, FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64)));
  CHECK(Contains(move, FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32)));
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

TEST_F(MoveOptimizerTest, KeepsCheapConstantsIndependent) {
  StartBlock();
  EndBlock(Last());

  auto gap = LastInstruction();
  TestOperand cheap = ConstWithValue(1);
  AddMove(gap, cheap, Reg(0));
  AddMove(gap, cheap, Reg(1));

  Optimize();

  auto move = gap->parallel_moves()[0];
#if V8_TARGET_ARCH_ARM64
  // A one-instruction constant rematerializes into each register; grouping
  // would only make the second move depend on the first.
  CHECK_EQ(2, NonRedundantSize(move));
  CHECK(Contains(move, cheap, Reg(0)));
  CHECK(Contains(move, cheap, Reg(1)));
  auto follow = gap->parallel_moves()[1];
  CHECK(follow == nullptr || NonRedundantSize(follow) == 0);
#else
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, cheap, Reg(0)));
  auto follow = gap->parallel_moves()[1];
  CHECK_EQ(1, NonRedundantSize(follow));
  CHECK(Contains(follow, Reg(0), Reg(1)));
#endif
}

TEST_F(MoveOptimizerTest, GroupsExpensiveConstants) {
  StartBlock();
  EndBlock(Last());

  auto gap = LastInstruction();
  // 0x12345678 is no single MOVZ/MOVN/ORR-immediate on arm64, so grouping
  // still pays off: materialize once, then copy.
  TestOperand expensive = ConstWithValue(0x12345678);
  AddMove(gap, expensive, Reg(0));
  AddMove(gap, expensive, Reg(1));

  Optimize();

  auto move = gap->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(move));
  CHECK(Contains(move, expensive, Reg(0)));
  auto follow = gap->parallel_moves()[1];
  CHECK_EQ(1, NonRedundantSize(follow));
  CHECK(Contains(follow, Reg(0), Reg(1)));
}

TEST_F(MoveOptimizerTest, SimpleMerge) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  AddMove(LastInstruction(), Reg(0), Reg(1));
  AddMove(LastInstruction(), FPReg(kS128_1, kSimd128),
          FPReg(kS128_2, kSimd128));
  AddMove(LastInstruction(), FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64));
  AddMove(LastInstruction(), FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32));

  StartBlock();
  EndBlock(Jump(1));
  AddMove(LastInstruction(), Reg(0), Reg(1));
  AddMove(LastInstruction(), FPReg(kS128_1, kSimd128),
          FPReg(kS128_2, kSimd128));
  AddMove(LastInstruction(), FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64));
  AddMove(LastInstruction(), FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32));

  StartBlock();
  EndBlock(Last());

  auto last = LastInstruction();

  Optimize();

  auto move = last->parallel_moves()[0];
  CHECK_EQ(4, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
  CHECK(Contains(move, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128)));
  CHECK(Contains(move, FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64)));
  CHECK(Contains(move, FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32)));
}

TEST_F(MoveOptimizerTest, SimpleMergeCycle) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  auto gap_0 = LastInstruction();
  AddMove(gap_0, Reg(0), Reg(1));
  AddMove(LastInstruction(), Reg(1), Reg(0));

  AddMove(gap_0, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128));
  AddMove(LastInstruction(), FPReg(kS128_2, kSimd128),
          FPReg(kS128_1, kSimd128));
  AddMove(gap_0, FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64));
  AddMove(LastInstruction(), FPReg(kF64_2, kFloat64), FPReg(kF64_1, kFloat64));
  AddMove(gap_0, FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32));
  AddMove(LastInstruction(), FPReg(kF32_2, kFloat32), FPReg(kF32_1, kFloat32));

  StartBlock();
  EndBlock(Jump(1));
  auto gap_1 = LastInstruction();
  AddMove(gap_1, Reg(0), Reg(1));
  AddMove(gap_1, Reg(1), Reg(0));
  AddMove(gap_1, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128));
  AddMove(gap_1, FPReg(kS128_2, kSimd128), FPReg(kS128_1, kSimd128));
  AddMove(gap_1, FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64));
  AddMove(gap_1, FPReg(kF64_2, kFloat64), FPReg(kF64_1, kFloat64));
  AddMove(gap_1, FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32));
  AddMove(gap_1, FPReg(kF32_2, kFloat32), FPReg(kF32_1, kFloat32));

  StartBlock();
  EndBlock(Last());

  auto last = LastInstruction();

  Optimize();

  CHECK(gap_0->AreMovesRedundant());
  CHECK(gap_1->AreMovesRedundant());
  auto move = last->parallel_moves()[0];
  CHECK_EQ(8, NonRedundantSize(move));
  CHECK(Contains(move, Reg(0), Reg(1)));
  CHECK(Contains(move, Reg(1), Reg(0)));
  CHECK(Contains(move, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128)));
  CHECK(Contains(move, FPReg(kS128_2, kSimd128), FPReg(kS128_1, kSimd128)));
  CHECK(Contains(move, FPReg(kF64_1, kFloat64), FPReg(kF64_2, kFloat64)));
  CHECK(Contains(move, FPReg(kF64_2, kFloat64), FPReg(kF64_1, kFloat64)));
  CHECK(Contains(move, FPReg(kF32_1, kFloat32), FPReg(kF32_2, kFloat32)));
  CHECK(Contains(move, FPReg(kF32_2, kFloat32), FPReg(kF32_1, kFloat32)));
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
  CHECK_EQ(2, last_start->size());
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

TEST_F(MoveOptimizerTest, ClobberedFPDestinationsAreEliminated) {
  StartBlock();
  EmitNop();
  Instruction* first_instr = LastInstruction();
  AddMove(first_instr, FPReg(4, kFloat64), FPReg(1, kFloat64));
  if (kFPAliasing == AliasingKind::kCombine) {
    // We clobber q0 below. This is aliased by d0, d1, s0, s1, s2, and s3.
    // Add moves to registers s2 and s3.
    AddMove(first_instr, FPReg(10, kFloat32), FPReg(0, kFloat32));
    AddMove(first_instr, FPReg(11, kFloat32), FPReg(1, kFloat32));
  }
  // Clobbers output register 0.
  EmitOI(FPReg(0, kSimd128), 0, nullptr);
  Instruction* last_instr = LastInstruction();
  EndBlock();
  Optimize();

  ParallelMove* first_move = first_instr->parallel_moves()[0];
  CHECK_EQ(0, NonRedundantSize(first_move));

  ParallelMove* last_move = last_instr->parallel_moves()[0];
  CHECK_EQ(0, NonRedundantSize(last_move));
}

TEST_F(MoveOptimizerTest,
       CompressesMultiSlotStackDestinationOverMultipleNarrowSlots) {
  const int kSimd128Slots =
      ElementSizeInPointers(MachineRepresentation::kSimd128);
  if (kSimd128Slots <= 1) return;

  StartBlock();
  Instruction* first_instr = EmitNop();
  Instruction* last_instr = EmitNop();

  AddMove(first_instr, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128));
  for (int i = 0; i < kSimd128Slots; ++i) {
    AddMove(first_instr, Reg(i % 2), Slot(i));
  }
  AddMove(last_instr, FPReg(kS128_2, kSimd128),
          Slot(kSimd128Slots - 1, kSimd128));

  EndBlock(Last());
  Optimize();

  ParallelMove* first_move = first_instr->parallel_moves()[0];
  CHECK(first_move == nullptr || NonRedundantSize(first_move) == 0);

  ParallelMove* last_move = last_instr->parallel_moves()[0];
  CHECK_EQ(2, NonRedundantSize(last_move));
  CHECK(
      Contains(last_move, FPReg(kS128_1, kSimd128), FPReg(kS128_2, kSimd128)));
  CHECK(Contains(last_move, FPReg(kS128_1, kSimd128),
                 Slot(kSimd128Slots - 1, kSimd128)));
}

TEST_F(MoveOptimizerTest, ClobberedMultiSlotStackDestinationsAreEliminated) {
  const int kSimd128Slots =
      ElementSizeInPointers(MachineRepresentation::kSimd128);
  StartBlock();
  Instruction* first_instr = EmitNop();
  for (int i = 0; i < kSimd128Slots; ++i) {
    AddMove(first_instr, Reg(0), Slot(i));
  }
  EmitOI(Slot(kSimd128Slots - 1, kSimd128), 0, nullptr);
  Instruction* last_instr = LastInstruction();
  EndBlock();
  Optimize();

  ParallelMove* first_move = first_instr->parallel_moves()[0];
  CHECK_EQ(0, NonRedundantSize(first_move));

  ParallelMove* last_move = last_instr->parallel_moves()[0];
  CHECK_EQ(0, NonRedundantSize(last_move));
}

TEST_F(MoveOptimizerTest, MultiSlotStackWriteBlocksOverlappingReadMigration) {
  const int kSimd128Slots =
      ElementSizeInPointers(MachineRepresentation::kSimd128);
  if (kSimd128Slots <= 1) return;

  StartBlock();
  EmitOI(Slot(kSimd128Slots - 1, kSimd128), 0, nullptr);
  Instruction* write_instr = LastInstruction();
  AddMove(write_instr, Slot(0), Reg(0));
  Instruction* after_instr = EmitNop();
  EndBlock(Last());

  Optimize();

  // Reading Slot(0) must happen before write_instr overwrites
  // Slot(0..kSimd128Slots-1), so the move cannot migrate down to after_instr.
  ParallelMove* write_move = write_instr->parallel_moves()[0];
  CHECK_EQ(1, NonRedundantSize(write_move));
  CHECK(Contains(write_move, Slot(0), Reg(0)));

  ParallelMove* after_move = after_instr->parallel_moves()[0];
  CHECK(after_move == nullptr || NonRedundantSize(after_move) == 0);
}

TEST_F(MoveOptimizerTest, MergeConflictWithOverlappingMultiSlotStackMove) {
  const int kSimd128Slots =
      ElementSizeInPointers(MachineRepresentation::kSimd128);
  if (kSimd128Slots <= 1) return;

  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  EndBlock(Jump(2));
  Instruction* b1_last = LastInstruction();
  AddMove(b1_last, Slot(0), Reg(0));
  AddMove(b1_last, FPReg(kS128_1, kSimd128), Slot(kSimd128Slots - 1, kSimd128));

  StartBlock();
  EndBlock(Jump(1));
  Instruction* b2_last = LastInstruction();
  AddMove(b2_last, Slot(0), Reg(0));

  StartBlock();
  EndBlock(Last());
  Instruction* merge_last = LastInstruction();

  Optimize();

  // Slot(0) -> Reg(0) cannot be hoisted into the merge block because b1_last
  // overwrites Slot(0..kSimd128Slots-1) in parallel with reading Slot(0).
  ParallelMove* merge_move = merge_last->parallel_moves()[0];
  CHECK(merge_move == nullptr || NonRedundantSize(merge_move) == 0);
  CHECK(Contains(b1_last->parallel_moves()[0], Slot(0), Reg(0)));
  CHECK(Contains(b2_last->parallel_moves()[0], Slot(0), Reg(0)));
}

TEST_F(MoveOptimizerTest, VerifierDropsOverlappingMultiSlotStackAssessments) {
  const int kSimd128Slots =
      ElementSizeInPointers(MachineRepresentation::kSimd128);
  if (kSimd128Slots <= 1) return;

  WireBlocks();
  BlockAssessments assessments(zone(), 0, sequence());

  InstructionOperand q0 =
      AllocatedOperand(LocationOperand::REGISTER, kSimd128, kS128_1);
  InstructionOperand r0 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kWord32, 0);
  InstructionOperand slot0 = AllocatedOperand(
      LocationOperand::STACK_SLOT, MachineRepresentation::kWord32, 0);
  InstructionOperand wide_slot = AllocatedOperand(LocationOperand::STACK_SLOT,
                                                  kSimd128, kSimd128Slots - 1);

  assessments.AddDefinition(q0, 10);
  assessments.AddDefinition(r0, 11);
  for (int i = 0; i < kSimd128Slots; ++i) {
    InstructionOperand narrow = AllocatedOperand(
        LocationOperand::STACK_SLOT, MachineRepresentation::kWord32, i);
    assessments.AddDefinition(narrow, 20 + i);
  }

  // Overwriting with a multi-slot move must drop all covered narrower slots.
  ParallelMove* wide_pm = zone()->New<ParallelMove>(zone());
  wide_pm->AddMove(q0, wide_slot);
  assessments.PerformParallelMoves(wide_pm);
  CHECK_EQ(assessments.map().end(), assessments.map().find(slot0));
  auto wide_it = assessments.map().find(wide_slot);
  CHECK_NE(assessments.map().end(), wide_it);
  CHECK_EQ(10, FinalAssessment::cast(wide_it->second)->virtual_register());

  // Subsequently writing a narrower overlapping slot (index 0) must drop the
  // wider assessment anchored at index kSimd128Slots - 1.
  ParallelMove* narrow_pm = zone()->New<ParallelMove>(zone());
  narrow_pm->AddMove(r0, slot0);
  assessments.PerformParallelMoves(narrow_pm);
  CHECK_EQ(assessments.map().end(), assessments.map().find(wide_slot));
  auto slot0_it = assessments.map().find(slot0);
  CHECK_NE(assessments.map().end(), slot0_it);
  CHECK_EQ(11, FinalAssessment::cast(slot0_it->second)->virtual_register());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
