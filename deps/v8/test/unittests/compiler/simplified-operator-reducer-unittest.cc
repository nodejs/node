// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-graph.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/simplified-operator-reducer.h"
#include "src/conversions.h"
#include "src/types.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedOperatorReducerTest : public GraphTest {
 public:
  explicit SimplifiedOperatorReducerTest(int num_parameters = 1)
      : GraphTest(num_parameters), simplified_(zone()) {}
  virtual ~SimplifiedOperatorReducerTest() {}

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine;
    JSOperatorBuilder javascript(zone());
    JSGraph jsgraph(graph(), common(), &javascript, &machine);
    SimplifiedOperatorReducer reducer(&jsgraph);
    return reducer.Reduce(node);
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  SimplifiedOperatorBuilder simplified_;
};


template <typename T>
class SimplifiedOperatorReducerTestWithParam
    : public SimplifiedOperatorReducerTest,
      public ::testing::WithParamInterface<T> {
 public:
  explicit SimplifiedOperatorReducerTestWithParam(int num_parameters = 1)
      : SimplifiedOperatorReducerTest(num_parameters) {}
  virtual ~SimplifiedOperatorReducerTestWithParam() {}
};


namespace {

static const double kFloat64Values[] = {
    -V8_INFINITY,  -6.52696e+290, -1.05768e+290, -5.34203e+268, -1.01997e+268,
    -8.22758e+266, -1.58402e+261, -5.15246e+241, -5.92107e+226, -1.21477e+226,
    -1.67913e+188, -1.6257e+184,  -2.60043e+170, -2.52941e+168, -3.06033e+116,
    -4.56201e+52,  -3.56788e+50,  -9.9066e+38,   -3.07261e+31,  -2.1271e+09,
    -1.91489e+09,  -1.73053e+09,  -9.30675e+08,  -26030,        -20453,
    -15790,        -11699,        -111,          -97,           -78,
    -63,           -58,           -1.53858e-06,  -2.98914e-12,  -1.14741e-39,
    -8.20347e-57,  -1.48932e-59,  -3.17692e-66,  -8.93103e-81,  -3.91337e-83,
    -6.0489e-92,   -8.83291e-113, -4.28266e-117, -1.92058e-178, -2.0567e-192,
    -1.68167e-194, -1.51841e-214, -3.98738e-234, -7.31851e-242, -2.21875e-253,
    -1.11612e-293, -0.0,          0.0,           2.22507e-308,  1.06526e-307,
    4.16643e-227,  6.76624e-223,  2.0432e-197,   3.16254e-184,  1.37315e-173,
    2.88603e-172,  1.54155e-99,   4.42923e-81,   1.40539e-73,   5.4462e-73,
    1.24064e-58,   3.11167e-58,   2.75826e-39,   0.143815,      58,
    67,            601,           7941,          11644,         13697,
    25680,         29882,         1.32165e+08,   1.62439e+08,   4.16837e+08,
    9.59097e+08,   1.32491e+09,   1.8728e+09,    1.0672e+17,    2.69606e+46,
    1.98285e+79,   1.0098e+82,    7.93064e+88,   3.67444e+121,  9.36506e+123,
    7.27954e+162,  3.05316e+168,  1.16171e+175,  1.64771e+189,  1.1622e+202,
    2.00748e+239,  2.51778e+244,  3.90282e+306,  1.79769e+308,  V8_INFINITY};


static const int32_t kInt32Values[] = {
    -2147483647 - 1, -2104508227, -2103151830, -1435284490, -1378926425,
    -1318814539,     -1289388009, -1287537572, -1279026536, -1241605942,
    -1226046939,     -941837148,  -779818051,  -413830641,  -245798087,
    -184657557,      -127145950,  -105483328,  -32325,      -26653,
    -23858,          -23834,      -22363,      -19858,      -19044,
    -18744,          -15528,      -5309,       -3372,       -2093,
    -104,            -98,         -97,         -93,         -84,
    -80,             -78,         -76,         -72,         -58,
    -57,             -56,         -55,         -45,         -40,
    -34,             -32,         -25,         -24,         -5,
    -2,              0,           3,           10,          24,
    34,              42,          46,          47,          48,
    52,              56,          64,          65,          71,
    76,              79,          81,          82,          97,
    102,             103,         104,         106,         107,
    109,             116,         122,         3653,        4485,
    12405,           16504,       26262,       28704,       29755,
    30554,           16476817,    605431957,   832401070,   873617242,
    914205764,       1062628108,  1087581664,  1488498068,  1534668023,
    1661587028,      1696896187,  1866841746,  2032089723,  2147483647};


static const uint32_t kUint32Values[] = {
    0x0,        0x5,        0x8,        0xc,        0xd,        0x26,
    0x28,       0x29,       0x30,       0x34,       0x3e,       0x42,
    0x50,       0x5b,       0x63,       0x71,       0x77,       0x7c,
    0x83,       0x88,       0x96,       0x9c,       0xa3,       0xfa,
    0x7a7,      0x165d,     0x234d,     0x3acb,     0x43a5,     0x4573,
    0x5b4f,     0x5f14,     0x6996,     0x6c6e,     0x7289,     0x7b9a,
    0x7bc9,     0x86bb,     0xa839,     0xaa41,     0xb03b,     0xc942,
    0xce68,     0xcf4c,     0xd3ad,     0xdea3,     0xe90c,     0xed86,
    0xfba5,     0x172dcc6,  0x114d8fc1, 0x182d6c9d, 0x1b1e3fad, 0x1db033bf,
    0x1e1de755, 0x1f625c80, 0x28f6cf00, 0x2acb6a94, 0x2c20240e, 0x2f0fe54e,
    0x31863a7c, 0x33325474, 0x3532fae3, 0x3bab82ea, 0x4c4b83a2, 0x4cd93d1e,
    0x4f7331d4, 0x5491b09b, 0x57cc6ff9, 0x60d3b4dc, 0x653f5904, 0x690ae256,
    0x69fe3276, 0x6bebf0ba, 0x6e2c69a3, 0x73b84ff7, 0x7b3a1924, 0x7ed032d9,
    0x84dd734b, 0x8552ea53, 0x8680754f, 0x8e9660eb, 0x94fe2b9c, 0x972d30cf,
    0x9b98c482, 0xb158667e, 0xb432932c, 0xb5b70989, 0xb669971a, 0xb7c359d1,
    0xbeb15c0d, 0xc171c53d, 0xc743dd38, 0xc8e2af50, 0xc98e2df0, 0xd9d1cdf9,
    0xdcc91049, 0xe46f396d, 0xee991950, 0xef64e521, 0xf7aeefc9, 0xffffffff};


MATCHER(IsNaN, std::string(negation ? "isn't" : "is") + " NaN") {
  return std::isnan(arg);
}

}  // namespace


