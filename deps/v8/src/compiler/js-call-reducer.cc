// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-call-reducer.h"

#include <functional>

#include "src/api/api-inl.h"
#include "src/builtins/builtins-promise.h"
#include "src/builtins/builtins-utils.h"
#include "src/codegen/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/access-info.h"
#include "src/compiler/allocation-builder.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/map-inference.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/property-access-builder.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/ic/call-optimization.h"
#include "src/logging/counters.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSCallReducer::ReduceMathUnary(Node* node, const Operator* op) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->NaNConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* input = NodeProperties::GetValueInput(node, 2);

  input = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       input, effect, control);
  Node* value = graph()->NewNode(op, input);
  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

Reduction JSCallReducer::ReduceMathBinary(Node* node, const Operator* op) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->NaNConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* left = NodeProperties::GetValueInput(node, 2);
  Node* right = node->op()->ValueInputCount() > 3
                    ? NodeProperties::GetValueInput(node, 3)
                    : jsgraph()->NaNConstant();
  left = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       left, effect, control);
  right = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       right, effect, control);
  Node* value = graph()->NewNode(op, left, right);
  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

// ES6 section 20.2.2.19 Math.imul ( x, y )
Reduction JSCallReducer::ReduceMathImul(Node* node) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->ZeroConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* left = NodeProperties::GetValueInput(node, 2);
  Node* right = node->op()->ValueInputCount() > 3
                    ? NodeProperties::GetValueInput(node, 3)
                    : jsgraph()->ZeroConstant();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  left = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       left, effect, control);
  right = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       right, effect, control);
  left = graph()->NewNode(simplified()->NumberToUint32(), left);
  right = graph()->NewNode(simplified()->NumberToUint32(), right);
  Node* value = graph()->NewNode(simplified()->NumberImul(), left, right);
  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

// ES6 section 20.2.2.11 Math.clz32 ( x )
Reduction JSCallReducer::ReduceMathClz32(Node* node) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->Constant(32);
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* input = NodeProperties::GetValueInput(node, 2);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  input = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       input, effect, control);
  input = graph()->NewNode(simplified()->NumberToUint32(), input);
  Node* value = graph()->NewNode(simplified()->NumberClz32(), input);
  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

// ES6 section 20.2.2.24 Math.max ( value1, value2, ...values )
// ES6 section 20.2.2.25 Math.min ( value1, value2, ...values )
Reduction JSCallReducer::ReduceMathMinMax(Node* node, const Operator* op,
                                          Node* empty_value) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() <= 2) {
    ReplaceWithValue(node, empty_value);
    return Replace(empty_value);
  }
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* value = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       NodeProperties::GetValueInput(node, 2), effect, control);
  for (int i = 3; i < node->op()->ValueInputCount(); i++) {
    Node* input = effect = graph()->NewNode(
        simplified()->SpeculativeToNumber(NumberOperationHint::kNumberOrOddball,
                                          p.feedback()),
        NodeProperties::GetValueInput(node, i), effect, control);
    value = graph()->NewNode(op, value, input);
  }

  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

Reduction JSCallReducer::Reduce(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

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
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* target = NodeProperties::GetValueInput(node, 0);
  CallParameters const& p = CallParametersOf(node->op());

  // Turn the {node} into a {JSCreateArray} call.
  DCHECK_LE(2u, p.arity());
  size_t const arity = p.arity() - 2;
  NodeProperties::ReplaceValueInput(node, target, 0);
  NodeProperties::ReplaceValueInput(node, target, 1);
  NodeProperties::ChangeOp(
      node, javascript()->CreateArray(arity, MaybeHandle<AllocationSite>()));
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

// ES section #sec-object-constructor
Reduction JSCallReducer::ReduceObjectConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.arity() < 3) return NoChange();
  Node* value = (p.arity() >= 3) ? NodeProperties::GetValueInput(node, 2)
                                 : jsgraph()->UndefinedConstant();
  Node* effect = NodeProperties::GetEffectInput(node);

  // We can fold away the Object(x) call if |x| is definitely not a primitive.
  if (NodeProperties::CanBePrimitive(broker(), value, effect)) {
    if (!NodeProperties::CanBeNullOrUndefined(broker(), value, effect)) {
      // Turn the {node} into a {JSToObject} call if we know that
      // the {value} cannot be null or undefined.
      NodeProperties::ReplaceValueInputs(node, value);
      NodeProperties::ChangeOp(node, javascript()->ToObject());
      return Changed(node);
    }
  } else {
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 19.2.3.1 Function.prototype.apply ( thisArg, argArray )
Reduction JSCallReducer::ReduceFunctionPrototypeApply(Node* node) {
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

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
    if (!NodeProperties::CanBeNullOrUndefined(broker(), arguments_list,
                                              effect)) {
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
  // TODO(mslekova): Since this introduces a Call that will get optimized by
  // the JSCallReducer, we basically might have to do all the serialization
  // that we do for that here as well. The only difference is that here we
  // disable speculation (cf. the empty FeedbackSource above), causing the
  // JSCallReducer to do much less work. We should revisit this later.
  NodeProperties::ChangeOp(
      node,
      javascript()->Call(arity, p.frequency(), FeedbackSource(), convert_mode));
  // Try to further reduce the JSCall {node}.
  Reduction const reduction = ReduceJSCall(node);
  return reduction.Changed() ? reduction : Changed(node);
}

// ES section #sec-function.prototype.bind
Reduction JSCallReducer::ReduceFunctionPrototypeBind(Node* node) {
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  // Value inputs to the {node} are as follows:
  //
  //  - target, which is Function.prototype.bind JSFunction
  //  - receiver, which is the [[BoundTargetFunction]]
  //  - bound_this (optional), which is the [[BoundThis]]
  //  - and all the remaining value inputs are [[BoundArguments]]
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
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  MapRef first_receiver_map(broker(), receiver_maps[0]);
  bool const is_constructor = first_receiver_map.is_constructor();

  if (FLAG_concurrent_inlining && !first_receiver_map.serialized_prototype()) {
    TRACE_BROKER_MISSING(broker(),
                         "serialized prototype on map " << first_receiver_map);
    return inference.NoChange();
  }
  ObjectRef const prototype = first_receiver_map.prototype();
  for (Handle<Map> const map : receiver_maps) {
    MapRef receiver_map(broker(), map);

    if (FLAG_concurrent_inlining && !receiver_map.serialized_prototype()) {
      TRACE_BROKER_MISSING(broker(),
                           "serialized prototype on map " << receiver_map);
      return inference.NoChange();
    }

    // Check for consistency among the {receiver_maps}.
    STATIC_ASSERT(LAST_TYPE == LAST_FUNCTION_TYPE);
    if (!receiver_map.prototype().equals(prototype) ||
        receiver_map.is_constructor() != is_constructor ||
        receiver_map.instance_type() < FIRST_FUNCTION_TYPE) {
      return inference.NoChange();
    }

    // Disallow binding of slow-mode functions. We need to figure out
    // whether the length and name property are in the original state.
    if (receiver_map.is_dictionary_map()) return inference.NoChange();

    // Check whether the length and name properties are still present
    // as AccessorInfo objects. In that case, their values can be
    // recomputed even if the actual value of the object changes.
    // This mirrors the checks done in builtins-function-gen.cc at
    // runtime otherwise.
    int minimum_nof_descriptors = i::Max(JSFunction::kLengthDescriptorIndex,
                                           JSFunction::kNameDescriptorIndex) +
                                  1;
    if (receiver_map.NumberOfOwnDescriptors() < minimum_nof_descriptors) {
      return inference.NoChange();
    }
    const InternalIndex kLengthIndex(JSFunction::kLengthDescriptorIndex);
    const InternalIndex kNameIndex(JSFunction::kNameDescriptorIndex);
    if (!receiver_map.serialized_own_descriptor(kLengthIndex) ||
        !receiver_map.serialized_own_descriptor(kNameIndex)) {
      TRACE_BROKER_MISSING(broker(),
                           "serialized descriptors on map " << receiver_map);
      return inference.NoChange();
    }
    ReadOnlyRoots roots(isolate());
    StringRef length_string(broker(), roots.length_string_handle());
    StringRef name_string(broker(), roots.name_string_handle());

    if (!receiver_map.GetPropertyKey(kLengthIndex).equals(length_string) ||
        !receiver_map.GetStrongValue(kLengthIndex).IsAccessorInfo() ||
        !receiver_map.GetPropertyKey(kNameIndex).equals(name_string) ||
        !receiver_map.GetStrongValue(kNameIndex).IsAccessorInfo()) {
      return inference.NoChange();
    }
  }

  // Choose the map for the resulting JSBoundFunction (but bail out in case of a
  // custom prototype).
  MapRef map = is_constructor
                   ? native_context().bound_function_with_constructor_map()
                   : native_context().bound_function_without_constructor_map();
  if (!map.prototype().equals(prototype)) return inference.NoChange();

  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

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
  Node* value = effect =
      graph()->NewNode(javascript()->CreateBoundFunction(arity, map.object()),
                       input_count, inputs);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES6 section 19.2.3.3 Function.prototype.call (thisArg, ...args)
Reduction JSCallReducer::ReduceFunctionPrototypeCall(Node* node) {
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Change context of {node} to the Function.prototype.call context,
  // to ensure any exception is thrown in the correct context.
  Node* context;
  HeapObjectMatcher m(target);
  if (m.HasValue()) {
    JSFunctionRef function = m.Ref(broker()).AsJSFunction();
    if (FLAG_concurrent_inlining && !function.serialized()) {
      TRACE_BROKER_MISSING(broker(), "Serialize call on function " << function);
      return NoChange();
    }
    context = jsgraph()->Constant(function.context());
  } else {
    context = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSFunctionContext()), target,
        effect, control);
  }
  NodeProperties::ReplaceContextInput(node, context);
  NodeProperties::ReplaceEffectInput(node, effect);

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
      javascript()->Call(arity, p.frequency(), FeedbackSource(), convert_mode));
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
  MapInference inference(broker(), object, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& object_maps = inference.GetMaps();

  MapRef candidate_map(broker(), object_maps[0]);
  if (FLAG_concurrent_inlining && !candidate_map.serialized_prototype()) {
    TRACE_BROKER_MISSING(broker(), "prototype for map " << candidate_map);
    return inference.NoChange();
  }
  ObjectRef candidate_prototype = candidate_map.prototype();

  // Check if we can constant-fold the {candidate_prototype}.
  for (size_t i = 0; i < object_maps.size(); ++i) {
    MapRef object_map(broker(), object_maps[i]);
    if (FLAG_concurrent_inlining && !object_map.serialized_prototype()) {
      TRACE_BROKER_MISSING(broker(), "prototype for map " << object_map);
      return inference.NoChange();
    }
    if (IsSpecialReceiverInstanceType(object_map.instance_type()) ||
        !object_map.prototype().equals(candidate_prototype)) {
      // We exclude special receivers, like JSProxy or API objects that
      // might require access checks here; we also don't want to deal
      // with hidden prototypes at this point.
      return inference.NoChange();
    }
    // The above check also excludes maps for primitive values, which is
    // important because we are not applying [[ToObject]] here as expected.
    DCHECK(!object_map.IsPrimitiveMap() && object_map.IsJSReceiverMap());
  }
  if (!inference.RelyOnMapsViaStability(dependencies())) {
    return inference.NoChange();
  }
  Node* value = jsgraph()->Constant(candidate_prototype);
  ReplaceWithValue(node, value);
  return Replace(value);
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
              simplified()->CheckIf(DeoptimizeReason::kWrongMap), check, effect,
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
  DisallowHeapAccessIf no_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* value = node->op()->ValueInputCount() > 2
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->UndefinedConstant();
  Node* effect = NodeProperties::GetEffectInput(node);

  // Ensure that the {receiver} is known to be a JSReceiver (so that
  // the ToObject step of Object.prototype.isPrototypeOf is a no-op).
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAreJSReceiver()) {
    return NoChange();
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

// ES6 section #sec-object.create Object.create(proto, properties)
Reduction JSCallReducer::ReduceObjectCreate(Node* node) {
  int arg_count = node->op()->ValueInputCount();
  Node* properties = arg_count >= 4 ? NodeProperties::GetValueInput(node, 3)
                                    : jsgraph()->UndefinedConstant();
  if (properties != jsgraph()->UndefinedConstant()) return NoChange();

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* prototype = arg_count >= 3 ? NodeProperties::GetValueInput(node, 2)
                                   : jsgraph()->UndefinedConstant();
  node->ReplaceInput(0, prototype);
  node->ReplaceInput(1, context);
  node->ReplaceInput(2, frame_state);
  node->ReplaceInput(3, effect);
  node->ReplaceInput(4, control);
  node->TrimInputCount(5);
  NodeProperties::ChangeOp(node, javascript()->CreateObject());
  return Changed(node);
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
        jsgraph()->Constant(
            static_cast<int>(MessageTemplate::kCalledOnNonObject)),
        jsgraph()->HeapConstant(factory()->ReflectGet_string()), context,
        frame_state, efalse, if_false);
  }

  // Otherwise just use the existing GetPropertyStub.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue;
  {
    Callable callable =
        Builtins::CallableFor(isolate(), Builtins::kGetProperty);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(),
        CallDescriptor::kNeedsFrameState, Operator::kNoProperties);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    vtrue = etrue = if_true =
        graph()->NewNode(common()->Call(call_descriptor), stub_code, target,
                         key, context, frame_state, etrue, if_true);
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
        jsgraph()->Constant(
            static_cast<int>(MessageTemplate::kCalledOnNonObject)),
        jsgraph()->HeapConstant(factory()->ReflectHas_string()), context,
        frame_state, efalse, if_false);
  }

  // Otherwise just use the existing {JSHasProperty} logic.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue;
  {
    // TODO(magardn): collect feedback so this can be optimized
    vtrue = etrue = if_true =
        graph()->NewNode(javascript()->HasProperty(FeedbackSource()), target,
                         key, context, frame_state, etrue, if_true);
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

Node* JSCallReducer::WireInLoopStart(Node* k, Node** control, Node** effect) {
  Node* loop = *control =
      graph()->NewNode(common()->Loop(2), *control, *control);
  Node* eloop = *effect =
      graph()->NewNode(common()->EffectPhi(2), *effect, *effect, loop);
  Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
  NodeProperties::MergeControlToEnd(graph(), common(), terminate);
  return graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2), k,
                          k, loop);
}

void JSCallReducer::WireInLoopEnd(Node* loop, Node* eloop, Node* vloop, Node* k,
                                  Node* control, Node* effect) {
  loop->ReplaceInput(1, control);
  vloop->ReplaceInput(1, k);
  eloop->ReplaceInput(1, effect);
}

namespace {
bool CanInlineArrayIteratingBuiltin(JSHeapBroker* broker,
                                    MapHandles const& receiver_maps,
                                    ElementsKind* kind_return) {
  DCHECK_NE(0, receiver_maps.size());
  *kind_return = MapRef(broker, receiver_maps[0]).elements_kind();
  for (auto receiver_map : receiver_maps) {
    MapRef map(broker, receiver_map);
    if (!map.supports_fast_array_iteration() ||
        !UnionElementsKindUptoSize(kind_return, map.elements_kind())) {
      return false;
    }
  }
  return true;
}

bool CanInlineArrayResizingBuiltin(JSHeapBroker* broker,
                                   MapHandles const& receiver_maps,
                                   std::vector<ElementsKind>* kinds,
                                   bool builtin_is_push = false) {
  DCHECK_NE(0, receiver_maps.size());
  for (auto receiver_map : receiver_maps) {
    MapRef map(broker, receiver_map);
    if (!map.supports_fast_array_resize()) return false;
    // TODO(turbofan): We should also handle fast holey double elements once
    // we got the hole NaN mess sorted out in TurboFan/V8.
    if (map.elements_kind() == HOLEY_DOUBLE_ELEMENTS && !builtin_is_push) {
      return false;
    }
    ElementsKind current_kind = map.elements_kind();
    auto kind_ptr = kinds->data();
    size_t i;
    for (i = 0; i < kinds->size(); i++, kind_ptr++) {
      if (UnionElementsKindUptoPackedness(kind_ptr, current_kind)) {
        break;
      }
    }
    if (i == kinds->size()) kinds->push_back(current_kind);
  }
  return true;
}
}  // namespace

Reduction JSCallReducer::ReduceArrayForEach(
    Node* node, const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  Node* k = jsgraph()->ZeroConstant();
  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      effect, control);

  Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                               original_length};
  const int stack_parameters = arraysize(checkpoint_params);

  // Check whether the given callback function is callable. Note that this has
  // to happen outside the loop to make sure we also throw on empty arrays.
  Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), shared, Builtins::kArrayForEachLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  WireInCallbackIsCallableCheck(fncallback, context, check_frame_state, effect,
                                &control, &check_fail, &check_throw);

  // Start the loop.
  Node* vloop = k = WireInLoopStart(k, &control, &effect);
  Node *loop = control, *eloop = effect;
  checkpoint_params[3] = k;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayForEachLoopEagerDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  checkpoint_params[3] = next_k;

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* check;
    if (IsDoubleElementsKind(kind)) {
      check = graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
    } else {
      check = graph()->NewNode(simplified()->ReferenceEqual(), element,
                               jsgraph()->TheHoleConstant());
    }
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = effect = graph()->NewNode(
        common()->TypeGuard(Type::NonInternal()), element, effect, control);
  }

  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), shared, Builtins::kArrayForEachLoopLazyDeoptContinuation,
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

  WireInLoopEnd(loop, eloop, vloop, next_k, control, effect);

  control = if_false;
  effect = eloop;

  // Introduce proper LoopExit and LoopExitEffect nodes to mark
  // {loop} as a candidate for loop peeling (crbug.com/v8/8273).
  control = graph()->NewNode(common()->LoopExit(), control, loop);
  effect = graph()->NewNode(common()->LoopExitEffect(), effect, control);

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

Reduction JSCallReducer::ReduceArrayReduce(
    Node* node, ArrayReduceDirection direction,
    const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  bool left = direction == ArrayReduceDirection::kLeft;

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS)),
      receiver, effect, control);

  Node* initial_index =
      left ? jsgraph()->ZeroConstant()
           : graph()->NewNode(simplified()->NumberSubtract(), original_length,
                              jsgraph()->OneConstant());
  const Operator* next_op =
      left ? simplified()->NumberAdd() : simplified()->NumberSubtract();
  Node* k = initial_index;

  Node* check_frame_state;
  {
    Builtins::Name builtin_lazy =
        left ? Builtins::kArrayReduceLoopLazyDeoptContinuation
             : Builtins::kArrayReduceRightLoopLazyDeoptContinuation;
    Node* checkpoint_params[] = {receiver, fncallback, k, original_length,
                                 jsgraph()->UndefinedConstant()};
    const int stack_parameters = arraysize(checkpoint_params);
    check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, builtin_lazy, node->InputAt(0), context,
        &checkpoint_params[0], stack_parameters - 1, outer_frame_state,
        ContinuationFrameStateMode::LAZY);
  }
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  // Check whether the given callback function is callable. Note that
  // this has to happen outside the loop to make sure we also throw on
  // empty arrays.
  WireInCallbackIsCallableCheck(fncallback, context, check_frame_state, effect,
                                &control, &check_fail, &check_throw);

  std::function<Node*(Node*)> hole_check = [this, kind](Node* element) {
    if (IsDoubleElementsKind(kind)) {
      return graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
    } else {
      return graph()->NewNode(simplified()->ReferenceEqual(), element,
                              jsgraph()->TheHoleConstant());
    }
  };

  // Set initial accumulator value
  Node* cur = jsgraph()->TheHoleConstant();

  if (node->op()->ValueInputCount() > 3) {
    cur = NodeProperties::GetValueInput(node, 3);
  } else {
    // Find first/last non holey element. In case the search fails, we need a
    // deopt continuation.
    Builtins::Name builtin_eager =
        left ? Builtins::kArrayReducePreLoopEagerDeoptContinuation
             : Builtins::kArrayReduceRightPreLoopEagerDeoptContinuation;
    Node* checkpoint_params[] = {receiver, fncallback, original_length};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* find_first_element_frame_state =
        CreateJavaScriptBuiltinContinuationFrameState(
            jsgraph(), shared, builtin_eager, node->InputAt(0), context,
            &checkpoint_params[0], stack_parameters, outer_frame_state,
            ContinuationFrameStateMode::EAGER);

    Node* vloop = k = WireInLoopStart(k, &control, &effect);
    Node* loop = control;
    Node* eloop = effect;
    effect = graph()->NewNode(common()->Checkpoint(),
                              find_first_element_frame_state, effect, control);
    Node* continue_test =
        left ? graph()->NewNode(simplified()->NumberLessThan(), k,
                                original_length)
             : graph()->NewNode(simplified()->NumberLessThanOrEqual(),
                                jsgraph()->ZeroConstant(), k);
    effect = graph()->NewNode(
        simplified()->CheckIf(DeoptimizeReason::kNoInitialElement),
        continue_test, effect, control);

    cur = SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
    Node* next_k = graph()->NewNode(next_op, k, jsgraph()->OneConstant());

    Node* hole_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                         hole_check(cur), control);
    Node* found_el = graph()->NewNode(common()->IfFalse(), hole_branch);
    control = found_el;
    Node* is_hole = graph()->NewNode(common()->IfTrue(), hole_branch);

    WireInLoopEnd(loop, eloop, vloop, next_k, is_hole, effect);
    // We did the hole-check, so exclude hole from the type.
    cur = effect = graph()->NewNode(common()->TypeGuard(Type::NonInternal()),
                                    cur, effect, control);
    k = next_k;
  }

  // Start the loop.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);
  Node* eloop = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
  Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
  NodeProperties::MergeControlToEnd(graph(), common(), terminate);
  Node* kloop = k = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), k, k, loop);
  Node* curloop = cur = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), cur, cur, loop);

  control = loop;
  effect = eloop;

  Node* continue_test =
      left
          ? graph()->NewNode(simplified()->NumberLessThan(), k, original_length)
          : graph()->NewNode(simplified()->NumberLessThanOrEqual(),
                             jsgraph()->ZeroConstant(), k);

  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    Builtins::Name builtin_eager =
        left ? Builtins::kArrayReduceLoopEagerDeoptContinuation
             : Builtins::kArrayReduceRightLoopEagerDeoptContinuation;
    Node* checkpoint_params[] = {receiver, fncallback, k, original_length,
                                 curloop};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, builtin_eager, node->InputAt(0), context,
        &checkpoint_params[0], stack_parameters, outer_frame_state,
        ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k = graph()->NewNode(next_op, k, jsgraph()->OneConstant());

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* branch = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                    hole_check(element), control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = effect = graph()->NewNode(
        common()->TypeGuard(Type::NonInternal()), element, effect, control);
  }

  Node* next_cur;
  {
    Builtins::Name builtin_lazy =
        left ? Builtins::kArrayReduceLoopLazyDeoptContinuation
             : Builtins::kArrayReduceRightLoopLazyDeoptContinuation;
    Node* checkpoint_params[] = {receiver, fncallback, next_k, original_length,
                                 curloop};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, builtin_lazy, node->InputAt(0), context,
        &checkpoint_params[0], stack_parameters - 1, outer_frame_state,
        ContinuationFrameStateMode::LAZY);

    next_cur = control = effect =
        graph()->NewNode(javascript()->Call(6, p.frequency()), fncallback,
                         jsgraph()->UndefinedConstant(), cur, element, k,
                         receiver, context, frame_state, effect, control);
  }

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
    next_cur =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2), cur,
                         next_cur, control);
  }

  k = next_k;
  cur = next_cur;

  loop->ReplaceInput(1, control);
  kloop->ReplaceInput(1, k);
  curloop->ReplaceInput(1, cur);
  eloop->ReplaceInput(1, effect);

  control = if_false;
  effect = eloop;

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the
  // successful completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, curloop, effect, control);
  return Replace(curloop);
}

