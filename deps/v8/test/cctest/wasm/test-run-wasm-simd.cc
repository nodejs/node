// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/base/bits.h"
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

#define WASM_SIMD_TEST(name)                                          \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                    \
                             ExecutionTier execution_tier);           \
  TEST(RunWasm_##name##_turbofan) {                                   \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                    \
    RunWasm_##name##_Impl(kNoLowerSimd, ExecutionTier::kOptimized);   \
  }                                                                   \
  TEST(RunWasm_##name##_interpreter) {                                \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                    \
    RunWasm_##name##_Impl(kNoLowerSimd, ExecutionTier::kInterpreter); \
  }                                                                   \
  TEST(RunWasm_##name##_simd_lowered) {                               \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                    \
    RunWasm_##name##_Impl(kLowerSimd, ExecutionTier::kOptimized);     \
  }                                                                   \
  void RunWasm_##name##_Impl(LowerSimd lower_simd, ExecutionTier execution_tier)

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
  WasmRunner<int32_t, float> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, float, float> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t, float, float> r(execution_tier, lower_simd);
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

void RunF32x4UnOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                      WasmOpcode simd_op, FloatUnOp expected_op,
                      float error = 0.0f) {
  WasmRunner<int32_t, float, float, float> r(execution_tier, lower_simd);
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
  RunF32x4UnOpTest(execution_tier, lower_simd, kExprF32x4Abs, std::abs);
}
WASM_SIMD_TEST(F32x4Neg) {
  RunF32x4UnOpTest(execution_tier, lower_simd, kExprF32x4Neg, Negate);
}

static const float kApproxError = 0.01f;

WASM_SIMD_TEST(F32x4RecipApprox) {
  RunF32x4UnOpTest(execution_tier, lower_simd, kExprF32x4RecipApprox, Recip,
                   kApproxError);
}

WASM_SIMD_TEST(F32x4RecipSqrtApprox) {
  RunF32x4UnOpTest(execution_tier, lower_simd, kExprF32x4RecipSqrtApprox,
                   RecipSqrt, kApproxError);
}

void RunF32x4BinOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                       WasmOpcode simd_op, FloatBinOp expected_op) {
  WasmRunner<int32_t, float, float, float> r(execution_tier, lower_simd);
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

WASM_SIMD_TEST(F32x4Add) {
  RunF32x4BinOpTest(execution_tier, lower_simd, kExprF32x4Add, Add);
}
WASM_SIMD_TEST(F32x4Sub) {
  RunF32x4BinOpTest(execution_tier, lower_simd, kExprF32x4Sub, Sub);
}
WASM_SIMD_TEST(F32x4Mul) {
  RunF32x4BinOpTest(execution_tier, lower_simd, kExprF32x4Mul, Mul);
}
WASM_SIMD_TEST(F32x4_Min) {
  RunF32x4BinOpTest(execution_tier, lower_simd, kExprF32x4Min, JSMin);
}
WASM_SIMD_TEST(F32x4_Max) {
  RunF32x4BinOpTest(execution_tier, lower_simd, kExprF32x4Max, JSMax);
}

void RunF32x4CompareOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                           WasmOpcode simd_op, FloatCompareOp expected_op) {
  WasmRunner<int32_t, float, float, int32_t> r(execution_tier, lower_simd);
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
  RunF32x4CompareOpTest(execution_tier, lower_simd, kExprF32x4Eq, Equal);
}

WASM_SIMD_TEST(F32x4Ne) {
  RunF32x4CompareOpTest(execution_tier, lower_simd, kExprF32x4Ne, NotEqual);
}

WASM_SIMD_TEST(F32x4Gt) {
  RunF32x4CompareOpTest(execution_tier, lower_simd, kExprF32x4Gt, Greater);
}

WASM_SIMD_TEST(F32x4Ge) {
  RunF32x4CompareOpTest(execution_tier, lower_simd, kExprF32x4Ge, GreaterEqual);
}

WASM_SIMD_TEST(F32x4Lt) {
  RunF32x4CompareOpTest(execution_tier, lower_simd, kExprF32x4Lt, Less);
}

