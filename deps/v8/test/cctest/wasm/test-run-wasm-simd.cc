// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_simd {

namespace {

typedef float (*FloatUnOp)(float);
typedef float (*FloatBinOp)(float, float);
typedef int (*FloatCompareOp)(float, float);
typedef int32_t (*Int32UnOp)(int32_t);
typedef int32_t (*Int32BinOp)(int32_t, int32_t);
typedef int (*Int32CompareOp)(int32_t, int32_t);
typedef int32_t (*Int32ShiftOp)(int32_t, int);
typedef int16_t (*Int16UnOp)(int16_t);
typedef int16_t (*Int16BinOp)(int16_t, int16_t);
typedef int (*Int16CompareOp)(int16_t, int16_t);
typedef int16_t (*Int16ShiftOp)(int16_t, int);
typedef int8_t (*Int8UnOp)(int8_t);
typedef int8_t (*Int8BinOp)(int8_t, int8_t);
typedef int (*Int8CompareOp)(int8_t, int8_t);
typedef int8_t (*Int8ShiftOp)(int8_t, int);

#define WASM_SIMD_TEST(name)                        \
  void RunWasm_##name##_Impl(LowerSimd lower_simd); \
  TEST(RunWasm_##name##_compiled) {                 \
    EXPERIMENTAL_FLAG_SCOPE(simd);                  \
    RunWasm_##name##_Impl(kNoLowerSimd);            \
  }                                                 \
  TEST(RunWasm_##name##_simd_lowered) {             \
    EXPERIMENTAL_FLAG_SCOPE(simd);                  \
    RunWasm_##name##_Impl(kLowerSimd);              \
  }                                                 \
  void RunWasm_##name##_Impl(LowerSimd lower_simd)

#define WASM_SIMD_COMPILED_TEST(name)               \
  void RunWasm_##name##_Impl(LowerSimd lower_simd); \
  TEST(RunWasm_##name##_compiled) {                 \
    EXPERIMENTAL_FLAG_SCOPE(simd);                  \
    RunWasm_##name##_Impl(kNoLowerSimd);            \
  }                                                 \
  void RunWasm_##name##_Impl(LowerSimd lower_simd)

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
T Div(T a, T b) {
  return a / b;
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
int Equal(T a, T b) {
  return a == b ? -1 : 0;
}

template <typename T>
int NotEqual(T a, T b) {
  return a != b ? -1 : 0;
}

template <typename T>
int Less(T a, T b) {
  return a < b ? -1 : 0;
}

template <typename T>
int LessEqual(T a, T b) {
  return a <= b ? -1 : 0;
}

template <typename T>
int Greater(T a, T b) {
  return a > b ? -1 : 0;
}

template <typename T>
int GreaterEqual(T a, T b) {
  return a >= b ? -1 : 0;
}

template <typename T>
int UnsignedLess(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) < static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
int UnsignedLessEqual(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) <= static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
int UnsignedGreater(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) > static_cast<UnsignedT>(b) ? -1 : 0;
}

template <typename T>
int UnsignedGreaterEqual(T a, T b) {
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<UnsignedT>(a) >= static_cast<UnsignedT>(b) ? -1 : 0;
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
T Clamp(int64_t value) {
  static_assert(sizeof(int64_t) > sizeof(T), "T must be int32_t or smaller");
  int64_t min = static_cast<int64_t>(std::numeric_limits<T>::min());
  int64_t max = static_cast<int64_t>(std::numeric_limits<T>::max());
  int64_t clamped = std::max(min, std::min(max, value));
  return static_cast<T>(clamped);
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
T Narrow(int64_t value) {
  return Clamp<T>(value);
}

template <typename T>
T UnsignedNarrow(int64_t value) {
  static_assert(sizeof(int64_t) > sizeof(T), "T must be int32_t or smaller");
  using UnsignedT = typename std::make_unsigned<T>::type;
  return static_cast<T>(Clamp<UnsignedT>(value & 0xFFFFFFFFu));
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

template <typename T>
T LogicalNot(T a) {
  return a == 0 ? -1 : 0;
}

template <typename T>
T Sqrt(T a) {
  return std::sqrt(a);
}

template <typename T>
T Recip(T a) {
  return 1.0f / a;
}

template <typename T>
T RecipSqrt(T a) {
  return 1.0f / std::sqrt(a);
}

}  // namespace

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

#define WASM_SIMD_CHECK_F32_LANE(value, lane_value, lane_index)             \
  WASM_IF(WASM_F32_NE(WASM_GET_LOCAL(lane_value),                           \
                      WASM_SIMD_F32x4_EXTRACT_LANE(lane_index,              \
                                                   WASM_GET_LOCAL(value))), \
          WASM_RETURN1(WASM_ZERO))

#define WASM_SIMD_CHECK_F32x4(value, lv0, lv1, lv2, lv3) \
  WASM_SIMD_CHECK_F32_LANE(value, lv0, 0)                \
  , WASM_SIMD_CHECK_F32_LANE(value, lv1, 1),             \
      WASM_SIMD_CHECK_F32_LANE(value, lv2, 2),           \
      WASM_SIMD_CHECK_F32_LANE(value, lv3, 3)

#define WASM_SIMD_CHECK_SPLAT_F32x4(value, lv) \
  WASM_SIMD_CHECK_F32x4(value, lv, lv, lv, lv)

#define WASM_SIMD_CHECK_F32_LANE_ESTIMATE(value, low, high, lane_index)       \
  WASM_IF(WASM_F32_GT(WASM_GET_LOCAL(low),                                    \
                      WASM_SIMD_F32x4_EXTRACT_LANE(lane_index,                \
                                                   WASM_GET_LOCAL(value))),   \
          WASM_RETURN1(WASM_ZERO))                                            \
  , WASM_IF(WASM_F32_LT(WASM_GET_LOCAL(high),                                 \
                        WASM_SIMD_F32x4_EXTRACT_LANE(lane_index,              \
                                                     WASM_GET_LOCAL(value))), \
            WASM_RETURN1(WASM_ZERO))

#define WASM_SIMD_CHECK_SPLAT_F32x4_ESTIMATE(value, low, high) \
  WASM_SIMD_CHECK_F32_LANE_ESTIMATE(value, low, high, 0)       \
  , WASM_SIMD_CHECK_F32_LANE_ESTIMATE(value, low, high, 1),    \
      WASM_SIMD_CHECK_F32_LANE_ESTIMATE(value, low, high, 2),  \
      WASM_SIMD_CHECK_F32_LANE_ESTIMATE(value, low, high, 3)

#define TO_BYTE(val) static_cast<byte>(val)
#define WASM_SIMD_OP(op) kSimdPrefix, TO_BYTE(op)
#define WASM_SIMD_SPLAT(Type, x) x, WASM_SIMD_OP(kExpr##Type##Splat)
#define WASM_SIMD_UNOP(op, x) x, WASM_SIMD_OP(op)
#define WASM_SIMD_BINOP(op, x, y) x, y, WASM_SIMD_OP(op)
#define WASM_SIMD_SHIFT_OP(op, shift, x) x, WASM_SIMD_OP(op), TO_BYTE(shift)
#define WASM_SIMD_CONCAT_OP(op, bytes, x, y) \
  x, y, WASM_SIMD_OP(op), TO_BYTE(bytes)
#define WASM_SIMD_SELECT(format, x, y, z) x, y, z, WASM_SIMD_OP(kExprS128Select)
#define WASM_SIMD_F32x4_SPLAT(x) x, WASM_SIMD_OP(kExprF32x4Splat)
#define WASM_SIMD_F32x4_EXTRACT_LANE(lane, x) \
  x, WASM_SIMD_OP(kExprF32x4ExtractLane), TO_BYTE(lane)
#define WASM_SIMD_F32x4_REPLACE_LANE(lane, x, y) \
  x, y, WASM_SIMD_OP(kExprF32x4ReplaceLane), TO_BYTE(lane)

#define WASM_SIMD_I32x4_SPLAT(x) x, WASM_SIMD_OP(kExprI32x4Splat)
#define WASM_SIMD_I32x4_EXTRACT_LANE(lane, x) \
  x, WASM_SIMD_OP(kExprI32x4ExtractLane), TO_BYTE(lane)
#define WASM_SIMD_I32x4_REPLACE_LANE(lane, x, y) \
  x, y, WASM_SIMD_OP(kExprI32x4ReplaceLane), TO_BYTE(lane)

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

#define WASM_SIMD_S8x16_SHUFFLE_OP(opcode, m, x, y)                        \
  x, y, WASM_SIMD_OP(opcode), TO_BYTE(m[0]), TO_BYTE(m[1]), TO_BYTE(m[2]), \
      TO_BYTE(m[3]), TO_BYTE(m[4]), TO_BYTE(m[5]), TO_BYTE(m[6]),          \
      TO_BYTE(m[7]), TO_BYTE(m[8]), TO_BYTE(m[9]), TO_BYTE(m[10]),         \
      TO_BYTE(m[11]), TO_BYTE(m[12]), TO_BYTE(m[13]), TO_BYTE(m[14]),      \
      TO_BYTE(m[15])

#define WASM_SIMD_LOAD_MEM(index) \
  index, WASM_SIMD_OP(kExprS128LoadMem), ZERO_ALIGNMENT, ZERO_OFFSET
#define WASM_SIMD_STORE_MEM(index, val) \
  index, val, WASM_SIMD_OP(kExprS128StoreMem), ZERO_ALIGNMENT, ZERO_OFFSET

// Skip FP tests involving extremely large or extremely small values, which
// may fail due to non-IEEE-754 SIMD arithmetic on some platforms.
bool SkipFPValue(float x) {
  float abs_x = std::fabs(x);
  const float kSmallFloatThreshold = 1.0e-32f;
  const float kLargeFloatThreshold = 1.0e32f;
  return abs_x != 0.0f &&  // 0 or -0 are fine.
         (abs_x < kSmallFloatThreshold || abs_x > kLargeFloatThreshold);
}

// Skip tests where the expected value is a NaN, since our wasm test code
// doesn't handle NaNs. Also skip extreme values.
bool SkipFPExpectedValue(float x) { return std::isnan(x) || SkipFPValue(x); }

WASM_SIMD_TEST(F32x4Splat) {
  WasmRunner<int32_t, float> r(kExecuteTurbofan, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT_F32x4(simd, lane_val), WASM_RETURN1(WASM_ONE));

  FOR_FLOAT32_INPUTS(i) {
    if (SkipFPExpectedValue(*i)) continue;
    CHECK_EQ(1, r.Call(*i));
  }
}

WASM_SIMD_TEST(F32x4ReplaceLane) {
  WasmRunner<int32_t, float, float> r(kExecuteTurbofan, lower_simd);
  byte old_val = 0;
  byte new_val = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(old_val))),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(0, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_F32x4(simd, new_val, old_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(1, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_F32x4(simd, new_val, new_val, old_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(2, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_F32x4(simd, new_val, new_val, new_val, old_val),
        WASM_SET_LOCAL(simd,
                       WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_LOCAL(simd),
                                                    WASM_GET_LOCAL(new_val))),
        WASM_SIMD_CHECK_SPLAT_F32x4(simd, new_val), WASM_RETURN1(WASM_ONE));

  CHECK_EQ(1, r.Call(3.14159f, -1.5f));
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
// Tests both signed and unsigned conversion.
WASM_SIMD_TEST(F32x4ConvertI32x4) {
  WasmRunner<int32_t, int32_t, float, float> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected_signed = 1;
  byte expected_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_UNOP(kExprF32x4SConvertI32x4,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT_F32x4(simd1, expected_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_UNOP(kExprF32x4UConvertI32x4,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT_F32x4(simd2, expected_unsigned),
        WASM_RETURN1(WASM_ONE));

  FOR_INT32_INPUTS(i) {
    CHECK_EQ(1, r.Call(*i, static_cast<float>(*i),
                       static_cast<float>(static_cast<uint32_t>(*i))));
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

void RunF32x4UnOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                      FloatUnOp expected_op, float error = 0.0f) {
  WasmRunner<int32_t, float, float, float> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte low = 1;
  byte high = 2;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT_F32x4_ESTIMATE(simd, low, high),
        WASM_RETURN1(WASM_ONE));

  FOR_FLOAT32_INPUTS(i) {
    if (SkipFPValue(*i)) continue;
    float expected = expected_op(*i);
    if (SkipFPExpectedValue(expected)) continue;
    float abs_error = std::abs(expected) * error;
    CHECK_EQ(1, r.Call(*i, expected - abs_error, expected + abs_error));
  }
}

WASM_SIMD_TEST(F32x4Abs) {
  RunF32x4UnOpTest(lower_simd, kExprF32x4Abs, std::abs);
}
WASM_SIMD_TEST(F32x4Neg) {
  RunF32x4UnOpTest(lower_simd, kExprF32x4Neg, Negate);
}

static const float kApproxError = 0.01f;

WASM_SIMD_TEST(F32x4RecipApprox) {
  RunF32x4UnOpTest(lower_simd, kExprF32x4RecipApprox, Recip, kApproxError);
}

WASM_SIMD_TEST(F32x4RecipSqrtApprox) {
  RunF32x4UnOpTest(lower_simd, kExprF32x4RecipSqrtApprox, RecipSqrt,
                   kApproxError);
}

void RunF32x4BinOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                       FloatBinOp expected_op) {
  WasmRunner<int32_t, float, float, float> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT_F32x4(simd1, expected), WASM_RETURN1(WASM_ONE));

  FOR_FLOAT32_INPUTS(i) {
    if (SkipFPValue(*i)) continue;
    FOR_FLOAT32_INPUTS(j) {
      if (SkipFPValue(*j)) continue;
      float expected = expected_op(*i, *j);
      if (SkipFPExpectedValue(expected)) continue;
      CHECK_EQ(1, r.Call(*i, *j, expected));
    }
  }
}

WASM_SIMD_TEST(F32x4Add) { RunF32x4BinOpTest(lower_simd, kExprF32x4Add, Add); }
WASM_SIMD_TEST(F32x4Sub) { RunF32x4BinOpTest(lower_simd, kExprF32x4Sub, Sub); }
WASM_SIMD_TEST(F32x4Mul) { RunF32x4BinOpTest(lower_simd, kExprF32x4Mul, Mul); }
WASM_SIMD_TEST(F32x4_Min) {
  RunF32x4BinOpTest(lower_simd, kExprF32x4Min, JSMin);
}
WASM_SIMD_TEST(F32x4_Max) {
  RunF32x4BinOpTest(lower_simd, kExprF32x4Max, JSMax);
}

void RunF32x4CompareOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                           FloatCompareOp expected_op) {
  WasmRunner<int32_t, float, float, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte b = 1;
  byte expected = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(simd_op, WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) {
    if (SkipFPValue(*i)) continue;
    FOR_FLOAT32_INPUTS(j) {
      if (SkipFPValue(*j)) continue;
      float diff = *i - *j;
      if (SkipFPExpectedValue(diff)) continue;
      CHECK_EQ(1, r.Call(*i, *j, expected_op(*i, *j)));
    }
  }
}

WASM_SIMD_TEST(F32x4Eq) {
  RunF32x4CompareOpTest(lower_simd, kExprF32x4Eq, Equal);
}

WASM_SIMD_TEST(F32x4Ne) {
  RunF32x4CompareOpTest(lower_simd, kExprF32x4Ne, NotEqual);
}

WASM_SIMD_TEST(F32x4Gt) {
  RunF32x4CompareOpTest(lower_simd, kExprF32x4Gt, Greater);
}

WASM_SIMD_TEST(F32x4Ge) {
  RunF32x4CompareOpTest(lower_simd, kExprF32x4Ge, GreaterEqual);
}

WASM_SIMD_TEST(F32x4Lt) {
  RunF32x4CompareOpTest(lower_simd, kExprF32x4Lt, Less);
}

WASM_SIMD_TEST(F32x4Le) {
  RunF32x4CompareOpTest(lower_simd, kExprF32x4Le, LessEqual);
}

WASM_SIMD_TEST(I32x4Splat) {
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
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, lane_val), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_SIMD_TEST(I32x4ReplaceLane) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
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

WASM_SIMD_TEST(I16x8Splat) {
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, lane_val), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_SIMD_TEST(I16x8ReplaceLane) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
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

WASM_SIMD_TEST(I8x16Splat) {
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT8(I8x16, simd, I32, lane_val), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_SIMD_TEST(I8x16ReplaceLane) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
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

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64
// Determines if conversion from float to int will be valid.
bool CanRoundToZeroAndConvert(double val, bool unsigned_integer) {
  const double max_uint = static_cast<double>(0xFFFFFFFFu);
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
      return (val < 0) ? 0 : 0xFFFFFFFFu;
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
WASM_SIMD_TEST(I32x4ConvertF32x4) {
  WasmRunner<int32_t, float, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected_signed = 1;
  byte expected_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_UNOP(kExprI32x4SConvertF32x4,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, expected_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_UNOP(kExprI32x4UConvertF32x4,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd2, I32, expected_unsigned), WASM_ONE);

  FOR_FLOAT32_INPUTS(i) {
    if (SkipFPValue(*i)) continue;
    int32_t signed_value = ConvertToInt(*i, false);
    int32_t unsigned_value = ConvertToInt(*i, true);
    CHECK_EQ(1, r.Call(*i, signed_value, unsigned_value));
  }
}

// Tests both signed and unsigned conversion from I16x8 (unpacking).
WASM_SIMD_COMPILED_TEST(I32x4ConvertI16x8) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
  byte a = 0;
  byte unpacked_signed = 1;
  byte unpacked_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_UNOP(kExprI32x4SConvertI16x8Low,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, unpacked_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_UNOP(kExprI32x4UConvertI16x8High,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd2, I32, unpacked_unsigned), WASM_ONE);

  FOR_INT16_INPUTS(i) {
    int32_t unpacked_signed = static_cast<int32_t>(Widen<int16_t>(*i));
    int32_t unpacked_unsigned =
        static_cast<int32_t>(UnsignedWiden<int16_t>(*i));
    CHECK_EQ(1, r.Call(*i, unpacked_signed, unpacked_unsigned));
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64

void RunI32x4UnOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                      Int32UnOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_SIMD_TEST(I32x4Neg) {
  RunI32x4UnOpTest(lower_simd, kExprI32x4Neg, Negate);
}

WASM_SIMD_TEST(S128Not) { RunI32x4UnOpTest(lower_simd, kExprS128Not, Not); }

void RunI32x4BinOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                       Int32BinOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
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

WASM_SIMD_TEST(I32x4Add) { RunI32x4BinOpTest(lower_simd, kExprI32x4Add, Add); }

WASM_SIMD_TEST(I32x4Sub) { RunI32x4BinOpTest(lower_simd, kExprI32x4Sub, Sub); }

WASM_SIMD_TEST(I32x4Mul) { RunI32x4BinOpTest(lower_simd, kExprI32x4Mul, Mul); }

WASM_SIMD_TEST(I32x4MinS) {
  RunI32x4BinOpTest(lower_simd, kExprI32x4MinS, Minimum);
}

WASM_SIMD_TEST(I32x4MaxS) {
  RunI32x4BinOpTest(lower_simd, kExprI32x4MaxS, Maximum);
}

WASM_SIMD_TEST(I32x4MinU) {
  RunI32x4BinOpTest(lower_simd, kExprI32x4MinU, UnsignedMinimum);
}

WASM_SIMD_TEST(I32x4MaxU) {
  RunI32x4BinOpTest(lower_simd, kExprI32x4MaxU, UnsignedMaximum);
}

WASM_SIMD_TEST(S128And) { RunI32x4BinOpTest(lower_simd, kExprS128And, And); }

WASM_SIMD_TEST(S128Or) { RunI32x4BinOpTest(lower_simd, kExprS128Or, Or); }

WASM_SIMD_TEST(S128Xor) { RunI32x4BinOpTest(lower_simd, kExprS128Xor, Xor); }

void RunI32x4CompareOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                           Int32CompareOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
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

WASM_SIMD_TEST(I32x4Eq) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4Eq, Equal);
}

WASM_SIMD_TEST(I32x4Ne) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4Ne, NotEqual);
}

WASM_SIMD_TEST(I32x4LtS) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4LtS, Less);
}

WASM_SIMD_TEST(I32x4LeS) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4LeS, LessEqual);
}

