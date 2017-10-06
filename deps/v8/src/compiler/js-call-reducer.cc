// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-call-reducer.h"

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/simplified-operator.h"
#include "src/feedback-vector-inl.h"
#include "src/ic/call-optimization.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSCallReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSConstruct:
      return ReduceJSConstruct(node);
    case IrOpcode::kJSConstructWithArrayLike:
      return ReduceJSConstructWithArrayLike(node);
    case IrOpcode::kJSConstructWithSpread:
      return ReduceJSConstructWithSpread(node);
    case IrOpcode::kJSCall:
      return ReduceJSCall(node);
    case IrOpcode::kJSCallWithArrayLike:
      return ReduceJSCallWithArrayLike(node);
    case IrOpcode::kJSCallWithSpread:
      return ReduceJSCallWithSpread(node);
    default:
      break;
  }
  return NoChange();
}

void JSCallReducer::Finalize() {
  // TODO(turbofan): This is not the best solution; ideally we would be able
  // to teach the GraphReducer about arbitrary dependencies between different
  // nodes, even if they don't show up in the use list of the other node.
  std::set<Node*> const waitlist = std::move(waitlist_);
  for (Node* node : waitlist) {
    if (!node->IsDead()) {
      Reduction const reduction = Reduce(node);
      if (reduction.Changed()) {
        Node* replacement = reduction.replacement();
        if (replacement != node) {
          Replace(node, replacement);
        }
      }
    }
  }
}

// ES6 section 22.1.1 The Array Constructor
Reduction JSCallReducer::ReduceArrayConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* target = NodeProperties::GetValueInput(node, 0);
  CallParameters const& p = CallParametersOf(node->op());

  // Check if we have an allocation site from the CallIC.
  Handle<AllocationSite> site;
  if (p.feedback().IsValid()) {
    CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
    Handle<Object> feedback(nexus.GetFeedback(), isolate());
    if (feedback->IsAllocationSite()) {
      site = Handle<AllocationSite>::cast(feedback);
    }
  }

  // Turn the {node} into a {JSCreateArray} call.
  DCHECK_LE(2u, p.arity());
  size_t const arity = p.arity() - 2;
  NodeProperties::ReplaceValueInput(node, target, 0);
  NodeProperties::ReplaceValueInput(node, target, 1);
  // TODO(bmeurer): We might need to propagate the tail call mode to
  // the JSCreateArray operator, because an Array call in tail call
  // position must always properly consume the parent stack frame.
  NodeProperties::ChangeOp(node, javascript()->CreateArray(arity, site));
  return Changed(node);
}

// ES6 section 19.3.1.1 Boolean ( value )
Reduction JSCallReducer::ReduceBooleanConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());

  // Replace the {node} with a proper {JSToBoolean} operator.
  DCHECK_LE(2u, p.arity());
  Node* value = (p.arity() == 2) ? jsgraph()->UndefinedConstant()
                                 : NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  value = graph()->NewNode(javascript()->ToBoolean(ToBooleanHint::kAny), value,
                           context);
  ReplaceWithValue(node, value);
  return Replace(value);
}

// ES6 section 20.1.1 The Number Constructor
Reduction JSCallReducer::ReduceNumberConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());

  // Turn the {node} into a {JSToNumber} call.
  DCHECK_LE(2u, p.arity());
  Node* value = (p.arity() == 2) ? jsgraph()->ZeroConstant()
                                 : NodeProperties::GetValueInput(node, 2);
  NodeProperties::ReplaceValueInputs(node, value);
  NodeProperties::ChangeOp(node, javascript()->ToNumber());
  return Changed(node);
}

namespace {

bool CanBeNullOrUndefined(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSCreate:
    case IrOpcode::kJSCreateArguments:
    case IrOpcode::kJSCreateArray:
    case IrOpcode::kJSCreateClosure:
    case IrOpcode::kJSCreateIterResultObject:
    case IrOpcode::kJSCreateKeyValueArray:
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:
    case IrOpcode::kJSCreateLiteralRegExp:
    case IrOpcode::kJSConstruct:
    case IrOpcode::kJSConstructForwardVarargs:
    case IrOpcode::kJSConstructWithSpread:
    case IrOpcode::kJSConvertReceiver:
    case IrOpcode::kJSToBoolean:
    case IrOpcode::kJSToInteger:
    case IrOpcode::kJSToLength:
    case IrOpcode::kJSToName:
    case IrOpcode::kJSToNumber:
    case IrOpcode::kJSToObject:
    case IrOpcode::kJSToString:
    case IrOpcode::kJSToPrimitiveToString:
      return false;
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> value = HeapObjectMatcher(node).Value();
      Isolate* const isolate = value->GetIsolate();
      return value->IsNull(isolate) || value->IsUndefined(isolate);
    }
    default:
      return true;
  }
}

}  // namespace

