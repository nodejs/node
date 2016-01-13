// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter.h"

#include "src/code-factory.h"
#include "src/compiler.h"
#include "src/compiler/interpreter-assembler.h"
#include "src/factory.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/bytecodes.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;
#define __ assembler->


Interpreter::Interpreter(Isolate* isolate)
    : isolate_(isolate) {}


// static
Handle<FixedArray> Interpreter::CreateUninitializedInterpreterTable(
    Isolate* isolate) {
  Handle<FixedArray> handler_table = isolate->factory()->NewFixedArray(
      static_cast<int>(Bytecode::kLast) + 1, TENURED);
  // We rely on the interpreter handler table being immovable, so check that
  // it was allocated on the first page (which is always immovable).
  DCHECK(isolate->heap()->old_space()->FirstPage()->Contains(
      handler_table->address()));
  return handler_table;
}


void Interpreter::Initialize() {
  DCHECK(FLAG_ignition);
  Handle<FixedArray> handler_table = isolate_->factory()->interpreter_table();
  if (!IsInterpreterTableInitialized(handler_table)) {
    Zone zone;
    HandleScope scope(isolate_);

#define GENERATE_CODE(Name, ...)                                      \
    {                                                                 \
      compiler::InterpreterAssembler assembler(isolate_, &zone,       \
                                               Bytecode::k##Name);    \
      Do##Name(&assembler);                                           \
      Handle<Code> code = assembler.GenerateCode();                   \
      handler_table->set(static_cast<int>(Bytecode::k##Name), *code); \
    }
    BYTECODE_LIST(GENERATE_CODE)
#undef GENERATE_CODE
  }
}


bool Interpreter::MakeBytecode(CompilationInfo* info) {
  Handle<SharedFunctionInfo> shared_info = info->shared_info();

  BytecodeGenerator generator(info->isolate(), info->zone());
  info->EnsureFeedbackVector();
  Handle<BytecodeArray> bytecodes = generator.MakeBytecode(info);
  if (FLAG_print_bytecode) {
    bytecodes->Print();
  }

  DCHECK(shared_info->function_data()->IsUndefined());
  if (!shared_info->function_data()->IsUndefined()) {
    return false;
  }

  shared_info->set_function_data(*bytecodes);
  info->SetCode(info->isolate()->builtins()->InterpreterEntryTrampoline());
  return true;
}


bool Interpreter::IsInterpreterTableInitialized(
    Handle<FixedArray> handler_table) {
  DCHECK(handler_table->length() == static_cast<int>(Bytecode::kLast) + 1);
  return handler_table->get(0) != isolate_->heap()->undefined_value();
}


// LdaZero
//
// Load literal '0' into the accumulator.
void Interpreter::DoLdaZero(compiler::InterpreterAssembler* assembler) {
  Node* zero_value = __ NumberConstant(0.0);
  __ SetAccumulator(zero_value);
  __ Dispatch();
}


// LdaSmi8 <imm8>
//
// Load an 8-bit integer literal into the accumulator as a Smi.
void Interpreter::DoLdaSmi8(compiler::InterpreterAssembler* assembler) {
  Node* raw_int = __ BytecodeOperandImm8(0);
  Node* smi_int = __ SmiTag(raw_int);
  __ SetAccumulator(smi_int);
  __ Dispatch();
}


// LdaConstant <idx>
//
// Load constant literal at |idx| in the constant pool into the accumulator.
void Interpreter::DoLdaConstant(compiler::InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  __ SetAccumulator(constant);
  __ Dispatch();
}


// LdaUndefined
//
// Load Undefined into the accumulator.
void Interpreter::DoLdaUndefined(compiler::InterpreterAssembler* assembler) {
  Node* undefined_value =
      __ HeapConstant(isolate_->factory()->undefined_value());
  __ SetAccumulator(undefined_value);
  __ Dispatch();
}


// LdaNull
//
// Load Null into the accumulator.
void Interpreter::DoLdaNull(compiler::InterpreterAssembler* assembler) {
  Node* null_value = __ HeapConstant(isolate_->factory()->null_value());
  __ SetAccumulator(null_value);
  __ Dispatch();
}


// LdaTheHole
//
// Load TheHole into the accumulator.
void Interpreter::DoLdaTheHole(compiler::InterpreterAssembler* assembler) {
  Node* the_hole_value = __ HeapConstant(isolate_->factory()->the_hole_value());
  __ SetAccumulator(the_hole_value);
  __ Dispatch();
}


// LdaTrue
//
// Load True into the accumulator.
void Interpreter::DoLdaTrue(compiler::InterpreterAssembler* assembler) {
  Node* true_value = __ HeapConstant(isolate_->factory()->true_value());
  __ SetAccumulator(true_value);
  __ Dispatch();
}


// LdaFalse
//
// Load False into the accumulator.
void Interpreter::DoLdaFalse(compiler::InterpreterAssembler* assembler) {
  Node* false_value = __ HeapConstant(isolate_->factory()->false_value());
  __ SetAccumulator(false_value);
  __ Dispatch();
}


// Ldar <src>
//
// Load accumulator with value from register <src>.
void Interpreter::DoLdar(compiler::InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* value = __ LoadRegister(reg_index);
  __ SetAccumulator(value);
  __ Dispatch();
}


// Star <dst>
//
// Store accumulator to register <dst>.
void Interpreter::DoStar(compiler::InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* accumulator = __ GetAccumulator();
  __ StoreRegister(accumulator, reg_index);
  __ Dispatch();
}


// LdaGlobal <slot_index>
//
// Load the global at |slot_index| into the accumulator.
void Interpreter::DoLdaGlobal(compiler::InterpreterAssembler* assembler) {
  Node* slot_index = __ BytecodeOperandIdx(0);
  Node* smi_slot_index = __ SmiTag(slot_index);
  Node* result = __ CallRuntime(Runtime::kLoadGlobalViaContext, smi_slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}


void Interpreter::DoPropertyLoadIC(Callable ic,
                                   compiler::InterpreterAssembler* assembler) {
  Node* code_target = __ HeapConstant(ic.code());
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(reg_index);
  Node* name = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(1);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* result = __ CallIC(ic.descriptor(), code_target, object, name, smi_slot,
                           type_feedback_vector);
  __ SetAccumulator(result);
  __ Dispatch();
}


// LoadIC <object> <slot>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name
// in the accumulator.
void Interpreter::DoLoadIC(compiler::InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF,
                                                   SLOPPY, UNINITIALIZED);
  DoPropertyLoadIC(ic, assembler);
}


// KeyedLoadIC <object> <slot>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator.
void Interpreter::DoKeyedLoadIC(compiler::InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedLoadICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoPropertyLoadIC(ic, assembler);
}


void Interpreter::DoPropertyStoreIC(Callable ic,
                                    compiler::InterpreterAssembler* assembler) {
  Node* code_target = __ HeapConstant(ic.code());
  Node* object_reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(object_reg_index);
  Node* name_reg_index = __ BytecodeOperandReg(1);
  Node* name = __ LoadRegister(name_reg_index);
  Node* value = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(2);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* result = __ CallIC(ic.descriptor(), code_target, object, name, value,
                           smi_slot, type_feedback_vector);
  __ SetAccumulator(result);
  __ Dispatch();
}


// StoreIC <object> <name> <slot>
//
// Calls the StoreIC at FeedBackVector slot <slot> for <object> and the name
// <name> with the value in the accumulator.
void Interpreter::DoStoreIC(compiler::InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoPropertyStoreIC(ic, assembler);
}


// KeyedStoreIC <object> <key> <slot>
//
// Calls the KeyStoreIC at FeedBackVector slot <slot> for <object> and the key
// <key> with the value in the accumulator.
void Interpreter::DoKeyedStoreIC(compiler::InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedStoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoPropertyStoreIC(ic, assembler);
}


void Interpreter::DoBinaryOp(Runtime::FunctionId function_id,
                             compiler::InterpreterAssembler* assembler) {
  // TODO(rmcilroy): Call ICs which back-patch bytecode with type specialized
  // operations, instead of calling builtins directly.
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* lhs = __ LoadRegister(reg_index);
  Node* rhs = __ GetAccumulator();
  Node* result = __ CallRuntime(function_id, lhs, rhs);
  __ SetAccumulator(result);
  __ Dispatch();
}


// Add <src>
//
// Add register <src> to accumulator.
void Interpreter::DoAdd(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kAdd, assembler);
}


// Sub <src>
//
// Subtract register <src> from accumulator.
void Interpreter::DoSub(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kSubtract, assembler);
}


// Mul <src>
//
// Multiply accumulator by register <src>.
void Interpreter::DoMul(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kMultiply, assembler);
}


