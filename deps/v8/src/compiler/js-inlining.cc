// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining.h"

#include "src/ast/ast.h"
#include "src/compiler.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/isolate-inl.h"
#include "src/optimized-compilation-info.h"
#include "src/parsing/parse-info.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
// This is just to avoid some corner cases, especially since we allow recursive
// inlining.
static const int kMaxDepthForInlining = 50;
}  // namespace

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_turbo_inlining) PrintF(__VA_ARGS__); \
  } while (false)


// Provides convenience accessors for the common layout of nodes having either
// the {JSCall} or the {JSConstruct} operator.
class JSCallAccessor {
 public:
  explicit JSCallAccessor(Node* call) : call_(call) {
    DCHECK(call->opcode() == IrOpcode::kJSCall ||
           call->opcode() == IrOpcode::kJSConstruct);
  }

  Node* target() {
    // Both, {JSCall} and {JSConstruct}, have same layout here.
    return call_->InputAt(0);
  }

  Node* receiver() {
    DCHECK_EQ(IrOpcode::kJSCall, call_->opcode());
    return call_->InputAt(1);
  }

  Node* new_target() {
    DCHECK_EQ(IrOpcode::kJSConstruct, call_->opcode());
    return call_->InputAt(formal_arguments() + 1);
  }

  Node* frame_state() {
    // Both, {JSCall} and {JSConstruct}, have frame state.
    return NodeProperties::GetFrameStateInput(call_);
  }

  int formal_arguments() {
    // Both, {JSCall} and {JSConstruct}, have two extra inputs:
    //  - JSConstruct: Includes target function and new target.
    //  - JSCall: Includes target function and receiver.
    return call_->op()->ValueInputCount() - 2;
  }

  CallFrequency frequency() const {
    return (call_->opcode() == IrOpcode::kJSCall)
               ? CallParametersOf(call_->op()).frequency()
               : ConstructParametersOf(call_->op()).frequency();
  }

 private:
  Node* call_;
};

