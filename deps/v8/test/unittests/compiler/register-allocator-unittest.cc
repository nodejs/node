// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/register-allocator.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef BasicBlock::RpoNumber Rpo;

namespace {

static const char*
    general_register_names_[RegisterConfiguration::kMaxGeneralRegisters];
static const char*
    double_register_names_[RegisterConfiguration::kMaxDoubleRegisters];
static char register_names_[10 * (RegisterConfiguration::kMaxGeneralRegisters +
                                  RegisterConfiguration::kMaxDoubleRegisters)];


static void InitializeRegisterNames() {
  char* loc = register_names_;
  for (int i = 0; i < RegisterConfiguration::kMaxGeneralRegisters; ++i) {
    general_register_names_[i] = loc;
    loc += base::OS::SNPrintF(loc, 100, "gp_%d", i);
    *loc++ = 0;
  }
  for (int i = 0; i < RegisterConfiguration::kMaxDoubleRegisters; ++i) {
    double_register_names_[i] = loc;
    loc += base::OS::SNPrintF(loc, 100, "fp_%d", i) + 1;
    *loc++ = 0;
  }
}

enum BlockCompletionType { kFallThrough, kBranch, kJump };

struct BlockCompletion {
  BlockCompletionType type_;
  int vreg_;
  int offset_0_;
  int offset_1_;
};

static const int kInvalidJumpOffset = kMinInt;

BlockCompletion FallThrough() {
  BlockCompletion completion = {kFallThrough, -1, 1, kInvalidJumpOffset};
  return completion;
}


BlockCompletion Jump(int offset) {
  BlockCompletion completion = {kJump, -1, offset, kInvalidJumpOffset};
  return completion;
}


BlockCompletion Branch(int vreg, int left_offset, int right_offset) {
  BlockCompletion completion = {kBranch, vreg, left_offset, right_offset};
  return completion;
}

}  // namespace


class RegisterAllocatorTest : public TestWithZone {
 public:
  static const int kDefaultNRegs = 4;

  RegisterAllocatorTest()
      : num_general_registers_(kDefaultNRegs),
        num_double_registers_(kDefaultNRegs),
        instruction_blocks_(zone()),
        current_block_(nullptr),
        is_last_block_(false) {
    InitializeRegisterNames();
  }

  void SetNumRegs(int num_general_registers, int num_double_registers) {
    CHECK(instruction_blocks_.empty());
    num_general_registers_ = num_general_registers;
    num_double_registers_ = num_double_registers;
  }

  RegisterConfiguration* config() {
    if (config_.is_empty()) {
      config_.Reset(new RegisterConfiguration(
          num_general_registers_, num_double_registers_, num_double_registers_,
          general_register_names_, double_register_names_));
    }
    return config_.get();
  }

  Frame* frame() {
    if (frame_.is_empty()) {
      frame_.Reset(new Frame());
    }
    return frame_.get();
  }

  InstructionSequence* sequence() {
    if (sequence_.is_empty()) {
      sequence_.Reset(new InstructionSequence(zone(), &instruction_blocks_));
    }
    return sequence_.get();
  }

  RegisterAllocator* allocator() {
    if (allocator_.is_empty()) {
      allocator_.Reset(
          new RegisterAllocator(config(), zone(), frame(), sequence()));
    }
    return allocator_.get();
  }

  void StartLoop(int loop_blocks) {
    CHECK(current_block_ == nullptr);
    if (!loop_blocks_.empty()) {
      CHECK(!loop_blocks_.back().loop_header_.IsValid());
    }
    LoopData loop_data = {Rpo::Invalid(), loop_blocks};
    loop_blocks_.push_back(loop_data);
  }

  void EndLoop() {
    CHECK(current_block_ == nullptr);
    CHECK(!loop_blocks_.empty());
    CHECK_EQ(0, loop_blocks_.back().expected_blocks_);
    loop_blocks_.pop_back();
  }

  void StartLastBlock() {
    CHECK(!is_last_block_);
    is_last_block_ = true;
    NewBlock();
  }

  void StartBlock() {
    CHECK(!is_last_block_);
    NewBlock();
  }

