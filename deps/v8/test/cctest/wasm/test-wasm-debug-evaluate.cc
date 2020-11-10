// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>

#include "src/api/api-inl.h"
#include "src/base/macros.h"
#include "src/codegen/assembler-inl.h"
#include "src/compiler/heap-refs.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug-interface.h"
#include "src/diagnostics/disassembler.h"
#include "src/execution/frames-inl.h"
#include "src/execution/frames.h"
#include "src/objects/js-objects.h"
#include "src/objects/property-descriptor.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-debug-evaluate.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-tier.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
template <typename... FunctionArgsT>
class TestCode {
 public:
  TestCode(WasmRunnerBase* runner, std::initializer_list<byte> code,
           std::initializer_list<ValueType::Kind> locals = {})
      : compiler_(&runner->NewFunction<FunctionArgsT...>()),
        code_(code),
        locals_(static_cast<uint32_t>(locals.size())) {
    for (ValueType::Kind T : locals) {
      compiler_->AllocateLocal(ValueType(T));
    }
    compiler_->Build(code.begin(), code.end());
  }

  Handle<BreakPoint> BreakOnReturn(WasmRunnerBase* runner) {
    runner->TierDown();
    uint32_t return_offset_in_function = locals_ + FindReturn();

    int function_index = compiler_->function_index();
    int function_offset =
        runner->builder().GetFunctionAt(function_index)->code.offset();
    int return_offset_in_module = function_offset + return_offset_in_function;

    Handle<WasmInstanceObject> instance = runner->builder().instance_object();
    Handle<Script> script(instance->module_object().script(),
                          runner->main_isolate());
    static int break_index = 0;
    Handle<BreakPoint> break_point =
        runner->main_isolate()->factory()->NewBreakPoint(
            break_index++, runner->main_isolate()->factory()->empty_string());
    CHECK(WasmScript::SetBreakPoint(script, &return_offset_in_module,
                                    break_point));
    return break_point;
  }

  MaybeHandle<Object> Run(WasmRunnerBase* runner) {
    Isolate* isolate = runner->main_isolate();
    Handle<JSFunction> fun_wrapper =
        runner->builder().WrapCode(compiler_->function_index());
    Handle<Object> global(isolate->context().global_object(), isolate);
    return Execution::Call(isolate, fun_wrapper, global, 0, nullptr);
  }

 private:
  uint32_t FindReturn() const {
    for (auto i = code_.begin(); i != code_.end();
         i += OpcodeLength(&*i, &*code_.end())) {
      if (*i == kExprReturn) {
        return static_cast<uint32_t>(std::distance(code_.begin(), i));
      }
    }
    UNREACHABLE();
  }

  WasmFunctionCompiler* compiler_;
  std::vector<byte> code_;
  uint32_t locals_;
};

class WasmEvaluatorBuilder {
 public:
  explicit WasmEvaluatorBuilder(ExecutionTier execution_tier,
                                uint32_t min_memory = 1,
                                uint32_t max_memory = 1)
      : zone_(&allocator_, ZONE_NAME), builder_(&zone_) {
    get_memory_function_index = AddImport<void, uint32_t, uint32_t, uint32_t>(
        CStrVector("__getMemory"));
    get_local_function_index =
        AddImport<void, uint32_t, uint32_t>(CStrVector("__getLocal"));
    sbrk_function_index = AddImport<uint32_t, uint32_t>(CStrVector("__sbrk"));
    wasm_format_function =
        builder_.AddFunction(WasmRunnerBase::CreateSig<uint32_t>(&zone_));
    wasm_format_function->SetName(CStrVector("wasm_format"));
    builder_.AddExport(CStrVector("wasm_format"), wasm_format_function);
    builder_.SetMinMemorySize(min_memory);
    builder_.SetMaxMemorySize(max_memory);
  }

  template <typename ReturnT, typename... ArgTs>
  uint32_t AddImport(Vector<const char> name) {
    return builder_.AddImport(
        name, WasmRunnerBase::CreateSig<ReturnT, ArgTs...>(&zone_),
        CStrVector("env"));
  }

  void push_back(std::initializer_list<byte> code) {
    wasm_format_function->EmitCode(code.begin(),
                                   static_cast<uint32_t>(code.size()));
  }

  void CallSbrk(std::initializer_list<byte> args) {
    push_back(args);
    push_back({WASM_CALL_FUNCTION0(sbrk_function_index)});
  }

  void CallGetLocal(std::initializer_list<byte> args) {
    push_back(args);
    push_back({WASM_CALL_FUNCTION0(get_local_function_index)});
  }

  void CallGetMemory(std::initializer_list<byte> args) {
    push_back(args);
    push_back({WASM_CALL_FUNCTION0(get_memory_function_index)});
  }