WASM_SIMD_TEST(F32x4Le) {
  RunF32x4CompareOpTest(execution_tier, lower_simd, kExprF32x4Le, LessEqual);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, lane_val), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_SIMD_TEST(I32x4ReplaceLane) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, lane_val), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_SIMD_TEST(I16x8ReplaceLane) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  byte lane_val = 0;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r,
        WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(lane_val))),
        WASM_SIMD_CHECK_SPLAT8(I8x16, simd, I32, lane_val), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

WASM_SIMD_TEST(I8x16ReplaceLane) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

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
  WasmRunner<int32_t, float, int32_t, int32_t> r(execution_tier, lower_simd);
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
WASM_SIMD_TEST(I32x4ConvertI16x8) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t, int32_t> r(execution_tier,
                                                            lower_simd);
  byte a = 0;
  byte unpacked_signed = 1;
  byte unpacked_unsigned = 2;
  byte zero_value = 3;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  byte simd3 = r.AllocateLocal(kWasmS128);
  byte simd4 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(
            simd0, WASM_SIMD_I16x8_REPLACE_LANE(0, WASM_GET_LOCAL(simd0),
                                                WASM_GET_LOCAL(zero_value))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_UNOP(kExprI32x4SConvertI16x8High,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd1, I32, unpacked_signed),
        WASM_SET_LOCAL(simd2, WASM_SIMD_UNOP(kExprI32x4UConvertI16x8High,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd2, I32, unpacked_unsigned),
        WASM_SET_LOCAL(simd3, WASM_SIMD_UNOP(kExprI32x4SConvertI16x8Low,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK4(I32x4, simd3, I32, zero_value, unpacked_signed,
                         unpacked_signed, unpacked_signed),
        WASM_SET_LOCAL(simd4, WASM_SIMD_UNOP(kExprI32x4UConvertI16x8Low,
                                             WASM_GET_LOCAL(simd0))),
        WASM_SIMD_CHECK4(I32x4, simd4, I32, zero_value, unpacked_unsigned,
                         unpacked_unsigned, unpacked_unsigned),
        WASM_ONE);

  FOR_INT16_INPUTS(i) {
    int32_t unpacked_signed = static_cast<int32_t>(Widen<int16_t>(*i));
    int32_t unpacked_unsigned =
        static_cast<int32_t>(UnsignedWiden<int16_t>(*i));
    CHECK_EQ(1, r.Call(*i, unpacked_signed, unpacked_unsigned, 0));
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

void RunI32x4UnOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                      WasmOpcode simd_op, Int32UnOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, expected), WASM_ONE);

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_SIMD_TEST(I32x4Neg) {
  RunI32x4UnOpTest(execution_tier, lower_simd, kExprI32x4Neg, Negate);
}

WASM_SIMD_TEST(S128Not) {
  RunI32x4UnOpTest(execution_tier, lower_simd, kExprS128Not, Not);
}

void RunI32x4BinOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                       WasmOpcode simd_op, Int32BinOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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

WASM_SIMD_TEST(I32x4Add) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4Add, Add);
}

WASM_SIMD_TEST(I32x4Sub) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4Sub, Sub);
}

WASM_SIMD_TEST(I32x4Mul) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4Mul, Mul);
}

WASM_SIMD_TEST(I32x4MinS) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4MinS, Minimum);
}

WASM_SIMD_TEST(I32x4MaxS) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4MaxS, Maximum);
}

WASM_SIMD_TEST(I32x4MinU) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4MinU,
                    UnsignedMinimum);
}
WASM_SIMD_TEST(I32x4MaxU) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprI32x4MaxU,

                    UnsignedMaximum);
}

WASM_SIMD_TEST(S128And) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprS128And, And);
}

WASM_SIMD_TEST(S128Or) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprS128Or, Or);
}

WASM_SIMD_TEST(S128Xor) {
  RunI32x4BinOpTest(execution_tier, lower_simd, kExprS128Xor, Xor);
}

void RunI32x4CompareOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                           WasmOpcode simd_op, Int32CompareOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4Eq, Equal);
}

WASM_SIMD_TEST(I32x4Ne) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4Ne, NotEqual);
}

WASM_SIMD_TEST(I32x4LtS) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4LtS, Less);
}

WASM_SIMD_TEST(I32x4LeS) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4LeS, LessEqual);
}

WASM_SIMD_TEST(I32x4GtS) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4GtS, Greater);
}

WASM_SIMD_TEST(I32x4GeS) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4GeS,
                        GreaterEqual);
}