Reduction JSCallReducer::ReduceArrayMap(Node* node,
                                        const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnArraySpeciesProtector())
    return inference.NoChange();
  if (IsHoleyElementsKind(kind)) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  Node* array_constructor = jsgraph()->Constant(
      native_context().GetInitialJSArrayMap(kind).GetConstructor());
  Node* k = jsgraph()->ZeroConstant();
  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      effect, control);

  // If the array length >= kMaxFastArrayLength, then CreateArray
  // will create a dictionary. We should deopt in this case, and make sure
  // not to attempt inlining again.
  original_length = effect = graph()->NewNode(
      simplified()->CheckBounds(p.feedback()), original_length,
      jsgraph()->Constant(JSArray::kMaxFastArrayLength), effect, control);

  // Even though {JSCreateArray} is not marked as {kNoThrow}, we can elide the
  // exceptional projections because it cannot throw with the given
  // parameters.
  Node* a = control = effect = graph()->NewNode(
      javascript()->CreateArray(1, MaybeHandle<AllocationSite>()),
      array_constructor, array_constructor, original_length, context,
      outer_frame_state, effect, control);

  Node* checkpoint_params[] = {receiver, fncallback, this_arg,
                               a,        k,          original_length};
  const int stack_parameters = arraysize(checkpoint_params);

  // Check whether the given callback function is callable. Note that this has
  // to happen outside the loop to make sure we also throw on empty arrays.
  Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), shared, Builtins::kArrayMapLoopLazyDeoptContinuation,
      node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
      outer_frame_state, ContinuationFrameStateMode::LAZY);
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  WireInCallbackIsCallableCheck(fncallback, context, check_frame_state, effect,
                                &control, &check_fail, &check_throw);

  // Start the loop.
  Node* vloop = k = WireInLoopStart(k, &control, &effect);
  Node *loop = control, *eloop = effect;
  checkpoint_params[4] = k;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayMapLoopEagerDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* check;
    if (IsDoubleElementsKind(kind)) {
      check = graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
    } else {
      check = graph()->NewNode(simplified()->ReferenceEqual(), element,
                               jsgraph()->TheHoleConstant());
    }
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = effect = graph()->NewNode(
        common()->TypeGuard(Type::NonInternal()), element, effect, control);
  }

  // This frame state is dealt with by hand in
  // ArrayMapLoopLazyDeoptContinuation.
  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), shared, Builtins::kArrayMapLoopLazyDeoptContinuation,
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

  // The array {a} should be HOLEY_SMI_ELEMENTS because we'd only come into
  // this loop if the input array length is non-zero, and "new Array({x > 0})"
  // always produces a HOLEY array.
  MapRef holey_double_map =
      native_context().GetInitialJSArrayMap(HOLEY_DOUBLE_ELEMENTS);
  MapRef holey_map = native_context().GetInitialJSArrayMap(HOLEY_ELEMENTS);
  effect = graph()->NewNode(simplified()->TransitionAndStoreElement(
                                holey_double_map.object(), holey_map.object()),
                            a, k, callback_value, effect, control);

  if (IsHoleyElementsKind(kind)) {
    Node* after_call_and_store_control = control;
    Node* after_call_and_store_effect = effect;
    control = hole_true;
    effect = effect_true;

    control = graph()->NewNode(common()->Merge(2), control,
                               after_call_and_store_control);
    effect = graph()->NewNode(common()->EffectPhi(2), effect,
                              after_call_and_store_effect, control);
  }

  WireInLoopEnd(loop, eloop, vloop, next_k, control, effect);

  control = if_false;
  effect = eloop;

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the
  // successful completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, a, effect, control);
  return Replace(a);
}

Reduction JSCallReducer::ReduceArrayFilter(
    Node* node, const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnArraySpeciesProtector())
    return inference.NoChange();
  if (IsHoleyElementsKind(kind)) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  // The output array is packed (filter doesn't visit holes).
  const ElementsKind packed_kind = GetPackedElementsKind(kind);
  MapRef initial_map = native_context().GetInitialJSArrayMap(packed_kind);

  Node* k = jsgraph()->ZeroConstant();
  Node* to = jsgraph()->ZeroConstant();

  Node* a;  // Construct the output array.
  {
    AllocationBuilder ab(jsgraph(), effect, control);
    ab.Allocate(initial_map.instance_size(), AllocationType::kYoung,
                Type::Array());
    ab.Store(AccessBuilder::ForMap(), initial_map);
    Node* empty_fixed_array = jsgraph()->EmptyFixedArrayConstant();
    ab.Store(AccessBuilder::ForJSObjectPropertiesOrHashKnownPointer(),
             empty_fixed_array);
    ab.Store(AccessBuilder::ForJSObjectElements(), empty_fixed_array);
    ab.Store(AccessBuilder::ForJSArrayLength(packed_kind),
             jsgraph()->ZeroConstant());
    for (int i = 0; i < initial_map.GetInObjectProperties(); ++i) {
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
    Node* checkpoint_params[] = {receiver, fncallback,      this_arg, a,
                                 k,        original_length, to,       to};
    const int stack_parameters = arraysize(checkpoint_params);

    Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayFilterLoopLazyDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::LAZY);
    WireInCallbackIsCallableCheck(fncallback, context, check_frame_state,
                                  effect, &control, &check_fail, &check_throw);
  }

  // Start the loop.
  Node* vloop = k = WireInLoopStart(k, &control, &effect);
  Node *loop = control, *eloop = effect;
  Node* v_to_loop = to = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTaggedSigned, 2), to, to, loop);

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    Node* checkpoint_params[] = {receiver, fncallback,      this_arg, a,
                                 k,        original_length, to};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayFilterLoopEagerDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;
  Node* hole_true_vto = to;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* check;
    if (IsDoubleElementsKind(kind)) {
      check = graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
    } else {
      check = graph()->NewNode(simplified()->ReferenceEqual(), element,
                               jsgraph()->TheHoleConstant());
    }
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = effect = graph()->NewNode(
        common()->TypeGuard(Type::NonInternal()), element, effect, control);
  }

  Node* callback_value = nullptr;
  {
    // This frame state is dealt with by hand in
    // Builtins::kArrayFilterLoopLazyDeoptContinuation.
    Node* checkpoint_params[] = {receiver, fncallback,      this_arg, a,
                                 k,        original_length, element,  to};
    const int stack_parameters = arraysize(checkpoint_params);

    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayFilterLoopLazyDeoptContinuation,
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
    Node* checkpoint_params[] = {receiver, fncallback, this_arg,
                                 a,        k,          original_length,
                                 element,  to,         callback_value};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayFilterLoopLazyDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);

    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // We have to coerce callback_value to boolean, and only store the element
  // in a if it's true. The checkpoint above protects against the case that
  // growing {a} fails.
  to = DoFilterPostCallbackWork(packed_kind, &control, &effect, a, to, element,
                                callback_value);

  if (IsHoleyElementsKind(kind)) {
    Node* after_call_control = control;
    Node* after_call_effect = effect;
    control = hole_true;
    effect = effect_true;

    control = graph()->NewNode(common()->Merge(2), control, after_call_control);
    effect = graph()->NewNode(common()->EffectPhi(2), effect, after_call_effect,
                              control);
    to =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTaggedSigned, 2),
                         hole_true_vto, to, control);
  }

  WireInLoopEnd(loop, eloop, vloop, next_k, control, effect);
  v_to_loop->ReplaceInput(1, to);

  control = if_false;
  effect = eloop;

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the
  // successful completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, a, effect, control);
  return Replace(a);
}

Reduction JSCallReducer::ReduceArrayFind(Node* node, ArrayFindVariant variant,
                                         const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Builtins::Name eager_continuation_builtin;
  Builtins::Name lazy_continuation_builtin;
  Builtins::Name after_callback_lazy_continuation_builtin;
  if (variant == ArrayFindVariant::kFind) {
    eager_continuation_builtin = Builtins::kArrayFindLoopEagerDeoptContinuation;
    lazy_continuation_builtin = Builtins::kArrayFindLoopLazyDeoptContinuation;
    after_callback_lazy_continuation_builtin =
        Builtins::kArrayFindLoopAfterCallbackLazyDeoptContinuation;
  } else {
    DCHECK_EQ(ArrayFindVariant::kFindIndex, variant);
    eager_continuation_builtin =
        Builtins::kArrayFindIndexLoopEagerDeoptContinuation;
    lazy_continuation_builtin =
        Builtins::kArrayFindIndexLoopLazyDeoptContinuation;
    after_callback_lazy_continuation_builtin =
        Builtins::kArrayFindIndexLoopAfterCallbackLazyDeoptContinuation;
  }

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  Node* k = jsgraph()->ZeroConstant();
  Node* original_length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      effect, control);
  Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                               original_length};
  const int stack_parameters = arraysize(checkpoint_params);

  // Check whether the given callback function is callable. Note that this has
  // to happen outside the loop to make sure we also throw on empty arrays.
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  {
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, lazy_continuation_builtin, node->InputAt(0), context,
        &checkpoint_params[0], stack_parameters, outer_frame_state,
        ContinuationFrameStateMode::LAZY);
    WireInCallbackIsCallableCheck(fncallback, context, frame_state, effect,
                                  &control, &check_fail, &check_throw);
  }

  // Start the loop.
  Node* vloop = k = WireInLoopStart(k, &control, &effect);
  Node *loop = control, *eloop = effect;
  checkpoint_params[3] = k;

  // Check if we've iterated past the last element of the array.
  Node* if_false = nullptr;
  {
    Node* continue_test =
        graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
    Node* continue_branch = graph()->NewNode(
        common()->Branch(BranchHint::kNone), continue_test, control);
    control = graph()->NewNode(common()->IfTrue(), continue_branch);
    if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  }

  {
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, eager_continuation_builtin, node->InputAt(0),
        context, &checkpoint_params[0], stack_parameters, outer_frame_state,
        ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k = checkpoint_params[3] =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  // Replace holes with undefined.
  if (kind == HOLEY_DOUBLE_ELEMENTS) {
    // TODO(7409): avoid deopt if not all uses of value are truncated.
    CheckFloat64HoleMode mode = CheckFloat64HoleMode::kAllowReturnHole;
    element = effect =
        graph()->NewNode(simplified()->CheckFloat64Hole(mode, p.feedback()),
                         element, effect, control);
  } else if (IsHoleyElementsKind(kind)) {
    element =
        graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(), element);
  }

  Node* if_found_return_value =
      (variant == ArrayFindVariant::kFind) ? element : k;

  // Call the callback.
  Node* callback_value = nullptr;
  {
    Node* call_checkpoint_params[] = {receiver,        fncallback,
                                      this_arg,        next_k,
                                      original_length, if_found_return_value};
    const int call_stack_parameters = arraysize(call_checkpoint_params);

    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, after_callback_lazy_continuation_builtin,
        node->InputAt(0), context, &call_checkpoint_params[0],
        call_stack_parameters, outer_frame_state,
        ContinuationFrameStateMode::LAZY);

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

  // Check whether the given callback function returned a truthy value.
  Node* boolean_result =
      graph()->NewNode(simplified()->ToBoolean(), callback_value);
  Node* efound_branch = effect;
  Node* found_branch = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                        boolean_result, control);
  Node* if_found = graph()->NewNode(common()->IfTrue(), found_branch);
  Node* if_notfound = graph()->NewNode(common()->IfFalse(), found_branch);
  control = if_notfound;

  // Close the loop.
  WireInLoopEnd(loop, eloop, vloop, next_k, control, effect);

  control = graph()->NewNode(common()->Merge(2), if_found, if_false);
  effect =
      graph()->NewNode(common()->EffectPhi(2), efound_branch, eloop, control);

  Node* if_not_found_value = (variant == ArrayFindVariant::kFind)
                                 ? jsgraph()->UndefinedConstant()
                                 : jsgraph()->MinusOneConstant();
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       if_found_return_value, if_not_found_value, control);

  // Introduce proper LoopExit/LoopExitEffect/LoopExitValue to mark
  // {loop} as a candidate for loop peeling (crbug.com/v8/8273).
  control = graph()->NewNode(common()->LoopExit(), control, loop);
  effect = graph()->NewNode(common()->LoopExitEffect(), effect, control);
  value = graph()->NewNode(common()->LoopExitValue(), value, control);

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the
  // successful completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Node* JSCallReducer::DoFilterPostCallbackWork(ElementsKind kind, Node** control,
                                              Node** effect, Node* a, Node* to,
                                              Node* element,
                                              Node* callback_value) {
  Node* boolean_result =
      graph()->NewNode(simplified()->ToBoolean(), callback_value);
  Node* boolean_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          boolean_result, *control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), boolean_branch);
  Node* etrue = *effect;
  Node* vtrue;
  {
    // Load the elements backing store of the {receiver}.
    Node* elements = etrue = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectElements()), a, etrue,
        if_true);

    DCHECK(TypeCache::Get()->kFixedDoubleArrayLengthType.Is(
        TypeCache::Get()->kFixedArrayLengthType));
    Node* checked_to = etrue = graph()->NewNode(
        common()->TypeGuard(TypeCache::Get()->kFixedArrayLengthType), to, etrue,
        if_true);
    Node* elements_length = etrue = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForFixedArrayLength()), elements,
        etrue, if_true);

    GrowFastElementsMode mode =
        IsDoubleElementsKind(kind) ? GrowFastElementsMode::kDoubleElements
                                   : GrowFastElementsMode::kSmiOrObjectElements;
    elements = etrue = graph()->NewNode(
        simplified()->MaybeGrowFastElements(mode, FeedbackSource()), a,
        elements, checked_to, elements_length, etrue, if_true);

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
      jsgraph()->Constant(
          static_cast<int>(MessageTemplate::kCalledNonCallable)),
      fncallback, context, check_frame_state, effect, *check_fail);
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
                                     Node* control, Node** effect, Node** k,
                                     const FeedbackSource& feedback) {
  // Make sure that the access is still in bounds, since the callback could
  // have changed the array's size.
  Node* length = *effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      *effect, control);
  *k = *effect = graph()->NewNode(simplified()->CheckBounds(feedback), *k,
                                  length, *effect, control);

  // Reload the elements pointer before calling the callback, since the
  // previous callback might have resized the array causing the elements
  // buffer to be re-allocated.
  Node* elements = *effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
      *effect, control);

  Node* element = *effect = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement(
          kind, LoadSensitivity::kCritical)),
      elements, *k, *effect, control);
  return element;
}

Reduction JSCallReducer::ReduceArrayEvery(Node* node,
                                          const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnArraySpeciesProtector())
    return inference.NoChange();
  if (IsHoleyElementsKind(kind)) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  Node* k = jsgraph()->ZeroConstant();
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
    // case.
    Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                                 original_length};
    const int stack_parameters = arraysize(checkpoint_params);

    Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayEveryLoopLazyDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::LAZY);
    WireInCallbackIsCallableCheck(fncallback, context, check_frame_state,
                                  effect, &control, &check_fail, &check_throw);
  }

  // Start the loop.
  Node* vloop = k = WireInLoopStart(k, &control, &effect);
  Node *loop = control, *eloop = effect;

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                                 original_length};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayEveryLoopEagerDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* check;
    if (IsDoubleElementsKind(kind)) {
      check = graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
    } else {
      check = graph()->NewNode(simplified()->ReferenceEqual(), element,
                               jsgraph()->TheHoleConstant());
    }
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = effect = graph()->NewNode(
        common()->TypeGuard(Type::NonInternal()), element, effect, control);
  }

  Node* callback_value = nullptr;
  {
    // This frame state is dealt with by hand in
    // Builtins::kArrayEveryLoopLazyDeoptContinuation.
    Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                                 original_length};
    const int stack_parameters = arraysize(checkpoint_params);

    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArrayEveryLoopLazyDeoptContinuation,
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

  // We have to coerce callback_value to boolean.
  Node* if_false_callback;
  Node* efalse_callback;
  {
    Node* boolean_result =
        graph()->NewNode(simplified()->ToBoolean(), callback_value);
    Node* boolean_branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                            boolean_result, control);
    if_false_callback = graph()->NewNode(common()->IfFalse(), boolean_branch);
    efalse_callback = effect;

    // Nothing to do in the true case.
    control = graph()->NewNode(common()->IfTrue(), boolean_branch);
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

  WireInLoopEnd(loop, eloop, vloop, next_k, control, effect);

  control = graph()->NewNode(common()->Merge(2), if_false, if_false_callback);
  effect =
      graph()->NewNode(common()->EffectPhi(2), eloop, efalse_callback, control);
  Node* value = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2),
      jsgraph()->TrueConstant(), jsgraph()->FalseConstant(), control);

  // Introduce proper LoopExit/LoopExitEffect/LoopExitValue to mark
  // {loop} as a candidate for loop peeling (crbug.com/v8/8273).
  control = graph()->NewNode(common()->LoopExit(), control, loop);
  effect = graph()->NewNode(common()->LoopExitEffect(), effect, control);
  value = graph()->NewNode(common()->LoopExitValue(), value, control);

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the
  // successful completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

namespace {

// Returns the correct Callable for Array's indexOf based on the receiver's
// |elements_kind| and |isolate|. Assumes that |elements_kind| is a fast one.
Callable GetCallableForArrayIndexOf(ElementsKind elements_kind,
                                    Isolate* isolate) {
  switch (elements_kind) {
    case PACKED_SMI_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case HOLEY_ELEMENTS:
      return Builtins::CallableFor(isolate, Builtins::kArrayIndexOfSmiOrObject);
    case PACKED_DOUBLE_ELEMENTS:
      return Builtins::CallableFor(isolate,
                                   Builtins::kArrayIndexOfPackedDoubles);
    default:
      DCHECK_EQ(HOLEY_DOUBLE_ELEMENTS, elements_kind);
      return Builtins::CallableFor(isolate,
                                   Builtins::kArrayIndexOfHoleyDoubles);
  }
}

// Returns the correct Callable for Array's includes based on the receiver's
// |elements_kind| and |isolate|. Assumes that |elements_kind| is a fast one.
Callable GetCallableForArrayIncludes(ElementsKind elements_kind,
                                     Isolate* isolate) {
  switch (elements_kind) {
    case PACKED_SMI_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case HOLEY_ELEMENTS:
      return Builtins::CallableFor(isolate,
                                   Builtins::kArrayIncludesSmiOrObject);
    case PACKED_DOUBLE_ELEMENTS:
      return Builtins::CallableFor(isolate,
                                   Builtins::kArrayIncludesPackedDoubles);
    default:
      DCHECK_EQ(HOLEY_DOUBLE_ELEMENTS, elements_kind);
      return Builtins::CallableFor(isolate,
                                   Builtins::kArrayIncludesHoleyDoubles);
  }
}

}  // namespace

// For search_variant == kIndexOf:
// ES6 Array.prototype.indexOf(searchElement[, fromIndex])
// #sec-array.prototype.indexof
// For search_variant == kIncludes:
// ES7 Array.prototype.inludes(searchElement[, fromIndex])
// #sec-array.prototype.includes
Reduction JSCallReducer::ReduceArrayIndexOfIncludes(
    SearchVariant search_variant, Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (IsHoleyElementsKind(kind)) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  Callable const callable = search_variant == SearchVariant::kIndexOf
                                ? GetCallableForArrayIndexOf(kind, isolate())
                                : GetCallableForArrayIncludes(kind, isolate());
  CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), CallDescriptor::kNoFlags,
      Operator::kEliminatable);
  // The stub expects the following arguments: the receiver array, its
  // elements, the search_element, the array length, and the index to start
  // searching from.
  Node* elements = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
      effect, control);
  Node* search_element = (node->op()->ValueInputCount() >= 3)
                             ? NodeProperties::GetValueInput(node, 2)
                             : jsgraph()->UndefinedConstant();
  Node* length = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)), receiver,
      effect, control);
  Node* new_from_index = jsgraph()->ZeroConstant();
  if (node->op()->ValueInputCount() >= 4) {
    Node* from_index = NodeProperties::GetValueInput(node, 3);
    from_index = effect = graph()->NewNode(simplified()->CheckSmi(p.feedback()),
                                           from_index, effect, control);
    // If the index is negative, it means the offset from the end and
    // therefore needs to be added to the length. If the result is still
    // negative, it needs to be clamped to 0.
    new_from_index = graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
        graph()->NewNode(simplified()->NumberLessThan(), from_index,
                         jsgraph()->ZeroConstant()),
        graph()->NewNode(
            simplified()->NumberMax(),
            graph()->NewNode(simplified()->NumberAdd(), length, from_index),
            jsgraph()->ZeroConstant()),
        from_index);
  }

  Node* context = NodeProperties::GetContextInput(node);
  Node* replacement_node = effect = graph()->NewNode(
      common()->Call(desc), jsgraph()->HeapConstant(callable.code()), elements,
      search_element, length, new_from_index, context, effect);
  ReplaceWithValue(node, replacement_node, effect);
  return Replace(replacement_node);
}

