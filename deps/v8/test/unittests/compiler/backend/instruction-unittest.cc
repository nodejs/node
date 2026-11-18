// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction.h"

#include "src/codegen/register-configuration.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/turbofan-graph.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace instruction_unittest {

namespace {

const MachineRepresentation kWord = MachineRepresentation::kWord32;
const MachineRepresentation kFloat = MachineRepresentation::kFloat32;
const MachineRepresentation kDouble = MachineRepresentation::kFloat64;

bool Interfere(LocationOperand::LocationKind kind, MachineRepresentation rep1,
               int index1, MachineRepresentation rep2, int index2) {
  return AllocatedOperand(kind, rep1, index1)
      .InterferesWith(AllocatedOperand(kind, rep2, index2));
}

bool Contains(const ZoneVector<MoveOperands*>* moves,
              const InstructionOperand& to, const InstructionOperand& from) {
  for (auto move : *moves) {
    if (move->destination().Equals(to) && move->source().Equals(from)) {
      return true;
    }
  }
  return false;
}

using TestInstr = v8::internal::compiler::Instruction;
using TestInstrSeq = v8::internal::compiler::InstructionSequence;

// A testing helper for the register code abstraction.
class InstructionTester {
 public:  // We're all friends here.
  InstructionTester(Isolate* isolate, Zone* zone)
      : isolate_(isolate),
        zone_(zone),
        graph(zone),
        schedule(zone),
        common(zone),
        machine(zone),
        code(nullptr) {}

  Isolate* isolate_;
  Zone* zone_;
  TFGraph graph;
  Schedule schedule;
  CommonOperatorBuilder common;
  MachineOperatorBuilder machine;
  TestInstrSeq* code;

  Isolate* main_isolate() { return isolate_; }
  Zone* main_zone() { return zone_; }
  Zone* zone() { return main_zone(); }

  void allocCode() {
    if (schedule.rpo_order()->size() == 0) {
      // Compute the RPO order.
      Scheduler::ComputeSpecialRPO(main_zone(), &schedule);
      CHECK_NE(0u, schedule.rpo_order()->size());
    }
    InstructionBlocks* instruction_blocks =
        TestInstrSeq::InstructionBlocksFor(main_zone(), &schedule);
    code = main_zone()->New<TestInstrSeq>(main_isolate(), main_zone(),
                                          instruction_blocks);
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

  int NewInstr() {
    InstructionCode opcode = static_cast<InstructionCode>(110);
    TestInstr* instr = TestInstr::New(zone(), opcode);
    return code->AddInstruction(instr);
  }

  int NewNop() {
    TestInstr* instr = TestInstr::New(zone(), kArchNop);
    return code->AddInstruction(instr);
  }

  UnallocatedOperand Unallocated(int vreg) {
    return UnallocatedOperand(UnallocatedOperand::REGISTER_OR_SLOT, vreg);
  }

  RpoNumber RpoFor(BasicBlock* block) {
    return RpoNumber::FromInt(block->rpo_number());
  }

  InstructionBlock* BlockAt(BasicBlock* block) {
    return code->InstructionBlockAt(RpoFor(block));
  }
  BasicBlock* GetBasicBlock(int instruction_index) {
    const InstructionBlock* block =
        code->GetInstructionBlock(instruction_index);
    return schedule.rpo_order()->at(block->rpo_number().ToSize());
  }
  int first_instruction_index(BasicBlock* block) {
    return BlockAt(block)->first_instruction_index();
  }
  int last_instruction_index(BasicBlock* block) {
    return BlockAt(block)->last_instruction_index();
  }
};

}  // namespace

class InstructionTest : public TestWithZone {
 public:
  InstructionTest() = default;
  ~InstructionTest() override = default;

