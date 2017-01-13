// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-builder.h"

#include "src/compiler.h"
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
      temporary_allocator_(zone, fixed_register_count()),
      bytecode_array_writer_(zone, &constant_array_builder_,
                             source_position_mode),
      pipeline_(&bytecode_array_writer_) {
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
    pipeline_ = new (zone) BytecodeRegisterOptimizer(
        zone, &temporary_allocator_, parameter_count, pipeline_);
  }

  return_position_ =
      literal ? std::max(literal->start_position(), literal->end_position() - 1)
              : kNoSourcePosition;
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

bool BytecodeArrayBuilder::RegisterIsParameterOrLocal(Register reg) const {
  return reg.is_parameter() || reg.index() < locals_count();
}

Handle<BytecodeArray> BytecodeArrayBuilder::ToBytecodeArray(Isolate* isolate) {
  DCHECK(return_seen_in_block_);
  DCHECK(!bytecode_generated_);
  bytecode_generated_ = true;

  Handle<FixedArray> handler_table =
      handler_table_builder()->ToHandlerTable(isolate);
  return pipeline_->ToBytecodeArray(isolate, fixed_register_count(),
                                    parameter_count(), handler_table);
}

namespace {

static bool ExpressionPositionIsNeeded(Bytecode bytecode) {
  // An expression position is always needed if filtering is turned
  // off. Otherwise an expression is only needed if the bytecode has
  // external side effects.
  return !FLAG_ignition_filter_expression_positions ||
         !Bytecodes::IsWithoutExternalSideEffects(bytecode);
}

}  // namespace

