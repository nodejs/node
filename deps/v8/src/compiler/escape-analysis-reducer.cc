// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/escape-analysis-reducer.h"

#include "src/compiler/js-graph.h"
#include "src/counters.h"

namespace v8 {
namespace internal {
namespace compiler {

EscapeAnalysisReducer::EscapeAnalysisReducer(Editor* editor, JSGraph* jsgraph,
                                             EscapeAnalysis* escape_analysis,
                                             Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      escape_analysis_(escape_analysis),
      zone_(zone),
      visited_(static_cast<int>(jsgraph->graph()->NodeCount()), zone) {}


Reduction EscapeAnalysisReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kLoadField:
    case IrOpcode::kLoadElement:
      return ReduceLoad(node);
    case IrOpcode::kStoreField:
    case IrOpcode::kStoreElement:
      return ReduceStore(node);
    case IrOpcode::kAllocate:
      return ReduceAllocate(node);
    case IrOpcode::kFinishRegion:
      return ReduceFinishRegion(node);
    case IrOpcode::kReferenceEqual:
      return ReduceReferenceEqual(node);
    case IrOpcode::kObjectIsSmi:
      return ReduceObjectIsSmi(node);
    default:
      // TODO(sigurds): Change this to GetFrameStateInputCount once
      // it is working. For now we use EffectInputCount > 0 to determine
      // whether a node might have a frame state input.
      if (node->op()->EffectInputCount() > 0) {
        return ReduceFrameStateUses(node);
      }
      break;
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceLoad(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kLoadField ||
         node->opcode() == IrOpcode::kLoadElement);
  if (visited_.Contains(node->id())) return NoChange();
  visited_.Add(node->id());
  if (Node* rep = escape_analysis()->GetReplacement(node)) {
    visited_.Add(node->id());
    counters()->turbo_escape_loads_replaced()->Increment();
    if (FLAG_trace_turbo_escape) {
      PrintF("Replaced #%d (%s) with #%d (%s)\n", node->id(),
             node->op()->mnemonic(), rep->id(), rep->op()->mnemonic());
    }
    ReplaceWithValue(node, rep);
    return Changed(rep);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceStore(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kStoreField ||
         node->opcode() == IrOpcode::kStoreElement);
  if (visited_.Contains(node->id())) return NoChange();
  visited_.Add(node->id());
  if (escape_analysis()->IsVirtual(NodeProperties::GetValueInput(node, 0))) {
    if (FLAG_trace_turbo_escape) {
      PrintF("Removed #%d (%s) from effect chain\n", node->id(),
             node->op()->mnemonic());
    }
    RelaxEffectsAndControls(node);
    return Changed(node);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceAllocate(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocate);
  if (visited_.Contains(node->id())) return NoChange();
  visited_.Add(node->id());
  if (escape_analysis()->IsVirtual(node)) {
    RelaxEffectsAndControls(node);
    counters()->turbo_escape_allocs_replaced()->Increment();
    if (FLAG_trace_turbo_escape) {
      PrintF("Removed allocate #%d from effect chain\n", node->id());
    }
    return Changed(node);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceFinishRegion(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kFinishRegion);
  Node* effect = NodeProperties::GetEffectInput(node, 0);
  if (effect->opcode() == IrOpcode::kBeginRegion) {
    RelaxEffectsAndControls(effect);
    RelaxEffectsAndControls(node);
    if (FLAG_trace_turbo_escape) {
      PrintF("Removed region #%d / #%d from effect chain,", effect->id(),
             node->id());
      PrintF(" %d user(s) of #%d remain(s):", node->UseCount(), node->id());
      for (Edge edge : node->use_edges()) {
        PrintF(" #%d", edge.from()->id());
      }
      PrintF("\n");
    }
    return Changed(node);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceReferenceEqual(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kReferenceEqual);
  Node* left = NodeProperties::GetValueInput(node, 0);
  Node* right = NodeProperties::GetValueInput(node, 1);
  if (escape_analysis()->IsVirtual(left)) {
    if (escape_analysis()->IsVirtual(right) &&
        escape_analysis()->CompareVirtualObjects(left, right)) {
      ReplaceWithValue(node, jsgraph()->TrueConstant());
      if (FLAG_trace_turbo_escape) {
        PrintF("Replaced ref eq #%d with true\n", node->id());
      }
    }
    // Right-hand side is not a virtual object, or a different one.
    ReplaceWithValue(node, jsgraph()->FalseConstant());
    if (FLAG_trace_turbo_escape) {
      PrintF("Replaced ref eq #%d with false\n", node->id());
    }
    return Replace(node);
  } else if (escape_analysis()->IsVirtual(right)) {
    // Left-hand side is not a virtual object.
    ReplaceWithValue(node, jsgraph()->FalseConstant());
    if (FLAG_trace_turbo_escape) {
      PrintF("Replaced ref eq #%d with false\n", node->id());
    }
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceObjectIsSmi(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kObjectIsSmi);
  Node* input = NodeProperties::GetValueInput(node, 0);
  if (escape_analysis()->IsVirtual(input)) {
    ReplaceWithValue(node, jsgraph()->FalseConstant());
    if (FLAG_trace_turbo_escape) {
      PrintF("Replaced ObjectIsSmi #%d with false\n", node->id());
    }
    return Replace(node);
  }
  return NoChange();
}


Reduction EscapeAnalysisReducer::ReduceFrameStateUses(Node* node) {
  if (visited_.Contains(node->id())) return NoChange();
  visited_.Add(node->id());
  DCHECK_GE(node->op()->EffectInputCount(), 1);
  bool changed = false;
  for (int i = 0; i < node->InputCount(); ++i) {
    Node* input = node->InputAt(i);
    if (input->opcode() == IrOpcode::kFrameState) {
      if (Node* ret = ReduceFrameState(input, node, false)) {
        node->ReplaceInput(i, ret);
        changed = true;
      }
    }
  }
  if (changed) {
    return Changed(node);
  }
  return NoChange();
}


// Returns the clone if it duplicated the node, and null otherwise.
Node* EscapeAnalysisReducer::ReduceFrameState(Node* node, Node* effect,
                                              bool multiple_users) {
  DCHECK(node->opcode() == IrOpcode::kFrameState);
  if (FLAG_trace_turbo_escape) {
    PrintF("Reducing FrameState %d\n", node->id());
  }
  Node* clone = nullptr;
  for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
    Node* input = NodeProperties::GetValueInput(node, i);
    Node* ret =
        input->opcode() == IrOpcode::kStateValues
            ? ReduceStateValueInputs(input, effect, node->UseCount() > 1)
            : ReduceStateValueInput(node, i, effect, node->UseCount() > 1);
    if (ret) {
      if (node->UseCount() > 1 || multiple_users) {
        if (FLAG_trace_turbo_escape) {
          PrintF("  Cloning #%d", node->id());
        }
        node = clone = jsgraph()->graph()->CloneNode(node);
        if (FLAG_trace_turbo_escape) {
          PrintF(" to #%d\n", node->id());
        }
        multiple_users = false;  // Don't clone anymore.
      }
      NodeProperties::ReplaceValueInput(node, ret, i);
    }
  }
  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node, 0);
  if (outer_frame_state->opcode() == IrOpcode::kFrameState) {
    if (Node* ret =
            ReduceFrameState(outer_frame_state, effect, node->UseCount() > 1)) {
      if (node->UseCount() > 1 || multiple_users) {
        if (FLAG_trace_turbo_escape) {
          PrintF("  Cloning #%d", node->id());
        }
        node = clone = jsgraph()->graph()->CloneNode(node);
        if (FLAG_trace_turbo_escape) {
          PrintF(" to #%d\n", node->id());
        }
        multiple_users = false;
      }
      NodeProperties::ReplaceFrameStateInput(node, 0, ret);
    }
  }
  return clone;
}


// Returns the clone if it duplicated the node, and null otherwise.
Node* EscapeAnalysisReducer::ReduceStateValueInputs(Node* node, Node* effect,
                                                    bool multiple_users) {
  if (FLAG_trace_turbo_escape) {
    PrintF("Reducing StateValue #%d\n", node->id());
  }
  DCHECK(node->opcode() == IrOpcode::kStateValues);
  DCHECK_NOT_NULL(effect);
  Node* clone = nullptr;
  for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
    Node* input = NodeProperties::GetValueInput(node, i);
    Node* ret = nullptr;
    if (input->opcode() == IrOpcode::kStateValues) {
      ret = ReduceStateValueInputs(input, effect, multiple_users);
    } else {
      ret = ReduceStateValueInput(node, i, effect, multiple_users);
    }
    if (ret) {
      node = ret;
      DCHECK_NULL(clone);
      clone = ret;
      multiple_users = false;
    }
  }
  return clone;
}


// Returns the clone if it duplicated the node, and null otherwise.
Node* EscapeAnalysisReducer::ReduceStateValueInput(Node* node, int node_index,
                                                   Node* effect,
                                                   bool multiple_users) {
  Node* input = NodeProperties::GetValueInput(node, node_index);
  if (FLAG_trace_turbo_escape) {
    PrintF("Reducing State Input #%d (%s)\n", input->id(),
           input->op()->mnemonic());
  }
  Node* clone = nullptr;
  if (input->opcode() == IrOpcode::kFinishRegion ||
      input->opcode() == IrOpcode::kAllocate) {
    if (escape_analysis()->IsVirtual(input)) {
      if (Node* object_state =
              escape_analysis()->GetOrCreateObjectState(effect, input)) {
        if (node->UseCount() > 1 || multiple_users) {
          if (FLAG_trace_turbo_escape) {
            PrintF("Cloning #%d", node->id());
          }
          node = clone = jsgraph()->graph()->CloneNode(node);
          if (FLAG_trace_turbo_escape) {
            PrintF(" to #%d\n", node->id());
          }
        }
        NodeProperties::ReplaceValueInput(node, object_state, node_index);
        if (FLAG_trace_turbo_escape) {
          PrintF("Replaced state #%d input #%d with object state #%d\n",
                 node->id(), input->id(), object_state->id());
        }
      } else {
        if (FLAG_trace_turbo_escape) {
          PrintF("No object state replacement available.\n");
        }
      }
    }
  }
  return clone;
}


Counters* EscapeAnalysisReducer::counters() const {
  return jsgraph_->isolate()->counters();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
