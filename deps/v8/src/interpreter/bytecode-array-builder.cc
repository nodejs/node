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
        zone, &register_allocator_, fixed_register_count(), parameter_count,
        pipeline_);
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

Handle<BytecodeArray> BytecodeArrayBuilder::ToBytecodeArray(Isolate* isolate) {
  DCHECK(return_seen_in_block_);
  DCHECK(!bytecode_generated_);
  bytecode_generated_ = true;

  Handle<FixedArray> handler_table =
      handler_table_builder()->ToHandlerTable(isolate);
  return pipeline_->ToBytecodeArray(isolate, total_register_count(),
                                    parameter_count(), handler_table);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1, uint32_t operand2,
                                  uint32_t operand3) {
  DCHECK(OperandsAreValid(bytecode, 4, operand0, operand1, operand2, operand3));
  BytecodeNode node(bytecode, operand0, operand1, operand2, operand3,
                    &latest_source_info_);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1, uint32_t operand2) {
  DCHECK(OperandsAreValid(bytecode, 3, operand0, operand1, operand2));
  BytecodeNode node(bytecode, operand0, operand1, operand2,
                    &latest_source_info_);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0,
                                  uint32_t operand1) {
  DCHECK(OperandsAreValid(bytecode, 2, operand0, operand1));
  BytecodeNode node(bytecode, operand0, operand1, &latest_source_info_);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode, uint32_t operand0) {
  DCHECK(OperandsAreValid(bytecode, 1, operand0));
  BytecodeNode node(bytecode, operand0, &latest_source_info_);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::Output(Bytecode bytecode) {
  DCHECK(OperandsAreValid(bytecode, 0));
  BytecodeNode node(bytecode, &latest_source_info_);
  pipeline()->Write(&node);
}

void BytecodeArrayBuilder::OutputJump(Bytecode bytecode, BytecodeLabel* label) {
  BytecodeNode node(bytecode, 0, &latest_source_info_);
  pipeline_->WriteJump(&node, label);
  LeaveBasicBlock();
}

void BytecodeArrayBuilder::OutputJump(Bytecode bytecode, uint32_t operand0,
                                      BytecodeLabel* label) {
  BytecodeNode node(bytecode, 0, operand0, &latest_source_info_);
  pipeline_->WriteJump(&node, label);
  LeaveBasicBlock();
}

BytecodeArrayBuilder& BytecodeArrayBuilder::BinaryOperation(Token::Value op,
                                                            Register reg,
                                                            int feedback_slot) {
  switch (op) {
    case Token::Value::ADD:
      Output(Bytecode::kAdd, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::SUB:
      Output(Bytecode::kSub, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::MUL:
      Output(Bytecode::kMul, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::DIV:
      Output(Bytecode::kDiv, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::MOD:
      Output(Bytecode::kMod, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::BIT_OR:
      Output(Bytecode::kBitwiseOr, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::BIT_XOR:
      Output(Bytecode::kBitwiseXor, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::BIT_AND:
      Output(Bytecode::kBitwiseAnd, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::SHL:
      Output(Bytecode::kShiftLeft, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::SAR:
      Output(Bytecode::kShiftRight, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::SHR:
      Output(Bytecode::kShiftRightLogical, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    default:
      UNREACHABLE();
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CountOperation(Token::Value op,
                                                           int feedback_slot) {
  if (op == Token::Value::ADD) {
    Output(Bytecode::kInc, UnsignedOperand(feedback_slot));
  } else {
    DCHECK_EQ(op, Token::Value::SUB);
    Output(Bytecode::kDec, UnsignedOperand(feedback_slot));
  }
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

BytecodeArrayBuilder& BytecodeArrayBuilder::CompareOperation(
    Token::Value op, Register reg, int feedback_slot) {
  switch (op) {
    case Token::Value::EQ:
      Output(Bytecode::kTestEqual, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::NE:
      Output(Bytecode::kTestNotEqual, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::EQ_STRICT:
      Output(Bytecode::kTestEqualStrict, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::LT:
      Output(Bytecode::kTestLessThan, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::GT:
      Output(Bytecode::kTestGreaterThan, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::LTE:
      Output(Bytecode::kTestLessThanOrEqual, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::GTE:
      Output(Bytecode::kTestGreaterThanOrEqual, RegisterOperand(reg),
             UnsignedOperand(feedback_slot));
      break;
    case Token::Value::INSTANCEOF:
      Output(Bytecode::kTestInstanceOf, RegisterOperand(reg));
      break;
    case Token::Value::IN:
      Output(Bytecode::kTestIn, RegisterOperand(reg));
      break;
    default:
      UNREACHABLE();
  }
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
  if (typeof_mode == INSIDE_TYPEOF) {
    Output(Bytecode::kLdaGlobalInsideTypeof, feedback_slot);
  } else {
    DCHECK_EQ(typeof_mode, NOT_INSIDE_TYPEOF);
    Output(Bytecode::kLdaGlobal, UnsignedOperand(feedback_slot));
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreGlobal(
    const Handle<String> name, int feedback_slot, LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    Output(Bytecode::kStaGlobalSloppy, UnsignedOperand(name_index),
           UnsignedOperand(feedback_slot));
  } else {
    DCHECK_EQ(language_mode, STRICT);
    Output(Bytecode::kStaGlobalStrict, UnsignedOperand(name_index),
           UnsignedOperand(feedback_slot));
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadContextSlot(Register context,
                                                            int slot_index,
                                                            int depth) {
  Output(Bytecode::kLdaContextSlot, RegisterOperand(context),
         UnsignedOperand(slot_index), UnsignedOperand(depth));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreContextSlot(Register context,
                                                             int slot_index,
                                                             int depth) {
  Output(Bytecode::kStaContextSlot, RegisterOperand(context),
         UnsignedOperand(slot_index), UnsignedOperand(depth));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupSlot(
    const Handle<String> name, TypeofMode typeof_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (typeof_mode == INSIDE_TYPEOF) {
    Output(Bytecode::kLdaLookupSlotInsideTypeof, UnsignedOperand(name_index));
  } else {
    DCHECK_EQ(typeof_mode, NOT_INSIDE_TYPEOF);
    Output(Bytecode::kLdaLookupSlot, UnsignedOperand(name_index));
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupContextSlot(
    const Handle<String> name, TypeofMode typeof_mode, int slot_index,
    int depth) {
  Bytecode bytecode = (typeof_mode == INSIDE_TYPEOF)
                          ? Bytecode::kLdaLookupContextSlotInsideTypeof
                          : Bytecode::kLdaLookupContextSlot;
  size_t name_index = GetConstantPoolEntry(name);
  Output(bytecode, UnsignedOperand(name_index), UnsignedOperand(slot_index),
         UnsignedOperand(depth));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::LoadLookupGlobalSlot(
    const Handle<String> name, TypeofMode typeof_mode, int feedback_slot,
    int depth) {
  Bytecode bytecode = (typeof_mode == INSIDE_TYPEOF)
                          ? Bytecode::kLdaLookupGlobalSlotInsideTypeof
                          : Bytecode::kLdaLookupGlobalSlot;
  size_t name_index = GetConstantPoolEntry(name);
  Output(bytecode, UnsignedOperand(name_index), UnsignedOperand(feedback_slot),
         UnsignedOperand(depth));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreLookupSlot(
    const Handle<String> name, LanguageMode language_mode) {
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    Output(Bytecode::kStaLookupSlotSloppy, UnsignedOperand(name_index));
  } else {
    DCHECK_EQ(language_mode, STRICT);
    Output(Bytecode::kStaLookupSlotStrict, UnsignedOperand(name_index));
  }
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
  size_t name_index = GetConstantPoolEntry(name);
  if (language_mode == SLOPPY) {
    Output(Bytecode::kStaNamedPropertySloppy, RegisterOperand(object),
           UnsignedOperand(name_index), UnsignedOperand(feedback_slot));
  } else {
    DCHECK_EQ(language_mode, STRICT);
    Output(Bytecode::kStaNamedPropertyStrict, RegisterOperand(object),
           UnsignedOperand(name_index), UnsignedOperand(feedback_slot));
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::StoreKeyedProperty(
    Register object, Register key, int feedback_slot,
    LanguageMode language_mode) {
  if (language_mode == SLOPPY) {
    Output(Bytecode::kStaKeyedPropertySloppy, RegisterOperand(object),
           RegisterOperand(key), UnsignedOperand(feedback_slot));
  } else {
    DCHECK_EQ(language_mode, STRICT);
    Output(Bytecode::kStaKeyedPropertyStrict, RegisterOperand(object),
           RegisterOperand(key), UnsignedOperand(feedback_slot));
  }
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
    Register exception, Handle<String> name, Handle<ScopeInfo> scope_info) {
  size_t name_index = GetConstantPoolEntry(name);
  size_t scope_info_index = GetConstantPoolEntry(scope_info);
  Output(Bytecode::kCreateCatchContext, RegisterOperand(exception),
         UnsignedOperand(name_index), UnsignedOperand(scope_info_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateFunctionContext(int slots) {
  Output(Bytecode::kCreateFunctionContext, UnsignedOperand(slots));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateWithContext(
    Register object, Handle<ScopeInfo> scope_info) {
  size_t scope_info_index = GetConstantPoolEntry(scope_info);
  Output(Bytecode::kCreateWithContext, RegisterOperand(object),
         UnsignedOperand(scope_info_index));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CreateArguments(
    CreateArgumentsType type) {
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      Output(Bytecode::kCreateMappedArguments);
      break;
    case CreateArgumentsType::kUnmappedArguments:
      Output(Bytecode::kCreateUnmappedArguments);
      break;
    case CreateArgumentsType::kRestParameter:
      Output(Bytecode::kCreateRestParameter);
      break;
    default:
      UNREACHABLE();
  }
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

BytecodeArrayBuilder& BytecodeArrayBuilder::ConvertAccumulatorToObject(
    Register out) {
  Output(Bytecode::kToObject, RegisterOperand(out));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ConvertAccumulatorToName(
    Register out) {
  Output(Bytecode::kToName, RegisterOperand(out));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ConvertAccumulatorToNumber(
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

BytecodeArrayBuilder& BytecodeArrayBuilder::Jump(BytecodeLabel* label) {
  OutputJump(Bytecode::kJump, label);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfTrue(BytecodeLabel* label) {
  // The peephole optimizer attempts to simplify JumpIfToBooleanTrue
  // to JumpIfTrue.
  OutputJump(Bytecode::kJumpIfToBooleanTrue, label);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfFalse(BytecodeLabel* label) {
  OutputJump(Bytecode::kJumpIfToBooleanFalse, label);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNull(BytecodeLabel* label) {
  OutputJump(Bytecode::kJumpIfNull, label);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfUndefined(
    BytecodeLabel* label) {
  OutputJump(Bytecode::kJumpIfUndefined, label);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpIfNotHole(
    BytecodeLabel* label) {
  OutputJump(Bytecode::kJumpIfNotHole, label);
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::JumpLoop(BytecodeLabel* label,
                                                     int loop_depth) {
  OutputJump(Bytecode::kJumpLoop, UnsignedOperand(loop_depth), label);
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
  Output(Bytecode::kStackCheck);
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
    Register receiver, RegisterList cache_info_triple) {
  DCHECK_EQ(3, cache_info_triple.register_count());
  Output(Bytecode::kForInPrepare, RegisterOperand(receiver),
         RegisterOperand(cache_info_triple.first_register()));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInContinue(
    Register index, Register cache_length) {
  Output(Bytecode::kForInContinue, RegisterOperand(index),
         RegisterOperand(cache_length));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::ForInNext(
    Register receiver, Register index, RegisterList cache_type_array_pair,
    int feedback_slot) {
  DCHECK_EQ(2, cache_type_array_pair.register_count());
  Output(Bytecode::kForInNext, RegisterOperand(receiver),
         RegisterOperand(index),
         RegisterOperand(cache_type_array_pair.first_register()),
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

BytecodeArrayBuilder& BytecodeArrayBuilder::Call(Register callable,
                                                 RegisterList args,
                                                 int feedback_slot,
                                                 TailCallMode tail_call_mode) {
  if (tail_call_mode == TailCallMode::kDisallow) {
    Output(Bytecode::kCall, RegisterOperand(callable),
           RegisterOperand(args.first_register()),
           UnsignedOperand(args.register_count()),
           UnsignedOperand(feedback_slot));
  } else {
    DCHECK(tail_call_mode == TailCallMode::kAllow);
    Output(Bytecode::kTailCall, RegisterOperand(callable),
           RegisterOperand(args.first_register()),
           UnsignedOperand(args.register_count()),
           UnsignedOperand(feedback_slot));
  }
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::New(Register constructor,
                                                RegisterList args,
                                                int feedback_slot_id) {
  Output(Bytecode::kNew, RegisterOperand(constructor),
         RegisterOperand(args.first_register()),
         UnsignedOperand(args.register_count()),
         UnsignedOperand(feedback_slot_id));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntime(
    Runtime::FunctionId function_id, RegisterList args) {
  DCHECK_EQ(1, Runtime::FunctionForId(function_id)->result_size);
  DCHECK(Bytecodes::SizeForUnsignedOperand(function_id) <= OperandSize::kShort);
  Bytecode bytecode;
  uint32_t id;
  if (IntrinsicsHelper::IsSupported(function_id)) {
    bytecode = Bytecode::kInvokeIntrinsic;
    id = static_cast<uint32_t>(IntrinsicsHelper::FromRuntimeId(function_id));
  } else {
    bytecode = Bytecode::kCallRuntime;
    id = static_cast<uint32_t>(function_id);
  }
  Output(bytecode, id, RegisterOperand(args.first_register()),
         UnsignedOperand(args.register_count()));
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
  Output(Bytecode::kCallRuntimeForPair, static_cast<uint16_t>(function_id),
         RegisterOperand(args.first_register()),
         UnsignedOperand(args.register_count()),
         RegisterOperand(return_pair.first_register()));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallRuntimeForPair(
    Runtime::FunctionId function_id, Register arg, RegisterList return_pair) {
  return CallRuntimeForPair(function_id, RegisterList(arg.index(), 1),
                            return_pair);
}

BytecodeArrayBuilder& BytecodeArrayBuilder::CallJSRuntime(int context_index,
                                                          RegisterList args) {
  Output(Bytecode::kCallJSRuntime, UnsignedOperand(context_index),
         RegisterOperand(args.first_register()),
         UnsignedOperand(args.register_count()));
  return *this;
}

BytecodeArrayBuilder& BytecodeArrayBuilder::Delete(Register object,
                                                   LanguageMode language_mode) {
  if (language_mode == SLOPPY) {
    Output(Bytecode::kDeletePropertySloppy, RegisterOperand(object));
  } else {
    DCHECK_EQ(language_mode, STRICT);
    Output(Bytecode::kDeletePropertyStrict, RegisterOperand(object));
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
        // TODO(leszeks): Possibly split this up into constant pool indices and
        // other indices, for checking.
        break;
      case OperandType::kUImm:
      case OperandType::kImm:
        break;
      case OperandType::kRegList: {
        CHECK_LT(i, operand_count - 1);
        CHECK(operand_types[i + 1] == OperandType::kRegCount);
        int reg_count = static_cast<int>(operands[i + 1]);
        if (reg_count == 0) {
          return Register::FromOperand(operands[i]) == Register(0);
        } else {
          Register start = Register::FromOperand(operands[i]);
          Register end(start.index() + reg_count - 1);
          if (!RegisterIsValid(start) || !RegisterIsValid(end) || start > end) {
            return false;
          }
        }
        i++;  // Skip past kRegCount operand.
        break;
      }
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
      case OperandType::kRegCount:
        UNREACHABLE();  // Dealt with in kRegList above.
    }
  }

  return true;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
