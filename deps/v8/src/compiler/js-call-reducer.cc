// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-call-reducer.h"

#include "src/api.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/allocation-builder.h"
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

  // Turn the {node} into a {JSCreateArray} call.
  DCHECK_LE(2u, p.arity());
  Handle<AllocationSite> site;
  size_t const arity = p.arity() - 2;
  NodeProperties::ReplaceValueInput(node, target, 0);
  NodeProperties::ReplaceValueInput(node, target, 1);
  NodeProperties::ChangeOp(node, javascript()->CreateArray(arity, site));
  return Changed(node);
}

// ES6 section 19.3.1.1 Boolean ( value )
Reduction JSCallReducer::ReduceBooleanConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());

  // Replace the {node} with a proper {ToBoolean} operator.
  DCHECK_LE(2u, p.arity());
  Node* value = (p.arity() == 2) ? jsgraph()->UndefinedConstant()
                                 : NodeProperties::GetValueInput(node, 2);
  value = graph()->NewNode(simplified()->ToBoolean(), value);
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

// ES section #sec-object-constructor
Reduction JSCallReducer::ReduceObjectConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.arity() < 3) return NoChange();
  Node* value = (p.arity() >= 3) ? NodeProperties::GetValueInput(node, 2)
                                 : jsgraph()->UndefinedConstant();
  Node* effect = NodeProperties::GetEffectInput(node);

  // We can fold away the Object(x) call if |x| is definitely not a primitive.
  if (NodeProperties::CanBePrimitive(value, effect)) {
    if (!NodeProperties::CanBeNullOrUndefined(value, effect)) {
      // Turn the {node} into a {JSToObject} call if we know that
      // the {value} cannot be null or undefined.
      NodeProperties::ReplaceValueInputs(node, value);
      NodeProperties::ChangeOp(node, javascript()->ToObject());
      return Changed(node);
    }
  } else {
    ReplaceWithValue(node, value);
    return Replace(node);
  }
  return NoChange();
}

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
    if (!NodeProperties::CanBeNullOrUndefined(arguments_list, effect)) {
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

// ES section #sec-function.prototype.bind
Reduction JSCallReducer::ReduceFunctionPrototypeBind(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  // Value inputs to the {node} are as follows:
  //
  //  - target, which is Function.prototype.bind JSFunction
  //  - receiver, which is the [[BoundTargetFunction]]
  //  - bound_this (optional), which is the [[BoundThis]]
  //  - and all the remaining value inouts are [[BoundArguments]]
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* bound_this = (node->op()->ValueInputCount() < 3)
                         ? jsgraph()->UndefinedConstant()
                         : NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Ensure that the {receiver} is known to be a JSBoundFunction or
  // a JSFunction with the same [[Prototype]], and all maps we've
  // seen for the {receiver} so far indicate that {receiver} is
  // definitely a constructor or not a constructor.
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result == NodeProperties::kNoReceiverMaps) return NoChange();
  DCHECK_NE(0, receiver_maps.size());
  bool const is_constructor = receiver_maps[0]->is_constructor();
  Handle<Object> const prototype(receiver_maps[0]->prototype(), isolate());
  for (Handle<Map> const receiver_map : receiver_maps) {
    // Check for consistency among the {receiver_maps}.
    STATIC_ASSERT(LAST_TYPE == LAST_FUNCTION_TYPE);
    if (receiver_map->prototype() != *prototype) return NoChange();
    if (receiver_map->is_constructor() != is_constructor) return NoChange();
    if (receiver_map->instance_type() < FIRST_FUNCTION_TYPE) return NoChange();

    // Disallow binding of slow-mode functions. We need to figure out
    // whether the length and name property are in the original state.
    if (receiver_map->is_dictionary_map()) return NoChange();

    // Check whether the length and name properties are still present
    // as AccessorInfo objects. In that case, their values can be
    // recomputed even if the actual value of the object changes.
    // This mirrors the checks done in builtins-function-gen.cc at
    // runtime otherwise.
    Handle<DescriptorArray> descriptors(receiver_map->instance_descriptors(),
                                        isolate());
    if (descriptors->length() < 2) return NoChange();
    if (descriptors->GetKey(JSFunction::kLengthDescriptorIndex) !=
        isolate()->heap()->length_string()) {
      return NoChange();
    }
    if (!descriptors->GetValue(JSFunction::kLengthDescriptorIndex)
             ->IsAccessorInfo()) {
      return NoChange();
    }
    if (descriptors->GetKey(JSFunction::kNameDescriptorIndex) !=
        isolate()->heap()->name_string()) {
      return NoChange();
    }
    if (!descriptors->GetValue(JSFunction::kNameDescriptorIndex)
             ->IsAccessorInfo()) {
      return NoChange();
    }
  }

  // Setup the map for the resulting JSBoundFunction with the
  // correct instance {prototype}.
  Handle<Map> map(
      is_constructor
          ? native_context()->bound_function_with_constructor_map()
          : native_context()->bound_function_without_constructor_map(),
      isolate());
  if (map->prototype() != *prototype) {
    map = Map::TransitionToPrototype(map, prototype);
  }

  // Make sure we can rely on the {receiver_maps}.
  if (result == NodeProperties::kUnreliableReceiverMaps) {
    effect = graph()->NewNode(
        simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps), receiver,
        effect, control);
  }

  // Replace the {node} with a JSCreateBoundFunction.
  int const arity = std::max(0, node->op()->ValueInputCount() - 3);
  int const input_count = 2 + arity + 3;
  Node** inputs = graph()->zone()->NewArray<Node*>(input_count);
  inputs[0] = receiver;
  inputs[1] = bound_this;
  for (int i = 0; i < arity; ++i) {
    inputs[2 + i] = NodeProperties::GetValueInput(node, 3 + i);
  }
  inputs[2 + arity + 0] = context;
  inputs[2 + arity + 1] = effect;
  inputs[2 + arity + 2] = control;
  Node* value = effect = graph()->NewNode(
      javascript()->CreateBoundFunction(arity, map), input_count, inputs);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
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