WASM_SIMD_TEST(I32x4GtS) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4GtS, Greater);
}

WASM_SIMD_TEST(I32x4GeS) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4GeS, GreaterEqual);
}

WASM_SIMD_TEST(I32x4LtU) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4LtU, UnsignedLess);
}

WASM_SIMD_TEST(I32x4LeU) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4LeU, UnsignedLessEqual);
}

WASM_SIMD_TEST(I32x4GtU) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I32x4GeU) {
  RunI32x4CompareOpTest(lower_simd, kExprI32x4GeU, UnsignedGreaterEqual);
}

void RunI32x4ShiftOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                         Int32ShiftOp expected_op, int shift) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
}

WASM_SIMD_TEST(I32x4Shl) {
  RunI32x4ShiftOpTest(lower_simd, kExprI32x4Shl, LogicalShiftLeft, 1);
}

WASM_SIMD_TEST(I32x4ShrS) {
  RunI32x4ShiftOpTest(lower_simd, kExprI32x4ShrS, ArithmeticShiftRight, 1);
}

WASM_SIMD_TEST(I32x4ShrU) {
  RunI32x4ShiftOpTest(lower_simd, kExprI32x4ShrU, LogicalShiftRight, 1);
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64
// Tests both signed and unsigned conversion from I8x16 (unpacking).
WASM_SIMD_COMPILED_TEST(I16x8ConvertI8x16) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
  byte a = 0;
  byte unpacked_signed = 1;
  byte unpacked_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_UNOP(kExprI16x8SConvertI8x16Low,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd1, I32, unpacked_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_UNOP(kExprI16x8UConvertI8x16High,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd2, I32, unpacked_unsigned), WASM_ONE);

  FOR_INT8_INPUTS(i) {
    int32_t unpacked_signed = static_cast<int32_t>(Widen<int8_t>(*i));
    int32_t unpacked_unsigned = static_cast<int32_t>(UnsignedWiden<int8_t>(*i));
    CHECK_EQ(1, r.Call(*i, unpacked_signed, unpacked_unsigned));
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64

void RunI16x8UnOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                      Int16UnOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_SIMD_TEST(I16x8Neg) {
  RunI16x8UnOpTest(lower_simd, kExprI16x8Neg, Negate);
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64
// Tests both signed and unsigned conversion from I32x4 (packing).
WASM_SIMD_COMPILED_TEST(I16x8ConvertI32x4) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
  byte a = 0;
  byte packed_signed = 1;
  byte packed_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(kExprI16x8SConvertI32x4,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd1, I32, packed_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_BINOP(kExprI16x8UConvertI32x4,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd2, I32, packed_unsigned), WASM_ONE);

  FOR_INT32_INPUTS(i) {
    int32_t packed_signed = Narrow<int16_t>(*i);
    int32_t packed_unsigned = UnsignedNarrow<int16_t>(*i);
    // Sign-extend here, since ExtractLane sign extends.
    if (packed_unsigned & 0x8000) packed_unsigned |= 0xFFFF0000;
    CHECK_EQ(1, r.Call(*i, packed_signed, packed_unsigned));
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64

void RunI16x8BinOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                       Int16BinOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
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

WASM_SIMD_TEST(I16x8Add) { RunI16x8BinOpTest(lower_simd, kExprI16x8Add, Add); }

WASM_SIMD_TEST(I16x8AddSaturateS) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8AddSaturateS, AddSaturate);
}

WASM_SIMD_TEST(I16x8Sub) { RunI16x8BinOpTest(lower_simd, kExprI16x8Sub, Sub); }

WASM_SIMD_TEST(I16x8SubSaturateS) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8SubSaturateS, SubSaturate);
}

