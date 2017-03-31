// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-call-reducer.h"

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/simplified-operator.h"
#include "src/feedback-vector-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSCallReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSCallConstruct:
      return ReduceJSCallConstruct(node);
    case IrOpcode::kJSCallFunction:
      return ReduceJSCallFunction(node);
    default:
      break;
  }
  return NoChange();
}


// ES6 section 22.1.1 The Array Constructor
Reduction JSCallReducer::ReduceArrayConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  Node* target = NodeProperties::GetValueInput(node, 0);
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());

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


// ES6 section 20.1.1 The Number Constructor
Reduction JSCallReducer::ReduceNumberConstructor(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());

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
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  Node* target = NodeProperties::GetValueInput(node, 0);
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
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
      if (edge.from()->opcode() == IrOpcode::kStateValues) continue;
      if (!NodeProperties::IsValueEdge(edge)) continue;
      if (edge.from() == node) continue;
      return NoChange();
    }
    // Get to the actual frame state from which to extract the arguments;
    // we can only optimize this in case the {node} was already inlined into
    // some other function (and same for the {arg_array}).
    CreateArgumentsType type = CreateArgumentsTypeOf(arg_array->op());
    Node* frame_state = NodeProperties::GetFrameStateInput(arg_array);
    Node* outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
    if (outer_state->opcode() != IrOpcode::kFrameState) return NoChange();
    FrameStateInfo outer_info = OpParameter<FrameStateInfo>(outer_state);
    if (outer_info.type() == FrameStateType::kArgumentsAdaptor) {
      // Need to take the parameters from the arguments adaptor.
      frame_state = outer_state;
    }
    FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);
    int start_index = 0;
    if (type == CreateArgumentsType::kMappedArguments) {
      // Mapped arguments (sloppy mode) cannot be handled if they are aliased.
      Handle<SharedFunctionInfo> shared;
      if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
      if (shared->internal_formal_parameter_count() != 0) return NoChange();
    } else if (type == CreateArgumentsType::kRestParameter) {
      Handle<SharedFunctionInfo> shared;
      if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
      start_index = shared->internal_formal_parameter_count();
    }
    // Remove the argArray input from the {node}.
    node->RemoveInput(static_cast<int>(--arity));
    // Add the actual parameters to the {node}, skipping the receiver.
    Node* const parameters = frame_state->InputAt(kFrameStateParametersInput);
    for (int i = start_index + 1; i < state_info.parameter_count(); ++i) {
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
  // Change {node} to the new {JSCallFunction} operator.
  NodeProperties::ChangeOp(
      node, javascript()->CallFunction(arity, p.frequency(), VectorSlotPair(),
                                       convert_mode, p.tail_call_mode()));
  // Change context of {node} to the Function.prototype.apply context,
  // to ensure any exception is thrown in the correct context.
  NodeProperties::ReplaceContextInput(
      node, jsgraph()->HeapConstant(handle(apply->context(), isolate())));
  // Try to further reduce the JSCallFunction {node}.
  Reduction const reduction = ReduceJSCallFunction(node);
  return reduction.Changed() ? reduction : Changed(node);
}


// ES6 section 19.2.3.3 Function.prototype.call (thisArg, ...args)
Reduction JSCallReducer::ReduceFunctionPrototypeCall(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
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
      node, javascript()->CallFunction(arity, p.frequency(), VectorSlotPair(),
                                       convert_mode, p.tail_call_mode()));
  // Try to further reduce the JSCallFunction {node}.
  Reduction const reduction = ReduceJSCallFunction(node);
  return reduction.Changed() ? reduction : Changed(node);
}

