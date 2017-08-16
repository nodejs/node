// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-builder.h"

#include "src/globals.h"
#include "src/interpreter/bytecode-array-writer.h"
#include "src/interpreter/bytecode-jump-table.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-node.h"
#include "src/interpreter/bytecode-register-optimizer.h"
#include "src/interpreter/bytecode-source-info.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

class RegisterTransferWriter final
    : public NON_EXPORTED_BASE(BytecodeRegisterOptimizer::BytecodeWriter),
      public NON_EXPORTED_BASE(ZoneObject) {
 public:
  RegisterTransferWriter(BytecodeArrayBuilder* builder) : builder_(builder) {}
  ~RegisterTransferWriter() override {}

  void EmitLdar(Register input) override { builder_->OutputLdarRaw(input); }

  void EmitStar(Register output) override { builder_->OutputStarRaw(output); }

  void EmitMov(Register input, Register output) override {
    builder_->OutputMovRaw(input, output);
  }

 private:
  BytecodeArrayBuilder* builder_;
};

BytecodeArrayBuilder::BytecodeArrayBuilder(
    Isolate* isolate, Zone* zone, int parameter_count, int locals_count,
    FunctionLiteral* literal,
    SourcePositionTableBuilder::RecordingMode source_position_mode)
    : zone_(zone),
      literal_(literal),
      bytecode_generated_(false),
      constant_array_builder_(zone),
      handler_table_builder_(zone),
      return_seen_in_block_(false),
      parameter_count_(parameter_count),
      local_register_count_(locals_count),
      register_allocator_(fixed_register_count()),
      bytecode_array_writer_(zone, &constant_array_builder_,
                             source_position_mode),
      register_optimizer_(nullptr) {
  DCHECK_GE(parameter_count_, 0);
  DCHECK_GE(local_register_count_, 0);

  if (FLAG_ignition_reo) {
    register_optimizer_ = new (zone) BytecodeRegisterOptimizer(
        zone, &register_allocator_, fixed_register_count(), parameter_count,
        new (zone) RegisterTransferWriter(this));
  }

  return_position_ = literal ? literal->return_position() : kNoSourcePosition;
}

Register BytecodeArrayBuilder::Parameter(int parameter_index) const {
  DCHECK_GE(parameter_index, 0);
  // The parameter indices are shifted by 1 (receiver is the
  // first entry).
  return Register::FromParameterIndex(parameter_index + 1, parameter_count());
}

Register BytecodeArrayBuilder::Receiver() const {
  return Register::FromParameterIndex(0, parameter_count());
}

