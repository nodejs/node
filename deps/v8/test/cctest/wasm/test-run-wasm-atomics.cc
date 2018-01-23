// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

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

void RunU32BinOp(WasmExecutionMode execution_mode, WasmOpcode wasm_op,
                 Uint32BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_mode);
  uint32_t* memory = r.builder().AddMemoryElems<uint32_t>(8);
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord32));

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

WASM_EXEC_TEST(I32AtomicAdd) {
  RunU32BinOp(execution_mode, kExprI32AtomicAdd, Add);
}
WASM_EXEC_TEST(I32AtomicSub) {
  RunU32BinOp(execution_mode, kExprI32AtomicSub, Sub);
}
WASM_EXEC_TEST(I32AtomicAnd) {
  RunU32BinOp(execution_mode, kExprI32AtomicAnd, And);
}
WASM_EXEC_TEST(I32AtomicOr) {
  RunU32BinOp(execution_mode, kExprI32AtomicOr, Or);
}
WASM_EXEC_TEST(I32AtomicXor) {
  RunU32BinOp(execution_mode, kExprI32AtomicXor, Xor);
}
WASM_EXEC_TEST(I32AtomicExchange) {
  RunU32BinOp(execution_mode, kExprI32AtomicExchange, Exchange);
}

void RunU16BinOp(WasmExecutionMode mode, WasmOpcode wasm_op,
                 Uint16BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(mode);
  r.builder().SetHasSharedMemory();
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(8);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord16));

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

WASM_EXEC_TEST(I32AtomicAdd16U) {
  RunU16BinOp(execution_mode, kExprI32AtomicAdd16U, Add);
}
WASM_EXEC_TEST(I32AtomicSub16U) {
  RunU16BinOp(execution_mode, kExprI32AtomicSub16U, Sub);
}
WASM_EXEC_TEST(I32AtomicAnd16U) {
  RunU16BinOp(execution_mode, kExprI32AtomicAnd16U, And);
}
WASM_EXEC_TEST(I32AtomicOr16U) {
  RunU16BinOp(execution_mode, kExprI32AtomicOr16U, Or);
}
WASM_EXEC_TEST(I32AtomicXor16U) {
  RunU16BinOp(execution_mode, kExprI32AtomicXor16U, Xor);
}
WASM_EXEC_TEST(I32AtomicExchange16U) {
  RunU16BinOp(execution_mode, kExprI32AtomicExchange16U, Exchange);
}

void RunU8BinOp(WasmExecutionMode execution_mode, WasmOpcode wasm_op,
                Uint8BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(8);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord8));

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

WASM_EXEC_TEST(I32AtomicAdd8U) {
  RunU8BinOp(execution_mode, kExprI32AtomicAdd8U, Add);
}
WASM_EXEC_TEST(I32AtomicSub8U) {
  RunU8BinOp(execution_mode, kExprI32AtomicSub8U, Sub);
}
WASM_EXEC_TEST(I32AtomicAnd8U) {
  RunU8BinOp(execution_mode, kExprI32AtomicAnd8U, And);
}
WASM_EXEC_TEST(I32AtomicOr8U) {
  RunU8BinOp(execution_mode, kExprI32AtomicOr8U, Or);
}
WASM_EXEC_TEST(I32AtomicXor8U) {
  RunU8BinOp(execution_mode, kExprI32AtomicXor8U, Xor);
}
WASM_EXEC_TEST(I32AtomicExchange8U) {
  RunU8BinOp(execution_mode, kExprI32AtomicExchange8U, Exchange);
}

WASM_COMPILED_EXEC_TEST(I32AtomicCompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint32_t* memory = r.builder().AddMemoryElems<uint32_t>(8);
  BUILD(r, WASM_ATOMICS_TERNARY_OP(
               kExprI32AtomicCompareExchange, WASM_I32V_1(0), WASM_GET_LOCAL(0),
               WASM_GET_LOCAL(1), MachineRepresentation::kWord32));

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

WASM_COMPILED_EXEC_TEST(I32AtomicCompareExchange16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(8);
  BUILD(r, WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange16U,
                                   WASM_I32V_1(0), WASM_GET_LOCAL(0),
                                   WASM_GET_LOCAL(1),
                                   MachineRepresentation::kWord16));

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

WASM_COMPILED_EXEC_TEST(I32AtomicCompareExchange8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(8);
  BUILD(r,
        WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange8U, WASM_I32V_1(0),
                                WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                MachineRepresentation::kWord8));

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

WASM_EXEC_TEST(I32AtomicLoad) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint32_t* memory = r.builder().AddMemoryElems<uint32_t>(8);
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad, WASM_ZERO,
                                MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32AtomicLoad16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(8);
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad16U, WASM_ZERO,
                                MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32AtomicLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(8);
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad8U, WASM_ZERO,
                                MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32AtomicStoreLoad) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint32_t* memory = r.builder().AddMemoryElems<uint32_t>(8);

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI32AtomicStore, WASM_ZERO, WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord32),
        WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad, WASM_ZERO,
                             MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I32AtomicStoreLoad16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(8);

  BUILD(
      r,
      WASM_ATOMICS_STORE_OP(kExprI32AtomicStore16U, WASM_ZERO,
                            WASM_GET_LOCAL(0), MachineRepresentation::kWord16),
      WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad16U, WASM_ZERO,
                           MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I32AtomicStoreLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_mode);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(8);

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI32AtomicStore8U, WASM_ZERO,
                              WASM_GET_LOCAL(0), MachineRepresentation::kWord8),
        WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad8U, WASM_ZERO,
                             MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(*i, r.builder().ReadMemory(&memory[0]));
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
