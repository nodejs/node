// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/objects/stack-frame-info-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_wasm_stack {

using v8::Local;
using v8::Utils;

namespace {

#define CHECK_CSTREQ(exp, found)                                              \
  do {                                                                        \
    const char* exp_ = (exp);                                                 \
    const char* found_ = (found);                                             \
    DCHECK_NOT_NULL(exp);                                                     \
    if (V8_UNLIKELY(found_ == nullptr || strcmp(exp_, found_) != 0)) {        \
      FATAL("Check failed: (%s) != (%s) ('%s' vs '%s').", #exp, #found, exp_, \
            found_ ? found_ : "<null>");                                      \
    }                                                                         \
  } while (false)

void PrintStackTrace(v8::Isolate* isolate, v8::Local<v8::StackTrace> stack) {
  printf("Stack Trace (length %d):\n", stack->GetFrameCount());
  for (int i = 0, e = stack->GetFrameCount(); i != e; ++i) {
    v8::Local<v8::StackFrame> frame = stack->GetFrame(isolate, i);
    v8::Local<v8::String> script = frame->GetScriptName();
    v8::Local<v8::String> func = frame->GetFunctionName();
    printf(
        "[%d] (%s) %s:%d:%d\n", i,
        script.IsEmpty() ? "<null>" : *v8::String::Utf8Value(isolate, script),
        func.IsEmpty() ? "<null>" : *v8::String::Utf8Value(isolate, func),
        frame->GetLineNumber(), frame->GetColumn());
  }
}

struct ExceptionInfo {
  const char* func_name;
  int line_nr;  // 1-based
  int column;   // 1-based
};

template <int N>
void CheckExceptionInfos(v8::internal::Isolate* i_isolate, Handle<Object> exc,
                         const ExceptionInfo (&excInfos)[N]) {
  // Check that it's indeed an Error object.
  CHECK(exc->IsJSError());

  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(i_isolate);

  // Extract stack frame from the exception.
  Local<v8::Value> localExc = Utils::ToLocal(exc);
  v8::Local<v8::StackTrace> stack = v8::Exception::GetStackTrace(localExc);
  PrintStackTrace(v8_isolate, stack);
  CHECK(!stack.IsEmpty());
  CHECK_EQ(N, stack->GetFrameCount());

  for (int frameNr = 0; frameNr < N; ++frameNr) {
    v8::Local<v8::StackFrame> frame = stack->GetFrame(v8_isolate, frameNr);
    v8::String::Utf8Value funName(v8_isolate, frame->GetFunctionName());
    CHECK_CSTREQ(excInfos[frameNr].func_name, *funName);
    // Line and column are 1-based in v8::StackFrame, just as in ExceptionInfo.
    CHECK_EQ(excInfos[frameNr].line_nr, frame->GetLineNumber());
    CHECK_EQ(excInfos[frameNr].column, frame->GetColumn());
  }

  CheckComputeLocation(i_isolate, exc, excInfos[0]);
}

void CheckComputeLocation(v8::internal::Isolate* i_isolate, Handle<Object> exc,
                          const ExceptionInfo& topLocation) {
  MessageLocation loc;
  CHECK(i_isolate->ComputeLocationFromStackTrace(&loc, exc));
  printf("loc start: %d, end: %d\n", loc.start_pos(), loc.end_pos());
  Handle<JSMessageObject> message = i_isolate->CreateMessage(exc, nullptr);
  printf("msg start: %d, end: %d, line: %d, col: %d\n",
         message->GetStartPosition(), message->GetEndPosition(),
         message->GetLineNumber(), message->GetColumnNumber());
  CHECK_EQ(loc.start_pos(), message->GetStartPosition());
  CHECK_EQ(loc.end_pos(), message->GetEndPosition());
  // In the message, the line is 1-based, but the column is 0-based.
  CHECK_EQ(topLocation.line_nr, message->GetLineNumber());
  CHECK_LE(1, topLocation.column);
  // TODO(szuend): Remove or re-enable the following check once it is decided
  //               whether Script::PositionInfo.column should be the offset
  //               relative to the module or relative to the function.
  // CHECK_EQ(topLocation.column - 1, message->GetColumnNumber());
}

#undef CHECK_CSTREQ

}  // namespace