Reduction JSInliner::InlineCall(Node* call, Node* new_target, Node* context,
                                Node* frame_state, Node* start, Node* end,
                                Node* exception_target,
                                const NodeVector& uncaught_subcalls) {
  // The scheduler is smart enough to place our code; we just ensure {control}
  // becomes the control input of the start of the inlinee, and {effect} becomes
  // the effect input of the start of the inlinee.
  Node* control = NodeProperties::GetControlInput(call);
  Node* effect = NodeProperties::GetEffectInput(call);

  int const inlinee_new_target_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 3;
  int const inlinee_arity_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 2;
  int const inlinee_context_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 1;

  // {inliner_inputs} counts JSFunction, receiver, arguments, but not
  // new target value, argument count, context, effect or control.
  int inliner_inputs = call->op()->ValueInputCount();
  // Iterate over all uses of the start node.
  for (Edge edge : start->use_edges()) {
    Node* use = edge.from();
    switch (use->opcode()) {
      case IrOpcode::kParameter: {
        int index = 1 + ParameterIndexOf(use->op());
        DCHECK_LE(index, inlinee_context_index);
        if (index < inliner_inputs && index < inlinee_new_target_index) {
          // There is an input from the call, and the index is a value
          // projection but not the context, so rewire the input.
          Replace(use, call->InputAt(index));
        } else if (index == inlinee_new_target_index) {
          // The projection is requesting the new target value.
          Replace(use, new_target);
        } else if (index == inlinee_arity_index) {
          // The projection is requesting the number of arguments.
          Replace(use, jsgraph()->Constant(inliner_inputs - 2));
        } else if (index == inlinee_context_index) {
          // The projection is requesting the inlinee function context.
          Replace(use, context);
        } else {
          // Call has fewer arguments than required, fill with undefined.
          Replace(use, jsgraph()->UndefinedConstant());
        }
        break;
      }
      default:
        if (NodeProperties::IsEffectEdge(edge)) {
          edge.UpdateTo(effect);
        } else if (NodeProperties::IsControlEdge(edge)) {
          edge.UpdateTo(control);
        } else if (NodeProperties::IsFrameStateEdge(edge)) {
          edge.UpdateTo(frame_state);
        } else {
          UNREACHABLE();
        }
        break;
    }
  }

  if (exception_target != nullptr) {
    // Link uncaught calls in the inlinee to {exception_target}
    int subcall_count = static_cast<int>(uncaught_subcalls.size());
    if (subcall_count > 0) {
      TRACE(
          "Inlinee contains %d calls without local exception handler; "
          "linking to surrounding exception handler\n",
          subcall_count);
    }
    NodeVector on_exception_nodes(local_zone_);
    for (Node* subcall : uncaught_subcalls) {
      Node* on_success = graph()->NewNode(common()->IfSuccess(), subcall);
      NodeProperties::ReplaceUses(subcall, subcall, subcall, on_success);
      NodeProperties::ReplaceControlInput(on_success, subcall);
      Node* on_exception =
          graph()->NewNode(common()->IfException(), subcall, subcall);
      on_exception_nodes.push_back(on_exception);
    }

    DCHECK_EQ(subcall_count, static_cast<int>(on_exception_nodes.size()));
    if (subcall_count > 0) {
      Node* control_output =
          graph()->NewNode(common()->Merge(subcall_count), subcall_count,
                           &on_exception_nodes.front());
      NodeVector values_effects(local_zone_);
      values_effects = on_exception_nodes;
      values_effects.push_back(control_output);
      Node* value_output = graph()->NewNode(
          common()->Phi(MachineRepresentation::kTagged, subcall_count),
          subcall_count + 1, &values_effects.front());
      Node* effect_output =
          graph()->NewNode(common()->EffectPhi(subcall_count),
                           subcall_count + 1, &values_effects.front());
      ReplaceWithValue(exception_target, value_output, effect_output,
                       control_output);
    } else {
      ReplaceWithValue(exception_target, exception_target, exception_target,
                       jsgraph()->Dead());
    }
  }

  NodeVector values(local_zone_);
  NodeVector effects(local_zone_);
  NodeVector controls(local_zone_);
  for (Node* const input : end->inputs()) {
    switch (input->opcode()) {
      case IrOpcode::kReturn:
        values.push_back(NodeProperties::GetValueInput(input, 1));
        effects.push_back(NodeProperties::GetEffectInput(input));
        controls.push_back(NodeProperties::GetControlInput(input));
        break;
      case IrOpcode::kDeoptimize:
      case IrOpcode::kTerminate:
      case IrOpcode::kThrow:
        NodeProperties::MergeControlToEnd(graph(), common(), input);
        Revisit(graph()->end());
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
  DCHECK_EQ(values.size(), effects.size());
  DCHECK_EQ(values.size(), controls.size());

  // Depending on whether the inlinee produces a value, we either replace value
  // uses with said value or kill value uses if no value can be returned.
  if (values.size() > 0) {
    int const input_count = static_cast<int>(controls.size());
    Node* control_output = graph()->NewNode(common()->Merge(input_count),
                                            input_count, &controls.front());
    values.push_back(control_output);
    effects.push_back(control_output);
    Node* value_output = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, input_count),
        static_cast<int>(values.size()), &values.front());
    Node* effect_output =
        graph()->NewNode(common()->EffectPhi(input_count),
                         static_cast<int>(effects.size()), &effects.front());
    ReplaceWithValue(call, value_output, effect_output, control_output);
    return Changed(value_output);
  } else {
    ReplaceWithValue(call, jsgraph()->Dead(), jsgraph()->Dead(),
                     jsgraph()->Dead());
    return Changed(call);
  }
}

