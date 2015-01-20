// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/pipeline.h"
#include "test/unittests/compiler/instruction-sequence-unittest.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

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


InstructionSequenceTest::InstructionSequenceTest()
    : sequence_(nullptr),
      num_general_registers_(kDefaultNRegs),
      num_double_registers_(kDefaultNRegs),
      instruction_blocks_(zone()),
      current_instruction_index_(-1),
      current_block_(nullptr),
      block_returns_(false) {
  InitializeRegisterNames();
}


void InstructionSequenceTest::SetNumRegs(int num_general_registers,
                                         int num_double_registers) {
  CHECK(config_.is_empty());
  CHECK(instructions_.empty());
  CHECK(instruction_blocks_.empty());
  num_general_registers_ = num_general_registers;
  num_double_registers_ = num_double_registers;
}


RegisterConfiguration* InstructionSequenceTest::config() {
  if (config_.is_empty()) {
    config_.Reset(new RegisterConfiguration(
        num_general_registers_, num_double_registers_, num_double_registers_,
        general_register_names_, double_register_names_));
  }
  return config_.get();
}


InstructionSequence* InstructionSequenceTest::sequence() {
  if (sequence_ == nullptr) {
    sequence_ = new (zone()) InstructionSequence(zone(), &instruction_blocks_);
  }
  return sequence_;
}


void InstructionSequenceTest::StartLoop(int loop_blocks) {
  CHECK(current_block_ == nullptr);
  if (!loop_blocks_.empty()) {
    CHECK(!loop_blocks_.back().loop_header_.IsValid());
  }
  LoopData loop_data = {Rpo::Invalid(), loop_blocks};
  loop_blocks_.push_back(loop_data);
}


void InstructionSequenceTest::EndLoop() {
  CHECK(current_block_ == nullptr);
  CHECK(!loop_blocks_.empty());
  CHECK_EQ(0, loop_blocks_.back().expected_blocks_);
  loop_blocks_.pop_back();
}


void InstructionSequenceTest::StartBlock() {
  block_returns_ = false;
  NewBlock();
}


int InstructionSequenceTest::EndBlock(BlockCompletion completion) {
  int instruction_index = kMinInt;
  if (block_returns_) {
    CHECK(completion.type_ == kBlockEnd || completion.type_ == kFallThrough);
    completion.type_ = kBlockEnd;
  }
  switch (completion.type_) {
    case kBlockEnd:
      break;
    case kFallThrough:
      instruction_index = EmitFallThrough();
      break;
    case kJump:
      CHECK(!block_returns_);
      instruction_index = EmitJump();
      break;
    case kBranch:
      CHECK(!block_returns_);
      instruction_index = EmitBranch(completion.op_);
      break;
  }
  completions_.push_back(completion);
  CHECK(current_block_ != nullptr);
  sequence()->EndBlock(current_block_->rpo_number());
  current_block_ = nullptr;
  return instruction_index;
}


InstructionSequenceTest::TestOperand InstructionSequenceTest::Imm(int32_t imm) {
  int index = sequence()->AddImmediate(Constant(imm));
  return TestOperand(kImmediate, index);
}


InstructionSequenceTest::VReg InstructionSequenceTest::Define(
    TestOperand output_op) {
  VReg vreg = NewReg();
  InstructionOperand* outputs[1]{ConvertOutputOp(vreg, output_op)};
  Emit(vreg.value_, kArchNop, 1, outputs);
  return vreg;
}


int InstructionSequenceTest::Return(TestOperand input_op_0) {
  block_returns_ = true;
  InstructionOperand* inputs[1]{ConvertInputOp(input_op_0)};
  return Emit(NewIndex(), kArchRet, 0, nullptr, 1, inputs);
}


