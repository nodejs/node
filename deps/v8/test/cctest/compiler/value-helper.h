// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_VALUE_HELPER_H_
#define V8_CCTEST_COMPILER_VALUE_HELPER_H_

#include <stdint.h>

#include "src/base/template-utils.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node.h"
#include "src/heap/heap-inl.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

// A collection of utilities related to numerical and heap values, including
// example input values of various types, including int32_t, uint32_t, double,
// etc.
class ValueHelper {
 public:
  Isolate* isolate_;

  ValueHelper() : isolate_(CcTest::InitIsolateOnce()) {}

  void CheckFloat64Constant(double expected, Node* node) {
    CHECK_EQ(IrOpcode::kFloat64Constant, node->opcode());
    CHECK_EQ(expected, OpParameter<double>(node->op()));
  }

  void CheckNumberConstant(double expected, Node* node) {
    CHECK_EQ(IrOpcode::kNumberConstant, node->opcode());
    CHECK_EQ(expected, OpParameter<double>(node->op()));
  }

  void CheckInt32Constant(int32_t expected, Node* node) {
    CHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    CHECK_EQ(expected, OpParameter<int32_t>(node->op()));
  }

  void CheckUint32Constant(int32_t expected, Node* node) {
    CHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    CHECK_EQ(expected, OpParameter<int32_t>(node->op()));
  }

  void CheckHeapConstant(HeapObject* expected, Node* node) {
    CHECK_EQ(IrOpcode::kHeapConstant, node->opcode());
    CHECK_EQ(expected, *HeapConstantOf(node->op()));
  }

  void CheckTrue(Node* node) {
    CheckHeapConstant(isolate_->heap()->true_value(), node);
  }

  void CheckFalse(Node* node) {
    CheckHeapConstant(isolate_->heap()->false_value(), node);
  }

  static constexpr float float32_array[] = {
      -std::numeric_limits<float>::infinity(),
      -2.70497e+38f,
      -1.4698e+37f,
      -1.22813e+35f,
      -1.20555e+35f,
      -1.34584e+34f,
      -1.0079e+32f,
      -6.49364e+26f,
      -3.06077e+25f,
      -1.46821e+25f,
      -1.17658e+23f,
      -1.9617e+22f,
      -2.7357e+20f,
      -9223372036854775808.0f,  // INT64_MIN
      -1.48708e+13f,
      -1.89633e+12f,
      -4.66622e+11f,
      -2.22581e+11f,
      -1.45381e+10f,
      -2147483904.0f,  // First float32 after INT32_MIN
      -2147483648.0f,  // INT32_MIN
      -2147483520.0f,  // Last float32 before INT32_MIN
      -1.3956e+09f,
      -1.32951e+09f,
      -1.30721e+09f,
      -1.19756e+09f,
      -9.26822e+08f,
      -6.35647e+08f,
      -4.00037e+08f,
      -1.81227e+08f,
      -5.09256e+07f,
      -964300.0f,
      -192446.0f,
      -28455.0f,
      -27194.0f,
      -26401.0f,
      -20575.0f,
      -17069.0f,
      -9167.0f,
      -960.178f,
      -113.0f,
      -62.0f,
      -15.0f,
      -7.0f,
      -1.0f,
      -0.0256635f,
      -4.60374e-07f,
      -3.63759e-10f,
      -4.30175e-14f,
      -5.27385e-15f,
      -1.5707963267948966f,
      -1.48084e-15f,
      -2.220446049250313e-16f,
      -1.05755e-19f,
      -3.2995e-21f,
      -1.67354e-23f,
      -1.11885e-23f,
      -1.78506e-30f,
      -5.07594e-31f,
      -3.65799e-31f,
      -1.43718e-34f,
      -1.27126e-38f,
      -0.0f,
      0.0f,
      1.17549e-38f,
      1.56657e-37f,
      4.08512e-29f,
      3.31357e-28f,
      6.25073e-22f,
      4.1723e-13f,
      1.44343e-09f,
      1.5707963267948966f,
      5.27004e-08f,
      9.48298e-08f,
      5.57888e-07f,
      4.89988e-05f,
      0.244326f,
      1.0f,
      12.4895f,
      19.0f,
      47.0f,
      106.0f,
      538.324f,
      564.536f,
      819.124f,
      7048.0f,
      12611.0f,
      19878.0f,
      20309.0f,
      797056.0f,
      1.77219e+09f,
      2147483648.0f,  // INT32_MAX + 1
      4294967296.0f,  // UINT32_MAX + 1
      1.51116e+11f,
      4.18193e+13f,
      3.59167e+16f,
      9223372036854775808.0f,   // INT64_MAX + 1
      18446744073709551616.0f,  // UINT64_MAX + 1
      3.38211e+19f,
      2.67488e+20f,
      1.78831e+21f,
      9.20914e+21f,
      8.35654e+23f,
      1.4495e+24f,
      5.94015e+25f,
      4.43608e+30f,
      2.44502e+33f,
      2.61152e+33f,
      1.38178e+37f,
      1.71306e+37f,
      3.31899e+38f,
      3.40282e+38f,
      std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::quiet_NaN(),
      -std::numeric_limits<float>::quiet_NaN()};

