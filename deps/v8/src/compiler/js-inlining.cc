// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-inlining.h"

#include "src/ast.h"
#include "src/ast-numbering.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/ast-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-context-specialization.h"
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


namespace {

// A facade on a JSFunction's graph to facilitate inlining. It assumes the
// that the function graph has only one return statement, and provides
// {UnifyReturn} to convert a function graph to that end.
class Inlinee {
 public:
  Inlinee(Node* start, Node* end) : start_(start), end_(end) {}

  // Returns the last regular control node, that is
  // the last control node before the end node.
  Node* end_block() { return NodeProperties::GetControlInput(unique_return()); }

  // Return the effect output of the graph,
  // that is the effect input of the return statement of the inlinee.
  Node* effect_output() {
    return NodeProperties::GetEffectInput(unique_return());
  }
  // Return the value output of the graph,
  // that is the value input of the return statement of the inlinee.
  Node* value_output() {
    return NodeProperties::GetValueInput(unique_return(), 0);
  }
  // Return the control output of the graph,
  // that is the control input of the return statement of the inlinee.
  Node* control_output() {
    return NodeProperties::GetControlInput(unique_return(), 0);
  }
  // Return the unique return statement of the graph.
  Node* unique_return() {
    Node* unique_return = NodeProperties::GetControlInput(end_);
    DCHECK_EQ(IrOpcode::kReturn, unique_return->opcode());
    return unique_return;
  }

  // Counts JSFunction, Receiver, arguments, context but not effect, control.
  size_t total_parameters() { return start_->op()->ValueOutputCount(); }

  // Counts only formal parameters.
  size_t formal_parameters() {
    DCHECK_GE(total_parameters(), 3u);
    return total_parameters() - 3;
  }

  // Inline this graph at {call}, use {jsgraph} and its zone to create
  // any new nodes.
  Reduction InlineAtCall(JSGraph* jsgraph, Node* call);

  // Ensure that only a single return reaches the end node.
  static void UnifyReturn(JSGraph* jsgraph);

 private:
  Node* start_;
  Node* end_;
};


void Inlinee::UnifyReturn(JSGraph* jsgraph) {
  Graph* graph = jsgraph->graph();

  Node* final_merge = NodeProperties::GetControlInput(graph->end(), 0);
  if (final_merge->opcode() == IrOpcode::kReturn) {
    // nothing to do
    return;
  }
  DCHECK_EQ(IrOpcode::kMerge, final_merge->opcode());

  int predecessors = final_merge->op()->ControlInputCount();

  const Operator* op_phi = jsgraph->common()->Phi(kMachAnyTagged, predecessors);
  const Operator* op_ephi = jsgraph->common()->EffectPhi(predecessors);

  NodeVector values(jsgraph->zone());
  NodeVector effects(jsgraph->zone());
  // Iterate over all control flow predecessors,
  // which must be return statements.
  for (Edge edge : final_merge->input_edges()) {
    Node* input = edge.to();
    switch (input->opcode()) {
      case IrOpcode::kReturn:
        values.push_back(NodeProperties::GetValueInput(input, 0));
        effects.push_back(NodeProperties::GetEffectInput(input));
        edge.UpdateTo(NodeProperties::GetControlInput(input));
        input->NullAllInputs();
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
  values.push_back(final_merge);
  effects.push_back(final_merge);
  Node* phi =
      graph->NewNode(op_phi, static_cast<int>(values.size()), &values.front());
  Node* ephi = graph->NewNode(op_ephi, static_cast<int>(effects.size()),
                              &effects.front());
  Node* new_return =
      graph->NewNode(jsgraph->common()->Return(), phi, ephi, final_merge);
  graph->end()->ReplaceInput(0, new_return);
}


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


Reduction Inlinee::InlineAtCall(JSGraph* jsgraph, Node* call) {
  // The scheduler is smart enough to place our code; we just ensure {control}
  // becomes the control input of the start of the inlinee, and {effect} becomes
  // the effect input of the start of the inlinee.
  Node* control = NodeProperties::GetControlInput(call);
  Node* effect = NodeProperties::GetEffectInput(call);

  // Context is last argument.
  int inlinee_context_index = static_cast<int>(total_parameters()) - 1;
  // {inliner_inputs} counts JSFunction, Receiver, arguments, but not
  // context, effect, control.
  int inliner_inputs = call->op()->ValueInputCount();
  // Iterate over all uses of the start node.
  for (Edge edge : start_->use_edges()) {
    Node* use = edge.from();
    switch (use->opcode()) {
      case IrOpcode::kParameter: {
        int index = 1 + ParameterIndexOf(use->op());
        if (index < inliner_inputs && index < inlinee_context_index) {
          // There is an input from the call, and the index is a value
          // projection but not the context, so rewire the input.
          NodeProperties::ReplaceWithValue(use, call->InputAt(index));
        } else if (index == inlinee_context_index) {
          // TODO(turbofan): We always context specialize inlinees currently, so
          // we should never get here.
          UNREACHABLE();
        } else if (index < inlinee_context_index) {
          // Call has fewer arguments than required, fill with undefined.
          NodeProperties::ReplaceWithValue(use, jsgraph->UndefinedConstant());
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
        } else {
          UNREACHABLE();
        }
        break;
    }
  }

  NodeProperties::ReplaceWithValue(call, value_output(), effect_output(),
                                   control_output());

  return Reducer::Replace(value_output());
}

}  // namespace


void JSInliner::AddClosureToFrameState(Node* frame_state,
                                       Handle<JSFunction> jsfunction) {
  FrameStateCallInfo call_info = OpParameter<FrameStateCallInfo>(frame_state);
  const Operator* op = jsgraph_->common()->FrameState(
      FrameStateType::JS_FRAME, call_info.bailout_id(),
      call_info.state_combine(), jsfunction);
  frame_state->set_op(op);
}


Node* JSInliner::CreateArgumentsAdaptorFrameState(JSCallFunctionAccessor* call,
                                                  Handle<JSFunction> jsfunction,
                                                  Zone* temp_zone) {
  const Operator* op = jsgraph_->common()->FrameState(
      FrameStateType::ARGUMENTS_ADAPTOR, BailoutId(-1),
      OutputFrameStateCombine::Ignore(), jsfunction);
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
                                    call->frame_state());
}


Reduction JSInliner::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kJSCallFunction) return NoChange();

