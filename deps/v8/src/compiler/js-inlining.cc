// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining.h"

#include "src/ast/ast.h"
#include "src/ast/ast-numbering.h"
#include "src/ast/scopes.h"
#include "src/compiler.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/isolate-inl.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_turbo_inlining) PrintF(__VA_ARGS__); \
  } while (false)


// Provides convenience accessors for the common layout of nodes having either
// the {JSCallFunction} or the {JSCallConstruct} operator.
class JSCallAccessor {
 public:
  explicit JSCallAccessor(Node* call) : call_(call) {
    DCHECK(call->opcode() == IrOpcode::kJSCallFunction ||
           call->opcode() == IrOpcode::kJSCallConstruct);
  }

  Node* target() {
    // Both, {JSCallFunction} and {JSCallConstruct}, have same layout here.
    return call_->InputAt(0);
  }

  Node* receiver() {
    DCHECK_EQ(IrOpcode::kJSCallFunction, call_->opcode());
    return call_->InputAt(1);
  }

  Node* new_target() {
    DCHECK_EQ(IrOpcode::kJSCallConstruct, call_->opcode());
    return call_->InputAt(formal_arguments() + 1);
  }

  Node* frame_state_before() {
    return NodeProperties::GetFrameStateInput(call_, 1);
  }

  Node* frame_state_after() {
    // Both, {JSCallFunction} and {JSCallConstruct}, have frame state after.
    return NodeProperties::GetFrameStateInput(call_, 0);
  }

  int formal_arguments() {
    // Both, {JSCallFunction} and {JSCallConstruct}, have two extra inputs:
    //  - JSCallConstruct: Includes target function and new target.
    //  - JSCallFunction: Includes target function and receiver.
    return call_->op()->ValueInputCount() - 2;
  }

 private:
  Node* call_;
};


class CopyVisitor {
 public:
  CopyVisitor(Graph* source_graph, Graph* target_graph, Zone* temp_zone)
      : sentinel_op_(IrOpcode::kDead, Operator::kNoProperties, "Sentinel", 0, 0,
                     0, 0, 0, 0),
        sentinel_(target_graph->NewNode(&sentinel_op_)),
        copies_(source_graph->NodeCount(), sentinel_, temp_zone),
        source_graph_(source_graph),
        target_graph_(target_graph),
        temp_zone_(temp_zone) {}

  Node* GetCopy(Node* orig) { return copies_[orig->id()]; }

  void CopyGraph() {
    NodeVector inputs(temp_zone_);
    // TODO(bmeurer): AllNodes should be turned into something like
    // Graph::CollectNodesReachableFromEnd() and the gray set stuff should be
    // removed since it's only needed by the visualizer.
    AllNodes all(temp_zone_, source_graph_);
    // Copy all nodes reachable from end.
    for (Node* orig : all.live) {
      Node* copy = GetCopy(orig);
      if (copy != sentinel_) {
        // Mapping already exists.
        continue;
      }
      // Copy the node.
      inputs.clear();
      for (Node* input : orig->inputs()) inputs.push_back(copies_[input->id()]);
      copy = target_graph_->NewNode(orig->op(), orig->InputCount(),
                                    inputs.empty() ? nullptr : &inputs[0]);
      copies_[orig->id()] = copy;
    }
    // For missing inputs.
    for (Node* orig : all.live) {
      Node* copy = copies_[orig->id()];
      for (int i = 0; i < copy->InputCount(); ++i) {
        Node* input = copy->InputAt(i);
        if (input == sentinel_) {
          copy->ReplaceInput(i, GetCopy(orig->InputAt(i)));
        }
      }
    }
  }

  const NodeVector& copies() const { return copies_; }

 private:
  Operator const sentinel_op_;
  Node* const sentinel_;
  NodeVector copies_;
  Graph* const source_graph_;
  Graph* const target_graph_;
  Zone* const temp_zone_;
};