  static constexpr Vector<const float> float32_vector() {
    return ArrayVector(float32_array);
  }

  static constexpr double float64_array[] = {
      -2e66,
      -2.220446049250313e-16,
      -9223373136366403584.0,
      -9223372036854775808.0,  // INT64_MIN
      -2147483649.5,
      -2147483648.25,
      -2147483648.0,
      -2147483647.875,
      -2147483647.125,
      -2147483647.0,
      -999.75,
      -2e66,
      -1.75,
      -1.5707963267948966,
      -1.0,
      -0.5,
      -0.0,
      0.0,
      3e-88,
      0.125,
      0.25,
      0.375,
      0.5,
      1.0,
      1.17549e-38,
      1.56657e-37,
      1.0000001,
      1.25,
      1.5707963267948966,
      2,
      3.1e7,
      5.125,
      6.25,
      888,
      982983.25,
      2147483647.0,
      2147483647.375,
      2147483647.75,
      2147483648.0,
      2147483648.25,
      2147483649.25,
      9223372036854775808.0,  // INT64_MAX + 1
      9223373136366403584.0,
      18446744073709551616.0,  // UINT64_MAX + 1
      2e66,
      V8_INFINITY,
      -V8_INFINITY,
      std::numeric_limits<double>::quiet_NaN(),
      -std::numeric_limits<double>::quiet_NaN()};

  static constexpr Vector<const double> float64_vector() {
    return ArrayVector(float64_array);
  }

  static constexpr uint32_t uint32_array[] = {
      0x00000000, 0x00000001, 0xFFFFFFFF, 0x1B09788B, 0x04C5FCE8, 0xCC0DE5BF,
      // This row is useful for testing lea optimizations on intel.
      0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000008, 0x00000009,
      0x273A798E, 0x187937A3, 0xECE3AF83, 0x5495A16B, 0x0B668ECC, 0x11223344,
      0x0000009E, 0x00000043, 0x0000AF73, 0x0000116B, 0x00658ECC, 0x002B3B4C,
      0x88776655, 0x70000000, 0x07200000, 0x7FFFFFFF, 0x56123761, 0x7FFFFF00,
      0x761C4761, 0x80000000, 0x88888888, 0xA0000000, 0xDDDDDDDD, 0xE0000000,
      0xEEEEEEEE, 0xFFFFFFFD, 0xF0000000, 0x007FFFFF, 0x003FFFFF, 0x001FFFFF,
      0x000FFFFF, 0x0007FFFF, 0x0003FFFF, 0x0001FFFF, 0x0000FFFF, 0x00007FFF,
      0x00003FFF, 0x00001FFF, 0x00000FFF, 0x000007FF, 0x000003FF, 0x000001FF,
      // Bit pattern of a quiet NaN and signaling NaN, with or without
      // additional payload.
      0x7FC00000, 0x7F800000, 0x7FFFFFFF, 0x7F876543};

  static constexpr Vector<const uint32_t> uint32_vector() {
    return ArrayVector(uint32_array);
  }

  static constexpr Vector<const int32_t> int32_vector() {
    return Vector<const int32_t>::cast(uint32_vector());
  }

  static constexpr uint64_t uint64_array[] = {
      0x00000000, 0x00000001, 0xFFFFFFFF, 0x1B09788B, 0x04C5FCE8, 0xCC0DE5BF,
      0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000008, 0x00000009,
      0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFE, 0xFFFFFFFFFFFFFFFD,
      0x0000000000000000, 0x0000000100000000, 0xFFFFFFFF00000000,
      0x1B09788B00000000, 0x04C5FCE800000000, 0xCC0DE5BF00000000,
      0x0000000200000000, 0x0000000300000000, 0x0000000400000000,
      0x0000000500000000, 0x0000000800000000, 0x0000000900000000,
      0x273A798E187937A3, 0xECE3AF835495A16B, 0x0B668ECC11223344, 0x0000009E,
      0x00000043, 0x0000AF73, 0x0000116B, 0x00658ECC, 0x002B3B4C, 0x88776655,
      0x70000000, 0x07200000, 0x7FFFFFFF, 0x56123761, 0x7FFFFF00,
      0x761C4761EEEEEEEE, 0x80000000EEEEEEEE, 0x88888888DDDDDDDD,
      0xA0000000DDDDDDDD, 0xDDDDDDDDAAAAAAAA, 0xE0000000AAAAAAAA,
      0xEEEEEEEEEEEEEEEE, 0xFFFFFFFDEEEEEEEE, 0xF0000000DDDDDDDD,
      0x007FFFFFDDDDDDDD, 0x003FFFFFAAAAAAAA, 0x001FFFFFAAAAAAAA, 0x000FFFFF,
      0x0007FFFF, 0x0003FFFF, 0x0001FFFF, 0x0000FFFF, 0x00007FFF, 0x00003FFF,
      0x00001FFF, 0x00000FFF, 0x000007FF, 0x000003FF, 0x000001FF,
      0x00003FFFFFFFFFFF, 0x00001FFFFFFFFFFF, 0x00000FFFFFFFFFFF,
      0x000007FFFFFFFFFF, 0x000003FFFFFFFFFF, 0x000001FFFFFFFFFF,
      0x8000008000000000, 0x8000008000000001, 0x8000000000000400,
      0x8000000000000401, 0x0000000000000020,
      // Bit pattern of a quiet NaN and signaling NaN, with or without
      // additional payload.
      0x7FF8000000000000, 0x7FF0000000000000, 0x7FF8123456789ABC,
      0x7FF7654321FEDCBA};