Node* JSInliner::CreateArtificialFrameState(Node* node, Node* outer_frame_state,
                                            int parameter_count,
                                            BailoutId bailout_id,
                                            FrameStateType frame_state_type,
                                            Handle<SharedFunctionInfo> shared,
                                            Node* context) {
  const FrameStateFunctionInfo* state_info =
      common()->CreateFrameStateFunctionInfo(frame_state_type,
                                             parameter_count + 1, 0, shared);

  const Operator* op = common()->FrameState(
      bailout_id, OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = common()->StateValues(0, SparseInputMask::Dense());
  Node* node0 = graph()->NewNode(op0);
  NodeVector params(local_zone_);
  for (int parameter = 0; parameter < parameter_count + 1; ++parameter) {
    params.push_back(node->InputAt(1 + parameter));
  }
  const Operator* op_param = common()->StateValues(
      static_cast<int>(params.size()), SparseInputMask::Dense());
  Node* params_node = graph()->NewNode(
      op_param, static_cast<int>(params.size()), &params.front());
  if (!context) {
    context = jsgraph()->UndefinedConstant();
  }
  return graph()->NewNode(op, params_node, node0, node0, context,
                          node->InputAt(0), outer_frame_state);
}

namespace {

// TODO(mstarzinger,verwaest): Move this predicate onto SharedFunctionInfo?
bool NeedsImplicitReceiver(Handle<SharedFunctionInfo> shared_info) {
  DisallowHeapAllocation no_gc;
  if (!shared_info->construct_as_builtin()) {
    return !IsDerivedConstructor(shared_info->kind());
  } else {
    return false;
  }
}

}  // namespace

// Determines whether the call target of the given call {node} is statically
// known and can be used as an inlining candidate. The {SharedFunctionInfo} of
// the call target is provided (the exact closure might be unknown).
bool JSInliner::DetermineCallTarget(
    Node* node, Handle<SharedFunctionInfo>& shared_info_out) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  HeapObjectMatcher match(node->InputAt(0));

  // This reducer can handle both normal function calls as well a constructor
  // calls whenever the target is a constant function object, as follows:
  //  - JSCall(target:constant, receiver, args...)
  //  - JSConstruct(target:constant, args..., new.target)
  if (match.HasValue() && match.Value()->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(match.Value());

    // Disallow cross native-context inlining for now. This means that all parts
    // of the resulting code will operate on the same global object. This also
    // prevents cross context leaks, where we could inline functions from a
    // different context and hold on to that context (and closure) from the code
    // object.
    // TODO(turbofan): We might want to revisit this restriction later when we
    // have a need for this, and we know how to model different native contexts
    // in the same graph in a compositional way.
    if (function->context()->native_context() !=
        info_->context()->native_context()) {
      return false;
    }

    shared_info_out = handle(function->shared(), isolate());
    return true;
  }

  // This reducer can also handle calls where the target is statically known to
  // be the result of a closure instantiation operation, as follows:
  //  - JSCall(JSCreateClosure[shared](context), receiver, args...)
  //  - JSConstruct(JSCreateClosure[shared](context), args..., new.target)
  if (match.IsJSCreateClosure()) {
    CreateClosureParameters const& p = CreateClosureParametersOf(match.op());

    // Disallow inlining in case the instantiation site was never run and hence
    // the vector cell does not contain a valid feedback vector for the call
    // target.
    // TODO(turbofan): We might consider to eagerly create the feedback vector
    // in such a case (in {DetermineCallContext} below) eventually.
    Handle<FeedbackCell> cell = p.feedback_cell();
    if (!cell->value()->IsFeedbackVector()) return false;

    shared_info_out = p.shared_info();
    return true;
  }

  return false;
}

