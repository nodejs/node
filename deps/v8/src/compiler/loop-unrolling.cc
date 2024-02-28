// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/loop-unrolling.h"

#include "src/base/small-vector.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/loop-peeling.h"

namespace v8 {
namespace internal {
namespace compiler {

void UnrollLoop(Node* loop_node, ZoneUnorderedSet<Node*>* loop, uint32_t depth,
                Graph* graph, CommonOperatorBuilder* common, Zone* tmp_zone,
                SourcePositionTable* source_positions,
                NodeOriginTable* node_origins) {
  DCHECK_EQ(loop_node->opcode(), IrOpcode::kLoop);
  DCHECK_NOT_NULL(loop);
  // No back-jump to the loop header means this is not really a loop.
  if (loop_node->InputCount() < 2) return;

  uint32_t unrolling_count =
      unrolling_count_heuristic(static_cast<uint32_t>(loop->size()), depth);
  if (unrolling_count == 0) return;

  uint32_t iteration_count = unrolling_count + 1;

  uint32_t copied_size = static_cast<uint32_t>(loop->size()) * iteration_count;

  NodeVector copies(tmp_zone);

  NodeCopier copier(graph, copied_size, &copies, unrolling_count);
  source_positions->AddDecorator();
  copier.CopyNodes(graph, tmp_zone, graph->NewNode(common->Dead()),
                   base::make_iterator_range(loop->begin(), loop->end()),
                   source_positions, node_origins);
  source_positions->RemoveDecorator();

  // The terminator nodes in the copies need to get connected to the graph's end
  // node, except Terminate nodes which will be deleted anyway.
  for (Node* node : copies) {
    if (IrOpcode::IsGraphTerminator(node->opcode()) &&
        node->opcode() != IrOpcode::kTerminate && node->UseCount() == 0) {
      NodeProperties::MergeControlToEnd(graph, common, node);
    }
  }

#define COPY(node, n) copier.map(node, n)
#define FOREACH_COPY_INDEX(i) for (uint32_t i = 0; i < unrolling_count; i++)

  for (Node* node : loop_node->uses()) {
    switch (node->opcode()) {
      case IrOpcode::kBranch: {
        /*** Step 1: Remove stack checks from all but the first iteration of the
             loop. ***/
        Node* stack_check = node->InputAt(0);
        if (stack_check->opcode() != IrOpcode::kStackPointerGreaterThan) {
          break;
        }
        // Replace value uses of the stack check with {true}, and remove the
        // stack check from the effect chain.
        FOREACH_COPY_INDEX(i) {
          for (Edge use_edge : COPY(stack_check, i)->use_edges()) {
            if (NodeProperties::IsValueEdge(use_edge)) {
              use_edge.UpdateTo(graph->NewNode(common->Int32Constant(1)));
            } else if (NodeProperties::IsEffectEdge(use_edge)) {
              use_edge.UpdateTo(
                  NodeProperties::GetEffectInput(COPY(stack_check, i)));
            } else {
              UNREACHABLE();
            }
          }
        }
        break;
      }

      case IrOpcode::kLoopExit: {
        /*** Step 2: Create merges for loop exits. ***/
        if (node->InputAt(1) == loop_node) {
          // Create a merge node from all iteration exits.
          Node** merge_inputs = tmp_zone->AllocateArray<Node*>(iteration_count);
          merge_inputs[0] = node;
          for (uint32_t i = 1; i < iteration_count; i++) {
            merge_inputs[i] = COPY(node, i - 1);
          }
          Node* merge_node = graph->NewNode(common->Merge(iteration_count),
                                            iteration_count, merge_inputs);
          // Replace all uses of the loop exit with the merge node.
          for (Edge use_edge : node->use_edges()) {
            Node* use = use_edge.from();
            if (loop->count(use) == 1) {
              // Uses within the loop will be LoopExitEffects and
              // LoopExitValues. We need to create a phi from all loop
              // iterations. Its merge will be the merge node for LoopExits.
              const Operator* phi_operator;
              if (use->opcode() == IrOpcode::kLoopExitEffect) {
                phi_operator = common->EffectPhi(iteration_count);
              } else {
                DCHECK(use->opcode() == IrOpcode::kLoopExitValue);
                phi_operator = common->Phi(
                    LoopExitValueRepresentationOf(use->op()), iteration_count);
              }
              Node** phi_inputs =
                  tmp_zone->AllocateArray<Node*>(iteration_count + 1);
              phi_inputs[0] = use;
              for (uint32_t i = 1; i < iteration_count; i++) {
                phi_inputs[i] = COPY(use, i - 1);
              }
              phi_inputs[iteration_count] = merge_node;
              Node* phi =
                  graph->NewNode(phi_operator, iteration_count + 1, phi_inputs);
              use->ReplaceUses(phi);
              // Repair phi which we just broke.
              phi->ReplaceInput(0, use);
            } else if (use != merge_node) {
              // For uses outside the loop, simply redirect them to the merge.
              use->ReplaceInput(use_edge.index(), merge_node);
            }
          }
        }
        break;
      }

      case IrOpcode::kTerminate: {
        // We only need to keep the Terminate node for the loop header of the
        // first iteration.
        FOREACH_COPY_INDEX(i) { COPY(node, i)->Kill(); }
        break;
      }

      default:
        break;
    }
  }

  /*** Step 3: Rewire the iterations of the loop. Each iteration should flow
       into the next one, and the last should flow into the first. ***/

  // 3a) Rewire control.

  // We start at index=1 assuming that index=0 is the (non-recursive) loop
  // entry.
  for (int input_index = 1; input_index < loop_node->InputCount();
       input_index++) {
    Node* last_iteration_input =
        COPY(loop_node, unrolling_count - 1)->InputAt(input_index);
    for (uint32_t copy_index = unrolling_count - 1; copy_index > 0;
         copy_index--) {
      COPY(loop_node, copy_index)
          ->ReplaceInput(input_index,
                         COPY(loop_node, copy_index - 1)->InputAt(input_index));
    }
    COPY(loop_node, 0)
        ->ReplaceInput(input_index, loop_node->InputAt(input_index));
    loop_node->ReplaceInput(input_index, last_iteration_input);
  }
  // The loop of each following iteration will become a merge. We need to remove
  // its non-recursive input.
  FOREACH_COPY_INDEX(i) {
    COPY(loop_node, i)->RemoveInput(0);
    NodeProperties::ChangeOp(COPY(loop_node, i),
                             common->Merge(loop_node->InputCount() - 1));
  }

  // 3b) Rewire phis and loop exits.
  for (Node* use : loop_node->uses()) {
    if (NodeProperties::IsPhi(use)) {
      int count = use->opcode() == IrOpcode::kPhi
                      ? use->op()->ValueInputCount()
                      : use->op()->EffectInputCount();
      // Phis depending on the loop header should take their input from the
      // previous iteration instead.
      for (int input_index = 1; input_index < count; input_index++) {
        Node* last_iteration_input =
            COPY(use, unrolling_count - 1)->InputAt(input_index);
        for (uint32_t copy_index = unrolling_count - 1; copy_index > 0;
             copy_index--) {
          COPY(use, copy_index)
              ->ReplaceInput(input_index,
                             COPY(use, copy_index - 1)->InputAt(input_index));
        }
        COPY(use, 0)->ReplaceInput(input_index, use->InputAt(input_index));
        use->ReplaceInput(input_index, last_iteration_input);
      }

      // Phis in each following iteration should not depend on the
      // (non-recursive) entry to the loop. Remove their first input.
      FOREACH_COPY_INDEX(i) {
        COPY(use, i)->RemoveInput(0);
        NodeProperties::ChangeOp(
            COPY(use, i), common->ResizeMergeOrPhi(use->op(), count - 1));
      }
    }

    // Loop exits should point to the loop header.
    if (use->opcode() == IrOpcode::kLoopExit) {
      FOREACH_COPY_INDEX(i) { COPY(use, i)->ReplaceInput(1, loop_node); }
    }
  }
}

#undef COPY
#undef FOREACH_COPY_INDEX

}  // namespace compiler
}  // namespace internal
}  // namespace v8
