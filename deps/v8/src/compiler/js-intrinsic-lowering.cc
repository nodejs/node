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

JSIntrinsicLowering::JSIntrinsicLowering(Editor* editor, JSGraph* jsgraph,
                                         DeoptimizationMode mode)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      mode_(mode),
      simplified_(jsgraph->zone()) {}


Reduction JSIntrinsicLowering::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kJSCallRuntime) return NoChange();
  const Runtime::Function* const f =
      Runtime::FunctionForId(CallRuntimeParametersOf(node->op()).id());
  if (f->intrinsic_type != Runtime::IntrinsicType::INLINE) return NoChange();
  switch (f->function_id) {
    case Runtime::kInlineConstructDouble:
      return ReduceConstructDouble(node);
    case Runtime::kInlineDateField:
      return ReduceDateField(node);
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
    case Runtime::kInlineIsDate:
      return ReduceIsInstanceType(node, JS_DATE_TYPE);
    case Runtime::kInlineIsTypedArray:
      return ReduceIsInstanceType(node, JS_TYPED_ARRAY_TYPE);
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
    case Runtime::kInlineIsMinusZero:
      return ReduceIsMinusZero(node);
    case Runtime::kInlineFixedArrayGet:
      return ReduceFixedArrayGet(node);
    case Runtime::kInlineFixedArraySet:
      return ReduceFixedArraySet(node);
    case Runtime::kInlineGetTypeFeedbackVector:
      return ReduceGetTypeFeedbackVector(node);
    case Runtime::kInlineGetCallerJSFunction:
      return ReduceGetCallerJSFunction(node);
    case Runtime::kInlineThrowNotDateError:
      return ReduceThrowNotDateError(node);
    case Runtime::kInlineCallFunction:
      return ReduceCallFunction(node);
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
  ReplaceWithValue(node, value);
  return Replace(value);
}


Reduction JSIntrinsicLowering::ReduceDateField(Node* node) {
  Node* const value = NodeProperties::GetValueInput(node, 0);
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  NumberMatcher mindex(index);
  if (mindex.Is(JSDate::kDateValue)) {
    return Change(
        node,
        simplified()->LoadField(AccessBuilder::ForJSDateField(
            static_cast<JSDate::FieldIndex>(static_cast<int>(mindex.Value())))),
        value, effect, control);
  }
  // TODO(turbofan): Optimize more patterns.
  return NoChange();
}


Reduction JSIntrinsicLowering::ReduceDeoptimizeNow(Node* node) {
  if (mode() != kDeoptimizationEnabled) return NoChange();
  Node* const frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);

  // TODO(bmeurer): Move MergeControlToEnd() to the AdvancedReducer.
  Node* deoptimize =
      graph()->NewNode(common()->Deoptimize(), frame_state, effect, control);
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);

  node->set_op(common()->Dead());
  node->TrimInputCount(0);
  return Changed(node);
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
  HeapObjectMatcher m(NodeProperties::GetValueInput(node, 0));
  if (!m.HasValue() || !m.Value().handle()->IsString()) {
    return ChangeToUndefined(node);
  }
  SmartArrayPointer<char> name =
      Handle<String>::cast(m.Value().handle())->ToCString();
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
  ReplaceWithValue(node, node, ephi);

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
  if (!machine()->Float64RoundDown().IsSupported()) return NoChange();
  return Change(node, machine()->Float64RoundDown().op());
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
  RelaxControls(node);
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
  ReplaceWithValue(node, string, node);
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
  ReplaceWithValue(node, value);
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
  ReplaceWithValue(node, node, ephi0);

  // Turn the {node} into a Phi.
  return Change(node, phi_op, vtrue0, vfalse0, merge0);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op) {
  // Replace all effect uses of {node} with the effect dependency.
  RelaxEffectsAndControls(node);
  // Remove the inputs corresponding to context, effect and control.
  NodeProperties::RemoveNonValueInputs(node);
  // Finally update the operator to the new one.
  node->set_op(op);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceIsMinusZero(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);

  Node* double_lo =
      graph()->NewNode(machine()->Float64ExtractLowWord32(), value);
  Node* check1 = graph()->NewNode(machine()->Word32Equal(), double_lo,
                                  jsgraph()->ZeroConstant());

  Node* double_hi =
      graph()->NewNode(machine()->Float64ExtractHighWord32(), value);
  Node* check2 = graph()->NewNode(
      machine()->Word32Equal(), double_hi,
      jsgraph()->Int32Constant(static_cast<int32_t>(0x80000000)));

  ReplaceWithValue(node, node, effect);

  Node* and_result = graph()->NewNode(machine()->Word32And(), check1, check2);

  return Change(node, machine()->Word32Equal(), and_result,
                jsgraph()->Int32Constant(1));
}


