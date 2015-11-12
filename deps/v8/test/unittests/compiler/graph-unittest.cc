// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/graph-unittest.h"

#include "src/compiler/node-properties.h"
#include "src/factory.h"
#include "src/objects-inl.h"  // TODO(everyone): Make typer.h IWYU compliant.
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

GraphTest::GraphTest(int num_parameters) : common_(zone()), graph_(zone()) {
  graph()->SetStart(graph()->NewNode(common()->Start(num_parameters)));
  graph()->SetEnd(graph()->NewNode(common()->End(1), graph()->start()));
}


GraphTest::~GraphTest() {}


Node* GraphTest::Parameter(int32_t index) {
  return graph()->NewNode(common()->Parameter(index), graph()->start());
}


Node* GraphTest::Float32Constant(volatile float value) {
  return graph()->NewNode(common()->Float32Constant(value));
}


Node* GraphTest::Float64Constant(volatile double value) {
  return graph()->NewNode(common()->Float64Constant(value));
}


Node* GraphTest::Int32Constant(int32_t value) {
  return graph()->NewNode(common()->Int32Constant(value));
}


Node* GraphTest::Int64Constant(int64_t value) {
  return graph()->NewNode(common()->Int64Constant(value));
}


Node* GraphTest::NumberConstant(volatile double value) {
  return graph()->NewNode(common()->NumberConstant(value));
}


Node* GraphTest::HeapConstant(const Handle<HeapObject>& value) {
  Node* node = graph()->NewNode(common()->HeapConstant(value));
  Type* type = Type::Constant(value, zone());
  NodeProperties::SetType(node, type);
  return node;
}


Node* GraphTest::FalseConstant() {
  return HeapConstant(factory()->false_value());
}


Node* GraphTest::TrueConstant() {
  return HeapConstant(factory()->true_value());
}


Node* GraphTest::UndefinedConstant() {
  return HeapConstant(factory()->undefined_value());
}


Node* GraphTest::EmptyFrameState() {
  Node* state_values = graph()->NewNode(common()->StateValues(0));
  return graph()->NewNode(
      common()->FrameState(BailoutId::None(), OutputFrameStateCombine::Ignore(),
                           nullptr),
      state_values, state_values, state_values, NumberConstant(0),
      UndefinedConstant(), graph()->start());
}


Matcher<Node*> GraphTest::IsFalseConstant() {
  return IsHeapConstant(factory()->false_value());
}


Matcher<Node*> GraphTest::IsTrueConstant() {
  return IsHeapConstant(factory()->true_value());
}


Matcher<Node*> GraphTest::IsUndefinedConstant() {
  return IsHeapConstant(factory()->undefined_value());
}


TypedGraphTest::TypedGraphTest(int num_parameters)
    : GraphTest(num_parameters), typer_(isolate(), graph()) {}


TypedGraphTest::~TypedGraphTest() {}


Node* TypedGraphTest::Parameter(Type* type, int32_t index) {
  Node* node = GraphTest::Parameter(index);
  NodeProperties::SetType(node, type);
  return node;
}


namespace {

const Operator kDummyOperator(0, Operator::kNoProperties, "Dummy", 0, 0, 0, 1,
                              0, 0);

}  // namespace


TEST_F(GraphTest, NewNode) {
  Node* n0 = graph()->NewNode(&kDummyOperator);
  Node* n1 = graph()->NewNode(&kDummyOperator);
  EXPECT_NE(n0, n1);
  EXPECT_LT(0u, n0->id());
  EXPECT_LT(0u, n1->id());
  EXPECT_NE(n0->id(), n1->id());
  EXPECT_EQ(&kDummyOperator, n0->op());
  EXPECT_EQ(&kDummyOperator, n1->op());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
