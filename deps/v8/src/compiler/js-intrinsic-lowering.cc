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
    case Runtime::kInlineDebugIsActive:
      return ReduceDebugIsActive(node);
    case Runtime::kInlineDeoptimizeNow:
      return ReduceDeoptimizeNow(node);
    case Runtime::kInlineGeneratorClose:
      return ReduceGeneratorClose(node);
    case Runtime::kInlineCreateJSGeneratorObject:
      return ReduceCreateJSGeneratorObject(node);
    case Runtime::kInlineGeneratorGetInputOrDebugPos:
      return ReduceGeneratorGetInputOrDebugPos(node);
    case Runtime::kInlineAsyncGeneratorGetAwaitInputOrDebugPos:
      return ReduceAsyncGeneratorGetAwaitInputOrDebugPos(node);
    case Runtime::kInlineAsyncGeneratorReject:
      return ReduceAsyncGeneratorReject(node);
    case Runtime::kInlineAsyncGeneratorResolve:
      return ReduceAsyncGeneratorResolve(node);
    case Runtime::kInlineGeneratorGetResumeMode:
      return ReduceGeneratorGetResumeMode(node);
    case Runtime::kInlineGeneratorGetContext:
      return ReduceGeneratorGetContext(node);
    case Runtime::kInlineIsArray:
      return ReduceIsInstanceType(node, JS_ARRAY_TYPE);
    case Runtime::kInlineIsTypedArray:
      return ReduceIsInstanceType(node, JS_TYPED_ARRAY_TYPE);
    case Runtime::kInlineIsJSProxy:
      return ReduceIsInstanceType(node, JS_PROXY_TYPE);
    case Runtime::kInlineIsJSMap:
      return ReduceIsInstanceType(node, JS_MAP_TYPE);
    case Runtime::kInlineIsJSSet:
      return ReduceIsInstanceType(node, JS_SET_TYPE);
    case Runtime::kInlineIsJSMapIterator:
      return ReduceIsInstanceType(node, JS_MAP_ITERATOR_TYPE);
    case Runtime::kInlineIsJSSetIterator:
      return ReduceIsInstanceType(node, JS_SET_ITERATOR_TYPE);
    case Runtime::kInlineIsJSWeakMap:
      return ReduceIsInstanceType(node, JS_WEAK_MAP_TYPE);
    case Runtime::kInlineIsJSWeakSet:
      return ReduceIsInstanceType(node, JS_WEAK_SET_TYPE);
    case Runtime::kInlineIsJSReceiver:
      return ReduceIsJSReceiver(node);
    case Runtime::kInlineIsSmi:
      return ReduceIsSmi(node);
    case Runtime::kInlineFixedArrayGet:
      return ReduceFixedArrayGet(node);
    case Runtime::kInlineFixedArraySet:
      return ReduceFixedArraySet(node);
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
    case Runtime::kInlineGetSuperConstructor:
      return ReduceGetSuperConstructor(node);
    case Runtime::kInlineArrayBufferViewGetByteLength:
      return ReduceArrayBufferViewField(
          node, AccessBuilder::ForJSArrayBufferViewByteLength());
    case Runtime::kInlineArrayBufferViewGetByteOffset:
      return ReduceArrayBufferViewField(
          node, AccessBuilder::ForJSArrayBufferViewByteOffset());
    case Runtime::kInlineArrayBufferViewWasNeutered:
      return ReduceArrayBufferViewWasNeutered(node);
    case Runtime::kInlineMaxSmi:
      return ReduceMaxSmi(node);
    case Runtime::kInlineTypedArrayGetLength:
      return ReduceArrayBufferViewField(node,
                                        AccessBuilder::ForJSTypedArrayLength());
    case Runtime::kInlineTypedArrayMaxSizeInHeap:
      return ReduceTypedArrayMaxSizeInHeap(node);
    case Runtime::kInlineJSCollectionGetTable:
      return ReduceJSCollectionGetTable(node);
    case Runtime::kInlineStringGetRawHashField:
      return ReduceStringGetRawHashField(node);
    case Runtime::kInlineTheHole:
      return ReduceTheHole(node);
    case Runtime::kInlineClassOf:
      return ReduceClassOf(node);
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

