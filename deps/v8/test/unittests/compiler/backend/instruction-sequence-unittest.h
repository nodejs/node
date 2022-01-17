// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_INSTRUCTION_SEQUENCE_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_INSTRUCTION_SEQUENCE_UNITTEST_H_

#include <memory>

#include "src/codegen/register-configuration.h"
#include "src/compiler/backend/instruction.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

class InstructionSequenceTest : public TestWithIsolateAndZone {
 public:
  static constexpr int kNoValue = kMinInt;
  static constexpr MachineRepresentation kNoRep = MachineRepresentation::kNone;
  static constexpr MachineRepresentation kFloat32 =
      MachineRepresentation::kFloat32;
  static constexpr MachineRepresentation kFloat64 =
      MachineRepresentation::kFloat64;
  static constexpr MachineRepresentation kSimd128 =
      MachineRepresentation::kSimd128;

  using Rpo = RpoNumber;

  struct VReg {
    VReg() : value_(kNoValue) {}
    VReg(PhiInstruction* phi) : value_(phi->virtual_register()) {}  // NOLINT
    explicit VReg(int value, MachineRepresentation rep = kNoRep)
        : value_(value), rep_(rep) {}
    int value_;
    MachineRepresentation rep_ = kNoRep;
  };

  using VRegPair = std::pair<VReg, VReg>;

  enum TestOperandType {
    kInvalid,
    kSameAsInput,
    kRegister,
    kFixedRegister,
    kSlot,
    kFixedSlot,
    kImmediate,
    kNone,
    kConstant,
    kUnique,
    kUniqueRegister,
    kDeoptArg
  };

  struct TestOperand {
    TestOperand() : type_(kInvalid), vreg_(), value_(kNoValue), rep_(kNoRep) {}
    explicit TestOperand(TestOperandType type)
        : type_(type), vreg_(), value_(kNoValue), rep_(kNoRep) {}
    // For tests that do register allocation.
    TestOperand(TestOperandType type, VReg vreg, int value = kNoValue)
        : type_(type), vreg_(vreg), value_(value), rep_(vreg.rep_) {}
    // For immediates, constants, and tests that don't do register allocation.
    TestOperand(TestOperandType type, int value,
                MachineRepresentation rep = kNoRep)
        : type_(type), vreg_(), value_(value), rep_(rep) {}

    TestOperandType type_;
    VReg vreg_;
    int value_;
    MachineRepresentation rep_;
  };

  static TestOperand Same() { return TestOperand(kSameAsInput); }

  static TestOperand Reg(VReg vreg, int index = kNoValue) {
    TestOperandType type = (index == kNoValue) ? kRegister : kFixedRegister;
    return TestOperand(type, vreg, index);
  }

  static TestOperand Reg(int index = kNoValue,
                         MachineRepresentation rep = kNoRep) {
    return Reg(VReg(kNoValue, rep), index);
  }

  static TestOperand FPReg(int index = kNoValue,
                           MachineRepresentation rep = kFloat64) {
    return Reg(index, rep);
  }

  static TestOperand Slot(VReg vreg, int index = kNoValue) {
    TestOperandType type = (index == kNoValue) ? kSlot : kFixedSlot;
    return TestOperand(type, vreg, index);
  }

  static TestOperand Slot(int index = kNoValue,
                          MachineRepresentation rep = kNoRep) {
    return Slot(VReg(kNoValue, rep), index);
  }

  static TestOperand Const(int index) {
    CHECK_NE(kNoValue, index);
    return TestOperand(kConstant, index);
  }

  static TestOperand DeoptArg(VReg vreg) {
    return TestOperand(kDeoptArg, vreg);
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
    BlockCompletion completion = {kFallThrough, TestOperand(kImmediate, 0), 1,
                                  kNoValue};
    return completion;
  }

  static BlockCompletion Jump(int offset,
                              TestOperand operand = TestOperand(kImmediate,
                                                                0)) {
    BlockCompletion completion = {kJump, operand, offset, kNoValue};
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
  InstructionSequenceTest(const InstructionSequenceTest&) = delete;
  InstructionSequenceTest& operator=(const InstructionSequenceTest&) = delete;

  void SetNumRegs(int num_general_registers, int num_double_registers);
  int GetNumRegs(MachineRepresentation rep);
  int GetAllocatableCode(int index, MachineRepresentation rep = kNoRep);
  const RegisterConfiguration* config();
  InstructionSequence* sequence();

  void StartLoop(int loop_blocks);
  void EndLoop();
  void StartBlock(bool deferred = false);
  Instruction* EndBlock(BlockCompletion completion = FallThrough());

  TestOperand Imm(int32_t imm = 0);
  VReg Define(TestOperand output_op);
  VReg Parameter(TestOperand output_op = Reg()) { return Define(output_op); }
  VReg FPParameter(MachineRepresentation rep = kFloat64) {
    return Parameter(FPReg(kNoValue, rep));
  }

  MachineRepresentation GetCanonicalRep(TestOperand op) {
    return IsFloatingPoint(op.rep_) ? op.rep_
                                    : sequence()->DefaultRepresentation();
  }

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

  // Called after all instructions have been inserted.
  void WireBlocks();

 private:
  virtual bool DoesRegisterAllocation() const { return true; }

  VReg NewReg(TestOperand op = TestOperand()) {
    int vreg = sequence()->NextVirtualRegister();
    if (IsFloatingPoint(op.rep_))
      sequence()->MarkAsRepresentation(op.rep_, vreg);
    return VReg(vreg, op.rep_);
  }

  static TestOperand Invalid() { return TestOperand(kInvalid); }

  Instruction* EmitBranch(TestOperand input_op);
  Instruction* EmitFallThrough();
  Instruction* EmitJump(TestOperand input_op);
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
  void CalculateDominators();

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

  using LoopBlocks = std::vector<LoopData>;
  using Instructions = std::map<int, const Instruction*>;
  using Completions = std::vector<BlockCompletion>;

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
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_INSTRUCTION_SEQUENCE_UNITTEST_H_
