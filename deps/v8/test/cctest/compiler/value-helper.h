// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_VALUE_HELPER_H_
#define V8_CCTEST_COMPILER_VALUE_HELPER_H_

#include "src/v8.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"
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
    CHECK_EQ(expected, OpParameter<double>(node));
  }

  void CheckNumberConstant(double expected, Node* node) {
    CHECK_EQ(IrOpcode::kNumberConstant, node->opcode());
    CHECK_EQ(expected, OpParameter<double>(node));
  }

  void CheckInt32Constant(int32_t expected, Node* node) {
    CHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    CHECK_EQ(expected, OpParameter<int32_t>(node));
  }

  void CheckUint32Constant(int32_t expected, Node* node) {
    CHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    CHECK_EQ(expected, OpParameter<uint32_t>(node));
  }

  void CheckHeapConstant(Object* expected, Node* node) {
    CHECK_EQ(IrOpcode::kHeapConstant, node->opcode());
    CHECK_EQ(expected, *OpParameter<Unique<Object> >(node).handle());
  }

  void CheckTrue(Node* node) {
    CheckHeapConstant(isolate_->heap()->true_value(), node);
  }

  void CheckFalse(Node* node) {
    CheckHeapConstant(isolate_->heap()->false_value(), node);
  }

  static std::vector<float> float32_vector() {
    static const float kValues[] = {
        -std::numeric_limits<float>::infinity(), -2.70497e+38f, -1.4698e+37f,
        -1.22813e+35f,                           -1.20555e+35f, -1.34584e+34f,
        -1.0079e+32f,                            -6.49364e+26f, -3.06077e+25f,
        -1.46821e+25f,                           -1.17658e+23f, -1.9617e+22f,
        -2.7357e+20f,                            -1.48708e+13f, -1.89633e+12f,
        -4.66622e+11f,                           -2.22581e+11f, -1.45381e+10f,
        -1.3956e+09f,                            -1.32951e+09f, -1.30721e+09f,
        -1.19756e+09f,                           -9.26822e+08f, -6.35647e+08f,
        -4.00037e+08f,                           -1.81227e+08f, -5.09256e+07f,
        -964300.0f,                              -192446.0f,    -28455.0f,
        -27194.0f,                               -26401.0f,     -20575.0f,
        -17069.0f,                               -9167.0f,      -960.178f,
        -113.0f,                                 -62.0f,        -15.0f,
        -7.0f,                                   -0.0256635f,   -4.60374e-07f,
        -3.63759e-10f,                           -4.30175e-14f, -5.27385e-15f,
        -1.48084e-15f,                           -1.05755e-19f, -3.2995e-21f,
        -1.67354e-23f,                           -1.11885e-23f, -1.78506e-30f,
        -5.07594e-31f,                           -3.65799e-31f, -1.43718e-34f,
        -1.27126e-38f,                           -0.0f,         0.0f,
        1.17549e-38f,                            1.56657e-37f,  4.08512e-29f,
        3.31357e-28f,                            6.25073e-22f,  4.1723e-13f,
        1.44343e-09f,                            5.27004e-08f,  9.48298e-08f,
        5.57888e-07f,                            4.89988e-05f,  0.244326f,
        12.4895f,                                19.0f,         47.0f,
        106.0f,                                  538.324f,      564.536f,
        819.124f,                                7048.0f,       12611.0f,
        19878.0f,                                20309.0f,      797056.0f,
        1.77219e+09f,                            1.51116e+11f,  4.18193e+13f,
        3.59167e+16f,                            3.38211e+19f,  2.67488e+20f,
        1.78831e+21f,                            9.20914e+21f,  8.35654e+23f,
        1.4495e+24f,                             5.94015e+25f,  4.43608e+30f,
        2.44502e+33f,                            2.61152e+33f,  1.38178e+37f,
        1.71306e+37f,                            3.31899e+38f,  3.40282e+38f,
        std::numeric_limits<float>::infinity()};
    return std::vector<float>(&kValues[0], &kValues[arraysize(kValues)]);
  }

  static std::vector<double> float64_vector() {
    static const double nan = v8::base::OS::nan_value();
    static const double values[] = {
        0.125,           0.25,            0.375,          0.5,
        1.25,            -1.75,           2,              5.125,
        6.25,            0.0,             -0.0,           982983.25,
        888,             2147483647.0,    -999.75,        3.1e7,
        -2e66,           3e-88,           -2147483648.0,  V8_INFINITY,
        -V8_INFINITY,    nan,             2147483647.375, 2147483647.75,
        2147483648.0,    2147483648.25,   2147483649.25,  -2147483647.0,
        -2147483647.125, -2147483647.875, -2147483648.25, -2147483649.5};
    return std::vector<double>(&values[0], &values[arraysize(values)]);
  }

  static const std::vector<int32_t> int32_vector() {
    std::vector<uint32_t> values = uint32_vector();
    return std::vector<int32_t>(values.begin(), values.end());
  }

  static const std::vector<uint32_t> uint32_vector() {
    static const uint32_t kValues[] = {
        0x00000000, 0x00000001, 0xffffffff, 0x1b09788b, 0x04c5fce8, 0xcc0de5bf,
        0x273a798e, 0x187937a3, 0xece3af83, 0x5495a16b, 0x0b668ecc, 0x11223344,
        0x0000009e, 0x00000043, 0x0000af73, 0x0000116b, 0x00658ecc, 0x002b3b4c,
        0x88776655, 0x70000000, 0x07200000, 0x7fffffff, 0x56123761, 0x7fffff00,
        0x761c4761, 0x80000000, 0x88888888, 0xa0000000, 0xdddddddd, 0xe0000000,
        0xeeeeeeee, 0xfffffffd, 0xf0000000, 0x007fffff, 0x003fffff, 0x001fffff,
        0x000fffff, 0x0007ffff, 0x0003ffff, 0x0001ffff, 0x0000ffff, 0x00007fff,
        0x00003fff, 0x00001fff, 0x00000fff, 0x000007ff, 0x000003ff, 0x000001ff};
    return std::vector<uint32_t>(&kValues[0], &kValues[arraysize(kValues)]);
  }

  static const std::vector<double> nan_vector(size_t limit = 0) {
    static const double nan = v8::base::OS::nan_value();
    static const double values[] = {-nan,               -V8_INFINITY * -0.0,
                                    -V8_INFINITY * 0.0, V8_INFINITY * -0.0,
                                    V8_INFINITY * 0.0,  nan};
    return std::vector<double>(&values[0], &values[arraysize(values)]);
  }

  static const std::vector<uint32_t> ror_vector() {
    static const uint32_t kValues[31] = {
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    return std::vector<uint32_t>(&kValues[0], &kValues[arraysize(kValues)]);
  }
};

// Helper macros that can be used in FOR_INT32_INPUTS(i) { ... *i ... }
// Watch out, these macros aren't hygenic; they pollute your scope. Thanks STL.
#define FOR_INPUTS(ctype, itype, var)                           \
  std::vector<ctype> var##_vec = ValueHelper::itype##_vector(); \
  for (std::vector<ctype>::iterator var = var##_vec.begin();    \
       var != var##_vec.end(); ++var)

#define FOR_INT32_INPUTS(var) FOR_INPUTS(int32_t, int32, var)
#define FOR_UINT32_INPUTS(var) FOR_INPUTS(uint32_t, uint32, var)
#define FOR_FLOAT32_INPUTS(var) FOR_INPUTS(float, float32, var)
#define FOR_FLOAT64_INPUTS(var) FOR_INPUTS(double, float64, var)

#define FOR_INT32_SHIFTS(var) for (int32_t var = 0; var < 32; var++)

#define FOR_UINT32_SHIFTS(var) for (uint32_t var = 0; var < 32; var++)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_VALUE_HELPER_H_