  ParallelMove* CreateParallelMove(
      const std::vector<InstructionOperand>& operand_pairs) {
    ParallelMove* parallel_move = zone()->New<ParallelMove>(zone());
    for (size_t i = 0; i < operand_pairs.size(); i += 2) {
      parallel_move->AddMove(operand_pairs[i + 1], operand_pairs[i]);
    }
    return parallel_move;
  }
};

using InstructionCodegenTest = TestWithIsolateAndZone;

TEST_F(InstructionTest, OperandInterference) {
  // All general registers and slots interfere only with themselves.
  for (int i = 0; i < RegisterConfiguration::kMaxGeneralRegisters; ++i) {
    EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kWord, i, kWord, i));
    EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT, kWord, i, kWord, i));
    for (int j = i + 1; j < RegisterConfiguration::kMaxGeneralRegisters; ++j) {
      EXPECT_FALSE(Interfere(LocationOperand::REGISTER, kWord, i, kWord, j));
      EXPECT_FALSE(Interfere(LocationOperand::STACK_SLOT, kWord, i, kWord, j));
    }
  }

  // 128 bit slots can interfere with other slots at a different index.
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 128 / kBitsPerByte / kSystemPointerSize; ++j) {
      EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT,
                            MachineRepresentation::kSimd128, i, kWord, i - j));
      EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT,
                            MachineRepresentation::kSimd128, i, kFloat, i - j));
      EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT,
                            MachineRepresentation::kSimd128, i, kDouble,
                            i - j));
      EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT,
                            MachineRepresentation::kSimd128, i,
                            MachineRepresentation::kSimd128, i - j));
    }
  }

  // All FP registers interfere with themselves.
  for (int i = 0; i < RegisterConfiguration::kMaxFPRegisters; ++i) {
    EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kFloat, i, kFloat, i));
    EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT, kFloat, i, kFloat, i));
    EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kDouble, i, kDouble, i));
    EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT, kDouble, i, kDouble, i));
  }

  if (kFPAliasing != AliasingKind::kCombine) {
    // Simple FP aliasing: interfering registers of different reps have the same
    // index.
    for (int i = 0; i < RegisterConfiguration::kMaxFPRegisters; ++i) {
      EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kFloat, i, kDouble, i));
      EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kDouble, i, kFloat, i));
      for (int j = i + 1; j < RegisterConfiguration::kMaxFPRegisters; ++j) {
        EXPECT_FALSE(Interfere(LocationOperand::REGISTER, kWord, i, kWord, j));
        EXPECT_FALSE(
            Interfere(LocationOperand::STACK_SLOT, kWord, i, kWord, j));
      }
    }
  } else {
    // Complex FP aliasing: sub-registers intefere with containing registers.
    // Test sub-register indices which may not exist on the platform. This is
    // necessary since the GapResolver may split large moves into smaller ones.
    for (int i = 0; i < RegisterConfiguration::kMaxFPRegisters; ++i) {
      EXPECT_TRUE(
          Interfere(LocationOperand::REGISTER, kFloat, i * 2, kDouble, i));
      EXPECT_TRUE(
          Interfere(LocationOperand::REGISTER, kFloat, i * 2 + 1, kDouble, i));
      EXPECT_TRUE(
          Interfere(LocationOperand::REGISTER, kDouble, i, kFloat, i * 2));
      EXPECT_TRUE(
          Interfere(LocationOperand::REGISTER, kDouble, i, kFloat, i * 2 + 1));

      for (int j = i + 1; j < RegisterConfiguration::kMaxFPRegisters; ++j) {
        EXPECT_FALSE(
            Interfere(LocationOperand::REGISTER, kFloat, i * 2, kDouble, j));
        EXPECT_FALSE(Interfere(LocationOperand::REGISTER, kFloat, i * 2 + 1,
                               kDouble, j));
        EXPECT_FALSE(
            Interfere(LocationOperand::REGISTER, kDouble, i, kFloat, j * 2));
        EXPECT_FALSE(Interfere(LocationOperand::REGISTER, kDouble, i, kFloat,
                               j * 2 + 1));
      }
    }
  }
}

TEST_F(InstructionTest, PrepareInsertAfter) {
  InstructionOperand r0 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kWord32, 0);
  InstructionOperand r1 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kWord32, 1);
  InstructionOperand r2 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kWord32, 2);

  InstructionOperand d0 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kFloat64, 0);
  InstructionOperand d1 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kFloat64, 1);
  InstructionOperand d2 = AllocatedOperand(LocationOperand::REGISTER,
                                           MachineRepresentation::kFloat64, 2);

  {
    // Moves inserted after should pick up assignments to their sources.
    // Moves inserted after should cause interfering moves to be eliminated.
    ZoneVector<MoveOperands*> to_eliminate(zone());
    std::vector<InstructionOperand> moves = {
        r1, r0,  // r1 <- r0
        r2, r0,  // r2 <- r0
        d1, d0,  // d1 <- d0
        d2, d0   // d2 <- d0
    };

    ParallelMove* pm = CreateParallelMove(moves);
    MoveOperands m1(r1, r2);  // r2 <- r1
    pm->PrepareInsertAfter(&m1, &to_eliminate);
    CHECK(m1.source().Equals(r0));
    CHECK(Contains(&to_eliminate, r2, r0));
    MoveOperands m2(d1, d2);  // d2 <- d1
    pm->PrepareInsertAfter(&m2, &to_eliminate);
    CHECK(m2.source().Equals(d0));
    CHECK(Contains(&to_eliminate, d2, d0));
  }

  if (kFPAliasing == AliasingKind::kCombine) {
    // Moves inserted after should cause all interfering moves to be eliminated.
    auto s0 = AllocatedOperand(LocationOperand::REGISTER,
                               MachineRepresentation::kFloat32, 0);
    auto s1 = AllocatedOperand(LocationOperand::REGISTER,
                               MachineRepresentation::kFloat32, 1);
    auto s2 = AllocatedOperand(LocationOperand::REGISTER,
                               MachineRepresentation::kFloat32, 2);

    {
      ZoneVector<MoveOperands*> to_eliminate(zone());
      std::vector<InstructionOperand> moves = {
          s0, s2,  // s0 <- s2
          s1, s2   // s1 <- s2
      };

      ParallelMove* pm = CreateParallelMove(moves);
      MoveOperands m1(d1, d0);  // d0 <- d1
      pm->PrepareInsertAfter(&m1, &to_eliminate);
      CHECK(Contains(&to_eliminate, s0, s2));
      CHECK(Contains(&to_eliminate, s1, s2));
    }
  }
}

