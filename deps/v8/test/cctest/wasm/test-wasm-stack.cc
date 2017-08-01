// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

using v8::Local;
using v8::Utils;

namespace {

#define CHECK_CSTREQ(exp, found)                                           \
  do {                                                                     \
    const char* exp_ = (exp);                                              \
    const char* found_ = (found);                                          \
    DCHECK_NOT_NULL(exp);                                                  \
    if (V8_UNLIKELY(found_ == nullptr || strcmp(exp_, found_) != 0)) {     \
      V8_Fatal(__FILE__, __LINE__,                                         \
               "Check failed: (%s) != (%s) ('%s' vs '%s').", #exp, #found, \
               exp_, found_ ? found_ : "<null>");                          \
    }                                                                      \
  } while (0)

void PrintStackTrace(v8::Local<v8::StackTrace> stack) {
  printf("Stack Trace (length %d):\n", stack->GetFrameCount());
  for (int i = 0, e = stack->GetFrameCount(); i != e; ++i) {
    v8::Local<v8::StackFrame> frame = stack->GetFrame(i);
    v8::Local<v8::String> script = frame->GetScriptName();
    v8::Local<v8::String> func = frame->GetFunctionName();
    printf("[%d] (%s) %s:%d:%d\n", i,
           script.IsEmpty() ? "<null>" : *v8::String::Utf8Value(script),
           func.IsEmpty() ? "<null>" : *v8::String::Utf8Value(func),
           frame->GetLineNumber(), frame->GetColumn());
  }
}

struct ExceptionInfo {
  const char* func_name;
  int line_nr;
  int column;
};

template <int N>
void CheckExceptionInfos(Handle<Object> exc,
                         const ExceptionInfo (&excInfos)[N]) {
  // Check that it's indeed an Error object.
  CHECK(exc->IsJSError());

  // Extract stack frame from the exception.
  Local<v8::Value> localExc = Utils::ToLocal(exc);
  v8::Local<v8::StackTrace> stack = v8::Exception::GetStackTrace(localExc);
  PrintStackTrace(stack);
  CHECK(!stack.IsEmpty());
  CHECK_EQ(N, stack->GetFrameCount());

  for (int frameNr = 0; frameNr < N; ++frameNr) {
    v8::Local<v8::StackFrame> frame = stack->GetFrame(frameNr);
    v8::String::Utf8Value funName(frame->GetFunctionName());
    CHECK_CSTREQ(excInfos[frameNr].func_name, *funName);
    CHECK_EQ(excInfos[frameNr].line_nr, frame->GetLineNumber());
    CHECK_EQ(excInfos[frameNr].column, frame->GetColumn());
  }
}

}  // namespace

// Call from JS to WASM to JS and throw an Error from JS.
TEST(CollectDetailedWasmStack_ExplicitThrowFromJs) {
  WasmRunner<void> r(kExecuteCompiled);
  TestSignatures sigs;

  uint32_t js_throwing_index = r.module().AddJsFunction(
      sigs.v_v(),
      "(function js() {\n function a() {\n throw new Error(); };\n a(); })");

  // Add a nop such that we don't always get position 1.
  BUILD(r, WASM_NOP, WASM_CALL_FUNCTION0(js_throwing_index));
  uint32_t wasm_index_1 = r.function()->func_index;

  WasmFunctionCompiler& f2 = r.NewFunction<void>("call_main");
  BUILD(f2, WASM_CALL_FUNCTION0(wasm_index_1));
  uint32_t wasm_index_2 = f2.function_index();

  Handle<JSFunction> js_wasm_wrapper = r.module().WrapCode(wasm_index_2);

  Handle<JSFunction> js_trampoline = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CompileRun("(function callFn(fn) { fn(); })"))));

  Isolate* isolate = js_wasm_wrapper->GetIsolate();
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 10,
                                                     v8::StackTrace::kOverview);
  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> maybe_exc;
  Handle<Object> args[] = {js_wasm_wrapper};
  MaybeHandle<Object> returnObjMaybe =
      Execution::TryCall(isolate, js_trampoline, global, 1, args,
                         Execution::MessageHandling::kReport, &maybe_exc);
  CHECK(returnObjMaybe.is_null());

  // Line and column are 1-based, so add 1 for the expected wasm output.
  ExceptionInfo expected_exceptions[] = {
      {"a", 3, 8},                                           // -
      {"js", 4, 2},                                          // -
      {"main", static_cast<int>(wasm_index_1) + 1, 3},       // -
      {"call_main", static_cast<int>(wasm_index_2) + 1, 2},  // -
      {"callFn", 1, 24}                                      // -
  };
  CheckExceptionInfos(maybe_exc.ToHandleChecked(), expected_exceptions);
}

// Trigger a trap in WASM, stack should be JS -> WASM -> WASM.
TEST(CollectDetailedWasmStack_WasmError) {
  TestSignatures sigs;
  WasmRunner<int> r(kExecuteCompiled);
  // Set the execution context, such that a runtime error can be thrown.
  r.SetModuleContext();

  BUILD(r, WASM_UNREACHABLE);
  uint32_t wasm_index_1 = r.function()->func_index;

  WasmFunctionCompiler& f2 = r.NewFunction<int>("call_main");
  BUILD(f2, WASM_CALL_FUNCTION0(0));
  uint32_t wasm_index_2 = f2.function_index();

  Handle<JSFunction> js_wasm_wrapper = r.module().WrapCode(wasm_index_2);

  Handle<JSFunction> js_trampoline = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CompileRun("(function callFn(fn) { fn(); })"))));

  Isolate* isolate = js_wasm_wrapper->GetIsolate();
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 10,
                                                     v8::StackTrace::kOverview);
  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> maybe_exc;
  Handle<Object> args[] = {js_wasm_wrapper};
  MaybeHandle<Object> maybe_return_obj =
      Execution::TryCall(isolate, js_trampoline, global, 1, args,
                         Execution::MessageHandling::kReport, &maybe_exc);
  CHECK(maybe_return_obj.is_null());

  // Line and column are 1-based, so add 1 for the expected wasm output.
  ExceptionInfo expected_exceptions[] = {
      {"main", static_cast<int>(wasm_index_1) + 1, 2},       // -
      {"call_main", static_cast<int>(wasm_index_2) + 1, 2},  // -
      {"callFn", 1, 24}                                      //-
  };
  CheckExceptionInfos(maybe_exc.ToHandleChecked(), expected_exceptions);
}