// ES6 section 19.2.3.1 Function.prototype.apply ( thisArg, argArray )
Reduction JSCallReducer::ReduceFunctionPrototypeApply(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  size_t arity = p.arity();
  DCHECK_LE(2u, arity);
  ConvertReceiverMode convert_mode = ConvertReceiverMode::kAny;
  if (arity == 2) {
    // Neither thisArg nor argArray was provided.
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
    node->ReplaceInput(0, node->InputAt(1));
    node->ReplaceInput(1, jsgraph()->UndefinedConstant());
  } else if (arity == 3) {
    // The argArray was not provided, just remove the {target}.
    node->RemoveInput(0);
    --arity;
  } else {
    Node* target = NodeProperties::GetValueInput(node, 1);
    Node* this_argument = NodeProperties::GetValueInput(node, 2);
    Node* arguments_list = NodeProperties::GetValueInput(node, 3);
    Node* context = NodeProperties::GetContextInput(node);
    Node* frame_state = NodeProperties::GetFrameStateInput(node);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    // If {arguments_list} cannot be null or undefined, we don't need
    // to expand this {node} to control-flow.
    if (!CanBeNullOrUndefined(arguments_list)) {
      // Massage the value inputs appropriately.
      node->ReplaceInput(0, target);
      node->ReplaceInput(1, this_argument);
      node->ReplaceInput(2, arguments_list);
      while (arity-- > 3) node->RemoveInput(3);

      // Morph the {node} to a {JSCallWithArrayLike}.
      NodeProperties::ChangeOp(node,
                               javascript()->CallWithArrayLike(p.frequency()));
      Reduction const reduction = ReduceJSCallWithArrayLike(node);
      return reduction.Changed() ? reduction : Changed(node);
    } else {
      // Check whether {arguments_list} is null.
      Node* check_null =
          graph()->NewNode(simplified()->ReferenceEqual(), arguments_list,
                           jsgraph()->NullConstant());
      control = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                 check_null, control);
      Node* if_null = graph()->NewNode(common()->IfTrue(), control);
      control = graph()->NewNode(common()->IfFalse(), control);

      // Check whether {arguments_list} is undefined.
      Node* check_undefined =
          graph()->NewNode(simplified()->ReferenceEqual(), arguments_list,
                           jsgraph()->UndefinedConstant());
      control = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                 check_undefined, control);
      Node* if_undefined = graph()->NewNode(common()->IfTrue(), control);
      control = graph()->NewNode(common()->IfFalse(), control);

      // Lower to {JSCallWithArrayLike} if {arguments_list} is neither null
      // nor undefined.
      Node* effect0 = effect;
      Node* control0 = control;
      Node* value0 = effect0 = control0 = graph()->NewNode(
          javascript()->CallWithArrayLike(p.frequency()), target, this_argument,
          arguments_list, context, frame_state, effect0, control0);

      // Lower to {JSCall} if {arguments_list} is either null or undefined.
      Node* effect1 = effect;
      Node* control1 =
          graph()->NewNode(common()->Merge(2), if_null, if_undefined);
      Node* value1 = effect1 = control1 =
          graph()->NewNode(javascript()->Call(2), target, this_argument,
                           context, frame_state, effect1, control1);

      // Rewire potential exception edges.
      Node* if_exception = nullptr;
      if (NodeProperties::IsExceptionalCall(node, &if_exception)) {
        // Create appropriate {IfException} and {IfSuccess} nodes.
        Node* if_exception0 =
            graph()->NewNode(common()->IfException(), control0, effect0);
        control0 = graph()->NewNode(common()->IfSuccess(), control0);
        Node* if_exception1 =
            graph()->NewNode(common()->IfException(), control1, effect1);
        control1 = graph()->NewNode(common()->IfSuccess(), control1);

        // Join the exception edges.
        Node* merge =
            graph()->NewNode(common()->Merge(2), if_exception0, if_exception1);
        Node* ephi = graph()->NewNode(common()->EffectPhi(2), if_exception0,
                                      if_exception1, merge);
        Node* phi =
            graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             if_exception0, if_exception1, merge);
        ReplaceWithValue(if_exception, phi, ephi, merge);
      }

      // Join control paths.
      control = graph()->NewNode(common()->Merge(2), control0, control1);
      effect =
          graph()->NewNode(common()->EffectPhi(2), effect0, effect1, control);
      Node* value =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           value0, value1, control);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }
  // Change {node} to the new {JSCall} operator.
  NodeProperties::ChangeOp(
      node,
      javascript()->Call(arity, p.frequency(), VectorSlotPair(), convert_mode));
  // Try to further reduce the JSCall {node}.
  Reduction const reduction = ReduceJSCall(node);
  return reduction.Changed() ? reduction : Changed(node);
}


// ES6 section 19.2.3.3 Function.prototype.call (thisArg, ...args)
Reduction JSCallReducer::ReduceFunctionPrototypeCall(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  Handle<JSFunction> call = Handle<JSFunction>::cast(
      HeapObjectMatcher(NodeProperties::GetValueInput(node, 0)).Value());
  // Change context of {node} to the Function.prototype.call context,
  // to ensure any exception is thrown in the correct context.
  NodeProperties::ReplaceContextInput(
      node, jsgraph()->HeapConstant(handle(call->context(), isolate())));
  // Remove the target from {node} and use the receiver as target instead, and
  // the thisArg becomes the new target.  If thisArg was not provided, insert
  // undefined instead.
  size_t arity = p.arity();
  DCHECK_LE(2u, arity);
  ConvertReceiverMode convert_mode;
  if (arity == 2) {
    // The thisArg was not provided, use undefined as receiver.
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
    node->ReplaceInput(0, node->InputAt(1));
    node->ReplaceInput(1, jsgraph()->UndefinedConstant());
  } else {
    // Just remove the target, which is the first value input.
    convert_mode = ConvertReceiverMode::kAny;
    node->RemoveInput(0);
    --arity;
  }
  NodeProperties::ChangeOp(
      node,
      javascript()->Call(arity, p.frequency(), VectorSlotPair(), convert_mode));
  // Try to further reduce the JSCall {node}.
  Reduction const reduction = ReduceJSCall(node);
  return reduction.Changed() ? reduction : Changed(node);
}

// ES6 section 19.2.3.6 Function.prototype [ @@hasInstance ] (V)
Reduction JSCallReducer::ReduceFunctionPrototypeHasInstance(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* object = (node->op()->ValueInputCount() >= 3)
                     ? NodeProperties::GetValueInput(node, 2)
                     : jsgraph()->UndefinedConstant();
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // TODO(turbofan): If JSOrdinaryToInstance raises an exception, the
  // stack trace doesn't contain the @@hasInstance call; we have the
  // corresponding bug in the baseline case. Some massaging of the frame
  // state would be necessary here.

  // Morph this {node} into a JSOrdinaryHasInstance node.
  node->ReplaceInput(0, receiver);
  node->ReplaceInput(1, object);
  node->ReplaceInput(2, context);
  node->ReplaceInput(3, frame_state);
  node->ReplaceInput(4, effect);
  node->ReplaceInput(5, control);
  node->TrimInputCount(6);
  NodeProperties::ChangeOp(node, javascript()->OrdinaryHasInstance());
  return Changed(node);
}