// Div <src>
//
// Divide register <src> by accumulator.
void Interpreter::DoDiv(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kDivide, assembler);
}


// Mod <src>
//
// Modulo register <src> by accumulator.
void Interpreter::DoMod(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kModulus, assembler);
}


// Call <receiver> <arg_count>
//
// Call a JS function with receiver and |arg_count| arguments in subsequent
// registers. The JSfunction or Callable to call is in the accumulator.
void Interpreter::DoCall(compiler::InterpreterAssembler* assembler) {
  Node* function_reg = __ BytecodeOperandReg(0);
  Node* function = __ LoadRegister(function_reg);
  Node* receiver_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(receiver_reg);
  Node* args_count = __ BytecodeOperandCount(2);
  Node* result = __ CallJS(function, first_arg, args_count);
  __ SetAccumulator(result);
  __ Dispatch();
}


// TestEqual <src>
//
// Test if the value in the <src> register equals the accumulator.
void Interpreter::DoTestEqual(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterEquals, assembler);
}


// TestNotEqual <src>
//
// Test if the value in the <src> register is not equal to the accumulator.
void Interpreter::DoTestNotEqual(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterNotEquals, assembler);
}


// TestEqualStrict <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
void Interpreter::DoTestEqualStrict(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterStrictEquals, assembler);
}