Reduction JSCallReducer::ReduceArraySome(Node* node,
                                         const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* fncallback = node->op()->ValueInputCount() > 2
                         ? NodeProperties::GetValueInput(node, 2)
                         : jsgraph()->UndefinedConstant();
  Node* this_arg = node->op()->ValueInputCount() > 3
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->UndefinedConstant();

  // Try to determine the {receiver} map.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  ElementsKind kind;
  if (!CanInlineArrayIteratingBuiltin(broker(), receiver_maps, &kind)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnArraySpeciesProtector())
    return inference.NoChange();
  if (IsHoleyElementsKind(kind)) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  bool const stability_dependency = inference.RelyOnMapsPreferStability(
      dependencies(), jsgraph(), &effect, control, p.feedback());

  Node* k = jsgraph()->ZeroConstant();
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
    // case.
    Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                                 original_length};
    const int stack_parameters = arraysize(checkpoint_params);

    Node* check_frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArraySomeLoopLazyDeoptContinuation,
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

  Node* continue_test =
      graph()->NewNode(simplified()->NumberLessThan(), k, original_length);
  Node* continue_branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                           continue_test, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), continue_branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), continue_branch);
  control = if_true;

  {
    Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                                 original_length};
    const int stack_parameters = arraysize(checkpoint_params);
    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArraySomeLoopEagerDeoptContinuation,
        node->InputAt(0), context, &checkpoint_params[0], stack_parameters,
        outer_frame_state, ContinuationFrameStateMode::EAGER);
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);
  }

  // Deopt if the map has changed during the iteration.
  if (!stability_dependency) {
    inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());
  }

  Node* element =
      SafeLoadElement(kind, receiver, control, &effect, &k, p.feedback());
  Node* next_k =
      graph()->NewNode(simplified()->NumberAdd(), k, jsgraph()->OneConstant());

  Node* hole_true = nullptr;
  Node* hole_false = nullptr;
  Node* effect_true = effect;

  if (IsHoleyElementsKind(kind)) {
    // Holey elements kind require a hole check and skipping of the element in
    // the case of a hole.
    Node* check;
    if (IsDoubleElementsKind(kind)) {
      check = graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
    } else {
      check = graph()->NewNode(simplified()->ReferenceEqual(), element,
                               jsgraph()->TheHoleConstant());
    }
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
    hole_true = graph()->NewNode(common()->IfTrue(), branch);
    hole_false = graph()->NewNode(common()->IfFalse(), branch);
    control = hole_false;

    // The contract is that we don't leak "the hole" into "user JavaScript",
    // so we must rename the {element} here to explicitly exclude "the hole"
    // from the type of {element}.
    element = effect = graph()->NewNode(
        common()->TypeGuard(Type::NonInternal()), element, effect, control);
  }

  Node* callback_value = nullptr;
  {
    // This frame state is dealt with by hand in
    // Builtins::kArrayEveryLoopLazyDeoptContinuation.
    Node* checkpoint_params[] = {receiver, fncallback, this_arg, k,
                                 original_length};
    const int stack_parameters = arraysize(checkpoint_params);

    Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
        jsgraph(), shared, Builtins::kArraySomeLoopLazyDeoptContinuation,
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

  // We have to coerce callback_value to boolean.
  Node* if_true_callback;
  Node* etrue_callback;
  {
    Node* boolean_result =
        graph()->NewNode(simplified()->ToBoolean(), callback_value);
    Node* boolean_branch = graph()->NewNode(
        common()->Branch(BranchHint::kFalse), boolean_result, control);
    if_true_callback = graph()->NewNode(common()->IfTrue(), boolean_branch);
    etrue_callback = effect;

    // Nothing to do in the false case.
    control = graph()->NewNode(common()->IfFalse(), boolean_branch);
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

  loop->ReplaceInput(1, control);
  vloop->ReplaceInput(1, next_k);
  eloop->ReplaceInput(1, effect);

  control = graph()->NewNode(common()->Merge(2), if_false, if_true_callback);
  effect =
      graph()->NewNode(common()->EffectPhi(2), eloop, etrue_callback, control);
  Node* value = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2),
      jsgraph()->FalseConstant(), jsgraph()->TrueConstant(), control);

  // Introduce proper LoopExit/LoopExitEffect/LoopExitValue to mark
  // {loop} as a candidate for loop peeling (crbug.com/v8/8273).
  control = graph()->NewNode(common()->LoopExit(), control, loop);
  effect = graph()->NewNode(common()->LoopExitEffect(), effect, control);
  value = graph()->NewNode(common()->LoopExitValue(), value, control);

  // Wire up the branch for the case when IsCallable fails for the callback.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the
  // successful completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSCallReducer::ReduceCallApiFunction(
    Node* node, const SharedFunctionInfoRef& shared) {
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int const argc = static_cast<int>(p.arity()) - 2;
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* global_proxy =
      jsgraph()->Constant(native_context().global_proxy_object());
  Node* receiver = (p.convert_mode() == ConvertReceiverMode::kNullOrUndefined)
                       ? global_proxy
                       : NodeProperties::GetValueInput(node, 1);
  Node* holder;
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);

  if (!shared.function_template_info().has_value()) {
    TRACE_BROKER_MISSING(
        broker(), "FunctionTemplateInfo for function with SFI " << shared);
    return NoChange();
  }

  // See if we can optimize this API call to {shared}.
  FunctionTemplateInfoRef function_template_info(
      shared.function_template_info().value());

  if (!function_template_info.has_call_code()) return NoChange();

  if (function_template_info.accept_any_receiver() &&
      function_template_info.is_signature_undefined()) {
    // We might be able to
    // optimize the API call depending on the {function_template_info}.
    // If the API function accepts any kind of {receiver}, we only need to
    // ensure that the {receiver} is actually a JSReceiver at this point,
    // and also pass that as the {holder}. There are two independent bits
    // here:
    //
    //  a. When the "accept any receiver" bit is set, it means we don't
    //     need to perform access checks, even if the {receiver}'s map
    //     has the "needs access check" bit set.
    //  b. When the {function_template_info} has no signature, we don't
    //     need to do the compatible receiver check, since all receivers
    //     are considered compatible at that point, and the {receiver}
    //     will be pass as the {holder}.
    //
    receiver = holder = effect =
        graph()->NewNode(simplified()->ConvertReceiver(p.convert_mode()),
                         receiver, global_proxy, effect, control);
  } else {
    // Try to infer the {receiver} maps from the graph.
    MapInference inference(broker(), receiver, effect);
    if (inference.HaveMaps()) {
      MapHandles const& receiver_maps = inference.GetMaps();
      MapRef first_receiver_map(broker(), receiver_maps[0]);

      // See if we can constant-fold the compatible receiver checks.
      HolderLookupResult api_holder =
          function_template_info.LookupHolderOfExpectedType(first_receiver_map);
      if (api_holder.lookup == CallOptimization::kHolderNotFound)
        return inference.NoChange();

      // Check that all {receiver_maps} are actually JSReceiver maps and
      // that the {function_template_info} accepts them without access
      // checks (even if "access check needed" is set for {receiver}).
      //
      // Note that we don't need to know the concrete {receiver} maps here,
      // meaning it's fine if the {receiver_maps} are unreliable, and we also
      // don't need to install any stability dependencies, since the only
      // relevant information regarding the {receiver} is the Map::constructor
      // field on the root map (which is different from the JavaScript exposed
      // "constructor" property) and that field cannot change.
      //
      // So if we know that {receiver} had a certain constructor at some point
      // in the past (i.e. it had a certain map), then this constructor is going
      // to be the same later, since this information cannot change with map
      // transitions.
      //
      // The same is true for the instance type, e.g. we still know that the
      // instance type is JSObject even if that information is unreliable, and
      // the "access check needed" bit, which also cannot change later.
      CHECK(first_receiver_map.IsJSReceiverMap());
      CHECK(!first_receiver_map.is_access_check_needed() ||
            function_template_info.accept_any_receiver());

      for (size_t i = 1; i < receiver_maps.size(); ++i) {
        MapRef receiver_map(broker(), receiver_maps[i]);
        HolderLookupResult holder_i =
            function_template_info.LookupHolderOfExpectedType(receiver_map);

        if (api_holder.lookup != holder_i.lookup) return inference.NoChange();
        if (!(api_holder.holder.has_value() && holder_i.holder.has_value()))
          return inference.NoChange();
        if (!api_holder.holder->equals(*holder_i.holder))
          return inference.NoChange();

        CHECK(receiver_map.IsJSReceiverMap());
        CHECK(!receiver_map.is_access_check_needed() ||
              function_template_info.accept_any_receiver());
      }

      if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation &&
          !inference.RelyOnMapsViaStability(dependencies())) {
        // We were not able to make the receiver maps reliable without map
        // checks but doing map checks would lead to deopt loops, so give up.
        return inference.NoChange();
      }

      // TODO(neis): The maps were used in a way that does not actually require
      // map checks or stability dependencies.
      inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                          control, p.feedback());

      // Determine the appropriate holder for the {lookup}.
      holder = api_holder.lookup == CallOptimization::kHolderFound
                   ? jsgraph()->Constant(*api_holder.holder)
                   : receiver;
    } else {
      // We don't have enough information to eliminate the access check
      // and/or the compatible receiver check, so use the generic builtin
      // that does those checks dynamically. This is still significantly
      // faster than the generic call sequence.
      Builtins::Name builtin_name;
      if (function_template_info.accept_any_receiver()) {
        builtin_name = Builtins::kCallFunctionTemplate_CheckCompatibleReceiver;
      } else if (function_template_info.is_signature_undefined()) {
        builtin_name = Builtins::kCallFunctionTemplate_CheckAccess;
      } else {
        builtin_name =
            Builtins::kCallFunctionTemplate_CheckAccessAndCompatibleReceiver;
      }

      // The CallFunctionTemplate builtin requires the {receiver} to be
      // an actual JSReceiver, so make sure we do the proper conversion
      // first if necessary.
      receiver = holder = effect =
          graph()->NewNode(simplified()->ConvertReceiver(p.convert_mode()),
                           receiver, global_proxy, effect, control);

      Callable callable = Builtins::CallableFor(isolate(), builtin_name);
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          graph()->zone(), callable.descriptor(),
          argc + 1 /* implicit receiver */, CallDescriptor::kNeedsFrameState);
      node->InsertInput(graph()->zone(), 0,
                        jsgraph()->HeapConstant(callable.code()));
      node->ReplaceInput(1, jsgraph()->Constant(function_template_info));
      node->InsertInput(graph()->zone(), 2, jsgraph()->Constant(argc));
      node->ReplaceInput(3, receiver);       // Update receiver input.
      node->ReplaceInput(6 + argc, effect);  // Update effect input.
      NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
      return Changed(node);
    }
  }

  // TODO(turbofan): Consider introducing a JSCallApiCallback operator for
  // this and lower it during JSGenericLowering, and unify this with the
  // JSNativeContextSpecialization::InlineApiCall method a bit.
  if (!function_template_info.call_code().has_value()) {
    TRACE_BROKER_MISSING(broker(), "call code for function template info "
                                       << function_template_info);
    return NoChange();
  }
  CallHandlerInfoRef call_handler_info = *function_template_info.call_code();
  Callable call_api_callback = CodeFactory::CallApiCallback(isolate());
  CallInterfaceDescriptor cid = call_api_callback.descriptor();
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), cid, argc + 1 /* implicit receiver */,
      CallDescriptor::kNeedsFrameState);
  ApiFunction api_function(call_handler_info.callback());
  ExternalReference function_reference = ExternalReference::Create(
      &api_function, ExternalReference::DIRECT_API_CALL);

  Node* continuation_frame_state = CreateGenericLazyDeoptContinuationFrameState(
      jsgraph(), shared, target, context, receiver, frame_state);

  node->InsertInput(graph()->zone(), 0,
                    jsgraph()->HeapConstant(call_api_callback.code()));
  node->ReplaceInput(1, jsgraph()->ExternalConstant(function_reference));
  node->InsertInput(graph()->zone(), 2, jsgraph()->Constant(argc));
  node->InsertInput(graph()->zone(), 3,
                    jsgraph()->Constant(call_handler_info.data()));
  node->InsertInput(graph()->zone(), 4, holder);
  node->ReplaceInput(5, receiver);       // Update receiver input.
  node->ReplaceInput(7 + argc, continuation_frame_state);
  node->ReplaceInput(8 + argc, effect);  // Update effect input.
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  return Changed(node);
}

namespace {

// Check whether elements aren't mutated; we play it extremely safe here by
// explicitly checking that {node} is only used by {LoadField} or
// {LoadElement}.
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
    FeedbackSource const& feedback) {
  DCHECK(node->opcode() == IrOpcode::kJSCallWithArrayLike ||
         node->opcode() == IrOpcode::kJSCallWithSpread ||
         node->opcode() == IrOpcode::kJSConstructWithArrayLike ||
         node->opcode() == IrOpcode::kJSConstructWithSpread);

  Node* arguments_list = NodeProperties::GetValueInput(node, arity);
  if (arguments_list->opcode() != IrOpcode::kJSCreateArguments) {
    return NoChange();
  }

  // Check if {node} is the only value user of {arguments_list} (except for
  // value uses in frame states). If not, we give up for now.
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
          STATIC_ASSERT(
              static_cast<int>(JSArray::kLengthOffset) ==
              static_cast<int>(JSArgumentsObjectWithLength::kLengthOffset));
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
  FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
  int start_index = 0;

  int formal_parameter_count;
  {
    Handle<SharedFunctionInfo> shared;
    if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
    formal_parameter_count = SharedFunctionInfoRef(broker(), shared)
                                 .internal_formal_parameter_count();
  }

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
  }

  // For call/construct with spread, we need to also install a code
  // dependency on the array iterator lookup protector cell to ensure
  // that no one messed with the %ArrayIteratorPrototype%.next method.
  if (node->opcode() == IrOpcode::kJSCallWithSpread ||
      node->opcode() == IrOpcode::kJSConstructWithSpread) {
    if (!dependencies()->DependOnArrayIteratorProtector()) return NoChange();
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
  FrameStateInfo outer_info = FrameStateInfoOf(outer_state->op());
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
    Node* check_throw = check_fail = graph()->NewNode(
        javascript()->CallRuntime(Runtime::kThrowTypeError, 2),
        jsgraph()->Constant(static_cast<int>(MessageTemplate::kNotConstructor)),
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

    // The above %ThrowTypeError runtime call is an unconditional throw,
    // making it impossible to return a successful completion in this case. We
    // simply connect the successful completion to the graph end.
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
    ObjectRef target_ref = m.Ref(broker());
    if (target_ref.IsJSFunction()) {
      JSFunctionRef function = target_ref.AsJSFunction();
      if (FLAG_concurrent_inlining && !function.serialized()) {
        TRACE_BROKER_MISSING(broker(), "data for function " << function);
        return NoChange();
      }

      // Don't inline cross native context.
      if (!function.native_context().equals(native_context())) {
        return NoChange();
      }

      return ReduceJSCall(node, function.shared());
    } else if (target_ref.IsJSBoundFunction()) {
      JSBoundFunctionRef function = target_ref.AsJSBoundFunction();
      if (FLAG_concurrent_inlining && !function.serialized()) {
        TRACE_BROKER_MISSING(broker(), "data for function " << function);
        return NoChange();
      }

      ObjectRef bound_this = function.bound_this();
      ConvertReceiverMode const convert_mode =
          bound_this.IsNullOrUndefined()
              ? ConvertReceiverMode::kNullOrUndefined
              : ConvertReceiverMode::kNotNullOrUndefined;

      // Patch {node} to use [[BoundTargetFunction]] and [[BoundThis]].
      NodeProperties::ReplaceValueInput(
          node, jsgraph()->Constant(function.bound_target_function()), 0);
      NodeProperties::ReplaceValueInput(node, jsgraph()->Constant(bound_this),
                                        1);

      // Insert the [[BoundArguments]] for {node}.
      FixedArrayRef bound_arguments = function.bound_arguments();
      for (int i = 0; i < bound_arguments.length(); ++i) {
        node->InsertInput(graph()->zone(), i + 2,
                          jsgraph()->Constant(bound_arguments.get(i)));
        arity++;
      }

      NodeProperties::ChangeOp(
          node, javascript()->Call(arity, p.frequency(), FeedbackSource(),
                                   convert_mode));

      // Try to further reduce the JSCall {node}.
      Reduction const reduction = ReduceJSCall(node);
      return reduction.Changed() ? reduction : Changed(node);
    }

    // Don't mess with other {node}s that have a constant {target}.
    // TODO(bmeurer): Also support proxies here.
    return NoChange();
  }

  // If {target} is the result of a JSCreateClosure operation, we can
  // just immediately try to inline based on the SharedFunctionInfo,
  // since TurboFan generally doesn't inline cross-context, and hence
  // the {target} must have the same native context as the call site.
  if (target->opcode() == IrOpcode::kJSCreateClosure) {
    CreateClosureParameters const& p = CreateClosureParametersOf(target->op());
    return ReduceJSCall(node, SharedFunctionInfoRef(broker(), p.shared_info()));
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
        NodeProperties::CanBeNullOrUndefined(broker(), bound_this, effect)
            ? ConvertReceiverMode::kAny
            : ConvertReceiverMode::kNotNullOrUndefined;
    NodeProperties::ChangeOp(
        node, javascript()->Call(arity, p.frequency(), FeedbackSource(),
                                 convert_mode));

    // Try to further reduce the JSCall {node}.
    Reduction const reduction = ReduceJSCall(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  if (!p.feedback().IsValid()) return NoChange();
  ProcessedFeedback const& feedback =
      broker()->GetFeedbackForCall(FeedbackSource(p.feedback()));
  if (feedback.IsInsufficient()) {
    return ReduceSoftDeoptimize(
        node, DeoptimizeReason::kInsufficientTypeFeedbackForCall);
  }

  base::Optional<HeapObjectRef> feedback_target = feedback.AsCall().target();
  if (feedback_target.has_value() && ShouldUseCallICFeedback(target) &&
      feedback_target->map().is_callable()) {
    Node* target_function = jsgraph()->Constant(*feedback_target);

    // Check that the {target} is still the {target_function}.
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                   target_function);
    effect = graph()->NewNode(
        simplified()->CheckIf(DeoptimizeReason::kWrongCallTarget), check,
        effect, control);

    // Specialize the JSCall node to the {target_function}.
    NodeProperties::ReplaceValueInput(node, target_function, 0);
    NodeProperties::ReplaceEffectInput(node, effect);

    // Try to further reduce the JSCall {node}.
    Reduction const reduction = ReduceJSCall(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  return NoChange();
}

Reduction JSCallReducer::ReduceJSCall(Node* node,
                                      const SharedFunctionInfoRef& shared) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* target = NodeProperties::GetValueInput(node, 0);

  // Do not reduce calls to functions with break points.
  if (shared.HasBreakInfo()) return NoChange();

  // Raise a TypeError if the {target} is a "classConstructor".
  if (IsClassConstructor(shared.kind())) {
    NodeProperties::ReplaceValueInputs(node, target);
    NodeProperties::ChangeOp(
        node, javascript()->CallRuntime(
                  Runtime::kThrowConstructorNonCallableError, 1));
    return Changed(node);
  }

  // Check for known builtin functions.

  int builtin_id =
      shared.HasBuiltinId() ? shared.builtin_id() : Builtins::kNoBuiltinId;
  switch (builtin_id) {
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
    case Builtins::kObjectConstructor:
      return ReduceObjectConstructor(node);
    case Builtins::kObjectCreate:
      return ReduceObjectCreate(node);
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
      return ReduceArrayForEach(node, shared);
    case Builtins::kArrayMap:
      return ReduceArrayMap(node, shared);
    case Builtins::kArrayFilter:
      return ReduceArrayFilter(node, shared);
    case Builtins::kArrayReduce:
      return ReduceArrayReduce(node, ArrayReduceDirection::kLeft, shared);
    case Builtins::kArrayReduceRight:
      return ReduceArrayReduce(node, ArrayReduceDirection::kRight, shared);
    case Builtins::kArrayPrototypeFind:
      return ReduceArrayFind(node, ArrayFindVariant::kFind, shared);
    case Builtins::kArrayPrototypeFindIndex:
      return ReduceArrayFind(node, ArrayFindVariant::kFindIndex, shared);
    case Builtins::kArrayEvery:
      return ReduceArrayEvery(node, shared);
    case Builtins::kArrayIndexOf:
      return ReduceArrayIndexOfIncludes(SearchVariant::kIndexOf, node);
    case Builtins::kArrayIncludes:
      return ReduceArrayIndexOfIncludes(SearchVariant::kIncludes, node);
    case Builtins::kArraySome:
      return ReduceArraySome(node, shared);
    case Builtins::kArrayPrototypePush:
      return ReduceArrayPrototypePush(node);
    case Builtins::kArrayPrototypePop:
      return ReduceArrayPrototypePop(node);
    case Builtins::kArrayPrototypeShift:
      return ReduceArrayPrototypeShift(node);
    case Builtins::kArrayPrototypeSlice:
      return ReduceArrayPrototypeSlice(node);
    case Builtins::kArrayPrototypeEntries:
      return ReduceArrayIterator(node, IterationKind::kEntries);
    case Builtins::kArrayPrototypeKeys:
      return ReduceArrayIterator(node, IterationKind::kKeys);
    case Builtins::kArrayPrototypeValues:
      return ReduceArrayIterator(node, IterationKind::kValues);
    case Builtins::kArrayIteratorPrototypeNext:
      return ReduceArrayIteratorPrototypeNext(node);
    case Builtins::kArrayIsArray:
      return ReduceArrayIsArray(node);
    case Builtins::kArrayBufferIsView:
      return ReduceArrayBufferIsView(node);
    case Builtins::kDataViewPrototypeGetByteLength:
      return ReduceArrayBufferViewAccessor(
          node, JS_DATA_VIEW_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteLength());
    case Builtins::kDataViewPrototypeGetByteOffset:
      return ReduceArrayBufferViewAccessor(
          node, JS_DATA_VIEW_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteOffset());
    case Builtins::kDataViewPrototypeGetUint8:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalUint8Array);
    case Builtins::kDataViewPrototypeGetInt8:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalInt8Array);
    case Builtins::kDataViewPrototypeGetUint16:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalUint16Array);
    case Builtins::kDataViewPrototypeGetInt16:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalInt16Array);
    case Builtins::kDataViewPrototypeGetUint32:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalUint32Array);
    case Builtins::kDataViewPrototypeGetInt32:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalInt32Array);
    case Builtins::kDataViewPrototypeGetFloat32:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalFloat32Array);
    case Builtins::kDataViewPrototypeGetFloat64:
      return ReduceDataViewAccess(node, DataViewAccess::kGet,
                                  ExternalArrayType::kExternalFloat64Array);
    case Builtins::kDataViewPrototypeSetUint8:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalUint8Array);
    case Builtins::kDataViewPrototypeSetInt8:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalInt8Array);
    case Builtins::kDataViewPrototypeSetUint16:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalUint16Array);
    case Builtins::kDataViewPrototypeSetInt16:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalInt16Array);
    case Builtins::kDataViewPrototypeSetUint32:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalUint32Array);
    case Builtins::kDataViewPrototypeSetInt32:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalInt32Array);
    case Builtins::kDataViewPrototypeSetFloat32:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalFloat32Array);
    case Builtins::kDataViewPrototypeSetFloat64:
      return ReduceDataViewAccess(node, DataViewAccess::kSet,
                                  ExternalArrayType::kExternalFloat64Array);
    case Builtins::kTypedArrayPrototypeByteLength:
      return ReduceArrayBufferViewAccessor(
          node, JS_TYPED_ARRAY_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteLength());
    case Builtins::kTypedArrayPrototypeByteOffset:
      return ReduceArrayBufferViewAccessor(
          node, JS_TYPED_ARRAY_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteOffset());
    case Builtins::kTypedArrayPrototypeLength:
      return ReduceArrayBufferViewAccessor(
          node, JS_TYPED_ARRAY_TYPE, AccessBuilder::ForJSTypedArrayLength());
    case Builtins::kTypedArrayPrototypeToStringTag:
      return ReduceTypedArrayPrototypeToStringTag(node);
    case Builtins::kMathAbs:
      return ReduceMathUnary(node, simplified()->NumberAbs());
    case Builtins::kMathAcos:
      return ReduceMathUnary(node, simplified()->NumberAcos());
    case Builtins::kMathAcosh:
      return ReduceMathUnary(node, simplified()->NumberAcosh());
    case Builtins::kMathAsin:
      return ReduceMathUnary(node, simplified()->NumberAsin());
    case Builtins::kMathAsinh:
      return ReduceMathUnary(node, simplified()->NumberAsinh());
    case Builtins::kMathAtan:
      return ReduceMathUnary(node, simplified()->NumberAtan());
    case Builtins::kMathAtanh:
      return ReduceMathUnary(node, simplified()->NumberAtanh());
    case Builtins::kMathCbrt:
      return ReduceMathUnary(node, simplified()->NumberCbrt());
    case Builtins::kMathCeil:
      return ReduceMathUnary(node, simplified()->NumberCeil());
    case Builtins::kMathCos:
      return ReduceMathUnary(node, simplified()->NumberCos());
    case Builtins::kMathCosh:
      return ReduceMathUnary(node, simplified()->NumberCosh());
    case Builtins::kMathExp:
      return ReduceMathUnary(node, simplified()->NumberExp());
    case Builtins::kMathExpm1:
      return ReduceMathUnary(node, simplified()->NumberExpm1());
    case Builtins::kMathFloor:
      return ReduceMathUnary(node, simplified()->NumberFloor());
    case Builtins::kMathFround:
      return ReduceMathUnary(node, simplified()->NumberFround());
    case Builtins::kMathLog:
      return ReduceMathUnary(node, simplified()->NumberLog());
    case Builtins::kMathLog1p:
      return ReduceMathUnary(node, simplified()->NumberLog1p());
    case Builtins::kMathLog10:
      return ReduceMathUnary(node, simplified()->NumberLog10());
    case Builtins::kMathLog2:
      return ReduceMathUnary(node, simplified()->NumberLog2());
    case Builtins::kMathRound:
      return ReduceMathUnary(node, simplified()->NumberRound());
    case Builtins::kMathSign:
      return ReduceMathUnary(node, simplified()->NumberSign());
    case Builtins::kMathSin:
      return ReduceMathUnary(node, simplified()->NumberSin());
    case Builtins::kMathSinh:
      return ReduceMathUnary(node, simplified()->NumberSinh());
    case Builtins::kMathSqrt:
      return ReduceMathUnary(node, simplified()->NumberSqrt());
    case Builtins::kMathTan:
      return ReduceMathUnary(node, simplified()->NumberTan());
    case Builtins::kMathTanh:
      return ReduceMathUnary(node, simplified()->NumberTanh());
    case Builtins::kMathTrunc:
      return ReduceMathUnary(node, simplified()->NumberTrunc());
    case Builtins::kMathAtan2:
      return ReduceMathBinary(node, simplified()->NumberAtan2());
    case Builtins::kMathPow:
      return ReduceMathBinary(node, simplified()->NumberPow());
    case Builtins::kMathClz32:
      return ReduceMathClz32(node);
    case Builtins::kMathImul:
      return ReduceMathImul(node);
    case Builtins::kMathMax:
      return ReduceMathMinMax(node, simplified()->NumberMax(),
                              jsgraph()->Constant(-V8_INFINITY));
    case Builtins::kMathMin:
      return ReduceMathMinMax(node, simplified()->NumberMin(),
                              jsgraph()->Constant(V8_INFINITY));
    case Builtins::kNumberIsFinite:
      return ReduceNumberIsFinite(node);
    case Builtins::kNumberIsInteger:
      return ReduceNumberIsInteger(node);
    case Builtins::kNumberIsSafeInteger:
      return ReduceNumberIsSafeInteger(node);
    case Builtins::kNumberIsNaN:
      return ReduceNumberIsNaN(node);
    case Builtins::kNumberParseInt:
      return ReduceNumberParseInt(node);
    case Builtins::kGlobalIsFinite:
      return ReduceGlobalIsFinite(node);
    case Builtins::kGlobalIsNaN:
      return ReduceGlobalIsNaN(node);
    case Builtins::kMapPrototypeGet:
      return ReduceMapPrototypeGet(node);
    case Builtins::kMapPrototypeHas:
      return ReduceMapPrototypeHas(node);
    case Builtins::kRegExpPrototypeTest:
      return ReduceRegExpPrototypeTest(node);
    case Builtins::kReturnReceiver:
      return ReduceReturnReceiver(node);
    case Builtins::kStringPrototypeIndexOf:
      return ReduceStringPrototypeIndexOf(node);
    case Builtins::kStringPrototypeCharAt:
      return ReduceStringPrototypeCharAt(node);
    case Builtins::kStringPrototypeCharCodeAt:
      return ReduceStringPrototypeStringAt(simplified()->StringCharCodeAt(),
                                           node);
    case Builtins::kStringPrototypeCodePointAt:
      return ReduceStringPrototypeStringAt(simplified()->StringCodePointAt(),
                                           node);
    case Builtins::kStringPrototypeSubstring:
      return ReduceStringPrototypeSubstring(node);
    case Builtins::kStringPrototypeSlice:
      return ReduceStringPrototypeSlice(node);
    case Builtins::kStringPrototypeSubstr:
      return ReduceStringPrototypeSubstr(node);
    case Builtins::kStringPrototypeStartsWith:
      return ReduceStringPrototypeStartsWith(node);
