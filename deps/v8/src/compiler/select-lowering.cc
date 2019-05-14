// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/select-lowering.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

SelectLowering::SelectLowering(Graph* graph, CommonOperatorBuilder* common)
    : common_(common), graph_(graph) {}

SelectLowering::~SelectLowering() = default;


Reduction SelectLowering::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kSelect) return NoChange();
  SelectParameters const p = SelectParametersOf(node->op());

  Node* cond = node->InputAt(0);
  Node* vthen = node->InputAt(1);
  Node* velse = node->InputAt(2);

  // Create a diamond and a phi.
  Diamond d(graph(), common(), cond, p.hint());
  node->ReplaceInput(0, vthen);
  node->ReplaceInput(1, velse);
  node->ReplaceInput(2, d.merge);
  NodeProperties::ChangeOp(node, common()->Phi(p.representation(), 2));
  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
