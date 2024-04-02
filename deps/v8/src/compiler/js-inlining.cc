// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining.h"

#include "src/ast/ast.h"
#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/parsing/parse-info.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/compiler/wasm-compiler.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {
namespace compiler {

namespace {
// This is just to avoid some corner cases, especially since we allow recursive
// inlining.
static const int kMaxDepthForInlining = 50;
}  // namespace

#define TRACE(x)                     \
  do {                               \
    if (FLAG_trace_turbo_inlining) { \
      StdoutStream() << x << "\n";   \
    }                                \
  } while (false)

// Provides convenience accessors for the common layout of nodes having either
// the {JSCall} or the {JSConstruct} operator.
class JSCallAccessor {
 public:
  explicit JSCallAccessor(Node* call) : call_(call) {
    DCHECK(call->opcode() == IrOpcode::kJSCall ||
           call->opcode() == IrOpcode::kJSConstruct);
  }

  Node* target() const {
    return call_->InputAt(JSCallOrConstructNode::TargetIndex());
  }

  Node* receiver() const {
    return JSCallNode{call_}.receiver();
  }

  Node* new_target() const { return JSConstructNode{call_}.new_target(); }

  FrameState frame_state() const {
    return FrameState{NodeProperties::GetFrameStateInput(call_)};
  }

  int argument_count() const {
    return (call_->opcode() == IrOpcode::kJSCall)
               ? JSCallNode{call_}.ArgumentCount()
               : JSConstructNode{call_}.ArgumentCount();
  }

  CallFrequency const& frequency() const {
    return (call_->opcode() == IrOpcode::kJSCall)
               ? JSCallNode{call_}.Parameters().frequency()
               : JSConstructNode{call_}.Parameters().frequency();
  }

