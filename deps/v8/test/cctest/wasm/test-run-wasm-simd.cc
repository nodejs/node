// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/memory.h"
#include "src/base/overflowing-math.h"
#include "src/base/safe_conversions.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/cctest/wasm/wasm-simd-utils.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_simd {

namespace {

using Shuffle = std::array<int8_t, kSimd128Size>;

#define WASM_SIMD_TEST(name)                                    \
  void RunWasm_##name##_Impl(TestExecutionTier execution_tier); \
  TEST(RunWasm_##name##_turbofan) {                             \
    EXPERIMENTAL_FLAG_SCOPE(simd);                              \
    RunWasm_##name##_Impl(TestExecutionTier::kTurbofan);        \
  }                                                             \
  TEST(RunWasm_##name##_liftoff) {                              \
    EXPERIMENTAL_FLAG_SCOPE(simd);                              \
    RunWasm_##name##_Impl(TestExecutionTier::kLiftoff);         \
  }                                                             \
  TEST(RunWasm_##name##_interpreter) {                          \
    EXPERIMENTAL_FLAG_SCOPE(simd);                              \
    RunWasm_##name##_Impl(TestExecutionTier::kInterpreter);     \
  }                                                             \
  void RunWasm_##name##_Impl(TestExecutionTier execution_tier)

// For signed integral types, use base::AddWithWraparound.
template <typename T, typename = typename std::enable_if<
                          std::is_floating_point<T>::value>::type>
T Add(T a, T b) {
  return a + b;
}

// For signed integral types, use base::SubWithWraparound.
template <typename T, typename = typename std::enable_if<
                          std::is_floating_point<T>::value>::type>
T Sub(T a, T b) {
  return a - b;
}

// For signed integral types, use base::MulWithWraparound.
template <typename T, typename = typename std::enable_if<
                          std::is_floating_point<T>::value>::type>
T Mul(T a, T b) {
  return a * b;
}

template <typename T>
T Minimum(T a, T b) {
  return std::min(a, b);
}

template <typename T>
T Maximum(T a, T b) {
  return std::max(a, b);
}

template <typename T>
T UnsignedMinimum(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) <= static_cast<UnsignedT>(b) ? a : b;
}

template <typename T>
T UnsignedMaximum(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) >= static_cast<UnsignedT>(b) ? a : b;
}

template <typename T, typename U = T>
U Equal(T a, T b) {
  return a == b ? -1 : 0;
}

template <>
int32_t Equal(float a, float b) {
  return a == b ? -1 : 0;
}

template <>
int64_t Equal(double a, double b) {
  return a == b ? -1 : 0;
}

template <typename T, typename U = T>
U NotEqual(T a, T b) {
  return a != b ? -1 : 0;
}

template <>
int32_t NotEqual(float a, float b) {
  return a != b ? -1 : 0;
}

template <>
int64_t NotEqual(double a, double b) {
  return a != b ? -1 : 0;
}

template <typename T, typename U = T>
U Less(T a, T b) {
  return a < b ? -1 : 0;
}

template <>
int32_t Less(float a, float b) {
  return a < b ? -1 : 0;
}

template <>
int64_t Less(double a, double b) {
  return a < b ? -1 : 0;
}

template <typename T, typename U = T>
U LessEqual(T a, T b) {
  return a <= b ? -1 : 0;
}

template <>
int32_t LessEqual(float a, float b) {
  return a <= b ? -1 : 0;
}

template <>
int64_t LessEqual(double a, double b) {
  return a <= b ? -1 : 0;
}

template <typename T, typename U = T>
U Greater(T a, T b) {
  return a > b ? -1 : 0;
}

template <>
int32_t Greater(float a, float b) {
  return a > b ? -1 : 0;
}

template <>
int64_t Greater(double a, double b) {
  return a > b ? -1 : 0;
}

template <typename T, typename U = T>
U GreaterEqual(T a, T b) {
  return a >= b ? -1 : 0;
}

template <>
int32_t GreaterEqual(float a, float b) {
  return a >= b ? -1 : 0;
}

template <>
int64_t GreaterEqual(double a, double b) {
  return a >= b ? -1 : 0;
}

template <typename T>
T UnsignedLess(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) < static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
T UnsignedLessEqual(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) <= static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
T UnsignedGreater(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) > static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
T UnsignedGreaterEqual(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) >= static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
T LogicalShiftLeft(T a, int shift) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) << (shift % (sizeof(T) * 8));
}

template <typename T>
T LogicalShiftRight(T a, int shift) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) >> (shift % (sizeof(T) * 8));
}

// Define our own ArithmeticShiftRight instead of using the one from utils.h
// because the shift amount needs to be taken modulo lane width.
template <typename T>
T ArithmeticShiftRight(T a, int shift) {
  return a >> (shift % (sizeof(T) * 8));
}

template <typename T>
T Abs(T a) {
  return std::abs(a);
}
}  // namespace

#define WASM_SIMD_CHECK_LANE_S(TYPE, value, LANE_TYPE, lane_value, lane_index) \
  WASM_IF(WASM_##LANE_TYPE##_NE(WASM_LOCAL_GET(lane_value),                    \
                                WASM_SIMD_##TYPE##_EXTRACT_LANE(               \
                                    lane_index, WASM_LOCAL_GET(value))),       \
          WASM_RETURN1(WASM_ZERO))

// Unsigned Extracts are only available for I8x16, I16x8 types
#define WASM_SIMD_CHECK_LANE_U(TYPE, value, LANE_TYPE, lane_value, lane_index) \
  WASM_IF(WASM_##LANE_TYPE##_NE(WASM_LOCAL_GET(lane_value),                    \
                                WASM_SIMD_##TYPE##_EXTRACT_LANE_U(             \
                                    lane_index, WASM_LOCAL_GET(value))),       \
          WASM_RETURN1(WASM_ZERO))

WASM_SIMD_TEST(S128Globals) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold input and output vectors.
  int32_t* g0 = r.builder().AddGlobal<int32_t>(kWasmS128);
  int32_t* g1 = r.builder().AddGlobal<int32_t>(kWasmS128);
  BUILD(r, WASM_GLOBAL_SET(1, WASM_GLOBAL_GET(0)), WASM_ONE);

  FOR_INT32_INPUTS(x) {
    for (int i = 0; i < 4; i++) {
      WriteLittleEndianValue<int32_t>(&g0[i], x);
    }
    r.Call();
    int32_t expected = x;
    for (int i = 0; i < 4; i++) {
      int32_t actual = ReadLittleEndianValue<int32_t>(&g1[i]);
      CHECK_EQ(actual, expected);
    }
  }
}

WASM_SIMD_TEST(F32x4Splat) {
  WasmRunner<int32_t, float> r(execution_tier);
  // Set up a global to hold output vector.
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  byte param1 = 0;
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(param1))),
        WASM_ONE);

  FOR_FLOAT32_INPUTS(x) {
    r.Call(x);
    float expected = x;
    for (int i = 0; i < 4; i++) {
      float actual = ReadLittleEndianValue<float>(&g[i]);
      if (std::isnan(expected)) {
        CHECK(std::isnan(actual));
      } else {
        CHECK_EQ(actual, expected);
      }
    }
  }
}

WASM_SIMD_TEST(F32x4ReplaceLane) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold input/output vector.
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  // Build function to replace each lane with its (FP) index.
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_F32x4_SPLAT(WASM_F32(3.14159f))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_F32x4_REPLACE_LANE(
                                  0, WASM_LOCAL_GET(temp1), WASM_F32(0.0f))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_F32x4_REPLACE_LANE(
                                  1, WASM_LOCAL_GET(temp1), WASM_F32(1.0f))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_F32x4_REPLACE_LANE(
                                  2, WASM_LOCAL_GET(temp1), WASM_F32(2.0f))),
        WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_REPLACE_LANE(
                               3, WASM_LOCAL_GET(temp1), WASM_F32(3.0f))),
        WASM_ONE);

  r.Call();
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(static_cast<float>(i), ReadLittleEndianValue<float>(&g[i]));
  }
}

// Tests both signed and unsigned conversion.
WASM_SIMD_TEST(F32x4ConvertI32x4) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create two output vectors to hold signed and unsigned results.
  float* g0 = r.builder().AddGlobal<float>(kWasmS128);
  float* g1 = r.builder().AddGlobal<float>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(
            0, WASM_SIMD_UNOP(kExprF32x4SConvertI32x4, WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(
            1, WASM_SIMD_UNOP(kExprF32x4UConvertI32x4, WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_INT32_INPUTS(x) {
    r.Call(x);
    float expected_signed = static_cast<float>(x);
    float expected_unsigned = static_cast<float>(static_cast<uint32_t>(x));
    for (int i = 0; i < 4; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<float>(&g0[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<float>(&g1[i]));
    }
  }
}

WASM_SIMD_TEST(F32x4Abs) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4Abs, std::abs);
}

WASM_SIMD_TEST(F32x4Neg) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4Neg, Negate);
}

WASM_SIMD_TEST(F32x4Sqrt) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4Sqrt, std::sqrt);
}

WASM_SIMD_TEST(F32x4Ceil) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4Ceil, ceilf, true);
}

WASM_SIMD_TEST(F32x4Floor) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4Floor, floorf, true);
}

WASM_SIMD_TEST(F32x4Trunc) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4Trunc, truncf, true);
}

WASM_SIMD_TEST(F32x4NearestInt) {
  RunF32x4UnOpTest(execution_tier, kExprF32x4NearestInt, nearbyintf, true);
}

WASM_SIMD_TEST(F32x4Add) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Add, Add);
}
WASM_SIMD_TEST(F32x4Sub) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Sub, Sub);
}
WASM_SIMD_TEST(F32x4Mul) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Mul, Mul);
}
WASM_SIMD_TEST(F32x4Div) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Div, base::Divide);
}
WASM_SIMD_TEST(F32x4Min) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Min, JSMin);
}
WASM_SIMD_TEST(F32x4Max) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Max, JSMax);
}

WASM_SIMD_TEST(F32x4Pmin) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Pmin, Minimum);
}

WASM_SIMD_TEST(F32x4Pmax) {
  RunF32x4BinOpTest(execution_tier, kExprF32x4Pmax, Maximum);
}

WASM_SIMD_TEST(F32x4Eq) {
  RunF32x4CompareOpTest(execution_tier, kExprF32x4Eq, Equal);
}

WASM_SIMD_TEST(F32x4Ne) {
  RunF32x4CompareOpTest(execution_tier, kExprF32x4Ne, NotEqual);
}

WASM_SIMD_TEST(F32x4Gt) {
  RunF32x4CompareOpTest(execution_tier, kExprF32x4Gt, Greater);
}

WASM_SIMD_TEST(F32x4Ge) {
  RunF32x4CompareOpTest(execution_tier, kExprF32x4Ge, GreaterEqual);
}

WASM_SIMD_TEST(F32x4Lt) {
  RunF32x4CompareOpTest(execution_tier, kExprF32x4Lt, Less);
}

WASM_SIMD_TEST(F32x4Le) {
  RunF32x4CompareOpTest(execution_tier, kExprF32x4Le, LessEqual);
}

WASM_SIMD_TEST(I64x2Splat) {
  WasmRunner<int32_t, int64_t> r(execution_tier);
  // Set up a global to hold output vector.
  int64_t* g = r.builder().AddGlobal<int64_t>(kWasmS128);
  byte param1 = 0;
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_I64x2_SPLAT(WASM_LOCAL_GET(param1))),
        WASM_ONE);

  FOR_INT64_INPUTS(x) {
    r.Call(x);
    int64_t expected = x;
    for (int i = 0; i < 2; i++) {
      int64_t actual = ReadLittleEndianValue<int64_t>(&g[i]);
      CHECK_EQ(actual, expected);
    }
  }
}

WASM_SIMD_TEST(I64x2ExtractLane) {
  WasmRunner<int64_t> r(execution_tier);
  r.AllocateLocal(kWasmI64);
  r.AllocateLocal(kWasmS128);
  BUILD(
      r,
      WASM_LOCAL_SET(0, WASM_SIMD_I64x2_EXTRACT_LANE(
                            0, WASM_SIMD_I64x2_SPLAT(WASM_I64V(0xFFFFFFFFFF)))),
      WASM_LOCAL_SET(1, WASM_SIMD_I64x2_SPLAT(WASM_LOCAL_GET(0))),
      WASM_SIMD_I64x2_EXTRACT_LANE(1, WASM_LOCAL_GET(1)));
  CHECK_EQ(0xFFFFFFFFFF, r.Call());
}

WASM_SIMD_TEST(I64x2ReplaceLane) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold input/output vector.
  int64_t* g = r.builder().AddGlobal<int64_t>(kWasmS128);
  // Build function to replace each lane with its index.
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I64x2_SPLAT(WASM_I64V(-1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I64x2_REPLACE_LANE(
                                  0, WASM_LOCAL_GET(temp1), WASM_I64V(0))),
        WASM_GLOBAL_SET(0, WASM_SIMD_I64x2_REPLACE_LANE(
                               1, WASM_LOCAL_GET(temp1), WASM_I64V(1))),
        WASM_ONE);

  r.Call();
  for (int64_t i = 0; i < 2; i++) {
    CHECK_EQ(i, ReadLittleEndianValue<int64_t>(&g[i]));
  }
}

WASM_SIMD_TEST(I64x2Neg) {
  RunI64x2UnOpTest(execution_tier, kExprI64x2Neg, base::NegateWithWraparound);
}

WASM_SIMD_TEST(I64x2Abs) {
  RunI64x2UnOpTest(execution_tier, kExprI64x2Abs, std::abs);
}

WASM_SIMD_TEST(I64x2Shl) {
  RunI64x2ShiftOpTest(execution_tier, kExprI64x2Shl, LogicalShiftLeft);
}

WASM_SIMD_TEST(I64x2ShrS) {
  RunI64x2ShiftOpTest(execution_tier, kExprI64x2ShrS, ArithmeticShiftRight);
}

WASM_SIMD_TEST(I64x2ShrU) {
  RunI64x2ShiftOpTest(execution_tier, kExprI64x2ShrU, LogicalShiftRight);
}

WASM_SIMD_TEST(I64x2Add) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2Add, base::AddWithWraparound);
}

WASM_SIMD_TEST(I64x2Sub) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2Sub, base::SubWithWraparound);
}

WASM_SIMD_TEST(I64x2Eq) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2Eq, Equal);
}

WASM_SIMD_TEST(I64x2Ne) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2Ne, NotEqual);
}

WASM_SIMD_TEST(I64x2LtS) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2LtS, Less);
}

WASM_SIMD_TEST(I64x2LeS) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2LeS, LessEqual);
}

WASM_SIMD_TEST(I64x2GtS) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2GtS, Greater);
}

WASM_SIMD_TEST(I64x2GeS) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2GeS, GreaterEqual);
}

WASM_SIMD_TEST(F64x2Splat) {
  WasmRunner<int32_t, double> r(execution_tier);
  // Set up a global to hold output vector.
  double* g = r.builder().AddGlobal<double>(kWasmS128);
  byte param1 = 0;
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(param1))),
        WASM_ONE);

  FOR_FLOAT64_INPUTS(x) {
    r.Call(x);
    double expected = x;
    for (int i = 0; i < 2; i++) {
      double actual = ReadLittleEndianValue<double>(&g[i]);
      if (std::isnan(expected)) {
        CHECK(std::isnan(actual));
      } else {
        CHECK_EQ(actual, expected);
      }
    }
  }
}

WASM_SIMD_TEST(F64x2ExtractLane) {
  WasmRunner<double, double> r(execution_tier);
  byte param1 = 0;
  byte temp1 = r.AllocateLocal(kWasmF64);
  byte temp2 = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_LOCAL_SET(temp1,
                       WASM_SIMD_F64x2_EXTRACT_LANE(
                           0, WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(param1)))),
        WASM_LOCAL_SET(temp2, WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(temp1))),
        WASM_SIMD_F64x2_EXTRACT_LANE(1, WASM_LOCAL_GET(temp2)));
  FOR_FLOAT64_INPUTS(x) {
    double actual = r.Call(x);
    double expected = x;
    if (std::isnan(expected)) {
      CHECK(std::isnan(actual));
    } else {
      CHECK_EQ(actual, expected);
    }
  }
}