  void EndBlock(BlockCompletion completion = FallThrough()) {
    completions_.push_back(completion);
    switch (completion.type_) {
      case kFallThrough:
        if (is_last_block_) break;
        // TODO(dcarney): we don't emit this after returns.
        EmitFallThrough();
        break;
      case kJump:
        EmitJump();
        break;
      case kBranch:
        EmitBranch(completion.vreg_);
        break;
    }
    CHECK(current_block_ != nullptr);
    sequence()->EndBlock(current_block_->rpo_number());
    current_block_ = nullptr;
  }

  void Allocate() {
    CHECK_EQ(nullptr, current_block_);
    CHECK(is_last_block_);
    WireBlocks();
    if (FLAG_trace_alloc || FLAG_trace_turbo) {
      OFStream os(stdout);
      PrintableInstructionSequence printable = {config(), sequence()};
      os << "Before: " << std::endl << printable << std::endl;
    }
    allocator()->Allocate();
    if (FLAG_trace_alloc || FLAG_trace_turbo) {
      OFStream os(stdout);
      PrintableInstructionSequence printable = {config(), sequence()};
      os << "After: " << std::endl << printable << std::endl;
    }
  }

  int NewReg() { return sequence()->NextVirtualRegister(); }

  int Parameter() {
    int vreg = NewReg();
    InstructionOperand* outputs[1]{UseRegister(vreg)};
    Emit(kArchNop, 1, outputs);
    return vreg;
  }

  Instruction* Return(int vreg) {
    InstructionOperand* inputs[1]{UseRegister(vreg)};
    return Emit(kArchRet, 0, nullptr, 1, inputs);
  }

  PhiInstruction* Phi(int vreg) {
    PhiInstruction* phi = new (zone()) PhiInstruction(zone(), NewReg());
    phi->operands().push_back(vreg);
    current_block_->AddPhi(phi);
    return phi;
  }

  int DefineConstant(int32_t imm = 0) {
    int virtual_register = NewReg();
    sequence()->AddConstant(virtual_register, Constant(imm));
    InstructionOperand* outputs[1]{
        ConstantOperand::Create(virtual_register, zone())};
    Emit(kArchNop, 1, outputs);
    return virtual_register;
  }

  ImmediateOperand* Immediate(int32_t imm = 0) {
    int index = sequence()->AddImmediate(Constant(imm));
    return ImmediateOperand::Create(index, zone());
  }

  Instruction* EmitFRI(int output_vreg, int input_vreg_0) {
    InstructionOperand* outputs[1]{DefineSameAsFirst(output_vreg)};
    InstructionOperand* inputs[2]{UseRegister(input_vreg_0), Immediate()};
    return Emit(kArchNop, 1, outputs, 2, inputs);
  }

  Instruction* EmitFRU(int output_vreg, int input_vreg_0, int input_vreg_1) {
    InstructionOperand* outputs[1]{DefineSameAsFirst(output_vreg)};
    InstructionOperand* inputs[2]{UseRegister(input_vreg_0), Use(input_vreg_1)};
    return Emit(kArchNop, 1, outputs, 2, inputs);
  }

  Instruction* EmitRRR(int output_vreg, int input_vreg_0, int input_vreg_1) {
    InstructionOperand* outputs[1]{UseRegister(output_vreg)};
    InstructionOperand* inputs[2]{UseRegister(input_vreg_0),
                                  UseRegister(input_vreg_1)};
    return Emit(kArchNop, 1, outputs, 2, inputs);
  }

 private:
  InstructionOperand* Unallocated(int vreg,
                                  UnallocatedOperand::ExtendedPolicy policy) {
    UnallocatedOperand* op = new (zone()) UnallocatedOperand(policy);
    op->set_virtual_register(vreg);
    return op;
  }

  InstructionOperand* Unallocated(int vreg,
                                  UnallocatedOperand::ExtendedPolicy policy,
                                  UnallocatedOperand::Lifetime lifetime) {
    UnallocatedOperand* op = new (zone()) UnallocatedOperand(policy, lifetime);
    op->set_virtual_register(vreg);
    return op;
  }

