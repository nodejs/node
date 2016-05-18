// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter.h"

#include "src/ast/prettyprinter.h"
#include "src/code-factory.h"
#include "src/compiler.h"
#include "src/factory.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;

#define __ assembler->

Interpreter::Interpreter(Isolate* isolate) : isolate_(isolate) {
  memset(&dispatch_table_, 0, sizeof(dispatch_table_));
}

void Interpreter::Initialize() {
  DCHECK(FLAG_ignition);
  if (IsDispatchTableInitialized()) return;
  Zone zone;
  HandleScope scope(isolate_);

#define GENERATE_CODE(Name, ...)                                        \
  {                                                                     \
    InterpreterAssembler assembler(isolate_, &zone, Bytecode::k##Name); \
    Do##Name(&assembler);                                               \
    Handle<Code> code = assembler.GenerateCode();                       \
    TraceCodegen(code, #Name);                                          \
    dispatch_table_[Bytecodes::ToByte(Bytecode::k##Name)] = *code;      \
  }
  BYTECODE_LIST(GENERATE_CODE)
#undef GENERATE_CODE
}

Code* Interpreter::GetBytecodeHandler(Bytecode bytecode) {
  DCHECK(IsDispatchTableInitialized());
  return dispatch_table_[Bytecodes::ToByte(bytecode)];
}

void Interpreter::IterateDispatchTable(ObjectVisitor* v) {
  v->VisitPointers(
      reinterpret_cast<Object**>(&dispatch_table_[0]),
      reinterpret_cast<Object**>(&dispatch_table_[0] + kDispatchTableSize));
}

// static
int Interpreter::InterruptBudget() {
  // TODO(ignition): Tune code size multiplier.
  const int kCodeSizeMultiplier = 32;
  return FLAG_interrupt_budget * kCodeSizeMultiplier;
}

bool Interpreter::MakeBytecode(CompilationInfo* info) {
  if (FLAG_print_bytecode || FLAG_print_source || FLAG_print_ast) {
    OFStream os(stdout);
    base::SmartArrayPointer<char> name = info->GetDebugName();
    os << "[generating bytecode for function: " << info->GetDebugName().get()
       << "]" << std::endl
       << std::flush;
  }

#ifdef DEBUG
  if (info->parse_info() && FLAG_print_source) {
    OFStream os(stdout);
    os << "--- Source from AST ---" << std::endl
       << PrettyPrinter(info->isolate()).PrintProgram(info->literal())
       << std::endl
       << std::flush;
  }

  if (info->parse_info() && FLAG_print_ast) {
    OFStream os(stdout);
    os << "--- AST ---" << std::endl
       << AstPrinter(info->isolate()).PrintProgram(info->literal()) << std::endl
       << std::flush;
  }
#endif  // DEBUG

  BytecodeGenerator generator(info->isolate(), info->zone());
  info->EnsureFeedbackVector();
  Handle<BytecodeArray> bytecodes = generator.MakeBytecode(info);
  if (FLAG_print_bytecode) {
    OFStream os(stdout);
    bytecodes->Print(os);
    os << std::flush;
  }

  info->SetBytecodeArray(bytecodes);
  info->SetCode(info->isolate()->builtins()->InterpreterEntryTrampoline());
  return true;
}

bool Interpreter::IsDispatchTableInitialized() {
  if (FLAG_trace_ignition) {
    // Regenerate table to add bytecode tracing operations.
    return false;
  }
  return dispatch_table_[0] != nullptr;
}

void Interpreter::TraceCodegen(Handle<Code> code, const char* name) {
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    OFStream os(stdout);
    code->Disassemble(name, os);
    os << std::flush;
  }
#endif  // ENABLE_DISASSEMBLER
}

// LdaZero
//
// Load literal '0' into the accumulator.
void Interpreter::DoLdaZero(InterpreterAssembler* assembler) {
  Node* zero_value = __ NumberConstant(0.0);
  __ SetAccumulator(zero_value);
  __ Dispatch();
}


// LdaSmi8 <imm8>
//
// Load an 8-bit integer literal into the accumulator as a Smi.
void Interpreter::DoLdaSmi8(InterpreterAssembler* assembler) {
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* smi_int = __ SmiTag(raw_int);
  __ SetAccumulator(smi_int);
  __ Dispatch();
}

void Interpreter::DoLoadConstant(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  __ SetAccumulator(constant);
  __ Dispatch();
}


// LdaConstant <idx>
//
// Load constant literal at |idx| in the constant pool into the accumulator.
void Interpreter::DoLdaConstant(InterpreterAssembler* assembler) {
  DoLoadConstant(assembler);
}


// LdaConstantWide <idx>
//
// Load constant literal at |idx| in the constant pool into the accumulator.
void Interpreter::DoLdaConstantWide(InterpreterAssembler* assembler) {
  DoLoadConstant(assembler);
}


// LdaUndefined
//
// Load Undefined into the accumulator.
void Interpreter::DoLdaUndefined(InterpreterAssembler* assembler) {
  Node* undefined_value =
      __ HeapConstant(isolate_->factory()->undefined_value());
  __ SetAccumulator(undefined_value);
  __ Dispatch();
}


// LdaNull
//
// Load Null into the accumulator.
void Interpreter::DoLdaNull(InterpreterAssembler* assembler) {
  Node* null_value = __ HeapConstant(isolate_->factory()->null_value());
  __ SetAccumulator(null_value);
  __ Dispatch();
}


// LdaTheHole
//
// Load TheHole into the accumulator.
void Interpreter::DoLdaTheHole(InterpreterAssembler* assembler) {
  Node* the_hole_value = __ HeapConstant(isolate_->factory()->the_hole_value());
  __ SetAccumulator(the_hole_value);
  __ Dispatch();
}


// LdaTrue
//
// Load True into the accumulator.
void Interpreter::DoLdaTrue(InterpreterAssembler* assembler) {
  Node* true_value = __ HeapConstant(isolate_->factory()->true_value());
  __ SetAccumulator(true_value);
  __ Dispatch();
}


// LdaFalse
//
// Load False into the accumulator.
void Interpreter::DoLdaFalse(InterpreterAssembler* assembler) {
  Node* false_value = __ HeapConstant(isolate_->factory()->false_value());
  __ SetAccumulator(false_value);
  __ Dispatch();
}


// Ldar <src>
//
// Load accumulator with value from register <src>.
void Interpreter::DoLdar(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* value = __ LoadRegister(reg_index);
  __ SetAccumulator(value);
  __ Dispatch();
}


// Star <dst>
//
// Store accumulator to register <dst>.
void Interpreter::DoStar(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* accumulator = __ GetAccumulator();
  __ StoreRegister(accumulator, reg_index);
  __ Dispatch();
}


// Mov <src> <dst>
//
// Stores the value of register <src> to register <dst>.
void Interpreter::DoMov(InterpreterAssembler* assembler) {
  Node* src_index = __ BytecodeOperandReg(0);
  Node* src_value = __ LoadRegister(src_index);
  Node* dst_index = __ BytecodeOperandReg(1);
  __ StoreRegister(src_value, dst_index);
  __ Dispatch();
}


// MovWide <src> <dst>
//
// Stores the value of register <src> to register <dst>.
void Interpreter::DoMovWide(InterpreterAssembler* assembler) {
  DoMov(assembler);
}

void Interpreter::DoLoadGlobal(Callable ic, InterpreterAssembler* assembler) {
  // Get the global object.
  Node* context = __ GetContext();
  Node* native_context =
      __ LoadContextSlot(context, Context::NATIVE_CONTEXT_INDEX);
  Node* global = __ LoadContextSlot(native_context, Context::EXTENSION_INDEX);

  // Load the global via the LoadIC.
  Node* code_target = __ HeapConstant(ic.code());
  Node* constant_index = __ BytecodeOperandIdx(0);
  Node* name = __ LoadConstantPoolEntry(constant_index);
  Node* raw_slot = __ BytecodeOperandIdx(1);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* result = __ CallStub(ic.descriptor(), code_target, context, global,
                             name, smi_slot, type_feedback_vector);
  __ SetAccumulator(result);
  __ Dispatch();
}

// LdaGlobal <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> outside of a typeof.
void Interpreter::DoLdaGlobal(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF,
                                                   UNINITIALIZED);
  DoLoadGlobal(ic, assembler);
}

// LdaGlobalInsideTypeof <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> inside of a typeof.
void Interpreter::DoLdaGlobalInsideTypeof(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, INSIDE_TYPEOF,
                                                   UNINITIALIZED);
  DoLoadGlobal(ic, assembler);
}

// LdaGlobalWide <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> outside of a typeof.
void Interpreter::DoLdaGlobalWide(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF,
                                                   UNINITIALIZED);
  DoLoadGlobal(ic, assembler);
}

