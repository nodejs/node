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

WASM_EXEC_TEST(TryCatchThrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(kWasmI32,
                            WASM_STMTS(WASM_I32V(kResult1),
                                       WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                               WASM_THROW(except))),
                            WASM_STMTS(WASM_I32V(kResult0)), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchThrowWithValue) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_i());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(WASM_I32V(kResult1),
                          WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                  WASM_I32V(kResult0), WASM_THROW(except))),
               WASM_STMTS(kExprNop), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryMultiCatchThrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except1 = r.builder().AddException(sigs.v_v());
  uint32_t except2 = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;
  constexpr uint32_t kResult2 = 51;

  // Build the main test function.
  BUILD(
      r, kExprTry, static_cast<byte>((kWasmI32).value_type_code()),
      WASM_STMTS(WASM_I32V(kResult2),
                 WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)), WASM_THROW(except1)),
                 WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_I32V(1)),
                         WASM_THROW(except2))),
      kExprCatch, except1, WASM_STMTS(WASM_I32V(kResult0)), kExprCatch, except2,
      WASM_STMTS(WASM_I32V(kResult1)), kExprEnd);

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
    r.CheckCallViaJS(kResult2, 2);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
    CHECK_EQ(kResult2, r.CallInterpreter(2));
  }
}

WASM_EXEC_TEST(TryCatchAllThrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r, kExprTry, static_cast<byte>((kWasmI32).value_type_code()),
        WASM_STMTS(WASM_I32V(kResult1), WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                                WASM_THROW(except))),
        kExprCatchAll, WASM_I32V(kResult0), kExprEnd);

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchCatchAllThrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except1 = r.builder().AddException(sigs.v_v());
  uint32_t except2 = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;
  constexpr uint32_t kResult2 = 51;

  // Build the main test function.
  BUILD(
      r, kExprTry, static_cast<byte>((kWasmI32).value_type_code()),
      WASM_STMTS(WASM_I32V(kResult2),
                 WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)), WASM_THROW(except1)),
                 WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(0), WASM_I32V(1)),
                         WASM_THROW(except2))),
      kExprCatch, except1, WASM_I32V(kResult0), kExprCatchAll,
      WASM_I32V(kResult1), kExprEnd);

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
    r.CheckCallViaJS(kResult2, 2);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
    CHECK_EQ(kResult2, r.CallInterpreter(2));
  }
}

WASM_EXEC_TEST(TryImplicitRethrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except1 = r.builder().AddException(sigs.v_v());
  uint32_t except2 = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;
  constexpr uint32_t kResult2 = 51;

  // Build the main test function.
  BUILD(r,
        WASM_TRY_CATCH_T(
            kWasmI32,
            WASM_TRY_CATCH_T(kWasmI32,
                             WASM_STMTS(WASM_I32V(kResult1),
                                        WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                                WASM_THROW(except2))),
                             WASM_STMTS(WASM_I32V(kResult2)), except1),
            WASM_I32V(kResult0), except2));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryDelegate) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r,
        WASM_TRY_CATCH_T(kWasmI32,
                         WASM_TRY_DELEGATE_T(
                             kWasmI32,
                             WASM_STMTS(WASM_I32V(kResult1),
                                        WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                                WASM_THROW(except))),
                             0),
                         WASM_I32V(kResult0), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryUnwind) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_TRY_UNWIND_T(
                   kWasmI32,
                   WASM_TRY_DELEGATE_T(
                       kWasmI32,
                       WASM_STMTS(WASM_I32V(kResult1),
                                  WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                          WASM_THROW(except))),
                       0),
                   kExprNop),
               WASM_I32V(kResult0), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchRethrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except1 = r.builder().AddException(sigs.v_v());
  uint32_t except2 = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;
  constexpr uint32_t kUnreachable = 51;

  // Build the main test function.
  BUILD(r,
        WASM_TRY_CATCH_CATCH_T(
            kWasmI32,
            WASM_TRY_CATCH_T(
                kWasmI32, WASM_THROW(except2),
                WASM_TRY_CATCH_T(
                    kWasmI32, WASM_THROW(except1),
                    WASM_STMTS(WASM_I32V(kUnreachable),
                               WASM_IF_ELSE(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                            WASM_RETHROW(1), WASM_RETHROW(2))),
                    except1),
                except2),
            except1, WASM_I32V(kResult0), except2, WASM_I32V(kResult1)));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryDelegateToCaller) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build the main test function.
  BUILD(r,
        WASM_TRY_CATCH_T(kWasmI32,
                         WASM_TRY_DELEGATE_T(
                             kWasmI32,
                             WASM_STMTS(WASM_I32V(kResult1),
                                        WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                                WASM_THROW(except))),
                             1),
                         WASM_I32V(kResult0), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    constexpr int64_t trap = 0xDEADBEEF;
    r.CheckCallViaJS(trap, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    constexpr int stopped = 0;
    CHECK_EQ(stopped, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchCallDirect) {
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
                          WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                  WASM_STMTS(WASM_CALL_FUNCTION(
                                                 throw_func.function_index(),
                                                 WASM_I32V(7), WASM_I32V(9)),
                                             WASM_DROP))),
               WASM_STMTS(WASM_I32V(kResult0)), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchAllCallDirect) {
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
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_STMTS(WASM_I32V(kResult1),
                          WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                                  WASM_STMTS(WASM_CALL_FUNCTION(
                                                 throw_func.function_index(),
                                                 WASM_I32V(7), WASM_I32V(9)),
                                             WASM_DROP))),
               WASM_STMTS(WASM_I32V(kResult0))));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchCallIndirect) {
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
                       WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                               WASM_STMTS(WASM_CALL_INDIRECT(
                                              sig_index, WASM_I32V(7),
                                              WASM_I32V(9), WASM_LOCAL_GET(0)),
                                          WASM_DROP))),
            WASM_I32V(kResult0), except));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
}

