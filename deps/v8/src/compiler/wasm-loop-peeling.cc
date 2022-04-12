// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-loop-peeling.h"

#include "src/base/small-vector.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/loop-peeling.h"

namespace v8 {
namespace internal {
namespace compiler {

void PeelWasmLoop(Node* loop_node, ZoneUnorderedSet<Node*>* loop, Graph* graph,
                  CommonOperatorBuilder* common, Zone* tmp_zone,
                  SourcePositionTable* source_positions,
                  NodeOriginTable* node_origins) {
  DCHECK_EQ(loop_node->opcode(), IrOpcode::kLoop);
  DCHECK_NOT_NULL(loop);
  // No back-jump to the loop header means this is not really a loop.
  if (loop_node->InputCount() < 2) return;

  uint32_t copied_size = static_cast<uint32_t>(loop->size()) * 2;

  NodeVector copied_nodes(tmp_zone);

  NodeCopier copier(graph, copied_size, &copied_nodes, 1);
  source_positions->AddDecorator();
  copier.CopyNodes(graph, tmp_zone, graph->NewNode(common->Dead()),
                   base::make_iterator_range(loop->begin(), loop->end()),
                   source_positions, node_origins);
  source_positions->RemoveDecorator();

  Node* peeled_iteration_header = copier.map(loop_node);

  // The terminator nodes in the copies need to get connected to the graph's end
  // node, except Terminate nodes which will be deleted anyway.
  for (Node* node : copied_nodes) {
    if (IrOpcode::IsGraphTerminator(node->opcode()) &&
        node->opcode() != IrOpcode::kTerminate && node->UseCount() == 0) {
      NodeProperties::MergeControlToEnd(graph, common, node);
    }
  }

  // Step 1: Create merges for loop exits.
  for (Node* node : loop_node->uses()) {
    // We do not need the Terminate node for the peeled iteration.
    if (node->opcode() == IrOpcode::kTerminate) {
      copier.map(node)->Kill();
      continue;
    }
    if (node->opcode() != IrOpcode::kLoopExit) continue;
    DCHECK_EQ(node->InputAt(1), loop_node);
    // Create a merge node for the peeled iteration and main loop. Skip the
    // LoopExit node in the peeled iteration, use its control input instead.
    Node* merge_node =
        graph->NewNode(common->Merge(2), node, copier.map(node)->InputAt(0));
    // Replace all uses of the loop exit with the merge node.
    for (Edge use_edge : node->use_edges()) {
      Node* use = use_edge.from();
      if (loop->count(use) == 1) {
        // Uses within the loop will be LoopExitEffects and LoopExitValues.
        // Those are used by nodes outside the loop. We need to create phis from
        // the main loop and peeled iteration to replace loop exits.
        DCHECK(use->opcode() == IrOpcode::kLoopExitEffect ||
               use->opcode() == IrOpcode::kLoopExitValue);
        const Operator* phi_operator =
            use->opcode() == IrOpcode::kLoopExitEffect
                ? common->EffectPhi(2)
                : common->Phi(LoopExitValueRepresentationOf(use->op()), 2);
        Node* phi = graph->NewNode(phi_operator, use,
                                   copier.map(use)->InputAt(0), merge_node);
        use->ReplaceUses(phi);
        // Fix the input of phi we just broke.
        phi->ReplaceInput(0, use);
        copier.map(use)->Kill();
      } else if (use != merge_node) {
        // For uses outside the loop, simply redirect them to the merge.
        use->ReplaceInput(use_edge.index(), merge_node);
      }
    }
    copier.map(node)->Kill();
  }

  // Step 2: The peeled iteration is not a loop anymore. Any control uses of
  // its loop header should now point to its non-recursive input. Any phi uses
  // should use the value coming from outside the loop.
  for (Edge use_edge : peeled_iteration_header->use_edges()) {
    if (NodeProperties::IsPhi(use_edge.from())) {
      use_edge.from()->ReplaceUses(use_edge.from()->InputAt(0));
    } else {
      use_edge.UpdateTo(loop_node->InputAt(0));
    }
  }

  // We are now left with an unconnected subgraph of the peeled Loop node and
  // its phi uses.

  // Step 3: Rewire the peeled iteration to flow into the main loop.

  // We are reusing the Loop node of the peeled iteration and its phis as the
  // merge and phis which flow from the peeled iteration into the main loop.
  // First, remove the non-recursive input.
  peeled_iteration_header->RemoveInput(0);
  NodeProperties::ChangeOp(
      peeled_iteration_header,
      common->Merge(peeled_iteration_header->InputCount()));

  // Remove the non-recursive input.
  for (Edge use_edge : peeled_iteration_header->use_edges()) {
    DCHECK(NodeProperties::IsPhi(use_edge.from()));
    use_edge.from()->RemoveInput(0);
    const Operator* phi = common->ResizeMergeOrPhi(
        use_edge.from()->op(),
        use_edge.from()->InputCount() - /* control input */ 1);
    NodeProperties::ChangeOp(use_edge.from(), phi);
  }

  // In the main loop, change inputs to the merge and phis above.
  loop_node->ReplaceInput(0, peeled_iteration_header);
  for (Edge use_edge : loop_node->use_edges()) {
    if (NodeProperties::IsPhi(use_edge.from())) {
      use_edge.from()->ReplaceInput(0, copier.map(use_edge.from()));
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