PhiInstruction* InstructionSequenceTest::Phi(VReg incoming_vreg_0,
                                             VReg incoming_vreg_1,
                                             VReg incoming_vreg_2,
                                             VReg incoming_vreg_3) {
  auto phi = new (zone()) PhiInstruction(zone(), NewReg().value_, 10);
  VReg inputs[] = {incoming_vreg_0, incoming_vreg_1, incoming_vreg_2,
                   incoming_vreg_3};
  for (size_t i = 0; i < arraysize(inputs); ++i) {
    if (inputs[i].value_ == kNoValue) break;
    Extend(phi, inputs[i]);
  }
  current_block_->AddPhi(phi);
  return phi;
}


void InstructionSequenceTest::Extend(PhiInstruction* phi, VReg vreg) {
  phi->Extend(zone(), vreg.value_);
}


InstructionSequenceTest::VReg InstructionSequenceTest::DefineConstant(
    int32_t imm) {
  VReg vreg = NewReg();
  sequence()->AddConstant(vreg.value_, Constant(imm));
  InstructionOperand* outputs[1]{ConstantOperand::Create(vreg.value_, zone())};
  Emit(vreg.value_, kArchNop, 1, outputs);
  return vreg;
}


int InstructionSequenceTest::EmitNop() { return Emit(NewIndex(), kArchNop); }


static size_t CountInputs(size_t size,
                          InstructionSequenceTest::TestOperand* inputs) {
  size_t i = 0;
  for (; i < size; ++i) {
    if (inputs[i].type_ == InstructionSequenceTest::kInvalid) break;
  }
  return i;
}


int InstructionSequenceTest::EmitI(size_t input_size, TestOperand* inputs) {
  InstructionOperand** mapped_inputs = ConvertInputs(input_size, inputs);
  return Emit(NewIndex(), kArchNop, 0, nullptr, input_size, mapped_inputs);
}


int InstructionSequenceTest::EmitI(TestOperand input_op_0,
                                   TestOperand input_op_1,
                                   TestOperand input_op_2,
                                   TestOperand input_op_3) {
  TestOperand inputs[] = {input_op_0, input_op_1, input_op_2, input_op_3};
  return EmitI(CountInputs(arraysize(inputs), inputs), inputs);
}


InstructionSequenceTest::VReg InstructionSequenceTest::EmitOI(
    TestOperand output_op, size_t input_size, TestOperand* inputs) {
  VReg output_vreg = NewReg();
  InstructionOperand* outputs[1]{ConvertOutputOp(output_vreg, output_op)};
  InstructionOperand** mapped_inputs = ConvertInputs(input_size, inputs);
  Emit(output_vreg.value_, kArchNop, 1, outputs, input_size, mapped_inputs);
  return output_vreg;
}


InstructionSequenceTest::VReg InstructionSequenceTest::EmitOI(
    TestOperand output_op, TestOperand input_op_0, TestOperand input_op_1,
    TestOperand input_op_2, TestOperand input_op_3) {
  TestOperand inputs[] = {input_op_0, input_op_1, input_op_2, input_op_3};
  return EmitOI(output_op, CountInputs(arraysize(inputs), inputs), inputs);
}


InstructionSequenceTest::VReg InstructionSequenceTest::EmitCall(
    TestOperand output_op, size_t input_size, TestOperand* inputs) {
  VReg output_vreg = NewReg();
  InstructionOperand* outputs[1]{ConvertOutputOp(output_vreg, output_op)};
  CHECK(UnallocatedOperand::cast(outputs[0])->HasFixedPolicy());
  InstructionOperand** mapped_inputs = ConvertInputs(input_size, inputs);
  Emit(output_vreg.value_, kArchCallCodeObject, 1, outputs, input_size,
       mapped_inputs, 0, nullptr, true);
  return output_vreg;
}


InstructionSequenceTest::VReg InstructionSequenceTest::EmitCall(
    TestOperand output_op, TestOperand input_op_0, TestOperand input_op_1,
    TestOperand input_op_2, TestOperand input_op_3) {
  TestOperand inputs[] = {input_op_0, input_op_1, input_op_2, input_op_3};
  return EmitCall(output_op, CountInputs(arraysize(inputs), inputs), inputs);
}