// -----------------------------------------------------------------------------
// Unary operators


namespace {

struct UnaryOperator {
  const Operator* (SimplifiedOperatorBuilder::*constructor)();
  const char* constructor_name;
};


std::ostream& operator<<(std::ostream& os, const UnaryOperator& unop) {
  return os << unop.constructor_name;
}


static const UnaryOperator kUnaryOperators[] = {
    {&SimplifiedOperatorBuilder::BooleanNot, "BooleanNot"},
    {&SimplifiedOperatorBuilder::ChangeBitToBool, "ChangeBitToBool"},
    {&SimplifiedOperatorBuilder::ChangeBoolToBit, "ChangeBoolToBit"},
    {&SimplifiedOperatorBuilder::ChangeFloat64ToTagged,
     "ChangeFloat64ToTagged"},
    {&SimplifiedOperatorBuilder::ChangeInt32ToTagged, "ChangeInt32ToTagged"},
    {&SimplifiedOperatorBuilder::ChangeTaggedToFloat64,
     "ChangeTaggedToFloat64"},
    {&SimplifiedOperatorBuilder::ChangeTaggedToInt32, "ChangeTaggedToInt32"},
    {&SimplifiedOperatorBuilder::ChangeTaggedToUint32, "ChangeTaggedToUint32"},
    {&SimplifiedOperatorBuilder::ChangeUint32ToTagged, "ChangeUint32ToTagged"}};

}  // namespace


