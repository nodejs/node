// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-builder.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayBuilder::BytecodeArrayBuilder(Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      bytecodes_(zone),
      bytecode_generated_(false),
      last_block_end_(0),
      last_bytecode_start_(~0),
      exit_seen_in_block_(false),
      constants_map_(isolate->heap(), zone),
      constants_(zone),
      parameter_count_(-1),
      local_register_count_(-1),
      context_register_count_(-1),
      temporary_register_count_(0),
      free_temporaries_(zone) {}


void BytecodeArrayBuilder::set_locals_count(int number_of_locals) {
  local_register_count_ = number_of_locals;
  DCHECK_LE(context_register_count_, 0);
}


void BytecodeArrayBuilder::set_parameter_count(int number_of_parameters) {
  parameter_count_ = number_of_parameters;
}


void BytecodeArrayBuilder::set_context_count(int number_of_contexts) {
  context_register_count_ = number_of_contexts;
  DCHECK_GE(local_register_count_, 0);
}


Register BytecodeArrayBuilder::first_context_register() const {
  DCHECK_GT(context_register_count_, 0);
  return Register(local_register_count_);
}


Register BytecodeArrayBuilder::last_context_register() const {
  DCHECK_GT(context_register_count_, 0);
  return Register(local_register_count_ + context_register_count_ - 1);
}


Register BytecodeArrayBuilder::first_temporary_register() const {
  DCHECK_GT(temporary_register_count_, 0);
  return Register(fixed_register_count());
}


Register BytecodeArrayBuilder::last_temporary_register() const {
  DCHECK_GT(temporary_register_count_, 0);
  return Register(fixed_register_count() + temporary_register_count_ - 1);
}


Register BytecodeArrayBuilder::Parameter(int parameter_index) const {
  DCHECK_GE(parameter_index, 0);
  return Register::FromParameterIndex(parameter_index, parameter_count());
}


bool BytecodeArrayBuilder::RegisterIsParameterOrLocal(Register reg) const {
  return reg.is_parameter() || reg.index() < locals_count();
}


bool BytecodeArrayBuilder::RegisterIsTemporary(Register reg) const {
  return temporary_register_count_ > 0 && first_temporary_register() <= reg &&
         reg <= last_temporary_register();
}


Handle<BytecodeArray> BytecodeArrayBuilder::ToBytecodeArray() {
  DCHECK_EQ(bytecode_generated_, false);

  EnsureReturn();

  int bytecode_size = static_cast<int>(bytecodes_.size());
  int register_count = fixed_register_count() + temporary_register_count_;
  int frame_size = register_count * kPointerSize;

  Factory* factory = isolate_->factory();
  int constants_count = static_cast<int>(constants_.size());
  Handle<FixedArray> constant_pool =
      factory->NewFixedArray(constants_count, TENURED);
  for (int i = 0; i < constants_count; i++) {
    constant_pool->set(i, *constants_[i]);
  }

  Handle<BytecodeArray> output =
      factory->NewBytecodeArray(bytecode_size, &bytecodes_.front(), frame_size,
                                parameter_count(), constant_pool);
  bytecode_generated_ = true;
  return output;
}


template <size_t N>
void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t(&operands)[N]) {
  // Don't output dead code.
  if (exit_seen_in_block_) return;

  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), static_cast<int>(N));
  last_bytecode_start_ = bytecodes()->size();
  bytecodes()->push_back(Bytecodes::ToByte(bytecode));
  for (int i = 0; i < static_cast<int>(N); i++) {
    DCHECK(OperandIsValid(bytecode, i, operands[i]));
    switch (Bytecodes::GetOperandSize(bytecode, i)) {
      case OperandSize::kNone:
        UNREACHABLE();
      case OperandSize::kByte:
        bytecodes()->push_back(static_cast<uint8_t>(operands[i]));
        break;
      case OperandSize::kShort: {
        uint8_t operand_bytes[2];
        WriteUnalignedUInt16(operand_bytes, operands[i]);
        bytecodes()->insert(bytecodes()->end(), operand_bytes,
                            operand_bytes + 2);
        break;
      }
    }
  }
}


void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1, uint32_t operand2) {
  uint32_t operands[] = {operand0, operand1, operand2};
  Output(bytecode, operands);
}


void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1) {
  uint32_t operands[] = {operand0, operand1};
  Output(bytecode, operands);
}


