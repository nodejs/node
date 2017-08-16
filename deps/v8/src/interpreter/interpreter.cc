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
  explicit InterpreterCompilationJob(CompilationInfo* info);

 protected:
  Status PrepareJobImpl() final;
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl() final;

 private:
  class TimerScope final {
   public:
    TimerScope(RuntimeCallStats* stats, RuntimeCallStats::CounterId counter_id)
        : stats_(stats) {
      if (V8_UNLIKELY(FLAG_runtime_stats)) {
        RuntimeCallStats::Enter(stats_, &timer_, counter_id);
      }
    }

    explicit TimerScope(RuntimeCallCounter* counter) : stats_(nullptr) {
      if (V8_UNLIKELY(FLAG_runtime_stats)) {
        timer_.Start(counter, nullptr);
      }
    }

    ~TimerScope() {
      if (V8_UNLIKELY(FLAG_runtime_stats)) {
        if (stats_) {
          RuntimeCallStats::Leave(stats_, &timer_);
        } else {
          timer_.Stop();
        }
      }
    }

   private:
    RuntimeCallStats* stats_;
    RuntimeCallTimer timer_;
  };

  BytecodeGenerator* generator() { return &generator_; }

  BytecodeGenerator generator_;
  RuntimeCallStats* runtime_call_stats_;
  RuntimeCallCounter background_execute_counter_;
  bool print_bytecode_;

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
  return 0;
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

InterpreterCompilationJob::InterpreterCompilationJob(CompilationInfo* info)
    : CompilationJob(info->isolate(), info, "Ignition"),
      generator_(info),
      runtime_call_stats_(info->isolate()->counters()->runtime_call_stats()),
      background_execute_counter_("CompileBackgroundIgnition"),
      print_bytecode_(ShouldPrintBytecode(info->shared_info())) {}

InterpreterCompilationJob::Status InterpreterCompilationJob::PrepareJobImpl() {
  CodeGenerator::MakeCodePrologue(info(), "interpreter");

  if (print_bytecode_) {
    OFStream os(stdout);
    std::unique_ptr<char[]> name = info()->GetDebugName();
    os << "[generating bytecode for function: " << info()->GetDebugName().get()
       << "]" << std::endl;
  }

  return SUCCEEDED;
}

InterpreterCompilationJob::Status InterpreterCompilationJob::ExecuteJobImpl() {
  TimerScope runtimeTimer =
      executed_on_background_thread()
          ? TimerScope(&background_execute_counter_)
          : TimerScope(runtime_call_stats_, &RuntimeCallStats::CompileIgnition);
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

  RuntimeCallTimerScope runtimeTimer(
      runtime_call_stats_, &RuntimeCallStats::CompileIgnitionFinalization);

  Handle<BytecodeArray> bytecodes = generator()->FinalizeBytecode(isolate());
  if (generator()->HasStackOverflow()) {
    return FAILED;
  }

  if (print_bytecode_) {
    OFStream os(stdout);
    bytecodes->Disassemble(os);
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
