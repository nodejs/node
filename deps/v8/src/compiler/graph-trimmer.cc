// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-trimmer.h"

#include "src/compiler/graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

GraphTrimmer::GraphTrimmer(Zone* zone, Graph* graph)
    : graph_(graph), is_live_(graph, 2), live_(zone) {
  live_.reserve(graph->NodeCount());
}


GraphTrimmer::~GraphTrimmer() {}

namespace {
bool IsSideEffectFree(const Operator* op) {
  switch (op->opcode()) {
    case IrOpcode::kLoad:
    case IrOpcode::kLoadElement:
    case IrOpcode::kLoadField:
    case IrOpcode::kTypeGuard:
      return true;
    default:
      return false;
  }
}
}  // namespace

void GraphTrimmer::TrimGraph() {
  // Mark end node as live.
  MarkAsLive(graph()->end());
  // Compute transitive closure of live nodes.
  for (size_t i = 0; i < live_.size(); ++i) {
    Node* const live = live_[i];
    for (Edge edge : live->input_edges()) {
      Node* input = edge.to();
      if (NodeProperties::IsEffectEdge(edge)) {
        while (IsSideEffectFree(input->op())) {
          // Side-effect free effect chain nodes can be removed from the effect
          // chain when only their effect output is used. So we ignore the
          // effect chain use when determining liveness.
          DCHECK_EQ(1, input->op()->EffectInputCount());
          input = NodeProperties::GetEffectInput(input);
        }
      }
      MarkAsLive(input);
    }
  }

  // Remove unused side-effect-free nodes from the effect chain.
  // These are exactly the nodes we skipped before and that don't have value
  // uses. This has to be done before we remove dead->live edges because we need
  // to read the effect predecessor from the dead effect chain node.
  for (Node* const live : live_) {
    DCHECK(IsLive(live));
    for (Edge edge : live->input_edges()) {
      Node* input = edge.to();
      if (NodeProperties::IsEffectEdge(edge)) {
        Node* new_input = input;
        while (!IsLive(new_input)) {
          DCHECK(IsSideEffectFree(new_input->op()));
          if (FLAG_trace_turbo_trimming) {
            OFStream os(stdout);
            os << "DeadEffectChainNode: " << *new_input << std::endl;
          }
          new_input = NodeProperties::GetEffectInput(new_input);
        }
        if (new_input != input) edge.UpdateTo(new_input);
      }
    }
  }

  // Remove dead->live edges.
  for (Node* const live : live_) {
    DCHECK(IsLive(live));
    for (Edge edge : live->use_edges()) {
      Node* const user = edge.from();
      if (!IsLive(user)) {
        if (FLAG_trace_turbo_trimming) {
          OFStream os(stdout);
          os << "DeadLink: " << *user << "(" << edge.index() << ") -> " << *live
             << std::endl;
        }
        edge.UpdateTo(nullptr);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
