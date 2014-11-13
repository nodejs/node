// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/select-lowering.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"

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

  Node* const cond = node->InputAt(0);

  // Check if we already have a diamond for this condition.
  auto i = merges_.find(cond);
  if (i == merges_.end()) {
    // Create a new diamond for this condition and remember its merge node.
    Diamond d(graph(), common(), cond, p.hint());
    i = merges_.insert(std::make_pair(cond, d.merge)).first;
  }

  DCHECK_EQ(cond, i->first);

  // Create a Phi hanging off the previously determined merge.
  node->set_op(common()->Phi(p.type(), 2));
  node->ReplaceInput(0, node->InputAt(1));
  node->ReplaceInput(1, node->InputAt(2));
  node->ReplaceInput(2, i->second);
  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
