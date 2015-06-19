// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-intrinsic-lowering.h"

#include <stack>

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

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
    case Runtime::kInlineConstructDouble:
      return ReduceConstructDouble(node);
    case Runtime::kInlineDeoptimizeNow:
      return ReduceDeoptimizeNow(node);
    case Runtime::kInlineDoubleHi:
      return ReduceDoubleHi(node);
    case Runtime::kInlineDoubleLo:
      return ReduceDoubleLo(node);
    case Runtime::kInlineHeapObjectGetMap:
      return ReduceHeapObjectGetMap(node);
    case Runtime::kInlineIncrementStatsCounter:
      return ReduceIncrementStatsCounter(node);
    case Runtime::kInlineIsArray:
      return ReduceIsInstanceType(node, JS_ARRAY_TYPE);
    case Runtime::kInlineIsFunction:
      return ReduceIsInstanceType(node, JS_FUNCTION_TYPE);
    case Runtime::kInlineIsNonNegativeSmi:
      return ReduceIsNonNegativeSmi(node);
    case Runtime::kInlineIsRegExp:
      return ReduceIsInstanceType(node, JS_REGEXP_TYPE);
    case Runtime::kInlineIsSmi:
      return ReduceIsSmi(node);
    case Runtime::kInlineJSValueGetValue:
      return ReduceJSValueGetValue(node);
    case Runtime::kInlineLikely:
      return ReduceUnLikely(node, BranchHint::kTrue);
    case Runtime::kInlineMapGetInstanceType:
      return ReduceMapGetInstanceType(node);
    case Runtime::kInlineMathClz32:
      return ReduceMathClz32(node);
    case Runtime::kInlineMathFloor:
      return ReduceMathFloor(node);
    case Runtime::kInlineMathSqrt:
      return ReduceMathSqrt(node);
    case Runtime::kInlineOneByteSeqStringGetChar:
      return ReduceSeqStringGetChar(node, String::ONE_BYTE_ENCODING);
    case Runtime::kInlineOneByteSeqStringSetChar:
      return ReduceSeqStringSetChar(node, String::ONE_BYTE_ENCODING);
    case Runtime::kInlineStringGetLength:
      return ReduceStringGetLength(node);
    case Runtime::kInlineTwoByteSeqStringGetChar:
      return ReduceSeqStringGetChar(node, String::TWO_BYTE_ENCODING);
    case Runtime::kInlineTwoByteSeqStringSetChar:
      return ReduceSeqStringSetChar(node, String::TWO_BYTE_ENCODING);
    case Runtime::kInlineUnlikely:
      return ReduceUnLikely(node, BranchHint::kFalse);
    case Runtime::kInlineValueOf:
      return ReduceValueOf(node);
    default:
      break;
  }
  return NoChange();
}


Reduction JSIntrinsicLowering::ReduceConstructDouble(Node* node) {
  Node* high = NodeProperties::GetValueInput(node, 0);
  Node* low = NodeProperties::GetValueInput(node, 1);
  Node* value =
      graph()->NewNode(machine()->Float64InsertHighWord32(),
                       graph()->NewNode(machine()->Float64InsertLowWord32(),
                                        jsgraph()->Constant(0), low),
                       high);
  NodeProperties::ReplaceWithValue(node, value);
  return Replace(value);
}


Reduction JSIntrinsicLowering::ReduceDeoptimizeNow(Node* node) {
  // TODO(jarin): This should not depend on the global flag.
  if (!FLAG_turbo_deoptimization) return NoChange();

  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  DCHECK_EQ(frame_state->opcode(), IrOpcode::kFrameState);

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // We are making the continuation after the call dead. To
  // model this, we generate if (true) statement with deopt
  // in the true branch and continuation in the false branch.
  Node* branch =
      graph()->NewNode(common()->Branch(), jsgraph()->TrueConstant(), control);

  // False branch - the original continuation.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  NodeProperties::ReplaceWithValue(node, jsgraph()->UndefinedConstant(), effect,
                                   if_false);

  // True branch: deopt.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* deopt =
      graph()->NewNode(common()->Deoptimize(), frame_state, effect, if_true);

  // Connect the deopt to the merge exiting the graph.
  NodeProperties::MergeControlToEnd(graph(), common(), deopt);

  return Changed(deopt);
}


Reduction JSIntrinsicLowering::ReduceDoubleHi(Node* node) {
  return Change(node, machine()->Float64ExtractHighWord32());
}


Reduction JSIntrinsicLowering::ReduceDoubleLo(Node* node) {
  return Change(node, machine()->Float64ExtractLowWord32());
}


Reduction JSIntrinsicLowering::ReduceHeapObjectGetMap(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(node, simplified()->LoadField(AccessBuilder::ForMap()), value,
                effect, control);
}


