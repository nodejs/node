// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter.h"

#include <fstream>
#include <memory>

#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/counters.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/bytecodes.h"
#include "src/log.h"
#include "src/objects.h"
#include "src/setup-isolate.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {
namespace interpreter {

class InterpreterCompilationJob final : public CompilationJob {
 public:
  InterpreterCompilationJob(ParseInfo* parse_info, FunctionLiteral* literal,
                            Isolate* isolate);

 protected:
  Status PrepareJobImpl() final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl() final;

 private:
  class TimerScope final {
   public:
    explicit TimerScope(RuntimeCallCounter* counter)
        : runtime_stats_enabled_(FLAG_runtime_stats) {
      if (V8_UNLIKELY(runtime_stats_enabled_ && counter != nullptr)) {
        timer_.Start(counter, nullptr);
      }
    }

    ~TimerScope() {
      if (V8_UNLIKELY(runtime_stats_enabled_)) {
        timer_.Stop();
      }
    }

   private:
    RuntimeCallTimer timer_;
    bool runtime_stats_enabled_;

    DISALLOW_COPY_AND_ASSIGN(TimerScope);
  };

  BytecodeGenerator* generator() { return &generator_; }

  Zone zone_;
  CompilationInfo compilation_info_;
  BytecodeGenerator generator_;
  RuntimeCallStats* runtime_call_stats_;
  RuntimeCallCounter background_execute_counter_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterCompilationJob);
};

Interpreter::Interpreter(Isolate* isolate) : isolate_(isolate) {
  memset(dispatch_table_, 0, sizeof(dispatch_table_));

  if (FLAG_trace_ignition_dispatches) {
    static const int kBytecodeCount = static_cast<int>(Bytecode::kLast) + 1;
    bytecode_dispatch_counters_table_.reset(
        new uintptr_t[kBytecodeCount * kBytecodeCount]);
    memset(bytecode_dispatch_counters_table_.get(), 0,
           sizeof(uintptr_t) * kBytecodeCount * kBytecodeCount);
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
}

void Interpreter::IterateDispatchTable(RootVisitor* v) {
  for (int i = 0; i < kDispatchTableSize; i++) {
    Address code_entry = dispatch_table_[i];
    Object* code = code_entry == nullptr
                       ? nullptr
                       : Code::GetCodeFromTargetAddress(code_entry);
    Object* old_code = code;
    v->VisitRootPointer(Root::kDispatchTable, &code);
    if (code != old_code) {
      dispatch_table_[i] = reinterpret_cast<Code*>(code)->entry();
    }
  }
}

// static
int Interpreter::InterruptBudget() {
  return FLAG_interrupt_budget * kCodeSizeMultiplier;
}

namespace {

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

InterpreterCompilationJob::InterpreterCompilationJob(ParseInfo* parse_info,
                                                     FunctionLiteral* literal,
                                                     Isolate* isolate)
    : CompilationJob(isolate, parse_info, &compilation_info_, "Ignition"),
      zone_(isolate->allocator(), ZONE_NAME),
      compilation_info_(&zone_, isolate, parse_info, literal),
      generator_(&compilation_info_),
      runtime_call_stats_(isolate->counters()->runtime_call_stats()),
      background_execute_counter_("CompileBackgroundIgnition") {}

InterpreterCompilationJob::Status InterpreterCompilationJob::PrepareJobImpl() {
  // TODO(5203): Move code out of codegen.cc once FCG goes away.
  CodeGenerator::MakeCodePrologue(parse_info(), compilation_info(),
                                  "interpreter");
  return SUCCEEDED;
}

InterpreterCompilationJob::Status InterpreterCompilationJob::ExecuteJobImpl() {
  TimerScope runtimeTimer(
      executed_on_background_thread() ? &background_execute_counter_ : nullptr);
  RuntimeCallTimerScope runtimeTimerScope(
      !executed_on_background_thread() ? runtime_call_stats_ : nullptr,
      &RuntimeCallStats::CompileIgnition);

  // TODO(lpy): add support for background compilation RCS trace.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileIgnition");

  generator()->GenerateBytecode(stack_limit());

  if (generator()->HasStackOverflow()) {
    return FAILED;
  }
  return SUCCEEDED;
}

InterpreterCompilationJob::Status InterpreterCompilationJob::FinalizeJobImpl() {
  // Add background runtime call stats.
  if (V8_UNLIKELY(FLAG_runtime_stats && executed_on_background_thread())) {
    runtime_call_stats_->CompileBackgroundIgnition.Add(
        &background_execute_counter_);
  }

  RuntimeCallTimerScope runtimeTimerScope(
      !executed_on_background_thread() ? runtime_call_stats_ : nullptr,
      &RuntimeCallStats::CompileIgnitionFinalization);

  Handle<BytecodeArray> bytecodes = generator()->FinalizeBytecode(isolate());
  if (generator()->HasStackOverflow()) {
    return FAILED;
  }

  if (ShouldPrintBytecode(compilation_info()->shared_info())) {
    OFStream os(stdout);
    std::unique_ptr<char[]> name = compilation_info()->GetDebugName();
    os << "[generating bytecode for function: "
       << compilation_info()->GetDebugName().get() << "]" << std::endl;
    bytecodes->Disassemble(os);
    os << std::flush;
  }

  compilation_info()->SetBytecodeArray(bytecodes);
  compilation_info()->SetCode(
      BUILTIN_CODE(compilation_info()->isolate(), InterpreterEntryTrampoline));
  return SUCCEEDED;
}

CompilationJob* Interpreter::NewCompilationJob(ParseInfo* parse_info,
                                               FunctionLiteral* literal,
                                               Isolate* isolate) {
  return new InterpreterCompilationJob(parse_info, literal, isolate);
}

bool Interpreter::IsDispatchTableInitialized() {
  return dispatch_table_[0] != nullptr;
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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
