// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/simplified-graph-builder.h"

namespace v8 {
namespace internal {
namespace compiler {

SimplifiedGraphBuilder::SimplifiedGraphBuilder(
    Graph* graph, CommonOperatorBuilder* common,
    MachineOperatorBuilder* machine, SimplifiedOperatorBuilder* simplified)
    : StructuredGraphBuilder(graph, common),
      machine_(machine),
      simplified_(simplified) {}


void SimplifiedGraphBuilder::Begin(int num_parameters) {
  DCHECK(graph()->start() == NULL);
  Node* start = graph()->NewNode(common()->Start(num_parameters));
  graph()->SetStart(start);
  set_environment(new (zone()) Environment(this, start));
}


void SimplifiedGraphBuilder::Return(Node* value) {
  Node* control = NewNode(common()->Return(), value);
  UpdateControlDependencyToLeaveFunction(control);
}


void SimplifiedGraphBuilder::End() {
  environment()->UpdateControlDependency(exit_control());
  graph()->SetEnd(NewNode(common()->End()));
}


SimplifiedGraphBuilder::Environment::Environment(
    SimplifiedGraphBuilder* builder, Node* control_dependency)
    : StructuredGraphBuilder::Environment(builder, control_dependency) {}


Node* SimplifiedGraphBuilder::Environment::Top() {
  DCHECK(!values()->empty());
  return values()->back();
}


void SimplifiedGraphBuilder::Environment::Push(Node* node) {
  values()->push_back(node);
}


Node* SimplifiedGraphBuilder::Environment::Pop() {
  DCHECK(!values()->empty());
  Node* back = values()->back();
  values()->pop_back();
  return back;
}


void SimplifiedGraphBuilder::Environment::Poke(size_t depth, Node* node) {
  DCHECK(depth < values()->size());
  size_t index = values()->size() - depth - 1;
  values()->at(index) = node;
}


Node* SimplifiedGraphBuilder::Environment::Peek(size_t depth) {
  DCHECK(depth < values()->size());
  size_t index = values()->size() - depth - 1;
  return values()->at(index);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
