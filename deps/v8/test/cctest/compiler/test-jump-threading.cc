// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-codes.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/backend/jump-threading.h"
#include "src/source-position.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

class TestCode : public HandleAndZoneScope {
 public:
  TestCode()
      : HandleAndZoneScope(),
        blocks_(main_zone()),
        sequence_(main_isolate(), main_zone(), &blocks_),
        rpo_number_(RpoNumber::FromInt(0)),
        current_(nullptr) {}

  ZoneVector<InstructionBlock*> blocks_;
  InstructionSequence sequence_;
  RpoNumber rpo_number_;
  InstructionBlock* current_;

  int Jump(int target) {
    Start();
    InstructionOperand ops[] = {UseRpo(target)};
    sequence_.AddInstruction(Instruction::New(main_zone(), kArchJmp, 0, nullptr,
                                              1, ops, 0, nullptr));
    int pos = static_cast<int>(sequence_.instructions().size() - 1);
    End();
    return pos;
  }
  void Fallthru() {
    Start();
    End();
  }
  int Branch(int ttarget, int ftarget) {
    Start();
    InstructionOperand ops[] = {UseRpo(ttarget), UseRpo(ftarget)};
    InstructionCode code = 119 | FlagsModeField::encode(kFlags_branch) |
                           FlagsConditionField::encode(kEqual);
    sequence_.AddInstruction(
        Instruction::New(main_zone(), code, 0, nullptr, 2, ops, 0, nullptr));
    int pos = static_cast<int>(sequence_.instructions().size() - 1);
    End();
    return pos;
  }
  void Nop() {
    Start();
    sequence_.AddInstruction(Instruction::New(main_zone(), kArchNop));
  }
  void RedundantMoves() {
    Start();
    sequence_.AddInstruction(Instruction::New(main_zone(), kArchNop));
    int index = static_cast<int>(sequence_.instructions().size()) - 1;
    AddGapMove(index, AllocatedOperand(LocationOperand::REGISTER,
                                       MachineRepresentation::kWord32, 13),
               AllocatedOperand(LocationOperand::REGISTER,
                                MachineRepresentation::kWord32, 13));
  }
  void NonRedundantMoves() {
    Start();
    sequence_.AddInstruction(Instruction::New(main_zone(), kArchNop));
    int index = static_cast<int>(sequence_.instructions().size()) - 1;
    AddGapMove(index, ConstantOperand(11),
               AllocatedOperand(LocationOperand::REGISTER,
                                MachineRepresentation::kWord32, 11));
  }
  void Other() {
    Start();
    sequence_.AddInstruction(Instruction::New(main_zone(), 155));
  }
  void End() {
    Start();
    int end = static_cast<int>(sequence_.instructions().size());
    if (current_->code_start() == end) {  // Empty block.  Insert a nop.
      sequence_.AddInstruction(Instruction::New(main_zone(), kArchNop));
    }
    sequence_.EndBlock(current_->rpo_number());
    current_ = nullptr;
    rpo_number_ = RpoNumber::FromInt(rpo_number_.ToInt() + 1);
  }
  InstructionOperand UseRpo(int num) {
    return sequence_.AddImmediate(Constant(RpoNumber::FromInt(num)));
  }
  void Start(bool deferred = false) {
    if (current_ == nullptr) {
      current_ = new (main_zone())
          InstructionBlock(main_zone(), rpo_number_, RpoNumber::Invalid(),
                           RpoNumber::Invalid(), deferred, false);
      blocks_.push_back(current_);
      sequence_.StartBlock(rpo_number_);
    }
  }
  void Defer() {
    CHECK_NULL(current_);
    Start(true);
  }
  void AddGapMove(int index, const InstructionOperand& from,
                  const InstructionOperand& to) {
    sequence_.InstructionAt(index)
        ->GetOrCreateParallelMove(Instruction::START, main_zone())
        ->AddMove(from, to);
  }
};


void VerifyForwarding(TestCode& code, int count, int* expected) {
  v8::internal::AccountingAllocator allocator;
  Zone local_zone(&allocator, ZONE_NAME);
  ZoneVector<RpoNumber> result(&local_zone);
  JumpThreading::ComputeForwarding(&local_zone, result, &code.sequence_, true);

  CHECK(count == static_cast<int>(result.size()));
  for (int i = 0; i < count; i++) {
    CHECK(expected[i] == result[i].ToInt());
  }
}


TEST(FwEmpty1) {
  TestCode code;

  // B0
  code.Jump(1);
  // B1
  code.Jump(2);
  // B2
  code.End();

  static int expected[] = {2, 2, 2};
  VerifyForwarding(code, 3, expected);
}


