// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter.h"

#include <fstream>
#include <memory>

#include "builtins-generated/bytecodes-builtins-list.h"
#include "src/ast/prettyprinter.h"
#include "src/bootstrapper.h"
#include "src/compiler.h"
#include "src/counters-inl.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots.h"
#include "src/ostreams.h"
#include "src/parsing/parse-info.h"
#include "src/setup-isolate.h"
#include "src/snapshot/snapshot.h"
#include "src/unoptimized-compilation-info.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {
namespace interpreter {

class InterpreterCompilationJob final : public UnoptimizedCompilationJob {
 public:
  InterpreterCompilationJob(
      ParseInfo* parse_info, FunctionLiteral* literal,
      AccountingAllocator* allocator,
      std::vector<FunctionLiteral*>* eager_inner_literals);

 protected:
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl(Handle<SharedFunctionInfo> shared_info,
                         Isolate* isolate) final;

 private:
  BytecodeGenerator* generator() { return &generator_; }

  Zone zone_;
  UnoptimizedCompilationInfo compilation_info_;
  BytecodeGenerator generator_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterCompilationJob);
};

Interpreter::Interpreter(Isolate* isolate)
    : isolate_(isolate),
      interpreter_entry_trampoline_instruction_start_(kNullAddress) {
  memset(dispatch_table_, 0, sizeof(dispatch_table_));

  if (FLAG_trace_ignition_dispatches) {
    static const int kBytecodeCount = static_cast<int>(Bytecode::kLast) + 1;
    bytecode_dispatch_counters_table_.reset(
        new uintptr_t[kBytecodeCount * kBytecodeCount]);
    memset(bytecode_dispatch_counters_table_.get(), 0,
           sizeof(uintptr_t) * kBytecodeCount * kBytecodeCount);
  }
}

namespace {

int BuiltinIndexFromBytecode(Bytecode bytecode, OperandScale operand_scale) {
  int index = BytecodeOperands::OperandScaleAsIndex(operand_scale) *
                  kNumberOfBytecodeHandlers +
              static_cast<int>(bytecode);
  int offset = kBytecodeToBuiltinsMapping[index];
  return offset >= 0 ? Builtins::kFirstBytecodeHandler + offset
                     : Builtins::kIllegalHandler;
}

}  // namespace

Code Interpreter::GetBytecodeHandler(Bytecode bytecode,
                                     OperandScale operand_scale) {
  int builtin_index = BuiltinIndexFromBytecode(bytecode, operand_scale);
  Builtins* builtins = isolate_->builtins();
  return builtins->builtin(builtin_index);
}

void Interpreter::SetBytecodeHandler(Bytecode bytecode,
                                     OperandScale operand_scale, Code handler) {
  DCHECK(handler->kind() == Code::BYTECODE_HANDLER);
  size_t index = GetDispatchTableIndex(bytecode, operand_scale);
  dispatch_table_[index] = handler->InstructionStart();
}

// static
size_t Interpreter::GetDispatchTableIndex(Bytecode bytecode,
                                          OperandScale operand_scale) {
  static const size_t kEntriesPerOperandScale = 1u << kBitsPerByte;
  size_t index = static_cast<size_t>(bytecode);
  return index + BytecodeOperands::OperandScaleAsIndex(operand_scale) *
                     kEntriesPerOperandScale;
}

void Interpreter::IterateDispatchTable(RootVisitor* v) {
  if (FLAG_embedded_builtins && !isolate_->serializer_enabled() &&
      isolate_->embedded_blob() != nullptr) {
// If builtins are embedded (and we're not generating a snapshot), then
// every bytecode handler will be off-heap, so there's no point iterating
// over them.
#ifdef DEBUG
    for (int i = 0; i < kDispatchTableSize; i++) {
      Address code_entry = dispatch_table_[i];
      CHECK(code_entry == kNullAddress ||
            InstructionStream::PcIsOffHeap(isolate_, code_entry));
    }
#endif  // ENABLE_SLOW_DCHECKS
    return;
  }

  for (int i = 0; i < kDispatchTableSize; i++) {
    Address code_entry = dispatch_table_[i];
    // Skip over off-heap bytecode handlers since they will never move.
    if (InstructionStream::PcIsOffHeap(isolate_, code_entry)) continue;

    // TODO(jkummerow): Would it hurt to simply do:
    // if (code_entry == kNullAddress) continue;
    Code code;
    if (code_entry != kNullAddress) {
      code = Code::GetCodeFromTargetAddress(code_entry);
    }
    Code old_code = code;
    v->VisitRootPointer(Root::kDispatchTable, nullptr, FullObjectSlot(&code));
    if (code != old_code) {
      dispatch_table_[i] = code->entry();
    }
  }
}

int Interpreter::InterruptBudget() {
  return FLAG_interrupt_budget;
}

