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

namespace v8 {
namespace internal {
namespace compiler {

JSIntrinsicLowering::JSIntrinsicLowering(Editor* editor, JSGraph* jsgraph,
                                         DeoptimizationMode mode)
    : AdvancedReducer(editor), jsgraph_(jsgraph), mode_(mode) {}

Reduction JSIntrinsicLowering::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kJSCallRuntime) return NoChange();
  const Runtime::Function* const f =
      Runtime::FunctionForId(CallRuntimeParametersOf(node->op()).id());
  if (f->intrinsic_type != Runtime::IntrinsicType::INLINE) return NoChange();
  switch (f->function_id) {
    case Runtime::kInlineCreateIterResultObject:
      return ReduceCreateIterResultObject(node);
    case Runtime::kInlineDeoptimizeNow:
      return ReduceDeoptimizeNow(node);
    case Runtime::kInlineGeneratorClose:
      return ReduceGeneratorClose(node);
    case Runtime::kInlineGeneratorGetInputOrDebugPos:
      return ReduceGeneratorGetInputOrDebugPos(node);
    case Runtime::kInlineGeneratorGetResumeMode:
      return ReduceGeneratorGetResumeMode(node);
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
    case Runtime::kInlineToNumber:
      return ReduceToNumber(node);
    case Runtime::kInlineToObject:
      return ReduceToObject(node);
    case Runtime::kInlineToString:
      return ReduceToString(node);
    case Runtime::kInlineCall:
      return ReduceCall(node);
    case Runtime::kInlineNewObject:
      return ReduceNewObject(node);
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


Reduction JSIntrinsicLowering::ReduceDeoptimizeNow(Node* node) {
  if (mode() != kDeoptimizationEnabled) return NoChange();
  Node* const frame_state = NodeProperties::GetFrameStateInput(node);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);

  // TODO(bmeurer): Move MergeControlToEnd() to the AdvancedReducer.
  Node* deoptimize = graph()->NewNode(
      common()->Deoptimize(DeoptimizeKind::kEager, DeoptimizeReason::kNoReason),
      frame_state, effect, control);
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
  Revisit(graph()->end());

  node->TrimInputCount(0);
  NodeProperties::ChangeOp(node, common()->Dead());
  return Changed(node);
}

Reduction JSIntrinsicLowering::ReduceGeneratorClose(Node* node) {
  Node* const generator = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Node* const closed = jsgraph()->Constant(JSGeneratorObject::kGeneratorClosed);
  Node* const undefined = jsgraph()->UndefinedConstant();
  Operator const* const op = simplified()->StoreField(
      AccessBuilder::ForJSGeneratorObjectContinuation());

  ReplaceWithValue(node, undefined, node);
  NodeProperties::RemoveType(node);
  return Change(node, op, generator, closed, effect, control);
}

Reduction JSIntrinsicLowering::ReduceGeneratorGetInputOrDebugPos(Node* node) {
  Node* const generator = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op = simplified()->LoadField(
      AccessBuilder::ForJSGeneratorObjectInputOrDebugPos());

  return Change(node, op, generator, effect, control);
}

Reduction JSIntrinsicLowering::ReduceGeneratorGetResumeMode(Node* node) {
  Node* const generator = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op =
      simplified()->LoadField(AccessBuilder::ForJSGeneratorObjectResumeMode());

  return Change(node, op, generator, effect, control);
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
  Node* vfalse = graph()->NewNode(simplified()->NumberEqual(), efalse,
                                  jsgraph()->Constant(instance_type));

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
  NodeProperties::ChangeOp(node, javascript()->ToInteger());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToNumber(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToNumber());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToLength(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToLength());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToObject(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToObject());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceToString(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->ToString());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceCall(Node* node) {
  size_t const arity = CallRuntimeParametersOf(node->op()).arity();
  NodeProperties::ChangeOp(
      node, javascript()->CallFunction(arity, 0.0f, VectorSlotPair(),
                                       ConvertReceiverMode::kAny,
                                       TailCallMode::kDisallow));
  return Changed(node);
}

Reduction JSIntrinsicLowering::ReduceNewObject(Node* node) {
  return Change(node, CodeFactory::FastNewObject(isolate()), 0);
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

SimplifiedOperatorBuilder* JSIntrinsicLowering::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
