// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/overflowing-math.h"
#include "src/codegen/assembler-inl.h"
#include "src/numbers/conversions.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-runner.h"
#include "test/cctest/wasm/wasm-simd-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "third_party/fp16/src/include/fp16.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_f16 {

WASM_EXEC_TEST(F16Load) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<float> r(execution_tier);
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(4);
  r.Build({WASM_F16_LOAD_MEM(WASM_I32V_1(4))});
  r.builder().WriteMemory(&memory[2], fp16_ieee_from_fp32_value(2.75));
  CHECK_EQ(2.75f, r.Call());
}

WASM_EXEC_TEST(F16Store) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t> r(execution_tier);
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(4);
  r.Build({WASM_F16_STORE_MEM(WASM_I32V_1(4), WASM_F32(2.75)), WASM_ZERO});
  r.Call();
  CHECK_EQ(r.builder().ReadMemory(&memory[2]), fp16_ieee_from_fp32_value(2.75));
}

WASM_EXEC_TEST(F16x8Splat) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, float> r(execution_tier);
  // Set up a global to hold output vector.
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  uint8_t param1 = 0;
  r.Build({WASM_GLOBAL_SET(0, WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(param1))),
           WASM_ONE});

  FOR_FLOAT32_INPUTS(x) {
    r.Call(x);
    uint16_t expected = fp16_ieee_from_fp32_value(x);
    Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
    for (int i = 0; i < 8; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      if (std::isnan(x)) {
        CHECK(isnan(actual_lane));
      } else {
        CHECK_EQ(actual_lane, expected);
      }
    }
  }
}

