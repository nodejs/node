// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "src/codegen/assembler-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-interpreter.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_interpreter {

TEST(Run_WasmInt8Const_i) {
  WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
  const byte kExpectedValue = 109;
  // return(kExpectedValue)
  BUILD(r, WASM_I32V_2(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}

TEST(Run_WasmIfElse) {
  WasmRunner<int32_t, int32_t> r(TestExecutionTier::kInterpreter);
  BUILD(r, WASM_IF_ELSE_I(WASM_LOCAL_GET(0), WASM_I32V_1(9), WASM_I32V_1(10)));
  CHECK_EQ(10, r.Call(0));
  CHECK_EQ(9, r.Call(1));
}

TEST(Run_WasmIfReturn) {
  WasmRunner<int32_t, int32_t> r(TestExecutionTier::kInterpreter);
  BUILD(r, WASM_IF(WASM_LOCAL_GET(0), WASM_RETURN(WASM_I32V_2(77))),
        WASM_I32V_2(65));
  CHECK_EQ(65, r.Call(0));
  CHECK_EQ(77, r.Call(1));
}

TEST(Run_WasmNopsN) {
  const int kMaxNops = 10;
  byte code[kMaxNops + 2];
  for (int nops = 0; nops < kMaxNops; nops++) {
    byte expected = static_cast<byte>(20 + nops);
    memset(code, kExprNop, sizeof(code));
    code[nops] = kExprI32Const;
    code[nops + 1] = expected;

    WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
    r.Build(code, code + nops + 2);
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmConstsN) {
  const int kMaxConsts = 5;
  byte code[kMaxConsts * 3];
  int32_t expected = 0;
  for (int count = 1; count < kMaxConsts; count++) {
    for (int i = 0; i < count; i++) {
      byte val = static_cast<byte>(count * 10 + i);
      code[i * 3] = kExprI32Const;
      code[i * 3 + 1] = val;
      if (i == (count - 1)) {
        code[i * 3 + 2] = kExprNop;
        expected = val;
      } else {
        code[i * 3 + 2] = kExprDrop;
      }
    }

    WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
    r.Build(code, code + (count * 3));
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmBlocksN) {
  const int kMaxNops = 10;
  const int kExtra = 5;
  byte code[kMaxNops + kExtra];
  for (int nops = 0; nops < kMaxNops; nops++) {
    byte expected = static_cast<byte>(30 + nops);
    memset(code, kExprNop, sizeof(code));
    code[0] = kExprBlock;
    code[1] = kI32Code;
    code[2 + nops] = kExprI32Const;
    code[2 + nops + 1] = expected;
    code[2 + nops + 2] = kExprEnd;

    WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
    r.Build(code, code + nops + kExtra);
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmBlockBreakN) {
  const int kMaxNops = 10;
  const int kExtra = 6;
  int run = 0;
  byte code[kMaxNops + kExtra];
  for (int nops = 0; nops < kMaxNops; nops++) {
    // Place the break anywhere within the block.
    for (int index = 0; index < nops; index++) {
      memset(code, kExprNop, sizeof(code));
      code[0] = kExprBlock;
      code[1] = kI32Code;
      code[sizeof(code) - 1] = kExprEnd;

      int expected = run++;
      code[2 + index + 0] = kExprI32Const;
      code[2 + index + 1] = static_cast<byte>(expected);
      code[2 + index + 2] = kExprBr;
      code[2 + index + 3] = 0;

      WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
      r.Build(code, code + kMaxNops + kExtra);
      CHECK_EQ(expected, r.Call());
    }
  }
}

TEST(Run_Wasm_nested_ifs_i) {
  WasmRunner<int32_t, int32_t, int32_t> r(TestExecutionTier::kInterpreter);

  BUILD(
      r,
      WASM_IF_ELSE_I(
          WASM_LOCAL_GET(0),
          WASM_IF_ELSE_I(WASM_LOCAL_GET(1), WASM_I32V_1(11), WASM_I32V_1(12)),
          WASM_IF_ELSE_I(WASM_LOCAL_GET(1), WASM_I32V_1(13), WASM_I32V_1(14))));

  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}

// Repeated from test-run-wasm.cc to avoid poluting header files.
template <typename T>
static T factorial(T v) {
  T expected = 1;
  for (T i = v; i > 1; i--) {
    expected *= i;
  }
  return expected;
}

// Basic test of return call in interpreter. Good old factorial.
TEST(Run_Wasm_returnCallFactorial) {
  EXPERIMENTAL_FLAG_SCOPE(return_call);
  // Run in bounded amount of stack - 8kb.
  FlagScope<int32_t> stack_size(&v8::internal::FLAG_stack_size, 8);

  WasmRunner<uint32_t, int32_t> r(TestExecutionTier::kInterpreter);

  WasmFunctionCompiler& fact_aux_fn =
      r.NewFunction<int32_t, int32_t, int32_t>("fact_aux");

  BUILD(r, WASM_RETURN_CALL_FUNCTION(fact_aux_fn.function_index(),
                                     WASM_LOCAL_GET(0), WASM_I32V(1)));

  BUILD(fact_aux_fn,
        WASM_IF_ELSE_I(
            WASM_I32_EQ(WASM_I32V(1), WASM_LOCAL_GET(0)), WASM_LOCAL_GET(1),
            WASM_RETURN_CALL_FUNCTION(
                fact_aux_fn.function_index(),
                WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_I32V(1)),
                WASM_I32_MUL(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)))));

  // Runs out of stack space without using return call.
  uint32_t test_values[] = {1, 2, 5, 10, 20, 20000};

  for (uint32_t v : test_values) {
    uint32_t found = r.Call(v);
    CHECK_EQ(factorial(v), found);
  }
}

TEST(Run_Wasm_returnCallFactorial64) {
  EXPERIMENTAL_FLAG_SCOPE(return_call);

  int32_t test_values[] = {1, 2, 5, 10, 20};
  WasmRunner<int64_t, int32_t> r(TestExecutionTier::kInterpreter);

  WasmFunctionCompiler& fact_aux_fn =
      r.NewFunction<int64_t, int32_t, int64_t>("fact_aux");

  BUILD(r, WASM_RETURN_CALL_FUNCTION(fact_aux_fn.function_index(),
                                     WASM_LOCAL_GET(0), WASM_I64V(1)));

  BUILD(fact_aux_fn,
        WASM_IF_ELSE_L(
            WASM_I32_EQ(WASM_I32V(1), WASM_LOCAL_GET(0)), WASM_LOCAL_GET(1),
            WASM_RETURN_CALL_FUNCTION(
                fact_aux_fn.function_index(),
                WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_I32V(1)),
                WASM_I64_MUL(WASM_I64_SCONVERT_I32(WASM_LOCAL_GET(0)),
                             WASM_LOCAL_GET(1)))));

  for (int32_t v : test_values) {
    CHECK_EQ(factorial<int64_t>(v), r.Call(v));
  }
}

TEST(Run_Wasm_returnCallIndirectFactorial) {
  EXPERIMENTAL_FLAG_SCOPE(return_call);

  TestSignatures sigs;

  WasmRunner<uint32_t, uint32_t> r(TestExecutionTier::kInterpreter);

  WasmFunctionCompiler& fact_aux_fn = r.NewFunction(sigs.i_ii(), "fact_aux");
  fact_aux_fn.SetSigIndex(0);

  byte sig_index = r.builder().AddSignature(sigs.i_ii());

  // Function table.
  uint16_t indirect_function_table[] = {
      static_cast<uint16_t>(fact_aux_fn.function_index())};

  r.builder().AddIndirectFunctionTable(indirect_function_table,
                                       arraysize(indirect_function_table));

  BUILD(r, WASM_RETURN_CALL_INDIRECT(sig_index, WASM_LOCAL_GET(0), WASM_I32V(1),
                                     WASM_ZERO));

  BUILD(
      fact_aux_fn,
      WASM_IF_ELSE_I(
          WASM_I32_EQ(WASM_I32V(1), WASM_LOCAL_GET(0)), WASM_LOCAL_GET(1),
          WASM_RETURN_CALL_INDIRECT(
              sig_index, WASM_I32_SUB(WASM_LOCAL_GET(0), WASM_I32V(1)),
              WASM_I32_MUL(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1)), WASM_ZERO)));

  uint32_t test_values[] = {1, 2, 5, 10, 20};

  for (uint32_t v : test_values) {
    CHECK_EQ(factorial(v), r.Call(v));
  }
}
// Make tests more robust by not hard-coding offsets of various operations.
// The {Find} method finds the offsets for the given bytecodes, returning
// the offsets in an array.
std::unique_ptr<int[]> Find(byte* code, size_t code_size, int n, ...) {
  va_list vl;
  va_start(vl, n);

  std::unique_ptr<int[]> offsets(new int[n]);

  for (int i = 0; i < n; i++) {
    offsets[i] = -1;
  }

  int pos = 0;
  WasmOpcode current = static_cast<WasmOpcode>(va_arg(vl, int));
  for (size_t i = 0; i < code_size; i++) {
    if (code[i] == current) {
      offsets[pos++] = static_cast<int>(i);
      if (pos == n) break;
      current = static_cast<WasmOpcode>(va_arg(vl, int));
    }
  }
  va_end(vl);

  return offsets;
}

TEST(Step_I32Mul) {
  static const int kTraceLength = 4;
  byte code[] = {WASM_I32_MUL(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))};

  WasmRunner<int32_t, uint32_t, uint32_t> r(TestExecutionTier::kInterpreter);

  r.Build(code, code + arraysize(code));

  WasmInterpreter* interpreter = r.interpreter();

  FOR_UINT32_INPUTS(a) {
    for (uint32_t b = 33; b < 3000000000u; b += 1000000000u) {
      interpreter->Reset();
      WasmValue args[] = {WasmValue(a), WasmValue(b)};
      interpreter->InitFrame(r.function(), args);

      // Run instructions one by one.
      for (int i = 0; i < kTraceLength - 1; i++) {
        interpreter->Step();
        // Check the interpreter stopped.
        CHECK_EQ(WasmInterpreter::PAUSED, interpreter->state());
      }

      // Run last instruction.
      interpreter->Step();

      // Check the interpreter finished with the right value.
      CHECK_EQ(WasmInterpreter::FINISHED, interpreter->state());
      uint32_t expected = (a) * (b);
      CHECK_EQ(expected, interpreter->GetReturnValue().to<uint32_t>());
    }
  }
}