// LdaGlobalInsideTypeofWide <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> inside of a typeof.
void Interpreter::DoLdaGlobalInsideTypeofWide(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, INSIDE_TYPEOF,
                                                   UNINITIALIZED);
  DoLoadGlobal(ic, assembler);
}


void Interpreter::DoStoreGlobal(Callable ic, InterpreterAssembler* assembler) {
  // Get the global object.
  Node* context = __ GetContext();
  Node* native_context =
      __ LoadContextSlot(context, Context::NATIVE_CONTEXT_INDEX);
  Node* global = __ LoadContextSlot(native_context, Context::EXTENSION_INDEX);

  // Store the global via the StoreIC.
  Node* code_target = __ HeapConstant(ic.code());
  Node* constant_index = __ BytecodeOperandIdx(0);
  Node* name = __ LoadConstantPoolEntry(constant_index);
  Node* value = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(1);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  __ CallStub(ic.descriptor(), code_target, context, global, name, value,
              smi_slot, type_feedback_vector);

  __ Dispatch();
}


// StaGlobalSloppy <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in sloppy mode.
void Interpreter::DoStaGlobalSloppy(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoStoreGlobal(ic, assembler);
}


// StaGlobalStrict <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in strict mode.
void Interpreter::DoStaGlobalStrict(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, STRICT, UNINITIALIZED);
  DoStoreGlobal(ic, assembler);
}


// StaGlobalSloppyWide <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in sloppy mode.
void Interpreter::DoStaGlobalSloppyWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoStoreGlobal(ic, assembler);
}


// StaGlobalStrictWide <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in strict mode.
void Interpreter::DoStaGlobalStrictWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, STRICT, UNINITIALIZED);
  DoStoreGlobal(ic, assembler);
}