WASM_SIMD_TEST(I32x4LtU) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4LtU,
                        UnsignedLess);
}

WASM_SIMD_TEST(I32x4LeU) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4LeU,
                        UnsignedLessEqual);
}

WASM_SIMD_TEST(I32x4GtU) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4GtU,
                        UnsignedGreater);
}

WASM_SIMD_TEST(I32x4GeU) {
  RunI32x4CompareOpTest(execution_tier, lower_simd, kExprI32x4GeU,
                        UnsignedGreaterEqual);
}

void RunI32x4ShiftOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                         WasmOpcode simd_op, Int32ShiftOp expected_op) {
  for (int shift = 1; shift < 32; ++shift) {
    WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
    byte a = 0;
    byte expected = 1;
    byte simd = r.AllocateLocal(kWasmS128);
    BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
          WASM_SET_LOCAL(
              simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
          WASM_SIMD_CHECK_SPLAT4(I32x4, simd, I32, expected), WASM_ONE);

    FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
  }
}

WASM_SIMD_TEST(I32x4Shl) {
  RunI32x4ShiftOpTest(execution_tier, lower_simd, kExprI32x4Shl,
                      LogicalShiftLeft);
}

WASM_SIMD_TEST(I32x4ShrS) {
  RunI32x4ShiftOpTest(execution_tier, lower_simd, kExprI32x4ShrS,
                      ArithmeticShiftRight);
}

WASM_SIMD_TEST(I32x4ShrU) {
  RunI32x4ShiftOpTest(execution_tier, lower_simd, kExprI32x4ShrU,
                      LogicalShiftRight);
}

// Tests both signed and unsigned conversion from I8x16 (unpacking).
WASM_SIMD_TEST(I16x8ConvertI8x16) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t, int32_t> r(execution_tier,
                                                            lower_simd);
  byte a = 0;
  byte unpacked_signed = 1;
  byte unpacked_unsigned = 2;
  byte zero_value = 3;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  byte simd3 = r.AllocateLocal(kWasmS128);
  byte simd4 = r.AllocateLocal(kWasmS128);
  BUILD(
      r, WASM_SET_LOCAL(simd0, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
      WASM_SET_LOCAL(simd0,
                     WASM_SIMD_I8x16_REPLACE_LANE(0, WASM_GET_LOCAL(simd0),
                                                  WASM_GET_LOCAL(zero_value))),
      WASM_SET_LOCAL(simd1, WASM_SIMD_UNOP(kExprI16x8SConvertI8x16High,
                                           WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK_SPLAT8(I16x8, simd1, I32, unpacked_signed),
      WASM_SET_LOCAL(simd2, WASM_SIMD_UNOP(kExprI16x8UConvertI8x16High,
                                           WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK_SPLAT8(I16x8, simd2, I32, unpacked_unsigned),
      WASM_SET_LOCAL(simd3, WASM_SIMD_UNOP(kExprI16x8SConvertI8x16Low,
                                           WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK8(I16x8, simd3, I32, zero_value, unpacked_signed,
                       unpacked_signed, unpacked_signed, unpacked_signed,
                       unpacked_signed, unpacked_signed, unpacked_signed),
      WASM_SET_LOCAL(simd4, WASM_SIMD_UNOP(kExprI16x8UConvertI8x16Low,
                                           WASM_GET_LOCAL(simd0))),
      WASM_SIMD_CHECK8(I16x8, simd4, I32, zero_value, unpacked_unsigned,
                       unpacked_unsigned, unpacked_unsigned, unpacked_unsigned,
                       unpacked_unsigned, unpacked_unsigned, unpacked_unsigned),
      WASM_ONE);

  FOR_INT8_INPUTS(i) {
    int32_t unpacked_signed = static_cast<int32_t>(Widen<int8_t>(*i));
    int32_t unpacked_unsigned = static_cast<int32_t>(UnsignedWiden<int8_t>(*i));
    CHECK_EQ(1, r.Call(*i, unpacked_signed, unpacked_unsigned, 0));
  }
}

void RunI16x8UnOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                      WasmOpcode simd_op, Int16UnOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, expected), WASM_ONE);

  FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_SIMD_TEST(I16x8Neg) {
  RunI16x8UnOpTest(execution_tier, lower_simd, kExprI16x8Neg, Negate);
}

// Tests both signed and unsigned conversion from I32x4 (packing).
WASM_SIMD_TEST(I16x8ConvertI32x4) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t> r(
      execution_tier, lower_simd);
  byte a = 0;
  byte b = 1;
  // indices for packed signed params
  byte ps_a = 2;
  byte ps_b = 3;
  // indices for packed unsigned params
  byte pu_a = 4;
  byte pu_b = 5;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd2, WASM_SIMD_BINOP(kExprI16x8SConvertI32x4,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK8(I16x8, simd2, I32, ps_a, ps_a, ps_a, ps_a, ps_b, ps_b,
                         ps_b, ps_b),
        WASM_SET_LOCAL(simd2, WASM_SIMD_BINOP(kExprI16x8UConvertI32x4,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK8(I16x8, simd2, I32, pu_a, pu_a, pu_a, pu_a, pu_b, pu_b,
                         pu_b, pu_b),
        WASM_ONE);

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      // packed signed values
      int32_t ps_a = Narrow<int16_t>(*i);
      int32_t ps_b = Narrow<int16_t>(*j);
      // packed unsigned values
      int32_t pu_a = UnsignedNarrow<int16_t>(*i);
      int32_t pu_b = UnsignedNarrow<int16_t>(*j);
      // Sign-extend here, since ExtractLane sign extends.
      if (pu_a & 0x8000) pu_a |= 0xFFFF0000;
      if (pu_b & 0x8000) pu_b |= 0xFFFF0000;
      CHECK_EQ(1, r.Call(*i, *j, ps_a, ps_b, pu_a, pu_b));
    }
  }
}

void RunI16x8BinOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                       WasmOpcode simd_op, Int16BinOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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

WASM_SIMD_TEST(I16x8Add) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8Add, Add);
}

WASM_SIMD_TEST(I16x8AddSaturateS) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8AddSaturateS,
                    AddSaturate);
}