  JSCallFunctionAccessor call(node);
  HeapObjectMatcher<JSFunction> match(call.jsfunction());
  if (!match.HasValue()) return NoChange();

  Handle<JSFunction> function = match.Value().handle();
  if (!function->IsJSFunction()) return NoChange();
  if (mode_ == kBuiltinsInlining && !function->shared()->inline_builtin()) {
    return NoChange();
  }

  Zone zone;
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info);

  if (!Compiler::ParseAndAnalyze(info.parse_info())) return NoChange();
  if (!Compiler::EnsureDeoptimizationSupport(&info)) return NoChange();

  if (info.scope()->arguments() != NULL && is_sloppy(info.language_mode())) {
    // For now do not inline functions that use their arguments array.
    TRACE("Not Inlining %s into %s because inlinee uses arguments array\n",
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

  // The inlinee specializes to the context from the JSFunction object.
  // TODO(turbofan): We might want to load the context from the JSFunction at
  // runtime in case we only know the SharedFunctionInfo once we have dynamic
  // type feedback in the compiler.
  AstGraphBuilder graph_builder(local_zone_, &info, &jsgraph);
  graph_builder.CreateGraph(true, false);
  JSContextSpecializer context_specializer(&jsgraph);
  GraphReducer graph_reducer(&graph, local_zone_);
  graph_reducer.AddReducer(&context_specializer);
  graph_reducer.ReduceGraph();
  Inlinee::UnifyReturn(&jsgraph);

  CopyVisitor visitor(&graph, jsgraph_->graph(), info.zone());
  visitor.CopyGraph();

  Inlinee inlinee(visitor.GetCopy(graph.start()), visitor.GetCopy(graph.end()));

  Node* outer_frame_state = call.frame_state();
  // Insert argument adaptor frame if required.
  if (call.formal_arguments() != inlinee.formal_parameters()) {
    outer_frame_state =
        CreateArgumentsAdaptorFrameState(&call, function, info.zone());
  }

  for (Node* node : visitor.copies()) {
    if (node && node->opcode() == IrOpcode::kFrameState) {
      DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
      AddClosureToFrameState(node, function);
      NodeProperties::ReplaceFrameStateInput(node, 0, outer_frame_state);
    }
  }

  return inlinee.InlineAtCall(jsgraph_, node);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