// ES section #sec-object.is
Reduction JSCallReducer::ReduceObjectIs(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& params = CallParametersOf(node->op());
  int const argc = static_cast<int>(params.arity() - 2);
  Node* lhs = (argc >= 1) ? NodeProperties::GetValueInput(node, 2)
                          : jsgraph()->UndefinedConstant();
  Node* rhs = (argc >= 2) ? NodeProperties::GetValueInput(node, 3)
                          : jsgraph()->UndefinedConstant();
  Node* value = graph()->NewNode(simplified()->SameValue(), lhs, rhs);
  ReplaceWithValue(node, value);
  return Replace(value);
}

// ES6 section B.2.2.1.1 get Object.prototype.__proto__
Reduction JSCallReducer::ReduceObjectPrototypeGetProto(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  return ReduceObjectGetPrototype(node, receiver);
}

// ES #sec-object.prototype.hasownproperty
Reduction JSCallReducer::ReduceObjectPrototypeHasOwnProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& params = CallParametersOf(node->op());
  int const argc = static_cast<int>(params.arity() - 2);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* name = (argc >= 1) ? NodeProperties::GetValueInput(node, 2)
                           : jsgraph()->UndefinedConstant();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // We can optimize a call to Object.prototype.hasOwnProperty if it's being
  // used inside a fast-mode for..in, so for code like this:
  //
  //   for (name in receiver) {
  //     if (receiver.hasOwnProperty(name)) {
  //        ...
  //     }
  //   }
  //
  // If the for..in is in fast-mode, we know that the {receiver} has {name}
  // as own property, otherwise the enumeration wouldn't include it. The graph
  // constructed by the BytecodeGraphBuilder in this case looks like this:

  // receiver
  //  ^    ^
  //  |    |
  //  |    +-+
  //  |      |
  //  |   JSToObject
  //  |      ^
  //  |      |
  //  |   JSForInNext
  //  |      ^
  //  +----+ |
  //       | |
  //  JSCall[hasOwnProperty]

  // We can constant-fold the {node} to True in this case, and insert
  // a (potentially redundant) map check to guard the fact that the
  // {receiver} map didn't change since the dominating JSForInNext. This
  // map check is only necessary when TurboFan cannot prove that there
  // is no observable side effect between the {JSForInNext} and the
  // {JSCall} to Object.prototype.hasOwnProperty.
  //
  // Also note that it's safe to look through the {JSToObject}, since the
  // Object.prototype.hasOwnProperty does an implicit ToObject anyway, and
  // these operations are not observable.
  if (name->opcode() == IrOpcode::kJSForInNext) {
    ForInMode const mode = ForInModeOf(name->op());
    if (mode != ForInMode::kGeneric) {
      Node* object = NodeProperties::GetValueInput(name, 0);
      Node* cache_type = NodeProperties::GetValueInput(name, 2);
      if (object->opcode() == IrOpcode::kJSToObject) {
        object = NodeProperties::GetValueInput(object, 0);
      }
      if (object == receiver) {
        // No need to repeat the map check if we can prove that there's no
        // observable side effect between {effect} and {name].
        if (!NodeProperties::NoObservableSideEffectBetween(effect, name)) {
          Node* receiver_map = effect =
              graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                               receiver, effect, control);
          Node* check = graph()->NewNode(simplified()->ReferenceEqual(),
                                         receiver_map, cache_type);
          effect = graph()->NewNode(
              simplified()->CheckIf(DeoptimizeReason::kNoReason), check, effect,
              control);
        }
        Node* value = jsgraph()->TrueConstant();
        ReplaceWithValue(node, value, effect, control);
        return Replace(value);
      }
    }
  }

  return NoChange();
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

// ES section #sec-reflect.get
Reduction JSCallReducer::ReduceReflectGet(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  if (arity != 2) return NoChange();
  Node* target = NodeProperties::GetValueInput(node, 2);
  Node* key = NodeProperties::GetValueInput(node, 3);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check whether {target} is a JSReceiver.
  Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), target);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // Throw an appropriate TypeError if the {target} is not a JSReceiver.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  {
    if_false = efalse = graph()->NewNode(
        javascript()->CallRuntime(Runtime::kThrowTypeError, 2),
        jsgraph()->Constant(MessageTemplate::kCalledOnNonObject),
        jsgraph()->HeapConstant(
            factory()->NewStringFromAsciiChecked("Reflect.get")),
        context, frame_state, efalse, if_false);
  }

  // Otherwise just use the existing GetPropertyStub.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue;
  {
    Callable callable = CodeFactory::GetProperty(isolate());
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState, Operator::kNoProperties,
        MachineType::AnyTagged(), 1);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    vtrue = etrue = if_true =
        graph()->NewNode(common()->Call(desc), stub_code, target, key, context,
                         frame_state, etrue, if_true);
  }

  // Rewire potential exception edges.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    // Create appropriate {IfException} and {IfSuccess} nodes.
    Node* extrue = graph()->NewNode(common()->IfException(), etrue, if_true);
    if_true = graph()->NewNode(common()->IfSuccess(), if_true);
    Node* exfalse = graph()->NewNode(common()->IfException(), efalse, if_false);
    if_false = graph()->NewNode(common()->IfSuccess(), if_false);

    // Join the exception edges.
    Node* merge = graph()->NewNode(common()->Merge(2), extrue, exfalse);
    Node* ephi =
        graph()->NewNode(common()->EffectPhi(2), extrue, exfalse, merge);
    Node* phi =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         extrue, exfalse, merge);
    ReplaceWithValue(on_exception, phi, ephi, merge);
  }

  // Connect the throwing path to end.
  if_false = graph()->NewNode(common()->Throw(), efalse, if_false);
  NodeProperties::MergeControlToEnd(graph(), common(), if_false);

  // Continue on the regular path.
  ReplaceWithValue(node, vtrue, etrue, if_true);
  return Changed(vtrue);
}

