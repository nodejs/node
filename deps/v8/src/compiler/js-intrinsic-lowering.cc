// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-intrinsic-lowering.h"

#include <stack>

#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/counters.h"
#include "src/objects-inl.h"
#include "src/type-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

JSIntrinsicLowering::JSIntrinsicLowering(Editor* editor, JSGraph* jsgraph,
                                         DeoptimizationMode mode)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      mode_(mode),
      type_cache_(TypeCache::Get()) {}


Reduction JSIntrinsicLowering::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kJSCallRuntime) return NoChange();
  const Runtime::Function* const f =
      Runtime::FunctionForId(CallRuntimeParametersOf(node->op()).id());
  if (f->intrinsic_type != Runtime::IntrinsicType::INLINE) return NoChange();
  switch (f->function_id) {
    case Runtime::kInlineConstructDouble:
      return ReduceConstructDouble(node);
    case Runtime::kInlineCreateIterResultObject:
      return ReduceCreateIterResultObject(node);
    case Runtime::kInlineDeoptimizeNow:
      return ReduceDeoptimizeNow(node);
    case Runtime::kInlineDoubleHi:
      return ReduceDoubleHi(node);
    case Runtime::kInlineDoubleLo:
      return ReduceDoubleLo(node);
    case Runtime::kInlineIncrementStatsCounter:
      return ReduceIncrementStatsCounter(node);
    case Runtime::kInlineIsArray:
      return ReduceIsInstanceType(node, JS_ARRAY_TYPE);
    case Runtime::kInlineIsTypedArray:
      return ReduceIsInstanceType(node, JS_TYPED_ARRAY_TYPE);
    case Runtime::kInlineIsRegExp:
      return ReduceIsInstanceType(node, JS_REGEXP_TYPE);
    case Runtime::kInlineIsJSReceiver:
      return ReduceIsJSReceiver(node);
    case Runtime::kInlineIsSmi:
      return ReduceIsSmi(node);
    case Runtime::kInlineMathClz32:
      return ReduceMathClz32(node);
    case Runtime::kInlineMathFloor:
      return ReduceMathFloor(node);
    case Runtime::kInlineMathSqrt:
      return ReduceMathSqrt(node);
    case Runtime::kInlineValueOf:
      return ReduceValueOf(node);
    case Runtime::kInlineFixedArrayGet:
      return ReduceFixedArrayGet(node);
    case Runtime::kInlineFixedArraySet:
      return ReduceFixedArraySet(node);
    case Runtime::kInlineRegExpConstructResult:
      return ReduceRegExpConstructResult(node);
    case Runtime::kInlineRegExpExec:
      return ReduceRegExpExec(node);
    case Runtime::kInlineRegExpFlags:
      return ReduceRegExpFlags(node);
    case Runtime::kInlineRegExpSource:
      return ReduceRegExpSource(node);
    case Runtime::kInlineSubString:
      return ReduceSubString(node);
    case Runtime::kInlineToInteger:
      return ReduceToInteger(node);
    case Runtime::kInlineToLength:
      return ReduceToLength(node);
    case Runtime::kInlineToName:
      return ReduceToName(node);
    case Runtime::kInlineToNumber:
      return ReduceToNumber(node);
    case Runtime::kInlineToObject:
      return ReduceToObject(node);
    case Runtime::kInlineToPrimitive:
      return ReduceToPrimitive(node);
    case Runtime::kInlineToString:
      return ReduceToString(node);
    case Runtime::kInlineCall:
      return ReduceCall(node);
    case Runtime::kInlineTailCall:
      return ReduceTailCall(node);
    case Runtime::kInlineGetSuperConstructor:
      return ReduceGetSuperConstructor(node);
    default:
      break;
  }
  return NoChange();
}