const Instruction* InstructionSequenceTest::GetInstruction(
    int instruction_index) {
  auto it = instructions_.find(instruction_index);
  CHECK(it != instructions_.end());
  return it->second;
}


int InstructionSequenceTest::EmitBranch(TestOperand input_op) {
  InstructionOperand* inputs[4]{ConvertInputOp(input_op), ConvertInputOp(Imm()),
                                ConvertInputOp(Imm()), ConvertInputOp(Imm())};
  InstructionCode opcode = kArchJmp | FlagsModeField::encode(kFlags_branch) |
                           FlagsConditionField::encode(kEqual);
  auto instruction =
      NewInstruction(opcode, 0, nullptr, 4, inputs)->MarkAsControl();
  return AddInstruction(NewIndex(), instruction);
}


int InstructionSequenceTest::EmitFallThrough() {
  auto instruction = NewInstruction(kArchNop, 0, nullptr)->MarkAsControl();
  return AddInstruction(NewIndex(), instruction);
}


int InstructionSequenceTest::EmitJump() {
  InstructionOperand* inputs[1]{ConvertInputOp(Imm())};
  auto instruction =
      NewInstruction(kArchJmp, 0, nullptr, 1, inputs)->MarkAsControl();
  return AddInstruction(NewIndex(), instruction);
}


Instruction* InstructionSequenceTest::NewInstruction(
    InstructionCode code, size_t outputs_size, InstructionOperand** outputs,
    size_t inputs_size, InstructionOperand** inputs, size_t temps_size,
    InstructionOperand** temps) {
  CHECK_NE(nullptr, current_block_);
  return Instruction::New(zone(), code, outputs_size, outputs, inputs_size,
                          inputs, temps_size, temps);
}


InstructionOperand* InstructionSequenceTest::Unallocated(
    TestOperand op, UnallocatedOperand::ExtendedPolicy policy) {
  auto unallocated = new (zone()) UnallocatedOperand(policy);
  unallocated->set_virtual_register(op.vreg_.value_);
  return unallocated;
}


InstructionOperand* InstructionSequenceTest::Unallocated(
    TestOperand op, UnallocatedOperand::ExtendedPolicy policy,
    UnallocatedOperand::Lifetime lifetime) {
  auto unallocated = new (zone()) UnallocatedOperand(policy, lifetime);
  unallocated->set_virtual_register(op.vreg_.value_);
  return unallocated;
}


InstructionOperand* InstructionSequenceTest::Unallocated(
    TestOperand op, UnallocatedOperand::ExtendedPolicy policy, int index) {
  auto unallocated = new (zone()) UnallocatedOperand(policy, index);
  unallocated->set_virtual_register(op.vreg_.value_);
  return unallocated;
}


InstructionOperand* InstructionSequenceTest::Unallocated(
    TestOperand op, UnallocatedOperand::BasicPolicy policy, int index) {
  auto unallocated = new (zone()) UnallocatedOperand(policy, index);
  unallocated->set_virtual_register(op.vreg_.value_);
  return unallocated;
}


InstructionOperand** InstructionSequenceTest::ConvertInputs(
    size_t input_size, TestOperand* inputs) {
  InstructionOperand** mapped_inputs =
      zone()->NewArray<InstructionOperand*>(static_cast<int>(input_size));
  for (size_t i = 0; i < input_size; ++i) {
    mapped_inputs[i] = ConvertInputOp(inputs[i]);
  }
  return mapped_inputs;
}


