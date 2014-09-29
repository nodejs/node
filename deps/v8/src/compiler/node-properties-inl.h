// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_PROPERTIES_INL_H_
#define V8_COMPILER_NODE_PROPERTIES_INL_H_

#include "src/v8.h"

#include "src/compiler/common-operator.h"
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

inline int NodeProperties::FirstEffectIndex(Node* node) {
  return PastContextIndex(node);
}

inline int NodeProperties::FirstControlIndex(Node* node) {
  return PastEffectIndex(node);
}


inline int NodeProperties::PastValueIndex(Node* node) {
  return FirstValueIndex(node) +
         OperatorProperties::GetValueInputCount(node->op());
}

inline int NodeProperties::PastContextIndex(Node* node) {
  return FirstContextIndex(node) +
         OperatorProperties::GetContextInputCount(node->op());
}

inline int NodeProperties::PastEffectIndex(Node* node) {
  return FirstEffectIndex(node) +
         OperatorProperties::GetEffectInputCount(node->op());
}

inline int NodeProperties::PastControlIndex(Node* node) {
  return FirstControlIndex(node) +
         OperatorProperties::GetControlInputCount(node->op());
}


// -----------------------------------------------------------------------------
// Input accessors.

inline Node* NodeProperties::GetValueInput(Node* node, int index) {
  DCHECK(0 <= index &&
         index < OperatorProperties::GetValueInputCount(node->op()));
  return node->InputAt(FirstValueIndex(node) + index);
}

inline Node* NodeProperties::GetContextInput(Node* node) {
  DCHECK(OperatorProperties::HasContextInput(node->op()));
  return node->InputAt(FirstContextIndex(node));
}

inline Node* NodeProperties::GetEffectInput(Node* node, int index) {
  DCHECK(0 <= index &&
         index < OperatorProperties::GetEffectInputCount(node->op()));
  return node->InputAt(FirstEffectIndex(node) + index);
}

inline Node* NodeProperties::GetControlInput(Node* node, int index) {
  DCHECK(0 <= index &&
         index < OperatorProperties::GetControlInputCount(node->op()));
  return node->InputAt(FirstControlIndex(node) + index);
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
                      OperatorProperties::GetValueInputCount(node->op()));
}

inline bool NodeProperties::IsContextEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstContextIndex(node),
                      OperatorProperties::GetContextInputCount(node->op()));
}

inline bool NodeProperties::IsEffectEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstEffectIndex(node),
                      OperatorProperties::GetEffectInputCount(node->op()));
}

inline bool NodeProperties::IsControlEdge(Node::Edge edge) {
  Node* node = edge.from();
  return IsInputRange(edge, FirstControlIndex(node),
                      OperatorProperties::GetControlInputCount(node->op()));
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
  DCHECK(index < OperatorProperties::GetEffectInputCount(node->op()));
  return node->ReplaceInput(FirstEffectIndex(node) + index, effect);
}

inline void NodeProperties::RemoveNonValueInputs(Node* node) {
  node->TrimInputCount(OperatorProperties::GetValueInputCount(node->op()));
}


// -----------------------------------------------------------------------------
// Type Bounds.

inline Bounds NodeProperties::GetBounds(Node* node) { return node->bounds(); }

inline void NodeProperties::SetBounds(Node* node, Bounds b) {
  node->set_bounds(b);
}


}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_NODE_PROPERTIES_INL_H_
