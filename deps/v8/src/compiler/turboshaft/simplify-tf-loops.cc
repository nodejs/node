// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/simplify-tf-loops.h"

#include "src/base/small-vector.h"
#include "src/compiler/machine-graph.h"
#include "src/compiler/node-properties.h"

namespace v8::internal::compiler {

Reduction SimplifyTFLoops::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kLoop) return NoChange();
  if (node->InputCount() <= 2) return NoChange();

  Node* new_loop = mcgraph_->graph()->NewNode(mcgraph_->common()->Loop(2),
                                              node->InputAt(0), node);
  node->RemoveInput(0);
  NodeProperties::ChangeOp(node, mcgraph_->common()->Merge(node->InputCount()));

  base::SmallVector<Edge, 4> control_uses;

  for (Edge edge : node->use_edges()) {
    Node* use = edge.from();
    if (!NodeProperties::IsPhi(use)) {
      control_uses.emplace_back(edge);
      continue;
    }
    Node* dominating_input = use->InputAt(0);
    use->RemoveInput(0);
    NodeProperties::ChangeOp(
        use, use->opcode() == IrOpcode::kPhi
                 ? mcgraph_->common()->Phi(PhiRepresentationOf(use->op()),
                                           use->InputCount() - 1)
                 : mcgraph_->common()->EffectPhi(use->InputCount() - 1));

    Node* new_phi = mcgraph_->graph()->NewNode(
        use->opcode() == IrOpcode::kPhi
            ? mcgraph_->common()->Phi(PhiRepresentationOf(use->op()), 2)
            : mcgraph_->common()->EffectPhi(2),
        dominating_input, use, new_loop);

    ReplaceWithValue(use, new_phi, new_phi, new_phi);
    // Restore the use <- new_phi edge we just broke.
    new_phi->ReplaceInput(1, use);
  }

  for (Edge edge : control_uses) {
    if (edge.from() != new_loop) {
      edge.from()->ReplaceInput(edge.index(), new_loop);
    }
  }

  return NoChange();
}

}  // namespace v8::internal::compiler