WASM_SIMD_TEST(I16x8Sub) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8Sub, Sub);
}

WASM_SIMD_TEST(I16x8SubSaturateS) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8SubSaturateS,
                    SubSaturate);
}

WASM_SIMD_TEST(I16x8Mul) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8Mul, Mul);
}

WASM_SIMD_TEST(I16x8MinS) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8MinS, Minimum);
}

WASM_SIMD_TEST(I16x8MaxS) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8MaxS, Maximum);
}

WASM_SIMD_TEST(I16x8AddSaturateU) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8AddSaturateU,
                    UnsignedAddSaturate);
}

WASM_SIMD_TEST(I16x8SubSaturateU) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8SubSaturateU,
                    UnsignedSubSaturate);
}

WASM_SIMD_TEST(I16x8MinU) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8MinU,
                    UnsignedMinimum);
}

WASM_SIMD_TEST(I16x8MaxU) {
  RunI16x8BinOpTest(execution_tier, lower_simd, kExprI16x8MaxU,
                    UnsignedMaximum);
}

void RunI16x8CompareOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                           WasmOpcode simd_op, Int16CompareOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8Eq, Equal);
}

WASM_SIMD_TEST(I16x8Ne) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8Ne, NotEqual);
}

WASM_SIMD_TEST(I16x8LtS) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8LtS, Less);
}

WASM_SIMD_TEST(I16x8LeS) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8LeS, LessEqual);
}

WASM_SIMD_TEST(I16x8GtS) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8GtS, Greater);
}

WASM_SIMD_TEST(I16x8GeS) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8GeS,
                        GreaterEqual);
}

WASM_SIMD_TEST(I16x8GtU) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8GtU,
                        UnsignedGreater);
}

WASM_SIMD_TEST(I16x8GeU) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8GeU,
                        UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I16x8LtU) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8LtU,
                        UnsignedLess);
}

WASM_SIMD_TEST(I16x8LeU) {
  RunI16x8CompareOpTest(execution_tier, lower_simd, kExprI16x8LeU,
                        UnsignedLessEqual);
}

void RunI16x8ShiftOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                         WasmOpcode simd_op, Int16ShiftOp expected_op) {
  for (int shift = 1; shift < 16; ++shift) {
    WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
    byte a = 0;
    byte expected = 1;
    byte simd = r.AllocateLocal(kWasmS128);
    BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
          WASM_SET_LOCAL(
              simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
          WASM_SIMD_CHECK_SPLAT8(I16x8, simd, I32, expected), WASM_ONE);

    FOR_INT16_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
  }
}