Reduction JSIntrinsicLowering::ReduceCreateIterResultObject(Node* node) {
  Node* const value = NodeProperties::GetValueInput(node, 0);
  Node* const done = NodeProperties::GetValueInput(node, 1);
  Node* const context = NodeProperties::GetContextInput(node);
  Node* const effect = NodeProperties::GetEffectInput(node);
  return Change(node, javascript()->CreateIterResultObject(), value, done,
                context, effect);
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


Reduction JSIntrinsicLowering::ReduceDeoptimizeNow(Node* node) {
  if (mode() != kDeoptimizationEnabled) return NoChange();
  Node* const frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);

  // TODO(bmeurer): Move MergeControlToEnd() to the AdvancedReducer.
  Node* deoptimize =
      graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kEager),
                       frame_state, effect, control);
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
  Revisit(graph()->end());

  node->TrimInputCount(0);
  NodeProperties::ChangeOp(node, common()->Dead());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceDoubleHi(Node* node) {
  return Change(node, machine()->Float64ExtractHighWord32());
}


Reduction JSIntrinsicLowering::ReduceDoubleLo(Node* node) {
  return Change(node, machine()->Float64ExtractLowWord32());
}


Reduction JSIntrinsicLowering::ReduceIncrementStatsCounter(Node* node) {
  if (!FLAG_native_code_counters) return ChangeToUndefined(node);
  HeapObjectMatcher m(NodeProperties::GetValueInput(node, 0));
  if (!m.HasValue() || !m.Value()->IsString()) {
    return ChangeToUndefined(node);
  }
  base::SmartArrayPointer<char> name =
      Handle<String>::cast(m.Value())->ToCString();
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
  return Change(node, common()->Phi(MachineRepresentation::kTagged, 2), vtrue,
                vfalse, merge);
}


Reduction JSIntrinsicLowering::ReduceIsJSReceiver(Node* node) {
  return Change(node, simplified()->ObjectIsReceiver());
}