Register BytecodeArrayBuilder::Local(int index) const {
  // TODO(marja): Make a DCHECK once crbug.com/706234 is fixed.
  CHECK_LT(index, locals_count());
  return Register(index);
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
  return bytecode_array_writer_.ToBytecodeArray(
      isolate, register_count, parameter_count(), handler_table);
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

void BytecodeArrayBuilder::SetDeferredSourceInfo(
    BytecodeSourceInfo source_info) {
  if (!source_info.is_valid()) return;
  if (deferred_source_info_.is_valid()) {
    // Emit any previous deferred source info now as a nop.
    BytecodeNode node = BytecodeNode::Nop(deferred_source_info_);
    bytecode_array_writer_.Write(&node);
  }
  deferred_source_info_ = source_info;
}

void BytecodeArrayBuilder::AttachOrEmitDeferredSourceInfo(BytecodeNode* node) {
  if (!deferred_source_info_.is_valid()) return;

  if (!node->source_info().is_valid()) {
    node->set_source_info(deferred_source_info_);
  } else {
    BytecodeNode node = BytecodeNode::Nop(deferred_source_info_);
    bytecode_array_writer_.Write(&node);
  }
  deferred_source_info_.set_invalid();
}

void BytecodeArrayBuilder::Write(BytecodeNode* node) {
  AttachOrEmitDeferredSourceInfo(node);
  bytecode_array_writer_.Write(node);
}

void BytecodeArrayBuilder::WriteJump(BytecodeNode* node, BytecodeLabel* label) {
  AttachOrEmitDeferredSourceInfo(node);
  bytecode_array_writer_.WriteJump(node, label);
}

void BytecodeArrayBuilder::WriteSwitch(BytecodeNode* node,
                                       BytecodeJumpTable* jump_table) {
  AttachOrEmitDeferredSourceInfo(node);
  bytecode_array_writer_.WriteSwitch(node, jump_table);
}

void BytecodeArrayBuilder::OutputLdarRaw(Register reg) {
  uint32_t operand = static_cast<uint32_t>(reg.ToOperand());
  BytecodeNode node(BytecodeNode::Ldar(BytecodeSourceInfo(), operand));
  Write(&node);
}

void BytecodeArrayBuilder::OutputStarRaw(Register reg) {
  uint32_t operand = static_cast<uint32_t>(reg.ToOperand());
  BytecodeNode node(BytecodeNode::Star(BytecodeSourceInfo(), operand));
  Write(&node);
}

void BytecodeArrayBuilder::OutputMovRaw(Register src, Register dest) {
  uint32_t operand0 = static_cast<uint32_t>(src.ToOperand());
  uint32_t operand1 = static_cast<uint32_t>(dest.ToOperand());
  BytecodeNode node(
      BytecodeNode::Mov(BytecodeSourceInfo(), operand0, operand1));
  Write(&node);
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
                                  Operands... operands)) {
    static_assert(sizeof...(Operands) <= Bytecodes::kMaxOperands,
                  "too many operands for bytecode");
    builder->PrepareToOutputBytecode<bytecode, accumulator_use>();
    // The "OperandHelper<operand_types>::Convert(builder, operands)..." will
    // expand both the OperandType... and Operands... parameter packs e.g. for:
    //   BytecodeNodeBuilder<OperandType::kReg, OperandType::kImm>::Make<
    //       Register, int>(..., Register reg, int immediate)
    // the code will expand into:
    //    OperandHelper<OperandType::kReg>::Convert(builder, reg),
    //    OperandHelper<OperandType::kImm>::Convert(builder, immediate),
    return BytecodeNode::Create<bytecode, accumulator_use, operand_types...>(
        builder->CurrentSourcePosition(bytecode),
        OperandHelper<operand_types>::Convert(builder, operands)...);
  }
};