Reduction JSCallReducer::ReduceObjectGetPrototype(Node* node, Node* object) {
  Node* effect = NodeProperties::GetEffectInput(node);

  // Try to determine the {object} map.
  ZoneHandleSet<Map> object_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(object, effect, &object_maps);
  if (result != NodeProperties::kNoReceiverMaps) {
    Handle<Map> candidate_map = object_maps[0];
    Handle<Object> candidate_prototype(candidate_map->prototype(), isolate());

    // Check if we can constant-fold the {candidate_prototype}.
    for (size_t i = 0; i < object_maps.size(); ++i) {
      Handle<Map> object_map = object_maps[i];
      if (object_map->IsSpecialReceiverMap() ||
          object_map->has_hidden_prototype() ||
          object_map->prototype() != *candidate_prototype) {
        // We exclude special receivers, like JSProxy or API objects that
        // might require access checks here; we also don't want to deal
        // with hidden prototypes at this point.
        return NoChange();
      }
      // The above check also excludes maps for primitive values, which is
      // important because we are not applying [[ToObject]] here as expected.
      DCHECK(!object_map->IsPrimitiveMap() && object_map->IsJSReceiverMap());
      if (result == NodeProperties::kUnreliableReceiverMaps &&
          !object_map->is_stable()) {
        return NoChange();
      }
    }
    if (result == NodeProperties::kUnreliableReceiverMaps) {
      for (size_t i = 0; i < object_maps.size(); ++i) {
        dependencies()->AssumeMapStable(object_maps[i]);
      }
    }
    Node* value = jsgraph()->Constant(candidate_prototype);
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  return NoChange();
}

// ES6 section 19.1.2.11 Object.getPrototypeOf ( O )
Reduction JSCallReducer::ReduceObjectGetPrototypeOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* object = (node->op()->ValueInputCount() >= 3)
                     ? NodeProperties::GetValueInput(node, 2)
                     : jsgraph()->UndefinedConstant();
  return ReduceObjectGetPrototype(node, object);
}

// ES6 section B.2.2.1.1 get Object.prototype.__proto__
Reduction JSCallReducer::ReduceObjectPrototypeGetProto(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  return ReduceObjectGetPrototype(node, receiver);
}

// ES #sec-object.prototype.isprototypeof
Reduction JSCallReducer::ReduceObjectPrototypeIsPrototypeOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* value = node->op()->ValueInputCount() > 2
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->UndefinedConstant();
  Node* effect = NodeProperties::GetEffectInput(node);

  // Ensure that the {receiver} is known to be a JSReceiver (so that
  // the ToObject step of Object.prototype.isPrototypeOf is a no-op).
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result == NodeProperties::kNoReceiverMaps) return NoChange();
  for (size_t i = 0; i < receiver_maps.size(); ++i) {
    if (!receiver_maps[i]->IsJSReceiverMap()) return NoChange();
  }

  // We don't check whether {value} is a proper JSReceiver here explicitly,
  // and don't explicitly rule out Primitive {value}s, since all of them
  // have null as their prototype, so the prototype chain walk inside the
  // JSHasInPrototypeChain operator immediately aborts and yields false.
  NodeProperties::ReplaceValueInput(node, value, 0);
  NodeProperties::ReplaceValueInput(node, receiver, 1);
  for (int i = node->op()->ValueInputCount(); i-- > 2;) {
    node->RemoveInput(i);
  }
  NodeProperties::ChangeOp(node, javascript()->HasInPrototypeChain());
  return Changed(node);
}

// ES6 section 26.1.1 Reflect.apply ( target, thisArgument, argumentsList )
Reduction JSCallReducer::ReduceReflectApply(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  DCHECK_LE(0, arity);
  // Massage value inputs appropriately.
  node->RemoveInput(0);
  node->RemoveInput(0);
  while (arity < 3) {
    node->InsertInput(graph()->zone(), arity++, jsgraph()->UndefinedConstant());
  }
  while (arity-- > 3) {
    node->RemoveInput(arity);
  }
  NodeProperties::ChangeOp(node,
                           javascript()->CallWithArrayLike(p.frequency()));
  Reduction const reduction = ReduceJSCallWithArrayLike(node);
  return reduction.Changed() ? reduction : Changed(node);
}

// ES6 section 26.1.2 Reflect.construct ( target, argumentsList [, newTarget] )
Reduction JSCallReducer::ReduceReflectConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  DCHECK_LE(0, arity);
  // Massage value inputs appropriately.
  node->RemoveInput(0);
  node->RemoveInput(0);
  while (arity < 2) {
    node->InsertInput(graph()->zone(), arity++, jsgraph()->UndefinedConstant());
  }
  if (arity < 3) {
    node->InsertInput(graph()->zone(), arity++, node->InputAt(0));
  }
  while (arity-- > 3) {
    node->RemoveInput(arity);
  }
  NodeProperties::ChangeOp(node,
                           javascript()->ConstructWithArrayLike(p.frequency()));
  Reduction const reduction = ReduceJSConstructWithArrayLike(node);
  return reduction.Changed() ? reduction : Changed(node);
}

// ES6 section 26.1.7 Reflect.getPrototypeOf ( target )
Reduction JSCallReducer::ReduceReflectGetPrototypeOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* target = (node->op()->ValueInputCount() >= 3)
                     ? NodeProperties::GetValueInput(node, 2)
                     : jsgraph()->UndefinedConstant();
  return ReduceObjectGetPrototype(node, target);
}

bool CanInlineArrayIteratingBuiltin(Handle<Map> receiver_map) {
  Isolate* const isolate = receiver_map->GetIsolate();
  if (!receiver_map->prototype()->IsJSArray()) return false;
  Handle<JSArray> receiver_prototype(JSArray::cast(receiver_map->prototype()),
                                     isolate);
  return receiver_map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(receiver_map->elements_kind()) &&
         (!receiver_map->is_prototype_map() || receiver_map->is_stable()) &&
         isolate->IsFastArrayConstructorPrototypeChainIntact() &&
         isolate->IsAnyInitialArrayPrototype(receiver_prototype);
}