WASM_SIMD_TEST(F64x2ReplaceLane) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up globals to hold input/output vector.
  double* g0 = r.builder().AddGlobal<double>(kWasmS128);
  double* g1 = r.builder().AddGlobal<double>(kWasmS128);
  // Build function to replace each lane with its (FP) index.
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_F64x2_SPLAT(WASM_F64(1e100))),
        // Replace lane 0.
        WASM_GLOBAL_SET(0, WASM_SIMD_F64x2_REPLACE_LANE(
                               0, WASM_LOCAL_GET(temp1), WASM_F64(0.0f))),
        // Replace lane 1.
        WASM_GLOBAL_SET(1, WASM_SIMD_F64x2_REPLACE_LANE(
                               1, WASM_LOCAL_GET(temp1), WASM_F64(1.0f))),
        WASM_ONE);

  r.Call();
  CHECK_EQ(0., ReadLittleEndianValue<double>(&g0[0]));
  CHECK_EQ(1e100, ReadLittleEndianValue<double>(&g0[1]));
  CHECK_EQ(1e100, ReadLittleEndianValue<double>(&g1[0]));
  CHECK_EQ(1., ReadLittleEndianValue<double>(&g1[1]));
}

WASM_SIMD_TEST(F64x2ExtractLaneWithI64x2) {
  WasmRunner<int64_t> r(execution_tier);
  BUILD(r, WASM_IF_ELSE_L(
               WASM_F64_EQ(WASM_SIMD_F64x2_EXTRACT_LANE(
                               0, WASM_SIMD_I64x2_SPLAT(WASM_I64V(1e15))),
                           WASM_F64_REINTERPRET_I64(WASM_I64V(1e15))),
               WASM_I64V(1), WASM_I64V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(I64x2ExtractWithF64x2) {
  WasmRunner<int64_t> r(execution_tier);
  BUILD(r, WASM_IF_ELSE_L(
               WASM_I64_EQ(WASM_SIMD_I64x2_EXTRACT_LANE(
                               0, WASM_SIMD_F64x2_SPLAT(WASM_F64(1e15))),
                           WASM_I64_REINTERPRET_F64(WASM_F64(1e15))),
               WASM_I64V(1), WASM_I64V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(F64x2Abs) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2Abs, std::abs);
}

WASM_SIMD_TEST(F64x2Neg) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2Neg, Negate);
}

WASM_SIMD_TEST(F64x2Sqrt) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2Sqrt, std::sqrt);
}

WASM_SIMD_TEST(F64x2Ceil) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2Ceil, ceil, true);
}

WASM_SIMD_TEST(F64x2Floor) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2Floor, floor, true);
}

WASM_SIMD_TEST(F64x2Trunc) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2Trunc, trunc, true);
}

WASM_SIMD_TEST(F64x2NearestInt) {
  RunF64x2UnOpTest(execution_tier, kExprF64x2NearestInt, nearbyint, true);
}

template <typename SrcType>
void RunF64x2ConvertLowI32x4Test(TestExecutionTier execution_tier,
                                 WasmOpcode opcode) {
  WasmRunner<int32_t, SrcType> r(execution_tier);
  double* g = r.builder().template AddGlobal<double>(kWasmS128);
  BUILD(r,
        WASM_GLOBAL_SET(
            0,
            WASM_SIMD_UNOP(
                opcode,
                // Set top lane of i64x2 == set top 2 lanes of i32x4.
                WASM_SIMD_I64x2_REPLACE_LANE(
                    1, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(0)), WASM_ZERO64))),
        WASM_ONE);

  for (SrcType x : compiler::ValueHelper::GetVector<SrcType>()) {
    r.Call(x);
    double expected = static_cast<double>(x);
    for (int i = 0; i < 2; i++) {
      double actual = ReadLittleEndianValue<double>(&g[i]);
      CheckDoubleResult(x, x, expected, actual, true);
    }
  }
}

WASM_SIMD_TEST(F64x2ConvertLowI32x4S) {
  RunF64x2ConvertLowI32x4Test<int32_t>(execution_tier,
                                       kExprF64x2ConvertLowI32x4S);
}

WASM_SIMD_TEST(F64x2ConvertLowI32x4U) {
  RunF64x2ConvertLowI32x4Test<uint32_t>(execution_tier,
                                        kExprF64x2ConvertLowI32x4U);
}

template <typename SrcType>
void RunI32x4TruncSatF64x2Test(TestExecutionTier execution_tier,
                               WasmOpcode opcode) {
  WasmRunner<int32_t, double> r(execution_tier);
  SrcType* g = r.builder().AddGlobal<SrcType>(kWasmS128);
  BUILD(
      r,
      WASM_GLOBAL_SET(
          0, WASM_SIMD_UNOP(opcode, WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(0)))),
      WASM_ONE);

  FOR_FLOAT64_INPUTS(x) {
    r.Call(x);
    SrcType expected = base::saturated_cast<SrcType>(x);
    for (int i = 0; i < 2; i++) {
      SrcType actual = ReadLittleEndianValue<SrcType>(&g[i]);
      CHECK_EQ(expected, actual);
    }
    // Top lanes are zero-ed.
    for (int i = 2; i < 4; i++) {
      CHECK_EQ(0, ReadLittleEndianValue<SrcType>(&g[i]));
    }
  }
}

WASM_SIMD_TEST(I32x4TruncSatF64x2SZero) {
  RunI32x4TruncSatF64x2Test<int32_t>(execution_tier,
                                     kExprI32x4TruncSatF64x2SZero);
}

WASM_SIMD_TEST(I32x4TruncSatF64x2UZero) {
  RunI32x4TruncSatF64x2Test<uint32_t>(execution_tier,
                                      kExprI32x4TruncSatF64x2UZero);
}

WASM_SIMD_TEST(F32x4DemoteF64x2Zero) {
  WasmRunner<int32_t, double> r(execution_tier);
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  BUILD(r,
        WASM_GLOBAL_SET(
            0, WASM_SIMD_UNOP(kExprF32x4DemoteF64x2Zero,
                              WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(0)))),
        WASM_ONE);

  FOR_FLOAT64_INPUTS(x) {
    r.Call(x);
    float expected = DoubleToFloat32(x);
    for (int i = 0; i < 2; i++) {
      float actual = ReadLittleEndianValue<float>(&g[i]);
      CheckFloatResult(x, x, expected, actual, true);
    }
    for (int i = 2; i < 4; i++) {
      float actual = ReadLittleEndianValue<float>(&g[i]);
      CheckFloatResult(x, x, 0, actual, true);
    }
  }
}

WASM_SIMD_TEST(F64x2PromoteLowF32x4) {
  WasmRunner<int32_t, float> r(execution_tier);
  double* g = r.builder().AddGlobal<double>(kWasmS128);
  BUILD(r,
        WASM_GLOBAL_SET(
            0, WASM_SIMD_UNOP(kExprF64x2PromoteLowF32x4,
                              WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(0)))),
        WASM_ONE);

  FOR_FLOAT32_INPUTS(x) {
    r.Call(x);
    double expected = static_cast<double>(x);
    for (int i = 0; i < 2; i++) {
      double actual = ReadLittleEndianValue<double>(&g[i]);
      CheckDoubleResult(x, x, expected, actual, true);
    }
  }
}

WASM_SIMD_TEST(F64x2Add) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Add, Add);
}

WASM_SIMD_TEST(F64x2Sub) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Sub, Sub);
}

WASM_SIMD_TEST(F64x2Mul) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Mul, Mul);
}

WASM_SIMD_TEST(F64x2Div) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Div, base::Divide);
}

WASM_SIMD_TEST(F64x2Pmin) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Pmin, Minimum);
}

WASM_SIMD_TEST(F64x2Pmax) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Pmax, Maximum);
}

WASM_SIMD_TEST(F64x2Eq) {
  RunF64x2CompareOpTest(execution_tier, kExprF64x2Eq, Equal);
}

WASM_SIMD_TEST(F64x2Ne) {
  RunF64x2CompareOpTest(execution_tier, kExprF64x2Ne, NotEqual);
}

WASM_SIMD_TEST(F64x2Gt) {
  RunF64x2CompareOpTest(execution_tier, kExprF64x2Gt, Greater);
}

WASM_SIMD_TEST(F64x2Ge) {
  RunF64x2CompareOpTest(execution_tier, kExprF64x2Ge, GreaterEqual);
}

WASM_SIMD_TEST(F64x2Lt) {
  RunF64x2CompareOpTest(execution_tier, kExprF64x2Lt, Less);
}

WASM_SIMD_TEST(F64x2Le) {
  RunF64x2CompareOpTest(execution_tier, kExprF64x2Le, LessEqual);
}

WASM_SIMD_TEST(F64x2Min) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Min, JSMin);
}

WASM_SIMD_TEST(F64x2Max) {
  RunF64x2BinOpTest(execution_tier, kExprF64x2Max, JSMax);
}

WASM_SIMD_TEST(I64x2Mul) {
  RunI64x2BinOpTest(execution_tier, kExprI64x2Mul, base::MulWithWraparound);
}

WASM_SIMD_TEST(I32x4Splat) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Set up a global to hold output vector.
  int32_t* g = r.builder().AddGlobal<int32_t>(kWasmS128);
  byte param1 = 0;
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(param1))),
        WASM_ONE);

  FOR_INT32_INPUTS(x) {
    r.Call(x);
    int32_t expected = x;
    for (int i = 0; i < 4; i++) {
      int32_t actual = ReadLittleEndianValue<int32_t>(&g[i]);
      CHECK_EQ(actual, expected);
    }
  }
}

WASM_SIMD_TEST(I32x4ReplaceLane) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold input/output vector.
  int32_t* g = r.builder().AddGlobal<int32_t>(kWasmS128);
  // Build function to replace each lane with its index.
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_SPLAT(WASM_I32V(-1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_REPLACE_LANE(
                                  0, WASM_LOCAL_GET(temp1), WASM_I32V(0))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_REPLACE_LANE(
                                  1, WASM_LOCAL_GET(temp1), WASM_I32V(1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_REPLACE_LANE(
                                  2, WASM_LOCAL_GET(temp1), WASM_I32V(2))),
        WASM_GLOBAL_SET(0, WASM_SIMD_I32x4_REPLACE_LANE(
                               3, WASM_LOCAL_GET(temp1), WASM_I32V(3))),
        WASM_ONE);

  r.Call();
  for (int32_t i = 0; i < 4; i++) {
    CHECK_EQ(i, ReadLittleEndianValue<int32_t>(&g[i]));
  }
}

WASM_SIMD_TEST(I16x8Splat) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Set up a global to hold output vector.
  int16_t* g = r.builder().AddGlobal<int16_t>(kWasmS128);
  byte param1 = 0;
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(param1))),
        WASM_ONE);

  FOR_INT16_INPUTS(x) {
    r.Call(x);
    int16_t expected = x;
    for (int i = 0; i < 8; i++) {
      int16_t actual = ReadLittleEndianValue<int16_t>(&g[i]);
      CHECK_EQ(actual, expected);
    }
  }

  // Test values that do not fit in a int16.
  FOR_INT32_INPUTS(x) {
    r.Call(x);
    int16_t expected = truncate_to_int16(x);
    for (int i = 0; i < 8; i++) {
      int16_t actual = ReadLittleEndianValue<int16_t>(&g[i]);
      CHECK_EQ(actual, expected);
    }
  }
}

WASM_SIMD_TEST(I16x8ReplaceLane) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold input/output vector.
  int16_t* g = r.builder().AddGlobal<int16_t>(kWasmS128);
  // Build function to replace each lane with its index.
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_SPLAT(WASM_I32V(-1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  0, WASM_LOCAL_GET(temp1), WASM_I32V(0))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  1, WASM_LOCAL_GET(temp1), WASM_I32V(1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  2, WASM_LOCAL_GET(temp1), WASM_I32V(2))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  3, WASM_LOCAL_GET(temp1), WASM_I32V(3))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  4, WASM_LOCAL_GET(temp1), WASM_I32V(4))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  5, WASM_LOCAL_GET(temp1), WASM_I32V(5))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_REPLACE_LANE(
                                  6, WASM_LOCAL_GET(temp1), WASM_I32V(6))),
        WASM_GLOBAL_SET(0, WASM_SIMD_I16x8_REPLACE_LANE(
                               7, WASM_LOCAL_GET(temp1), WASM_I32V(7))),
        WASM_ONE);

  r.Call();
  for (int16_t i = 0; i < 8; i++) {
    CHECK_EQ(i, ReadLittleEndianValue<int16_t>(&g[i]));
  }
}

WASM_SIMD_TEST(I8x16BitMask) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  byte value1 = r.AllocateLocal(kWasmS128);

  BUILD(r, WASM_LOCAL_SET(value1, WASM_SIMD_I8x16_SPLAT(WASM_LOCAL_GET(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I8x16_REPLACE_LANE(
                                   0, WASM_LOCAL_GET(value1), WASM_I32V(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I8x16_REPLACE_LANE(
                                   1, WASM_LOCAL_GET(value1), WASM_I32V(-1))),
        WASM_SIMD_UNOP(kExprI8x16BitMask, WASM_LOCAL_GET(value1)));

  FOR_INT8_INPUTS(x) {
    int32_t actual = r.Call(x);
    // Lane 0 is always 0 (positive), lane 1 is always -1.
    int32_t expected = std::signbit(static_cast<double>(x)) ? 0xFFFE : 0x0002;
    CHECK_EQ(actual, expected);
  }
}

WASM_SIMD_TEST(I16x8BitMask) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  byte value1 = r.AllocateLocal(kWasmS128);

  BUILD(r, WASM_LOCAL_SET(value1, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I16x8_REPLACE_LANE(
                                   0, WASM_LOCAL_GET(value1), WASM_I32V(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I16x8_REPLACE_LANE(
                                   1, WASM_LOCAL_GET(value1), WASM_I32V(-1))),
        WASM_SIMD_UNOP(kExprI16x8BitMask, WASM_LOCAL_GET(value1)));

  FOR_INT16_INPUTS(x) {
    int32_t actual = r.Call(x);
    // Lane 0 is always 0 (positive), lane 1 is always -1.
    int32_t expected = std::signbit(static_cast<double>(x)) ? 0xFE : 2;
    CHECK_EQ(actual, expected);
  }
}

WASM_SIMD_TEST(I32x4BitMask) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  byte value1 = r.AllocateLocal(kWasmS128);

  BUILD(r, WASM_LOCAL_SET(value1, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I32x4_REPLACE_LANE(
                                   0, WASM_LOCAL_GET(value1), WASM_I32V(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I32x4_REPLACE_LANE(
                                   1, WASM_LOCAL_GET(value1), WASM_I32V(-1))),
        WASM_SIMD_UNOP(kExprI32x4BitMask, WASM_LOCAL_GET(value1)));

  FOR_INT32_INPUTS(x) {
    int32_t actual = r.Call(x);
    // Lane 0 is always 0 (positive), lane 1 is always -1.
    int32_t expected = std::signbit(static_cast<double>(x)) ? 0xE : 2;
    CHECK_EQ(actual, expected);
  }
}

WASM_SIMD_TEST(I64x2BitMask) {
  WasmRunner<int32_t, int64_t> r(execution_tier);
  byte value1 = r.AllocateLocal(kWasmS128);

  BUILD(r, WASM_LOCAL_SET(value1, WASM_SIMD_I64x2_SPLAT(WASM_LOCAL_GET(0))),
        WASM_LOCAL_SET(value1, WASM_SIMD_I64x2_REPLACE_LANE(
                                   0, WASM_LOCAL_GET(value1), WASM_I64V_1(0))),
        WASM_SIMD_UNOP(kExprI64x2BitMask, WASM_LOCAL_GET(value1)));

  for (int64_t x : compiler::ValueHelper::GetVector<int64_t>()) {
    int32_t actual = r.Call(x);
    // Lane 0 is always 0 (positive).
    int32_t expected = std::signbit(static_cast<double>(x)) ? 0x2 : 0x0;
    CHECK_EQ(actual, expected);
  }
}

WASM_SIMD_TEST(I8x16Splat) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Set up a global to hold output vector.
  int8_t* g = r.builder().AddGlobal<int8_t>(kWasmS128);
  byte param1 = 0;
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_I8x16_SPLAT(WASM_LOCAL_GET(param1))),
        WASM_ONE);

  FOR_INT8_INPUTS(x) {
    r.Call(x);
    int8_t expected = x;
    for (int i = 0; i < 16; i++) {
      int8_t actual = ReadLittleEndianValue<int8_t>(&g[i]);
      CHECK_EQ(actual, expected);
    }
  }

  // Test values that do not fit in a int16.
  FOR_INT16_INPUTS(x) {
    r.Call(x);
    int8_t expected = truncate_to_int8(x);
    for (int i = 0; i < 16; i++) {
      int8_t actual = ReadLittleEndianValue<int8_t>(&g[i]);
      CHECK_EQ(actual, expected);
    }
  }
}

WASM_SIMD_TEST(I8x16ReplaceLane) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold input/output vector.
  int8_t* g = r.builder().AddGlobal<int8_t>(kWasmS128);
  // Build function to replace each lane with its index.
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_SPLAT(WASM_I32V(-1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  0, WASM_LOCAL_GET(temp1), WASM_I32V(0))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  1, WASM_LOCAL_GET(temp1), WASM_I32V(1))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  2, WASM_LOCAL_GET(temp1), WASM_I32V(2))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  3, WASM_LOCAL_GET(temp1), WASM_I32V(3))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  4, WASM_LOCAL_GET(temp1), WASM_I32V(4))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  5, WASM_LOCAL_GET(temp1), WASM_I32V(5))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  6, WASM_LOCAL_GET(temp1), WASM_I32V(6))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  7, WASM_LOCAL_GET(temp1), WASM_I32V(7))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  8, WASM_LOCAL_GET(temp1), WASM_I32V(8))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  9, WASM_LOCAL_GET(temp1), WASM_I32V(9))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  10, WASM_LOCAL_GET(temp1), WASM_I32V(10))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  11, WASM_LOCAL_GET(temp1), WASM_I32V(11))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  12, WASM_LOCAL_GET(temp1), WASM_I32V(12))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  13, WASM_LOCAL_GET(temp1), WASM_I32V(13))),
        WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_REPLACE_LANE(
                                  14, WASM_LOCAL_GET(temp1), WASM_I32V(14))),
        WASM_GLOBAL_SET(0, WASM_SIMD_I8x16_REPLACE_LANE(
                               15, WASM_LOCAL_GET(temp1), WASM_I32V(15))),
        WASM_ONE);

  r.Call();
  for (int8_t i = 0; i < 16; i++) {
    CHECK_EQ(i, ReadLittleEndianValue<int8_t>(&g[i]));
  }
}

