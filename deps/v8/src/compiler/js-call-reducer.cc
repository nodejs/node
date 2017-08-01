// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-call-reducer.h"

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compilation-dependencies.h"
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
    case IrOpcode::kJSConstructWithSpread:
      return ReduceJSConstructWithSpread(node);
    case IrOpcode::kJSCall:
      return ReduceJSCall(node);
    case IrOpcode::kJSCallWithSpread:
      return ReduceJSCallWithSpread(node);
    default:
      break;
  }
  return NoChange();
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


// ES6 section 19.2.3.1 Function.prototype.apply ( thisArg, argArray )
Reduction JSCallReducer::ReduceFunctionPrototypeApply(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* target = NodeProperties::GetValueInput(node, 0);
  CallParameters const& p = CallParametersOf(node->op());
  // Tail calls to Function.prototype.apply are not properly supported
  // down the pipeline, so we disable this optimization completely for
  // tail calls (for now).
  if (p.tail_call_mode() == TailCallMode::kAllow) return NoChange();
  Handle<JSFunction> apply =
      Handle<JSFunction>::cast(HeapObjectMatcher(target).Value());
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
  } else if (arity == 4) {
    // Check if argArray is an arguments object, and {node} is the only value
    // user of argArray (except for value uses in frame states).
    Node* arg_array = NodeProperties::GetValueInput(node, 3);
    if (arg_array->opcode() != IrOpcode::kJSCreateArguments) return NoChange();
    for (Edge edge : arg_array->use_edges()) {
      Node* const user = edge.from();
      if (user == node) continue;
      // Ignore uses as frame state's locals or parameters.
      if (user->opcode() == IrOpcode::kStateValues) continue;
      // Ignore uses as frame state's accumulator.
      if (user->opcode() == IrOpcode::kFrameState &&
          user->InputAt(2) == arg_array) {
        continue;
      }
      if (!NodeProperties::IsValueEdge(edge)) continue;
      return NoChange();
    }
    // Check if the arguments can be handled in the fast case (i.e. we don't
    // have aliased sloppy arguments), and compute the {start_index} for
    // rest parameters.
    CreateArgumentsType const type = CreateArgumentsTypeOf(arg_array->op());
    Node* frame_state = NodeProperties::GetFrameStateInput(arg_array);
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
        while (effect != arg_array) {
          if (effect->op()->EffectInputCount() != 1 ||
              !(effect->op()->properties() & Operator::kNoWrite)) {
            return NoChange();
          }
          effect = NodeProperties::GetEffectInput(effect);
        }
      }
    } else if (type == CreateArgumentsType::kRestParameter) {
      start_index = formal_parameter_count;
    }
    // Check if are applying to inlined arguments or to the arguments of
    // the outermost function.
    Node* outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
    if (outer_state->opcode() != IrOpcode::kFrameState) {
      // Reduce {node} to a JSCallForwardVarargs operation, which just
      // re-pushes the incoming arguments and calls the {target}.
      node->RemoveInput(0);  // Function.prototype.apply
      node->RemoveInput(2);  // arguments
      NodeProperties::ChangeOp(node, javascript()->CallForwardVarargs(
                                         2, start_index, p.tail_call_mode()));
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
    // Remove the argArray input from the {node}.
    node->RemoveInput(static_cast<int>(--arity));
    // Add the actual parameters to the {node}, skipping the receiver,
    // starting from {start_index}.
    Node* const parameters = frame_state->InputAt(kFrameStateParametersInput);
    for (int i = start_index + 1; i < parameters->InputCount(); ++i) {
      node->InsertInput(graph()->zone(), static_cast<int>(arity),
                        parameters->InputAt(i));
      ++arity;
    }
    // Drop the {target} from the {node}.
    node->RemoveInput(0);
    --arity;
  } else {
    return NoChange();
  }
  // Change {node} to the new {JSCall} operator.
  NodeProperties::ChangeOp(
      node,
      javascript()->Call(arity, p.frequency(), VectorSlotPair(), convert_mode,
                         p.tail_call_mode()));
  // Change context of {node} to the Function.prototype.apply context,
  // to ensure any exception is thrown in the correct context.
  NodeProperties::ReplaceContextInput(
      node, jsgraph()->HeapConstant(handle(apply->context(), isolate())));
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
      javascript()->Call(arity, p.frequency(), VectorSlotPair(), convert_mode,
                         p.tail_call_mode()));
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
    Handle<Map> candidate_map(
        object_maps[0]->GetPrototypeChainRootMap(isolate()));
    Handle<Object> candidate_prototype(candidate_map->prototype(), isolate());

    // We cannot deal with primitives here.
    if (candidate_map->IsPrimitiveMap()) return NoChange();

    // Check if we can constant-fold the {candidate_prototype}.
    for (size_t i = 0; i < object_maps.size(); ++i) {
      Handle<Map> const object_map(
          object_maps[i]->GetPrototypeChainRootMap(isolate()));
      if (object_map->IsSpecialReceiverMap() ||
          object_map->has_hidden_prototype() ||
          object_map->prototype() != *candidate_prototype) {
        // We exclude special receivers, like JSProxy or API objects that
        // might require access checks here; we also don't want to deal
        // with hidden prototypes at this point.
        return NoChange();
      }
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

// ES6 section 26.1.7 Reflect.getPrototypeOf ( target )
Reduction JSCallReducer::ReduceReflectGetPrototypeOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* target = (node->op()->ValueInputCount() >= 3)
                     ? NodeProperties::GetValueInput(node, 2)
                     : jsgraph()->UndefinedConstant();
  return ReduceObjectGetPrototype(node, target);
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

Reduction JSCallReducer::ReduceSpreadCall(Node* node, int arity) {
  DCHECK(node->opcode() == IrOpcode::kJSCallWithSpread ||
         node->opcode() == IrOpcode::kJSConstructWithSpread);

  // Do check to make sure we can actually avoid iteration.
  if (!isolate()->initial_array_iterator_prototype_map()->is_stable()) {
    return NoChange();
  }

  Node* spread = NodeProperties::GetValueInput(node, arity);

  // Check if spread is an arguments object, and {node} is the only value user
  // of spread (except for value uses in frame states).
  if (spread->opcode() != IrOpcode::kJSCreateArguments) return NoChange();
  for (Edge edge : spread->use_edges()) {
    Node* const user = edge.from();
    if (user == node) continue;
    // Ignore uses as frame state's locals or parameters.
    if (user->opcode() == IrOpcode::kStateValues) continue;
    // Ignore uses as frame state's accumulator.
    if (user->opcode() == IrOpcode::kFrameState && user->InputAt(2) == spread) {
      continue;
    }
    if (!NodeProperties::IsValueEdge(edge)) continue;
    return NoChange();
  }

  // Get to the actual frame state from which to extract the arguments;
  // we can only optimize this in case the {node} was already inlined into
  // some other function (and same for the {spread}).
  CreateArgumentsType const type = CreateArgumentsTypeOf(spread->op());
  Node* frame_state = NodeProperties::GetFrameStateInput(spread);
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
      while (effect != spread) {
        if (effect->op()->EffectInputCount() != 1 ||
            !(effect->op()->properties() & Operator::kNoWrite)) {
          return NoChange();
        }
        effect = NodeProperties::GetEffectInput(effect);
      }
    }
  } else if (type == CreateArgumentsType::kRestParameter) {
    start_index = formal_parameter_count;

    // Only check the array iterator protector when we have a rest object.
    if (!isolate()->IsArrayIteratorLookupChainIntact()) return NoChange();
  }

  // Install appropriate code dependencies.
  dependencies()->AssumeMapStable(
      isolate()->initial_array_iterator_prototype_map());
  if (type == CreateArgumentsType::kRestParameter) {
    dependencies()->AssumePropertyCell(factory()->array_iterator_protector());
  }
  // Remove the spread input from the {node}.
  node->RemoveInput(arity--);
  // Check if are spreading to inlined arguments or to the arguments of
  // the outermost function.
  Node* outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    Operator const* op =
        (node->opcode() == IrOpcode::kJSCallWithSpread)
            ? javascript()->CallForwardVarargs(arity + 1, start_index,
                                               TailCallMode::kDisallow)
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

  // TODO(turbofan): Collect call counts on spread call/construct and thread it
  // through here.
  if (node->opcode() == IrOpcode::kJSCallWithSpread) {
    NodeProperties::ChangeOp(node, javascript()->Call(arity + 1));
    Reduction const r = ReduceJSCall(node);
    return r.Changed() ? r : Changed(node);
  } else {
    NodeProperties::ChangeOp(node, javascript()->Construct(arity + 2));
    Reduction const r = ReduceJSConstruct(node);
    return r.Changed() ? r : Changed(node);
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
        case Builtins::kReflectGetPrototypeOf:
          return ReduceReflectGetPrototypeOf(node);
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
          node,
          javascript()->Call(arity, p.frequency(), VectorSlotPair(),
                             convert_mode, p.tail_call_mode()));
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
    // TODO(turbofan): Tail-calling to a CallIC stub is not supported.
    if (p.tail_call_mode() == TailCallMode::kAllow) return NoChange();

    // Insert a CallIC here to collect feedback for uninitialized calls.
    int const arg_count = static_cast<int>(p.arity() - 2);
    Callable callable = CodeFactory::CallIC(isolate(), p.convert_mode());
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), arg_count + 1,
        flags);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* stub_arity = jsgraph()->Constant(arg_count);
    Node* slot_index =
        jsgraph()->Constant(FeedbackVector::GetIndex(p.feedback().slot()));
    Node* feedback_vector = jsgraph()->HeapConstant(p.feedback().vector());
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->InsertInput(graph()->zone(), 2, stub_arity);
    node->InsertInput(graph()->zone(), 3, slot_index);
    node->InsertInput(graph()->zone(), 4, feedback_vector);
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
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

Reduction JSCallReducer::ReduceJSCallWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallWithSpread, node->opcode());
  SpreadWithArityParameter const& p = SpreadWithArityParameterOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 1);

  return ReduceSpreadCall(node, arity);
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

  if (!p.feedback().IsValid()) return NoChange();
  CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
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

Reduction JSCallReducer::ReduceJSConstructWithSpread(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConstructWithSpread, node->opcode());
  SpreadWithArityParameter const& p = SpreadWithArityParameterOf(node->op());
  DCHECK_LE(3u, p.arity());
  int arity = static_cast<int>(p.arity() - 2);

  return ReduceSpreadCall(node, arity);
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
