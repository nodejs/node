// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/overflowing-math.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen/assembler-inl.h"
#include "src/numbers/conversions.h"
#include "src/wasm/wasm-opcodes.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/wasm/wasm-runner.h"
#include "test/unittests/wasm/wasm-simd-utils.h"
#include "third_party/fp16/src/include/fp16.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_f16 {

using RunWasmF16Test = WasmExecutionTestBase;
#define WASM_EXEC_TEST_SUITE RunWasmF16Test

WASM_EXEC_TEST(F16Load) {
  i::v8_flags.wasm_fp16.Set(true);
  WasmRunner<float> r(execution_tier);
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(4);
  r.Build({WASM_F16_LOAD_MEM(WASM_I32V_1(4))});
  r.builder().WriteMemory(&memory[2], fp16_ieee_from_fp32_value(2.75));
  CHECK_EQ(2.75f, r.Call());
}

WASM_EXEC_TEST(F16Store) {
  i::v8_flags.wasm_fp16.Set(true);
  WasmRunner<int32_t> r(execution_tier);
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(4);
  r.Build({WASM_F16_STORE_MEM(WASM_I32V_1(4), WASM_F32(2.75)), WASM_ZERO});
  r.Call();
  CHECK_EQ(r.builder().ReadMemory(&memory[2]), fp16_ieee_from_fp32_value(2.75));
}