// Use doubles to ensure exact conversion.
int32_t ConvertToInt(double val, bool unsigned_integer) {
  if (std::isnan(val)) return 0;
  if (unsigned_integer) {
    if (val < 0) return 0;
    if (val > kMaxUInt32) return kMaxUInt32;
    return static_cast<uint32_t>(val);
  } else {
    if (val < kMinInt) return kMinInt;
    if (val > kMaxInt) return kMaxInt;
    return static_cast<int>(val);
  }
}

// Tests both signed and unsigned conversion.
WASM_SIMD_TEST(I32x4ConvertF32x4) {
  WasmRunner<int32_t, float> r(execution_tier);
  // Create two output vectors to hold signed and unsigned results.
  int32_t* g0 = r.builder().AddGlobal<int32_t>(kWasmS128);
  int32_t* g1 = r.builder().AddGlobal<int32_t>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(
            0, WASM_SIMD_UNOP(kExprI32x4SConvertF32x4, WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(
            1, WASM_SIMD_UNOP(kExprI32x4UConvertF32x4, WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_FLOAT32_INPUTS(x) {
    if (!PlatformCanRepresent(x)) continue;
    r.Call(x);
    int32_t expected_signed = ConvertToInt(x, false);
    int32_t expected_unsigned = ConvertToInt(x, true);
    for (int i = 0; i < 4; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int32_t>(&g0[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<int32_t>(&g1[i]));
    }
  }
}

// Tests both signed and unsigned conversion from I16x8 (unpacking).
WASM_SIMD_TEST(I32x4ConvertI16x8) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create four output vectors to hold signed and unsigned results.
  int32_t* g0 = r.builder().AddGlobal<int32_t>(kWasmS128);
  int32_t* g1 = r.builder().AddGlobal<int32_t>(kWasmS128);
  int32_t* g2 = r.builder().AddGlobal<int32_t>(kWasmS128);
  int32_t* g3 = r.builder().AddGlobal<int32_t>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(0, WASM_SIMD_UNOP(kExprI32x4SConvertI16x8High,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(1, WASM_SIMD_UNOP(kExprI32x4SConvertI16x8Low,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(2, WASM_SIMD_UNOP(kExprI32x4UConvertI16x8High,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(3, WASM_SIMD_UNOP(kExprI32x4UConvertI16x8Low,
                                          WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_INT16_INPUTS(x) {
    r.Call(x);
    int32_t expected_signed = static_cast<int32_t>(x);
    int32_t expected_unsigned = static_cast<int32_t>(static_cast<uint16_t>(x));
    for (int i = 0; i < 4; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int32_t>(&g0[i]));
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int32_t>(&g1[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<int32_t>(&g2[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<int32_t>(&g3[i]));
    }
  }
}

// Tests both signed and unsigned conversion from I32x4 (unpacking).
WASM_SIMD_TEST(I64x2ConvertI32x4) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create four output vectors to hold signed and unsigned results.
  int64_t* g0 = r.builder().AddGlobal<int64_t>(kWasmS128);
  int64_t* g1 = r.builder().AddGlobal<int64_t>(kWasmS128);
  uint64_t* g2 = r.builder().AddGlobal<uint64_t>(kWasmS128);
  uint64_t* g3 = r.builder().AddGlobal<uint64_t>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(0, WASM_SIMD_UNOP(kExprI64x2SConvertI32x4High,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(1, WASM_SIMD_UNOP(kExprI64x2SConvertI32x4Low,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(2, WASM_SIMD_UNOP(kExprI64x2UConvertI32x4High,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(3, WASM_SIMD_UNOP(kExprI64x2UConvertI32x4Low,
                                          WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_INT32_INPUTS(x) {
    r.Call(x);
    int64_t expected_signed = static_cast<int64_t>(x);
    uint64_t expected_unsigned =
        static_cast<uint64_t>(static_cast<uint32_t>(x));
    for (int i = 0; i < 2; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int64_t>(&g0[i]));
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int64_t>(&g1[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<uint64_t>(&g2[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<uint64_t>(&g3[i]));
    }
  }
}

WASM_SIMD_TEST(I32x4Neg) {
  RunI32x4UnOpTest(execution_tier, kExprI32x4Neg, base::NegateWithWraparound);
}

WASM_SIMD_TEST(I32x4Abs) {
  RunI32x4UnOpTest(execution_tier, kExprI32x4Abs, std::abs);
}

WASM_SIMD_TEST(S128Not) {
  RunI32x4UnOpTest(execution_tier, kExprS128Not, [](int32_t x) { return ~x; });
}

template <typename Narrow, typename Wide>
void RunExtAddPairwiseTest(TestExecutionTier execution_tier,
                           WasmOpcode ext_add_pairwise, WasmOpcode splat,
                           Shuffle interleaving_shuffle) {
  constexpr int num_lanes = kSimd128Size / sizeof(Wide);
  WasmRunner<int32_t, Narrow, Narrow> r(execution_tier);
  Wide* g = r.builder().template AddGlobal<Wide>(kWasmS128);

  BUILD(r,
        WASM_SIMD_I8x16_SHUFFLE_OP(kExprI8x16Shuffle, interleaving_shuffle,
                                   WASM_SIMD_UNOP(splat, WASM_LOCAL_GET(0)),
                                   WASM_SIMD_UNOP(splat, WASM_LOCAL_GET(1))),
        WASM_SIMD_OP(ext_add_pairwise), kExprGlobalSet, 0, WASM_ONE);

  auto v = compiler::ValueHelper::GetVector<Narrow>();
  // Iterate vector from both ends to try and splat two different values.
  for (auto i = v.begin(), j = v.end() - 1; i < v.end(); i++, j--) {
    r.Call(*i, *j);
    Wide expected = AddLong<Wide>(*i, *j);
    for (int i = 0; i < num_lanes; i++) {
      CHECK_EQ(expected, ReadLittleEndianValue<Wide>(&g[i]));
    }
  }
}

// interleave even lanes from one input and odd lanes from another.
constexpr Shuffle interleave_16x8_shuffle = {0, 1, 18, 19, 4,  5,  22, 23,
                                             8, 9, 26, 27, 12, 13, 30, 31};
constexpr Shuffle interleave_8x16_shuffle = {0, 17, 2,  19, 4,  21, 6,  23,
                                             8, 25, 10, 27, 12, 29, 14, 31};

WASM_SIMD_TEST(I32x4ExtAddPairwiseI16x8S) {
  RunExtAddPairwiseTest<int16_t, int32_t>(
      execution_tier, kExprI32x4ExtAddPairwiseI16x8S, kExprI16x8Splat,
      interleave_16x8_shuffle);
}

WASM_SIMD_TEST(I32x4ExtAddPairwiseI16x8U) {
  RunExtAddPairwiseTest<uint16_t, uint32_t>(
      execution_tier, kExprI32x4ExtAddPairwiseI16x8U, kExprI16x8Splat,
      interleave_16x8_shuffle);
}

WASM_SIMD_TEST(I16x8ExtAddPairwiseI8x16S) {
  RunExtAddPairwiseTest<int8_t, int16_t>(
      execution_tier, kExprI16x8ExtAddPairwiseI8x16S, kExprI8x16Splat,
      interleave_8x16_shuffle);
}

WASM_SIMD_TEST(I16x8ExtAddPairwiseI8x16U) {
  RunExtAddPairwiseTest<uint8_t, uint16_t>(
      execution_tier, kExprI16x8ExtAddPairwiseI8x16U, kExprI8x16Splat,
      interleave_8x16_shuffle);
}

WASM_SIMD_TEST(I32x4Add) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4Add, base::AddWithWraparound);
}

WASM_SIMD_TEST(I32x4Sub) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4Sub, base::SubWithWraparound);
}

WASM_SIMD_TEST(I32x4Mul) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4Mul, base::MulWithWraparound);
}

WASM_SIMD_TEST(I32x4MinS) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4MinS, Minimum);
}

WASM_SIMD_TEST(I32x4MaxS) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4MaxS, Maximum);
}

WASM_SIMD_TEST(I32x4MinU) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4MinU, UnsignedMinimum);
}
WASM_SIMD_TEST(I32x4MaxU) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4MaxU,

                    UnsignedMaximum);
}

WASM_SIMD_TEST(S128And) {
  RunI32x4BinOpTest(execution_tier, kExprS128And,
                    [](int32_t x, int32_t y) { return x & y; });
}

WASM_SIMD_TEST(S128Or) {
  RunI32x4BinOpTest(execution_tier, kExprS128Or,
                    [](int32_t x, int32_t y) { return x | y; });
}

WASM_SIMD_TEST(S128Xor) {
  RunI32x4BinOpTest(execution_tier, kExprS128Xor,
                    [](int32_t x, int32_t y) { return x ^ y; });
}

// Bitwise operation, doesn't really matter what simd type we test it with.
WASM_SIMD_TEST(S128AndNot) {
  RunI32x4BinOpTest(execution_tier, kExprS128AndNot,
                    [](int32_t x, int32_t y) { return x & ~y; });
}

WASM_SIMD_TEST(I32x4Eq) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4Eq, Equal);
}

WASM_SIMD_TEST(I32x4Ne) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4Ne, NotEqual);
}

WASM_SIMD_TEST(I32x4LtS) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4LtS, Less);
}

WASM_SIMD_TEST(I32x4LeS) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4LeS, LessEqual);
}

WASM_SIMD_TEST(I32x4GtS) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4GtS, Greater);
}

WASM_SIMD_TEST(I32x4GeS) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4GeS, GreaterEqual);
}

WASM_SIMD_TEST(I32x4LtU) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4LtU, UnsignedLess);
}

WASM_SIMD_TEST(I32x4LeU) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4LeU, UnsignedLessEqual);
}

WASM_SIMD_TEST(I32x4GtU) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I32x4GeU) {
  RunI32x4BinOpTest(execution_tier, kExprI32x4GeU, UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I32x4Shl) {
  RunI32x4ShiftOpTest(execution_tier, kExprI32x4Shl, LogicalShiftLeft);
}

WASM_SIMD_TEST(I32x4ShrS) {
  RunI32x4ShiftOpTest(execution_tier, kExprI32x4ShrS, ArithmeticShiftRight);
}

WASM_SIMD_TEST(I32x4ShrU) {
  RunI32x4ShiftOpTest(execution_tier, kExprI32x4ShrU, LogicalShiftRight);
}

// Tests both signed and unsigned conversion from I8x16 (unpacking).
WASM_SIMD_TEST(I16x8ConvertI8x16) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create four output vectors to hold signed and unsigned results.
  int16_t* g0 = r.builder().AddGlobal<int16_t>(kWasmS128);
  int16_t* g1 = r.builder().AddGlobal<int16_t>(kWasmS128);
  int16_t* g2 = r.builder().AddGlobal<int16_t>(kWasmS128);
  int16_t* g3 = r.builder().AddGlobal<int16_t>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(0, WASM_SIMD_UNOP(kExprI16x8SConvertI8x16High,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(1, WASM_SIMD_UNOP(kExprI16x8SConvertI8x16Low,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(2, WASM_SIMD_UNOP(kExprI16x8UConvertI8x16High,
                                          WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(3, WASM_SIMD_UNOP(kExprI16x8UConvertI8x16Low,
                                          WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_INT8_INPUTS(x) {
    r.Call(x);
    int16_t expected_signed = static_cast<int16_t>(x);
    int16_t expected_unsigned = static_cast<int16_t>(static_cast<uint8_t>(x));
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int16_t>(&g0[i]));
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int16_t>(&g1[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<int16_t>(&g2[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<int16_t>(&g3[i]));
    }
  }
}

// Tests both signed and unsigned conversion from I32x4 (packing).
WASM_SIMD_TEST(I16x8ConvertI32x4) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create output vectors to hold signed and unsigned results.
  int16_t* g0 = r.builder().AddGlobal<int16_t>(kWasmS128);
  int16_t* g1 = r.builder().AddGlobal<int16_t>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(
            0, WASM_SIMD_BINOP(kExprI16x8SConvertI32x4, WASM_LOCAL_GET(temp1),
                               WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(
            1, WASM_SIMD_BINOP(kExprI16x8UConvertI32x4, WASM_LOCAL_GET(temp1),
                               WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_INT32_INPUTS(x) {
    r.Call(x);
    int16_t expected_signed = base::saturated_cast<int16_t>(x);
    int16_t expected_unsigned = base::saturated_cast<uint16_t>(x);
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int16_t>(&g0[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<int16_t>(&g1[i]));
    }
  }
}

WASM_SIMD_TEST(I16x8Neg) {
  RunI16x8UnOpTest(execution_tier, kExprI16x8Neg, base::NegateWithWraparound);
}

WASM_SIMD_TEST(I16x8Abs) {
  RunI16x8UnOpTest(execution_tier, kExprI16x8Abs, Abs);
}

WASM_SIMD_TEST(I16x8Add) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8Add, base::AddWithWraparound);
}

WASM_SIMD_TEST(I16x8AddSatS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8AddSatS, SaturateAdd<int16_t>);
}

WASM_SIMD_TEST(I16x8Sub) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8Sub, base::SubWithWraparound);
}

WASM_SIMD_TEST(I16x8SubSatS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8SubSatS, SaturateSub<int16_t>);
}

WASM_SIMD_TEST(I16x8Mul) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8Mul, base::MulWithWraparound);
}

WASM_SIMD_TEST(I16x8MinS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8MinS, Minimum);
}

WASM_SIMD_TEST(I16x8MaxS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8MaxS, Maximum);
}

WASM_SIMD_TEST(I16x8AddSatU) {
  RunI16x8BinOpTest<uint16_t>(execution_tier, kExprI16x8AddSatU,
                              SaturateAdd<uint16_t>);
}

WASM_SIMD_TEST(I16x8SubSatU) {
  RunI16x8BinOpTest<uint16_t>(execution_tier, kExprI16x8SubSatU,
                              SaturateSub<uint16_t>);
}

WASM_SIMD_TEST(I16x8MinU) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8MinU, UnsignedMinimum);
}

WASM_SIMD_TEST(I16x8MaxU) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8MaxU, UnsignedMaximum);
}

WASM_SIMD_TEST(I16x8Eq) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8Eq, Equal);
}

WASM_SIMD_TEST(I16x8Ne) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8Ne, NotEqual);
}

WASM_SIMD_TEST(I16x8LtS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8LtS, Less);
}

WASM_SIMD_TEST(I16x8LeS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8LeS, LessEqual);
}

WASM_SIMD_TEST(I16x8GtS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8GtS, Greater);
}

