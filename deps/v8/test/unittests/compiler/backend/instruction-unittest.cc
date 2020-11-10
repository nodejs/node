// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction.h"
#include "src/codegen/register-configuration.h"
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

}  // namespace

class InstructionTest : public TestWithZone {
 public:
  InstructionTest() = default;
  ~InstructionTest() override = default;

  ParallelMove* CreateParallelMove(
      const std::vector<InstructionOperand>& operand_pairs) {
    ParallelMove* parallel_move = new (zone()) ParallelMove(zone());
    for (size_t i = 0; i < operand_pairs.size(); i += 2)
      parallel_move->AddMove(operand_pairs[i + 1], operand_pairs[i]);
    return parallel_move;
  }
};

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

  // All FP registers interfere with themselves.
  for (int i = 0; i < RegisterConfiguration::kMaxFPRegisters; ++i) {
    EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kFloat, i, kFloat, i));
    EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT, kFloat, i, kFloat, i));
    EXPECT_TRUE(Interfere(LocationOperand::REGISTER, kDouble, i, kDouble, i));
    EXPECT_TRUE(Interfere(LocationOperand::STACK_SLOT, kDouble, i, kDouble, i));
  }

  if (kSimpleFPAliasing) {
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

  if (!kSimpleFPAliasing) {
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

}  // namespace instruction_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