#ifdef V8_INTL_SUPPORT
    case Builtins::kStringPrototypeToLowerCaseIntl:
      return ReduceStringPrototypeToLowerCaseIntl(node);
    case Builtins::kStringPrototypeToUpperCaseIntl:
      return ReduceStringPrototypeToUpperCaseIntl(node);
#endif  // V8_INTL_SUPPORT
    case Builtins::kStringFromCharCode:
      return ReduceStringFromCharCode(node);
    case Builtins::kStringFromCodePoint:
      return ReduceStringFromCodePoint(node);
    case Builtins::kStringPrototypeIterator:
      return ReduceStringPrototypeIterator(node);
    case Builtins::kStringIteratorPrototypeNext:
      return ReduceStringIteratorPrototypeNext(node);
    case Builtins::kStringPrototypeConcat:
      return ReduceStringPrototypeConcat(node);
    case Builtins::kTypedArrayPrototypeEntries:
      return ReduceArrayIterator(node, IterationKind::kEntries);
    case Builtins::kTypedArrayPrototypeKeys:
      return ReduceArrayIterator(node, IterationKind::kKeys);
    case Builtins::kTypedArrayPrototypeValues:
      return ReduceArrayIterator(node, IterationKind::kValues);
    case Builtins::kPromiseInternalConstructor:
      return ReducePromiseInternalConstructor(node);
    case Builtins::kPromiseInternalReject:
      return ReducePromiseInternalReject(node);
    case Builtins::kPromiseInternalResolve:
      return ReducePromiseInternalResolve(node);
    case Builtins::kPromisePrototypeCatch:
      return ReducePromisePrototypeCatch(node);
    case Builtins::kPromisePrototypeFinally:
      return ReducePromisePrototypeFinally(node);
    case Builtins::kPromisePrototypeThen:
      return ReducePromisePrototypeThen(node);
    case Builtins::kPromiseResolveTrampoline:
      return ReducePromiseResolveTrampoline(node);
    case Builtins::kMapPrototypeEntries:
      return ReduceCollectionIteration(node, CollectionKind::kMap,
                                       IterationKind::kEntries);
    case Builtins::kMapPrototypeKeys:
      return ReduceCollectionIteration(node, CollectionKind::kMap,
                                       IterationKind::kKeys);
    case Builtins::kMapPrototypeGetSize:
      return ReduceCollectionPrototypeSize(node, CollectionKind::kMap);
    case Builtins::kMapPrototypeValues:
      return ReduceCollectionIteration(node, CollectionKind::kMap,
                                       IterationKind::kValues);
    case Builtins::kMapIteratorPrototypeNext:
      return ReduceCollectionIteratorPrototypeNext(
          node, OrderedHashMap::kEntrySize, factory()->empty_ordered_hash_map(),
          FIRST_JS_MAP_ITERATOR_TYPE, LAST_JS_MAP_ITERATOR_TYPE);
    case Builtins::kSetPrototypeEntries:
      return ReduceCollectionIteration(node, CollectionKind::kSet,
                                       IterationKind::kEntries);
    case Builtins::kSetPrototypeGetSize:
      return ReduceCollectionPrototypeSize(node, CollectionKind::kSet);
    case Builtins::kSetPrototypeValues:
      return ReduceCollectionIteration(node, CollectionKind::kSet,
                                       IterationKind::kValues);
    case Builtins::kSetIteratorPrototypeNext:
      return ReduceCollectionIteratorPrototypeNext(
          node, OrderedHashSet::kEntrySize, factory()->empty_ordered_hash_set(),
          FIRST_JS_SET_ITERATOR_TYPE, LAST_JS_SET_ITERATOR_TYPE);
    case Builtins::kDatePrototypeGetTime:
      return ReduceDatePrototypeGetTime(node);
    case Builtins::kDateNow:
      return ReduceDateNow(node);
    case Builtins::kNumberConstructor:
      return ReduceNumberConstructor(node);
    case Builtins::kBigIntAsUintN:
      return ReduceBigIntAsUintN(node);
    default:
      break;
  }

  if (shared.function_template_info().has_value()) {
    return ReduceCallApiFunction(node, shared);
  }
  return NoChange();
}

Reduction JSCallReducer::ReduceJSCallWithArrayLike(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallWithArrayLike, node->opcode());
  CallFrequency frequency = CallFrequencyOf(node->op());
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, 2, frequency,
                                                    FeedbackSource());
}

Reduction JSCallReducer::ReduceJSCallWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallWithSpread, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 1);
  CallFrequency frequency = p.frequency();
  FeedbackSource feedback = p.feedback();
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

  if (p.feedback().IsValid()) {
    ProcessedFeedback const& feedback =
        broker()->GetFeedbackForCall(p.feedback());
    if (feedback.IsInsufficient()) {
      return ReduceSoftDeoptimize(
          node, DeoptimizeReason::kInsufficientTypeFeedbackForConstruct);
    }

    base::Optional<HeapObjectRef> feedback_target = feedback.AsCall().target();
    if (feedback_target.has_value() && feedback_target->IsAllocationSite()) {
      // The feedback is an AllocationSite, which means we have called the
      // Array function and collected transition (and pretenuring) feedback
      // for the resulting arrays.  This has to be kept in sync with the
      // implementation in Ignition.

      Node* array_function =
          jsgraph()->Constant(native_context().array_function());

      // Check that the {target} is still the {array_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     array_function);
      effect = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kWrongCallTarget), check,
          effect, control);

      // Turn the {node} into a {JSCreateArray} call.
      NodeProperties::ReplaceEffectInput(node, effect);
      for (int i = arity; i > 0; --i) {
        NodeProperties::ReplaceValueInput(
            node, NodeProperties::GetValueInput(node, i), i + 1);
      }
      NodeProperties::ReplaceValueInput(node, array_function, 1);
      NodeProperties::ChangeOp(
          node, javascript()->CreateArray(
                    arity, feedback_target->AsAllocationSite().object()));
      return Changed(node);
    } else if (feedback_target.has_value() &&
               !HeapObjectMatcher(new_target).HasValue() &&
               feedback_target->map().is_constructor()) {
      Node* new_target_feedback = jsgraph()->Constant(*feedback_target);

      // Check that the {new_target} is still the {new_target_feedback}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), new_target,
                                     new_target_feedback);
      effect = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kWrongCallTarget), check,
          effect, control);

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

  // Try to specialize JSConstruct {node}s with constant {target}s.
  HeapObjectMatcher m(target);
  if (m.HasValue()) {
    HeapObjectRef target_ref = m.Ref(broker());

    // Raise a TypeError if the {target} is not a constructor.
    if (!target_ref.map().is_constructor()) {
      NodeProperties::ReplaceValueInputs(node, target);
      NodeProperties::ChangeOp(node,
                               javascript()->CallRuntime(
                                   Runtime::kThrowConstructedNonConstructable));
      return Changed(node);
    }

    if (target_ref.IsJSFunction()) {
      JSFunctionRef function = target_ref.AsJSFunction();
      if (FLAG_concurrent_inlining && !function.serialized()) {
        TRACE_BROKER_MISSING(broker(),
                             "function, not serialized: " << function);
        return NoChange();
      }

      // Do not reduce constructors with break points.
      if (function.shared().HasBreakInfo()) return NoChange();

      // Don't inline cross native context.
      if (!function.native_context().equals(native_context())) {
        return NoChange();
      }

      // Check for known builtin functions.
      int builtin_id = function.shared().HasBuiltinId()
                           ? function.shared().builtin_id()
                           : Builtins::kNoBuiltinId;
      switch (builtin_id) {
        case Builtins::kArrayConstructor: {
          // TODO(bmeurer): Deal with Array subclasses here.
          // Turn the {node} into a {JSCreateArray} call.
          for (int i = arity; i > 0; --i) {
            NodeProperties::ReplaceValueInput(
                node, NodeProperties::GetValueInput(node, i), i + 1);
          }
          NodeProperties::ReplaceValueInput(node, new_target, 1);
          NodeProperties::ChangeOp(
              node, javascript()->CreateArray(arity, Handle<AllocationSite>()));
          return Changed(node);
        }
        case Builtins::kObjectConstructor: {
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
          if (mnew_target.HasValue() &&
              !mnew_target.Ref(broker()).equals(function)) {
            // Drop the value inputs.
            for (int i = arity; i > 0; --i) node->RemoveInput(i);
            NodeProperties::ChangeOp(node, javascript()->Create());
            return Changed(node);
          }
          break;
        }
        case Builtins::kPromiseConstructor:
          return ReducePromiseConstructor(node);
        case Builtins::kTypedArrayConstructor:
          return ReduceTypedArrayConstructor(node, function.shared());
        default:
          break;
      }
    } else if (target_ref.IsJSBoundFunction()) {
      JSBoundFunctionRef function = target_ref.AsJSBoundFunction();
      if (FLAG_concurrent_inlining && !function.serialized()) {
        TRACE_BROKER_MISSING(broker(),
                             "function, not serialized: " << function);
        return NoChange();
      }

      ObjectRef bound_target_function = function.bound_target_function();
      FixedArrayRef bound_arguments = function.bound_arguments();

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
      for (int i = 0; i < bound_arguments.length(); ++i) {
        node->InsertInput(graph()->zone(), i + 1,
                          jsgraph()->Constant(bound_arguments.get(i)));
        arity++;
      }

      // Update the JSConstruct operator on {node}.
      NodeProperties::ChangeOp(
          node,
          javascript()->Construct(arity + 2, p.frequency(), FeedbackSource()));

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
        javascript()->Construct(arity + 2, p.frequency(), FeedbackSource()));

    // Try to further reduce the JSConstruct {node}.
    Reduction const reduction = ReduceJSConstruct(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  return NoChange();
}

// ES #sec-string.prototype.indexof
Reduction JSCallReducer::ReduceStringPrototypeIndexOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (node->op()->ValueInputCount() >= 3) {
    Node* receiver = NodeProperties::GetValueInput(node, 1);
    Node* new_receiver = effect = graph()->NewNode(
        simplified()->CheckString(p.feedback()), receiver, effect, control);

    Node* search_string = NodeProperties::GetValueInput(node, 2);
    Node* new_search_string = effect =
        graph()->NewNode(simplified()->CheckString(p.feedback()), search_string,
                         effect, control);

    Node* new_position = jsgraph()->ZeroConstant();
    if (node->op()->ValueInputCount() >= 4) {
      Node* position = NodeProperties::GetValueInput(node, 3);
      new_position = effect = graph()->NewNode(
          simplified()->CheckSmi(p.feedback()), position, effect, control);
    }

    NodeProperties::ReplaceEffectInput(node, effect);
    RelaxEffectsAndControls(node);
    node->ReplaceInput(0, new_receiver);
    node->ReplaceInput(1, new_search_string);
    node->ReplaceInput(2, new_position);
    node->TrimInputCount(3);
    NodeProperties::ChangeOp(node, simplified()->StringIndexOf());
    return Changed(node);
  }
  return NoChange();
}

// ES #sec-string.prototype.substring
Reduction JSCallReducer::ReduceStringPrototypeSubstring(Node* node) {
  if (node->op()->ValueInputCount() < 3) return NoChange();
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* start = NodeProperties::GetValueInput(node, 2);
  Node* end = node->op()->ValueInputCount() > 3
                  ? NodeProperties::GetValueInput(node, 3)
                  : jsgraph()->UndefinedConstant();

  receiver = effect = graph()->NewNode(simplified()->CheckString(p.feedback()),
                                       receiver, effect, control);

  start = effect = graph()->NewNode(simplified()->CheckSmi(p.feedback()), start,
                                    effect, control);

  Node* length = graph()->NewNode(simplified()->StringLength(), receiver);

  Node* check = graph()->NewNode(simplified()->ReferenceEqual(), end,
                                 jsgraph()->UndefinedConstant());
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = length;

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse = efalse = graph()->NewNode(simplified()->CheckSmi(p.feedback()),
                                           end, efalse, if_false);

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  end = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue, vfalse, control);
  Node* finalStart =
      graph()->NewNode(simplified()->NumberMin(),
                       graph()->NewNode(simplified()->NumberMax(), start,
                                        jsgraph()->ZeroConstant()),
                       length);
  Node* finalEnd =
      graph()->NewNode(simplified()->NumberMin(),
                       graph()->NewNode(simplified()->NumberMax(), end,
                                        jsgraph()->ZeroConstant()),
                       length);

  Node* from =
      graph()->NewNode(simplified()->NumberMin(), finalStart, finalEnd);
  Node* to = graph()->NewNode(simplified()->NumberMax(), finalStart, finalEnd);

  Node* value = effect = graph()->NewNode(simplified()->StringSubstring(),
                                          receiver, from, to, effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES #sec-string.prototype.slice
Reduction JSCallReducer::ReduceStringPrototypeSlice(Node* node) {
  if (node->op()->ValueInputCount() < 3) return NoChange();
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* start = NodeProperties::GetValueInput(node, 2);
  Node* end = node->op()->ValueInputCount() > 3
                  ? NodeProperties::GetValueInput(node, 3)
                  : jsgraph()->UndefinedConstant();

  receiver = effect = graph()->NewNode(simplified()->CheckString(p.feedback()),
                                       receiver, effect, control);

  start = effect = graph()->NewNode(simplified()->CheckSmi(p.feedback()), start,
                                    effect, control);

  Node* length = graph()->NewNode(simplified()->StringLength(), receiver);

  // Replace {end} argument with {length} if it is undefined.
  {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), end,
                                   jsgraph()->UndefinedConstant());

    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* vtrue = length;

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse = efalse = graph()->NewNode(
        simplified()->CheckSmi(p.feedback()), end, efalse, if_false);

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
    end = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);
  }

  Node* from = graph()->NewNode(
      common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
      graph()->NewNode(simplified()->NumberLessThan(), start,
                       jsgraph()->ZeroConstant()),
      graph()->NewNode(
          simplified()->NumberMax(),
          graph()->NewNode(simplified()->NumberAdd(), length, start),
          jsgraph()->ZeroConstant()),
      graph()->NewNode(simplified()->NumberMin(), start, length));
  // {from} is always in non-negative Smi range, but our typer cannot
  // figure that out yet.
  from = effect = graph()->NewNode(common()->TypeGuard(Type::UnsignedSmall()),
                                   from, effect, control);

  Node* to = graph()->NewNode(
      common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
      graph()->NewNode(simplified()->NumberLessThan(), end,
                       jsgraph()->ZeroConstant()),
      graph()->NewNode(simplified()->NumberMax(),
                       graph()->NewNode(simplified()->NumberAdd(), length, end),
                       jsgraph()->ZeroConstant()),
      graph()->NewNode(simplified()->NumberMin(), end, length));
  // {to} is always in non-negative Smi range, but our typer cannot
  // figure that out yet.
  to = effect = graph()->NewNode(common()->TypeGuard(Type::UnsignedSmall()), to,
                                 effect, control);

  Node* result_string = nullptr;
  // Return empty string if {from} is smaller than {to}.
  {
    Node* check = graph()->NewNode(simplified()->NumberLessThan(), from, to);

    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* vtrue = etrue = graph()->NewNode(simplified()->StringSubstring(),
                                           receiver, from, to, etrue, if_true);

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse = jsgraph()->EmptyStringConstant();

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
    result_string =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue, vfalse, control);
  }

  ReplaceWithValue(node, result_string, effect, control);
  return Replace(result_string);
}

// ES #sec-string.prototype.substr
Reduction JSCallReducer::ReduceStringPrototypeSubstr(Node* node) {
  if (node->op()->ValueInputCount() < 3) return NoChange();
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* start = NodeProperties::GetValueInput(node, 2);
  Node* end = node->op()->ValueInputCount() > 3
                  ? NodeProperties::GetValueInput(node, 3)
                  : jsgraph()->UndefinedConstant();

  receiver = effect = graph()->NewNode(simplified()->CheckString(p.feedback()),
                                       receiver, effect, control);

  start = effect = graph()->NewNode(simplified()->CheckSmi(p.feedback()), start,
                                    effect, control);

  Node* length = graph()->NewNode(simplified()->StringLength(), receiver);

  // Replace {end} argument with {length} if it is undefined.
  {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), end,
                                   jsgraph()->UndefinedConstant());
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* vtrue = length;

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse = efalse = graph()->NewNode(
        simplified()->CheckSmi(p.feedback()), end, efalse, if_false);

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
    end = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);
  }

  Node* initStart = graph()->NewNode(
      common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
      graph()->NewNode(simplified()->NumberLessThan(), start,
                       jsgraph()->ZeroConstant()),
      graph()->NewNode(
          simplified()->NumberMax(),
          graph()->NewNode(simplified()->NumberAdd(), length, start),
          jsgraph()->ZeroConstant()),
      start);
  // The select above guarantees that initStart is non-negative, but
  // our typer can't figure that out yet.
  initStart = effect = graph()->NewNode(
      common()->TypeGuard(Type::UnsignedSmall()), initStart, effect, control);

  Node* resultLength = graph()->NewNode(
      simplified()->NumberMin(),
      graph()->NewNode(simplified()->NumberMax(), end,
                       jsgraph()->ZeroConstant()),
      graph()->NewNode(simplified()->NumberSubtract(), length, initStart));

  // The the select below uses {resultLength} only if {resultLength > 0},
  // but our typer can't figure that out yet.
  Node* to = effect = graph()->NewNode(
      common()->TypeGuard(Type::UnsignedSmall()),
      graph()->NewNode(simplified()->NumberAdd(), initStart, resultLength),
      effect, control);

  Node* result_string = nullptr;
  // Return empty string if {from} is smaller than {to}.
  {
    Node* check = graph()->NewNode(simplified()->NumberLessThan(),
                                   jsgraph()->ZeroConstant(), resultLength);

    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* vtrue = etrue =
        graph()->NewNode(simplified()->StringSubstring(), receiver, initStart,
                         to, etrue, if_true);

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse = jsgraph()->EmptyStringConstant();

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
    result_string =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue, vfalse, control);
  }

  ReplaceWithValue(node, result_string, effect, control);
  return Replace(result_string);
}

