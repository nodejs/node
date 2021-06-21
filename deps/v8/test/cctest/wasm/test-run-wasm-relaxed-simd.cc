// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/overflowing-math.h"
#include "src/wasm/compilation-environment.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/cctest/wasm/wasm-simd-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_relaxed_simd {

// Use this for experimental relaxed-simd opcodes.
#define WASM_RELAXED_SIMD_TEST(name)                                      \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                        \
                             TestExecutionTier execution_tier);           \
  TEST(RunWasm_##name##_turbofan) {                                       \
    if (!CpuFeatures::SupportsWasmSimd128()) return;                      \
    EXPERIMENTAL_FLAG_SCOPE(relaxed_simd);                                \
    RunWasm_##name##_Impl(kNoLowerSimd, TestExecutionTier::kTurbofan);    \
  }                                                                       \
  TEST(RunWasm_##name##_interpreter) {                                    \
    EXPERIMENTAL_FLAG_SCOPE(relaxed_simd);                                \
    RunWasm_##name##_Impl(kNoLowerSimd, TestExecutionTier::kInterpreter); \
  }                                                                       \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                        \
                             TestExecutionTier execution_tier)

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X || \
    V8_TARGET_ARCH_PPC64
// Only used for qfma and qfms tests below.

// FMOperation holds the params (a, b, c) for a Multiply-Add or
// Multiply-Subtract operation, and the expected result if the operation was
// fused, rounded only once for the entire operation, or unfused, rounded after
// multiply and again after add/subtract.
template <typename T>
struct FMOperation {
  const T a;
  const T b;
  const T c;
  const T fused_result;
  const T unfused_result;
};

// large_n is large number that overflows T when multiplied by itself, this is a
// useful constant to test fused/unfused behavior.
template <typename T>
constexpr T large_n = T(0);

template <>
constexpr double large_n<double> = 1e200;

template <>
constexpr float large_n<float> = 1e20;

// Fused Multiply-Add performs a + b * c.
template <typename T>
static constexpr FMOperation<T> qfma_array[] = {
    {1.0f, 2.0f, 3.0f, 7.0f, 7.0f},
    // fused: a + b * c = -inf + (positive overflow) = -inf
    // unfused: a + b * c = -inf + inf = NaN
    {-std::numeric_limits<T>::infinity(), large_n<T>, large_n<T>,
     -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()},
    // fused: a + b * c = inf + (negative overflow) = inf
    // unfused: a + b * c = inf + -inf = NaN
    {std::numeric_limits<T>::infinity(), -large_n<T>, large_n<T>,
     std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()},
    // NaN
    {std::numeric_limits<T>::quiet_NaN(), 2.0f, 3.0f,
     std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN()},
    // -NaN
    {-std::numeric_limits<T>::quiet_NaN(), 2.0f, 3.0f,
     std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN()}};

template <typename T>
static constexpr Vector<const FMOperation<T>> qfma_vector() {
  return ArrayVector(qfma_array<T>);
}

// Fused Multiply-Subtract performs a - b * c.
template <typename T>
static constexpr FMOperation<T> qfms_array[]{
    {1.0f, 2.0f, 3.0f, -5.0f, -5.0f},
    // fused: a - b * c = inf - (positive overflow) = inf
    // unfused: a - b * c = inf - inf = NaN
    {std::numeric_limits<T>::infinity(), large_n<T>, large_n<T>,
     std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()},
    // fused: a - b * c = -inf - (negative overflow) = -inf
    // unfused: a - b * c = -inf - -inf = NaN
    {-std::numeric_limits<T>::infinity(), -large_n<T>, large_n<T>,
     -std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()},
    // NaN
    {std::numeric_limits<T>::quiet_NaN(), 2.0f, 3.0f,
     std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN()},
    // -NaN
    {-std::numeric_limits<T>::quiet_NaN(), 2.0f, 3.0f,
     std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN()}};

template <typename T>
static constexpr Vector<const FMOperation<T>> qfms_vector() {
  return ArrayVector(qfms_array<T>);
}

// Fused results only when fma3 feature is enabled, and running on TurboFan or
// Liftoff (which can fall back to TurboFan if FMA is not implemented).
bool ExpectFused(TestExecutionTier tier) {
#ifdef V8_TARGET_ARCH_X64
  return CpuFeatures::IsSupported(FMA3) &&
         (tier == TestExecutionTier::kTurbofan ||
          tier == TestExecutionTier::kLiftoff);
#else
  return (tier == TestExecutionTier::kTurbofan ||
          tier == TestExecutionTier::kLiftoff);
#endif
}
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X ||
        // V8_TARGET_ARCH_PPC64

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X || \
    V8_TARGET_ARCH_PPC64
