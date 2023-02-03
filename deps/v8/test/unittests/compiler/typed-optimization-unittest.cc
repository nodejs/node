// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typed-optimization.h"

#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/execution/isolate-inl.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::IsNaN;

namespace v8 {
namespace internal {
namespace compiler {
namespace typed_optimization_unittest {

class TypedOptimizationTest : public TypedGraphTest {
 public:
  TypedOptimizationTest()
      : TypedGraphTest(3), simplified_(zone()), deps_(broker(), zone()) {}
  ~TypedOptimizationTest() override = default;

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    JSOperatorBuilder javascript(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, simplified(),
                    &machine);
    GraphReducer graph_reducer(zone(), graph(), tick_counter(), broker());
    TypedOptimization reducer(&graph_reducer, &deps_, &jsgraph, broker());
    return reducer.Reduce(node);
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  SimplifiedOperatorBuilder simplified_;
  CompilationDependencies deps_;
};

// -----------------------------------------------------------------------------
// ToBoolean

TEST_F(TypedOptimizationTest, ToBooleanWithBoolean) {
  Node* input = Parameter(Type::Boolean(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(TypedOptimizationTest, ToBooleanWithOrderedNumber) {
  Node* input = Parameter(Type::OrderedNumber(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(input, IsNumberConstant(0.0))));
}

TEST_F(TypedOptimizationTest, ToBooleanWithNumber) {
  Node* input = Parameter(Type::Number(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberToBoolean(input));
}

TEST_F(TypedOptimizationTest, ToBooleanWithDetectableReceiverOrNull) {
  Node* input = Parameter(Type::DetectableReceiverOrNull(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsReferenceEqual(input, IsNullConstant())));
}

TEST_F(TypedOptimizationTest, ToBooleanWithReceiverOrNullOrUndefined) {
  Node* input = Parameter(Type::ReceiverOrNullOrUndefined(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsBooleanNot(IsObjectIsUndetectable(input)));
}

TEST_F(TypedOptimizationTest, ToBooleanWithString) {
  Node* input = Parameter(Type::String(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsReferenceEqual(
                  input, IsHeapConstant(factory()->empty_string()))));
}

TEST_F(TypedOptimizationTest, ToBooleanWithAny) {
  Node* input = Parameter(Type::Any(), 0);
  Reduction r = Reduce(graph()->NewNode(simplified()->ToBoolean(), input));
  ASSERT_FALSE(r.Changed());
}

// -----------------------------------------------------------------------------
// ReferenceEqual
TEST_F(TypedOptimizationTest, ReferenceEqualWithBooleanTrueConstant) {
  Node* left = Parameter(Type::Boolean(), 0);
  Node* right = TrueConstant();
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ReferenceEqual(), left, right));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), left);
}

TEST_F(TypedOptimizationTest, ReferenceEqualWithBooleanFalseConstant) {
  Node* left = Parameter(Type::Boolean(), 0);
  Node* right = FalseConstant();
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ReferenceEqual(), left, right));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsBooleanNot(left));
}

}  // namespace typed_optimization_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