WASM_SIMD_TEST(I16x8Mul) { RunI16x8BinOpTest(lower_simd, kExprI16x8Mul, Mul); }

WASM_SIMD_TEST(I16x8MinS) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8MinS, Minimum);
}

WASM_SIMD_TEST(I16x8MaxS) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8MaxS, Maximum);
}

WASM_SIMD_TEST(I16x8AddSaturateU) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8AddSaturateU, UnsignedAddSaturate);
}

WASM_SIMD_TEST(I16x8SubSaturateU) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8SubSaturateU, UnsignedSubSaturate);
}

WASM_SIMD_TEST(I16x8MinU) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8MinU, UnsignedMinimum);
}

WASM_SIMD_TEST(I16x8MaxU) {
  RunI16x8BinOpTest(lower_simd, kExprI16x8MaxU, UnsignedMaximum);
}

void RunI16x8CompareOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                           Int16CompareOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
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

WASM_SIMD_TEST(I16x8Eq) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8Eq, Equal);
}

WASM_SIMD_TEST(I16x8Ne) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8Ne, NotEqual);
}

WASM_SIMD_TEST(I16x8LtS) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8LtS, Less);
}

WASM_SIMD_TEST(I16x8LeS) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8LeS, LessEqual);
}

WASM_SIMD_TEST(I16x8GtS) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8GtS, Greater);
}