// LdaContextSlot <context> <slot_index>
//
// Load the object in |slot_index| of |context| into the accumulator.
void Interpreter::DoLdaContextSlot(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* context = __ LoadRegister(reg_index);
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* result = __ LoadContextSlot(context, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}


// LdaContextSlotWide <context> <slot_index>
//
// Load the object in |slot_index| of |context| into the accumulator.
void Interpreter::DoLdaContextSlotWide(InterpreterAssembler* assembler) {
  DoLdaContextSlot(assembler);
}


// StaContextSlot <context> <slot_index>
//
// Stores the object in the accumulator into |slot_index| of |context|.
void Interpreter::DoStaContextSlot(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* context = __ LoadRegister(reg_index);
  Node* slot_index = __ BytecodeOperandIdx(1);
  __ StoreContextSlot(context, slot_index, value);
  __ Dispatch();
}


// StaContextSlot <context> <slot_index>
//
// Stores the object in the accumulator into |slot_index| of |context|.
void Interpreter::DoStaContextSlotWide(InterpreterAssembler* assembler) {
  DoStaContextSlot(assembler);
}

void Interpreter::DoLoadLookupSlot(Runtime::FunctionId function_id,
                                   InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* name = __ LoadConstantPoolEntry(index);
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(function_id, context, name);
  __ SetAccumulator(result);
  __ Dispatch();
}


// LdaLookupSlot <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
void Interpreter::DoLdaLookupSlot(InterpreterAssembler* assembler) {
  DoLoadLookupSlot(Runtime::kLoadLookupSlot, assembler);
}


// LdaLookupSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
void Interpreter::DoLdaLookupSlotInsideTypeof(InterpreterAssembler* assembler) {
  DoLoadLookupSlot(Runtime::kLoadLookupSlotInsideTypeof, assembler);
}


// LdaLookupSlotWide <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
void Interpreter::DoLdaLookupSlotWide(InterpreterAssembler* assembler) {
  DoLdaLookupSlot(assembler);
}


// LdaLookupSlotInsideTypeofWide <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
void Interpreter::DoLdaLookupSlotInsideTypeofWide(
    InterpreterAssembler* assembler) {
  DoLdaLookupSlotInsideTypeof(assembler);
}

void Interpreter::DoStoreLookupSlot(LanguageMode language_mode,
                                    InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* name = __ LoadConstantPoolEntry(index);
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(is_strict(language_mode)
                                    ? Runtime::kStoreLookupSlot_Strict
                                    : Runtime::kStoreLookupSlot_Sloppy,
                                context, name, value);
  __ SetAccumulator(result);
  __ Dispatch();
}


// StaLookupSlotSloppy <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in sloppy mode.
void Interpreter::DoStaLookupSlotSloppy(InterpreterAssembler* assembler) {
  DoStoreLookupSlot(LanguageMode::SLOPPY, assembler);
}


// StaLookupSlotStrict <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in strict mode.
void Interpreter::DoStaLookupSlotStrict(InterpreterAssembler* assembler) {
  DoStoreLookupSlot(LanguageMode::STRICT, assembler);
}


// StaLookupSlotSloppyWide <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in sloppy mode.
void Interpreter::DoStaLookupSlotSloppyWide(InterpreterAssembler* assembler) {
  DoStaLookupSlotSloppy(assembler);
}


// StaLookupSlotStrictWide <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in strict mode.
void Interpreter::DoStaLookupSlotStrictWide(InterpreterAssembler* assembler) {
  DoStaLookupSlotStrict(assembler);
}

void Interpreter::DoLoadIC(Callable ic, InterpreterAssembler* assembler) {
  Node* code_target = __ HeapConstant(ic.code());
  Node* register_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(register_index);
  Node* constant_index = __ BytecodeOperandIdx(1);
  Node* name = __ LoadConstantPoolEntry(constant_index);
  Node* raw_slot = __ BytecodeOperandIdx(2);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  Node* result = __ CallStub(ic.descriptor(), code_target, context, object,
                             name, smi_slot, type_feedback_vector);
  __ SetAccumulator(result);
  __ Dispatch();
}

// LoadIC <object> <name_index> <slot>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name at
// constant pool entry <name_index>.
void Interpreter::DoLoadIC(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF,
                                                   UNINITIALIZED);
  DoLoadIC(ic, assembler);
}

// LoadICWide <object> <name_index> <slot>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name at
// constant pool entry <name_index>.
void Interpreter::DoLoadICWide(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF,
                                                   UNINITIALIZED);
  DoLoadIC(ic, assembler);
}


void Interpreter::DoKeyedLoadIC(Callable ic, InterpreterAssembler* assembler) {
  Node* code_target = __ HeapConstant(ic.code());
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(reg_index);
  Node* name = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(1);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  Node* result = __ CallStub(ic.descriptor(), code_target, context, object,
                             name, smi_slot, type_feedback_vector);
  __ SetAccumulator(result);
  __ Dispatch();
}

// KeyedLoadIC <object> <slot>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator.
void Interpreter::DoKeyedLoadIC(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedLoadICInOptimizedCode(isolate_, UNINITIALIZED);
  DoKeyedLoadIC(ic, assembler);
}

// KeyedLoadICWide <object> <slot>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator.
void Interpreter::DoKeyedLoadICWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedLoadICInOptimizedCode(isolate_, UNINITIALIZED);
  DoKeyedLoadIC(ic, assembler);
}


void Interpreter::DoStoreIC(Callable ic, InterpreterAssembler* assembler) {
  Node* code_target = __ HeapConstant(ic.code());
  Node* object_reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(object_reg_index);
  Node* constant_index = __ BytecodeOperandIdx(1);
  Node* name = __ LoadConstantPoolEntry(constant_index);
  Node* value = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(2);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  __ CallStub(ic.descriptor(), code_target, context, object, name, value,
              smi_slot, type_feedback_vector);
  __ Dispatch();
}


// StoreICSloppy <object> <name_index> <slot>
//
// Calls the sloppy mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
void Interpreter::DoStoreICSloppy(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoStoreIC(ic, assembler);
}


// StoreICStrict <object> <name_index> <slot>
//
// Calls the strict mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
void Interpreter::DoStoreICStrict(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, STRICT, UNINITIALIZED);
  DoStoreIC(ic, assembler);
}


// StoreICSloppyWide <object> <name_index> <slot>
//
// Calls the sloppy mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
void Interpreter::DoStoreICSloppyWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoStoreIC(ic, assembler);
}


// StoreICStrictWide <object> <name_index> <slot>
//
// Calls the strict mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
void Interpreter::DoStoreICStrictWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::StoreICInOptimizedCode(isolate_, STRICT, UNINITIALIZED);
  DoStoreIC(ic, assembler);
}

void Interpreter::DoKeyedStoreIC(Callable ic, InterpreterAssembler* assembler) {
  Node* code_target = __ HeapConstant(ic.code());
  Node* object_reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(object_reg_index);
  Node* name_reg_index = __ BytecodeOperandReg(1);
  Node* name = __ LoadRegister(name_reg_index);
  Node* value = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(2);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  __ CallStub(ic.descriptor(), code_target, context, object, name, value,
              smi_slot, type_feedback_vector);
  __ Dispatch();
}


// KeyedStoreICSloppy <object> <key> <slot>
//
// Calls the sloppy mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
void Interpreter::DoKeyedStoreICSloppy(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedStoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoKeyedStoreIC(ic, assembler);
}


// KeyedStoreICStore <object> <key> <slot>
//
// Calls the strict mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
void Interpreter::DoKeyedStoreICStrict(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedStoreICInOptimizedCode(isolate_, STRICT, UNINITIALIZED);
  DoKeyedStoreIC(ic, assembler);
}


// KeyedStoreICSloppyWide <object> <key> <slot>
//
// Calls the sloppy mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
void Interpreter::DoKeyedStoreICSloppyWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedStoreICInOptimizedCode(isolate_, SLOPPY, UNINITIALIZED);
  DoKeyedStoreIC(ic, assembler);
}


// KeyedStoreICStoreWide <object> <key> <slot>
//
// Calls the strict mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
void Interpreter::DoKeyedStoreICStrictWide(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::KeyedStoreICInOptimizedCode(isolate_, STRICT, UNINITIALIZED);
  DoKeyedStoreIC(ic, assembler);
}

