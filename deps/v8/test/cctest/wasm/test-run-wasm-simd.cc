// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
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
typedef int32_t (*Int32UnOp)(int32_t);
typedef int32_t (*Int32BinOp)(int32_t, int32_t);
typedef int32_t (*Int32ShiftOp)(int32_t, int);
typedef int16_t (*Int16UnOp)(int16_t);
typedef int16_t (*Int16BinOp)(int16_t, int16_t);
typedef int16_t (*Int16ShiftOp)(int16_t, int);
typedef int8_t (*Int8UnOp)(int8_t);
typedef int8_t (*Int8BinOp)(int8_t, int8_t);
typedef int8_t (*Int8ShiftOp)(int8_t, int);

#if V8_TARGET_ARCH_ARM
// Floating point specific value functions, only used by ARM so far.
int32_t Equal(float a, float b) { return a == b ? 1 : 0; }

int32_t NotEqual(float a, float b) { return a != b ? 1 : 0; }
#endif  // V8_TARGET_ARCH_ARM

// Generic expected value functions.
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
T Mul(T a, T b) {
  return a * b;
}

template <typename T>
T Minimum(T a, T b) {
  return a <= b ? a : b;
}

template <typename T>
T Maximum(T a, T b) {
  return a >= b ? a : b;
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

template <typename T>
T Equal(T a, T b) {
  return a == b ? 1 : 0;
}

template <typename T>
T NotEqual(T a, T b) {
  return a != b ? 1 : 0;
}

template <typename T>
T Greater(T a, T b) {
  return a > b ? 1 : 0;
}

template <typename T>
T GreaterEqual(T a, T b) {
  return a >= b ? 1 : 0;
}

template <typename T>
T Less(T a, T b) {
  return a < b ? 1 : 0;
}

template <typename T>
T LessEqual(T a, T b) {
  return a <= b ? 1 : 0;
}

template <typename T>
T UnsignedGreater(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) > static_cast<UnsignedT>(b) ? 1 : 0;
}

template <typename T>
T UnsignedGreaterEqual(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) >= static_cast<UnsignedT>(b) ? 1 : 0;
}

template <typename T>
T UnsignedLess(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) < static_cast<UnsignedT>(b) ? 1 : 0;
}

template <typename T>
T UnsignedLessEqual(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) <= static_cast<UnsignedT>(b) ? 1 : 0;
}

template <typename T>
T LogicalShiftLeft(T a, int shift) {
  return a << shift;
}

template <typename T>
T LogicalShiftRight(T a, int shift) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) >> shift;
}

template <typename T>
int64_t Widen(T value) {
  static_assert(sizeof(int64_t) > sizeof(T), "T must be int32_t or smaller");
  return static_cast<int64_t>(value);
}

template <typename T>
int64_t UnsignedWiden(T value) {
  static_assert(sizeof(int64_t) > sizeof(T), "T must be int32_t or smaller");
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<int64_t>(static_cast<UnsignedT>(value));
}

template <typename T>
T Clamp(int64_t value) {
  static_assert(sizeof(int64_t) > sizeof(T), "T must be int32_t or smaller");
  int64_t min = static_cast<int64_t>(std::numeric_limits<T>::min());
  int64_t max = static_cast<int64_t>(std::numeric_limits<T>::max());
  int64_t clamped = std::max(min, std::min(max, value));
  return static_cast<T>(clamped);
}

template <typename T>
T AddSaturate(T a, T b) {
  return Clamp<T>(Widen(a) + Widen(b));
}

template <typename T>
T SubSaturate(T a, T b) {
  return Clamp<T>(Widen(a) - Widen(b));
}

template <typename T>
T UnsignedAddSaturate(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return Clamp<UnsignedT>(UnsignedWiden(a) + UnsignedWiden(b));
}

template <typename T>
T UnsignedSubSaturate(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return Clamp<UnsignedT>(UnsignedWiden(a) - UnsignedWiden(b));
}

template <typename T>
T And(T a, T b) {
  return a & b;
}

template <typename T>
T Or(T a, T b) {
  return a | b;
}

template <typename T>
T Xor(T a, T b) {
  return a ^ b;
}

template <typename T>
T Not(T a) {
  return ~a;
}

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

