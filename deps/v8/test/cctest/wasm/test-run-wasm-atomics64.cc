// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/wasm/wasm-atomics-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_atomics_64 {

void RunU64BinOp(TestExecutionTier execution_tier, WasmOpcode wasm_op,
                 Uint64BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_LOCAL_GET(0),
                              MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t initial = i;
    FOR_UINT64_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(j));
      uint64_t expected = expected_op(i, j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

#define TEST_OPERATION(Name)                                 \
  WASM_EXEC_TEST(I64Atomic##Name) {                          \
    RunU64BinOp(execution_tier, kExprI64Atomic##Name, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

void RunU32BinOp(TestExecutionTier execution_tier, WasmOpcode wasm_op,
                 Uint32BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_LOCAL_GET(0),
                              MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t initial = i;
    FOR_UINT32_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(j));
      uint32_t expected = expected_op(i, j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

#define TEST_OPERATION(Name)                                      \
  WASM_EXEC_TEST(I64Atomic##Name##32U) {                          \
    RunU32BinOp(execution_tier, kExprI64Atomic##Name##32U, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

void RunU16BinOp(TestExecutionTier tier, WasmOpcode wasm_op,
                 Uint16BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_LOCAL_GET(0),
                              MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t initial = i;
    FOR_UINT16_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(j));
      uint16_t expected = expected_op(i, j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

#define TEST_OPERATION(Name)                                      \
  WASM_EXEC_TEST(I64Atomic##Name##16U) {                          \
    RunU16BinOp(execution_tier, kExprI64Atomic##Name##16U, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

void RunU8BinOp(TestExecutionTier execution_tier, WasmOpcode wasm_op,
                Uint8BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_LOCAL_GET(0),
                              MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t initial = i;
    FOR_UINT8_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(j));
      uint8_t expected = expected_op(i, j);
      CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
    }
  }
}

#define TEST_OPERATION(Name)                                    \
  WASM_EXEC_TEST(I64Atomic##Name##8U) {                         \
    RunU8BinOp(execution_tier, kExprI64Atomic##Name##8U, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

WASM_EXEC_TEST(I64AtomicCompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange, WASM_I32V_1(0), WASM_LOCAL_GET(0),
               WASM_LOCAL_GET(1), MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t initial = i;
    FOR_UINT64_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(i, j));
      uint64_t expected = CompareExchange(initial, i, j);
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
                                   WASM_I32V_1(0), WASM_LOCAL_GET(0),
                                   WASM_LOCAL_GET(1),
                                   MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t initial = i;
    FOR_UINT32_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(i, j));
      uint32_t expected = CompareExchange(initial, i, j);
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
                                   WASM_I32V_1(0), WASM_LOCAL_GET(0),
                                   WASM_LOCAL_GET(1),
                                   MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t initial = i;
    FOR_UINT16_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(i, j));
      uint16_t expected = CompareExchange(initial, i, j);
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
                                WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                MachineRepresentation::kWord8));
  FOR_UINT8_INPUTS(i) {
    uint8_t initial = i;
    FOR_UINT8_INPUTS(j) {
      r.builder().WriteMemory(&memory[0], initial);
      CHECK_EQ(initial, r.Call(i, j));
      uint8_t expected = CompareExchange(initial, i, j);
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
    uint64_t expected = i;
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
    uint32_t expected = i;
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
    uint16_t expected = i;
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
    uint8_t expected = i;
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
        WASM_ATOMICS_STORE_OP(kExprI64AtomicStore, WASM_ZERO, WASM_LOCAL_GET(0),
                              MachineRepresentation::kWord64),
        WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad, WASM_ZERO,
                             MachineRepresentation::kWord64));

  FOR_UINT64_INPUTS(i) {
    uint64_t expected = i;
    CHECK_EQ(expected, r.Call(i));
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
                            WASM_LOCAL_GET(0), MachineRepresentation::kWord32),
      WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad32U, WASM_ZERO,
                           MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = i;
    CHECK_EQ(expected, r.Call(i));
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
                            WASM_LOCAL_GET(0), MachineRepresentation::kWord16),
      WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad16U, WASM_ZERO,
                           MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = i;
    CHECK_EQ(expected, r.Call(i));
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
                              WASM_LOCAL_GET(0), MachineRepresentation::kWord8),
        WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad8U, WASM_ZERO,
                             MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = i;
    CHECK_EQ(expected, r.Call(i));
    CHECK_EQ(i, r.builder().ReadMemory(&memory[0]));
  }
}

// Drop tests verify atomic operations are run correctly when the
// entire 64-bit output is optimized out
void RunDropTest(TestExecutionTier execution_tier, WasmOpcode wasm_op,
                 Uint64BinOp op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r,
        WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_LOCAL_GET(0),
                           MachineRepresentation::kWord64),
        WASM_DROP, WASM_LOCAL_GET(0));

  uint64_t initial = 0x1111222233334444, local = 0x1111111111111111;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(local, r.Call(local));
  uint64_t expected = op(initial, local);
  CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
}