InstructionOperand* InstructionSequenceTest::ConvertInputOp(TestOperand op) {
  if (op.type_ == kImmediate) {
    CHECK_EQ(op.vreg_.value_, kNoValue);
    return ImmediateOperand::Create(op.value_, zone());
  }
  CHECK_NE(op.vreg_.value_, kNoValue);
  switch (op.type_) {
    case kNone:
      return Unallocated(op, UnallocatedOperand::NONE,
                         UnallocatedOperand::USED_AT_START);
    case kUnique:
      return Unallocated(op, UnallocatedOperand::NONE);
    case kUniqueRegister:
      return Unallocated(op, UnallocatedOperand::MUST_HAVE_REGISTER);
    case kRegister:
      return Unallocated(op, UnallocatedOperand::MUST_HAVE_REGISTER,
                         UnallocatedOperand::USED_AT_START);
    case kFixedRegister:
      CHECK(0 <= op.value_ && op.value_ < num_general_registers_);
      return Unallocated(op, UnallocatedOperand::FIXED_REGISTER, op.value_);
    case kFixedSlot:
      return Unallocated(op, UnallocatedOperand::FIXED_SLOT, op.value_);
    default:
      break;
  }
  CHECK(false);
  return NULL;
}


InstructionOperand* InstructionSequenceTest::ConvertOutputOp(VReg vreg,
                                                             TestOperand op) {
  CHECK_EQ(op.vreg_.value_, kNoValue);
  op.vreg_ = vreg;
  switch (op.type_) {
    case kSameAsFirst:
      return Unallocated(op, UnallocatedOperand::SAME_AS_FIRST_INPUT);
    case kRegister:
      return Unallocated(op, UnallocatedOperand::MUST_HAVE_REGISTER);
    case kFixedSlot:
      return Unallocated(op, UnallocatedOperand::FIXED_SLOT, op.value_);
    case kFixedRegister:
      CHECK(0 <= op.value_ && op.value_ < num_general_registers_);
      return Unallocated(op, UnallocatedOperand::FIXED_REGISTER, op.value_);
    default:
      break;
  }
  CHECK(false);
  return NULL;
}


InstructionBlock* InstructionSequenceTest::NewBlock() {
  CHECK(current_block_ == nullptr);
  auto block_id = BasicBlock::Id::FromSize(instruction_blocks_.size());
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
  auto instruction_block = new (zone())
      InstructionBlock(zone(), block_id, rpo, loop_header, loop_end, false);
  instruction_blocks_.push_back(instruction_block);
  current_block_ = instruction_block;
  sequence()->StartBlock(rpo);
  return instruction_block;
}


void InstructionSequenceTest::WireBlocks() {
  CHECK_EQ(nullptr, current_block());
  CHECK(instruction_blocks_.size() == completions_.size());
  size_t offset = 0;
  for (const auto& completion : completions_) {
    switch (completion.type_) {
      case kBlockEnd:
        break;
      case kFallThrough:  // Fallthrough.
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


void InstructionSequenceTest::WireBlock(size_t block_offset, int jump_offset) {
  size_t target_block_offset = block_offset + static_cast<size_t>(jump_offset);
  CHECK(block_offset < instruction_blocks_.size());
  CHECK(target_block_offset < instruction_blocks_.size());
  auto block = instruction_blocks_[block_offset];
  auto target = instruction_blocks_[target_block_offset];
  block->successors().push_back(target->rpo_number());
  target->predecessors().push_back(block->rpo_number());
}


int InstructionSequenceTest::Emit(int instruction_index, InstructionCode code,
                                  size_t outputs_size,
                                  InstructionOperand** outputs,
                                  size_t inputs_size,
                                  InstructionOperand** inputs,
                                  size_t temps_size, InstructionOperand** temps,
                                  bool is_call) {
  auto instruction = NewInstruction(code, outputs_size, outputs, inputs_size,
                                    inputs, temps_size, temps);
  if (is_call) instruction->MarkAsCall();
  return AddInstruction(instruction_index, instruction);
}


int InstructionSequenceTest::AddInstruction(int instruction_index,
                                            Instruction* instruction) {
  sequence()->AddInstruction(instruction);
  return instruction_index;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