Reduction JSCallReducer::ReduceArrayForEach(Handle<JSFunction> function,
                                            Node* node) {
  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  CallParameters const& p = CallParametersOf(node->op());

  // Try to determine the {receiver} map.
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result != NodeProperties::kReliableReceiverMaps) {
    return NoChange();
  }
  if (receiver_maps.size() != 1) return NoChange();
  Handle<Map> receiver_map(receiver_maps[0]);
  ElementsKind kind = receiver_map->elements_kind();
  // TODO(danno): Handle double packed elements
  if (!IsFastElementsKind(kind) || IsDoubleElementsKind(kind) ||
      !CanInlineArrayIteratingBuiltin(receiver_map)) {
    return NoChange();
  }

  // TODO(danno): forEach can throw. Hook up exceptional edges.
  if (NodeProperties::IsExceptionalCall(node)) return NoChange();

  // Install code dependencies on the {receiver} prototype maps and the
  // global array protector cell.
  dependencies()->AssumePropertyCell(factory()->array_protector());

  Node* k = jsgraph()->ZeroConstant();

  Node* original_length = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS)),
      receiver, effect, control);

  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* vloop = k = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), k, k, loop);

  control = loop;
  effect = eloop;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  std::vector<Node*> checkpoint_params(
      {receiver, fncallback, this_arg, k, original_length});
  const int stack_parameters = static_cast<int>(checkpoint_params.size());

  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayForEachLoopEagerDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::EAGER);

  effect =
      graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);

  // Make sure the map hasn't changed during the iteration
  Node* orig_map = jsgraph()->HeapConstant(receiver_map);
  Node* array_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);
  Node* check_map =
      graph()->NewNode(simplified()->ReferenceEqual(), array_map, orig_map);
  effect =
      graph()->NewNode(simplified()->CheckIf(), check_map, effect, control);

  // Make sure that the access is still in bounds, since the callback could have
  // changed the array's size.
  Node* length = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS)),
      receiver, effect, control);
  k = effect =
      graph()->NewNode(simplified()->CheckBounds(), k, length, effect, control);

  // Reload the elements pointer before calling the callback, since the previous
  // callback might have resized the array causing the elements buffer to be
  // re-allocated.
  Node* elements = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
      effect, control);

  Node* element = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
      elements, k, effect, control);

  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->Constant(1));
  checkpoint_params[3] = next_k;

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), element,
                                   jsgraph()->TheHoleConstant());
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;
  }

  frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayForEachLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);

  control = effect = graph()->NewNode(
      javascript()->Call(5, p.frequency()), fncallback, this_arg, element, k,
      receiver, context, frame_state, effect, control);

  if (IsHoleyElementsKind(kind)) {
    Node* after_call_control = control;
    Node* after_call_effect = effect;
    control = hole_true;
    effect = effect_true;

    control = graph()->NewNode(common()->Merge(2), control, after_call_control);
    effect = graph()->NewNode(common()->EffectPhi(2), effect, after_call_effect,
                              control);
  }

  k = next_k;

  loop->ReplaceInput(1, control);
  vloop->ReplaceInput(1, k);
  eloop->ReplaceInput(1, effect);

  control = if_false;
  effect = eloop;

  ReplaceWithValue(node, jsgraph()->UndefinedConstant(), effect, control);
  return Replace(jsgraph()->UndefinedConstant());
}

Reduction JSCallReducer::ReduceArrayMap(Handle<JSFunction> function,
                                        Node* node) {
  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  CallParameters const& p = CallParametersOf(node->op());

  // Try to determine the {receiver} map.
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result != NodeProperties::kReliableReceiverMaps) {
    return NoChange();
  }
  if (receiver_maps.size() != 1) return NoChange();
  Handle<Map> receiver_map(receiver_maps[0]);
  ElementsKind kind = receiver_map->elements_kind();
  // TODO(danno): Handle holey Smi and Object fast elements kinds and double
  // packed.
  if (!IsFastPackedElementsKind(kind) || IsDoubleElementsKind(kind)) {
    return NoChange();
  }

  // TODO(danno): map can throw. Hook up exceptional edges.
  if (NodeProperties::IsExceptionalCall(node)) return NoChange();

  // We want the input to be a generic Array.
  const int map_index = Context::ArrayMapIndex(kind);
  Handle<JSFunction> handle_constructor(
      JSFunction::cast(
          Map::cast(native_context()->get(map_index))->GetConstructor()),
      isolate());
  Node* array_constructor = jsgraph()->HeapConstant(handle_constructor);
  if (receiver_map->prototype() !=
      native_context()->get(Context::INITIAL_ARRAY_PROTOTYPE_INDEX)) {
    return NoChange();
  }

  // And ensure that any changes to the Array species constructor cause deopt.
  if (!isolate()->IsArraySpeciesLookupChainIntact()) return NoChange();

  dependencies()->AssumePropertyCell(factory()->species_protector());

  Node* k = jsgraph()->ZeroConstant();
  Node* orig_map = jsgraph()->HeapConstant(receiver_map);

  // Make sure the map hasn't changed before we construct the output array.
  {
    Node* array_map = effect =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         receiver, effect, control);
    Node* check_map =
        graph()->NewNode(simplified()->ReferenceEqual(), array_map, orig_map);
    effect =
        graph()->NewNode(simplified()->CheckIf(), check_map, effect, control);
  }

  Node* original_length = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS)),
      receiver, effect, control);

  // This array should be HOLEY_SMI_ELEMENTS because of the non-zero length.
  Node* a = control = effect = graph()->NewNode(
      javascript()->CreateArray(1, Handle<AllocationSite>::null()),
      array_constructor, array_constructor, original_length, context,
      outer_frame_state, effect, control);

  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* vloop = k = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), k, k, loop);

  control = loop;
  effect = eloop;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  std::vector<Node*> checkpoint_params(
      {receiver, fncallback, this_arg, a, k, original_length});
  const int stack_parameters = static_cast<int>(checkpoint_params.size());

  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayMapLoopEagerDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::EAGER);

  effect =
      graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);

  // Make sure the map hasn't changed during the iteration
  Node* array_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);
  Node* check_map =
      graph()->NewNode(simplified()->ReferenceEqual(), array_map, orig_map);
  effect =
      graph()->NewNode(simplified()->CheckIf(), check_map, effect, control);

  // Make sure that the access is still in bounds, since the callback could have
  // changed the array's size.
  Node* length = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS)),
      receiver, effect, control);
  k = effect =
      graph()->NewNode(simplified()->CheckBounds(), k, length, effect, control);

  // Reload the elements pointer before calling the callback, since the previous
  // callback might have resized the array causing the elements buffer to be
  // re-allocated.
  Node* elements = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
      effect, control);

  Node* element = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
      elements, k, effect, control);

  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  // This frame state is dealt with by hand in
  // ArrayMapLoopLazyDeoptContinuation.
  frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayMapLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);

  Node* callback_value = control = effect = graph()->NewNode(
      javascript()->Call(5, p.frequency()), fncallback, this_arg, element, k,
      receiver, context, frame_state, effect, control);

  Handle<Map> double_map(Map::cast(
      native_context()->get(Context::ArrayMapIndex(HOLEY_DOUBLE_ELEMENTS))));
  Handle<Map> fast_map(
      Map::cast(native_context()->get(Context::ArrayMapIndex(HOLEY_ELEMENTS))));
  effect = graph()->NewNode(
      simplified()->TransitionAndStoreElement(double_map, fast_map), a, k,
      callback_value, effect, control);

  k = next_k;

  loop->ReplaceInput(1, control);
  vloop->ReplaceInput(1, k);
  eloop->ReplaceInput(1, effect);

  control = if_false;
  effect = eloop;

  ReplaceWithValue(node, a, effect, control);
  return Replace(a);
}