 private:
  Node* call_;
};

#if V8_ENABLE_WEBASSEMBLY
Reduction JSInliner::InlineJSWasmCall(Node* call, Node* new_target,
                                      Node* context, Node* frame_state,
                                      StartNode start, Node* end,
                                      Node* exception_target,
                                      const NodeVector& uncaught_subcalls) {
  JSWasmCallNode n(call);
  return InlineCall(
      call, new_target, context, frame_state, start, end, exception_target,
      uncaught_subcalls,
      static_cast<int>(n.Parameters().signature()->parameter_count()));
}
#endif  // V8_ENABLE_WEBASSEMBLY

Reduction JSInliner::InlineCall(Node* call, Node* new_target, Node* context,
                                Node* frame_state, StartNode start, Node* end,
                                Node* exception_target,
                                const NodeVector& uncaught_subcalls,
                                int argument_count) {
  DCHECK_IMPLIES(IrOpcode::IsInlineeOpcode(call->opcode()),
                 argument_count == JSCallAccessor(call).argument_count());

  // The scheduler is smart enough to place our code; we just ensure {control}
  // becomes the control input of the start of the inlinee, and {effect} becomes
  // the effect input of the start of the inlinee.
  Node* control = NodeProperties::GetControlInput(call);
  Node* effect = NodeProperties::GetEffectInput(call);

  int const inlinee_new_target_index = start.NewTargetOutputIndex();
  int const inlinee_arity_index = start.ArgCountOutputIndex();
  int const inlinee_context_index = start.ContextOutputIndex();

  // {inliner_inputs} counts the target, receiver/new_target, and arguments; but
  // not feedback vector, context, effect or control.
  const int inliner_inputs = argument_count +
                             JSCallOrConstructNode::kExtraInputCount -
                             JSCallOrConstructNode::kFeedbackVectorInputCount;
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
          Replace(use, jsgraph()->Constant(argument_count));
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
      TRACE("Inlinee contains " << subcall_count
                                << " calls without local exception handler; "
                                << "linking to surrounding exception handler.");
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

FrameState JSInliner::CreateArtificialFrameState(
    Node* node, FrameState outer_frame_state, int parameter_count,
    BytecodeOffset bailout_id, FrameStateType frame_state_type,
    SharedFunctionInfoRef shared, Node* context) {
  const int parameter_count_with_receiver =
      parameter_count + JSCallOrConstructNode::kReceiverOrNewTargetInputCount;
  const FrameStateFunctionInfo* state_info =
      common()->CreateFrameStateFunctionInfo(
          frame_state_type, parameter_count_with_receiver, 0, shared.object());

  const Operator* op = common()->FrameState(
      bailout_id, OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = common()->StateValues(0, SparseInputMask::Dense());
  Node* node0 = graph()->NewNode(op0);

  NodeVector params(local_zone_);
  params.push_back(
      node->InputAt(JSCallOrConstructNode::ReceiverOrNewTargetIndex()));
  for (int i = 0; i < parameter_count; i++) {
    params.push_back(node->InputAt(JSCallOrConstructNode::ArgumentIndex(i)));
  }
  const Operator* op_param = common()->StateValues(
      static_cast<int>(params.size()), SparseInputMask::Dense());
  Node* params_node = graph()->NewNode(
      op_param, static_cast<int>(params.size()), &params.front());
  if (context == nullptr) context = jsgraph()->UndefinedConstant();
  return FrameState{graph()->NewNode(
      op, params_node, node0, node0, context,
      node->InputAt(JSCallOrConstructNode::TargetIndex()), outer_frame_state)};
}

namespace {

bool NeedsImplicitReceiver(SharedFunctionInfoRef shared_info) {
  DisallowGarbageCollection no_gc;
  return !shared_info.construct_as_builtin() &&
         !IsDerivedConstructor(shared_info.kind());
}

}  // namespace

// Determines whether the call target of the given call {node} is statically
// known and can be used as an inlining candidate. The {SharedFunctionInfo} of
// the call target is provided (the exact closure might be unknown).
base::Optional<SharedFunctionInfoRef> JSInliner::DetermineCallTarget(
    Node* node) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  Node* target = node->InputAt(JSCallOrConstructNode::TargetIndex());
  HeapObjectMatcher match(target);

  // This reducer can handle both normal function calls as well a constructor
  // calls whenever the target is a constant function object, as follows:
  //  - JSCall(target:constant, receiver, args..., vector)
  //  - JSConstruct(target:constant, new.target, args..., vector)
  if (match.HasResolvedValue() && match.Ref(broker()).IsJSFunction()) {
    JSFunctionRef function = match.Ref(broker()).AsJSFunction();

    // The function might have not been called yet.
    if (!function.feedback_vector(broker()->dependencies()).has_value()) {
      return base::nullopt;
    }

    // Disallow cross native-context inlining for now. This means that all parts
    // of the resulting code will operate on the same global object. This also
    // prevents cross context leaks, where we could inline functions from a
    // different context and hold on to that context (and closure) from the code
    // object.
    // TODO(turbofan): We might want to revisit this restriction later when we
    // have a need for this, and we know how to model different native contexts
    // in the same graph in a compositional way.
    if (!function.native_context().equals(broker()->target_native_context())) {
      return base::nullopt;
    }

    return function.shared();
  }

  // This reducer can also handle calls where the target is statically known to
  // be the result of a closure instantiation operation, as follows:
  //  - JSCall(JSCreateClosure[shared](context), receiver, args..., vector)
  //  - JSConstruct(JSCreateClosure[shared](context),
  //                new.target, args..., vector)
  if (match.IsJSCreateClosure()) {
    JSCreateClosureNode n(target);
    FeedbackCellRef cell = n.GetFeedbackCellRefChecked(broker());
    return cell.shared_function_info();
  } else if (match.IsCheckClosure()) {
    FeedbackCellRef cell = MakeRef(broker(), FeedbackCellOf(match.op()));
    return cell.shared_function_info();
  }

  return base::nullopt;
}

// Determines statically known information about the call target (assuming that
// the call target is known according to {DetermineCallTarget} above). The
// following static information is provided:
//  - context         : The context (as SSA value) bound by the call target.
//  - feedback_vector : The target is guaranteed to use this feedback vector.
FeedbackCellRef JSInliner::DetermineCallContext(Node* node,
                                                Node** context_out) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  Node* target = node->InputAt(JSCallOrConstructNode::TargetIndex());
  HeapObjectMatcher match(target);

  if (match.HasResolvedValue() && match.Ref(broker()).IsJSFunction()) {
    JSFunctionRef function = match.Ref(broker()).AsJSFunction();
    // This was already ensured by DetermineCallTarget
    CHECK(function.feedback_vector(broker()->dependencies()).has_value());

    // The inlinee specializes to the context from the JSFunction object.
    *context_out = jsgraph()->Constant(function.context());
    return function.raw_feedback_cell(broker()->dependencies());
  }

  if (match.IsJSCreateClosure()) {
    // Load the feedback vector of the target by looking up its vector cell at
    // the instantiation site (we only decide to inline if it's populated).
    JSCreateClosureNode n(target);
    FeedbackCellRef cell = n.GetFeedbackCellRefChecked(broker());

    // The inlinee uses the locally provided context at instantiation.
    *context_out = NodeProperties::GetContextInput(match.node());
    return cell;
  } else if (match.IsCheckClosure()) {
    FeedbackCellRef cell = MakeRef(broker(), FeedbackCellOf(match.op()));

    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    *context_out = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSFunctionContext()),
        match.node(), effect, control);
    NodeProperties::ReplaceEffectInput(node, effect);

