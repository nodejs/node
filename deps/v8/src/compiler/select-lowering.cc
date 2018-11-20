// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/select-lowering.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

SelectLowering::SelectLowering(Graph* graph, CommonOperatorBuilder* common)
    : common_(common),
      graph_(graph),
      merges_(Merges::key_compare(), Merges::allocator_type(graph->zone())) {}


SelectLowering::~SelectLowering() {}


Reduction SelectLowering::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kSelect) return NoChange();
  SelectParameters const p = SelectParametersOf(node->op());

  Node* cond = node->InputAt(0);
  Node* vthen = node->InputAt(1);
  Node* velse = node->InputAt(2);
  Node* merge = nullptr;

  // Check if we already have a diamond for this condition.
  auto range = merges_.equal_range(cond);
  for (auto i = range.first;; ++i) {
    if (i == range.second) {
      // Create a new diamond for this condition and remember its merge node.
      Diamond d(graph(), common(), cond, p.hint());
      merges_.insert(std::make_pair(cond, d.merge));
      merge = d.merge;
      break;
    }

    // If the diamond is reachable from the Select, merging them would result in
    // an unschedulable graph, so we cannot reuse the diamond in that case.
    merge = i->second;
    if (!ReachableFrom(merge, node)) {
      break;
    }
  }

  // Create a Phi hanging off the previously determined merge.
  node->set_op(common()->Phi(p.type(), 2));
  node->ReplaceInput(0, vthen);
  node->ReplaceInput(1, velse);
  node->ReplaceInput(2, merge);
  return Changed(node);
}


bool SelectLowering::ReachableFrom(Node* const sink, Node* const source) {
  // TODO(turbofan): This is probably horribly expensive, and it should be moved
  // into node.h or somewhere else?!
  Zone zone;
  std::queue<Node*, NodeDeque> queue((NodeDeque(&zone)));
  BoolVector visited(graph()->NodeCount(), false, &zone);
  queue.push(source);
  visited[source->id()] = true;
  while (!queue.empty()) {
    Node* current = queue.front();
    if (current == sink) return true;
    queue.pop();
    for (auto input : current->inputs()) {
      if (!visited[input->id()]) {
        queue.push(input);
        visited[input->id()] = true;
      }
    }
  }
  return false;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