WASM_SIMD_TEST(I16x8GeS) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8GeS, GreaterEqual);
}

WASM_SIMD_TEST(I16x8GtU) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I16x8GeU) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8GeU, UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I16x8LtU) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8LtU, UnsignedLess);
}

WASM_SIMD_TEST(I16x8LeU) {
  RunI16x8CompareOpTest(lower_simd, kExprI16x8LeU, UnsignedLessEqual);
}

void RunI16x8ShiftOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                         Int16ShiftOp expected_op, int shift) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
}

WASM_SIMD_TEST(I16x8Shl) {
  RunI16x8ShiftOpTest(lower_simd, kExprI16x8Shl, LogicalShiftLeft, 1);
}

WASM_SIMD_TEST(I16x8ShrS) {
  RunI16x8ShiftOpTest(lower_simd, kExprI16x8ShrS, ArithmeticShiftRight, 1);
}

WASM_SIMD_TEST(I16x8ShrU) {
  RunI16x8ShiftOpTest(lower_simd, kExprI16x8ShrU, LogicalShiftRight, 1);
}

void RunI8x16UnOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                      Int8UnOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_SIMD_TEST(I8x16Neg) {
  RunI8x16UnOpTest(lower_simd, kExprI8x16Neg, Negate);
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64
// Tests both signed and unsigned conversion from I16x8 (packing).
WASM_SIMD_COMPILED_TEST(I8x16ConvertI16x8) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
  byte a = 0;
  byte packed_signed = 1;
  byte packed_unsigned = 2;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_BINOP(kExprI8x16SConvertI16x8,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd1, I32, packed_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_BINOP(kExprI8x16UConvertI16x8,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd2, I32, packed_unsigned), WASM_ONE);

  FOR_INT16_INPUTS(i) {
    int32_t packed_signed = Narrow<int8_t>(*i);
    int32_t packed_unsigned = UnsignedNarrow<int8_t>(*i);
    // Sign-extend here, since ExtractLane sign extends.
    if (packed_unsigned & 0x80) packed_unsigned |= 0xFFFFFF00;
    CHECK_EQ(1, r.Call(*i, packed_signed, packed_unsigned));
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64

void RunI8x16BinOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                       Int8BinOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
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

WASM_SIMD_TEST(I8x16Add) { RunI8x16BinOpTest(lower_simd, kExprI8x16Add, Add); }

WASM_SIMD_TEST(I8x16AddSaturateS) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16AddSaturateS, AddSaturate);
}

WASM_SIMD_TEST(I8x16Sub) { RunI8x16BinOpTest(lower_simd, kExprI8x16Sub, Sub); }