Reduction JSCallReducer::ReduceCallApiFunction(
    Node* node, Handle<FunctionTemplateInfo> function_template_info) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int const argc = static_cast<int>(p.arity()) - 2;
  Node* receiver = (p.convert_mode() == ConvertReceiverMode::kNullOrUndefined)
                       ? jsgraph()->HeapConstant(global_proxy())
                       : NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  // CallApiCallbackStub expects the target in a register, so we count it out,
  // and counts the receiver as an implicit argument, so we count the receiver
  // out too.
  if (argc > CallApiCallbackStub::kArgMax) return NoChange();

  // Infer the {receiver} maps, and check if we can inline the API function
  // callback based on those.
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result == NodeProperties::kNoReceiverMaps) return NoChange();
  for (size_t i = 0; i < receiver_maps.size(); ++i) {
    Handle<Map> receiver_map = receiver_maps[i];
    if (!receiver_map->IsJSObjectMap() ||
        (!function_template_info->accept_any_receiver() &&
         receiver_map->is_access_check_needed())) {
      return NoChange();
    }
    // In case of unreliable {receiver} information, the {receiver_maps}
    // must all be stable in order to consume the information.
    if (result == NodeProperties::kUnreliableReceiverMaps) {
      if (!receiver_map->is_stable()) return NoChange();
    }
  }

  // See if we can constant-fold the compatible receiver checks.
  CallOptimization call_optimization(function_template_info);
  if (!call_optimization.is_simple_api_call()) return NoChange();
  CallOptimization::HolderLookup lookup;
  Handle<JSObject> api_holder =
      call_optimization.LookupHolderOfExpectedType(receiver_maps[0], &lookup);
  if (lookup == CallOptimization::kHolderNotFound) return NoChange();
  for (size_t i = 1; i < receiver_maps.size(); ++i) {
    CallOptimization::HolderLookup lookupi;
    Handle<JSObject> holder = call_optimization.LookupHolderOfExpectedType(
        receiver_maps[i], &lookupi);
    if (lookup != lookupi) return NoChange();
    if (!api_holder.is_identical_to(holder)) return NoChange();
  }

  // Install stability dependencies for unreliable {receiver_maps}.
  if (result == NodeProperties::kUnreliableReceiverMaps) {
    for (size_t i = 0; i < receiver_maps.size(); ++i) {
      dependencies()->AssumeMapStable(receiver_maps[i]);
    }
  }

  // CallApiCallbackStub's register arguments: code, target, call data, holder,
  // function address.
  // TODO(turbofan): Consider introducing a JSCallApiCallback operator for
  // this and lower it during JSGenericLowering, and unify this with the
  // JSNativeContextSpecialization::InlineApiCall method a bit.
  Handle<CallHandlerInfo> call_handler_info(
      CallHandlerInfo::cast(function_template_info->call_code()), isolate());
  Handle<Object> data(call_handler_info->data(), isolate());
  CallApiCallbackStub stub(isolate(), argc, false);
  CallInterfaceDescriptor cid = stub.GetCallInterfaceDescriptor();
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), cid,
      cid.GetStackParameterCount() + argc + 1 /* implicit receiver */,
      CallDescriptor::kNeedsFrameState, Operator::kNoProperties,
      MachineType::AnyTagged(), 1);
  ApiFunction api_function(v8::ToCData<Address>(call_handler_info->callback()));
  Node* holder = lookup == CallOptimization::kHolderFound
                     ? jsgraph()->HeapConstant(api_holder)
                     : receiver;
  ExternalReference function_reference(
      &api_function, ExternalReference::DIRECT_API_CALL, isolate());
  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(stub.GetCode()));
  node->InsertInput(graph()->zone(), 2, jsgraph()->Constant(data));
  node->InsertInput(graph()->zone(), 3, holder);
  node->InsertInput(graph()->zone(), 4,
                    jsgraph()->ExternalConstant(function_reference));
  node->ReplaceInput(5, receiver);
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  return Changed(node);
}

namespace {

// Check whether elements aren't mutated; we play it extremely safe here by
// explicitly checking that {node} is only used by {LoadField} or {LoadElement}.
bool IsSafeArgumentsElements(Node* node) {
  for (Edge const edge : node->use_edges()) {
    if (!NodeProperties::IsValueEdge(edge)) continue;
    if (edge.from()->opcode() != IrOpcode::kLoadField &&
        edge.from()->opcode() != IrOpcode::kLoadElement) {
      return false;
    }
  }
  return true;
}

}  // namespace

