// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-escape-analysis.h"

#include "src/compiler/js-graph.h"
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
  std::vector<Edge> use_edges;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsValueEdge(edge)) {
      if (edge.index() != 0 ||
          edge.from()->opcode() != IrOpcode::kStoreToObject) {
        return NoChange();
      }
    }
    use_edges.push_back(edge);
  }

  // Remove all stores from the effect chain.
  for (Edge edge : use_edges) {
    if (NodeProperties::IsValueEdge(edge)) {
      Node* use = edge.from();
      DCHECK_EQ(edge.index(), 0);
      DCHECK_EQ(use->opcode(), IrOpcode::kStoreToObject);
      ReplaceWithValue(use, mcgraph_->Dead(),
                       NodeProperties::GetEffectInput(use), mcgraph_->Dead());
    }
  }

  // Remove the allocation from the effect and control chains.
  ReplaceWithValue(node, mcgraph_->Dead(), NodeProperties::GetEffectInput(node),
                   NodeProperties::GetControlInput(node));

  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
