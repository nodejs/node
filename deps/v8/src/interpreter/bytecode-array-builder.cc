// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-builder.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilder::PreviousBytecodeHelper {
 public:
  explicit PreviousBytecodeHelper(const BytecodeArrayBuilder& array_builder)
      : array_builder_(array_builder),
        previous_bytecode_start_(array_builder_.last_bytecode_start_) {
    // This helper is expected to be instantiated only when the last bytecode is
    // in the same basic block.
    DCHECK(array_builder_.LastBytecodeInSameBlock());
  }

  // Returns the previous bytecode in the same basic block.
  MUST_USE_RESULT Bytecode GetBytecode() const {
    DCHECK_EQ(array_builder_.last_bytecode_start_, previous_bytecode_start_);
    return Bytecodes::FromByte(
        array_builder_.bytecodes()->at(previous_bytecode_start_));
  }

  // Returns the operand at operand_index for the previous bytecode in the
  // same basic block.
  MUST_USE_RESULT uint32_t GetOperand(int operand_index) const {
    DCHECK_EQ(array_builder_.last_bytecode_start_, previous_bytecode_start_);
    Bytecode bytecode = GetBytecode();
    DCHECK_GE(operand_index, 0);
    DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode));
    size_t operand_offset =
        previous_bytecode_start_ +
        Bytecodes::GetOperandOffset(bytecode, operand_index);
    OperandSize size = Bytecodes::GetOperandSize(bytecode, operand_index);
    switch (size) {
      default:
      case OperandSize::kNone:
        UNREACHABLE();
      case OperandSize::kByte:
        return static_cast<uint32_t>(
            array_builder_.bytecodes()->at(operand_offset));
      case OperandSize::kShort:
        uint16_t operand =
            (array_builder_.bytecodes()->at(operand_offset) << 8) +
            array_builder_.bytecodes()->at(operand_offset + 1);
        return static_cast<uint32_t>(operand);
    }
  }

  Handle<Object> GetConstantForIndexOperand(int operand_index) const {
    return array_builder_.constant_array_builder()->At(
        GetOperand(operand_index));
  }

 private:
  const BytecodeArrayBuilder& array_builder_;
  size_t previous_bytecode_start_;

  DISALLOW_COPY_AND_ASSIGN(PreviousBytecodeHelper);
};