WASM_EXEC_TEST(F16x8ReplaceLane) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t> r(execution_tier);
  // Set up a global to hold output vector.
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  // Build function to replace each lane with its (FP) index.
  r.Build({WASM_SIMD_F16x8_SPLAT(WASM_F32(3.14159f)),
           WASM_F32(0.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           0,
           WASM_F32(1.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           1,
           WASM_F32(2.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           2,
           WASM_F32(3.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           3,
           WASM_F32(4.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           4,
           WASM_F32(5.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           5,
           WASM_F32(6.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           6,
           WASM_F32(7.0f),
           WASM_SIMD_OP(kExprF16x8ReplaceLane),
           7,
           kExprGlobalSet,
           0,
           WASM_ONE});

  r.Call();
  Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
  for (int i = 0; i < 8; i++) {
    CHECK_EQ(fp16_ieee_from_fp32_value(static_cast<float>(i)),
             LANE(actual.to_i16x8(), i));
  }
}

WASM_EXEC_TEST(F16x8ExtractLane) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t> r(execution_tier);
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  const WasmGlobal* globals[8];
  std::array<int16_t, 8> g_val;
  for (int i = 0; i < 8; i++) {
    LANE(g_val, i) =
        static_cast<int16_t>(fp16_ieee_from_fp32_value(static_cast<float>(i)));
    globals[i] = r.builder().AddGlobal(kWasmF32);
  }
  r.builder().WriteGlobal(*g, WasmValue(Simd128(g_val)));

  r.Build(
      {WASM_GLOBAL_SET(1, WASM_SIMD_F16x8_EXTRACT_LANE(0, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(2, WASM_SIMD_F16x8_EXTRACT_LANE(1, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(3, WASM_SIMD_F16x8_EXTRACT_LANE(2, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(4, WASM_SIMD_F16x8_EXTRACT_LANE(3, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(5, WASM_SIMD_F16x8_EXTRACT_LANE(4, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(6, WASM_SIMD_F16x8_EXTRACT_LANE(5, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(7, WASM_SIMD_F16x8_EXTRACT_LANE(6, WASM_GLOBAL_GET(0))),
       WASM_GLOBAL_SET(8, WASM_SIMD_F16x8_EXTRACT_LANE(7, WASM_GLOBAL_GET(0))),
       WASM_ONE});

  r.Call();
  for (int i = 0; i < 8; i++) {
    CHECK_EQ(r.builder().ReadGlobal(*globals[i]).to_f32(), i);
  }
}

#define UN_OP_LIST(V) \
  V(Abs, std::abs)    \
  V(Neg, -)           \
  V(Sqrt, std::sqrt)  \
  V(Ceil, ceilf)      \
  V(Floor, floorf)    \
  V(Trunc, truncf)    \
  V(NearestInt, nearbyintf)

#define TEST_UN_OP(WasmName, COp)                                          \
  uint16_t WasmName##F16(uint16_t a) {                                     \
    return fp16_ieee_from_fp32_value(COp(fp16_ieee_to_fp32_value(a)));     \
  }                                                                        \
  WASM_EXEC_TEST(F16x8##WasmName) {                                        \
    i::v8_flags.experimental_wasm_fp16 = true;                             \
    RunF16x8UnOpTest(execution_tier, kExprF16x8##WasmName, WasmName##F16); \
  }

UN_OP_LIST(TEST_UN_OP)

#undef TEST_UN_OP
#undef UN_OP_LIST

#define CMP_OP_LIST(V) \
  V(Eq, ==)            \
  V(Ne, !=)            \
  V(Gt, >)             \
  V(Ge, >=)            \
  V(Lt, <)             \
  V(Le, <=)

#define TEST_CMP_OP(WasmName, COp)                                             \
  int16_t WasmName(uint16_t a, uint16_t b) {                                   \
    return fp16_ieee_to_fp32_value(a) COp fp16_ieee_to_fp32_value(b) ? -1 : 0; \
  }                                                                            \
  WASM_EXEC_TEST(F16x8##WasmName) {                                            \
    i::v8_flags.experimental_wasm_fp16 = true;                                 \
    RunF16x8CompareOpTest(execution_tier, kExprF16x8##WasmName, WasmName);     \
  }

CMP_OP_LIST(TEST_CMP_OP)

#undef TEST_CMP_OP
#undef UN_CMP_LIST

float Add(float a, float b) { return a + b; }
float Sub(float a, float b) { return a - b; }
float Mul(float a, float b) { return a * b; }

#define BIN_OP_LIST(V) \
  V(Add, Add)          \
  V(Sub, Sub)          \
  V(Mul, Mul)          \
  V(Div, base::Divide) \
  V(Min, JSMin)        \
  V(Max, JSMax)        \
  V(Pmin, Minimum)     \
  V(Pmax, Maximum)

#define TEST_BIN_OP(WasmName, COp)                                          \
  uint16_t WasmName##F16(uint16_t a, uint16_t b) {                          \
    return fp16_ieee_from_fp32_value(                                       \
        COp(fp16_ieee_to_fp32_value(a), fp16_ieee_to_fp32_value(b)));       \
  }                                                                         \
  WASM_EXEC_TEST(F16x8##WasmName) {                                         \
    i::v8_flags.experimental_wasm_fp16 = true;                              \
    RunF16x8BinOpTest(execution_tier, kExprF16x8##WasmName, WasmName##F16); \
  }

BIN_OP_LIST(TEST_BIN_OP)

#undef TEST_BIN_OP
#undef BIN_OP_LIST

WASM_EXEC_TEST(F16x8ConvertI16x8) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, int32_t> r(execution_tier);
  // Create two output vectors to hold signed and unsigned results.
  const WasmGlobal* g0 = r.builder().AddGlobal(kWasmS128);
  const WasmGlobal* g1 = r.builder().AddGlobal(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  uint8_t value = 0;
  uint8_t temp1 = r.AllocateLocal(kWasmS128);
  r.Build({WASM_LOCAL_SET(temp1, WASM_SIMD_I16x8_SPLAT(WASM_LOCAL_GET(value))),
           WASM_GLOBAL_SET(0, WASM_SIMD_UNOP(kExprF16x8SConvertI16x8,
                                             WASM_LOCAL_GET(temp1))),
           WASM_GLOBAL_SET(1, WASM_SIMD_UNOP(kExprF16x8UConvertI16x8,
                                             WASM_LOCAL_GET(temp1))),
           WASM_ONE});

  FOR_INT16_INPUTS(x) {
    r.Call(x);
    uint16_t expected_signed = fp16_ieee_from_fp32_value(static_cast<float>(x));
    uint16_t expected_unsigned =
        fp16_ieee_from_fp32_value(static_cast<float>(static_cast<uint16_t>(x)));
    Simd128 actual0 = r.builder().ReadGlobal(*g0).to_s128();
    Simd128 actual1 = r.builder().ReadGlobal(*g1).to_s128();
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(expected_signed, ULANE(actual0.to_i16x8(), i));
      CHECK_EQ(expected_unsigned, LANE(actual1.to_i16x8(), i));
    }
  }
}

int16_t ConvertToInt(uint16_t f16, bool unsigned_result) {
  float f32 = fp16_ieee_to_fp32_value(f16);
  if (std::isnan(f32)) return 0;
  if (unsigned_result) {
    if (f32 > float{kMaxUInt16}) return static_cast<uint16_t>(kMaxUInt16);
    if (f32 < 0) return 0;
    return static_cast<uint16_t>(f32);
  } else {
    if (f32 > float{kMaxInt16}) return static_cast<int16_t>(kMaxInt16);
    if (f32 < float{kMinInt16}) return static_cast<int16_t>(kMinInt16);
    return static_cast<int16_t>(f32);
  }
}

// Tests both signed and unsigned conversion.
WASM_EXEC_TEST(I16x8ConvertF16x8) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, float> r(execution_tier);
  // Create two output vectors to hold signed and unsigned results.
  const WasmGlobal* g0 = r.builder().AddGlobal(kWasmS128);
  const WasmGlobal* g1 = r.builder().AddGlobal(kWasmS128);
  // Build fn to splat test value, perform conversions, and write the results.
  uint8_t value = 0;
  uint8_t temp1 = r.AllocateLocal(kWasmS128);
  r.Build({WASM_LOCAL_SET(temp1, WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value))),
           WASM_GLOBAL_SET(0, WASM_SIMD_UNOP(kExprI16x8SConvertF16x8,
                                             WASM_LOCAL_GET(temp1))),
           WASM_GLOBAL_SET(1, WASM_SIMD_UNOP(kExprI16x8UConvertF16x8,
                                             WASM_LOCAL_GET(temp1))),
           WASM_ONE});

  FOR_FLOAT32_INPUTS(x) {
    if (!PlatformCanRepresent(x)) continue;
    r.Call(x);
    int16_t expected_signed = ConvertToInt(fp16_ieee_from_fp32_value(x), false);
    uint16_t expected_unsigned =
        ConvertToInt(fp16_ieee_from_fp32_value(x), true);
    Simd128 actual0 = r.builder().ReadGlobal(*g0).to_s128();
    Simd128 actual1 = r.builder().ReadGlobal(*g1).to_s128();
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(expected_signed, LANE(actual0.to_i16x8(), i));
      CHECK_EQ(expected_unsigned, ULANE(actual1.to_i16x8(), i));
    }
  }
}

WASM_EXEC_TEST(F16x8DemoteF32x4Zero) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, float> r(execution_tier);
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  r.Build({WASM_GLOBAL_SET(
               0, WASM_SIMD_UNOP(kExprF16x8DemoteF32x4Zero,
                                 WASM_SIMD_F32x4_SPLAT(WASM_LOCAL_GET(0)))),
           WASM_ONE});

  FOR_FLOAT32_INPUTS(x) {
    r.Call(x);
    uint16_t expected = fp16_ieee_from_fp32_value(x);
    Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
    for (int i = 0; i < 4; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      CheckFloat16LaneResult(x, x, expected, actual_lane, true);
    }
    for (int i = 4; i < 8; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      CheckFloat16LaneResult(x, x, 0, actual_lane, true);
    }
  }
}

WASM_EXEC_TEST(F16x8DemoteF64x2Zero) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, double> r(execution_tier);
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  r.Build({WASM_GLOBAL_SET(
               0, WASM_SIMD_UNOP(kExprF16x8DemoteF64x2Zero,
                                 WASM_SIMD_F64x2_SPLAT(WASM_LOCAL_GET(0)))),
           WASM_ONE});

  FOR_FLOAT64_INPUTS(x) {
    r.Call(x);
    uint16_t expected = DoubleToFloat16(x);
    Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
    for (int i = 0; i < 2; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      CheckFloat16LaneResult(x, x, expected, actual_lane, true);
    }
    for (int i = 2; i < 8; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      CheckFloat16LaneResult(x, x, 0, actual_lane, true);
    }
  }
}

WASM_EXEC_TEST(F32x4PromoteLowF16x8) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, float> r(execution_tier);
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  r.Build({WASM_GLOBAL_SET(
               0, WASM_SIMD_UNOP(kExprF32x4PromoteLowF16x8,
                                 WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(0)))),
           WASM_ONE});

  FOR_FLOAT32_INPUTS(x) {
    r.Call(x);
    float expected = fp16_ieee_to_fp32_value(fp16_ieee_from_fp32_value(x));
    Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
    for (int i = 0; i < 4; i++) {
      float actual_lane = LANE(actual.to_f32x4(), i);
      CheckFloatResult(x, x, expected, actual_lane, true);
    }
  }
}

struct FMOperation {
  const float a;
  const float b;
  const float c;
  const float fused_result;
};

constexpr float large_n = 1e4;
constexpr float finf = std::numeric_limits<float>::infinity();
constexpr float qNan = std::numeric_limits<float>::quiet_NaN();

// Fused Multiply-Add performs a * b + c.
static FMOperation qfma_array[] = {
    {2.0f, 3.0f, 1.0f, 7.0f},
    // fused: a * b + c = (positive overflow) + -inf = -inf
    // unfused: a * b + c = inf + -inf = NaN
    {large_n, large_n, -finf, -finf},
    // fused: a * b + c = (negative overflow) + inf = inf
    // unfused: a * b + c = -inf + inf = NaN
    {-large_n, large_n, finf, finf},
    // NaN
    {2.0f, 3.0f, qNan, qNan},
    // -NaN
    {2.0f, 3.0f, -qNan, qNan}};

base::Vector<const FMOperation> qfma_vector() {
  return base::ArrayVector(qfma_array);
}

// Fused Multiply-Subtract performs -(a * b) + c.
static FMOperation qfms_array[]{
    {2.0f, 3.0f, 1.0f, -5.0f},
    // fused: -(a * b) + c = - (positive overflow) + inf = inf
    // unfused: -(a * b) + c = - inf + inf = NaN
    {large_n, large_n, finf, finf},
    // fused: -(a * b) + c = (negative overflow) + -inf = -inf
    // unfused: -(a * b) + c = -inf - -inf = NaN
    {-large_n, large_n, -finf, -finf},
    // NaN
    {2.0f, 3.0f, qNan, qNan},
    // -NaN
    {2.0f, 3.0f, -qNan, qNan}};

base::Vector<const FMOperation> qfms_vector() {
  return base::ArrayVector(qfms_array);
}

WASM_EXEC_TEST(F16x8Qfma) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, float, float, float> r(execution_tier);
  // Set up global to hold output.
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  uint8_t value1 = 0, value2 = 1, value3 = 2;
  r.Build(
      {WASM_GLOBAL_SET(0, WASM_SIMD_F16x8_QFMA(
                              WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value1)),
                              WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value2)),
                              WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value3)))),
       WASM_ONE});
  for (FMOperation x : qfma_vector()) {
    r.Call(x.a, x.b, x.c);
    uint16_t expected = fp16_ieee_from_fp32_value(x.fused_result);
    Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
    for (int i = 0; i < 8; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      CheckFloat16LaneResult(x.a, x.b, x.c, expected, actual_lane,
                             true /* exact */);
    }
  }
}

WASM_EXEC_TEST(F16x8Qfms) {
  i::v8_flags.experimental_wasm_fp16 = true;
  WasmRunner<int32_t, float, float, float> r(execution_tier);
  // Set up global to hold output.
  const WasmGlobal* g = r.builder().AddGlobal(kWasmS128);
  uint8_t value1 = 0, value2 = 1, value3 = 2;
  r.Build(
      {WASM_GLOBAL_SET(0, WASM_SIMD_F16x8_QFMS(
                              WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value1)),
                              WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value2)),
                              WASM_SIMD_F16x8_SPLAT(WASM_LOCAL_GET(value3)))),
       WASM_ONE});

  for (FMOperation x : qfms_vector()) {
    r.Call(x.a, x.b, x.c);
    uint16_t expected = fp16_ieee_from_fp32_value(x.fused_result);
    Simd128 actual = r.builder().ReadGlobal(*g).to_s128();
    for (int i = 0; i < 8; i++) {
      uint16_t actual_lane = LANE(actual.to_i16x8(), i);
      CheckFloat16LaneResult(x.a, x.b, x.c, expected, actual_lane,
                             true /* exact */);
    }
  }
}

}  // namespace test_run_wasm_f16
}  // namespace wasm
}  // namespace internal
}  // namespace v8