// Determines statically known information about the call target (assuming that
// the call target is known according to {DetermineCallTarget} above). The
// following static information is provided:
//  - context         : The context (as SSA value) bound by the call target.
//  - feedback_vector : The target is guaranteed to use this feedback vector.
void JSInliner::DetermineCallContext(
    Node* node, Node*& context_out,
    Handle<FeedbackVector>& feedback_vector_out) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  HeapObjectMatcher match(node->InputAt(0));

  if (match.HasValue() && match.Value()->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(match.Value());

    // If the target function was never invoked, its feedback cell array might
    // not contain a feedback vector. We ensure at this point that it's created.
    JSFunction::EnsureFeedbackVector(function);

    // The inlinee specializes to the context from the JSFunction object.
    context_out = jsgraph()->Constant(handle(function->context(), isolate()));
    feedback_vector_out = handle(function->feedback_vector(), isolate());
    return;
  }

  if (match.IsJSCreateClosure()) {
    CreateClosureParameters const& p = CreateClosureParametersOf(match.op());

    // Load the feedback vector of the target by looking up its vector cell at
    // the instantiation site (we only decide to inline if it's populated).
    Handle<FeedbackCell> cell = p.feedback_cell();
    DCHECK(cell->value()->IsFeedbackVector());

    // The inlinee uses the locally provided context at instantiation.
    context_out = NodeProperties::GetContextInput(match.node());
    feedback_vector_out =
        handle(FeedbackVector::cast(cell->value()), isolate());
    return;
  }

  // Must succeed.
  UNREACHABLE();
}

Reduction JSInliner::Reduce(Node* node) {
  if (!IrOpcode::IsInlineeOpcode(node->opcode())) return NoChange();
  return ReduceJSCall(node);
}

Handle<Context> JSInliner::native_context() const {
  return handle(info_->context()->native_context(), isolate());
}