WASM_SIMD_TEST(I16x8Shl) {
  RunI16x8ShiftOpTest(execution_tier, lower_simd, kExprI16x8Shl,
                      LogicalShiftLeft);
}

WASM_SIMD_TEST(I16x8ShrS) {
  RunI16x8ShiftOpTest(execution_tier, lower_simd, kExprI16x8ShrS,
                      ArithmeticShiftRight);
}

WASM_SIMD_TEST(I16x8ShrU) {
  RunI16x8ShiftOpTest(execution_tier, lower_simd, kExprI16x8ShrU,
                      LogicalShiftRight);
}

void RunI8x16UnOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                      WasmOpcode simd_op, Int8UnOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
  byte a = 0;
  byte expected = 1;
  byte simd = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd, WASM_SIMD_UNOP(simd_op, WASM_GET_LOCAL(simd))),
        WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, expected), WASM_ONE);

  FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i))); }
}

WASM_SIMD_TEST(I8x16Neg) {
  RunI8x16UnOpTest(execution_tier, lower_simd, kExprI8x16Neg, Negate);
}

// Tests both signed and unsigned conversion from I16x8 (packing).
WASM_SIMD_TEST(I8x16ConvertI16x8) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t> r(
      execution_tier, lower_simd);
  byte a = 0;
  byte b = 1;
  // indices for packed signed params
  byte ps_a = 2;
  byte ps_b = 3;
  // indices for packed unsigned params
  byte pu_a = 4;
  byte pu_b = 5;
  byte simd0 = r.AllocateLocal(kWasmS128);
  byte simd1 = r.AllocateLocal(kWasmS128);
  byte simd2 = r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(simd0, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(a))),
        WASM_SET_LOCAL(simd1, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(b))),
        WASM_SET_LOCAL(simd2, WASM_SIMD_BINOP(kExprI8x16SConvertI16x8,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK16(I8x16, simd2, I32, ps_a, ps_a, ps_a, ps_a, ps_a, ps_a,
                          ps_a, ps_a, ps_b, ps_b, ps_b, ps_b, ps_b, ps_b, ps_b,
                          ps_b),
        WASM_SET_LOCAL(simd2, WASM_SIMD_BINOP(kExprI8x16UConvertI16x8,
                                              WASM_GET_LOCAL(simd0),
                                              WASM_GET_LOCAL(simd1))),
        WASM_SIMD_CHECK16(I8x16, simd2, I32, pu_a, pu_a, pu_a, pu_a, pu_a, pu_a,
                          pu_a, pu_a, pu_b, pu_b, pu_b, pu_b, pu_b, pu_b, pu_b,
                          pu_b),
        WASM_ONE);

  FOR_INT16_INPUTS(i) {
    FOR_INT16_INPUTS(j) {
      // packed signed values
      int32_t ps_a = Narrow<int8_t>(*i);
      int32_t ps_b = Narrow<int8_t>(*j);
      // packed unsigned values
      int32_t pu_a = UnsignedNarrow<int8_t>(*i);
      int32_t pu_b = UnsignedNarrow<int8_t>(*j);
      // Sign-extend here, since ExtractLane sign extends.
      if (pu_a & 0x80) pu_a |= 0xFFFFFF00;
      if (pu_b & 0x80) pu_b |= 0xFFFFFF00;
      CHECK_EQ(1, r.Call(*i, *j, ps_a, ps_b, pu_a, pu_b));
    }
  }
}

void RunI8x16BinOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                       WasmOpcode simd_op, Int8BinOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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

WASM_SIMD_TEST(I8x16Add) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16Add, Add);
}

WASM_SIMD_TEST(I8x16AddSaturateS) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16AddSaturateS,
                    AddSaturate);
}

WASM_SIMD_TEST(I8x16Sub) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16Sub, Sub);
}

WASM_SIMD_TEST(I8x16SubSaturateS) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16SubSaturateS,
                    SubSaturate);
}

WASM_SIMD_TEST(I8x16MinS) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16MinS, Minimum);
}

WASM_SIMD_TEST(I8x16MaxS) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16MaxS, Maximum);
}