// TestNotEqualStrict <src>
//
// Test if the value in the <src> register is not strictly equal to the
// accumulator.
void Interpreter::DoTestNotEqualStrict(
    compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterStrictNotEquals, assembler);
}


// TestLessThan <src>
//
// Test if the value in the <src> register is less than the accumulator.
void Interpreter::DoTestLessThan(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterLessThan, assembler);
}


// TestGreaterThan <src>
//
// Test if the value in the <src> register is greater than the accumulator.
void Interpreter::DoTestGreaterThan(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterGreaterThan, assembler);
}


// TestLessThanOrEqual <src>
//
// Test if the value in the <src> register is less than or equal to the
// accumulator.
void Interpreter::DoTestLessThanOrEqual(
    compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterLessThanOrEqual, assembler);
}


// TestGreaterThanOrEqual <src>
//
// Test if the value in the <src> register is greater than or equal to the
// accumulator.
void Interpreter::DoTestGreaterThanOrEqual(
    compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInterpreterGreaterThanOrEqual, assembler);
}


// TestIn <src>
//
// Test if the object referenced by the register operand is a property of the
// object referenced by the accumulator.
void Interpreter::DoTestIn(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kHasProperty, assembler);
}


// TestInstanceOf <src>
//
// Test if the object referenced by the <src> register is an an instance of type
// referenced by the accumulator.
void Interpreter::DoTestInstanceOf(compiler::InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInstanceOf, assembler);
}


// ToBoolean
//
// Cast the object referenced by the accumulator to a boolean.
void Interpreter::DoToBoolean(compiler::InterpreterAssembler* assembler) {
  // TODO(oth): The next CL for test operations has interpreter specific
  // runtime calls. This looks like another candidate.
  __ Dispatch();
}


// Jump <imm8>
//
// Jump by number of bytes represented by an immediate operand.
void Interpreter::DoJump(compiler::InterpreterAssembler* assembler) {
  Node* relative_jump = __ BytecodeOperandImm8(0);
  __ Jump(relative_jump);
}


// JumpConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool.
void Interpreter::DoJumpConstant(compiler::InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  __ Jump(relative_jump);
}


// JumpIfTrue <imm8>
//
// Jump by number of bytes represented by an immediate operand if the
// accumulator contains true.
void Interpreter::DoJumpIfTrue(compiler::InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm8(0);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(accumulator, true_value, relative_jump);
}


// JumpIfTrueConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the accumulator contains true.
void Interpreter::DoJumpIfTrueConstant(
    compiler::InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(accumulator, true_value, relative_jump);
}


// JumpIfFalse <imm8>
//
// Jump by number of bytes represented by an immediate operand if the
// accumulator contains false.
void Interpreter::DoJumpIfFalse(compiler::InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm8(0);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(accumulator, false_value, relative_jump);
}


// JumpIfFalseConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the accumulator contains false.
void Interpreter::DoJumpIfFalseConstant(
    compiler::InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(accumulator, false_value, relative_jump);
}


// Return
//
// Return the value in register 0.
void Interpreter::DoReturn(compiler::InterpreterAssembler* assembler) {
  __ Return();
}


}  // namespace interpreter
}  // namespace internal
}  // namespace v8
