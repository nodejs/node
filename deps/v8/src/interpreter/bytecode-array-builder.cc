// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-builder.h"
#include "src/compiler.h"
#include "src/interpreter/interpreter-intrinsics.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilder::PreviousBytecodeHelper BASE_EMBEDDED {
 public:
  explicit PreviousBytecodeHelper(const BytecodeArrayBuilder& array_builder)
      : array_builder_(array_builder),
        previous_bytecode_start_(array_builder_.last_bytecode_start_) {
    // This helper is expected to be instantiated only when the last bytecode is
    // in the same basic block.
    DCHECK(array_builder_.LastBytecodeInSameBlock());
    bytecode_ = Bytecodes::FromByte(
        array_builder_.bytecodes()->at(previous_bytecode_start_));
    operand_scale_ = OperandScale::kSingle;
    if (Bytecodes::IsPrefixScalingBytecode(bytecode_)) {
      operand_scale_ = Bytecodes::PrefixBytecodeToOperandScale(bytecode_);
      bytecode_ = Bytecodes::FromByte(
          array_builder_.bytecodes()->at(previous_bytecode_start_ + 1));
    }
  }

  // Returns the previous bytecode in the same basic block.
  MUST_USE_RESULT Bytecode GetBytecode() const {
    DCHECK_EQ(array_builder_.last_bytecode_start_, previous_bytecode_start_);
    return bytecode_;
  }

  MUST_USE_RESULT Register GetRegisterOperand(int operand_index) const {
    return Register::FromOperand(GetSignedOperand(operand_index));
  }

  MUST_USE_RESULT uint32_t GetIndexOperand(int operand_index) const {
    return GetUnsignedOperand(operand_index);
  }

  Handle<Object> GetConstantForIndexOperand(int operand_index) const {
    return array_builder_.constant_array_builder()->At(
        GetIndexOperand(operand_index));
  }

 private:
  // Returns the signed operand at operand_index for the previous
  // bytecode in the same basic block.
  MUST_USE_RESULT int32_t GetSignedOperand(int operand_index) const {
    DCHECK_EQ(array_builder_.last_bytecode_start_, previous_bytecode_start_);
    OperandType operand_type =
        Bytecodes::GetOperandType(bytecode_, operand_index);
    DCHECK(!Bytecodes::IsUnsignedOperandType(operand_type));
    const uint8_t* operand_start = GetOperandStart(operand_index);
    return Bytecodes::DecodeSignedOperand(operand_start, operand_type,
                                          operand_scale_);
  }

  // Returns the unsigned operand at operand_index for the previous
  // bytecode in the same basic block.
  MUST_USE_RESULT uint32_t GetUnsignedOperand(int operand_index) const {
    DCHECK_EQ(array_builder_.last_bytecode_start_, previous_bytecode_start_);
    OperandType operand_type =
        Bytecodes::GetOperandType(bytecode_, operand_index);
    DCHECK(Bytecodes::IsUnsignedOperandType(operand_type));
    const uint8_t* operand_start = GetOperandStart(operand_index);
    return Bytecodes::DecodeUnsignedOperand(operand_start, operand_type,
                                            operand_scale_);
  }

  const uint8_t* GetOperandStart(int operand_index) const {
    size_t operand_offset =
        previous_bytecode_start_ + prefix_offset() +
        Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale_);
    return &(*array_builder_.bytecodes())[0] + operand_offset;
  }

  int prefix_offset() const {
    return Bytecodes::OperandScaleRequiresPrefixBytecode(operand_scale_) ? 1
                                                                         : 0;
  }

  const BytecodeArrayBuilder& array_builder_;
  OperandScale operand_scale_;
  Bytecode bytecode_;
  size_t previous_bytecode_start_;

  DISALLOW_COPY_AND_ASSIGN(PreviousBytecodeHelper);
};

BytecodeArrayBuilder::BytecodeArrayBuilder(Isolate* isolate, Zone* zone,
                                           int parameter_count,
                                           int context_count, int locals_count,
                                           FunctionLiteral* literal)
    : isolate_(isolate),
      zone_(zone),
      bytecodes_(zone),
      bytecode_generated_(false),
      constant_array_builder_(isolate, zone),
      handler_table_builder_(isolate, zone),
      source_position_table_builder_(isolate, zone),
      last_block_end_(0),
      last_bytecode_start_(~0),
      exit_seen_in_block_(false),
      unbound_jumps_(0),
      parameter_count_(parameter_count),
      local_register_count_(locals_count),
      context_register_count_(context_count),
      temporary_allocator_(zone, fixed_register_count()) {
  DCHECK_GE(parameter_count_, 0);
  DCHECK_GE(context_register_count_, 0);
  DCHECK_GE(local_register_count_, 0);
  return_position_ =
      literal ? std::max(literal->start_position(), literal->end_position() - 1)
              : RelocInfo::kNoPosition;
  LOG_CODE_EVENT(isolate_, CodeStartLinePosInfoRecordEvent(
                               source_position_table_builder()));
}