typedef SimplifiedOperatorReducerTestWithParam<UnaryOperator>
    SimplifiedUnaryOperatorTest;


TEST_P(SimplifiedUnaryOperatorTest, Parameter) {
  const UnaryOperator& unop = GetParam();
  Reduction reduction = Reduce(
      graph()->NewNode((simplified()->*unop.constructor)(), Parameter(0)));
  EXPECT_FALSE(reduction.Changed());
}


INSTANTIATE_TEST_CASE_P(SimplifiedOperatorReducerTest,
                        SimplifiedUnaryOperatorTest,
                        ::testing::ValuesIn(kUnaryOperators));


// -----------------------------------------------------------------------------
// BooleanNot


TEST_F(SimplifiedOperatorReducerTest, BooleanNotWithBooleanNot) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(
      graph()->NewNode(simplified()->BooleanNot(),
                       graph()->NewNode(simplified()->BooleanNot(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(param0, reduction.replacement());
}


TEST_F(SimplifiedOperatorReducerTest, BooleanNotWithFalseConstant) {
  Reduction reduction0 =
      Reduce(graph()->NewNode(simplified()->BooleanNot(), FalseConstant()));
  ASSERT_TRUE(reduction0.Changed());
  EXPECT_THAT(reduction0.replacement(), IsTrueConstant());
}


TEST_F(SimplifiedOperatorReducerTest, BooleanNotWithTrueConstant) {
  Reduction reduction1 =
      Reduce(graph()->NewNode(simplified()->BooleanNot(), TrueConstant()));
  ASSERT_TRUE(reduction1.Changed());
  EXPECT_THAT(reduction1.replacement(), IsFalseConstant());
}


// -----------------------------------------------------------------------------
// ChangeBoolToBit


TEST_F(SimplifiedOperatorReducerTest, ChangeBitToBoolWithChangeBoolToBit) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeBitToBool(),
      graph()->NewNode(simplified()->ChangeBoolToBit(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(param0, reduction.replacement());
}


TEST_F(SimplifiedOperatorReducerTest, ChangeBitToBoolWithZeroConstant) {
  Reduction reduction = Reduce(
      graph()->NewNode(simplified()->ChangeBitToBool(), Int32Constant(0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsFalseConstant());
}


TEST_F(SimplifiedOperatorReducerTest, ChangeBitToBoolWithOneConstant) {
  Reduction reduction = Reduce(
      graph()->NewNode(simplified()->ChangeBitToBool(), Int32Constant(1)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsTrueConstant());
}


// -----------------------------------------------------------------------------
// ChangeBoolToBit


TEST_F(SimplifiedOperatorReducerTest, ChangeBoolToBitWithFalseConstant) {
  Reduction reduction = Reduce(
      graph()->NewNode(simplified()->ChangeBoolToBit(), FalseConstant()));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsInt32Constant(0));
}


TEST_F(SimplifiedOperatorReducerTest, ChangeBoolToBitWithTrueConstant) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeBoolToBit(), TrueConstant()));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsInt32Constant(1));
}


TEST_F(SimplifiedOperatorReducerTest, ChangeBoolToBitWithChangeBitToBool) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeBoolToBit(),
      graph()->NewNode(simplified()->ChangeBitToBool(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(param0, reduction.replacement());
}


// -----------------------------------------------------------------------------
// ChangeFloat64ToTagged


TEST_F(SimplifiedOperatorReducerTest, ChangeFloat64ToTaggedWithConstant) {
  TRACED_FOREACH(double, n, kFloat64Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        simplified()->ChangeFloat64ToTagged(), Float64Constant(n)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsNumberConstant(n));
  }
}


// -----------------------------------------------------------------------------
// ChangeInt32ToTagged


TEST_F(SimplifiedOperatorReducerTest, ChangeInt32ToTaggedWithConstant) {
  TRACED_FOREACH(int32_t, n, kInt32Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        simplified()->ChangeInt32ToTagged(), Int32Constant(n)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsNumberConstant(FastI2D(n)));
  }
}


// -----------------------------------------------------------------------------
// ChangeTaggedToFloat64


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToFloat64WithChangeFloat64ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToFloat64(),
      graph()->NewNode(simplified()->ChangeFloat64ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(param0, reduction.replacement());
}


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToFloat64WithChangeInt32ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToFloat64(),
      graph()->NewNode(simplified()->ChangeInt32ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsChangeInt32ToFloat64(param0));
}


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToFloat64WithChangeUint32ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToFloat64(),
      graph()->NewNode(simplified()->ChangeUint32ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsChangeUint32ToFloat64(param0));
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToFloat64WithConstant) {
  TRACED_FOREACH(double, n, kFloat64Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        simplified()->ChangeTaggedToFloat64(), NumberConstant(n)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsFloat64Constant(n));
  }
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToFloat64WithNaNConstant1) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToFloat64(),
                              NumberConstant(-base::OS::nan_value())));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsFloat64Constant(IsNaN()));
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToFloat64WithNaNConstant2) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToFloat64(),
                              NumberConstant(base::OS::nan_value())));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsFloat64Constant(IsNaN()));
}