TEST(MemoryGrow) {
  {
    WasmRunner<int32_t, uint32_t> r(TestExecutionTier::kInterpreter);
    r.builder().AddMemory(kWasmPageSize);
    r.builder().SetMaxMemPages(10);
    BUILD(r, WASM_MEMORY_GROW(WASM_LOCAL_GET(0)));
    CHECK_EQ(1, r.Call(1));
  }
  {
    WasmRunner<int32_t, uint32_t> r(TestExecutionTier::kInterpreter);
    r.builder().AddMemory(kWasmPageSize);
    r.builder().SetMaxMemPages(10);
    BUILD(r, WASM_MEMORY_GROW(WASM_LOCAL_GET(0)));
    CHECK_EQ(-1, r.Call(11));
  }
}

TEST(MemoryGrowPreservesData) {
  int32_t index = 16;
  int32_t value = 2335;
  WasmRunner<int32_t, uint32_t> r(TestExecutionTier::kInterpreter);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(
      r,
      WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index), WASM_I32V(value)),
      WASM_MEMORY_GROW(WASM_LOCAL_GET(0)), WASM_DROP,
      WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index)));
  CHECK_EQ(value, r.Call(1));
}

TEST(MemoryGrowInvalidSize) {
  // Grow memory by an invalid amount without initial memory.
  WasmRunner<int32_t, uint32_t> r(TestExecutionTier::kInterpreter);
  r.builder().AddMemory(kWasmPageSize);
  BUILD(r, WASM_MEMORY_GROW(WASM_LOCAL_GET(0)));
  CHECK_EQ(-1, r.Call(1048575));
}

