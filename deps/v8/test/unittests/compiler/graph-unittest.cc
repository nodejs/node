// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/graph-unittest.h"

#include <ostream>  // NOLINT(readability/streams)

#include "src/compiler/node-properties-inl.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

GraphTest::GraphTest(int num_parameters) : common_(zone()), graph_(zone()) {
  graph()->SetStart(graph()->NewNode(common()->Start(num_parameters)));
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
  return HeapConstant(Unique<HeapObject>::CreateUninitialized(value));
}


Node* GraphTest::HeapConstant(const Unique<HeapObject>& value) {
  Node* node = graph()->NewNode(common()->HeapConstant(value));
  Type* type = Type::Constant(value.handle(), zone());
  NodeProperties::SetBounds(node, Bounds(type));
  return node;
}


Node* GraphTest::FalseConstant() {
  return HeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->false_value()));
}


Node* GraphTest::TrueConstant() {
  return HeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->true_value()));
}


Node* GraphTest::UndefinedConstant() {
  return HeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->undefined_value()));
}


Matcher<Node*> GraphTest::IsFalseConstant() {
  return IsHeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->false_value()));
}


Matcher<Node*> GraphTest::IsTrueConstant() {
  return IsHeapConstant(
      Unique<HeapObject>::CreateImmovable(factory()->true_value()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