// ES section #sec-reflect.has
Reduction JSCallReducer::ReduceReflectHas(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  DCHECK_LE(0, arity);
  Node* target = (arity >= 1) ? NodeProperties::GetValueInput(node, 2)
                              : jsgraph()->UndefinedConstant();
  Node* key = (arity >= 2) ? NodeProperties::GetValueInput(node, 3)
                           : jsgraph()->UndefinedConstant();
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check whether {target} is a JSReceiver.
  Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), target);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  // Throw an appropriate TypeError if the {target} is not a JSReceiver.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  {
    if_false = efalse = graph()->NewNode(
        javascript()->CallRuntime(Runtime::kThrowTypeError, 2),
        jsgraph()->Constant(MessageTemplate::kCalledOnNonObject),
        jsgraph()->HeapConstant(
            factory()->NewStringFromAsciiChecked("Reflect.has")),
        context, frame_state, efalse, if_false);
  }

  // Otherwise just use the existing {JSHasProperty} logic.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue;
  {
    vtrue = etrue = if_true =
        graph()->NewNode(javascript()->HasProperty(), key, target, context,
                         frame_state, etrue, if_true);
  }

  // Rewire potential exception edges.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    // Create appropriate {IfException} and {IfSuccess} nodes.
    Node* extrue = graph()->NewNode(common()->IfException(), etrue, if_true);
    if_true = graph()->NewNode(common()->IfSuccess(), if_true);
    Node* exfalse = graph()->NewNode(common()->IfException(), efalse, if_false);
    if_false = graph()->NewNode(common()->IfSuccess(), if_false);

    // Join the exception edges.
    Node* merge = graph()->NewNode(common()->Merge(2), extrue, exfalse);
    Node* ephi =
        graph()->NewNode(common()->EffectPhi(2), extrue, exfalse, merge);
    Node* phi =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         extrue, exfalse, merge);
    ReplaceWithValue(on_exception, phi, ephi, merge);
  }

  // Connect the throwing path to end.
  if_false = graph()->NewNode(common()->Throw(), efalse, if_false);
  NodeProperties::MergeControlToEnd(graph(), common(), if_false);

  // Continue on the regular path.
  ReplaceWithValue(node, vtrue, etrue, if_true);
  return Changed(vtrue);
}

bool CanInlineArrayIteratingBuiltin(Handle<Map> receiver_map) {
  Isolate* const isolate = receiver_map->GetIsolate();
  if (!receiver_map->prototype()->IsJSArray()) return false;
  Handle<JSArray> receiver_prototype(JSArray::cast(receiver_map->prototype()),
                                     isolate);
  return receiver_map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(receiver_map->elements_kind()) &&
         (!receiver_map->is_prototype_map() || receiver_map->is_stable()) &&
         isolate->IsNoElementsProtectorIntact() &&
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
  if (receiver_maps.size() == 0) return NoChange();

  ElementsKind kind = IsDoubleElementsKind(receiver_maps[0]->elements_kind())
                          ? PACKED_DOUBLE_ELEMENTS
                          : PACKED_ELEMENTS;
  for (Handle<Map> receiver_map : receiver_maps) {
    ElementsKind next_kind = receiver_map->elements_kind();
    if (!CanInlineArrayIteratingBuiltin(receiver_map)) {
      return NoChange();
    }
    if (!IsFastElementsKind(next_kind) ||
        (IsDoubleElementsKind(next_kind) && IsHoleyElementsKind(next_kind))) {
      return NoChange();
    }
    if (IsDoubleElementsKind(kind) != IsDoubleElementsKind(next_kind)) {
      return NoChange();
    }
    if (IsHoleyElementsKind(next_kind)) {
      kind = HOLEY_ELEMENTS;
    }
  }

  // Install code dependencies on the {receiver} prototype maps and the
  // global array protector cell.
  dependencies()->AssumePropertyCell(factory()->no_elements_protector());

  Node* k = jsgraph()->ZeroConstant();

  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS)),
      receiver, effect, control);

  std::vector<Node*> checkpoint_params(
      {receiver, fncallback, this_arg, k, original_length});
  const int stack_parameters = static_cast<int>(checkpoint_params.size());

  // Check whether the given callback function is callable. Note that this has
  // to happen outside the loop to make sure we also throw on empty arrays.
  Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayForEachLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  WireInCallbackIsCallableCheck(fncallback, context, check_frame_state, effect,
                                &control, &check_fail, &check_throw);

  // Start the loop.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
  NodeProperties::MergeControlToEnd(graph(), common(), terminate);
  Node* vloop = k = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), k, k, loop);
  checkpoint_params[3] = k;

  control = loop;
  effect = eloop;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayForEachLoopEagerDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::EAGER);

  effect =
      graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);

  // Make sure the map hasn't changed during the iteration
  effect = graph()->NewNode(
      simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps), receiver,
      effect, control);

  Node* element = SafeLoadElement(kind, receiver, control, &effect, &k);

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

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = graph()->NewNode(common()->TypeGuard(Type::NonInternal()),
                               element, control);
  }

  frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayForEachLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);

  control = effect = graph()->NewNode(
      javascript()->Call(5, p.frequency()), fncallback, this_arg, element, k,
      receiver, context, frame_state, effect, control);

  // Rewire potential exception edges.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    RewirePostCallbackExceptionEdges(check_throw, on_exception, effect,
                                     &check_fail, &control);
  }

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

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the successful
  // completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

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

  // Ensure that any changes to the Array species constructor cause deopt.
  if (!isolate()->IsArraySpeciesLookupChainIntact()) return NoChange();

  if (receiver_maps.size() == 0) return NoChange();

  const ElementsKind kind = receiver_maps[0]->elements_kind();

  // TODO(danno): Handle holey elements kinds.
  if (!IsFastPackedElementsKind(kind)) {
    return NoChange();
  }

  for (Handle<Map> receiver_map : receiver_maps) {
    if (!CanInlineArrayIteratingBuiltin(receiver_map)) {
      return NoChange();
    }
    // We can handle different maps, as long as their elements kind are the
    // same.
    if (receiver_map->elements_kind() != kind) {
      return NoChange();
    }
  }

  dependencies()->AssumePropertyCell(factory()->species_protector());

  Handle<JSFunction> handle_constructor(
      JSFunction::cast(
          native_context()->GetInitialJSArrayMap(kind)->GetConstructor()),
      isolate());
  Node* array_constructor = jsgraph()->HeapConstant(handle_constructor);


  Node* k = jsgraph()->ZeroConstant();

  // Make sure the map hasn't changed before we construct the output array.
  effect = graph()->NewNode(
      simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps), receiver,
      effect, control);

  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      effect, control);

  // This array should be HOLEY_SMI_ELEMENTS because of the non-zero length.
  // Even though {JSCreateArray} is not marked as {kNoThrow}, we can elide the
  // exceptional projections because it cannot throw with the given parameters.
  Node* a = control = effect = graph()->NewNode(
      javascript()->CreateArray(1, Handle<AllocationSite>::null()),
      array_constructor, array_constructor, original_length, context,
      outer_frame_state, effect, control);

  std::vector<Node*> checkpoint_params(
      {receiver, fncallback, this_arg, a, k, original_length});
  const int stack_parameters = static_cast<int>(checkpoint_params.size());

  // Check whether the given callback function is callable. Note that this has
  // to happen outside the loop to make sure we also throw on empty arrays.
  Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayMapLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  WireInCallbackIsCallableCheck(fncallback, context, check_frame_state, effect,
                                &control, &check_fail, &check_throw);

  // Start the loop.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
  NodeProperties::MergeControlToEnd(graph(), common(), terminate);
  Node* vloop = k = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), k, k, loop);
  checkpoint_params[4] = k;

  control = loop;
  effect = eloop;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), function, Builtins::kArrayMapLoopEagerDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::EAGER);

  effect =
      graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);

  // Make sure the map hasn't changed during the iteration
  effect = graph()->NewNode(
      simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps), receiver,
      effect, control);

  Node* element = SafeLoadElement(kind, receiver, control, &effect, &k);

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

  // Rewire potential exception edges.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    RewirePostCallbackExceptionEdges(check_throw, on_exception, effect,
                                     &check_fail, &control);
  }

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

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the successful
  // completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, a, effect, control);
  return Replace(a);
}