WASM_RELAXED_SIMD_TEST(F32x4Qfma) {
  WasmRunner<int32_t, float, float, float> r(execution_tier, lower_simd);
  // Set up global to hold mask output.
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  // Build fn to splat test values, perform compare op, and write the result.
  byte value1 = 0, value2 = 1, value3 = 2;
  BUILD(r,
        WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_QFMA(
                               WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value1)),
                               WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value2)),
                               WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value3)))),
        WASM_ONE);

  for (FMOperation<float> x : qfma_vector<float>()) {
    r.Call(x.a, x.b, x.c);
    float expected =
        ExpectFused(execution_tier) ? x.fused_result : x.unfused_result;
    for (int i = 0; i < 4; i++) {
      float actual = ReadLittleEndianValue<float>(&g[i]);
      CheckFloatResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}

WASM_RELAXED_SIMD_TEST(F32x4Qfms) {
  WasmRunner<int32_t, float, float, float> r(execution_tier, lower_simd);
  // Set up global to hold mask output.
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  // Build fn to splat test values, perform compare op, and write the result.
  byte value1 = 0, value2 = 1, value3 = 2;
  BUILD(r,
        WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_QFMS(
                               WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value1)),
                               WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value2)),
                               WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value3)))),
        WASM_ONE);

  for (FMOperation<float> x : qfms_vector<float>()) {
    r.Call(x.a, x.b, x.c);
    float expected =
        ExpectFused(execution_tier) ? x.fused_result : x.unfused_result;
    for (int i = 0; i < 4; i++) {
      float actual = ReadLittleEndianValue<float>(&g[i]);
      CheckFloatResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}

WASM_RELAXED_SIMD_TEST(F64x2Qfma) {
  WasmRunner<int32_t, double, double, double> r(execution_tier, lower_simd);
  // Set up global to hold mask output.
  double* g = r.builder().AddGlobal<double>(kWasmS128);
  // Build fn to splat test values, perform compare op, and write the result.
  byte value1 = 0, value2 = 1, value3 = 2;
  BUILD(r,
        WASM_GLOBAL_SET(0, WASM_SIMD_F64x2_QFMA(
                               WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value1)),
                               WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value2)),
                               WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value3)))),
        WASM_ONE);

  for (FMOperation<double> x : qfma_vector<double>()) {
    r.Call(x.a, x.b, x.c);
    double expected =
        ExpectFused(execution_tier) ? x.fused_result : x.unfused_result;
    for (int i = 0; i < 2; i++) {
      double actual = ReadLittleEndianValue<double>(&g[i]);
      CheckDoubleResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}

WASM_RELAXED_SIMD_TEST(F64x2Qfms) {
  WasmRunner<int32_t, double, double, double> r(execution_tier, lower_simd);
  // Set up global to hold mask output.
  double* g = r.builder().AddGlobal<double>(kWasmS128);
  // Build fn to splat test values, perform compare op, and write the result.
  byte value1 = 0, value2 = 1, value3 = 2;
  BUILD(r,
        WASM_GLOBAL_SET(0, WASM_SIMD_F64x2_QFMS(
                               WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value1)),
                               WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value2)),
                               WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(value3)))),
        WASM_ONE);

  for (FMOperation<double> x : qfms_vector<double>()) {
    r.Call(x.a, x.b, x.c);
    double expected =
        ExpectFused(execution_tier) ? x.fused_result : x.unfused_result;
    for (int i = 0; i < 2; i++) {
      double actual = ReadLittleEndianValue<double>(&g[i]);
      CheckDoubleResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X ||
        // V8_TARGET_ARCH_PPC64

WASM_RELAXED_SIMD_TEST(F32x4RecipApprox) {
  RunF32x4UnOpTest(execution_tier, lower_simd, kExprF32x4RecipApprox,
                   base::Recip, false /* !exact */);
}

WASM_RELAXED_SIMD_TEST(F32x4RecipSqrtApprox) {
  RunF32x4UnOpTest(execution_tier, lower_simd, kExprF32x4RecipSqrtApprox,
                   base::RecipSqrt, false /* !exact */);
}

#undef WASM_RELAXED_SIMD_TEST
}  // namespace test_run_wasm_relaxed_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
