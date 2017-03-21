// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/platform/elapsed-timer.h"

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

// for even shorter tests.
#define B2(a, b) kExprBlock, a, b, kExprEnd
#define B1(a) kExprBlock, a, kExprEnd
#define RET(x) x, kExprReturn, 1
#define RET_I8(x) kExprI8Const, x, kExprReturn, 1

namespace {
uint32_t GetMatchingRelocInfoCount(Handle<Code> code, RelocInfo::Mode rmode) {
  int filter = 1 << rmode;
  uint32_t ret = 0;
  for (RelocIterator it(*code, filter); !it.done(); it.next()) {
    ++ret;
  }
  return ret;
}
}

WASM_EXEC_TEST(Int32AsmjsDivS) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_BINOP(kExprI32AsmjsDivS, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(kMin, 0));
}

WASM_EXEC_TEST(Int32AsmjsRemS) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_BINOP(kExprI32AsmjsRemS, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(33, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
}

WASM_EXEC_TEST(Int32AsmjsDivU) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_BINOP(kExprI32AsmjsDivU, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
}

WASM_EXEC_TEST(Int32AsmjsRemU) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_BINOP(kExprI32AsmjsRemU, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(17, r.Call(217, 100));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
}

WASM_EXEC_TEST(I32AsmjsSConvertF32) {
  WasmRunner<int32_t, float> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_UNOP(kExprI32AsmjsSConvertF32, WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    int32_t expected = DoubleToInt32(*i);
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(I32AsmjsSConvertF64) {
  WasmRunner<int32_t, double> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_UNOP(kExprI32AsmjsSConvertF64, WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    int32_t expected = DoubleToInt32(*i);
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(I32AsmjsUConvertF32) {
  WasmRunner<uint32_t, float> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_UNOP(kExprI32AsmjsUConvertF32, WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    uint32_t expected = DoubleToUint32(*i);
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(I32AsmjsUConvertF64) {
  WasmRunner<uint32_t, double> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_UNOP(kExprI32AsmjsUConvertF64, WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    uint32_t expected = DoubleToUint32(*i);
    CHECK_EQ(expected, r.Call(*i));
  }
}

WASM_EXEC_TEST(LoadMemI32_oob_asm) {
  WasmRunner<int32_t, uint32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);
  r.module().RandomizeMemory(1112);

  BUILD(r, WASM_UNOP(kExprI32AsmjsLoadMem, WASM_GET_LOCAL(0)));

  memory[0] = 999999;
  CHECK_EQ(999999, r.Call(0u));
  // TODO(titzer): offset 29-31 should also be OOB.
  for (uint32_t offset = 32; offset < 40; offset++) {
    CHECK_EQ(0, r.Call(offset));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK_EQ(0, r.Call(offset));
  }
}

WASM_EXEC_TEST(LoadMemF32_oob_asm) {
  WasmRunner<float, uint32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  float* memory = r.module().AddMemoryElems<float>(8);
  r.module().RandomizeMemory(1112);

  BUILD(r, WASM_UNOP(kExprF32AsmjsLoadMem, WASM_GET_LOCAL(0)));

  memory[0] = 9999.5f;
  CHECK_EQ(9999.5f, r.Call(0u));
  // TODO(titzer): offset 29-31 should also be OOB.
  for (uint32_t offset = 32; offset < 40; offset++) {
    CHECK(std::isnan(r.Call(offset)));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK(std::isnan(r.Call(offset)));
  }
}

WASM_EXEC_TEST(LoadMemF64_oob_asm) {
  WasmRunner<double, uint32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  double* memory = r.module().AddMemoryElems<double>(8);
  r.module().RandomizeMemory(1112);

  BUILD(r, WASM_UNOP(kExprF64AsmjsLoadMem, WASM_GET_LOCAL(0)));

  memory[0] = 9799.5;
  CHECK_EQ(9799.5, r.Call(0u));
  memory[1] = 11799.25;
  CHECK_EQ(11799.25, r.Call(8u));
  // TODO(titzer): offset 57-63 should also be OOB.
  for (uint32_t offset = 64; offset < 80; offset++) {
    CHECK(std::isnan(r.Call(offset)));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK(std::isnan(r.Call(offset)));
  }
}

WASM_EXEC_TEST(StoreMemI32_oob_asm) {
  WasmRunner<int32_t, uint32_t, uint32_t> r(execution_mode);
  r.module().ChangeOriginToAsmjs();
  int32_t* memory = r.module().AddMemoryElems<int32_t>(8);
  r.module().RandomizeMemory(1112);

  BUILD(r, WASM_BINOP(kExprI32AsmjsStoreMem, WASM_GET_LOCAL(0),
                      WASM_GET_LOCAL(1)));

  memory[0] = 7777;
  CHECK_EQ(999999, r.Call(0u, 999999));
  CHECK_EQ(999999, memory[0]);
  // TODO(titzer): offset 29-31 should also be OOB.
  for (uint32_t offset = 32; offset < 40; offset++) {
    CHECK_EQ(8888, r.Call(offset, 8888));
  }

  for (uint32_t offset = 0x10000000; offset < 0xF0000000; offset += 0x1000000) {
    CHECK_EQ(7777, r.Call(offset, 7777));
  }
}

#define FOREACH_INT_CHECKED_LOAD_OP(TEST_BODY) \
  TEST_BODY(kExprI32AsmjsLoadMem8S)            \
  TEST_BODY(kExprI32AsmjsLoadMem8U)            \
  TEST_BODY(kExprI32AsmjsLoadMem16S)           \
  TEST_BODY(kExprI32AsmjsLoadMem16U)           \
  TEST_BODY(kExprI32AsmjsLoadMem)

#define FOREACH_INT_CHECKED_STORE_OP(TEST_BODY) \
  TEST_BODY(kExprI32AsmjsStoreMem8)             \
  TEST_BODY(kExprI32AsmjsStoreMem16)            \
  TEST_BODY(kExprI32AsmjsStoreMem)

#define INT_LOAD_TEST(OP_TYPE)                                                \
  TEST(RunWasm_AsmCheckedRelocInfo##OP_TYPE) {                                \
    WasmRunner<int32_t, uint32_t> r(kExecuteCompiled);                        \
    r.module().ChangeOriginToAsmjs();                                         \
    BUILD(r, WASM_UNOP(OP_TYPE, WASM_GET_LOCAL(0)));                          \
    CHECK_EQ(1,                                                               \
             GetMatchingRelocInfoCount(r.module().instance->function_code[0], \
                                       RelocInfo::WASM_MEMORY_REFERENCE));    \
    CHECK_NE(                                                                 \
        0, GetMatchingRelocInfoCount(r.module().instance->function_code[0],   \
                                     RelocInfo::WASM_MEMORY_SIZE_REFERENCE)); \
  }

FOREACH_INT_CHECKED_LOAD_OP(INT_LOAD_TEST)

#define INT_STORE_TEST(OP_TYPE)                                               \
  TEST(RunWasm_AsmCheckedRelocInfo##OP_TYPE) {                                \
    WasmRunner<int32_t, uint32_t, uint32_t> r(kExecuteCompiled);              \
    r.module().ChangeOriginToAsmjs();                                         \
    BUILD(r, WASM_BINOP(OP_TYPE, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));      \
    CHECK_EQ(1,                                                               \
             GetMatchingRelocInfoCount(r.module().instance->function_code[0], \
                                       RelocInfo::WASM_MEMORY_REFERENCE));    \
    CHECK_NE(                                                                 \
        0, GetMatchingRelocInfoCount(r.module().instance->function_code[0],   \
                                     RelocInfo::WASM_MEMORY_SIZE_REFERENCE)); \
  }

FOREACH_INT_CHECKED_STORE_OP(INT_STORE_TEST)

TEST(RunWasm_AsmCheckedLoadFloat32RelocInfo) {
  WasmRunner<float, uint32_t> r(kExecuteCompiled);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_UNOP(kExprF32AsmjsLoadMem, WASM_GET_LOCAL(0)));

  CHECK_EQ(1, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_REFERENCE));
  CHECK_NE(0, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
}

TEST(RunWasm_AsmCheckedStoreFloat32RelocInfo) {
  WasmRunner<float, uint32_t, float> r(kExecuteCompiled);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_BINOP(kExprF32AsmjsStoreMem, WASM_GET_LOCAL(0),
                      WASM_GET_LOCAL(1)));

  CHECK_EQ(1, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_REFERENCE));
  CHECK_NE(0, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
}

TEST(RunWasm_AsmCheckedLoadFloat64RelocInfo) {
  WasmRunner<double, uint32_t> r(kExecuteCompiled);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_UNOP(kExprF64AsmjsLoadMem, WASM_GET_LOCAL(0)));

  CHECK_EQ(1, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_REFERENCE));
  CHECK_NE(0, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
}

TEST(RunWasm_AsmCheckedStoreFloat64RelocInfo) {
  WasmRunner<double, uint32_t, double> r(kExecuteCompiled);
  r.module().ChangeOriginToAsmjs();
  BUILD(r, WASM_BINOP(kExprF64AsmjsStoreMem, WASM_GET_LOCAL(0),
                      WASM_GET_LOCAL(1)));

  CHECK_EQ(1, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_REFERENCE));
  CHECK_NE(0, GetMatchingRelocInfoCount(r.module().instance->function_code[0],
                                        RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
}