BytecodeArrayBuilder::BytecodeArrayBuilder(Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      zone_(zone),
      bytecodes_(zone),
      bytecode_generated_(false),
      constant_array_builder_(isolate, zone),
      last_block_end_(0),
      last_bytecode_start_(~0),
      exit_seen_in_block_(false),
      unbound_jumps_(0),
      parameter_count_(-1),
      local_register_count_(-1),
      context_register_count_(-1),
      temporary_register_count_(0),
      free_temporaries_(zone) {}


BytecodeArrayBuilder::~BytecodeArrayBuilder() { DCHECK_EQ(0, unbound_jumps_); }


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
  Handle<FixedArray> constant_pool =
      constant_array_builder()->ToFixedArray(factory);
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
                                  uint32_t operand1, uint32_t operand2,
                                  uint32_t operand3) {
  uint32_t operands[] = {operand0, operand1, operand2, operand3};
  Output(bytecode, operands);
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


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadBooleanConstant(bool value) {
  if (value) {
    LoadTrue();
  } else {
    LoadFalse();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadAccumulatorWithRegister(
    Register reg) {
  if (!IsRegisterInAccumulator(reg)) {
    Output(Bytecode::kLdar, reg.ToOperand());
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreAccumulatorInRegister(
    Register reg) {
  // TODO(oth): Avoid storing the accumulator in the register if the
  // previous bytecode loaded the accumulator with the same register.
  //
  // TODO(oth): If the previous bytecode is a MOV into this register,
  // the previous instruction can be removed. The logic for determining
  // these redundant MOVs appears complex.
  Output(Bytecode::kStar, reg.ToOperand());
  if (!IsRegisterInAccumulator(reg)) {
    Output(Bytecode::kStar, reg.ToOperand());
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::MoveRegister(Register from,
                                                         Register to) {
  DCHECK(from != to);
  Output(Bytecode::kMov, from.ToOperand(), to.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ExchangeRegisters(Register reg0,
                                                              Register reg1) {
  DCHECK(reg0 != reg1);
  if (FitsInReg8Operand(reg0)) {
    Output(Bytecode::kExchange, reg0.ToOperand(), reg1.ToWideOperand());
  } else if (FitsInReg8Operand(reg1)) {
    Output(Bytecode::kExchange, reg1.ToOperand(), reg0.ToWideOperand());
  } else {
    Output(Bytecode::kExchangeWide, reg0.ToWideOperand(), reg1.ToWideOperand());
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadGlobal(
    const Handle<String> name, int feedback_slot, LanguageMode language_mode,
    TypeofMode typeof_mode) {
  // TODO(rmcilroy): Potentially store language and typeof information in an
  // operand rather than having extra bytecodes.
  Bytecode bytecode = BytecodeForLoadGlobal(language_mode, typeof_mode);
  size_t name_index = GetConstantPoolEntry(name);
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
    const Handle<String> name, int feedback_slot, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreGlobal(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
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
  } else if (FitsInIdx16Operand(slot_index)) {
    Output(Bytecode::kLdaContextSlotWide, context.ToOperand(),
           static_cast<uint16_t>(slot_index));
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
  } else if (FitsInIdx16Operand(slot_index)) {
    Output(Bytecode::kStaContextSlotWide, context.ToOperand(),
           static_cast<uint16_t>(slot_index));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupSlot(
    const Handle<String> name, TypeofMode typeof_mode) {
  Bytecode bytecode = (typeof_mode == INSIDE_TYPEOF)
                          ? Bytecode::kLdaLookupSlotInsideTypeof
                          : Bytecode::kLdaLookupSlot;
  size_t name_index = GetConstantPoolEntry(name);
  if (FitsInIdx8Operand(name_index)) {
    Output(bytecode, static_cast<uint8_t>(name_index));
  } else if (FitsInIdx16Operand(name_index)) {
    Output(BytecodeForWideOperands(bytecode),
           static_cast<uint16_t>(name_index));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreLookupSlot(
    const Handle<String> name, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreLookupSlot(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  if (FitsInIdx8Operand(name_index)) {
    Output(bytecode, static_cast<uint8_t>(name_index));
  } else if (FitsInIdx16Operand(name_index)) {
    Output(BytecodeForWideOperands(bytecode),
           static_cast<uint16_t>(name_index));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNamedProperty(
    Register object, const Handle<String> name, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForLoadIC(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
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
    Register object, const Handle<String> name, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreIC(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
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
    Handle<SharedFunctionInfo> shared_info, PretenureFlag tenured) {
  size_t entry = GetConstantPoolEntry(shared_info);
  DCHECK(FitsInImm8Operand(tenured));
  if (FitsInIdx8Operand(entry)) {
    Output(Bytecode::kCreateClosure, static_cast<uint8_t>(entry),
           static_cast<uint8_t>(tenured));
  } else if (FitsInIdx16Operand(entry)) {
    Output(Bytecode::kCreateClosureWide, static_cast<uint16_t>(entry),
           static_cast<uint8_t>(tenured));
  } else {
    UNIMPLEMENTED();
  }
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
    Handle<String> pattern, int literal_index, int flags) {
  DCHECK(FitsInImm8Operand(flags));  // Flags should fit in 8 bits.
  size_t pattern_entry = GetConstantPoolEntry(pattern);
  if (FitsInIdx8Operand(literal_index) && FitsInIdx8Operand(pattern_entry)) {
    Output(Bytecode::kCreateRegExpLiteral, static_cast<uint8_t>(pattern_entry),
           static_cast<uint8_t>(literal_index), static_cast<uint8_t>(flags));
  } else if (FitsInIdx16Operand(literal_index) &&
             FitsInIdx16Operand(pattern_entry)) {
    Output(Bytecode::kCreateRegExpLiteralWide,
           static_cast<uint16_t>(pattern_entry),
           static_cast<uint16_t>(literal_index), static_cast<uint8_t>(flags));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArrayLiteral(
    Handle<FixedArray> constant_elements, int literal_index, int flags) {
  DCHECK(FitsInImm8Operand(flags));  // Flags should fit in 8 bits.
  size_t constant_elements_entry = GetConstantPoolEntry(constant_elements);
  if (FitsInIdx8Operand(literal_index) &&
      FitsInIdx8Operand(constant_elements_entry)) {
    Output(Bytecode::kCreateArrayLiteral,
           static_cast<uint8_t>(constant_elements_entry),
           static_cast<uint8_t>(literal_index), static_cast<uint8_t>(flags));
  } else if (FitsInIdx16Operand(literal_index) &&
             FitsInIdx16Operand(constant_elements_entry)) {
    Output(Bytecode::kCreateArrayLiteralWide,
           static_cast<uint16_t>(constant_elements_entry),
           static_cast<uint16_t>(literal_index), static_cast<uint8_t>(flags));
  } else {
    UNIMPLEMENTED();
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateObjectLiteral(
    Handle<FixedArray> constant_properties, int literal_index, int flags) {
  DCHECK(FitsInImm8Operand(flags));  // Flags should fit in 8 bits.
  size_t constant_properties_entry = GetConstantPoolEntry(constant_properties);
  if (FitsInIdx8Operand(literal_index) &&
      FitsInIdx8Operand(constant_properties_entry)) {
    Output(Bytecode::kCreateObjectLiteral,
           static_cast<uint8_t>(constant_properties_entry),
           static_cast<uint8_t>(literal_index), static_cast<uint8_t>(flags));
  } else if (FitsInIdx16Operand(literal_index) &&
             FitsInIdx16Operand(constant_properties_entry)) {
    Output(Bytecode::kCreateObjectLiteralWide,
           static_cast<uint16_t>(constant_properties_entry),
           static_cast<uint16_t>(literal_index), static_cast<uint8_t>(flags));
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
    return true;
  }
  PreviousBytecodeHelper previous_bytecode(*this);
  switch (previous_bytecode.GetBytecode()) {
    // If the previous bytecode puts a boolean in the accumulator return true.
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


BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToJSObject() {
  Output(Bytecode::kToObject);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToName() {
  if (LastBytecodeInSameBlock()) {
    PreviousBytecodeHelper previous_bytecode(*this);
    switch (previous_bytecode.GetBytecode()) {
      case Bytecode::kToName:
      case Bytecode::kTypeOf:
        return *this;
      case Bytecode::kLdaConstantWide:
      case Bytecode::kLdaConstant: {
        Handle<Object> object = previous_bytecode.GetConstantForIndexOperand(0);
        if (object->IsName()) return *this;
        break;
      }
      default:
        break;
    }
  }
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
      return static_cast<Bytecode>(-1);
  }
}


// static
Bytecode BytecodeArrayBuilder::GetJumpWithConstantWideOperand(
    Bytecode jump_bytecode) {
  switch (jump_bytecode) {
    case Bytecode::kJump:
      return Bytecode::kJumpConstantWide;
    case Bytecode::kJumpIfTrue:
      return Bytecode::kJumpIfTrueConstantWide;
    case Bytecode::kJumpIfFalse:
      return Bytecode::kJumpIfFalseConstantWide;
    case Bytecode::kJumpIfToBooleanTrue:
      return Bytecode::kJumpIfToBooleanTrueConstantWide;
    case Bytecode::kJumpIfToBooleanFalse:
      return Bytecode::kJumpIfToBooleanFalseConstantWide;
    case Bytecode::kJumpIfNull:
      return Bytecode::kJumpIfNullConstantWide;
    case Bytecode::kJumpIfUndefined:
      return Bytecode::kJumpIfUndefinedConstantWide;
    default:
      UNREACHABLE();
      return static_cast<Bytecode>(-1);
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


void BytecodeArrayBuilder::PatchIndirectJumpWith8BitOperand(
    const ZoneVector<uint8_t>::iterator& jump_location, int delta) {
  Bytecode jump_bytecode = Bytecodes::FromByte(*jump_location);
  DCHECK(Bytecodes::IsJumpImmediate(jump_bytecode));
  ZoneVector<uint8_t>::iterator operand_location = jump_location + 1;
  DCHECK_EQ(*operand_location, 0);
  if (FitsInImm8Operand(delta)) {
    // The jump fits within the range of an Imm8 operand, so cancel
    // the reservation and jump directly.
    constant_array_builder()->DiscardReservedEntry(OperandSize::kByte);
    *operand_location = static_cast<uint8_t>(delta);
  } else {
    // The jump does not fit within the range of an Imm8 operand, so
    // commit reservation putting the offset into the constant pool,
    // and update the jump instruction and operand.
    size_t entry = constant_array_builder()->CommitReservedEntry(
        OperandSize::kByte, handle(Smi::FromInt(delta), isolate()));
    DCHECK(FitsInIdx8Operand(entry));
    jump_bytecode = GetJumpWithConstantOperand(jump_bytecode);
    *jump_location = Bytecodes::ToByte(jump_bytecode);
    *operand_location = static_cast<uint8_t>(entry);
  }
}


void BytecodeArrayBuilder::PatchIndirectJumpWith16BitOperand(
    const ZoneVector<uint8_t>::iterator& jump_location, int delta) {
  DCHECK(Bytecodes::IsJumpConstantWide(Bytecodes::FromByte(*jump_location)));
  ZoneVector<uint8_t>::iterator operand_location = jump_location + 1;
  size_t entry = constant_array_builder()->CommitReservedEntry(
      OperandSize::kShort, handle(Smi::FromInt(delta), isolate()));
  DCHECK(FitsInIdx16Operand(entry));
  uint8_t operand_bytes[2];
  WriteUnalignedUInt16(operand_bytes, static_cast<uint16_t>(entry));
  DCHECK(*operand_location == 0 && *(operand_location + 1) == 0);
  *operand_location++ = operand_bytes[0];
  *operand_location = operand_bytes[1];
}


void BytecodeArrayBuilder::PatchJump(
    const ZoneVector<uint8_t>::iterator& jump_target,
    const ZoneVector<uint8_t>::iterator& jump_location) {
  Bytecode jump_bytecode = Bytecodes::FromByte(*jump_location);
  int delta = static_cast<int>(jump_target - jump_location);
  DCHECK(Bytecodes::IsJump(jump_bytecode));
  switch (Bytecodes::GetOperandSize(jump_bytecode, 0)) {
    case OperandSize::kByte:
      PatchIndirectJumpWith8BitOperand(jump_location, delta);
      break;
    case OperandSize::kShort:
      PatchIndirectJumpWith16BitOperand(jump_location, delta);
      break;
    case OperandSize::kNone:
      UNREACHABLE();
  }
  unbound_jumps_--;
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

  if (label->is_bound()) {
    // Label has been bound already so this is a backwards jump.
    CHECK_GE(bytecodes()->size(), label->offset());
    CHECK_LE(bytecodes()->size(), static_cast<size_t>(kMaxInt));
    size_t abs_delta = bytecodes()->size() - label->offset();
    int delta = -static_cast<int>(abs_delta);

    if (FitsInImm8Operand(delta)) {
      Output(jump_bytecode, static_cast<uint8_t>(delta));
    } else {
      size_t entry =
          GetConstantPoolEntry(handle(Smi::FromInt(delta), isolate()));
      if (FitsInIdx8Operand(entry)) {
        Output(GetJumpWithConstantOperand(jump_bytecode),
               static_cast<uint8_t>(entry));
      } else if (FitsInIdx16Operand(entry)) {
        Output(GetJumpWithConstantWideOperand(jump_bytecode),
               static_cast<uint16_t>(entry));
      } else {
        UNREACHABLE();
      }
    }
  } else {
    // The label has not yet been bound so this is a forward reference
    // that will be patched when the label is bound. We create a
    // reservation in the constant pool so the jump can be patched
    // when the label is bound. The reservation means the maximum size
    // of the operand for the constant is known and the jump can
    // be emitted into the bytecode stream with space for the operand.
    label->set_referrer(bytecodes()->size());
    unbound_jumps_++;
    OperandSize reserved_operand_size =
        constant_array_builder()->CreateReservedEntry();
    switch (reserved_operand_size) {
      case OperandSize::kByte:
        Output(jump_bytecode, 0);
        break;
      case OperandSize::kShort:
        Output(GetJumpWithConstantWideOperand(jump_bytecode), 0);
        break;
      case OperandSize::kNone:
        UNREACHABLE();
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


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInPrepare(
    Register cache_type, Register cache_array, Register cache_length) {
  Output(Bytecode::kForInPrepare, cache_type.ToOperand(),
         cache_array.ToOperand(), cache_length.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInDone(Register index,
                                                      Register cache_length) {
  Output(Bytecode::kForInDone, index.ToOperand(), cache_length.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInNext(Register receiver,
                                                      Register cache_type,
                                                      Register cache_array,
                                                      Register index) {
  Output(Bytecode::kForInNext, receiver.ToOperand(), cache_type.ToOperand(),
         cache_array.ToOperand(), index.ToOperand());
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInStep(Register index) {
  Output(Bytecode::kForInStep, index.ToOperand());
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
                                                 size_t arg_count,
                                                 int feedback_slot) {
  if (FitsInIdx8Operand(arg_count) && FitsInIdx8Operand(feedback_slot)) {
    Output(Bytecode::kCall, callable.ToOperand(), receiver.ToOperand(),
           static_cast<uint8_t>(arg_count),
           static_cast<uint8_t>(feedback_slot));
  } else if (FitsInIdx16Operand(arg_count) &&
             FitsInIdx16Operand(feedback_slot)) {
    Output(Bytecode::kCallWide, callable.ToOperand(), receiver.ToOperand(),
           static_cast<uint16_t>(arg_count),
           static_cast<uint16_t>(feedback_slot));
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
  DCHECK_EQ(1, Runtime::FunctionForId(function_id)->result_size);
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


BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntimeForPair(
    Runtime::FunctionId function_id, Register first_arg, size_t arg_count,
    Register first_return) {
  DCHECK_EQ(2, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(FitsInIdx16Operand(function_id));
  DCHECK(FitsInIdx8Operand(arg_count));
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  Output(Bytecode::kCallRuntimeForPair, static_cast<uint16_t>(function_id),
         first_arg.ToOperand(), static_cast<uint8_t>(arg_count),
         first_return.ToOperand());
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


BytecodeArrayBuilder& BytecodeArrayBuilder::DeleteLookupSlot() {
  Output(Bytecode::kDeleteLookupSlot);
  return *this;
}


size_t BytecodeArrayBuilder::GetConstantPoolEntry(Handle<Object> object) {
  return constant_array_builder()->Insert(object);
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


int BytecodeArrayBuilder::BorrowTemporaryRegisterNotInRange(int start_index,
                                                            int end_index) {
  auto index = free_temporaries_.lower_bound(start_index);
  if (index == free_temporaries_.begin()) {
    // If start_index is the first free register, check for a register
    // greater than end_index.
    index = free_temporaries_.upper_bound(end_index);
    if (index == free_temporaries_.end()) {
      temporary_register_count_ += 1;
      return last_temporary_register().index();
    }
  } else {
    // If there is a free register < start_index
    index--;
  }

  int retval = *index;
  free_temporaries_.erase(index);
  return retval;
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


bool BytecodeArrayBuilder::RegisterIsValid(Register reg) const {
  if (reg.is_function_context() || reg.is_function_closure() ||
      reg.is_new_target()) {
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


bool BytecodeArrayBuilder::OperandIsValid(Bytecode bytecode, int operand_index,
                                          uint32_t operand_value) const {
  OperandType operand_type = Bytecodes::GetOperandType(bytecode, operand_index);
  switch (operand_type) {
    case OperandType::kNone:
      return false;
    case OperandType::kCount16:
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
    case OperandType::kReg8:
      return RegisterIsValid(
          Register::FromOperand(static_cast<uint8_t>(operand_value)));
    case OperandType::kRegPair8: {
      Register reg0 =
          Register::FromOperand(static_cast<uint8_t>(operand_value));
      Register reg1 = Register(reg0.index() + 1);
      return RegisterIsValid(reg0) && RegisterIsValid(reg1);
    }
    case OperandType::kReg16:
      if (bytecode != Bytecode::kExchange &&
          bytecode != Bytecode::kExchangeWide) {
        return false;
      }
      return RegisterIsValid(
          Register::FromWideOperand(static_cast<uint16_t>(operand_value)));
  }
  UNREACHABLE();
  return false;
}


bool BytecodeArrayBuilder::LastBytecodeInSameBlock() const {
  return last_bytecode_start_ < bytecodes()->size() &&
         last_bytecode_start_ >= last_block_end_;
}


bool BytecodeArrayBuilder::IsRegisterInAccumulator(Register reg) {
  if (LastBytecodeInSameBlock()) {
    PreviousBytecodeHelper previous_bytecode(*this);
    Bytecode bytecode = previous_bytecode.GetBytecode();
    if ((bytecode == Bytecode::kLdar || bytecode == Bytecode::kStar) &&
        (reg == Register::FromOperand(previous_bytecode.GetOperand(0)))) {
      return true;
    }
  }
  return false;
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
    case Bytecode::kLdaLookupSlot:
      return Bytecode::kLdaLookupSlotWide;
    case Bytecode::kLdaLookupSlotInsideTypeof:
      return Bytecode::kLdaLookupSlotInsideTypeofWide;
    case Bytecode::kStaLookupSlotStrict:
      return Bytecode::kStaLookupSlotStrictWide;
    case Bytecode::kStaLookupSlotSloppy:
      return Bytecode::kStaLookupSlotSloppyWide;
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
Bytecode BytecodeArrayBuilder::BytecodeForStoreLookupSlot(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStaLookupSlotSloppy;
    case STRICT:
      return Bytecode::kStaLookupSlotStrict;
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
  return kMinInt8 <= value && value <= kMaxInt8;
}


// static
bool BytecodeArrayBuilder::FitsInIdx16Operand(int value) {
  return kMinUInt16 <= value && value <= kMaxUInt16;
}


// static
bool BytecodeArrayBuilder::FitsInIdx16Operand(size_t value) {
  return value <= static_cast<size_t>(kMaxUInt16);
}


// static
bool BytecodeArrayBuilder::FitsInReg8Operand(Register value) {
  return kMinInt8 <= value.index() && value.index() <= kMaxInt8;
}


// static
bool BytecodeArrayBuilder::FitsInReg16Operand(Register value) {
  return kMinInt16 <= value.index() && value.index() <= kMaxInt16;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
