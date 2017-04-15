// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

namespace {

typedef float (*FloatUnOp)(float);
typedef float (*FloatBinOp)(float, float);
typedef int32_t (*FloatCompareOp)(float, float);
typedef int32_t (*Int32BinOp)(int32_t, int32_t);

template <typename T>
T Negate(T a) {
  return -a;
}

template <typename T>
T Add(T a, T b) {
  return a + b;
}

template <typename T>
T Sub(T a, T b) {
  return a - b;
}

template <typename T>
int32_t Equal(T a, T b) {
  return a == b ? 0xFFFFFFFF : 0;
}

template <typename T>
int32_t NotEqual(T a, T b) {
  return a != b ? 0xFFFFFFFF : 0;
}

#if V8_TARGET_ARCH_ARM
int32_t Equal(float a, float b) { return a == b ? 0xFFFFFFFF : 0; }

int32_t NotEqual(float a, float b) { return a != b ? 0xFFFFFFFF : 0; }
#endif  // V8_TARGET_ARCH_ARM

}  // namespace

// TODO(gdeepti): These are tests using sample values to verify functional
// correctness of opcodes, add more tests for a range of values and macroize
// tests.

// TODO(bbudge) Figure out how to compare floats in Wasm code that can handle
// NaNs. For now, our tests avoid using NaNs.
#define WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lane_value, lane_index) \
  WASM_IF(WASM_##LANE_TYPE##_NE(WASM_GET_LOCAL(lane_value),                  \
                                WASM_SIMD_##TYPE##_EXTRACT_LANE(             \
                                    lane_index, WASM_GET_LOCAL(value))),     \
          WASM_RETURN1(WASM_ZERO))

#define WASM_SIMD_CHECK4(TYPE, value, LANE_TYPE, lv0, lv1, lv2, lv3) \
  WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv0, 0)               \
  , WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv1, 1),            \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv2, 2),          \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv3, 3)

#define WASM_SIMD_CHECK_SPLAT4(TYPE, value, LANE_TYPE, lv) \
  WASM_SIMD_CHECK4(TYPE, value, LANE_TYPE, lv, lv, lv, lv)

#define WASM_SIMD_CHECK_F32_LANE(TYPE, value, lane_value, lane_index)       \
  WASM_IF(                                                                  \
      WASM_I32_NE(WASM_I32_REINTERPRET_F32(WASM_GET_LOCAL(lane_value)),     \
                  WASM_I32_REINTERPRET_F32(WASM_SIMD_##TYPE##_EXTRACT_LANE( \
                      lane_index, WASM_GET_LOCAL(value)))),                 \
      WASM_RETURN1(WASM_ZERO))

#define WASM_SIMD_CHECK4_F32(TYPE, value, lv0, lv1, lv2, lv3) \
  WASM_SIMD_CHECK_F32_LANE(TYPE, value, lv0, 0)               \
  , WASM_SIMD_CHECK_F32_LANE(TYPE, value, lv1, 1),            \
      WASM_SIMD_CHECK_F32_LANE(TYPE, value, lv2, 2),          \
      WASM_SIMD_CHECK_F32_LANE(TYPE, value, lv3, 3)

#define WASM_SIMD_CHECK_SPLAT4_F32(TYPE, value, lv) \
  WASM_SIMD_CHECK4_F32(TYPE, value, lv, lv, lv, lv)

