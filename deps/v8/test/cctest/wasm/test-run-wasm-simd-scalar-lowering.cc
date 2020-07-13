// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-tier.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_simd {

#define WASM_SIMD_TEST(name)                                     \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,               \
                             ExecutionTier execution_tier);      \
  TEST(RunWasm_##name##_simd_lowered) {                          \
    EXPERIMENTAL_FLAG_SCOPE(simd);                               \
    RunWasm_##name##_Impl(kLowerSimd, ExecutionTier::kTurbofan); \
  }                                                              \
  void RunWasm_##name##_Impl(LowerSimd lower_simd, ExecutionTier execution_tier)

WASM_SIMD_TEST(I8x16ToF32x4) {
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  byte param1 = 0;
  BUILD(r,
        WASM_SET_GLOBAL(
            0, WASM_SIMD_UNOP(kExprF32x4Sqrt,
                              WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(param1)))),
        WASM_ONE);

  // Arbitrary pattern that doesn't end up creating a NaN.
  r.Call(0x5b);
  float f = bit_cast<float>(0x5b5b5b5b);
  float actual = ReadLittleEndianValue<float>(&g[0]);
  float expected = std::sqrt(f);
  CHECK_EQ(expected, actual);
}

}  // namespace test_run_wasm_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