#define TEST_OPERATION(Name)                                 \
  WASM_EXEC_TEST(I64Atomic##Name##Drop) {                    \
    RunDropTest(execution_tier, kExprI64Atomic##Name, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

WASM_EXEC_TEST(I64AtomicSub16UDrop) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));
  r.builder().SetHasSharedMemory();

  BUILD(r,
        WASM_ATOMICS_BINOP(kExprI64AtomicSub16U, WASM_I32V_1(0),
                           WASM_LOCAL_GET(0), MachineRepresentation::kWord16),
        WASM_DROP, WASM_LOCAL_GET(0));

  uint16_t initial = 0x7, local = 0xffe0;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(local, r.Call(local));
  uint16_t expected = Sub(initial, local);
  CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(I64AtomicCompareExchangeDrop) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r,
        WASM_ATOMICS_TERNARY_OP(kExprI64AtomicCompareExchange, WASM_I32V_1(0),
                                WASM_LOCAL_GET(0), WASM_LOCAL_GET(1),
                                MachineRepresentation::kWord64),
        WASM_DROP, WASM_LOCAL_GET(1));

  uint64_t initial = 0x1111222233334444, local = 0x1111111111111111;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(local, r.Call(initial, local));
  uint64_t expected = CompareExchange(initial, initial, local);
  CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(I64AtomicStoreLoadDrop) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI64AtomicStore, WASM_ZERO, WASM_LOCAL_GET(0),
                              MachineRepresentation::kWord64),
        WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad, WASM_ZERO,
                             MachineRepresentation::kWord64),
        WASM_DROP, WASM_LOCAL_GET(1));

  uint64_t store_value = 0x1111111111111111, expected = 0xC0DE;
  CHECK_EQ(expected, r.Call(store_value, expected));
  CHECK_EQ(store_value, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(I64AtomicAddConvertDrop) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r,
        WASM_ATOMICS_BINOP(kExprI64AtomicAdd, WASM_I32V_1(0), WASM_LOCAL_GET(0),
                           MachineRepresentation::kWord64),
        kExprI32ConvertI64, WASM_DROP, WASM_LOCAL_GET(0));

  uint64_t initial = 0x1111222233334444, local = 0x1111111111111111;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(local, r.Call(local));
  uint64_t expected = Add(initial, local);
  CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(I64AtomicLoadConvertDrop) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_LOAD_OP(
               kExprI64AtomicLoad, WASM_ZERO, MachineRepresentation::kWord64)));

  uint64_t initial = 0x1111222233334444;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint32_t>(initial), r.Call(initial));
}

// Convert tests verify atomic operations are run correctly when the
// upper half of the 64-bit output is optimized out
void RunConvertTest(TestExecutionTier execution_tier, WasmOpcode wasm_op,
                    Uint64BinOp op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_I32_CONVERT_I64(
               WASM_ATOMICS_BINOP(wasm_op, WASM_ZERO, WASM_LOCAL_GET(0),
                                  MachineRepresentation::kWord64)));

  uint64_t initial = 0x1111222233334444, local = 0x1111111111111111;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint32_t>(initial), r.Call(local));
  uint64_t expected = op(initial, local);
  CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
}

#define TEST_OPERATION(Name)                                    \
  WASM_EXEC_TEST(I64AtomicConvert##Name) {                      \
    RunConvertTest(execution_tier, kExprI64Atomic##Name, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

WASM_EXEC_TEST(I64AtomicConvertCompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange, WASM_I32V_1(0), WASM_LOCAL_GET(0),
               WASM_LOCAL_GET(1), MachineRepresentation::kWord64)));

  uint64_t initial = 0x1111222233334444, local = 0x1111111111111111;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint32_t>(initial), r.Call(initial, local));
  uint64_t expected = CompareExchange(initial, initial, local);
  CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
}

// The WASM_I64_EQ operation is used here to test that the index node
// is lowered correctly.
void RunNonConstIndexTest(TestExecutionTier execution_tier, WasmOpcode wasm_op,
                          Uint64BinOp op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_BINOP(
               wasm_op, WASM_I64_EQ(WASM_I64V(1), WASM_I64V(0)),
               WASM_LOCAL_GET(0), MachineRepresentation::kWord32)));

  uint64_t initial = 0x1111222233334444, local = 0x5555666677778888;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint32_t>(initial), r.Call(local));
  CHECK_EQ(static_cast<uint32_t>(op(initial, local)),
           static_cast<uint32_t>(r.builder().ReadMemory(&memory[0])));
}

