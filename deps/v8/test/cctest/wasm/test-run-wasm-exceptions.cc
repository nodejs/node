// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-function.h"
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
  byte except = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_i());
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
  byte except1 = r.builder().AddException(sigs.v_v());
  byte except2 = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_v());
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
  byte except1 = r.builder().AddException(sigs.v_v());
  byte except2 = r.builder().AddException(sigs.v_v());
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
  byte except1 = r.builder().AddException(sigs.v_v());
  byte except2 = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_v());
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

WASM_EXEC_TEST(TestCatchlessTry) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t> r(execution_tier);
  byte except = r.builder().AddException(sigs.v_i());
  BUILD(r,
        WASM_TRY_CATCH_T(
            kWasmI32,
            WASM_TRY_T(kWasmI32, WASM_STMTS(WASM_I32V(0), WASM_THROW(except))),
            WASM_NOP, except));
  if (execution_tier != TestExecutionTier::kInterpreter) {
    r.CheckCallViaJS(0);
  } else {
    CHECK_EQ(0, r.CallInterpreter());
  }
}

WASM_EXEC_TEST(TryCatchRethrow) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  byte except1 = r.builder().AddException(sigs.v_v());
  byte except2 = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_v());
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
  byte except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build a throwing helper function.
  WasmFunctionCompiler& throw_func = r.NewFunction(sigs.i_ii());
  BUILD(throw_func, WASM_THROW(except));

  // Add an indirect function table.
  uint16_t indirect_function_table[] = {
      static_cast<uint16_t>(throw_func.function_index())};
  r.builder().AddIndirectFunctionTable(indirect_function_table,
                                       arraysize(indirect_function_table));

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(kResult1),
                   WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                           WASM_STMTS(WASM_CALL_INDIRECT(
                                          throw_func.sig_index(), WASM_I32V(7),
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
  byte except = r.builder().AddException(sigs.v_v());
  constexpr uint32_t kResult0 = 23;
  constexpr uint32_t kResult1 = 42;

  // Build a throwing helper function.
  WasmFunctionCompiler& throw_func = r.NewFunction(sigs.i_ii());
  BUILD(throw_func, WASM_THROW(except));

  // Add an indirect function table.
  uint16_t indirect_function_table[] = {
      static_cast<uint16_t>(throw_func.function_index())};
  r.builder().AddIndirectFunctionTable(indirect_function_table,
                                       arraysize(indirect_function_table));

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_STMTS(
                   WASM_I32V(kResult1),
                   WASM_IF(WASM_I32_EQZ(WASM_LOCAL_GET(0)),
                           WASM_STMTS(WASM_CALL_INDIRECT(
                                          throw_func.sig_index(), WASM_I32V(7),
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

  // Add an indirect function table.
  const int kTableSize = 2;
  r.builder().AddIndirectFunctionTable(nullptr, kTableSize);

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

WASM_EXEC_TEST(TryCatchTrapTableFill) {
  int table_index = 0;
  int length = 10;  // OOB.
  int start = 10;   // OOB.
  byte code[] = {WASM_TABLE_FILL(table_index, WASM_I32V(length),
                                 WASM_REF_NULL(kFuncRefCode), WASM_I32V(start)),
                 WASM_I32V_1(42)};
  TestTrapNotCaught(code, arraysize(code), execution_tier);
}

namespace {
// TODO(cleanup): Define in cctest.h and re-use where appropriate.
class IsolateScope {
 public:
  IsolateScope() {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    isolate_ = v8::Isolate::New(create_params);
    isolate_->Enter();
  }

  ~IsolateScope() {
    isolate_->Exit();
    isolate_->Dispose();
  }

  v8::Isolate* isolate() { return isolate_; }
  Isolate* i_isolate() { return reinterpret_cast<Isolate*>(isolate_); }

 private:
  v8::Isolate* isolate_;
};
}  // namespace

UNINITIALIZED_WASM_EXEC_TEST(TestStackOverflowNotCaught) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  // v8_flags.stack_size must be set before isolate initialization.
  FlagScope<int32_t> stack_size(&v8_flags.stack_size, 8);

  IsolateScope isolate_scope;
  LocalContext context(isolate_scope.isolate());

  WasmRunner<uint32_t> r(execution_tier, nullptr, "main",
                         kRuntimeExceptionSupport, kMemory32,
                         isolate_scope.i_isolate());

  // Build a function that calls itself until stack overflow.
  WasmFunctionCompiler& stack_overflow = r.NewFunction(sigs.v_v());
  byte stack_overflow_code[] = {
      kExprCallFunction, static_cast<byte>(stack_overflow.function_index())};
  stack_overflow.Build(stack_overflow_code,
                       stack_overflow_code + arraysize(stack_overflow_code));

  // Build the main test function.
  BUILD(r, WASM_TRY_CATCH_ALL_T(
               kWasmI32,
               WASM_STMTS(WASM_I32V(1), kExprCallFunction,
                          static_cast<byte>(stack_overflow.function_index())),
               WASM_STMTS(WASM_I32V(1))));

  if (execution_tier != TestExecutionTier::kInterpreter) {
    // Need to call through JS to allow for creation of stack traces.
    r.CheckCallViaJSTraps();
  } else {
    constexpr int stopped = 0;
    CHECK_EQ(stopped, r.CallInterpreter());
  }
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
  byte except = r.builder().AddException(sigs.v_i());
  BUILD(r,
        WASM_TRY_CATCH_T(
            kWasmI32,
            WASM_STMTS(WASM_I32V(0), WASM_I32V(0), WASM_I32V(0), WASM_I32V(0),
                       WASM_I32V(0), WASM_I32V(0), WASM_I32V(0),
                       WASM_TRY_T(kWasmI32,
                                  WASM_STMTS(WASM_I32V(0), WASM_THROW(except))),
                       WASM_DROP, WASM_DROP, WASM_DROP, WASM_DROP, WASM_DROP,
                       WASM_DROP, WASM_DROP),
            WASM_NOP, except));
  CHECK_EQ(0, r.CallInterpreter());
}

TEST(Regress1197408) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(
      TestExecutionTier::kInterpreter);
  byte sig_id = r.builder().AddSignature(sigs.i_iii());
  BUILD(r, WASM_STMTS(WASM_I32V(0), WASM_I32V(0), WASM_I32V(0), kExprTry,
                      sig_id, kExprTry, sig_id, kExprCallFunction, 0,
                      kExprDelegate, 0, kExprDelegate, 0));
  CHECK_EQ(0, r.CallInterpreter(0, 0, 0));
}

TEST(Regress1212396) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
  byte except = r.builder().AddException(sigs.v_v());
  BUILD(r, kExprTry, kVoidCode, kExprTry, kVoidCode, kExprI32Const, 0,
        kExprThrow, except, kExprDelegate, 0, kExprCatch, except, kExprEnd,
        kExprI32Const, 42);
  CHECK_EQ(42, r.CallInterpreter());
}

TEST(Regress1219746) {
  TestSignatures sigs;
  EXPERIMENTAL_FLAG_SCOPE(eh);
  WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
  BUILD(r, kExprTry, kVoidCode, kExprI32Const, 0, kExprEnd);
  CHECK_EQ(0, r.CallInterpreter());
}

}  // namespace test_run_wasm_exceptions
}  // namespace wasm
}  // namespace internal
}  // namespace v8