// -----------------------------------------------------------------------------
// ChangeTaggedToInt32


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToInt32WithChangeFloat64ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToInt32(),
      graph()->NewNode(simplified()->ChangeFloat64ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsChangeFloat64ToInt32(param0));
}


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToInt32WithChangeInt32ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToInt32(),
      graph()->NewNode(simplified()->ChangeInt32ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(param0, reduction.replacement());
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToInt32WithConstant) {
  TRACED_FOREACH(double, n, kFloat64Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        simplified()->ChangeTaggedToInt32(), NumberConstant(n)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsInt32Constant(DoubleToInt32(n)));
  }
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToInt32WithNaNConstant1) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToInt32(),
                              NumberConstant(-base::OS::nan_value())));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsInt32Constant(0));
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToInt32WithNaNConstant2) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToInt32(),
                              NumberConstant(base::OS::nan_value())));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsInt32Constant(0));
}


// -----------------------------------------------------------------------------
// ChangeTaggedToUint32


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToUint32WithChangeFloat64ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToUint32(),
      graph()->NewNode(simplified()->ChangeFloat64ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsChangeFloat64ToUint32(param0));
}


TEST_F(SimplifiedOperatorReducerTest,
       ChangeTaggedToUint32WithChangeUint32ToTagged) {
  Node* param0 = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      simplified()->ChangeTaggedToUint32(),
      graph()->NewNode(simplified()->ChangeUint32ToTagged(), param0)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(param0, reduction.replacement());
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToUint32WithConstant) {
  TRACED_FOREACH(double, n, kFloat64Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        simplified()->ChangeTaggedToUint32(), NumberConstant(n)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(),
                IsInt32Constant(bit_cast<int32_t>(DoubleToUint32(n))));
  }
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToUint32WithNaNConstant1) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToUint32(),
                              NumberConstant(-base::OS::nan_value())));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsInt32Constant(0));
}


TEST_F(SimplifiedOperatorReducerTest, ChangeTaggedToUint32WithNaNConstant2) {
  Reduction reduction =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToUint32(),
                              NumberConstant(base::OS::nan_value())));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_THAT(reduction.replacement(), IsInt32Constant(0));
}


// -----------------------------------------------------------------------------
// ChangeUint32ToTagged


TEST_F(SimplifiedOperatorReducerTest, ChangeUint32ToTagged) {
  TRACED_FOREACH(uint32_t, n, kUint32Values) {
    Reduction reduction =
        Reduce(graph()->NewNode(simplified()->ChangeUint32ToTagged(),
                                Int32Constant(bit_cast<int32_t>(n))));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsNumberConstant(FastUI2D(n)));
  }
}