Reduction JSCallReducer::ReduceArrayFilter(Handle<JSFunction> function,
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

  // And ensure that any changes to the Array species constructor cause deopt.
  if (!isolate()->IsArraySpeciesLookupChainIntact()) return NoChange();

  if (receiver_maps.size() == 0) return NoChange();

  const ElementsKind kind = receiver_maps[0]->elements_kind();

  // TODO(danno): Handle holey elements kinds.
  if (!IsFastPackedElementsKind(kind)) {
    return NoChange();
  }

  for (Handle<Map> receiver_map : receiver_maps) {
    if (!CanInlineArrayIteratingBuiltin(receiver_map)) {
      return NoChange();
    }
    // We can handle different maps, as long as their elements kind are the
    // same.
    if (receiver_map->elements_kind() != kind) {
      return NoChange();
    }
  }

  dependencies()->AssumePropertyCell(factory()->species_protector());

  Handle<Map> initial_map(
      Map::cast(native_context()->GetInitialJSArrayMap(kind)));

  Node* k = jsgraph()->ZeroConstant();
  Node* to = jsgraph()->ZeroConstant();

  // Make sure the map hasn't changed before we construct the output array.
  effect = graph()->NewNode(
      simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps), receiver,
      effect, control);

  Node* a;  // Construct the output array.
  {
    AllocationBuilder ab(jsgraph(), effect, control);
    ab.Allocate(initial_map->instance_size(), NOT_TENURED, Type::Array());
    ab.Store(AccessBuilder::ForMap(), initial_map);
    Node* empty_fixed_array = jsgraph()->EmptyFixedArrayConstant();
    ab.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), empty_fixed_array);
    ab.Store(AccessBuilder::ForJSObjectElements(), empty_fixed_array);
    ab.Store(AccessBuilder::ForJSArrayLength(kind), jsgraph()->ZeroConstant());
    for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
      ab.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
               jsgraph()->UndefinedConstant());
    }
    a = effect = ab.Finish();
  }

  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      effect, control);

  // Check whether the given callback function is callable. Note that this has
  // to happen outside the loop to make sure we also throw on empty arrays.
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  {
    // This frame state doesn't ever call the deopt continuation, it's only
    // necessary to specifiy a continuation in order to handle the exceptional
    // case. We don't have all the values available to completely fill out
    // checkpoint_params yet, but that's okay because it'll never be called.
    // Therefore, "to" is mentioned twice, once standing in for the k_value
    // value.
    std::vector<Node*> checkpoint_params(
        {receiver, fncallback, this_arg, a, k, original_length, to, to});
    const int stack_parameters = static_cast<int>(checkpoint_params.size());

    Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), function, Builtins::kArrayFilterLoopLazyDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::LAZY);
    WireInCallbackIsCallableCheck(fncallback, context, check_frame_state,
                                  effect, &control, &check_fail, &check_throw);
  }

  // Start the loop.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
  NodeProperties::MergeControlToEnd(graph(), common(), terminate);
  Node* vloop = k = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), k, k, loop);
  Node* v_to_loop = to = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTaggedSigned, 2), to, to, loop);

  control = loop;
  effect = eloop;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    std::vector<Node*> checkpoint_params(
        {receiver, fncallback, this_arg, a, k, original_length, to});
    const int stack_parameters = static_cast<int>(checkpoint_params.size());

    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), function, Builtins::kArrayFilterLoopEagerDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);

    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Make sure the map hasn't changed during the iteration.
  effect = graph()->NewNode(
      simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps), receiver,
      effect, control);

  Node* element = SafeLoadElement(kind, receiver, control, &effect, &k);

  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  Node* callback_value = nullptr;
  {
    // This frame state is dealt with by hand in
    // Builtins::kArrayFilterLoopLazyDeoptContinuation.
    std::vector<Node*> checkpoint_params(
        {receiver, fncallback, this_arg, a, k, original_length, element, to});
    const int stack_parameters = static_cast<int>(checkpoint_params.size());

    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), function, Builtins::kArrayFilterLoopLazyDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::LAZY);

    callback_value = control = effect = graph()->NewNode(
        javascript()->Call(5, p.frequency()), fncallback, this_arg, element, k,
        receiver, context, frame_state, effect, control);
  }

  // Rewire potential exception edges.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    RewirePostCallbackExceptionEdges(check_throw, on_exception, effect,
                                     &check_fail, &control);
  }

  // We need an eager frame state for right after the callback function
  // returned, just in case an attempt to grow the output array fails.
  //
  // Note that we are intentionally reusing the
  // Builtins::kArrayFilterLoopLazyDeoptContinuation as an *eager* entry
  // point in this case. This is safe, because re-evaluating a [ToBoolean]
  // coercion is safe.
  {
    std::vector<Node*> checkpoint_params({receiver, fncallback, this_arg, a, k,
                                          original_length, element, to,
                                          callback_value});
    const int stack_parameters = static_cast<int>(checkpoint_params.size());
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), function, Builtins::kArrayFilterLoopLazyDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);

    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // We have to coerce callback_value to boolean, and only store the element in
  // a if it's true. The checkpoint above protects against the case that
  // growing {a} fails.
  to = DoFilterPostCallbackWork(kind, &control, &effect, a, to, element,
                                callback_value);
  k = next_k;

  loop->ReplaceInput(1, control);
  vloop->ReplaceInput(1, k);
  v_to_loop->ReplaceInput(1, to);
  eloop->ReplaceInput(1, effect);

  control = if_false;
  effect = eloop;

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the successful
  // completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, a, effect, control);
  return Replace(a);
}