WASM_SIMD_TEST(I8x16AddSaturateU) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16AddSaturateU,
                    UnsignedAddSaturate);
}

WASM_SIMD_TEST(I8x16SubSaturateU) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16SubSaturateU,
                    UnsignedSubSaturate);
}

WASM_SIMD_TEST(I8x16MinU) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16MinU,
                    UnsignedMinimum);
}

WASM_SIMD_TEST(I8x16MaxU) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16MaxU,
                    UnsignedMaximum);
}

void RunI8x16CompareOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                           WasmOpcode simd_op, Int8CompareOp expected_op) {
  WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
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
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16Eq, Equal);
}

WASM_SIMD_TEST(I8x16Ne) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16Ne, NotEqual);
}

WASM_SIMD_TEST(I8x16GtS) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16GtS, Greater);
}

WASM_SIMD_TEST(I8x16GeS) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16GeS,
                        GreaterEqual);
}

WASM_SIMD_TEST(I8x16LtS) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16LtS, Less);
}

WASM_SIMD_TEST(I8x16LeS) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16LeS, LessEqual);
}

WASM_SIMD_TEST(I8x16GtU) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16GtU,
                        UnsignedGreater);
}

WASM_SIMD_TEST(I8x16GeU) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16GeU,
                        UnsignedGreaterEqual);
}

WASM_SIMD_TEST(I8x16LtU) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16LtU,
                        UnsignedLess);
}

WASM_SIMD_TEST(I8x16LeU) {
  RunI8x16CompareOpTest(execution_tier, lower_simd, kExprI8x16LeU,
                        UnsignedLessEqual);
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
WASM_SIMD_TEST(I8x16Mul) {
  RunI8x16BinOpTest(execution_tier, lower_simd, kExprI8x16Mul, Mul);
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

void RunI8x16ShiftOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                         WasmOpcode simd_op, Int8ShiftOp expected_op) {
  for (int shift = 1; shift < 8; ++shift) {
    WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);
    byte a = 0;
    byte expected = 1;
    byte simd = r.AllocateLocal(kWasmS128);
    BUILD(r, WASM_SET_LOCAL(simd, WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(a))),
          WASM_SET_LOCAL(
              simd, WASM_SIMD_SHIFT_OP(simd_op, shift, WASM_GET_LOCAL(simd))),
          WASM_SIMD_CHECK_SPLAT16(I8x16, simd, I32, expected), WASM_ONE);

    FOR_INT8_INPUTS(i) { CHECK_EQ(1, r.Call(*i, expected_op(*i, shift))); }
  }
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
WASM_SIMD_TEST(I8x16Shl) {
  RunI8x16ShiftOpTest(execution_tier, lower_simd, kExprI8x16Shl,
                      LogicalShiftLeft);
}

WASM_SIMD_TEST(I8x16ShrS) {
  RunI8x16ShiftOpTest(execution_tier, lower_simd, kExprI8x16ShrS,
                      ArithmeticShiftRight);
}