Reduction JSCallReducer::ReduceJSConstructWithArrayLike(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithArrayLike, node->opcode());
  CallFrequency frequency = CallFrequencyOf(node->op());
  return ReduceCallOrConstructWithArrayLikeOrSpread(node, 1, frequency,
                                                    FeedbackSource());
}

Reduction JSCallReducer::ReduceJSConstructWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithSpread, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 2);
  CallFrequency frequency = p.frequency();
  FeedbackSource feedback = p.feedback();
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
  if (!(flags() & kBailoutOnUninitialized)) return NoChange();

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state =
      NodeProperties::FindFrameStateBefore(node, jsgraph()->Dead());
  Node* deoptimize = graph()->NewNode(
      common()->Deoptimize(DeoptimizeKind::kSoft, reason, FeedbackSource()),
      frame_state, effect, control);
  // TODO(bmeurer): This should be on the AdvancedReducer somehow.
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
  Revisit(graph()->end());
  node->TrimInputCount(0);
  NodeProperties::ChangeOp(node, common()->Dead());
  return Changed(node);
}

Node* JSCallReducer::LoadReceiverElementsKind(Node* receiver, Node** effect,
                                              Node** control) {
  Node* receiver_map = *effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, *effect, *control);
  Node* receiver_bit_field2 = *effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapBitField2()), receiver_map,
      *effect, *control);
  Node* receiver_elements_kind = graph()->NewNode(
      simplified()->NumberShiftRightLogical(),
      graph()->NewNode(simplified()->NumberBitwiseAnd(), receiver_bit_field2,
                       jsgraph()->Constant(Map::ElementsKindBits::kMask)),
      jsgraph()->Constant(Map::ElementsKindBits::kShift));
  return receiver_elements_kind;
}

void JSCallReducer::CheckIfElementsKind(Node* receiver_elements_kind,
                                        ElementsKind kind, Node* control,
                                        Node** if_true, Node** if_false) {
  Node* is_packed_kind =
      graph()->NewNode(simplified()->NumberEqual(), receiver_elements_kind,
                       jsgraph()->Constant(GetPackedElementsKind(kind)));
  Node* packed_branch =
      graph()->NewNode(common()->Branch(), is_packed_kind, control);
  Node* if_packed = graph()->NewNode(common()->IfTrue(), packed_branch);

  if (IsHoleyElementsKind(kind)) {
    Node* if_not_packed = graph()->NewNode(common()->IfFalse(), packed_branch);
    Node* is_holey_kind =
        graph()->NewNode(simplified()->NumberEqual(), receiver_elements_kind,
                         jsgraph()->Constant(GetHoleyElementsKind(kind)));
    Node* holey_branch =
        graph()->NewNode(common()->Branch(), is_holey_kind, if_not_packed);
    Node* if_holey = graph()->NewNode(common()->IfTrue(), holey_branch);

    Node* if_not_packed_not_holey =
        graph()->NewNode(common()->IfFalse(), holey_branch);

    *if_true = graph()->NewNode(common()->Merge(2), if_packed, if_holey);
    *if_false = if_not_packed_not_holey;
  } else {
    *if_true = if_packed;
    *if_false = graph()->NewNode(common()->IfFalse(), packed_branch);
  }
}

// ES6 section 22.1.3.18 Array.prototype.push ( )
Reduction JSCallReducer::ReduceArrayPrototypePush(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  int const num_values = node->op()->ValueInputCount() - 2;
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  std::vector<ElementsKind> kinds;
  if (!CanInlineArrayResizingBuiltin(broker(), receiver_maps, &kinds, true)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  std::vector<Node*> controls_to_merge;
  std::vector<Node*> effects_to_merge;
  std::vector<Node*> values_to_merge;
  Node* return_value = jsgraph()->UndefinedConstant();

  Node* receiver_elements_kind =
      LoadReceiverElementsKind(receiver, &effect, &control);
  Node* next_control = control;
  Node* next_effect = effect;
  for (size_t i = 0; i < kinds.size(); i++) {
    ElementsKind kind = kinds[i];
    control = next_control;
    effect = next_effect;
    // We do not need branch for the last elements kind.
    if (i != kinds.size() - 1) {
      CheckIfElementsKind(receiver_elements_kind, kind, control, &control,
                          &next_control);
    }

    // Collect the value inputs to push.
    std::vector<Node*> values(num_values);
    for (int i = 0; i < num_values; ++i) {
      values[i] = NodeProperties::GetValueInput(node, 2 + i);
    }

    for (auto& value : values) {
      if (IsSmiElementsKind(kind)) {
        value = effect = graph()->NewNode(simplified()->CheckSmi(p.feedback()),
                                          value, effect, control);
      } else if (IsDoubleElementsKind(kind)) {
        value = effect = graph()->NewNode(
            simplified()->CheckNumber(p.feedback()), value, effect, control);
        // Make sure we do not store signaling NaNs into double arrays.
        value = graph()->NewNode(simplified()->NumberSilenceNaN(), value);
      }
    }

    // Load the "length" property of the {receiver}.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)),
        receiver, effect, control);
    return_value = length;

    // Check if we have any {values} to push.
    if (num_values > 0) {
      // Compute the resulting "length" of the {receiver}.
      Node* new_length = return_value = graph()->NewNode(
          simplified()->NumberAdd(), length, jsgraph()->Constant(num_values));

      // Load the elements backing store of the {receiver}.
      Node* elements = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
          receiver, effect, control);
      Node* elements_length = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForFixedArrayLength()),
          elements, effect, control);

      GrowFastElementsMode mode =
          IsDoubleElementsKind(kind)
              ? GrowFastElementsMode::kDoubleElements
              : GrowFastElementsMode::kSmiOrObjectElements;
      elements = effect = graph()->NewNode(
          simplified()->MaybeGrowFastElements(mode, p.feedback()), receiver,
          elements,
          graph()->NewNode(simplified()->NumberAdd(), length,
                           jsgraph()->Constant(num_values - 1)),
          elements_length, effect, control);

      // Update the JSArray::length field. Since this is observable,
      // there must be no other check after this.
      effect = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSArrayLength(kind)),
          receiver, new_length, effect, control);

      // Append the {values} to the {elements}.
      for (int i = 0; i < num_values; ++i) {
        Node* value = values[i];
        Node* index = graph()->NewNode(simplified()->NumberAdd(), length,
                                       jsgraph()->Constant(i));
        effect =
            graph()->NewNode(simplified()->StoreElement(
                                 AccessBuilder::ForFixedArrayElement(kind)),
                             elements, index, value, effect, control);
      }
    }

    controls_to_merge.push_back(control);
    effects_to_merge.push_back(effect);
    values_to_merge.push_back(return_value);
  }

  if (controls_to_merge.size() > 1) {
    int const count = static_cast<int>(controls_to_merge.size());

    control = graph()->NewNode(common()->Merge(count), count,
                               &controls_to_merge.front());
    effects_to_merge.push_back(control);
    effect = graph()->NewNode(common()->EffectPhi(count), count + 1,
                              &effects_to_merge.front());
    values_to_merge.push_back(control);
    return_value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                         count + 1, &values_to_merge.front());
  }

  ReplaceWithValue(node, return_value, effect, control);
  return Replace(return_value);
}

// ES6 section 22.1.3.17 Array.prototype.pop ( )
Reduction JSCallReducer::ReduceArrayPrototypePop(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  std::vector<ElementsKind> kinds;
  if (!CanInlineArrayResizingBuiltin(broker(), receiver_maps, &kinds)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  std::vector<Node*> controls_to_merge;
  std::vector<Node*> effects_to_merge;
  std::vector<Node*> values_to_merge;
  Node* value = jsgraph()->UndefinedConstant();

  Node* receiver_elements_kind =
      LoadReceiverElementsKind(receiver, &effect, &control);
  Node* next_control = control;
  Node* next_effect = effect;
  for (size_t i = 0; i < kinds.size(); i++) {
    ElementsKind kind = kinds[i];
    control = next_control;
    effect = next_effect;
    // We do not need branch for the last elements kind.
    if (i != kinds.size() - 1) {
      CheckIfElementsKind(receiver_elements_kind, kind, control, &control,
                          &next_control);
    }

    // Load the "length" property of the {receiver}.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)),
        receiver, effect, control);

    // Check if the {receiver} has any elements.
    Node* check = graph()->NewNode(simplified()->NumberEqual(), length,
                                   jsgraph()->ZeroConstant());
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* vtrue = jsgraph()->UndefinedConstant();

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse;
    {
      // TODO(tebbi): We should trim the backing store if the capacity is too
      // big, as implemented in elements.cc:ElementsAccessorBase::SetLengthImpl.

      // Load the elements backing store from the {receiver}.
      Node* elements = efalse = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
          receiver, efalse, if_false);

      // Ensure that we aren't popping from a copy-on-write backing store.
      if (IsSmiOrObjectElementsKind(kind)) {
        elements = efalse =
            graph()->NewNode(simplified()->EnsureWritableFastElements(),
                             receiver, elements, efalse, if_false);
      }

      // Compute the new {length}.
      length = graph()->NewNode(simplified()->NumberSubtract(), length,
                                jsgraph()->OneConstant());

      // Store the new {length} to the {receiver}.
      efalse = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSArrayLength(kind)),
          receiver, length, efalse, if_false);

      // Load the last entry from the {elements}.
      vfalse = efalse = graph()->NewNode(
          simplified()->LoadElement(AccessBuilder::ForFixedArrayElement(kind)),
          elements, length, efalse, if_false);

      // Store a hole to the element we just removed from the {receiver}.
      efalse = graph()->NewNode(
          simplified()->StoreElement(
              AccessBuilder::ForFixedArrayElement(GetHoleyElementsKind(kind))),
          elements, length, jsgraph()->TheHoleConstant(), efalse, if_false);
    }

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
    value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue, vfalse, control);

    // Convert the hole to undefined. Do this last, so that we can optimize
    // conversion operator via some smart strength reduction in many cases.
    if (IsHoleyElementsKind(kind)) {
      value =
          graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(), value);
    }

    controls_to_merge.push_back(control);
    effects_to_merge.push_back(effect);
    values_to_merge.push_back(value);
  }

  if (controls_to_merge.size() > 1) {
    int const count = static_cast<int>(controls_to_merge.size());

    control = graph()->NewNode(common()->Merge(count), count,
                               &controls_to_merge.front());
    effects_to_merge.push_back(control);
    effect = graph()->NewNode(common()->EffectPhi(count), count + 1,
                              &effects_to_merge.front());
    values_to_merge.push_back(control);
    value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                         count + 1, &values_to_merge.front());
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES6 section 22.1.3.22 Array.prototype.shift ( )
Reduction JSCallReducer::ReduceArrayPrototypeShift(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  std::vector<ElementsKind> kinds;
  if (!CanInlineArrayResizingBuiltin(broker(), receiver_maps, &kinds)) {
    return inference.NoChange();
  }
  if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  std::vector<Node*> controls_to_merge;
  std::vector<Node*> effects_to_merge;
  std::vector<Node*> values_to_merge;
  Node* value = jsgraph()->UndefinedConstant();

  Node* receiver_elements_kind =
      LoadReceiverElementsKind(receiver, &effect, &control);
  Node* next_control = control;
  Node* next_effect = effect;
  for (size_t i = 0; i < kinds.size(); i++) {
    ElementsKind kind = kinds[i];
    control = next_control;
    effect = next_effect;
    // We do not need branch for the last elements kind.
    if (i != kinds.size() - 1) {
      CheckIfElementsKind(receiver_elements_kind, kind, control, &control,
                          &next_control);
    }

    // Load length of the {receiver}.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayLength(kind)),
        receiver, effect, control);

    // Return undefined if {receiver} has no elements.
    Node* check0 = graph()->NewNode(simplified()->NumberEqual(), length,
                                    jsgraph()->ZeroConstant());
    Node* branch0 =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);

    Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
    Node* etrue0 = effect;
    Node* vtrue0 = jsgraph()->UndefinedConstant();

    Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
    Node* efalse0 = effect;
    Node* vfalse0;
    {
      // Check if we should take the fast-path.
      Node* check1 =
          graph()->NewNode(simplified()->NumberLessThanOrEqual(), length,
                           jsgraph()->Constant(JSArray::kMaxCopyElements));
      Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                       check1, if_false0);

      Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
      Node* etrue1 = efalse0;
      Node* vtrue1;
      {
        Node* elements = etrue1 = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
            receiver, etrue1, if_true1);

        // Load the first element here, which we return below.
        vtrue1 = etrue1 = graph()->NewNode(
            simplified()->LoadElement(
                AccessBuilder::ForFixedArrayElement(kind)),
            elements, jsgraph()->ZeroConstant(), etrue1, if_true1);

        // Ensure that we aren't shifting a copy-on-write backing store.
        if (IsSmiOrObjectElementsKind(kind)) {
          elements = etrue1 =
              graph()->NewNode(simplified()->EnsureWritableFastElements(),
                               receiver, elements, etrue1, if_true1);
        }

        // Shift the remaining {elements} by one towards the start.
        Node* loop = graph()->NewNode(common()->Loop(2), if_true1, if_true1);
        Node* eloop =
            graph()->NewNode(common()->EffectPhi(2), etrue1, etrue1, loop);
        Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
        NodeProperties::MergeControlToEnd(graph(), common(), terminate);
        Node* index = graph()->NewNode(
            common()->Phi(MachineRepresentation::kTagged, 2),
            jsgraph()->OneConstant(),
            jsgraph()->Constant(JSArray::kMaxCopyElements - 1), loop);

        {
          Node* check2 =
              graph()->NewNode(simplified()->NumberLessThan(), index, length);
          Node* branch2 = graph()->NewNode(common()->Branch(), check2, loop);

          if_true1 = graph()->NewNode(common()->IfFalse(), branch2);
          etrue1 = eloop;

          Node* control = graph()->NewNode(common()->IfTrue(), branch2);
          Node* effect = etrue1;

          ElementAccess const access =
              AccessBuilder::ForFixedArrayElement(kind);
          Node* value = effect =
              graph()->NewNode(simplified()->LoadElement(access), elements,
                               index, effect, control);
          effect = graph()->NewNode(
              simplified()->StoreElement(access), elements,
              graph()->NewNode(simplified()->NumberSubtract(), index,
                               jsgraph()->OneConstant()),
              value, effect, control);

          loop->ReplaceInput(1, control);
          eloop->ReplaceInput(1, effect);
          index->ReplaceInput(1,
                              graph()->NewNode(simplified()->NumberAdd(), index,
                                               jsgraph()->OneConstant()));
        }

        // Compute the new {length}.
        length = graph()->NewNode(simplified()->NumberSubtract(), length,
                                  jsgraph()->OneConstant());

        // Store the new {length} to the {receiver}.
        etrue1 = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForJSArrayLength(kind)),
            receiver, length, etrue1, if_true1);

        // Store a hole to the element we just removed from the {receiver}.
        etrue1 = graph()->NewNode(
            simplified()->StoreElement(AccessBuilder::ForFixedArrayElement(
                GetHoleyElementsKind(kind))),
            elements, length, jsgraph()->TheHoleConstant(), etrue1, if_true1);
      }

      Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
      Node* efalse1 = efalse0;
      Node* vfalse1;
      {
        // Call the generic C++ implementation.
        const int builtin_index = Builtins::kArrayShift;
        auto call_descriptor = Linkage::GetCEntryStubCallDescriptor(
            graph()->zone(), 1, BuiltinArguments::kNumExtraArgsWithReceiver,
            Builtins::name(builtin_index), node->op()->properties(),
            CallDescriptor::kNeedsFrameState);
        Node* stub_code = jsgraph()->CEntryStubConstant(1, kDontSaveFPRegs,
                                                        kArgvOnStack, true);
        Address builtin_entry = Builtins::CppEntryOf(builtin_index);
        Node* entry = jsgraph()->ExternalConstant(
            ExternalReference::Create(builtin_entry));
        Node* argc =
            jsgraph()->Constant(BuiltinArguments::kNumExtraArgsWithReceiver);
        if_false1 = efalse1 = vfalse1 =
            graph()->NewNode(common()->Call(call_descriptor), stub_code,
                             receiver, jsgraph()->PaddingConstant(), argc,
                             target, jsgraph()->UndefinedConstant(), entry,
                             argc, context, frame_state, efalse1, if_false1);
      }

      if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
      efalse0 =
          graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
      vfalse0 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue1, vfalse1, if_false0);
    }

    control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
    value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue0, vfalse0, control);

    // Convert the hole to undefined. Do this last, so that we can optimize
    // conversion operator via some smart strength reduction in many cases.
    if (IsHoleyElementsKind(kind)) {
      value =
          graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(), value);
    }

    controls_to_merge.push_back(control);
    effects_to_merge.push_back(effect);
    values_to_merge.push_back(value);
  }

  if (controls_to_merge.size() > 1) {
    int const count = static_cast<int>(controls_to_merge.size());

    control = graph()->NewNode(common()->Merge(count), count,
                               &controls_to_merge.front());
    effects_to_merge.push_back(control);
    effect = graph()->NewNode(common()->EffectPhi(count), count + 1,
                              &effects_to_merge.front());
    values_to_merge.push_back(control);
    value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                         count + 1, &values_to_merge.front());
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES6 section 22.1.3.23 Array.prototype.slice ( )
Reduction JSCallReducer::ReduceArrayPrototypeSlice(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (!FLAG_turbo_inline_array_builtins) return NoChange();
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* start = node->op()->ValueInputCount() > 2
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->ZeroConstant();
  Node* end = node->op()->ValueInputCount() > 3
                  ? NodeProperties::GetValueInput(node, 3)
                  : jsgraph()->UndefinedConstant();
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Optimize for the case where we simply clone the {receiver},
  // i.e. when the {start} is zero and the {end} is undefined
  // (meaning it will be set to {receiver}s "length" property).
  if (!NumberMatcher(start).Is(0) ||
      !HeapObjectMatcher(end).Is(factory()->undefined_value())) {
    return NoChange();
  }

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  // Check that the maps are of JSArray (and more).
  // TODO(turbofan): Consider adding special case for the common pattern
  // `slice.call(arguments)`, for example jQuery makes heavy use of that.
  bool can_be_holey = false;
  for (Handle<Map> map : receiver_maps) {
    MapRef receiver_map(broker(), map);
    if (!receiver_map.supports_fast_array_iteration())
      return inference.NoChange();
    if (IsHoleyElementsKind(receiver_map.elements_kind())) {
      can_be_holey = true;
    }
  }

  if (!dependencies()->DependOnArraySpeciesProtector())
    return inference.NoChange();
  if (can_be_holey) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  // TODO(turbofan): We can do even better here, either adding a CloneArray
  // simplified operator, whose output type indicates that it's an Array,
  // saving subsequent checks, or yet better, by introducing new operators
  // CopySmiOrObjectElements / CopyDoubleElements and inlining the JSArray
  // allocation in here. That way we'd even get escape analysis and scalar
  // replacement to help in some cases.
  Callable callable =
      Builtins::CallableFor(isolate(), Builtins::kCloneFastJSArray);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), callable.descriptor(),
      callable.descriptor().GetStackParameterCount(), CallDescriptor::kNoFlags,
      Operator::kNoThrow | Operator::kNoDeopt);

  // Calls to Builtins::kCloneFastJSArray produce COW arrays
  // if the original array is COW
  Node* clone = effect = graph()->NewNode(
      common()->Call(call_descriptor), jsgraph()->HeapConstant(callable.code()),
      receiver, context, effect, control);

  ReplaceWithValue(node, clone, effect, control);
  return Replace(clone);
}

// ES6 section 22.1.2.2 Array.isArray ( arg )
Reduction JSCallReducer::ReduceArrayIsArray(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  // We certainly know that undefined is not an array.
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* object = NodeProperties::GetValueInput(node, 2);
  node->ReplaceInput(0, object);
  node->ReplaceInput(1, context);
  node->ReplaceInput(2, frame_state);
  node->ReplaceInput(3, effect);
  node->ReplaceInput(4, control);
  node->TrimInputCount(5);
  NodeProperties::ChangeOp(node, javascript()->ObjectIsArray());
  return Changed(node);
}

Reduction JSCallReducer::ReduceArrayIterator(Node* node, IterationKind kind) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if we know that {receiver} is a valid JSReceiver.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAreJSReceiver()) {
    return NoChange();
  }

  // Morph the {node} into a JSCreateArrayIterator with the given {kind}.
  RelaxControls(node);
  node->ReplaceInput(0, receiver);
  node->ReplaceInput(1, context);
  node->ReplaceInput(2, effect);
  node->ReplaceInput(3, control);
  node->TrimInputCount(4);
  NodeProperties::ChangeOp(node, javascript()->CreateArrayIterator(kind));
  return Changed(node);
}