  ZoneBuffer bytes() {
    ZoneBuffer bytes(&zone_);
    builder_.WriteTo(&bytes);
    return bytes;
  }

 private:
  v8::internal::AccountingAllocator allocator_;
  Zone zone_;
  WasmModuleBuilder builder_;
  uint32_t get_memory_function_index = 0;
  uint32_t get_local_function_index = 0;
  uint32_t sbrk_function_index = 0;
  WasmFunctionBuilder* wasm_format_function = nullptr;
};

class WasmBreakHandler : public debug::DebugDelegate {
 public:
  struct EvaluationResult {
    Maybe<std::string> result = Nothing<std::string>();
    Maybe<std::string> error = Nothing<std::string>();
  };

  WasmBreakHandler(Isolate* isolate, ZoneBuffer evaluator_bytes)
      : isolate_(isolate),
        evaluator_bytes_(std::move(evaluator_bytes)),
        result_(Nothing<EvaluationResult>()) {
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_), this);
  }

  ~WasmBreakHandler() override {
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_),
                                nullptr);
  }

  const Maybe<EvaluationResult>& result() const { return result_; }

 private:
  Isolate* isolate_;
  ZoneBuffer evaluator_bytes_;
  Maybe<EvaluationResult> result_;

  Maybe<std::string> GetPendingExceptionAsString() {
    if (!isolate_->has_pending_exception()) return Nothing<std::string>();
    Handle<Object> exception(isolate_->pending_exception(), isolate_);
    isolate_->clear_pending_exception();

    Handle<String> exception_string;
    if (!Object::ToString(isolate_, exception).ToHandle(&exception_string)) {
      return Just<std::string>("");
    }
    return Just<std::string>(exception_string->ToCString().get());
  }

  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             const std::vector<int>&) override {
    // Check the current position.
    StackTraceFrameIterator frame_it(isolate_);

    WasmFrame* frame = WasmFrame::cast(frame_it.frame());
    Handle<WasmInstanceObject> instance{frame->wasm_instance(), isolate_};

    MaybeHandle<String> result_handle = v8::internal::wasm::DebugEvaluate(
        {evaluator_bytes_.begin(), evaluator_bytes_.size()}, instance,
        frame_it.frame());

    Maybe<std::string> error_message = GetPendingExceptionAsString();
    Maybe<std::string> result_message =
        result_handle.is_null()
            ? Nothing<std::string>()
            : Just<std::string>(
                  result_handle.ToHandleChecked()->ToCString().get());

    isolate_->clear_pending_exception();
    result_ = Just<EvaluationResult>({result_message, error_message});
  }
};

WASM_COMPILED_EXEC_TEST(WasmDebugEvaluate_CompileFailed) {
  WasmRunner<int> runner(execution_tier);

  TestCode<int> code(&runner, {WASM_RETURN1(WASM_I32V_1(32))});
  code.BreakOnReturn(&runner);

  WasmEvaluatorBuilder evaluator(execution_tier);
  // Create a module that doesn't compile by missing the END bytecode.
  evaluator.push_back({WASM_RETURN1(WASM_I32V_1(33))});

  Isolate* isolate = runner.main_isolate();
  WasmBreakHandler break_handler(isolate, evaluator.bytes());
  CHECK(!code.Run(&runner).is_null());

  WasmBreakHandler::EvaluationResult result =
      break_handler.result().ToChecked();
  CHECK(result.result.IsNothing());
  CHECK_NE(result.error.ToChecked().find(
               "function body must end with \"end\" opcode"),
           std::string::npos);
}

WASM_COMPILED_EXEC_TEST(WasmDebugEvaluate_MissingEntrypoint) {
  WasmRunner<int> runner(execution_tier);

  TestCode<int> code(&runner, {WASM_RETURN1(WASM_I32V_1(32))});
  code.BreakOnReturn(&runner);

  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  WasmModuleBuilder evaluator(&zone);
  ZoneBuffer evaluator_bytes(&zone);
  evaluator.WriteTo(&evaluator_bytes);

  Isolate* isolate = runner.main_isolate();
  WasmBreakHandler break_handler(isolate, std::move(evaluator_bytes));
  CHECK(!code.Run(&runner).is_null());

  WasmBreakHandler::EvaluationResult result =
      break_handler.result().ToChecked();
  CHECK(result.result.IsNothing());
  CHECK_NE(result.error.ToChecked().find("Missing export: \"wasm_format\""),
           std::string::npos);
}

