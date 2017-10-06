// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typed-optimization.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/isolate-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::IsNaN;

namespace v8 {
namespace internal {
namespace compiler {

namespace {

const double kFloat64Values[] = {
    -V8_INFINITY,  -4.23878e+275, -5.82632e+265, -6.60355e+220,
    -6.26172e+212, -2.56222e+211, -4.82408e+201, -1.84106e+157,
    -1.63662e+127, -1.55772e+100, -1.67813e+72,  -2.3382e+55,
    -3.179e+30,    -1.441e+09,    -1.0647e+09,   -7.99361e+08,
    -5.77375e+08,  -2.20984e+08,  -32757,        -13171,
    -9970,         -3984,         -107,          -105,
    -92,           -77,           -61,           -0.000208163,
    -1.86685e-06,  -1.17296e-10,  -9.26358e-11,  -5.08004e-60,
    -1.74753e-65,  -1.06561e-71,  -5.67879e-79,  -5.78459e-130,
    -2.90989e-171, -7.15489e-243, -3.76242e-252, -1.05639e-263,
    -4.40497e-267, -2.19666e-273, -4.9998e-276,  -5.59821e-278,
    -2.03855e-282, -5.99335e-283, -7.17554e-284, -3.11744e-309,
    -0.0,          0.0,           2.22507e-308,  1.30127e-270,
    7.62898e-260,  4.00313e-249,  3.16829e-233,  1.85244e-228,
    2.03544e-129,  1.35126e-110,  1.01182e-106,  5.26333e-94,
    1.35292e-90,   2.85394e-83,   1.78323e-77,   5.4967e-57,
    1.03207e-25,   4.57401e-25,   1.58738e-05,   2,
    125,           2310,          9636,          14802,
    17168,         28945,         29305,         4.81336e+07,
    1.41207e+08,   4.65962e+08,   1.40499e+09,   2.12648e+09,
    8.80006e+30,   1.4446e+45,    1.12164e+54,   2.48188e+89,
    6.71121e+102,  3.074e+112,    4.9699e+152,   5.58383e+166,
    4.30654e+172,  7.08824e+185,  9.6586e+214,   2.028e+223,
    6.63277e+243,  1.56192e+261,  1.23202e+269,  5.72883e+289,
    8.5798e+290,   1.40256e+294,  1.79769e+308,  V8_INFINITY};

const double kIntegerValues[] = {-V8_INFINITY, INT_MIN, -1000.0,  -42.0,
                                 -1.0,         0.0,     1.0,      42.0,
                                 1000.0,       INT_MAX, UINT_MAX, V8_INFINITY};

}  // namespace

class TypedOptimizationTest : public TypedGraphTest {
 public:
  TypedOptimizationTest()
      : TypedGraphTest(3), javascript_(zone()), deps_(isolate(), zone()) {}
  ~TypedOptimizationTest() override {}

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    TypedOptimization reducer(&graph_reducer, &deps_,
                              TypedOptimization::kDeoptimizationEnabled,
                              &jsgraph);
    return reducer.Reduce(node);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
  CompilationDependencies deps_;
};

TEST_F(TypedOptimizationTest, ParameterWithMinusZero) {
  {
    Reduction r = Reduce(
        Parameter(Type::NewConstant(factory()->minus_zero_value(), zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(-0.0));
  }
  {
    Reduction r = Reduce(Parameter(Type::MinusZero()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(-0.0));
  }
  {
    Reduction r = Reduce(Parameter(Type::Union(
        Type::MinusZero(), Type::NewConstant(factory()->NewNumber(0), zone()),
        zone())));
    EXPECT_FALSE(r.Changed());
  }
}

TEST_F(TypedOptimizationTest, ParameterWithNull) {
  Handle<HeapObject> null = factory()->null_value();
  {
    Reduction r = Reduce(Parameter(Type::NewConstant(null, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsHeapConstant(null));
  }
  {
    Reduction r = Reduce(Parameter(Type::Null()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsHeapConstant(null));
  }
}

TEST_F(TypedOptimizationTest, ParameterWithNaN) {
  const double kNaNs[] = {-std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::signaling_NaN()};
  TRACED_FOREACH(double, nan, kNaNs) {
    Handle<Object> constant = factory()->NewNumber(nan);
    Reduction r = Reduce(Parameter(Type::NewConstant(constant, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(IsNaN()));
  }
  {
    Reduction r =
        Reduce(Parameter(Type::NewConstant(factory()->nan_value(), zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(IsNaN()));
  }
  {
    Reduction r = Reduce(Parameter(Type::NaN()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(IsNaN()));
  }
}

TEST_F(TypedOptimizationTest, ParameterWithPlainNumber) {
  TRACED_FOREACH(double, value, kFloat64Values) {
    Handle<Object> constant = factory()->NewNumber(value);
    Reduction r = Reduce(Parameter(Type::NewConstant(constant, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(value));
  }
  TRACED_FOREACH(double, value, kIntegerValues) {
    Reduction r = Reduce(Parameter(Type::Range(value, value, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(value));
  }
}

TEST_F(TypedOptimizationTest, ParameterWithUndefined) {
  Handle<HeapObject> undefined = factory()->undefined_value();
  {
    Reduction r = Reduce(Parameter(Type::Undefined()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsHeapConstant(undefined));
  }
  {
    Reduction r = Reduce(Parameter(Type::NewConstant(undefined, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsHeapConstant(undefined));
  }
}

TEST_F(TypedOptimizationTest, JSToBooleanWithFalsish) {
  Node* input = Parameter(
      Type::Union(
          Type::MinusZero(),
          Type::Union(
              Type::NaN(),
              Type::Union(
                  Type::Null(),
                  Type::Union(
                      Type::Undefined(),
                      Type::Union(
                          Type::Undetectable(),
                          Type::Union(Type::NewConstant(
                                          factory()->false_value(), zone()),
                                      Type::Range(0.0, 0.0, zone()), zone()),
                          zone()),
                      zone()),
                  zone()),
              zone()),
          zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}

TEST_F(TypedOptimizationTest, JSToBooleanWithTruish) {
  Node* input = Parameter(
      Type::Union(
          Type::NewConstant(factory()->true_value(), zone()),
          Type::Union(Type::DetectableReceiver(), Type::Symbol(), zone()),
          zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsTrueConstant());
}

TEST_F(TypedOptimizationTest, JSToBooleanWithNonZeroPlainNumber) {
  Node* input = Parameter(Type::Range(1, V8_INFINITY, zone()), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsTrueConstant());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