Reduction JSCallReducer::ReduceCallOrConstructWithArrayLikeOrSpread(
    Node* node, int arity, CallFrequency const& frequency) {
  DCHECK(node->opcode() == IrOpcode::kJSCallWithArrayLike ||
         node->opcode() == IrOpcode::kJSCallWithSpread ||
         node->opcode() == IrOpcode::kJSConstructWithArrayLike ||
         node->opcode() == IrOpcode::kJSConstructWithSpread);

  // In case of a call/construct with spread, we need to
  // ensure that it's safe to avoid the actual iteration.
  if ((node->opcode() == IrOpcode::kJSCallWithSpread ||
       node->opcode() == IrOpcode::kJSConstructWithSpread) &&
      !isolate()->initial_array_iterator_prototype_map()->is_stable()) {
    return NoChange();
  }

  // Check if {arguments_list} is an arguments object, and {node} is the only
  // value user of {arguments_list} (except for value uses in frame states).
  Node* arguments_list = NodeProperties::GetValueInput(node, arity);
  if (arguments_list->opcode() != IrOpcode::kJSCreateArguments) {
    return NoChange();
  }
  for (Edge edge : arguments_list->use_edges()) {
    if (!NodeProperties::IsValueEdge(edge)) continue;
    Node* const user = edge.from();
    switch (user->opcode()) {
      case IrOpcode::kCheckMaps:
      case IrOpcode::kFrameState:
      case IrOpcode::kStateValues:
      case IrOpcode::kReferenceEqual:
      case IrOpcode::kReturn:
        // Ignore safe uses that definitely don't mess with the arguments.
        continue;
      case IrOpcode::kLoadField: {
        DCHECK_EQ(arguments_list, user->InputAt(0));
        FieldAccess const& access = FieldAccessOf(user->op());
        if (access.offset == JSArray::kLengthOffset) {
          // Ignore uses for arguments#length.
          STATIC_ASSERT(JSArray::kLengthOffset ==
                        JSArgumentsObject::kLengthOffset);
          continue;
        } else if (access.offset == JSObject::kElementsOffset) {
          // Ignore safe uses for arguments#elements.
          if (IsSafeArgumentsElements(user)) continue;
        }
        break;
      }
      case IrOpcode::kJSCallWithArrayLike:
        // Ignore uses as argumentsList input to calls with array like.
        if (user->InputAt(2) == arguments_list) continue;
        break;
      case IrOpcode::kJSConstructWithArrayLike:
        // Ignore uses as argumentsList input to calls with array like.
        if (user->InputAt(1) == arguments_list) continue;
        break;
      case IrOpcode::kJSCallWithSpread: {
        // Ignore uses as spread input to calls with spread.
        SpreadWithArityParameter p = SpreadWithArityParameterOf(user->op());
        int const arity = static_cast<int>(p.arity() - 1);
        if (user->InputAt(arity) == arguments_list) continue;
        break;
      }
      case IrOpcode::kJSConstructWithSpread: {
        // Ignore uses as spread input to construct with spread.
        SpreadWithArityParameter p = SpreadWithArityParameterOf(user->op());
        int const arity = static_cast<int>(p.arity() - 2);
        if (user->InputAt(arity) == arguments_list) continue;
        break;
      }
      default:
        break;
    }
    // We cannot currently reduce the {node} to something better than what
    // it already is, but we might be able to do something about the {node}
    // later, so put it on the waitlist and try again during finalization.
    waitlist_.insert(node);
    return NoChange();
  }

  // Get to the actual frame state from which to extract the arguments;
  // we can only optimize this in case the {node} was already inlined into
  // some other function (and same for the {arguments_list}).
  CreateArgumentsType const type = CreateArgumentsTypeOf(arguments_list->op());
  Node* frame_state = NodeProperties::GetFrameStateInput(arguments_list);
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);
  int start_index = 0;
  // Determine the formal parameter count;
  Handle<SharedFunctionInfo> shared;
  if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
  int formal_parameter_count = shared->internal_formal_parameter_count();
  if (type == CreateArgumentsType::kMappedArguments) {
    // Mapped arguments (sloppy mode) that are aliased can only be handled
    // here if there's no side-effect between the {node} and the {arg_array}.
    // TODO(turbofan): Further relax this constraint.
    if (formal_parameter_count != 0) {
      Node* effect = NodeProperties::GetEffectInput(node);
      while (effect != arguments_list) {
        if (effect->op()->EffectInputCount() != 1 ||
            !(effect->op()->properties() & Operator::kNoWrite)) {
          return NoChange();
        }
        effect = NodeProperties::GetEffectInput(effect);
      }
    }
  } else if (type == CreateArgumentsType::kRestParameter) {
    start_index = formal_parameter_count;

    // For spread calls/constructs with rest parameters we need to ensure that
    // the array iterator protector is intact, which guards that the rest
    // parameter iteration is not observable.
    if (node->opcode() == IrOpcode::kJSCallWithSpread ||
        node->opcode() == IrOpcode::kJSConstructWithSpread) {
      if (!isolate()->IsArrayIteratorLookupChainIntact()) return NoChange();
      dependencies()->AssumePropertyCell(factory()->array_iterator_protector());
    }
  }

  // For call/construct with spread, we need to also install a code
  // dependency on the initial %ArrayIteratorPrototype% map here to
  // ensure that no one messes with the next method.
  if (node->opcode() == IrOpcode::kJSCallWithSpread ||
      node->opcode() == IrOpcode::kJSConstructWithSpread) {
    dependencies()->AssumeMapStable(
        isolate()->initial_array_iterator_prototype_map());
  }

  // Remove the {arguments_list} input from the {node}.
  node->RemoveInput(arity--);
  // Check if are spreading to inlined arguments or to the arguments of
  // the outermost function.
  Node* outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    Operator const* op =
        (node->opcode() == IrOpcode::kJSCallWithArrayLike ||
         node->opcode() == IrOpcode::kJSCallWithSpread)
            ? javascript()->CallForwardVarargs(arity + 1, start_index)
            : javascript()->ConstructForwardVarargs(arity + 2, start_index);
    NodeProperties::ChangeOp(node, op);
    return Changed(node);
  }
  // Get to the actual frame state from which to extract the arguments;
  // we can only optimize this in case the {node} was already inlined into
  // some other function (and same for the {arg_array}).
  FrameStateInfo outer_info = OpParameter<FrameStateInfo>(outer_state);
  if (outer_info.type() == FrameStateType::kArgumentsAdaptor) {
    // Need to take the parameters from the arguments adaptor.
    frame_state = outer_state;
  }
  // Add the actual parameters to the {node}, skipping the receiver.
  Node* const parameters = frame_state->InputAt(kFrameStateParametersInput);
  for (int i = start_index + 1; i < parameters->InputCount(); ++i) {
    node->InsertInput(graph()->zone(), static_cast<int>(++arity),
                      parameters->InputAt(i));
  }

  if (node->opcode() == IrOpcode::kJSCallWithArrayLike ||
      node->opcode() == IrOpcode::kJSCallWithSpread) {
    NodeProperties::ChangeOp(node, javascript()->Call(arity + 1, frequency));
    Reduction const reduction = ReduceJSCall(node);
    return reduction.Changed() ? reduction : Changed(node);
  } else {
    NodeProperties::ChangeOp(node,
                             javascript()->Construct(arity + 2, frequency));
    Reduction const reduction = ReduceJSConstruct(node);
    return reduction.Changed() ? reduction : Changed(node);
  }
}

