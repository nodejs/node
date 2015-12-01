// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/dead-code-elimination.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

DeadCodeElimination::DeadCodeElimination(Editor* editor, Graph* graph,
                                         CommonOperatorBuilder* common)
    : AdvancedReducer(editor),
      graph_(graph),
      common_(common),
      dead_(graph->NewNode(common->Dead())) {}


Reduction DeadCodeElimination::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kEnd:
      return ReduceEnd(node);
    case IrOpcode::kLoop:
    case IrOpcode::kMerge:
      return ReduceLoopOrMerge(node);
    default:
      return ReduceNode(node);
  }
  UNREACHABLE();
  return NoChange();
}


Reduction DeadCodeElimination::ReduceEnd(Node* node) {
  DCHECK_EQ(IrOpcode::kEnd, node->opcode());
  int const input_count = node->InputCount();
  DCHECK_LE(1, input_count);
  int live_input_count = 0;
  for (int i = 0; i < input_count; ++i) {
    Node* const input = node->InputAt(i);
    // Skip dead inputs.
    if (input->opcode() == IrOpcode::kDead) continue;
    // Compact live inputs.
    if (i != live_input_count) node->ReplaceInput(live_input_count, input);
    ++live_input_count;
  }
  if (live_input_count == 0) {
    return Replace(dead());
  } else if (live_input_count < input_count) {
    node->TrimInputCount(live_input_count);
    NodeProperties::ChangeOp(node, common()->End(live_input_count));
    return Changed(node);
  }
  DCHECK_EQ(input_count, live_input_count);
  return NoChange();
}


Reduction DeadCodeElimination::ReduceLoopOrMerge(Node* node) {
  DCHECK(IrOpcode::IsMergeOpcode(node->opcode()));
  int const input_count = node->InputCount();
  DCHECK_LE(1, input_count);
  // Count the number of live inputs to {node} and compact them on the fly, also
  // compacting the inputs of the associated {Phi} and {EffectPhi} uses at the
  // same time.  We consider {Loop}s dead even if only the first control input
  // is dead.
  int live_input_count = 0;
  if (node->opcode() != IrOpcode::kLoop ||
      node->InputAt(0)->opcode() != IrOpcode::kDead) {
    for (int i = 0; i < input_count; ++i) {
      Node* const input = node->InputAt(i);
      // Skip dead inputs.
      if (input->opcode() == IrOpcode::kDead) continue;
      // Compact live inputs.
      if (live_input_count != i) {
        node->ReplaceInput(live_input_count, input);
        for (Node* const use : node->uses()) {
          if (NodeProperties::IsPhi(use)) {
            DCHECK_EQ(input_count + 1, use->InputCount());
            use->ReplaceInput(live_input_count, use->InputAt(i));
          }
        }
      }
      ++live_input_count;
    }
  }
  if (live_input_count == 0) {
    return Replace(dead());
  } else if (live_input_count == 1) {
    // Due to compaction above, the live input is at offset 0.
    for (Node* const use : node->uses()) {
      if (NodeProperties::IsPhi(use)) {
        Replace(use, use->InputAt(0));
      } else if (use->opcode() == IrOpcode::kTerminate) {
        DCHECK_EQ(IrOpcode::kLoop, node->opcode());
        Replace(use, dead());
      }
    }
    return Replace(node->InputAt(0));
  }
  DCHECK_LE(2, live_input_count);
  DCHECK_LE(live_input_count, input_count);
  // Trim input count for the {Merge} or {Loop} node.
  if (live_input_count < input_count) {
    // Trim input counts for all phi uses and revisit them.
    for (Node* const use : node->uses()) {
      if (NodeProperties::IsPhi(use)) {
        use->ReplaceInput(live_input_count, node);
        TrimMergeOrPhi(use, live_input_count);
        Revisit(use);
      }
    }
    TrimMergeOrPhi(node, live_input_count);
    return Changed(node);
  }
  return NoChange();
}


Reduction DeadCodeElimination::ReduceNode(Node* node) {
  // If {node} has exactly one control input and this is {Dead},
  // replace {node} with {Dead}.
  int const control_input_count = node->op()->ControlInputCount();
  if (control_input_count == 0) return NoChange();
  DCHECK_EQ(1, control_input_count);
  Node* control = NodeProperties::GetControlInput(node);
  if (control->opcode() == IrOpcode::kDead) return Replace(control);
  return NoChange();
}


void DeadCodeElimination::TrimMergeOrPhi(Node* node, int size) {
  const Operator* const op = common()->ResizeMergeOrPhi(node->op(), size);
  node->TrimInputCount(OperatorProperties::GetTotalInputCount(op));
  NodeProperties::ChangeOp(node, op);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
