// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "src/api/api-inl.h"
// #include "test/cctest/wasm/wasm-atomics-utils.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_liftoff_for_fuzzing {

TEST(MaxSteps) {
  WasmRunner<uint32_t> r(TestExecutionTier::kLiftoffForFuzzing);

  BUILD(r, WASM_LOOP(WASM_BR(0)), WASM_I32V(23));
  r.SetMaxSteps(10);
  r.CheckCallViaJSTraps();
}

TEST(NondeterminismUnopF32) {
  WasmRunner<float> r(TestExecutionTier::kLiftoffForFuzzing);

  BUILD(r, WASM_F32_ABS(WASM_F32(std::nanf(""))));
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(std::nanf(""));
  CHECK(r.HasNondeterminism());
}

TEST(NondeterminismUnopF64) {
  WasmRunner<double> r(TestExecutionTier::kLiftoffForFuzzing);

  BUILD(r, WASM_F64_ABS(WASM_F64(std::nan(""))));
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(std::nan(""));
  CHECK(r.HasNondeterminism());
}

TEST(NondeterminismUnopF32x4) {
  WasmRunner<int32_t, float> r(TestExecutionTier::kLiftoffForFuzzing);

  byte value = 0;
  BUILD(r,
        WASM_SIMD_UNOP(kExprF32x4Ceil,
                       WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value))),
        kExprDrop, WASM_ONE);
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(1, 0.0);
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(1, std::nanf(""));
  CHECK(r.HasNondeterminism());
}

TEST(NondeterminismUnopF64x2) {
  WasmRunner<int32_t, double> r(TestExecutionTier::kLiftoffForFuzzing);

  byte value = 0;
  BUILD(r,
        WASM_SIMD_UNOP(kExprF64x2Ceil,
                       WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value))),
        kExprDrop, WASM_ONE);
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(1, 0.0);
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(1, std::nan(""));
  CHECK(r.HasNondeterminism());
}

TEST(NondeterminismBinop) {
  WasmRunner<float> r(TestExecutionTier::kLiftoffForFuzzing);

  BUILD(r, WASM_F32_ADD(WASM_F32(std::nanf("")), WASM_F32(0)));
  CHECK(!r.HasNondeterminism());
  r.CheckCallViaJS(std::nanf(""));
  CHECK(r.HasNondeterminism());
}

}  // namespace test_liftoff_for_fuzzing
}  // namespace wasm
}  // namespace internal
}  // namespace v8