WASM_SIMD_TEST(I16x8GeS) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8GeS, GreaterEqual);
}

WASM_SIMD_TEST(I16x8GtU) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I16x8GeU) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8GeU, UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I16x8LtU) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8LtU, UnsignedLess);
}

WASM_SIMD_TEST(I16x8LeU) {
  RunI16x8BinOpTest(execution_tier, kExprI16x8LeU, UnsignedLessEqual);
}

WASM_SIMD_TEST(I16x8RoundingAverageU) {
  RunI16x8BinOpTest<uint16_t>(execution_tier, kExprI16x8RoundingAverageU,
                              RoundingAverageUnsigned);
}

WASM_SIMD_TEST(I16x8Q15MulRSatS) {
  RunI16x8BinOpTest<int16_t>(execution_tier, kExprI16x8Q15MulRSatS,
                             SaturateRoundingQMul<int16_t>);
}

namespace {
enum class MulHalf { kLow, kHigh };

// Helper to run ext mul tests. It will splat 2 input values into 2 v128, call
// the mul op on these operands, and set the result into a global.
// It will zero the top or bottom half of one of the operands, this will catch
// mistakes if we are multiply the incorrect halves.
template <typename S, typename T, typename OpType = T (*)(S, S)>
void RunExtMulTest(TestExecutionTier execution_tier, WasmOpcode opcode,
                   OpType expected_op, WasmOpcode splat, MulHalf half) {
  WasmRunner<int32_t, S, S> r(execution_tier);
  int lane_to_zero = half == MulHalf::kLow ? 1 : 0;
  T* g = r.builder().template AddGlobal<T>(kWasmS128);

  BUILD(r,
        WASM_GLOBAL_SET(
            0, WASM_SIMD_BINOP(
                   opcode,
                   WASM_SIMD_I64x2_REPLACE_LANE(
                       lane_to_zero, WASM_SIMD_UNOP(splat, WASM_LOCAL_GET(0)),
                       WASM_I64V_1(0)),
                   WASM_SIMD_UNOP(splat, WASM_LOCAL_GET(1)))),
        WASM_ONE);

  constexpr int lanes = kSimd128Size / sizeof(T);
  for (S x : compiler::ValueHelper::GetVector<S>()) {
    for (S y : compiler::ValueHelper::GetVector<S>()) {
      r.Call(x, y);
      T expected = expected_op(x, y);
      for (int i = 0; i < lanes; i++) {
        CHECK_EQ(expected, ReadLittleEndianValue<T>(&g[i]));
      }
    }
  }
}
}  // namespace

WASM_SIMD_TEST(I16x8ExtMulLowI8x16S) {
  RunExtMulTest<int8_t, int16_t>(execution_tier, kExprI16x8ExtMulLowI8x16S,
                                 MultiplyLong, kExprI8x16Splat, MulHalf::kLow);
}

WASM_SIMD_TEST(I16x8ExtMulHighI8x16S) {
  RunExtMulTest<int8_t, int16_t>(execution_tier, kExprI16x8ExtMulHighI8x16S,
                                 MultiplyLong, kExprI8x16Splat, MulHalf::kHigh);
}

WASM_SIMD_TEST(I16x8ExtMulLowI8x16U) {
  RunExtMulTest<uint8_t, uint16_t>(execution_tier, kExprI16x8ExtMulLowI8x16U,
                                   MultiplyLong, kExprI8x16Splat,
                                   MulHalf::kLow);
}

WASM_SIMD_TEST(I16x8ExtMulHighI8x16U) {
  RunExtMulTest<uint8_t, uint16_t>(execution_tier, kExprI16x8ExtMulHighI8x16U,
                                   MultiplyLong, kExprI8x16Splat,
                                   MulHalf::kHigh);
}

WASM_SIMD_TEST(I32x4ExtMulLowI16x8S) {
  RunExtMulTest<int16_t, int32_t>(execution_tier, kExprI32x4ExtMulLowI16x8S,
                                  MultiplyLong, kExprI16x8Splat, MulHalf::kLow);
}

WASM_SIMD_TEST(I32x4ExtMulHighI16x8S) {
  RunExtMulTest<int16_t, int32_t>(execution_tier, kExprI32x4ExtMulHighI16x8S,
                                  MultiplyLong, kExprI16x8Splat,
                                  MulHalf::kHigh);
}

WASM_SIMD_TEST(I32x4ExtMulLowI16x8U) {
  RunExtMulTest<uint16_t, uint32_t>(execution_tier, kExprI32x4ExtMulLowI16x8U,
                                    MultiplyLong, kExprI16x8Splat,
                                    MulHalf::kLow);
}

WASM_SIMD_TEST(I32x4ExtMulHighI16x8U) {
  RunExtMulTest<uint16_t, uint32_t>(execution_tier, kExprI32x4ExtMulHighI16x8U,
                                    MultiplyLong, kExprI16x8Splat,
                                    MulHalf::kHigh);
}

WASM_SIMD_TEST(I64x2ExtMulLowI32x4S) {
  RunExtMulTest<int32_t, int64_t>(execution_tier, kExprI64x2ExtMulLowI32x4S,
                                  MultiplyLong, kExprI32x4Splat, MulHalf::kLow);
}

WASM_SIMD_TEST(I64x2ExtMulHighI32x4S) {
  RunExtMulTest<int32_t, int64_t>(execution_tier, kExprI64x2ExtMulHighI32x4S,
                                  MultiplyLong, kExprI32x4Splat,
                                  MulHalf::kHigh);
}

WASM_SIMD_TEST(I64x2ExtMulLowI32x4U) {
  RunExtMulTest<uint32_t, uint64_t>(execution_tier, kExprI64x2ExtMulLowI32x4U,
                                    MultiplyLong, kExprI32x4Splat,
                                    MulHalf::kLow);
}

WASM_SIMD_TEST(I64x2ExtMulHighI32x4U) {
  RunExtMulTest<uint32_t, uint64_t>(execution_tier, kExprI64x2ExtMulHighI32x4U,
                                    MultiplyLong, kExprI32x4Splat,
                                    MulHalf::kHigh);
}

WASM_SIMD_TEST(I32x4DotI16x8S) {
  WasmRunner<int32_t, int16_t, int16_t> r(execution_tier);
  int32_t* g = r.builder().template AddGlobal<int32_t>(kWasmS128);
  byte value1 = 0, value2 = 1;
  byte temp1 = r.AllocateLocal(kWasmS128);
  byte temp2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(value1))),
        WASM_LOCAL_SET(temp2, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(value2))),
        WASM_GLOBAL_SET(
            0, WASM_SIMD_BINOP(kExprI32x4DotI16x8S, WASM_LOCAL_GET(temp1),
                               WASM_LOCAL_GET(temp2))),
        WASM_ONE);

  for (int16_t x : compiler::ValueHelper::GetVector<int16_t>()) {
    for (int16_t y : compiler::ValueHelper::GetVector<int16_t>()) {
      r.Call(x, y);
      // x * y * 2 can overflow (0x8000), the behavior is to wraparound.
      int32_t expected = base::MulWithWraparound(x * y, 2);
      for (int i = 0; i < 4; i++) {
        CHECK_EQ(expected, ReadLittleEndianValue<int32_t>(&g[i]));
      }
    }
  }
}

WASM_SIMD_TEST(I16x8Shl) {
  RunI16x8ShiftOpTest(execution_tier, kExprI16x8Shl, LogicalShiftLeft);
}

WASM_SIMD_TEST(I16x8ShrS) {
  RunI16x8ShiftOpTest(execution_tier, kExprI16x8ShrS, ArithmeticShiftRight);
}

WASM_SIMD_TEST(I16x8ShrU) {
  RunI16x8ShiftOpTest(execution_tier, kExprI16x8ShrU, LogicalShiftRight);
}

WASM_SIMD_TEST(I8x16Neg) {
  RunI8x16UnOpTest(execution_tier, kExprI8x16Neg, base::NegateWithWraparound);
}

WASM_SIMD_TEST(I8x16Abs) {
  RunI8x16UnOpTest(execution_tier, kExprI8x16Abs, Abs);
}

WASM_SIMD_TEST(I8x16Popcnt) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Global to hold output.
  int8_t* g = r.builder().AddGlobal<int8_t>(kWasmS128);
  // Build fn to splat test value, perform unop, and write the result.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I8x16_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(
            0, WASM_SIMD_UNOP(kExprI8x16Popcnt, WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_UINT8_INPUTS(x) {
    r.Call(x);
    unsigned expected = base::bits::CountPopulation(x);
    for (int i = 0; i < 16; i++) {
      CHECK_EQ(expected, ReadLittleEndianValue<int8_t>(&g[i]));
    }
  }
}

// Tests both signed and unsigned conversion from I16x8 (packing).
WASM_SIMD_TEST(I8x16ConvertI16x8) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create output vectors to hold signed and unsigned results.
  int8_t* g_s = r.builder().AddGlobal<int8_t>(kWasmS128);
  uint8_t* g_u = r.builder().AddGlobal<uint8_t>(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  byte value = 0;
  byte temp1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(value))),
        WASM_GLOBAL_SET(
            0, WASM_SIMD_BINOP(kExprI8x16SConvertI16x8, WASM_LOCAL_GET(temp1),
                               WASM_LOCAL_GET(temp1))),
        WASM_GLOBAL_SET(
            1, WASM_SIMD_BINOP(kExprI8x16UConvertI16x8, WASM_LOCAL_GET(temp1),
                               WASM_LOCAL_GET(temp1))),
        WASM_ONE);

  FOR_INT16_INPUTS(x) {
    r.Call(x);
    int8_t expected_signed = base::saturated_cast<int8_t>(x);
    uint8_t expected_unsigned = base::saturated_cast<uint8_t>(x);
    for (int i = 0; i < 16; i++) {
      CHECK_EQ(expected_signed, ReadLittleEndianValue<int8_t>(&g_s[i]));
      CHECK_EQ(expected_unsigned, ReadLittleEndianValue<uint8_t>(&g_u[i]));
    }
  }
}

WASM_SIMD_TEST(I8x16Add) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16Add, base::AddWithWraparound);
}

WASM_SIMD_TEST(I8x16AddSatS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16AddSatS, SaturateAdd<int8_t>);
}

WASM_SIMD_TEST(I8x16Sub) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16Sub, base::SubWithWraparound);
}

WASM_SIMD_TEST(I8x16SubSatS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16SubSatS, SaturateSub<int8_t>);
}

WASM_SIMD_TEST(I8x16MinS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16MinS, Minimum);
}

WASM_SIMD_TEST(I8x16MaxS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16MaxS, Maximum);
}

WASM_SIMD_TEST(I8x16AddSatU) {
  RunI8x16BinOpTest<uint8_t>(execution_tier, kExprI8x16AddSatU,
                             SaturateAdd<uint8_t>);
}

WASM_SIMD_TEST(I8x16SubSatU) {
  RunI8x16BinOpTest<uint8_t>(execution_tier, kExprI8x16SubSatU,
                             SaturateSub<uint8_t>);
}

WASM_SIMD_TEST(I8x16MinU) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16MinU, UnsignedMinimum);
}

WASM_SIMD_TEST(I8x16MaxU) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16MaxU, UnsignedMaximum);
}

WASM_SIMD_TEST(I8x16Eq) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16Eq, Equal);
}

WASM_SIMD_TEST(I8x16Ne) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16Ne, NotEqual);
}

WASM_SIMD_TEST(I8x16GtS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16GtS, Greater);
}

WASM_SIMD_TEST(I8x16GeS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16GeS, GreaterEqual);
}

WASM_SIMD_TEST(I8x16LtS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16LtS, Less);
}

WASM_SIMD_TEST(I8x16LeS) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16LeS, LessEqual);
}

WASM_SIMD_TEST(I8x16GtU) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I8x16GeU) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16GeU, UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I8x16LtU) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16LtU, UnsignedLess);
}

WASM_SIMD_TEST(I8x16LeU) {
  RunI8x16BinOpTest(execution_tier, kExprI8x16LeU, UnsignedLessEqual);
}

WASM_SIMD_TEST(I8x16RoundingAverageU) {
  RunI8x16BinOpTest<uint8_t>(execution_tier, kExprI8x16RoundingAverageU,
                             RoundingAverageUnsigned);
}

WASM_SIMD_TEST(I8x16Shl) {
  RunI8x16ShiftOpTest(execution_tier, kExprI8x16Shl, LogicalShiftLeft);
}

WASM_SIMD_TEST(I8x16ShrS) {
  RunI8x16ShiftOpTest(execution_tier, kExprI8x16ShrS, ArithmeticShiftRight);
}

WASM_SIMD_TEST(I8x16ShrU) {
  RunI8x16ShiftOpTest(execution_tier, kExprI8x16ShrU, LogicalShiftRight);
}

// Test Select by making a mask where the 0th and 3rd lanes are true and the
// rest false, and comparing for non-equality with zero to convert to a boolean
// vector.
#define WASM_SIMD_SELECT_TEST(format)                                        \
  WASM_SIMD_TEST(S##format##Select) {                                        \
    WasmRunner<int32_t, int32_t, int32_t> r(execution_tier);                 \
    byte val1 = 0;                                                           \
    byte val2 = 1;                                                           \
    byte src1 = r.AllocateLocal(kWasmS128);                                  \
    byte src2 = r.AllocateLocal(kWasmS128);                                  \
    byte zero = r.AllocateLocal(kWasmS128);                                  \
    byte mask = r.AllocateLocal(kWasmS128);                                  \
    BUILD(r,                                                                 \
          WASM_LOCAL_SET(src1,                                               \
                         WASM_SIMD_I##format##_SPLAT(WASM_LOCAL_GET(val1))), \
          WASM_LOCAL_SET(src2,                                               \
                         WASM_SIMD_I##format##_SPLAT(WASM_LOCAL_GET(val2))), \
          WASM_LOCAL_SET(zero, WASM_SIMD_I##format##_SPLAT(WASM_ZERO)),      \
          WASM_LOCAL_SET(mask, WASM_SIMD_I##format##_REPLACE_LANE(           \
                                   1, WASM_LOCAL_GET(zero), WASM_I32V(-1))), \
          WASM_LOCAL_SET(mask, WASM_SIMD_I##format##_REPLACE_LANE(           \
                                   2, WASM_LOCAL_GET(mask), WASM_I32V(-1))), \
          WASM_LOCAL_SET(                                                    \
              mask,                                                          \
              WASM_SIMD_SELECT(                                              \
                  format, WASM_LOCAL_GET(src1), WASM_LOCAL_GET(src2),        \
                  WASM_SIMD_BINOP(kExprI##format##Ne, WASM_LOCAL_GET(mask),  \
                                  WASM_LOCAL_GET(zero)))),                   \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, val2, 0),             \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, val1, 1),             \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, val1, 2),             \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, val2, 3), WASM_ONE);  \
                                                                             \
    CHECK_EQ(1, r.Call(0x12, 0x34));                                         \
  }

WASM_SIMD_SELECT_TEST(32x4)
WASM_SIMD_SELECT_TEST(16x8)
WASM_SIMD_SELECT_TEST(8x16)

// Test Select by making a mask where the 0th and 3rd lanes are non-zero and the
// rest 0. The mask is not the result of a comparison op.
#define WASM_SIMD_NON_CANONICAL_SELECT_TEST(format)                           \
  WASM_SIMD_TEST(S##format##NonCanonicalSelect) {                             \
    WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier);         \
    byte val1 = 0;                                                            \
    byte val2 = 1;                                                            \
    byte combined = 2;                                                        \
    byte src1 = r.AllocateLocal(kWasmS128);                                   \
    byte src2 = r.AllocateLocal(kWasmS128);                                   \
    byte zero = r.AllocateLocal(kWasmS128);                                   \
    byte mask = r.AllocateLocal(kWasmS128);                                   \
    BUILD(r,                                                                  \
          WASM_LOCAL_SET(src1,                                                \
                         WASM_SIMD_I##format##_SPLAT(WASM_LOCAL_GET(val1))),  \
          WASM_LOCAL_SET(src2,                                                \
                         WASM_SIMD_I##format##_SPLAT(WASM_LOCAL_GET(val2))),  \
          WASM_LOCAL_SET(zero, WASM_SIMD_I##format##_SPLAT(WASM_ZERO)),       \
          WASM_LOCAL_SET(mask, WASM_SIMD_I##format##_REPLACE_LANE(            \
                                   1, WASM_LOCAL_GET(zero), WASM_I32V(0xF))), \
          WASM_LOCAL_SET(mask, WASM_SIMD_I##format##_REPLACE_LANE(            \
                                   2, WASM_LOCAL_GET(mask), WASM_I32V(0xF))), \
          WASM_LOCAL_SET(mask, WASM_SIMD_SELECT(format, WASM_LOCAL_GET(src1), \
                                                WASM_LOCAL_GET(src2),         \
                                                WASM_LOCAL_GET(mask))),       \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, val2, 0),              \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, combined, 1),          \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, combined, 2),          \
          WASM_SIMD_CHECK_LANE_S(I##format, mask, I32, val2, 3), WASM_ONE);   \
                                                                              \
    CHECK_EQ(1, r.Call(0x12, 0x34, 0x32));                                    \
  }