TEST(ReferenceTypeLocals) {
  {
    WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_REF_IS_NULL(WASM_REF_NULL(kAnyRefCode)));
    CHECK_EQ(1, r.Call());
  }
  {
    WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
    r.AllocateLocal(kWasmAnyRef);
    BUILD(r, WASM_REF_IS_NULL(WASM_LOCAL_GET(0)));
    CHECK_EQ(1, r.Call());
  }
  {
    WasmRunner<int32_t> r(TestExecutionTier::kInterpreter);
    r.AllocateLocal(kWasmAnyRef);
    BUILD(r, WASM_REF_IS_NULL(WASM_LOCAL_TEE(0, WASM_REF_NULL(kAnyRefCode))));
    CHECK_EQ(1, r.Call());
  }
}

TEST(TestPossibleNondeterminism) {
  {
    WasmRunner<int32_t, float> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_I32_REINTERPRET_F32(WASM_LOCAL_GET(0)));
    r.Call(1048575.5f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    WasmRunner<int64_t, double> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_I64_REINTERPRET_F64(WASM_LOCAL_GET(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    WasmRunner<float, float> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F32_COPYSIGN(WASM_F32(42.0f), WASM_LOCAL_GET(0)));
    r.Call(16.0f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    WasmRunner<double, double> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F64_COPYSIGN(WASM_F64(42.0), WASM_LOCAL_GET(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    int32_t index = 16;
    WasmRunner<int32_t, float> r(TestExecutionTier::kInterpreter);
    r.builder().AddMemory(kWasmPageSize);
    BUILD(r,
          WASM_STORE_MEM(MachineType::Float32(), WASM_I32V(index),
                         WASM_LOCAL_GET(0)),
          WASM_I32V(index));
    r.Call(1345.3456f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    int32_t index = 16;
    WasmRunner<int32_t, double> r(TestExecutionTier::kInterpreter);
    r.builder().AddMemory(kWasmPageSize);
    BUILD(r,
          WASM_STORE_MEM(MachineType::Float64(), WASM_I32V(index),
                         WASM_LOCAL_GET(0)),
          WASM_I32V(index));
    r.Call(1345.3456);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    WasmRunner<float, float> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F32_ADD(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)));
    r.Call(1048575.5f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    WasmRunner<double, double> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F64_ADD(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    WasmRunner<int32_t, float> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F32_EQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    WasmRunner<int32_t, double> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F64_EQ(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(!r.possible_nondeterminism());
  }
  {
    WasmRunner<float, float> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F32_MIN(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)));
    r.Call(1048575.5f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    WasmRunner<double, double> r(TestExecutionTier::kInterpreter);
    BUILD(r, WASM_F64_MAX(WASM_LOCAL_GET(0), WASM_LOCAL_GET(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
}

TEST(InterpreterLoadWithoutMemory) {
  WasmRunner<int32_t, int32_t> r(TestExecutionTier::kInterpreter);
  r.builder().AddMemory(0);
  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_LOCAL_GET(0)));
  CHECK_TRAP32(r.Call(0));
}

TEST(Regress1111015) {
  EXPERIMENTAL_FLAG_SCOPE(return_call);
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  WasmFunctionCompiler& f = r.NewFunction<int32_t>("f");
  BUILD(r, WASM_BLOCK_I(WASM_RETURN_CALL_FUNCTION0(f.function_index()),
                        kExprDrop));
  BUILD(f, WASM_I32V(0));
}

TEST(Regress1092130) {
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  TestSignatures sigs;
  byte sig_v_i = r.builder().AddSignature(sigs.v_i());
  BUILD(r, WASM_I32V(0),
        WASM_IF_ELSE_I(
            WASM_I32V(0),
            WASM_SEQ(WASM_UNREACHABLE, WASM_BLOCK_X(sig_v_i, WASM_NOP)),
            WASM_I32V(0)),
        WASM_DROP);
  r.Call();
}

TEST(Regress1247119) {
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  BUILD(r, kExprLoop, 0, kExprTry, 0, kExprUnreachable, kExprDelegate, 0,
        kExprEnd);
  r.Call();
}

TEST(Regress1246712) {
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  TestSignatures sigs;
  const int kExpected = 1;
  uint8_t except = r.builder().AddException(sigs.v_v());
  BUILD(r, kExprTry, kWasmI32.value_type_code(), kExprTry,
        kWasmI32.value_type_code(), kExprThrow, except, kExprEnd, kExprCatchAll,
        kExprI32Const, kExpected, kExprEnd);
  CHECK_EQ(kExpected, r.Call());
}

TEST(Regress1249306) {
  WasmRunner<uint32_t> r(TestExecutionTier::kInterpreter);
  BUILD(r, kExprTry, kVoid, kExprCatchAll, kExprTry, kVoid, kExprDelegate, 0,
        kExprEnd, kExprI32Const, 0);
  r.Call();
}

TEST(Regress1251845) {
  WasmRunner<uint32_t, uint32_t, uint32_t, uint32_t> r(
      TestExecutionTier::kInterpreter);
  ValueType reps[] = {kWasmI32, kWasmI32, kWasmI32, kWasmI32};
  FunctionSig sig_iii_i(1, 3, reps);
  byte sig = r.builder().AddSignature(&sig_iii_i);
  BUILD(r, kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 0, kExprTry, sig,
        kExprI32Const, 0, kExprTry, 0, kExprTry, 0, kExprI32Const, 0, kExprTry,
        sig, kExprUnreachable, kExprTry, 0, kExprUnreachable, kExprEnd,
        kExprTry, sig, kExprUnreachable, kExprEnd, kExprEnd, kExprUnreachable,
        kExprEnd, kExprEnd, kExprUnreachable, kExprEnd);
  r.Call(0, 0, 0);
}

}  // namespace test_run_wasm_interpreter
}  // namespace wasm
}  // namespace internal
}  // namespace v8