WASM_SIMD_TEST(I8x16SubSaturateS) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16SubSaturateS, SubSaturate);
}

WASM_SIMD_TEST(I8x16MinS) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16MinS, Minimum);
}

WASM_SIMD_TEST(I8x16MaxS) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16MaxS, Maximum);
}

WASM_SIMD_TEST(I8x16AddSaturateU) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16AddSaturateU, UnsignedAddSaturate);
}

WASM_SIMD_TEST(I8x16SubSaturateU) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16SubSaturateU, UnsignedSubSaturate);
}

WASM_SIMD_TEST(I8x16MinU) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16MinU, UnsignedMinimum);
}

WASM_SIMD_TEST(I8x16MaxU) {
  RunI8x16BinOpTest(lower_simd, kExprI8x16MaxU, UnsignedMaximum);
}

void RunI8x16CompareOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                           Int8CompareOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,
                                                   lower_simd);
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

WASM_SIMD_TEST(I8x16Eq) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16Eq, Equal);
}

WASM_SIMD_TEST(I8x16Ne) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16Ne, NotEqual);
}

WASM_SIMD_TEST(I8x16GtS) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16GtS, Greater);
}

WASM_SIMD_TEST(I8x16GeS) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16GeS, GreaterEqual);
}

WASM_SIMD_TEST(I8x16LtS) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16LtS, Less);
}

WASM_SIMD_TEST(I8x16LeS) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16LeS, LessEqual);
}

WASM_SIMD_TEST(I8x16GtU) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16GtU, UnsignedGreater);
}

WASM_SIMD_TEST(I8x16GeU) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16GeU, UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I8x16LtU) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16LtU, UnsignedLess);
}

WASM_SIMD_TEST(I8x16LeU) {
  RunI8x16CompareOpTest(lower_simd, kExprI8x16LeU, UnsignedLessEqual);
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
WASM_SIMD_TEST(I8x16Mul) { RunI8x16BinOpTest(lower_simd, kExprI8x16Mul, Mul); }
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

void RunI8x16ShiftOpTest(LowerSimd lower_simd, WasmOpcode simd_op,
                         Int8ShiftOp expected_op, int shift) {
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
WASM_SIMD_TEST(I8x16Shl) {
  RunI8x16ShiftOpTest(lower_simd, kExprI8x16Shl, LogicalShiftLeft, 1);
}

WASM_SIMD_TEST(I8x16ShrS) {
  RunI8x16ShiftOpTest(lower_simd, kExprI8x16ShrS, ArithmeticShiftRight, 1);
}

WASM_SIMD_TEST(I8x16ShrU) {
  RunI8x16ShiftOpTest(lower_simd, kExprI8x16ShrU, LogicalShiftRight, 1);
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

// Test Select by making a mask where the 0th and 3rd lanes are true and the
// rest false, and comparing for non-equality with zero to convert to a boolean
// vector.
#define WASM_SIMD_SELECT_TEST(format)                                        \
  WASM_SIMD_TEST(S##format##Select) {                                        \
    WasmRunner<int32_t, int32_t, int32_t> r(kExecuteTurbofan, lower_simd);   \
    byte val1 = 0;                                                           \
    byte val2 = 1;                                                           \
    byte src1 = r.AllocateLocal(kWasmS128);                                  \
    byte src2 = r.AllocateLocal(kWasmS128);                                  \
    byte zero = r.AllocateLocal(kWasmS128);                                  \
    byte mask = r.AllocateLocal(kWasmS128);                                  \
    BUILD(r,                                                                 \
          WASM_SET_LOCAL(src1,                                               \
                         WASM_SIMD_I##format##_SPLAT(WASM_GET_LOCAL(val1))), \
          WASM_SET_LOCAL(src2,                                               \
                         WASM_SIMD_I##format##_SPLAT(WASM_GET_LOCAL(val2))), \
          WASM_SET_LOCAL(zero, WASM_SIMD_I##format##_SPLAT(WASM_ZERO)),      \
          WASM_SET_LOCAL(mask, WASM_SIMD_I##format##_REPLACE_LANE(           \
                                   1, WASM_GET_LOCAL(zero), WASM_I32V(-1))), \
          WASM_SET_LOCAL(mask, WASM_SIMD_I##format##_REPLACE_LANE(           \
                                   2, WASM_GET_LOCAL(mask), WASM_I32V(-1))), \
          WASM_SET_LOCAL(                                                    \
              mask,                                                          \
              WASM_SIMD_SELECT(                                              \
                  format,                                                    \
                  WASM_SIMD_BINOP(kExprI##format##Ne, WASM_GET_LOCAL(mask),  \
                                  WASM_GET_LOCAL(zero)),                     \
                  WASM_GET_LOCAL(src1), WASM_GET_LOCAL(src2))),              \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val2, 0),               \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val1, 1),               \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val1, 2),               \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val2, 3), WASM_ONE);    \
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
    WasmRunner<int32_t, int32_t, int32_t, int32_t> r(kExecuteTurbofan,        \
                                                     lower_simd);             \
    byte val1 = 0;                                                            \
    byte val2 = 1;                                                            \
    byte combined = 2;                                                        \
    byte src1 = r.AllocateLocal(kWasmS128);                                   \
    byte src2 = r.AllocateLocal(kWasmS128);                                   \
    byte zero = r.AllocateLocal(kWasmS128);                                   \
    byte mask = r.AllocateLocal(kWasmS128);                                   \
    BUILD(r,                                                                  \
          WASM_SET_LOCAL(src1,                                                \
                         WASM_SIMD_I##format##_SPLAT(WASM_GET_LOCAL(val1))),  \
          WASM_SET_LOCAL(src2,                                                \
                         WASM_SIMD_I##format##_SPLAT(WASM_GET_LOCAL(val2))),  \
          WASM_SET_LOCAL(zero, WASM_SIMD_I##format##_SPLAT(WASM_ZERO)),       \
          WASM_SET_LOCAL(mask, WASM_SIMD_I##format##_REPLACE_LANE(            \
                                   1, WASM_GET_LOCAL(zero), WASM_I32V(0xF))), \
          WASM_SET_LOCAL(mask, WASM_SIMD_I##format##_REPLACE_LANE(            \
                                   2, WASM_GET_LOCAL(mask), WASM_I32V(0xF))), \
          WASM_SET_LOCAL(mask, WASM_SIMD_SELECT(format, WASM_GET_LOCAL(mask), \
                                                WASM_GET_LOCAL(src1),         \
                                                WASM_GET_LOCAL(src2))),       \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val2, 0),                \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, combined, 1),            \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, combined, 2),            \
          WASM_SIMD_CHECK_LANE(I##format, mask, I32, val2, 3), WASM_ONE);     \
                                                                              \
    CHECK_EQ(1, r.Call(0x12, 0x34, 0x32));                                    \
  }

WASM_SIMD_NON_CANONICAL_SELECT_TEST(32x4)
WASM_SIMD_NON_CANONICAL_SELECT_TEST(16x8)
WASM_SIMD_NON_CANONICAL_SELECT_TEST(8x16)

