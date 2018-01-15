// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

#define WASM_ATOMICS_OP(op) kAtomicPrefix, static_cast<byte>(op)
#define WASM_ATOMICS_BINOP(op, x, y) x, y, WASM_ATOMICS_OP(op)
#define WASM_ATOMICS_TERNARY_OP(op, x, y, z) x, y, z, WASM_ATOMICS_OP(op)

typedef uint32_t (*Uint32BinOp)(uint32_t, uint32_t);
typedef uint16_t (*Uint16BinOp)(uint16_t, uint16_t);
typedef uint8_t (*Uint8BinOp)(uint8_t, uint8_t);

template <typename T>
T Add(T a, T b) {
  return a + b;
}

template <typename T>
T Sub(T a, T b) {
  return a - b;
}

template <typename T>
T And(T a, T b) {
  return a & b;
}

template <typename T>
T Or(T a, T b) {
  return a | b;
}

template <typename T>
T Xor(T a, T b) {
  return a ^ b;
}

template <typename T>
T Exchange(T a, T b) {
  return b;
}

template <typename T>
T CompareExchange(T initial, T a, T b) {
  if (initial == a) return b;
  return a;
}

void RunU32BinOp(WasmOpcode wasm_op, Uint32BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(kExecuteCompiled);
  uint32_t* memory = r.builder().AddMemoryElems<uint32_t>(8);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0)));

  FOR_UINT32_INPUTS(i) {
    uint32_t initial = *i;
    FOR_UINT32_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*j));
      uint32_t expected = expected_op(*i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I32Add) { RunU32BinOp(kExprI32AtomicAdd, Add); }
WASM_EXEC_TEST(I32Sub) { RunU32BinOp(kExprI32AtomicSub, Sub); }
WASM_EXEC_TEST(I32And) { RunU32BinOp(kExprI32AtomicAnd, And); }
WASM_EXEC_TEST(I32Or) { RunU32BinOp(kExprI32AtomicOr, Or); }
WASM_EXEC_TEST(I32Xor) { RunU32BinOp(kExprI32AtomicXor, Xor); }
WASM_EXEC_TEST(I32Exchange) { RunU32BinOp(kExprI32AtomicExchange, Exchange); }

void RunU16BinOp(WasmOpcode wasm_op, Uint16BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(kExecuteCompiled);
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(8);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0)));

  FOR_UINT16_INPUTS(i) {
    uint16_t initial = *i;
    FOR_UINT16_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*j));
      uint16_t expected = expected_op(*i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I32Add16U) { RunU16BinOp(kExprI32AtomicAdd16U, Add); }
WASM_EXEC_TEST(I32Sub16U) { RunU16BinOp(kExprI32AtomicSub16U, Sub); }
WASM_EXEC_TEST(I32And16U) { RunU16BinOp(kExprI32AtomicAnd16U, And); }
WASM_EXEC_TEST(I32Or16U) { RunU16BinOp(kExprI32AtomicOr16U, Or); }
WASM_EXEC_TEST(I32Xor16U) { RunU16BinOp(kExprI32AtomicXor16U, Xor); }
WASM_EXEC_TEST(I32Exchange16U) {
  RunU16BinOp(kExprI32AtomicExchange16U, Exchange);
}

void RunU8BinOp(WasmOpcode wasm_op, Uint8BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(kExecuteCompiled);
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(8);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0)));

  FOR_UINT8_INPUTS(i) {
    uint8_t initial = *i;
    FOR_UINT8_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*j));
      uint8_t expected = expected_op(*i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I32Add8U) { RunU8BinOp(kExprI32AtomicAdd8U, Add); }
WASM_EXEC_TEST(I32Sub8U) { RunU8BinOp(kExprI32AtomicSub8U, Sub); }
WASM_EXEC_TEST(I32And8U) { RunU8BinOp(kExprI32AtomicAnd8U, And); }
WASM_EXEC_TEST(I32Or8U) { RunU8BinOp(kExprI32AtomicOr8U, Or); }
WASM_EXEC_TEST(I32Xor8U) { RunU8BinOp(kExprI32AtomicXor8U, Xor); }
WASM_EXEC_TEST(I32Exchange8U) {
  RunU8BinOp(kExprI32AtomicExchange8U, Exchange);
}

WASM_EXEC_TEST(I32CompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(kExecuteCompiled);
  uint32_t* memory = r.builder().AddMemoryElems<uint32_t>(8);
  BUILD(r,
        WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange, WASM_I32V_1(0),
                                WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT32_INPUTS(i) {
    uint32_t initial = *i;
    FOR_UINT32_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*i, *j));
      uint32_t expected = CompareExchange(initial, *i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I32CompareExchange16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(kExecuteCompiled);
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(8);
  BUILD(r, WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange16U,
                                   WASM_I32V_1(0), WASM_GET_LOCAL(0),
                                   WASM_GET_LOCAL(1)));

  FOR_UINT16_INPUTS(i) {
    uint16_t initial = *i;
    FOR_UINT16_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*i, *j));
      uint16_t expected = CompareExchange(initial, *i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I32CompareExchange8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(kExecuteCompiled);
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(8);
  BUILD(r,
        WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange8U, WASM_I32V_1(0),
                                WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT8_INPUTS(i) {
    uint8_t initial = *i;
    FOR_UINT8_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*i, *j));
      uint8_t expected = CompareExchange(initial, *i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}