Node* JSCallReducer::DoFilterPostCallbackWork(ElementsKind kind, Node** control,
                                              Node** effect, Node* a, Node* to,
                                              Node* element,
                                              Node* callback_value) {
  Node* boolean_result =
      graph()->NewNode(simplified()->ToBoolean(), callback_value);

  Node* check_boolean_result =
      graph()->NewNode(simplified()->ReferenceEqual(), boolean_result,
                       jsgraph()->TrueConstant());
  Node* boolean_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          check_boolean_result, *control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), boolean_branch);
  Node* etrue = *effect;
  Node* vtrue;
  {
    // Load the elements backing store of the {receiver}.
    Node* elements = etrue = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectElements()), a, etrue,
        if_true);

    // We know that {to} is in Unsigned31 range here, being smaller than
    // {original_length} at all times.
    Node* checked_to =
        graph()->NewNode(common()->TypeGuard(Type::Unsigned31()), to, if_true);
    Node* elements_length = etrue = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForFixedArrayLength()), elements,
        etrue, if_true);

    GrowFastElementsMode mode =
        IsDoubleElementsKind(kind) ? GrowFastElementsMode::kDoubleElements
                                   : GrowFastElementsMode::kSmiOrObjectElements;
    elements = etrue =
        graph()->NewNode(simplified()->MaybeGrowFastElements(mode), a, elements,
                         checked_to, elements_length, etrue, if_true);

    // Update the length of {a}.
    Node* new_length_a = graph()->NewNode(simplified()->NumberAdd(), checked_to,
                                          jsgraph()->OneConstant());

    etrue = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSArrayLength(kind)), a,
        new_length_a, etrue, if_true);

    // Append the value to the {elements}.
    etrue = graph()->NewNode(
        simplified()->StoreElement(AccessBuilder::ForFixedArrayElement(kind)),
        elements, checked_to, element, etrue, if_true);

    vtrue = new_length_a;
  }

  Node* if_false = graph()->NewNode(common()->IfFalse(), boolean_branch);
  Node* efalse = *effect;
  Node* vfalse = to;

  *control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  *effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, *control);
  to = graph()->NewNode(common()->Phi(MachineRepresentation::kTaggedSigned, 2),
                        vtrue, vfalse, *control);
  return to;
}

void JSCallReducer::WireInCallbackIsCallableCheck(
    Node* fncallback, Node* context, Node* check_frame_state, Node* effect,
    Node** control, Node** check_fail, Node** check_throw) {
  Node* check = graph()->NewNode(simplified()->ObjectIsCallable(), fncallback);
  Node* check_branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, *control);
  *check_fail = graph()->NewNode(common()->IfFalse(), check_branch);
  *check_throw = *check_fail = graph()->NewNode(
      javascript()->CallRuntime(Runtime::kThrowTypeError, 2),
      jsgraph()->Constant(MessageTemplate::kCalledNonCallable), fncallback,
      context, check_frame_state, effect, *check_fail);
  *control = graph()->NewNode(common()->IfTrue(), check_branch);
}

void JSCallReducer::RewirePostCallbackExceptionEdges(Node* check_throw,
                                                     Node* on_exception,
                                                     Node* effect,
                                                     Node** check_fail,
                                                     Node** control) {
  // Create appropriate {IfException} and {IfSuccess} nodes.
  Node* if_exception0 =
      graph()->NewNode(common()->IfException(), check_throw, *check_fail);
  *check_fail = graph()->NewNode(common()->IfSuccess(), *check_fail);
  Node* if_exception1 =
      graph()->NewNode(common()->IfException(), effect, *control);
  *control = graph()->NewNode(common()->IfSuccess(), *control);

  // Join the exception edges.
  Node* merge =
      graph()->NewNode(common()->Merge(2), if_exception0, if_exception1);
  Node* ephi = graph()->NewNode(common()->EffectPhi(2), if_exception0,
                                if_exception1, merge);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               if_exception0, if_exception1, merge);
  ReplaceWithValue(on_exception, phi, ephi, merge);
}

Node* JSCallReducer::SafeLoadElement(ElementsKind kind, Node* receiver,
                                     Node* control, Node** effect, Node** k) {
  // Make sure that the access is still in bounds, since the callback could have
  // changed the array's size.
  Node* length = *effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      *effect, control);
  *k = *effect = graph()->NewNode(simplified()->CheckBounds(), *k, length,
                                  *effect, control);

  // Reload the elements pointer before calling the callback, since the previous
  // callback might have resized the array causing the elements buffer to be
  // re-allocated.
  Node* elements = *effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
      *effect, control);

  Node* masked_index =
      graph()->NewNode(simplified()->MaskIndexWithBound(), *k, length);

  Node* element = *effect = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement(kind)),
      elements, masked_index, *effect, control);
  return element;
}