namespace {

bool ShouldUseCallICFeedback(Node* node) {
  HeapObjectMatcher m(node);
  if (m.HasValue() || m.IsJSCreateClosure()) {
    // Don't use CallIC feedback when we know the function
    // being called, i.e. either know the closure itself or
    // at least the SharedFunctionInfo.
    return false;
  } else if (m.IsPhi()) {
    // Protect against endless loops here.
    Node* control = NodeProperties::GetControlInput(node);
    if (control->opcode() == IrOpcode::kLoop) return false;
    // Check if {node} is a Phi of nodes which shouldn't
    // use CallIC feedback (not looking through loops).
    int const value_input_count = m.node()->op()->ValueInputCount();
    for (int n = 0; n < value_input_count; ++n) {
      if (ShouldUseCallICFeedback(node->InputAt(n))) return true;
    }
    return false;
  }
  return true;
}

}  // namespace

Reduction JSCallReducer::ReduceJSCall(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* control = NodeProperties::GetControlInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);

  // Try to specialize JSCall {node}s with constant {target}s.
  HeapObjectMatcher m(target);
  if (m.HasValue()) {
    if (m.Value()->IsJSFunction()) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
      Handle<SharedFunctionInfo> shared(function->shared(), isolate());

      // Raise a TypeError if the {target} is a "classConstructor".
      if (IsClassConstructor(shared->kind())) {
        NodeProperties::ReplaceValueInputs(node, target);
        NodeProperties::ChangeOp(
            node, javascript()->CallRuntime(
                      Runtime::kThrowConstructorNonCallableError, 1));
        return Changed(node);
      }

      // Don't inline cross native context.
      if (function->native_context() != *native_context()) return NoChange();

      // Check for known builtin functions.
      switch (shared->code()->builtin_index()) {
        case Builtins::kBooleanConstructor:
          return ReduceBooleanConstructor(node);
        case Builtins::kFunctionPrototypeApply:
          return ReduceFunctionPrototypeApply(node);
        case Builtins::kFunctionPrototypeCall:
          return ReduceFunctionPrototypeCall(node);
        case Builtins::kFunctionPrototypeHasInstance:
          return ReduceFunctionPrototypeHasInstance(node);
        case Builtins::kNumberConstructor:
          return ReduceNumberConstructor(node);
        case Builtins::kObjectGetPrototypeOf:
          return ReduceObjectGetPrototypeOf(node);
        case Builtins::kObjectPrototypeGetProto:
          return ReduceObjectPrototypeGetProto(node);
        case Builtins::kObjectPrototypeIsPrototypeOf:
          return ReduceObjectPrototypeIsPrototypeOf(node);
        case Builtins::kReflectApply:
          return ReduceReflectApply(node);
        case Builtins::kReflectConstruct:
          return ReduceReflectConstruct(node);
        case Builtins::kReflectGetPrototypeOf:
          return ReduceReflectGetPrototypeOf(node);
        case Builtins::kArrayForEach:
          return ReduceArrayForEach(function, node);
        case Builtins::kArrayMap:
          return ReduceArrayMap(function, node);
        case Builtins::kReturnReceiver:
          return ReduceReturnReceiver(node);
        default:
          break;
      }

      // Check for the Array constructor.
      if (*function == function->native_context()->array_function()) {
        return ReduceArrayConstructor(node);
      }

      if (!FLAG_runtime_stats && shared->IsApiFunction()) {
        Handle<FunctionTemplateInfo> function_template_info(
            FunctionTemplateInfo::cast(shared->function_data()), isolate());
        return ReduceCallApiFunction(node, function_template_info);
      }
    } else if (m.Value()->IsJSBoundFunction()) {
      Handle<JSBoundFunction> function =
          Handle<JSBoundFunction>::cast(m.Value());
      Handle<JSReceiver> bound_target_function(
          function->bound_target_function(), isolate());
      Handle<Object> bound_this(function->bound_this(), isolate());
      Handle<FixedArray> bound_arguments(function->bound_arguments(),
                                         isolate());
      CallParameters const& p = CallParametersOf(node->op());
      ConvertReceiverMode const convert_mode =
          (bound_this->IsNullOrUndefined(isolate()))
              ? ConvertReceiverMode::kNullOrUndefined
              : ConvertReceiverMode::kNotNullOrUndefined;
      size_t arity = p.arity();
      DCHECK_LE(2u, arity);
      // Patch {node} to use [[BoundTargetFunction]] and [[BoundThis]].
      NodeProperties::ReplaceValueInput(
          node, jsgraph()->Constant(bound_target_function), 0);
      NodeProperties::ReplaceValueInput(node, jsgraph()->Constant(bound_this),
                                        1);
      // Insert the [[BoundArguments]] for {node}.
      for (int i = 0; i < bound_arguments->length(); ++i) {
        node->InsertInput(
            graph()->zone(), i + 2,
            jsgraph()->Constant(handle(bound_arguments->get(i), isolate())));
        arity++;
      }
      NodeProperties::ChangeOp(
          node, javascript()->Call(arity, p.frequency(), VectorSlotPair(),
                                   convert_mode));
      // Try to further reduce the JSCall {node}.
      Reduction const reduction = ReduceJSCall(node);
      return reduction.Changed() ? reduction : Changed(node);
    }

    // Don't mess with other {node}s that have a constant {target}.
    // TODO(bmeurer): Also support proxies here.
    return NoChange();
  }

  // Extract feedback from the {node} using the CallICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
  if (nexus.IsUninitialized()) {
    if (flags() & kBailoutOnUninitialized) {
      // Introduce a SOFT deopt if the call {node} wasn't executed so far.
      return ReduceSoftDeoptimize(
          node, DeoptimizeReason::kInsufficientTypeFeedbackForCall);
    }
    return NoChange();
  }

  Handle<Object> feedback(nexus.GetFeedback(), isolate());
  if (feedback->IsAllocationSite()) {
    // Retrieve the Array function from the {node}.
    Node* array_function = jsgraph()->HeapConstant(
        handle(native_context()->array_function(), isolate()));

    // Check that the {target} is still the {array_function}.
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                   array_function);
    effect = graph()->NewNode(simplified()->CheckIf(), check, effect, control);

    // Turn the {node} into a {JSCreateArray} call.
    NodeProperties::ReplaceValueInput(node, array_function, 0);
    NodeProperties::ReplaceEffectInput(node, effect);
    return ReduceArrayConstructor(node);
  } else if (feedback->IsWeakCell()) {
    // Check if we want to use CallIC feedback here.
    if (!ShouldUseCallICFeedback(target)) return NoChange();

    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsJSFunction()) {
      Node* target_function =
          jsgraph()->Constant(handle(cell->value(), isolate()));

      // Check that the {target} is still the {target_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     target_function);
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);

      // Specialize the JSCall node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceEffectInput(node, effect);

      // Try to further reduce the JSCall {node}.
      Reduction const reduction = ReduceJSCall(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }
  return NoChange();
}