// ES #sec-%arrayiteratorprototype%.next
Reduction JSCallReducer::ReduceArrayIteratorPrototypeNext(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  Node* iterator = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  if (iterator->opcode() != IrOpcode::kJSCreateArrayIterator) return NoChange();

  IterationKind const iteration_kind =
      CreateArrayIteratorParametersOf(iterator->op()).kind();
  Node* iterated_object = NodeProperties::GetValueInput(iterator, 0);
  Node* iterator_effect = NodeProperties::GetEffectInput(iterator);

  MapInference inference(broker(), iterated_object, iterator_effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& iterated_object_maps = inference.GetMaps();

  // Check that various {iterated_object_maps} have compatible elements kinds.
  ElementsKind elements_kind =
      MapRef(broker(), iterated_object_maps[0]).elements_kind();
  if (IsTypedArrayElementsKind(elements_kind)) {
    // TurboFan doesn't support loading from BigInt typed arrays yet.
    if (elements_kind == BIGUINT64_ELEMENTS ||
        elements_kind == BIGINT64_ELEMENTS) {
      return inference.NoChange();
    }
    for (Handle<Map> map : iterated_object_maps) {
      MapRef iterated_object_map(broker(), map);
      if (iterated_object_map.elements_kind() != elements_kind) {
        return inference.NoChange();
      }
    }
  } else {
    if (!CanInlineArrayIteratingBuiltin(broker(), iterated_object_maps,
                                        &elements_kind)) {
      return inference.NoChange();
    }
  }

  if (IsHoleyElementsKind(elements_kind)) {
    if (!dependencies()->DependOnNoElementsProtector()) UNREACHABLE();
  }
  // Since the map inference was done relative to {iterator_effect} rather than
  // {effect}, we need to guard the use of the map(s) even when the inference
  // was reliable.
  inference.InsertMapChecks(jsgraph(), &effect, control, p.feedback());

  if (IsTypedArrayElementsKind(elements_kind)) {
    // See if we can skip the detaching check.
    if (!dependencies()->DependOnArrayBufferDetachingProtector()) {
      // Bail out if the {iterated_object}s JSArrayBuffer was detached.
      Node* buffer = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
          iterated_object, effect, control);
      Node* buffer_bit_field = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSArrayBufferBitField()),
          buffer, effect, control);
      Node* check = graph()->NewNode(
          simplified()->NumberEqual(),
          graph()->NewNode(
              simplified()->NumberBitwiseAnd(), buffer_bit_field,
              jsgraph()->Constant(JSArrayBuffer::WasDetachedBit::kMask)),
          jsgraph()->ZeroConstant());
      effect = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kArrayBufferWasDetached,
                                p.feedback()),
          check, effect, control);
    }
  }

  // Load the [[NextIndex]] from the {iterator} and leverage the fact
  // that we definitely know that it's in Unsigned32 range since the
  // {iterated_object} is either a JSArray or a JSTypedArray. For the
  // latter case we even know that it's a Smi in UnsignedSmall range.
  FieldAccess index_access = AccessBuilder::ForJSArrayIteratorNextIndex();
  if (IsTypedArrayElementsKind(elements_kind)) {
    index_access.type = TypeCache::Get()->kJSTypedArrayLengthType;
    index_access.machine_type = MachineType::TypeCompressedTaggedSigned();
    index_access.write_barrier_kind = kNoWriteBarrier;
  } else {
    index_access.type = TypeCache::Get()->kJSArrayLengthType;
  }
  Node* index = effect = graph()->NewNode(simplified()->LoadField(index_access),
                                          iterator, effect, control);

  // Load the elements of the {iterated_object}. While it feels
  // counter-intuitive to place the elements pointer load before
  // the condition below, as it might not be needed (if the {index}
  // is out of bounds for the {iterated_object}), it's better this
  // way as it allows the LoadElimination to eliminate redundant
  // reloads of the elements pointer.
  Node* elements = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
      iterated_object, effect, control);

  // Load the length of the {iterated_object}. Due to the map checks we
  // already know something about the length here, which we can leverage
  // to generate Word32 operations below without additional checking.
  FieldAccess length_access =
      IsTypedArrayElementsKind(elements_kind)
          ? AccessBuilder::ForJSTypedArrayLength()
          : AccessBuilder::ForJSArrayLength(elements_kind);
  Node* length = effect = graph()->NewNode(
      simplified()->LoadField(length_access), iterated_object, effect, control);

  // Check whether {index} is within the valid range for the {iterated_object}.
  Node* check = graph()->NewNode(simplified()->NumberLessThan(), index, length);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kNone), check, control);

  Node* done_true;
  Node* value_true;
  Node* etrue = effect;
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  {
    // We know that the {index} is range of the {length} now.
    index = etrue = graph()->NewNode(
        common()->TypeGuard(
            Type::Range(0.0, length_access.type.Max() - 1.0, graph()->zone())),
        index, etrue, if_true);

    done_true = jsgraph()->FalseConstant();
    if (iteration_kind == IterationKind::kKeys) {
      // Just return the {index}.
      value_true = index;
    } else {
      DCHECK(iteration_kind == IterationKind::kEntries ||
             iteration_kind == IterationKind::kValues);

      if (IsTypedArrayElementsKind(elements_kind)) {
        Node* base_ptr = etrue =
            graph()->NewNode(simplified()->LoadField(
                                 AccessBuilder::ForJSTypedArrayBasePointer()),
                             iterated_object, etrue, if_true);
        Node* external_ptr = etrue = graph()->NewNode(
            simplified()->LoadField(
                AccessBuilder::ForJSTypedArrayExternalPointer()),
            iterated_object, etrue, if_true);

        ExternalArrayType array_type = kExternalInt8Array;
        switch (elements_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    array_type = kExternal##Type##Array;          \
    break;
          TYPED_ARRAYS(TYPED_ARRAY_CASE)
          default:
            UNREACHABLE();
#undef TYPED_ARRAY_CASE
        }

        Node* buffer = etrue =
            graph()->NewNode(simplified()->LoadField(
                                 AccessBuilder::ForJSArrayBufferViewBuffer()),
                             iterated_object, etrue, if_true);

        value_true = etrue =
            graph()->NewNode(simplified()->LoadTypedElement(array_type), buffer,
                             base_ptr, external_ptr, index, etrue, if_true);
      } else {
        value_true = etrue = graph()->NewNode(
            simplified()->LoadElement(
                AccessBuilder::ForFixedArrayElement(elements_kind)),
            elements, index, etrue, if_true);

        // Convert hole to undefined if needed.
        if (elements_kind == HOLEY_ELEMENTS ||
            elements_kind == HOLEY_SMI_ELEMENTS) {
          value_true = graph()->NewNode(
              simplified()->ConvertTaggedHoleToUndefined(), value_true);
        } else if (elements_kind == HOLEY_DOUBLE_ELEMENTS) {
          // TODO(6587): avoid deopt if not all uses of value are truncated.
          CheckFloat64HoleMode mode = CheckFloat64HoleMode::kAllowReturnHole;
          value_true = etrue = graph()->NewNode(
              simplified()->CheckFloat64Hole(mode, p.feedback()), value_true,
              etrue, if_true);
        }
      }

      if (iteration_kind == IterationKind::kEntries) {
        // Allocate elements for key/value pair
        value_true = etrue =
            graph()->NewNode(javascript()->CreateKeyValueArray(), index,
                             value_true, context, etrue);
      } else {
        DCHECK_EQ(IterationKind::kValues, iteration_kind);
      }
    }

    // Increment the [[NextIndex]] field in the {iterator}. The TypeGuards
    // above guarantee that the {next_index} is in the UnsignedSmall range.
    Node* next_index = graph()->NewNode(simplified()->NumberAdd(), index,
                                        jsgraph()->OneConstant());
    etrue = graph()->NewNode(simplified()->StoreField(index_access), iterator,
                             next_index, etrue, if_true);
  }

  Node* done_false;
  Node* value_false;
  Node* efalse = effect;
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  {
    // iterator.[[NextIndex]] >= array.length, stop iterating.
    done_false = jsgraph()->TrueConstant();
    value_false = jsgraph()->UndefinedConstant();

    if (!IsTypedArrayElementsKind(elements_kind)) {
      // Mark the {iterator} as exhausted by setting the [[NextIndex]] to a
      // value that will never pass the length check again (aka the maximum
      // value possible for the specific iterated object). Note that this is
      // different from what the specification says, which is changing the
      // [[IteratedObject]] field to undefined, but that makes it difficult
      // to eliminate the map checks and "length" accesses in for..of loops.
      //
      // This is not necessary for JSTypedArray's, since the length of those
      // cannot change later and so if we were ever out of bounds for them
      // we will stay out-of-bounds forever.
      Node* end_index = jsgraph()->Constant(index_access.type.Max());
      efalse = graph()->NewNode(simplified()->StoreField(index_access),
                                iterator, end_index, efalse, if_false);
    }
  }

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       value_true, value_false, control);
  Node* done =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       done_true, done_false, control);

  // Create IteratorResult object.
  value = effect = graph()->NewNode(javascript()->CreateIterResultObject(),
                                    value, done, context, effect);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES6 section 21.1.3.2 String.prototype.charCodeAt ( pos )
// ES6 section 21.1.3.3 String.prototype.codePointAt ( pos )
Reduction JSCallReducer::ReduceStringPrototypeStringAt(
    const Operator* string_access_operator, Node* node) {
  DCHECK(string_access_operator->opcode() == IrOpcode::kStringCharCodeAt ||
         string_access_operator->opcode() == IrOpcode::kStringCodePointAt);
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* index = node->op()->ValueInputCount() >= 3
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->ZeroConstant();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Ensure that the {receiver} is actually a String.
  receiver = effect = graph()->NewNode(simplified()->CheckString(p.feedback()),
                                       receiver, effect, control);

  // Determine the {receiver} length.
  Node* receiver_length =
      graph()->NewNode(simplified()->StringLength(), receiver);

  // Check that the {index} is within range.
  index = effect = graph()->NewNode(simplified()->CheckBounds(p.feedback()),
                                    index, receiver_length, effect, control);

  // Return the character from the {receiver} as single character string.
  Node* masked_index = graph()->NewNode(simplified()->PoisonIndex(), index);
  Node* value = effect = graph()->NewNode(string_access_operator, receiver,
                                          masked_index, effect, control);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES section 21.1.3.20
// String.prototype.startsWith ( searchString [ , position ] )
Reduction JSCallReducer::ReduceStringPrototypeStartsWith(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  Node* string = NodeProperties::GetValueInput(node, 1);
  Node* search_string = NodeProperties::GetValueInput(node, 2);
  Node* position = node->op()->ValueInputCount() >= 4
                       ? NodeProperties::GetValueInput(node, 3)
                       : jsgraph()->ZeroConstant();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  HeapObjectMatcher m(search_string);
  if (m.HasValue()) {
    ObjectRef target_ref = m.Ref(broker());
    if (target_ref.IsString()) {
      StringRef str = target_ref.AsString();
      if (str.length() == 1) {
        string = effect = graph()->NewNode(
            simplified()->CheckString(p.feedback()), string, effect, control);
        position = effect = graph()->NewNode(
            simplified()->CheckSmi(p.feedback()), position, effect, control);

        Node* string_length =
            graph()->NewNode(simplified()->StringLength(), string);
        Node* unsigned_position = graph()->NewNode(
            simplified()->NumberMax(), position, jsgraph()->ZeroConstant());

        Node* check = graph()->NewNode(simplified()->NumberLessThan(),
                                       unsigned_position, string_length);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kNone),
                                        check, control);

        Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
        Node* efalse = effect;
        Node* vfalse = jsgraph()->FalseConstant();

        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* etrue = effect;
        Node* vtrue;
        {
          Node* masked_position =
              graph()->NewNode(simplified()->PoisonIndex(), unsigned_position);
          Node* string_first = etrue =
              graph()->NewNode(simplified()->StringCharCodeAt(), string,
                               masked_position, etrue, if_true);

          Node* search_first = jsgraph()->Constant(str.GetFirstChar());
          vtrue = graph()->NewNode(simplified()->NumberEqual(), string_first,
                                   search_first);
        }

        control = graph()->NewNode(common()->Merge(2), if_true, if_false);
        Node* value =
            graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue, vfalse, control);
        effect =
            graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

        ReplaceWithValue(node, value, effect, control);
        return Replace(value);
      }
    }
  }

  return NoChange();
}

// ES section 21.1.3.1 String.prototype.charAt ( pos )
Reduction JSCallReducer::ReduceStringPrototypeCharAt(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* index = node->op()->ValueInputCount() >= 3
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->ZeroConstant();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Ensure that the {receiver} is actually a String.
  receiver = effect = graph()->NewNode(simplified()->CheckString(p.feedback()),
                                       receiver, effect, control);

  // Determine the {receiver} length.
  Node* receiver_length =
      graph()->NewNode(simplified()->StringLength(), receiver);

  // Check that the {index} is within range.
  index = effect = graph()->NewNode(simplified()->CheckBounds(p.feedback()),
                                    index, receiver_length, effect, control);

  // Return the character from the {receiver} as single character string.
  Node* masked_index = graph()->NewNode(simplified()->PoisonIndex(), index);
  Node* value = effect =
      graph()->NewNode(simplified()->StringCharCodeAt(), receiver, masked_index,
                       effect, control);
  value = graph()->NewNode(simplified()->StringFromSingleCharCode(), value);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

#ifdef V8_INTL_SUPPORT

Reduction JSCallReducer::ReduceStringPrototypeToLowerCaseIntl(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* receiver = effect =
      graph()->NewNode(simplified()->CheckString(p.feedback()),
                       NodeProperties::GetValueInput(node, 1), effect, control);

  NodeProperties::ReplaceEffectInput(node, effect);
  RelaxEffectsAndControls(node);
  node->ReplaceInput(0, receiver);
  node->TrimInputCount(1);
  NodeProperties::ChangeOp(node, simplified()->StringToLowerCaseIntl());
  NodeProperties::SetType(node, Type::String());
  return Changed(node);
}

Reduction JSCallReducer::ReduceStringPrototypeToUpperCaseIntl(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Node* receiver = effect =
      graph()->NewNode(simplified()->CheckString(p.feedback()),
                       NodeProperties::GetValueInput(node, 1), effect, control);

  NodeProperties::ReplaceEffectInput(node, effect);
  RelaxEffectsAndControls(node);
  node->ReplaceInput(0, receiver);
  node->TrimInputCount(1);
  NodeProperties::ChangeOp(node, simplified()->StringToUpperCaseIntl());
  NodeProperties::SetType(node, Type::String());
  return Changed(node);
}

#endif  // V8_INTL_SUPPORT

// ES #sec-string.fromcharcode
Reduction JSCallReducer::ReduceStringFromCharCode(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() == 3) {
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* input = NodeProperties::GetValueInput(node, 2);

    input = effect = graph()->NewNode(
        simplified()->SpeculativeToNumber(NumberOperationHint::kNumberOrOddball,
                                          p.feedback()),
        input, effect, control);

    Node* value =
        graph()->NewNode(simplified()->StringFromSingleCharCode(), input);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }
  return NoChange();
}

// ES #sec-string.fromcodepoint
Reduction JSCallReducer::ReduceStringFromCodePoint(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() == 3) {
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* input = NodeProperties::GetValueInput(node, 2);

    input = effect =
        graph()->NewNode(simplified()->CheckBounds(p.feedback()), input,
                         jsgraph()->Constant(0x10FFFF + 1), effect, control);

    Node* value =
        graph()->NewNode(simplified()->StringFromSingleCodePoint(), input);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSCallReducer::ReduceStringPrototypeIterator(Node* node) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* receiver = effect =
      graph()->NewNode(simplified()->CheckString(p.feedback()),
                       NodeProperties::GetValueInput(node, 1), effect, control);
  Node* iterator = effect =
      graph()->NewNode(javascript()->CreateStringIterator(), receiver,
                       jsgraph()->NoContextConstant(), effect);
  ReplaceWithValue(node, iterator, effect, control);
  return Replace(iterator);
}

Reduction JSCallReducer::ReduceStringIteratorPrototypeNext(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() ||
      !inference.AllOfInstanceTypesAre(JS_STRING_ITERATOR_TYPE)) {
    return NoChange();
  }

  Node* string = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSStringIteratorString()),
      receiver, effect, control);
  Node* index = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSStringIteratorIndex()),
      receiver, effect, control);
  Node* length = graph()->NewNode(simplified()->StringLength(), string);

  // branch0: if (index < length)
  Node* check0 =
      graph()->NewNode(simplified()->NumberLessThan(), index, length);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kNone), check0, control);

  Node* etrue0 = effect;
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* done_true;
  Node* vtrue0;
  {
    done_true = jsgraph()->FalseConstant();
    vtrue0 = etrue0 = graph()->NewNode(simplified()->StringFromCodePointAt(),
                                       string, index, etrue0, if_true0);

    // Update iterator.[[NextIndex]]
    Node* char_length = graph()->NewNode(simplified()->StringLength(), vtrue0);
    index = graph()->NewNode(simplified()->NumberAdd(), index, char_length);
    etrue0 = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSStringIteratorIndex()),
        receiver, index, etrue0, if_true0);
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* done_false;
  Node* vfalse0;
  {
    vfalse0 = jsgraph()->UndefinedConstant();
    done_false = jsgraph()->TrueConstant();
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, effect, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2), vtrue0,
                       vfalse0, control);
  Node* done =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       done_true, done_false, control);

  value = effect = graph()->NewNode(javascript()->CreateIterResultObject(),
                                    value, done, context, effect);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES #sec-string.prototype.concat
Reduction JSCallReducer::ReduceStringPrototypeConcat(Node* node) {
  if (node->op()->ValueInputCount() < 2 || node->op()->ValueInputCount() > 3) {
    return NoChange();
  }
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* receiver = effect =
      graph()->NewNode(simplified()->CheckString(p.feedback()),
                       NodeProperties::GetValueInput(node, 1), effect, control);

  if (node->op()->ValueInputCount() < 3) {
    ReplaceWithValue(node, receiver, effect, control);
    return Replace(receiver);
  }

  Node* argument = effect =
      graph()->NewNode(simplified()->CheckString(p.feedback()),
                       NodeProperties::GetValueInput(node, 2), effect, control);
  Node* receiver_length =
      graph()->NewNode(simplified()->StringLength(), receiver);
  Node* argument_length =
      graph()->NewNode(simplified()->StringLength(), argument);
  Node* length = graph()->NewNode(simplified()->NumberAdd(), receiver_length,
                                  argument_length);
  length = effect = graph()->NewNode(
      simplified()->CheckBounds(p.feedback()), length,
      jsgraph()->Constant(String::kMaxLength + 1), effect, control);

  Node* value = graph()->NewNode(simplified()->StringConcat(), length, receiver,
                                 argument);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Node* JSCallReducer::CreateArtificialFrameState(
    Node* node, Node* outer_frame_state, int parameter_count,
    BailoutId bailout_id, FrameStateType frame_state_type,
    const SharedFunctionInfoRef& shared, Node* context) {
  const FrameStateFunctionInfo* state_info =
      common()->CreateFrameStateFunctionInfo(
          frame_state_type, parameter_count + 1, 0, shared.object());

  const Operator* op = common()->FrameState(
      bailout_id, OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = common()->StateValues(0, SparseInputMask::Dense());
  Node* node0 = graph()->NewNode(op0);

  static constexpr int kTargetInputIndex = 0;
  static constexpr int kReceiverInputIndex = 1;
  const int parameter_count_with_receiver = parameter_count + 1;
  std::vector<Node*> params;
  params.reserve(parameter_count_with_receiver);
  for (int i = 0; i < parameter_count_with_receiver; i++) {
    params.push_back(node->InputAt(kReceiverInputIndex + i));
  }
  const Operator* op_param = common()->StateValues(
      static_cast<int>(params.size()), SparseInputMask::Dense());
  Node* params_node = graph()->NewNode(
      op_param, static_cast<int>(params.size()), &params.front());
  if (!context) {
    context = jsgraph()->UndefinedConstant();
  }
  return graph()->NewNode(op, params_node, node0, node0, context,
                          node->InputAt(kTargetInputIndex), outer_frame_state);
}

Reduction JSCallReducer::ReducePromiseConstructor(Node* node) {
  DisallowHeapAccessIf no_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSConstruct, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  // We only inline when we have the executor.
  if (arity < 1) return NoChange();
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* executor = NodeProperties::GetValueInput(node, 1);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* outer_frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Only handle builtins Promises, not subclasses.
  if (target != new_target) return NoChange();

  if (!dependencies()->DependOnPromiseHookProtector()) return NoChange();

  SharedFunctionInfoRef promise_shared =
      native_context().promise_function().shared();

  // Insert a construct stub frame into the chain of frame states. This will
  // reconstruct the proper frame when deoptimizing within the constructor.
  // For the frame state, we only provide the executor parameter, even if more
  // arugments were passed. This is not observable from JS.
  DCHECK_EQ(1, promise_shared.internal_formal_parameter_count());
  Node* constructor_frame_state = CreateArtificialFrameState(
      node, outer_frame_state, 1, BailoutId::ConstructStubInvoke(),
      FrameStateType::kConstructStub, promise_shared, context);

  // The deopt continuation of this frame state is never called; the frame state
  // is only necessary to obtain the right stack trace.
  const std::vector<Node*> checkpoint_parameters({
      jsgraph()->UndefinedConstant(), /* receiver */
      jsgraph()->UndefinedConstant(), /* promise */
      jsgraph()->UndefinedConstant(), /* reject function */
      jsgraph()->TheHoleConstant()    /* exception */
  });
  int checkpoint_parameters_size =
      static_cast<int>(checkpoint_parameters.size());
  Node* frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), promise_shared,
      Builtins::kPromiseConstructorLazyDeoptContinuation, target, context,
      checkpoint_parameters.data(), checkpoint_parameters_size,
      constructor_frame_state, ContinuationFrameStateMode::LAZY);

  // Check if executor is callable
  Node* check_fail = nullptr;
  Node* check_throw = nullptr;
  WireInCallbackIsCallableCheck(executor, context, frame_state, effect,
                                &control, &check_fail, &check_throw);

  // Create the resulting JSPromise.
  Node* promise = effect =
      graph()->NewNode(javascript()->CreatePromise(), context, effect);

  // 8. CreatePromiseResolvingFunctions
  // Allocate a promise context for the closures below.
  Node* promise_context = effect = graph()->NewNode(
      javascript()->CreateFunctionContext(
          native_context().scope_info().object(),
          PromiseBuiltins::kPromiseContextLength - Context::MIN_CONTEXT_SLOTS,
          FUNCTION_SCOPE),
      context, effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(
          AccessBuilder::ForContextSlot(PromiseBuiltins::kPromiseSlot)),
      promise_context, promise, effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(
          AccessBuilder::ForContextSlot(PromiseBuiltins::kAlreadyResolvedSlot)),
      promise_context, jsgraph()->FalseConstant(), effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(
          AccessBuilder::ForContextSlot(PromiseBuiltins::kDebugEventSlot)),
      promise_context, jsgraph()->TrueConstant(), effect, control);

  // Allocate the closure for the resolve case.
  Node* resolve = effect = CreateClosureFromBuiltinSharedFunctionInfo(
      native_context().promise_capability_default_resolve_shared_fun(),
      promise_context, effect, control);

  // Allocate the closure for the reject case.
  Node* reject = effect = CreateClosureFromBuiltinSharedFunctionInfo(
      native_context().promise_capability_default_reject_shared_fun(),
      promise_context, effect, control);

  const std::vector<Node*> checkpoint_parameters_continuation(
      {jsgraph()->UndefinedConstant() /* receiver */, promise, reject});
  int checkpoint_parameters_continuation_size =
      static_cast<int>(checkpoint_parameters_continuation.size());
  // This continuation just returns the created promise and takes care of
  // exceptions thrown by the executor.
  frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), promise_shared,
      Builtins::kPromiseConstructorLazyDeoptContinuation, target, context,
      checkpoint_parameters_continuation.data(),
      checkpoint_parameters_continuation_size, constructor_frame_state,
      ContinuationFrameStateMode::LAZY_WITH_CATCH);

  // 9. Call executor with both resolving functions
  effect = control = graph()->NewNode(
      javascript()->Call(4, p.frequency(), FeedbackSource(),
                         ConvertReceiverMode::kNullOrUndefined,
                         SpeculationMode::kDisallowSpeculation),
      executor, jsgraph()->UndefinedConstant(), resolve, reject, context,
      frame_state, effect, control);

  Node* exception_effect = effect;
  Node* exception_control = control;
  {
    Node* reason = exception_effect = exception_control = graph()->NewNode(
        common()->IfException(), exception_control, exception_effect);
    // 10a. Call reject if the call to executor threw.
    exception_effect = exception_control = graph()->NewNode(
        javascript()->Call(3, p.frequency(), FeedbackSource(),
                           ConvertReceiverMode::kNullOrUndefined,
                           SpeculationMode::kDisallowSpeculation),
        reject, jsgraph()->UndefinedConstant(), reason, context, frame_state,
        exception_effect, exception_control);

    // Rewire potential exception edges.
    Node* on_exception = nullptr;
    if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
      RewirePostCallbackExceptionEdges(check_throw, on_exception,
                                       exception_effect, &check_fail,
                                       &exception_control);
    }
  }

  Node* success_effect = effect;
  Node* success_control = control;
  {
    success_control = graph()->NewNode(common()->IfSuccess(), success_control);
  }

  control =
      graph()->NewNode(common()->Merge(2), success_control, exception_control);
  effect = graph()->NewNode(common()->EffectPhi(2), success_effect,
                            exception_effect, control);

  // Wire up the branch for the case when IsCallable fails for the executor.
  // Since {check_throw} is an unconditional throw, it's impossible to
  // return a successful completion. Therefore, we simply connect the successful
  // completion to the graph end.
  Node* throw_node =
      graph()->NewNode(common()->Throw(), check_throw, check_fail);
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);

  ReplaceWithValue(node, promise, effect, control);
  return Replace(promise);
}