// Call from JS to wasm to JS and throw an Error from JS.
WASM_COMPILED_EXEC_TEST(CollectDetailedWasmStack_ExplicitThrowFromJs) {
  TestSignatures sigs;
  HandleScope scope(CcTest::InitIsolateOnce());
  const char* source =
      "(function js() {\n function a() {\n throw new Error(); };\n a(); })";
  Handle<JSFunction> js_function =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source))));
  ManuallyImportedJSFunction import = {sigs.v_v(), js_function};
  uint32_t js_throwing_index = 0;
  WasmRunner<void> r(execution_tier, &import);

  // Add a nop such that we don't always get position 1.
  BUILD(r, WASM_NOP, WASM_CALL_FUNCTION0(js_throwing_index));
  uint32_t wasm_index_1 = r.function()->func_index;

  WasmFunctionCompiler& f2 = r.NewFunction<void>("call_main");
  BUILD(f2, WASM_CALL_FUNCTION0(wasm_index_1));
  uint32_t wasm_index_2 = f2.function_index();

  Handle<JSFunction> js_wasm_wrapper = r.builder().WrapCode(wasm_index_2);

  Handle<JSFunction> js_trampoline = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CompileRun("(function callFn(fn) { fn(); })"))));

  Isolate* isolate = js_wasm_wrapper->GetIsolate();
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 10,
                                                     v8::StackTrace::kOverview);
  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> maybe_exc;
  Handle<Object> args[] = {js_wasm_wrapper};
  MaybeHandle<Object> returnObjMaybe =
      Execution::TryCall(isolate, js_trampoline, global, 1, args,
                         Execution::MessageHandling::kReport, &maybe_exc);
  CHECK(returnObjMaybe.is_null());

  ExceptionInfo expected_exceptions[] = {
      {"a", 3, 8},           // -
      {"js", 4, 2},          // -
      {"main", 1, 8},        // -
      {"call_main", 1, 21},  // -
      {"callFn", 1, 24}      // -
  };
  CheckExceptionInfos(isolate, maybe_exc.ToHandleChecked(),
                      expected_exceptions);
}

// Trigger a trap in wasm, stack should contain a source url.
WASM_COMPILED_EXEC_TEST(CollectDetailedWasmStack_WasmUrl) {
  // Create a WasmRunner with stack checks and traps enabled.
  WasmRunner<int> r(execution_tier, nullptr, "main", kRuntimeExceptionSupport);

  std::vector<byte> code(1, kExprUnreachable);
  r.Build(code.data(), code.data() + code.size());

  WasmFunctionCompiler& f = r.NewFunction<int>("call_main");
  BUILD(f, WASM_CALL_FUNCTION0(0));
  uint32_t wasm_index = f.function_index();

  Handle<JSFunction> js_wasm_wrapper = r.builder().WrapCode(wasm_index);

  Handle<JSFunction> js_trampoline = Handle<JSFunction>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
          CompileRun("(function callFn(fn) { fn(); })"))));

  Isolate* isolate = js_wasm_wrapper->GetIsolate();
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 10,
                                                     v8::StackTrace::kOverview);

  // Set the wasm script source url.
  const char* url = "http://example.com/example.wasm";
  const Handle<String> source_url =
      isolate->factory()->InternalizeUtf8String(url);
  r.builder().instance_object()->module_object().script().set_source_url(
      *source_url);

  // Run the js wrapper.
  Handle<Object> global(isolate->context().global_object(), isolate);
  MaybeHandle<Object> maybe_exc;
  Handle<Object> args[] = {js_wasm_wrapper};
  MaybeHandle<Object> maybe_return_obj =
      Execution::TryCall(isolate, js_trampoline, global, 1, args,
                         Execution::MessageHandling::kReport, &maybe_exc);

  CHECK(maybe_return_obj.is_null());
  Handle<Object> exception = maybe_exc.ToHandleChecked();

  // Extract stack trace from the exception.
  Handle<FixedArray> stack_trace_object =
      isolate->GetDetailedStackTrace(Handle<JSObject>::cast(exception));
  CHECK(!stack_trace_object.is_null());
  Handle<StackFrameInfo> stack_frame(
      StackFrameInfo::cast(stack_trace_object->get(0)), isolate);

  MaybeHandle<String> maybe_stack_trace_str =
      SerializeStackFrameInfo(isolate, stack_frame);
  CHECK(!maybe_stack_trace_str.is_null());
  Handle<String> stack_trace_str = maybe_stack_trace_str.ToHandleChecked();

  // Check if the source_url is part of the stack trace.
  CHECK_NE(std::string(stack_trace_str->ToCString().get()).find(url),
           std::string::npos);
}