// Test binary ops with two lane test patterns, all lanes distinct.
template <typename T>
void RunBinaryLaneOpTest(
    LowerSimd lower_simd, WasmOpcode simd_op,
    const std::array<T, kSimd128Size / sizeof(T)>& expected) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  // Set up two test patterns as globals, e.g. [0, 1, 2, 3] and [4, 5, 6, 7].
  T* src0 = r.builder().AddGlobal<T>(kWasmS128);
  T* src1 = r.builder().AddGlobal<T>(kWasmS128);
  static const int kElems = kSimd128Size / sizeof(T);
  for (int i = 0; i < kElems; i++) {
    src0[i] = i;
    src1[i] = kElems + i;
  }
  if (simd_op == kExprS8x16Shuffle) {
    BUILD(r,
          WASM_SET_GLOBAL(0, WASM_SIMD_S8x16_SHUFFLE_OP(simd_op, expected,
                                                        WASM_GET_GLOBAL(0),
                                                        WASM_GET_GLOBAL(1))),
          WASM_ONE);
  } else {
    BUILD(r,
          WASM_SET_GLOBAL(0, WASM_SIMD_BINOP(simd_op, WASM_GET_GLOBAL(0),
                                             WASM_GET_GLOBAL(1))),
          WASM_ONE);
  }

  CHECK_EQ(1, r.Call());
  for (size_t i = 0; i < expected.size(); i++) {
    CHECK_EQ(src0[i], expected[i]);
  }
}

WASM_SIMD_TEST(I32x4AddHoriz) {
  RunBinaryLaneOpTest<int32_t>(lower_simd, kExprI32x4AddHoriz, {{1, 5, 9, 13}});
}

WASM_SIMD_TEST(I16x8AddHoriz) {
  RunBinaryLaneOpTest<int16_t>(lower_simd, kExprI16x8AddHoriz,
                               {{1, 5, 9, 13, 17, 21, 25, 29}});
}

WASM_SIMD_TEST(F32x4AddHoriz) {
  RunBinaryLaneOpTest<float>(lower_simd, kExprF32x4AddHoriz,
                             {{1.0f, 5.0f, 9.0f, 13.0f}});
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
// Test some regular shuffles that may have special handling on some targets.
// Test a normal and unary versions (where second operand isn't used).
WASM_SIMD_TEST(S32x4Dup) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19, 16, 17, 18, 19}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7}});
}

WASM_SIMD_TEST(S32x4ZipLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 4, 5, 6, 7}});
}

WASM_SIMD_TEST(S32x4ZipRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{8, 9, 10, 11, 8, 9, 10, 11, 12, 13, 14, 15, 12, 13, 14, 15}});
}

WASM_SIMD_TEST(S32x4UnzipLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 8, 9, 10, 11, 0, 1, 2, 3, 8, 9, 10, 11}});
}

WASM_SIMD_TEST(S32x4UnzipRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{4, 5, 6, 7, 12, 13, 14, 15, 4, 5, 6, 7, 12, 13, 14, 15}});
}

WASM_SIMD_TEST(S32x4TransposeLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 16, 17, 18, 19, 8, 9, 10, 11, 24, 25, 26, 27}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 0, 1, 2, 3, 8, 9, 10, 11, 8, 9, 10, 11}});
}

WASM_SIMD_TEST(S32x4TransposeRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{4, 5, 6, 7, 20, 21, 22, 23, 12, 13, 14, 15, 28, 29, 30, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{4, 5, 6, 7, 4, 5, 6, 7, 12, 13, 14, 15, 12, 13, 14, 15}});
}

// Reverses are only unary.
WASM_SIMD_TEST(S32x2Reverse) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{4, 5, 6, 7, 0, 1, 2, 3, 12, 13, 14, 15, 8, 9, 10, 11}});
}

// Test irregular shuffle.
WASM_SIMD_TEST(S32x4Irregular) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 16, 17, 18, 19, 16, 17, 18, 19, 20, 21, 22, 23}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7}});
}

WASM_SIMD_TEST(S16x8Dup) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7}});
}

WASM_SIMD_TEST(S16x8ZipLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21, 6, 7, 22, 23}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7}});
}

WASM_SIMD_TEST(S16x8ZipRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{8, 9, 24, 25, 10, 11, 26, 27, 12, 13, 28, 29, 14, 15, 30, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{8, 9, 8, 9, 10, 11, 10, 11, 12, 13, 12, 13, 14, 15, 14, 15}});
}

WASM_SIMD_TEST(S16x8UnzipLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 4, 5, 8, 9, 12, 13, 0, 1, 4, 5, 8, 9, 12, 13}});
}

WASM_SIMD_TEST(S16x8UnzipRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{2, 3, 6, 7, 10, 11, 14, 15, 2, 3, 6, 7, 10, 11, 14, 15}});
}

WASM_SIMD_TEST(S16x8TransposeLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 16, 17, 4, 5, 20, 21, 8, 9, 24, 25, 12, 13, 28, 29}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 0, 1, 4, 5, 4, 5, 8, 9, 8, 9, 12, 13, 12, 13}});
}

WASM_SIMD_TEST(S16x8TransposeRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{2, 3, 18, 19, 6, 7, 22, 23, 10, 11, 26, 27, 14, 15, 30, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{2, 3, 2, 3, 6, 7, 6, 7, 10, 11, 10, 11, 14, 15, 14, 15}});
}

WASM_SIMD_TEST(S16x4Reverse) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{6, 7, 4, 5, 2, 3, 0, 1, 14, 15, 12, 13, 10, 11, 8, 9}});
}

WASM_SIMD_TEST(S16x2Reverse) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{2, 3, 0, 1, 6, 7, 4, 5, 10, 11, 8, 9, 14, 15, 12, 13}});
}

WASM_SIMD_TEST(S16x8Irregular) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 16, 17, 16, 17, 0, 1, 4, 5, 20, 21, 6, 7, 22, 23}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 1, 0, 1, 0, 1, 0, 1, 4, 5, 4, 5, 6, 7, 6, 7}});
}

WASM_SIMD_TEST(S8x16Dup) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7}});
}

WASM_SIMD_TEST(S8x16ZipLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}});
}

WASM_SIMD_TEST(S8x16ZipRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15}});
}

WASM_SIMD_TEST(S8x16UnzipLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 2, 4, 6, 8, 10, 12, 14, 0, 2, 4, 6, 8, 10, 12, 14}});
}

WASM_SIMD_TEST(S8x16UnzipRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{1, 3, 5, 7, 9, 11, 13, 15, 1, 3, 5, 7, 9, 11, 13, 15}});
}

WASM_SIMD_TEST(S8x16TransposeLeft) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 16, 2, 18, 4, 20, 6, 22, 8, 24, 10, 26, 12, 28, 14, 30}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14}});
}

WASM_SIMD_TEST(S8x16TransposeRight) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{1, 17, 3, 19, 5, 21, 7, 23, 9, 25, 11, 27, 13, 29, 15, 31}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{1, 1, 3, 3, 5, 5, 7, 7, 9, 9, 11, 11, 13, 13, 15, 15}});
}