Reduction JSInliner::InlineCall(Node* call, Node* new_target, Node* context,
                                Node* frame_state, Node* start, Node* end) {
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
          Replace(use, jsgraph_->Int32Constant(inliner_inputs - 2));
        } else if (index == inlinee_context_index) {
          // The projection is requesting the inlinee function context.
          Replace(use, context);
        } else {
          // Call has fewer arguments than required, fill with undefined.
          Replace(use, jsgraph_->UndefinedConstant());
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

  NodeVector values(local_zone_);
  NodeVector effects(local_zone_);
  NodeVector controls(local_zone_);
  for (Node* const input : end->inputs()) {
    switch (input->opcode()) {
      case IrOpcode::kReturn:
        values.push_back(NodeProperties::GetValueInput(input, 0));
        effects.push_back(NodeProperties::GetEffectInput(input));
        controls.push_back(NodeProperties::GetControlInput(input));
        break;
      case IrOpcode::kDeoptimize:
      case IrOpcode::kTerminate:
      case IrOpcode::kThrow:
        NodeProperties::MergeControlToEnd(jsgraph_->graph(), jsgraph_->common(),
                                          input);
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
    Node* control_output = jsgraph_->graph()->NewNode(
        jsgraph_->common()->Merge(input_count), input_count, &controls.front());
    values.push_back(control_output);
    effects.push_back(control_output);
    Node* value_output = jsgraph_->graph()->NewNode(
        jsgraph_->common()->Phi(MachineRepresentation::kTagged, input_count),
        static_cast<int>(values.size()), &values.front());
    Node* effect_output = jsgraph_->graph()->NewNode(
        jsgraph_->common()->EffectPhi(input_count),
        static_cast<int>(effects.size()), &effects.front());
    ReplaceWithValue(call, value_output, effect_output, control_output);
    return Changed(value_output);
  } else {
    ReplaceWithValue(call, call, call, jsgraph_->Dead());
    return Changed(call);
  }
}


Node* JSInliner::CreateArtificialFrameState(Node* node, Node* outer_frame_state,
                                            int parameter_count,
                                            FrameStateType frame_state_type,
                                            Handle<SharedFunctionInfo> shared) {
  const FrameStateFunctionInfo* state_info =
      jsgraph_->common()->CreateFrameStateFunctionInfo(
          frame_state_type, parameter_count + 1, 0, shared,
          CALL_MAINTAINS_NATIVE_CONTEXT);

  const Operator* op = jsgraph_->common()->FrameState(
      BailoutId(-1), OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = jsgraph_->common()->StateValues(0);
  Node* node0 = jsgraph_->graph()->NewNode(op0);
  NodeVector params(local_zone_);
  for (int parameter = 0; parameter < parameter_count + 1; ++parameter) {
    params.push_back(node->InputAt(1 + parameter));
  }
  const Operator* op_param =
      jsgraph_->common()->StateValues(static_cast<int>(params.size()));
  Node* params_node = jsgraph_->graph()->NewNode(
      op_param, static_cast<int>(params.size()), &params.front());
  return jsgraph_->graph()->NewNode(op, params_node, node0, node0,
                                    jsgraph_->UndefinedConstant(),
                                    node->InputAt(0), outer_frame_state);
}


namespace {

// TODO(mstarzinger,verwaest): Move this predicate onto SharedFunctionInfo?
bool NeedsImplicitReceiver(Handle<JSFunction> function, Isolate* isolate) {
  Code* construct_stub = function->shared()->construct_stub();
  return construct_stub != *isolate->builtins()->JSBuiltinsConstructStub() &&
         construct_stub != *isolate->builtins()->ConstructedNonConstructable();
}

}  // namespace


Reduction JSInliner::Reduce(Node* node) {
  if (!IrOpcode::IsInlineeOpcode(node->opcode())) return NoChange();

  // This reducer can handle both normal function calls as well a constructor
  // calls whenever the target is a constant function object, as follows:
  //  - JSCallFunction(target:constant, receiver, args...)
  //  - JSCallConstruct(target:constant, args..., new.target)
  HeapObjectMatcher match(node->InputAt(0));
  if (!match.HasValue() || !match.Value()->IsJSFunction()) return NoChange();
  Handle<JSFunction> function = Handle<JSFunction>::cast(match.Value());

  return ReduceJSCall(node, function);
}


Reduction JSInliner::ReduceJSCall(Node* node, Handle<JSFunction> function) {
  DCHECK(IrOpcode::IsInlineeOpcode(node->opcode()));
  JSCallAccessor call(node);

  // Function must be inlineable.
  if (!function->shared()->IsInlineable()) {
    TRACE("Not inlining %s into %s because callee is not inlineable\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Constructor must be constructable.
  if (node->opcode() == IrOpcode::kJSCallConstruct &&
      !function->IsConstructor()) {
    TRACE("Not inlining %s into %s because constructor is not constructable.\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Class constructors are callable, but [[Call]] will raise an exception.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
  if (node->opcode() == IrOpcode::kJSCallFunction &&
      IsClassConstructor(function->shared()->kind())) {
    TRACE("Not inlining %s into %s because callee is a class constructor.\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Function contains break points.
  if (function->shared()->HasDebugInfo()) {
    TRACE("Not inlining %s into %s because callee may contain break points\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // Disallow cross native-context inlining for now. This means that all parts
  // of the resulting code will operate on the same global object.
  // This also prevents cross context leaks for asm.js code, where we could
  // inline functions from a different context and hold on to that context (and
  // closure) from the code object.
  // TODO(turbofan): We might want to revisit this restriction later when we
  // have a need for this, and we know how to model different native contexts
  // in the same graph in a compositional way.
  if (function->context()->native_context() !=
      info_->context()->native_context()) {
    TRACE("Not inlining %s into %s because of different native contexts\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  // TODO(turbofan): TranslatedState::GetAdaptedArguments() currently relies on
  // not inlining recursive functions. We might want to relax that at some
  // point.
  for (Node* frame_state = call.frame_state_after();
       frame_state->opcode() == IrOpcode::kFrameState;
       frame_state = frame_state->InputAt(kFrameStateOuterStateInput)) {
    FrameStateInfo const& info = OpParameter<FrameStateInfo>(frame_state);
    Handle<SharedFunctionInfo> shared_info;
    if (info.shared_info().ToHandle(&shared_info) &&
        *shared_info == function->shared()) {
      TRACE("Not inlining %s into %s because call is recursive\n",
            function->shared()->DebugName()->ToCString().get(),
            info_->shared_info()->DebugName()->ToCString().get());
      return NoChange();
    }
  }

  // TODO(turbofan): Inlining into a try-block is not yet supported.
  if (NodeProperties::IsExceptionalCall(node)) {
    TRACE("Not inlining %s into %s because of surrounding try-block\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  Zone zone;
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info);
  if (info_->is_deoptimization_enabled()) {
    info.MarkAsDeoptimizationEnabled();
  }

  if (!Compiler::ParseAndAnalyze(info.parse_info())) {
    TRACE("Not inlining %s into %s because parsing failed\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    if (info_->isolate()->has_pending_exception()) {
      info_->isolate()->clear_pending_exception();
    }
    return NoChange();
  }

  // In strong mode, in case of too few arguments we need to throw a TypeError
  // so we must not inline this call.
  int parameter_count = info.literal()->parameter_count();
  if (is_strong(info.language_mode()) &&
      call.formal_arguments() < parameter_count) {
    TRACE("Not inlining %s into %s because too few arguments for strong mode\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  if (!Compiler::EnsureDeoptimizationSupport(&info)) {
    TRACE("Not inlining %s into %s because deoptimization support failed\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }
  // Remember that we inlined this function. This needs to be called right
  // after we ensure deoptimization support so that the code flusher
  // does not remove the code with the deoptimization support.
  info_->AddInlinedFunction(info.shared_info());

  // ----------------------------------------------------------------
  // After this point, we've made a decision to inline this function.
  // We shall not bailout from inlining if we got here.

  TRACE("Inlining %s into %s\n",
        function->shared()->DebugName()->ToCString().get(),
        info_->shared_info()->DebugName()->ToCString().get());

  // TODO(mstarzinger): We could use the temporary zone for the graph because
  // nodes are copied. This however leads to Zone-Types being allocated in the
  // wrong zone and makes the engine explode at high speeds. Explosion bad!
  Graph graph(jsgraph_->zone());
  JSGraph jsgraph(info.isolate(), &graph, jsgraph_->common(),
                  jsgraph_->javascript(), jsgraph_->simplified(),
                  jsgraph_->machine());
  AstGraphBuilder graph_builder(local_zone_, &info, &jsgraph);
  graph_builder.CreateGraph(false);

  CopyVisitor visitor(&graph, jsgraph_->graph(), &zone);
  visitor.CopyGraph();

  Node* start = visitor.GetCopy(graph.start());
  Node* end = visitor.GetCopy(graph.end());
  Node* frame_state = call.frame_state_after();
  Node* new_target = jsgraph_->UndefinedConstant();

  // Insert nodes around the call that model the behavior required for a
  // constructor dispatch (allocate implicit receiver and check return value).
  // This models the behavior usually accomplished by our {JSConstructStub}.
  // Note that the context has to be the callers context (input to call node).
  Node* receiver = jsgraph_->UndefinedConstant();  // Implicit receiver.
  if (node->opcode() == IrOpcode::kJSCallConstruct &&
      NeedsImplicitReceiver(function, info_->isolate())) {
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* context = NodeProperties::GetContextInput(node);
    Node* create = jsgraph_->graph()->NewNode(
        jsgraph_->javascript()->Create(), call.target(), call.new_target(),
        context, call.frame_state_before(), effect);
    NodeProperties::ReplaceEffectInput(node, create);
    // Insert a check of the return value to determine whether the return value
    // or the implicit receiver should be selected as a result of the call.
    Node* check = jsgraph_->graph()->NewNode(
        jsgraph_->javascript()->CallRuntime(Runtime::kInlineIsJSReceiver, 1),
        node, context, node, start);
    Node* select = jsgraph_->graph()->NewNode(
        jsgraph_->common()->Select(MachineRepresentation::kTagged), check, node,
        create);
    NodeProperties::ReplaceUses(node, select, check, node, node);
    NodeProperties::ReplaceValueInput(select, node, 1);
    NodeProperties::ReplaceValueInput(check, node, 0);
    NodeProperties::ReplaceEffectInput(check, node);
    receiver = create;  // The implicit receiver.
  }

  // Swizzle the inputs of the {JSCallConstruct} node to look like inputs to a
  // normal {JSCallFunction} node so that the rest of the inlining machinery
  // behaves as if we were dealing with a regular function invocation.
  if (node->opcode() == IrOpcode::kJSCallConstruct) {
    new_target = call.new_target();  // Retrieve new target value input.
    node->RemoveInput(call.formal_arguments() + 1);  // Drop new target.
    node->InsertInput(jsgraph_->graph()->zone(), 1, receiver);
    // Insert a construct stub frame into the chain of frame states. This will
    // reconstruct the proper frame when deoptimizing within the constructor.
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.formal_arguments(),
        FrameStateType::kConstructStub, info.shared_info());
  }

  // The inlinee specializes to the context from the JSFunction object.
  // TODO(turbofan): We might want to load the context from the JSFunction at
  // runtime in case we only know the SharedFunctionInfo once we have dynamic
  // type feedback in the compiler.
  Node* context = jsgraph_->Constant(handle(function->context()));

  // Insert a JSConvertReceiver node for sloppy callees. Note that the context
  // passed into this node has to be the callees context (loaded above). Note
  // that the frame state passed to the JSConvertReceiver must be the frame
  // state _before_ the call; it is not necessary to fiddle with the receiver
  // in that frame state tho, as the conversion of the receiver can be repeated
  // any number of times, it's not observable.
  if (node->opcode() == IrOpcode::kJSCallFunction &&
      is_sloppy(info.language_mode()) && !function->shared()->native()) {
    const CallFunctionParameters& p = CallFunctionParametersOf(node->op());
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* convert = jsgraph_->graph()->NewNode(
        jsgraph_->javascript()->ConvertReceiver(p.convert_mode()),
        call.receiver(), context, call.frame_state_before(), effect, start);
    NodeProperties::ReplaceValueInput(node, convert, 1);
    NodeProperties::ReplaceEffectInput(node, convert);
  }

  // Insert argument adaptor frame if required. The callees formal parameter
  // count (i.e. value outputs of start node minus target, receiver, new target,
  // arguments count and context) have to match the number of arguments passed
  // to the call.
  DCHECK_EQ(parameter_count, start->op()->ValueOutputCount() - 5);
  if (call.formal_arguments() != parameter_count) {
    frame_state = CreateArtificialFrameState(
        node, frame_state, call.formal_arguments(),
        FrameStateType::kArgumentsAdaptor, info.shared_info());
  }

  return InlineCall(node, new_target, context, frame_state, start, end);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
