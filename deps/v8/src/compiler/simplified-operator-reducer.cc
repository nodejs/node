// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-operator-reducer.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

SimplifiedOperatorReducer::SimplifiedOperatorReducer(JSGraph* jsgraph)
    : jsgraph_(jsgraph), simplified_(jsgraph->zone()) {}


SimplifiedOperatorReducer::~SimplifiedOperatorReducer() {}


Reduction SimplifiedOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kAnyToBoolean:
      return ReduceAnyToBoolean(node);
    case IrOpcode::kBooleanNot: {
      HeapObjectMatcher<HeapObject> m(node->InputAt(0));
      if (m.Is(Unique<HeapObject>::CreateImmovable(factory()->false_value()))) {
        return Replace(jsgraph()->TrueConstant());
      }
      if (m.Is(Unique<HeapObject>::CreateImmovable(factory()->true_value()))) {
        return Replace(jsgraph()->FalseConstant());
      }
      if (m.IsBooleanNot()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeBitToBool: {
      Int32Matcher m(node->InputAt(0));
      if (m.Is(0)) return Replace(jsgraph()->FalseConstant());
      if (m.Is(1)) return Replace(jsgraph()->TrueConstant());
      if (m.IsChangeBoolToBit()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeBoolToBit: {
      HeapObjectMatcher<HeapObject> m(node->InputAt(0));
      if (m.Is(Unique<HeapObject>::CreateImmovable(factory()->false_value()))) {
        return ReplaceInt32(0);
      }
      if (m.Is(Unique<HeapObject>::CreateImmovable(factory()->true_value()))) {
        return ReplaceInt32(1);
      }
      if (m.IsChangeBitToBool()) return Replace(m.node()->InputAt(0));
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
        return Change(node, machine()->ChangeInt32ToFloat64(),
                      m.node()->InputAt(0));
      }
      if (m.IsChangeUint32ToTagged()) {
        return Change(node, machine()->ChangeUint32ToFloat64(),
                      m.node()->InputAt(0));
      }
      break;
    }
    case IrOpcode::kChangeTaggedToInt32: {
      NumberMatcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceInt32(DoubleToInt32(m.Value()));
      if (m.IsChangeFloat64ToTagged()) {
        return Change(node, machine()->ChangeFloat64ToInt32(),
                      m.node()->InputAt(0));
      }
      if (m.IsChangeInt32ToTagged()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeTaggedToUint32: {
      NumberMatcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceUint32(DoubleToUint32(m.Value()));
      if (m.IsChangeFloat64ToTagged()) {
        return Change(node, machine()->ChangeFloat64ToUint32(),
                      m.node()->InputAt(0));
      }
      if (m.IsChangeUint32ToTagged()) return Replace(m.node()->InputAt(0));
      break;
    }
    case IrOpcode::kChangeUint32ToTagged: {
      Uint32Matcher m(node->InputAt(0));
      if (m.HasValue()) return ReplaceNumber(FastUI2D(m.Value()));
      break;
    }
    default:
      break;
  }
  return NoChange();
}


Reduction SimplifiedOperatorReducer::ReduceAnyToBoolean(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Boolean())) {
    // AnyToBoolean(x:boolean) => x
    return Replace(input);
  }
  if (input_type->Is(Type::OrderedNumber())) {
    // AnyToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x, #0))
    Node* compare = graph()->NewNode(simplified()->NumberEqual(), input,
                                     jsgraph()->ZeroConstant());
    return Change(node, simplified()->BooleanNot(), compare);
  }
  if (input_type->Is(Type::String())) {
    // AnyToBoolean(x:string) => BooleanNot(NumberEqual(x.length, #0))
    FieldAccess const access = AccessBuilder::ForStringLength();
    Node* length = graph()->NewNode(simplified()->LoadField(access), input,
                                    graph()->start(), graph()->start());
    Node* compare = graph()->NewNode(simplified()->NumberEqual(), length,
                                     jsgraph()->ZeroConstant());
    return Change(node, simplified()->BooleanNot(), compare);
  }
  return NoChange();
}


Reduction SimplifiedOperatorReducer::Change(Node* node, const Operator* op,
                                            Node* a) {
  DCHECK_EQ(node->InputCount(), OperatorProperties::GetTotalInputCount(op));
  DCHECK_LE(1, node->InputCount());
  node->set_op(op);
  node->ReplaceInput(0, a);
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


Factory* SimplifiedOperatorReducer::factory() const {
  return jsgraph()->isolate()->factory();
}


CommonOperatorBuilder* SimplifiedOperatorReducer::common() const {
  return jsgraph()->common();
}


MachineOperatorBuilder* SimplifiedOperatorReducer::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