WASM_SIMD_NON_CANONICAL_SELECT_TEST(32x4)
WASM_SIMD_NON_CANONICAL_SELECT_TEST(16x8)
WASM_SIMD_NON_CANONICAL_SELECT_TEST(8x16)

// Test binary ops with two lane test patterns, all lanes distinct.
template <typename T>
void RunBinaryLaneOpTest(
    TestExecutionTier execution_tier, WasmOpcode simd_op,
    const std::array<T, kSimd128Size / sizeof(T)>& expected) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up two test patterns as globals, e.g. [0, 1, 2, 3] and [4, 5, 6, 7].
  T* src0 = r.builder().AddGlobal<T>(kWasmS128);
  T* src1 = r.builder().AddGlobal<T>(kWasmS128);
  static const int kElems = kSimd128Size / sizeof(T);
  for (int i = 0; i < kElems; i++) {
    WriteLittleEndianValue<T>(&src0[i], i);
    WriteLittleEndianValue<T>(&src1[i], kElems + i);
  }
  if (simd_op == kExprI8x16Shuffle) {
    BUILD(r,
          WASM_GLOBAL_SET(0, WASM_SIMD_I8x16_SHUFFLE_OP(simd_op, expected,
                                                        WASM_GLOBAL_GET(0),
                                                        WASM_GLOBAL_GET(1))),
          WASM_ONE);
  } else {
    BUILD(r,
          WASM_GLOBAL_SET(0, WASM_SIMD_BINOP(simd_op, WASM_GLOBAL_GET(0),
                                             WASM_GLOBAL_GET(1))),
          WASM_ONE);
  }

  CHECK_EQ(1, r.Call());
  for (size_t i = 0; i < expected.size(); i++) {
    CHECK_EQ(ReadLittleEndianValue<T>(&src0[i]), expected[i]);
  }
}

// Test shuffle ops.
void RunShuffleOpTest(TestExecutionTier execution_tier, WasmOpcode simd_op,
                      const std::array<int8_t, kSimd128Size>& shuffle) {
  // Test the original shuffle.
  RunBinaryLaneOpTest<int8_t>(execution_tier, simd_op, shuffle);

  // Test a non-canonical (inputs reversed) version of the shuffle.
  std::array<int8_t, kSimd128Size> other_shuffle(shuffle);
  for (size_t i = 0; i < shuffle.size(); ++i) other_shuffle[i] ^= kSimd128Size;
  RunBinaryLaneOpTest<int8_t>(execution_tier, simd_op, other_shuffle);

  // Test the swizzle (one-operand) version of the shuffle.
  std::array<int8_t, kSimd128Size> swizzle(shuffle);
  for (size_t i = 0; i < shuffle.size(); ++i) swizzle[i] &= (kSimd128Size - 1);
  RunBinaryLaneOpTest<int8_t>(execution_tier, simd_op, swizzle);

  // Test the non-canonical swizzle (one-operand) version of the shuffle.
  std::array<int8_t, kSimd128Size> other_swizzle(shuffle);
  for (size_t i = 0; i < shuffle.size(); ++i) other_swizzle[i] |= kSimd128Size;
  RunBinaryLaneOpTest<int8_t>(execution_tier, simd_op, other_swizzle);
}

#define SHUFFLE_LIST(V)  \
  V(S128Identity)        \
  V(S32x4Dup)            \
  V(S32x4ZipLeft)        \
  V(S32x4ZipRight)       \
  V(S32x4UnzipLeft)      \
  V(S32x4UnzipRight)     \
  V(S32x4TransposeLeft)  \
  V(S32x4TransposeRight) \
  V(S32x2Reverse)        \
  V(S32x4Irregular)      \
  V(S32x4Rotate)         \
  V(S16x8Dup)            \
  V(S16x8ZipLeft)        \
  V(S16x8ZipRight)       \
  V(S16x8UnzipLeft)      \
  V(S16x8UnzipRight)     \
  V(S16x8TransposeLeft)  \
  V(S16x8TransposeRight) \
  V(S16x4Reverse)        \
  V(S16x2Reverse)        \
  V(S16x8Irregular)      \
  V(S8x16Dup)            \
  V(S8x16ZipLeft)        \
  V(S8x16ZipRight)       \
  V(S8x16UnzipLeft)      \
  V(S8x16UnzipRight)     \
  V(S8x16TransposeLeft)  \
  V(S8x16TransposeRight) \
  V(S8x8Reverse)         \
  V(S8x4Reverse)         \
  V(S8x2Reverse)         \
  V(S8x16Irregular)

enum ShuffleKey {
#define SHUFFLE_ENUM_VALUE(Name) k##Name,
  SHUFFLE_LIST(SHUFFLE_ENUM_VALUE)
#undef SHUFFLE_ENUM_VALUE
      kNumShuffleKeys
};

using ShuffleMap = std::map<ShuffleKey, const Shuffle>;

ShuffleMap test_shuffles = {
    {kS128Identity,
     {{16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}}},
    {kS32x4Dup,
     {{16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19}}},
    {kS32x4ZipLeft, {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23}}},
    {kS32x4ZipRight,
     {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31}}},
    {kS32x4UnzipLeft,
     {{0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27}}},
    {kS32x4UnzipRight,
     {{4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31}}},
    {kS32x4TransposeLeft,
     {{0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27}}},
    {kS32x4TransposeRight,
     {{4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 28, 29, 30, 31}}},
    {kS32x2Reverse,  // swizzle only
     {{4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11}}},
    {kS32x4Irregular,
     {{0, 1, 2, 3, 16, 17, 18, 19, 16, 17, 18, 19, 20, 21, 22, 23}}},
    {kS32x4Rotate, {{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3}}},
    {kS16x8Dup,
     {{18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19}}},
    {kS16x8ZipLeft, {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23}}},
    {kS16x8ZipRight,
     {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31}}},
    {kS16x8UnzipLeft,
     {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29}}},
    {kS16x8UnzipRight,
     {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31}}},
    {kS16x8TransposeLeft,
     {{0, 1, 16, 17, 4, 5, 20, 21, 8, 9, 24, 25, 12, 13, 28, 29}}},
    {kS16x8TransposeRight,
     {{2, 3, 18, 19, 6, 7, 22, 23, 10, 11, 26, 27, 14, 15, 30, 31}}},
    {kS16x4Reverse,  // swizzle only
     {{6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9}}},
    {kS16x2Reverse,  // swizzle only
     {{2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13}}},
    {kS16x8Irregular,
     {{0, 1, 16, 17, 16, 17, 0, 1, 4, 5, 20, 21, 6, 7, 22, 23}}},
    {kS8x16Dup,
     {{19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19}}},
    {kS8x16ZipLeft, {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}}},
    {kS8x16ZipRight,
     {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31}}},
    {kS8x16UnzipLeft,
     {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30}}},
    {kS8x16UnzipRight,
     {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31}}},
    {kS8x16TransposeLeft,
     {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30}}},
    {kS8x16TransposeRight,
     {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31}}},
    {kS8x8Reverse,  // swizzle only
     {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8}}},
    {kS8x4Reverse,  // swizzle only
     {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12}}},
    {kS8x2Reverse,  // swizzle only
     {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14}}},
    {kS8x16Irregular,
     {{0, 16, 0, 16, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}}},
};

