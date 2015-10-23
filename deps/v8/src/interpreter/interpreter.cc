// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter.h"

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
  info->EnsureFeedbackVector();
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


// LdaUndefined
//
// Load Undefined into the accumulator.
void Interpreter::DoLdaUndefined(compiler::InterpreterAssembler* assembler) {
  Node* undefined_value = __ HeapConstant(Unique<HeapObject>::CreateImmovable(
      isolate_->factory()->undefined_value()));
  __ SetAccumulator(undefined_value);
  __ Dispatch();
}


// LdaNull
//
// Load Null into the accumulator.
void Interpreter::DoLdaNull(compiler::InterpreterAssembler* assembler) {
  Node* null_value = __ HeapConstant(
      Unique<HeapObject>::CreateImmovable(isolate_->factory()->null_value()));
  __ SetAccumulator(null_value);
  __ Dispatch();
}


// LdaTheHole
//
// Load TheHole into the accumulator.
void Interpreter::DoLdaTheHole(compiler::InterpreterAssembler* assembler) {
  Node* the_hole_value = __ HeapConstant(Unique<HeapObject>::CreateImmovable(
      isolate_->factory()->the_hole_value()));
  __ SetAccumulator(the_hole_value);
  __ Dispatch();
}


// LdaTrue
//
// Load True into the accumulator.
void Interpreter::DoLdaTrue(compiler::InterpreterAssembler* assembler) {
  Node* true_value = __ HeapConstant(
      Unique<HeapObject>::CreateImmovable(isolate_->factory()->true_value()));
  __ SetAccumulator(true_value);
  __ Dispatch();
}


// LdaFalse
//
// Load False into the accumulator.
void Interpreter::DoLdaFalse(compiler::InterpreterAssembler* assembler) {
  Node* false_value = __ HeapConstant(
      Unique<HeapObject>::CreateImmovable(isolate_->factory()->false_value()));
  __ SetAccumulator(false_value);
  __ Dispatch();
}


// Ldar <src>
//
// Load accumulator with value from register <src>.
void Interpreter::DoLdar(compiler::InterpreterAssembler* assembler) {
  Node* value = __ LoadRegister(__ BytecodeOperandReg(0));
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


// Add <src>
//
// Add register <src> to accumulator.
void Interpreter::DoAdd(compiler::InterpreterAssembler* assembler) {
  // TODO(rmcilroy) Implement.
  __ Dispatch();
}


// Sub <src>
//
// Subtract register <src> from accumulator.
void Interpreter::DoSub(compiler::InterpreterAssembler* assembler) {
  // TODO(rmcilroy) Implement.
  __ Dispatch();
}


// Mul <src>
//
// Multiply accumulator by register <src>.
void Interpreter::DoMul(compiler::InterpreterAssembler* assembler) {
  // TODO(rmcilroy) Implement add register to accumulator.
  __ Dispatch();
}


// Div <src>
//
// Divide register <src> by accumulator.
void Interpreter::DoDiv(compiler::InterpreterAssembler* assembler) {
  // TODO(rmcilroy) Implement.
  __ Dispatch();
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