// ES6 section 19.2.3.6 Function.prototype [ @@hasInstance ] (V)
Reduction JSCallReducer::ReduceFunctionPrototypeHasInstance(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
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

namespace {

// TODO(turbofan): Shall we move this to the NodeProperties? Or some (untyped)
// alias analyzer?
bool IsSame(Node* a, Node* b) {
  if (a == b) {
    return true;
  } else if (a->opcode() == IrOpcode::kCheckHeapObject) {
    return IsSame(a->InputAt(0), b);
  } else if (b->opcode() == IrOpcode::kCheckHeapObject) {
    return IsSame(a, b->InputAt(0));
  }
  return false;
}

// TODO(turbofan): Share with similar functionality in JSInliningHeuristic
// and JSNativeContextSpecialization, i.e. move to NodeProperties helper?!
MaybeHandle<Map> InferReceiverMap(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  // Check if the {node} is dominated by a CheckMaps with a single map
  // for the {receiver}, and if so use that map for the lowering below.
  for (Node* dominator = effect;;) {
    if (dominator->opcode() == IrOpcode::kCheckMaps &&
        IsSame(dominator->InputAt(0), receiver)) {
      if (dominator->op()->ValueInputCount() == 2) {
        HeapObjectMatcher m(dominator->InputAt(1));
        if (m.HasValue()) return Handle<Map>::cast(m.Value());
      }
      return MaybeHandle<Map>();
    }
    if (dominator->op()->EffectInputCount() != 1) {
      // Didn't find any appropriate CheckMaps node.
      return MaybeHandle<Map>();
    }
    dominator = NodeProperties::GetEffectInput(dominator);
  }
}

bool CanInlineApiCall(Isolate* isolate, Node* node,
                      Handle<FunctionTemplateInfo> function_template_info) {
  DCHECK(node->opcode() == IrOpcode::kJSCallFunction);
  if (V8_UNLIKELY(FLAG_runtime_stats)) return false;
  if (function_template_info->call_code()->IsUndefined(isolate)) {
    return false;
  }
  CallFunctionParameters const& params = CallFunctionParametersOf(node->op());
  // CallApiCallbackStub expects the target in a register, so we count it out,
  // and counts the receiver as an implicit argument, so we count the receiver
  // out too.
  int const argc = static_cast<int>(params.arity()) - 2;
  if (argc > CallApiCallbackStub::kArgMax || !params.feedback().IsValid()) {
    return false;
  }
  HeapObjectMatcher receiver(NodeProperties::GetValueInput(node, 1));
  if (!receiver.HasValue()) {
    return false;
  }
  return receiver.Value()->IsUndefined(isolate) ||
         (receiver.Value()->map()->IsJSObjectMap() &&
          !receiver.Value()->map()->is_access_check_needed());
}

}  // namespace

JSCallReducer::HolderLookup JSCallReducer::LookupHolder(
    Handle<JSObject> object,
    Handle<FunctionTemplateInfo> function_template_info,
    Handle<JSObject>* holder) {
  DCHECK(object->map()->IsJSObjectMap());
  Handle<Map> object_map(object->map());
  Handle<FunctionTemplateInfo> expected_receiver_type;
  if (!function_template_info->signature()->IsUndefined(isolate())) {
    expected_receiver_type =
        handle(FunctionTemplateInfo::cast(function_template_info->signature()));
  }
  if (expected_receiver_type.is_null() ||
      expected_receiver_type->IsTemplateFor(*object_map)) {
    *holder = Handle<JSObject>::null();
    return kHolderIsReceiver;
  }
  while (object_map->has_hidden_prototype()) {
    Handle<JSObject> prototype(JSObject::cast(object_map->prototype()));
    object_map = handle(prototype->map());
    if (expected_receiver_type->IsTemplateFor(*object_map)) {
      *holder = prototype;
      return kHolderFound;
    }
  }
  return kHolderNotFound;
}

// ES6 section B.2.2.1.1 get Object.prototype.__proto__
Reduction JSCallReducer::ReduceObjectPrototypeGetProto(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());

  // Try to determine the {receiver} map.
  Handle<Map> receiver_map;
  if (InferReceiverMap(node).ToHandle(&receiver_map)) {
    // Check if we can constant-fold the {receiver} map.
    if (!receiver_map->IsJSProxyMap() &&
        !receiver_map->has_hidden_prototype() &&
        !receiver_map->is_access_check_needed()) {
      Handle<Object> receiver_prototype(receiver_map->prototype(), isolate());
      Node* value = jsgraph()->Constant(receiver_prototype);
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }

  return NoChange();
}

Reduction JSCallReducer::ReduceCallApiFunction(
    Node* node, Node* target,
    Handle<FunctionTemplateInfo> function_template_info) {
  Isolate* isolate = this->isolate();
  CHECK(!isolate->serializer_enabled());
  HeapObjectMatcher m(target);
  DCHECK(m.HasValue() && m.Value()->IsJSFunction());
  if (!CanInlineApiCall(isolate, node, function_template_info)) {
    return NoChange();
  }
  Handle<CallHandlerInfo> call_handler_info(
      handle(CallHandlerInfo::cast(function_template_info->call_code())));
  Handle<Object> data(call_handler_info->data(), isolate);

  Node* receiver_node = NodeProperties::GetValueInput(node, 1);
  CallFunctionParameters const& params = CallFunctionParametersOf(node->op());

  Handle<HeapObject> receiver = HeapObjectMatcher(receiver_node).Value();
  bool const receiver_is_undefined = receiver->IsUndefined(isolate);
  if (receiver_is_undefined) {
    receiver = handle(Handle<JSFunction>::cast(m.Value())->global_proxy());
  } else {
    DCHECK(receiver->map()->IsJSObjectMap() &&
           !receiver->map()->is_access_check_needed());
  }

  Handle<JSObject> holder;
  HolderLookup lookup = LookupHolder(Handle<JSObject>::cast(receiver),
                                     function_template_info, &holder);
  if (lookup == kHolderNotFound) return NoChange();
  if (receiver_is_undefined) {
    receiver_node = jsgraph()->HeapConstant(receiver);
    NodeProperties::ReplaceValueInput(node, receiver_node, 1);
  }
  Node* holder_node =
      lookup == kHolderFound ? jsgraph()->HeapConstant(holder) : receiver_node;

  Zone* zone = graph()->zone();
  // Same as CanInlineApiCall: exclude the target (which goes in a register) and
  // the receiver (which is implicitly counted by CallApiCallbackStub) from the
  // arguments count.
  int const argc = static_cast<int>(params.arity() - 2);
  CallApiCallbackStub stub(isolate, argc, data->IsUndefined(isolate), false);
  CallInterfaceDescriptor cid = stub.GetCallInterfaceDescriptor();
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate, zone, cid,
      cid.GetStackParameterCount() + argc + 1 /* implicit receiver */,
      CallDescriptor::kNeedsFrameState, Operator::kNoProperties,
      MachineType::AnyTagged(), 1);
  ApiFunction api_function(v8::ToCData<Address>(call_handler_info->callback()));
  ExternalReference function_reference(
      &api_function, ExternalReference::DIRECT_API_CALL, isolate);

  // CallApiCallbackStub's register arguments: code, target, call data, holder,
  // function address.
  node->InsertInput(zone, 0, jsgraph()->HeapConstant(stub.GetCode()));
  node->InsertInput(zone, 2, jsgraph()->Constant(data));
  node->InsertInput(zone, 3, holder_node);
  node->InsertInput(zone, 4, jsgraph()->ExternalConstant(function_reference));
  NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  return Changed(node);
}