#define SHUFFLE_TEST(Name)                                           \
  WASM_SIMD_TEST(Name) {                                             \
    ShuffleMap::const_iterator it = test_shuffles.find(k##Name);     \
    DCHECK_NE(it, test_shuffles.end());                              \
    RunShuffleOpTest(execution_tier, kExprI8x16Shuffle, it->second); \
  }
SHUFFLE_LIST(SHUFFLE_TEST)
#undef SHUFFLE_TEST
#undef SHUFFLE_LIST

// Test shuffles that blend the two vectors (elements remain in their lanes.)
WASM_SIMD_TEST(S8x16Blend) {
  std::array<int8_t, kSimd128Size> expected;
  for (int bias = 1; bias < kSimd128Size; bias++) {
    for (int i = 0; i < bias; i++) expected[i] = i;
    for (int i = bias; i < kSimd128Size; i++) expected[i] = i + kSimd128Size;
    RunShuffleOpTest(execution_tier, kExprI8x16Shuffle, expected);
  }
}

// Test shuffles that concatenate the two vectors.
WASM_SIMD_TEST(S8x16Concat) {
  std::array<int8_t, kSimd128Size> expected;
  // n is offset or bias of concatenation.
  for (int n = 1; n < kSimd128Size; ++n) {
    int i = 0;
    // last kLanes - n bytes of first vector.
    for (int j = n; j < kSimd128Size; ++j) {
      expected[i++] = j;
    }
    // first n bytes of second vector
    for (int j = 0; j < n; ++j) {
      expected[i++] = j + kSimd128Size;
    }
    RunShuffleOpTest(execution_tier, kExprI8x16Shuffle, expected);
  }
}

WASM_SIMD_TEST(ShuffleShufps) {
  // We reverse engineer the shufps immediates into 8x16 shuffles.
  std::array<int8_t, kSimd128Size> expected;
  for (int mask = 0; mask < 256; mask++) {
    // Each iteration of this loop sets byte[i] of the 32x4 lanes.
    // Low 2 lanes (2-bits each) select from first input.
    uint8_t index0 = (mask & 3) * 4;
    uint8_t index1 = ((mask >> 2) & 3) * 4;
    // Next 2 bits select from src2, so add 16 to the index.
    uint8_t index2 = ((mask >> 4) & 3) * 4 + 16;
    uint8_t index3 = ((mask >> 6) & 3) * 4 + 16;

    for (int i = 0; i < 4; i++) {
      expected[0 + i] = index0 + i;
      expected[4 + i] = index1 + i;
      expected[8 + i] = index2 + i;
      expected[12 + i] = index3 + i;
    }
    RunShuffleOpTest(execution_tier, kExprI8x16Shuffle, expected);
  }
}

struct SwizzleTestArgs {
  const Shuffle input;
  const Shuffle indices;
  const Shuffle expected;
};

static constexpr SwizzleTestArgs swizzle_test_args[] = {
    {{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
     {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
     {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
     {15, 0, 14, 1, 13, 2, 12, 3, 11, 4, 10, 5, 9, 6, 8, 7},
     {0, 15, 1, 14, 2, 13, 3, 12, 4, 11, 5, 10, 6, 9, 7, 8}},
    {{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
     {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30},
     {15, 13, 11, 9, 7, 5, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0}},
    // all indices are out of range
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
     {16, 17, 18, 19, 20, 124, 125, 126, 127, -1, -2, -3, -4, -5, -6, -7},
     {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};

static constexpr Vector<const SwizzleTestArgs> swizzle_test_vector =
    ArrayVector(swizzle_test_args);

WASM_SIMD_TEST(I8x16Swizzle) {
  // RunBinaryLaneOpTest set up the two globals to be consecutive integers,
  // [0-15] and [16-31]. Using [0-15] as the indices will not sufficiently test
  // swizzle since the expected result is a no-op, using [16-31] will result in
  // all 0s.
  WasmRunner<int32_t> r(execution_tier);
  static const int kElems = kSimd128Size / sizeof(uint8_t);
  uint8_t* dst = r.builder().AddGlobal<uint8_t>(kWasmS128);
  uint8_t* src0 = r.builder().AddGlobal<uint8_t>(kWasmS128);
  uint8_t* src1 = r.builder().AddGlobal<uint8_t>(kWasmS128);
  BUILD(
      r,
      WASM_GLOBAL_SET(0, WASM_SIMD_BINOP(kExprI8x16Swizzle, WASM_GLOBAL_GET(1),
                                         WASM_GLOBAL_GET(2))),
      WASM_ONE);

  for (SwizzleTestArgs si : swizzle_test_vector) {
    for (int i = 0; i < kElems; i++) {
      WriteLittleEndianValue<uint8_t>(&src0[i], si.input[i]);
      WriteLittleEndianValue<uint8_t>(&src1[i], si.indices[i]);
    }

    CHECK_EQ(1, r.Call());

    for (int i = 0; i < kElems; i++) {
      CHECK_EQ(ReadLittleEndianValue<uint8_t>(&dst[i]), si.expected[i]);
    }
  }

  {
    // We have an optimization for constant indices, test this case.
    for (SwizzleTestArgs si : swizzle_test_vector) {
      WasmRunner<int32_t> r(execution_tier);
      uint8_t* dst = r.builder().AddGlobal<uint8_t>(kWasmS128);
      uint8_t* src0 = r.builder().AddGlobal<uint8_t>(kWasmS128);
      BUILD(r,
            WASM_GLOBAL_SET(
                0, WASM_SIMD_BINOP(kExprI8x16Swizzle, WASM_GLOBAL_GET(1),
                                   WASM_SIMD_CONSTANT(si.indices))),
            WASM_ONE);

      for (int i = 0; i < kSimd128Size; i++) {
        WriteLittleEndianValue<uint8_t>(&src0[i], si.input[i]);
      }

      CHECK_EQ(1, r.Call());

      for (int i = 0; i < kSimd128Size; i++) {
        CHECK_EQ(ReadLittleEndianValue<uint8_t>(&dst[i]), si.expected[i]);
      }
    }
  }
}

// Combine 3 shuffles a, b, and c by applying both a and b and then applying c
// to those two results.
Shuffle Combine(const Shuffle& a, const Shuffle& b, const Shuffle& c) {
  Shuffle result;
  for (int i = 0; i < kSimd128Size; ++i) {
    result[i] = c[i] < kSimd128Size ? a[c[i]] : b[c[i] - kSimd128Size];
  }
  return result;
}

const Shuffle& GetRandomTestShuffle(v8::base::RandomNumberGenerator* rng) {
  return test_shuffles[static_cast<ShuffleKey>(rng->NextInt(kNumShuffleKeys))];
}

// Test shuffles that are random combinations of 3 test shuffles. Completely
// random shuffles almost always generate the slow general shuffle code, so
// don't exercise as many code paths.
WASM_SIMD_TEST(I8x16ShuffleFuzz) {
  v8::base::RandomNumberGenerator* rng = CcTest::random_number_generator();
  static const int kTests = 100;
  for (int i = 0; i < kTests; ++i) {
    auto shuffle = Combine(GetRandomTestShuffle(rng), GetRandomTestShuffle(rng),
                           GetRandomTestShuffle(rng));
    RunShuffleOpTest(execution_tier, kExprI8x16Shuffle, shuffle);
  }
}

void AppendShuffle(const Shuffle& shuffle, std::vector<byte>* buffer) {
  byte opcode[] = {WASM_SIMD_OP(kExprI8x16Shuffle)};
  for (size_t i = 0; i < arraysize(opcode); ++i) buffer->push_back(opcode[i]);
  for (size_t i = 0; i < kSimd128Size; ++i) buffer->push_back((shuffle[i]));
}

void BuildShuffle(const std::vector<Shuffle>& shuffles,
                  std::vector<byte>* buffer) {
  // Perform the leaf shuffles on globals 0 and 1.
  size_t row_index = (shuffles.size() - 1) / 2;
  for (size_t i = row_index; i < shuffles.size(); ++i) {
    byte operands[] = {WASM_GLOBAL_GET(0), WASM_GLOBAL_GET(1)};
    for (size_t j = 0; j < arraysize(operands); ++j)
      buffer->push_back(operands[j]);
    AppendShuffle(shuffles[i], buffer);
  }
  // Now perform inner shuffles in the correct order on operands on the stack.
  do {
    for (size_t i = row_index / 2; i < row_index; ++i) {
      AppendShuffle(shuffles[i], buffer);
    }
    row_index /= 2;
  } while (row_index != 0);
  byte epilog[] = {kExprGlobalSet, static_cast<byte>(0), WASM_ONE};
  for (size_t j = 0; j < arraysize(epilog); ++j) buffer->push_back(epilog[j]);
}

void RunWasmCode(TestExecutionTier execution_tier,
                 const std::vector<byte>& code,
                 std::array<int8_t, kSimd128Size>* result) {
  WasmRunner<int32_t> r(execution_tier);
  // Set up two test patterns as globals, e.g. [0, 1, 2, 3] and [4, 5, 6, 7].
  int8_t* src0 = r.builder().AddGlobal<int8_t>(kWasmS128);
  int8_t* src1 = r.builder().AddGlobal<int8_t>(kWasmS128);
  for (int i = 0; i < kSimd128Size; ++i) {
    WriteLittleEndianValue<int8_t>(&src0[i], i);
    WriteLittleEndianValue<int8_t>(&src1[i], kSimd128Size + i);
  }
  r.Build(code.data(), code.data() + code.size());
  CHECK_EQ(1, r.Call());
  for (size_t i = 0; i < kSimd128Size; i++) {
    (*result)[i] = ReadLittleEndianValue<int8_t>(&src0[i]);
  }
}

// Test multiple shuffles executed in sequence.
WASM_SIMD_TEST(S8x16MultiShuffleFuzz) {
  // Don't compare interpreter results with itself.
  if (execution_tier == TestExecutionTier::kInterpreter) {
    return;
  }
  v8::base::RandomNumberGenerator* rng = CcTest::random_number_generator();
  static const int kShuffles = 100;
  for (int i = 0; i < kShuffles; ++i) {
    // Create an odd number in [3..23] of random test shuffles so we can build
    // a complete binary tree (stored as a heap) of shuffle operations. The leaf
    // shuffles operate on the test pattern inputs, while the interior shuffles
    // operate on the results of the two child shuffles.
    int num_shuffles = rng->NextInt(10) * 2 + 3;
    std::vector<Shuffle> shuffles;
    for (int j = 0; j < num_shuffles; ++j) {
      shuffles.push_back(GetRandomTestShuffle(rng));
    }
    // Generate the code for the shuffle expression.
    std::vector<byte> buffer;
    BuildShuffle(shuffles, &buffer);

    // Run the code using the interpreter to get the expected result.
    std::array<int8_t, kSimd128Size> expected;
    RunWasmCode(TestExecutionTier::kInterpreter, buffer, &expected);
    // Run the SIMD or scalar lowered compiled code and compare results.
    std::array<int8_t, kSimd128Size> result;
    RunWasmCode(execution_tier, buffer, &result);
    for (size_t i = 0; i < kSimd128Size; ++i) {
      CHECK_EQ(result[i], expected[i]);
    }
  }
}

// Boolean unary operations are 'AllTrue' and 'AnyTrue', which return an integer
// result. Use relational ops on numeric vectors to create the boolean vector
// test inputs. Test inputs with all true, all false, one true, and one false.
#define WASM_SIMD_BOOL_REDUCTION_TEST(format, lanes, int_type)                 \
  WASM_SIMD_TEST(ReductionTest##lanes) {                                       \
    WasmRunner<int32_t> r(execution_tier);                                     \
    if (lanes == 2) return;                                                    \
    byte zero = r.AllocateLocal(kWasmS128);                                    \
    byte one_one = r.AllocateLocal(kWasmS128);                                 \
    byte reduced = r.AllocateLocal(kWasmI32);                                  \
    BUILD(r, WASM_LOCAL_SET(zero, WASM_SIMD_I##format##_SPLAT(int_type(0))),   \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprV128AnyTrue,                        \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_LOCAL_GET(zero),    \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprV128AnyTrue,                        \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_LOCAL_GET(zero),    \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprI##format##AllTrue,                 \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_LOCAL_GET(zero),    \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprI##format##AllTrue,                 \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_LOCAL_GET(zero),    \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(one_one,                                              \
                         WASM_SIMD_I##format##_REPLACE_LANE(                   \
                             lanes - 1, WASM_LOCAL_GET(zero), int_type(1))),   \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprV128AnyTrue,                        \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_LOCAL_GET(one_one), \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprV128AnyTrue,                        \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_LOCAL_GET(one_one), \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprI##format##AllTrue,                 \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_LOCAL_GET(one_one), \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_LOCAL_SET(                                                      \
              reduced, WASM_SIMD_UNOP(kExprI##format##AllTrue,                 \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_LOCAL_GET(one_one), \
                                                      WASM_LOCAL_GET(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_LOCAL_GET(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_ONE);                                                           \
    CHECK_EQ(1, r.Call());                                                     \
  }

WASM_SIMD_BOOL_REDUCTION_TEST(64x2, 2, WASM_I64V)
WASM_SIMD_BOOL_REDUCTION_TEST(32x4, 4, WASM_I32V)
WASM_SIMD_BOOL_REDUCTION_TEST(16x8, 8, WASM_I32V)
WASM_SIMD_BOOL_REDUCTION_TEST(8x16, 16, WASM_I32V)

WASM_SIMD_TEST(SimdI32x4ExtractWithF32x4) {
  WasmRunner<int32_t> r(execution_tier);
  BUILD(r, WASM_IF_ELSE_I(
               WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                               0, WASM_SIMD_F32x4_SPLAT(WASM_F32(30.5))),
                           WASM_I32_REINTERPRET_F32(WASM_F32(30.5))),
               WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdF32x4ExtractWithI32x4) {
  WasmRunner<int32_t> r(execution_tier);
  BUILD(r,
        WASM_IF_ELSE_I(WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                                       0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(15))),
                                   WASM_F32_REINTERPRET_I32(WASM_I32V(15))),
                       WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdF32x4ExtractLane) {
  WasmRunner<float> r(execution_tier);
  r.AllocateLocal(kWasmF32);
  r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_LOCAL_SET(0, WASM_SIMD_F32x4_EXTRACT_LANE(
                              0, WASM_SIMD_F32x4_SPLAT(WASM_F32(30.5)))),
        WASM_LOCAL_SET(1, WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(0))),
        WASM_SIMD_F32x4_EXTRACT_LANE(1, WASM_LOCAL_GET(1)));
  CHECK_EQ(30.5, r.Call());
}

WASM_SIMD_TEST(SimdF32x4AddWithI32x4) {
  // Choose two floating point values whose sum is normal and exactly
  // representable as a float.
  const int kOne = 0x3F800000;
  const int kTwo = 0x40000000;
  WasmRunner<int32_t> r(execution_tier);
  BUILD(r,
        WASM_IF_ELSE_I(
            WASM_F32_EQ(
                WASM_SIMD_F32x4_EXTRACT_LANE(
                    0, WASM_SIMD_BINOP(kExprF32x4Add,
                                       WASM_SIMD_I32x4_SPLAT(WASM_I32V(kOne)),
                                       WASM_SIMD_I32x4_SPLAT(WASM_I32V(kTwo)))),
                WASM_F32_ADD(WASM_F32_REINTERPRET_I32(WASM_I32V(kOne)),
                             WASM_F32_REINTERPRET_I32(WASM_I32V(kTwo)))),
            WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdI32x4AddWithF32x4) {
  WasmRunner<int32_t> r(execution_tier);
  BUILD(r,
        WASM_IF_ELSE_I(
            WASM_I32_EQ(
                WASM_SIMD_I32x4_EXTRACT_LANE(
                    0, WASM_SIMD_BINOP(kExprI32x4Add,
                                       WASM_SIMD_F32x4_SPLAT(WASM_F32(21.25)),
                                       WASM_SIMD_F32x4_SPLAT(WASM_F32(31.5)))),
                WASM_I32_ADD(WASM_I32_REINTERPRET_F32(WASM_F32(21.25)),
                             WASM_I32_REINTERPRET_F32(WASM_F32(31.5)))),
            WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdI32x4Local) {
  WasmRunner<int32_t> r(execution_tier);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),

        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_LOCAL_GET(0)));
  CHECK_EQ(31, r.Call());
}

WASM_SIMD_TEST(SimdI32x4SplatFromExtract) {
  WasmRunner<int32_t> r(execution_tier);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_LOCAL_SET(0, WASM_SIMD_I32x4_EXTRACT_LANE(
                              0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(76)))),
        WASM_LOCAL_SET(1, WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(0))),
        WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_LOCAL_GET(1)));
  CHECK_EQ(76, r.Call());
}

WASM_SIMD_TEST(SimdI32x4For) {
  WasmRunner<int32_t> r(execution_tier);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r,

        WASM_LOCAL_SET(1, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),
        WASM_LOCAL_SET(1, WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_LOCAL_GET(1),
                                                       WASM_I32V(53))),
        WASM_LOCAL_SET(1, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_LOCAL_GET(1),
                                                       WASM_I32V(23))),
        WASM_LOCAL_SET(0, WASM_I32V(0)),
        WASM_LOOP(
            WASM_LOCAL_SET(
                1, WASM_SIMD_BINOP(kExprI32x4Add, WASM_LOCAL_GET(1),
                                   WASM_SIMD_I32x4_SPLAT(WASM_I32V(1)))),
            WASM_IF(WASM_I32_NE(WASM_INC_LOCAL(0), WASM_I32V(5)), WASM_BR(1))),
        WASM_LOCAL_SET(0, WASM_I32V(1)),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_LOCAL_GET(1)),
                            WASM_I32V(36)),
                WASM_LOCAL_SET(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_LOCAL_GET(1)),
                            WASM_I32V(58)),
                WASM_LOCAL_SET(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_LOCAL_GET(1)),
                            WASM_I32V(28)),
                WASM_LOCAL_SET(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(3, WASM_LOCAL_GET(1)),
                            WASM_I32V(36)),
                WASM_LOCAL_SET(0, WASM_I32V(0))),
        WASM_LOCAL_GET(0));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdF32x4For) {
  WasmRunner<int32_t> r(execution_tier);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(1, WASM_SIMD_F32x4_SPLAT(WASM_F32(21.25))),
        WASM_LOCAL_SET(1, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_LOCAL_GET(1),
                                                       WASM_F32(19.5))),
        WASM_LOCAL_SET(0, WASM_I32V(0)),
        WASM_LOOP(
            WASM_LOCAL_SET(
                1, WASM_SIMD_BINOP(kExprF32x4Add, WASM_LOCAL_GET(1),
                                   WASM_SIMD_F32x4_SPLAT(WASM_F32(2.0)))),
            WASM_IF(WASM_I32_NE(WASM_INC_LOCAL(0), WASM_I32V(3)), WASM_BR(1))),
        WASM_LOCAL_SET(0, WASM_I32V(1)),
        WASM_IF(WASM_F32_NE(WASM_SIMD_F32x4_EXTRACT_LANE(0, WASM_LOCAL_GET(1)),
                            WASM_F32(27.25)),
                WASM_LOCAL_SET(0, WASM_I32V(0))),
        WASM_IF(WASM_F32_NE(WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_LOCAL_GET(1)),
                            WASM_F32(25.5)),
                WASM_LOCAL_SET(0, WASM_I32V(0))),
        WASM_LOCAL_GET(0));
  CHECK_EQ(1, r.Call());
}

template <typename T, int numLanes = 4>
void SetVectorByLanes(T* v, const std::array<T, numLanes>& arr) {
  for (int lane = 0; lane < numLanes; lane++) {
    WriteLittleEndianValue<T>(&v[lane], arr[lane]);
  }
}

template <typename T>
const T GetScalar(T* v, int lane) {
  constexpr int kElems = kSimd128Size / sizeof(T);
  const int index = lane;
  USE(kElems);
  DCHECK(index >= 0 && index < kElems);
  return ReadLittleEndianValue<T>(&v[index]);
}

WASM_SIMD_TEST(SimdI32x4GetGlobal) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Pad the globals with a few unused slots to get a non-zero offset.
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  int32_t* global = r.builder().AddGlobal<int32_t>(kWasmS128);
  SetVectorByLanes(global, {{0, 1, 2, 3}});
  r.AllocateLocal(kWasmI32);
  BUILD(
      r, WASM_LOCAL_SET(1, WASM_I32V(1)),
      WASM_IF(WASM_I32_NE(WASM_I32V(0),
                          WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GLOBAL_GET(4))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(1),
                          WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GLOBAL_GET(4))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(2),
                          WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_GLOBAL_GET(4))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(3),
                          WASM_SIMD_I32x4_EXTRACT_LANE(3, WASM_GLOBAL_GET(4))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_LOCAL_GET(1));
  CHECK_EQ(1, r.Call(0));
}

WASM_SIMD_TEST(SimdI32x4SetGlobal) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Pad the globals with a few unused slots to get a non-zero offset.
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  int32_t* global = r.builder().AddGlobal<int32_t>(kWasmS128);
  BUILD(r, WASM_GLOBAL_SET(4, WASM_SIMD_I32x4_SPLAT(WASM_I32V(23))),
        WASM_GLOBAL_SET(4, WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_GLOBAL_GET(4),
                                                        WASM_I32V(34))),
        WASM_GLOBAL_SET(4, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GLOBAL_GET(4),
                                                        WASM_I32V(45))),
        WASM_GLOBAL_SET(4, WASM_SIMD_I32x4_REPLACE_LANE(3, WASM_GLOBAL_GET(4),
                                                        WASM_I32V(56))),
        WASM_I32V(1));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(GetScalar(global, 0), 23);
  CHECK_EQ(GetScalar(global, 1), 34);
  CHECK_EQ(GetScalar(global, 2), 45);
  CHECK_EQ(GetScalar(global, 3), 56);
}

WASM_SIMD_TEST(SimdF32x4GetGlobal) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  float* global = r.builder().AddGlobal<float>(kWasmS128);
  SetVectorByLanes<float>(global, {{0.0, 1.5, 2.25, 3.5}});
  r.AllocateLocal(kWasmI32);
  BUILD(
      r, WASM_LOCAL_SET(1, WASM_I32V(1)),
      WASM_IF(WASM_F32_NE(WASM_F32(0.0),
                          WASM_SIMD_F32x4_EXTRACT_LANE(0, WASM_GLOBAL_GET(0))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(1.5),
                          WASM_SIMD_F32x4_EXTRACT_LANE(1, WASM_GLOBAL_GET(0))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(2.25),
                          WASM_SIMD_F32x4_EXTRACT_LANE(2, WASM_GLOBAL_GET(0))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(3.5),
                          WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_GLOBAL_GET(0))),
              WASM_LOCAL_SET(1, WASM_I32V(0))),
      WASM_LOCAL_GET(1));
  CHECK_EQ(1, r.Call(0));
}

WASM_SIMD_TEST(SimdF32x4SetGlobal) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  float* global = r.builder().AddGlobal<float>(kWasmS128);
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_SPLAT(WASM_F32(13.5))),
        WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_REPLACE_LANE(1, WASM_GLOBAL_GET(0),
                                                        WASM_F32(45.5))),
        WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_REPLACE_LANE(2, WASM_GLOBAL_GET(0),
                                                        WASM_F32(32.25))),
        WASM_GLOBAL_SET(0, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GLOBAL_GET(0),
                                                        WASM_F32(65.0))),
        WASM_I32V(1));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(GetScalar(global, 0), 13.5f);
  CHECK_EQ(GetScalar(global, 1), 45.5f);
  CHECK_EQ(GetScalar(global, 2), 32.25f);
  CHECK_EQ(GetScalar(global, 3), 65.0f);
}

WASM_SIMD_TEST(SimdLoadStoreLoad) {
  WasmRunner<int32_t> r(execution_tier);
  int32_t* memory =
      r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
  // Load memory, store it, then reload it and extract the first lane. Use a
  // non-zero offset into the memory of 1 lane (4 bytes) to test indexing.
  BUILD(r, WASM_SIMD_STORE_MEM(WASM_I32V(8), WASM_SIMD_LOAD_MEM(WASM_I32V(4))),
        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_SIMD_LOAD_MEM(WASM_I32V(8))));

  FOR_INT32_INPUTS(i) {
    int32_t expected = i;
    r.builder().WriteMemory(&memory[1], expected);
    CHECK_EQ(expected, r.Call());
  }

  {
    // OOB tests for loads.
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
    BUILD(r, WASM_SIMD_I32x4_EXTRACT_LANE(
                 0, WASM_SIMD_LOAD_MEM(WASM_LOCAL_GET(0))));

    for (uint32_t offset = kWasmPageSize - (kSimd128Size - 1);
         offset < kWasmPageSize; ++offset) {
      CHECK_TRAP(r.Call(offset));
    }
  }

  {
    // OOB tests for stores.
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
    BUILD(r,
          WASM_SIMD_STORE_MEM(WASM_LOCAL_GET(0), WASM_SIMD_LOAD_MEM(WASM_ZERO)),
          WASM_ONE);

    for (uint32_t offset = kWasmPageSize - (kSimd128Size - 1);
         offset < kWasmPageSize; ++offset) {
      CHECK_TRAP(r.Call(offset));
    }
  }
}

