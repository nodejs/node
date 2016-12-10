// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_INSTRUCTION_SEQUENCE_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_INSTRUCTION_SEQUENCE_UNITTEST_H_

#include <memory>

#include "src/compiler/instruction.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

class InstructionSequenceTest : public TestWithIsolateAndZone {
 public:
  static const int kDefaultNRegs = 8;
  static const int kNoValue = kMinInt;

  typedef RpoNumber Rpo;

  struct VReg {
    VReg() : value_(kNoValue) {}
    VReg(PhiInstruction* phi) : value_(phi->virtual_register()) {}  // NOLINT
    explicit VReg(int value) : value_(value) {}
    int value_;
  };

  typedef std::pair<VReg, VReg> VRegPair;

  enum TestOperandType {
    kInvalid,
    kSameAsFirst,
    kRegister,
    kFixedRegister,
    kSlot,
    kFixedSlot,
    kExplicit,
    kImmediate,
    kNone,
    kConstant,
    kUnique,
    kUniqueRegister
  };

  struct TestOperand {
    TestOperand() : type_(kInvalid), vreg_(), value_(kNoValue) {}
    TestOperand(TestOperandType type, int imm)
        : type_(type), vreg_(), value_(imm) {}
    TestOperand(TestOperandType type, VReg vreg, int value = kNoValue)
        : type_(type), vreg_(vreg), value_(value) {}

    TestOperandType type_;
    VReg vreg_;
    int value_;
  };

  static TestOperand Same() { return TestOperand(kSameAsFirst, VReg()); }

  static TestOperand ExplicitReg(int index) {
    TestOperandType type = kExplicit;
    return TestOperand(type, VReg(), index);
  }

  static TestOperand Reg(VReg vreg, int index = kNoValue) {
    TestOperandType type = kRegister;
    if (index != kNoValue) type = kFixedRegister;
    return TestOperand(type, vreg, index);
  }

  static TestOperand Reg(int index = kNoValue) { return Reg(VReg(), index); }

  static TestOperand Slot(VReg vreg, int index = kNoValue) {
    TestOperandType type = kSlot;
    if (index != kNoValue) type = kFixedSlot;
    return TestOperand(type, vreg, index);
  }

  static TestOperand Slot(int index = kNoValue) { return Slot(VReg(), index); }

  static TestOperand Const(int index) {
    CHECK_NE(kNoValue, index);
    return TestOperand(kConstant, VReg(), index);
  }

  static TestOperand Use(VReg vreg) { return TestOperand(kNone, vreg); }

  static TestOperand Use() { return Use(VReg()); }

  static TestOperand Unique(VReg vreg) { return TestOperand(kUnique, vreg); }

  static TestOperand UniqueReg(VReg vreg) {
    return TestOperand(kUniqueRegister, vreg);
  }

  enum BlockCompletionType { kBlockEnd, kFallThrough, kBranch, kJump };

  struct BlockCompletion {
    BlockCompletionType type_;
    TestOperand op_;
    int offset_0_;
    int offset_1_;
  };

  static BlockCompletion FallThrough() {
    BlockCompletion completion = {kFallThrough, TestOperand(), 1, kNoValue};
    return completion;
  }

  static BlockCompletion Jump(int offset) {
    BlockCompletion completion = {kJump, TestOperand(), offset, kNoValue};
    return completion;
  }

  static BlockCompletion Branch(TestOperand op, int left_offset,
                                int right_offset) {
    BlockCompletion completion = {kBranch, op, left_offset, right_offset};
    return completion;
  }

  static BlockCompletion Last() {
    BlockCompletion completion = {kBlockEnd, TestOperand(), kNoValue, kNoValue};
    return completion;
  }

  InstructionSequenceTest();

  void SetNumRegs(int num_general_registers, int num_double_registers);
  RegisterConfiguration* config();
  InstructionSequence* sequence();

  void StartLoop(int loop_blocks);
  void EndLoop();
  void StartBlock(bool deferred = false);
  Instruction* EndBlock(BlockCompletion completion = FallThrough());

  TestOperand Imm(int32_t imm = 0);
  VReg Define(TestOperand output_op);
  VReg Parameter(TestOperand output_op = Reg()) { return Define(output_op); }

  Instruction* Return(TestOperand input_op_0);
  Instruction* Return(VReg vreg) { return Return(Reg(vreg, 0)); }

  PhiInstruction* Phi(VReg incoming_vreg_0 = VReg(),
                      VReg incoming_vreg_1 = VReg(),
                      VReg incoming_vreg_2 = VReg(),
                      VReg incoming_vreg_3 = VReg());
  PhiInstruction* Phi(VReg incoming_vreg_0, size_t input_count);
  void SetInput(PhiInstruction* phi, size_t input, VReg vreg);