Reduction JSCallReducer::ReduceJSCallFunction(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* control = NodeProperties::GetControlInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);

  // Try to specialize JSCallFunction {node}s with constant {target}s.
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

      // Check for known builtin functions.
      switch (shared->code()->builtin_index()) {
        case Builtins::kFunctionPrototypeApply:
          return ReduceFunctionPrototypeApply(node);
        case Builtins::kFunctionPrototypeCall:
          return ReduceFunctionPrototypeCall(node);
        case Builtins::kFunctionPrototypeHasInstance:
          return ReduceFunctionPrototypeHasInstance(node);
        case Builtins::kNumberConstructor:
          return ReduceNumberConstructor(node);
        case Builtins::kObjectPrototypeGetProto:
          return ReduceObjectPrototypeGetProto(node);
        default:
          break;
      }

      // Check for the Array constructor.
      if (*function == function->native_context()->array_function()) {
        return ReduceArrayConstructor(node);
      }

      if (shared->IsApiFunction()) {
        return ReduceCallApiFunction(
            node, target,
            handle(FunctionTemplateInfo::cast(shared->function_data())));
      }
    } else if (m.Value()->IsJSBoundFunction()) {
      Handle<JSBoundFunction> function =
          Handle<JSBoundFunction>::cast(m.Value());
      Handle<JSReceiver> bound_target_function(
          function->bound_target_function(), isolate());
      Handle<Object> bound_this(function->bound_this(), isolate());
      Handle<FixedArray> bound_arguments(function->bound_arguments(),
                                         isolate());
      CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
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
      NodeProperties::ChangeOp(node, javascript()->CallFunction(
                                         arity, p.frequency(), VectorSlotPair(),
                                         convert_mode, p.tail_call_mode()));
      // Try to further reduce the JSCallFunction {node}.
      Reduction const reduction = ReduceJSCallFunction(node);
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
    Callable callable =
        CodeFactory::CallICInOptimizedCode(isolate(), p.convert_mode());
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

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

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
    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsJSFunction()) {
      Node* target_function =
          jsgraph()->Constant(handle(cell->value(), isolate()));

      // Check that the {target} is still the {target_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     target_function);
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);

      // Specialize the JSCallFunction node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceEffectInput(node, effect);

      // Try to further reduce the JSCallFunction {node}.
      Reduction const reduction = ReduceJSCallFunction(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }
  return NoChange();
}


Reduction JSCallReducer::ReduceJSCallConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallConstruct, node->opcode());
  CallConstructParameters const& p = CallConstructParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Try to specialize JSCallConstruct {node}s with constant {target}s.
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

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

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
    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsJSFunction()) {
      Node* target_function =
          jsgraph()->Constant(handle(cell->value(), isolate()));

      // Check that the {target} is still the {target_function}.
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), target,
                                     target_function);
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);

      // Specialize the JSCallConstruct node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceEffectInput(node, effect);
      if (target == new_target) {
        NodeProperties::ReplaceValueInput(node, target_function, arity + 1);
      }

      // Try to further reduce the JSCallConstruct {node}.
      Reduction const reduction = ReduceJSCallConstruct(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  }

  return NoChange();
}

Graph* JSCallReducer::graph() const { return jsgraph()->graph(); }

Isolate* JSCallReducer::isolate() const { return jsgraph()->isolate(); }

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