WASM_SIMD_TEST(SimdLoadStoreLoadMemargOffset) {
  WasmRunner<int32_t> r(execution_tier);
  int32_t* memory =
      r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
  constexpr byte offset_1 = 4;
  constexpr byte offset_2 = 8;
  // Load from memory at offset_1, store to offset_2, load from offset_2, and
  // extract first lane. We use non-zero memarg offsets to test offset decoding.
  BUILD(
      r,
      WASM_SIMD_STORE_MEM_OFFSET(
          offset_2, WASM_ZERO, WASM_SIMD_LOAD_MEM_OFFSET(offset_1, WASM_ZERO)),
      WASM_SIMD_I32x4_EXTRACT_LANE(
          0, WASM_SIMD_LOAD_MEM_OFFSET(offset_2, WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = i;
    // Index 1 of memory (int32_t) will be bytes 4 to 8.
    r.builder().WriteMemory(&memory[1], expected);
    CHECK_EQ(expected, r.Call());
  }

  {
    // OOB tests for loads with offsets.
    for (uint32_t offset = kWasmPageSize - (kSimd128Size - 1);
         offset < kWasmPageSize; ++offset) {
      WasmRunner<int32_t> r(execution_tier);
      r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
      BUILD(r, WASM_SIMD_I32x4_EXTRACT_LANE(
                   0, WASM_SIMD_LOAD_MEM_OFFSET(U32V_3(offset), WASM_ZERO)));
      CHECK_TRAP(r.Call());
    }
  }

  {
    // OOB tests for stores with offsets
    for (uint32_t offset = kWasmPageSize - (kSimd128Size - 1);
         offset < kWasmPageSize; ++offset) {
      WasmRunner<int32_t, uint32_t> r(execution_tier);
      r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
      BUILD(r,
            WASM_SIMD_STORE_MEM_OFFSET(U32V_3(offset), WASM_ZERO,
                                       WASM_SIMD_LOAD_MEM(WASM_ZERO)),
            WASM_ONE);
      CHECK_TRAP(r.Call(offset));
    }
  }
}

// Test a multi-byte opcode with offset values that encode into valid opcodes.
// This is to exercise decoding logic and make sure we get the lengths right.
WASM_SIMD_TEST(S128Load8SplatOffset) {
  // This offset is [82, 22] when encoded, which contains valid opcodes.
  constexpr int offset = 4354;
  WasmRunner<int32_t> r(execution_tier);
  int8_t* memory = r.builder().AddMemoryElems<int8_t>(kWasmPageSize);
  int8_t* global = r.builder().AddGlobal<int8_t>(kWasmS128);
  BUILD(r,
        WASM_GLOBAL_SET(
            0, WASM_SIMD_LOAD_OP_OFFSET(kExprS128Load8Splat, WASM_I32V(0),
                                        U32V_2(offset))),
        WASM_ONE);

  // We don't really care about all valid values, so just test for 1.
  int8_t x = 7;
  r.builder().WriteMemory(&memory[offset], x);
  r.Call();
  for (int i = 0; i < 16; i++) {
    CHECK_EQ(x, ReadLittleEndianValue<int8_t>(&global[i]));
  }
}

template <typename T>
void RunLoadSplatTest(TestExecutionTier execution_tier, WasmOpcode op) {
  constexpr int lanes = 16 / sizeof(T);
  constexpr int mem_index = 16;  // Load from mem index 16 (bytes).
  WasmRunner<int32_t> r(execution_tier);
  T* memory = r.builder().AddMemoryElems<T>(kWasmPageSize / sizeof(T));
  T* global = r.builder().AddGlobal<T>(kWasmS128);
  BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP(op, WASM_I32V(mem_index))),
        WASM_ONE);

  for (T x : compiler::ValueHelper::GetVector<T>()) {
    // 16-th byte in memory is lanes-th element (size T) of memory.
    r.builder().WriteMemory(&memory[lanes], x);
    r.Call();
    for (int i = 0; i < lanes; i++) {
      CHECK_EQ(x, ReadLittleEndianValue<T>(&global[i]));
    }
  }

  // Test for OOB.
  {
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<T>(kWasmPageSize / sizeof(T));
    r.builder().AddGlobal<T>(kWasmS128);

    BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP(op, WASM_LOCAL_GET(0))),
          WASM_ONE);

    // Load splats load sizeof(T) bytes.
    for (uint32_t offset = kWasmPageSize - (sizeof(T) - 1);
         offset < kWasmPageSize; ++offset) {
      CHECK_TRAP(r.Call(offset));
    }
  }
}

WASM_SIMD_TEST(S128Load8Splat) {
  RunLoadSplatTest<int8_t>(execution_tier, kExprS128Load8Splat);
}

WASM_SIMD_TEST(S128Load16Splat) {
  RunLoadSplatTest<int16_t>(execution_tier, kExprS128Load16Splat);
}

WASM_SIMD_TEST(S128Load32Splat) {
  RunLoadSplatTest<int32_t>(execution_tier, kExprS128Load32Splat);
}

WASM_SIMD_TEST(S128Load64Splat) {
  RunLoadSplatTest<int64_t>(execution_tier, kExprS128Load64Splat);
}

template <typename S, typename T>
void RunLoadExtendTest(TestExecutionTier execution_tier, WasmOpcode op) {
  static_assert(sizeof(S) < sizeof(T),
                "load extend should go from smaller to larger type");
  constexpr int lanes_s = 16 / sizeof(S);
  constexpr int lanes_t = 16 / sizeof(T);
  constexpr int mem_index = 16;  // Load from mem index 16 (bytes).
  // Load extends always load 64 bits, so alignment values can be from 0 to 3.
  for (byte alignment = 0; alignment <= 3; alignment++) {
    WasmRunner<int32_t> r(execution_tier);
    S* memory = r.builder().AddMemoryElems<S>(kWasmPageSize / sizeof(S));
    T* global = r.builder().AddGlobal<T>(kWasmS128);
    BUILD(r,
          WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP_ALIGNMENT(
                                 op, WASM_I32V(mem_index), alignment)),
          WASM_ONE);

    for (S x : compiler::ValueHelper::GetVector<S>()) {
      for (int i = 0; i < lanes_s; i++) {
        // 16-th byte in memory is lanes-th element (size T) of memory.
        r.builder().WriteMemory(&memory[lanes_s + i], x);
      }
      r.Call();
      for (int i = 0; i < lanes_t; i++) {
        CHECK_EQ(static_cast<T>(x), ReadLittleEndianValue<T>(&global[i]));
      }
    }
  }

  // Test for offset.
  {
    WasmRunner<int32_t> r(execution_tier);
    S* memory = r.builder().AddMemoryElems<S>(kWasmPageSize / sizeof(S));
    T* global = r.builder().AddGlobal<T>(kWasmS128);
    constexpr byte offset = sizeof(S);
    BUILD(r,
          WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP_OFFSET(op, WASM_ZERO, offset)),
          WASM_ONE);

    // Let max_s be the max_s value for type S, we set up the memory as such:
    // memory = [max_s, max_s - 1, ... max_s - (lane_s - 1)].
    constexpr S max_s = std::numeric_limits<S>::max();
    for (int i = 0; i < lanes_s; i++) {
      // Integer promotion due to -, static_cast to narrow.
      r.builder().WriteMemory(&memory[i], static_cast<S>(max_s - i));
    }

    r.Call();

    // Loads will be offset by sizeof(S), so will always start from (max_s - 1).
    for (int i = 0; i < lanes_t; i++) {
      // Integer promotion due to -, static_cast to narrow.
      T expected = static_cast<T>(max_s - i - 1);
      CHECK_EQ(expected, ReadLittleEndianValue<T>(&global[i]));
    }
  }

  // Test for OOB.
  {
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<S>(kWasmPageSize / sizeof(S));
    r.builder().AddGlobal<T>(kWasmS128);

    BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP(op, WASM_LOCAL_GET(0))),
          WASM_ONE);

    // Load extends load 8 bytes, so should trap from -7.
    for (uint32_t offset = kWasmPageSize - 7; offset < kWasmPageSize;
         ++offset) {
      CHECK_TRAP(r.Call(offset));
    }
  }
}

WASM_SIMD_TEST(S128Load8x8U) {
  RunLoadExtendTest<uint8_t, uint16_t>(execution_tier, kExprS128Load8x8U);
}

WASM_SIMD_TEST(S128Load8x8S) {
  RunLoadExtendTest<int8_t, int16_t>(execution_tier, kExprS128Load8x8S);
}
WASM_SIMD_TEST(S128Load16x4U) {
  RunLoadExtendTest<uint16_t, uint32_t>(execution_tier, kExprS128Load16x4U);
}

WASM_SIMD_TEST(S128Load16x4S) {
  RunLoadExtendTest<int16_t, int32_t>(execution_tier, kExprS128Load16x4S);
}

WASM_SIMD_TEST(S128Load32x2U) {
  RunLoadExtendTest<uint32_t, uint64_t>(execution_tier, kExprS128Load32x2U);
}

WASM_SIMD_TEST(S128Load32x2S) {
  RunLoadExtendTest<int32_t, int64_t>(execution_tier, kExprS128Load32x2S);
}

template <typename S>
void RunLoadZeroTest(TestExecutionTier execution_tier, WasmOpcode op) {
  constexpr int lanes_s = kSimd128Size / sizeof(S);
  constexpr int mem_index = 16;  // Load from mem index 16 (bytes).
  constexpr S sentinel = S{-1};
  S* memory;
  S* global;

  auto initialize_builder = [=](WasmRunner<int32_t>* r) -> std::tuple<S*, S*> {
    S* memory = r->builder().AddMemoryElems<S>(kWasmPageSize / sizeof(S));
    S* global = r->builder().AddGlobal<S>(kWasmS128);
    r->builder().RandomizeMemory();
    r->builder().WriteMemory(&memory[lanes_s], sentinel);
    return std::make_tuple(memory, global);
  };

  // Check all supported alignments.
  constexpr int max_alignment = base::bits::CountTrailingZeros(sizeof(S));
  for (byte alignment = 0; alignment <= max_alignment; alignment++) {
    WasmRunner<int32_t> r(execution_tier);
    std::tie(memory, global) = initialize_builder(&r);

    BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP(op, WASM_I32V(mem_index))),
          WASM_ONE);
    r.Call();

    // Only first lane is set to sentinel.
    CHECK_EQ(sentinel, ReadLittleEndianValue<S>(&global[0]));
    // The other lanes are zero.
    for (int i = 1; i < lanes_s; i++) {
      CHECK_EQ(S{0}, ReadLittleEndianValue<S>(&global[i]));
    }
  }

  {
    // Use memarg to specific offset.
    WasmRunner<int32_t> r(execution_tier);
    std::tie(memory, global) = initialize_builder(&r);

    BUILD(
        r,
        WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP_OFFSET(op, WASM_ZERO, mem_index)),
        WASM_ONE);
    r.Call();

    // Only first lane is set to sentinel.
    CHECK_EQ(sentinel, ReadLittleEndianValue<S>(&global[0]));
    // The other lanes are zero.
    for (int i = 1; i < lanes_s; i++) {
      CHECK_EQ(S{0}, ReadLittleEndianValue<S>(&global[i]));
    }
  }

  // Test for OOB.
  {
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<S>(kWasmPageSize / sizeof(S));
    r.builder().AddGlobal<S>(kWasmS128);

    BUILD(r, WASM_GLOBAL_SET(0, WASM_SIMD_LOAD_OP(op, WASM_LOCAL_GET(0))),
          WASM_ONE);

    // Load extends load sizeof(S) bytes.
    for (uint32_t offset = kWasmPageSize - (sizeof(S) - 1);
         offset < kWasmPageSize; ++offset) {
      CHECK_TRAP(r.Call(offset));
    }
  }
}

WASM_SIMD_TEST(S128Load32Zero) {
  RunLoadZeroTest<int32_t>(execution_tier, kExprS128Load32Zero);
}

WASM_SIMD_TEST(S128Load64Zero) {
  RunLoadZeroTest<int64_t>(execution_tier, kExprS128Load64Zero);
}

template <typename T>
void RunLoadLaneTest(TestExecutionTier execution_tier, WasmOpcode load_op,
                     WasmOpcode splat_op) {
  WasmOpcode const_op =
      splat_op == kExprI64x2Splat ? kExprI64Const : kExprI32Const;

  constexpr int lanes_s = kSimd128Size / sizeof(T);
  constexpr int mem_index = 16;  // Load from mem index 16 (bytes).
  constexpr int splat_value = 33;
  T sentinel = T{-1};

  T* memory;
  T* global;

  auto build_fn = [=, &memory, &global](WasmRunner<int32_t>& r, int mem_index,
                                        int lane, int alignment, int offset) {
    memory = r.builder().AddMemoryElems<T>(kWasmPageSize / sizeof(T));
    global = r.builder().AddGlobal<T>(kWasmS128);
    r.builder().WriteMemory(&memory[lanes_s], sentinel);
    // Splat splat_value, then only load and replace a single lane with the
    // sentinel value.
    BUILD(r, WASM_I32V(mem_index), const_op, splat_value,
          WASM_SIMD_OP(splat_op), WASM_SIMD_OP(load_op), alignment, offset,
          lane, kExprGlobalSet, 0, WASM_ONE);
  };

  auto check_results = [=](T* global, int sentinel_lane = 0) {
    // Only one lane is loaded, the rest of the lanes are unchanged.
    for (int i = 0; i < lanes_s; i++) {
      T expected = i == sentinel_lane ? sentinel : static_cast<T>(splat_value);
      CHECK_EQ(expected, ReadLittleEndianValue<T>(&global[i]));
    }
  };

  for (int lane_index = 0; lane_index < lanes_s; ++lane_index) {
    WasmRunner<int32_t> r(execution_tier);
    build_fn(r, mem_index, lane_index, /*alignment=*/0, /*offset=*/0);
    r.Call();
    check_results(global, lane_index);
  }

  // Check all possible alignments.
  constexpr int max_alignment = base::bits::CountTrailingZeros(sizeof(T));
  for (byte alignment = 0; alignment <= max_alignment; ++alignment) {
    WasmRunner<int32_t> r(execution_tier);
    build_fn(r, mem_index, /*lane=*/0, alignment, /*offset=*/0);
    r.Call();
    check_results(global);
  }

  {
    // Use memarg to specify offset.
    int lane_index = 0;
    WasmRunner<int32_t> r(execution_tier);
    build_fn(r, /*mem_index=*/0, /*lane=*/0, /*alignment=*/0,
             /*offset=*/mem_index);
    r.Call();
    check_results(global, lane_index);
  }

  // Test for OOB.
  {
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<T>(kWasmPageSize / sizeof(T));
    r.builder().AddGlobal<T>(kWasmS128);

    BUILD(r, WASM_LOCAL_GET(0), const_op, splat_value, WASM_SIMD_OP(splat_op),
          WASM_SIMD_OP(load_op), ZERO_ALIGNMENT, ZERO_OFFSET, 0, kExprGlobalSet,
          0, WASM_ONE);

    // Load lane load sizeof(T) bytes.
    for (uint32_t index = kWasmPageSize - (sizeof(T) - 1);
         index < kWasmPageSize; ++index) {
      CHECK_TRAP(r.Call(index));
    }
  }
}

WASM_SIMD_TEST(S128Load8Lane) {
  RunLoadLaneTest<int8_t>(execution_tier, kExprS128Load8Lane, kExprI8x16Splat);
}

WASM_SIMD_TEST(S128Load16Lane) {
  RunLoadLaneTest<int16_t>(execution_tier, kExprS128Load16Lane,
                           kExprI16x8Splat);
}

WASM_SIMD_TEST(S128Load32Lane) {
  RunLoadLaneTest<int32_t>(execution_tier, kExprS128Load32Lane,
                           kExprI32x4Splat);
}

WASM_SIMD_TEST(S128Load64Lane) {
  RunLoadLaneTest<int64_t>(execution_tier, kExprS128Load64Lane,
                           kExprI64x2Splat);
}