// PushContext <context>
//
// Saves the current context in <context>, and pushes the accumulator as the
// new current context.
void Interpreter::DoPushContext(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* new_context = __ GetAccumulator();
  Node* old_context = __ GetContext();
  __ StoreRegister(old_context, reg_index);
  __ SetContext(new_context);
  __ Dispatch();
}


// PopContext <context>
//
// Pops the current context and sets <context> as the new context.
void Interpreter::DoPopContext(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* context = __ LoadRegister(reg_index);
  __ SetContext(context);
  __ Dispatch();
}

void Interpreter::DoBinaryOp(Runtime::FunctionId function_id,
                             InterpreterAssembler* assembler) {
  // TODO(rmcilroy): Call ICs which back-patch bytecode with type specialized
  // operations, instead of calling builtins directly.
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* lhs = __ LoadRegister(reg_index);
  Node* rhs = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(function_id, context, lhs, rhs);
  __ SetAccumulator(result);
  __ Dispatch();
}


// Add <src>
//
// Add register <src> to accumulator.
void Interpreter::DoAdd(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kAdd, assembler);
}


// Sub <src>
//
// Subtract register <src> from accumulator.
void Interpreter::DoSub(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kSubtract, assembler);
}


// Mul <src>
//
// Multiply accumulator by register <src>.
void Interpreter::DoMul(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kMultiply, assembler);
}


// Div <src>
//
// Divide register <src> by accumulator.
void Interpreter::DoDiv(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kDivide, assembler);
}


// Mod <src>
//
// Modulo register <src> by accumulator.
void Interpreter::DoMod(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kModulus, assembler);
}


// BitwiseOr <src>
//
// BitwiseOr register <src> to accumulator.
void Interpreter::DoBitwiseOr(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kBitwiseOr, assembler);
}


// BitwiseXor <src>
//
// BitwiseXor register <src> to accumulator.
void Interpreter::DoBitwiseXor(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kBitwiseXor, assembler);
}


// BitwiseAnd <src>
//
// BitwiseAnd register <src> to accumulator.
void Interpreter::DoBitwiseAnd(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kBitwiseAnd, assembler);
}


// ShiftLeft <src>
//
// Left shifts register <src> by the count specified in the accumulator.
// Register <src> is converted to an int32 and the accumulator to uint32
// before the operation. 5 lsb bits from the accumulator are used as count
// i.e. <src> << (accumulator & 0x1F).
void Interpreter::DoShiftLeft(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kShiftLeft, assembler);
}


// ShiftRight <src>
//
// Right shifts register <src> by the count specified in the accumulator.
// Result is sign extended. Register <src> is converted to an int32 and the
// accumulator to uint32 before the operation. 5 lsb bits from the accumulator
// are used as count i.e. <src> >> (accumulator & 0x1F).
void Interpreter::DoShiftRight(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kShiftRight, assembler);
}


// ShiftRightLogical <src>
//
// Right Shifts register <src> by the count specified in the accumulator.
// Result is zero-filled. The accumulator and register <src> are converted to
// uint32 before the operation 5 lsb bits from the accumulator are used as
// count i.e. <src> << (accumulator & 0x1F).
void Interpreter::DoShiftRightLogical(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kShiftRightLogical, assembler);
}

void Interpreter::DoCountOp(Runtime::FunctionId function_id,
                            InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* one = __ NumberConstant(1);
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(function_id, context, value, one);
  __ SetAccumulator(result);
  __ Dispatch();
}


// Inc
//
// Increments value in the accumulator by one.
void Interpreter::DoInc(InterpreterAssembler* assembler) {
  DoCountOp(Runtime::kAdd, assembler);
}


// Dec
//
// Decrements value in the accumulator by one.
void Interpreter::DoDec(InterpreterAssembler* assembler) {
  DoCountOp(Runtime::kSubtract, assembler);
}


// LogicalNot
//
// Perform logical-not on the accumulator, first casting the
// accumulator to a boolean value if required.
void Interpreter::DoLogicalNot(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result =
      __ CallRuntime(Runtime::kInterpreterLogicalNot, context, accumulator);
  __ SetAccumulator(result);
  __ Dispatch();
}


// TypeOf
//
// Load the accumulator with the string representating type of the
// object in the accumulator.
void Interpreter::DoTypeOf(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result =
      __ CallRuntime(Runtime::kInterpreterTypeOf, context, accumulator);
  __ SetAccumulator(result);
  __ Dispatch();
}

void Interpreter::DoDelete(Runtime::FunctionId function_id,
                           InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(reg_index);
  Node* key = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(function_id, context, object, key);
  __ SetAccumulator(result);
  __ Dispatch();
}


// DeletePropertyStrict
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following strict mode semantics.
void Interpreter::DoDeletePropertyStrict(InterpreterAssembler* assembler) {
  DoDelete(Runtime::kDeleteProperty_Strict, assembler);
}


// DeletePropertySloppy
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following sloppy mode semantics.
void Interpreter::DoDeletePropertySloppy(InterpreterAssembler* assembler) {
  DoDelete(Runtime::kDeleteProperty_Sloppy, assembler);
}

void Interpreter::DoJSCall(InterpreterAssembler* assembler,
                           TailCallMode tail_call_mode) {
  Node* function_reg = __ BytecodeOperandReg(0);
  Node* function = __ LoadRegister(function_reg);
  Node* receiver_reg = __ BytecodeOperandReg(1);
  Node* receiver_arg = __ RegisterLocation(receiver_reg);
  Node* receiver_args_count = __ BytecodeOperandCount(2);
  Node* receiver_count = __ Int32Constant(1);
  Node* args_count = __ Int32Sub(receiver_args_count, receiver_count);
  Node* context = __ GetContext();
  // TODO(rmcilroy): Use the call type feedback slot to call via CallStub.
  Node* result =
      __ CallJS(function, context, receiver_arg, args_count, tail_call_mode);
  __ SetAccumulator(result);
  __ Dispatch();
}


// Call <callable> <receiver> <arg_count>
//
// Call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers.
void Interpreter::DoCall(InterpreterAssembler* assembler) {
  DoJSCall(assembler, TailCallMode::kDisallow);
}


// CallWide <callable> <receiver> <arg_count>
//
// Call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers.
void Interpreter::DoCallWide(InterpreterAssembler* assembler) {
  DoJSCall(assembler, TailCallMode::kDisallow);
}

