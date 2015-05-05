// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator-reducer.h"

#include <algorithm>

#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction CommonOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kEffectPhi:
      return ReduceEffectPhi(node);
    case IrOpcode::kPhi:
      return ReducePhi(node);
    case IrOpcode::kSelect:
      return ReduceSelect(node);
    default:
      break;
  }
  return NoChange();
}


Reduction CommonOperatorReducer::ReduceEffectPhi(Node* node) {
  DCHECK_EQ(IrOpcode::kEffectPhi, node->opcode());
  int const input_count = node->InputCount();
  if (input_count > 1) {
    Node* const replacement = node->InputAt(0);
    for (int i = 1; i < input_count - 1; ++i) {
      if (node->InputAt(i) != replacement) return NoChange();
    }
    return Replace(replacement);
  }
  return NoChange();
}


Reduction CommonOperatorReducer::ReducePhi(Node* node) {
  DCHECK_EQ(IrOpcode::kPhi, node->opcode());
  int const input_count = node->InputCount();
  if (input_count == 3) {
    Node* vtrue = NodeProperties::GetValueInput(node, 0);
    Node* vfalse = NodeProperties::GetValueInput(node, 1);
    Node* merge = NodeProperties::GetControlInput(node);
    Node* if_true = NodeProperties::GetControlInput(merge, 0);
    Node* if_false = NodeProperties::GetControlInput(merge, 1);
    if (if_true->opcode() != IrOpcode::kIfTrue) {
      std::swap(if_true, if_false);
      std::swap(vtrue, vfalse);
    }
    if (if_true->opcode() == IrOpcode::kIfTrue &&
        if_false->opcode() == IrOpcode::kIfFalse &&
        if_true->InputAt(0) == if_false->InputAt(0)) {
      Node* branch = if_true->InputAt(0);
      Node* cond = branch->InputAt(0);
      if (cond->opcode() == IrOpcode::kFloat64LessThan) {
        if (cond->InputAt(0) == vtrue && cond->InputAt(1) == vfalse &&
            machine()->HasFloat64Min()) {
          node->set_op(machine()->Float64Min());
          node->ReplaceInput(0, vtrue);
          node->ReplaceInput(1, vfalse);
          node->TrimInputCount(2);
          return Changed(node);
        } else if (cond->InputAt(0) == vfalse && cond->InputAt(1) == vtrue &&
                   machine()->HasFloat64Max()) {
          node->set_op(machine()->Float64Max());
          node->ReplaceInput(0, vtrue);
          node->ReplaceInput(1, vfalse);
          node->TrimInputCount(2);
          return Changed(node);
        }
      }
    }
  }
  if (input_count > 1) {
    Node* const replacement = node->InputAt(0);
    for (int i = 1; i < input_count - 1; ++i) {
      if (node->InputAt(i) != replacement) return NoChange();
    }
    return Replace(replacement);
  }
  return NoChange();
}


Reduction CommonOperatorReducer::ReduceSelect(Node* node) {
  DCHECK_EQ(IrOpcode::kSelect, node->opcode());
  Node* cond = NodeProperties::GetValueInput(node, 0);
  Node* vtrue = NodeProperties::GetValueInput(node, 1);
  Node* vfalse = NodeProperties::GetValueInput(node, 2);
  if (vtrue == vfalse) return Replace(vtrue);
  if (cond->opcode() == IrOpcode::kFloat64LessThan) {
    if (cond->InputAt(0) == vtrue && cond->InputAt(1) == vfalse &&
        machine()->HasFloat64Min()) {
      node->set_op(machine()->Float64Min());
      node->ReplaceInput(0, vtrue);
      node->ReplaceInput(1, vfalse);
      node->TrimInputCount(2);
      return Changed(node);
    } else if (cond->InputAt(0) == vfalse && cond->InputAt(1) == vtrue &&
               machine()->HasFloat64Max()) {
      node->set_op(machine()->Float64Max());
      node->ReplaceInput(0, vtrue);
      node->ReplaceInput(1, vfalse);
      node->TrimInputCount(2);
      return Changed(node);
    }
  }
  return NoChange();
}


CommonOperatorBuilder* CommonOperatorReducer::common() const {
  return jsgraph()->common();
}


Graph* CommonOperatorReducer::graph() const { return jsgraph()->graph(); }


MachineOperatorBuilder* CommonOperatorReducer::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
