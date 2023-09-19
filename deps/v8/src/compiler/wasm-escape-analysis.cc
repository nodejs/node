// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-escape-analysis.h"

#include "src/compiler/machine-graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction WasmEscapeAnalysis::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kAllocateRaw:
      return ReduceAllocateRaw(node);
    default:
      return NoChange();
  }
}

Reduction WasmEscapeAnalysis::ReduceAllocateRaw(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocateRaw);
  // TODO(manoskouk): Account for phis.

  // Collect all value edges of {node} in this vector.
  std::vector<Edge> value_edges;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsValueEdge(edge)) {
      if (edge.index() != 0 ||
          (edge.from()->opcode() != IrOpcode::kStoreToObject &&
           edge.from()->opcode() != IrOpcode::kInitializeImmutableInObject)) {
        return NoChange();
      }
      value_edges.push_back(edge);
    }
  }

  // Remove all discovered stores from the effect chain.
  for (Edge edge : value_edges) {
    DCHECK(NodeProperties::IsValueEdge(edge));
    DCHECK_EQ(edge.index(), 0);
    Node* use = edge.from();
    DCHECK(!use->IsDead());
    DCHECK(use->opcode() == IrOpcode::kStoreToObject ||
           use->opcode() == IrOpcode::kInitializeImmutableInObject);
    // The value stored by this StoreToObject node might be another allocation
    // which has no more uses. Therefore we have to revisit it. Note that this
    // will not happen automatically: ReplaceWithValue does not trigger revisits
    // of former inputs of the replaced node.
    Node* stored_value = NodeProperties::GetValueInput(use, 2);
    Revisit(stored_value);
    ReplaceWithValue(use, mcgraph_->Dead(), NodeProperties::GetEffectInput(use),
                     mcgraph_->Dead());
    use->Kill();
  }

  // Remove the allocation from the effect and control chains.
  ReplaceWithValue(node, mcgraph_->Dead(), NodeProperties::GetEffectInput(node),
                   NodeProperties::GetControlInput(node));

  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