// TailCall <callable> <receiver> <arg_count>
//
// Tail call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers.
void Interpreter::DoTailCall(InterpreterAssembler* assembler) {
  DoJSCall(assembler, TailCallMode::kAllow);
}

// TailCallWide <callable> <receiver> <arg_count>
//
// Tail call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers.
void Interpreter::DoTailCallWide(InterpreterAssembler* assembler) {
  DoJSCall(assembler, TailCallMode::kAllow);
}

void Interpreter::DoCallRuntimeCommon(InterpreterAssembler* assembler) {
  Node* function_id = __ BytecodeOperandIdx(0);
  Node* first_arg_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(first_arg_reg);
  Node* args_count = __ BytecodeOperandCount(2);
  Node* context = __ GetContext();
  Node* result = __ CallRuntimeN(function_id, context, first_arg, args_count);
  __ SetAccumulator(result);
  __ Dispatch();
}


// CallRuntime <function_id> <first_arg> <arg_count>
//
// Call the runtime function |function_id| with the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers.
void Interpreter::DoCallRuntime(InterpreterAssembler* assembler) {
  DoCallRuntimeCommon(assembler);
}


// CallRuntime <function_id> <first_arg> <arg_count>
//
// Call the runtime function |function_id| with the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers.
void Interpreter::DoCallRuntimeWide(InterpreterAssembler* assembler) {
  DoCallRuntimeCommon(assembler);
}

void Interpreter::DoCallRuntimeForPairCommon(InterpreterAssembler* assembler) {
  // Call the runtime function.
  Node* function_id = __ BytecodeOperandIdx(0);
  Node* first_arg_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(first_arg_reg);
  Node* args_count = __ BytecodeOperandCount(2);
  Node* context = __ GetContext();
  Node* result_pair =
      __ CallRuntimeN(function_id, context, first_arg, args_count, 2);

  // Store the results in <first_return> and <first_return + 1>
  Node* first_return_reg = __ BytecodeOperandReg(3);
  Node* second_return_reg = __ NextRegister(first_return_reg);
  Node* result0 = __ Projection(0, result_pair);
  Node* result1 = __ Projection(1, result_pair);
  __ StoreRegister(result0, first_return_reg);
  __ StoreRegister(result1, second_return_reg);
  __ Dispatch();
}


// CallRuntimeForPair <function_id> <first_arg> <arg_count> <first_return>
//
// Call the runtime function |function_id| which returns a pair, with the
// first argument in register |first_arg| and |arg_count| arguments in
// subsequent registers. Returns the result in <first_return> and
// <first_return + 1>
void Interpreter::DoCallRuntimeForPair(InterpreterAssembler* assembler) {
  DoCallRuntimeForPairCommon(assembler);
}


// CallRuntimeForPairWide <function_id> <first_arg> <arg_count> <first_return>
//
// Call the runtime function |function_id| which returns a pair, with the
// first argument in register |first_arg| and |arg_count| arguments in
// subsequent registers. Returns the result in <first_return> and
// <first_return + 1>
void Interpreter::DoCallRuntimeForPairWide(InterpreterAssembler* assembler) {
  DoCallRuntimeForPairCommon(assembler);
}

void Interpreter::DoCallJSRuntimeCommon(InterpreterAssembler* assembler) {
  Node* context_index = __ BytecodeOperandIdx(0);
  Node* receiver_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(receiver_reg);
  Node* receiver_args_count = __ BytecodeOperandCount(2);
  Node* receiver_count = __ Int32Constant(1);
  Node* args_count = __ Int32Sub(receiver_args_count, receiver_count);

  // Get the function to call from the native context.
  Node* context = __ GetContext();
  Node* native_context =
      __ LoadContextSlot(context, Context::NATIVE_CONTEXT_INDEX);
  Node* function = __ LoadContextSlot(native_context, context_index);

  // Call the function.
  Node* result = __ CallJS(function, context, first_arg, args_count,
                           TailCallMode::kDisallow);
  __ SetAccumulator(result);
  __ Dispatch();
}


// CallJSRuntime <context_index> <receiver> <arg_count>
//
// Call the JS runtime function that has the |context_index| with the receiver
// in register |receiver| and |arg_count| arguments in subsequent registers.
void Interpreter::DoCallJSRuntime(InterpreterAssembler* assembler) {
  DoCallJSRuntimeCommon(assembler);
}


// CallJSRuntimeWide <context_index> <receiver> <arg_count>
//
// Call the JS runtime function that has the |context_index| with the receiver
// in register |receiver| and |arg_count| arguments in subsequent registers.
void Interpreter::DoCallJSRuntimeWide(InterpreterAssembler* assembler) {
  DoCallJSRuntimeCommon(assembler);
}

void Interpreter::DoCallConstruct(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::InterpreterPushArgsAndConstruct(isolate_);
  Node* new_target = __ GetAccumulator();
  Node* constructor_reg = __ BytecodeOperandReg(0);
  Node* constructor = __ LoadRegister(constructor_reg);
  Node* first_arg_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(first_arg_reg);
  Node* args_count = __ BytecodeOperandCount(2);
  Node* context = __ GetContext();
  Node* result =
      __ CallConstruct(constructor, context, new_target, first_arg, args_count);
  __ SetAccumulator(result);
  __ Dispatch();
}


// New <constructor> <first_arg> <arg_count>
//
// Call operator new with |constructor| and the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers. The new.target is in the accumulator.
//
void Interpreter::DoNew(InterpreterAssembler* assembler) {
  DoCallConstruct(assembler);
}


// NewWide <constructor> <first_arg> <arg_count>
//
// Call operator new with |constructor| and the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers. The new.target is in the accumulator.
//
void Interpreter::DoNewWide(InterpreterAssembler* assembler) {
  DoCallConstruct(assembler);
}


// TestEqual <src>
//
// Test if the value in the <src> register equals the accumulator.
void Interpreter::DoTestEqual(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kEqual, assembler);
}


// TestNotEqual <src>
//
// Test if the value in the <src> register is not equal to the accumulator.
void Interpreter::DoTestNotEqual(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kNotEqual, assembler);
}


// TestEqualStrict <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
void Interpreter::DoTestEqualStrict(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kStrictEqual, assembler);
}


