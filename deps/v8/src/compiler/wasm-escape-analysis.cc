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
  // TODO(manoskouk): Account for phis that still have uses.

  // Collect all value edges of {node} in this vector.
  std::vector<Edge> value_edges;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsValueEdge(edge)) {
      if ((edge.from()->opcode() == IrOpcode::kPhi &&
           edge.from()->use_edges().empty()) ||
          (edge.index() == 0 &&
           (edge.from()->opcode() == IrOpcode::kStoreToObject ||
            edge.from()->opcode() == IrOpcode::kInitializeImmutableInObject))) {
        // StoreToObject, InitializeImmutableInObject and phis without uses can
        // be replaced and do not require the allocation.
        value_edges.push_back(edge);
      } else {
        // Allocation not reducible.
        return NoChange();
      }
    }
  }

  // Remove all discovered stores from the effect chain.
  for (Edge edge : value_edges) {
    DCHECK(NodeProperties::IsValueEdge(edge));
    Node* use = edge.from();

    if (use->opcode() == IrOpcode::kPhi) {
      DCHECK(use->use_edges().empty());
      // Useless phi. Kill it.
      use->Kill();

    } else {
      DCHECK_EQ(edge.index(), 0);
      DCHECK(!use->IsDead());
      DCHECK(use->opcode() == IrOpcode::kStoreToObject ||
             use->opcode() == IrOpcode::kInitializeImmutableInObject);
      // The value stored by this StoreToObject node might be another allocation
      // which has no more uses. Therefore we have to revisit it. Note that this
      // will not happen automatically: ReplaceWithValue does not trigger
      // revisits of former inputs of the replaced node.
      Node* stored_value = NodeProperties::GetValueInput(use, 2);
      Revisit(stored_value);
      ReplaceWithValue(use, mcgraph_->Dead(),
                       NodeProperties::GetEffectInput(use), mcgraph_->Dead());
      use->Kill();
    }
  }

  // Remove the allocation from the effect and control chains.
  ReplaceWithValue(node, mcgraph_->Dead(), NodeProperties::GetEffectInput(node),
                   NodeProperties::GetControlInput(node));

  return Changed(node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
