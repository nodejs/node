// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "src/base/overflowing-math.h"
#include "src/base/safe_conversions.h"
#include "src/common/globals.h"
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
#define WASM_RELAXED_SIMD_TEST(name)                            \
  void RunWasm_##name##_Impl(TestExecutionTier execution_tier); \
  TEST(RunWasm_##name##_turbofan) {                             \
    if (!CpuFeatures::SupportsWasmSimd128()) return;            \
    EXPERIMENTAL_FLAG_SCOPE(relaxed_simd);                      \
    RunWasm_##name##_Impl(TestExecutionTier::kTurbofan);        \
  }                                                             \
  TEST(RunWasm_##name##_interpreter) {                          \
    EXPERIMENTAL_FLAG_SCOPE(relaxed_simd);                      \
    RunWasm_##name##_Impl(TestExecutionTier::kInterpreter);     \
  }                                                             \
  void RunWasm_##name##_Impl(TestExecutionTier execution_tier)

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X || \
    V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_RISCV64
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
static constexpr base::Vector<const FMOperation<T>> qfma_vector() {
  return base::ArrayVector(qfma_array<T>);
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
static constexpr base::Vector<const FMOperation<T>> qfms_vector() {
  return base::ArrayVector(qfms_array<T>);
}

// Fused results only when fma3 feature is enabled, and running on TurboFan or
// Liftoff (which can fall back to TurboFan if FMA is not implemented).
bool ExpectFused(TestExecutionTier tier) {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  return CpuFeatures::IsSupported(FMA3) &&
         (tier == TestExecutionTier::kTurbofan ||
          tier == TestExecutionTier::kLiftoff);
#else
  return (tier == TestExecutionTier::kTurbofan ||
          tier == TestExecutionTier::kLiftoff);
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
}
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X ||
        // V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_RISCV64

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X || \
    V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_RISCV64
WASM_RELAXED_SIMD_TEST(F32x4Qfma) {
  WasmRunner<int32_t, float, float, float> r(execution_tier);
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
      float actual = LANE(g, i);
      CheckFloatResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}

WASM_RELAXED_SIMD_TEST(F32x4Qfms) {
  WasmRunner<int32_t, float, float, float> r(execution_tier);
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
      float actual = LANE(g, i);
      CheckFloatResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}

WASM_RELAXED_SIMD_TEST(F64x2Qfma) {
  WasmRunner<int32_t, double, double, double> r(execution_tier);
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
      double actual = LANE(g, i);
      CheckDoubleResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}

WASM_RELAXED_SIMD_TEST(F64x2Qfms) {
  WasmRunner<int32_t, double, double, double> r(execution_tier);
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
      double actual = LANE(g, i);
      CheckDoubleResult(x.a, x.b, expected, actual, true /* exact */);
    }
  }
}
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_S390X ||
        // V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_RISCV64

WASM_RELAXED_SIMD_TEST(F32x4RecipApprox) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4RecipApprox, base::Recip,
                   false /* !exact */);
}

WASM_RELAXED_SIMD_TEST(F32x4RecipSqrtApprox) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4RecipSqrtApprox, base::RecipSqrt,
                   false /* !exact */);
}

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM64 || \
    V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64
namespace {
// Helper to convert an array of T into an array of uint8_t to be used a v128
// constants.
template <typename T, size_t N = kSimd128Size / sizeof(T)>
std::array<uint8_t, kSimd128Size> as_uint8(const T* src) {
  std::array<uint8_t, kSimd128Size> arr;
  for (size_t i = 0; i < N; i++) {
    WriteLittleEndianValue<T>(bit_cast<T*>(&arr[0]) + i, src[i]);
  }
  return arr;
}

template <typename T, int kElems>
void RelaxedLaneSelectTest(TestExecutionTier execution_tier, const T v1[kElems],
                           const T v2[kElems], const T s[kElems],
                           const T expected[kElems], WasmOpcode laneselect) {
  auto lhs = as_uint8<T>(v1);
  auto rhs = as_uint8<T>(v2);
  auto mask = as_uint8<T>(s);
  WasmRunner<int32_t> r(execution_tier);
  T* dst = r.builder().AddGlobal<T>(kWasmS128);
  BUILD(r,
        WASM_GLOBAL_SET(0, WASM_SIMD_OPN(laneselect, WASM_SIMD_CONSTANT(lhs),
                                         WASM_SIMD_CONSTANT(rhs),
                                         WASM_SIMD_CONSTANT(mask))),
        WASM_ONE);

  CHECK_EQ(1, r.Call());
  for (int i = 0; i < kElems; i++) {
    CHECK_EQ(expected[i], LANE(dst, i));
  }
}

}  // namespace

