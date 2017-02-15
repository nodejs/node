// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter.h"

#include <fstream>
#include <memory>

#include "src/ast/prettyprinter.h"
#include "src/code-factory.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/factory.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/log.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;
typedef CodeStubAssembler::Label Label;
typedef CodeStubAssembler::Variable Variable;
typedef InterpreterAssembler::Arg Arg;

#define __ assembler->

class InterpreterCompilationJob final : public CompilationJob {
 public:
  explicit InterpreterCompilationJob(CompilationInfo* info);

 protected:
  Status PrepareJobImpl() final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl() final;

 private:
  BytecodeGenerator* generator() { return &generator_; }

  BytecodeGenerator generator_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterCompilationJob);
};

Interpreter::Interpreter(Isolate* isolate) : isolate_(isolate) {
  memset(dispatch_table_, 0, sizeof(dispatch_table_));
}

void Interpreter::Initialize() {
  if (IsDispatchTableInitialized()) return;
  Zone zone(isolate_->allocator());
  HandleScope scope(isolate_);

  if (FLAG_trace_ignition_dispatches) {
    static const int kBytecodeCount = static_cast<int>(Bytecode::kLast) + 1;
    bytecode_dispatch_counters_table_.reset(
        new uintptr_t[kBytecodeCount * kBytecodeCount]);
    memset(bytecode_dispatch_counters_table_.get(), 0,
           sizeof(uintptr_t) * kBytecodeCount * kBytecodeCount);
  }

  // Generate bytecode handlers for all bytecodes and scales.
  const OperandScale kOperandScales[] = {
#define VALUE(Name, _) OperandScale::k##Name,
      OPERAND_SCALE_LIST(VALUE)
#undef VALUE
  };

  for (OperandScale operand_scale : kOperandScales) {
#define GENERATE_CODE(Name, ...)                                               \
  {                                                                            \
    if (Bytecodes::BytecodeHasHandler(Bytecode::k##Name, operand_scale)) {     \
      InterpreterAssembler assembler(isolate_, &zone, Bytecode::k##Name,       \
                                     operand_scale);                           \
      Do##Name(&assembler);                                                    \
      Handle<Code> code = assembler.GenerateCode();                            \
      size_t index = GetDispatchTableIndex(Bytecode::k##Name, operand_scale);  \
      dispatch_table_[index] = code->entry();                                  \
      TraceCodegen(code);                                                      \
      PROFILE(                                                                 \
          isolate_,                                                            \
          CodeCreateEvent(                                                     \
              CodeEventListener::BYTECODE_HANDLER_TAG,                         \
              AbstractCode::cast(*code),                                       \
              Bytecodes::ToString(Bytecode::k##Name, operand_scale).c_str())); \
    }                                                                          \
  }
    BYTECODE_LIST(GENERATE_CODE)
#undef GENERATE_CODE
  }

  // Fill unused entries will the illegal bytecode handler.
  size_t illegal_index =
      GetDispatchTableIndex(Bytecode::kIllegal, OperandScale::kSingle);
  for (size_t index = 0; index < arraysize(dispatch_table_); ++index) {
    if (dispatch_table_[index] == nullptr) {
      dispatch_table_[index] = dispatch_table_[illegal_index];
    }
  }
}

Code* Interpreter::GetBytecodeHandler(Bytecode bytecode,
                                      OperandScale operand_scale) {
  DCHECK(IsDispatchTableInitialized());
  DCHECK(Bytecodes::BytecodeHasHandler(bytecode, operand_scale));
  size_t index = GetDispatchTableIndex(bytecode, operand_scale);
  Address code_entry = dispatch_table_[index];
  return Code::GetCodeFromTargetAddress(code_entry);
}

// static
size_t Interpreter::GetDispatchTableIndex(Bytecode bytecode,
                                          OperandScale operand_scale) {
  static const size_t kEntriesPerOperandScale = 1u << kBitsPerByte;
  size_t index = static_cast<size_t>(bytecode);
  switch (operand_scale) {
    case OperandScale::kSingle:
      return index;
    case OperandScale::kDouble:
      return index + kEntriesPerOperandScale;
    case OperandScale::kQuadruple:
      return index + 2 * kEntriesPerOperandScale;
  }
  UNREACHABLE();
  return 0;
}

void Interpreter::IterateDispatchTable(ObjectVisitor* v) {
  for (int i = 0; i < kDispatchTableSize; i++) {
    Address code_entry = dispatch_table_[i];
    Object* code = code_entry == nullptr
                       ? nullptr
                       : Code::GetCodeFromTargetAddress(code_entry);
    Object* old_code = code;
    v->VisitPointer(&code);
    if (code != old_code) {
      dispatch_table_[i] = reinterpret_cast<Code*>(code)->entry();
    }
  }
}

// static
int Interpreter::InterruptBudget() {
  return FLAG_interrupt_budget * kCodeSizeMultiplier;
}

InterpreterCompilationJob::InterpreterCompilationJob(CompilationInfo* info)
    : CompilationJob(info->isolate(), info, "Ignition"), generator_(info) {}

InterpreterCompilationJob::Status InterpreterCompilationJob::PrepareJobImpl() {
  if (FLAG_print_bytecode || FLAG_print_ast) {
    OFStream os(stdout);
    std::unique_ptr<char[]> name = info()->GetDebugName();
    os << "[generating bytecode for function: " << info()->GetDebugName().get()
       << "]" << std::endl
       << std::flush;
  }

#ifdef DEBUG
  if (info()->parse_info() && FLAG_print_ast) {
    OFStream os(stdout);
    os << "--- AST ---" << std::endl
       << AstPrinter(info()->isolate()).PrintProgram(info()->literal())
       << std::endl
       << std::flush;
  }
#endif  // DEBUG

  return SUCCEEDED;
}

InterpreterCompilationJob::Status InterpreterCompilationJob::ExecuteJobImpl() {
  // TODO(5203): These timers aren't thread safe, move to using the CompilerJob
  // timers.
  RuntimeCallTimerScope runtimeTimer(info()->isolate(),
                                     &RuntimeCallStats::CompileIgnition);
  TimerEventScope<TimerEventCompileIgnition> timer(info()->isolate());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileIgnition");

  generator()->GenerateBytecode(stack_limit());

  if (generator()->HasStackOverflow()) {
    return FAILED;
  }
  return SUCCEEDED;
}

InterpreterCompilationJob::Status InterpreterCompilationJob::FinalizeJobImpl() {
  Handle<BytecodeArray> bytecodes = generator()->FinalizeBytecode(isolate());
  if (generator()->HasStackOverflow()) {
    return FAILED;
  }

  if (FLAG_print_bytecode) {
    OFStream os(stdout);
    bytecodes->Print(os);
    os << std::flush;
  }

  info()->SetBytecodeArray(bytecodes);
  info()->SetCode(info()->isolate()->builtins()->InterpreterEntryTrampoline());
  return SUCCEEDED;
}

CompilationJob* Interpreter::NewCompilationJob(CompilationInfo* info) {
  return new InterpreterCompilationJob(info);
}

bool Interpreter::IsDispatchTableInitialized() {
  if (FLAG_trace_ignition || FLAG_trace_ignition_codegen ||
      FLAG_trace_ignition_dispatches) {
    // Regenerate table to add bytecode tracing operations, print the assembly
    // code generated by TurboFan or instrument handlers with dispatch counters.
    return false;
  }
  return dispatch_table_[0] != nullptr;
}

void Interpreter::TraceCodegen(Handle<Code> code) {
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    OFStream os(stdout);
    code->Disassemble(nullptr, os);
    os << std::flush;
  }
#endif  // ENABLE_DISASSEMBLER
}

const char* Interpreter::LookupNameOfBytecodeHandler(Code* code) {
#ifdef ENABLE_DISASSEMBLER
#define RETURN_NAME(Name, ...)                                 \
  if (dispatch_table_[Bytecodes::ToByte(Bytecode::k##Name)] == \
      code->entry()) {                                         \
    return #Name;                                              \
  }
  BYTECODE_LIST(RETURN_NAME)
#undef RETURN_NAME
#endif  // ENABLE_DISASSEMBLER
  return nullptr;
}

uintptr_t Interpreter::GetDispatchCounter(Bytecode from, Bytecode to) const {
  int from_index = Bytecodes::ToByte(from);
  int to_index = Bytecodes::ToByte(to);
  return bytecode_dispatch_counters_table_[from_index * kNumberOfBytecodes +
                                           to_index];
}

Local<v8::Object> Interpreter::GetDispatchCountersObject() {
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  Local<v8::Context> context = isolate->GetCurrentContext();

  Local<v8::Object> counters_map = v8::Object::New(isolate);

  // Output is a JSON-encoded object of objects.
  //
  // The keys on the top level object are source bytecodes,
  // and corresponding value are objects. Keys on these last are the
  // destinations of the dispatch and the value associated is a counter for
  // the correspondent source-destination dispatch chain.
  //
  // Only non-zero counters are written to file, but an entry in the top-level
  // object is always present, even if the value is empty because all counters
  // for that source are zero.

  for (int from_index = 0; from_index < kNumberOfBytecodes; ++from_index) {
    Bytecode from_bytecode = Bytecodes::FromByte(from_index);
    Local<v8::Object> counters_row = v8::Object::New(isolate);

    for (int to_index = 0; to_index < kNumberOfBytecodes; ++to_index) {
      Bytecode to_bytecode = Bytecodes::FromByte(to_index);
      uintptr_t counter = GetDispatchCounter(from_bytecode, to_bytecode);

      if (counter > 0) {
        std::string to_name = Bytecodes::ToString(to_bytecode);
        Local<v8::String> to_name_object =
            v8::String::NewFromUtf8(isolate, to_name.c_str(),
                                    NewStringType::kNormal)
                .ToLocalChecked();
        Local<v8::Number> counter_object = v8::Number::New(isolate, counter);
        CHECK(counters_row
                  ->DefineOwnProperty(context, to_name_object, counter_object)
                  .IsJust());
      }
    }

    std::string from_name = Bytecodes::ToString(from_bytecode);
    Local<v8::String> from_name_object =
        v8::String::NewFromUtf8(isolate, from_name.c_str(),
                                NewStringType::kNormal)
            .ToLocalChecked();

    CHECK(
        counters_map->DefineOwnProperty(context, from_name_object, counters_row)
            .IsJust());
  }

  return counters_map;
}

// LdaZero
//
// Load literal '0' into the accumulator.
void Interpreter::DoLdaZero(InterpreterAssembler* assembler) {
  Node* zero_value = __ NumberConstant(0.0);
  __ SetAccumulator(zero_value);
  __ Dispatch();
}

// LdaSmi <imm>
//
// Load an integer literal into the accumulator as a Smi.
void Interpreter::DoLdaSmi(InterpreterAssembler* assembler) {
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* smi_int = __ SmiTag(raw_int);
  __ SetAccumulator(smi_int);
  __ Dispatch();
}

// LdaConstant <idx>
//
// Load constant literal at |idx| in the constant pool into the accumulator.
void Interpreter::DoLdaConstant(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* constant = __ LoadConstantPoolEntry(index);
  __ SetAccumulator(constant);
  __ Dispatch();
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

// LdrUndefined <reg>
//
// Loads undefined into the accumulator and |reg|.
void Interpreter::DoLdrUndefined(InterpreterAssembler* assembler) {
  Node* undefined_value =
      __ HeapConstant(isolate_->factory()->undefined_value());
  Node* destination = __ BytecodeOperandReg(0);
  __ StoreRegister(undefined_value, destination);
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

Node* Interpreter::BuildLoadGlobal(Callable ic, Node* context,
                                   Node* feedback_slot,
                                   InterpreterAssembler* assembler) {
  typedef LoadGlobalWithVectorDescriptor Descriptor;

  // Load the global via the LoadGlobalIC.
  Node* code_target = __ HeapConstant(ic.code());
  Node* smi_slot = __ SmiTag(feedback_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  return __ CallStub(ic.descriptor(), code_target, context,
                     Arg(Descriptor::kSlot, smi_slot),
                     Arg(Descriptor::kVector, type_feedback_vector));
}

// LdaGlobal <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> outside of a typeof.
void Interpreter::DoLdaGlobal(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::LoadGlobalICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF);

  Node* context = __ GetContext();

  Node* raw_slot = __ BytecodeOperandIdx(0);
  Node* result = BuildLoadGlobal(ic, context, raw_slot, assembler);
  __ SetAccumulator(result);
  __ Dispatch();
}

// LdrGlobal <slot> <reg>
//
// Load the global with name in constant pool entry <name_index> into
// register <reg> using FeedBackVector slot <slot> outside of a typeof.
void Interpreter::DoLdrGlobal(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::LoadGlobalICInOptimizedCode(isolate_, NOT_INSIDE_TYPEOF);

  Node* context = __ GetContext();

  Node* raw_slot = __ BytecodeOperandIdx(0);
  Node* result = BuildLoadGlobal(ic, context, raw_slot, assembler);
  Node* destination = __ BytecodeOperandReg(1);
  __ StoreRegister(result, destination);
  __ Dispatch();
}

// LdaGlobalInsideTypeof <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> inside of a typeof.
void Interpreter::DoLdaGlobalInsideTypeof(InterpreterAssembler* assembler) {
  Callable ic =
      CodeFactory::LoadGlobalICInOptimizedCode(isolate_, INSIDE_TYPEOF);

  Node* context = __ GetContext();

  Node* raw_slot = __ BytecodeOperandIdx(0);
  Node* result = BuildLoadGlobal(ic, context, raw_slot, assembler);
  __ SetAccumulator(result);
  __ Dispatch();
}

void Interpreter::DoStaGlobal(Callable ic, InterpreterAssembler* assembler) {
  typedef StoreWithVectorDescriptor Descriptor;
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
  __ CallStub(ic.descriptor(), code_target, context,
              Arg(Descriptor::kReceiver, global), Arg(Descriptor::kName, name),
              Arg(Descriptor::kValue, value), Arg(Descriptor::kSlot, smi_slot),
              Arg(Descriptor::kVector, type_feedback_vector));
  __ Dispatch();
}

// StaGlobalSloppy <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in sloppy mode.
void Interpreter::DoStaGlobalSloppy(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY);
  DoStaGlobal(ic, assembler);
}

// StaGlobalStrict <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in strict mode.
void Interpreter::DoStaGlobalStrict(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, STRICT);
  DoStaGlobal(ic, assembler);
}

compiler::Node* Interpreter::BuildLoadContextSlot(
    InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* context = __ LoadRegister(reg_index);
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* depth = __ BytecodeOperandUImm(2);
  Node* slot_context = __ GetContextAtDepth(context, depth);
  return __ LoadContextSlot(slot_context, slot_index);
}

// LdaContextSlot <context> <slot_index> <depth>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain starting at |context| into the accumulator.
void Interpreter::DoLdaContextSlot(InterpreterAssembler* assembler) {
  Node* result = BuildLoadContextSlot(assembler);
  __ SetAccumulator(result);
  __ Dispatch();
}

// LdrContextSlot <context> <slot_index> <depth> <reg>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain of |context| into register |reg|.
void Interpreter::DoLdrContextSlot(InterpreterAssembler* assembler) {
  Node* result = BuildLoadContextSlot(assembler);
  Node* destination = __ BytecodeOperandReg(3);
  __ StoreRegister(result, destination);
  __ Dispatch();
}

// StaContextSlot <context> <slot_index> <depth>
//
// Stores the object in the accumulator into |slot_index| of the context at
// |depth| in the context chain starting at |context|.
void Interpreter::DoStaContextSlot(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* context = __ LoadRegister(reg_index);
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* depth = __ BytecodeOperandUImm(2);
  Node* slot_context = __ GetContextAtDepth(context, depth);
  __ StoreContextSlot(slot_context, slot_index, value);
  __ Dispatch();
}

void Interpreter::DoLdaLookupSlot(Runtime::FunctionId function_id,
                                  InterpreterAssembler* assembler) {
  Node* name_index = __ BytecodeOperandIdx(0);
  Node* name = __ LoadConstantPoolEntry(name_index);
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
  DoLdaLookupSlot(Runtime::kLoadLookupSlot, assembler);
}

// LdaLookupSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
void Interpreter::DoLdaLookupSlotInsideTypeof(InterpreterAssembler* assembler) {
  DoLdaLookupSlot(Runtime::kLoadLookupSlotInsideTypeof, assembler);
}

void Interpreter::DoLdaLookupContextSlot(Runtime::FunctionId function_id,
                                         InterpreterAssembler* assembler) {
  Node* context = __ GetContext();
  Node* name_index = __ BytecodeOperandIdx(0);
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* depth = __ BytecodeOperandUImm(2);

  Label slowpath(assembler, Label::kDeferred);

  // Check for context extensions to allow the fast path.
  __ GotoIfHasContextExtensionUpToDepth(context, depth, &slowpath);

  // Fast path does a normal load context.
  {
    Node* slot_context = __ GetContextAtDepth(context, depth);
    Node* result = __ LoadContextSlot(slot_context, slot_index);
    __ SetAccumulator(result);
    __ Dispatch();
  }

  // Slow path when we have to call out to the runtime.
  __ Bind(&slowpath);
  {
    Node* name = __ LoadConstantPoolEntry(name_index);
    Node* result = __ CallRuntime(function_id, context, name);
    __ SetAccumulator(result);
    __ Dispatch();
  }
}

// LdaLookupSlot <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
void Interpreter::DoLdaLookupContextSlot(InterpreterAssembler* assembler) {
  DoLdaLookupContextSlot(Runtime::kLoadLookupSlot, assembler);
}

// LdaLookupSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
void Interpreter::DoLdaLookupContextSlotInsideTypeof(
    InterpreterAssembler* assembler) {
  DoLdaLookupContextSlot(Runtime::kLoadLookupSlotInsideTypeof, assembler);
}

void Interpreter::DoLdaLookupGlobalSlot(Runtime::FunctionId function_id,
                                        InterpreterAssembler* assembler) {
  Node* context = __ GetContext();
  Node* name_index = __ BytecodeOperandIdx(0);
  Node* feedback_slot = __ BytecodeOperandIdx(1);
  Node* depth = __ BytecodeOperandUImm(2);

  Label slowpath(assembler, Label::kDeferred);

  // Check for context extensions to allow the fast path
  __ GotoIfHasContextExtensionUpToDepth(context, depth, &slowpath);

  // Fast path does a normal load global
  {
    Callable ic = CodeFactory::LoadGlobalICInOptimizedCode(
        isolate_, function_id == Runtime::kLoadLookupSlotInsideTypeof
                      ? INSIDE_TYPEOF
                      : NOT_INSIDE_TYPEOF);
    Node* result = BuildLoadGlobal(ic, context, feedback_slot, assembler);
    __ SetAccumulator(result);
    __ Dispatch();
  }

  // Slow path when we have to call out to the runtime
  __ Bind(&slowpath);
  {
    Node* name = __ LoadConstantPoolEntry(name_index);
    Node* result = __ CallRuntime(function_id, context, name);
    __ SetAccumulator(result);
    __ Dispatch();
  }
}

// LdaLookupGlobalSlot <name_index> <feedback_slot> <depth>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
void Interpreter::DoLdaLookupGlobalSlot(InterpreterAssembler* assembler) {
  DoLdaLookupGlobalSlot(Runtime::kLoadLookupSlot, assembler);
}

// LdaLookupGlobalSlotInsideTypeof <name_index> <feedback_slot> <depth>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
void Interpreter::DoLdaLookupGlobalSlotInsideTypeof(
    InterpreterAssembler* assembler) {
  DoLdaLookupGlobalSlot(Runtime::kLoadLookupSlotInsideTypeof, assembler);
}

void Interpreter::DoStaLookupSlot(LanguageMode language_mode,
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
  DoStaLookupSlot(LanguageMode::SLOPPY, assembler);
}

// StaLookupSlotStrict <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in strict mode.
void Interpreter::DoStaLookupSlotStrict(InterpreterAssembler* assembler) {
  DoStaLookupSlot(LanguageMode::STRICT, assembler);
}

Node* Interpreter::BuildLoadNamedProperty(Callable ic,
                                          InterpreterAssembler* assembler) {
  typedef LoadWithVectorDescriptor Descriptor;
  Node* code_target = __ HeapConstant(ic.code());
  Node* register_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(register_index);
  Node* constant_index = __ BytecodeOperandIdx(1);
  Node* name = __ LoadConstantPoolEntry(constant_index);
  Node* raw_slot = __ BytecodeOperandIdx(2);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  return __ CallStub(
      ic.descriptor(), code_target, context, Arg(Descriptor::kReceiver, object),
      Arg(Descriptor::kName, name), Arg(Descriptor::kSlot, smi_slot),
      Arg(Descriptor::kVector, type_feedback_vector));
}

// LdaNamedProperty <object> <name_index> <slot>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name at
// constant pool entry <name_index>.
void Interpreter::DoLdaNamedProperty(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_);
  Node* result = BuildLoadNamedProperty(ic, assembler);
  __ SetAccumulator(result);
  __ Dispatch();
}

// LdrNamedProperty <object> <name_index> <slot> <reg>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name at
// constant pool entry <name_index> and puts the result into register <reg>.
void Interpreter::DoLdrNamedProperty(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::LoadICInOptimizedCode(isolate_);
  Node* result = BuildLoadNamedProperty(ic, assembler);
  Node* destination = __ BytecodeOperandReg(3);
  __ StoreRegister(result, destination);
  __ Dispatch();
}

Node* Interpreter::BuildLoadKeyedProperty(Callable ic,
                                          InterpreterAssembler* assembler) {
  typedef LoadWithVectorDescriptor Descriptor;
  Node* code_target = __ HeapConstant(ic.code());
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(reg_index);
  Node* name = __ GetAccumulator();
  Node* raw_slot = __ BytecodeOperandIdx(1);
  Node* smi_slot = __ SmiTag(raw_slot);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  return __ CallStub(
      ic.descriptor(), code_target, context, Arg(Descriptor::kReceiver, object),
      Arg(Descriptor::kName, name), Arg(Descriptor::kSlot, smi_slot),
      Arg(Descriptor::kVector, type_feedback_vector));
}

// KeyedLoadIC <object> <slot>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator.
void Interpreter::DoLdaKeyedProperty(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::KeyedLoadICInOptimizedCode(isolate_);
  Node* result = BuildLoadKeyedProperty(ic, assembler);
  __ SetAccumulator(result);
  __ Dispatch();
}

// LdrKeyedProperty <object> <slot> <reg>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator and puts the result in register <reg>.
void Interpreter::DoLdrKeyedProperty(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::KeyedLoadICInOptimizedCode(isolate_);
  Node* result = BuildLoadKeyedProperty(ic, assembler);
  Node* destination = __ BytecodeOperandReg(2);
  __ StoreRegister(result, destination);
  __ Dispatch();
}

void Interpreter::DoStoreIC(Callable ic, InterpreterAssembler* assembler) {
  typedef StoreWithVectorDescriptor Descriptor;
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
  __ CallStub(ic.descriptor(), code_target, context,
              Arg(Descriptor::kReceiver, object), Arg(Descriptor::kName, name),
              Arg(Descriptor::kValue, value), Arg(Descriptor::kSlot, smi_slot),
              Arg(Descriptor::kVector, type_feedback_vector));
  __ Dispatch();
}

// StaNamedPropertySloppy <object> <name_index> <slot>
//
// Calls the sloppy mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
void Interpreter::DoStaNamedPropertySloppy(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, SLOPPY);
  DoStoreIC(ic, assembler);
}

// StaNamedPropertyStrict <object> <name_index> <slot>
//
// Calls the strict mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
void Interpreter::DoStaNamedPropertyStrict(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate_, STRICT);
  DoStoreIC(ic, assembler);
}

void Interpreter::DoKeyedStoreIC(Callable ic, InterpreterAssembler* assembler) {
  typedef StoreWithVectorDescriptor Descriptor;
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
  __ CallStub(ic.descriptor(), code_target, context,
              Arg(Descriptor::kReceiver, object), Arg(Descriptor::kName, name),
              Arg(Descriptor::kValue, value), Arg(Descriptor::kSlot, smi_slot),
              Arg(Descriptor::kVector, type_feedback_vector));
  __ Dispatch();
}

// StaKeyedPropertySloppy <object> <key> <slot>
//
// Calls the sloppy mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
void Interpreter::DoStaKeyedPropertySloppy(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::KeyedStoreICInOptimizedCode(isolate_, SLOPPY);
  DoKeyedStoreIC(ic, assembler);
}

// StaKeyedPropertyStrict <object> <key> <slot>
//
// Calls the strict mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
void Interpreter::DoStaKeyedPropertyStrict(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::KeyedStoreICInOptimizedCode(isolate_, STRICT);
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

// TODO(mythria): Remove this function once all BinaryOps record type feedback.
template <class Generator>
void Interpreter::DoBinaryOp(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* lhs = __ LoadRegister(reg_index);
  Node* rhs = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = Generator::Generate(assembler, lhs, rhs, context);
  __ SetAccumulator(result);
  __ Dispatch();
}

template <class Generator>
void Interpreter::DoBinaryOpWithFeedback(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* lhs = __ LoadRegister(reg_index);
  Node* rhs = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* result = Generator::Generate(assembler, lhs, rhs, slot_index,
                                     type_feedback_vector, context);
  __ SetAccumulator(result);
  __ Dispatch();
}

template <class Generator>
void Interpreter::DoCompareOpWithFeedback(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* lhs = __ LoadRegister(reg_index);
  Node* rhs = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();

  // TODO(interpreter): the only reason this check is here is because we
  // sometimes emit comparisons that shouldn't collect feedback (e.g.
  // try-finally blocks and generators), and we could get rid of this by
  // introducing Smi equality tests.
  Label skip_feedback_update(assembler);
  __ GotoIf(__ WordEqual(slot_index, __ IntPtrConstant(0)),
            &skip_feedback_update);

  Variable var_type_feedback(assembler, MachineRepresentation::kWord32);
  Label lhs_is_smi(assembler), lhs_is_not_smi(assembler),
      gather_rhs_type(assembler), do_compare(assembler);
  __ Branch(__ WordIsSmi(lhs), &lhs_is_smi, &lhs_is_not_smi);

  __ Bind(&lhs_is_smi);
  var_type_feedback.Bind(
      __ Int32Constant(CompareOperationFeedback::kSignedSmall));
  __ Goto(&gather_rhs_type);

  __ Bind(&lhs_is_not_smi);
  {
    Label lhs_is_number(assembler), lhs_is_not_number(assembler);
    Node* lhs_map = __ LoadMap(lhs);
    __ Branch(__ WordEqual(lhs_map, __ HeapNumberMapConstant()), &lhs_is_number,
              &lhs_is_not_number);

    __ Bind(&lhs_is_number);
    var_type_feedback.Bind(__ Int32Constant(CompareOperationFeedback::kNumber));
    __ Goto(&gather_rhs_type);

    __ Bind(&lhs_is_not_number);
    var_type_feedback.Bind(__ Int32Constant(CompareOperationFeedback::kAny));
    __ Goto(&do_compare);
  }

  __ Bind(&gather_rhs_type);
  {
    Label rhs_is_smi(assembler);
    __ GotoIf(__ WordIsSmi(rhs), &rhs_is_smi);

    Node* rhs_map = __ LoadMap(rhs);
    Node* rhs_type =
        __ Select(__ WordEqual(rhs_map, __ HeapNumberMapConstant()),
                  __ Int32Constant(CompareOperationFeedback::kNumber),
                  __ Int32Constant(CompareOperationFeedback::kAny));
    var_type_feedback.Bind(__ Word32Or(var_type_feedback.value(), rhs_type));
    __ Goto(&do_compare);

    __ Bind(&rhs_is_smi);
    var_type_feedback.Bind(
        __ Word32Or(var_type_feedback.value(),
                    __ Int32Constant(CompareOperationFeedback::kSignedSmall)));
    __ Goto(&do_compare);
  }

  __ Bind(&do_compare);
  __ UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                    slot_index);
  __ Goto(&skip_feedback_update);

  __ Bind(&skip_feedback_update);
  Node* result = Generator::Generate(assembler, lhs, rhs, context);
  __ SetAccumulator(result);
  __ Dispatch();
}

// Add <src>
//
// Add register <src> to accumulator.
void Interpreter::DoAdd(InterpreterAssembler* assembler) {
  DoBinaryOpWithFeedback<AddWithFeedbackStub>(assembler);
}

// Sub <src>
//
// Subtract register <src> from accumulator.
void Interpreter::DoSub(InterpreterAssembler* assembler) {
  DoBinaryOpWithFeedback<SubtractWithFeedbackStub>(assembler);
}

// Mul <src>
//
// Multiply accumulator by register <src>.
void Interpreter::DoMul(InterpreterAssembler* assembler) {
  DoBinaryOpWithFeedback<MultiplyWithFeedbackStub>(assembler);
}

// Div <src>
//
// Divide register <src> by accumulator.
void Interpreter::DoDiv(InterpreterAssembler* assembler) {
  DoBinaryOpWithFeedback<DivideWithFeedbackStub>(assembler);
}

// Mod <src>
//
// Modulo register <src> by accumulator.
void Interpreter::DoMod(InterpreterAssembler* assembler) {
  DoBinaryOpWithFeedback<ModulusWithFeedbackStub>(assembler);
}

void Interpreter::DoBitwiseBinaryOp(Token::Value bitwise_op,
                                    InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* lhs = __ LoadRegister(reg_index);
  Node* rhs = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(1);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();

  Variable var_lhs_type_feedback(assembler, MachineRepresentation::kWord32),
      var_rhs_type_feedback(assembler, MachineRepresentation::kWord32);
  Node* lhs_value = __ TruncateTaggedToWord32WithFeedback(
      context, lhs, &var_lhs_type_feedback);
  Node* rhs_value = __ TruncateTaggedToWord32WithFeedback(
      context, rhs, &var_rhs_type_feedback);
  Node* result = nullptr;

  switch (bitwise_op) {
    case Token::BIT_OR: {
      Node* value = __ Word32Or(lhs_value, rhs_value);
      result = __ ChangeInt32ToTagged(value);
    } break;
    case Token::BIT_AND: {
      Node* value = __ Word32And(lhs_value, rhs_value);
      result = __ ChangeInt32ToTagged(value);
    } break;
    case Token::BIT_XOR: {
      Node* value = __ Word32Xor(lhs_value, rhs_value);
      result = __ ChangeInt32ToTagged(value);
    } break;
    case Token::SHL: {
      Node* value = __ Word32Shl(
          lhs_value, __ Word32And(rhs_value, __ Int32Constant(0x1f)));
      result = __ ChangeInt32ToTagged(value);
    } break;
    case Token::SHR: {
      Node* value = __ Word32Shr(
          lhs_value, __ Word32And(rhs_value, __ Int32Constant(0x1f)));
      result = __ ChangeUint32ToTagged(value);
    } break;
    case Token::SAR: {
      Node* value = __ Word32Sar(
          lhs_value, __ Word32And(rhs_value, __ Int32Constant(0x1f)));
      result = __ ChangeInt32ToTagged(value);
    } break;
    default:
      UNREACHABLE();
  }

  Node* result_type =
      __ Select(__ WordIsSmi(result),
                __ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                __ Int32Constant(BinaryOperationFeedback::kNumber));

  if (FLAG_debug_code) {
    Label ok(assembler);
    __ GotoIf(__ WordIsSmi(result), &ok);
    Node* result_map = __ LoadMap(result);
    __ AbortIfWordNotEqual(result_map, __ HeapNumberMapConstant(),
                           kExpectedHeapNumber);
    __ Goto(&ok);
    __ Bind(&ok);
  }

  Node* input_feedback =
      __ Word32Or(var_lhs_type_feedback.value(), var_rhs_type_feedback.value());
  __ UpdateFeedback(__ Word32Or(result_type, input_feedback),
                    type_feedback_vector, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}

// BitwiseOr <src>
//
// BitwiseOr register <src> to accumulator.
void Interpreter::DoBitwiseOr(InterpreterAssembler* assembler) {
  DoBitwiseBinaryOp(Token::BIT_OR, assembler);
}

// BitwiseXor <src>
//
// BitwiseXor register <src> to accumulator.
void Interpreter::DoBitwiseXor(InterpreterAssembler* assembler) {
  DoBitwiseBinaryOp(Token::BIT_XOR, assembler);
}

// BitwiseAnd <src>
//
// BitwiseAnd register <src> to accumulator.
void Interpreter::DoBitwiseAnd(InterpreterAssembler* assembler) {
  DoBitwiseBinaryOp(Token::BIT_AND, assembler);
}

// ShiftLeft <src>
//
// Left shifts register <src> by the count specified in the accumulator.
// Register <src> is converted to an int32 and the accumulator to uint32
// before the operation. 5 lsb bits from the accumulator are used as count
// i.e. <src> << (accumulator & 0x1F).
void Interpreter::DoShiftLeft(InterpreterAssembler* assembler) {
  DoBitwiseBinaryOp(Token::SHL, assembler);
}

// ShiftRight <src>
//
// Right shifts register <src> by the count specified in the accumulator.
// Result is sign extended. Register <src> is converted to an int32 and the
// accumulator to uint32 before the operation. 5 lsb bits from the accumulator
// are used as count i.e. <src> >> (accumulator & 0x1F).
void Interpreter::DoShiftRight(InterpreterAssembler* assembler) {
  DoBitwiseBinaryOp(Token::SAR, assembler);
}

// ShiftRightLogical <src>
//
// Right Shifts register <src> by the count specified in the accumulator.
// Result is zero-filled. The accumulator and register <src> are converted to
// uint32 before the operation 5 lsb bits from the accumulator are used as
// count i.e. <src> << (accumulator & 0x1F).
void Interpreter::DoShiftRightLogical(InterpreterAssembler* assembler) {
  DoBitwiseBinaryOp(Token::SHR, assembler);
}

// AddSmi <imm> <reg>
//
// Adds an immediate value <imm> to register <reg>. For this
// operation <reg> is the lhs operand and <imm> is the <rhs> operand.
void Interpreter::DoAddSmi(InterpreterAssembler* assembler) {
  Variable var_result(assembler, MachineRepresentation::kTagged);
  Label fastpath(assembler), slowpath(assembler, Label::kDeferred),
      end(assembler);

  Node* reg_index = __ BytecodeOperandReg(1);
  Node* left = __ LoadRegister(reg_index);
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* right = __ SmiTag(raw_int);
  Node* slot_index = __ BytecodeOperandIdx(2);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  __ BranchIf(__ WordIsSmi(left), &fastpath, &slowpath);
  __ Bind(&fastpath);
  {
    // Try fast Smi addition first.
    Node* pair = __ SmiAddWithOverflow(left, right);
    Node* overflow = __ Projection(1, pair);

    // Check if the Smi additon overflowed.
    Label if_notoverflow(assembler);
    __ BranchIf(overflow, &slowpath, &if_notoverflow);
    __ Bind(&if_notoverflow);
    {
      __ UpdateFeedback(__ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                        type_feedback_vector, slot_index);
      var_result.Bind(__ Projection(0, pair));
      __ Goto(&end);
    }
  }
  __ Bind(&slowpath);
  {
    Node* context = __ GetContext();
    AddWithFeedbackStub stub(__ isolate());
    Callable callable =
        Callable(stub.GetCode(), AddWithFeedbackStub::Descriptor(__ isolate()));
    Node* args[] = {left, right, slot_index, type_feedback_vector, context};
    var_result.Bind(__ CallStubN(callable, args, 1));
    __ Goto(&end);
  }
  __ Bind(&end);
  {
    __ SetAccumulator(var_result.value());
    __ Dispatch();
  }
}

// SubSmi <imm> <reg>
//
// Subtracts an immediate value <imm> to register <reg>. For this
// operation <reg> is the lhs operand and <imm> is the rhs operand.
void Interpreter::DoSubSmi(InterpreterAssembler* assembler) {
  Variable var_result(assembler, MachineRepresentation::kTagged);
  Label fastpath(assembler), slowpath(assembler, Label::kDeferred),
      end(assembler);

  Node* reg_index = __ BytecodeOperandReg(1);
  Node* left = __ LoadRegister(reg_index);
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* right = __ SmiTag(raw_int);
  Node* slot_index = __ BytecodeOperandIdx(2);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  __ BranchIf(__ WordIsSmi(left), &fastpath, &slowpath);
  __ Bind(&fastpath);
  {
    // Try fast Smi subtraction first.
    Node* pair = __ SmiSubWithOverflow(left, right);
    Node* overflow = __ Projection(1, pair);

    // Check if the Smi subtraction overflowed.
    Label if_notoverflow(assembler);
    __ BranchIf(overflow, &slowpath, &if_notoverflow);
    __ Bind(&if_notoverflow);
    {
      __ UpdateFeedback(__ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                        type_feedback_vector, slot_index);
      var_result.Bind(__ Projection(0, pair));
      __ Goto(&end);
    }
  }
  __ Bind(&slowpath);
  {
    Node* context = __ GetContext();
    SubtractWithFeedbackStub stub(__ isolate());
    Callable callable = Callable(
        stub.GetCode(), SubtractWithFeedbackStub::Descriptor(__ isolate()));
    Node* args[] = {left, right, slot_index, type_feedback_vector, context};
    var_result.Bind(__ CallStubN(callable, args, 1));
    __ Goto(&end);
  }
  __ Bind(&end);
  {
    __ SetAccumulator(var_result.value());
    __ Dispatch();
  }
}

// BitwiseOr <imm> <reg>
//
// BitwiseOr <reg> with <imm>. For this operation <reg> is the lhs
// operand and <imm> is the rhs operand.
void Interpreter::DoBitwiseOrSmi(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(1);
  Node* left = __ LoadRegister(reg_index);
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* right = __ SmiTag(raw_int);
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(2);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Variable var_lhs_type_feedback(assembler, MachineRepresentation::kWord32);
  Node* lhs_value = __ TruncateTaggedToWord32WithFeedback(
      context, left, &var_lhs_type_feedback);
  Node* rhs_value = __ SmiToWord32(right);
  Node* value = __ Word32Or(lhs_value, rhs_value);
  Node* result = __ ChangeInt32ToTagged(value);
  Node* result_type =
      __ Select(__ WordIsSmi(result),
                __ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                __ Int32Constant(BinaryOperationFeedback::kNumber));
  __ UpdateFeedback(__ Word32Or(result_type, var_lhs_type_feedback.value()),
                    type_feedback_vector, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}

// BitwiseAnd <imm> <reg>
//
// BitwiseAnd <reg> with <imm>. For this operation <reg> is the lhs
// operand and <imm> is the rhs operand.
void Interpreter::DoBitwiseAndSmi(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(1);
  Node* left = __ LoadRegister(reg_index);
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* right = __ SmiTag(raw_int);
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(2);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Variable var_lhs_type_feedback(assembler, MachineRepresentation::kWord32);
  Node* lhs_value = __ TruncateTaggedToWord32WithFeedback(
      context, left, &var_lhs_type_feedback);
  Node* rhs_value = __ SmiToWord32(right);
  Node* value = __ Word32And(lhs_value, rhs_value);
  Node* result = __ ChangeInt32ToTagged(value);
  Node* result_type =
      __ Select(__ WordIsSmi(result),
                __ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                __ Int32Constant(BinaryOperationFeedback::kNumber));
  __ UpdateFeedback(__ Word32Or(result_type, var_lhs_type_feedback.value()),
                    type_feedback_vector, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}

// ShiftLeftSmi <imm> <reg>
//
// Left shifts register <src> by the count specified in <imm>.
// Register <src> is converted to an int32 before the operation. The 5
// lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
void Interpreter::DoShiftLeftSmi(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(1);
  Node* left = __ LoadRegister(reg_index);
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* right = __ SmiTag(raw_int);
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(2);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Variable var_lhs_type_feedback(assembler, MachineRepresentation::kWord32);
  Node* lhs_value = __ TruncateTaggedToWord32WithFeedback(
      context, left, &var_lhs_type_feedback);
  Node* rhs_value = __ SmiToWord32(right);
  Node* shift_count = __ Word32And(rhs_value, __ Int32Constant(0x1f));
  Node* value = __ Word32Shl(lhs_value, shift_count);
  Node* result = __ ChangeInt32ToTagged(value);
  Node* result_type =
      __ Select(__ WordIsSmi(result),
                __ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                __ Int32Constant(BinaryOperationFeedback::kNumber));
  __ UpdateFeedback(__ Word32Or(result_type, var_lhs_type_feedback.value()),
                    type_feedback_vector, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}

// ShiftRightSmi <imm> <reg>
//
// Right shifts register <src> by the count specified in <imm>.
// Register <src> is converted to an int32 before the operation. The 5
// lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
void Interpreter::DoShiftRightSmi(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(1);
  Node* left = __ LoadRegister(reg_index);
  Node* raw_int = __ BytecodeOperandImm(0);
  Node* right = __ SmiTag(raw_int);
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(2);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Variable var_lhs_type_feedback(assembler, MachineRepresentation::kWord32);
  Node* lhs_value = __ TruncateTaggedToWord32WithFeedback(
      context, left, &var_lhs_type_feedback);
  Node* rhs_value = __ SmiToWord32(right);
  Node* shift_count = __ Word32And(rhs_value, __ Int32Constant(0x1f));
  Node* value = __ Word32Sar(lhs_value, shift_count);
  Node* result = __ ChangeInt32ToTagged(value);
  Node* result_type =
      __ Select(__ WordIsSmi(result),
                __ Int32Constant(BinaryOperationFeedback::kSignedSmall),
                __ Int32Constant(BinaryOperationFeedback::kNumber));
  __ UpdateFeedback(__ Word32Or(result_type, var_lhs_type_feedback.value()),
                    type_feedback_vector, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}

Node* Interpreter::BuildUnaryOp(Callable callable,
                                InterpreterAssembler* assembler) {
  Node* target = __ HeapConstant(callable.code());
  Node* accumulator = __ GetAccumulator();
  Node* context = __ GetContext();
  return __ CallStub(callable.descriptor(), target, context, accumulator);
}

template <class Generator>
void Interpreter::DoUnaryOp(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = Generator::Generate(assembler, value, context);
  __ SetAccumulator(result);
  __ Dispatch();
}

template <class Generator>
void Interpreter::DoUnaryOpWithFeedback(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* slot_index = __ BytecodeOperandIdx(0);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* result = Generator::Generate(assembler, value, context,
                                     type_feedback_vector, slot_index);
  __ SetAccumulator(result);
  __ Dispatch();
}

// ToName
//
// Convert the object referenced by the accumulator to a name.
void Interpreter::DoToName(InterpreterAssembler* assembler) {
  Node* object = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ ToName(context, object);
  __ StoreRegister(result, __ BytecodeOperandReg(0));
  __ Dispatch();
}

// ToNumber
//
// Convert the object referenced by the accumulator to a number.
void Interpreter::DoToNumber(InterpreterAssembler* assembler) {
  Node* object = __ GetAccumulator();
  Node* context = __ GetContext();
  Node* result = __ ToNumber(context, object);
  __ StoreRegister(result, __ BytecodeOperandReg(0));
  __ Dispatch();
}

// ToObject
//
// Convert the object referenced by the accumulator to a JSReceiver.
void Interpreter::DoToObject(InterpreterAssembler* assembler) {
  Node* result = BuildUnaryOp(CodeFactory::ToObject(isolate_), assembler);
  __ StoreRegister(result, __ BytecodeOperandReg(0));
  __ Dispatch();
}

// Inc
//
// Increments value in the accumulator by one.
void Interpreter::DoInc(InterpreterAssembler* assembler) {
  DoUnaryOpWithFeedback<IncStub>(assembler);
}

// Dec
//
// Decrements value in the accumulator by one.
void Interpreter::DoDec(InterpreterAssembler* assembler) {
  DoUnaryOpWithFeedback<DecStub>(assembler);
}

// LogicalNot
//
// Perform logical-not on the accumulator, first casting the
// accumulator to a boolean value if required.
// ToBooleanLogicalNot
void Interpreter::DoToBooleanLogicalNot(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Variable result(assembler, MachineRepresentation::kTagged);
  Label if_true(assembler), if_false(assembler), end(assembler);
  Node* true_value = __ BooleanConstant(true);
  Node* false_value = __ BooleanConstant(false);
  __ BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  __ Bind(&if_true);
  {
    result.Bind(false_value);
    __ Goto(&end);
  }
  __ Bind(&if_false);
  {
    result.Bind(true_value);
    __ Goto(&end);
  }
  __ Bind(&end);
  __ SetAccumulator(result.value());
  __ Dispatch();
}

// LogicalNot
//
// Perform logical-not on the accumulator, which must already be a boolean
// value.
void Interpreter::DoLogicalNot(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Variable result(assembler, MachineRepresentation::kTagged);
  Label if_true(assembler), if_false(assembler), end(assembler);
  Node* true_value = __ BooleanConstant(true);
  Node* false_value = __ BooleanConstant(false);
  __ BranchIfWordEqual(value, true_value, &if_true, &if_false);
  __ Bind(&if_true);
  {
    result.Bind(false_value);
    __ Goto(&end);
  }
  __ Bind(&if_false);
  {
    if (FLAG_debug_code) {
      __ AbortIfWordNotEqual(value, false_value,
                             BailoutReason::kExpectedBooleanValue);
    }
    result.Bind(true_value);
    __ Goto(&end);
  }
  __ Bind(&end);
  __ SetAccumulator(result.value());
  __ Dispatch();
}

// TypeOf
//
// Load the accumulator with the string representating type of the
// object in the accumulator.
void Interpreter::DoTypeOf(InterpreterAssembler* assembler) {
  DoUnaryOp<TypeofStub>(assembler);
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
  Node* slot_id = __ BytecodeOperandIdx(3);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  Node* result =
      __ CallJSWithFeedback(function, context, receiver_arg, args_count,
                            slot_id, type_feedback_vector, tail_call_mode);
  __ SetAccumulator(result);
  __ Dispatch();
}

// Call <callable> <receiver> <arg_count> <feedback_slot_id>
//
// Call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers. Collect type feedback
// into |feedback_slot_id|
void Interpreter::DoCall(InterpreterAssembler* assembler) {
  DoJSCall(assembler, TailCallMode::kDisallow);
}

// TailCall <callable> <receiver> <arg_count> <feedback_slot_id>
//
// Tail call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers. Collect type feedback
// into |feedback_slot_id|
void Interpreter::DoTailCall(InterpreterAssembler* assembler) {
  DoJSCall(assembler, TailCallMode::kAllow);
}

// CallRuntime <function_id> <first_arg> <arg_count>
//
// Call the runtime function |function_id| with the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers.
void Interpreter::DoCallRuntime(InterpreterAssembler* assembler) {
  Node* function_id = __ BytecodeOperandRuntimeId(0);
  Node* first_arg_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(first_arg_reg);
  Node* args_count = __ BytecodeOperandCount(2);
  Node* context = __ GetContext();
  Node* result = __ CallRuntimeN(function_id, context, first_arg, args_count);
  __ SetAccumulator(result);
  __ Dispatch();
}

// InvokeIntrinsic <function_id> <first_arg> <arg_count>
//
// Implements the semantic equivalent of calling the runtime function
// |function_id| with the first argument in |first_arg| and |arg_count|
// arguments in subsequent registers.
void Interpreter::DoInvokeIntrinsic(InterpreterAssembler* assembler) {
  Node* function_id = __ BytecodeOperandIntrinsicId(0);
  Node* first_arg_reg = __ BytecodeOperandReg(1);
  Node* arg_count = __ BytecodeOperandCount(2);
  Node* context = __ GetContext();
  IntrinsicsHelper helper(assembler);
  Node* result =
      helper.InvokeIntrinsic(function_id, context, first_arg_reg, arg_count);
  __ SetAccumulator(result);
  __ Dispatch();
}

// CallRuntimeForPair <function_id> <first_arg> <arg_count> <first_return>
//
// Call the runtime function |function_id| which returns a pair, with the
// first argument in register |first_arg| and |arg_count| arguments in
// subsequent registers. Returns the result in <first_return> and
// <first_return + 1>
void Interpreter::DoCallRuntimeForPair(InterpreterAssembler* assembler) {
  // Call the runtime function.
  Node* function_id = __ BytecodeOperandRuntimeId(0);
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

// CallJSRuntime <context_index> <receiver> <arg_count>
//
// Call the JS runtime function that has the |context_index| with the receiver
// in register |receiver| and |arg_count| arguments in subsequent registers.
void Interpreter::DoCallJSRuntime(InterpreterAssembler* assembler) {
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

// New <constructor> <first_arg> <arg_count>
//
// Call operator new with |constructor| and the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers. The new.target is in the accumulator.
//
void Interpreter::DoNew(InterpreterAssembler* assembler) {
  Callable ic = CodeFactory::InterpreterPushArgsAndConstruct(isolate_);
  Node* new_target = __ GetAccumulator();
  Node* constructor_reg = __ BytecodeOperandReg(0);
  Node* constructor = __ LoadRegister(constructor_reg);
  Node* first_arg_reg = __ BytecodeOperandReg(1);
  Node* first_arg = __ RegisterLocation(first_arg_reg);
  Node* args_count = __ BytecodeOperandCount(2);
  Node* slot_id = __ BytecodeOperandIdx(3);
  Node* type_feedback_vector = __ LoadTypeFeedbackVector();
  Node* context = __ GetContext();
  Node* result = __ CallConstruct(constructor, context, new_target, first_arg,
                                  args_count, slot_id, type_feedback_vector);
  __ SetAccumulator(result);
  __ Dispatch();
}

// TestEqual <src>
//
// Test if the value in the <src> register equals the accumulator.
void Interpreter::DoTestEqual(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<EqualStub>(assembler);
}

// TestNotEqual <src>
//
// Test if the value in the <src> register is not equal to the accumulator.
void Interpreter::DoTestNotEqual(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<NotEqualStub>(assembler);
}

// TestEqualStrict <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
void Interpreter::DoTestEqualStrict(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<StrictEqualStub>(assembler);
}

// TestLessThan <src>
//
// Test if the value in the <src> register is less than the accumulator.
void Interpreter::DoTestLessThan(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<LessThanStub>(assembler);
}

// TestGreaterThan <src>
//
// Test if the value in the <src> register is greater than the accumulator.
void Interpreter::DoTestGreaterThan(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<GreaterThanStub>(assembler);
}

// TestLessThanOrEqual <src>
//
// Test if the value in the <src> register is less than or equal to the
// accumulator.
void Interpreter::DoTestLessThanOrEqual(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<LessThanOrEqualStub>(assembler);
}

// TestGreaterThanOrEqual <src>
//
// Test if the value in the <src> register is greater than or equal to the
// accumulator.
void Interpreter::DoTestGreaterThanOrEqual(InterpreterAssembler* assembler) {
  DoCompareOpWithFeedback<GreaterThanOrEqualStub>(assembler);
}

// TestIn <src>
//
// Test if the object referenced by the register operand is a property of the
// object referenced by the accumulator.
void Interpreter::DoTestIn(InterpreterAssembler* assembler) {
  DoBinaryOp<HasPropertyStub>(assembler);
}

// TestInstanceOf <src>
//
// Test if the object referenced by the <src> register is an an instance of type
// referenced by the accumulator.
void Interpreter::DoTestInstanceOf(InterpreterAssembler* assembler) {
  DoBinaryOp<InstanceOfStub>(assembler);
}

// Jump <imm>
//
// Jump by number of bytes represented by the immediate operand |imm|.
void Interpreter::DoJump(InterpreterAssembler* assembler) {
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ Jump(relative_jump);
}

// JumpConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool.
void Interpreter::DoJumpConstant(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  __ Jump(relative_jump);
}

// JumpIfTrue <imm>
//
// Jump by number of bytes represented by an immediate operand if the
// accumulator contains true.
void Interpreter::DoJumpIfTrue(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(accumulator, true_value, relative_jump);
}

// JumpIfTrueConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the accumulator contains true.
void Interpreter::DoJumpIfTrueConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  Node* true_value = __ BooleanConstant(true);
  __ JumpIfWordEqual(accumulator, true_value, relative_jump);
}

// JumpIfFalse <imm>
//
// Jump by number of bytes represented by an immediate operand if the
// accumulator contains false.
void Interpreter::DoJumpIfFalse(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(accumulator, false_value, relative_jump);
}

// JumpIfFalseConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the accumulator contains false.
void Interpreter::DoJumpIfFalseConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  Node* false_value = __ BooleanConstant(false);
  __ JumpIfWordEqual(accumulator, false_value, relative_jump);
}

// JumpIfToBooleanTrue <imm>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is true when the object is cast to boolean.
void Interpreter::DoJumpIfToBooleanTrue(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm(0);
  Label if_true(assembler), if_false(assembler);
  __ BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  __ Bind(&if_true);
  __ Jump(relative_jump);
  __ Bind(&if_false);
  __ Dispatch();
}

// JumpIfToBooleanTrueConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the object referenced by the accumulator is true when the object is cast
// to boolean.
void Interpreter::DoJumpIfToBooleanTrueConstant(
    InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  Label if_true(assembler), if_false(assembler);
  __ BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  __ Bind(&if_true);
  __ Jump(relative_jump);
  __ Bind(&if_false);
  __ Dispatch();
}

// JumpIfToBooleanFalse <imm>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is false when the object is cast to boolean.
void Interpreter::DoJumpIfToBooleanFalse(InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* relative_jump = __ BytecodeOperandImm(0);
  Label if_true(assembler), if_false(assembler);
  __ BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  __ Bind(&if_true);
  __ Dispatch();
  __ Bind(&if_false);
  __ Jump(relative_jump);
}

// JumpIfToBooleanFalseConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the object referenced by the accumulator is false when the object is cast
// to boolean.
void Interpreter::DoJumpIfToBooleanFalseConstant(
    InterpreterAssembler* assembler) {
  Node* value = __ GetAccumulator();
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  Label if_true(assembler), if_false(assembler);
  __ BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  __ Bind(&if_true);
  __ Dispatch();
  __ Bind(&if_false);
  __ Jump(relative_jump);
}

// JumpIfNull <imm>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the null constant.
void Interpreter::DoJumpIfNull(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* null_value = __ HeapConstant(isolate_->factory()->null_value());
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ JumpIfWordEqual(accumulator, null_value, relative_jump);
}

// JumpIfNullConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the object referenced by the accumulator is the null constant.
void Interpreter::DoJumpIfNullConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* null_value = __ HeapConstant(isolate_->factory()->null_value());
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  __ JumpIfWordEqual(accumulator, null_value, relative_jump);
}

// JumpIfUndefined <imm>
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

// JumpIfUndefinedConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the object referenced by the accumulator is the undefined constant.
void Interpreter::DoJumpIfUndefinedConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* undefined_value =
      __ HeapConstant(isolate_->factory()->undefined_value());
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  __ JumpIfWordEqual(accumulator, undefined_value, relative_jump);
}

// JumpIfNotHole <imm>
//
// Jump by number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the hole.
void Interpreter::DoJumpIfNotHole(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* the_hole_value = __ HeapConstant(isolate_->factory()->the_hole_value());
  Node* relative_jump = __ BytecodeOperandImm(0);
  __ JumpIfWordNotEqual(accumulator, the_hole_value, relative_jump);
}

// JumpIfNotHoleConstant <idx>
//
// Jump by number of bytes in the Smi in the |idx| entry in the constant pool
// if the object referenced by the accumulator is the hole constant.
void Interpreter::DoJumpIfNotHoleConstant(InterpreterAssembler* assembler) {
  Node* accumulator = __ GetAccumulator();
  Node* the_hole_value = __ HeapConstant(isolate_->factory()->the_hole_value());
  Node* index = __ BytecodeOperandIdx(0);
  Node* relative_jump = __ LoadAndUntagConstantPoolEntry(index);
  __ JumpIfWordNotEqual(accumulator, the_hole_value, relative_jump);
}

// JumpLoop <imm> <loop_depth>
//
// Jump by number of bytes represented by the immediate operand |imm|. Also
// performs a loop nesting check and potentially triggers OSR in case the
// current OSR level matches (or exceeds) the specified |loop_depth|.
void Interpreter::DoJumpLoop(InterpreterAssembler* assembler) {
  Node* relative_jump = __ BytecodeOperandImm(0);
  Node* loop_depth = __ BytecodeOperandImm(1);
  Node* osr_level = __ LoadOSRNestingLevel();

  // Check if OSR points at the given {loop_depth} are armed by comparing it to
  // the current {osr_level} loaded from the header of the BytecodeArray.
  Label ok(assembler), osr_armed(assembler, Label::kDeferred);
  Node* condition = __ Int32GreaterThanOrEqual(loop_depth, osr_level);
  __ Branch(condition, &ok, &osr_armed);

  __ Bind(&ok);
  __ Jump(relative_jump);

  __ Bind(&osr_armed);
  {
    Callable callable = CodeFactory::InterpreterOnStackReplacement(isolate_);
    Node* target = __ HeapConstant(callable.code());
    Node* context = __ GetContext();
    __ CallStub(callable.descriptor(), target, context);
    __ Jump(relative_jump);
  }
}

// CreateRegExpLiteral <pattern_idx> <literal_idx> <flags>
//
// Creates a regular expression literal for literal index <literal_idx> with
// <flags> and the pattern in <pattern_idx>.
void Interpreter::DoCreateRegExpLiteral(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* pattern = __ LoadConstantPoolEntry(index);
  Node* literal_index_raw = __ BytecodeOperandIdx(1);
  Node* literal_index = __ SmiTag(literal_index_raw);
  Node* flags_raw = __ BytecodeOperandFlag(2);
  Node* flags = __ SmiTag(flags_raw);
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* context = __ GetContext();
  Node* result = FastCloneRegExpStub::Generate(
      assembler, closure, literal_index, pattern, flags, context);
  __ SetAccumulator(result);
  __ Dispatch();
}

// CreateArrayLiteral <element_idx> <literal_idx> <flags>
//
// Creates an array literal for literal index <literal_idx> with
// CreateArrayLiteral flags <flags> and constant elements in <element_idx>.
void Interpreter::DoCreateArrayLiteral(InterpreterAssembler* assembler) {
  Node* literal_index_raw = __ BytecodeOperandIdx(1);
  Node* literal_index = __ SmiTag(literal_index_raw);
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* context = __ GetContext();
  Node* bytecode_flags = __ BytecodeOperandFlag(2);

  Label fast_shallow_clone(assembler),
      call_runtime(assembler, Label::kDeferred);
  Node* use_fast_shallow_clone = __ Word32And(
      bytecode_flags,
      __ Int32Constant(CreateArrayLiteralFlags::FastShallowCloneBit::kMask));
  __ BranchIf(use_fast_shallow_clone, &fast_shallow_clone, &call_runtime);

  __ Bind(&fast_shallow_clone);
  {
    DCHECK(FLAG_allocation_site_pretenuring);
    Node* result = FastCloneShallowArrayStub::Generate(
        assembler, closure, literal_index, context, &call_runtime,
        TRACK_ALLOCATION_SITE);
    __ SetAccumulator(result);
    __ Dispatch();
  }

  __ Bind(&call_runtime);
  {
    STATIC_ASSERT(CreateArrayLiteralFlags::FlagsBits::kShift == 0);
    Node* flags_raw = __ Word32And(
        bytecode_flags,
        __ Int32Constant(CreateArrayLiteralFlags::FlagsBits::kMask));
    Node* flags = __ SmiTag(flags_raw);
    Node* index = __ BytecodeOperandIdx(0);
    Node* constant_elements = __ LoadConstantPoolEntry(index);
    Node* result =
        __ CallRuntime(Runtime::kCreateArrayLiteral, context, closure,
                       literal_index, constant_elements, flags);
    __ SetAccumulator(result);
    __ Dispatch();
  }
}

// CreateObjectLiteral <element_idx> <literal_idx> <flags>
//
// Creates an object literal for literal index <literal_idx> with
// CreateObjectLiteralFlags <flags> and constant elements in <element_idx>.
void Interpreter::DoCreateObjectLiteral(InterpreterAssembler* assembler) {
  Node* literal_index_raw = __ BytecodeOperandIdx(1);
  Node* literal_index = __ SmiTag(literal_index_raw);
  Node* bytecode_flags = __ BytecodeOperandFlag(2);
  Node* closure = __ LoadRegister(Register::function_closure());

  // Check if we can do a fast clone or have to call the runtime.
  Label if_fast_clone(assembler),
      if_not_fast_clone(assembler, Label::kDeferred);
  Node* fast_clone_properties_count =
      __ BitFieldDecode<CreateObjectLiteralFlags::FastClonePropertiesCountBits>(
          bytecode_flags);
  __ BranchIf(fast_clone_properties_count, &if_fast_clone, &if_not_fast_clone);

  __ Bind(&if_fast_clone);
  {
    // If we can do a fast clone do the fast-path in FastCloneShallowObjectStub.
    Node* result = FastCloneShallowObjectStub::GenerateFastPath(
        assembler, &if_not_fast_clone, closure, literal_index,
        fast_clone_properties_count);
    __ StoreRegister(result, __ BytecodeOperandReg(3));
    __ Dispatch();
  }

  __ Bind(&if_not_fast_clone);
  {
    // If we can't do a fast clone, call into the runtime.
    Node* index = __ BytecodeOperandIdx(0);
    Node* constant_elements = __ LoadConstantPoolEntry(index);
    Node* context = __ GetContext();

    STATIC_ASSERT(CreateObjectLiteralFlags::FlagsBits::kShift == 0);
    Node* flags_raw = __ Word32And(
        bytecode_flags,
        __ Int32Constant(CreateObjectLiteralFlags::FlagsBits::kMask));
    Node* flags = __ SmiTag(flags_raw);

    Node* result =
        __ CallRuntime(Runtime::kCreateObjectLiteral, context, closure,
                       literal_index, constant_elements, flags);
    __ StoreRegister(result, __ BytecodeOperandReg(3));
    // TODO(klaasb) build a single dispatch once the call is inlined
    __ Dispatch();
  }
}

// CreateClosure <index> <tenured>
//
// Creates a new closure for SharedFunctionInfo at position |index| in the
// constant pool and with the PretenureFlag <tenured>.
void Interpreter::DoCreateClosure(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* shared = __ LoadConstantPoolEntry(index);
  Node* flags = __ BytecodeOperandFlag(1);
  Node* context = __ GetContext();

  Label call_runtime(assembler, Label::kDeferred);
  Node* fast_new_closure = __ Word32And(
      flags, __ Int32Constant(CreateClosureFlags::FastNewClosureBit::kMask));
  __ GotoUnless(fast_new_closure, &call_runtime);
  __ SetAccumulator(FastNewClosureStub::Generate(assembler, shared, context));
  __ Dispatch();

  __ Bind(&call_runtime);
  {
    STATIC_ASSERT(CreateClosureFlags::PretenuredBit::kShift == 0);
    Node* tenured_raw = __ Word32And(
        flags, __ Int32Constant(CreateClosureFlags::PretenuredBit::kMask));
    Node* tenured = __ SmiTag(tenured_raw);
    Node* result = __ CallRuntime(Runtime::kInterpreterNewClosure, context,
                                  shared, tenured);
    __ SetAccumulator(result);
    __ Dispatch();
  }
}

// CreateBlockContext <index>
//
// Creates a new block context with the scope info constant at |index| and the
// closure in the accumulator.
void Interpreter::DoCreateBlockContext(InterpreterAssembler* assembler) {
  Node* index = __ BytecodeOperandIdx(0);
  Node* scope_info = __ LoadConstantPoolEntry(index);
  Node* closure = __ GetAccumulator();
  Node* context = __ GetContext();
  __ SetAccumulator(
      __ CallRuntime(Runtime::kPushBlockContext, context, scope_info, closure));
  __ Dispatch();
}

// CreateCatchContext <exception> <name_idx> <scope_info_idx>
//
// Creates a new context for a catch block with the |exception| in a register,
// the variable name at |name_idx|, the ScopeInfo at |scope_info_idx|, and the
// closure in the accumulator.
void Interpreter::DoCreateCatchContext(InterpreterAssembler* assembler) {
  Node* exception_reg = __ BytecodeOperandReg(0);
  Node* exception = __ LoadRegister(exception_reg);
  Node* name_idx = __ BytecodeOperandIdx(1);
  Node* name = __ LoadConstantPoolEntry(name_idx);
  Node* scope_info_idx = __ BytecodeOperandIdx(2);
  Node* scope_info = __ LoadConstantPoolEntry(scope_info_idx);
  Node* closure = __ GetAccumulator();
  Node* context = __ GetContext();
  __ SetAccumulator(__ CallRuntime(Runtime::kPushCatchContext, context, name,
                                   exception, scope_info, closure));
  __ Dispatch();
}

// CreateFunctionContext <slots>
//
// Creates a new context with number of |slots| for the function closure.
void Interpreter::DoCreateFunctionContext(InterpreterAssembler* assembler) {
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* slots = __ BytecodeOperandUImm(0);
  Node* context = __ GetContext();
  __ SetAccumulator(
      FastNewFunctionContextStub::Generate(assembler, closure, slots, context));
  __ Dispatch();
}

// CreateWithContext <register> <scope_info_idx>
//
// Creates a new context with the ScopeInfo at |scope_info_idx| for a
// with-statement with the object in |register| and the closure in the
// accumulator.
void Interpreter::DoCreateWithContext(InterpreterAssembler* assembler) {
  Node* reg_index = __ BytecodeOperandReg(0);
  Node* object = __ LoadRegister(reg_index);
  Node* scope_info_idx = __ BytecodeOperandIdx(1);
  Node* scope_info = __ LoadConstantPoolEntry(scope_info_idx);
  Node* closure = __ GetAccumulator();
  Node* context = __ GetContext();
  __ SetAccumulator(__ CallRuntime(Runtime::kPushWithContext, context, object,
                                   scope_info, closure));
  __ Dispatch();
}

// CreateMappedArguments
//
// Creates a new mapped arguments object.
void Interpreter::DoCreateMappedArguments(InterpreterAssembler* assembler) {
  Node* closure = __ LoadRegister(Register::function_closure());
  Node* context = __ GetContext();

  Label if_duplicate_parameters(assembler, Label::kDeferred);
  Label if_not_duplicate_parameters(assembler);

  // Check if function has duplicate parameters.
  // TODO(rmcilroy): Remove this check when FastNewSloppyArgumentsStub supports
  // duplicate parameters.
  Node* shared_info =
      __ LoadObjectField(closure, JSFunction::kSharedFunctionInfoOffset);
  Node* compiler_hints = __ LoadObjectField(
      shared_info, SharedFunctionInfo::kHasDuplicateParametersByteOffset,
      MachineType::Uint8());
  Node* duplicate_parameters_bit = __ Int32Constant(
      1 << SharedFunctionInfo::kHasDuplicateParametersBitWithinByte);
  Node* compare = __ Word32And(compiler_hints, duplicate_parameters_bit);
  __ BranchIf(compare, &if_duplicate_parameters, &if_not_duplicate_parameters);

  __ Bind(&if_not_duplicate_parameters);
  {
    // TODO(rmcilroy): Inline FastNewSloppyArguments when it is a TurboFan stub.
    Callable callable = CodeFactory::FastNewSloppyArguments(isolate_, true);
    Node* target = __ HeapConstant(callable.code());
    Node* result = __ CallStub(callable.descriptor(), target, context, closure);
    __ SetAccumulator(result);
    __ Dispatch();
  }

  __ Bind(&if_duplicate_parameters);
  {
    Node* result =
        __ CallRuntime(Runtime::kNewSloppyArguments_Generic, context, closure);
    __ SetAccumulator(result);
    __ Dispatch();
  }
}

// CreateUnmappedArguments
//
// Creates a new unmapped arguments object.
void Interpreter::DoCreateUnmappedArguments(InterpreterAssembler* assembler) {
  // TODO(rmcilroy): Inline FastNewStrictArguments when it is a TurboFan stub.
  Callable callable = CodeFactory::FastNewStrictArguments(isolate_, true);
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
  // TODO(rmcilroy): Inline FastNewRestArguments when it is a TurboFan stub.
  Callable callable = CodeFactory::FastNewRestParameter(isolate_, true);
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
  Label ok(assembler), stack_check_interrupt(assembler, Label::kDeferred);

  Node* interrupt = __ StackCheckTriggeredInterrupt();
  __ BranchIf(interrupt, &stack_check_interrupt, &ok);

  __ Bind(&ok);
  __ Dispatch();

  __ Bind(&stack_check_interrupt);
  {
    Node* context = __ GetContext();
    __ CallRuntime(Runtime::kStackGuard, context);
    __ Dispatch();
  }
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
  __ UpdateInterruptBudgetOnReturn();
  Node* accumulator = __ GetAccumulator();
  __ Return(accumulator);
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
#define DEBUG_BREAK(Name, ...)                                                \
  void Interpreter::Do##Name(InterpreterAssembler* assembler) {               \
    Node* context = __ GetContext();                                          \
    Node* accumulator = __ GetAccumulator();                                  \
    Node* original_handler =                                                  \
        __ CallRuntime(Runtime::kDebugBreakOnBytecode, context, accumulator); \
    __ DispatchToBytecodeHandler(original_handler);                           \
  }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK);
#undef DEBUG_BREAK

void Interpreter::BuildForInPrepareResult(Node* output_register,
                                          Node* cache_type, Node* cache_array,
                                          Node* cache_length,
                                          InterpreterAssembler* assembler) {
  __ StoreRegister(cache_type, output_register);
  output_register = __ NextRegister(output_register);
  __ StoreRegister(cache_array, output_register);
  output_register = __ NextRegister(output_register);
  __ StoreRegister(cache_length, output_register);
}

// ForInPrepare <receiver> <cache_info_triple>
//
// Returns state for for..in loop execution based on the object in the register
// |receiver|. The object must not be null or undefined and must have been
// converted to a receiver already.
// The result is output in registers |cache_info_triple| to
// |cache_info_triple + 2|, with the registers holding cache_type, cache_array,
// and cache_length respectively.
void Interpreter::DoForInPrepare(InterpreterAssembler* assembler) {
  Node* object_reg = __ BytecodeOperandReg(0);
  Node* receiver = __ LoadRegister(object_reg);
  Node* context = __ GetContext();
  Node* const zero_smi = __ SmiConstant(Smi::FromInt(0));

  Label nothing_to_iterate(assembler, Label::kDeferred),
      use_enum_cache(assembler), use_runtime(assembler, Label::kDeferred);

  if (FLAG_debug_code) {
    Label already_receiver(assembler), abort(assembler);
    Node* instance_type = __ LoadInstanceType(receiver);
    __ Branch(__ IsJSReceiverInstanceType(instance_type), &already_receiver,
              &abort);
    __ Bind(&abort);
    {
      __ Abort(kExpectedJSReceiver);
      // TODO(klaasb) remove this unreachable Goto once Abort ends the block
      __ Goto(&already_receiver);
    }
    __ Bind(&already_receiver);
  }

  __ CheckEnumCache(receiver, &use_enum_cache, &use_runtime);

  __ Bind(&use_enum_cache);
  {
    // The enum cache is valid.  Load the map of the object being
    // iterated over and use the cache for the iteration.
    Node* cache_type = __ LoadMap(receiver);
    Node* cache_length = __ EnumLength(cache_type);
    __ GotoIf(assembler->WordEqual(cache_length, zero_smi),
              &nothing_to_iterate);
    Node* descriptors = __ LoadMapDescriptors(cache_type);
    Node* cache_offset =
        __ LoadObjectField(descriptors, DescriptorArray::kEnumCacheOffset);
    Node* cache_array = __ LoadObjectField(
        cache_offset, DescriptorArray::kEnumCacheBridgeCacheOffset);
    Node* output_register = __ BytecodeOperandReg(1);
    BuildForInPrepareResult(output_register, cache_type, cache_array,
                            cache_length, assembler);
    __ Dispatch();
  }

  __ Bind(&use_runtime);
  {
    Node* result_triple =
        __ CallRuntime(Runtime::kForInPrepare, context, receiver);
    Node* cache_type = __ Projection(0, result_triple);
    Node* cache_array = __ Projection(1, result_triple);
    Node* cache_length = __ Projection(2, result_triple);
    Node* output_register = __ BytecodeOperandReg(1);
    BuildForInPrepareResult(output_register, cache_type, cache_array,
                            cache_length, assembler);
    __ Dispatch();
  }

  __ Bind(&nothing_to_iterate);
  {
    // Receiver is null or undefined or descriptors are zero length.
    Node* output_register = __ BytecodeOperandReg(1);
    BuildForInPrepareResult(output_register, zero_smi, zero_smi, zero_smi,
                            assembler);
    __ Dispatch();
  }
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

  // Load the next key from the enumeration array.
  Node* key = __ LoadFixedArrayElement(cache_array, index, 0,
                                       CodeStubAssembler::SMI_PARAMETERS);

  // Check if we can use the for-in fast path potentially using the enum cache.
  Label if_fast(assembler), if_slow(assembler, Label::kDeferred);
  Node* receiver_map = __ LoadObjectField(receiver, HeapObject::kMapOffset);
  __ BranchIfWordEqual(receiver_map, cache_type, &if_fast, &if_slow);
  __ Bind(&if_fast);
  {
    // Enum cache in use for {receiver}, the {key} is definitely valid.
    __ SetAccumulator(key);
    __ Dispatch();
  }
  __ Bind(&if_slow);
  {
    // Record the fact that we hit the for-in slow path.
    Node* vector_index = __ BytecodeOperandIdx(3);
    Node* type_feedback_vector = __ LoadTypeFeedbackVector();
    Node* megamorphic_sentinel =
        __ HeapConstant(TypeFeedbackVector::MegamorphicSentinel(isolate_));
    __ StoreFixedArrayElement(type_feedback_vector, vector_index,
                              megamorphic_sentinel, SKIP_WRITE_BARRIER);

    // Need to filter the {key} for the {receiver}.
    Node* context = __ GetContext();
    Callable callable = CodeFactory::ForInFilter(assembler->isolate());
    Node* result = __ CallStub(callable, context, key, receiver);
    __ SetAccumulator(result);
    __ Dispatch();
  }
}

// ForInContinue <index> <cache_length>
//
// Returns false if the end of the enumerable properties has been reached.
void Interpreter::DoForInContinue(InterpreterAssembler* assembler) {
  Node* index_reg = __ BytecodeOperandReg(0);
  Node* index = __ LoadRegister(index_reg);
  Node* cache_length_reg = __ BytecodeOperandReg(1);
  Node* cache_length = __ LoadRegister(cache_length_reg);

  // Check if {index} is at {cache_length} already.
  Label if_true(assembler), if_false(assembler), end(assembler);
  __ BranchIfWordEqual(index, cache_length, &if_true, &if_false);
  __ Bind(&if_true);
  {
    __ SetAccumulator(__ BooleanConstant(false));
    __ Goto(&end);
  }
  __ Bind(&if_false);
  {
    __ SetAccumulator(__ BooleanConstant(true));
    __ Goto(&end);
  }
  __ Bind(&end);
  __ Dispatch();
}

// ForInStep <index>
//
// Increments the loop counter in register |index| and stores the result
// in the accumulator.
void Interpreter::DoForInStep(InterpreterAssembler* assembler) {
  Node* index_reg = __ BytecodeOperandReg(0);
  Node* index = __ LoadRegister(index_reg);
  Node* one = __ SmiConstant(Smi::FromInt(1));
  Node* result = __ SmiAdd(index, one);
  __ SetAccumulator(result);
  __ Dispatch();
}

// Wide
//
// Prefix bytecode indicating next bytecode has wide (16-bit) operands.
void Interpreter::DoWide(InterpreterAssembler* assembler) {
  __ DispatchWide(OperandScale::kDouble);
}

// ExtraWide
//
// Prefix bytecode indicating next bytecode has extra-wide (32-bit) operands.
void Interpreter::DoExtraWide(InterpreterAssembler* assembler) {
  __ DispatchWide(OperandScale::kQuadruple);
}

// Illegal
//
// An invalid bytecode aborting execution if dispatched.
void Interpreter::DoIllegal(InterpreterAssembler* assembler) {
  __ Abort(kInvalidBytecode);
}

// Nop
//
// No operation.
void Interpreter::DoNop(InterpreterAssembler* assembler) { __ Dispatch(); }

// SuspendGenerator <generator>
//
// Exports the register file and stores it into the generator.  Also stores the
// current context, the state given in the accumulator, and the current bytecode
// offset (for debugging purposes) into the generator.
void Interpreter::DoSuspendGenerator(InterpreterAssembler* assembler) {
  Node* generator_reg = __ BytecodeOperandReg(0);
  Node* generator = __ LoadRegister(generator_reg);

  Label if_stepping(assembler, Label::kDeferred), ok(assembler);
  Node* step_action_address = __ ExternalConstant(
      ExternalReference::debug_last_step_action_address(isolate_));
  Node* step_action = __ Load(MachineType::Int8(), step_action_address);
  STATIC_ASSERT(StepIn > StepNext);
  STATIC_ASSERT(StepFrame > StepNext);
  STATIC_ASSERT(LastStepAction == StepFrame);
  Node* step_next = __ Int32Constant(StepNext);
  __ BranchIfInt32LessThanOrEqual(step_next, step_action, &if_stepping, &ok);
  __ Bind(&ok);

  Node* array =
      __ LoadObjectField(generator, JSGeneratorObject::kOperandStackOffset);
  Node* context = __ GetContext();
  Node* state = __ GetAccumulator();

  __ ExportRegisterFile(array);
  __ StoreObjectField(generator, JSGeneratorObject::kContextOffset, context);
  __ StoreObjectField(generator, JSGeneratorObject::kContinuationOffset, state);

  Node* offset = __ SmiTag(__ BytecodeOffset());
  __ StoreObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset,
                      offset);

  __ Dispatch();

  __ Bind(&if_stepping);
  {
    Node* context = __ GetContext();
    __ CallRuntime(Runtime::kDebugRecordAsyncFunction, context, generator);
    __ Goto(&ok);
  }
}

// ResumeGenerator <generator>
//
// Imports the register file stored in the generator. Also loads the
// generator's state and stores it in the accumulator, before overwriting it
// with kGeneratorExecuting.
void Interpreter::DoResumeGenerator(InterpreterAssembler* assembler) {
  Node* generator_reg = __ BytecodeOperandReg(0);
  Node* generator = __ LoadRegister(generator_reg);

  __ ImportRegisterFile(
      __ LoadObjectField(generator, JSGeneratorObject::kOperandStackOffset));

  Node* old_state =
      __ LoadObjectField(generator, JSGeneratorObject::kContinuationOffset);
  Node* new_state = __ Int32Constant(JSGeneratorObject::kGeneratorExecuting);
  __ StoreObjectField(generator, JSGeneratorObject::kContinuationOffset,
      __ SmiTag(new_state));
  __ SetAccumulator(old_state);

  __ Dispatch();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
