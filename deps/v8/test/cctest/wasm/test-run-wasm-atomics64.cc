// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/wasm/wasm-atomics-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_atomics_64 {

void RunU64BinOp(ExecutionTier execution_tier, WasmOpcode wasm_op,
                 Uint64BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t initial = *i;
    FOR_UINT64_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*j));
      uint64_t expected = expected_op(*i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I64AtomicAdd) {
  RunU64BinOp(execution_tier, kExprI64AtomicAdd, Add);
}
WASM_EXEC_TEST(I64AtomicSub) {
  RunU64BinOp(execution_tier, kExprI64AtomicSub, Sub);
}
WASM_EXEC_TEST(I64AtomicAnd) {
  RunU64BinOp(execution_tier, kExprI64AtomicAnd, And);
}
WASM_EXEC_TEST(I64AtomicOr) {
  RunU64BinOp(execution_tier, kExprI64AtomicOr, Or);
}
WASM_EXEC_TEST(I64AtomicXor) {
  RunU64BinOp(execution_tier, kExprI64AtomicXor, Xor);
}
WASM_EXEC_TEST(I64AtomicExchange) {
  RunU64BinOp(execution_tier, kExprI64AtomicExchange, Exchange);
}

void RunU32BinOp(ExecutionTier execution_tier, WasmOpcode wasm_op,
                 Uint32BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
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

WASM_EXEC_TEST(I64AtomicAdd32U) {
  RunU32BinOp(execution_tier, kExprI64AtomicAdd32U, Add);
}
WASM_EXEC_TEST(I64AtomicSub32U) {
  RunU32BinOp(execution_tier, kExprI64AtomicSub32U, Sub);
}
WASM_EXEC_TEST(I64AtomicAnd32U) {
  RunU32BinOp(execution_tier, kExprI64AtomicAnd32U, And);
}
WASM_EXEC_TEST(I64AtomicOr32U) {
  RunU32BinOp(execution_tier, kExprI64AtomicOr32U, Or);
}
WASM_EXEC_TEST(I64AtomicXor32U) {
  RunU32BinOp(execution_tier, kExprI64AtomicXor32U, Xor);
}
WASM_EXEC_TEST(I64AtomicExchange32U) {
  RunU32BinOp(execution_tier, kExprI64AtomicExchange32U, Exchange);
}

void RunU16BinOp(ExecutionTier tier, WasmOpcode wasm_op,
                 Uint16BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));

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

WASM_EXEC_TEST(I64AtomicAdd16U) {
  RunU16BinOp(execution_tier, kExprI64AtomicAdd16U, Add);
}
WASM_EXEC_TEST(I64AtomicSub16U) {
  RunU16BinOp(execution_tier, kExprI64AtomicSub16U, Sub);
}
WASM_EXEC_TEST(I64AtomicAnd16U) {
  RunU16BinOp(execution_tier, kExprI64AtomicAnd16U, And);
}
WASM_EXEC_TEST(I64AtomicOr16U) {
  RunU16BinOp(execution_tier, kExprI64AtomicOr16U, Or);
}
WASM_EXEC_TEST(I64AtomicXor16U) {
  RunU16BinOp(execution_tier, kExprI64AtomicXor16U, Xor);
}
WASM_EXEC_TEST(I64AtomicExchange16U) {
  RunU16BinOp(execution_tier, kExprI64AtomicExchange16U, Exchange);
}

void RunU8BinOp(ExecutionTier execution_tier, WasmOpcode wasm_op,
                Uint8BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);

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

WASM_EXEC_TEST(I64AtomicAdd8U) {
  RunU8BinOp(execution_tier, kExprI64AtomicAdd8U, Add);
}
WASM_EXEC_TEST(I64AtomicSub8U) {
  RunU8BinOp(execution_tier, kExprI64AtomicSub8U, Sub);
}
WASM_EXEC_TEST(I64AtomicAnd8U) {
  RunU8BinOp(execution_tier, kExprI64AtomicAnd8U, And);
}
WASM_EXEC_TEST(I64AtomicOr8U) {
  RunU8BinOp(execution_tier, kExprI64AtomicOr8U, Or);
}
WASM_EXEC_TEST(I64AtomicXor8U) {
  RunU8BinOp(execution_tier, kExprI64AtomicXor8U, Xor);
}
WASM_EXEC_TEST(I64AtomicExchange8U) {
  RunU8BinOp(execution_tier, kExprI64AtomicExchange8U, Exchange);
}

WASM_EXEC_TEST(I64AtomicCompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange, WASM_I32V_1(0), WASM_GET_LOCAL(0),
               WASM_GET_LOCAL(1), MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t initial = *i;
    FOR_UINT64_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(*i, *j));
      uint64_t expected = CompareExchange(initial, *i, *j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

WASM_EXEC_TEST(I64AtomicCompareExchange32U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(kExprI64AtomicCompareExchange32U,
                                   WASM_I32V_1(0), WASM_GET_LOCAL(0),
                                   WASM_GET_LOCAL(1),
                                   MachineRepresentation::kWord32));

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

WASM_EXEC_TEST(I64AtomicCompareExchange16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(kExprI64AtomicCompareExchange16U,
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

WASM_EXEC_TEST(I32AtomicCompareExchange8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);
  BUILD(r,
        WASM_ATOMICS_TERNARY_OP(kExprI64AtomicCompareExchange8U, WASM_I32V_1(0),
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

WASM_EXEC_TEST(I64AtomicLoad) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad, WASM_ZERO,
                                MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I64AtomicLoad32U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad32U, WASM_ZERO,
                                MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I64AtomicLoad16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad16U, WASM_ZERO,
                                MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I64AtomicLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad8U, WASM_ZERO,
                                MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = *i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I64AtomicStoreLoad) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI64AtomicStore, WASM_ZERO, WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord64),
        WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad, WASM_ZERO,
                             MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I64AtomicStoreLoad32U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));

  BUILD(
      r,
      WASM_ATOMICS_STORE_OP(kExprI64AtomicStore32U, WASM_ZERO,
                            WASM_GET_LOCAL(0), MachineRepresentation::kWord32),
      WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad32U, WASM_ZERO,
                           MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I64AtomicStoreLoad16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));

  BUILD(
      r,
      WASM_ATOMICS_STORE_OP(kExprI64AtomicStore16U, WASM_ZERO,
                            WASM_GET_LOCAL(0), MachineRepresentation::kWord16),
      WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad16U, WASM_ZERO,
                           MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I64AtomicStoreLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI64AtomicStore8U, WASM_ZERO,
                              WASM_GET_LOCAL(0), MachineRepresentation::kWord8),
        WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad8U, WASM_ZERO,
                             MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = *i;
    CHECK_EQ(expected, r.Call(*i));
    CHECK_EQ(*i, r.builder().ReadMemory(&memory[0]));
  }
}

}  // namespace test_run_wasm_atomics_64
}  // namespace wasm
}  // namespace internal
}  // namespace v8