  static constexpr Vector<const uint64_t> uint64_vector() {
    return ArrayVector(uint64_array);
  }

  static constexpr Vector<const int64_t> int64_vector() {
    return Vector<const int64_t>::cast(uint64_vector());
  }

  static constexpr int16_t int16_array[] = {
      0, 1, 2, INT16_MAX - 1, INT16_MAX, INT16_MIN, INT16_MIN + 1, -2, -1};

  static constexpr Vector<const int16_t> int16_vector() {
    return ArrayVector(int16_array);
  }

  static constexpr Vector<const uint16_t> uint16_vector() {
    return Vector<const uint16_t>::cast(int16_vector());
  }

  static constexpr int8_t int8_array[] = {
      0, 1, 2, INT8_MAX - 1, INT8_MAX, INT8_MIN, INT8_MIN + 1, -2, -1};

  static constexpr Vector<const int8_t> int8_vector() {
    return ArrayVector(int8_array);
  }

  static constexpr Vector<const uint8_t> uint8_vector() {
    return Vector<const uint8_t>::cast(ArrayVector(int8_array));
  }

  static constexpr uint32_t ror_array[31] = {
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

  static constexpr Vector<const uint32_t> ror_vector() {
    return ArrayVector(ror_array);
  }
};

// Helper macros that can be used in FOR_INT32_INPUTS(i) { ... *i ... }
// Watch out, these macros aren't hygenic; they pollute your scope. Thanks STL.
#define FOR_INPUTS(ctype, itype, var)                             \
  Vector<const ctype> var##_vec =                                 \
      ::v8::internal::compiler::ValueHelper::itype##_vector();    \
  for (Vector<const ctype>::iterator var = var##_vec.begin(),     \
                                     var##_end = var##_vec.end(); \
       var != var##_end; ++var)

#define FOR_INT32_INPUTS(var) FOR_INPUTS(int32_t, int32, var)
#define FOR_UINT32_INPUTS(var) FOR_INPUTS(uint32_t, uint32, var)
#define FOR_INT16_INPUTS(var) FOR_INPUTS(int16_t, int16, var)
#define FOR_UINT16_INPUTS(var) FOR_INPUTS(uint16_t, uint16, var)
#define FOR_INT8_INPUTS(var) FOR_INPUTS(int8_t, int8, var)
#define FOR_UINT8_INPUTS(var) FOR_INPUTS(uint8_t, uint8, var)
#define FOR_INT64_INPUTS(var) FOR_INPUTS(int64_t, int64, var)
#define FOR_UINT64_INPUTS(var) FOR_INPUTS(uint64_t, uint64, var)
#define FOR_FLOAT32_INPUTS(var) FOR_INPUTS(float, float32, var)
#define FOR_FLOAT64_INPUTS(var) FOR_INPUTS(double, float64, var)

#define FOR_INT32_SHIFTS(var) for (int32_t var = 0; var < 32; var++)

#define FOR_UINT32_SHIFTS(var) for (uint32_t var = 0; var < 32; var++)

// TODO(bmeurer): Drop this crap once we switch to GTest/Gmock.
static inline void CheckFloatEq(volatile float x, volatile float y) {
  if (std::isnan(x)) {
    CHECK(std::isnan(y));
  } else {
    CHECK_EQ(x, y);
    CHECK_EQ(std::signbit(x), std::signbit(y));
  }
}

#define CHECK_FLOAT_EQ(lhs, rhs)                      \
  do {                                                \
    volatile float tmp = lhs;                         \
    ::v8::internal::compiler::CheckFloatEq(tmp, rhs); \
  } while (0)

static inline void CheckDoubleEq(volatile double x, volatile double y) {
  if (std::isnan(x)) {
    CHECK(std::isnan(y));
  } else {
    CHECK_EQ(x, y);
    CHECK_EQ(std::signbit(x), std::signbit(y));
  }
}

#define CHECK_DOUBLE_EQ(lhs, rhs)                      \
  do {                                                 \
    volatile double tmp = lhs;                         \
    ::v8::internal::compiler::CheckDoubleEq(tmp, rhs); \
  } while (0)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_VALUE_HELPER_H_
