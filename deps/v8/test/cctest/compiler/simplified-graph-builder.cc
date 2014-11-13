// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/simplified-graph-builder.h"

#include "src/compiler/operator-properties.h"
#include "src/compiler/operator-properties-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

SimplifiedGraphBuilder::SimplifiedGraphBuilder(
    Graph* graph, CommonOperatorBuilder* common,
    MachineOperatorBuilder* machine, SimplifiedOperatorBuilder* simplified)
    : GraphBuilder(graph),
      effect_(NULL),
      return_(NULL),
      common_(common),
      machine_(machine),
      simplified_(simplified) {}


void SimplifiedGraphBuilder::Begin(int num_parameters) {
  DCHECK(graph()->start() == NULL);
  Node* start = graph()->NewNode(common()->Start(num_parameters));
  graph()->SetStart(start);
  effect_ = start;
}


void SimplifiedGraphBuilder::Return(Node* value) {
  return_ =
      graph()->NewNode(common()->Return(), value, effect_, graph()->start());
  effect_ = NULL;
}


void SimplifiedGraphBuilder::End() {
  Node* end = graph()->NewNode(common()->End(), return_);
  graph()->SetEnd(end);
}


Node* SimplifiedGraphBuilder::MakeNode(const Operator* op,
                                       int value_input_count,
                                       Node** value_inputs, bool incomplete) {
  DCHECK(op->InputCount() == value_input_count);

  DCHECK(!OperatorProperties::HasContextInput(op));
  DCHECK(!OperatorProperties::HasFrameStateInput(op));
  bool has_control = op->ControlInputCount() == 1;
  bool has_effect = op->EffectInputCount() == 1;

  DCHECK(op->ControlInputCount() < 2);
  DCHECK(op->EffectInputCount() < 2);

  Node* result = NULL;
  if (!has_control && !has_effect) {
    result = graph()->NewNode(op, value_input_count, value_inputs, incomplete);
  } else {
    int input_count_with_deps = value_input_count;
    if (has_control) ++input_count_with_deps;
    if (has_effect) ++input_count_with_deps;
    Node** buffer = zone()->NewArray<Node*>(input_count_with_deps);
    memcpy(buffer, value_inputs, kPointerSize * value_input_count);
    Node** current_input = buffer + value_input_count;
    if (has_effect) {
      *current_input++ = effect_;
    }
    if (has_control) {
      *current_input++ = graph()->start();
    }
    result = graph()->NewNode(op, input_count_with_deps, buffer, incomplete);
    if (has_effect) {
      effect_ = result;
    }
    // This graph builder does not support control flow.
    CHECK_EQ(0, op->ControlOutputCount());
  }

  return result;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