Reduction JSIntrinsicLowering::ReduceIncrementStatsCounter(Node* node) {
  if (!FLAG_native_code_counters) return ChangeToUndefined(node);
  HeapObjectMatcher<String> m(NodeProperties::GetValueInput(node, 0));
  if (!m.HasValue() || !m.Value().handle()->IsString()) {
    return ChangeToUndefined(node);
  }
  SmartArrayPointer<char> name = m.Value().handle()->ToCString();
  StatsCounter counter(jsgraph()->isolate(), name.get());
  if (!counter.Enabled()) return ChangeToUndefined(node);

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  FieldAccess access = AccessBuilder::ForStatsCounter();
  Node* cnt = jsgraph()->ExternalConstant(ExternalReference(&counter));
  Node* load =
      graph()->NewNode(simplified()->LoadField(access), cnt, effect, control);
  Node* inc =
      graph()->NewNode(machine()->Int32Add(), load, jsgraph()->OneConstant());
  Node* store = graph()->NewNode(simplified()->StoreField(access), cnt, inc,
                                 load, control);
  return ChangeToUndefined(node, store);
}


Reduction JSIntrinsicLowering::ReduceIsInstanceType(
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


Reduction JSIntrinsicLowering::ReduceIsNonNegativeSmi(Node* node) {
  return Change(node, simplified()->ObjectIsNonNegativeSmi());
}


Reduction JSIntrinsicLowering::ReduceIsSmi(Node* node) {
  return Change(node, simplified()->ObjectIsSmi());
}


Reduction JSIntrinsicLowering::ReduceJSValueGetValue(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(node, simplified()->LoadField(AccessBuilder::ForValue()), value,
                effect, control);
}


Reduction JSIntrinsicLowering::ReduceMapGetInstanceType(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(node,
                simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
                value, effect, control);
}


Reduction JSIntrinsicLowering::ReduceMathClz32(Node* node) {
  return Change(node, machine()->Word32Clz());
}


Reduction JSIntrinsicLowering::ReduceMathFloor(Node* node) {
  if (!machine()->HasFloat64RoundDown()) return NoChange();
  return Change(node, machine()->Float64RoundDown());
}


Reduction JSIntrinsicLowering::ReduceMathSqrt(Node* node) {
  return Change(node, machine()->Float64Sqrt());
}


Reduction JSIntrinsicLowering::ReduceSeqStringGetChar(
    Node* node, String::Encoding encoding) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  node->set_op(
      simplified()->LoadElement(AccessBuilder::ForSeqStringChar(encoding)));
  node->ReplaceInput(2, effect);
  node->ReplaceInput(3, control);
  node->TrimInputCount(4);
  NodeProperties::ReplaceWithValue(node, node, node);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceSeqStringSetChar(
    Node* node, String::Encoding encoding) {
  // Note: The intrinsic has a strange argument order, so we need to reshuffle.
  Node* index = NodeProperties::GetValueInput(node, 0);
  Node* chr = NodeProperties::GetValueInput(node, 1);
  Node* string = NodeProperties::GetValueInput(node, 2);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  node->set_op(
      simplified()->StoreElement(AccessBuilder::ForSeqStringChar(encoding)));
  node->ReplaceInput(0, string);
  node->ReplaceInput(1, index);
  node->ReplaceInput(2, chr);
  node->ReplaceInput(3, effect);
  node->ReplaceInput(4, control);
  node->TrimInputCount(5);
  NodeProperties::RemoveBounds(node);
  NodeProperties::ReplaceWithValue(node, string, node);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceStringGetLength(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(node, simplified()->LoadField(
                          AccessBuilder::ForStringLength(graph()->zone())),
                value, effect, control);
}


Reduction JSIntrinsicLowering::ReduceUnLikely(Node* node, BranchHint hint) {
  std::stack<Node*> nodes_to_visit;
  nodes_to_visit.push(node);
  while (!nodes_to_visit.empty()) {
    Node* current = nodes_to_visit.top();
    nodes_to_visit.pop();
    for (Node* use : current->uses()) {
      if (use->opcode() == IrOpcode::kJSToBoolean) {
        // We have to "look through" ToBoolean calls.
        nodes_to_visit.push(use);
      } else if (use->opcode() == IrOpcode::kBranch) {
        // Actually set the hint on any branch using the intrinsic node.
        use->set_op(common()->Branch(hint));
      }
    }
  }
  // Apart from adding hints to branchs nodes, this is the identity function.
  Node* value = NodeProperties::GetValueInput(node, 0);
  NodeProperties::ReplaceWithValue(node, value);
  return Changed(value);
}


Reduction JSIntrinsicLowering::ReduceValueOf(Node* node) {
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
  // Replace all effect uses of {node} with the effect dependency.
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
  NodeProperties::ReplaceWithValue(node, node, node);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ChangeToUndefined(Node* node, Node* effect) {
  NodeProperties::ReplaceWithValue(node, jsgraph()->UndefinedConstant(),
                                   effect);
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