TEST(FwEmptyN) {
  for (int i = 0; i < 9; i++) {
    TestCode code;

    // B0
    code.Jump(1);
    // B1
    for (int j = 0; j < i; j++) code.Nop();
    code.Jump(2);
    // B2
    code.End();

    static int expected[] = {2, 2, 2};
    VerifyForwarding(code, 3, expected);
  }
}


TEST(FwNone1) {
  TestCode code;

  // B0
  code.End();

  static int expected[] = {0};
  VerifyForwarding(code, 1, expected);
}


TEST(FwMoves1) {
  TestCode code;

  // B0
  code.RedundantMoves();
  code.End();

  static int expected[] = {0};
  VerifyForwarding(code, 1, expected);
}


TEST(FwMoves2) {
  TestCode code;

  // B0
  code.RedundantMoves();
  code.Fallthru();
  // B1
  code.End();

  static int expected[] = {1, 1};
  VerifyForwarding(code, 2, expected);
}


TEST(FwMoves2b) {
  TestCode code;

  // B0
  code.NonRedundantMoves();
  code.Fallthru();
  // B1
  code.End();

  static int expected[] = {0, 1};
  VerifyForwarding(code, 2, expected);
}


TEST(FwOther2) {
  TestCode code;

  // B0
  code.Other();
  code.Fallthru();
  // B1
  code.End();

  static int expected[] = {0, 1};
  VerifyForwarding(code, 2, expected);
}


TEST(FwNone2a) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.End();

  static int expected[] = {1, 1};
  VerifyForwarding(code, 2, expected);
}


TEST(FwNone2b) {
  TestCode code;

  // B0
  code.Jump(1);
  // B1
  code.End();

  static int expected[] = {1, 1};
  VerifyForwarding(code, 2, expected);
}


TEST(FwLoop1) {
  TestCode code;

  // B0
  code.Jump(0);

  static int expected[] = {0};
  VerifyForwarding(code, 1, expected);
}


TEST(FwLoop2) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Jump(0);

  static int expected[] = {0, 0};
  VerifyForwarding(code, 2, expected);
}


TEST(FwLoop3) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Jump(0);

  static int expected[] = {0, 0, 0};
  VerifyForwarding(code, 3, expected);
}


TEST(FwLoop1b) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Jump(1);

  static int expected[] = {1, 1};
  VerifyForwarding(code, 2, expected);
}


TEST(FwLoop2b) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Jump(1);

  static int expected[] = {1, 1, 1};
  VerifyForwarding(code, 3, expected);
}


TEST(FwLoop3b) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Fallthru();
  // B3
  code.Jump(1);

  static int expected[] = {1, 1, 1, 1};
  VerifyForwarding(code, 4, expected);
}


TEST(FwLoop2_1a) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Fallthru();
  // B3
  code.Jump(1);
  // B4
  code.Jump(2);

  static int expected[] = {1, 1, 1, 1, 1};
  VerifyForwarding(code, 5, expected);
}


TEST(FwLoop2_1b) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Jump(4);
  // B3
  code.Jump(1);
  // B4
  code.Jump(2);

  static int expected[] = {2, 2, 2, 2, 2};
  VerifyForwarding(code, 5, expected);
}


TEST(FwLoop2_1c) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Jump(4);
  // B3
  code.Jump(2);
  // B4
  code.Jump(1);

  static int expected[] = {1, 1, 1, 1, 1};
  VerifyForwarding(code, 5, expected);
}


TEST(FwLoop2_1d) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Jump(1);
  // B3
  code.Jump(1);
  // B4
  code.Jump(1);

  static int expected[] = {1, 1, 1, 1, 1};
  VerifyForwarding(code, 5, expected);
}


TEST(FwLoop3_1a) {
  TestCode code;

  // B0
  code.Fallthru();
  // B1
  code.Fallthru();
  // B2
  code.Fallthru();
  // B3
  code.Jump(2);
  // B4
  code.Jump(1);
  // B5
  code.Jump(0);

  static int expected[] = {2, 2, 2, 2, 2, 2};
  VerifyForwarding(code, 6, expected);
}


TEST(FwDiamonds) {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      TestCode code;
      // B0
      code.Branch(1, 2);
      // B1
      if (i) code.Other();
      code.Jump(3);
      // B2
      if (j) code.Other();
      code.Jump(3);
      // B3
      code.End();

      int expected[] = {0, i ? 1 : 3, j ? 2 : 3, 3};
      VerifyForwarding(code, 4, expected);
    }
  }
}


