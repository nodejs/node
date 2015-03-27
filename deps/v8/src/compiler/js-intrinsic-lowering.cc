// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-intrinsic-lowering.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

JSIntrinsicLowering::JSIntrinsicLowering(JSGraph* jsgraph)
    : jsgraph_(jsgraph), simplified_(jsgraph->zone()) {}


Reduction JSIntrinsicLowering::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kJSCallRuntime) return NoChange();
  const Runtime::Function* const f =
      Runtime::FunctionForId(CallRuntimeParametersOf(node->op()).id());
  if (f->intrinsic_type != Runtime::IntrinsicType::INLINE) return NoChange();
  switch (f->function_id) {
    case Runtime::kInlineIsSmi:
      return ReduceInlineIsSmi(node);
    case Runtime::kInlineIsNonNegativeSmi:
      return ReduceInlineIsNonNegativeSmi(node);
    case Runtime::kInlineIsArray:
      return ReduceInlineIsInstanceType(node, JS_ARRAY_TYPE);
    case Runtime::kInlineIsFunction:
      return ReduceInlineIsInstanceType(node, JS_FUNCTION_TYPE);
    case Runtime::kInlineIsRegExp:
      return ReduceInlineIsInstanceType(node, JS_REGEXP_TYPE);
    case Runtime::kInlineValueOf:
      return ReduceInlineValueOf(node);
    default:
      break;
  }
  return NoChange();
}


Reduction JSIntrinsicLowering::ReduceInlineIsSmi(Node* node) {
  return Change(node, simplified()->ObjectIsSmi());
}


Reduction JSIntrinsicLowering::ReduceInlineIsNonNegativeSmi(Node* node) {
  return Change(node, simplified()->ObjectIsNonNegativeSmi());
}


Reduction JSIntrinsicLowering::ReduceInlineIsInstanceType(
    Node* node, InstanceType instance_type) {
  // if (%_IsSmi(value)) {
  //   return false;
  // } else {
  //   return %_GetInstanceType(%_GetMap(value)) == instance_type;
  // }
  MachineType const type = static_cast<MachineType>(kTypeBool | kRepTagged);

  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch = graph()->NewNode(common()->Branch(), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->FalseConstant();

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()), value,
                       effect, if_false),
      effect, if_false);
  Node* vfalse = graph()->NewNode(machine()->Word32Equal(), efalse,
                                  jsgraph()->Int32Constant(instance_type));

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);

  // Replace all effect uses of {node} with the {ephi}.
  Node* ephi = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);
  NodeProperties::ReplaceWithValue(node, node, ephi);

  // Turn the {node} into a Phi.
  return Change(node, common()->Phi(type, 2), vtrue, vfalse, merge);
}


Reduction JSIntrinsicLowering::ReduceInlineValueOf(Node* node) {
  // if (%_IsSmi(value)) {
  //   return value;
  // } else if (%_GetInstanceType(%_GetMap(value)) == JS_VALUE_TYPE) {
  //   return %_GetValue(value);
  // } else {
  //   return value;
  // }
  const Operator* const merge_op = common()->Merge(2);
  const Operator* const ephi_op = common()->EffectPhi(2);
  const Operator* const phi_op = common()->Phi(kMachAnyTagged, 2);

  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch0 = graph()->NewNode(common()->Branch(), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0 = effect;
  Node* vtrue0 = value;

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0;
  Node* vfalse0;
  {
    Node* check1 = graph()->NewNode(
        machine()->Word32Equal(),
        graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
            graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                             value, effect, if_false0),
            effect, if_false0),
        jsgraph()->Int32Constant(JS_VALUE_TYPE));
    Node* branch1 = graph()->NewNode(common()->Branch(), check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForValue()),
                         value, effect, if_true1);
    Node* vtrue1 = etrue1;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = effect;
    Node* vfalse1 = value;

    Node* merge1 = graph()->NewNode(merge_op, if_true1, if_false1);
    efalse0 = graph()->NewNode(ephi_op, etrue1, efalse1, merge1);
    vfalse0 = graph()->NewNode(phi_op, vtrue1, vfalse1, merge1);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);


  // Replace all effect uses of {node} with the {ephi0}.
  Node* ephi0 = graph()->NewNode(ephi_op, etrue0, efalse0, merge0);
  NodeProperties::ReplaceWithValue(node, node, ephi0);

  // Turn the {node} into a Phi.
  return Change(node, phi_op, vtrue0, vfalse0, merge0);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op) {
  // Remove the effects from the node and update its effect usages.
  NodeProperties::ReplaceWithValue(node, node);
  // Remove the inputs corresponding to context, effect and control.
  NodeProperties::RemoveNonValueInputs(node);
  // Finally update the operator to the new one.
  node->set_op(op);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b, Node* c) {
  node->set_op(op);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->ReplaceInput(2, c);
  node->TrimInputCount(3);
  return Changed(node);
}


Graph* JSIntrinsicLowering::graph() const { return jsgraph()->graph(); }


CommonOperatorBuilder* JSIntrinsicLowering::common() const {
  return jsgraph()->common();
}


MachineOperatorBuilder* JSIntrinsicLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
