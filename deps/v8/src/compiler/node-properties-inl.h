// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_PROPERTIES_INL_H_
#define V8_COMPILER_NODE_PROPERTIES_INL_H_

#include "src/v8.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/operator-properties-inl.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

// -----------------------------------------------------------------------------
// Input layout.
// Inputs are always arranged in order as follows:
//     0 [ values, context, effects, control ] node->InputCount()

inline int NodeProperties::FirstValueIndex(Node* node) { return 0; }

inline int NodeProperties::FirstContextIndex(Node* node) {
  return PastValueIndex(node);
}

inline int NodeProperties::FirstFrameStateIndex(Node* node) {
  return PastContextIndex(node);
}

inline int NodeProperties::FirstEffectIndex(Node* node) {
  return PastFrameStateIndex(node);
}

inline int NodeProperties::FirstControlIndex(Node* node) {
  return PastEffectIndex(node);
}


inline int NodeProperties::PastValueIndex(Node* node) {
  return FirstValueIndex(node) + node->op()->ValueInputCount();
}

inline int NodeProperties::PastContextIndex(Node* node) {
  return FirstContextIndex(node) +
         OperatorProperties::GetContextInputCount(node->op());
}

inline int NodeProperties::PastFrameStateIndex(Node* node) {
  return FirstFrameStateIndex(node) +
         OperatorProperties::GetFrameStateInputCount(node->op());
}

inline int NodeProperties::PastEffectIndex(Node* node) {
  return FirstEffectIndex(node) + node->op()->EffectInputCount();
}

inline int NodeProperties::PastControlIndex(Node* node) {
  return FirstControlIndex(node) + node->op()->ControlInputCount();
}


// -----------------------------------------------------------------------------
// Input accessors.

inline Node* NodeProperties::GetValueInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->ValueInputCount());
  return node->InputAt(FirstValueIndex(node) + index);
}

inline Node* NodeProperties::GetContextInput(Node* node) {
  DCHECK(OperatorProperties::HasContextInput(node->op()));
  return node->InputAt(FirstContextIndex(node));
}

inline Node* NodeProperties::GetFrameStateInput(Node* node) {
  DCHECK(OperatorProperties::HasFrameStateInput(node->op()));
  return node->InputAt(FirstFrameStateIndex(node));
}

inline Node* NodeProperties::GetEffectInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->EffectInputCount());
  return node->InputAt(FirstEffectIndex(node) + index);
}

inline Node* NodeProperties::GetControlInput(Node* node, int index) {
  DCHECK(0 <= index && index < node->op()->ControlInputCount());
  return node->InputAt(FirstControlIndex(node) + index);
}

inline int NodeProperties::GetFrameStateIndex(Node* node) {
  DCHECK(OperatorProperties::HasFrameStateInput(node->op()));
  return FirstFrameStateIndex(node);
}

// -----------------------------------------------------------------------------
// Edge kinds.

inline bool NodeProperties::IsInputRange(Node::Edge edge, int first, int num) {
  // TODO(titzer): edge.index() is linear time;
  // edges maybe need to be marked as value/effect/control.
  if (num == 0) return false;
  int index = edge.index();
  return first <= index && index < first + num;
}

inline bool NodeProperties::IsValueEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstValueIndex(node),
                      node->op()->ValueInputCount());
}

inline bool NodeProperties::IsContextEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstContextIndex(node),
                      OperatorProperties::GetContextInputCount(node->op()));
}

inline bool NodeProperties::IsEffectEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstEffectIndex(node),
                      node->op()->EffectInputCount());
}

inline bool NodeProperties::IsControlEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstControlIndex(node),
                      node->op()->ControlInputCount());
}


// -----------------------------------------------------------------------------
// Miscellaneous predicates.

inline bool NodeProperties::IsControl(Node* node) {
  return IrOpcode::IsControlOpcode(node->opcode());
}


// -----------------------------------------------------------------------------
// Miscellaneous mutators.

inline void NodeProperties::ReplaceControlInput(Node* node, Node* control) {
  node->ReplaceInput(FirstControlIndex(node), control);
}

inline void NodeProperties::ReplaceEffectInput(Node* node, Node* effect,
                                               int index) {
  DCHECK(index < node->op()->EffectInputCount());
  return node->ReplaceInput(FirstEffectIndex(node) + index, effect);
}

inline void NodeProperties::ReplaceFrameStateInput(Node* node,
                                                   Node* frame_state) {
  DCHECK(OperatorProperties::HasFrameStateInput(node->op()));
  node->ReplaceInput(FirstFrameStateIndex(node), frame_state);
}

inline void NodeProperties::RemoveNonValueInputs(Node* node) {
  node->TrimInputCount(node->op()->ValueInputCount());
}


// Replace value uses of {node} with {value} and effect uses of {node} with
// {effect}. If {effect == NULL}, then use the effect input to {node}.
inline void NodeProperties::ReplaceWithValue(Node* node, Node* value,
                                             Node* effect) {
  DCHECK(node->op()->ControlOutputCount() == 0);
  if (effect == NULL && node->op()->EffectInputCount() > 0) {
    effect = NodeProperties::GetEffectInput(node);
  }

  // Requires distinguishing between value and effect edges.
  UseIter iter = node->uses().begin();
  while (iter != node->uses().end()) {
    if (NodeProperties::IsEffectEdge(iter.edge())) {
      DCHECK_NE(NULL, effect);
      iter = iter.UpdateToAndIncrement(effect);
    } else {
      iter = iter.UpdateToAndIncrement(value);
    }
  }
}


// -----------------------------------------------------------------------------
// Type Bounds.

inline bool NodeProperties::IsTyped(Node* node) {
  Bounds bounds = node->bounds();
  DCHECK((bounds.lower == NULL) == (bounds.upper == NULL));
  return bounds.upper != NULL;
}

inline Bounds NodeProperties::GetBounds(Node* node) {
  DCHECK(IsTyped(node));
  return node->bounds();
}

inline void NodeProperties::SetBounds(Node* node, Bounds b) {
  DCHECK(b.lower != NULL && b.upper != NULL);
  node->set_bounds(b);
}

inline bool NodeProperties::AllValueInputsAreTyped(Node* node) {
  int input_count = node->op()->ValueInputCount();
  for (int i = 0; i < input_count; ++i) {
    if (!IsTyped(GetValueInput(node, i))) return false;
  }
  return true;
}


}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_NODE_PROPERTIES_INL_H_