TEST_F(InstructionCodegenTest, InstructionBasic) {
  InstructionTester R(i_isolate(), zone());

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

  BasicBlockVector* blocks = R.schedule.rpo_order();
  CHECK_EQ(static_cast<int>(blocks->size()), R.code->InstructionBlockCount());

  for (auto block : *blocks) {
    CHECK_EQ(block->rpo_number(), R.BlockAt(block)->rpo_number().ToInt());
    CHECK(!block->loop_end());
  }
}

TEST_F(InstructionCodegenTest, InstructionGetBasicBlock) {
  InstructionTester R(i_isolate(), zone());

  BasicBlock* b0 = R.schedule.start();
  BasicBlock* b1 = R.schedule.NewBasicBlock();
  BasicBlock* b2 = R.schedule.NewBasicBlock();
  BasicBlock* b3 = R.schedule.end();

  R.schedule.AddGoto(b0, b1);
  R.schedule.AddGoto(b1, b2);
  R.schedule.AddGoto(b2, b3);

  R.allocCode();

  R.code->StartBlock(R.RpoFor(b0));
  int i0 = R.NewInstr();
  int i1 = R.NewInstr();
  R.code->EndBlock(R.RpoFor(b0));
  R.code->StartBlock(R.RpoFor(b1));
  int i2 = R.NewInstr();
  int i3 = R.NewInstr();
  int i4 = R.NewInstr();
  int i5 = R.NewInstr();
  R.code->EndBlock(R.RpoFor(b1));
  R.code->StartBlock(R.RpoFor(b2));
  int i6 = R.NewInstr();
  int i7 = R.NewInstr();
  int i8 = R.NewInstr();
  R.code->EndBlock(R.RpoFor(b2));
  R.code->StartBlock(R.RpoFor(b3));
  R.NewNop();
  R.code->EndBlock(R.RpoFor(b3));

  CHECK_EQ(b0, R.GetBasicBlock(i0));
  CHECK_EQ(b0, R.GetBasicBlock(i1));

  CHECK_EQ(b1, R.GetBasicBlock(i2));
  CHECK_EQ(b1, R.GetBasicBlock(i3));
  CHECK_EQ(b1, R.GetBasicBlock(i4));
  CHECK_EQ(b1, R.GetBasicBlock(i5));

  CHECK_EQ(b2, R.GetBasicBlock(i6));
  CHECK_EQ(b2, R.GetBasicBlock(i7));
  CHECK_EQ(b2, R.GetBasicBlock(i8));

  CHECK_EQ(b0, R.GetBasicBlock(R.first_instruction_index(b0)));
  CHECK_EQ(b0, R.GetBasicBlock(R.last_instruction_index(b0)));

  CHECK_EQ(b1, R.GetBasicBlock(R.first_instruction_index(b1)));
  CHECK_EQ(b1, R.GetBasicBlock(R.last_instruction_index(b1)));

  CHECK_EQ(b2, R.GetBasicBlock(R.first_instruction_index(b2)));
  CHECK_EQ(b2, R.GetBasicBlock(R.last_instruction_index(b2)));

  CHECK_EQ(b3, R.GetBasicBlock(R.first_instruction_index(b3)));
  CHECK_EQ(b3, R.GetBasicBlock(R.last_instruction_index(b3)));
}