Reduction JSCallReducer::ReduceCallApiFunction(Node* node,
                                               Handle<JSFunction> function) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int const argc = static_cast<int>(p.arity()) - 2;
  Node* receiver = (p.convert_mode() == ConvertReceiverMode::kNullOrUndefined)
                       ? jsgraph()->HeapConstant(global_proxy())
                       : NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  Handle<FunctionTemplateInfo> function_template_info(
      FunctionTemplateInfo::cast(function->shared()->function_data()));
  Handle<Context> context(function->context());

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
  CallApiCallbackStub stub(isolate(), argc);
  CallInterfaceDescriptor cid = stub.GetCallInterfaceDescriptor();
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), cid,
      cid.GetStackParameterCount() + argc + 1 /* implicit receiver */,
      CallDescriptor::kNeedsFrameState, Operator::kNoProperties,
      MachineType::AnyTagged(), 1, Linkage::kNoContext);
  ApiFunction api_function(v8::ToCData<Address>(call_handler_info->callback()));
  Node* holder = lookup == CallOptimization::kHolderFound
                     ? jsgraph()->HeapConstant(api_holder)
                     : receiver;
  ExternalReference function_reference(
      &api_function, ExternalReference::DIRECT_API_CALL, isolate());
  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(stub.GetCode()));
  node->ReplaceInput(1, jsgraph()->Constant(context));
  node->InsertInput(graph()->zone(), 2, jsgraph()->Constant(data));
  node->InsertInput(graph()->zone(), 3, holder);
  node->InsertInput(graph()->zone(), 4,
                    jsgraph()->ExternalConstant(function_reference));
  node->ReplaceInput(5, receiver);
  // Remove context input.
  node->RemoveInput(6 + argc);
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
    Node* node, int arity, CallFrequency const& frequency,
    VectorSlotPair const& feedback) {
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
        CallParameters p = CallParametersOf(user->op());
        int const arity = static_cast<int>(p.arity() - 1);
        if (user->InputAt(arity) == arguments_list) continue;
        break;
      }
      case IrOpcode::kJSConstructWithSpread: {
        // Ignore uses as spread input to construct with spread.
        ConstructParameters p = ConstructParametersOf(user->op());
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
      if (!NodeProperties::NoObservableSideEffectBetween(effect,
                                                         arguments_list)) {
        return NoChange();
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
    NodeProperties::ChangeOp(
        node, javascript()->Call(arity + 1, frequency, feedback));
    Reduction const reduction = ReduceJSCall(node);
    return reduction.Changed() ? reduction : Changed(node);
  } else {
    NodeProperties::ChangeOp(
        node, javascript()->Construct(arity + 2, frequency, feedback));
    Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
    Node* frame_state = NodeProperties::GetFrameStateInput(node);
    Node* context = NodeProperties::GetContextInput(node);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    // Check whether the given new target value is a constructor function. The
    // replacement {JSConstruct} operator only checks the passed target value
    // but relies on the new target value to be implicitly valid.
    Node* check =
        graph()->NewNode(simplified()->ObjectIsConstructor(), new_target);
    Node* check_branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
    Node* check_fail = graph()->NewNode(common()->IfFalse(), check_branch);
    Node* check_throw = check_fail =
        graph()->NewNode(javascript()->CallRuntime(Runtime::kThrowTypeError, 2),
                         jsgraph()->Constant(MessageTemplate::kNotConstructor),
                         new_target, context, frame_state, effect, check_fail);
    control = graph()->NewNode(common()->IfTrue(), check_branch);
    NodeProperties::ReplaceControlInput(node, control);

    // Rewire potential exception edges.
    Node* on_exception = nullptr;
    if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
      // Create appropriate {IfException}  and {IfSuccess} nodes.
      Node* if_exception =
          graph()->NewNode(common()->IfException(), check_throw, check_fail);
      check_fail = graph()->NewNode(common()->IfSuccess(), check_fail);

      // Join the exception edges.
      Node* merge =
          graph()->NewNode(common()->Merge(2), if_exception, on_exception);
      Node* ephi = graph()->NewNode(common()->EffectPhi(2), if_exception,
                                    on_exception, merge);
      Node* phi =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           if_exception, on_exception, merge);
      ReplaceWithValue(on_exception, phi, ephi, merge);
      merge->ReplaceInput(1, on_exception);
      ephi->ReplaceInput(1, on_exception);
      phi->ReplaceInput(1, on_exception);
    }

    // The above %ThrowTypeError runtime call is an unconditional throw, making
    // it impossible to return a successful completion in this case. We simply
    // connect the successful completion to the graph end.
    Node* throw_node =
        graph()->NewNode(common()->Throw(), check_throw, check_fail);
    NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

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
  size_t arity = p.arity();
  DCHECK_LE(2u, arity);

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
        case Builtins::kArrayConstructor:
          return ReduceArrayConstructor(node);
        case Builtins::kBooleanConstructor:
          return ReduceBooleanConstructor(node);
        case Builtins::kFunctionPrototypeApply:
          return ReduceFunctionPrototypeApply(node);
        case Builtins::kFastFunctionPrototypeBind:
          return ReduceFunctionPrototypeBind(node);
        case Builtins::kFunctionPrototypeCall:
          return ReduceFunctionPrototypeCall(node);
        case Builtins::kFunctionPrototypeHasInstance:
          return ReduceFunctionPrototypeHasInstance(node);
        case Builtins::kNumberConstructor:
          return ReduceNumberConstructor(node);
        case Builtins::kObjectConstructor:
          return ReduceObjectConstructor(node);
        case Builtins::kObjectGetPrototypeOf:
          return ReduceObjectGetPrototypeOf(node);
        case Builtins::kObjectIs:
          return ReduceObjectIs(node);
        case Builtins::kObjectPrototypeGetProto:
          return ReduceObjectPrototypeGetProto(node);
        case Builtins::kObjectPrototypeHasOwnProperty:
          return ReduceObjectPrototypeHasOwnProperty(node);
        case Builtins::kObjectPrototypeIsPrototypeOf:
          return ReduceObjectPrototypeIsPrototypeOf(node);
        case Builtins::kReflectApply:
          return ReduceReflectApply(node);
        case Builtins::kReflectConstruct:
          return ReduceReflectConstruct(node);
        case Builtins::kReflectGet:
          return ReduceReflectGet(node);
        case Builtins::kReflectGetPrototypeOf:
          return ReduceReflectGetPrototypeOf(node);
        case Builtins::kReflectHas:
          return ReduceReflectHas(node);
        case Builtins::kArrayForEach:
          return ReduceArrayForEach(function, node);
        case Builtins::kArrayMap:
          return ReduceArrayMap(function, node);
        case Builtins::kArrayFilter:
          return ReduceArrayFilter(function, node);
        case Builtins::kReturnReceiver:
          return ReduceReturnReceiver(node);
        default:
          break;
      }

      if (!FLAG_runtime_stats && shared->IsApiFunction()) {
        return ReduceCallApiFunction(node, function);
      }
    } else if (m.Value()->IsJSBoundFunction()) {
      Handle<JSBoundFunction> function =
          Handle<JSBoundFunction>::cast(m.Value());
      Handle<JSReceiver> bound_target_function(
          function->bound_target_function(), isolate());
      Handle<Object> bound_this(function->bound_this(), isolate());
      Handle<FixedArray> bound_arguments(function->bound_arguments(),
                                         isolate());
      ConvertReceiverMode const convert_mode =
          (bound_this->IsNullOrUndefined(isolate()))
              ? ConvertReceiverMode::kNullOrUndefined
              : ConvertReceiverMode::kNotNullOrUndefined;
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

  // If {target} is the result of a JSCreateBoundFunction operation,
  // we can just fold the construction and call the bound target
  // function directly instead.
  if (target->opcode() == IrOpcode::kJSCreateBoundFunction) {
    Node* bound_target_function = NodeProperties::GetValueInput(target, 0);
    Node* bound_this = NodeProperties::GetValueInput(target, 1);
    int const bound_arguments_length =
        static_cast<int>(CreateBoundFunctionParametersOf(target->op()).arity());

    // Patch the {node} to use [[BoundTargetFunction]] and [[BoundThis]].
    NodeProperties::ReplaceValueInput(node, bound_target_function, 0);
    NodeProperties::ReplaceValueInput(node, bound_this, 1);

    // Insert the [[BoundArguments]] for {node}.
    for (int i = 0; i < bound_arguments_length; ++i) {
      Node* value = NodeProperties::GetValueInput(target, 2 + i);
      node->InsertInput(graph()->zone(), 2 + i, value);
      arity++;
    }

    // Update the JSCall operator on {node}.
    ConvertReceiverMode const convert_mode =
        NodeProperties::CanBeNullOrUndefined(bound_this, effect)
            ? ConvertReceiverMode::kAny
            : ConvertReceiverMode::kNotNullOrUndefined;
    NodeProperties::ChangeOp(
        node, javascript()->Call(arity, p.frequency(), VectorSlotPair(),
                                 convert_mode));

    // Try to further reduce the JSCall {node}.
    Reduction const reduction = ReduceJSCall(node);
    return reduction.Changed() ? reduction : Changed(node);
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
  if (feedback->IsWeakCell()) {
    // Check if we want to use CallIC feedback here.
    if (!ShouldUseCallICFeedback(target)) return NoChange();

    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsCallable()) {
      Node* target_function =
          jsgraph()->Constant(handle(cell->value(), isolate()));

      // Check that the {target} is still the {target_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     target_function);
      effect =
          graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kNoReason),
                           check, effect, control);

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
  VectorSlotPair feedback;
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, 2, frequency,
                                                    feedback);
}

