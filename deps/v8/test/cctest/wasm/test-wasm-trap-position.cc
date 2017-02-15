// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

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

// Trigger a trap for executing unreachable.
TEST(Unreachable) {
  TestSignatures sigs;
  TestingModule module;

  WasmFunctionCompiler comp1(sigs.v_v(), &module,
                             ArrayVector("exec_unreachable"));
  // Set the execution context, such that a runtime error can be thrown.
  comp1.SetModuleContext();
  BUILD(comp1, WASM_UNREACHABLE);
  uint32_t wasm_index = comp1.CompileAndAdd();

  Handle<JSFunction> js_wasm_wrapper = module.WrapCode(wasm_index);

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
      Execution::TryCall(isolate, js_trampoline, global, 1, args, &maybe_exc);
  CHECK(returnObjMaybe.is_null());

  // The column is 1-based, so add 1 to the actual byte offset.
  ExceptionInfo expected_exceptions[] = {
      {"<WASM UNNAMED>", static_cast<int>(wasm_index), 2},  // --
      {"callFn", 1, 24}                                     // --
  };
  CheckExceptionInfos(maybe_exc.ToHandleChecked(), expected_exceptions);
}

// Trigger a trap for loading from out-of-bounds.
TEST(IllegalLoad) {
  TestSignatures sigs;
  TestingModule module;

  WasmFunctionCompiler comp1(sigs.v_v(), &module, ArrayVector("mem_oob"));
  // Set the execution context, such that a runtime error can be thrown.
  comp1.SetModuleContext();
  BUILD(comp1, WASM_IF(WASM_ONE, WASM_SEQ(WASM_LOAD_MEM(MachineType::Int32(),
                                                        WASM_I32V_1(-3)),
                                          WASM_DROP)));
  uint32_t wasm_index = comp1.CompileAndAdd();

  WasmFunctionCompiler comp2(sigs.v_v(), &module, ArrayVector("call_mem_oob"));
  // Insert a NOP such that the position of the call is not one.
  BUILD(comp2, WASM_NOP, WASM_CALL_FUNCTION0(wasm_index));
  uint32_t wasm_index_2 = comp2.CompileAndAdd();

  Handle<JSFunction> js_wasm_wrapper = module.WrapCode(wasm_index_2);

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
      Execution::TryCall(isolate, js_trampoline, global, 1, args, &maybe_exc);
  CHECK(returnObjMaybe.is_null());

  // The column is 1-based, so add 1 to the actual byte offset.
  ExceptionInfo expected_exceptions[] = {
      {"<WASM UNNAMED>", static_cast<int>(wasm_index), 8},    // --
      {"<WASM UNNAMED>", static_cast<int>(wasm_index_2), 3},  // --
      {"callFn", 1, 24}                                       // --
  };
  CheckExceptionInfos(maybe_exc.ToHandleChecked(), expected_exceptions);
}