Reduction JSIntrinsicLowering::ReduceIsSmi(Node* node) {
  return Change(node, simplified()->ObjectIsSmi());
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
  const Operator* const phi_op =
      common()->Phi(MachineRepresentation::kTagged, 2);

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
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
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


Reduction JSIntrinsicLowering::ReduceRegExpConstructResult(Node* node) {
  // TODO(bmeurer): Introduce JSCreateRegExpResult?
  return Change(node, CodeFactory::RegExpConstructResult(isolate()), 0);
}


Reduction JSIntrinsicLowering::ReduceRegExpExec(Node* node) {
  return Change(node, CodeFactory::RegExpExec(isolate()), 4);
}


Reduction JSIntrinsicLowering::ReduceRegExpFlags(Node* node) {
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op =
      simplified()->LoadField(AccessBuilder::ForJSRegExpFlags());
  return Change(node, op, receiver, effect, control);
}


Reduction JSIntrinsicLowering::ReduceRegExpSource(Node* node) {
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op =
      simplified()->LoadField(AccessBuilder::ForJSRegExpSource());
  return Change(node, op, receiver, effect, control);
}


Reduction JSIntrinsicLowering::ReduceSubString(Node* node) {
  return Change(node, CodeFactory::SubString(isolate()), 3);
}


Reduction JSIntrinsicLowering::ReduceToInteger(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // ToInteger is a no-op on integer values and -0.
  Type* value_type = NodeProperties::GetType(value);
  if (value_type->Is(type_cache().kIntegerOrMinusZero)) {
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = value;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse;
  {
    vfalse = efalse =
        graph()->NewNode(javascript()->CallRuntime(Runtime::kToInteger), value,
                         context, frame_state, efalse, if_false);
    if_false = graph()->NewNode(common()->IfSuccess(), vfalse);
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);
  // TODO(bmeurer, mstarzinger): Rewire IfException inputs to {vfalse}.
  ReplaceWithValue(node, value, effect, control);
  return Changed(value);
}


Reduction JSIntrinsicLowering::ReduceToName(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToName());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToNumber(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToNumber());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToLength(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Type* value_type = NodeProperties::GetType(value);
  if (value_type->Is(type_cache().kIntegerOrMinusZero)) {
    if (value_type->Max() <= 0.0) {
      value = jsgraph()->ZeroConstant();
    } else if (value_type->Min() >= kMaxSafeInteger) {
      value = jsgraph()->Constant(kMaxSafeInteger);
    } else {
      if (value_type->Min() <= 0.0) {
        value = graph()->NewNode(
            common()->Select(MachineRepresentation::kTagged),
            graph()->NewNode(simplified()->NumberLessThanOrEqual(), value,
                             jsgraph()->ZeroConstant()),
            jsgraph()->ZeroConstant(), value);
        value_type = Type::Range(0.0, value_type->Max(), graph()->zone());
        NodeProperties::SetType(value, value_type);
      }
      if (value_type->Max() > kMaxSafeInteger) {
        value = graph()->NewNode(
            common()->Select(MachineRepresentation::kTagged),
            graph()->NewNode(simplified()->NumberLessThanOrEqual(),
                             jsgraph()->Constant(kMaxSafeInteger), value),
            jsgraph()->Constant(kMaxSafeInteger), value);
        value_type =
            Type::Range(value_type->Min(), kMaxSafeInteger, graph()->zone());
        NodeProperties::SetType(value, value_type);
      }
    }
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return Change(node, CodeFactory::ToLength(isolate()), 0);
}


Reduction JSIntrinsicLowering::ReduceToObject(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToObject());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToPrimitive(Node* node) {
  Node* value = NodeProperties::GetValueInput(node, 0);
  Type* value_type = NodeProperties::GetType(value);
  if (value_type->Is(Type::Primitive())) {
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}


Reduction JSIntrinsicLowering::ReduceToString(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToString());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceCall(Node* node) {
  size_t const arity = CallRuntimeParametersOf(node->op()).arity();
  NodeProperties::ChangeOp(node,
                           javascript()->CallFunction(arity, VectorSlotPair(),
                                                      ConvertReceiverMode::kAny,
                                                      TailCallMode::kDisallow));
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceTailCall(Node* node) {
  size_t const arity = CallRuntimeParametersOf(node->op()).arity();
  NodeProperties::ChangeOp(node,
                           javascript()->CallFunction(arity, VectorSlotPair(),
                                                      ConvertReceiverMode::kAny,
                                                      TailCallMode::kAllow));
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceGetSuperConstructor(Node* node) {
  Node* active_function = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* active_function_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       active_function, effect, control);
  return Change(node, simplified()->LoadField(AccessBuilder::ForMapPrototype()),
                active_function_map, effect, control);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b) {
  RelaxControls(node);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b, Node* c) {
  RelaxControls(node);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->ReplaceInput(2, c);
  node->TrimInputCount(3);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, const Operator* op, Node* a,
                                      Node* b, Node* c, Node* d) {
  RelaxControls(node);
  node->ReplaceInput(0, a);
  node->ReplaceInput(1, b);
  node->ReplaceInput(2, c);
  node->ReplaceInput(3, d);
  node->TrimInputCount(4);
  NodeProperties::ChangeOp(node, op);
  return Changed(node);
}


Reduction JSIntrinsicLowering::ChangeToUndefined(Node* node, Node* effect) {
  ReplaceWithValue(node, jsgraph()->UndefinedConstant(), effect);
  return Changed(node);
}


Reduction JSIntrinsicLowering::Change(Node* node, Callable const& callable,
                                      int stack_parameter_count) {
  CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), callable.descriptor(), stack_parameter_count,
      CallDescriptor::kNeedsFrameState, node->op()->properties());
  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(callable.code()));
  NodeProperties::ChangeOp(node, common()->Call(desc));
  return Changed(node);
}


Graph* JSIntrinsicLowering::graph() const { return jsgraph()->graph(); }


Isolate* JSIntrinsicLowering::isolate() const { return jsgraph()->isolate(); }


CommonOperatorBuilder* JSIntrinsicLowering::common() const {
  return jsgraph()->common();
}

JSOperatorBuilder* JSIntrinsicLowering::javascript() const {
  return jsgraph_->javascript();
}


MachineOperatorBuilder* JSIntrinsicLowering::machine() const {
  return jsgraph()->machine();
}


SimplifiedOperatorBuilder* JSIntrinsicLowering::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
