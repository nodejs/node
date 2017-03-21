// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-builder.h"

#include "src/globals.h"
#include "src/interpreter/bytecode-array-writer.h"
#include "src/interpreter/bytecode-dead-code-optimizer.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-peephole-optimizer.h"
#include "src/interpreter/bytecode-register-optimizer.h"
#include "src/interpreter/interpreter-intrinsics.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayBuilder::BytecodeArrayBuilder(
    Isolate* isolate, Zone* zone, int parameter_count, int context_count,
    int locals_count, FunctionLiteral* literal,
    SourcePositionTableBuilder::RecordingMode source_position_mode)
    : zone_(zone),
      bytecode_generated_(false),
      constant_array_builder_(zone, isolate->factory()->the_hole_value()),
      handler_table_builder_(zone),
      return_seen_in_block_(false),
      parameter_count_(parameter_count),
      local_register_count_(locals_count),
      context_register_count_(context_count),
      register_allocator_(fixed_register_count()),
      bytecode_array_writer_(zone, &constant_array_builder_,
                             source_position_mode),
      pipeline_(&bytecode_array_writer_),
      register_optimizer_(nullptr) {
  DCHECK_GE(parameter_count_, 0);
  DCHECK_GE(context_register_count_, 0);
  DCHECK_GE(local_register_count_, 0);

  if (FLAG_ignition_deadcode) {
    pipeline_ = new (zone) BytecodeDeadCodeOptimizer(pipeline_);
  }

  if (FLAG_ignition_peephole) {
    pipeline_ = new (zone) BytecodePeepholeOptimizer(pipeline_);
  }

  if (FLAG_ignition_reo) {
    register_optimizer_ = new (zone) BytecodeRegisterOptimizer(
        zone, &register_allocator_, fixed_register_count(), parameter_count,
        pipeline_);
  }

  return_position_ = literal ? literal->return_position() : kNoSourcePosition;
}

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

Handle<BytecodeArray> BytecodeArrayBuilder::ToBytecodeArray(Isolate* isolate) {
  DCHECK(return_seen_in_block_);
  DCHECK(!bytecode_generated_);
  bytecode_generated_ = true;

  int register_count = total_register_count();

  if (register_optimizer_) {
    register_optimizer_->Flush();
    register_count = register_optimizer_->maxiumum_register_index() + 1;
  }

  Handle<FixedArray> handler_table =
      handler_table_builder()->ToHandlerTable(isolate);
  return pipeline_->ToBytecodeArray(isolate, register_count, parameter_count(),
                                    handler_table);
}

BytecodeSourceInfo BytecodeArrayBuilder::CurrentSourcePosition(
    Bytecode bytecode) {
  BytecodeSourceInfo source_position;
  if (latest_source_info_.is_valid()) {
    // Statement positions need to be emitted immediately.  Expression
    // positions can be pushed back until a bytecode is found that can
    // throw (if expression position filtering is turned on). We only
    // invalidate the existing source position information if it is used.
    if (latest_source_info_.is_statement() ||
        !FLAG_ignition_filter_expression_positions ||
        !Bytecodes::IsWithoutExternalSideEffects(bytecode)) {
      source_position = latest_source_info_;
      latest_source_info_.set_invalid();
    }
  }
  return source_position;
}

namespace {

template <OperandTypeInfo type_info>
class UnsignedOperandHelper {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder, size_t value)) {
    DCHECK(IsValid(value));
    return static_cast<uint32_t>(value);
  }

  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder, int value)) {
    DCHECK_GE(value, 0);
    return Convert(builder, static_cast<size_t>(value));
  }

 private:
  static bool IsValid(size_t value) {
    switch (type_info) {
      case OperandTypeInfo::kFixedUnsignedByte:
        return value <= kMaxUInt8;
      case OperandTypeInfo::kFixedUnsignedShort:
        return value <= kMaxUInt16;
      case OperandTypeInfo::kScalableUnsignedByte:
        return value <= kMaxUInt32;
      default:
        UNREACHABLE();
        return false;
    }
  }
};