  VReg DefineConstant(int32_t imm = 0);
  Instruction* EmitNop();
  Instruction* EmitI(size_t input_size, TestOperand* inputs);
  Instruction* EmitI(TestOperand input_op_0 = TestOperand(),
                     TestOperand input_op_1 = TestOperand(),
                     TestOperand input_op_2 = TestOperand(),
                     TestOperand input_op_3 = TestOperand());
  VReg EmitOI(TestOperand output_op, size_t input_size, TestOperand* inputs);
  VReg EmitOI(TestOperand output_op, TestOperand input_op_0 = TestOperand(),
              TestOperand input_op_1 = TestOperand(),
              TestOperand input_op_2 = TestOperand(),
              TestOperand input_op_3 = TestOperand());
  VRegPair EmitOOI(TestOperand output_op_0, TestOperand output_op_1,
                   size_t input_size, TestOperand* inputs);
  VRegPair EmitOOI(TestOperand output_op_0, TestOperand output_op_1,
                   TestOperand input_op_0 = TestOperand(),
                   TestOperand input_op_1 = TestOperand(),
                   TestOperand input_op_2 = TestOperand(),
                   TestOperand input_op_3 = TestOperand());
  VReg EmitCall(TestOperand output_op, size_t input_size, TestOperand* inputs);
  VReg EmitCall(TestOperand output_op, TestOperand input_op_0 = TestOperand(),
                TestOperand input_op_1 = TestOperand(),
                TestOperand input_op_2 = TestOperand(),
                TestOperand input_op_3 = TestOperand());

  InstructionBlock* current_block() const { return current_block_; }
  int num_general_registers() const { return num_general_registers_; }
  int num_double_registers() const { return num_double_registers_; }

  // Called after all instructions have been inserted.
  void WireBlocks();

 private:
  VReg NewReg() { return VReg(sequence()->NextVirtualRegister()); }

  static TestOperand Invalid() { return TestOperand(kInvalid, VReg()); }

  Instruction* EmitBranch(TestOperand input_op);
  Instruction* EmitFallThrough();
  Instruction* EmitJump();
  Instruction* NewInstruction(InstructionCode code, size_t outputs_size,
                              InstructionOperand* outputs,
                              size_t inputs_size = 0,
                              InstructionOperand* inputs = nullptr,
                              size_t temps_size = 0,
                              InstructionOperand* temps = nullptr);
  InstructionOperand Unallocated(TestOperand op,
                                 UnallocatedOperand::ExtendedPolicy policy);
  InstructionOperand Unallocated(TestOperand op,
                                 UnallocatedOperand::ExtendedPolicy policy,
                                 UnallocatedOperand::Lifetime lifetime);
  InstructionOperand Unallocated(TestOperand op,
                                 UnallocatedOperand::ExtendedPolicy policy,
                                 int index);
  InstructionOperand Unallocated(TestOperand op,
                                 UnallocatedOperand::BasicPolicy policy,
                                 int index);
  InstructionOperand* ConvertInputs(size_t input_size, TestOperand* inputs);
  InstructionOperand ConvertInputOp(TestOperand op);
  InstructionOperand ConvertOutputOp(VReg vreg, TestOperand op);
  InstructionBlock* NewBlock(bool deferred = false);
  void WireBlock(size_t block_offset, int jump_offset);

  Instruction* Emit(InstructionCode code, size_t outputs_size = 0,
                    InstructionOperand* outputs = nullptr,
                    size_t inputs_size = 0,
                    InstructionOperand* inputs = nullptr, size_t temps_size = 0,
                    InstructionOperand* temps = nullptr, bool is_call = false);

  Instruction* AddInstruction(Instruction* instruction);

  struct LoopData {
    Rpo loop_header_;
    int expected_blocks_;
  };

  typedef std::vector<LoopData> LoopBlocks;
  typedef std::map<int, const Instruction*> Instructions;
  typedef std::vector<BlockCompletion> Completions;

  std::unique_ptr<RegisterConfiguration> config_;
  InstructionSequence* sequence_;
  int num_general_registers_;
  int num_double_registers_;

  // Block building state.
  InstructionBlocks instruction_blocks_;
  Instructions instructions_;
  Completions completions_;
  LoopBlocks loop_blocks_;
  InstructionBlock* current_block_;
  bool block_returns_;

  DISALLOW_COPY_AND_ASSIGN(InstructionSequenceTest);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_INSTRUCTION_SEQUENCE_UNITTEST_H_
