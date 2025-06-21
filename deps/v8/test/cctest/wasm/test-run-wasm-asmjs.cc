// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/platform/elapsed-timer.h"
#include "src/codegen/assembler-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/value-helper.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

TEST(RunAsmJs_Int32AsmjsDivS) {
  WasmRunner<int32_t, int32_t, int32_t> r(TestExecutionTier::kTurbofan,
                                          kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_DIVS(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(kMin, 0));
}

TEST(RunAsmJs_Int32AsmjsRemS) {
  WasmRunner<int32_t, int32_t, int32_t> r(TestExecutionTier::kTurbofan,
                                          kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_REMS(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(33, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
}

TEST(RunAsmJs_Int32AsmjsDivU) {
  WasmRunner<int32_t, int32_t, int32_t> r(TestExecutionTier::kTurbofan,
                                          kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_DIVU(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
}

TEST(RunAsmJs_Int32AsmjsRemU) {
  WasmRunner<int32_t, int32_t, int32_t> r(TestExecutionTier::kTurbofan,
                                          kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_REMU(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(17, r.Call(217, 100));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
}

TEST(RunAsmJs_I32AsmjsSConvertF32) {
  WasmRunner<int32_t, float> r(TestExecutionTier::kTurbofan,
                               kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_SCONVERTF32(WASM_LOCAL_GET(0))});

  FOR_FLOAT32_INPUTS(i) {
    int32_t expected = DoubleToInt32(i);
    CHECK_EQ(expected, r.Call(i));
  }
}

TEST(RunAsmJs_I32AsmjsSConvertF64) {
  WasmRunner<int32_t, double> r(TestExecutionTier::kTurbofan,
                                kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_SCONVERTF64(WASM_LOCAL_GET(0))});

  FOR_FLOAT64_INPUTS(i) {
    int32_t expected = DoubleToInt32(i);
    CHECK_EQ(expected, r.Call(i));
  }
}

TEST(RunAsmJs_I32AsmjsUConvertF32) {
  WasmRunner<uint32_t, float> r(TestExecutionTier::kTurbofan,
                                kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_UCONVERTF32(WASM_LOCAL_GET(0))});

  FOR_FLOAT32_INPUTS(i) {
    uint32_t expected = DoubleToUint32(i);
    CHECK_EQ(expected, r.Call(i));
  }
}

TEST(RunAsmJs_I32AsmjsUConvertF64) {
  WasmRunner<uint32_t, double> r(TestExecutionTier::kTurbofan,
                                 kAsmJsSloppyOrigin);
  r.Build({WASM_I32_ASMJS_UCONVERTF64(WASM_LOCAL_GET(0))});

  FOR_FLOAT64_INPUTS(i) {
    uint32_t expected = DoubleToUint32(i);
    CHECK_EQ(expected, r.Call(i));
  }
}

TEST(RunAsmJs_LoadMemI32_oob_asm) {
  WasmRunner<int32_t, uint32_t> r(TestExecutionTier::kTurbofan,
                                  kAsmJsSloppyOrigin);
  int32_t* memory = r.builder().AddMemoryElems<int32_t>(8);
  r.builder().RandomizeMemory(1112);

  r.Build({WASM_I32_ASMJS_LOADMEM(WASM_LOCAL_GET(0))});

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

TEST(RunAsmJs_LoadMemF32_oob_asm) {
  WasmRunner<float, uint32_t> r(TestExecutionTier::kTurbofan,
                                kAsmJsSloppyOrigin);
  float* memory = r.builder().AddMemoryElems<float>(8);
  r.builder().RandomizeMemory(1112);

  r.Build({WASM_F32_ASMJS_LOADMEM(WASM_LOCAL_GET(0))});

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

TEST(RunAsmJs_LoadMemF64_oob_asm) {
  WasmRunner<double, uint32_t> r(TestExecutionTier::kTurbofan,
                                 kAsmJsSloppyOrigin);
  double* memory = r.builder().AddMemoryElems<double>(8);
  r.builder().RandomizeMemory(1112);

  r.Build({WASM_F64_ASMJS_LOADMEM(WASM_LOCAL_GET(0))});

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

TEST(RunAsmJs_StoreMemI32_oob_asm) {
  WasmRunner<int32_t, uint32_t, uint32_t> r(TestExecutionTier::kTurbofan,
                                            kAsmJsSloppyOrigin);
  int32_t* memory = r.builder().AddMemoryElems<int32_t>(8);
  r.builder().RandomizeMemory(1112);

  r.Build({WASM_I32_ASMJS_STOREMEM(WASM_LOCAL_GET(0), WASM_LOCAL_GET(1))});

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

TEST(RunAsmJs_Int32AsmjsDivS_byzero_const) {
  for (int8_t denom = -2; denom < 8; ++denom) {
    WasmRunner<int32_t, int32_t> r(TestExecutionTier::kTurbofan,
                                   kAsmJsSloppyOrigin);
    r.Build({WASM_I32_ASMJS_DIVS(WASM_LOCAL_GET(0), WASM_I32V_1(denom))});
    FOR_INT32_INPUTS(i) {
      if (denom == 0) {
        CHECK_EQ(0, r.Call(i));
      } else if (denom == -1 && i == std::numeric_limits<int32_t>::min()) {
        CHECK_EQ(std::numeric_limits<int32_t>::min(), r.Call(i));
      } else {
        CHECK_EQ(i / denom, r.Call(i));
      }
    }
  }
}

TEST(RunAsmJs_Int32AsmjsRemS_byzero_const) {
  for (int8_t denom = -2; denom < 8; ++denom) {
    WasmRunner<int32_t, int32_t> r(TestExecutionTier::kTurbofan,
                                   kAsmJsSloppyOrigin);
    r.Build({WASM_I32_ASMJS_REMS(WASM_LOCAL_GET(0), WASM_I32V_1(denom))});
    FOR_INT32_INPUTS(i) {
      if (denom == 0) {
        CHECK_EQ(0, r.Call(i));
      } else if (denom == -1 && i == std::numeric_limits<int32_t>::min()) {
        CHECK_EQ(0, r.Call(i));
      } else {
        CHECK_EQ(i % denom, r.Call(i));
      }
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
