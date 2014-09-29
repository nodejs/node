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

  template <typename T>
  void CheckConstant(T expected, Node* node) {
    CHECK_EQ(expected, ValueOf<T>(node->op()));
  }

  void CheckFloat64Constant(double expected, Node* node) {
    CHECK_EQ(IrOpcode::kFloat64Constant, node->opcode());
    CHECK_EQ(expected, ValueOf<double>(node->op()));
  }

  void CheckNumberConstant(double expected, Node* node) {
    CHECK_EQ(IrOpcode::kNumberConstant, node->opcode());
    CHECK_EQ(expected, ValueOf<double>(node->op()));
  }

  void CheckInt32Constant(int32_t expected, Node* node) {
    CHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    CHECK_EQ(expected, ValueOf<int32_t>(node->op()));
  }

  void CheckUint32Constant(int32_t expected, Node* node) {
    CHECK_EQ(IrOpcode::kInt32Constant, node->opcode());
    CHECK_EQ(expected, ValueOf<uint32_t>(node->op()));
  }

  void CheckHeapConstant(Object* expected, Node* node) {
    CHECK_EQ(IrOpcode::kHeapConstant, node->opcode());
    CHECK_EQ(expected, *ValueOf<Handle<Object> >(node->op()));
  }

  void CheckTrue(Node* node) {
    CheckHeapConstant(isolate_->heap()->true_value(), node);
  }

  void CheckFalse(Node* node) {
    CheckHeapConstant(isolate_->heap()->false_value(), node);
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
    return std::vector<double>(&values[0], &values[ARRAY_SIZE(values)]);
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
    return std::vector<uint32_t>(&kValues[0], &kValues[ARRAY_SIZE(kValues)]);
  }

  static const std::vector<double> nan_vector(size_t limit = 0) {
    static const double nan = v8::base::OS::nan_value();
    static const double values[] = {-nan,               -V8_INFINITY * -0.0,
                                    -V8_INFINITY * 0.0, V8_INFINITY * -0.0,
                                    V8_INFINITY * 0.0,  nan};
    return std::vector<double>(&values[0], &values[ARRAY_SIZE(values)]);
  }

  static const std::vector<uint32_t> ror_vector() {
    static const uint32_t kValues[31] = {
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    return std::vector<uint32_t>(&kValues[0], &kValues[ARRAY_SIZE(kValues)]);
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
#define FOR_FLOAT64_INPUTS(var) FOR_INPUTS(double, float64, var)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_VALUE_HELPER_H_
