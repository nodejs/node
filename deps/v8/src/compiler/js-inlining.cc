// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining.h"

#include "src/ast.h"
#include "src/ast-numbering.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/full-codegen.h"
#include "src/parser.h"
#include "src/rewriter.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_turbo_inlining) PrintF(__VA_ARGS__); \
  } while (false)


// Provides convenience accessors for calls to JS functions.
class JSCallFunctionAccessor {
 public:
  explicit JSCallFunctionAccessor(Node* call) : call_(call) {
    DCHECK_EQ(IrOpcode::kJSCallFunction, call->opcode());
  }

  Node* jsfunction() { return call_->InputAt(0); }

  Node* receiver() { return call_->InputAt(1); }

  Node* formal_argument(size_t index) {
    DCHECK(index < formal_arguments());
    return call_->InputAt(static_cast<int>(2 + index));
  }

  size_t formal_arguments() {
    // {value_inputs} includes jsfunction and receiver.
    size_t value_inputs = call_->op()->ValueInputCount();
    DCHECK_GE(call_->InputCount(), 2);
    return value_inputs - 2;
  }

  Node* frame_state() { return NodeProperties::GetFrameStateInput(call_, 0); }

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


Reduction JSInliner::InlineCall(Node* call, Node* context, Node* frame_state,
                                Node* start, Node* end) {
  // The scheduler is smart enough to place our code; we just ensure {control}
  // becomes the control input of the start of the inlinee, and {effect} becomes
  // the effect input of the start of the inlinee.
  Node* control = NodeProperties::GetControlInput(call);
  Node* effect = NodeProperties::GetEffectInput(call);

  // Context is last argument.
  int const inlinee_context_index =
      static_cast<int>(start->op()->ValueOutputCount()) - 1;

  // {inliner_inputs} counts JSFunction, Receiver, arguments, but not
  // context, effect, control.
  int inliner_inputs = call->op()->ValueInputCount();
  // Iterate over all uses of the start node.
  for (Edge edge : start->use_edges()) {
    Node* use = edge.from();
    switch (use->opcode()) {
      case IrOpcode::kParameter: {
        int index = 1 + ParameterIndexOf(use->op());
        if (index < inliner_inputs && index < inlinee_context_index) {
          // There is an input from the call, and the index is a value
          // projection but not the context, so rewire the input.
          Replace(use, call->InputAt(index));
        } else if (index == inlinee_context_index) {
          Replace(use, context);
        } else if (index < inlinee_context_index) {
          // Call has fewer arguments than required, fill with undefined.
          Replace(use, jsgraph_->UndefinedConstant());
        } else {
          // We got too many arguments, discard for now.
          // TODO(sigurds): Fix to treat arguments array correctly.
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
        jsgraph_->graph()->end()->AppendInput(jsgraph_->zone(), input);
        jsgraph_->graph()->end()->set_op(
            jsgraph_->common()->End(jsgraph_->graph()->end()->InputCount()));
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
  DCHECK_NE(0u, values.size());
  DCHECK_EQ(values.size(), effects.size());
  DCHECK_EQ(values.size(), controls.size());
  int const input_count = static_cast<int>(controls.size());
  Node* control_output = jsgraph_->graph()->NewNode(
      jsgraph_->common()->Merge(input_count), input_count, &controls.front());
  values.push_back(control_output);
  effects.push_back(control_output);
  Node* value_output = jsgraph_->graph()->NewNode(
      jsgraph_->common()->Phi(kMachAnyTagged, input_count),
      static_cast<int>(values.size()), &values.front());
  Node* effect_output = jsgraph_->graph()->NewNode(
      jsgraph_->common()->EffectPhi(input_count),
      static_cast<int>(effects.size()), &effects.front());

  ReplaceWithValue(call, value_output, effect_output, control_output);

  return Changed(value_output);
}


Node* JSInliner::CreateArgumentsAdaptorFrameState(
    JSCallFunctionAccessor* call, Handle<SharedFunctionInfo> shared_info,
    Zone* temp_zone) {
  const FrameStateFunctionInfo* state_info =
      jsgraph_->common()->CreateFrameStateFunctionInfo(
          FrameStateType::kArgumentsAdaptor,
          static_cast<int>(call->formal_arguments()) + 1, 0, shared_info);

  const Operator* op = jsgraph_->common()->FrameState(
      BailoutId(-1), OutputFrameStateCombine::Ignore(), state_info);
  const Operator* op0 = jsgraph_->common()->StateValues(0);
  Node* node0 = jsgraph_->graph()->NewNode(op0);
  NodeVector params(temp_zone);
  params.push_back(call->receiver());
  for (size_t argument = 0; argument != call->formal_arguments(); ++argument) {
    params.push_back(call->formal_argument(argument));
  }
  const Operator* op_param =
      jsgraph_->common()->StateValues(static_cast<int>(params.size()));
  Node* params_node = jsgraph_->graph()->NewNode(
      op_param, static_cast<int>(params.size()), &params.front());
  return jsgraph_->graph()->NewNode(op, params_node, node0, node0,
                                    jsgraph_->UndefinedConstant(),
                                    call->jsfunction(), call->frame_state());
}


Reduction JSInliner::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kJSCallFunction) return NoChange();

  JSCallFunctionAccessor call(node);
  HeapObjectMatcher match(call.jsfunction());
  if (!match.HasValue()) return NoChange();

  if (!match.Value().handle()->IsJSFunction()) return NoChange();
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(match.Value().handle());
  if (mode_ == kRestrictedInlining && !function->shared()->force_inline()) {
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
  for (Node* frame_state = call.frame_state();
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

  Zone zone;
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info);
  if (info_->is_deoptimization_enabled()) info.MarkAsDeoptimizationEnabled();

  if (!Compiler::ParseAndAnalyze(info.parse_info())) return NoChange();
  if (!Compiler::EnsureDeoptimizationSupport(&info)) return NoChange();

  if (info.scope()->arguments() != NULL && is_sloppy(info.language_mode())) {
    // For now do not inline functions that use their arguments array.
    TRACE("Not inlining %s into %s because inlinee uses arguments array\n",
          function->shared()->DebugName()->ToCString().get(),
          info_->shared_info()->DebugName()->ToCString().get());
    return NoChange();
  }

  TRACE("Inlining %s into %s\n",
        function->shared()->DebugName()->ToCString().get(),
        info_->shared_info()->DebugName()->ToCString().get());

  Graph graph(info.zone());
  JSGraph jsgraph(info.isolate(), &graph, jsgraph_->common(),
                  jsgraph_->javascript(), jsgraph_->machine());
  AstGraphBuilder graph_builder(local_zone_, &info, &jsgraph);
  graph_builder.CreateGraph(false);

  // The inlinee specializes to the context from the JSFunction object.
  // TODO(turbofan): We might want to load the context from the JSFunction at
  // runtime in case we only know the SharedFunctionInfo once we have dynamic
  // type feedback in the compiler.
  Node* context = jsgraph_->Constant(handle(function->context()));

  CopyVisitor visitor(&graph, jsgraph_->graph(), info.zone());
  visitor.CopyGraph();

  Node* start = visitor.GetCopy(graph.start());
  Node* end = visitor.GetCopy(graph.end());

  Node* frame_state = call.frame_state();
  size_t const inlinee_formal_parameters = start->op()->ValueOutputCount() - 3;
  // Insert argument adaptor frame if required.
  if (call.formal_arguments() != inlinee_formal_parameters) {
    // In strong mode, in case of too few arguments we need to throw a
    // TypeError so we must not inline this call.
    if (is_strong(info.language_mode()) &&
        call.formal_arguments() < inlinee_formal_parameters) {
      return NoChange();
    }
    frame_state = CreateArgumentsAdaptorFrameState(&call, info.shared_info(),
                                                   info.zone());
  }

  // Remember that we inlined this function.
  info_->AddInlinedFunction(info.shared_info());

  return InlineCall(node, context, frame_state, start, end);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