  InstructionOperand* UseRegister(int vreg) {
    return Unallocated(vreg, UnallocatedOperand::MUST_HAVE_REGISTER);
  }

  InstructionOperand* DefineSameAsFirst(int vreg) {
    return Unallocated(vreg, UnallocatedOperand::SAME_AS_FIRST_INPUT);
  }

  InstructionOperand* Use(int vreg) {
    return Unallocated(vreg, UnallocatedOperand::NONE,
                       UnallocatedOperand::USED_AT_START);
  }

  void EmitBranch(int vreg) {
    InstructionOperand* inputs[4]{UseRegister(vreg), Immediate(), Immediate(),
                                  Immediate()};
    InstructionCode opcode = kArchJmp | FlagsModeField::encode(kFlags_branch) |
                             FlagsConditionField::encode(kEqual);
    Instruction* instruction =
        NewInstruction(opcode, 0, nullptr, 4, inputs)->MarkAsControl();
    sequence()->AddInstruction(instruction);
  }

  void EmitFallThrough() {
    Instruction* instruction =
        NewInstruction(kArchNop, 0, nullptr)->MarkAsControl();
    sequence()->AddInstruction(instruction);
  }

  void EmitJump() {
    InstructionOperand* inputs[1]{Immediate()};
    Instruction* instruction =
        NewInstruction(kArchJmp, 0, nullptr, 1, inputs)->MarkAsControl();
    sequence()->AddInstruction(instruction);
  }

  Instruction* NewInstruction(InstructionCode code, size_t outputs_size,
                              InstructionOperand** outputs,
                              size_t inputs_size = 0,
                              InstructionOperand* *inputs = nullptr,
                              size_t temps_size = 0,
                              InstructionOperand* *temps = nullptr) {
    CHECK_NE(nullptr, current_block_);
    return Instruction::New(zone(), code, outputs_size, outputs, inputs_size,
                            inputs, temps_size, temps);
  }

  Instruction* Emit(InstructionCode code, size_t outputs_size,
                    InstructionOperand** outputs, size_t inputs_size = 0,
                    InstructionOperand* *inputs = nullptr,
                    size_t temps_size = 0,
                    InstructionOperand* *temps = nullptr) {
    Instruction* instruction = NewInstruction(
        code, outputs_size, outputs, inputs_size, inputs, temps_size, temps);
    sequence()->AddInstruction(instruction);
    return instruction;
  }

  InstructionBlock* NewBlock() {
    CHECK(current_block_ == nullptr);
    BasicBlock::Id block_id =
        BasicBlock::Id::FromSize(instruction_blocks_.size());
    Rpo rpo = Rpo::FromInt(block_id.ToInt());
    Rpo loop_header = Rpo::Invalid();
    Rpo loop_end = Rpo::Invalid();
    if (!loop_blocks_.empty()) {
      auto& loop_data = loop_blocks_.back();
      // This is a loop header.
      if (!loop_data.loop_header_.IsValid()) {
        loop_end = Rpo::FromInt(block_id.ToInt() + loop_data.expected_blocks_);
        loop_data.expected_blocks_--;
        loop_data.loop_header_ = rpo;
      } else {
        // This is a loop body.
        CHECK_NE(0, loop_data.expected_blocks_);
        // TODO(dcarney): handle nested loops.
        loop_data.expected_blocks_--;
        loop_header = loop_data.loop_header_;
      }
    }
    // Construct instruction block.
    InstructionBlock* instruction_block = new (zone()) InstructionBlock(
        zone(), block_id, rpo, rpo, loop_header, loop_end, false);
    instruction_blocks_.push_back(instruction_block);
    current_block_ = instruction_block;
    sequence()->StartBlock(rpo);
    return instruction_block;
  }

  void WireBlocks() {
    CHECK(instruction_blocks_.size() == completions_.size());
    size_t offset = 0;
    size_t size = instruction_blocks_.size();
    for (const auto& completion : completions_) {
      switch (completion.type_) {
        case kFallThrough:
          if (offset == size - 1) break;
        // Fallthrough.
        case kJump:
          WireBlock(offset, completion.offset_0_);
          break;
        case kBranch:
          WireBlock(offset, completion.offset_0_);
          WireBlock(offset, completion.offset_1_);
          break;
      }
      ++offset;
    }
  }