WASM_SIMD_TEST(I8x16ShrU) {
  RunI8x16ShiftOpTest(execution_tier, lower_simd, kExprI8x16ShrU,
                      LogicalShiftRight);
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

// Test Select by making a mask where the 0th and 3rd lanes are true and the
// rest false, and comparing for non-equality with zero to convert to a boolean
// vector.
#define WASM_SIMD_SELECT_TEST(format)                                        \
  WASM_SIMD_TEST(S##format##Select) {                                        \
    WasmRunner<int32_t, int32_t, int32_t> r(execution_tier, lower_simd);     \
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
    WasmRunner<int32_t, int32_t, int32_t, int32_t> r(execution_tier,          \
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
    ExecutionTier execution_tier, LowerSimd lower_simd, WasmOpcode simd_op,
    const std::array<T, kSimd128Size / sizeof(T)>& expected) {
  WasmRunner<int32_t> r(execution_tier, lower_simd);
  // Set up two test patterns as globals, e.g. [0, 1, 2, 3] and [4, 5, 6, 7].
  T* src0 = r.builder().AddGlobal<T>(kWasmS128);
  T* src1 = r.builder().AddGlobal<T>(kWasmS128);
  static const int kElems = kSimd128Size / sizeof(T);
  for (int i = 0; i < kElems; i++) {
    WriteLittleEndianValue<T>(&src0[i], i);
    WriteLittleEndianValue<T>(&src1[i], kElems + i);
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
    CHECK_EQ(ReadLittleEndianValue<T>(&src0[i]), expected[i]);
  }
}

WASM_SIMD_TEST(I32x4AddHoriz) {
  // Inputs are [0 1 2 3] and [4 5 6 7].
  RunBinaryLaneOpTest<int32_t>(execution_tier, lower_simd, kExprI32x4AddHoriz,
                               {{1, 5, 9, 13}});
}

WASM_SIMD_TEST(I16x8AddHoriz) {
  // Inputs are [0 1 2 3 4 5 6 7] and [8 9 10 11 12 13 14 15].
  RunBinaryLaneOpTest<int16_t>(execution_tier, lower_simd, kExprI16x8AddHoriz,
                               {{1, 5, 9, 13, 17, 21, 25, 29}});
}

WASM_SIMD_TEST(F32x4AddHoriz) {
  // Inputs are [0.0f 1.0f 2.0f 3.0f] and [4.0f 5.0f 6.0f 7.0f].
  RunBinaryLaneOpTest<float>(execution_tier, lower_simd, kExprF32x4AddHoriz,
                             {{1.0f, 5.0f, 9.0f, 13.0f}});
}

// Test shuffle ops.
void RunShuffleOpTest(ExecutionTier execution_tier, LowerSimd lower_simd,
                      WasmOpcode simd_op,
                      const std::array<int8_t, kSimd128Size>& shuffle) {
  // Test the original shuffle.
  RunBinaryLaneOpTest<int8_t>(execution_tier, lower_simd, simd_op, shuffle);

  // Test a non-canonical (inputs reversed) version of the shuffle.
  std::array<int8_t, kSimd128Size> other_shuffle(shuffle);
  for (size_t i = 0; i < shuffle.size(); ++i) other_shuffle[i] ^= kSimd128Size;
  RunBinaryLaneOpTest<int8_t>(execution_tier, lower_simd, simd_op,
                              other_shuffle);

  // Test the swizzle (one-operand) version of the shuffle.
  std::array<int8_t, kSimd128Size> swizzle(shuffle);
  for (size_t i = 0; i < shuffle.size(); ++i) swizzle[i] &= (kSimd128Size - 1);
  RunBinaryLaneOpTest<int8_t>(execution_tier, lower_simd, simd_op, swizzle);

  // Test the non-canonical swizzle (one-operand) version of the shuffle.
  std::array<int8_t, kSimd128Size> other_swizzle(shuffle);
  for (size_t i = 0; i < shuffle.size(); ++i) other_swizzle[i] |= kSimd128Size;
  RunBinaryLaneOpTest<int8_t>(execution_tier, lower_simd, simd_op,
                              other_swizzle);
}

#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32
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

using Shuffle = std::array<int8_t, kSimd128Size>;
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

#define SHUFFLE_TEST(Name)                                          \
  WASM_SIMD_TEST(Name) {                                            \
    ShuffleMap::const_iterator it = test_shuffles.find(k##Name);    \
    DCHECK_NE(it, test_shuffles.end());                             \
    RunShuffleOpTest(execution_tier, lower_simd, kExprS8x16Shuffle, \
                     it->second);                                   \
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
    RunShuffleOpTest(execution_tier, lower_simd, kExprS8x16Shuffle, expected);
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
    RunShuffleOpTest(execution_tier, lower_simd, kExprS8x16Shuffle, expected);
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
WASM_SIMD_TEST(S8x16ShuffleFuzz) {
  v8::base::RandomNumberGenerator* rng = CcTest::random_number_generator();
  static const int kTests = 100;
  for (int i = 0; i < kTests; ++i) {
    auto shuffle = Combine(GetRandomTestShuffle(rng), GetRandomTestShuffle(rng),
                           GetRandomTestShuffle(rng));
    RunShuffleOpTest(execution_tier, lower_simd, kExprS8x16Shuffle, shuffle);
  }
}

void AppendShuffle(const Shuffle& shuffle, std::vector<byte>* buffer) {
  byte opcode[] = {WASM_SIMD_OP(kExprS8x16Shuffle)};
  for (size_t i = 0; i < arraysize(opcode); ++i) buffer->push_back(opcode[i]);
  for (size_t i = 0; i < kSimd128Size; ++i) buffer->push_back((shuffle[i]));
}

void BuildShuffle(std::vector<Shuffle>& shuffles, std::vector<byte>* buffer) {
  // Perform the leaf shuffles on globals 0 and 1.
  size_t row_index = (shuffles.size() - 1) / 2;
  for (size_t i = row_index; i < shuffles.size(); ++i) {
    byte operands[] = {WASM_GET_GLOBAL(0), WASM_GET_GLOBAL(1)};
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
  byte epilog[] = {kExprSetGlobal, static_cast<byte>(0), WASM_ONE};
  for (size_t j = 0; j < arraysize(epilog); ++j) buffer->push_back(epilog[j]);
}

// Runs tests of compiled code, using the interpreter as a reference.
#define WASM_SIMD_COMPILED_TEST(name)                               \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                  \
                             ExecutionTier execution_tier);         \
  TEST(RunWasm_##name##_turbofan) {                                 \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                  \
    RunWasm_##name##_Impl(kNoLowerSimd, ExecutionTier::kOptimized); \
  }                                                                 \
  TEST(RunWasm_##name##_simd_lowered) {                             \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                  \
    RunWasm_##name##_Impl(kLowerSimd, ExecutionTier::kOptimized);   \
  }                                                                 \
  void RunWasm_##name##_Impl(LowerSimd lower_simd, ExecutionTier execution_tier)

void RunWasmCode(ExecutionTier execution_tier, LowerSimd lower_simd,
                 const std::vector<byte>& code,
                 std::array<int8_t, kSimd128Size>* result) {
  WasmRunner<int32_t> r(execution_tier, lower_simd);
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
WASM_SIMD_COMPILED_TEST(S8x16MultiShuffleFuzz) {
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
    RunWasmCode(ExecutionTier::kInterpreter, kNoLowerSimd, buffer, &expected);
    // Run the SIMD or scalar lowered compiled code and compare results.
    std::array<int8_t, kSimd128Size> result;
    RunWasmCode(execution_tier, lower_simd, buffer, &result);
    for (size_t i = 0; i < kSimd128Size; ++i) {
      CHECK_EQ(result[i], expected[i]);
    }
  }
}
#endif  // V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS ||
        // V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_IA32

// Boolean unary operations are 'AllTrue' and 'AnyTrue', which return an integer
// result. Use relational ops on numeric vectors to create the boolean vector
// test inputs. Test inputs with all true, all false, one true, and one false.
#define WASM_SIMD_BOOL_REDUCTION_TEST(format, lanes)                           \
  WASM_SIMD_TEST(ReductionTest##lanes) {                                       \
    WasmRunner<int32_t> r(execution_tier, lower_simd);                         \
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
  WasmRunner<int32_t> r(execution_tier, lower_simd);
  BUILD(r, WASM_IF_ELSE_I(
               WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                               0, WASM_SIMD_F32x4_SPLAT(WASM_F32(30.5))),
                           WASM_I32_REINTERPRET_F32(WASM_F32(30.5))),
               WASM_I32V(1), WASM_I32V(0)));
  CHECK_EQ(1, r.Call());
}

WASM_SIMD_TEST(SimdF32x4ExtractWithI32x4) {
  WasmRunner<int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t> r(execution_tier, lower_simd);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),

        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(0)));
  CHECK_EQ(31, r.Call());
}

WASM_SIMD_TEST(SimdI32x4SplatFromExtract) {
  WasmRunner<int32_t> r(execution_tier, lower_simd);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_EXTRACT_LANE(
                                 0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(76)))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0))),
        WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_LOCAL(1)));
  CHECK_EQ(76, r.Call());
}

WASM_SIMD_TEST(SimdI32x4For) {
  WasmRunner<int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
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
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
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

WASM_SIMD_TEST(SimdLoadStoreLoad) {
  WasmRunner<int32_t> r(execution_tier, lower_simd);
  int32_t* memory =
      r.builder().AddMemoryElems<int32_t>(kWasmPageSize / sizeof(int32_t));
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
#undef WASM_SIMD_COMPILED_TEST
#undef WASM_SIMD_BOOL_REDUCTION_TEST

}  // namespace test_run_wasm_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