// Trigger a trap in wasm, stack should be JS -> wasm -> wasm.
WASM_COMPILED_EXEC_TEST(CollectDetailedWasmStack_WasmError) {
  for (int pos_shift = 0; pos_shift < 3; ++pos_shift) {
    // Test a position with 1, 2 or 3 bytes needed to represent it.
    int unreachable_pos = 1 << (8 * pos_shift);
    TestSignatures sigs;
    // Create a WasmRunner with stack checks and traps enabled.
    WasmRunner<int> r(execution_tier, nullptr, "main",
                      kRuntimeExceptionSupport);

    std::vector<byte> code(unreachable_pos + 1, kExprNop);
    code[unreachable_pos] = kExprUnreachable;
    r.Build(code.data(), code.data() + code.size());

    uint32_t wasm_index_1 = r.function()->func_index;

    WasmFunctionCompiler& f2 = r.NewFunction<int>("call_main");
    BUILD(f2, WASM_CALL_FUNCTION0(0));
    uint32_t wasm_index_2 = f2.function_index();

    Handle<JSFunction> js_wasm_wrapper = r.builder().WrapCode(wasm_index_2);

    Handle<JSFunction> js_trampoline = Handle<JSFunction>::cast(
        v8::Utils::OpenHandle(*v8::Local<v8::Function>::Cast(
            CompileRun("(function callFn(fn) { fn(); })"))));

    Isolate* isolate = js_wasm_wrapper->GetIsolate();
    isolate->SetCaptureStackTraceForUncaughtExceptions(
        true, 10, v8::StackTrace::kOverview);
    Handle<Object> global(isolate->context().global_object(), isolate);
    MaybeHandle<Object> maybe_exc;
    Handle<Object> args[] = {js_wasm_wrapper};
    MaybeHandle<Object> maybe_return_obj =
        Execution::TryCall(isolate, js_trampoline, global, 1, args,
                           Execution::MessageHandling::kReport, &maybe_exc);
    CHECK(maybe_return_obj.is_null());
    Handle<Object> exception = maybe_exc.ToHandleChecked();

    static constexpr int kMainLocalsLength = 1;
    const int main_offset =
        r.builder().GetFunctionAt(wasm_index_1)->code.offset();
    const int call_main_offset =
        r.builder().GetFunctionAt(wasm_index_2)->code.offset();

    // Column is 1-based, so add 1 for the expected wasm output. Line number
    // is always 1.
    const int expected_main_pos =
        unreachable_pos + main_offset + kMainLocalsLength + 1;
    const int expected_call_main_pos = call_main_offset + kMainLocalsLength + 1;
    ExceptionInfo expected_exceptions[] = {
        {"main", 1, expected_main_pos},            // -
        {"call_main", 1, expected_call_main_pos},  // -
        {"callFn", 1, 24}                          //-
    };
    CheckExceptionInfos(isolate, exception, expected_exceptions);
  }
}

}  // namespace test_wasm_stack
}  // namespace wasm
}  // namespace internal
}  // namespace v8