WASM_RELAXED_SIMD_TEST(I8x16RelaxedLaneSelect) {
  constexpr int kElems = 16;
  constexpr uint8_t v1[kElems] = {0, 1, 2,  3,  4,  5,  6,  7,
                                  8, 9, 10, 11, 12, 13, 14, 15};
  constexpr uint8_t v2[kElems] = {16, 17, 18, 19, 20, 21, 22, 23,
                                  24, 25, 26, 27, 28, 29, 30, 31};
  constexpr uint8_t s[kElems] = {0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF,
                                 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF};
  constexpr uint8_t expected[kElems] = {16, 1, 18, 3,  20, 5,  22, 7,
                                        24, 9, 26, 11, 28, 13, 30, 15};
  RelaxedLaneSelectTest<uint8_t, kElems>(execution_tier, v1, v2, s, expected,
                                         kExprI8x16RelaxedLaneSelect);
}

WASM_RELAXED_SIMD_TEST(I16x8RelaxedLaneSelect) {
  constexpr int kElems = 8;
  uint16_t v1[kElems] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint16_t v2[kElems] = {8, 9, 10, 11, 12, 13, 14, 15};
  uint16_t s[kElems] = {0, 0xFFFF, 0, 0xFFFF, 0, 0xFFFF, 0, 0xFFFF};
  constexpr uint16_t expected[kElems] = {8, 1, 10, 3, 12, 5, 14, 7};
  RelaxedLaneSelectTest<uint16_t, kElems>(execution_tier, v1, v2, s, expected,
                                          kExprI16x8RelaxedLaneSelect);
}

WASM_RELAXED_SIMD_TEST(I32x4RelaxedLaneSelect) {
  constexpr int kElems = 4;
  uint32_t v1[kElems] = {0, 1, 2, 3};
  uint32_t v2[kElems] = {4, 5, 6, 7};
  uint32_t s[kElems] = {0, 0xFFFF'FFFF, 0, 0xFFFF'FFFF};
  constexpr uint32_t expected[kElems] = {4, 1, 6, 3};
  RelaxedLaneSelectTest<uint32_t, kElems>(execution_tier, v1, v2, s, expected,
                                          kExprI32x4RelaxedLaneSelect);
}

WASM_RELAXED_SIMD_TEST(I64x2RelaxedLaneSelect) {
  constexpr int kElems = 2;
  uint64_t v1[kElems] = {0, 1};
  uint64_t v2[kElems] = {2, 3};
  uint64_t s[kElems] = {0, 0xFFFF'FFFF'FFFF'FFFF};
  constexpr uint64_t expected[kElems] = {2, 1};
  RelaxedLaneSelectTest<uint64_t, kElems>(execution_tier, v1, v2, s, expected,
                                          kExprI64x2RelaxedLaneSelect);
}

WASM_RELAXED_SIMD_TEST(F32x4RelaxedMin) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4RelaxedMin, Minimum);
}

WASM_RELAXED_SIMD_TEST(F32x4RelaxedMax) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4RelaxedMax, Maximum);
}

WASM_RELAXED_SIMD_TEST(F64x2RelaxedMin) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2RelaxedMin, Minimum);
}

WASM_RELAXED_SIMD_TEST(F64x2RelaxedMax) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2RelaxedMax, Maximum);
}