namespace {

void MaybePrintAst(ParseInfo* parse_info,
                   UnoptimizedCompilationInfo* compilation_info) {
  if (!FLAG_print_ast) return;

  StdoutStream os;
  std::unique_ptr<char[]> name = compilation_info->literal()->GetDebugName();
  os << "[generating bytecode for function: " << name.get() << "]" << std::endl;
#ifdef DEBUG
  os << "--- AST ---" << std::endl
     << AstPrinter(parse_info->stack_limit())
            .PrintProgram(compilation_info->literal())
     << std::endl;
#endif  // DEBUG
}

bool ShouldPrintBytecode(Handle<SharedFunctionInfo> shared) {
  if (!FLAG_print_bytecode) return false;

  // Checks whether function passed the filter.
  if (shared->is_toplevel()) {
    Vector<const char> filter = CStrVector(FLAG_print_bytecode_filter);
    return (filter.length() == 0) || (filter.length() == 1 && filter[0] == '*');
  } else {
    return shared->PassesFilter(FLAG_print_bytecode_filter);
  }
}

}  // namespace

InterpreterCompilationJob::InterpreterCompilationJob(
    ParseInfo* parse_info, FunctionLiteral* literal,
    AccountingAllocator* allocator,
    std::vector<FunctionLiteral*>* eager_inner_literals)
    : UnoptimizedCompilationJob(parse_info->stack_limit(), parse_info,
                                &compilation_info_),
      zone_(allocator, ZONE_NAME),
      compilation_info_(&zone_, parse_info, literal),
      generator_(&compilation_info_, parse_info->ast_string_constants(),
                 eager_inner_literals) {}

InterpreterCompilationJob::Status InterpreterCompilationJob::ExecuteJobImpl() {
  RuntimeCallTimerScope runtimeTimerScope(
      parse_info()->runtime_call_stats(),
      parse_info()->on_background_thread()
          ? RuntimeCallCounterId::kCompileBackgroundIgnition
          : RuntimeCallCounterId::kCompileIgnition);
  // TODO(lpy): add support for background compilation RCS trace.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileIgnition");

  // Print AST if flag is enabled. Note, if compiling on a background thread
  // then ASTs from different functions may be intersperse when printed.
  MaybePrintAst(parse_info(), compilation_info());

  generator()->GenerateBytecode(stack_limit());

  if (generator()->HasStackOverflow()) {
    return FAILED;
  }
  return SUCCEEDED;
}

InterpreterCompilationJob::Status InterpreterCompilationJob::FinalizeJobImpl(
    Handle<SharedFunctionInfo> shared_info, Isolate* isolate) {
  RuntimeCallTimerScope runtimeTimerScope(
      parse_info()->runtime_call_stats(),
      RuntimeCallCounterId::kCompileIgnitionFinalization);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileIgnitionFinalization");

  Handle<BytecodeArray> bytecodes =
      generator()->FinalizeBytecode(isolate, parse_info()->script());
  if (generator()->HasStackOverflow()) {
    return FAILED;
  }

  if (ShouldPrintBytecode(shared_info)) {
    StdoutStream os;
    std::unique_ptr<char[]> name =
        compilation_info()->literal()->GetDebugName();
    os << "[generated bytecode for function: " << name.get() << "]"
       << std::endl;
    bytecodes->Disassemble(os);
    os << std::flush;
  }

  compilation_info()->SetBytecodeArray(bytecodes);
  return SUCCEEDED;
}

UnoptimizedCompilationJob* Interpreter::NewCompilationJob(
    ParseInfo* parse_info, FunctionLiteral* literal,
    AccountingAllocator* allocator,
    std::vector<FunctionLiteral*>* eager_inner_literals) {
  return new InterpreterCompilationJob(parse_info, literal, allocator,
                                       eager_inner_literals);
}

void Interpreter::ForEachBytecode(
    const std::function<void(Bytecode, OperandScale)>& f) {
  constexpr OperandScale kOperandScales[] = {
#define VALUE(Name, _) OperandScale::k##Name,
      OPERAND_SCALE_LIST(VALUE)
#undef VALUE
  };

  for (OperandScale operand_scale : kOperandScales) {
    for (int i = 0; i < Bytecodes::kBytecodeCount; i++) {
      f(Bytecodes::FromByte(i), operand_scale);
    }
  }
}

void Interpreter::Initialize() {
  Builtins* builtins = isolate_->builtins();

  // Set the interpreter entry trampoline entry point now that builtins are
  // initialized.
  Handle<Code> code = BUILTIN_CODE(isolate_, InterpreterEntryTrampoline);
  DCHECK(builtins->is_initialized());
  DCHECK(code->is_off_heap_trampoline() ||
         isolate_->heap()->IsImmovable(*code));
  interpreter_entry_trampoline_instruction_start_ = code->InstructionStart();

  // Initialize the dispatch table.
  Code illegal = builtins->builtin(Builtins::kIllegalHandler);
  int builtin_id = Builtins::kFirstBytecodeHandler;
  ForEachBytecode([=, &builtin_id](Bytecode bytecode,
                                   OperandScale operand_scale) {
    Code handler = illegal;
    if (Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) {
#ifdef DEBUG
      std::string builtin_name(Builtins::name(builtin_id));
      std::string expected_name =
          Bytecodes::ToString(bytecode, operand_scale, "") + "Handler";
      DCHECK_EQ(expected_name, builtin_name);
#endif
      handler = builtins->builtin(builtin_id++);
    }
    SetBytecodeHandler(bytecode, operand_scale, handler);
  });
  DCHECK(builtin_id == Builtins::builtin_count);
  DCHECK(IsDispatchTableInitialized());
}

bool Interpreter::IsDispatchTableInitialized() const {
  return dispatch_table_[0] != kNullAddress;
}

const char* Interpreter::LookupNameOfBytecodeHandler(const Code code) {
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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