Reduction JSIntrinsicLowering::ReduceDebugIsActive(Node* node) {
  Node* const value = jsgraph()->ExternalConstant(
      ExternalReference::debug_is_active_address(isolate()));
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op =
      simplified()->LoadField(AccessBuilder::ForExternalUint8Value());
  return Change(node, op, value, effect, control);
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

Reduction JSIntrinsicLowering::ReduceCreateJSGeneratorObject(Node* node) {
  Node* const closure = NodeProperties::GetValueInput(node, 0);
  Node* const receiver = NodeProperties::GetValueInput(node, 1);
  Node* const context = NodeProperties::GetContextInput(node);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op = javascript()->CreateGeneratorObject();
  Node* create_generator =
      graph()->NewNode(op, closure, receiver, context, effect, control);
  ReplaceWithValue(node, create_generator, create_generator);
  return Changed(create_generator);
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

Reduction JSIntrinsicLowering::ReduceAsyncGeneratorGetAwaitInputOrDebugPos(
    Node* node) {
  Node* const generator = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op = simplified()->LoadField(
      AccessBuilder::ForJSAsyncGeneratorObjectAwaitInputOrDebugPos());

  return Change(node, op, generator, effect, control);
}

Reduction JSIntrinsicLowering::ReduceAsyncGeneratorReject(Node* node) {
  return Change(node, CodeFactory::AsyncGeneratorReject(isolate()), 0);
}

Reduction JSIntrinsicLowering::ReduceAsyncGeneratorResolve(Node* node) {
  return Change(node, CodeFactory::AsyncGeneratorResolve(isolate()), 0);
}

Reduction JSIntrinsicLowering::ReduceGeneratorGetContext(Node* node) {
  Node* const generator = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  Operator const* const op =
      simplified()->LoadField(AccessBuilder::ForJSGeneratorObjectContext());

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
  // ToString is unnecessary if the input is a string.
  HeapObjectMatcher m(NodeProperties::GetValueInput(node, 0));
  if (m.HasValue() && m.Value()->IsString()) {
    ReplaceWithValue(node, m.node());
    return Replace(m.node());
  }
  NodeProperties::ChangeOp(node, javascript()->ToString());
  return Changed(node);
}


Reduction JSIntrinsicLowering::ReduceCall(Node* node) {
  size_t const arity = CallRuntimeParametersOf(node->op()).arity();
  NodeProperties::ChangeOp(node, javascript()->Call(arity));
  return Changed(node);
}

Reduction JSIntrinsicLowering::ReduceGetSuperConstructor(Node* node) {
  NodeProperties::ChangeOp(node, javascript()->GetSuperConstructor());
  return Changed(node);
}

Reduction JSIntrinsicLowering::ReduceArrayBufferViewField(
    Node* node, FieldAccess const& access) {
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Load the {receiver}s field.
  Node* value = effect = graph()->NewNode(simplified()->LoadField(access),
                                          receiver, effect, control);

  // Check if the {receiver}s buffer was neutered.
  Node* receiver_buffer = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
      receiver, effect, control);
  Node* check = effect = graph()->NewNode(
      simplified()->ArrayBufferWasNeutered(), receiver_buffer, effect, control);

  // Default to zero if the {receiver}s buffer was neutered.
  value = graph()->NewNode(
      common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
      check, jsgraph()->ZeroConstant(), value);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSIntrinsicLowering::ReduceArrayBufferViewWasNeutered(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if the {receiver}s buffer was neutered.
  Node* receiver_buffer = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
      receiver, effect, control);
  Node* value = effect = graph()->NewNode(
      simplified()->ArrayBufferWasNeutered(), receiver_buffer, effect, control);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSIntrinsicLowering::ReduceMaxSmi(Node* node) {
  Node* value = jsgraph()->Constant(Smi::kMaxValue);
  ReplaceWithValue(node, value);
  return Replace(value);
}

Reduction JSIntrinsicLowering::ReduceTypedArrayMaxSizeInHeap(Node* node) {
  Node* value = jsgraph()->Constant(FLAG_typed_array_max_size_in_heap);
  ReplaceWithValue(node, value);
  return Replace(value);
}

Reduction JSIntrinsicLowering::ReduceJSCollectionGetTable(Node* node) {
  Node* collection = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(node,
                simplified()->LoadField(AccessBuilder::ForJSCollectionTable()),
                collection, effect, control);
}

Reduction JSIntrinsicLowering::ReduceStringGetRawHashField(Node* node) {
  Node* string = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  return Change(node,
                simplified()->LoadField(AccessBuilder::ForNameHashField()),
                string, effect, control);
}

Reduction JSIntrinsicLowering::ReduceTheHole(Node* node) {
  Node* value = jsgraph()->TheHoleConstant();
  ReplaceWithValue(node, value);
  return Replace(value);
}

Reduction JSIntrinsicLowering::ReduceClassOf(Node* node) {
  RelaxEffectsAndControls(node);
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, javascript()->ClassOf());
  return Changed(node);
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