// Test a set of Narrow operations
#define TEST_OPERATION(Name)                                               \
  WASM_EXEC_TEST(I64AtomicConstIndex##Name##Narrow) {                      \
    RunNonConstIndexTest(execution_tier, kExprI64Atomic##Name##32U, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

// Test a set of Regular operations
#define TEST_OPERATION(Name)                                          \
  WASM_EXEC_TEST(I64AtomicConstIndex##Name) {                         \
    RunNonConstIndexTest(execution_tier, kExprI64Atomic##Name, Name); \
  }
WASM_ATOMIC_OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

WASM_EXEC_TEST(I64AtomicNonConstIndexCompareExchangeNarrow) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange16U,
               WASM_I64_EQ(WASM_I64V(1), WASM_I64V(0)), WASM_LOCAL_GET(0),
               WASM_LOCAL_GET(1), MachineRepresentation::kWord16)));

  uint64_t initial = 0x4444333322221111, local = 0x9999888877776666;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint16_t>(initial), r.Call(initial, local));
  CHECK_EQ(static_cast<uint16_t>(CompareExchange(initial, initial, local)),
           static_cast<uint16_t>(r.builder().ReadMemory(&memory[0])));
}

WASM_EXEC_TEST(I64AtomicNonConstIndexCompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint64_t, uint64_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange,
               WASM_I64_EQ(WASM_I64V(1), WASM_I64V(0)), WASM_LOCAL_GET(0),
               WASM_LOCAL_GET(1), MachineRepresentation::kWord16)));

  uint64_t initial = 4444333322221111, local = 0x9999888877776666;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint32_t>(initial), r.Call(initial, local));
  CHECK_EQ(CompareExchange(initial, initial, local),
           r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(I64AtomicNonConstIndexLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_LOAD_OP(
               kExprI64AtomicLoad8U, WASM_I64_EQ(WASM_I64V(1), WASM_I64V(0)),
               MachineRepresentation::kWord8)));

  uint64_t expected = 0xffffeeeeddddcccc;
  r.builder().WriteMemory(&memory[0], expected);
  CHECK_EQ(static_cast<uint8_t>(expected), r.Call());
}

WASM_EXEC_TEST(I64AtomicCompareExchangeFail) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange, WASM_I32V_1(0), WASM_LOCAL_GET(0),
               WASM_LOCAL_GET(1), MachineRepresentation::kWord64));

  uint64_t initial = 0x1111222233334444, local = 0x1111111111111111,
           test = 0x2222222222222222;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(initial, r.Call(test, local));
  // No memory change on failed compare exchange
  CHECK_EQ(initial, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(I64AtomicCompareExchange32UFail) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint64_t, uint64_t, uint64_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(kExprI64AtomicCompareExchange32U,
                                   WASM_I32V_1(0), WASM_LOCAL_GET(0),
                                   WASM_LOCAL_GET(1),
                                   MachineRepresentation::kWord32));

  uint64_t initial = 0x1111222233334444, test = 0xffffffff, local = 0xeeeeeeee;
  r.builder().WriteMemory(&memory[0], initial);
  CHECK_EQ(static_cast<uint32_t>(initial), r.Call(test, local));
  // No memory change on failed compare exchange
  CHECK_EQ(initial, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(AtomicStoreNoConsideredEffectful) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  // Use {Load} instead of {ProtectedLoad}.
  FLAG_SCOPE(wasm_enforce_bounds_checks);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemoryElems<int64_t>(kWasmPageSize / sizeof(int64_t));
  r.builder().SetHasSharedMemory();
  BUILD(r, WASM_LOAD_MEM(MachineType::Int64(), WASM_ZERO),
        WASM_ATOMICS_STORE_OP(kExprI64AtomicStore, WASM_ZERO, WASM_I64V(20),
                              MachineRepresentation::kWord64),
        kExprI64Eqz);
  CHECK_EQ(1, r.Call());
}

void RunNoEffectTest(TestExecutionTier execution_tier, WasmOpcode wasm_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  // Use {Load} instead of {ProtectedLoad}.
  FLAG_SCOPE(wasm_enforce_bounds_checks);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemoryElems<int64_t>(kWasmPageSize / sizeof(int64_t));
  r.builder().SetHasSharedMemory();
  BUILD(r, WASM_LOAD_MEM(MachineType::Int64(), WASM_ZERO),
        WASM_ATOMICS_BINOP(wasm_op, WASM_ZERO, WASM_I64V(20),
                           MachineRepresentation::kWord64),
        WASM_DROP, kExprI64Eqz);
  CHECK_EQ(1, r.Call());
}

WASM_EXEC_TEST(AtomicAddNoConsideredEffectful) {
  RunNoEffectTest(execution_tier, kExprI64AtomicAdd);
}

WASM_EXEC_TEST(AtomicExchangeNoConsideredEffectful) {
  RunNoEffectTest(execution_tier, kExprI64AtomicExchange);
}