// V8 Extras: v8.createPromise(parent)
Reduction JSCallReducer::ReducePromiseInternalConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);

  // Check that promises aren't being observed through (debug) hooks.
  if (!dependencies()->DependOnPromiseHookProtector()) return NoChange();

  // Create a new pending promise.
  Node* value = effect =
      graph()->NewNode(javascript()->CreatePromise(), context, effect);

  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

// V8 Extras: v8.rejectPromise(promise, reason)
Reduction JSCallReducer::ReducePromiseInternalReject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* promise = node->op()->ValueInputCount() >= 2
                      ? NodeProperties::GetValueInput(node, 2)
                      : jsgraph()->UndefinedConstant();
  Node* reason = node->op()->ValueInputCount() >= 3
                     ? NodeProperties::GetValueInput(node, 3)
                     : jsgraph()->UndefinedConstant();
  Node* debug_event = jsgraph()->TrueConstant();
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Reject the {promise} using the given {reason}, and trigger debug logic.
  Node* value = effect =
      graph()->NewNode(javascript()->RejectPromise(), promise, reason,
                       debug_event, context, frame_state, effect, control);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// V8 Extras: v8.resolvePromise(promise, resolution)
Reduction JSCallReducer::ReducePromiseInternalResolve(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* promise = node->op()->ValueInputCount() >= 2
                      ? NodeProperties::GetValueInput(node, 2)
                      : jsgraph()->UndefinedConstant();
  Node* resolution = node->op()->ValueInputCount() >= 3
                         ? NodeProperties::GetValueInput(node, 3)
                         : jsgraph()->UndefinedConstant();
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Resolve the {promise} using the given {resolution}.
  Node* value = effect =
      graph()->NewNode(javascript()->ResolvePromise(), promise, resolution,
                       context, frame_state, effect, control);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

bool JSCallReducer::DoPromiseChecks(MapInference* inference) {
  if (!inference->HaveMaps()) return false;
  MapHandles const& receiver_maps = inference->GetMaps();

  // Check whether all {receiver_maps} are JSPromise maps and
  // have the initial Promise.prototype as their [[Prototype]].
  for (Handle<Map> map : receiver_maps) {
    MapRef receiver_map(broker(), map);
    if (!receiver_map.IsJSPromiseMap()) return false;
    if (FLAG_concurrent_inlining && !receiver_map.serialized_prototype()) {
      TRACE_BROKER_MISSING(broker(), "prototype for map " << receiver_map);
      return false;
    }
    if (!receiver_map.prototype().equals(
            native_context().promise_prototype())) {
      return false;
    }
  }

  return true;
}

// ES section #sec-promise.prototype.catch
Reduction JSCallReducer::ReducePromisePrototypeCatch(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  int arity = static_cast<int>(p.arity() - 2);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!DoPromiseChecks(&inference)) return inference.NoChange();

  if (!dependencies()->DependOnPromiseThenProtector())
    return inference.NoChange();
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  // Massage the {node} to call "then" instead by first removing all inputs
  // following the onRejected parameter, and then filling up the parameters
  // to two inputs from the left with undefined.
  Node* target = jsgraph()->Constant(native_context().promise_then());
  NodeProperties::ReplaceValueInput(node, target, 0);
  NodeProperties::ReplaceEffectInput(node, effect);
  for (; arity > 1; --arity) node->RemoveInput(3);
  for (; arity < 2; ++arity) {
    node->InsertInput(graph()->zone(), 2, jsgraph()->UndefinedConstant());
  }
  NodeProperties::ChangeOp(
      node, javascript()->Call(2 + arity, p.frequency(), p.feedback(),
                               ConvertReceiverMode::kNotNullOrUndefined,
                               p.speculation_mode()));
  Reduction const reduction = ReducePromisePrototypeThen(node);
  return reduction.Changed() ? reduction : Changed(node);
}

Node* JSCallReducer::CreateClosureFromBuiltinSharedFunctionInfo(
    SharedFunctionInfoRef shared, Node* context, Node* effect, Node* control) {
  DCHECK(shared.HasBuiltinId());
  Callable const callable = Builtins::CallableFor(
      isolate(), static_cast<Builtins::Name>(shared.builtin_id()));
  return graph()->NewNode(
      javascript()->CreateClosure(
          shared.object(), factory()->many_closures_cell(), callable.code()),
      context, effect, control);
}

// ES section #sec-promise.prototype.finally
Reduction JSCallReducer::ReducePromisePrototypeFinally(Node* node) {
  DisallowHeapAccessIf no_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* on_finally = arity >= 1 ? NodeProperties::GetValueInput(node, 2)
                                : jsgraph()->UndefinedConstant();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  MapInference inference(broker(), receiver, effect);
  if (!DoPromiseChecks(&inference)) return inference.NoChange();
  MapHandles const& receiver_maps = inference.GetMaps();

  if (!dependencies()->DependOnPromiseHookProtector())
    return inference.NoChange();
  if (!dependencies()->DependOnPromiseThenProtector())
    return inference.NoChange();
  if (!dependencies()->DependOnPromiseSpeciesProtector())
    return inference.NoChange();
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  // Check if {on_finally} is callable, and if so wrap it into appropriate
  // closures that perform the finalization.
  Node* check = graph()->NewNode(simplified()->ObjectIsCallable(), on_finally);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* catch_true;
  Node* then_true;
  {
    Node* context = jsgraph()->Constant(native_context());
    Node* constructor =
        jsgraph()->Constant(native_context().promise_function());

    // Allocate shared context for the closures below.
    context = etrue =
        graph()->NewNode(javascript()->CreateFunctionContext(
                             native_context().scope_info().object(),
                             PromiseBuiltins::kPromiseFinallyContextLength -
                                 Context::MIN_CONTEXT_SLOTS,
                             FUNCTION_SCOPE),
                         context, etrue, if_true);
    etrue = graph()->NewNode(
        simplified()->StoreField(
            AccessBuilder::ForContextSlot(PromiseBuiltins::kOnFinallySlot)),
        context, on_finally, etrue, if_true);
    etrue = graph()->NewNode(
        simplified()->StoreField(
            AccessBuilder::ForContextSlot(PromiseBuiltins::kConstructorSlot)),
        context, constructor, etrue, if_true);

    // Allocate the closure for the reject case.
    catch_true = etrue = CreateClosureFromBuiltinSharedFunctionInfo(
        native_context().promise_catch_finally_shared_fun(), context, etrue,
        if_true);

    // Allocate the closure for the fulfill case.
    then_true = etrue = CreateClosureFromBuiltinSharedFunctionInfo(
        native_context().promise_then_finally_shared_fun(), context, etrue,
        if_true);
  }

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* catch_false = on_finally;
  Node* then_false = on_finally;

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
  Node* catch_finally =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       catch_true, catch_false, control);
  Node* then_finally =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       then_true, then_false, control);

  // At this point we definitely know that {receiver} has one of the
  // {receiver_maps}, so insert a MapGuard as a hint for the lowering
  // of the call to "then" below.
  {
    ZoneHandleSet<Map> maps;
    for (Handle<Map> map : receiver_maps) maps.insert(map, graph()->zone());
    effect = graph()->NewNode(simplified()->MapGuard(maps), receiver, effect,
                              control);
  }

  // Massage the {node} to call "then" instead by first removing all inputs
  // following the onFinally parameter, and then replacing the only parameter
  // input with the {on_finally} value.
  Node* target = jsgraph()->Constant(native_context().promise_then());
  NodeProperties::ReplaceValueInput(node, target, 0);
  NodeProperties::ReplaceEffectInput(node, effect);
  NodeProperties::ReplaceControlInput(node, control);
  for (; arity > 2; --arity) node->RemoveInput(2);
  for (; arity < 2; ++arity)
    node->InsertInput(graph()->zone(), 2, then_finally);
  node->ReplaceInput(2, then_finally);
  node->ReplaceInput(3, catch_finally);
  NodeProperties::ChangeOp(
      node, javascript()->Call(2 + arity, p.frequency(), p.feedback(),
                               ConvertReceiverMode::kNotNullOrUndefined,
                               p.speculation_mode()));
  Reduction const reduction = ReducePromisePrototypeThen(node);
  return reduction.Changed() ? reduction : Changed(node);
}

Reduction JSCallReducer::ReducePromisePrototypeThen(Node* node) {
  DisallowHeapAccessIf no_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* on_fulfilled = node->op()->ValueInputCount() > 2
                           ? NodeProperties::GetValueInput(node, 2)
                           : jsgraph()->UndefinedConstant();
  Node* on_rejected = node->op()->ValueInputCount() > 3
                          ? NodeProperties::GetValueInput(node, 3)
                          : jsgraph()->UndefinedConstant();
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!DoPromiseChecks(&inference)) return inference.NoChange();

  if (!dependencies()->DependOnPromiseHookProtector())
    return inference.NoChange();
  if (!dependencies()->DependOnPromiseSpeciesProtector())
    return inference.NoChange();
  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  // Check that {on_fulfilled} is callable.
  on_fulfilled = graph()->NewNode(
      common()->Select(MachineRepresentation::kTagged, BranchHint::kTrue),
      graph()->NewNode(simplified()->ObjectIsCallable(), on_fulfilled),
      on_fulfilled, jsgraph()->UndefinedConstant());

  // Check that {on_rejected} is callable.
  on_rejected = graph()->NewNode(
      common()->Select(MachineRepresentation::kTagged, BranchHint::kTrue),
      graph()->NewNode(simplified()->ObjectIsCallable(), on_rejected),
      on_rejected, jsgraph()->UndefinedConstant());

  // Create the resulting JSPromise.
  Node* promise = effect =
      graph()->NewNode(javascript()->CreatePromise(), context, effect);

  // Chain {result} onto {receiver}.
  promise = effect = graph()->NewNode(
      javascript()->PerformPromiseThen(), receiver, on_fulfilled, on_rejected,
      promise, context, frame_state, effect, control);

  // At this point we know that {promise} is going to have the
  // initial Promise map, since even if {PerformPromiseThen}
  // above called into the host rejection tracker, the {promise}
  // doesn't escape to user JavaScript. So bake this information
  // into the graph such that subsequent passes can use the
  // information for further optimizations.
  MapRef promise_map = native_context().promise_function().initial_map();
  effect = graph()->NewNode(
      simplified()->MapGuard(ZoneHandleSet<Map>(promise_map.object())), promise,
      effect, control);

  ReplaceWithValue(node, promise, effect, control);
  return Replace(promise);
}

// ES section #sec-promise.resolve
Reduction JSCallReducer::ReducePromiseResolveTrampoline(Node* node) {
  DisallowHeapAccessIf no_heap_access(FLAG_concurrent_inlining);

  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* value = node->op()->ValueInputCount() > 2
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->UndefinedConstant();
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Only reduce when the receiver is guaranteed to be a JSReceiver.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAreJSReceiver()) {
    return NoChange();
  }

  // Morph the {node} into a JSPromiseResolve operation.
  node->ReplaceInput(0, receiver);
  node->ReplaceInput(1, value);
  node->ReplaceInput(2, context);
  node->ReplaceInput(3, frame_state);
  node->ReplaceInput(4, effect);
  node->ReplaceInput(5, control);
  node->TrimInputCount(6);
  NodeProperties::ChangeOp(node, javascript()->PromiseResolve());
  return Changed(node);
}

// ES #sec-typedarray-constructors
Reduction JSCallReducer::ReduceTypedArrayConstructor(
    Node* node, const SharedFunctionInfoRef& shared) {
  DCHECK_EQ(IrOpcode::kJSConstruct, node->opcode());
  ConstructParameters const& p = ConstructParametersOf(node->op());
  int arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* arg1 = (arity >= 1) ? NodeProperties::GetValueInput(node, 1)
                            : jsgraph()->UndefinedConstant();
  Node* arg2 = (arity >= 2) ? NodeProperties::GetValueInput(node, 2)
                            : jsgraph()->UndefinedConstant();
  Node* arg3 = (arity >= 3) ? NodeProperties::GetValueInput(node, 3)
                            : jsgraph()->UndefinedConstant();
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Insert a construct stub frame into the chain of frame states. This will
  // reconstruct the proper frame when deoptimizing within the constructor.
  frame_state = CreateArtificialFrameState(
      node, frame_state, arity, BailoutId::ConstructStubInvoke(),
      FrameStateType::kConstructStub, shared, context);

  // This continuation just returns the newly created JSTypedArray. We
  // pass the_hole as the receiver, just like the builtin construct stub
  // does in this case.
  Node* const parameters[] = {jsgraph()->TheHoleConstant()};
  int const num_parameters = static_cast<int>(arraysize(parameters));
  frame_state = CreateJavaScriptBuiltinContinuationFrameState(
      jsgraph(), shared, Builtins::kGenericLazyDeoptContinuation, target,
      context, parameters, num_parameters, frame_state,
      ContinuationFrameStateMode::LAZY);

  Node* result =
      graph()->NewNode(javascript()->CreateTypedArray(), target, new_target,
                       arg1, arg2, arg3, context, frame_state, effect, control);
  return Replace(result);
}

// ES #sec-get-%typedarray%.prototype-@@tostringtag
Reduction JSCallReducer::ReduceTypedArrayPrototypeToStringTag(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  NodeVector values(graph()->zone());
  NodeVector effects(graph()->zone());
  NodeVector controls(graph()->zone());

  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
  control =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  values.push_back(jsgraph()->UndefinedConstant());
  effects.push_back(effect);
  controls.push_back(graph()->NewNode(common()->IfTrue(), control));

  control = graph()->NewNode(common()->IfFalse(), control);
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);
  Node* receiver_bit_field2 = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapBitField2()), receiver_map,
      effect, control);
  Node* receiver_elements_kind = graph()->NewNode(
      simplified()->NumberShiftRightLogical(),
      graph()->NewNode(simplified()->NumberBitwiseAnd(), receiver_bit_field2,
                       jsgraph()->Constant(Map::ElementsKindBits::kMask)),
      jsgraph()->Constant(Map::ElementsKindBits::kShift));

  // Offset the elements kind by FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND,
  // so that the branch cascade below is turned into a simple table
  // switch by the ControlFlowOptimizer later.
  receiver_elements_kind = graph()->NewNode(
      simplified()->NumberSubtract(), receiver_elements_kind,
      jsgraph()->Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)                      \
  do {                                                                 \
    Node* check = graph()->NewNode(                                    \
        simplified()->NumberEqual(), receiver_elements_kind,           \
        jsgraph()->Constant(TYPE##_ELEMENTS -                          \
                            FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));   \
    control = graph()->NewNode(common()->Branch(), check, control);    \
    if (FLAG_concurrent_inlining) {                                    \
      values.push_back(jsgraph()->Constant(                            \
          broker()->GetTypedArrayStringTag(TYPE##_ELEMENTS)));         \
    } else {                                                           \
      values.push_back(jsgraph()->HeapConstant(                        \
          factory()->InternalizeUtf8String(#Type "Array")));           \
    }                                                                  \
    effects.push_back(effect);                                         \
    controls.push_back(graph()->NewNode(common()->IfTrue(), control)); \
    control = graph()->NewNode(common()->IfFalse(), control);          \
  } while (false);
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  values.push_back(jsgraph()->UndefinedConstant());
  effects.push_back(effect);
  controls.push_back(control);

  int const count = static_cast<int>(controls.size());
  control = graph()->NewNode(common()->Merge(count), count, &controls.front());
  effects.push_back(control);
  effect =
      graph()->NewNode(common()->EffectPhi(count), count + 1, &effects.front());
  values.push_back(control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                       count + 1, &values.front());
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES #sec-number.isfinite
Reduction JSCallReducer::ReduceNumberIsFinite(Node* node) {
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* input = NodeProperties::GetValueInput(node, 2);
  Node* value = graph()->NewNode(simplified()->ObjectIsFiniteNumber(), input);
  ReplaceWithValue(node, value);
  return Replace(value);
}

// ES #sec-number.isfinite
Reduction JSCallReducer::ReduceNumberIsInteger(Node* node) {
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* input = NodeProperties::GetValueInput(node, 2);
  Node* value = graph()->NewNode(simplified()->ObjectIsInteger(), input);
  ReplaceWithValue(node, value);
  return Replace(value);
}

// ES #sec-number.issafeinteger
Reduction JSCallReducer::ReduceNumberIsSafeInteger(Node* node) {
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* input = NodeProperties::GetValueInput(node, 2);
  Node* value = graph()->NewNode(simplified()->ObjectIsSafeInteger(), input);
  ReplaceWithValue(node, value);
  return Replace(value);
}

// ES #sec-number.isnan
Reduction JSCallReducer::ReduceNumberIsNaN(Node* node) {
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* input = NodeProperties::GetValueInput(node, 2);
  Node* value = graph()->NewNode(simplified()->ObjectIsNaN(), input);
  ReplaceWithValue(node, value);
  return Replace(value);
}

Reduction JSCallReducer::ReduceMapPrototypeGet(Node* node) {
  // We only optimize if we have target, receiver and key parameters.
  if (node->op()->ValueInputCount() != 3) return NoChange();
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* key = NodeProperties::GetValueInput(node, 2);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAre(JS_MAP_TYPE)) {
    return NoChange();
  }

  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionTable()), receiver,
      effect, control);

  Node* entry = effect = graph()->NewNode(
      simplified()->FindOrderedHashMapEntry(), table, key, effect, control);

  Node* check = graph()->NewNode(simplified()->NumberEqual(), entry,
                                 jsgraph()->MinusOneConstant());

  Node* branch = graph()->NewNode(common()->Branch(), check, control);

  // Key not found.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->UndefinedConstant();

  // Key found.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse = efalse = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForOrderedHashMapEntryValue()),
      table, entry, efalse, if_false);

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* value = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), vtrue, vfalse, control);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSCallReducer::ReduceMapPrototypeHas(Node* node) {
  // We only optimize if we have target, receiver and key parameters.
  if (node->op()->ValueInputCount() != 3) return NoChange();
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* key = NodeProperties::GetValueInput(node, 2);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAre(JS_MAP_TYPE)) {
    return NoChange();
  }

  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionTable()), receiver,
      effect, control);

  Node* index = effect = graph()->NewNode(
      simplified()->FindOrderedHashMapEntry(), table, key, effect, control);

  Node* value = graph()->NewNode(simplified()->NumberEqual(), index,
                                 jsgraph()->MinusOneConstant());
  value = graph()->NewNode(simplified()->BooleanNot(), value);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

namespace {

InstanceType InstanceTypeForCollectionKind(CollectionKind kind) {
  switch (kind) {
    case CollectionKind::kMap:
      return JS_MAP_TYPE;
    case CollectionKind::kSet:
      return JS_SET_TYPE;
  }
  UNREACHABLE();
}

}  // namespace

Reduction JSCallReducer::ReduceCollectionIteration(
    Node* node, CollectionKind collection_kind, IterationKind iteration_kind) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  InstanceType type = InstanceTypeForCollectionKind(collection_kind);
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAre(type)) {
    return NoChange();
  }

  Node* js_create_iterator = effect = graph()->NewNode(
      javascript()->CreateCollectionIterator(collection_kind, iteration_kind),
      receiver, context, effect, control);
  ReplaceWithValue(node, js_create_iterator, effect);
  return Replace(js_create_iterator);
}