template <OperandType>
class OperandHelper {};

#define DEFINE_UNSIGNED_OPERAND_HELPER(Name, Type) \
  template <>                                      \
  class OperandHelper<OperandType::k##Name>        \
      : public UnsignedOperandHelper<Type> {};
UNSIGNED_FIXED_SCALAR_OPERAND_TYPE_LIST(DEFINE_UNSIGNED_OPERAND_HELPER)
UNSIGNED_SCALABLE_SCALAR_OPERAND_TYPE_LIST(DEFINE_UNSIGNED_OPERAND_HELPER)
#undef DEFINE_UNSIGNED_OPERAND_HELPER

template <>
class OperandHelper<OperandType::kImm> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder, int value)) {
    return static_cast<uint32_t>(value);
  }
};

template <>
class OperandHelper<OperandType::kReg> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder, Register reg)) {
    return builder->GetInputRegisterOperand(reg);
  }
};

template <>
class OperandHelper<OperandType::kRegList> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder,
                                 RegisterList reg_list)) {
    return builder->GetInputRegisterListOperand(reg_list);
  }
};

template <>
class OperandHelper<OperandType::kRegPair> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder,
                                 RegisterList reg_list)) {
    DCHECK_EQ(reg_list.register_count(), 2);
    return builder->GetInputRegisterListOperand(reg_list);
  }
};

template <>
class OperandHelper<OperandType::kRegOut> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder, Register reg)) {
    return builder->GetOutputRegisterOperand(reg);
  }
};

template <>
class OperandHelper<OperandType::kRegOutPair> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder,
                                 RegisterList reg_list)) {
    DCHECK_EQ(2, reg_list.register_count());
    return builder->GetOutputRegisterListOperand(reg_list);
  }
};

template <>
class OperandHelper<OperandType::kRegOutTriple> {
 public:
  INLINE(static uint32_t Convert(BytecodeArrayBuilder* builder,
                                 RegisterList reg_list)) {
    DCHECK_EQ(3, reg_list.register_count());
    return builder->GetOutputRegisterListOperand(reg_list);
  }
};

}  // namespace

template <Bytecode bytecode, AccumulatorUse accumulator_use,
          OperandType... operand_types>
class BytecodeNodeBuilder {
 public:
  template <typename... Operands>
  INLINE(static BytecodeNode Make(BytecodeArrayBuilder* builder,
                                  BytecodeSourceInfo source_info,
                                  Operands... operands)) {
    builder->PrepareToOutputBytecode<bytecode, accumulator_use>();
    // The "OperandHelper<operand_types>::Convert(builder, operands)..." will
    // expand both the OperandType... and Operands... parameter packs e.g. for:
    //   BytecodeNodeBuilder<OperandType::kReg, OperandType::kImm>::Make<
    //       Register, int>(..., Register reg, int immediate)
    // the code will expand into:
    //    OperandHelper<OperandType::kReg>::Convert(builder, reg),
    //    OperandHelper<OperandType::kImm>::Convert(builder, immediate),
    return BytecodeNode::Create<bytecode, accumulator_use, operand_types...>(
        source_info,
        OperandHelper<operand_types>::Convert(builder, operands)...);
  }
};