WASM_EXEC_TEST(AtomicCompareExchangeNoConsideredEffectful) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  // Use {Load} instead of {ProtectedLoad}.
  FLAG_SCOPE(wasm_enforce_bounds_checks);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  r.builder().SetHasSharedMemory();
  BUILD(r, WASM_LOAD_MEM(MachineType::Int64(), WASM_ZERO),
        WASM_ATOMICS_TERNARY_OP(kExprI64AtomicCompareExchange, WASM_ZERO,
                                WASM_I64V(0), WASM_I64V(30),
                                MachineRepresentation::kWord64),
        WASM_DROP, kExprI64Eqz);
  CHECK_EQ(1, r.Call());
}

WASM_EXEC_TEST(I64AtomicLoadUseOnlyLowWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the low word of an I64AtomicLoad.
  BUILD(r,
        WASM_I32_CONVERT_I64(WASM_ATOMICS_LOAD_OP(
            kExprI64AtomicLoad, WASM_I32V(8), MachineRepresentation::kWord64)));
  CHECK_EQ(0x90abcdef, r.Call());
}

WASM_EXEC_TEST(I64AtomicLoadUseOnlyHighWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the high word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(WASM_I64_ROR(
               WASM_ATOMICS_LOAD_OP(kExprI64AtomicLoad, WASM_I32V(8),
                                    MachineRepresentation::kWord64),
               WASM_I64V(32))));
  CHECK_EQ(0x12345678, r.Call());
}

WASM_EXEC_TEST(I64AtomicAddUseOnlyLowWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the low word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(
               WASM_ATOMICS_BINOP(kExprI64AtomicAdd, WASM_I32V(8), WASM_I64V(1),
                                  MachineRepresentation::kWord64)));
  CHECK_EQ(0x90abcdef, r.Call());
}

WASM_EXEC_TEST(I64AtomicAddUseOnlyHighWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the high word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(WASM_I64_ROR(
               WASM_ATOMICS_BINOP(kExprI64AtomicAdd, WASM_I32V(8), WASM_I64V(1),
                                  MachineRepresentation::kWord64),
               WASM_I64V(32))));
  CHECK_EQ(0x12345678, r.Call());
}

WASM_EXEC_TEST(I64AtomicCompareExchangeUseOnlyLowWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the low word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange, WASM_I32V(8), WASM_I64V(1),
               WASM_I64V(memory[1]), MachineRepresentation::kWord64)));
  CHECK_EQ(0x90abcdef, r.Call());
}

WASM_EXEC_TEST(I64AtomicCompareExchangeUseOnlyHighWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the high word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(WASM_I64_ROR(
               WASM_ATOMICS_TERNARY_OP(
                   kExprI64AtomicCompareExchange, WASM_I32V(8), WASM_I64V(1),
                   WASM_I64V(memory[1]), MachineRepresentation::kWord64),
               WASM_I64V(32))));
  CHECK_EQ(0x12345678, r.Call());
}

WASM_EXEC_TEST(I64AtomicExchangeUseOnlyLowWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the low word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(WASM_ATOMICS_BINOP(
               kExprI64AtomicExchange, WASM_I32V(8), WASM_I64V(1),
               MachineRepresentation::kWord64)));
  CHECK_EQ(0x90abcdef, r.Call());
}

WASM_EXEC_TEST(I64AtomicExchangeUseOnlyHighWord) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  uint64_t initial = 0x1234567890abcdef;
  r.builder().WriteMemory(&memory[1], initial);
  r.builder().SetHasSharedMemory();
  // Test that we can use just the high word of an I64AtomicLoad.
  BUILD(r, WASM_I32_CONVERT_I64(WASM_I64_ROR(
               WASM_ATOMICS_BINOP(kExprI64AtomicExchange, WASM_I32V(8),
                                  WASM_I64V(1), MachineRepresentation::kWord64),
               WASM_I64V(32))));
  CHECK_EQ(0x12345678, r.Call());
}

WASM_EXEC_TEST(I64AtomicCompareExchange32UZeroExtended) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  uint64_t* memory =
      r.builder().AddMemoryElems<uint64_t>(kWasmPageSize / sizeof(uint64_t));
  memory[1] = 0;
  r.builder().SetHasSharedMemory();
  // Test that the high word of the expected value is cleared in the return
  // value.
  BUILD(r, WASM_I64_EQZ(WASM_ATOMICS_TERNARY_OP(
               kExprI64AtomicCompareExchange32U, WASM_I32V(8),
               WASM_I64V(0x1234567800000000), WASM_I64V(0),
               MachineRepresentation::kWord32)));
  CHECK_EQ(1, r.Call());
}

}  // namespace test_run_wasm_atomics_64
}  // namespace wasm
}  // namespace internal
}  // namespace v8