#define WASM_SIMD_CHECK8(TYPE, value, LANE_TYPE, lv0, lv1, lv2, lv3, lv4, lv5, \
                         lv6, lv7)                                             \
  WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv0, 0)                         \
  , WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv1, 1),                      \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv2, 2),                    \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv3, 3),                    \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv4, 4),                    \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv5, 5),                    \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv6, 6),                    \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv7, 7)

#define WASM_SIMD_CHECK_SPLAT8(TYPE, value, LANE_TYPE, lv) \
  WASM_SIMD_CHECK8(TYPE, value, LANE_TYPE, lv, lv, lv, lv, lv, lv, lv, lv)

#define WASM_SIMD_CHECK16(TYPE, value, LANE_TYPE, lv0, lv1, lv2, lv3, lv4, \
                          lv5, lv6, lv7, lv8, lv9, lv10, lv11, lv12, lv13, \
                          lv14, lv15)                                      \
  WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv0, 0)                     \
  , WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv1, 1),                  \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv2, 2),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv3, 3),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv4, 4),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv5, 5),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv6, 6),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv7, 7),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv8, 8),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv9, 9),                \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv10, 10),              \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv11, 11),              \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv12, 12),              \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv13, 13),              \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv14, 14),              \
      WASM_SIMD_CHECK_LANE(TYPE, value, LANE_TYPE, lv15, 15)

#define WASM_SIMD_CHECK_SPLAT16(TYPE, value, LANE_TYPE, lv)                 \
  WASM_SIMD_CHECK16(TYPE, value, LANE_TYPE, lv, lv, lv, lv, lv, lv, lv, lv, \
                    lv, lv, lv, lv, lv, lv, lv, lv)

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

