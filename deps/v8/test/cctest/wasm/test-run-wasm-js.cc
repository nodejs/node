// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/wasm/test-signatures.h"
#include "test/cctest/wasm/wasm-run-utils.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

#define BUILD(r, ...)                      \
  do {                                     \
    byte code[] = {__VA_ARGS__};           \
    r.Build(code, code + arraysize(code)); \
  } while (false)


static uint32_t AddJsFunction(TestingModule* module, FunctionSig* sig,
                              const char* source) {
  Handle<JSFunction> jsfunc = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));
  module->AddFunction(sig, Handle<Code>::null());
  uint32_t index = static_cast<uint32_t>(module->module->functions->size() - 1);
  Isolate* isolate = CcTest::InitIsolateOnce();
  Handle<Code> code = CompileWasmToJSWrapper(isolate, module, jsfunc, index);
  module->function_code->at(index) = code;
  return index;
}


static Handle<JSFunction> WrapCode(ModuleEnv* module, uint32_t index) {
  Isolate* isolate = module->module->shared_isolate;
  // Wrap the code so it can be called as a JS function.
  Handle<String> name = isolate->factory()->NewStringFromStaticChars("main");
  Handle<JSObject> module_object = Handle<JSObject>(0, isolate);
  Handle<Code> code = module->function_code->at(index);
  WasmJs::InstallWasmFunctionMap(isolate, isolate->native_context());
  return compiler::CompileJSToWasmWrapper(isolate, module, name, code,
                                          module_object, index);
}


static void EXPECT_CALL(double expected, Handle<JSFunction> jsfunc, double a,
                        double b) {
  Isolate* isolate = jsfunc->GetIsolate();
  Handle<Object> buffer[] = {isolate->factory()->NewNumber(a),
                             isolate->factory()->NewNumber(b)};
  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, jsfunc, global, 2, buffer);

  CHECK(!retval.is_null());
  Handle<Object> result = retval.ToHandleChecked();
  if (result->IsSmi()) {
    CHECK_EQ(expected, Smi::cast(*result)->value());
  } else {
    CHECK(result->IsHeapNumber());
    CHECK_EQ(expected, HeapNumber::cast(*result)->value());
  }
}


TEST(Run_Int32Sub_jswrapped) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_ii());
  BUILD(t, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = WrapCode(&module, t.CompileAndAdd(&module));

  EXPECT_CALL(33, jsfunc, 44, 11);
  EXPECT_CALL(-8723487, jsfunc, -8000000, 723487);
}


TEST(Run_Float32Div_jswrapped) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.f_ff());
  BUILD(t, WASM_F32_DIV(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = WrapCode(&module, t.CompileAndAdd(&module));

  EXPECT_CALL(92, jsfunc, 46, 0.5);
  EXPECT_CALL(64, jsfunc, -16, -0.25);
}


TEST(Run_Float64Add_jswrapped) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.d_dd());
  BUILD(t, WASM_F64_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  Handle<JSFunction> jsfunc = WrapCode(&module, t.CompileAndAdd(&module));

  EXPECT_CALL(3, jsfunc, 2, 1);
  EXPECT_CALL(-5.5, jsfunc, -5.25, -0.25);
}


TEST(Run_I32Popcount_jswrapped) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_i());
  BUILD(t, WASM_I32_POPCNT(WASM_GET_LOCAL(0)));
  Handle<JSFunction> jsfunc = WrapCode(&module, t.CompileAndAdd(&module));

  EXPECT_CALL(2, jsfunc, 9, 0);
  EXPECT_CALL(3, jsfunc, 11, 0);
  EXPECT_CALL(6, jsfunc, 0x3F, 0);

  USE(AddJsFunction);
}


#if !V8_TARGET_ARCH_ARM64
// TODO(titzer): fix wasm->JS calls on arm64 (wrapper issues)

TEST(Run_CallJS_Add_jswrapped) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_i(), &module);
  uint32_t js_index =
      AddJsFunction(&module, sigs.i_i(), "(function(a) { return a + 99; })");
  BUILD(t, WASM_CALL_FUNCTION(js_index, WASM_GET_LOCAL(0)));

  Handle<JSFunction> jsfunc = WrapCode(&module, t.CompileAndAdd(&module));

  EXPECT_CALL(101, jsfunc, 2, -8);
  EXPECT_CALL(199, jsfunc, 100, -1);
  EXPECT_CALL(-666666801, jsfunc, -666666900, -1);
}

#endif