WASM_SIMD_TEST(S8x8Reverse) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8}});
}

WASM_SIMD_TEST(S8x4Reverse) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12}});
}

WASM_SIMD_TEST(S8x2Reverse) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14}});
}

WASM_SIMD_TEST(S8x16Irregular) {
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 16, 0, 16, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}});
  RunBinaryLaneOpTest<int8_t>(
      lower_simd, kExprS8x16Shuffle,
      {{0, 0, 0, 0, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}});
}

// Test shuffles that concatenate the two vectors.

WASM_SIMD_TEST(S8x16Concat) {
  static const int kLanes = 16;
  std::array<uint8_t, kLanes> expected;
  for (int bias = 1; bias < kLanes; bias++) {
    int i = 0;
    // last kLanes - bias bytes of first vector.
    for (int j = bias; j < kLanes; j++) {
      expected[i++] = j;
    }
    // first bias lanes of second vector
    for (int j = 0; j < bias; j++) {
      expected[i++] = j + kLanes;
    }
    RunBinaryLaneOpTest(lower_simd, kExprS8x16Shuffle, expected);
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64
// Boolean unary operations are 'AllTrue' and 'AnyTrue', which return an integer
// result. Use relational ops on numeric vectors to create the boolean vector
// test inputs. Test inputs with all true, all false, one true, and one false.
#define WASM_SIMD_BOOL_REDUCTION_TEST(format, lanes)                           \
  WASM_SIMD_COMPILED_TEST(ReductionTest##lanes) {                              \
    WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);                       \
    byte zero = r.AllocateLocal(kWasmS128);                                    \
    byte one_one = r.AllocateLocal(kWasmS128);                                 \
    byte reduced = r.AllocateLocal(kWasmI32);                                  \
    BUILD(r, WASM_SET_LOCAL(zero, WASM_SIMD_I##format##_SPLAT(WASM_ZERO)),     \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AnyTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_GET_LOCAL(zero),    \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AnyTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_GET_LOCAL(zero),    \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AllTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_GET_LOCAL(zero),    \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AllTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_GET_LOCAL(zero),    \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(one_one,                                              \
                         WASM_SIMD_I##format##_REPLACE_LANE(                   \
                             lanes - 1, WASM_GET_LOCAL(zero), WASM_ONE)),      \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AnyTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_GET_LOCAL(one_one), \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AnyTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_GET_LOCAL(one_one), \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AllTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Eq,      \
                                                      WASM_GET_LOCAL(one_one), \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_SET_LOCAL(                                                      \
              reduced, WASM_SIMD_UNOP(kExprS1x##lanes##AllTrue,                \
                                      WASM_SIMD_BINOP(kExprI##format##Ne,      \
                                                      WASM_GET_LOCAL(one_one), \
                                                      WASM_GET_LOCAL(zero)))), \
          WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(reduced), WASM_ZERO),             \
                  WASM_RETURN1(WASM_ZERO)),                                    \
          WASM_ONE);                                                           \
    CHECK_EQ(1, r.Call());                                                     \
  }

WASM_SIMD_BOOL_REDUCTION_TEST(32x4, 4)
WASM_SIMD_BOOL_REDUCTION_TEST(16x8, 8)
WASM_SIMD_BOOL_REDUCTION_TEST(8x16, 16)

WASM_SIMD_TEST(SimdI32x4ExtractWithF32x4) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  BUILD(r, WASM_IF_ELSE_I(
               WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                               0, WASM_SIMD_F32x4_SPLAT(WASM_F32(30.5))),
                           WASM_I32_REINTERPRET_F32(WASM_F32(30.5))),
               WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64

WASM_SIMD_TEST(SimdF32x4ExtractWithI32x4) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  BUILD(r,
        WASM_IF_ELSE_I(WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                                       0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(15))),
                                   WASM_F32_REINTERPRET_I32(WASM_I32V(15))),
                       WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdF32x4AddWithI32x4) {
  // Choose two floating point values whose sum is normal and exactly
  // representable as a float.
  const int kOne = 0x3F800000;
  const int kTwo = 0x40000000;
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
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
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
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
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),

        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(0)));
  CHECK_EQ(31, r.Call());
}

WASM_SIMD_TEST(SimdI32x4SplatFromExtract) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_EXTRACT_LANE(
                                 0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(76)))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0))),
        WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_LOCAL(1)));
  CHECK_EQ(76, r.Call());
}

WASM_SIMD_TEST(SimdI32x4For) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r,

        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_GET_LOCAL(1),
                                                       WASM_I32V(53))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GET_LOCAL(1),
                                                       WASM_I32V(23))),
        WASM_SET_LOCAL(0, WASM_I32V(0)),
        WASM_LOOP(
            WASM_SET_LOCAL(
                1, WASM_SIMD_BINOP(kExprI32x4Add, WASM_GET_LOCAL(1),
                                   WASM_SIMD_I32x4_SPLAT(WASM_I32V(1)))),
            WASM_IF(WASM_I32_NE(WASM_INC_LOCAL(0), WASM_I32V(5)), WASM_BR(1))),
        WASM_SET_LOCAL(0, WASM_I32V(1)),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(1)),
                            WASM_I32V(36)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_LOCAL(1)),
                            WASM_I32V(58)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_GET_LOCAL(1)),
                            WASM_I32V(28)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(3, WASM_GET_LOCAL(1)),
                            WASM_I32V(36)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdF32x4For) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(1, WASM_SIMD_F32x4_SPLAT(WASM_F32(21.25))),
        WASM_SET_LOCAL(1, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_LOCAL(1),
                                                       WASM_F32(19.5))),
        WASM_SET_LOCAL(0, WASM_I32V(0)),
        WASM_LOOP(
            WASM_SET_LOCAL(
                1, WASM_SIMD_BINOP(kExprF32x4Add, WASM_GET_LOCAL(1),
                                   WASM_SIMD_F32x4_SPLAT(WASM_F32(2.0)))),
            WASM_IF(WASM_I32_NE(WASM_INC_LOCAL(0), WASM_I32V(3)), WASM_BR(1))),
        WASM_SET_LOCAL(0, WASM_I32V(1)),
        WASM_IF(WASM_F32_NE(WASM_SIMD_F32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(1)),
                            WASM_F32(27.25)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_F32_NE(WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_GET_LOCAL(1)),
                            WASM_F32(25.5)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(1, r.Call());
}

template <typename T, int numLanes = 4>
void SetVectorByLanes(T* v, const std::array<T, numLanes>& arr) {
  for (int lane = 0; lane < numLanes; lane++) {
    const T& value = arr[lane];
#if defined(V8_TARGET_BIG_ENDIAN)
    v[numLanes - 1 - lane] = value;
#else
    v[lane] = value;
#endif
  }
}