namespace {
// For relaxed trunc instructions, don't test out of range values.
// FloatType comes later so caller can rely on template argument deduction and
// just pass IntType.
template <typename IntType, typename FloatType>
typename std::enable_if<std::is_floating_point<FloatType>::value, bool>::type
ShouldSkipTestingConstant(FloatType x) {
  return std::isnan(x) || !base::IsValueInRangeForNumericType<IntType>(x) ||
         !PlatformCanRepresent(x);
}

template <typename IntType, typename FloatType>
void IntRelaxedTruncFloatTest(TestExecutionTier execution_tier,
                              WasmOpcode trunc_op, WasmOpcode splat_op) {
  WasmRunner<int, FloatType> r(execution_tier);
  IntType* g0 = r.builder().template AddGlobal<IntType>(kWasmS128);
  constexpr int lanes = kSimd128Size / sizeof(FloatType);

  // global[0] = trunc(splat(local[0])).
  BUILD(r,
        WASM_GLOBAL_SET(
            0, WASM_SIMD_UNOP(trunc_op,
                              WASM_SIMD_UNOP(splat_op, WASM_LOCAL_GET(0)))),
        WASM_ONE);

  for (FloatType x : compiler::ValueHelper::GetVector<FloatType>()) {
    if (ShouldSkipTestingConstant<IntType>(x)) continue;
    CHECK_EQ(1, r.Call(x));
    IntType expected = base::checked_cast<IntType>(x);
    for (int i = 0; i < lanes; i++) {
      CHECK_EQ(expected, LANE(g0, i));
    }
  }
}
}  // namespace

WASM_RELAXED_SIMD_TEST(I32x4RelaxedTruncF64x2SZero) {
  IntRelaxedTruncFloatTest<int32_t, double>(
      execution_tier, kExprI32x4RelaxedTruncF64x2SZero, kExprF64x2Splat);
}

WASM_RELAXED_SIMD_TEST(I32x4RelaxedTruncF64x2UZero) {
  IntRelaxedTruncFloatTest<uint32_t, double>(
      execution_tier, kExprI32x4RelaxedTruncF64x2UZero, kExprF64x2Splat);
}

WASM_RELAXED_SIMD_TEST(I32x4RelaxedTruncF32x4S) {
  IntRelaxedTruncFloatTest<int32_t, float>(
      execution_tier, kExprI32x4RelaxedTruncF32x4S, kExprF32x4Splat);
}

WASM_RELAXED_SIMD_TEST(I32x4RelaxedTruncF32x4U) {
  IntRelaxedTruncFloatTest<uint32_t, float>(
      execution_tier, kExprI32x4RelaxedTruncF32x4U, kExprF32x4Splat);
}
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM64 ||
        // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM64 || \
    V8_TARGET_ARCH_RISCV64
WASM_RELAXED_SIMD_TEST(I8x16RelaxedSwizzle) {
  // Output is only defined for indices in the range [0,15].
  WasmRunner<int32_t> r(execution_tier);
  static const int kElems = kSimd128Size / sizeof(uint8_t);
  uint8_t* dst = r.builder().AddGlobal<uint8_t>(kWasmS128);
  uint8_t* src = r.builder().AddGlobal<uint8_t>(kWasmS128);
  uint8_t* indices = r.builder().AddGlobal<uint8_t>(kWasmS128);
  BUILD(r,
        WASM_GLOBAL_SET(
            0, WASM_SIMD_BINOP(kExprI8x16RelaxedSwizzle, WASM_GLOBAL_GET(1),
                               WASM_GLOBAL_GET(2))),
        WASM_ONE);
  for (int i = 0; i < kElems; i++) {
    LANE(src, i) = kElems - i - 1;
    LANE(indices, i) = kElems - i - 1;
  }
  CHECK_EQ(1, r.Call());
  for (int i = 0; i < kElems; i++) {
    CHECK_EQ(LANE(dst, i), i);
  }
}
#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM64 ||
        // V8_TARGET_ARCH_RISCV64

#undef WASM_RELAXED_SIMD_TEST
}  // namespace test_run_wasm_relaxed_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