#if V8_TARGET_ARCH_ARM
WASM_EXEC_TEST(F32x4Splat) {
  FLAG_wasm_simd_prototype = true;

  WasmRunner<int32_t, float> r(kExecuteCompiled);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT4_F32(F32x4, simd, lane_val), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_EXEC_TEST(F32x4ReplaceLane) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, float, float> r(kExecuteCompiled);
  byte old_val = 0;
  byte new_val = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(old_val))),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(0, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK4(F32x4, simd, F32, new_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(1, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK4(F32x4, simd, F32, new_val, new_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(2, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK4(F32x4, simd, F32, new_val, new_val, new_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_SPLAT4(F32x4, simd, F32, new_val), WASM_ONE);

  CHECK_EQ(1, r.Call(3.14159, -1.5));
}

// Tests both signed and unsigned conversion.
WASM_EXEC_TEST(F32x4FromInt32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, float, float> r(kExecuteCompiled);
  byte a = 0;
  byte expected_signed = 1;
  byte expected_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(
      r, WASM_SET_LOCAL(simd0, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
      WASM_SET_LOCAL(simd1, WASM_SIMD_F32x4_FROM_I32x4(WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK_SPLAT4_F32(F32x4, simd1, expected_signed),
      WASM_SET_LOCAL(simd2, WASM_SIMD_F32x4_FROM_U32x4(WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK_SPLAT4_F32(F32x4, simd2, expected_unsigned), WASM_ONE);

  FOR_INT32_INPUTS(i) {
    CHECK_EQ(1, r.Call(*i, static_cast<float>(*i),
                       static_cast<float>(static_cast<uint32_t>(*i))));
  }
}

WASM_EXEC_TEST(S32x4Select) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte val1 = 0;
  byte val2 = 1;
  byte mask = r.AllocateLocal(kWasmS128);
  byte src1 = r.AllocateLocal(kWasmS128);
  byte src2 = r.AllocateLocal(kWasmS128);
  BUILD(r,

        WASM_SET_LOCAL(mask, WASM_SIMD_I32x4_SPLAT(WASM_ZERO)),
        WASM_SET_LOCAL(src1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(val1))),
        WASM_SET_LOCAL(src2, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(val2))),
        WASM_SET_LOCAL(mask, WASM_SIMD_I32x4_REPLACE_LANE(
                                 1, WASM_GET_LOCAL(mask), WASM_I32V(-1))),
        WASM_SET_LOCAL(mask, WASM_SIMD_I32x4_REPLACE_LANE(
                                 2, WASM_GET_LOCAL(mask), WASM_I32V(-1))),
        WASM_SET_LOCAL(mask, WASM_SIMD_S32x4_SELECT(WASM_GET_LOCAL(mask),
                                                    WASM_GET_LOCAL(src1),
                                                    WASM_GET_LOCAL(src2))),
        WASM_SIMD_CHECK_LANE(I32x4, mask, I32, val2, 0),
        WASM_SIMD_CHECK_LANE(I32x4, mask, I32, val1, 1),
        WASM_SIMD_CHECK_LANE(I32x4, mask, I32, val1, 2),
        WASM_SIMD_CHECK_LANE(I32x4, mask, I32, val2, 3), WASM_ONE);

  CHECK_EQ(1, r.Call(0x1234, 0x5678));
}

void RunF32x4UnOpTest(WasmOpcode simd_op, FloatUnOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, float, float> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_UNOP(simd_op & 0xffu, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT4_F32(F32x4, simd, expected), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) {
    if (std::isnan(*i)) continue;
    CHECK_EQ(1, r.Call(*i, expected_op(*i)));
  }
}

WASM_EXEC_TEST(F32x4Abs) { RunF32x4UnOpTest(kExprF32x4Abs, std::abs); }
WASM_EXEC_TEST(F32x4Neg) { RunF32x4UnOpTest(kExprF32x4Neg, Negate); }

void RunF32x4BinOpTest(WasmOpcode simd_op, FloatBinOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, float, float, float> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1,
                       WASM_SIMD_BINOP(simd_op & 0xffu, WASM_GET_LOCAL(simd0),
                                       WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT4_F32(F32x4, simd1, expected), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) {
    if (std::isnan(*i)) continue;
    FOR_FLOAT32_INPUTS(j) {
      if (std::isnan(*j)) continue;
      float expected = expected_op(*i, *j);
      // SIMD on some platforms may handle denormalized numbers differently.
      // TODO(bbudge) On platforms that flush denorms to zero, test with
      // expected == 0.
      if (std::fpclassify(expected) == FP_SUBNORMAL) continue;
      CHECK_EQ(1, r.Call(*i, *j, expected));
    }
  }
}

WASM_EXEC_TEST(F32x4Add) { RunF32x4BinOpTest(kExprF32x4Add, Add); }
WASM_EXEC_TEST(F32x4Sub) { RunF32x4BinOpTest(kExprF32x4Sub, Sub); }

void RunF32x4CompareOpTest(WasmOpcode simd_op, FloatCompareOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, float, float, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1,
                       WASM_SIMD_BINOP(simd_op & 0xffu, WASM_GET_LOCAL(simd0),
                                       WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) {
    if (std::isnan(*i)) continue;
    FOR_FLOAT32_INPUTS(j) {
      if (std::isnan(*j)) continue;
      // SIMD on some platforms may handle denormalized numbers differently.
      // Check for number pairs that are very close together.
      if (std::fpclassify(*i - *j) == FP_SUBNORMAL) continue;
      CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j)));
    }
  }
}

WASM_EXEC_TEST(F32x4Equal) { RunF32x4CompareOpTest(kExprF32x4Eq, Equal); }
WASM_EXEC_TEST(F32x4NotEqual) { RunF32x4CompareOpTest(kExprF32x4Ne, NotEqual); }
#endif  // V8_TARGET_ARCH_ARM

WASM_EXEC_TEST(I32x4Splat) {
  FLAG_wasm_simd_prototype = true;

  // Store SIMD value in a local variable, use extract lane to check lane values
  // This test is not a test for ExtractLane as Splat does not create
  // interesting SIMD values.
  //
  // SetLocal(1, I32x4Splat(Local(0)));
  // For each lane index
  // if(Local(0) != I32x4ExtractLane(Local(1), index)
  //   return 0
  //
  // return 1
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, lane_val), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_EXEC_TEST(I32x4ReplaceLane) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte old_val = 0;
  byte new_val = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(old_val))),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I32x4_REPLACE_LANE(0, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK4(I32x4, simd, I32, new_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK4(I32x4, simd, I32, new_val, new_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK4(I32x4, simd, I32, new_val, new_val, new_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I32x4_REPLACE_LANE(3, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, new_val), WASM_ONE);

  CHECK_EQ(1, r.Call(1, 2));
}

#if V8_TARGET_ARCH_ARM

// Determines if conversion from float to int will be valid.
bool CanRoundToZeroAndConvert(double val, bool unsigned_integer) {
  const double max_uint = static_cast<double>(0xffffffffu);
  const double max_int = static_cast<double>(kMaxInt);
  const double min_int = static_cast<double>(kMinInt);

  // Check for NaN.
  if (val != val) {
    return false;
  }

  // Round to zero and check for overflow. This code works because 32 bit
  // integers can be exactly represented by ieee-754 64bit floating-point
  // values.
  return unsigned_integer ? (val < (max_uint + 1.0)) && (val > -1)
                          : (val < (max_int + 1.0)) && (val > (min_int - 1.0));
}

int ConvertInvalidValue(double val, bool unsigned_integer) {
  if (val != val) {
    return 0;
  } else {
    if (unsigned_integer) {
      return (val < 0) ? 0 : 0xffffffffu;
    } else {
      return (val < 0) ? kMinInt : kMaxInt;
    }
  }
}

int32_t ConvertToInt(double val, bool unsigned_integer) {
  int32_t result =
      unsigned_integer ? static_cast<uint32_t>(val) : static_cast<int32_t>(val);

  if (!CanRoundToZeroAndConvert(val, unsigned_integer)) {
    result = ConvertInvalidValue(val, unsigned_integer);
  }
  return result;
}

// Tests both signed and unsigned conversion.
WASM_EXEC_TEST(I32x4FromFloat32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, float, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected_signed = 1;
  byte expected_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(
      r, WASM_SET_LOCAL(simd0, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
      WASM_SET_LOCAL(simd1, WASM_SIMD_I32x4_FROM_F32x4(WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected_signed),
      WASM_SET_LOCAL(simd2, WASM_SIMD_U32x4_FROM_F32x4(WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK_SPLAT4(I32x4, simd2, I32, expected_unsigned), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) {
    int32_t signed_value = ConvertToInt(*i, false);
    int32_t unsigned_value = ConvertToInt(*i, true);
    CHECK_EQ(1, r.Call(*i, signed_value, unsigned_value));
  }
}
#endif  // V8_TARGET_ARCH_ARM

void RunI32x4BinOpTest(WasmOpcode simd_op, Int32BinOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1,
                       WASM_SIMD_BINOP(simd_op & 0xffu, WASM_GET_LOCAL(simd0),
                                       WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I32x4Add) { RunI32x4BinOpTest(kExprI32x4Add, Add); }

WASM_EXEC_TEST(I32x4Sub) { RunI32x4BinOpTest(kExprI32x4Sub, Sub); }

#if V8_TARGET_ARCH_ARM
WASM_EXEC_TEST(I32x4Equal) { RunI32x4BinOpTest(kExprI32x4Eq, Equal); }

WASM_EXEC_TEST(I32x4NotEqual) { RunI32x4BinOpTest(kExprI32x4Ne, NotEqual); }
#endif  // V8_TARGET_ARCH_ARM