template <typename T>
void RunStoreLaneTest(TestExecutionTier execution_tier, WasmOpcode store_op,
                      WasmOpcode splat_op) {
  constexpr int lanes = kSimd128Size / sizeof(T);
  constexpr int mem_index = 16;  // Store to mem index 16 (bytes).
  constexpr int splat_value = 33;
  WasmOpcode const_op =
      splat_op == kExprI64x2Splat ? kExprI64Const : kExprI32Const;

  T* memory;  // Will be set by build_fn.

  auto build_fn = [=, &memory](WasmRunner<int32_t>& r, int mem_index,
                               int lane_index, int alignment, int offset) {
    memory = r.builder().AddMemoryElems<T>(kWasmPageSize / sizeof(T));
    // Splat splat_value, then only Store and replace a single lane.
    BUILD(r, WASM_I32V(mem_index), const_op, splat_value,
          WASM_SIMD_OP(splat_op), WASM_SIMD_OP(store_op), alignment, offset,
          lane_index, WASM_ONE);
    r.builder().BlankMemory();
  };

  auto check_results = [=](WasmRunner<int32_t>& r, T* memory) {
    for (int i = 0; i < lanes; i++) {
      CHECK_EQ(0, r.builder().ReadMemory(&memory[i]));
    }

    CHECK_EQ(splat_value, r.builder().ReadMemory(&memory[lanes]));

    for (int i = lanes + 1; i < lanes * 2; i++) {
      CHECK_EQ(0, r.builder().ReadMemory(&memory[i]));
    }
  };

  for (int lane_index = 0; lane_index < lanes; lane_index++) {
    WasmRunner<int32_t> r(execution_tier);
    build_fn(r, mem_index, lane_index, ZERO_ALIGNMENT, ZERO_OFFSET);
    r.Call();
    check_results(r, memory);
  }

  // Check all possible alignments.
  constexpr int max_alignment = base::bits::CountTrailingZeros(sizeof(T));
  for (byte alignment = 0; alignment <= max_alignment; ++alignment) {
    WasmRunner<int32_t> r(execution_tier);
    build_fn(r, mem_index, /*lane_index=*/0, alignment, ZERO_OFFSET);
    r.Call();
    check_results(r, memory);
  }

  {
    // Use memarg for offset.
    WasmRunner<int32_t> r(execution_tier);
    build_fn(r, /*mem_index=*/0, /*lane_index=*/0, ZERO_ALIGNMENT, mem_index);
    r.Call();
    check_results(r, memory);
  }

  // OOB stores
  {
    WasmRunner<int32_t, uint32_t> r(execution_tier);
    r.builder().AddMemoryElems<T>(kWasmPageSize / sizeof(T));

    BUILD(r, WASM_LOCAL_GET(0), const_op, splat_value, WASM_SIMD_OP(splat_op),
          WASM_SIMD_OP(store_op), ZERO_ALIGNMENT, ZERO_OFFSET, 0, WASM_ONE);

    // StoreLane stores sizeof(T) bytes.
    for (uint32_t index = kWasmPageSize - (sizeof(T) - 1);
         index < kWasmPageSize; ++index) {
      CHECK_TRAP(r.Call(index));
    }
  }
}

WASM_SIMD_TEST(S128Store8Lane) {
  RunStoreLaneTest<int8_t>(execution_tier, kExprS128Store8Lane,
                           kExprI8x16Splat);
}

WASM_SIMD_TEST(S128Store16Lane) {
  RunStoreLaneTest<int16_t>(execution_tier, kExprS128Store16Lane,
                            kExprI16x8Splat);
}

WASM_SIMD_TEST(S128Store32Lane) {
  RunStoreLaneTest<int32_t>(execution_tier, kExprS128Store32Lane,
                            kExprI32x4Splat);
}

WASM_SIMD_TEST(S128Store64Lane) {
  RunStoreLaneTest<int64_t>(execution_tier, kExprS128Store64Lane,
                            kExprI64x2Splat);
}

#define WASM_SIMD_ANYTRUE_TEST(format, lanes, max, param_type)                \
  WASM_SIMD_TEST(S##format##AnyTrue) {                                        \
    WasmRunner<int32_t, param_type> r(execution_tier);                        \
    if (lanes == 2) return;                                                   \
    byte simd = r.AllocateLocal(kWasmS128);                                   \
    BUILD(                                                                    \
        r,                                                                    \
        WASM_LOCAL_SET(simd, WASM_SIMD_I##format##_SPLAT(WASM_LOCAL_GET(0))), \
        WASM_SIMD_UNOP(kExprV128AnyTrue, WASM_LOCAL_GET(simd)));              \
    CHECK_EQ(1, r.Call(max));                                                 \
    CHECK_EQ(1, r.Call(5));                                                   \
    CHECK_EQ(0, r.Call(0));                                                   \
  }
WASM_SIMD_ANYTRUE_TEST(32x4, 4, 0xffffffff, int32_t)
WASM_SIMD_ANYTRUE_TEST(16x8, 8, 0xffff, int32_t)
WASM_SIMD_ANYTRUE_TEST(8x16, 16, 0xff, int32_t)

// Special any true test cases that splats a -0.0 double into a i64x2.
// This is specifically to ensure that our implementation correct handles that
// 0.0 and -0.0 will be different in an anytrue (IEEE753 says they are equals).
WASM_SIMD_TEST(V128AnytrueWithNegativeZero) {
  WasmRunner<int32_t, int64_t> r(execution_tier);
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(simd, WASM_SIMD_I64x2_SPLAT(WASM_LOCAL_GET(0))),
        WASM_SIMD_UNOP(kExprV128AnyTrue, WASM_LOCAL_GET(simd)));
  CHECK_EQ(1, r.Call(0x8000000000000000));
  CHECK_EQ(0, r.Call(0x0000000000000000));
}

#define WASM_SIMD_ALLTRUE_TEST(format, lanes, max, param_type)                \
  WASM_SIMD_TEST(I##format##AllTrue) {                                        \
    WasmRunner<int32_t, param_type> r(execution_tier);                        \
    if (lanes == 2) return;                                                   \
    byte simd = r.AllocateLocal(kWasmS128);                                   \
    BUILD(                                                                    \
        r,                                                                    \
        WASM_LOCAL_SET(simd, WASM_SIMD_I##format##_SPLAT(WASM_LOCAL_GET(0))), \
        WASM_SIMD_UNOP(kExprI##format##AllTrue, WASM_LOCAL_GET(simd)));       \
    CHECK_EQ(1, r.Call(max));                                                 \
    CHECK_EQ(1, r.Call(0x1));                                                 \
    CHECK_EQ(0, r.Call(0));                                                   \
  }
WASM_SIMD_ALLTRUE_TEST(64x2, 2, 0xffffffffffffffff, int64_t)
WASM_SIMD_ALLTRUE_TEST(32x4, 4, 0xffffffff, int32_t)
WASM_SIMD_ALLTRUE_TEST(16x8, 8, 0xffff, int32_t)
WASM_SIMD_ALLTRUE_TEST(8x16, 16, 0xff, int32_t)

WASM_SIMD_TEST(BitSelect) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_LOCAL_SET(
            simd,
            WASM_SIMD_SELECT(32x4, WASM_SIMD_I32x4_SPLAT(WASM_I32V(0x01020304)),
                             WASM_SIMD_I32x4_SPLAT(WASM_I32V(0)),
                             WASM_SIMD_I32x4_SPLAT(WASM_LOCAL_GET(0)))),
        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_LOCAL_GET(simd)));
  CHECK_EQ(0x01020304, r.Call(0xFFFFFFFF));
}

void RunSimdConstTest(TestExecutionTier execution_tier,
                      const std::array<uint8_t, kSimd128Size>& expected) {
  WasmRunner<uint32_t> r(execution_tier);
  byte temp1 = r.AllocateLocal(kWasmS128);
  uint8_t* src0 = r.builder().AddGlobal<uint8_t>(kWasmS128);
  BUILD(r, WASM_GLOBAL_SET(temp1, WASM_SIMD_CONSTANT(expected)), WASM_ONE);
  CHECK_EQ(1, r.Call());
  for (size_t i = 0; i < expected.size(); i++) {
    CHECK_EQ(ReadLittleEndianValue<uint8_t>(&src0[i]), expected[i]);
  }
}

WASM_SIMD_TEST(S128Const) {
  std::array<uint8_t, kSimd128Size> expected;
  // Test for generic constant
  for (int i = 0; i < kSimd128Size; i++) {
    expected[i] = i;
  }
  RunSimdConstTest(execution_tier, expected);

  // Keep the first 4 lanes as 0, set the remaining ones.
  for (int i = 0; i < 4; i++) {
    expected[i] = 0;
  }
  for (int i = 4; i < kSimd128Size; i++) {
    expected[i] = i;
  }
  RunSimdConstTest(execution_tier, expected);

  // Check sign extension logic used to pack int32s into int64.
  expected = {0};
  // Set the top bit of lane 3 (top bit of first int32), the rest can be 0.
  expected[3] = 0x80;
  RunSimdConstTest(execution_tier, expected);
}

WASM_SIMD_TEST(S128ConstAllZero) {
  std::array<uint8_t, kSimd128Size> expected = {0};
  RunSimdConstTest(execution_tier, expected);
}

WASM_SIMD_TEST(S128ConstAllOnes) {
  std::array<uint8_t, kSimd128Size> expected;
  // Test for generic constant
  for (int i = 0; i < kSimd128Size; i++) {
    expected[i] = 0xff;
  }
  RunSimdConstTest(execution_tier, expected);
}

WASM_SIMD_TEST(I8x16LeUMixed) {
  RunI8x16MixedRelationalOpTest(execution_tier, kExprI8x16LeU,
                                UnsignedLessEqual);
}
WASM_SIMD_TEST(I8x16LtUMixed) {
  RunI8x16MixedRelationalOpTest(execution_tier, kExprI8x16LtU, UnsignedLess);
}
WASM_SIMD_TEST(I8x16GeUMixed) {
  RunI8x16MixedRelationalOpTest(execution_tier, kExprI8x16GeU,
                                UnsignedGreaterEqual);
}
WASM_SIMD_TEST(I8x16GtUMixed) {
  RunI8x16MixedRelationalOpTest(execution_tier, kExprI8x16GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I16x8LeUMixed) {
  RunI16x8MixedRelationalOpTest(execution_tier, kExprI16x8LeU,
                                UnsignedLessEqual);
}
WASM_SIMD_TEST(I16x8LtUMixed) {
  RunI16x8MixedRelationalOpTest(execution_tier, kExprI16x8LtU, UnsignedLess);
}
WASM_SIMD_TEST(I16x8GeUMixed) {
  RunI16x8MixedRelationalOpTest(execution_tier, kExprI16x8GeU,
                                UnsignedGreaterEqual);
}
WASM_SIMD_TEST(I16x8GtUMixed) {
  RunI16x8MixedRelationalOpTest(execution_tier, kExprI16x8GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I16x8ExtractLaneU_I8x16Splat) {
  // Test that we are correctly signed/unsigned extending when extracting.
  WasmRunner<int32_t, int32_t> r(execution_tier);
  byte simd_val = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_LOCAL_SET(simd_val, WASM_SIMD_I8x16_SPLAT(WASM_LOCAL_GET(0))),
        WASM_SIMD_I16x8_EXTRACT_LANE_U(0, WASM_LOCAL_GET(simd_val)));
  CHECK_EQ(0xfafa, r.Call(0xfa));
}

#define WASM_EXTRACT_I16x8_TEST(Sign, Type)                                    \
  WASM_SIMD_TEST(I16X8ExtractLane##Sign) {                                     \
    WasmRunner<int32_t, int32_t> r(execution_tier);                            \
    byte int_val = r.AllocateLocal(kWasmI32);                                  \
    byte simd_val = r.AllocateLocal(kWasmS128);                                \
    BUILD(r,                                                                   \
          WASM_LOCAL_SET(simd_val,                                             \
                         WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(int_val))),      \
          WASM_SIMD_CHECK_LANE_U(I16x8, simd_val, I32, int_val, 0),            \
          WASM_SIMD_CHECK_LANE_U(I16x8, simd_val, I32, int_val, 2),            \
          WASM_SIMD_CHECK_LANE_U(I16x8, simd_val, I32, int_val, 4),            \
          WASM_SIMD_CHECK_LANE_U(I16x8, simd_val, I32, int_val, 6), WASM_ONE); \
    FOR_##Type##_INPUTS(x) { CHECK_EQ(1, r.Call(x)); }                         \
  }
WASM_EXTRACT_I16x8_TEST(S, UINT16) WASM_EXTRACT_I16x8_TEST(I, INT16)
#undef WASM_EXTRACT_I16x8_TEST

#define WASM_EXTRACT_I8x16_TEST(Sign, Type)                               \
  WASM_SIMD_TEST(I8x16ExtractLane##Sign) {                                \
    WasmRunner<int32_t, int32_t> r(execution_tier);                       \
    byte int_val = r.AllocateLocal(kWasmI32);                             \
    byte simd_val = r.AllocateLocal(kWasmS128);                           \
    BUILD(r,                                                              \
          WASM_LOCAL_SET(simd_val,                                        \
                         WASM_SIMD_I8x16_SPLAT(WASM_LOCAL_GET(int_val))), \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 1),       \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 3),       \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 5),       \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 7),       \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 9),       \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 10),      \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 11),      \
          WASM_SIMD_CHECK_LANE_U(I8x16, simd_val, I32, int_val, 13),      \
          WASM_ONE);                                                      \
    FOR_##Type##_INPUTS(x) { CHECK_EQ(1, r.Call(x)); }                    \
  }
    WASM_EXTRACT_I8x16_TEST(S, UINT8) WASM_EXTRACT_I8x16_TEST(I, INT8)
#undef WASM_EXTRACT_I8x16_TEST

#undef WASM_SIMD_TEST
#undef WASM_SIMD_CHECK_LANE_S
#undef WASM_SIMD_CHECK_LANE_U
#undef TO_BYTE
#undef WASM_SIMD_OP
#undef WASM_SIMD_SPLAT
#undef WASM_SIMD_UNOP
#undef WASM_SIMD_BINOP
#undef WASM_SIMD_SHIFT_OP
#undef WASM_SIMD_CONCAT_OP
#undef WASM_SIMD_SELECT
#undef WASM_SIMD_F64x2_SPLAT
#undef WASM_SIMD_F64x2_EXTRACT_LANE
#undef WASM_SIMD_F64x2_REPLACE_LANE
#undef WASM_SIMD_F32x4_SPLAT
#undef WASM_SIMD_F32x4_EXTRACT_LANE
#undef WASM_SIMD_F32x4_REPLACE_LANE
#undef WASM_SIMD_I64x2_SPLAT
#undef WASM_SIMD_I64x2_EXTRACT_LANE
#undef WASM_SIMD_I64x2_REPLACE_LANE
#undef WASM_SIMD_I32x4_SPLAT
#undef WASM_SIMD_I32x4_EXTRACT_LANE
#undef WASM_SIMD_I32x4_REPLACE_LANE
#undef WASM_SIMD_I16x8_SPLAT
#undef WASM_SIMD_I16x8_EXTRACT_LANE
#undef WASM_SIMD_I16x8_EXTRACT_LANE_U
#undef WASM_SIMD_I16x8_REPLACE_LANE
#undef WASM_SIMD_I8x16_SPLAT
#undef WASM_SIMD_I8x16_EXTRACT_LANE
#undef WASM_SIMD_I8x16_EXTRACT_LANE_U
#undef WASM_SIMD_I8x16_REPLACE_LANE
#undef WASM_SIMD_I8x16_SHUFFLE_OP
#undef WASM_SIMD_LOAD_MEM
#undef WASM_SIMD_LOAD_MEM_OFFSET
#undef WASM_SIMD_STORE_MEM
#undef WASM_SIMD_STORE_MEM_OFFSET
#undef WASM_SIMD_SELECT_TEST
#undef WASM_SIMD_NON_CANONICAL_SELECT_TEST
#undef WASM_SIMD_BOOL_REDUCTION_TEST
#undef WASM_SIMD_TEST
#undef WASM_SIMD_ANYTRUE_TEST
#undef WASM_SIMD_ALLTRUE_TEST
#undef WASM_SIMD_F64x2_QFMA
#undef WASM_SIMD_F64x2_QFMS
#undef WASM_SIMD_F32x4_QFMA
#undef WASM_SIMD_F32x4_QFMS
#undef WASM_SIMD_LOAD_OP
#undef WASM_SIMD_LOAD_OP_OFFSET
#undef WASM_SIMD_LOAD_OP_ALIGNMENT

}  // namespace test_run_wasm_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