WASM_EXEC_TEST(F16x8Splat) {
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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
    i::v8_flags.wasm_fp16.Set(true);                                       \
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
    i::v8_flags.wasm_fp16.Set(true);                                           \
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
    i::v8_flags.wasm_fp16.Set(true);                                        \
    RunF16x8BinOpTest(execution_tier, kExprF16x8##WasmName, WasmName##F16); \
  }

BIN_OP_LIST(TEST_BIN_OP)

#undef TEST_BIN_OP
#undef BIN_OP_LIST

WASM_COMPILED_EXEC_TEST(F16x8AddRaw) {
  i::v8_flags.wasm_fp16.Set(true);
  WasmRunner<int32_t> r(execution_tier);
  constexpr int kLanes = 8;
  // Two inputs and three outputs: a+b, a+a, and (a+b)+a.
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(5 * kLanes);
  uint8_t lhs = r.AllocateLocal(kWasmS128);
  uint8_t rhs = r.AllocateLocal(kWasmS128);
  uint8_t sum = r.AllocateLocal(kWasmS128);
  r.Build(
      {WASM_LOCAL_SET(lhs, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_LOCAL_SET(rhs, WASM_SIMD_LOAD_MEM(WASM_I32V(16))),
       WASM_LOCAL_SET(sum, WASM_SIMD_BINOP(kExprF16x8Add, WASM_LOCAL_GET(lhs),
                                           WASM_LOCAL_GET(rhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(32), WASM_LOCAL_GET(sum)),
       WASM_SIMD_STORE_MEM(WASM_I32V(48),
                           WASM_SIMD_BINOP(kExprF16x8Add, WASM_LOCAL_GET(lhs),
                                           WASM_LOCAL_GET(lhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(64),
                           WASM_SIMD_BINOP(kExprF16x8Add, WASM_LOCAL_GET(sum),
                                           WASM_LOCAL_GET(lhs))),
       WASM_ONE});

  auto check_lane = [](uint16_t a, uint16_t b, uint16_t expected,
                       uint16_t actual) {
    if (!isnan(expected)) {
      CHECK_EQ(expected, actual);  // Includes the sign of zero.
      return;
    }
    // The reference FP32-to-FP16 conversion canonicalizes NaNs. Check raw
    // operands instead: Wasm requires a canonical NaN if all NaN inputs are
    // canonical, and otherwise permits any arithmetic (quiet) NaN.
    CHECK_EQ(0x7e00, actual & 0x7e00);
    if ((!isnan(a) || IsCanonical(a)) && (!isnan(b) || IsCanonical(b))) {
      CHECK(IsCanonical(actual));
    }
  };
  auto check = [&]() {
    CHECK_EQ(1, r.Call());
    for (int lane = 0; lane < kLanes; ++lane) {
      uint16_t a = r.builder().ReadMemory(&memory[lane]);
      uint16_t b = r.builder().ReadMemory(&memory[kLanes + lane]);
      uint16_t expected_sum = AddF16(a, b);
      uint16_t actual_sum = r.builder().ReadMemory(&memory[2 * kLanes + lane]);
      check_lane(a, b, expected_sum, actual_sum);
      check_lane(a, a, AddF16(a, a),
                 r.builder().ReadMemory(&memory[3 * kLanes + lane]));
      // Use the already-checked intermediate's actual NaN class.
      check_lane(actual_sum, a, AddF16(expected_sum, a),
                 r.builder().ReadMemory(&memory[4 * kLanes + lane]));
    }
  };

  // Raw bits avoid splat conversions quieting NaNs or hiding subnormals.
  constexpr uint16_t inputs[] = {
      0x0000, 0x8000,                  // +/- zero
      0x0001, 0x8001, 0x03ff, 0x83ff,  // Subnormal boundaries
      0x0400, 0x8400, 0x0401, 0x8401,  // Normal/subnormal cancellation
      0x1000, 0x9000,                  // Half an ULP at 1: ties to even
      0x3bff, 0x3c00, 0xbc00, 0x3c01, 0xbc01, 0x4000, 0xc000,  // Around +/-1
      0x7bff, 0xfbff,                  // Largest finite values / overflow
      0x7c00, 0xfc00,                  // Infinities
      0x7c01, 0xfc01, 0x7dff, 0xfdff,  // Signaling NaNs
      0x7e00, 0xfe00, 0x7e55, 0xfe55, 0x7fff, 0xffff};  // Quiet NaNs
  for (size_t i = 0; i < arraysize(inputs); i += kLanes) {
    for (size_t j = 0; j < arraysize(inputs); ++j) {
      for (int lane = 0; lane < kLanes; ++lane) {
        r.builder().WriteMemory(&memory[lane],
                                inputs[(i + lane) % arraysize(inputs)]);
        r.builder().WriteMemory(&memory[kLanes + lane],
                                inputs[(j + 3 * lane) % arraysize(inputs)]);
      }
      check();
    }
  }

  base::RandomNumberGenerator rng(v8_flags.random_seed);
  for (int trial = 0; trial < 512; ++trial) {
    for (int lane = 0; lane < 2 * kLanes; ++lane) {
      r.builder().WriteMemory(&memory[lane],
                              static_cast<uint16_t>(rng.NextInt(65536)));
    }
    check();
  }
}

WASM_COMPILED_EXEC_TEST(F16x8SubRaw) {
  i::v8_flags.wasm_fp16.Set(true);
  WasmRunner<int32_t> runner(execution_tier);
  constexpr int kLanes = 8;
  uint16_t* memory = runner.builder().AddMemoryElems<uint16_t>(10 * kLanes);
  uint8_t lhs = runner.AllocateLocal(kWasmS128);
  uint8_t rhs = runner.AllocateLocal(kWasmS128);
  uint8_t difference = runner.AllocateLocal(kWasmS128);
  runner.Build(
      {WASM_LOCAL_SET(lhs, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_LOCAL_SET(rhs, WASM_SIMD_LOAD_MEM(WASM_I32V(16))),
       WASM_LOCAL_SET(difference,
                      WASM_SIMD_BINOP(kExprF16x8Sub, WASM_LOCAL_GET(lhs),
                                      WASM_LOCAL_GET(rhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(32), WASM_LOCAL_GET(difference)),
       WASM_SIMD_STORE_MEM(WASM_I32V(48),
                           WASM_SIMD_BINOP(kExprF16x8Sub, WASM_LOCAL_GET(lhs),
                                           WASM_LOCAL_GET(lhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(64),
                           WASM_SIMD_BINOP(kExprF16x8Sub, WASM_LOCAL_GET(rhs),
                                           WASM_LOCAL_GET(lhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(80),
                           WASM_SIMD_BINOP(kExprF16x8Sub, WASM_LOCAL_GET(lhs),
                                           WASM_LOCAL_GET(difference))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(96),
           WASM_SIMD_BINOP(kExprF16x8Sub, WASM_SIMD_LOAD_MEM(WASM_ZERO),
                           WASM_LOCAL_GET(rhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(112),
                           WASM_SIMD_BINOP(kExprF16x8Sub, WASM_LOCAL_GET(lhs),
                                           WASM_SIMD_LOAD_MEM(WASM_I32V(16)))),
       WASM_SIMD_STORE_MEM(WASM_I32V(128), WASM_LOCAL_GET(lhs)),
       WASM_SIMD_STORE_MEM(WASM_I32V(144), WASM_LOCAL_GET(rhs)), WASM_ONE});

  auto check_lane = [](uint16_t lhs_bits, uint16_t rhs_bits, uint16_t expected,
                       uint16_t actual) {
    if (!isnan(expected)) {
      CHECK_EQ(expected, actual);
      return;
    }
    CHECK_EQ(0x7e00, actual & 0x7e00);
    if ((!isnan(lhs_bits) || IsCanonical(lhs_bits)) &&
        (!isnan(rhs_bits) || IsCanonical(rhs_bits))) {
      CHECK(IsCanonical(actual));
    }
  };
  auto check = [&]() {
    CHECK_EQ(1, runner.Call());
    for (int lane = 0; lane < kLanes; ++lane) {
      uint16_t lhs_bits = runner.builder().ReadMemory(&memory[lane]);
      uint16_t rhs_bits = runner.builder().ReadMemory(&memory[kLanes + lane]);
      uint16_t expected_difference = SubF16(lhs_bits, rhs_bits);
      uint16_t actual_difference =
          runner.builder().ReadMemory(&memory[2 * kLanes + lane]);
      check_lane(lhs_bits, rhs_bits, expected_difference, actual_difference);
      check_lane(lhs_bits, lhs_bits, SubF16(lhs_bits, lhs_bits),
                 runner.builder().ReadMemory(&memory[3 * kLanes + lane]));
      check_lane(rhs_bits, lhs_bits, SubF16(rhs_bits, lhs_bits),
                 runner.builder().ReadMemory(&memory[4 * kLanes + lane]));
      check_lane(lhs_bits, actual_difference,
                 SubF16(lhs_bits, expected_difference),
                 runner.builder().ReadMemory(&memory[5 * kLanes + lane]));
      for (int slot : {6, 7}) {
        check_lane(lhs_bits, rhs_bits, expected_difference,
                   runner.builder().ReadMemory(&memory[slot * kLanes + lane]));
      }
      CHECK_EQ(lhs_bits,
               runner.builder().ReadMemory(&memory[8 * kLanes + lane]));
      CHECK_EQ(rhs_bits,
               runner.builder().ReadMemory(&memory[9 * kLanes + lane]));
    }
  };

  constexpr uint16_t inputs[] = {
      0x0000, 0x8000, 0x0001, 0x8001, 0x03ff, 0x83ff, 0x0400, 0x8400, 0x0401,
      0x8401, 0x1000, 0x9000, 0x3bff, 0x3c00, 0xbc00, 0x3c01, 0xbc01, 0x4000,
      0xc000, 0x7bff, 0xfbff, 0x7c00, 0xfc00, 0x7c01, 0xfc01, 0x7dff, 0xfdff,
      0x7e00, 0xfe00, 0x7e55, 0xfe55, 0x7fff, 0xffff};
  for (size_t left = 0; left < arraysize(inputs); left += kLanes) {
    for (size_t right = 0; right < arraysize(inputs); ++right) {
      for (int lane = 0; lane < kLanes; ++lane) {
        runner.builder().WriteMemory(&memory[lane],
                                     inputs[(left + lane) % arraysize(inputs)]);
        runner.builder().WriteMemory(
            &memory[kLanes + lane],
            inputs[(right + 3 * lane) % arraysize(inputs)]);
      }
      check();
    }
  }

  base::RandomNumberGenerator rng(v8_flags.random_seed);
  for (int trial = 0; trial < 512; ++trial) {
    for (int lane = 0; lane < 2 * kLanes; ++lane) {
      runner.builder().WriteMemory(&memory[lane],
                                   static_cast<uint16_t>(rng.NextInt(65536)));
    }
    check();
  }
}

WASM_COMPILED_EXEC_TEST(F16x8MulRaw) {
  i::v8_flags.wasm_fp16.Set(true);
  WasmRunner<int32_t> runner(execution_tier);
  constexpr int kLanes = 8;
  uint16_t* memory = runner.builder().AddMemoryElems<uint16_t>(9 * kLanes);
  uint8_t lhs = runner.AllocateLocal(kWasmS128);
  uint8_t rhs = runner.AllocateLocal(kWasmS128);
  uint8_t product = runner.AllocateLocal(kWasmS128);
  runner.Build(
      {WASM_LOCAL_SET(lhs, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_LOCAL_SET(rhs, WASM_SIMD_LOAD_MEM(WASM_I32V(16))),
       WASM_LOCAL_SET(product,
                      WASM_SIMD_BINOP(kExprF16x8Mul, WASM_LOCAL_GET(lhs),
                                      WASM_LOCAL_GET(rhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(32), WASM_LOCAL_GET(product)),
       WASM_SIMD_STORE_MEM(WASM_I32V(48),
                           WASM_SIMD_BINOP(kExprF16x8Mul, WASM_LOCAL_GET(lhs),
                                           WASM_LOCAL_GET(lhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(64),
                           WASM_SIMD_BINOP(kExprF16x8Mul, WASM_LOCAL_GET(lhs),
                                           WASM_LOCAL_GET(product))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(80),
           WASM_SIMD_BINOP(kExprF16x8Mul, WASM_SIMD_LOAD_MEM(WASM_ZERO),
                           WASM_LOCAL_GET(rhs))),
       WASM_SIMD_STORE_MEM(WASM_I32V(96),
                           WASM_SIMD_BINOP(kExprF16x8Mul, WASM_LOCAL_GET(lhs),
                                           WASM_SIMD_LOAD_MEM(WASM_I32V(16)))),
       WASM_SIMD_STORE_MEM(WASM_I32V(112), WASM_LOCAL_GET(lhs)),
       WASM_SIMD_STORE_MEM(WASM_I32V(128), WASM_LOCAL_GET(rhs)), WASM_ONE});

  auto check_lane = [](uint16_t lhs_bits, uint16_t rhs_bits, uint16_t expected,
                       uint16_t actual) {
    if (!isnan(expected)) {
      CHECK_EQ(expected, actual);
      return;
    }
    CHECK_EQ(0x7e00, actual & 0x7e00);
    if ((!isnan(lhs_bits) || IsCanonical(lhs_bits)) &&
        (!isnan(rhs_bits) || IsCanonical(rhs_bits))) {
      CHECK(IsCanonical(actual));
    }
  };
  auto check = [&]() {
    CHECK_EQ(1, runner.Call());
    for (int lane = 0; lane < kLanes; ++lane) {
      uint16_t lhs_bits = runner.builder().ReadMemory(&memory[lane]);
      uint16_t rhs_bits = runner.builder().ReadMemory(&memory[kLanes + lane]);
      uint16_t expected_product = MulF16(lhs_bits, rhs_bits);
      uint16_t actual_product =
          runner.builder().ReadMemory(&memory[2 * kLanes + lane]);
      check_lane(lhs_bits, rhs_bits, expected_product, actual_product);
      check_lane(lhs_bits, lhs_bits, MulF16(lhs_bits, lhs_bits),
                 runner.builder().ReadMemory(&memory[3 * kLanes + lane]));
      check_lane(lhs_bits, actual_product, MulF16(lhs_bits, expected_product),
                 runner.builder().ReadMemory(&memory[4 * kLanes + lane]));
      for (int slot : {5, 6}) {
        check_lane(lhs_bits, rhs_bits, expected_product,
                   runner.builder().ReadMemory(&memory[slot * kLanes + lane]));
      }
      CHECK_EQ(lhs_bits,
               runner.builder().ReadMemory(&memory[7 * kLanes + lane]));
      CHECK_EQ(rhs_bits,
               runner.builder().ReadMemory(&memory[8 * kLanes + lane]));
    }
  };

  constexpr struct {
    uint16_t lhs;
    uint16_t rhs;
    uint16_t expected;
  } rounding_cases[kLanes] = {
      {0x0001, 0x3800, 0x0000}, {0x0003, 0x3800, 0x0002},
      {0x8001, 0x3800, 0x8000}, {0x03ff, 0x3c01, 0x0400},
      {0x0400, 0x3bff, 0x0400}, {0x3e00, 0x3c01, 0x3e02},
      {0x3e00, 0x3c03, 0x3e04}, {0x7bff, 0x3c01, 0x7c00}};
  for (int lane = 0; lane < kLanes; ++lane) {
    const auto& test = rounding_cases[lane];
    CHECK_EQ(test.expected, MulF16(test.lhs, test.rhs));
    runner.builder().WriteMemory(&memory[lane], test.lhs);
    runner.builder().WriteMemory(&memory[kLanes + lane], test.rhs);
  }
  check();
  for (int lane = 0; lane < kLanes; ++lane) {
    CHECK_EQ(rounding_cases[lane].expected,
             runner.builder().ReadMemory(&memory[2 * kLanes + lane]));
  }

  constexpr uint16_t inputs[] = {
      0x0000, 0x8000, 0x0001, 0x8001, 0x0002, 0x8002, 0x0003, 0x8003,
      0x03ff, 0x83ff, 0x0400, 0x8400, 0x0401, 0x8401, 0x3800, 0xb800,
      0x3bff, 0xbbff, 0x3c00, 0xbc00, 0x3c01, 0xbc01, 0x3c03, 0xbc03,
      0x3e00, 0xbe00, 0x4000, 0xc000, 0x7bff, 0xfbff, 0x7c00, 0xfc00,
      0x7c01, 0xfc01, 0x7dff, 0xfdff, 0x7e00, 0xfe00, 0x7e55, 0xfe55};
  for (size_t left = 0; left < arraysize(inputs); left += kLanes) {
    for (size_t right = 0; right < arraysize(inputs); ++right) {
      for (int lane = 0; lane < kLanes; ++lane) {
        runner.builder().WriteMemory(&memory[lane],
                                     inputs[(left + lane) % arraysize(inputs)]);
        runner.builder().WriteMemory(
            &memory[kLanes + lane],
            inputs[(right + 3 * lane) % arraysize(inputs)]);
      }
      check();
    }
  }

  base::RandomNumberGenerator rng(v8_flags.random_seed);
  for (int trial = 0; trial < 512; ++trial) {
    for (int lane = 0; lane < 2 * kLanes; ++lane) {
      runner.builder().WriteMemory(&memory[lane],
                                   static_cast<uint16_t>(rng.NextInt(65536)));
    }
    check();
  }
}

WASM_EXEC_TEST(F16x8ConvertI16x8) {
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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

// A binary16 product has at most 22 significant bits. Binary64 retains the
// product and any addend capable of affecting binary16 rounding, including
// perturbations of a halfway product. Convert directly to half: going through
// float can erase those perturbations and introduce double rounding.
uint16_t FmaF16(uint16_t a, uint16_t b, uint16_t c, bool negate,
                bool through_float = false) {
  double lhs = fp16_ieee_to_fp32_value(a);
  double rhs = fp16_ieee_to_fp32_value(b);
  double acc = fp16_ieee_to_fp32_value(c);
  if (negate) lhs = -lhs;
  if (through_float) {
    return fp16_ieee_from_fp32_value(std::fma(static_cast<float>(lhs),
                                              static_cast<float>(rhs),
                                              static_cast<float>(acc)));
  }
  return DoubleToFloat16(std::fma(lhs, rhs, acc));
}

TEST_F(RunWasmF16Test, F16x8FmaReference) {
  for (bool negate : {false, true}) {
    uint16_t sign = negate ? 0x8000 : 0;
    // A tiny addend changes which side of a half-way product is rounded.
    CHECK_EQ(0x3e01, FmaF16(0x3e00 ^ sign, 0x3c01, 0x8001, negate));
    CHECK_EQ(0x3e02, FmaF16(0x3e00 ^ sign, 0x3c01, 0x8001, negate, true));
    CHECK_EQ(0x3e05, FmaF16(0x3e00 ^ sign, 0x3c03, 0x0001, negate));
    CHECK_EQ(0x3e04, FmaF16(0x3e00 ^ sign, 0x3c03, 0x0001, negate, true));
    // (1 + 2^-10) * (1 - 2^-10) - 1 = -2^-20, not zero.
    CHECK_EQ(0x8010, FmaF16(0x3c01 ^ sign, 0x3bfe, 0xbc00, negate));
    CHECK_EQ(0x0000, FmaF16(0x3c00 ^ sign, 0x3c00, 0xbc00, negate));
    CHECK_EQ(0x8000, FmaF16(0x8000 ^ sign, 0x3c00, 0x8000, negate));
    CHECK_EQ(0x0000, FmaF16(0x0001 ^ sign, 0x3800, 0x0000, negate));
    CHECK_EQ(0x0002, FmaF16(0x0003 ^ sign, 0x3800, 0x0000, negate));
  }
}

void RunF16x8FmaRaw(TestExecutionTier execution_tier, bool negate) {
  i::v8_flags.wasm_fp16.Set(true);
  WasmRunner<int32_t> r(execution_tier);
  constexpr int kLanes = 8;
  // Three inputs, thirteen FMA results, and three copies of the live inputs.
  uint16_t* memory = r.builder().AddMemoryElems<uint16_t>(19 * kLanes);
  uint8_t a = r.AllocateLocal(kWasmS128);
  uint8_t b = r.AllocateLocal(kWasmS128);
  uint8_t c = r.AllocateLocal(kWasmS128);
  uint8_t result = r.AllocateLocal(kWasmS128);
  WasmOpcode opcode = negate ? kExprF16x8Qfms : kExprF16x8Qfma;
  r.Build(
      {WASM_LOCAL_SET(a, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_LOCAL_SET(b, WASM_SIMD_LOAD_MEM(WASM_I32V(16))),
       WASM_LOCAL_SET(c, WASM_SIMD_LOAD_MEM(WASM_I32V(32))),
       WASM_LOCAL_SET(result,
                      WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a),
                                       WASM_LOCAL_GET(b), WASM_LOCAL_GET(c))),
       WASM_SIMD_STORE_MEM(WASM_I32V(48), WASM_LOCAL_GET(result)),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(64),
           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(a),
                            WASM_LOCAL_GET(c))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(80),
           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(b),
                            WASM_LOCAL_GET(a))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(96),
           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(b),
                            WASM_LOCAL_GET(b))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(112),
           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(a),
                            WASM_LOCAL_GET(a))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(128),
           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(b),
                            WASM_LOCAL_GET(result))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(192),
           WASM_SIMD_TERNOP(opcode, WASM_SIMD_LOAD_MEM(WASM_ZERO),
                            WASM_LOCAL_GET(b), WASM_LOCAL_GET(c))),
       WASM_SIMD_STORE_MEM(WASM_I32V(208),
                           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a),
                                            WASM_SIMD_LOAD_MEM(WASM_I32V(16)),
                                            WASM_LOCAL_GET(c))),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(224),
           WASM_SIMD_TERNOP(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(b),
                            WASM_SIMD_LOAD_MEM(WASM_I32V(32)))),
       WASM_SIMD_STORE_MEM(WASM_I32V(144), WASM_LOCAL_GET(a)),
       WASM_SIMD_STORE_MEM(WASM_I32V(160), WASM_LOCAL_GET(b)),
       WASM_SIMD_STORE_MEM(WASM_I32V(176), WASM_LOCAL_GET(c)),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(240),
           WASM_SIMD_OPN(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(a),
                         WASM_LOCAL_GET(a),
                         WASM_LOCAL_SET(a, WASM_SIMD_I32x4_SPLAT(WASM_ZERO)))),
       WASM_LOCAL_SET(a, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(256),
           WASM_SIMD_OPN(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(a),
                         WASM_LOCAL_GET(b),
                         WASM_LOCAL_SET(a, WASM_SIMD_I32x4_SPLAT(WASM_ZERO)))),
       WASM_LOCAL_SET(a, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(272),
           WASM_SIMD_OPN(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(b),
                         WASM_LOCAL_GET(a),
                         WASM_LOCAL_SET(a, WASM_SIMD_I32x4_SPLAT(WASM_ZERO)))),
       WASM_LOCAL_SET(a, WASM_SIMD_LOAD_MEM(WASM_ZERO)),
       WASM_SIMD_STORE_MEM(
           WASM_I32V(288),
           WASM_SIMD_OPN(opcode, WASM_LOCAL_GET(a), WASM_LOCAL_GET(b),
                         WASM_LOCAL_GET(b),
                         WASM_LOCAL_SET(b, WASM_SIMD_I32x4_SPLAT(WASM_ZERO)))),
       WASM_ONE});

  auto check_lane = [=](uint16_t lhs, uint16_t rhs, uint16_t acc,
                        uint16_t actual) {
    uint16_t fused = FmaF16(lhs, rhs, acc, negate);
    uint16_t widened = FmaF16(lhs, rhs, acc, negate, true);
    if (!isnan(fused)) {
      // The proposal's native-half and FP32-widened lowerings can differ.
      // Keep both references separate; neither is an oracle for the other.
#if V8_TARGET_ARCH_X64
      bool native =
          UseAvx10_1() && (execution_tier == TestExecutionTier::kLiftoff ||
                           CpuFeatures::IsSupported(F16C));
      CHECK_EQ(native ? fused : widened, actual);
#else
      CHECK(actual == fused || actual == widened);
#endif
      return;
    }
    CHECK_EQ(0x7e00, actual & 0x7e00);
    if ((!isnan(lhs) || IsCanonical(lhs)) &&
        (!isnan(rhs) || IsCanonical(rhs)) &&
        (!isnan(acc) || IsCanonical(acc))) {
      CHECK(IsCanonical(actual));
    }
  };
  auto check = [&]() {
    CHECK_EQ(1, r.Call());
    for (int lane = 0; lane < kLanes; ++lane) {
      auto read = [&](int slot) {
        return r.builder().ReadMemory(&memory[slot * kLanes + lane]);
      };
      uint16_t lhs = read(0), rhs = read(1), acc = read(2);
      check_lane(lhs, rhs, acc, read(3));
      check_lane(lhs, lhs, acc, read(4));
      check_lane(lhs, rhs, lhs, read(5));
      check_lane(lhs, rhs, rhs, read(6));
      check_lane(lhs, lhs, lhs, read(7));
      // Use the checked intermediate, including its actual NaN class.
      check_lane(lhs, rhs, read(3), read(8));
      CHECK_EQ(lhs, read(9));
      CHECK_EQ(rhs, read(10));
      CHECK_EQ(acc, read(11));
      check_lane(lhs, rhs, acc, read(12));
      check_lane(lhs, rhs, acc, read(13));
      check_lane(lhs, rhs, acc, read(14));
      check_lane(lhs, lhs, lhs, read(15));
      check_lane(lhs, lhs, rhs, read(16));
      check_lane(lhs, rhs, lhs, read(17));
      check_lane(lhs, rhs, rhs, read(18));
    }
  };

  constexpr uint16_t rounding[][3] = {
      {0x3e00, 0x3c01, 0x8001}, {0x3e00, 0x3c03, 0x0001},
      {0x3c01, 0x3bfe, 0xbc00}, {0x7bff, 0x4000, 0xfbff},
      {0x0001, 0x3800, 0x0000}, {0x0003, 0x3800, 0x0000},
      {0x8000, 0x3c00, 0x8000}, {0x3c00, 0x3c00, 0xbc00}};
  for (int lane = 0; lane < kLanes; ++lane) {
    for (int input = 0; input < 3; ++input) {
      uint16_t bits = rounding[lane][input];
      if (negate && input == 0) bits ^= 0x8000;
      r.builder().WriteMemory(&memory[input * kLanes + lane], bits);
    }
  }
  check();

  constexpr uint16_t edges[] = {0x0000, 0x8000, 0x0001, 0x8001, 0x03ff, 0x83ff,
                                0x0400, 0x8400, 0x3800, 0xb800, 0x3bff, 0x3c00,
                                0xbc00, 0x3c01, 0x4000, 0xc000, 0x7bff, 0xfbff,
                                0x7c00, 0xfc00, 0x7c01, 0xfc01, 0x7dff, 0xfdff,
                                0x7e00, 0xfe00, 0x7e55, 0xfe55, 0x7fff, 0xffff};
  for (uint16_t lhs : edges) {
    for (uint16_t rhs : edges) {
      for (size_t acc = 0; acc < arraysize(edges); acc += kLanes) {
        for (int lane = 0; lane < kLanes; ++lane) {
          r.builder().WriteMemory(&memory[lane], lhs);
          r.builder().WriteMemory(&memory[kLanes + lane], rhs);
          r.builder().WriteMemory(&memory[2 * kLanes + lane],
                                  edges[(acc + lane) % arraysize(edges)]);
        }
        check();
      }
    }
  }
  base::RandomNumberGenerator rng(0xf16f00);
  for (int trial = 0; trial < 1024; ++trial) {
    for (int lane = 0; lane < 3 * kLanes; ++lane) {
      r.builder().WriteMemory(&memory[lane],
                              static_cast<uint16_t>(rng.NextInt(65536)));
    }
    check();
  }
}

WASM_COMPILED_EXEC_TEST(F16x8QfmaRaw) { RunF16x8FmaRaw(execution_tier, false); }
WASM_COMPILED_EXEC_TEST(F16x8QfmsRaw) { RunF16x8FmaRaw(execution_tier, true); }

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
  i::v8_flags.wasm_fp16.Set(true);
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
  i::v8_flags.wasm_fp16.Set(true);
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

#undef WASM_EXEC_TEST_SUITE

}  // namespace test_run_wasm_f16
}  // namespace wasm
}  // namespace internal
}  // namespace v8