// -----------------------------------------------------------------------------
// LoadElement


TEST_F(SimplifiedOperatorReducerTest, LoadElementWithConstantKeyAndLength) {
  ElementAccess const access = {kTypedArrayBoundsCheck, kUntaggedBase, 0,
                                Type::Any(), kMachAnyTagged};
  ElementAccess access_nocheck = access;
  access_nocheck.bounds_check = kNoBoundsCheck;
  Node* const base = Parameter(0);
  Node* const effect = graph()->start();
  {
    Node* const key = NumberConstant(-42.0);
    Node* const length = NumberConstant(100.0);
    Reduction r = Reduce(graph()->NewNode(simplified()->LoadElement(access),
                                          base, key, length, effect));
    ASSERT_FALSE(r.Changed());
  }
  {
    Node* const key = NumberConstant(-0.0);
    Node* const length = NumberConstant(1.0);
    Reduction r = Reduce(graph()->NewNode(simplified()->LoadElement(access),
                                          base, key, length, effect));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsLoadElement(access_nocheck, base, key, length, effect));
  }
  {
    Node* const key = NumberConstant(0);
    Node* const length = NumberConstant(1);
    Reduction r = Reduce(graph()->NewNode(simplified()->LoadElement(access),
                                          base, key, length, effect));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsLoadElement(access_nocheck, base, key, length, effect));
  }
  {
    Node* const key = NumberConstant(42.2);
    Node* const length = NumberConstant(128);
    Reduction r = Reduce(graph()->NewNode(simplified()->LoadElement(access),
                                          base, key, length, effect));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsLoadElement(access_nocheck, base, key, length, effect));
  }
  {
    Node* const key = NumberConstant(39.2);
    Node* const length = NumberConstant(32.0);
    Reduction r = Reduce(graph()->NewNode(simplified()->LoadElement(access),
                                          base, key, length, effect));
    ASSERT_FALSE(r.Changed());
  }
}


// -----------------------------------------------------------------------------
// StoreElement


TEST_F(SimplifiedOperatorReducerTest, StoreElementWithConstantKeyAndLength) {
  ElementAccess const access = {kTypedArrayBoundsCheck, kUntaggedBase, 0,
                                Type::Any(), kMachAnyTagged};
  ElementAccess access_nocheck = access;
  access_nocheck.bounds_check = kNoBoundsCheck;
  Node* const base = Parameter(0);
  Node* const value = Parameter(1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  {
    Node* const key = NumberConstant(-72.1);
    Node* const length = NumberConstant(0.0);
    Reduction r =
        Reduce(graph()->NewNode(simplified()->StoreElement(access), base, key,
                                length, value, effect, control));
    ASSERT_FALSE(r.Changed());
  }
  {
    Node* const key = NumberConstant(-0.0);
    Node* const length = NumberConstant(999);
    Reduction r =
        Reduce(graph()->NewNode(simplified()->StoreElement(access), base, key,
                                length, value, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsStoreElement(access_nocheck, base, key, length, value, effect,
                               control));
  }
  {
    Node* const key = NumberConstant(0);
    Node* const length = NumberConstant(1);
    Reduction r =
        Reduce(graph()->NewNode(simplified()->StoreElement(access), base, key,
                                length, value, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsStoreElement(access_nocheck, base, key, length, value, effect,
                               control));
  }
  {
    Node* const key = NumberConstant(42.2);
    Node* const length = NumberConstant(128);
    Reduction r =
        Reduce(graph()->NewNode(simplified()->StoreElement(access), base, key,
                                length, value, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsStoreElement(access_nocheck, base, key, length, value, effect,
                               control));
  }
  {
    Node* const key = NumberConstant(39.2);
    Node* const length = NumberConstant(32.0);
    Reduction r =
        Reduce(graph()->NewNode(simplified()->StoreElement(access), base, key,
                                length, value, effect, control));
    ASSERT_FALSE(r.Changed());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