Reduction JSInliner::ReduceJSCall(Node* node) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  Handle<SharedFunctionInfo> shared_info;
  JSCallAccessor call(node);

  // Determine the call target.
  if (!DetermineCallTarget(node, shared_info)) return NoChange();

  // Function must be inlineable.
  if (!shared_info->IsInlineable()) {
    TRACE("Not inlining %s into %s because callee is not inlineable\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Constructor must be constructable.
  if (node->opcode() == IrOpcode::kJSConstruct &&
      !IsConstructable(shared_info->kind())) {
    TRACE("Not inlining %s into %s because constructor is not constructable.\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Class constructors are callable, but [[Call]] will raise an exception.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
  if (node->opcode() == IrOpcode::kJSCall &&
      IsClassConstructor(shared_info->kind())) {
    TRACE("Not inlining %s into %s because callee is a class constructor.\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Function contains break points.
  if (shared_info->HasBreakInfo()) {
    TRACE("Not inlining %s into %s because callee may contain break points\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // To ensure inlining always terminates, we have an upper limit on inlining
  // the nested calls.
  int nesting_level = 0;
  for (Node* frame_state = call.frame_state();
       frame_state->opcode() == IrOpcode::kFrameState;
       frame_state = frame_state->InputAt(kFrameStateOuterStateInput)) {
    nesting_level++;
    if (nesting_level > kMaxDepthForInlining) {
      TRACE(
          "Not inlining %s into %s because call has exceeded the maximum depth "
          "for function inlining\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
      return NoChange();
    }
  }

  // Calls surrounded by a local try-block are only inlined if the appropriate
  // flag is active. We also discover the {IfException} projection this way.
  Node* exception_target = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &exception_target) &&
      !FLAG_inline_into_try) {
    TRACE(
        "Try block surrounds #%d:%s and --no-inline-into-try active, so not "
        "inlining %s into %s.\n",
        exception_target->id(), exception_target->op()->mnemonic(),
        shared_info->DebugName()->ToCString().get(),
        info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  if (!shared_info->is_compiled() &&
      !Compiler::Compile(shared_info, Compiler::CLEAR_EXCEPTION)) {
    TRACE("Not inlining %s into %s because bytecode generation failed\n",
          shared_info->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // ----------------------------------------------------------------
  // After this point, we've made a decision to inline this function.
  // We shall not bailout from inlining if we got here.

  TRACE("Inlining %s into %s%s\n", shared_info->DebugName()->ToCString().get(),
        info_->shared_info()->DebugName()->ToCString().get(),
        (exception_target != nullptr) ? " (inside try-block)" : "");

  // Determine the targets feedback vector and its context.
  Node* context;
  Handle<FeedbackVector> feedback_vector;
  DetermineCallContext(node, context, feedback_vector);

  // Remember that we inlined this function.
  int inlining_id = info_->AddInlinedFunction(
      shared_info, source_positions_->GetSourcePosition(node));

  // Create the subgraph for the inlinee.
  Node* start;
  Node* end;
  {
    // Run the BytecodeGraphBuilder to create the subgraph.
    Graph::SubgraphScope scope(graph());
    JSTypeHintLowering::Flags flags = JSTypeHintLowering::kNoFlags;
    if (info_->is_bailout_on_uninitialized()) {
      flags |= JSTypeHintLowering::kBailoutOnUninitialized;
    }
    CallFrequency frequency = call.frequency();
    BytecodeGraphBuilder graph_builder(
        zone(), shared_info, feedback_vector, BailoutId::None(), jsgraph(),
        frequency, source_positions_, native_context(), inlining_id,
        flags, false, info_->is_analyze_environment_liveness());
    graph_builder.CreateGraph();

    // Extract the inlinee start/end nodes.
    start = graph()->start();
    end = graph()->end();
  }

  // If we are inlining into a surrounding exception handler, we collect all
  // potentially throwing nodes within the inlinee that are not handled locally
  // by the inlinee itself. They are later wired into the surrounding handler.
  NodeVector uncaught_subcalls(local_zone_);
  if (exception_target != nullptr) {
    // Find all uncaught 'calls' in the inlinee.
    AllNodes inlined_nodes(local_zone_, end, graph());
    for (Node* subnode : inlined_nodes.reachable) {
      // Every possibly throwing node should get {IfSuccess} and {IfException}
      // projections, unless there already is local exception handling.
      if (subnode->op()->HasProperty(Operator::kNoThrow)) continue;
      if (!NodeProperties::IsExceptionalCall(subnode)) {
        DCHECK_EQ(2, subnode->op()->ControlOutputCount());
        uncaught_subcalls.push_back(subnode);
      }
    }
  }

  Node* frame_state = call.frame_state();
  Node* new_target = jsgraph()->UndefinedConstant();

  // Inline {JSConstruct} requires some additional magic.
  if (node->opcode() == IrOpcode::kJSConstruct) {
    // Swizzle the inputs of the {JSConstruct} node to look like inputs to a
    // normal {JSCall} node so that the rest of the inlining machinery
    // behaves as if we were dealing with a regular function invocation.
    new_target = call.new_target();  // Retrieve new target value input.
    node->RemoveInput(call.formal_arguments() + 1);  // Drop new target.
    node->InsertInput(graph()->zone(), 1, new_target);

    // Insert nodes around the call that model the behavior required for a
    // constructor dispatch (allocate implicit receiver and check return value).
    // This models the behavior usually accomplished by our {JSConstructStub}.
    // Note that the context has to be the callers context (input to call node).
    // Also note that by splitting off the {JSCreate} piece of the constructor
    // call, we create an observable deoptimization point after the receiver
    // instantiation but before the invocation (i.e. inside {JSConstructStub}
    // where execution continues at {construct_stub_create_deopt_pc_offset}).
    Node* receiver = jsgraph()->TheHoleConstant();  // Implicit receiver.
    Node* context = NodeProperties::GetContextInput(node);
    if (NeedsImplicitReceiver(shared_info)) {
      Node* effect = NodeProperties::GetEffectInput(node);
      Node* control = NodeProperties::GetControlInput(node);
      Node* frame_state_inside = CreateArtificialFrameState(
          node, frame_state, call.formal_arguments(),
          BailoutId::ConstructStubCreate(), FrameStateType::kConstructStub,
          shared_info, context);
      Node* create =
          graph()->NewNode(javascript()->Create(), call.target(), new_target,
                           context, frame_state_inside, effect, control);
      uncaught_subcalls.push_back(create);  // Adds {IfSuccess} & {IfException}.
      NodeProperties::ReplaceControlInput(node, create);
      NodeProperties::ReplaceEffectInput(node, create);
      // Placeholder to hold {node}'s value dependencies while {node} is
      // replaced.
      Node* dummy = graph()->NewNode(common()->Dead());
      NodeProperties::ReplaceUses(node, dummy, node, node, node);
      Node* result;
      // Insert a check of the return value to determine whether the return
      // value or the implicit receiver should be selected as a result of the
      // call.
      Node* check = graph()->NewNode(simplified()->ObjectIsReceiver(), node);
      result =
          graph()->NewNode(common()->Select(MachineRepresentation::kTagged),
                           check, node, create);
      receiver = create;  // The implicit receiver.
      ReplaceWithValue(dummy, result);
    } else if (IsDerivedConstructor(shared_info->kind())) {
      Node* node_success =
          NodeProperties::FindSuccessfulControlProjection(node);
      Node* is_receiver =
          graph()->NewNode(simplified()->ObjectIsReceiver(), node);
      Node* branch_is_receiver =
          graph()->NewNode(common()->Branch(), is_receiver, node_success);
      Node* branch_is_receiver_true =
          graph()->NewNode(common()->IfTrue(), branch_is_receiver);
      Node* branch_is_receiver_false =
          graph()->NewNode(common()->IfFalse(), branch_is_receiver);
      branch_is_receiver_false =
          graph()->NewNode(javascript()->CallRuntime(
                               Runtime::kThrowConstructorReturnedNonObject),
                           context, NodeProperties::GetFrameStateInput(node),
                           node, branch_is_receiver_false);
      uncaught_subcalls.push_back(branch_is_receiver_false);
      branch_is_receiver_false =
          graph()->NewNode(common()->Throw(), branch_is_receiver_false,
                           branch_is_receiver_false);
      NodeProperties::MergeControlToEnd(graph(), common(),
                                        branch_is_receiver_false);

      ReplaceWithValue(node_success, node_success, node_success,
                       branch_is_receiver_true);
      // Fix input destroyed by the above {ReplaceWithValue} call.
      NodeProperties::ReplaceControlInput(branch_is_receiver, node_success, 0);
    }
    node->ReplaceInput(1, receiver);
    // Insert a construct stub frame into the chain of frame states. This will
    // reconstruct the proper frame when deoptimizing within the constructor.
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.formal_arguments(),
        BailoutId::ConstructStubInvoke(), FrameStateType::kConstructStub,
        shared_info, context);
  }

  // Insert a JSConvertReceiver node for sloppy callees. Note that the context
  // passed into this node has to be the callees context (loaded above).
  if (node->opcode() == IrOpcode::kJSCall &&
      is_sloppy(shared_info->language_mode()) && !shared_info->native()) {
    Node* effect = NodeProperties::GetEffectInput(node);
    if (NodeProperties::CanBePrimitive(isolate(), call.receiver(), effect)) {
      CallParameters const& p = CallParametersOf(node->op());
      Node* global_proxy = jsgraph()->HeapConstant(
          handle(info_->native_context()->global_proxy(), isolate()));
      Node* receiver = effect =
          graph()->NewNode(simplified()->ConvertReceiver(p.convert_mode()),
                           call.receiver(), global_proxy, effect, start);
      NodeProperties::ReplaceValueInput(node, receiver, 1);
      NodeProperties::ReplaceEffectInput(node, effect);
    }
  }

  // Insert argument adaptor frame if required. The callees formal parameter
  // count (i.e. value outputs of start node minus target, receiver, new target,
  // arguments count and context) have to match the number of arguments passed
  // to the call.
  int parameter_count = shared_info->internal_formal_parameter_count();
  DCHECK_EQ(parameter_count, start->op()->ValueOutputCount() - 5);
  if (call.formal_arguments() != parameter_count) {
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.formal_arguments(), BailoutId::None(),
        FrameStateType::kArgumentsAdaptor, shared_info);
  }

  return InlineCall(node, new_target, context, frame_state, start, end,
                    exception_target, uncaught_subcalls);
}

Graph* JSInliner::graph() const { return jsgraph()->graph(); }

JSOperatorBuilder* JSInliner::javascript() const {
  return jsgraph()->javascript();
}

CommonOperatorBuilder* JSInliner::common() const { return jsgraph()->common(); }

SimplifiedOperatorBuilder* JSInliner::simplified() const {
  return jsgraph()->simplified();
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