Reduction JSCallReducer::ReduceJSCallWithArrayLike(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallWithArrayLike, node->opcode());
  CallFrequency frequency = CallFrequencyOf(node->op());
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, 2, frequency);
}

Reduction JSCallReducer::ReduceJSCallWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallWithSpread, node->opcode());
  SpreadWithArityParameter const& p = SpreadWithArityParameterOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 1);

  // TODO(turbofan): Collect call counts on spread call/construct and thread it
  // through here.
  CallFrequency frequency;
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, arity, frequency);
}

Reduction JSCallReducer::ReduceJSConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstruct, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Try to specialize JSConstruct {node}s with constant {target}s.
  HeapObjectMatcher m(target);
  if (m.HasValue()) {
    if (m.Value()->IsJSFunction()) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());

      // Raise a TypeError if the {target} is not a constructor.
      if (!function->IsConstructor()) {
        NodeProperties::ReplaceValueInputs(node, target);
        NodeProperties::ChangeOp(
            node, javascript()->CallRuntime(
                      Runtime::kThrowConstructedNonConstructable));
        return Changed(node);
      }

      // Don't inline cross native context.
      if (function->native_context() != *native_context()) return NoChange();

      // Check for the ArrayConstructor.
      if (*function == function->native_context()->array_function()) {
        // Check if we have an allocation site.
        Handle<AllocationSite> site;
        if (p.feedback().IsValid()) {
          CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
          Handle<Object> feedback(nexus.GetFeedback(), isolate());
          if (feedback->IsAllocationSite()) {
            site = Handle<AllocationSite>::cast(feedback);
          }
        }

        // Turn the {node} into a {JSCreateArray} call.
        for (int i = arity; i > 0; --i) {
          NodeProperties::ReplaceValueInput(
              node, NodeProperties::GetValueInput(node, i), i + 1);
        }
        NodeProperties::ReplaceValueInput(node, new_target, 1);
        NodeProperties::ChangeOp(node, javascript()->CreateArray(arity, site));
        return Changed(node);
      }
    }

    // Don't mess with other {node}s that have a constant {target}.
    // TODO(bmeurer): Also support optimizing bound functions and proxies here.
    return NoChange();
  }

  // Extract feedback from the {node} using the CallICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
  if (nexus.IsUninitialized()) {
    if (flags() & kBailoutOnUninitialized) {
      // Introduce a SOFT deopt if the construct {node} wasn't executed so far.
      return ReduceSoftDeoptimize(
          node, DeoptimizeReason::kInsufficientTypeFeedbackForConstruct);
    }
    return NoChange();
  }

  Handle<Object> feedback(nexus.GetFeedback(), isolate());
  if (feedback->IsAllocationSite()) {
    // The feedback is an AllocationSite, which means we have called the
    // Array function and collected transition (and pretenuring) feedback
    // for the resulting arrays.  This has to be kept in sync with the
    // implementation of the CallConstructStub.
    Handle<AllocationSite> site = Handle<AllocationSite>::cast(feedback);

    // Retrieve the Array function from the {node}.
    Node* array_function = jsgraph()->HeapConstant(
        handle(native_context()->array_function(), isolate()));

    // Check that the {target} is still the {array_function}.
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                   array_function);
    effect = graph()->NewNode(simplified()->CheckIf(), check, effect, control);

    // Turn the {node} into a {JSCreateArray} call.
    NodeProperties::ReplaceEffectInput(node, effect);
    for (int i = arity; i > 0; --i) {
      NodeProperties::ReplaceValueInput(
          node, NodeProperties::GetValueInput(node, i), i + 1);
    }
    NodeProperties::ReplaceValueInput(node, new_target, 1);
    NodeProperties::ChangeOp(node, javascript()->CreateArray(arity, site));
    return Changed(node);
  } else if (feedback->IsWeakCell()) {
    // Check if we want to use CallIC feedback here.
    if (!ShouldUseCallICFeedback(target)) return NoChange();

    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsJSFunction()) {
      Node* target_function =
          jsgraph()->Constant(handle(cell->value(), isolate()));

      // Check that the {target} is still the {target_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     target_function);
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);

      // Specialize the JSConstruct node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceEffectInput(node, effect);
      if (target == new_target) {
        NodeProperties::ReplaceValueInput(node, target_function, arity + 1);
      }

      // Try to further reduce the JSConstruct {node}.
      Reduction const reduction = ReduceJSConstruct(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }

  return NoChange();
}

Reduction JSCallReducer::ReduceJSConstructWithArrayLike(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithArrayLike, node->opcode());
  CallFrequency frequency = CallFrequencyOf(node->op());
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, 1, frequency);
}

Reduction JSCallReducer::ReduceJSConstructWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithSpread, node->opcode());
  SpreadWithArityParameter const& p = SpreadWithArityParameterOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 2);

  // TODO(turbofan): Collect call counts on spread call/construct and thread it
  // through here.
  CallFrequency frequency;
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, arity, frequency);
}

Reduction JSCallReducer::ReduceReturnReceiver(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  ReplaceWithValue(node, receiver);
  return Replace(receiver);
}

Reduction JSCallReducer::ReduceSoftDeoptimize(Node* node,
                                              DeoptimizeReason reason) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::FindFrameStateBefore(node);
  Node* deoptimize =
      graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kSoft, reason),
                       frame_state, effect, control);
  // TODO(bmeurer): This should be on the AdvancedReducer somehow.
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
  Revisit(graph()->end());
  node->TrimInputCount(0);
  NodeProperties::ChangeOp(node, common()->Dead());
  return Changed(node);
}

Graph* JSCallReducer::graph() const { return jsgraph()->graph(); }

Isolate* JSCallReducer::isolate() const { return jsgraph()->isolate(); }

Factory* JSCallReducer::factory() const { return isolate()->factory(); }

Handle<JSGlobalProxy> JSCallReducer::global_proxy() const {
  return handle(JSGlobalProxy::cast(native_context()->global_proxy()),
                isolate());
}

CommonOperatorBuilder* JSCallReducer::common() const {
  return jsgraph()->common();
}

JSOperatorBuilder* JSCallReducer::javascript() const {
  return jsgraph()->javascript();
}

SimplifiedOperatorBuilder* JSCallReducer::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