Reduction JSCallReducer::ReduceCollectionPrototypeSize(
    Node* node, CollectionKind collection_kind) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  InstanceType type = InstanceTypeForCollectionKind(collection_kind);
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAre(type)) {
    return NoChange();
  }

  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionTable()), receiver,
      effect, control);
  Node* value = effect = graph()->NewNode(
      simplified()->LoadField(
          AccessBuilder::ForOrderedHashMapOrSetNumberOfElements()),
      table, effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSCallReducer::ReduceCollectionIteratorPrototypeNext(
    Node* node, int entry_size, Handle<HeapObject> empty_collection,
    InstanceType collection_iterator_instance_type_first,
    InstanceType collection_iterator_instance_type_last) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // A word of warning to begin with: This whole method might look a bit
  // strange at times, but that's mostly because it was carefully handcrafted
  // to allow for full escape analysis and scalar replacement of both the
  // collection iterator object and the iterator results, including the
  // key-value arrays in case of Set/Map entry iteration.
  //
  // TODO(turbofan): Currently the escape analysis (and the store-load
  // forwarding) is unable to eliminate the allocations for the key-value
  // arrays in case of Set/Map entry iteration, and we should investigate
  // how to update the escape analysis / arrange the graph in a way that
  // this becomes possible.

  InstanceType receiver_instance_type;
  {
    MapInference inference(broker(), receiver, effect);
    if (!inference.HaveMaps()) return NoChange();
    MapHandles const& receiver_maps = inference.GetMaps();
    receiver_instance_type = MapRef(broker(), receiver_maps[0]).instance_type();
    for (size_t i = 1; i < receiver_maps.size(); ++i) {
      if (MapRef(broker(), receiver_maps[i]).instance_type() !=
          receiver_instance_type) {
        return inference.NoChange();
      }
    }
    if (receiver_instance_type < collection_iterator_instance_type_first ||
        receiver_instance_type > collection_iterator_instance_type_last) {
      return inference.NoChange();
    }
    inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                        control, p.feedback());
  }

  // Transition the JSCollectionIterator {receiver} if necessary
  // (i.e. there were certain mutations while we're iterating).
  {
    Node* done_loop;
    Node* done_eloop;
    Node* loop = control =
        graph()->NewNode(common()->Loop(2), control, control);
    Node* eloop = effect =
        graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
    Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
    NodeProperties::MergeControlToEnd(graph(), common(), terminate);

    // Check if reached the final table of the {receiver}.
    Node* table = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorTable()),
        receiver, effect, control);
    Node* next_table = effect =
        graph()->NewNode(simplified()->LoadField(
                             AccessBuilder::ForOrderedHashMapOrSetNextTable()),
                         table, effect, control);
    Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), next_table);
    control =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    // Abort the {loop} when we reach the final table.
    done_loop = graph()->NewNode(common()->IfTrue(), control);
    done_eloop = effect;

    // Migrate to the {next_table} otherwise.
    control = graph()->NewNode(common()->IfFalse(), control);

    // Self-heal the {receiver}s index.
    Node* index = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorIndex()),
        receiver, effect, control);
    Callable const callable =
        Builtins::CallableFor(isolate(), Builtins::kOrderedHashTableHealIndex);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(),
        CallDescriptor::kNoFlags, Operator::kEliminatable);
    index = effect =
        graph()->NewNode(common()->Call(call_descriptor),
                         jsgraph()->HeapConstant(callable.code()), table, index,
                         jsgraph()->NoContextConstant(), effect);

    index = effect = graph()->NewNode(
        common()->TypeGuard(TypeCache::Get()->kFixedArrayLengthType), index,
        effect, control);

    // Update the {index} and {table} on the {receiver}.
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSCollectionIteratorIndex()),
        receiver, index, effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSCollectionIteratorTable()),
        receiver, next_table, effect, control);

    // Tie the knot.
    loop->ReplaceInput(1, control);
    eloop->ReplaceInput(1, effect);

    control = done_loop;
    effect = done_eloop;
  }

  // Get current index and table from the JSCollectionIterator {receiver}.
  Node* index = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorIndex()),
      receiver, effect, control);
  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorTable()),
      receiver, effect, control);

  // Create the {JSIteratorResult} first to ensure that we always have
  // a dominating Allocate node for the allocation folding phase.
  Node* iterator_result = effect = graph()->NewNode(
      javascript()->CreateIterResultObject(), jsgraph()->UndefinedConstant(),
      jsgraph()->TrueConstant(), context, effect);

  // Look for the next non-holey key, starting from {index} in the {table}.
  Node* controls[2];
  Node* effects[3];
  {
    // Compute the currently used capacity.
    Node* number_of_buckets = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashMapOrSetNumberOfBuckets()),
        table, effect, control);
    Node* number_of_elements = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashMapOrSetNumberOfElements()),
        table, effect, control);
    Node* number_of_deleted_elements = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashMapOrSetNumberOfDeletedElements()),
        table, effect, control);
    Node* used_capacity =
        graph()->NewNode(simplified()->NumberAdd(), number_of_elements,
                         number_of_deleted_elements);

    // Skip holes and update the {index}.
    Node* loop = graph()->NewNode(common()->Loop(2), control, control);
    Node* eloop =
        graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
    Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
    NodeProperties::MergeControlToEnd(graph(), common(), terminate);
    Node* iloop = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, 2), index, index, loop);

    Node* index = effect = graph()->NewNode(
        common()->TypeGuard(TypeCache::Get()->kFixedArrayLengthType), iloop,
        eloop, control);
    {
      Node* check0 = graph()->NewNode(simplified()->NumberLessThan(), index,
                                      used_capacity);
      Node* branch0 =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, loop);

      Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
      Node* efalse0 = effect;
      {
        // Mark the {receiver} as exhausted.
        efalse0 = graph()->NewNode(
            simplified()->StoreField(
                AccessBuilder::ForJSCollectionIteratorTable()),
            receiver, jsgraph()->HeapConstant(empty_collection), efalse0,
            if_false0);

        controls[0] = if_false0;
        effects[0] = efalse0;
      }

      Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
      Node* etrue0 = effect;
      {
        // Load the key of the entry.
        STATIC_ASSERT(OrderedHashMap::HashTableStartIndex() ==
                      OrderedHashSet::HashTableStartIndex());
        Node* entry_start_position = graph()->NewNode(
            simplified()->NumberAdd(),
            graph()->NewNode(
                simplified()->NumberAdd(),
                graph()->NewNode(simplified()->NumberMultiply(), index,
                                 jsgraph()->Constant(entry_size)),
                number_of_buckets),
            jsgraph()->Constant(OrderedHashMap::HashTableStartIndex()));
        Node* entry_key = etrue0 = graph()->NewNode(
            simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
            table, entry_start_position, etrue0, if_true0);

        // Advance the index.
        index = graph()->NewNode(simplified()->NumberAdd(), index,
                                 jsgraph()->OneConstant());

        Node* check1 =
            graph()->NewNode(simplified()->ReferenceEqual(), entry_key,
                             jsgraph()->TheHoleConstant());
        Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                         check1, if_true0);

        {
          // Abort loop with resulting value.
          Node* control = graph()->NewNode(common()->IfFalse(), branch1);
          Node* effect = etrue0;
          Node* value = effect =
              graph()->NewNode(common()->TypeGuard(Type::NonInternal()),
                               entry_key, effect, control);
          Node* done = jsgraph()->FalseConstant();

          // Advance the index on the {receiver}.
          effect = graph()->NewNode(
              simplified()->StoreField(
                  AccessBuilder::ForJSCollectionIteratorIndex()),
              receiver, index, effect, control);

          // The actual {value} depends on the {receiver} iteration type.
          switch (receiver_instance_type) {
            case JS_MAP_KEY_ITERATOR_TYPE:
            case JS_SET_VALUE_ITERATOR_TYPE:
              break;

            case JS_SET_KEY_VALUE_ITERATOR_TYPE:
              value = effect =
                  graph()->NewNode(javascript()->CreateKeyValueArray(), value,
                                   value, context, effect);
              break;

            case JS_MAP_VALUE_ITERATOR_TYPE:
              value = effect = graph()->NewNode(
                  simplified()->LoadElement(
                      AccessBuilder::ForFixedArrayElement()),
                  table,
                  graph()->NewNode(
                      simplified()->NumberAdd(), entry_start_position,
                      jsgraph()->Constant(OrderedHashMap::kValueOffset)),
                  effect, control);
              break;

            case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
              value = effect = graph()->NewNode(
                  simplified()->LoadElement(
                      AccessBuilder::ForFixedArrayElement()),
                  table,
                  graph()->NewNode(
                      simplified()->NumberAdd(), entry_start_position,
                      jsgraph()->Constant(OrderedHashMap::kValueOffset)),
                  effect, control);
              value = effect =
                  graph()->NewNode(javascript()->CreateKeyValueArray(),
                                   entry_key, value, context, effect);
              break;

            default:
              UNREACHABLE();
              break;
          }

          // Store final {value} and {done} into the {iterator_result}.
          effect =
              graph()->NewNode(simplified()->StoreField(
                                   AccessBuilder::ForJSIteratorResultValue()),
                               iterator_result, value, effect, control);
          effect =
              graph()->NewNode(simplified()->StoreField(
                                   AccessBuilder::ForJSIteratorResultDone()),
                               iterator_result, done, effect, control);

          controls[1] = control;
          effects[1] = effect;
        }

        // Continue with next loop index.
        loop->ReplaceInput(1, graph()->NewNode(common()->IfTrue(), branch1));
        eloop->ReplaceInput(1, etrue0);
        iloop->ReplaceInput(1, index);
      }
    }

    control = effects[2] = graph()->NewNode(common()->Merge(2), 2, controls);
    effect = graph()->NewNode(common()->EffectPhi(2), 3, effects);
  }

  // Yield the final {iterator_result}.
  ReplaceWithValue(node, iterator_result, effect, control);
  return Replace(iterator_result);
}

Reduction JSCallReducer::ReduceArrayBufferIsView(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  Node* value = node->op()->ValueInputCount() >= 3
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->UndefinedConstant();
  RelaxEffectsAndControls(node);
  node->ReplaceInput(0, value);
  node->TrimInputCount(1);
  NodeProperties::ChangeOp(node, simplified()->ObjectIsArrayBufferView());
  return Changed(node);
}

Reduction JSCallReducer::ReduceArrayBufferViewAccessor(
    Node* node, InstanceType instance_type, FieldAccess const& access) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() ||
      !inference.AllOfInstanceTypesAre(instance_type)) {
    return NoChange();
  }

  // Load the {receiver}s field.
  Node* value = effect = graph()->NewNode(simplified()->LoadField(access),
                                          receiver, effect, control);

  // See if we can skip the detaching check.
  if (!dependencies()->DependOnArrayBufferDetachingProtector()) {
    // Check whether {receiver}s JSArrayBuffer was detached.
    Node* buffer = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
        receiver, effect, control);
    Node* buffer_bit_field = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferBitField()),
        buffer, effect, control);
    Node* check = graph()->NewNode(
        simplified()->NumberEqual(),
        graph()->NewNode(
            simplified()->NumberBitwiseAnd(), buffer_bit_field,
            jsgraph()->Constant(JSArrayBuffer::WasDetachedBit::kMask)),
        jsgraph()->ZeroConstant());

    // TODO(turbofan): Ideally we would bail out here if the {receiver}s
    // JSArrayBuffer was detached, but there's no way to guard against
    // deoptimization loops right now, since the JSCall {node} is usually
    // created from a LOAD_IC inlining, and so there's no CALL_IC slot
    // from which we could use the speculation bit.
    value = graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged, BranchHint::kTrue),
        check, value, jsgraph()->ZeroConstant());
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

namespace {
uint32_t ExternalArrayElementSize(const ExternalArrayType element_type) {
  switch (element_type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                    \
    DCHECK_LE(sizeof(ctype), 8);                  \
    return sizeof(ctype);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
    default:
      UNREACHABLE();
#undef TYPED_ARRAY_CASE
  }
}
}  // namespace

Reduction JSCallReducer::ReduceDataViewAccess(Node* node, DataViewAccess access,
                                              ExternalArrayType element_type) {
  size_t const element_size = ExternalArrayElementSize(element_type);
  CallParameters const& p = CallParametersOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* offset = node->op()->ValueInputCount() > 2
                     ? NodeProperties::GetValueInput(node, 2)
                     : jsgraph()->ZeroConstant();
  Node* value = (access == DataViewAccess::kGet)
                    ? nullptr
                    : (node->op()->ValueInputCount() > 3
                           ? NodeProperties::GetValueInput(node, 3)
                           : jsgraph()->ZeroConstant());
  Node* is_little_endian = (access == DataViewAccess::kGet)
                               ? (node->op()->ValueInputCount() > 3
                                      ? NodeProperties::GetValueInput(node, 3)
                                      : jsgraph()->FalseConstant())
                               : (node->op()->ValueInputCount() > 4
                                      ? NodeProperties::GetValueInput(node, 4)
                                      : jsgraph()->FalseConstant());

  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  // Only do stuff if the {receiver} is really a DataView.
  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() ||
      !inference.AllOfInstanceTypesAre(JS_DATA_VIEW_TYPE)) {
    return NoChange();
  }

  // Check that the {offset} is within range for the {receiver}.
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    // We only deal with DataViews here whose [[ByteLength]] is at least
    // {element_size}, as for all other DataViews it'll be out-of-bounds.
    JSDataViewRef dataview = m.Ref(broker()).AsJSDataView();
    if (dataview.byte_length() < element_size) return NoChange();

    // Check that the {offset} is within range of the {byte_length}.
    Node* byte_length =
        jsgraph()->Constant(dataview.byte_length() - (element_size - 1));
    offset = effect = graph()->NewNode(simplified()->CheckBounds(p.feedback()),
                                       offset, byte_length, effect, control);
  } else {
    // We only deal with DataViews here that have Smi [[ByteLength]]s.
    Node* byte_length = effect =
        graph()->NewNode(simplified()->LoadField(
                             AccessBuilder::ForJSArrayBufferViewByteLength()),
                         receiver, effect, control);

    if (element_size > 1) {
      // For non-byte accesses we also need to check that the {offset}
      // plus the {element_size}-1 fits within the given {byte_length}.
      // So to keep this as a single check on the {offset}, we subtract
      // the {element_size}-1 from the {byte_length} here (clamped to
      // positive safe integer range), and perform a check against that
      // with the {offset} below.
      byte_length = graph()->NewNode(
          simplified()->NumberMax(), jsgraph()->ZeroConstant(),
          graph()->NewNode(simplified()->NumberSubtract(), byte_length,
                           jsgraph()->Constant(element_size - 1)));
    }

    // Check that the {offset} is within range of the {byte_length}.
    offset = effect = graph()->NewNode(simplified()->CheckBounds(p.feedback()),
                                       offset, byte_length, effect, control);
  }

  // Coerce {is_little_endian} to boolean.
  is_little_endian =
      graph()->NewNode(simplified()->ToBoolean(), is_little_endian);

  // Coerce {value} to Number.
  if (access == DataViewAccess::kSet) {
    value = effect = graph()->NewNode(
        simplified()->SpeculativeToNumber(NumberOperationHint::kNumberOrOddball,
                                          p.feedback()),
        value, effect, control);
  }

  // We need to retain either the {receiver} itself or it's backing
  // JSArrayBuffer to make sure that the GC doesn't collect the raw
  // memory. We default to {receiver} here, and only use the buffer
  // if we anyways have to load it (to reduce register pressure).
  Node* buffer_or_receiver = receiver;

  if (!dependencies()->DependOnArrayBufferDetachingProtector()) {
    // Get the underlying buffer and check that it has not been detached.
    Node* buffer = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
        receiver, effect, control);

    // Bail out if the {buffer} was detached.
    Node* buffer_bit_field = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferBitField()),
        buffer, effect, control);
    Node* check = graph()->NewNode(
        simplified()->NumberEqual(),
        graph()->NewNode(
            simplified()->NumberBitwiseAnd(), buffer_bit_field,
            jsgraph()->Constant(JSArrayBuffer::WasDetachedBit::kMask)),
        jsgraph()->ZeroConstant());
    effect = graph()->NewNode(
        simplified()->CheckIf(DeoptimizeReason::kArrayBufferWasDetached,
                              p.feedback()),
        check, effect, control);

    // We can reduce register pressure by holding on to the {buffer}
    // now to retain the backing store memory.
    buffer_or_receiver = buffer;
  }

  // Load the {receiver}s data pointer.
  Node* data_pointer = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSDataViewDataPointer()),
      receiver, effect, control);

  switch (access) {
    case DataViewAccess::kGet:
      // Perform the load.
      value = effect = graph()->NewNode(
          simplified()->LoadDataViewElement(element_type), buffer_or_receiver,
          data_pointer, offset, is_little_endian, effect, control);
      break;
    case DataViewAccess::kSet:
      // Perform the store.
      effect = graph()->NewNode(
          simplified()->StoreDataViewElement(element_type), buffer_or_receiver,
          data_pointer, offset, value, is_little_endian, effect, control);
      value = jsgraph()->UndefinedConstant();
      break;
  }

  ReplaceWithValue(node, value, effect, control);
  return Changed(value);
}

// ES6 section 18.2.2 isFinite ( number )
Reduction JSCallReducer::ReduceGlobalIsFinite(Node* node) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* input = NodeProperties::GetValueInput(node, 2);

  input = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       input, effect, control);
  Node* value = graph()->NewNode(simplified()->NumberIsFinite(), input);
  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

// ES6 section 18.2.3 isNaN ( number )
Reduction JSCallReducer::ReduceGlobalIsNaN(Node* node) {
  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->TrueConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* input = NodeProperties::GetValueInput(node, 2);

  input = effect =
      graph()->NewNode(simplified()->SpeculativeToNumber(
                           NumberOperationHint::kNumberOrOddball, p.feedback()),
                       input, effect, control);
  Node* value = graph()->NewNode(simplified()->NumberIsNaN(), input);
  ReplaceWithValue(node, value, effect);
  return Replace(value);
}

// ES6 section 20.3.4.10 Date.prototype.getTime ( )
Reduction JSCallReducer::ReduceDatePrototypeGetTime(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAre(JS_DATE_TYPE)) {
    return NoChange();
  }

  Node* value = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForJSDateValue()),
                       receiver, effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES6 section 20.3.3.1 Date.now ( )
Reduction JSCallReducer::ReduceDateNow(Node* node) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* value = effect =
      graph()->NewNode(simplified()->DateNow(), effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES6 section 20.1.2.13 Number.parseInt ( string, radix )
Reduction JSCallReducer::ReduceNumberParseInt(Node* node) {
  // We certainly know that undefined is not an array.
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->NaNConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  int arg_count = node->op()->ValueInputCount();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* object = NodeProperties::GetValueInput(node, 2);
  Node* radix = arg_count >= 4 ? NodeProperties::GetValueInput(node, 3)
                               : jsgraph()->UndefinedConstant();
  node->ReplaceInput(0, object);
  node->ReplaceInput(1, radix);
  node->ReplaceInput(2, context);
  node->ReplaceInput(3, frame_state);
  node->ReplaceInput(4, effect);
  node->ReplaceInput(5, control);
  node->TrimInputCount(6);
  NodeProperties::ChangeOp(node, javascript()->ParseInt());
  return Changed(node);
}

Reduction JSCallReducer::ReduceRegExpPrototypeTest(Node* node) {
  DisallowHeapAccessIf disallow_heap_access(FLAG_concurrent_inlining);

  if (FLAG_force_slow_path) return NoChange();
  if (node->op()->ValueInputCount() < 3) return NoChange();

  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* regexp = NodeProperties::GetValueInput(node, 1);

  MapInference inference(broker(), regexp, effect);
  if (!inference.HaveMaps() ||
      !inference.AllOfInstanceTypes(InstanceTypeChecker::IsJSRegExp)) {
    return inference.NoChange();
  }
  MapHandles const& regexp_maps = inference.GetMaps();

  ZoneVector<PropertyAccessInfo> access_infos(graph()->zone());
  AccessInfoFactory access_info_factory(broker(), dependencies(),
                                        graph()->zone());
  if (FLAG_concurrent_inlining) {
    // Obtain precomputed access infos from the broker.
    for (auto map : regexp_maps) {
      MapRef map_ref(broker(), map);
      PropertyAccessInfo access_info = broker()->GetPropertyAccessInfo(
          map_ref, NameRef(broker(), isolate()->factory()->exec_string()),
          AccessMode::kLoad);
      access_infos.push_back(access_info);
    }
  } else {
    // Compute property access info for "exec" on {resolution}.
    access_info_factory.ComputePropertyAccessInfos(
        MapHandles(regexp_maps.begin(), regexp_maps.end()),
        factory()->exec_string(), AccessMode::kLoad, &access_infos);
  }

  PropertyAccessInfo ai_exec =
      access_info_factory.FinalizePropertyAccessInfosAsOne(access_infos,
                                                           AccessMode::kLoad);
  if (ai_exec.IsInvalid()) return inference.NoChange();

  // If "exec" has been modified on {regexp}, we can't do anything.
  if (ai_exec.IsDataConstant()) {
    Handle<JSObject> holder;
    // Do not reduce if the exec method is not on the prototype chain.
    if (!ai_exec.holder().ToHandle(&holder)) return inference.NoChange();

    JSObjectRef holder_ref(broker(), holder);

    // Bail out if the exec method is not the original one.
    base::Optional<ObjectRef> constant = holder_ref.GetOwnDataProperty(
        ai_exec.field_representation(), ai_exec.field_index());
    if (!constant.has_value() ||
        !constant->equals(native_context().regexp_exec_function())) {
      return inference.NoChange();
    }

    // Add proper dependencies on the {regexp}s [[Prototype]]s.
    dependencies()->DependOnStablePrototypeChains(
        ai_exec.receiver_maps(), kStartAtPrototype,
        JSObjectRef(broker(), holder));
  } else {
    return inference.NoChange();
  }

  inference.RelyOnMapsPreferStability(dependencies(), jsgraph(), &effect,
                                      control, p.feedback());

  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* search = NodeProperties::GetValueInput(node, 2);
  Node* search_string = effect = graph()->NewNode(
      simplified()->CheckString(p.feedback()), search, effect, control);

  Node* lastIndex = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSRegExpLastIndex()), regexp,
      effect, control);

  Node* lastIndexSmi = effect = graph()->NewNode(
      simplified()->CheckSmi(p.feedback()), lastIndex, effect, control);

  Node* is_positive = graph()->NewNode(simplified()->NumberLessThanOrEqual(),
                                       jsgraph()->ZeroConstant(), lastIndexSmi);

  effect = graph()->NewNode(
      simplified()->CheckIf(DeoptimizeReason::kNotASmi, p.feedback()),
      is_positive, effect, control);

  node->ReplaceInput(0, regexp);
  node->ReplaceInput(1, search_string);
  node->ReplaceInput(2, context);
  node->ReplaceInput(3, frame_state);
  node->ReplaceInput(4, effect);
  node->ReplaceInput(5, control);
  node->TrimInputCount(6);
  NodeProperties::ChangeOp(node, javascript()->RegExpTest());
  return Changed(node);
}

// ES section #sec-number-constructor
Reduction JSCallReducer::ReduceNumberConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  CallParameters const& p = CallParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* value = p.arity() < 3 ? jsgraph()->ZeroConstant()
                              : NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);

  // Create the artificial frame state in the middle of the Number constructor.
  SharedFunctionInfoRef shared_info =
      native_context().number_function().shared();
  Node* stack_parameters[] = {receiver};
  int stack_parameter_count = arraysize(stack_parameters);
  Node* continuation_frame_state =
      CreateJavaScriptBuiltinContinuationFrameState(
          jsgraph(), shared_info, Builtins::kGenericLazyDeoptContinuation,
          target, context, stack_parameters, stack_parameter_count, frame_state,
          ContinuationFrameStateMode::LAZY);

  // Convert the {value} to a Number.
  NodeProperties::ReplaceValueInputs(node, value);
  NodeProperties::ChangeOp(node, javascript()->ToNumberConvertBigInt());
  NodeProperties::ReplaceFrameStateInput(node, continuation_frame_state);
  return Changed(node);
}

Reduction JSCallReducer::ReduceBigIntAsUintN(Node* node) {
  if (!jsgraph()->machine()->Is64()) {
    return NoChange();
  }

  CallParameters const& p = CallParametersOf(node->op());
  if (p.speculation_mode() == SpeculationMode::kDisallowSpeculation) {
    return NoChange();
  }
  if (node->op()->ValueInputCount() < 3) {
    return NoChange();
  }

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* bits = NodeProperties::GetValueInput(node, 2);
  Node* value = NodeProperties::GetValueInput(node, 3);

  NumberMatcher matcher(bits);
  if (matcher.IsInteger() && matcher.IsInRange(0, 64)) {
    const int bits_value = static_cast<int>(matcher.Value());
    value = effect = graph()->NewNode(simplified()->CheckBigInt(p.feedback()),
                                      value, effect, control);
    value = graph()->NewNode(simplified()->BigIntAsUintN(bits_value), value);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }

  return NoChange();
}

Graph* JSCallReducer::graph() const { return jsgraph()->graph(); }

Isolate* JSCallReducer::isolate() const { return jsgraph()->isolate(); }

Factory* JSCallReducer::factory() const { return isolate()->factory(); }

NativeContextRef JSCallReducer::native_context() const {
  return broker()->target_native_context();
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