void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0) {
  uint32_t operands[] = {operand0};
  Output(bytecode, operands);
}


void BytecodeArrayBuilder::Output(Bytecode bytecode) {
  // Don't output dead code.
  if (exit_seen_in_block_) return;

  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 0);
  last_bytecode_start_ = bytecodes()->size();
  bytecodes()->push_back(Bytecodes::ToByte(bytecode));
}


BytecodeArrayBuilder& BytecodeArrayBuilder::BinaryOperation(Token::Value op,
                                                            Register reg,
                                                            Strength strength) {
  if (is_strong(strength)) {
    UNIMPLEMENTED();
  }

  Output(BytecodeForBinaryOperation(op), reg.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CountOperation(Token::Value op,
                                                           Strength strength) {
  if (is_strong(strength)) {
    UNIMPLEMENTED();
  }

  Output(BytecodeForCountOperation(op));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LogicalNot() {
  Output(Bytecode::kLogicalNot);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::TypeOf() {
  Output(Bytecode::kTypeOf);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CompareOperation(
    Token::Value op, Register reg, Strength strength) {
  if (is_strong(strength)) {
    UNIMPLEMENTED();
  }

  Output(BytecodeForCompareOperation(op), reg.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(
    v8::internal::Smi* smi) {
  int32_t raw_smi = smi->value();
  if (raw_smi == 0) {
    Output(Bytecode::kLdaZero);
  } else if (raw_smi >= -128 && raw_smi <= 127) {
    Output(Bytecode::kLdaSmi8, static_cast<uint8_t>(raw_smi));
  } else {
    LoadLiteral(Handle<Object>(smi, isolate_));
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(Handle<Object> object) {
  size_t entry = GetConstantPoolEntry(object);
  if (FitsInIdx8Operand(entry)) {
    Output(Bytecode::kLdaConstant, static_cast<uint8_t>(entry));
  } else if (FitsInIdx16Operand(entry)) {
    Output(Bytecode::kLdaConstantWide, static_cast<uint16_t>(entry));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadUndefined() {
  Output(Bytecode::kLdaUndefined);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNull() {
  Output(Bytecode::kLdaNull);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadTheHole() {
  Output(Bytecode::kLdaTheHole);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadTrue() {
  Output(Bytecode::kLdaTrue);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadFalse() {
  Output(Bytecode::kLdaFalse);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadAccumulatorWithRegister(
    Register reg) {
  // TODO(oth): Avoid loading the accumulator with the register if the
  // previous bytecode stored the accumulator with the same register.
  Output(Bytecode::kLdar, reg.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreAccumulatorInRegister(
    Register reg) {
  // TODO(oth): Avoid storing the accumulator in the register if the
  // previous bytecode loaded the accumulator with the same register.
  Output(Bytecode::kStar, reg.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadGlobal(
    size_t name_index, int feedback_slot, LanguageMode language_mode,
    TypeofMode typeof_mode) {
  // TODO(rmcilroy): Potentially store language and typeof information in an
  // operand rather than having extra bytecodes.
  Bytecode bytecode = BytecodeForLoadGlobal(language_mode, typeof_mode);
  if (FitsInIdx8Operand(name_index) && FitsInIdx8Operand(feedback_slot)) {
    Output(bytecode, static_cast<uint8_t>(name_index),
           static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(name_index) &&
             FitsInIdx16Operand(feedback_slot)) {
    Output(BytecodeForWideOperands(bytecode), static_cast<uint16_t>(name_index),
           static_cast<uint16_t>(feedback_slot));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreGlobal(
    size_t name_index, int feedback_slot, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreGlobal(language_mode);
  if (FitsInIdx8Operand(name_index) && FitsInIdx8Operand(feedback_slot)) {
    Output(bytecode, static_cast<uint8_t>(name_index),
           static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(name_index) &&
             FitsInIdx16Operand(feedback_slot)) {
    Output(BytecodeForWideOperands(bytecode), static_cast<uint16_t>(name_index),
           static_cast<uint16_t>(feedback_slot));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadContextSlot(Register context,
                                                            int slot_index) {
  DCHECK(slot_index >= 0);
  if (FitsInIdx8Operand(slot_index)) {
    Output(Bytecode::kLdaContextSlot, context.ToOperand(),
           static_cast<uint8_t>(slot_index));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreContextSlot(Register context,
                                                             int slot_index) {
  DCHECK(slot_index >= 0);
  if (FitsInIdx8Operand(slot_index)) {
    Output(Bytecode::kStaContextSlot, context.ToOperand(),
           static_cast<uint8_t>(slot_index));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNamedProperty(
    Register object, size_t name_index, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForLoadIC(language_mode);
  if (FitsInIdx8Operand(name_index) && FitsInIdx8Operand(feedback_slot)) {
    Output(bytecode, object.ToOperand(), static_cast<uint8_t>(name_index),
           static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(name_index) &&
             FitsInIdx16Operand(feedback_slot)) {
    Output(BytecodeForWideOperands(bytecode), object.ToOperand(),
           static_cast<uint16_t>(name_index),
           static_cast<uint16_t>(feedback_slot));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadKeyedProperty(
    Register object, int feedback_slot, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForKeyedLoadIC(language_mode);
  if (FitsInIdx8Operand(feedback_slot)) {
    Output(bytecode, object.ToOperand(), static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(feedback_slot)) {
    Output(BytecodeForWideOperands(bytecode), object.ToOperand(),
           static_cast<uint16_t>(feedback_slot));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedProperty(
    Register object, size_t name_index, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreIC(language_mode);
  if (FitsInIdx8Operand(name_index) && FitsInIdx8Operand(feedback_slot)) {
    Output(bytecode, object.ToOperand(), static_cast<uint8_t>(name_index),
           static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(name_index) &&
             FitsInIdx16Operand(feedback_slot)) {
    Output(BytecodeForWideOperands(bytecode), object.ToOperand(),
           static_cast<uint16_t>(name_index),
           static_cast<uint16_t>(feedback_slot));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreKeyedProperty(
    Register object, Register key, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForKeyedStoreIC(language_mode);
  if (FitsInIdx8Operand(feedback_slot)) {
    Output(bytecode, object.ToOperand(), key.ToOperand(),
           static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(feedback_slot)) {
    Output(BytecodeForWideOperands(bytecode), object.ToOperand(),
           key.ToOperand(), static_cast<uint16_t>(feedback_slot));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateClosure(
    PretenureFlag tenured) {
  DCHECK(FitsInImm8Operand(tenured));
  Output(Bytecode::kCreateClosure, static_cast<uint8_t>(tenured));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArguments(
    CreateArgumentsType type) {
  // TODO(rmcilroy): Consider passing the type as a bytecode operand rather
  // than having two different bytecodes once we have better support for
  // branches in the InterpreterAssembler.
  Bytecode bytecode = BytecodeForCreateArguments(type);
  Output(bytecode);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateRegExpLiteral(
    int literal_index, Register flags) {
  if (FitsInIdx8Operand(literal_index)) {
    Output(Bytecode::kCreateRegExpLiteral, static_cast<uint8_t>(literal_index),
           flags.ToOperand());
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArrayLiteral(
    int literal_index, int flags) {
  DCHECK(FitsInImm8Operand(flags));  // Flags should fit in 8 bytes.
  if (FitsInIdx8Operand(literal_index)) {
    Output(Bytecode::kCreateArrayLiteral, static_cast<uint8_t>(literal_index),
           static_cast<uint8_t>(flags));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateObjectLiteral(
    int literal_index, int flags) {
  DCHECK(FitsInImm8Operand(flags));  // Flags should fit in 8 bytes.
  if (FitsInIdx8Operand(literal_index)) {
    Output(Bytecode::kCreateObjectLiteral, static_cast<uint8_t>(literal_index),
           static_cast<uint8_t>(flags));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::PushContext(Register context) {
  Output(Bytecode::kPushContext, context.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::PopContext(Register context) {
  Output(Bytecode::kPopContext, context.ToOperand());
  return *this;
}


bool BytecodeArrayBuilder::NeedToBooleanCast() {
  if (!LastBytecodeInSameBlock()) {
    // If the previous bytecode was from a different block return false.
    return true;
  }

  // If the previous bytecode puts a boolean in the accumulator return true.
  switch (Bytecodes::FromByte(bytecodes()->at(last_bytecode_start_))) {
    case Bytecode::kToBoolean:
      UNREACHABLE();
    case Bytecode::kLdaTrue:
    case Bytecode::kLdaFalse:
    case Bytecode::kLogicalNot:
    case Bytecode::kTestEqual:
    case Bytecode::kTestNotEqual:
    case Bytecode::kTestEqualStrict:
    case Bytecode::kTestNotEqualStrict:
    case Bytecode::kTestLessThan:
    case Bytecode::kTestLessThanOrEqual:
    case Bytecode::kTestGreaterThan:
    case Bytecode::kTestGreaterThanOrEqual:
    case Bytecode::kTestInstanceOf:
    case Bytecode::kTestIn:
    case Bytecode::kForInDone:
      return false;
    default:
      return true;
  }
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToBoolean() {
  // If the previous bytecode puts a boolean in the accumulator
  // there is no need to emit an instruction.
  if (NeedToBooleanCast()) {
    Output(Bytecode::kToBoolean);
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToJSObject() {
  Output(Bytecode::kToObject);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToName() {
  Output(Bytecode::kToName);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToNumber() {
  // TODO(rmcilroy): consider omitting if the preceeding bytecode always returns
  // a number.
  Output(Bytecode::kToNumber);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(BytecodeLabel* label) {
  if (label->is_forward_target()) {
    // An earlier jump instruction refers to this label. Update it's location.
    PatchJump(bytecodes()->end(), bytecodes()->begin() + label->offset());
    // Now treat as if the label will only be back referred to.
  }
  label->bind_to(bytecodes()->size());
  LeaveBasicBlock();
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(const BytecodeLabel& target,
                                                 BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  DCHECK(target.is_bound());
  PatchJump(bytecodes()->begin() + target.offset(),
            bytecodes()->begin() + label->offset());
  label->bind_to(target.offset());
  LeaveBasicBlock();
  return *this;
}


// static
Bytecode BytecodeArrayBuilder::GetJumpWithConstantOperand(
    Bytecode jump_bytecode) {
  switch (jump_bytecode) {
    case Bytecode::kJump:
      return Bytecode::kJumpConstant;
    case Bytecode::kJumpIfTrue:
      return Bytecode::kJumpIfTrueConstant;
    case Bytecode::kJumpIfFalse:
      return Bytecode::kJumpIfFalseConstant;
    case Bytecode::kJumpIfToBooleanTrue:
      return Bytecode::kJumpIfToBooleanTrueConstant;
    case Bytecode::kJumpIfToBooleanFalse:
      return Bytecode::kJumpIfToBooleanFalseConstant;
    case Bytecode::kJumpIfNull:
      return Bytecode::kJumpIfNullConstant;
    case Bytecode::kJumpIfUndefined:
      return Bytecode::kJumpIfUndefinedConstant;
    default:
      UNREACHABLE();
      return Bytecode::kJumpConstant;
  }
}


void BytecodeArrayBuilder::PatchJump(
    const ZoneVector<uint8_t>::iterator& jump_target,
    ZoneVector<uint8_t>::iterator jump_location) {
  Bytecode jump_bytecode = Bytecodes::FromByte(*jump_location);
  int delta = static_cast<int>(jump_target - jump_location);

  DCHECK(Bytecodes::IsJump(jump_bytecode));
  DCHECK_EQ(Bytecodes::Size(jump_bytecode), 2);
  DCHECK_NE(delta, 0);

  if (FitsInImm8Operand(delta)) {
    // Just update the operand
    jump_location++;
    *jump_location = static_cast<uint8_t>(delta);
  } else {
    // Update the jump type and operand
    size_t entry = GetConstantPoolEntry(handle(Smi::FromInt(delta), isolate()));
    if (FitsInIdx8Operand(entry)) {
      jump_bytecode = GetJumpWithConstantOperand(jump_bytecode);
      *jump_location++ = Bytecodes::ToByte(jump_bytecode);
      *jump_location = static_cast<uint8_t>(entry);
    } else {
      // TODO(oth): OutputJump should reserve a constant pool entry
      // when jump is written. The reservation should be used here if
      // needed, or cancelled if not. This is due to the patch needing
      // to match the size of the code it's replacing. In future,
      // there will probably be a jump with 32-bit operand for cases
      // when constant pool is full, but that needs to be emitted in
      // OutputJump too.
      UNIMPLEMENTED();
    }
  }
}


// static
Bytecode BytecodeArrayBuilder::GetJumpWithToBoolean(Bytecode jump_bytecode) {
  switch (jump_bytecode) {
    case Bytecode::kJump:
    case Bytecode::kJumpIfNull:
    case Bytecode::kJumpIfUndefined:
      return jump_bytecode;
    case Bytecode::kJumpIfTrue:
      return Bytecode::kJumpIfToBooleanTrue;
    case Bytecode::kJumpIfFalse:
      return Bytecode::kJumpIfToBooleanFalse;
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


BytecodeArrayBuilder& BytecodeArrayBuilder::OutputJump(Bytecode jump_bytecode,
                                                       BytecodeLabel* label) {
  // Don't emit dead code.
  if (exit_seen_in_block_) return *this;

  // Check if the value in accumulator is boolean, if not choose an
  // appropriate JumpIfToBoolean bytecode.
  if (NeedToBooleanCast()) {
    jump_bytecode = GetJumpWithToBoolean(jump_bytecode);
  }

  int delta;
  if (label->is_bound()) {
    // Label has been bound already so this is a backwards jump.
    CHECK_GE(bytecodes()->size(), label->offset());
    CHECK_LE(bytecodes()->size(), static_cast<size_t>(kMaxInt));
    size_t abs_delta = bytecodes()->size() - label->offset();
    delta = -static_cast<int>(abs_delta);
  } else {
    // Label has not yet been bound so this is a forward reference
    // that will be patched when the label is bound.
    label->set_referrer(bytecodes()->size());
    delta = 0;
  }

  if (FitsInImm8Operand(delta)) {
    Output(jump_bytecode, static_cast<uint8_t>(delta));
  } else {
    size_t entry = GetConstantPoolEntry(handle(Smi::FromInt(delta), isolate()));
    if (FitsInIdx8Operand(entry)) {
      Output(GetJumpWithConstantOperand(jump_bytecode),
             static_cast<uint8_t>(entry));
    } else {
      UNIMPLEMENTED();
    }
  }
  LeaveBasicBlock();
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Jump(BytecodeLabel* label) {
  return OutputJump(Bytecode::kJump, label);
}


BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfTrue(BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfTrue, label);
}


BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfFalse(BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfFalse, label);
}


BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNull(BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfNull, label);
}


BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfUndefined(
    BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfUndefined, label);
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Throw() {
  Output(Bytecode::kThrow);
  exit_seen_in_block_ = true;
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Return() {
  Output(Bytecode::kReturn);
  exit_seen_in_block_ = true;
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInPrepare(Register receiver) {
  Output(Bytecode::kForInPrepare, receiver.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInNext(Register for_in_state,
                                                      Register index) {
  Output(Bytecode::kForInNext, for_in_state.ToOperand(), index.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInDone(Register for_in_state) {
  Output(Bytecode::kForInDone, for_in_state.ToOperand());
  return *this;
}


void BytecodeArrayBuilder::LeaveBasicBlock() {
  last_block_end_ = bytecodes()->size();
  exit_seen_in_block_ = false;
}


void BytecodeArrayBuilder::EnsureReturn() {
  if (!exit_seen_in_block_) {
    LoadUndefined();
    Return();
  }
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Call(Register callable,
                                                 Register receiver,
                                                 size_t arg_count) {
  if (FitsInIdx8Operand(arg_count)) {
    Output(Bytecode::kCall, callable.ToOperand(), receiver.ToOperand(),
           static_cast<uint8_t>(arg_count));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::New(Register constructor,
                                                Register first_arg,
                                                size_t arg_count) {
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  DCHECK(FitsInIdx8Operand(arg_count));
  Output(Bytecode::kNew, constructor.ToOperand(), first_arg.ToOperand(),
         static_cast<uint8_t>(arg_count));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id, Register first_arg, size_t arg_count) {
  DCHECK(FitsInIdx16Operand(function_id));
  DCHECK(FitsInIdx8Operand(arg_count));
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  Output(Bytecode::kCallRuntime, static_cast<uint16_t>(function_id),
         first_arg.ToOperand(), static_cast<uint8_t>(arg_count));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CallJSRuntime(int context_index,
                                                          Register receiver,
                                                          size_t arg_count) {
  DCHECK(FitsInIdx16Operand(context_index));
  DCHECK(FitsInIdx8Operand(arg_count));
  Output(Bytecode::kCallJSRuntime, static_cast<uint16_t>(context_index),
         receiver.ToOperand(), static_cast<uint8_t>(arg_count));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Delete(Register object,
                                                   LanguageMode language_mode) {
  Output(BytecodeForDelete(language_mode), object.ToOperand());
  return *this;
}


size_t BytecodeArrayBuilder::GetConstantPoolEntry(Handle<Object> object) {
  // These constants shouldn't be added to the constant pool, the should use
  // specialzed bytecodes instead.
  DCHECK(!object.is_identical_to(isolate_->factory()->undefined_value()));
  DCHECK(!object.is_identical_to(isolate_->factory()->null_value()));
  DCHECK(!object.is_identical_to(isolate_->factory()->the_hole_value()));
  DCHECK(!object.is_identical_to(isolate_->factory()->true_value()));
  DCHECK(!object.is_identical_to(isolate_->factory()->false_value()));

  size_t* entry = constants_map_.Find(object);
  if (!entry) {
    entry = constants_map_.Get(object);
    *entry = constants_.size();
    constants_.push_back(object);
  }
  DCHECK(constants_[*entry].is_identical_to(object));
  return *entry;
}


int BytecodeArrayBuilder::BorrowTemporaryRegister() {
  if (free_temporaries_.empty()) {
    temporary_register_count_ += 1;
    return last_temporary_register().index();
  } else {
    auto pos = free_temporaries_.begin();
    int retval = *pos;
    free_temporaries_.erase(pos);
    return retval;
  }
}


void BytecodeArrayBuilder::BorrowConsecutiveTemporaryRegister(int reg_index) {
  DCHECK(free_temporaries_.find(reg_index) != free_temporaries_.end());
  free_temporaries_.erase(reg_index);
}


void BytecodeArrayBuilder::ReturnTemporaryRegister(int reg_index) {
  DCHECK(free_temporaries_.find(reg_index) == free_temporaries_.end());
  free_temporaries_.insert(reg_index);
}


int BytecodeArrayBuilder::PrepareForConsecutiveTemporaryRegisters(
    size_t count) {
  if (count == 0) {
    return -1;
  }

  // Search within existing temporaries for a run.
  auto start = free_temporaries_.begin();
  size_t run_length = 0;
  for (auto run_end = start; run_end != free_temporaries_.end(); run_end++) {
    if (*run_end != *start + static_cast<int>(run_length)) {
      start = run_end;
      run_length = 0;
    }
    if (++run_length == count) {
      return *start;
    }
  }

  // Continue run if possible across existing last temporary.
  if (temporary_register_count_ > 0 &&
      (start == free_temporaries_.end() ||
       *start + static_cast<int>(run_length) !=
           last_temporary_register().index() + 1)) {
    run_length = 0;
  }

  // Ensure enough registers for run.
  while (run_length++ < count) {
    temporary_register_count_++;
    free_temporaries_.insert(last_temporary_register().index());
  }
  return last_temporary_register().index() - static_cast<int>(count) + 1;
}


bool BytecodeArrayBuilder::TemporaryRegisterIsLive(Register reg) const {
  if (temporary_register_count_ > 0) {
    DCHECK(reg.index() >= first_temporary_register().index() &&
           reg.index() <= last_temporary_register().index());
    return free_temporaries_.find(reg.index()) == free_temporaries_.end();
  } else {
    return false;
  }
}


bool BytecodeArrayBuilder::OperandIsValid(Bytecode bytecode, int operand_index,
                                          uint32_t operand_value) const {
  OperandType operand_type = Bytecodes::GetOperandType(bytecode, operand_index);
  switch (operand_type) {
    case OperandType::kNone:
      return false;
    case OperandType::kIdx16:
      return static_cast<uint16_t>(operand_value) == operand_value;
    case OperandType::kCount8:
    case OperandType::kImm8:
    case OperandType::kIdx8:
      return static_cast<uint8_t>(operand_value) == operand_value;
    case OperandType::kMaybeReg8:
      if (operand_value == 0) {
        return true;
      }
    // Fall-through to kReg8 case.
    case OperandType::kReg8: {
      Register reg = Register::FromOperand(static_cast<uint8_t>(operand_value));
      if (reg.is_function_context() || reg.is_function_closure()) {
        return true;
      } else if (reg.is_parameter()) {
        int parameter_index = reg.ToParameterIndex(parameter_count_);
        return parameter_index >= 0 && parameter_index < parameter_count_;
      } else if (reg.index() < fixed_register_count()) {
        return true;
      } else {
        return TemporaryRegisterIsLive(reg);
      }
    }
  }
  UNREACHABLE();
  return false;
}

bool BytecodeArrayBuilder::LastBytecodeInSameBlock() const {
  return last_bytecode_start_ < bytecodes()->size() &&
         last_bytecode_start_ >= last_block_end_;
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForBinaryOperation(Token::Value op) {
  switch (op) {
    case Token::Value::ADD:
      return Bytecode::kAdd;
    case Token::Value::SUB:
      return Bytecode::kSub;
    case Token::Value::MUL:
      return Bytecode::kMul;
    case Token::Value::DIV:
      return Bytecode::kDiv;
    case Token::Value::MOD:
      return Bytecode::kMod;
    case Token::Value::BIT_OR:
      return Bytecode::kBitwiseOr;
    case Token::Value::BIT_XOR:
      return Bytecode::kBitwiseXor;
    case Token::Value::BIT_AND:
      return Bytecode::kBitwiseAnd;
    case Token::Value::SHL:
      return Bytecode::kShiftLeft;
    case Token::Value::SAR:
      return Bytecode::kShiftRight;
    case Token::Value::SHR:
      return Bytecode::kShiftRightLogical;
    default:
      UNREACHABLE();
      return static_cast<Bytecode>(-1);
  }
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForCountOperation(Token::Value op) {
  switch (op) {
    case Token::Value::ADD:
      return Bytecode::kInc;
    case Token::Value::SUB:
      return Bytecode::kDec;
    default:
      UNREACHABLE();
      return static_cast<Bytecode>(-1);
  }
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForCompareOperation(Token::Value op) {
  switch (op) {
    case Token::Value::EQ:
      return Bytecode::kTestEqual;
    case Token::Value::NE:
      return Bytecode::kTestNotEqual;
    case Token::Value::EQ_STRICT:
      return Bytecode::kTestEqualStrict;
    case Token::Value::NE_STRICT:
      return Bytecode::kTestNotEqualStrict;
    case Token::Value::LT:
      return Bytecode::kTestLessThan;
    case Token::Value::GT:
      return Bytecode::kTestGreaterThan;
    case Token::Value::LTE:
      return Bytecode::kTestLessThanOrEqual;
    case Token::Value::GTE:
      return Bytecode::kTestGreaterThanOrEqual;
    case Token::Value::INSTANCEOF:
      return Bytecode::kTestInstanceOf;
    case Token::Value::IN:
      return Bytecode::kTestIn;
    default:
      UNREACHABLE();
      return static_cast<Bytecode>(-1);
  }
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForWideOperands(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kLoadICSloppy:
      return Bytecode::kLoadICSloppyWide;
    case Bytecode::kLoadICStrict:
      return Bytecode::kLoadICStrictWide;
    case Bytecode::kKeyedLoadICSloppy:
      return Bytecode::kKeyedLoadICSloppyWide;
    case Bytecode::kKeyedLoadICStrict:
      return Bytecode::kKeyedLoadICStrictWide;
    case Bytecode::kStoreICSloppy:
      return Bytecode::kStoreICSloppyWide;
    case Bytecode::kStoreICStrict:
      return Bytecode::kStoreICStrictWide;
    case Bytecode::kKeyedStoreICSloppy:
      return Bytecode::kKeyedStoreICSloppyWide;
    case Bytecode::kKeyedStoreICStrict:
      return Bytecode::kKeyedStoreICStrictWide;
    case Bytecode::kLdaGlobalSloppy:
      return Bytecode::kLdaGlobalSloppyWide;
    case Bytecode::kLdaGlobalStrict:
      return Bytecode::kLdaGlobalStrictWide;
    case Bytecode::kLdaGlobalInsideTypeofSloppy:
      return Bytecode::kLdaGlobalInsideTypeofSloppyWide;
    case Bytecode::kLdaGlobalInsideTypeofStrict:
      return Bytecode::kLdaGlobalInsideTypeofStrictWide;
    case Bytecode::kStaGlobalSloppy:
      return Bytecode::kStaGlobalSloppyWide;
    case Bytecode::kStaGlobalStrict:
      return Bytecode::kStaGlobalStrictWide;
    default:
      UNREACHABLE();
      return static_cast<Bytecode>(-1);
  }
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForLoadIC(LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kLoadICSloppy;
    case STRICT:
      return Bytecode::kLoadICStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForKeyedLoadIC(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kKeyedLoadICSloppy;
    case STRICT:
      return Bytecode::kKeyedLoadICStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForStoreIC(LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStoreICSloppy;
    case STRICT:
      return Bytecode::kStoreICStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForKeyedStoreIC(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kKeyedStoreICSloppy;
    case STRICT:
      return Bytecode::kKeyedStoreICStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForLoadGlobal(LanguageMode language_mode,
                                                     TypeofMode typeof_mode) {
  switch (language_mode) {
    case SLOPPY:
      return typeof_mode == INSIDE_TYPEOF
                 ? Bytecode::kLdaGlobalInsideTypeofSloppy
                 : Bytecode::kLdaGlobalSloppy;
    case STRICT:
      return typeof_mode == INSIDE_TYPEOF
                 ? Bytecode::kLdaGlobalInsideTypeofStrict
                 : Bytecode::kLdaGlobalStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForStoreGlobal(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStaGlobalSloppy;
    case STRICT:
      return Bytecode::kStaGlobalStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForCreateArguments(
    CreateArgumentsType type) {
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      return Bytecode::kCreateMappedArguments;
    case CreateArgumentsType::kUnmappedArguments:
      return Bytecode::kCreateUnmappedArguments;
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForDelete(LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kDeletePropertySloppy;
    case STRICT:
      return Bytecode::kDeletePropertyStrict;
    case STRONG:
      UNIMPLEMENTED();
    default:
      UNREACHABLE();
  }
  return static_cast<Bytecode>(-1);
}


// static
bool BytecodeArrayBuilder::FitsInIdx8Operand(int value) {
  return kMinUInt8 <= value && value <= kMaxUInt8;
}


// static
bool BytecodeArrayBuilder::FitsInIdx8Operand(size_t value) {
  return value <= static_cast<size_t>(kMaxUInt8);
}


// static
bool BytecodeArrayBuilder::FitsInImm8Operand(int value) {
  return kMinInt8 <= value && value < kMaxInt8;
}


// static
bool BytecodeArrayBuilder::FitsInIdx16Operand(int value) {
  return kMinUInt16 <= value && value <= kMaxUInt16;
}


// static
bool BytecodeArrayBuilder::FitsInIdx16Operand(size_t value) {
  return value <= static_cast<size_t>(kMaxUInt16);
}


TemporaryRegisterScope::TemporaryRegisterScope(BytecodeArrayBuilder* builder)
    : builder_(builder),
      allocated_(builder->zone()),
      next_consecutive_register_(-1),
      next_consecutive_count_(-1) {}


TemporaryRegisterScope::~TemporaryRegisterScope() {
  for (auto i = allocated_.rbegin(); i != allocated_.rend(); i++) {
    builder_->ReturnTemporaryRegister(*i);
  }
  allocated_.clear();
}


Register TemporaryRegisterScope::NewRegister() {
  int allocated = builder_->BorrowTemporaryRegister();
  allocated_.push_back(allocated);
  return Register(allocated);
}


bool TemporaryRegisterScope::RegisterIsAllocatedInThisScope(
    Register reg) const {
  for (auto i = allocated_.begin(); i != allocated_.end(); i++) {
    if (*i == reg.index()) return true;
  }
  return false;
}


void TemporaryRegisterScope::PrepareForConsecutiveAllocations(size_t count) {
  if (static_cast<int>(count) > next_consecutive_count_) {
    next_consecutive_register_ =
        builder_->PrepareForConsecutiveTemporaryRegisters(count);
    next_consecutive_count_ = static_cast<int>(count);
  }
}


Register TemporaryRegisterScope::NextConsecutiveRegister() {
  DCHECK_GE(next_consecutive_register_, 0);
  DCHECK_GT(next_consecutive_count_, 0);
  builder_->BorrowConsecutiveTemporaryRegister(next_consecutive_register_);
  allocated_.push_back(next_consecutive_register_);
  next_consecutive_count_--;
  return Register(next_consecutive_register_++);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