// TestNotEqualStrict <src>
//
// Test if the value in the <src> register is not strictly equal to the
// accumulator.
void Interpreter::DoTestNotEqualStrict(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kStrictNotEqual, assembler);
}


// TestLessThan <src>
//
// Test if the value in the <src> register is less than the accumulator.
void Interpreter::DoTestLessThan(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kLessThan, assembler);
}


// TestGreaterThan <src>
//
// Test if the value in the <src> register is greater than the accumulator.
void Interpreter::DoTestGreaterThan(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kGreaterThan, assembler);
}


// TestLessThanOrEqual <src>
//
// Test if the value in the <src> register is less than or equal to the
// accumulator.
void Interpreter::DoTestLessThanOrEqual(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kLessThanOrEqual, assembler);
}


// TestGreaterThanOrEqual <src>
//
// Test if the value in the <src> register is greater than or equal to the
// accumulator.
void Interpreter::DoTestGreaterThanOrEqual(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kGreaterThanOrEqual, assembler);
}


// TestIn <src>
//
// Test if the object referenced by the register operand is a property of the
// object referenced by the accumulator.
void Interpreter::DoTestIn(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kHasProperty, assembler);
}


// TestInstanceOf <src>
//
// Test if the object referenced by the <src> register is an an instance of type
// referenced by the accumulator.
void Interpreter::DoTestInstanceOf(InterpreterAssembler* assembler) {
  DoBinaryOp(Runtime::kInstanceOf, assembler);
}


// ToName
//
// Cast the object referenced by the accumulator to a name.
void Interpreter::DoToName(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(Runtime::kToName, context, accumulator);
  __ SetAccumulator(result);
  __ Dispatch();
}


// ToNumber
//
// Cast the object referenced by the accumulator to a number.
void Interpreter::DoToNumber(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(Runtime::kToNumber, context, accumulator);
  __ SetAccumulator(result);
  __ Dispatch();
}


// ToObject
//
// Cast the object referenced by the accumulator to a JSObject.
void Interpreter::DoToObject(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(Runtime::kToObject, context, accumulator);
  __ SetAccumulator(result);
  __ Dispatch();
}


// Jump <imm8>
//
// Jump by number of bytes represented by the immediate operand |imm8|.
void Interpreter::DoJump(InterpreterAssembler* assembler) {
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ Jump(relative_jump);
}


// JumpConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool.
void Interpreter::DoJumpConstant(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  __ Jump(relative_jump);
}


// JumpConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the
// constant pool.
void Interpreter::DoJumpConstantWide(InterpreterAssembler* assembler) {
  DoJumpConstant(assembler);
}


// JumpIfTrue <imm8>
//
// Jump by number of bytes represented by an immediate operand if the
// accumulator contains true.
void Interpreter::DoJumpIfTrue(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(accumulator, true_value, relative_jump);
}


// JumpIfTrueConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the accumulator contains true.
void Interpreter::DoJumpIfTrueConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(accumulator, true_value, relative_jump);
}


// JumpIfTrueConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the accumulator contains true.
void Interpreter::DoJumpIfTrueConstantWide(InterpreterAssembler* assembler) {
  DoJumpIfTrueConstant(assembler);
}


// JumpIfFalse <imm8>
//
// Jump by number of bytes represented by an immediate operand if the
// accumulator contains false.
void Interpreter::DoJumpIfFalse(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(accumulator, false_value, relative_jump);
}


// JumpIfFalseConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the accumulator contains false.
void Interpreter::DoJumpIfFalseConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(accumulator, false_value, relative_jump);
}


// JumpIfFalseConstant <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the accumulator contains false.
void Interpreter::DoJumpIfFalseConstantWide(InterpreterAssembler* assembler) {
  DoJumpIfFalseConstant(assembler);
}


// JumpIfToBooleanTrue <imm8>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is true when the object is cast to boolean.
void Interpreter::DoJumpIfToBooleanTrue(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* to_boolean_value =
      __ CallRuntime(Runtime::kInterpreterToBoolean, context, accumulator);
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(to_boolean_value, true_value, relative_jump);
}


// JumpIfToBooleanTrueConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the object referenced by the accumulator is true when the object is cast
// to boolean.
void Interpreter::DoJumpIfToBooleanTrueConstant(
    InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* to_boolean_value =
      __ CallRuntime(Runtime::kInterpreterToBoolean, context, accumulator);
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(to_boolean_value, true_value, relative_jump);
}


// JumpIfToBooleanTrueConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the object referenced by the accumulator is true when the object is cast
// to boolean.
void Interpreter::DoJumpIfToBooleanTrueConstantWide(
    InterpreterAssembler* assembler) {
  DoJumpIfToBooleanTrueConstant(assembler);
}


// JumpIfToBooleanFalse <imm8>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is false when the object is cast to boolean.
void Interpreter::DoJumpIfToBooleanFalse(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* to_boolean_value =
      __ CallRuntime(Runtime::kInterpreterToBoolean, context, accumulator);
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(to_boolean_value, false_value, relative_jump);
}


// JumpIfToBooleanFalseConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the object referenced by the accumulator is false when the object is cast
// to boolean.
void Interpreter::DoJumpIfToBooleanFalseConstant(
    InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* to_boolean_value =
      __ CallRuntime(Runtime::kInterpreterToBoolean, context, accumulator);
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(to_boolean_value, false_value, relative_jump);
}


// JumpIfToBooleanFalseConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the object referenced by the accumulator is false when the object is cast
// to boolean.
void Interpreter::DoJumpIfToBooleanFalseConstantWide(
    InterpreterAssembler* assembler) {
  DoJumpIfToBooleanFalseConstant(assembler);
}


// JumpIfNull <imm8>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the null constant.
void Interpreter::DoJumpIfNull(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* null_value = __ HeapConstant(isolate_->factory()->null_value());
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ JumpIfWordEqual(accumulator, null_value, relative_jump);
}


// JumpIfNullConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the object referenced by the accumulator is the null constant.
void Interpreter::DoJumpIfNullConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* null_value = __ HeapConstant(isolate_->factory()->null_value());
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  __ JumpIfWordEqual(accumulator, null_value, relative_jump);
}


// JumpIfNullConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the object referenced by the accumulator is the null constant.
void Interpreter::DoJumpIfNullConstantWide(InterpreterAssembler* assembler) {
  DoJumpIfNullConstant(assembler);
}

// JumpIfUndefined <imm8>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the undefined constant.
void Interpreter::DoJumpIfUndefined(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* undefined_value =
      __ HeapConstant(isolate_->factory()->undefined_value());
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ JumpIfWordEqual(accumulator, undefined_value, relative_jump);
}


// JumpIfUndefinedConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the object referenced by the accumulator is the undefined constant.
void Interpreter::DoJumpIfUndefinedConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* undefined_value =
      __ HeapConstant(isolate_->factory()->undefined_value());
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  __ JumpIfWordEqual(accumulator, undefined_value, relative_jump);
}


// JumpIfUndefinedConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the object referenced by the accumulator is the undefined constant.
void Interpreter::DoJumpIfUndefinedConstantWide(
    InterpreterAssembler* assembler) {
  DoJumpIfUndefinedConstant(assembler);
}

// JumpIfNotHole <imm8>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the hole.
void Interpreter::DoJumpIfNotHole(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* the_hole_value = __ HeapConstant(isolate_->factory()->the_hole_value());
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ JumpIfWordNotEqual(accumulator, the_hole_value, relative_jump);
}

// JumpIfNotHoleConstant <idx8>
//
// Jump by number of bytes in the Smi in the |idx8| entry in the constant pool
// if the object referenced by the accumulator is the hole constant.
void Interpreter::DoJumpIfNotHoleConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* the_hole_value = __ HeapConstant(isolate_->factory()->the_hole_value());
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  Node* relative_jump = __ SmiUntag(constant);
  __ JumpIfWordNotEqual(accumulator, the_hole_value, relative_jump);
}

// JumpIfNotHoleConstantWide <idx16>
//
// Jump by number of bytes in the Smi in the |idx16| entry in the constant pool
// if the object referenced by the accumulator is the hole constant.
void Interpreter::DoJumpIfNotHoleConstantWide(InterpreterAssembler* assembler) {
  DoJumpIfNotHoleConstant(assembler);
}

void Interpreter::DoCreateLiteral(Runtime::FunctionId function_id,
                                  InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant_elements = __ LoadConstantPoolEntry(index);
  Node* literal_index_raw = __ BytecodeOperandIdx(1);
  Node* literal_index = __ SmiTag(literal_index_raw);
  Node* flags_raw = __ BytecodeOperandImm(2);
  Node* flags = __ SmiTag(flags_raw);
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(function_id, context, closure, literal_index,
                                constant_elements, flags);
  __ SetAccumulator(result);
  __ Dispatch();
}


// CreateRegExpLiteral <pattern_idx> <literal_idx> <flags>
//
// Creates a regular expression literal for literal index <literal_idx> with
// <flags> and the pattern in <pattern_idx>.
void Interpreter::DoCreateRegExpLiteral(InterpreterAssembler* assembler) {
  DoCreateLiteral(Runtime::kCreateRegExpLiteral, assembler);
}


// CreateRegExpLiteralWide <pattern_idx> <literal_idx> <flags>
//
// Creates a regular expression literal for literal index <literal_idx> with
// <flags> and the pattern in <pattern_idx>.
void Interpreter::DoCreateRegExpLiteralWide(InterpreterAssembler* assembler) {
  DoCreateLiteral(Runtime::kCreateRegExpLiteral, assembler);
}


// CreateArrayLiteral <element_idx> <literal_idx> <flags>
//
// Creates an array literal for literal index <literal_idx> with flags <flags>
// and constant elements in <element_idx>.
void Interpreter::DoCreateArrayLiteral(InterpreterAssembler* assembler) {
  DoCreateLiteral(Runtime::kCreateArrayLiteral, assembler);
}


// CreateArrayLiteralWide <element_idx> <literal_idx> <flags>
//
// Creates an array literal for literal index <literal_idx> with flags <flags>
// and constant elements in <element_idx>.
void Interpreter::DoCreateArrayLiteralWide(InterpreterAssembler* assembler) {
  DoCreateLiteral(Runtime::kCreateArrayLiteral, assembler);
}


// CreateObjectLiteral <element_idx> <literal_idx> <flags>
//
// Creates an object literal for literal index <literal_idx> with flags <flags>
// and constant elements in <element_idx>.
void Interpreter::DoCreateObjectLiteral(InterpreterAssembler* assembler) {
  DoCreateLiteral(Runtime::kCreateObjectLiteral, assembler);
}


// CreateObjectLiteralWide <element_idx> <literal_idx> <flags>
//
// Creates an object literal for literal index <literal_idx> with flags <flags>
// and constant elements in <element_idx>.
void Interpreter::DoCreateObjectLiteralWide(InterpreterAssembler* assembler) {
  DoCreateLiteral(Runtime::kCreateObjectLiteral, assembler);
}


// CreateClosure <index> <tenured>
//
// Creates a new closure for SharedFunctionInfo at position |index| in the
// constant pool and with the PretenureFlag <tenured>.
void Interpreter::DoCreateClosure(InterpreterAssembler* assembler) {
  // TODO(rmcilroy): Possibly call FastNewClosureStub when possible instead of
  // calling into the runtime.
  Node* index = __ BytecodeOperandIdx(0);
  Node* shared = __ LoadConstantPoolEntry(index);
  Node* tenured_raw = __ BytecodeOperandImm(1);
  Node* tenured = __ SmiTag(tenured_raw);
  Node* context = __ GetContext();
  Node* result =
      __ CallRuntime(Runtime::kInterpreterNewClosure, context, shared, tenured);
  __ SetAccumulator(result);
  __ Dispatch();
}


// CreateClosureWide <index> <tenured>
//
// Creates a new closure for SharedFunctionInfo at position |index| in the
// constant pool and with the PretenureFlag <tenured>.
void Interpreter::DoCreateClosureWide(InterpreterAssembler* assembler) {
  return DoCreateClosure(assembler);
}


// CreateMappedArguments
//
// Creates a new mapped arguments object.
void Interpreter::DoCreateMappedArguments(InterpreterAssembler* assembler) {
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* context = __ GetContext();
  Node* result =
      __ CallRuntime(Runtime::kNewSloppyArguments_Generic, context, closure);
  __ SetAccumulator(result);
  __ Dispatch();
}