BytecodeArrayBuilder::~BytecodeArrayBuilder() { DCHECK_EQ(0, unbound_jumps_); }

Register BytecodeArrayBuilder::first_context_register() const {
  DCHECK_GT(context_register_count_, 0);
  return Register(local_register_count_);
}


Register BytecodeArrayBuilder::last_context_register() const {
  DCHECK_GT(context_register_count_, 0);
  return Register(local_register_count_ + context_register_count_ - 1);
}


Register BytecodeArrayBuilder::Parameter(int parameter_index) const {
  DCHECK_GE(parameter_index, 0);
  return Register::FromParameterIndex(parameter_index, parameter_count());
}


bool BytecodeArrayBuilder::RegisterIsParameterOrLocal(Register reg) const {
  return reg.is_parameter() || reg.index() < locals_count();
}


Handle<BytecodeArray> BytecodeArrayBuilder::ToBytecodeArray() {
  DCHECK_EQ(bytecode_generated_, false);
  DCHECK(exit_seen_in_block_);

  int bytecode_size = static_cast<int>(bytecodes_.size());
  int register_count = fixed_and_temporary_register_count();
  int frame_size = register_count * kPointerSize;
  Handle<FixedArray> constant_pool = constant_array_builder()->ToFixedArray();
  Handle<FixedArray> handler_table = handler_table_builder()->ToHandlerTable();
  Handle<ByteArray> source_position_table =
      source_position_table_builder()->ToSourcePositionTable();
  Handle<BytecodeArray> bytecode_array = isolate_->factory()->NewBytecodeArray(
      bytecode_size, &bytecodes_.front(), frame_size, parameter_count(),
      constant_pool);
  bytecode_array->set_handler_table(*handler_table);
  bytecode_array->set_source_position_table(*source_position_table);

  void* line_info = source_position_table_builder()->DetachJITHandlerData();
  LOG_CODE_EVENT(isolate_, CodeEndLinePosInfoRecordEvent(
                               AbstractCode::cast(*bytecode_array), line_info));

  bytecode_generated_ = true;
  return bytecode_array;
}

template <size_t N>
void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t (&operands)[N],
                                  OperandScale operand_scale) {
  // Don't output dead code.
  if (exit_seen_in_block_) return;

  int operand_count = static_cast<int>(N);
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), operand_count);

  last_bytecode_start_ = bytecodes()->size();
  // Emit prefix bytecode for scale if required.
  if (Bytecodes::OperandScaleRequiresPrefixBytecode(operand_scale)) {
    bytecodes()->push_back(Bytecodes::ToByte(
        Bytecodes::OperandScaleToPrefixBytecode(operand_scale)));
  }

  // Emit bytecode.
  bytecodes()->push_back(Bytecodes::ToByte(bytecode));

  // Emit operands.
  for (int i = 0; i < operand_count; i++) {
    DCHECK(OperandIsValid(bytecode, operand_scale, i, operands[i]));
    switch (Bytecodes::GetOperandSize(bytecode, i, operand_scale)) {
      case OperandSize::kNone:
        UNREACHABLE();
        break;
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
      case OperandSize::kQuad: {
        uint8_t operand_bytes[4];
        WriteUnalignedUInt32(operand_bytes, operands[i]);
        bytecodes()->insert(bytecodes()->end(), operand_bytes,
                            operand_bytes + 4);
        break;
      }
    }
  }
}

void BytecodeArrayBuilder::Output(Bytecode bytecode) {
  // Don't output dead code.
  if (exit_seen_in_block_) return;

  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 0);
  last_bytecode_start_ = bytecodes()->size();
  bytecodes()->push_back(Bytecodes::ToByte(bytecode));
}

void BytecodeArrayBuilder::OutputScaled(Bytecode bytecode,
                                        OperandScale operand_scale,
                                        uint32_t operand0, uint32_t operand1,
                                        uint32_t operand2, uint32_t operand3) {
  uint32_t operands[] = {operand0, operand1, operand2, operand3};
  Output(bytecode, operands, operand_scale);
}

void BytecodeArrayBuilder::OutputScaled(Bytecode bytecode,
                                        OperandScale operand_scale,
                                        uint32_t operand0, uint32_t operand1,
                                        uint32_t operand2) {
  uint32_t operands[] = {operand0, operand1, operand2};
  Output(bytecode, operands, operand_scale);
}

void BytecodeArrayBuilder::OutputScaled(Bytecode bytecode,
                                        OperandScale operand_scale,
                                        uint32_t operand0, uint32_t operand1) {
  uint32_t operands[] = {operand0, operand1};
  Output(bytecode, operands, operand_scale);
}