TEST(FwDiamonds2) {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      for (int k = 0; k < 2; k++) {
        TestCode code;
        // B0
        code.Branch(1, 2);
        // B1
        if (i) code.Other();
        code.Jump(3);
        // B2
        if (j) code.Other();
        code.Jump(3);
        // B3
        if (k) code.NonRedundantMoves();
        code.Jump(4);
        // B4
        code.End();

        int merge = k ? 3 : 4;
        int expected[] = {0, i ? 1 : merge, j ? 2 : merge, merge, 4};
        VerifyForwarding(code, 5, expected);
      }
    }
  }
}


TEST(FwDoubleDiamonds) {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
          TestCode code;
          // B0
          code.Branch(1, 2);
          // B1
          if (i) code.Other();
          code.Jump(3);
          // B2
          if (j) code.Other();
          code.Jump(3);
          // B3
          code.Branch(4, 5);
          // B4
          if (x) code.Other();
          code.Jump(6);
          // B5
          if (y) code.Other();
          code.Jump(6);
          // B6
          code.End();

          int expected[] = {0,         i ? 1 : 3, j ? 2 : 3, 3,
                            x ? 4 : 6, y ? 5 : 6, 6};
          VerifyForwarding(code, 7, expected);
        }
      }
    }
  }
}

template <int kSize>
void RunPermutationsRecursive(int outer[kSize], int start,
                              void (*run)(int*, int)) {
  int permutation[kSize];

  for (int i = 0; i < kSize; i++) permutation[i] = outer[i];

  int count = kSize - start;
  if (count == 0) return run(permutation, kSize);
  for (int i = start; i < kSize; i++) {
    permutation[start] = outer[i];
    permutation[i] = outer[start];
    RunPermutationsRecursive<kSize>(permutation, start + 1, run);
    permutation[i] = outer[i];
    permutation[start] = outer[start];
  }
}


template <int kSize>
void RunAllPermutations(void (*run)(int*, int)) {
  int permutation[kSize];
  for (int i = 0; i < kSize; i++) permutation[i] = i;
  RunPermutationsRecursive<kSize>(permutation, 0, run);
}


void PrintPermutation(int* permutation, int size) {
  printf("{ ");
  for (int i = 0; i < size; i++) {
    if (i > 0) printf(", ");
    printf("%d", permutation[i]);
  }
  printf(" }\n");
}


int find(int x, int* permutation, int size) {
  for (int i = 0; i < size; i++) {
    if (permutation[i] == x) return i;
  }
  return size;
}


void RunPermutedChain(int* permutation, int size) {
  TestCode code;
  int cur = -1;
  for (int i = 0; i < size; i++) {
    code.Jump(find(cur + 1, permutation, size) + 1);
    cur = permutation[i];
  }
  code.Jump(find(cur + 1, permutation, size) + 1);
  code.End();

  int expected[] = {size + 1, size + 1, size + 1, size + 1,
                    size + 1, size + 1, size + 1};
  VerifyForwarding(code, size + 2, expected);
}


TEST(FwPermuted_chain) {
  RunAllPermutations<3>(RunPermutedChain);
  RunAllPermutations<4>(RunPermutedChain);
  RunAllPermutations<5>(RunPermutedChain);
}


void RunPermutedDiamond(int* permutation, int size) {
  TestCode code;
  int br = 1 + find(0, permutation, size);
  code.Jump(br);
  for (int i = 0; i < size; i++) {
    switch (permutation[i]) {
      case 0:
        code.Branch(1 + find(1, permutation, size),
                    1 + find(2, permutation, size));
        break;
      case 1:
        code.Jump(1 + find(3, permutation, size));
        break;
      case 2:
        code.Jump(1 + find(3, permutation, size));
        break;
      case 3:
        code.Jump(5);
        break;
    }
  }
  code.End();

  int expected[] = {br, 5, 5, 5, 5, 5};
  expected[br] = br;
  VerifyForwarding(code, 6, expected);
}


TEST(FwPermuted_diamond) { RunAllPermutations<4>(RunPermutedDiamond); }


void ApplyForwarding(TestCode& code, int size, int* forward) {
  code.sequence_.RecomputeAssemblyOrderForTesting();
  ZoneVector<RpoNumber> vector(code.main_zone());
  for (int i = 0; i < size; i++) {
    vector.push_back(RpoNumber::FromInt(forward[i]));
  }
  JumpThreading::ApplyForwarding(code.main_zone(), vector, &code.sequence_);
}


