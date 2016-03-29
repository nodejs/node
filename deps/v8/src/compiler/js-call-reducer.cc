// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-call-reducer.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/objects-inl.h"
#include "src/type-feedback-vector-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

VectorSlotPair CallCountFeedback(VectorSlotPair p) {
  // Extract call count from {p}.
  if (!p.IsValid()) return VectorSlotPair();
  CallICNexus n(p.vector(), p.slot());
  int const call_count = n.ExtractCallCount();
  if (call_count <= 0) return VectorSlotPair();

  // Create megamorphic CallIC feedback with the given {call_count}.
  StaticFeedbackVectorSpec spec;
  FeedbackVectorSlot slot = spec.AddCallICSlot();
  Handle<TypeFeedbackMetadata> metadata =
      TypeFeedbackMetadata::New(n.GetIsolate(), &spec);
  Handle<TypeFeedbackVector> vector =
      TypeFeedbackVector::New(n.GetIsolate(), metadata);
  CallICNexus nexus(vector, slot);
  nexus.ConfigureMegamorphic(call_count);
  return VectorSlotPair(vector, slot);
}

}  // namespace


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
  NodeProperties::RemoveFrameStateInput(node, 1);
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
  NodeProperties::RemoveFrameStateInput(node, 1);
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
    Node* frame_state = NodeProperties::GetFrameStateInput(arg_array, 0);
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
      node, javascript()->CallFunction(arity, CallCountFeedback(p.feedback()),
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
      node, javascript()->CallFunction(arity, CallCountFeedback(p.feedback()),
                                       convert_mode, p.tail_call_mode()));
  // Try to further reduce the JSCallFunction {node}.
  Reduction const reduction = ReduceJSCallFunction(node);
  return reduction.Changed() ? reduction : Changed(node);
}