void BytecodeArrayBuilder::AttachSourceInfo(BytecodeNode* node) {
  if (latest_source_info_.is_valid()) {
    // Statement positions need to be emitted immediately.  Expression
    // positions can be pushed back until a bytecode is found that can
    // throw. Hence we only invalidate the existing source position
    // information if it is used.
    if (latest_source_info_.is_statement() ||
        ExpressionPositionIsNeeded(node->bytecode())) {
      node->source_info().Clone(latest_source_info_);
      latest_source_info_.set_invalid();
    }
  }
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1, uint32_t operand2,
                                  uint32_t operand3) {
  DCHECK(OperandsAreValid(bytecode, 4, operand0, operand1, operand2, operand3));
  BytecodeNode node(bytecode, operand0, operand1, operand2, operand3);
  AttachSourceInfo(&node);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1, uint32_t operand2) {
  DCHECK(OperandsAreValid(bytecode, 3, operand0, operand1, operand2));
  BytecodeNode node(bytecode, operand0, operand1, operand2);
  AttachSourceInfo(&node);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1) {
  DCHECK(OperandsAreValid(bytecode, 2, operand0, operand1));
  BytecodeNode node(bytecode, operand0, operand1);
  AttachSourceInfo(&node);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0) {
  DCHECK(OperandsAreValid(bytecode, 1, operand0));
  BytecodeNode node(bytecode, operand0);
  AttachSourceInfo(&node);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode) {
  DCHECK(OperandsAreValid(bytecode, 0));
  BytecodeNode node(bytecode);
  AttachSourceInfo(&node);
  pipeline()->Write(&node);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::BinaryOperation(Token::Value op,
                                                            Register reg,
                                                            int feedback_slot) {
  Output(BytecodeForBinaryOperation(op), RegisterOperand(reg),
         UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CountOperation(Token::Value op,
                                                           int feedback_slot) {
  Output(BytecodeForCountOperation(op), UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LogicalNot() {
  Output(Bytecode::kToBooleanLogicalNot);
  return *this;
}


BytecodeArrayBuilder& BytecodeArrayBuilder::TypeOf() {
  Output(Bytecode::kTypeOf);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareOperation(Token::Value op,
                                                             Register reg) {
  Output(BytecodeForCompareOperation(op), RegisterOperand(reg));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadConstantPoolEntry(
    size_t entry) {
  Output(Bytecode::kLdaConstant, UnsignedOperand(entry));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(
    v8::internal::Smi* smi) {
  int32_t raw_smi = smi->value();
  if (raw_smi == 0) {
    Output(Bytecode::kLdaZero);
  } else {
    Output(Bytecode::kLdaSmi, SignedOperand(raw_smi));
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLiteral(Handle<Object> object) {
  size_t entry = GetConstantPoolEntry(object);
  Output(Bytecode::kLdaConstant, UnsignedOperand(entry));
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
  Output(Bytecode::kLdar, RegisterOperand(reg));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreAccumulatorInRegister(
    Register reg) {
  Output(Bytecode::kStar, RegisterOperand(reg));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::MoveRegister(Register from,
                                                         Register to) {
  DCHECK(from != to);
  Output(Bytecode::kMov, RegisterOperand(from), RegisterOperand(to));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadGlobal(int feedback_slot,
                                                       TypeofMode typeof_mode) {
  // TODO(rmcilroy): Potentially store typeof information in an
  // operand rather than having extra bytecodes.
  Bytecode bytecode = BytecodeForLoadGlobal(typeof_mode);
  Output(bytecode, UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreGlobal(
    const Handle<String> name, int feedback_slot, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreGlobal(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  Output(bytecode, UnsignedOperand(name_index), UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadContextSlot(Register context,
                                                            int slot_index) {
  Output(Bytecode::kLdaContextSlot, RegisterOperand(context),
         UnsignedOperand(slot_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreContextSlot(Register context,
                                                             int slot_index) {
  Output(Bytecode::kStaContextSlot, RegisterOperand(context),
         UnsignedOperand(slot_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupSlot(
    const Handle<String> name, TypeofMode typeof_mode) {
  Bytecode bytecode = (typeof_mode == INSIDE_TYPEOF)
                          ? Bytecode::kLdaLookupSlotInsideTypeof
                          : Bytecode::kLdaLookupSlot;
  size_t name_index = GetConstantPoolEntry(name);
  Output(bytecode, UnsignedOperand(name_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreLookupSlot(
    const Handle<String> name, LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreLookupSlot(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  Output(bytecode, UnsignedOperand(name_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadNamedProperty(
    Register object, const Handle<Name> name, int feedback_slot) {
  size_t name_index = GetConstantPoolEntry(name);
  Output(Bytecode::kLdaNamedProperty, RegisterOperand(object),
         UnsignedOperand(name_index), UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadKeyedProperty(
    Register object, int feedback_slot) {
  Output(Bytecode::kLdaKeyedProperty, RegisterOperand(object),
         UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreNamedProperty(
    Register object, const Handle<Name> name, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreNamedProperty(language_mode);
  size_t name_index = GetConstantPoolEntry(name);
  Output(bytecode, RegisterOperand(object), UnsignedOperand(name_index),
         UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreKeyedProperty(
    Register object, Register key, int feedback_slot,
    LanguageMode language_mode) {
  Bytecode bytecode = BytecodeForStoreKeyedProperty(language_mode);
  Output(bytecode, RegisterOperand(object), RegisterOperand(key),
         UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateClosure(size_t entry,
                                                          int flags) {
  Output(Bytecode::kCreateClosure, UnsignedOperand(entry),
         UnsignedOperand(flags));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateBlockContext(
    Handle<ScopeInfo> scope_info) {
  size_t entry = GetConstantPoolEntry(scope_info);
  Output(Bytecode::kCreateBlockContext, UnsignedOperand(entry));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateCatchContext(
    Register exception, Handle<String> name) {
  size_t name_index = GetConstantPoolEntry(name);
  Output(Bytecode::kCreateCatchContext, RegisterOperand(exception),
         UnsignedOperand(name_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateFunctionContext(int slots) {
  Output(Bytecode::kCreateFunctionContext, UnsignedOperand(slots));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateWithContext(Register object) {
    Output(Bytecode::kCreateWithContext, RegisterOperand(object));
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
  Output(Bytecode::kCreateRegExpLiteral, UnsignedOperand(pattern_entry),
         UnsignedOperand(literal_index), UnsignedOperand(flags));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArrayLiteral(
    Handle<FixedArray> constant_elements, int literal_index, int flags) {
  size_t constant_elements_entry = GetConstantPoolEntry(constant_elements);
  Output(Bytecode::kCreateArrayLiteral,
         UnsignedOperand(constant_elements_entry),
         UnsignedOperand(literal_index), UnsignedOperand(flags));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateObjectLiteral(
    Handle<FixedArray> constant_properties, int literal_index, int flags,
    Register output) {
  size_t constant_properties_entry = GetConstantPoolEntry(constant_properties);
  Output(Bytecode::kCreateObjectLiteral,
         UnsignedOperand(constant_properties_entry),
         UnsignedOperand(literal_index), UnsignedOperand(flags),
         RegisterOperand(output));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::PushContext(Register context) {
  Output(Bytecode::kPushContext, RegisterOperand(context));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::PopContext(Register context) {
  Output(Bytecode::kPopContext, RegisterOperand(context));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToJSObject(
    Register out) {
  Output(Bytecode::kToObject, RegisterOperand(out));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToName(
    Register out) {
  Output(Bytecode::kToName, RegisterOperand(out));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CastAccumulatorToNumber(
    Register out) {
  Output(Bytecode::kToNumber, RegisterOperand(out));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Bind(BytecodeLabel* label) {
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

BytecodeArrayBuilder& BytecodeArrayBuilder::OutputJump(Bytecode jump_bytecode,
                                                       BytecodeLabel* label) {
  BytecodeNode node(jump_bytecode, 0);
  AttachSourceInfo(&node);
  pipeline_->WriteJump(&node, label);
  LeaveBasicBlock();
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Jump(BytecodeLabel* label) {
  return OutputJump(Bytecode::kJump, label);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfTrue(BytecodeLabel* label) {
  // The peephole optimizer attempts to simplify JumpIfToBooleanTrue
  // to JumpIfTrue.
  return OutputJump(Bytecode::kJumpIfToBooleanTrue, label);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfFalse(BytecodeLabel* label) {
  // The peephole optimizer attempts to simplify JumpIfToBooleanFalse
  // to JumpIfFalse.
  return OutputJump(Bytecode::kJumpIfToBooleanFalse, label);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNull(BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfNull, label);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfUndefined(
    BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfUndefined, label);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotHole(
    BytecodeLabel* label) {
  return OutputJump(Bytecode::kJumpIfNotHole, label);
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
  Output(Bytecode::kStackCheck);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::OsrPoll(int loop_depth) {
  Output(Bytecode::kOsrPoll, UnsignedOperand(loop_depth));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Throw() {
  Output(Bytecode::kThrow);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ReThrow() {
  Output(Bytecode::kReThrow);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Return() {
  SetReturnPosition();
  Output(Bytecode::kReturn);
  return_seen_in_block_ = true;
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Debugger() {
  Output(Bytecode::kDebugger);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInPrepare(
    Register receiver, Register cache_info_triple) {
  Output(Bytecode::kForInPrepare, RegisterOperand(receiver),
         RegisterOperand(cache_info_triple));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInDone(Register index,
                                                      Register cache_length) {
  Output(Bytecode::kForInDone, RegisterOperand(index),
         RegisterOperand(cache_length));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInNext(
    Register receiver, Register index, Register cache_type_array_pair,
    int feedback_slot) {
  Output(Bytecode::kForInNext, RegisterOperand(receiver),
         RegisterOperand(index), RegisterOperand(cache_type_array_pair),
         UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInStep(Register index) {
  Output(Bytecode::kForInStep, RegisterOperand(index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::SuspendGenerator(
    Register generator) {
  Output(Bytecode::kSuspendGenerator, RegisterOperand(generator));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ResumeGenerator(
    Register generator) {
  Output(Bytecode::kResumeGenerator, RegisterOperand(generator));
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

void BytecodeArrayBuilder::EnsureReturn() {
  if (!return_seen_in_block_) {
    LoadUndefined();
    Return();
  }
  DCHECK(return_seen_in_block_);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Call(Register callable,
                                                 Register receiver_args,
                                                 size_t receiver_args_count,
                                                 int feedback_slot,
                                                 TailCallMode tail_call_mode) {
  Bytecode bytecode = BytecodeForCall(tail_call_mode);
  Output(bytecode, RegisterOperand(callable), RegisterOperand(receiver_args),
         UnsignedOperand(receiver_args_count), UnsignedOperand(feedback_slot));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::New(Register constructor,
                                                Register first_arg,
                                                size_t arg_count) {
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  Output(Bytecode::kNew, RegisterOperand(constructor),
         RegisterOperand(first_arg), UnsignedOperand(arg_count));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id, Register first_arg, size_t arg_count) {
  DCHECK_EQ(1, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(Bytecodes::SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  Bytecode bytecode;
  uint32_t id;
  if (IntrinsicsHelper::IsSupported(function_id)) {
    bytecode = Bytecode::kInvokeIntrinsic;
    id = static_cast<uint32_t>(IntrinsicsHelper::FromRuntimeId(function_id));
  } else {
    bytecode = Bytecode::kCallRuntime;
    id = static_cast<uint32_t>(function_id);
  }
  Output(bytecode, id, RegisterOperand(first_arg), UnsignedOperand(arg_count));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntimeForPair(
    Runtime::FunctionId function_id, Register first_arg, size_t arg_count,
    Register first_return) {
  DCHECK_EQ(2, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(Bytecodes::SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  if (!first_arg.is_valid()) {
    DCHECK_EQ(0u, arg_count);
    first_arg = Register(0);
  }
  Output(Bytecode::kCallRuntimeForPair, static_cast<uint16_t>(function_id),
         RegisterOperand(first_arg), UnsignedOperand(arg_count),
         RegisterOperand(first_return));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallJSRuntime(
    int context_index, Register receiver_args, size_t receiver_args_count) {
  Output(Bytecode::kCallJSRuntime, UnsignedOperand(context_index),
         RegisterOperand(receiver_args), UnsignedOperand(receiver_args_count));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Delete(Register object,
                                                   LanguageMode language_mode) {
  Output(BytecodeForDelete(language_mode), RegisterOperand(object));
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

void BytecodeArrayBuilder::SetStatementPosition(Statement* stmt) {
  if (stmt->position() == kNoSourcePosition) return;
  latest_source_info_.MakeStatementPosition(stmt->position());
}

void BytecodeArrayBuilder::SetExpressionPosition(Expression* expr) {
  if (expr->position() == kNoSourcePosition) return;
  if (!latest_source_info_.is_statement()) {
    // Ensure the current expression position is overwritten with the
    // latest value.
    latest_source_info_.MakeExpressionPosition(expr->position());
  }
}

void BytecodeArrayBuilder::SetExpressionAsStatementPosition(Expression* expr) {
  if (expr->position() == kNoSourcePosition) return;
  latest_source_info_.MakeStatementPosition(expr->position());
}

bool BytecodeArrayBuilder::TemporaryRegisterIsLive(Register reg) const {
  return temporary_register_allocator()->RegisterIsLive(reg);
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
    return TemporaryRegisterIsLive(reg);
  }
}

bool BytecodeArrayBuilder::OperandsAreValid(
    Bytecode bytecode, int operand_count, uint32_t operand0, uint32_t operand1,
    uint32_t operand2, uint32_t operand3) const {
  if (Bytecodes::NumberOfOperands(bytecode) != operand_count) {
    return false;
  }

  uint32_t operands[] = {operand0, operand1, operand2, operand3};
  const OperandType* operand_types = Bytecodes::GetOperandTypes(bytecode);
  for (int i = 0; i < operand_count; ++i) {
    switch (operand_types[i]) {
      case OperandType::kNone:
        return false;
      case OperandType::kRegCount: {
        CHECK_NE(i, 0);
        CHECK(operand_types[i - 1] == OperandType::kMaybeReg ||
              operand_types[i - 1] == OperandType::kReg);
        if (i > 0 && operands[i] > 0) {
          Register start = Register::FromOperand(operands[i - 1]);
          Register end(start.index() + static_cast<int>(operands[i]) - 1);
          if (!RegisterIsValid(start) || !RegisterIsValid(end) || start > end) {
            return false;
          }
        }
        break;
      }
      case OperandType::kFlag8:
      case OperandType::kIntrinsicId:
        if (Bytecodes::SizeForUnsignedOperand(operands[i]) >
            OperandSize::kByte) {
          return false;
        }
        break;
      case OperandType::kRuntimeId:
        if (Bytecodes::SizeForUnsignedOperand(operands[i]) >
            OperandSize::kShort) {
          return false;
        }
        break;
      case OperandType::kIdx:
        // TODO(oth): Consider splitting OperandType::kIdx into two
        // operand types. One which is a constant pool index that can
        // be checked, and the other is an unsigned value.
        break;
      case OperandType::kImm:
        break;
      case OperandType::kMaybeReg:
        if (Register::FromOperand(operands[i]) == Register(0)) {
          break;
        }
      // Fall-through to kReg case.
      case OperandType::kReg:
      case OperandType::kRegOut: {
        Register reg = Register::FromOperand(operands[i]);
        if (!RegisterIsValid(reg)) {
          return false;
        }
        break;
      }
      case OperandType::kRegOutPair:
      case OperandType::kRegPair: {
        Register reg0 = Register::FromOperand(operands[i]);
        Register reg1 = Register(reg0.index() + 1);
        if (!RegisterIsValid(reg0) || !RegisterIsValid(reg1)) {
          return false;
        }
        break;
      }
      case OperandType::kRegOutTriple: {
        Register reg0 = Register::FromOperand(operands[i]);
        Register reg1 = Register(reg0.index() + 1);
        Register reg2 = Register(reg0.index() + 2);
        if (!RegisterIsValid(reg0) || !RegisterIsValid(reg1) ||
            !RegisterIsValid(reg2)) {
          return false;
        }
        break;
      }
    }
  }

  return true;
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
Bytecode BytecodeArrayBuilder::BytecodeForStoreNamedProperty(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStaNamedPropertySloppy;
    case STRICT:
      return Bytecode::kStaNamedPropertyStrict;
    default:
      UNREACHABLE();
  }
  return Bytecode::kIllegal;
}

// static
Bytecode BytecodeArrayBuilder::BytecodeForStoreKeyedProperty(
    LanguageMode language_mode) {
  switch (language_mode) {
    case SLOPPY:
      return Bytecode::kStaKeyedPropertySloppy;
    case STRICT:
      return Bytecode::kStaKeyedPropertyStrict;
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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