WASM_EXEC_TEST(TryCatchAllCallIndirect) {
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
        WASM_TRY_CATCH_ALL_T(
            kWasmI32,
            WASM_STMTS(WASM_I32V(kResult1),
                       WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                               WASM_STMTS(WASM_CALL_INDIRECT(
                                              sig_index, WASM_I32V(7),
                                              WASM_I32V(9), WASM_LOCAL_GET(0)),
                                          WASM_DROP))),
            WASM_I32V(kResult0)));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJS(kResult0, 0);
    r.CheckCallViaJS(kResult1, 1);
  } else {
    CHECK_EQ(kResult0, r.CallInterpreter(0));
    CHECK_EQ(kResult1, r.CallInterpreter(1));
  }
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
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(kResult1),
                   WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                           WASM_STMTS(WASM_CALL_FUNCTION(kJSFunc, WASM_I32V(7),
                                                         WASM_I32V(9)),
                                      WASM_DROP))),
               WASM_I32V(kResult0)));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

WASM_COMPILED_EXEC_TEST(TryCatchAllCallExternal) {
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
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(kResult1),
                   WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                           WASM_STMTS(WASM_CALL_FUNCTION(kJSFunc, WASM_I32V(7),
                                                         WASM_I32V(9)),
                                      WASM_DROP))),
               WASM_I32V(kResult0)));

  // Need to call through JS to allow for creation of stack traces.
  r.CheckCallViaJS(kResult0, 0);
  r.CheckCallViaJS(kResult1, 1);
}

namespace {

void TestTrapNotCaught(byte* code, size_t code_size,
                       TestExecutionTier execution_tier) {
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
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_STMTS(WASM_I32V(kResultSuccess),
                          WASM_CALL_FUNCTION(trap_func.function_index(),
                                             WASM_I32V(7), WASM_I32V(9)),
                          WASM_DROP),
               WASM_STMTS(WASM_I32V(kResultCaught))));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJSTraps();
  } else {
    r.CallInterpreter();
  }
}

}  // namespace

WASM_EXEC_TEST(TryCatchTrapUnreachable) {
  byte code[] = {WASM_UNREACHABLE};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

WASM_EXEC_TEST(TryCatchTrapMemOutOfBounds) {
  byte code[] = {WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V_1(-1))};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

WASM_EXEC_TEST(TryCatchTrapDivByZero) {
  byte code[] = {WASM_I32_DIVS(WASM_LOCAL_GET(0), WASM_I32V_1(0))};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

WASM_EXEC_TEST(TryCatchTrapRemByZero) {
  byte code[] = {WASM_I32_REMS(WASM_LOCAL_GET(0), WASM_I32V_1(0))};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

TEST(Regress1180457) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kUnreachable = 42;
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_TRY_DELEGATE_T(
                   kWasmI32, WASM_STMTS(WASM_I32V(kResult0), WASM_BR(0)), 0),
               WASM_I32V(kUnreachable)));

  CHECK_EQ(kResult0, r.CallInterpreter());
}

TEST(Regress1187896) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  byte try_sig = r.builder().AddSignature(sigs.v_i());
  constexpr uint32_t kResult = 23;
  BUILD(r, kExprI32Const, 0, kExprTry, try_sig, kExprDrop, kExprCatchAll,
        kExprNop, kExprEnd, kExprI32Const, kResult);
  CHECK_EQ(kResult, r.CallInterpreter());
}

TEST(Regress1190291) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  byte try_sig = r.builder().AddSignature(sigs.v_i());
  BUILD(r, kExprUnreachable, kExprTry, try_sig, kExprCatchAll, kExprEnd,
        kExprI32Const, 0);
  r.CallInterpreter();
}

TEST(Regress1186795) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  uint32_t except = r.builder().AddException(sigs.v_i());
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(0), WASM_I32V(0), WASM_I32V(0), WASM_I32V(0),
                   WASM_I32V(0), WASM_I32V(0), WASM_I32V(0),
                   WASM_TRY_UNWIND_T(
                       kWasmI32, WASM_STMTS(WASM_I32V(0), WASM_THROW(except)),
                       WASM_I32V(0)),
                   WASM_DROP, WASM_DROP, WASM_DROP, WASM_DROP, WASM_DROP,
                   WASM_DROP, WASM_DROP),
               WASM_NOP, except));
  CHECK_EQ(0, r.CallInterpreter());
}

}  // namespace test_run_wasm_exceptions
}  // namespace wasm
}  // namespace internal
}  // namespace v8