Reduction JSCallReducer::ReduceJSCallFunction(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
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
        NodeProperties::RemoveFrameStateInput(node, 0);
        NodeProperties::ReplaceValueInputs(node, target);
        NodeProperties::ChangeOp(
            node, javascript()->CallRuntime(
                      Runtime::kThrowConstructorNonCallableError, 1));
        return Changed(node);
      }

      // Check for known builtin functions.
      if (shared->HasBuiltinFunctionId()) {
        switch (shared->builtin_function_id()) {
          case kFunctionApply:
            return ReduceFunctionPrototypeApply(node);
          case kFunctionCall:
            return ReduceFunctionPrototypeCall(node);
          default:
            break;
        }
      }

      // Check for the Array constructor.
      if (*function == function->native_context()->array_function()) {
        return ReduceArrayConstructor(node);
      }

      // Check for the Number constructor.
      if (*function == function->native_context()->number_function()) {
        return ReduceNumberConstructor(node);
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
          (bound_this->IsNull() || bound_this->IsUndefined())
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
                                         arity, CallCountFeedback(p.feedback()),
                                         convert_mode, p.tail_call_mode()));
      // Try to further reduce the JSCallFunction {node}.
      Reduction const reduction = ReduceJSCallFunction(node);
      return reduction.Changed() ? reduction : Changed(node);
    }

    // Don't mess with other {node}s that have a constant {target}.
    // TODO(bmeurer): Also support proxies here.
    return NoChange();
  }

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // Extract feedback from the {node} using the CallICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
  Handle<Object> feedback(nexus.GetFeedback(), isolate());
  if (feedback->IsAllocationSite()) {
    // Retrieve the Array function from the {node}.
    Node* array_function;
    Handle<Context> native_context;
    if (GetNativeContext(node).ToHandle(&native_context)) {
      array_function = jsgraph()->HeapConstant(
          handle(native_context->array_function(), isolate()));
    } else {
      Node* native_context = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, effect);
      array_function = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::ARRAY_FUNCTION_INDEX, true),
          native_context, native_context, effect);
    }

    // Check that the {target} is still the {array_function}.
    Node* check = effect =
        graph()->NewNode(javascript()->StrictEqual(), target, array_function,
                         context, effect, control);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* deoptimize =
        graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kEager),
                         frame_state, effect, if_false);
    // TODO(bmeurer): This should be on the AdvancedReducer somehow.
    NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
    Revisit(graph()->end());
    control = graph()->NewNode(common()->IfTrue(), branch);

    // Turn the {node} into a {JSCreateArray} call.
    NodeProperties::ReplaceValueInput(node, array_function, 0);
    NodeProperties::ReplaceEffectInput(node, effect);
    NodeProperties::ReplaceControlInput(node, control);
    return ReduceArrayConstructor(node);
  } else if (feedback->IsWeakCell()) {
    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsJSFunction()) {
      Node* target_function =
          jsgraph()->Constant(handle(cell->value(), isolate()));

      // Check that the {target} is still the {target_function}.
      Node* check = effect =
          graph()->NewNode(javascript()->StrictEqual(), target, target_function,
                           context, effect, control);
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* deoptimize =
          graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kEager),
                           frame_state, effect, if_false);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
      Revisit(graph()->end());
      control = graph()->NewNode(common()->IfTrue(), branch);

      // Specialize the JSCallFunction node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceEffectInput(node, effect);
      NodeProperties::ReplaceControlInput(node, control);

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
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Try to specialize JSCallConstruct {node}s with constant {target}s.
  HeapObjectMatcher m(target);
  if (m.HasValue()) {
    if (m.Value()->IsJSFunction()) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());

      // Raise a TypeError if the {target} is not a constructor.
      if (!function->IsConstructor()) {
        // Drop the lazy bailout location and use the eager bailout point for
        // the runtime function (actually as lazy bailout point). It doesn't
        // really matter which bailout location we use since we never really
        // go back after throwing the exception.
        NodeProperties::RemoveFrameStateInput(node, 0);
        NodeProperties::ReplaceValueInputs(node, target);
        NodeProperties::ChangeOp(
            node, javascript()->CallRuntime(Runtime::kThrowCalledNonCallable));
        return Changed(node);
      }

      // Check for the ArrayConstructor.
      if (*function == function->native_context()->array_function()) {
        // Check if we have an allocation site.
        Handle<AllocationSite> site;
        if (p.feedback().IsValid()) {
          Handle<Object> feedback(
              p.feedback().vector()->Get(p.feedback().slot()), isolate());
          if (feedback->IsAllocationSite()) {
            site = Handle<AllocationSite>::cast(feedback);
          }
        }

        // Turn the {node} into a {JSCreateArray} call.
        NodeProperties::RemoveFrameStateInput(node, 1);
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

  // TODO(mvstanton): Use ConstructICNexus here, once available.
  Handle<Object> feedback;
  if (!p.feedback().IsValid()) return NoChange();
  feedback = handle(p.feedback().vector()->Get(p.feedback().slot()), isolate());
  if (feedback->IsAllocationSite()) {
    // The feedback is an AllocationSite, which means we have called the
    // Array function and collected transition (and pretenuring) feedback
    // for the resulting arrays.  This has to be kept in sync with the
    // implementation of the CallConstructStub.
    Handle<AllocationSite> site = Handle<AllocationSite>::cast(feedback);

    // Retrieve the Array function from the {node}.
    Node* array_function;
    Handle<Context> native_context;
    if (GetNativeContext(node).ToHandle(&native_context)) {
      array_function = jsgraph()->HeapConstant(
          handle(native_context->array_function(), isolate()));
    } else {
      Node* native_context = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, effect);
      array_function = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::ARRAY_FUNCTION_INDEX, true),
          native_context, native_context, effect);
    }

    // Check that the {target} is still the {array_function}.
    Node* check = effect =
        graph()->NewNode(javascript()->StrictEqual(), target, array_function,
                         context, effect, control);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* deoptimize =
        graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kEager),
                         frame_state, effect, if_false);
    // TODO(bmeurer): This should be on the AdvancedReducer somehow.
    NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
    Revisit(graph()->end());
    control = graph()->NewNode(common()->IfTrue(), branch);

    // Turn the {node} into a {JSCreateArray} call.
    NodeProperties::ReplaceEffectInput(node, effect);
    NodeProperties::ReplaceControlInput(node, control);
    NodeProperties::RemoveFrameStateInput(node, 1);
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
      Node* check = effect =
          graph()->NewNode(javascript()->StrictEqual(), target, target_function,
                           context, effect, control);
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* deoptimize =
          graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kEager),
                           frame_state, effect, if_false);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
      Revisit(graph()->end());
      control = graph()->NewNode(common()->IfTrue(), branch);

      // Specialize the JSCallConstruct node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceEffectInput(node, effect);
      NodeProperties::ReplaceControlInput(node, control);
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


MaybeHandle<Context> JSCallReducer::GetNativeContext(Node* node) {
  Node* const context = NodeProperties::GetContextInput(node);
  return NodeProperties::GetSpecializationNativeContext(context,
                                                        native_context());
}


Graph* JSCallReducer::graph() const { return jsgraph()->graph(); }


Isolate* JSCallReducer::isolate() const { return jsgraph()->isolate(); }


CommonOperatorBuilder* JSCallReducer::common() const {
  return jsgraph()->common();
}


JSOperatorBuilder* JSCallReducer::javascript() const {
  return jsgraph()->javascript();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
