// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-operator-reducer.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/operator-properties.h"
#include "src/conversions-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

SimplifiedOperatorReducer::SimplifiedOperatorReducer(JSGraph* jsgraph)
    : jsgraph_(jsgraph) {}


SimplifiedOperatorReducer::~SimplifiedOperatorReducer() {}


Reduction SimplifiedOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kBooleanNot: {
      HeapObjectMatcher m(node->InputAt(0));
      if (m.HasValue()) {
        return Replace(jsgraph()->BooleanConstant(!m.Value()->BooleanValue()));
      }
      if (m.IsBooleanNot()) return Replace(m.InputAt(0));
      break;
    }
    case IrOpcode::kChangeBitToBool: {
      Int32Matcher m(node->InputAt(0));
      if (m.Is(0)) return Replace(jsgraph()->FalseConstant());
      if (m.Is(1)) return Replace(jsgraph()->TrueConstant());
      if (m.IsChangeBoolToBit()) return Replace(m.InputAt(0));
      break;
    }
    case IrOpcode::kChangeBoolToBit: {
      HeapObjectMatcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(m.Value()->BooleanValue());
      if (m.IsChangeBitToBool()) return Replace(m.InputAt(0));
      break;
    }
    case IrOpcode::kChangeFloat64ToTagged: {
      Float64Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceNumber(m.Value());
      break;
    }
    case IrOpcode::kChangeInt32ToTagged: {
      Int32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceNumber(m.Value());
      break;
    }
    case IrOpcode::kChangeTaggedToFloat64: {
      NumberMatcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceFloat64(m.Value());
      if (m.IsChangeFloat64ToTagged()) return Replace(m.node()->InputAt(0));
      if (m.IsChangeInt32ToTagged()) {
        return Change(node, machine()->ChangeInt32ToFloat64(), m.InputAt(0));
      }
      if (m.IsChangeUint32ToTagged()) {
        return Change(node, machine()->ChangeUint32ToFloat64(), m.InputAt(0));
      }
      break;
    }
    case IrOpcode::kChangeTaggedToInt32: {
      NumberMatcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(DoubleToInt32(m.Value()));
      if (m.IsChangeFloat64ToTagged()) {
        return Change(node, machine()->ChangeFloat64ToInt32(), m.InputAt(0));
      }
      if (m.IsChangeInt32ToTagged()) return Replace(m.InputAt(0));
      break;
    }
    case IrOpcode::kChangeTaggedToUint32: {
      NumberMatcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceUint32(DoubleToUint32(m.Value()));
      if (m.IsChangeFloat64ToTagged()) {
        return Change(node, machine()->ChangeFloat64ToUint32(), m.InputAt(0));
      }
      if (m.IsChangeUint32ToTagged()) return Replace(m.InputAt(0));
      break;
    }
    case IrOpcode::kChangeUint32ToTagged: {
      Uint32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceNumber(FastUI2D(m.Value()));
      break;
    }
    case IrOpcode::kReferenceEqual:
      return ReduceReferenceEqual(node);
    default:
      break;
  }
  return NoChange();
}


Reduction SimplifiedOperatorReducer::ReduceReferenceEqual(Node* node) {
  DCHECK_EQ(IrOpcode::kReferenceEqual, node->opcode());
  Node* const left = NodeProperties::GetValueInput(node, 0);
  Node* const right = NodeProperties::GetValueInput(node, 1);
  HeapObjectMatcher match_left(left);
  HeapObjectMatcher match_right(right);
  if (match_left.HasValue() && match_right.HasValue()) {
    if (match_left.Value().is_identical_to(match_right.Value())) {
      return Replace(jsgraph()->TrueConstant());
    } else {
      return Replace(jsgraph()->FalseConstant());
    }
  }
  return NoChange();
}


Reduction SimplifiedOperatorReducer::Change(Node* node, const Operator* op,
                                            Node* a) {
  DCHECK_EQ(node->InputCount(), OperatorProperties::GetTotalInputCount(op));
  DCHECK_LE(1, node->InputCount());
  node->ReplaceInput(0, a);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}


Reduction SimplifiedOperatorReducer::ReplaceFloat64(double value) {
  return Replace(jsgraph()->Float64Constant(value));
}


Reduction SimplifiedOperatorReducer::ReplaceInt32(int32_t value) {
  return Replace(jsgraph()->Int32Constant(value));
}


Reduction SimplifiedOperatorReducer::ReplaceNumber(double value) {
  return Replace(jsgraph()->Constant(value));
}


Reduction SimplifiedOperatorReducer::ReplaceNumber(int32_t value) {
  return Replace(jsgraph()->Constant(value));
}


Graph* SimplifiedOperatorReducer::graph() const { return jsgraph()->graph(); }


MachineOperatorBuilder* SimplifiedOperatorReducer::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
