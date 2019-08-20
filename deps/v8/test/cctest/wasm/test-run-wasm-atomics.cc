// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/wasm/wasm-atomics-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_atomics {

void RunU32BinOp(ExecutionTier execution_tier, WasmOpcode wasm_op,
                 Uint32BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  r.builder().SetHasSharedMemory();

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
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

#define TEST_OPERATION(Name)                                 \
  WASM_EXEC_TEST(I32Atomic##Name) {                          \
    RunU32BinOp(execution_tier, kExprI32Atomic##Name, Name); \
  }
OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

void RunU16BinOp(ExecutionTier tier, WasmOpcode wasm_op,
                 Uint16BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
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
  WASM_EXEC_TEST(I32Atomic##Name##16U) {                          \
    RunU16BinOp(execution_tier, kExprI32Atomic##Name##16U, Name); \
  }
OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

void RunU8BinOp(ExecutionTier execution_tier, WasmOpcode wasm_op,
                Uint8BinOp expected_op) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);

  BUILD(r, WASM_ATOMICS_BINOP(wasm_op, WASM_I32V_1(0), WASM_GET_LOCAL(0),
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
  WASM_EXEC_TEST(I32Atomic##Name##8U) {                         \
    RunU8BinOp(execution_tier, kExprI32Atomic##Name##8U, Name); \
  }
OPERATION_LIST(TEST_OPERATION)
#undef TEST_OPERATION

WASM_EXEC_TEST(I32AtomicCompareExchange) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(
               kExprI32AtomicCompareExchange, WASM_I32V_1(0), WASM_GET_LOCAL(0),
               WASM_GET_LOCAL(1), MachineRepresentation::kWord32));

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

WASM_EXEC_TEST(I32AtomicCompareExchange16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange16U,
                                   WASM_I32V_1(0), WASM_GET_LOCAL(0),
                                   WASM_GET_LOCAL(1),
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
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);
  BUILD(r,
        WASM_ATOMICS_TERNARY_OP(kExprI32AtomicCompareExchange8U, WASM_I32V_1(0),
                                WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
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

WASM_EXEC_TEST(I32AtomicCompareExchange_fail) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  BUILD(r, WASM_ATOMICS_TERNARY_OP(
               kExprI32AtomicCompareExchange, WASM_I32V_1(0), WASM_GET_LOCAL(0),
               WASM_GET_LOCAL(1), MachineRepresentation::kWord32));

  // The original value at the memory location.
  uint32_t old_val = 4;
  // The value we use as the expected value for the compare-exchange so that it
  // fails.
  uint32_t expected = 6;
  // The new value for the compare-exchange.
  uint32_t new_val = 5;

  r.builder().WriteMemory(&memory[0], old_val);
  CHECK_EQ(old_val, r.Call(expected, new_val));
}

WASM_EXEC_TEST(I32AtomicLoad) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad, WASM_ZERO,
                                MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32AtomicLoad16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad16U, WASM_ZERO,
                                MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32AtomicLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);
  BUILD(r, WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad8U, WASM_ZERO,
                                MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = i;
    r.builder().WriteMemory(&memory[0], expected);
    CHECK_EQ(expected, r.Call());
  }
}

WASM_EXEC_TEST(I32AtomicStoreLoad) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI32AtomicStore, WASM_ZERO, WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord32),
        WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad, WASM_ZERO,
                             MachineRepresentation::kWord32));

  FOR_UINT32_INPUTS(i) {
    uint32_t expected = i;
    CHECK_EQ(expected, r.Call(i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I32AtomicStoreLoad16U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint16_t* memory =
      r.builder().AddMemoryElems<uint16_t>(kWasmPageSize / sizeof(uint16_t));

  BUILD(
      r,
      WASM_ATOMICS_STORE_OP(kExprI32AtomicStore16U, WASM_ZERO,
                            WASM_GET_LOCAL(0), MachineRepresentation::kWord16),
      WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad16U, WASM_ZERO,
                           MachineRepresentation::kWord16));

  FOR_UINT16_INPUTS(i) {
    uint16_t expected = i;
    CHECK_EQ(expected, r.Call(i));
    CHECK_EQ(expected, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I32AtomicStoreLoad8U) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  r.builder().SetHasSharedMemory();
  uint8_t* memory = r.builder().AddMemoryElems<uint8_t>(kWasmPageSize);

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI32AtomicStore8U, WASM_ZERO,
                              WASM_GET_LOCAL(0), MachineRepresentation::kWord8),
        WASM_ATOMICS_LOAD_OP(kExprI32AtomicLoad8U, WASM_ZERO,
                             MachineRepresentation::kWord8));

  FOR_UINT8_INPUTS(i) {
    uint8_t expected = i;
    CHECK_EQ(expected, r.Call(i));
    CHECK_EQ(i, r.builder().ReadMemory(&memory[0]));
  }
}

WASM_EXEC_TEST(I32AtomicStoreParameter) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t, uint32_t> r(execution_tier);
  uint32_t* memory =
      r.builder().AddMemoryElems<uint32_t>(kWasmPageSize / sizeof(uint32_t));
  r.builder().SetHasSharedMemory();

  BUILD(r,
        WASM_ATOMICS_STORE_OP(kExprI32AtomicStore, WASM_ZERO, WASM_GET_LOCAL(0),
                              MachineRepresentation::kWord8),
        WASM_ATOMICS_BINOP(kExprI32AtomicAdd, WASM_I32V_1(0), WASM_GET_LOCAL(0),
                           MachineRepresentation::kWord32));
  CHECK_EQ(10, r.Call(10));
  CHECK_EQ(20, r.builder().ReadMemory(&memory[0]));
}

WASM_EXEC_TEST(AtomicFence) {
  EXPERIMENTAL_FLAG_SCOPE(threads);
  WasmRunner<uint32_t> r(execution_tier);
  // Note that this test specifically doesn't use a shared memory, as the fence
  // instruction does not target a particular linear memory. It may occur in
  // modules which declare no memory, or a non-shared memory, without causing a
  // validation error.

  BUILD(r, WASM_ATOMICS_FENCE, WASM_ZERO);
  CHECK_EQ(0, r.Call());
}

}  // namespace test_run_wasm_atomics
}  // namespace wasm
}  // namespace internal
}  // namespace v8