Reduction JSIntrinsicLowering::ReduceFixedArrayGet(Node* node) {
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(
      node, simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
      base, index, effect, control);
}


Reduction JSIntrinsicLowering::ReduceFixedArraySet(Node* node) {
  Node* base = node->InputAt(0);
  Node* index = node->InputAt(1);
  Node* value = node->InputAt(2);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* store = (graph()->NewNode(
      simplified()->StoreElement(AccessBuilder::ForFixedArrayElement()), base,
      index, value, effect, control));
  ReplaceWithValue(node, value, store);
  return Changed(store);
}


Reduction JSIntrinsicLowering::ReduceGetTypeFeedbackVector(Node* node) {
  Node* func = node->InputAt(0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  FieldAccess access = AccessBuilder::ForJSFunctionSharedFunctionInfo();
  Node* load =
      graph()->NewNode(simplified()->LoadField(access), func, effect, control);
  access = AccessBuilder::ForSharedFunctionInfoTypeFeedbackVector();
  return Change(node, simplified()->LoadField(access), load, load, control);
}


Reduction JSIntrinsicLowering::ReduceGetCallerJSFunction(Node* node) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* const frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* outer_frame = frame_state->InputAt(kFrameStateOuterStateInput);
  if (outer_frame->opcode() == IrOpcode::kFrameState) {
    // Use the runtime implementation to throw the appropriate error if the
    // containing function is inlined.
    return NoChange();
  }

  // TODO(danno): This implementation forces intrinsic lowering to happen after
  // inlining, which is fine for now, but eventually the frame-querying logic
  // probably should go later, e.g. in instruction selection, so that there is
  // no phase-ordering dependency.
  FieldAccess access = AccessBuilder::ForFrameCallerFramePtr();
  Node* fp = graph()->NewNode(machine()->LoadFramePointer());
  Node* next_fp =
      graph()->NewNode(simplified()->LoadField(access), fp, effect, control);
  return Change(node, simplified()->LoadField(AccessBuilder::ForFrameMarker()),
                next_fp, effect, control);
}


Reduction JSIntrinsicLowering::ReduceThrowNotDateError(Node* node) {
  if (mode() != kDeoptimizationEnabled) return NoChange();
  Node* const frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);

  // TODO(bmeurer): Move MergeControlToEnd() to the AdvancedReducer.
  Node* deoptimize =
      graph()->NewNode(common()->Deoptimize(), frame_state, effect, control);
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);

  node->set_op(common()->Dead());
  node->TrimInputCount(0);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceCallFunction(Node* node) {
  CallRuntimeParameters params = OpParameter<CallRuntimeParameters>(node->op());
  size_t arity = params.arity();
  node->set_op(javascript()->CallFunction(arity, NO_CALL_FUNCTION_FLAGS, STRICT,
                                          VectorSlotPair(), ALLOW_TAIL_CALLS));
  Node* function = node->InputAt(static_cast<int>(arity - 1));
  while (--arity != 0) {
    node->ReplaceInput(static_cast<int>(arity),
                       node->InputAt(static_cast<int>(arity - 1)));
  }
  node->ReplaceInput(0, function);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b) {
  node->set_op(op);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->TrimInputCount(2);
  RelaxControls(node);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b, Node* c) {
  node->set_op(op);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->ReplaceInput(2, c);
  node->TrimInputCount(3);
  RelaxControls(node);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b, Node* c, Node* d) {
  node->set_op(op);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->ReplaceInput(2, c);
  node->ReplaceInput(3, d);
  node->TrimInputCount(4);
  RelaxControls(node);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ChangeToUndefined(Node* node, Node* effect) {
  ReplaceWithValue(node, jsgraph()->UndefinedConstant(), effect);
  return Changed(node);
}


Graph* JSIntrinsicLowering::graph() const { return jsgraph()->graph(); }


CommonOperatorBuilder* JSIntrinsicLowering::common() const {
  return jsgraph()->common();
}

JSOperatorBuilder* JSIntrinsicLowering::javascript() const {
  return jsgraph_->javascript();
}


MachineOperatorBuilder* JSIntrinsicLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