    return cell;
  }

  // Must succeed.
  UNREACHABLE();
}

#if V8_ENABLE_WEBASSEMBLY
Reduction JSInliner::ReduceJSWasmCall(Node* node) {
  // Create the subgraph for the inlinee.
  Node* start_node;
  Node* end;
  size_t subgraph_min_node_id;
  {
    Graph::SubgraphScope scope(graph());

    graph()->SetEnd(nullptr);

    JSWasmCallNode n(node);
    const JSWasmCallParameters& wasm_call_params = n.Parameters();

    // Create a nested frame state inside the frame state attached to the
    // call; this will ensure that lazy deoptimizations at this point will
    // still return the result of the Wasm function call.
    Node* continuation_frame_state =
        CreateJSWasmCallBuiltinContinuationFrameState(
            jsgraph(), n.context(), n.frame_state(),
            wasm_call_params.signature());
    JSWasmCallData js_wasm_call_data(wasm_call_params.signature());

    // All the nodes inserted by the inlined subgraph will have
    // id >= subgraph_min_node_id. We use this later to avoid wire nodes that
    // are not inserted by the inlinee but were already part of the graph to the
    // surrounding exception handler, if present.
    subgraph_min_node_id = graph()->NodeCount();

    BuildInlinedJSToWasmWrapper(
        graph()->zone(), jsgraph(), wasm_call_params.signature(),
        wasm_call_params.module(), isolate(), source_positions_,
        StubCallMode::kCallBuiltinPointer, wasm::WasmFeatures::FromFlags(),
        &js_wasm_call_data, continuation_frame_state);

    // Extract the inlinee start/end nodes.
    start_node = graph()->start();
    end = graph()->end();
  }
  StartNode start{start_node};

  Node* exception_target = nullptr;
  NodeProperties::IsExceptionalCall(node, &exception_target);

  // If we are inlining into a surrounding exception handler, we collect all
  // potentially throwing nodes within the inlinee that are not handled locally
  // by the inlinee itself. They are later wired into the surrounding handler.
  NodeVector uncaught_subcalls(local_zone_);
  if (exception_target != nullptr) {
    // Find all uncaught 'calls' in the inlinee.
    AllNodes inlined_nodes(local_zone_, end, graph());
    for (Node* subnode : inlined_nodes.reachable) {
      // Ignore nodes that are not part of the inlinee.
      if (subnode->id() < subgraph_min_node_id) continue;

      // Every possibly throwing node should get {IfSuccess} and {IfException}
      // projections, unless there already is local exception handling.
      if (subnode->op()->HasProperty(Operator::kNoThrow)) continue;
      if (!NodeProperties::IsExceptionalCall(subnode)) {
        DCHECK_EQ(2, subnode->op()->ControlOutputCount());
        uncaught_subcalls.push_back(subnode);
      }
    }
  }

  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* new_target = jsgraph()->UndefinedConstant();

  return InlineJSWasmCall(node, new_target, context, frame_state, start, end,
                          exception_target, uncaught_subcalls);
}
#endif  // V8_ENABLE_WEBASSEMBLY

Reduction JSInliner::ReduceJSCall(Node* node) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
#if V8_ENABLE_WEBASSEMBLY
  DCHECK_NE(node->opcode(), IrOpcode::kJSWasmCall);