WASM_COMPILED_EXEC_TEST(WasmDebugEvaluate_ExecuteFailed_SEGV) {
  WasmRunner<int> runner(execution_tier);
  runner.builder().AddMemoryElems<int32_t>(64);

  TestCode<int> code(&runner, {WASM_RETURN1(WASM_I32V_1(32))});

  // Use a max memory size of 2 here to verify the precondition for the
  // GrowMemory test below.
  WasmEvaluatorBuilder evaluator(execution_tier, 1, 2);
  code.BreakOnReturn(&runner);

  // Load 1 byte from an address that's too high.
  evaluator.CallGetMemory(
      {WASM_I32V_1(32), WASM_I32V_1(1), WASM_I32V_3((1 << 16) + 1)});
  evaluator.push_back({WASM_RETURN1(WASM_I32V_1(33)), WASM_END});

  Isolate* isolate = runner.main_isolate();
  WasmBreakHandler break_handler(isolate, evaluator.bytes());
  CHECK(!code.Run(&runner).is_null());

  WasmBreakHandler::EvaluationResult result =
      break_handler.result().ToChecked();
  CHECK(result.result.IsNothing());
  CHECK_NE(
      result.error.ToChecked().find("Illegal access to out-of-bounds memory"),
      std::string::npos);
}

WASM_COMPILED_EXEC_TEST(WasmDebugEvaluate_GrowMemory) {
  WasmRunner<int> runner(execution_tier);
  runner.builder().AddMemoryElems<int32_t>(64);

  TestCode<int> code(
      &runner,
      {WASM_STORE_MEM(MachineType::Int32(), WASM_I32V_1(32), WASM_I32V_2('A')),
       WASM_RETURN1(WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(32)))});
  code.BreakOnReturn(&runner);

  WasmEvaluatorBuilder evaluator(execution_tier, 1, 2);
  // Grow the memory.
  evaluator.CallSbrk({WASM_I32V_1(1)});
  // Load 1 byte from an address that's too high for the default memory.
  evaluator.CallGetMemory(
      {WASM_I32V_1(32), WASM_I32V_1(1), WASM_I32V_3((1 << 16) + 1)});
  evaluator.push_back({WASM_RETURN1(WASM_I32V_3((1 << 16) + 1)), WASM_END});

  Isolate* isolate = runner.main_isolate();
  WasmBreakHandler break_handler(isolate, evaluator.bytes());
  CHECK(!code.Run(&runner).is_null());

  WasmBreakHandler::EvaluationResult result =
      break_handler.result().ToChecked();
  CHECK(result.error.IsNothing());
  CHECK_EQ(result.result.ToChecked(), "A");
}

WASM_COMPILED_EXEC_TEST(WasmDebugEvaluate_LinearMemory) {
  WasmRunner<int> runner(execution_tier);
  runner.builder().AddMemoryElems<int32_t>(64);

  TestCode<int> code(
      &runner,
      {WASM_STORE_MEM(MachineType::Int32(), WASM_I32V_1(32), WASM_I32V_2('A')),
       WASM_RETURN1(WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(32)))});
  code.BreakOnReturn(&runner);

  WasmEvaluatorBuilder evaluator(execution_tier);
  // Load 4 bytes from debuggee memory at address 32, and store at the offset 33
  // of the linear memory.
  evaluator.CallGetMemory({WASM_I32V_1(32), WASM_I32V_1(4), WASM_I32V_1(33)});
  evaluator.push_back({WASM_RETURN1(WASM_I32V_1(33)), WASM_END});

  Isolate* isolate = runner.main_isolate();
  WasmBreakHandler break_handler(isolate, evaluator.bytes());
  CHECK(!code.Run(&runner).is_null());

  WasmBreakHandler::EvaluationResult result =
      break_handler.result().ToChecked();
  CHECK(result.error.IsNothing());
  CHECK_EQ(result.result.ToChecked(), "A");
}

WASM_COMPILED_EXEC_TEST(WasmDebugEvaluate_Locals) {
  WasmRunner<int> runner(execution_tier);
  runner.builder().AddMemoryElems<int32_t>(64);

  TestCode<int> code(
      &runner,
      {WASM_SET_LOCAL(0, WASM_I32V_2('A')), WASM_RETURN1(WASM_GET_LOCAL(0))},
      {ValueType::kI32});
  code.BreakOnReturn(&runner);

  WasmEvaluatorBuilder evaluator(execution_tier);
  evaluator.CallGetLocal({WASM_I32V_1(0), WASM_I32V_1(33)});
  evaluator.push_back({WASM_RETURN1(WASM_I32V_1(33)), WASM_END});

  Isolate* isolate = runner.main_isolate();
  WasmBreakHandler break_handler(isolate, evaluator.bytes());
  CHECK(!code.Run(&runner).is_null());

  WasmBreakHandler::EvaluationResult result =
      break_handler.result().ToChecked();
  CHECK(result.error.IsNothing());
  CHECK_EQ(result.result.ToChecked(), "A");
}

}  // namespace
}  // namespace wasm
}  // namespace internal
}  // namespace v8
