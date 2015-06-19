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
    DiamondMatcher matcher(merge);
    if (matcher.Matched()) {
      if (matcher.IfTrue() == merge->InputAt(1)) std::swap(vtrue, vfalse);
      Node* cond = matcher.Branch()->InputAt(0);
      if (cond->opcode() == IrOpcode::kFloat32LessThan) {
        Float32BinopMatcher mcond(cond);
        if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
            vfalse->opcode() == IrOpcode::kFloat32Sub) {
          Float32BinopMatcher mvfalse(vfalse);
          if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
            return Change(node, machine()->Float32Abs(), vtrue);
          }
        }
        if (mcond.left().Equals(vtrue) && mcond.right().Equals(vfalse) &&
            machine()->HasFloat32Min()) {
          return Change(node, machine()->Float32Min(), vtrue, vfalse);
        } else if (mcond.left().Equals(vfalse) && mcond.right().Equals(vtrue) &&
                   machine()->HasFloat32Max()) {
          return Change(node, machine()->Float32Max(), vtrue, vfalse);
        }
      } else if (cond->opcode() == IrOpcode::kFloat64LessThan) {
        Float64BinopMatcher mcond(cond);
        if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
            vfalse->opcode() == IrOpcode::kFloat64Sub) {
          Float64BinopMatcher mvfalse(vfalse);
          if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
            return Change(node, machine()->Float64Abs(), vtrue);
          }
        }
        if (mcond.left().Equals(vtrue) && mcond.right().Equals(vfalse) &&
            machine()->HasFloat64Min()) {
          return Change(node, machine()->Float64Min(), vtrue, vfalse);
        } else if (mcond.left().Equals(vfalse) && mcond.right().Equals(vtrue) &&
                   machine()->HasFloat64Max()) {
          return Change(node, machine()->Float64Max(), vtrue, vfalse);
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
  switch (cond->opcode()) {
    case IrOpcode::kHeapConstant: {
      HeapObjectMatcher<HeapObject> mcond(cond);
      return Replace(mcond.Value().handle()->BooleanValue() ? vtrue : vfalse);
    }
    case IrOpcode::kFloat32LessThan: {
      Float32BinopMatcher mcond(cond);
      if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
          vfalse->opcode() == IrOpcode::kFloat32Sub) {
        Float32BinopMatcher mvfalse(vfalse);
        if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
          return Change(node, machine()->Float32Abs(), vtrue);
        }
      }
      if (mcond.left().Equals(vtrue) && mcond.right().Equals(vfalse) &&
          machine()->HasFloat32Min()) {
        return Change(node, machine()->Float32Min(), vtrue, vfalse);
      } else if (mcond.left().Equals(vfalse) && mcond.right().Equals(vtrue) &&
                 machine()->HasFloat32Max()) {
        return Change(node, machine()->Float32Max(), vtrue, vfalse);
      }
      break;
    }
    case IrOpcode::kFloat64LessThan: {
      Float64BinopMatcher mcond(cond);
      if (mcond.left().Is(0.0) && mcond.right().Equals(vtrue) &&
          vfalse->opcode() == IrOpcode::kFloat64Sub) {
        Float64BinopMatcher mvfalse(vfalse);
        if (mvfalse.left().IsZero() && mvfalse.right().Equals(vtrue)) {
          return Change(node, machine()->Float64Abs(), vtrue);
        }
      }
      if (mcond.left().Equals(vtrue) && mcond.right().Equals(vfalse) &&
          machine()->HasFloat64Min()) {
        return Change(node, machine()->Float64Min(), vtrue, vfalse);
      } else if (mcond.left().Equals(vfalse) && mcond.right().Equals(vtrue) &&
                 machine()->HasFloat64Max()) {
        return Change(node, machine()->Float64Max(), vtrue, vfalse);
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}


Reduction CommonOperatorReducer::Change(Node* node, Operator const* op,
                                        Node* a) {
  node->set_op(op);
  node->ReplaceInput(0, a);
  node->TrimInputCount(1);
  return Changed(node);
}


Reduction CommonOperatorReducer::Change(Node* node, Operator const* op, Node* a,
                                        Node* b) {
  node->set_op(op);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->TrimInputCount(2);
  return Changed(node);
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