// CreateUnmappedArguments
//
// Creates a new unmapped arguments object.
void Interpreter::DoCreateUnmappedArguments(InterpreterAssembler* assembler) {
  Callable callable = CodeFactory::FastNewStrictArguments(isolate_);
  Node* target = __ HeapConstant(callable.code());
  Node* context = __ GetContext();
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* result = __ CallStub(callable.descriptor(), target, context, closure);
  __ SetAccumulator(result);
  __ Dispatch();
}

// CreateRestParameter
//
// Creates a new rest parameter array.
void Interpreter::DoCreateRestParameter(InterpreterAssembler* assembler) {
  Callable callable = CodeFactory::FastNewRestParameter(isolate_);
  Node* target = __ HeapConstant(callable.code());
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* context = __ GetContext();
  Node* result = __ CallStub(callable.descriptor(), target, context, closure);
  __ SetAccumulator(result);
  __ Dispatch();
}

// StackCheck
//
// Performs a stack guard check.
void Interpreter::DoStackCheck(InterpreterAssembler* assembler) {
  __ StackCheck();
  __ Dispatch();
}

// Throw
//
// Throws the exception in the accumulator.
void Interpreter::DoThrow(InterpreterAssembler* assembler) {
  Node* exception = __ GetAccumulator();
  Node* context = __ GetContext();
  __ CallRuntime(Runtime::kThrow, context, exception);
  // We shouldn't ever return from a throw.
  __ Abort(kUnexpectedReturnFromThrow);
}


// ReThrow
//
// Re-throws the exception in the accumulator.
void Interpreter::DoReThrow(InterpreterAssembler* assembler) {
  Node* exception = __ GetAccumulator();
  Node* context = __ GetContext();
  __ CallRuntime(Runtime::kReThrow, context, exception);
  // We shouldn't ever return from a throw.
  __ Abort(kUnexpectedReturnFromThrow);
}


// Return
//
// Return the value in the accumulator.
void Interpreter::DoReturn(InterpreterAssembler* assembler) {
  __ InterpreterReturn();
}

// Debugger
//
// Call runtime to handle debugger statement.
void Interpreter::DoDebugger(InterpreterAssembler* assembler) {
  Node* context = __ GetContext();
  __ CallRuntime(Runtime::kHandleDebuggerStatement, context);
  __ Dispatch();
}

// DebugBreak
//
// Call runtime to handle a debug break.
#define DEBUG_BREAK(Name, ...)                                              \
  void Interpreter::Do##Name(InterpreterAssembler* assembler) {             \
    Node* context = __ GetContext();                                        \
    Node* original_handler = __ CallRuntime(Runtime::kDebugBreak, context); \
    __ DispatchToBytecodeHandler(original_handler);                         \
  }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK);
#undef DEBUG_BREAK

// ForInPrepare <cache_info_triple>
//
// Returns state for for..in loop execution based on the object in the
// accumulator. The result is output in registers |cache_info_triple| to
// |cache_info_triple + 2|, with the registers holding cache_type, cache_array,
// and cache_length respectively.
void Interpreter::DoForInPrepare(InterpreterAssembler* assembler) {
  Node* object = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result_triple = __ CallRuntime(Runtime::kForInPrepare, context, object);

  // Set output registers:
  //   0 == cache_type, 1 == cache_array, 2 == cache_length
  Node* output_register = __ BytecodeOperandReg(0);
  for (int i = 0; i < 3; i++) {
    Node* cache_info = __ Projection(i, result_triple);
    __ StoreRegister(cache_info, output_register);
    output_register = __ NextRegister(output_register);
  }
  __ Dispatch();
}


// ForInPrepareWide <cache_info_triple>
//
// Returns state for for..in loop execution based on the object in the
// accumulator. The result is output in registers |cache_info_triple| to
// |cache_info_triple + 2|, with the registers holding cache_type, cache_array,
// and cache_length respectively.
void Interpreter::DoForInPrepareWide(InterpreterAssembler* assembler) {
  DoForInPrepare(assembler);
}


// ForInNext <receiver> <index> <cache_info_pair>
//
// Returns the next enumerable property in the the accumulator.
void Interpreter::DoForInNext(InterpreterAssembler* assembler) {
  Node* receiver_reg = __ BytecodeOperandReg(0);
  Node* receiver = __ LoadRegister(receiver_reg);
  Node* index_reg = __ BytecodeOperandReg(1);
  Node* index = __ LoadRegister(index_reg);
  Node* cache_type_reg = __ BytecodeOperandReg(2);
  Node* cache_type = __ LoadRegister(cache_type_reg);
  Node* cache_array_reg = __ NextRegister(cache_type_reg);
  Node* cache_array = __ LoadRegister(cache_array_reg);
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(Runtime::kForInNext, context, receiver,
                                cache_array, cache_type, index);
  __ SetAccumulator(result);
  __ Dispatch();
}


// ForInNextWide <receiver> <index> <cache_info_pair>
//
// Returns the next enumerable property in the the accumulator.
void Interpreter::DoForInNextWide(InterpreterAssembler* assembler) {
  return DoForInNext(assembler);
}


// ForInDone <index> <cache_length>
//
// Returns true if the end of the enumerable properties has been reached.
void Interpreter::DoForInDone(InterpreterAssembler* assembler) {
  // TODO(oth): Implement directly rather than making a runtime call.
  Node* index_reg = __ BytecodeOperandReg(0);
  Node* index = __ LoadRegister(index_reg);
  Node* cache_length_reg = __ BytecodeOperandReg(1);
  Node* cache_length = __ LoadRegister(cache_length_reg);
  Node* context = __ GetContext();
  Node* result =
      __ CallRuntime(Runtime::kForInDone, context, index, cache_length);
  __ SetAccumulator(result);
  __ Dispatch();
}


// ForInStep <index>
//
// Increments the loop counter in register |index| and stores the result
// in the accumulator.
void Interpreter::DoForInStep(InterpreterAssembler* assembler) {
  // TODO(oth): Implement directly rather than making a runtime call.
  Node* index_reg = __ BytecodeOperandReg(0);
  Node* index = __ LoadRegister(index_reg);
  Node* context = __ GetContext();
  Node* result = __ CallRuntime(Runtime::kForInStep, context, index);
  __ SetAccumulator(result);
  __ Dispatch();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