TEST_F(InstructionCodegenTest, InstructionIsGapAt) {
  InstructionTester R(i_isolate(), zone());

  BasicBlock* b0 = R.schedule.start();
  R.schedule.AddReturn(b0, R.Int32Constant(1));

  R.allocCode();
  TestInstr* i0 = TestInstr::New(R.zone(), 100);
  TestInstr* g = TestInstr::New(R.zone(), 103);
  R.code->StartBlock(R.RpoFor(b0));
  R.code->AddInstruction(i0);
  R.code->AddInstruction(g);
  R.code->EndBlock(R.RpoFor(b0));

  CHECK_EQ(2, R.code->instructions().size());
}

TEST_F(InstructionCodegenTest, InstructionIsGapAt2) {
  InstructionTester R(i_isolate(), zone());

  BasicBlock* b0 = R.schedule.start();
  BasicBlock* b1 = R.schedule.end();
  R.schedule.AddGoto(b0, b1);
  R.schedule.AddReturn(b1, R.Int32Constant(1));

  R.allocCode();
  TestInstr* i0 = TestInstr::New(R.zone(), 100);
  TestInstr* g = TestInstr::New(R.zone(), 103);
  R.code->StartBlock(R.RpoFor(b0));
  R.code->AddInstruction(i0);
  R.code->AddInstruction(g);
  R.code->EndBlock(R.RpoFor(b0));

  TestInstr* i1 = TestInstr::New(R.zone(), 102);
  TestInstr* g1 = TestInstr::New(R.zone(), 104);
  R.code->StartBlock(R.RpoFor(b1));
  R.code->AddInstruction(i1);
  R.code->AddInstruction(g1);
  R.code->EndBlock(R.RpoFor(b1));

  CHECK_EQ(4, R.code->instructions().size());
}

TEST_F(InstructionCodegenTest, InstructionAddGapMove) {
  InstructionTester R(i_isolate(), zone());

  BasicBlock* b0 = R.schedule.start();
  R.schedule.AddReturn(b0, R.Int32Constant(1));

  R.allocCode();
  TestInstr* i0 = TestInstr::New(R.zone(), 100);
  TestInstr* g = TestInstr::New(R.zone(), 103);
  R.code->StartBlock(R.RpoFor(b0));
  R.code->AddInstruction(i0);
  R.code->AddInstruction(g);
  R.code->EndBlock(R.RpoFor(b0));

  CHECK_EQ(2, R.code->instructions().size());

  int index = 0;
  for (auto instr : R.code->instructions()) {
    UnallocatedOperand op1 = R.Unallocated(index++);
    UnallocatedOperand op2 = R.Unallocated(index++);
    instr->GetOrCreateParallelMove(TestInstr::START, R.zone())
        ->AddMove(op1, op2);
    ParallelMove* move = instr->GetParallelMove(TestInstr::START);
    CHECK(move);
    CHECK_EQ(1u, move->size());
    MoveOperands* cur = move->at(0);
    CHECK(op1.Equals(cur->source()));
    CHECK(op2.Equals(cur->destination()));
  }
}

TEST_F(InstructionTest, InstructionOperands) {
  {
    TestInstr* i = TestInstr::New(zone(), 101);
    CHECK_EQ(0, static_cast<int>(i->OutputCount()));
    CHECK_EQ(0, static_cast<int>(i->InputCount()));
    CHECK_EQ(0, static_cast<int>(i->TempCount()));
  }

  int vreg = 15;
  InstructionOperand outputs[] = {
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg)};

  InstructionOperand inputs[] = {
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg)};

  InstructionOperand temps[] = {
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg),
      UnallocatedOperand(UnallocatedOperand::MUST_HAVE_REGISTER, vreg)};

  for (size_t i = 0; i < arraysize(outputs); i++) {
    for (size_t j = 0; j < arraysize(inputs); j++) {
      for (size_t k = 0; k < arraysize(temps); k++) {
        TestInstr* m =
            TestInstr::New(zone(), 101, i, outputs, j, inputs, k, temps);
        CHECK(i == m->OutputCount());
        CHECK(j == m->InputCount());
        CHECK(k == m->TempCount());

        for (size_t z = 0; z < i; z++) {
          CHECK(outputs[z].Equals(*m->OutputAt(z)));
        }

        for (size_t z = 0; z < j; z++) {
          CHECK(inputs[z].Equals(*m->InputAt(z)));
        }

        for (size_t z = 0; z < k; z++) {
          CHECK(temps[z].Equals(*m->TempAt(z)));
        }
      }
    }
  }
}

}  // namespace instruction_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