#endif  // V8_ENABLE_WEBASSEMBLY
  JSCallAccessor call(node);

  // Determine the call target.
  base::Optional<SharedFunctionInfoRef> shared_info(DetermineCallTarget(node));
  if (!shared_info.has_value()) return NoChange();

  SharedFunctionInfoRef outer_shared_info =
      MakeRef(broker(), info_->shared_info());

  SharedFunctionInfo::Inlineability inlineability =
      shared_info->GetInlineability();
  if (inlineability != SharedFunctionInfo::kIsInlineable) {
    // The function is no longer inlineable. The only way this can happen is if
    // the function had its optimization disabled in the meantime, e.g. because
    // another optimization job failed too often.
    CHECK_EQ(inlineability, SharedFunctionInfo::kHasOptimizationDisabled);
    TRACE("Not inlining " << *shared_info << " into " << outer_shared_info
                          << " because it had its optimization disabled.");
    return NoChange();
  }
  // NOTE: Even though we bailout in the kHasOptimizationDisabled case above, we
  // won't notice if the function's optimization is disabled after this point.

  // Constructor must be constructable.
  if (node->opcode() == IrOpcode::kJSConstruct &&
      !IsConstructable(shared_info->kind())) {
    TRACE("Not inlining " << *shared_info << " into " << outer_shared_info
                          << " because constructor is not constructable.");
    return NoChange();
  }

  // Class constructors are callable, but [[Call]] will raise an exception.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
  if (node->opcode() == IrOpcode::kJSCall &&
      IsClassConstructor(shared_info->kind())) {
    TRACE("Not inlining " << *shared_info << " into " << outer_shared_info
                          << " because callee is a class constructor.");
    return NoChange();
  }

  // To ensure inlining always terminates, we have an upper limit on inlining
  // the nested calls.
  int nesting_level = 0;
  for (Node* frame_state = call.frame_state();
       frame_state->opcode() == IrOpcode::kFrameState;
       frame_state = FrameState{frame_state}.outer_frame_state()) {
    nesting_level++;
    if (nesting_level > kMaxDepthForInlining) {
      TRACE("Not inlining "
            << *shared_info << " into " << outer_shared_info
            << " because call has exceeded the maximum depth for function "
               "inlining.");
      return NoChange();
    }
  }

  Node* exception_target = nullptr;
  NodeProperties::IsExceptionalCall(node, &exception_target);

  // JSInliningHeuristic has already filtered candidates without a BytecodeArray
  // based on SharedFunctionInfoRef::GetInlineability. For the inlineable ones
  // (kIsInlineable), the broker holds a reference to the bytecode array, which
  // prevents it from getting flushed.  Therefore, the following check should
  // always hold true.
  CHECK(shared_info->is_compiled());

  if (info_->source_positions() &&
      !shared_info->object()->AreSourcePositionsAvailable(
          broker()->local_isolate_or_isolate())) {
    // This case is expected to be very rare, since we generate source
    // positions for all functions when debugging or profiling are turned
    // on (see Isolate::NeedsDetailedOptimizedCodeLineInfo). Source
    // positions should only be missing here if there is a race between 1)
    // enabling/disabling the debugger/profiler, and 2) this compile job.
    // In that case, we simply don't inline.
    TRACE("Not inlining " << *shared_info << " into " << outer_shared_info
                          << " because source positions are missing.");
    return NoChange();
  }

  // Determine the target's feedback vector and its context.
  Node* context;
  FeedbackCellRef feedback_cell = DetermineCallContext(node, &context);

  TRACE("Inlining " << *shared_info << " into " << outer_shared_info
                    << ((exception_target != nullptr) ? " (inside try-block)"
                                                      : ""));
  // ----------------------------------------------------------------
  // After this point, we've made a decision to inline this function.
  // We shall not bailout from inlining if we got here.

  BytecodeArrayRef bytecode_array = shared_info->GetBytecodeArray();

  // Remember that we inlined this function.
  int inlining_id =
      info_->AddInlinedFunction(shared_info->object(), bytecode_array.object(),
                                source_positions_->GetSourcePosition(node));

  // Create the subgraph for the inlinee.
  Node* start_node;
  Node* end;
  {
    // Run the BytecodeGraphBuilder to create the subgraph.
    Graph::SubgraphScope scope(graph());
    BytecodeGraphBuilderFlags flags(
        BytecodeGraphBuilderFlag::kSkipFirstStackAndTierupCheck);
    if (info_->analyze_environment_liveness()) {
      flags |= BytecodeGraphBuilderFlag::kAnalyzeEnvironmentLiveness;
    }
    if (info_->bailout_on_uninitialized()) {
      flags |= BytecodeGraphBuilderFlag::kBailoutOnUninitialized;
    }
    {
      CallFrequency frequency = call.frequency();
      BuildGraphFromBytecode(broker(), zone(), *shared_info, feedback_cell,
                             BytecodeOffset::None(), jsgraph(), frequency,
                             source_positions_, inlining_id, info_->code_kind(),
                             flags, &info_->tick_counter());
    }

    // Extract the inlinee start/end nodes.
    start_node = graph()->start();
    end = graph()->end();
  }
  StartNode start{start_node};

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

  FrameState frame_state = call.frame_state();
  Node* new_target = jsgraph()->UndefinedConstant();

  // Inline {JSConstruct} requires some additional magic.
  if (node->opcode() == IrOpcode::kJSConstruct) {
    STATIC_ASSERT(JSCallOrConstructNode::kHaveIdenticalLayouts);
    JSConstructNode n(node);

    new_target = n.new_target();

    // Insert nodes around the call that model the behavior required for a
    // constructor dispatch (allocate implicit receiver and check return value).
    // This models the behavior usually accomplished by our {JSConstructStub}.
    // Note that the context has to be the callers context (input to call node).
    // Also note that by splitting off the {JSCreate} piece of the constructor
    // call, we create an observable deoptimization point after the receiver
    // instantiation but before the invocation (i.e. inside {JSConstructStub}
    // where execution continues at {construct_stub_create_deopt_pc_offset}).
    Node* receiver = jsgraph()->TheHoleConstant();  // Implicit receiver.
    Node* caller_context = NodeProperties::GetContextInput(node);
    if (NeedsImplicitReceiver(*shared_info)) {
      Effect effect = n.effect();
      Control control = n.control();
      Node* frame_state_inside = CreateArtificialFrameState(
          node, frame_state, n.ArgumentCount(),
          BytecodeOffset::ConstructStubCreate(), FrameStateType::kConstructStub,
          *shared_info, caller_context);
      Node* create =
          graph()->NewNode(javascript()->Create(), call.target(), new_target,
                           caller_context, frame_state_inside, effect, control);
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
      branch_is_receiver_false = graph()->NewNode(
          javascript()->CallRuntime(
              Runtime::kThrowConstructorReturnedNonObject),
          caller_context, NodeProperties::GetFrameStateInput(node), node,
          branch_is_receiver_false);
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
    node->ReplaceInput(JSCallNode::ReceiverIndex(), receiver);
    // Insert a construct stub frame into the chain of frame states. This will
    // reconstruct the proper frame when deoptimizing within the constructor.
    frame_state = CreateArtificialFrameState(
        node, frame_state, n.ArgumentCount(),
        BytecodeOffset::ConstructStubInvoke(), FrameStateType::kConstructStub,
        *shared_info, caller_context);
  }

  // Insert a JSConvertReceiver node for sloppy callees. Note that the context
  // passed into this node has to be the callees context (loaded above).
  if (node->opcode() == IrOpcode::kJSCall &&
      is_sloppy(shared_info->language_mode()) && !shared_info->native()) {
    Effect effect{NodeProperties::GetEffectInput(node)};
    if (NodeProperties::CanBePrimitive(broker(), call.receiver(), effect)) {
      CallParameters const& p = CallParametersOf(node->op());
      Node* global_proxy = jsgraph()->Constant(
          broker()->target_native_context().global_proxy_object());
      Node* receiver = effect =
          graph()->NewNode(simplified()->ConvertReceiver(p.convert_mode()),
                           call.receiver(), global_proxy, effect, start);
      NodeProperties::ReplaceValueInput(node, receiver,
                                        JSCallNode::ReceiverIndex());
      NodeProperties::ReplaceEffectInput(node, effect);
    }
  }

  // Insert argument adaptor frame if required. The callees formal parameter
  // count have to match the number of arguments passed
  // to the call.
  int parameter_count =
      shared_info->internal_formal_parameter_count_without_receiver();
  DCHECK_EQ(parameter_count, start.FormalParameterCountWithoutReceiver());
  if (call.argument_count() != parameter_count) {
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.argument_count(), BytecodeOffset::None(),
        FrameStateType::kArgumentsAdaptor, *shared_info);
  }

  return InlineCall(node, new_target, context, frame_state, start, end,
                    exception_target, uncaught_subcalls, call.argument_count());
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
