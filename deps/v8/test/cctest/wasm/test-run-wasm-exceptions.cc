// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "test/cctest/wasm/wasm-atomics-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_exceptions {

WASM_COMPILED_EXEC_TEST(TryCatchThrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(kWasmI32,
                            WASM_STMTS(WASM_I32V(kResult1),
                                       WASM_IF(WASM_I32_EQZ(WASM_GET_LOCAL(0)),
                                               WASM_THROW(except))),
                            WASM_STMTS(WASM_DROP, WASM_I32V(kResult0))));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

WASM_COMPILED_EXEC_TEST(TryCatchCallDirect) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build a throwing helper function.
  WasmFunctionCompiler& throw_func = r.NewFunction(sigs.i_ii());
  BUILD(throw_func, WASM_THROW(except));

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(WASM_I32V(kResult1),
                          WASM_IF(WASM_I32_EQZ(WASM_GET_LOCAL(0)),
                                  WASM_STMTS(WASM_CALL_FUNCTION(
                                                 throw_func.function_index(),
                                                 WASM_I32V(7), WASM_I32V(9)),
                                             WASM_DROP))),
               WASM_STMTS(WASM_DROP, WASM_I32V(kResult0))));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

WASM_COMPILED_EXEC_TEST(TryCatchCallIndirect) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build a throwing helper function.
  WasmFunctionCompiler& throw_func = r.NewFunction(sigs.i_ii());
  BUILD(throw_func, WASM_THROW(except));
  byte sig_index = r.builder().AddSignature(sigs.i_ii());
  throw_func.SetSigIndex(0);

  // Add an indirect function table.
  uint16_t indirect_function_table[] = {
      static_cast<uint16_t>(throw_func.function_index())};
  r.builder().AddIndirectFunctionTable(indirect_function_table,
                                       arraysize(indirect_function_table));

  // Build the main test function.
  BUILD(r,
        WASM_TRY_CATCH_T(
            kWasmI32,
            WASM_STMTS(WASM_I32V(kResult1),
                       WASM_IF(WASM_I32_EQZ(WASM_GET_LOCAL(0)),
                               WASM_STMTS(WASM_CALL_INDIRECT(
                                              sig_index, WASM_I32V(7),
                                              WASM_I32V(9), WASM_GET_LOCAL(0)),
                                          WASM_DROP))),
            WASM_STMTS(WASM_DROP, WASM_I32V(kResult0))));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

WASM_COMPILED_EXEC_TEST(TryCatchCallExternal) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  HandleScope scope(CcTest::InitIsolateOnce());
  const char* source = "(function() { throw 'ball'; })";
  Handle<JSFunction> js_function =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source))));
  ManuallyImportedJSFunction import = {sigs.i_ii(), js_function};
  WasmRunner<uint32_t, uint32_t> r(execution_tier, &import);
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;
  constexpr uint32_t kJSFunc = 0;

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(kResult1),
                   WASM_IF(WASM_I32_EQZ(WASM_GET_LOCAL(0)),
                           WASM_STMTS(WASM_CALL_FUNCTION(kJSFunc, WASM_I32V(7),
                                                         WASM_I32V(9)),
                                      WASM_DROP))),
               WASM_STMTS(WASM_DROP, WASM_I32V(kResult0))));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

WASM_COMPILED_EXEC_TEST(TryCatchTrapTypeError) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  HandleScope scope(CcTest::InitIsolateOnce());
  const char* source = "(function() { return 0; })";
  Handle<JSFunction> js_function =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source))));
  // Make sure to use a signature incompatible with JS below.
  ManuallyImportedJSFunction import = {sigs.i_ll(), js_function};
  WasmRunner<uint32_t, uint32_t> r(execution_tier, &import);
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;
  constexpr uint32_t kJSFunc = 0;

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(kResult1),
                   WASM_IF(WASM_I32_EQZ(WASM_GET_LOCAL(0)),
                           WASM_STMTS(WASM_CALL_FUNCTION(kJSFunc, WASM_I64V(7),
                                                         WASM_I64V(9)),
                                      WASM_DROP))),
               WASM_STMTS(WASM_DROP, WASM_I32V(kResult0))));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

namespace {

void TestTrapNotCaught(byte* code, size_t code_size,
                       ExecutionTier execution_tier) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t> r(execution_tier, nullptr, "main",
                         kRuntimeExceptionSupport);
  r.builder().AddMemory(kWasmPageSize);
  constexpr uint32_t kResultSuccess = 23;
  constexpr uint32_t kResultCaught = 47;

  // Build a trapping helper function.
  WasmFunctionCompiler& trap_func = r.NewFunction(sigs.i_ii());
  trap_func.Build(code, code + code_size);

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(WASM_I32V(kResultSuccess),
                          WASM_CALL_FUNCTION(trap_func.function_index(),
                                             WASM_I32V(7), WASM_I32V(9)),
                          WASM_DROP),
               WASM_STMTS(WASM_DROP, WASM_I32V(kResultCaught))));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJSTraps();
}

}  // namespace

WASM_COMPILED_EXEC_TEST(TryCatchTrapUnreachable) {
  byte code[] = {WASM_UNREACHABLE};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

WASM_COMPILED_EXEC_TEST(TryCatchTrapMemOutOfBounds) {
  byte code[] = {WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(-1))};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

WASM_COMPILED_EXEC_TEST(TryCatchTrapDivByZero) {
  byte code[] = {WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I32V_1(0))};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

WASM_COMPILED_EXEC_TEST(TryCatchTrapRemByZero) {
  byte code[] = {WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_I32V_1(0))};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

}  // namespace test_run_wasm_exceptions
}  // namespace wasm
}  // namespace internal
}  // namespace v8