#define DEFINE_BYTECODE_OUTPUT(name, ...)                                \
  template <typename... Operands>                                        \
  void BytecodeArrayBuilder::Output##name(Operands... operands) {        \
    static_assert(sizeof...(Operands) <= Bytecodes::kMaxOperands,        \
                  "too many operands for bytecode");                     \
    BytecodeNode node(                                                   \
        BytecodeNodeBuilder<Bytecode::k##name, __VA_ARGS__>::Make<       \
            Operands...>(this, CurrentSourcePosition(Bytecode::k##name), \
                         operands...));                                  \
    pipeline()->Write(&node);                                            \
  }                                                                      \
                                                                         \
  template <typename... Operands>                                        \
  void BytecodeArrayBuilder::Output##name(BytecodeLabel* label,          \
                                          Operands... operands) {        \
    DCHECK(Bytecodes::IsJump(Bytecode::k##name));                        \
    BytecodeNode node(                                                   \
        BytecodeNodeBuilder<Bytecode::k##name, __VA_ARGS__>::Make<       \
            Operands...>(this, CurrentSourcePosition(Bytecode::k##name), \
                         operands...));                                  \
    pipeline()->WriteJump(&node, label);                                 \
    LeaveBasicBlock();                                                   \
  }
BYTECODE_LIST(DEFINE_BYTECODE_OUTPUT)
#undef DEFINE_BYTECODE_OUTPUT

BytecodeArrayBuilder& BytecodeArrayBuilder::BinaryOperation(Token::Value op,
                                                            Register reg,
                                                            int feedback_slot) {
  switch (op) {
    case Token::Value::ADD:
      OutputAdd(reg, feedback_slot);
      break;
    case Token::Value::SUB:
      OutputSub(reg, feedback_slot);
      break;
    case Token::Value::MUL:
      OutputMul(reg, feedback_slot);
      break;
    case Token::Value::DIV:
      OutputDiv(reg, feedback_slot);
      break;
    case Token::Value::MOD:
      OutputMod(reg, feedback_slot);
      break;
    case Token::Value::BIT_OR:
      OutputBitwiseOr(reg, feedback_slot);
      break;
    case Token::Value::BIT_XOR:
      OutputBitwiseXor(reg, feedback_slot);
      break;
    case Token::Value::BIT_AND:
      OutputBitwiseAnd(reg, feedback_slot);
      break;
    case Token::Value::SHL:
      OutputShiftLeft(reg, feedback_slot);
      break;
    case Token::Value::SAR:
      OutputShiftRight(reg, feedback_slot);
      break;
    case Token::Value::SHR:
      OutputShiftRightLogical(reg, feedback_slot);
      break;
    default:
      UNREACHABLE();
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CountOperation(Token::Value op,
                                                           int feedback_slot) {
  if (op == Token::Value::ADD) {
    OutputInc(feedback_slot);
  } else {
    DCHECK_EQ(op, Token::Value::SUB);
    OutputDec(feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LogicalNot() {
  OutputToBooleanLogicalNot();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::TypeOf() {
  OutputTypeOf();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::GetSuperConstructor(Register out) {
  OutputGetSuperConstructor(out);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareOperation(
    Token::Value op, Register reg, int feedback_slot) {
  switch (op) {
    case Token::Value::EQ:
      OutputTestEqual(reg, feedback_slot);
      break;
    case Token::Value::NE:
      OutputTestNotEqual(reg, feedback_slot);
      break;
    case Token::Value::EQ_STRICT:
      OutputTestEqualStrict(reg, feedback_slot);
      break;
    case Token::Value::LT:
      OutputTestLessThan(reg, feedback_slot);
      break;
    case Token::Value::GT:
      OutputTestGreaterThan(reg, feedback_slot);
      break;
    case Token::Value::LTE:
      OutputTestLessThanOrEqual(reg, feedback_slot);
      break;
    case Token::Value::GTE:
      OutputTestGreaterThanOrEqual(reg, feedback_slot);
      break;
    case Token::Value::INSTANCEOF:
      OutputTestInstanceOf(reg);
      break;
    case Token::Value::IN:
      OutputTestIn(reg);
      break;
    default:
      UNREACHABLE();
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadConstantPoolEntry(
    size_t entry) {
  OutputLdaConstant(entry);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(
    v8::internal::Smi* smi) {
  int32_t raw_smi = smi->value();
  if (raw_smi == 0) {
    OutputLdaZero();
  } else {
    OutputLdaSmi(raw_smi);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(Handle<Object> object) {
  size_t entry = GetConstantPoolEntry(object);
  OutputLdaConstant(entry);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadUndefined() {
  OutputLdaUndefined();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNull() {
  OutputLdaNull();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadTheHole() {
  OutputLdaTheHole();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadTrue() {
  OutputLdaTrue();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadFalse() {
  OutputLdaFalse();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadAccumulatorWithRegister(
    Register reg) {
  if (register_optimizer_) {
    register_optimizer_->DoLdar(reg, CurrentSourcePosition(Bytecode::kLdar));
  } else {
    OutputLdar(reg);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreAccumulatorInRegister(
    Register reg) {
  if (register_optimizer_) {
    register_optimizer_->DoStar(reg, CurrentSourcePosition(Bytecode::kStar));
  } else {
    OutputStar(reg);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::MoveRegister(Register from,
                                                         Register to) {
  DCHECK(from != to);
  if (register_optimizer_) {
    register_optimizer_->DoMov(from, to, CurrentSourcePosition(Bytecode::kMov));
  } else {
    OutputMov(from, to);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadGlobal(
    const Handle<String> name, int feedback_slot, TypeofMode typeof_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (typeof_mode == INSIDE_TYPEOF) {
    OutputLdaGlobalInsideTypeof(name_index, feedback_slot);
  } else {
    DCHECK_EQ(typeof_mode, NOT_INSIDE_TYPEOF);
    OutputLdaGlobal(name_index, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreGlobal(
    const Handle<String> name, int feedback_slot, LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    OutputStaGlobalSloppy(name_index, feedback_slot);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaGlobalStrict(name_index, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadContextSlot(Register context,
                                                            int slot_index,
                                                            int depth) {
  if (context.is_current_context() && depth == 0) {
    OutputLdaCurrentContextSlot(slot_index);
  } else {
    OutputLdaContextSlot(context, slot_index, depth);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreContextSlot(Register context,
                                                             int slot_index,
                                                             int depth) {
  if (context.is_current_context() && depth == 0) {
    OutputStaCurrentContextSlot(slot_index);
  } else {
    OutputStaContextSlot(context, slot_index, depth);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupSlot(
    const Handle<String> name, TypeofMode typeof_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (typeof_mode == INSIDE_TYPEOF) {
    OutputLdaLookupSlotInsideTypeof(name_index);
  } else {
    DCHECK_EQ(typeof_mode, NOT_INSIDE_TYPEOF);
    OutputLdaLookupSlot(name_index);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupContextSlot(
    const Handle<String> name, TypeofMode typeof_mode, int slot_index,
    int depth) {
  size_t name_index = GetConstantPoolEntry(name);
  if (typeof_mode == INSIDE_TYPEOF) {
    OutputLdaLookupContextSlotInsideTypeof(name_index, slot_index, depth);
  } else {
    DCHECK(typeof_mode == NOT_INSIDE_TYPEOF);
    OutputLdaLookupContextSlot(name_index, slot_index, depth);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupGlobalSlot(
    const Handle<String> name, TypeofMode typeof_mode, int feedback_slot,
    int depth) {
  size_t name_index = GetConstantPoolEntry(name);
  if (typeof_mode == INSIDE_TYPEOF) {
    OutputLdaLookupGlobalSlotInsideTypeof(name_index, feedback_slot, depth);
  } else {
    DCHECK(typeof_mode == NOT_INSIDE_TYPEOF);
    OutputLdaLookupGlobalSlot(name_index, feedback_slot, depth);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreLookupSlot(
    const Handle<String> name, LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    OutputStaLookupSlotSloppy(name_index);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaLookupSlotStrict(name_index);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNamedProperty(
    Register object, const Handle<Name> name, int feedback_slot) {
  size_t name_index = GetConstantPoolEntry(name);
  OutputLdaNamedProperty(object, name_index, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadKeyedProperty(
    Register object, int feedback_slot) {
  OutputLdaKeyedProperty(object, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreDataPropertyInLiteral(
    Register object, Register name, DataPropertyInLiteralFlags flags,
    int feedback_slot) {
  OutputStaDataPropertyInLiteral(object, name, flags, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedProperty(
    Register object, const Handle<Name> name, int feedback_slot,
    LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    OutputStaNamedPropertySloppy(object, name_index, feedback_slot);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaNamedPropertyStrict(object, name_index, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreKeyedProperty(
    Register object, Register key, int feedback_slot,
    LanguageMode language_mode) {
  if (language_mode == SLOPPY) {
    OutputStaKeyedPropertySloppy(object, key, feedback_slot);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaKeyedPropertyStrict(object, key, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateClosure(
    size_t shared_function_info_entry, int slot, int flags) {
  OutputCreateClosure(shared_function_info_entry, slot, flags);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateBlockContext(
    Handle<ScopeInfo> scope_info) {
  size_t entry = GetConstantPoolEntry(scope_info);
  OutputCreateBlockContext(entry);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateCatchContext(
    Register exception, Handle<String> name, Handle<ScopeInfo> scope_info) {
  size_t name_index = GetConstantPoolEntry(name);
  size_t scope_info_index = GetConstantPoolEntry(scope_info);
  OutputCreateCatchContext(exception, name_index, scope_info_index);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateFunctionContext(int slots) {
  OutputCreateFunctionContext(slots);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateEvalContext(int slots) {
  OutputCreateEvalContext(slots);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateWithContext(
    Register object, Handle<ScopeInfo> scope_info) {
  size_t scope_info_index = GetConstantPoolEntry(scope_info);
  OutputCreateWithContext(object, scope_info_index);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArguments(
    CreateArgumentsType type) {
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      OutputCreateMappedArguments();
      break;
    case CreateArgumentsType::kUnmappedArguments:
      OutputCreateUnmappedArguments();
      break;
    case CreateArgumentsType::kRestParameter:
      OutputCreateRestParameter();
      break;
    default:
      UNREACHABLE();
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateRegExpLiteral(
    Handle<String> pattern, int literal_index, int flags) {
  size_t pattern_entry = GetConstantPoolEntry(pattern);
  OutputCreateRegExpLiteral(pattern_entry, literal_index, flags);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArrayLiteral(
    size_t constant_elements_entry, int literal_index, int flags) {
  OutputCreateArrayLiteral(constant_elements_entry, literal_index, flags);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateObjectLiteral(
    size_t constant_properties_entry, int literal_index, int flags,
    Register output) {
  OutputCreateObjectLiteral(constant_properties_entry, literal_index, flags,
                            output);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::PushContext(Register context) {
  OutputPushContext(context);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::PopContext(Register context) {
  OutputPopContext(context);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ConvertAccumulatorToObject(
    Register out) {
  OutputToObject(out);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ConvertAccumulatorToName(
    Register out) {
  OutputToName(out);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ConvertAccumulatorToNumber(
    Register out) {
  OutputToNumber(out);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(BytecodeLabel* label) {
  // Flush the register optimizer when binding a label to ensure all
  // expected registers are valid when jumping to this label.
  if (register_optimizer_) register_optimizer_->Flush();
  pipeline_->BindLabel(label);
  LeaveBasicBlock();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(const BytecodeLabel& target,
                                                 BytecodeLabel* label) {
  pipeline_->BindLabel(target, label);
  LeaveBasicBlock();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Jump(BytecodeLabel* label) {
  OutputJump(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfTrue(BytecodeLabel* label) {
  // The peephole optimizer attempts to simplify JumpIfToBooleanTrue
  // to JumpIfTrue.
  OutputJumpIfToBooleanTrue(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfFalse(BytecodeLabel* label) {
  OutputJumpIfToBooleanFalse(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNull(BytecodeLabel* label) {
  OutputJumpIfNull(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfUndefined(
    BytecodeLabel* label) {
  OutputJumpIfUndefined(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotHole(
    BytecodeLabel* label) {
  OutputJumpIfNotHole(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfJSReceiver(
    BytecodeLabel* label) {
  OutputJumpIfJSReceiver(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpLoop(BytecodeLabel* label,
                                                     int loop_depth) {
  OutputJumpLoop(label, 0, loop_depth);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StackCheck(int position) {
  if (position != kNoSourcePosition) {
    // We need to attach a non-breakable source position to a stack
    // check, so we simply add it as expression position. There can be
    // a prior statement position from constructs like:
    //
    //    do var x;  while (false);
    //
    // A Nop could be inserted for empty statements, but since no code
    // is associated with these positions, instead we force the stack
    // check's expression position which eliminates the empty
    // statement's position.
    latest_source_info_.ForceExpressionPosition(position);
  }
  OutputStackCheck();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::SetPendingMessage() {
  OutputSetPendingMessage();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Throw() {
  OutputThrow();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ReThrow() {
  OutputReThrow();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Return() {
  SetReturnPosition();
  OutputReturn();
  return_seen_in_block_ = true;
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Debugger() {
  OutputDebugger();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInPrepare(
    Register receiver, RegisterList cache_info_triple) {
  DCHECK_EQ(3, cache_info_triple.register_count());
  OutputForInPrepare(receiver, cache_info_triple);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInContinue(
    Register index, Register cache_length) {
  OutputForInContinue(index, cache_length);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInNext(
    Register receiver, Register index, RegisterList cache_type_array_pair,
    int feedback_slot) {
  DCHECK_EQ(2, cache_type_array_pair.register_count());
  OutputForInNext(receiver, index, cache_type_array_pair, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInStep(Register index) {
  OutputForInStep(index);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreModuleVariable(int cell_index,
                                                                int depth) {
  OutputStaModuleVariable(cell_index, depth);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadModuleVariable(int cell_index,
                                                               int depth) {
  OutputLdaModuleVariable(cell_index, depth);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::SuspendGenerator(
    Register generator) {
  OutputSuspendGenerator(generator);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ResumeGenerator(
    Register generator) {
  OutputResumeGenerator(generator);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::MarkHandler(
    int handler_id, HandlerTable::CatchPrediction catch_prediction) {
  BytecodeLabel handler;
  Bind(&handler);
  handler_table_builder()->SetHandlerTarget(handler_id, handler.offset());
  handler_table_builder()->SetPrediction(handler_id, catch_prediction);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::MarkTryBegin(int handler_id,
                                                         Register context) {
  BytecodeLabel try_begin;
  Bind(&try_begin);
  handler_table_builder()->SetTryRegionStart(handler_id, try_begin.offset());
  handler_table_builder()->SetContextRegister(handler_id, context);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::MarkTryEnd(int handler_id) {
  BytecodeLabel try_end;
  Bind(&try_end);
  handler_table_builder()->SetTryRegionEnd(handler_id, try_end.offset());
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Call(Register callable,
                                                 RegisterList args,
                                                 int feedback_slot,
                                                 Call::CallType call_type,
                                                 TailCallMode tail_call_mode) {
  if (tail_call_mode == TailCallMode::kDisallow) {
    if (call_type == Call::NAMED_PROPERTY_CALL ||
        call_type == Call::KEYED_PROPERTY_CALL) {
      OutputCallProperty(callable, args, args.register_count(), feedback_slot);
    } else {
      OutputCall(callable, args, args.register_count(), feedback_slot);
    }
  } else {
    DCHECK(tail_call_mode == TailCallMode::kAllow);
    OutputTailCall(callable, args, args.register_count(), feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::New(Register constructor,
                                                RegisterList args,
                                                int feedback_slot_id) {
  OutputNew(constructor, args, args.register_count(), feedback_slot_id);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id, RegisterList args) {
  DCHECK_EQ(1, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(Bytecodes::SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  if (IntrinsicsHelper::IsSupported(function_id)) {
    IntrinsicsHelper::IntrinsicId intrinsic_id =
        IntrinsicsHelper::FromRuntimeId(function_id);
    OutputInvokeIntrinsic(static_cast<int>(intrinsic_id), args,
                          args.register_count());
  } else {
    OutputCallRuntime(static_cast<int>(function_id), args,
                      args.register_count());
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id, Register arg) {
  return CallRuntime(function_id, RegisterList(arg.index(), 1));
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id) {
  return CallRuntime(function_id, RegisterList());
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntimeForPair(
    Runtime::FunctionId function_id, RegisterList args,
    RegisterList return_pair) {
  DCHECK_EQ(2, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(Bytecodes::SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  DCHECK_EQ(2, return_pair.register_count());
  OutputCallRuntimeForPair(static_cast<uint16_t>(function_id), args,
                           args.register_count(), return_pair);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntimeForPair(
    Runtime::FunctionId function_id, Register arg, RegisterList return_pair) {
  return CallRuntimeForPair(function_id, RegisterList(arg.index(), 1),
                            return_pair);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallJSRuntime(int context_index,
                                                          RegisterList args) {
  OutputCallJSRuntime(context_index, args, args.register_count());
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::NewWithSpread(RegisterList args) {
  OutputNewWithSpread(args, args.register_count());
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Delete(Register object,
                                                   LanguageMode language_mode) {
  if (language_mode == SLOPPY) {
    OutputDeletePropertySloppy(object);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputDeletePropertyStrict(object);
  }
  return *this;
}

size_t BytecodeArrayBuilder::GetConstantPoolEntry(Handle<Object> object) {
  return constant_array_builder()->Insert(object);
}

size_t BytecodeArrayBuilder::AllocateConstantPoolEntry() {
  return constant_array_builder()->AllocateEntry();
}

void BytecodeArrayBuilder::InsertConstantPoolEntryAt(size_t entry,
                                                     Handle<Object> object) {
  constant_array_builder()->InsertAllocatedEntry(entry, object);
}

void BytecodeArrayBuilder::SetReturnPosition() {
  if (return_position_ == kNoSourcePosition) return;
  latest_source_info_.MakeStatementPosition(return_position_);
}

bool BytecodeArrayBuilder::RegisterIsValid(Register reg) const {
  if (!reg.is_valid()) {
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
    return register_allocator()->RegisterIsLive(reg);
  }
}

bool BytecodeArrayBuilder::RegisterListIsValid(RegisterList reg_list) const {
  if (reg_list.register_count() == 0) {
    return reg_list.first_register() == Register(0);
  } else {
    int first_reg_index = reg_list.first_register().index();
    for (int i = 0; i < reg_list.register_count(); i++) {
      if (!RegisterIsValid(Register(first_reg_index + i))) {
        return false;
      }
    }
    return true;
  }
}

template <Bytecode bytecode, AccumulatorUse accumulator_use>
void BytecodeArrayBuilder::PrepareToOutputBytecode() {
  if (register_optimizer_)
    register_optimizer_->PrepareForBytecode<bytecode, accumulator_use>();
}

uint32_t BytecodeArrayBuilder::GetInputRegisterOperand(Register reg) {
  DCHECK(RegisterIsValid(reg));
  if (register_optimizer_) reg = register_optimizer_->GetInputRegister(reg);
  return static_cast<uint32_t>(reg.ToOperand());
}

uint32_t BytecodeArrayBuilder::GetOutputRegisterOperand(Register reg) {
  DCHECK(RegisterIsValid(reg));
  if (register_optimizer_) register_optimizer_->PrepareOutputRegister(reg);
  return static_cast<uint32_t>(reg.ToOperand());
}

uint32_t BytecodeArrayBuilder::GetInputRegisterListOperand(
    RegisterList reg_list) {
  DCHECK(RegisterListIsValid(reg_list));
  if (register_optimizer_)
    reg_list = register_optimizer_->GetInputRegisterList(reg_list);
  return static_cast<uint32_t>(reg_list.first_register().ToOperand());
}

uint32_t BytecodeArrayBuilder::GetOutputRegisterListOperand(
    RegisterList reg_list) {
  DCHECK(RegisterListIsValid(reg_list));
  if (register_optimizer_)
    register_optimizer_->PrepareOutputRegisterList(reg_list);
  return static_cast<uint32_t>(reg_list.first_register().ToOperand());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
