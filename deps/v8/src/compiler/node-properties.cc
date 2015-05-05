// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
int NodeProperties::PastValueIndex(Node* node) {
  return FirstValueIndex(node) + node->op()->ValueInputCount();
}


// static
int NodeProperties::PastContextIndex(Node* node) {
  return FirstContextIndex(node) +
         OperatorProperties::GetContextInputCount(node->op());
}


// static
int NodeProperties::PastFrameStateIndex(Node* node) {
  return FirstFrameStateIndex(node) +
         OperatorProperties::GetFrameStateInputCount(node->op());
}


// static
int NodeProperties::PastEffectIndex(Node* node) {
  return FirstEffectIndex(node) + node->op()->EffectInputCount();
}


// static
int NodeProperties::PastControlIndex(Node* node) {
  return FirstControlIndex(node) + node->op()->ControlInputCount();
}


// static
Node* NodeProperties::GetValueInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->ValueInputCount());
  return node->InputAt(FirstValueIndex(node) + index);
}


// static
Node* NodeProperties::GetContextInput(Node* node) {
  DCHECK(OperatorProperties::HasContextInput(node->op()));
  return node->InputAt(FirstContextIndex(node));
}


// static
Node* NodeProperties::GetFrameStateInput(Node* node, int index) {
  DCHECK_LT(index, OperatorProperties::GetFrameStateInputCount(node->op()));
  return node->InputAt(FirstFrameStateIndex(node) + index);
}


// static
Node* NodeProperties::GetEffectInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->EffectInputCount());
  return node->InputAt(FirstEffectIndex(node) + index);
}


// static
Node* NodeProperties::GetControlInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->ControlInputCount());
  return node->InputAt(FirstControlIndex(node) + index);
}


// static
bool NodeProperties::IsValueEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstValueIndex(node),
                      node->op()->ValueInputCount());
}


// static
bool NodeProperties::IsContextEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstContextIndex(node),
                      OperatorProperties::GetContextInputCount(node->op()));
}


// static
bool NodeProperties::IsFrameStateEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstFrameStateIndex(node),
                      OperatorProperties::GetFrameStateInputCount(node->op()));
}


// static
bool NodeProperties::IsEffectEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstEffectIndex(node),
                      node->op()->EffectInputCount());
}


// static
bool NodeProperties::IsControlEdge(Edge edge) {
  Node* const node = edge.from();
  return IsInputRange(edge, FirstControlIndex(node),
                      node->op()->ControlInputCount());
}


// static
void NodeProperties::ReplaceContextInput(Node* node, Node* context) {
  node->ReplaceInput(FirstContextIndex(node), context);
}


// static
void NodeProperties::ReplaceControlInput(Node* node, Node* control) {
  node->ReplaceInput(FirstControlIndex(node), control);
}


// static
void NodeProperties::ReplaceEffectInput(Node* node, Node* effect, int index) {
  DCHECK(index < node->op()->EffectInputCount());
  return node->ReplaceInput(FirstEffectIndex(node) + index, effect);
}


// static
void NodeProperties::ReplaceFrameStateInput(Node* node, int index,
                                            Node* frame_state) {
  DCHECK_LT(index, OperatorProperties::GetFrameStateInputCount(node->op()));
  node->ReplaceInput(FirstFrameStateIndex(node) + index, frame_state);
}


// static
void NodeProperties::RemoveNonValueInputs(Node* node) {
  node->TrimInputCount(node->op()->ValueInputCount());
}


void NodeProperties::MergeControlToEnd(Graph* graph,
                                       CommonOperatorBuilder* common,
                                       Node* node) {
  // Connect the node to the merge exiting the graph.
  Node* end_pred = NodeProperties::GetControlInput(graph->end());
  if (end_pred->opcode() == IrOpcode::kMerge) {
    int inputs = end_pred->op()->ControlInputCount() + 1;
    end_pred->AppendInput(graph->zone(), node);
    end_pred->set_op(common->Merge(inputs));
  } else {
    Node* merge = graph->NewNode(common->Merge(2), end_pred, node);
    NodeProperties::ReplaceControlInput(graph->end(), merge);
  }
}


// static
void NodeProperties::ReplaceWithValue(Node* node, Node* value, Node* effect,
                                      Node* control) {
  if (!effect && node->op()->EffectInputCount() > 0) {
    effect = NodeProperties::GetEffectInput(node);
  }
  if (control == nullptr && node->op()->ControlInputCount() > 0) {
    control = NodeProperties::GetControlInput(node);
  }

  // Requires distinguishing between value, effect and control edges.
  for (Edge edge : node->use_edges()) {
    if (IsControlEdge(edge)) {
      DCHECK_EQ(IrOpcode::kIfSuccess, edge.from()->opcode());
      DCHECK_NOT_NULL(control);
      edge.from()->ReplaceUses(control);
      edge.UpdateTo(NULL);
    } else if (IsEffectEdge(edge)) {
      DCHECK_NOT_NULL(effect);
      edge.UpdateTo(effect);
    } else {
      edge.UpdateTo(value);
    }
  }
}


// static
Node* NodeProperties::FindProjection(Node* node, size_t projection_index) {
  for (auto use : node->uses()) {
    if (use->opcode() == IrOpcode::kProjection &&
        ProjectionIndexOf(use->op()) == projection_index) {
      return use;
    }
  }
  return nullptr;
}


// static
void NodeProperties::CollectControlProjections(Node* node, Node** projections,
                                               size_t projection_count) {
#ifdef DEBUG
  DCHECK_LE(static_cast<int>(projection_count), node->UseCount());
  std::memset(projections, 0, sizeof(*projections) * projection_count);
#endif
  size_t if_value_index = 0;
  for (Node* const use : node->uses()) {
    size_t index;
    switch (use->opcode()) {
      case IrOpcode::kIfTrue:
        DCHECK_EQ(IrOpcode::kBranch, node->opcode());
        index = 0;
        break;
      case IrOpcode::kIfFalse:
        DCHECK_EQ(IrOpcode::kBranch, node->opcode());
        index = 1;
        break;
      case IrOpcode::kIfSuccess:
        DCHECK_EQ(IrOpcode::kCall, node->opcode());
        index = 0;
        break;
      case IrOpcode::kIfException:
        DCHECK_EQ(IrOpcode::kCall, node->opcode());
        index = 1;
        break;
      case IrOpcode::kIfValue:
        DCHECK_EQ(IrOpcode::kSwitch, node->opcode());
        index = if_value_index++;
        break;
      case IrOpcode::kIfDefault:
        DCHECK_EQ(IrOpcode::kSwitch, node->opcode());
        index = projection_count - 1;
        break;
      default:
        continue;
    }
    DCHECK_LT(if_value_index, projection_count);
    DCHECK_LT(index, projection_count);
    DCHECK_NULL(projections[index]);
    projections[index] = use;
  }
#ifdef DEBUG
  for (size_t index = 0; index < projection_count; ++index) {
    DCHECK_NOT_NULL(projections[index]);
  }
#endif
}


// static
bool NodeProperties::AllValueInputsAreTyped(Node* node) {
  int input_count = node->op()->ValueInputCount();
  for (int index = 0; index < input_count; ++index) {
    if (!IsTyped(GetValueInput(node, index))) return false;
  }
  return true;
}


// static
bool NodeProperties::IsInputRange(Edge edge, int first, int num) {
  if (num == 0) return false;
  int const index = edge.index();
  return first <= index && index < first + num;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