void BytecodeArrayBuilder::OutputScaled(Bytecode bytecode,
                                        OperandScale operand_scale,
                                        uint32_t operand0) {
  uint32_t operands[] = {operand0};
  Output(bytecode, operands, operand_scale);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::BinaryOperation(Token::Value op,
                                                            Register reg) {
  OperandScale operand_scale = OperandSizesToScale(SizeForRegisterOperand(reg));
  OutputScaled(BytecodeForBinaryOperation(op), operand_scale,
               RegisterOperand(reg));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CountOperation(Token::Value op) {
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

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareOperation(Token::Value op,
                                                             Register reg) {
  OperandScale operand_scale = OperandSizesToScale(SizeForRegisterOperand(reg));
  OutputScaled(BytecodeForCompareOperation(op), operand_scale,
               RegisterOperand(reg));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(
    v8::internal::Smi* smi) {
  int32_t raw_smi = smi->value();
  if (raw_smi == 0) {
    Output(Bytecode::kLdaZero);
  } else {
    OperandSize operand_size = SizeForSignedOperand(raw_smi);
    OperandScale operand_scale = OperandSizesToScale(operand_size);
    OutputScaled(Bytecode::kLdaSmi, operand_scale,
                 SignedOperand(raw_smi, operand_size));
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(Handle<Object> object) {
  size_t entry = GetConstantPoolEntry(object);
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(entry));
  OutputScaled(Bytecode::kLdaConstant, operand_scale, UnsignedOperand(entry));
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
  if (!IsRegisterInAccumulator(reg)) {
    OperandScale operand_scale =
        OperandSizesToScale(SizeForRegisterOperand(reg));
    OutputScaled(Bytecode::kLdar, operand_scale, RegisterOperand(reg));
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreAccumulatorInRegister(
    Register reg) {
  if (!IsRegisterInAccumulator(reg)) {
    OperandScale operand_scale =
        OperandSizesToScale(SizeForRegisterOperand(reg));
    OutputScaled(Bytecode::kStar, operand_scale, RegisterOperand(reg));
  }
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::MoveRegister(Register from,
                                                         Register to) {
  DCHECK(from != to);
  OperandScale operand_scale = OperandSizesToScale(SizeForRegisterOperand(from),
                                                   SizeForRegisterOperand(to));
  OutputScaled(Bytecode::kMov, operand_scale, RegisterOperand(from),
               RegisterOperand(to));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadGlobal(
    const Handle<String> name, int feedback_slot, TypeofMode typeof_mode) {
  // TODO(rmcilroy): Potentially store typeof information in an
  // operand rather than having extra bytecodes.
  Bytecode bytecode = BytecodeForLoadGlobal(typeof_mode);
  size_t name_index = GetConstantPoolEntry(name);
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(name_index),
                          SizeForUnsignedOperand(feedback_slot));
  OutputScaled(bytecode, operand_scale, UnsignedOperand(name_index),
               UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreGlobal(
    const Handle<String> name, int feedback_slot, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreGlobal(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(name_index),
                          SizeForUnsignedOperand(feedback_slot));
  OutputScaled(bytecode, operand_scale, UnsignedOperand(name_index),
               UnsignedOperand(feedback_slot));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::LoadContextSlot(Register context,
                                                            int slot_index) {
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(context), SizeForUnsignedOperand(slot_index));
  OutputScaled(Bytecode::kLdaContextSlot, operand_scale,
               RegisterOperand(context), UnsignedOperand(slot_index));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreContextSlot(Register context,
                                                             int slot_index) {
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(context), SizeForUnsignedOperand(slot_index));
  OutputScaled(Bytecode::kStaContextSlot, operand_scale,
               RegisterOperand(context), UnsignedOperand(slot_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupSlot(
    const Handle<String> name, TypeofMode typeof_mode) {
  Bytecode bytecode = (typeof_mode == INSIDE_TYPEOF)
                          ? Bytecode::kLdaLookupSlotInsideTypeof
                          : Bytecode::kLdaLookupSlot;
  size_t name_index = GetConstantPoolEntry(name);
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(name_index));
  OutputScaled(bytecode, operand_scale, UnsignedOperand(name_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreLookupSlot(
    const Handle<String> name, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreLookupSlot(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(name_index));
  OutputScaled(bytecode, operand_scale, UnsignedOperand(name_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNamedProperty(
    Register object, const Handle<Name> name, int feedback_slot) {
  size_t name_index = GetConstantPoolEntry(name);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(object), SizeForUnsignedOperand(name_index),
      SizeForUnsignedOperand(feedback_slot));
  OutputScaled(Bytecode::kLoadIC, operand_scale, RegisterOperand(object),
               UnsignedOperand(name_index), UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadKeyedProperty(
    Register object, int feedback_slot) {
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(object), SizeForUnsignedOperand(feedback_slot));
  OutputScaled(Bytecode::kKeyedLoadIC, operand_scale, RegisterOperand(object),
               UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedProperty(
    Register object, const Handle<Name> name, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreIC(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(object), SizeForUnsignedOperand(name_index),
      SizeForUnsignedOperand(feedback_slot));
  OutputScaled(bytecode, operand_scale, RegisterOperand(object),
               UnsignedOperand(name_index), UnsignedOperand(feedback_slot));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::StoreKeyedProperty(
    Register object, Register key, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForKeyedStoreIC(language_mode);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(object), SizeForRegisterOperand(key),
      SizeForUnsignedOperand(feedback_slot));
  OutputScaled(bytecode, operand_scale, RegisterOperand(object),
               RegisterOperand(key), UnsignedOperand(feedback_slot));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateClosure(
    Handle<SharedFunctionInfo> shared_info, PretenureFlag tenured) {
  size_t entry = GetConstantPoolEntry(shared_info);
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(entry));
  OutputScaled(Bytecode::kCreateClosure, operand_scale, UnsignedOperand(entry),
               UnsignedOperand(static_cast<size_t>(tenured)));
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
  size_t pattern_entry = GetConstantPoolEntry(pattern);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForUnsignedOperand(pattern_entry),
      SizeForUnsignedOperand(literal_index), SizeForUnsignedOperand(flags));
  OutputScaled(Bytecode::kCreateRegExpLiteral, operand_scale,
               UnsignedOperand(pattern_entry), UnsignedOperand(literal_index),
               UnsignedOperand(flags));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArrayLiteral(
    Handle<FixedArray> constant_elements, int literal_index, int flags) {
  size_t constant_elements_entry = GetConstantPoolEntry(constant_elements);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForUnsignedOperand(constant_elements_entry),
      SizeForUnsignedOperand(literal_index), SizeForUnsignedOperand(flags));
  OutputScaled(Bytecode::kCreateArrayLiteral, operand_scale,
               UnsignedOperand(constant_elements_entry),
               UnsignedOperand(literal_index), UnsignedOperand(flags));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CreateObjectLiteral(
    Handle<FixedArray> constant_properties, int literal_index, int flags) {
  size_t constant_properties_entry = GetConstantPoolEntry(constant_properties);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForUnsignedOperand(constant_properties_entry),
      SizeForUnsignedOperand(literal_index), SizeForUnsignedOperand(flags));
  OutputScaled(Bytecode::kCreateObjectLiteral, operand_scale,
               UnsignedOperand(constant_properties_entry),
               UnsignedOperand(literal_index), UnsignedOperand(flags));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::PushContext(Register context) {
  OperandScale operand_scale =
      OperandSizesToScale(SizeForRegisterOperand(context));
  OutputScaled(Bytecode::kPushContext, operand_scale, RegisterOperand(context));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::PopContext(Register context) {
  OperandScale operand_scale =
      OperandSizesToScale(SizeForRegisterOperand(context));
  OutputScaled(Bytecode::kPopContext, operand_scale, RegisterOperand(context));
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
  if (label->is_forward_target()) {
    // An earlier jump instruction refers to this label. Update it's location.
    PatchJump(bytecodes()->begin() + target.offset(),
              bytecodes()->begin() + label->offset());
    // Now treat as if the label will only be back referred to.
  }
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
    case Bytecode::kJumpIfNotHole:
      return Bytecode::kJumpIfNotHoleConstant;
    case Bytecode::kJumpIfNull:
      return Bytecode::kJumpIfNullConstant;
    case Bytecode::kJumpIfUndefined:
      return Bytecode::kJumpIfUndefinedConstant;
    default:
      UNREACHABLE();
      return Bytecode::kIllegal;
  }
}

// static
Bytecode BytecodeArrayBuilder::GetJumpWithToBoolean(Bytecode jump_bytecode) {
  switch (jump_bytecode) {
    case Bytecode::kJump:
    case Bytecode::kJumpIfNull:
    case Bytecode::kJumpIfUndefined:
    case Bytecode::kJumpIfNotHole:
      return jump_bytecode;
    case Bytecode::kJumpIfTrue:
      return Bytecode::kJumpIfToBooleanTrue;
    case Bytecode::kJumpIfFalse:
      return Bytecode::kJumpIfToBooleanFalse;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}


void BytecodeArrayBuilder::PatchIndirectJumpWith8BitOperand(
    const ZoneVector<uint8_t>::iterator& jump_location, int delta) {
  Bytecode jump_bytecode = Bytecodes::FromByte(*jump_location);
  DCHECK(Bytecodes::IsJumpImmediate(jump_bytecode));
  ZoneVector<uint8_t>::iterator operand_location = jump_location + 1;
  DCHECK_EQ(*operand_location, 0);
  if (SizeForSignedOperand(delta) == OperandSize::kByte) {
    // The jump fits within the range of an Imm operand, so cancel
    // the reservation and jump directly.
    constant_array_builder()->DiscardReservedEntry(OperandSize::kByte);
    *operand_location = static_cast<uint8_t>(delta);
  } else {
    // The jump does not fit within the range of an Imm operand, so
    // commit reservation putting the offset into the constant pool,
    // and update the jump instruction and operand.
    size_t entry = constant_array_builder()->CommitReservedEntry(
        OperandSize::kByte, handle(Smi::FromInt(delta), isolate()));
    DCHECK(SizeForUnsignedOperand(entry) == OperandSize::kByte);
    jump_bytecode = GetJumpWithConstantOperand(jump_bytecode);
    *jump_location = Bytecodes::ToByte(jump_bytecode);
    *operand_location = static_cast<uint8_t>(entry);
  }
}

void BytecodeArrayBuilder::PatchIndirectJumpWith16BitOperand(
    const ZoneVector<uint8_t>::iterator& jump_location, int delta) {
  Bytecode jump_bytecode = Bytecodes::FromByte(*jump_location);
  DCHECK(Bytecodes::IsJumpImmediate(jump_bytecode));
  ZoneVector<uint8_t>::iterator operand_location = jump_location + 1;
  uint8_t operand_bytes[2];
  if (SizeForSignedOperand(delta) <= OperandSize::kShort) {
    constant_array_builder()->DiscardReservedEntry(OperandSize::kShort);
    WriteUnalignedUInt16(operand_bytes, static_cast<uint16_t>(delta));
  } else {
    jump_bytecode = GetJumpWithConstantOperand(jump_bytecode);
    *jump_location = Bytecodes::ToByte(jump_bytecode);
    size_t entry = constant_array_builder()->CommitReservedEntry(
        OperandSize::kShort, handle(Smi::FromInt(delta), isolate()));
    WriteUnalignedUInt16(operand_bytes, static_cast<uint16_t>(entry));
  }
  DCHECK(*operand_location == 0 && *(operand_location + 1) == 0);
  *operand_location++ = operand_bytes[0];
  *operand_location = operand_bytes[1];
}

void BytecodeArrayBuilder::PatchIndirectJumpWith32BitOperand(
    const ZoneVector<uint8_t>::iterator& jump_location, int delta) {
  DCHECK(Bytecodes::IsJumpImmediate(Bytecodes::FromByte(*jump_location)));
  constant_array_builder()->DiscardReservedEntry(OperandSize::kQuad);
  ZoneVector<uint8_t>::iterator operand_location = jump_location + 1;
  uint8_t operand_bytes[4];
  WriteUnalignedUInt32(operand_bytes, static_cast<uint32_t>(delta));
  DCHECK(*operand_location == 0 && *(operand_location + 1) == 0 &&
         *(operand_location + 2) == 0 && *(operand_location + 3) == 0);
  *operand_location++ = operand_bytes[0];
  *operand_location++ = operand_bytes[1];
  *operand_location++ = operand_bytes[2];
  *operand_location = operand_bytes[3];
}

void BytecodeArrayBuilder::PatchJump(
    const ZoneVector<uint8_t>::iterator& jump_target,
    const ZoneVector<uint8_t>::iterator& jump_location) {
  int delta = static_cast<int>(jump_target - jump_location);
  Bytecode jump_bytecode = Bytecodes::FromByte(*jump_location);
  int prefix_offset = 0;
  OperandScale operand_scale = OperandScale::kSingle;
  if (Bytecodes::IsPrefixScalingBytecode(jump_bytecode)) {
    // If a prefix scaling bytecode is emitted the target offset is one
    // less than the case of no prefix scaling bytecode.
    delta -= 1;
    prefix_offset = 1;
    operand_scale = Bytecodes::PrefixBytecodeToOperandScale(jump_bytecode);
    jump_bytecode = Bytecodes::FromByte(*(jump_location + prefix_offset));
  }

  DCHECK(Bytecodes::IsJump(jump_bytecode));
  switch (operand_scale) {
    case OperandScale::kSingle:
      PatchIndirectJumpWith8BitOperand(jump_location, delta);
      break;
    case OperandScale::kDouble:
      PatchIndirectJumpWith16BitOperand(jump_location + prefix_offset, delta);
      break;
    case OperandScale::kQuadruple:
      PatchIndirectJumpWith32BitOperand(jump_location + prefix_offset, delta);
      break;
    default:
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
    OperandSize operand_size = SizeForSignedOperand(delta);
    if (operand_size > OperandSize::kByte) {
      // Adjust for scaling byte prefix for wide jump offset.
      DCHECK_LE(delta, 0);
      delta -= 1;
    }
    OutputScaled(jump_bytecode, OperandSizesToScale(operand_size),
                 SignedOperand(delta, operand_size));
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
    OutputScaled(jump_bytecode, OperandSizesToScale(reserved_operand_size), 0);
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

BytecodeArrayBuilder& BytecodeArrayBuilder::StackCheck() {
  Output(Bytecode::kStackCheck);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotHole(
    BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfNotHole, label);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Throw() {
  Output(Bytecode::kThrow);
  exit_seen_in_block_ = true;
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ReThrow() {
  Output(Bytecode::kReThrow);
  exit_seen_in_block_ = true;
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Return() {
  SetReturnPosition();
  Output(Bytecode::kReturn);
  exit_seen_in_block_ = true;
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Debugger() {
  Output(Bytecode::kDebugger);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInPrepare(
    Register cache_info_triple) {
  OperandScale operand_scale =
      OperandSizesToScale(SizeForRegisterOperand(cache_info_triple));
  OutputScaled(Bytecode::kForInPrepare, operand_scale,
               RegisterOperand(cache_info_triple));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInDone(Register index,
                                                      Register cache_length) {
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(index), SizeForRegisterOperand(cache_length));
  OutputScaled(Bytecode::kForInDone, operand_scale, RegisterOperand(index),
               RegisterOperand(cache_length));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInNext(
    Register receiver, Register index, Register cache_type_array_pair,
    int feedback_slot) {
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(receiver), SizeForRegisterOperand(index),
      SizeForRegisterOperand(cache_type_array_pair),
      SizeForUnsignedOperand(feedback_slot));
  OutputScaled(Bytecode::kForInNext, operand_scale, RegisterOperand(receiver),
               RegisterOperand(index), RegisterOperand(cache_type_array_pair),
               UnsignedOperand(feedback_slot));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::ForInStep(Register index) {
  OperandScale operand_scale =
      OperandSizesToScale(SizeForRegisterOperand(index));
  OutputScaled(Bytecode::kForInStep, operand_scale, RegisterOperand(index));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::MarkHandler(int handler_id,
                                                        bool will_catch) {
  handler_table_builder()->SetHandlerTarget(handler_id, bytecodes()->size());
  handler_table_builder()->SetPrediction(handler_id, will_catch);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::MarkTryBegin(int handler_id,
                                                         Register context) {
  handler_table_builder()->SetTryRegionStart(handler_id, bytecodes()->size());
  handler_table_builder()->SetContextRegister(handler_id, context);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::MarkTryEnd(int handler_id) {
  handler_table_builder()->SetTryRegionEnd(handler_id, bytecodes()->size());
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
  DCHECK(exit_seen_in_block_);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Call(Register callable,
                                                 Register receiver_args,
                                                 size_t receiver_args_count,
                                                 int feedback_slot,
                                                 TailCallMode tail_call_mode) {
  Bytecode bytecode = BytecodeForCall(tail_call_mode);
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(callable), SizeForRegisterOperand(receiver_args),
      SizeForUnsignedOperand(receiver_args_count),
      SizeForUnsignedOperand(feedback_slot));
  OutputScaled(bytecode, operand_scale, RegisterOperand(callable),
               RegisterOperand(receiver_args),
               UnsignedOperand(receiver_args_count),
               UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::New(Register constructor,
                                                Register first_arg,
                                                size_t arg_count) {
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(constructor), SizeForRegisterOperand(first_arg),
      SizeForUnsignedOperand(arg_count));
  OutputScaled(Bytecode::kNew, operand_scale, RegisterOperand(constructor),
               RegisterOperand(first_arg), UnsignedOperand(arg_count));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id, Register first_arg, size_t arg_count) {
  DCHECK_EQ(1, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  Bytecode bytecode = IntrinsicsHelper::IsSupported(function_id)
                          ? Bytecode::kInvokeIntrinsic
                          : Bytecode::kCallRuntime;
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(first_arg), SizeForUnsignedOperand(arg_count));
  OutputScaled(bytecode, operand_scale, static_cast<uint16_t>(function_id),
               RegisterOperand(first_arg), UnsignedOperand(arg_count));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntimeForPair(
    Runtime::FunctionId function_id, Register first_arg, size_t arg_count,
    Register first_return) {
  DCHECK_EQ(2, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  OperandScale operand_scale = OperandSizesToScale(
      SizeForRegisterOperand(first_arg), SizeForUnsignedOperand(arg_count),
      SizeForRegisterOperand(first_return));
  OutputScaled(Bytecode::kCallRuntimeForPair, operand_scale,
               static_cast<uint16_t>(function_id), RegisterOperand(first_arg),
               UnsignedOperand(arg_count), RegisterOperand(first_return));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallJSRuntime(
    int context_index, Register receiver_args, size_t receiver_args_count) {
  OperandScale operand_scale =
      OperandSizesToScale(SizeForUnsignedOperand(context_index),
                          SizeForRegisterOperand(receiver_args),
                          SizeForUnsignedOperand(receiver_args_count));
  OutputScaled(Bytecode::kCallJSRuntime, operand_scale,
               UnsignedOperand(context_index), RegisterOperand(receiver_args),
               UnsignedOperand(receiver_args_count));
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::Delete(Register object,
                                                   LanguageMode language_mode) {
  OperandScale operand_scale =
      OperandSizesToScale(SizeForRegisterOperand(object));
  OutputScaled(BytecodeForDelete(language_mode), operand_scale,
               RegisterOperand(object));
  return *this;
}

size_t BytecodeArrayBuilder::GetConstantPoolEntry(Handle<Object> object) {
  return constant_array_builder()->Insert(object);
}

void BytecodeArrayBuilder::SetReturnPosition() {
  if (return_position_ == RelocInfo::kNoPosition) return;
  if (exit_seen_in_block_) return;
  source_position_table_builder_.AddStatementPosition(bytecodes_.size(),
                                                      return_position_);
}

void BytecodeArrayBuilder::SetStatementPosition(Statement* stmt) {
  if (stmt->position() == RelocInfo::kNoPosition) return;
  if (exit_seen_in_block_) return;
  source_position_table_builder_.AddStatementPosition(bytecodes_.size(),
                                                      stmt->position());
}

void BytecodeArrayBuilder::SetExpressionPosition(Expression* expr) {
  if (expr->position() == RelocInfo::kNoPosition) return;
  if (exit_seen_in_block_) return;
  source_position_table_builder_.AddExpressionPosition(bytecodes_.size(),
                                                       expr->position());
}

void BytecodeArrayBuilder::SetExpressionAsStatementPosition(Expression* expr) {
  if (expr->position() == RelocInfo::kNoPosition) return;
  if (exit_seen_in_block_) return;
  source_position_table_builder_.AddStatementPosition(bytecodes_.size(),
                                                      expr->position());
}

bool BytecodeArrayBuilder::TemporaryRegisterIsLive(Register reg) const {
  return temporary_register_allocator()->RegisterIsLive(reg);
}

bool BytecodeArrayBuilder::OperandIsValid(Bytecode bytecode,
                                          OperandScale operand_scale,
                                          int operand_index,
                                          uint32_t operand_value) const {
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode, operand_index, operand_scale);
  OperandType operand_type = Bytecodes::GetOperandType(bytecode, operand_index);
  switch (operand_type) {
    case OperandType::kNone:
      return false;
    case OperandType::kRegCount: {
      if (operand_index > 0) {
        OperandType previous_operand_type =
            Bytecodes::GetOperandType(bytecode, operand_index - 1);
        if (previous_operand_type != OperandType::kMaybeReg &&
            previous_operand_type != OperandType::kReg) {
          return false;
        }
      }
    }  // Fall-through
    case OperandType::kFlag8:
    case OperandType::kIdx:
    case OperandType::kRuntimeId:
    case OperandType::kImm: {
      size_t unsigned_value = static_cast<size_t>(operand_value);
      return SizeForUnsignedOperand(unsigned_value) <= operand_size;
    }
    case OperandType::kMaybeReg:
      if (operand_value == 0) {
        return true;
      }
    // Fall-through to kReg case.
    case OperandType::kReg:
    case OperandType::kRegOut: {
      Register reg = RegisterFromOperand(operand_value);
      return RegisterIsValid(reg, operand_size);
    }
    case OperandType::kRegOutPair:
    case OperandType::kRegPair: {
      Register reg0 = RegisterFromOperand(operand_value);
      Register reg1 = Register(reg0.index() + 1);
      // The size of reg1 is immaterial.
      return RegisterIsValid(reg0, operand_size) &&
             RegisterIsValid(reg1, OperandSize::kQuad);
    }
    case OperandType::kRegOutTriple: {
      Register reg0 = RegisterFromOperand(operand_value);
      Register reg1 = Register(reg0.index() + 1);
      Register reg2 = Register(reg0.index() + 2);
      // The size of reg1 and reg2 is immaterial.
      return RegisterIsValid(reg0, operand_size) &&
             RegisterIsValid(reg1, OperandSize::kQuad) &&
             RegisterIsValid(reg2, OperandSize::kQuad);
    }
  }
  UNREACHABLE();
  return false;
}

bool BytecodeArrayBuilder::RegisterIsValid(Register reg,
                                           OperandSize reg_size) const {
  if (!reg.is_valid()) {
    return false;
  }

  if (SizeForRegisterOperand(reg) > reg_size) {
    return false;
  }

  if (reg.is_current_context() || reg.is_function_closure() ||
      reg.is_new_target()) {
    return true;
  } else if (reg.is_parameter()) {
    int parameter_index = reg.ToParameterIndex(parameter_count());
    return parameter_index >= 0 && parameter_index < parameter_count();
  } else if (reg.index() < fixed_register_count()) {
    return true;
  } else {
    return TemporaryRegisterIsLive(reg);
  }
}


bool BytecodeArrayBuilder::LastBytecodeInSameBlock() const {
  return last_bytecode_start_ < bytecodes()->size() &&
         last_bytecode_start_ >= last_block_end_;
}


bool BytecodeArrayBuilder::IsRegisterInAccumulator(Register reg) {
  if (LastBytecodeInSameBlock()) {
    PreviousBytecodeHelper previous_bytecode(*this);
    Bytecode bytecode = previous_bytecode.GetBytecode();
    if (bytecode == Bytecode::kLdar || bytecode == Bytecode::kStar) {
      return previous_bytecode.GetRegisterOperand(0) == reg;
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
      return Bytecode::kIllegal;
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
      return Bytecode::kIllegal;
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
      return Bytecode::kIllegal;
  }
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForStoreIC(LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStoreICSloppy;
    case STRICT:
      return Bytecode::kStoreICStrict;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForKeyedStoreIC(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kKeyedStoreICSloppy;
    case STRICT:
      return Bytecode::kKeyedStoreICStrict;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForLoadGlobal(TypeofMode typeof_mode) {
  return typeof_mode == INSIDE_TYPEOF ? Bytecode::kLdaGlobalInsideTypeof
                                      : Bytecode::kLdaGlobal;
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForStoreGlobal(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStaGlobalSloppy;
    case STRICT:
      return Bytecode::kStaGlobalStrict;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForStoreLookupSlot(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStaLookupSlotSloppy;
    case STRICT:
      return Bytecode::kStaLookupSlotStrict;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}

// static
Bytecode BytecodeArrayBuilder::BytecodeForCreateArguments(
    CreateArgumentsType type) {
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      return Bytecode::kCreateMappedArguments;
    case CreateArgumentsType::kUnmappedArguments:
      return Bytecode::kCreateUnmappedArguments;
    case CreateArgumentsType::kRestParameter:
      return Bytecode::kCreateRestParameter;
  }
  UNREACHABLE();
  return Bytecode::kIllegal;
}


// static
Bytecode BytecodeArrayBuilder::BytecodeForDelete(LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kDeletePropertySloppy;
    case STRICT:
      return Bytecode::kDeletePropertyStrict;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}

// static
Bytecode BytecodeArrayBuilder::BytecodeForCall(TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      return Bytecode::kCall;
    case TailCallMode::kAllow:
      return Bytecode::kTailCall;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}

// static
OperandSize BytecodeArrayBuilder::SizeForRegisterOperand(Register value) {
  if (value.is_byte_operand()) {
    return OperandSize::kByte;
  } else if (value.is_short_operand()) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
}

// static
OperandSize BytecodeArrayBuilder::SizeForSignedOperand(int value) {
  if (kMinInt8 <= value && value <= kMaxInt8) {
    return OperandSize::kByte;
  } else if (kMinInt16 <= value && value <= kMaxInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
}

// static
OperandSize BytecodeArrayBuilder::SizeForUnsignedOperand(int value) {
  DCHECK_GE(value, 0);
  if (value <= kMaxUInt8) {
    return OperandSize::kByte;
  } else if (value <= kMaxUInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
}

OperandSize BytecodeArrayBuilder::SizeForUnsignedOperand(size_t value) {
  if (value <= static_cast<size_t>(kMaxUInt8)) {
    return OperandSize::kByte;
  } else if (value <= static_cast<size_t>(kMaxUInt16)) {
    return OperandSize::kShort;
  } else if (value <= kMaxUInt32) {
    return OperandSize::kQuad;
  } else {
    UNREACHABLE();
    return OperandSize::kQuad;
  }
}

OperandScale BytecodeArrayBuilder::OperandSizesToScale(OperandSize size0,
                                                       OperandSize size1,
                                                       OperandSize size2,
                                                       OperandSize size3) {
  OperandSize upper = std::max(size0, size1);
  OperandSize lower = std::max(size2, size3);
  OperandSize result = std::max(upper, lower);
  // Operand sizes have been scaled before calling this function.
  // Currently all scalable operands are byte sized at
  // OperandScale::kSingle.
  STATIC_ASSERT(static_cast<int>(OperandSize::kByte) ==
                    static_cast<int>(OperandScale::kSingle) &&
                static_cast<int>(OperandSize::kShort) ==
                    static_cast<int>(OperandScale::kDouble) &&
                static_cast<int>(OperandSize::kQuad) ==
                    static_cast<int>(OperandScale::kQuadruple));
  OperandScale operand_scale = static_cast<OperandScale>(result);
  DCHECK(operand_scale == OperandScale::kSingle ||
         operand_scale == OperandScale::kDouble ||
         operand_scale == OperandScale::kQuadruple);
  return operand_scale;
}

uint32_t BytecodeArrayBuilder::RegisterOperand(Register reg) {
  return static_cast<uint32_t>(reg.ToOperand());
}

Register BytecodeArrayBuilder::RegisterFromOperand(uint32_t operand) {
  return Register::FromOperand(static_cast<int32_t>(operand));
}

uint32_t BytecodeArrayBuilder::SignedOperand(int value, OperandSize size) {
  switch (size) {
    case OperandSize::kByte:
      return static_cast<uint8_t>(value & 0xff);
    case OperandSize::kShort:
      return static_cast<uint16_t>(value & 0xffff);
    case OperandSize::kQuad:
      return static_cast<uint32_t>(value);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

uint32_t BytecodeArrayBuilder::UnsignedOperand(int value) {
  DCHECK_GE(value, 0);
  return static_cast<uint32_t>(value);
}

uint32_t BytecodeArrayBuilder::UnsignedOperand(size_t value) {
  DCHECK_LE(value, kMaxUInt32);
  return static_cast<uint32_t>(value);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