Reduction JSCallReducer::ReduceJSCallWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallWithSpread, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 1);
  CallFrequency frequency = p.frequency();
  VectorSlotPair feedback = p.feedback();
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, arity, frequency,
                                                    feedback);
}

Reduction JSCallReducer::ReduceJSConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstruct, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Extract feedback from the {node} using the CallICNexus.
  if (p.feedback().IsValid()) {
    CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
    if (nexus.IsUninitialized()) {
      if (flags() & kBailoutOnUninitialized) {
        // Introduce a SOFT deopt if the construct {node} wasn't executed so
        // far.
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
      // implementation in Ignition.
      Handle<AllocationSite> site = Handle<AllocationSite>::cast(feedback);

      // Retrieve the Array function from the {node}.
      Node* array_function = jsgraph()->HeapConstant(
          handle(native_context()->array_function(), isolate()));

      // Check that the {target} is still the {array_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     array_function);
      effect =
          graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kNoReason),
                           check, effect, control);

      // Turn the {node} into a {JSCreateArray} call.
      NodeProperties::ReplaceEffectInput(node, effect);
      for (int i = arity; i > 0; --i) {
        NodeProperties::ReplaceValueInput(
            node, NodeProperties::GetValueInput(node, i), i + 1);
      }
      NodeProperties::ReplaceValueInput(node, array_function, 1);
      NodeProperties::ChangeOp(node, javascript()->CreateArray(arity, site));
      return Changed(node);
    } else if (feedback->IsWeakCell() &&
               !HeapObjectMatcher(new_target).HasValue()) {
      Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
      if (cell->value()->IsConstructor()) {
        Node* new_target_feedback =
            jsgraph()->Constant(handle(cell->value(), isolate()));

        // Check that the {new_target} is still the {new_target_feedback}.
        Node* check = graph()->NewNode(simplified()->ReferenceEqual(),
                                       new_target, new_target_feedback);
        effect =
            graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kNoReason),
                             check, effect, control);

        // Specialize the JSConstruct node to the {new_target_feedback}.
        NodeProperties::ReplaceValueInput(node, new_target_feedback, arity + 1);
        NodeProperties::ReplaceEffectInput(node, effect);
        if (target == new_target) {
          NodeProperties::ReplaceValueInput(node, new_target_feedback, 0);
        }

        // Try to further reduce the JSConstruct {node}.
        Reduction const reduction = ReduceJSConstruct(node);
        return reduction.Changed() ? reduction : Changed(node);
      }
    }
  }

  // Try to specialize JSConstruct {node}s with constant {target}s.
  HeapObjectMatcher m(target);
  if (m.HasValue()) {
    // Raise a TypeError if the {target} is not a constructor.
    if (!m.Value()->IsConstructor()) {
      NodeProperties::ReplaceValueInputs(node, target);
      NodeProperties::ChangeOp(node,
                               javascript()->CallRuntime(
                                   Runtime::kThrowConstructedNonConstructable));
      return Changed(node);
    }

    if (m.Value()->IsJSFunction()) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());

      // Don't inline cross native context.
      if (function->native_context() != *native_context()) return NoChange();

      // Check for the ArrayConstructor.
      if (*function == function->native_context()->array_function()) {
        // TODO(bmeurer): Deal with Array subclasses here.
        Handle<AllocationSite> site;
        // Turn the {node} into a {JSCreateArray} call.
        for (int i = arity; i > 0; --i) {
          NodeProperties::ReplaceValueInput(
              node, NodeProperties::GetValueInput(node, i), i + 1);
        }
        NodeProperties::ReplaceValueInput(node, new_target, 1);
        NodeProperties::ChangeOp(node, javascript()->CreateArray(arity, site));
        return Changed(node);
      }

      // Check for the ObjectConstructor.
      if (*function == function->native_context()->object_function()) {
        // If no value is passed, we can immediately lower to a simple
        // JSCreate and don't need to do any massaging of the {node}.
        if (arity == 0) {
          NodeProperties::ChangeOp(node, javascript()->Create());
          return Changed(node);
        }

        // Otherwise we can only lower to JSCreate if we know that
        // the value parameter is ignored, which is only the case if
        // the {new_target} and {target} are definitely not identical.
        HeapObjectMatcher mnew_target(new_target);
        if (mnew_target.HasValue() && *mnew_target.Value() != *function) {
          // Drop the value inputs.
          for (int i = arity; i > 0; --i) node->RemoveInput(i);
          NodeProperties::ChangeOp(node, javascript()->Create());
          return Changed(node);
        }
      }
    } else if (m.Value()->IsJSBoundFunction()) {
      Handle<JSBoundFunction> function =
          Handle<JSBoundFunction>::cast(m.Value());
      Handle<JSReceiver> bound_target_function(
          function->bound_target_function(), isolate());
      Handle<FixedArray> bound_arguments(function->bound_arguments(),
                                         isolate());

      // Patch {node} to use [[BoundTargetFunction]].
      NodeProperties::ReplaceValueInput(
          node, jsgraph()->Constant(bound_target_function), 0);

      // Patch {node} to use [[BoundTargetFunction]]
      // as new.target if {new_target} equals {target}.
      NodeProperties::ReplaceValueInput(
          node,
          graph()->NewNode(common()->Select(MachineRepresentation::kTagged),
                           graph()->NewNode(simplified()->ReferenceEqual(),
                                            target, new_target),
                           jsgraph()->Constant(bound_target_function),
                           new_target),
          arity + 1);

      // Insert the [[BoundArguments]] for {node}.
      for (int i = 0; i < bound_arguments->length(); ++i) {
        node->InsertInput(
            graph()->zone(), i + 1,
            jsgraph()->Constant(handle(bound_arguments->get(i), isolate())));
        arity++;
      }

      // Update the JSConstruct operator on {node}.
      NodeProperties::ChangeOp(
          node,
          javascript()->Construct(arity + 2, p.frequency(), VectorSlotPair()));

      // Try to further reduce the JSConstruct {node}.
      Reduction const reduction = ReduceJSConstruct(node);
      return reduction.Changed() ? reduction : Changed(node);
    }

    // TODO(bmeurer): Also support optimizing proxies here.
  }

  // If {target} is the result of a JSCreateBoundFunction operation,
  // we can just fold the construction and construct the bound target
  // function directly instead.
  if (target->opcode() == IrOpcode::kJSCreateBoundFunction) {
    Node* bound_target_function = NodeProperties::GetValueInput(target, 0);
    int const bound_arguments_length =
        static_cast<int>(CreateBoundFunctionParametersOf(target->op()).arity());

    // Patch the {node} to use [[BoundTargetFunction]].
    NodeProperties::ReplaceValueInput(node, bound_target_function, 0);

    // Patch {node} to use [[BoundTargetFunction]]
    // as new.target if {new_target} equals {target}.
    NodeProperties::ReplaceValueInput(
        node,
        graph()->NewNode(common()->Select(MachineRepresentation::kTagged),
                         graph()->NewNode(simplified()->ReferenceEqual(),
                                          target, new_target),
                         bound_target_function, new_target),
        arity + 1);

    // Insert the [[BoundArguments]] for {node}.
    for (int i = 0; i < bound_arguments_length; ++i) {
      Node* value = NodeProperties::GetValueInput(target, 2 + i);
      node->InsertInput(graph()->zone(), 1 + i, value);
      arity++;
    }

    // Update the JSConstruct operator on {node}.
    NodeProperties::ChangeOp(
        node,
        javascript()->Construct(arity + 2, p.frequency(), VectorSlotPair()));

    // Try to further reduce the JSConstruct {node}.
    Reduction const reduction = ReduceJSConstruct(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  return NoChange();
}

Reduction JSCallReducer::ReduceJSConstructWithArrayLike(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithArrayLike, node->opcode());
  CallFrequency frequency = CallFrequencyOf(node->op());
  VectorSlotPair feedback;
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, 1, frequency,
                                                    feedback);
}

Reduction JSCallReducer::ReduceJSConstructWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithSpread, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 2);
  CallFrequency frequency = p.frequency();
  VectorSlotPair feedback = p.feedback();
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, arity, frequency,
                                                    feedback);
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