template <typename T>
const T& GetScalar(T* v, int lane) {
  constexpr int kElems = kSimd128Size / sizeof(T);
#if defined(V8_TARGET_BIG_ENDIAN)
  const int index = kElems - 1 - lane;
#else
  const int index = lane;
#endif
  USE(kElems);
  DCHECK(index >= 0 && index < kElems);
  return v[index];
}

WASM_SIMD_TEST(SimdI32x4GetGlobal) {
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  // Pad the globals with a few unused slots to get a non-zero offset.
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  int32_t* global = r.builder().AddGlobal<int32_t>(kWasmS128);
  SetVectorByLanes(global, {{0, 1, 2, 3}});
  r.AllocateLocal(kWasmI32);
  BUILD(
      r, WASM_SET_LOCAL(1, WASM_I32V(1)),
      WASM_IF(WASM_I32_NE(WASM_I32V(0),
                          WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_GLOBAL(4))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(1),
                          WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_GLOBAL(4))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(2),
                          WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_GET_GLOBAL(4))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(3),
                          WASM_SIMD_I32x4_EXTRACT_LANE(3, WASM_GET_GLOBAL(4))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_GET_LOCAL(1));
  CHECK_EQ(1, r.Call(0));
}

WASM_SIMD_TEST(SimdI32x4SetGlobal) {
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  // Pad the globals with a few unused slots to get a non-zero offset.
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  r.builder().AddGlobal<int32_t>(kWasmI32);  // purposefully unused
  int32_t* global = r.builder().AddGlobal<int32_t>(kWasmS128);
  BUILD(r, WASM_SET_GLOBAL(4, WASM_SIMD_I32x4_SPLAT(WASM_I32V(23))),
        WASM_SET_GLOBAL(4, WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_GET_GLOBAL(4),
                                                        WASM_I32V(34))),
        WASM_SET_GLOBAL(4, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GET_GLOBAL(4),
                                                        WASM_I32V(45))),
        WASM_SET_GLOBAL(4, WASM_SIMD_I32x4_REPLACE_LANE(3, WASM_GET_GLOBAL(4),
                                                        WASM_I32V(56))),
        WASM_I32V(1));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(GetScalar(global, 0), 23);
  CHECK_EQ(GetScalar(global, 1), 34);
  CHECK_EQ(GetScalar(global, 2), 45);
  CHECK_EQ(GetScalar(global, 3), 56);
}

WASM_SIMD_TEST(SimdF32x4GetGlobal) {
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  float* global = r.builder().AddGlobal<float>(kWasmS128);
  SetVectorByLanes<float>(global, {{0.0, 1.5, 2.25, 3.5}});
  r.AllocateLocal(kWasmI32);
  BUILD(
      r, WASM_SET_LOCAL(1, WASM_I32V(1)),
      WASM_IF(WASM_F32_NE(WASM_F32(0.0),
                          WASM_SIMD_F32x4_EXTRACT_LANE(0, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(1.5),
                          WASM_SIMD_F32x4_EXTRACT_LANE(1, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(2.25),
                          WASM_SIMD_F32x4_EXTRACT_LANE(2, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(3.5),
                          WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_GET_LOCAL(1));
  CHECK_EQ(1, r.Call(0));
}

WASM_SIMD_TEST(SimdF32x4SetGlobal) {
  WasmRunner<int32_t, int32_t> r(kExecuteTurbofan, lower_simd);
  float* global = r.builder().AddGlobal<float>(kWasmS128);
  BUILD(r, WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_SPLAT(WASM_F32(13.5))),
        WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_REPLACE_LANE(1, WASM_GET_GLOBAL(0),
                                                        WASM_F32(45.5))),
        WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_REPLACE_LANE(2, WASM_GET_GLOBAL(0),
                                                        WASM_F32(32.25))),
        WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_GLOBAL(0),
                                                        WASM_F32(65.0))),
        WASM_I32V(1));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(GetScalar(global, 0), 13.5f);
  CHECK_EQ(GetScalar(global, 1), 45.5f);
  CHECK_EQ(GetScalar(global, 2), 32.25f);
  CHECK_EQ(GetScalar(global, 3), 65.0f);
}

WASM_SIMD_COMPILED_TEST(SimdLoadStoreLoad) {
  WasmRunner<int32_t> r(kExecuteTurbofan, lower_simd);
  int32_t* memory = r.builder().AddMemoryElems<int32_t>(8);
  // Load memory, store it, then reload it and extract the first lane. Use a
  // non-zero offset into the memory of 1 lane (4 bytes) to test indexing.
  BUILD(r, WASM_SIMD_STORE_MEM(WASM_I32V(4), WASM_SIMD_LOAD_MEM(WASM_I32V(4))),
        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_SIMD_LOAD_MEM(WASM_I32V(4))));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    r.builder().WriteMemory(&memory[1], expected);
    CHECK_EQ(expected, r.Call());
  }
}

#undef WASM_SIMD_TEST
#undef WASM_SIMD_COMPILED_TEST
#undef WASM_SIMD_CHECK_LANE
#undef WASM_SIMD_CHECK4
#undef WASM_SIMD_CHECK_SPLAT4
#undef WASM_SIMD_CHECK8
#undef WASM_SIMD_CHECK_SPLAT8
#undef WASM_SIMD_CHECK16
#undef WASM_SIMD_CHECK_SPLAT16
#undef WASM_SIMD_CHECK_F32_LANE
#undef WASM_SIMD_CHECK_F32x4
#undef WASM_SIMD_CHECK_SPLAT_F32x4
#undef WASM_SIMD_CHECK_F32_LANE_ESTIMATE
#undef WASM_SIMD_CHECK_SPLAT_F32x4_ESTIMATE
#undef TO_BYTE
#undef WASM_SIMD_OP
#undef WASM_SIMD_SPLAT
#undef WASM_SIMD_UNOP
#undef WASM_SIMD_BINOP
#undef WASM_SIMD_SHIFT_OP
#undef WASM_SIMD_CONCAT_OP
#undef WASM_SIMD_SELECT
#undef WASM_SIMD_F32x4_SPLAT
#undef WASM_SIMD_F32x4_EXTRACT_LANE
#undef WASM_SIMD_F32x4_REPLACE_LANE
#undef WASM_SIMD_I32x4_SPLAT
#undef WASM_SIMD_I32x4_EXTRACT_LANE
#undef WASM_SIMD_I32x4_REPLACE_LANE
#undef WASM_SIMD_I16x8_SPLAT
#undef WASM_SIMD_I16x8_EXTRACT_LANE
#undef WASM_SIMD_I16x8_REPLACE_LANE
#undef WASM_SIMD_I8x16_SPLAT
#undef WASM_SIMD_I8x16_EXTRACT_LANE
#undef WASM_SIMD_I8x16_REPLACE_LANE
#undef WASM_SIMD_S8x16_SHUFFLE_OP
#undef WASM_SIMD_LOAD_MEM
#undef WASM_SIMD_STORE_MEM
#undef WASM_SIMD_SELECT_TEST
#undef WASM_SIMD_NON_CANONICAL_SELECT_TEST
#undef WASM_SIMD_BOOL_REDUCTION_TEST

}  // namespace test_run_wasm_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