#define TO_BYTE(val) static_cast<byte>(val)
#define WASM_SIMD_OP(op) kSimdPrefix, TO_BYTE(op)
#define WASM_SIMD_SPLAT(Type, x) x, WASM_SIMD_OP(kExpr##Type##Splat)
#define WASM_SIMD_UNOP(op, x) x, WASM_SIMD_OP(op)
#define WASM_SIMD_BINOP(op, x, y) x, y, WASM_SIMD_OP(op)
#define WASM_SIMD_SHIFT_OP(op, shift, x) x, WASM_SIMD_OP(op), TO_BYTE(shift)
#define WASM_SIMD_SELECT(format, x, y, z) \
  x, y, z, WASM_SIMD_OP(kExprS##format##Select)
// Since boolean vectors can't be checked directly, materialize them into
// integer vectors using a Select operation.
#define WASM_SIMD_MATERIALIZE_BOOLS(format, x) \
  x, WASM_SIMD_I##format##_SPLAT(WASM_ONE),    \
      WASM_SIMD_I##format##_SPLAT(WASM_ZERO),  \
      WASM_SIMD_OP(kExprS##format##Select)

#define WASM_SIMD_I16x8_SPLAT(x) x, WASM_SIMD_OP(kExprI16x8Splat)
#define WASM_SIMD_I16x8_EXTRACT_LANE(lane, x) \
  x, WASM_SIMD_OP(kExprI16x8ExtractLane), TO_BYTE(lane)
#define WASM_SIMD_I16x8_REPLACE_LANE(lane, x, y) \
  x, y, WASM_SIMD_OP(kExprI16x8ReplaceLane), TO_BYTE(lane)
#define WASM_SIMD_I8x16_SPLAT(x) x, WASM_SIMD_OP(kExprI8x16Splat)
#define WASM_SIMD_I8x16_EXTRACT_LANE(lane, x) \
  x, WASM_SIMD_OP(kExprI8x16ExtractLane), TO_BYTE(lane)
#define WASM_SIMD_I8x16_REPLACE_LANE(lane, x, y) \
  x, y, WASM_SIMD_OP(kExprI8x16ReplaceLane), TO_BYTE(lane)

#define WASM_SIMD_F32x4_FROM_I32x4(x) x, WASM_SIMD_OP(kExprF32x4SConvertI32x4)
#define WASM_SIMD_F32x4_FROM_U32x4(x) x, WASM_SIMD_OP(kExprF32x4UConvertI32x4)
#define WASM_SIMD_I32x4_FROM_F32x4(x) x, WASM_SIMD_OP(kExprI32x4SConvertF32x4)
#define WASM_SIMD_U32x4_FROM_F32x4(x) x, WASM_SIMD_OP(kExprI32x4UConvertF32x4)

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

void RunF32x4UnOpTest(WasmOpcode simd_op, FloatUnOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, float, float> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
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
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
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
                       WASM_SIMD_MATERIALIZE_BOOLS(
                           32x4, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                                 WASM_GET_LOCAL(simd1)))),
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

WASM_EXEC_TEST(I16x8Splat) {
  FLAG_wasm_simd_prototype = true;

  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, lane_val), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_EXEC_TEST(I16x8ReplaceLane) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte old_val = 0;
  byte new_val = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(old_val))),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(0, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, old_val, old_val, old_val,
                         old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(1, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, new_val, old_val, old_val,
                         old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(2, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, new_val, new_val, old_val,
                         old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(3, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, new_val, new_val, new_val,
                         old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(4, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, new_val, new_val, new_val,
                         new_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(5, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, new_val, new_val, new_val,
                         new_val, new_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(6, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK8(I16x8, simd, I32, new_val, new_val, new_val, new_val,
                         new_val, new_val, new_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I16x8_REPLACE_LANE(7, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, new_val), WASM_ONE);

  CHECK_EQ(1, r.Call(1, 2));
}

WASM_EXEC_TEST(I8x16Splat) {
  FLAG_wasm_simd_prototype = true;

  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT8(I8x16, simd, I32, lane_val), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_EXEC_TEST(I8x16ReplaceLane) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte old_val = 0;
  byte new_val = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(old_val))),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(0, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(1, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(2, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(3, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          old_val, old_val, old_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(4, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, old_val, old_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(5, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, old_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(6, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, old_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(7, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, old_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(8, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, old_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(9, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, new_val,
                          old_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(10, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, new_val,
                          new_val, old_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(11, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, new_val,
                          new_val, new_val, old_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(12, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(13, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(14, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK16(I8x16, simd, I32, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, new_val,
                          new_val, new_val, new_val, new_val, new_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_I8x16_REPLACE_LANE(15, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, new_val), WASM_ONE);

  CHECK_EQ(1, r.Call(1, 2));
}

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

void RunI32x4UnOpTest(WasmOpcode simd_op, Int32UnOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_EXEC_TEST(I32x4Neg) { RunI32x4UnOpTest(kExprI32x4Neg, Negate); }

WASM_EXEC_TEST(S128Not) { RunI32x4UnOpTest(kExprS128Not, Not); }
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
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I32x4Add) { RunI32x4BinOpTest(kExprI32x4Add, Add); }

WASM_EXEC_TEST(I32x4Sub) { RunI32x4BinOpTest(kExprI32x4Sub, Sub); }

#if V8_TARGET_ARCH_ARM
WASM_EXEC_TEST(I32x4Mul) { RunI32x4BinOpTest(kExprI32x4Mul, Mul); }

WASM_EXEC_TEST(I32x4Min) { RunI32x4BinOpTest(kExprI32x4MinS, Minimum); }

WASM_EXEC_TEST(I32x4Max) { RunI32x4BinOpTest(kExprI32x4MaxS, Maximum); }

WASM_EXEC_TEST(Ui32x4Min) {
  RunI32x4BinOpTest(kExprI32x4MinU, UnsignedMinimum);
}

WASM_EXEC_TEST(Ui32x4Max) {
  RunI32x4BinOpTest(kExprI32x4MaxU, UnsignedMaximum);
}

WASM_EXEC_TEST(S128And) { RunI32x4BinOpTest(kExprS128And, And); }

WASM_EXEC_TEST(S128Or) { RunI32x4BinOpTest(kExprS128Or, Or); }

WASM_EXEC_TEST(S128Xor) { RunI32x4BinOpTest(kExprS128Xor, Xor); }

void RunI32x4CompareOpTest(WasmOpcode simd_op, Int32BinOp expected_op) {
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
                       WASM_SIMD_MATERIALIZE_BOOLS(
                           32x4, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                                 WASM_GET_LOCAL(simd1)))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I32x4Equal) { RunI32x4CompareOpTest(kExprI32x4Eq, Equal); }

WASM_EXEC_TEST(I32x4NotEqual) { RunI32x4CompareOpTest(kExprI32x4Ne, NotEqual); }

WASM_EXEC_TEST(I32x4Greater) { RunI32x4CompareOpTest(kExprI32x4GtS, Greater); }

WASM_EXEC_TEST(I32x4GreaterEqual) {
  RunI32x4CompareOpTest(kExprI32x4GeS, GreaterEqual);
}

WASM_EXEC_TEST(I32x4Less) { RunI32x4CompareOpTest(kExprI32x4LtS, Less); }

WASM_EXEC_TEST(I32x4LessEqual) {
  RunI32x4CompareOpTest(kExprI32x4LeS, LessEqual);
}

WASM_EXEC_TEST(Ui32x4Greater) {
  RunI32x4CompareOpTest(kExprI32x4GtU, UnsignedGreater);
}

WASM_EXEC_TEST(Ui32x4GreaterEqual) {
  RunI32x4CompareOpTest(kExprI32x4GeU, UnsignedGreaterEqual);
}

WASM_EXEC_TEST(Ui32x4Less) {
  RunI32x4CompareOpTest(kExprI32x4LtU, UnsignedLess);
}

WASM_EXEC_TEST(Ui32x4LessEqual) {
  RunI32x4CompareOpTest(kExprI32x4LeU, UnsignedLessEqual);
}

void RunI32x4ShiftOpTest(WasmOpcode simd_op, Int32ShiftOp expected_op,
                         int shift) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
}

WASM_EXEC_TEST(I32x4Shl) {
  RunI32x4ShiftOpTest(kExprI32x4Shl, LogicalShiftLeft, 1);
}

WASM_EXEC_TEST(I32x4ShrS) {
  RunI32x4ShiftOpTest(kExprI32x4ShrS, ArithmeticShiftRight, 1);
}

WASM_EXEC_TEST(I32x4ShrU) {
  RunI32x4ShiftOpTest(kExprI32x4ShrU, LogicalShiftRight, 1);
}

void RunI16x8UnOpTest(WasmOpcode simd_op, Int16UnOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_EXEC_TEST(I16x8Neg) { RunI16x8UnOpTest(kExprI16x8Neg, Negate); }

void RunI16x8BinOpTest(WasmOpcode simd_op, Int16BinOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd1, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) {
    FOR_INT16_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I16x8Add) { RunI16x8BinOpTest(kExprI16x8Add, Add); }

WASM_EXEC_TEST(I16x8AddSaturate) {
  RunI16x8BinOpTest(kExprI16x8AddSaturateS, AddSaturate);
}

WASM_EXEC_TEST(I16x8Sub) { RunI16x8BinOpTest(kExprI16x8Sub, Sub); }

WASM_EXEC_TEST(I16x8SubSaturate) {
  RunI16x8BinOpTest(kExprI16x8SubSaturateS, SubSaturate);
}

WASM_EXEC_TEST(I16x8Mul) { RunI16x8BinOpTest(kExprI16x8Mul, Mul); }

WASM_EXEC_TEST(I16x8Min) { RunI16x8BinOpTest(kExprI16x8MinS, Minimum); }

WASM_EXEC_TEST(I16x8Max) { RunI16x8BinOpTest(kExprI16x8MaxS, Maximum); }

WASM_EXEC_TEST(Ui16x8AddSaturate) {
  RunI16x8BinOpTest(kExprI16x8AddSaturateU, UnsignedAddSaturate);
}

WASM_EXEC_TEST(Ui16x8SubSaturate) {
  RunI16x8BinOpTest(kExprI16x8SubSaturateU, UnsignedSubSaturate);
}

WASM_EXEC_TEST(Ui16x8Min) {
  RunI16x8BinOpTest(kExprI16x8MinU, UnsignedMinimum);
}

WASM_EXEC_TEST(Ui16x8Max) {
  RunI16x8BinOpTest(kExprI16x8MaxU, UnsignedMaximum);
}

void RunI16x8CompareOpTest(WasmOpcode simd_op, Int16BinOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1,
                       WASM_SIMD_MATERIALIZE_BOOLS(
                           16x8, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                                 WASM_GET_LOCAL(simd1)))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd1, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) {
    FOR_INT16_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I16x8Equal) { RunI16x8CompareOpTest(kExprI16x8Eq, Equal); }

WASM_EXEC_TEST(I16x8NotEqual) { RunI16x8CompareOpTest(kExprI16x8Ne, NotEqual); }

WASM_EXEC_TEST(I16x8Greater) { RunI16x8CompareOpTest(kExprI16x8GtS, Greater); }

WASM_EXEC_TEST(I16x8GreaterEqual) {
  RunI16x8CompareOpTest(kExprI16x8GeS, GreaterEqual);
}

WASM_EXEC_TEST(I16x8Less) { RunI16x8CompareOpTest(kExprI16x8LtS, Less); }

WASM_EXEC_TEST(I16x8LessEqual) {
  RunI16x8CompareOpTest(kExprI16x8LeS, LessEqual);
}

WASM_EXEC_TEST(Ui16x8Greater) {
  RunI16x8CompareOpTest(kExprI16x8GtU, UnsignedGreater);
}

WASM_EXEC_TEST(Ui16x8GreaterEqual) {
  RunI16x8CompareOpTest(kExprI16x8GeU, UnsignedGreaterEqual);
}

WASM_EXEC_TEST(Ui16x8Less) {
  RunI16x8CompareOpTest(kExprI16x8LtU, UnsignedLess);
}

WASM_EXEC_TEST(Ui16x8LessEqual) {
  RunI16x8CompareOpTest(kExprI16x8LeU, UnsignedLessEqual);
}

void RunI16x8ShiftOpTest(WasmOpcode simd_op, Int16ShiftOp expected_op,
                         int shift) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
}

WASM_EXEC_TEST(I16x8Shl) {
  RunI16x8ShiftOpTest(kExprI16x8Shl, LogicalShiftLeft, 1);
}

WASM_EXEC_TEST(I16x8ShrS) {
  RunI16x8ShiftOpTest(kExprI16x8ShrS, ArithmeticShiftRight, 1);
}

WASM_EXEC_TEST(I16x8ShrU) {
  RunI16x8ShiftOpTest(kExprI16x8ShrU, LogicalShiftRight, 1);
}

void RunI8x16UnOpTest(WasmOpcode simd_op, Int8UnOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_EXEC_TEST(I8x16Neg) { RunI8x16UnOpTest(kExprI8x16Neg, Negate); }

void RunI8x16BinOpTest(WasmOpcode simd_op, Int8BinOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd1, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) {
    FOR_INT8_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I8x16Add) { RunI8x16BinOpTest(kExprI8x16Add, Add); }

WASM_EXEC_TEST(I8x16AddSaturate) {
  RunI8x16BinOpTest(kExprI8x16AddSaturateS, AddSaturate);
}

WASM_EXEC_TEST(I8x16Sub) { RunI8x16BinOpTest(kExprI8x16Sub, Sub); }

WASM_EXEC_TEST(I8x16SubSaturate) {
  RunI8x16BinOpTest(kExprI8x16SubSaturateS, SubSaturate);
}

WASM_EXEC_TEST(I8x16Mul) { RunI8x16BinOpTest(kExprI8x16Mul, Mul); }

WASM_EXEC_TEST(I8x16Min) { RunI8x16BinOpTest(kExprI8x16MinS, Minimum); }

WASM_EXEC_TEST(I8x16Max) { RunI8x16BinOpTest(kExprI8x16MaxS, Maximum); }

WASM_EXEC_TEST(Ui8x16AddSaturate) {
  RunI8x16BinOpTest(kExprI8x16AddSaturateU, UnsignedAddSaturate);
}

WASM_EXEC_TEST(Ui8x16SubSaturate) {
  RunI8x16BinOpTest(kExprI8x16SubSaturateU, UnsignedSubSaturate);
}

WASM_EXEC_TEST(Ui8x16Min) {
  RunI8x16BinOpTest(kExprI8x16MinU, UnsignedMinimum);
}

WASM_EXEC_TEST(Ui8x16Max) {
  RunI8x16BinOpTest(kExprI8x16MaxU, UnsignedMaximum);
}

void RunI8x16CompareOpTest(WasmOpcode simd_op, Int8BinOp expected_op) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1,
                       WASM_SIMD_MATERIALIZE_BOOLS(
                           8x16, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                                 WASM_GET_LOCAL(simd1)))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd1, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) {
    FOR_INT8_INPUTS(j) { CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j))); }
  }
}

WASM_EXEC_TEST(I8x16Equal) { RunI8x16CompareOpTest(kExprI8x16Eq, Equal); }

WASM_EXEC_TEST(I8x16NotEqual) { RunI8x16CompareOpTest(kExprI8x16Ne, NotEqual); }

WASM_EXEC_TEST(I8x16Greater) { RunI8x16CompareOpTest(kExprI8x16GtS, Greater); }

WASM_EXEC_TEST(I8x16GreaterEqual) {
  RunI8x16CompareOpTest(kExprI8x16GeS, GreaterEqual);
}

WASM_EXEC_TEST(I8x16Less) { RunI8x16CompareOpTest(kExprI8x16LtS, Less); }

WASM_EXEC_TEST(I8x16LessEqual) {
  RunI8x16CompareOpTest(kExprI8x16LeS, LessEqual);
}

WASM_EXEC_TEST(Ui8x16Greater) {
  RunI8x16CompareOpTest(kExprI8x16GtU, UnsignedGreater);
}

WASM_EXEC_TEST(Ui8x16GreaterEqual) {
  RunI8x16CompareOpTest(kExprI8x16GeU, UnsignedGreaterEqual);
}

WASM_EXEC_TEST(Ui8x16Less) {
  RunI8x16CompareOpTest(kExprI8x16LtU, UnsignedLess);
}

WASM_EXEC_TEST(Ui8x16LessEqual) {
  RunI8x16CompareOpTest(kExprI8x16LeU, UnsignedLessEqual);
}

void RunI8x16ShiftOpTest(WasmOpcode simd_op, Int8ShiftOp expected_op,
                         int shift) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
}

WASM_EXEC_TEST(I8x16Shl) {
  RunI8x16ShiftOpTest(kExprI8x16Shl, LogicalShiftLeft, 1);
}

WASM_EXEC_TEST(I8x16ShrS) {
  RunI8x16ShiftOpTest(kExprI8x16ShrS, ArithmeticShiftRight, 1);
}

WASM_EXEC_TEST(I8x16ShrU) {
  RunI8x16ShiftOpTest(kExprI8x16ShrU, LogicalShiftRight, 1);
}

// Test Select by making a mask where the first two lanes are true and the rest
// false, and comparing for non-equality with zero to materialize a bool vector.
#define WASM_SIMD_SELECT_TEST(format)                                         \
  WASM_EXEC_TEST(S##format##Select) {                                         \
    FLAG_wasm_simd_prototype = true;                                          \
    WasmRunner<int32_t, int32_t, int32_t> r(kExecuteCompiled);                \
    byte val1 = 0;                                                            \
    byte val2 = 1;                                                            \
    byte src1 = r.AllocateLocal(kWasmS128);                                   \
    byte src2 = r.AllocateLocal(kWasmS128);                                   \
    byte zero = r.AllocateLocal(kWasmS128);                                   \
    byte mask = r.AllocateLocal(kWasmS128);                                   \
    BUILD(r, WASM_SET_LOCAL(                                                  \
                 src1, WASM_SIMD_I##format##_SPLAT(WASM_GET_LOCAL(val1))),    \
          WASM_SET_LOCAL(src2,                                                \
                         WASM_SIMD_I##format##_SPLAT(WASM_GET_LOCAL(val2))),  \
          WASM_SET_LOCAL(zero, WASM_SIMD_I##format##_SPLAT(WASM_ZERO)),       \
          WASM_SET_LOCAL(mask, WASM_SIMD_I##format##_REPLACE_LANE(            \
                                   1, WASM_GET_LOCAL(zero), WASM_I32V(-1))),  \
          WASM_SET_LOCAL(mask, WASM_SIMD_I##format##_REPLACE_LANE(            \
                                   2, WASM_GET_LOCAL(mask), WASM_I32V(-1))),  \
          WASM_SET_LOCAL(                                                     \
              mask,                                                           \
              WASM_SIMD_SELECT(format, WASM_SIMD_BINOP(kExprI##format##Ne,    \
                                                       WASM_GET_LOCAL(mask),  \
                                                       WASM_GET_LOCAL(zero)), \
                               WASM_GET_LOCAL(src1), WASM_GET_LOCAL(src2))),  \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val2, 0),                \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val1, 1),                \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val1, 2),                \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val2, 3), WASM_ONE);     \
                                                                              \
    CHECK_EQ(1, r.Call(0x12, 0x34));                                          \
  }

WASM_SIMD_SELECT_TEST(32x4)
WASM_SIMD_SELECT_TEST(16x8)
WASM_SIMD_SELECT_TEST(8x16)
#endif  // V8_TARGET_ARCH_ARM