#define DEFINE_BYTECODE_OUTPUT(name, ...)                             \
  template <typename... Operands>                                     \
  BytecodeNode BytecodeArrayBuilder::Create##name##Node(              \
      Operands... operands) {                                         \
    return BytecodeNodeBuilder<Bytecode::k##name, __VA_ARGS__>::Make( \
        this, operands...);                                           \
  }                                                                   \
                                                                      \
  template <typename... Operands>                                     \
  void BytecodeArrayBuilder::Output##name(Operands... operands) {     \
    BytecodeNode node(Create##name##Node(operands...));               \
    Write(&node);                                                     \
  }                                                                   \
                                                                      \
  template <typename... Operands>                                     \
  void BytecodeArrayBuilder::Output##name(BytecodeLabel* label,       \
                                          Operands... operands) {     \
    DCHECK(Bytecodes::IsJump(Bytecode::k##name));                     \
    BytecodeNode node(Create##name##Node(operands...));               \
    WriteJump(&node, label);                                          \
    LeaveBasicBlock();                                                \
  }
BYTECODE_LIST(DEFINE_BYTECODE_OUTPUT)
#undef DEFINE_BYTECODE_OUTPUT

void BytecodeArrayBuilder::OutputSwitchOnSmiNoFeedback(
    BytecodeJumpTable* jump_table) {
  BytecodeNode node(CreateSwitchOnSmiNoFeedbackNode(
      jump_table->constant_pool_index(), jump_table->size(),
      jump_table->case_value_base()));
  WriteSwitch(&node, jump_table);
  LeaveBasicBlock();
}

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

BytecodeArrayBuilder& BytecodeArrayBuilder::BinaryOperationSmiLiteral(
    Token::Value op, Smi* literal, int feedback_slot) {
  switch (op) {
    case Token::Value::ADD:
      OutputAddSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::SUB:
      OutputSubSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::MUL:
      OutputMulSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::DIV:
      OutputDivSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::MOD:
      OutputModSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::BIT_OR:
      OutputBitwiseOrSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::BIT_XOR:
      OutputBitwiseXorSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::BIT_AND:
      OutputBitwiseAndSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::SHL:
      OutputShiftLeftSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::SAR:
      OutputShiftRightSmi(literal->value(), feedback_slot);
      break;
    case Token::Value::SHR:
      OutputShiftRightLogicalSmi(literal->value(), feedback_slot);
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

BytecodeArrayBuilder& BytecodeArrayBuilder::LogicalNot(ToBooleanMode mode) {
  if (mode == ToBooleanMode::kAlreadyBoolean) {
    OutputLogicalNot();
  } else {
    DCHECK_EQ(mode, ToBooleanMode::kConvertToBoolean);
    OutputToBooleanLogicalNot();
  }
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
  DCHECK(feedback_slot != kNoFeedbackSlot);
  switch (op) {
    case Token::Value::EQ:
      OutputTestEqual(reg, feedback_slot);
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
    default:
      UNREACHABLE();
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareOperation(Token::Value op,
                                                             Register reg) {
  switch (op) {
    case Token::Value::EQ_STRICT:
      OutputTestEqualStrictNoFeedback(reg);
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

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareUndetectable() {
  OutputTestUndetectable();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareUndefined() {
  OutputTestUndefined();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareNull() {
  OutputTestNull();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareNil(Token::Value op,
                                                       NilValue nil) {
  if (op == Token::EQ) {
    return CompareUndetectable();
  } else {
    DCHECK_EQ(Token::EQ_STRICT, op);
    if (nil == kUndefinedValue) {
      return CompareUndefined();
    } else {
      DCHECK_EQ(kNullValue, nil);
      return CompareNull();
    }
  }
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareTypeOf(
    TestTypeOfFlags::LiteralFlag literal_flag) {
  DCHECK(literal_flag != TestTypeOfFlags::LiteralFlag::kOther);
  OutputTestTypeOf(TestTypeOfFlags::Encode(literal_flag));
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

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(
    const AstRawString* raw_string) {
  size_t entry = GetConstantPoolEntry(raw_string);
  OutputLdaConstant(entry);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(const Scope* scope) {
  size_t entry = GetConstantPoolEntry(scope);
  OutputLdaConstant(entry);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(
    const AstValue* ast_value) {
  if (ast_value->IsSmi()) {
    return LoadLiteral(ast_value->AsSmi());
  } else if (ast_value->IsUndefined()) {
    return LoadUndefined();
  } else if (ast_value->IsTrue()) {
    return LoadTrue();
  } else if (ast_value->IsFalse()) {
    return LoadFalse();
  } else if (ast_value->IsNull()) {
    return LoadNull();
  } else if (ast_value->IsTheHole()) {
    return LoadTheHole();
  } else if (ast_value->IsString()) {
    return LoadLiteral(ast_value->AsString());
  } else if (ast_value->IsHeapNumber()) {
    size_t entry = GetConstantPoolEntry(ast_value);
    OutputLdaConstant(entry);
    return *this;
  } else {
    // This should be the only ast value type left.
    DCHECK(ast_value->IsSymbol());
    size_t entry;
    switch (ast_value->AsSymbol()) {
      case AstSymbol::kHomeObjectSymbol:
        entry = HomeObjectSymbolConstantPoolEntry();
        break;
        // No default case so that we get a warning if AstSymbol changes
    }
    OutputLdaConstant(entry);
    return *this;
  }
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
    // Defer source info so that if we elide the bytecode transfer, we attach
    // the source info to a subsequent bytecode or to a nop.
    SetDeferredSourceInfo(CurrentSourcePosition(Bytecode::kLdar));
    register_optimizer_->DoLdar(reg);
  } else {
    OutputLdar(reg);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreAccumulatorInRegister(
    Register reg) {
  if (register_optimizer_) {
    // Defer source info so that if we elide the bytecode transfer, we attach
    // the source info to a subsequent bytecode or to a nop.
    SetDeferredSourceInfo(CurrentSourcePosition(Bytecode::kStar));
    register_optimizer_->DoStar(reg);
  } else {
    OutputStar(reg);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::MoveRegister(Register from,
                                                         Register to) {
  DCHECK(from != to);
  if (register_optimizer_) {
    // Defer source info so that if we elide the bytecode transfer, we attach
    // the source info to a subsequent bytecode or to a nop.
    SetDeferredSourceInfo(CurrentSourcePosition(Bytecode::kMov));
    register_optimizer_->DoMov(from, to);
  } else {
    OutputMov(from, to);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadGlobal(const AstRawString* name,
                                                       int feedback_slot,
                                                       TypeofMode typeof_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  // Ensure that typeof mode is in sync with the IC slot kind if the function
  // literal is available (not a unit test case).
  // TODO(ishell): check only in debug mode.
  if (literal_) {
    FeedbackSlot slot = FeedbackVector::ToSlot(feedback_slot);
    CHECK_EQ(GetTypeofModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
             typeof_mode);
  }
  if (typeof_mode == INSIDE_TYPEOF) {
    OutputLdaGlobalInsideTypeof(name_index, feedback_slot);
  } else {
    DCHECK_EQ(typeof_mode, NOT_INSIDE_TYPEOF);
    OutputLdaGlobal(name_index, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreGlobal(
    const AstRawString* name, int feedback_slot, LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    OutputStaGlobalSloppy(name_index, feedback_slot);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaGlobalStrict(name_index, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadContextSlot(
    Register context, int slot_index, int depth,
    ContextSlotMutability mutability) {
  if (context.is_current_context() && depth == 0) {
    if (mutability == kImmutableSlot) {
      OutputLdaImmutableCurrentContextSlot(slot_index);
    } else {
      DCHECK_EQ(kMutableSlot, mutability);
      OutputLdaCurrentContextSlot(slot_index);
    }
  } else if (mutability == kImmutableSlot) {
    OutputLdaImmutableContextSlot(context, slot_index, depth);
  } else {
    DCHECK_EQ(mutability, kMutableSlot);
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
    const AstRawString* name, TypeofMode typeof_mode) {
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
    const AstRawString* name, TypeofMode typeof_mode, int slot_index,
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
    const AstRawString* name, TypeofMode typeof_mode, int feedback_slot,
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
    const AstRawString* name, LanguageMode language_mode) {
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
    Register object, const AstRawString* name, int feedback_slot) {
  size_t name_index = GetConstantPoolEntry(name);
  OutputLdaNamedProperty(object, name_index, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadKeyedProperty(
    Register object, int feedback_slot) {
  OutputLdaKeyedProperty(object, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadIteratorProperty(
    Register object, int feedback_slot) {
  size_t name_index = IteratorSymbolConstantPoolEntry();
  OutputLdaNamedProperty(object, name_index, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadAsyncIteratorProperty(
    Register object, int feedback_slot) {
  size_t name_index = AsyncIteratorSymbolConstantPoolEntry();
  OutputLdaNamedProperty(object, name_index, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreDataPropertyInLiteral(
    Register object, Register name, DataPropertyInLiteralFlags flags,
    int feedback_slot) {
  OutputStaDataPropertyInLiteral(object, name, flags, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CollectTypeProfile(int position) {
  DCHECK(FLAG_type_profile);
  OutputCollectTypeProfile(position);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedProperty(
    Register object, size_t name_index, int feedback_slot,
    LanguageMode language_mode) {
  // Ensure that language mode is in sync with the IC slot kind if the function
  // literal is available (not a unit test case).
  // TODO(ishell): check only in debug mode.
  if (literal_) {
    FeedbackSlot slot = FeedbackVector::ToSlot(feedback_slot);
    CHECK_EQ(GetLanguageModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
             language_mode);
  }
  if (language_mode == SLOPPY) {
    OutputStaNamedPropertySloppy(object, name_index, feedback_slot);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaNamedPropertyStrict(object, name_index, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedProperty(
    Register object, const AstRawString* name, int feedback_slot,
    LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  return StoreNamedProperty(object, name_index, feedback_slot, language_mode);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedOwnProperty(
    Register object, const AstRawString* name, int feedback_slot) {
  size_t name_index = GetConstantPoolEntry(name);
  // Ensure that the store operation is in sync with the IC slot kind if
  // the function literal is available (not a unit test case).
  // TODO(ishell): check only in debug mode.
  if (literal_) {
    FeedbackSlot slot = FeedbackVector::ToSlot(feedback_slot);
    CHECK_EQ(FeedbackSlotKind::kStoreOwnNamed,
             feedback_vector_spec()->GetKind(slot));
  }
  OutputStaNamedOwnProperty(object, name_index, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreKeyedProperty(
    Register object, Register key, int feedback_slot,
    LanguageMode language_mode) {
  // Ensure that language mode is in sync with the IC slot kind if the function
  // literal is available (not a unit test case).
  // TODO(ishell): check only in debug mode.
  if (literal_) {
    FeedbackSlot slot = FeedbackVector::ToSlot(feedback_slot);
    CHECK_EQ(GetLanguageModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
             language_mode);
  }
  if (language_mode == SLOPPY) {
    OutputStaKeyedPropertySloppy(object, key, feedback_slot);
  } else {
    DCHECK_EQ(language_mode, STRICT);
    OutputStaKeyedPropertyStrict(object, key, feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreHomeObjectProperty(
    Register object, int feedback_slot, LanguageMode language_mode) {
  size_t name_index = HomeObjectSymbolConstantPoolEntry();
  return StoreNamedProperty(object, name_index, feedback_slot, language_mode);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateClosure(
    size_t shared_function_info_entry, int slot, int flags) {
  OutputCreateClosure(shared_function_info_entry, slot, flags);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateBlockContext(
    const Scope* scope) {
  size_t entry = GetConstantPoolEntry(scope);
  OutputCreateBlockContext(entry);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateCatchContext(
    Register exception, const AstRawString* name, const Scope* scope) {
  size_t name_index = GetConstantPoolEntry(name);
  size_t scope_index = GetConstantPoolEntry(scope);
  OutputCreateCatchContext(exception, name_index, scope_index);
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
    Register object, const Scope* scope) {
  size_t scope_index = GetConstantPoolEntry(scope);
  OutputCreateWithContext(object, scope_index);
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
    const AstRawString* pattern, int literal_index, int flags) {
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
    Register out, int feedback_slot) {
  OutputToNumber(out, feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(BytecodeLabel* label) {
  // Flush the register optimizer when binding a label to ensure all
  // expected registers are valid when jumping to this label.
  if (register_optimizer_) register_optimizer_->Flush();
  bytecode_array_writer_.BindLabel(label);
  LeaveBasicBlock();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(const BytecodeLabel& target,
                                                 BytecodeLabel* label) {
  bytecode_array_writer_.BindLabel(target, label);
  LeaveBasicBlock();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(BytecodeJumpTable* jump_table,
                                                 int case_value) {
  // Flush the register optimizer when binding a jump table entry to ensure
  // all expected registers are valid when jumping to this location.
  if (register_optimizer_) register_optimizer_->Flush();
  bytecode_array_writer_.BindJumpTableEntry(jump_table, case_value);
  LeaveBasicBlock();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Jump(BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJump(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfTrue(ToBooleanMode mode,
                                                       BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  if (mode == ToBooleanMode::kAlreadyBoolean) {
    OutputJumpIfTrue(label, 0);
  } else {
    DCHECK_EQ(mode, ToBooleanMode::kConvertToBoolean);
    OutputJumpIfToBooleanTrue(label, 0);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfFalse(ToBooleanMode mode,
                                                        BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  if (mode == ToBooleanMode::kAlreadyBoolean) {
    OutputJumpIfFalse(label, 0);
  } else {
    DCHECK_EQ(mode, ToBooleanMode::kConvertToBoolean);
    OutputJumpIfToBooleanFalse(label, 0);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNull(BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJumpIfNull(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotNull(
    BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJumpIfNotNull(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfUndefined(
    BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJumpIfUndefined(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotUndefined(
    BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJumpIfNotUndefined(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNil(BytecodeLabel* label,
                                                      Token::Value op,
                                                      NilValue nil) {
  if (op == Token::EQ) {
    // TODO(rmcilroy): Implement JumpIfUndetectable.
    return CompareUndetectable().JumpIfTrue(ToBooleanMode::kAlreadyBoolean,
                                            label);
  } else {
    DCHECK_EQ(Token::EQ_STRICT, op);
    if (nil == kUndefinedValue) {
      return JumpIfUndefined(label);
    } else {
      DCHECK_EQ(kNullValue, nil);
      return JumpIfNull(label);
    }
  }
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotNil(BytecodeLabel* label,
                                                         Token::Value op,
                                                         NilValue nil) {
  if (op == Token::EQ) {
    // TODO(rmcilroy): Implement JumpIfUndetectable.
    return CompareUndetectable().JumpIfFalse(ToBooleanMode::kAlreadyBoolean,
                                             label);
  } else {
    DCHECK_EQ(Token::EQ_STRICT, op);
    if (nil == kUndefinedValue) {
      return JumpIfNotUndefined(label);
    } else {
      DCHECK_EQ(kNullValue, nil);
      return JumpIfNotNull(label);
    }
  }
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotHole(
    BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJumpIfNotHole(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfJSReceiver(
    BytecodeLabel* label) {
  DCHECK(!label->is_bound());
  OutputJumpIfJSReceiver(label, 0);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpLoop(BytecodeLabel* label,
                                                     int loop_depth) {
  DCHECK(label->is_bound());
  OutputJumpLoop(label, 0, loop_depth);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::SwitchOnSmiNoFeedback(
    BytecodeJumpTable* jump_table) {
  OutputSwitchOnSmiNoFeedback(jump_table);
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
    Register generator, SuspendFlags flags) {
  OutputSuspendGenerator(generator,
                         SuspendGeneratorBytecodeFlags::Encode(flags));
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

BytecodeArrayBuilder& BytecodeArrayBuilder::CallProperty(Register callable,
                                                         RegisterList args,
                                                         int feedback_slot) {
  if (args.register_count() == 1) {
    OutputCallProperty0(callable, args[0], feedback_slot);
  } else if (args.register_count() == 2) {
    OutputCallProperty1(callable, args[0], args[1], feedback_slot);
  } else if (args.register_count() == 3) {
    OutputCallProperty2(callable, args[0], args[1], args[2], feedback_slot);
  } else {
    OutputCallProperty(callable, args, args.register_count(), feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallUndefinedReceiver(
    Register callable, RegisterList args, int feedback_slot) {
  if (args.register_count() == 0) {
    OutputCallUndefinedReceiver0(callable, feedback_slot);
  } else if (args.register_count() == 1) {
    OutputCallUndefinedReceiver1(callable, args[0], feedback_slot);
  } else if (args.register_count() == 2) {
    OutputCallUndefinedReceiver2(callable, args[0], args[1], feedback_slot);
  } else {
    OutputCallUndefinedReceiver(callable, args, args.register_count(),
                                feedback_slot);
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallAnyReceiver(Register callable,
                                                            RegisterList args,
                                                            int feedback_slot) {
  OutputCallAnyReceiver(callable, args, args.register_count(), feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::TailCall(Register callable,
                                                     RegisterList args,
                                                     int feedback_slot) {
  OutputTailCall(callable, args, args.register_count(), feedback_slot);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallWithSpread(Register callable,
                                                           RegisterList args) {
  OutputCallWithSpread(callable, args, args.register_count());
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Construct(Register constructor,
                                                      RegisterList args,
                                                      int feedback_slot_id) {
  OutputConstruct(constructor, args, args.register_count(), feedback_slot_id);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ConstructWithSpread(
    Register constructor, RegisterList args) {
  OutputConstructWithSpread(constructor, args, args.register_count());
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

size_t BytecodeArrayBuilder::GetConstantPoolEntry(
    const AstRawString* raw_string) {
  return constant_array_builder()->Insert(raw_string);
}

size_t BytecodeArrayBuilder::GetConstantPoolEntry(const AstValue* heap_number) {
  DCHECK(heap_number->IsHeapNumber());
  return constant_array_builder()->Insert(heap_number);
}

size_t BytecodeArrayBuilder::GetConstantPoolEntry(const Scope* scope) {
  return constant_array_builder()->Insert(scope);
}

#define ENTRY_GETTER(NAME, ...)                            \
  size_t BytecodeArrayBuilder::NAME##ConstantPoolEntry() { \
    return constant_array_builder()->Insert##NAME();       \
  }
SINGLETON_CONSTANT_ENTRY_TYPES(ENTRY_GETTER)
#undef ENTRY_GETTER

BytecodeJumpTable* BytecodeArrayBuilder::AllocateJumpTable(
    int size, int case_value_base) {
  DCHECK_GT(size, 0);

  size_t constant_pool_index = constant_array_builder()->InsertJumpTable(size);

  return new (zone())
      BytecodeJumpTable(constant_pool_index, size, case_value_base, zone());
}

size_t BytecodeArrayBuilder::AllocateDeferredConstantPoolEntry() {
  return constant_array_builder()->InsertDeferred();
}

void BytecodeArrayBuilder::SetDeferredConstantPoolEntry(size_t entry,
                                                        Handle<Object> object) {
  constant_array_builder()->SetDeferredAt(entry, object);
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

std::ostream& operator<<(std::ostream& os,
                         const BytecodeArrayBuilder::ToBooleanMode& mode) {
  switch (mode) {
    case BytecodeArrayBuilder::ToBooleanMode::kAlreadyBoolean:
      return os << "AlreadyBoolean";
    case BytecodeArrayBuilder::ToBooleanMode::kConvertToBoolean:
      return os << "ConvertToBoolean";
  }
  UNREACHABLE();
  return os;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
