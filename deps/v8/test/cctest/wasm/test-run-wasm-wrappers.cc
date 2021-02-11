// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/common/wasm/wasm-module-runner.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_wrappers {

using testing::CompileAndInstantiateForTesting;

#ifdef V8_TARGET_ARCH_X64
namespace {
void Cleanup() {
  // By sending a low memory notifications, we will try hard to collect all
  // garbage and will therefore also invoke all weak callbacks of actually
  // unreachable persistent handles.
  Isolate* isolate = CcTest::InitIsolateOnce();
  reinterpret_cast<v8::Isolate*>(isolate)->LowMemoryNotification();
}

}  // namespace

TEST(CallCounter) {
  {
    // This test assumes use of the generic wrapper.
    FlagScope<bool> use_wasm_generic_wrapper(&FLAG_wasm_generic_wrapper, true);

    TestSignatures sigs;
    AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    // Define the Wasm function.
    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_ii());
    f->builder()->AddExport(CStrVector("main"), f);
    byte code[] = {WASM_I32_MUL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)),
                   WASM_END};
    f->EmitCode(code, sizeof(code));

    // Compile module.
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
    MaybeHandle<WasmInstanceObject> instance = CompileAndInstantiateForTesting(
        isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));

    MaybeHandle<WasmExportedFunction> maybe_export =
        testing::GetExportedFunction(isolate, instance.ToHandleChecked(),
                                     "main");
    Handle<WasmExportedFunction> main_export = maybe_export.ToHandleChecked();

    // Check that the counter has initially a value of 0.
    CHECK_EQ(main_export->shared().wasm_exported_function_data().call_count(),
             0);

    // Call the exported Wasm function and get the result.
    Handle<Object> params[2] = {Handle<Object>(Smi::FromInt(6), isolate),
                                Handle<Object>(Smi::FromInt(7), isolate)};
    static const int32_t kExpectedValue = 42;
    Handle<Object> receiver = isolate->factory()->undefined_value();
    MaybeHandle<Object> maybe_result =
        Execution::Call(isolate, main_export, receiver, 2, params);
    Handle<Object> result = maybe_result.ToHandleChecked();

    // Check that the counter has now a value of 1.
    CHECK_EQ(main_export->shared().wasm_exported_function_data().call_count(),
             1);

    CHECK(result->IsSmi() && Smi::ToInt(*result) == kExpectedValue);
  }
  Cleanup();
}

TEST(WrapperReplacement) {
  {
    // This test assumes use of the generic wrapper.
    FlagScope<bool> use_wasm_generic_wrapper(&FLAG_wasm_generic_wrapper, true);

    TestSignatures sigs;
    AccountingAllocator allocator;
    Zone zone(&allocator, ZONE_NAME);

    // Define the Wasm function.
    WasmModuleBuilder* builder = zone.New<WasmModuleBuilder>(&zone);
    WasmFunctionBuilder* f = builder->AddFunction(sigs.i_i());
    f->builder()->AddExport(CStrVector("main"), f);
    byte code[] = {WASM_RETURN1(WASM_GET_LOCAL(0)), WASM_END};
    f->EmitCode(code, sizeof(code));

    // Compile module.
    ZoneBuffer buffer(&zone);
    builder->WriteTo(&buffer);
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    testing::SetupIsolateForWasmModule(isolate);
    ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
    MaybeHandle<WasmInstanceObject> instance = CompileAndInstantiateForTesting(
        isolate, &thrower, ModuleWireBytes(buffer.begin(), buffer.end()));

    // Get the exported function.
    MaybeHandle<WasmExportedFunction> maybe_export =
        testing::GetExportedFunction(isolate, instance.ToHandleChecked(),
                                     "main");
    Handle<WasmExportedFunction> main_export = maybe_export.ToHandleChecked();

    // Check that the counter has initially a value of 0.
    CHECK_EQ(main_export->shared().wasm_exported_function_data().call_count(),
             0);
    CHECK_GT(kGenericWrapperThreshold, 0);

    // Call the exported Wasm function as many times as required to reach the
    // threshold for compiling the specific wrapper.
    const int threshold = static_cast<int>(kGenericWrapperThreshold);
    for (int i = 1; i < threshold; ++i) {
      // Verify that the wrapper to be used is still the generic one.
      Code wrapper =
          main_export->shared().wasm_exported_function_data().wrapper_code();
      CHECK(wrapper.is_builtin() &&
            wrapper.builtin_index() == Builtins::kGenericJSToWasmWrapper);
      // Call the function.
      int32_t expected_value = i;
      Handle<Object> params[1] = {
          Handle<Object>(Smi::FromInt(expected_value), isolate)};
      Handle<Object> receiver = isolate->factory()->undefined_value();
      MaybeHandle<Object> maybe_result =
          Execution::Call(isolate, main_export, receiver, 1, params);
      Handle<Object> result = maybe_result.ToHandleChecked();
      // Verify that the counter has now a value of i and the return value is
      // correct.
      CHECK_EQ(main_export->shared().wasm_exported_function_data().call_count(),
               i);
      CHECK(result->IsSmi() && Smi::ToInt(*result) == expected_value);
    }

    // Get the wrapper-code object before making the call that will kick off the
    // wrapper replacement.
    Code wrapper_before_call =
        main_export->shared().wasm_exported_function_data().wrapper_code();
    // Verify that the wrapper before the call is the generic wrapper.
    CHECK(wrapper_before_call.is_builtin() &&
          wrapper_before_call.builtin_index() ==
              Builtins::kGenericJSToWasmWrapper);

    // Call the exported Wasm function one more time to kick off the wrapper
    // replacement.
    int32_t expected_value = 42;
    Handle<Object> params[1] = {
        Handle<Object>(Smi::FromInt(expected_value), isolate)};
    Handle<Object> receiver = isolate->factory()->undefined_value();
    MaybeHandle<Object> maybe_result =
        Execution::Call(isolate, main_export, receiver, 1, params);
    Handle<Object> result = maybe_result.ToHandleChecked();
    // Check that the counter has the threshold value and the result is correct.
    CHECK_EQ(main_export->shared().wasm_exported_function_data().call_count(),
             kGenericWrapperThreshold);
    CHECK(result->IsSmi() && Smi::ToInt(*result) == expected_value);

    // Verify that the wrapper-code object has changed.
    Code wrapper_after_call =
        main_export->shared().wasm_exported_function_data().wrapper_code();
    CHECK_NE(wrapper_after_call, wrapper_before_call);
    // Verify that the wrapper is now a specific one.
    CHECK(wrapper_after_call.kind() == CodeKind::JS_TO_WASM_FUNCTION);
  }
  Cleanup();
}
#endif

}  // namespace test_run_wasm_wrappers
}  // namespace wasm
}  // namespace internal
}  // namespace v8