  void WireBlock(size_t block_offset, int jump_offset) {
    size_t target_block_offset =
        block_offset + static_cast<size_t>(jump_offset);
    CHECK(block_offset < instruction_blocks_.size());
    CHECK(target_block_offset < instruction_blocks_.size());
    InstructionBlock* block = instruction_blocks_[block_offset];
    InstructionBlock* target = instruction_blocks_[target_block_offset];
    block->successors().push_back(target->rpo_number());
    target->predecessors().push_back(block->rpo_number());
  }

  struct LoopData {
    Rpo loop_header_;
    int expected_blocks_;
  };
  typedef std::vector<LoopData> LoopBlocks;
  typedef std::vector<BlockCompletion> Completions;

  SmartPointer<RegisterConfiguration> config_;
  SmartPointer<Frame> frame_;
  SmartPointer<RegisterAllocator> allocator_;
  SmartPointer<InstructionSequence> sequence_;
  int num_general_registers_;
  int num_double_registers_;

  // Block building state.
  InstructionBlocks instruction_blocks_;
  Completions completions_;
  LoopBlocks loop_blocks_;
  InstructionBlock* current_block_;
  bool is_last_block_;
};


TEST_F(RegisterAllocatorTest, CanAllocateThreeRegisters) {
  StartLastBlock();
  int a_reg = Parameter();
  int b_reg = Parameter();
  int c_reg = NewReg();
  Instruction* res = EmitRRR(c_reg, a_reg, b_reg);
  Return(c_reg);
  EndBlock();

  Allocate();

  ASSERT_TRUE(res->OutputAt(0)->IsRegister());
}


TEST_F(RegisterAllocatorTest, SimpleLoop) {
  // i = K;
  // while(true) { i++ }

  StartBlock();
  int i_reg = DefineConstant();
  EndBlock();

  {
    StartLoop(1);

    StartLastBlock();
    PhiInstruction* phi = Phi(i_reg);
    int ipp = NewReg();
    EmitFRU(ipp, phi->virtual_register(), DefineConstant());
    phi->operands().push_back(ipp);
    EndBlock(Jump(0));

    EndLoop();
  }

  Allocate();
}


TEST_F(RegisterAllocatorTest, SimpleBranch) {
  // return i ? K1 : K2
  StartBlock();
  int i_reg = DefineConstant();
  EndBlock(Branch(i_reg, 1, 2));

  StartBlock();
  Return(DefineConstant());
  EndBlock();

  StartLastBlock();
  Return(DefineConstant());
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, RegressionPhisNeedTooManyRegisters) {
  const size_t kNumRegs = 3;
  const size_t kParams = kNumRegs + 1;
  int parameters[kParams];

  // Override number of registers.
  SetNumRegs(kNumRegs, kNumRegs);

  // Initial block.
  StartBlock();
  int constant = DefineConstant();
  for (size_t i = 0; i < arraysize(parameters); ++i) {
    parameters[i] = DefineConstant();
  }
  EndBlock();

  PhiInstruction* phis[kParams];
  {
    StartLoop(2);

    // Loop header.
    StartBlock();

    for (size_t i = 0; i < arraysize(parameters); ++i) {
      phis[i] = Phi(parameters[i]);
    }

    // Perform some computations.
    // something like phi[i] += const
    for (size_t i = 0; i < arraysize(parameters); ++i) {
      int result = NewReg();
      EmitFRU(result, phis[i]->virtual_register(), constant);
      phis[i]->operands().push_back(result);
    }

    EndBlock(Branch(DefineConstant(), 1, 2));

    // Jump back to loop header.
    StartBlock();
    EndBlock(Jump(-1));

    EndLoop();
  }

  // End block.
  StartLastBlock();

  // Return sum.
  Return(DefineConstant());
  EndBlock();

  Allocate();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