void CheckJump(TestCode& code, int pos, int target) {
  Instruction* instr = code.sequence_.InstructionAt(pos);
  CHECK_EQ(kArchJmp, instr->arch_opcode());
  CHECK_EQ(1, static_cast<int>(instr->InputCount()));
  CHECK_EQ(0, static_cast<int>(instr->OutputCount()));
  CHECK_EQ(0, static_cast<int>(instr->TempCount()));
  CHECK_EQ(target, code.sequence_.InputRpo(instr, 0).ToInt());
}


void CheckNop(TestCode& code, int pos) {
  Instruction* instr = code.sequence_.InstructionAt(pos);
  CHECK_EQ(kArchNop, instr->arch_opcode());
  CHECK_EQ(0, static_cast<int>(instr->InputCount()));
  CHECK_EQ(0, static_cast<int>(instr->OutputCount()));
  CHECK_EQ(0, static_cast<int>(instr->TempCount()));
}


void CheckBranch(TestCode& code, int pos, int t1, int t2) {
  Instruction* instr = code.sequence_.InstructionAt(pos);
  CHECK_EQ(2, static_cast<int>(instr->InputCount()));
  CHECK_EQ(0, static_cast<int>(instr->OutputCount()));
  CHECK_EQ(0, static_cast<int>(instr->TempCount()));
  CHECK_EQ(t1, code.sequence_.InputRpo(instr, 0).ToInt());
  CHECK_EQ(t2, code.sequence_.InputRpo(instr, 1).ToInt());
}


void CheckAssemblyOrder(TestCode& code, int size, int* expected) {
  int i = 0;
  for (auto const block : code.sequence_.instruction_blocks()) {
    CHECK_EQ(expected[i++], block->ao_number().ToInt());
  }
}


TEST(Rewire1) {
  TestCode code;

  // B0
  int j1 = code.Jump(1);
  // B1
  int j2 = code.Jump(2);
  // B2
  code.End();

  static int forward[] = {2, 2, 2};
  ApplyForwarding(code, 3, forward);
  CheckJump(code, j1, 2);
  CheckNop(code, j2);

  static int assembly[] = {0, 1, 1};
  CheckAssemblyOrder(code, 3, assembly);
}


TEST(Rewire1_deferred) {
  TestCode code;

  // B0
  int j1 = code.Jump(1);
  // B1
  int j2 = code.Jump(2);
  // B2
  code.Defer();
  int j3 = code.Jump(3);
  // B3
  code.End();

  static int forward[] = {3, 3, 3, 3};
  ApplyForwarding(code, 4, forward);
  CheckJump(code, j1, 3);
  CheckNop(code, j2);
  CheckNop(code, j3);

  static int assembly[] = {0, 1, 2, 1};
  CheckAssemblyOrder(code, 4, assembly);
}


TEST(Rewire2_deferred) {
  TestCode code;

  // B0
  code.Other();
  int j1 = code.Jump(1);
  // B1
  code.Defer();
  code.Fallthru();
  // B2
  code.Defer();
  int j2 = code.Jump(3);
  // B3
  code.End();

  static int forward[] = {0, 1, 2, 3};
  ApplyForwarding(code, 4, forward);
  CheckJump(code, j1, 1);
  CheckJump(code, j2, 3);

  static int assembly[] = {0, 2, 3, 1};
  CheckAssemblyOrder(code, 4, assembly);
}


TEST(Rewire_diamond) {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      TestCode code;
      // B0
      int j1 = code.Jump(1);
      // B1
      int b1 = code.Branch(2, 3);
      // B2
      int j2 = code.Jump(4);
      // B3
      int j3 = code.Jump(4);
      // B5
      code.End();

      int forward[] = {0, 1, i ? 4 : 2, j ? 4 : 3, 4};
      ApplyForwarding(code, 5, forward);
      CheckJump(code, j1, 1);
      CheckBranch(code, b1, i ? 4 : 2, j ? 4 : 3);
      if (i) {
        CheckNop(code, j2);
      } else {
        CheckJump(code, j2, 4);
      }
      if (j) {
        CheckNop(code, j3);
      } else {
        CheckJump(code, j3, 4);
      }

      int assembly[] = {0, 1, 2, 3, 4};
      if (i) {
        for (int k = 3; k < 5; k++) assembly[k]--;
      }
      if (j) {
        for (int k = 4; k < 5; k++) assembly[k]--;
      }
      CheckAssemblyOrder(code, 5, assembly);
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
