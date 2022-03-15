// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-inlining.h"

#include "src/compiler/all-nodes.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction WasmInliner::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kCall:
    case IrOpcode::kTailCall:
      return ReduceCall(node);
    default:
      return NoChange();
  }
}

#define TRACE(...) \
  if (FLAG_trace_wasm_inlining) PrintF(__VA_ARGS__);

// TODO(12166): Save inlined frames for trap/--trace-wasm purposes. Consider
//              tail calls.
Reduction WasmInliner::ReduceCall(Node* call) {
  DCHECK(call->opcode() == IrOpcode::kCall ||
         call->opcode() == IrOpcode::kTailCall);

  if (seen_.find(call) != seen_.end()) {
    TRACE("function %d: have already seen node %d, skipping\n", function_index_,
          call->id());
    return NoChange();
  }
  seen_.insert(call);

  Node* callee = NodeProperties::GetValueInput(call, 0);
  IrOpcode::Value reloc_opcode = mcgraph_->machine()->Is32()
                                     ? IrOpcode::kRelocatableInt32Constant
                                     : IrOpcode::kRelocatableInt64Constant;
  if (callee->opcode() != reloc_opcode) {
    TRACE("[function %d: considering node %d... not a relocatable constant]\n",
          function_index_, call->id());
    return NoChange();
  }
  auto info = OpParameter<RelocatablePtrConstantInfo>(callee->op());
  uint32_t inlinee_index = static_cast<uint32_t>(info.value());
  TRACE("[function %d: considering node %d, call to %d... ", function_index_,
        call->id(), inlinee_index)
  if (info.rmode() != RelocInfo::WASM_CALL) {
    TRACE("not a wasm call]\n")
    return NoChange();
  }
  if (inlinee_index < module()->num_imported_functions) {
    TRACE("imported function]\n")
    return NoChange();
  }
  if (inlinee_index == function_index_) {
    TRACE("recursive call]\n")
    return NoChange();
  }

  TRACE("adding to inlining candidates!]\n")

  bool is_speculative_call_ref = false;
  int call_count = 0;
  if (FLAG_wasm_speculative_inlining) {
    base::MutexGuard guard(&module()->type_feedback.mutex);
    auto maybe_feedback =
        module()->type_feedback.feedback_for_function.find(function_index_);
    if (maybe_feedback != module()->type_feedback.feedback_for_function.end()) {
      wasm::FunctionTypeFeedback feedback = maybe_feedback->second;
      wasm::WasmCodePosition position =
          source_positions_->GetSourcePosition(call).ScriptOffset();
      DCHECK_NE(position, wasm::kNoCodePosition);
      auto index_in_feedback_vector = feedback.positions.find(position);
      if (index_in_feedback_vector != feedback.positions.end()) {
        is_speculative_call_ref = true;
        call_count = feedback.feedback_vector[index_in_feedback_vector->second]
                         .absolute_call_frequency;
      }
    }
  }

  CHECK_LT(inlinee_index, module()->functions.size());
  const wasm::WasmFunction* inlinee = &module()->functions[inlinee_index];
  base::Vector<const byte> function_bytes = wire_bytes_->GetCode(inlinee->code);

  CandidateInfo candidate{call, inlinee_index, is_speculative_call_ref,
                          call_count, function_bytes.length()};

  inlining_candidates_.push(candidate);
  return NoChange();
}

void WasmInliner::Finalize() {
  TRACE("function %d: going though inlining candidates...\n", function_index_);
  while (!inlining_candidates_.empty()) {
    CandidateInfo candidate = inlining_candidates_.top();
    inlining_candidates_.pop();
    Node* call = candidate.node;
    TRACE(
        "  [function %d: considering candidate {@%d, index=%d, type=%s, "
        "count=%d, size=%d}... ",
        function_index_, call->id(), candidate.inlinee_index,
        candidate.is_speculative_call_ref ? "ref" : "direct",
        candidate.call_count, candidate.wire_byte_size);
    if (call->IsDead()) {
      TRACE("dead node]\n");
      continue;
    }
    const wasm::WasmFunction* inlinee =
        &module()->functions[candidate.inlinee_index];
    base::Vector<const byte> function_bytes =
        wire_bytes_->GetCode(inlinee->code);
    // We use the signature based on the real argument types stored in the call
    // node. This is more specific than the callee's formal signature and might
    // enable some optimizations.
    const wasm::FunctionSig* real_sig =
        CallDescriptorOf(call->op())->wasm_sig();

#if DEBUG
    // Check that the real signature is a subtype of the formal one.
    const wasm::FunctionSig* formal_sig =
        WasmGraphBuilder::Int64LoweredSig(zone(), inlinee->sig);
    CHECK_EQ(real_sig->parameter_count(), formal_sig->parameter_count());
    CHECK_EQ(real_sig->return_count(), formal_sig->return_count());
    for (size_t i = 0; i < real_sig->parameter_count(); i++) {
      CHECK(wasm::IsSubtypeOf(real_sig->GetParam(i), formal_sig->GetParam(i),
                              module()));
    }
    for (size_t i = 0; i < real_sig->return_count(); i++) {
      CHECK(wasm::IsSubtypeOf(formal_sig->GetReturn(i), real_sig->GetReturn(i),
                              module()));
    }
#endif

    const wasm::FunctionBody inlinee_body(real_sig, inlinee->code.offset(),
                                          function_bytes.begin(),
                                          function_bytes.end());
    wasm::WasmFeatures detected;
    WasmGraphBuilder builder(env_, zone(), mcgraph_, inlinee_body.sig,
                             source_positions_);
    std::vector<WasmLoopInfo> infos;

    size_t subgraph_min_node_id = graph()->NodeCount();
    Node* inlinee_start;
    Node* inlinee_end;
    {
      Graph::SubgraphScope scope(graph());
      wasm::DecodeResult result = wasm::BuildTFGraph(
          zone()->allocator(), env_->enabled_features, module(), &builder,
          &detected, inlinee_body, &infos, node_origins_,
          candidate.inlinee_index, wasm::kInlinedFunction);
      if (result.failed()) {
        // This can happen if the inlinee has never been compiled before and is
        // invalid. Return, as there is no point to keep optimizing.
        TRACE("failed to compile]\n")
        return;
      }

      builder.LowerInt64(WasmGraphBuilder::kCalledFromWasm);
      inlinee_start = graph()->start();
      inlinee_end = graph()->end();
    }

    size_t additional_nodes = graph()->NodeCount() - subgraph_min_node_id;
    if (current_graph_size_ + additional_nodes >
        size_limit(initial_graph_size_)) {
      // This is not based on the accurate graph size, as it may have been
      // shrunk by other optimizations. We could recompute the accurate size
      // with a traversal, but it is most probably not worth the time.
      TRACE("not enough inlining budget]\n");
      continue;
    }
    TRACE("inlining!]\n");
    current_graph_size_ += additional_nodes;

    if (call->opcode() == IrOpcode::kCall) {
      InlineCall(call, inlinee_start, inlinee_end, inlinee->sig,
                 subgraph_min_node_id);
    } else {
      InlineTailCall(call, inlinee_start, inlinee_end);
    }
    call->Kill();
    // Returning after only one inlining has been tried and found worse.
  }
}

/* Rewire callee formal parameters to the call-site real parameters. Rewire
 * effect and control dependencies of callee's start node with the respective
 * inputs of the call node.
 */
void WasmInliner::RewireFunctionEntry(Node* call, Node* callee_start) {
  Node* control = NodeProperties::GetControlInput(call);
  Node* effect = NodeProperties::GetEffectInput(call);

  for (Edge edge : callee_start->use_edges()) {
    Node* use = edge.from();
    switch (use->opcode()) {
      case IrOpcode::kParameter: {
        // Index 0 is the callee node.
        int index = 1 + ParameterIndexOf(use->op());
        Replace(use, NodeProperties::GetValueInput(call, index));
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
        Revisit(edge.from());
        break;
    }
  }
}

void WasmInliner::InlineTailCall(Node* call, Node* callee_start,
                                 Node* callee_end) {
  DCHECK(call->opcode() == IrOpcode::kTailCall);
  // 1) Rewire function entry.
  RewireFunctionEntry(call, callee_start);
  // 2) For tail calls, all we have to do is rewire all terminators of the
  // inlined graph to the end of the caller graph.
  for (Node* const input : callee_end->inputs()) {
    DCHECK(IrOpcode::IsGraphTerminator(input->opcode()));
    NodeProperties::MergeControlToEnd(graph(), common(), input);
  }
  for (Edge edge_to_end : call->use_edges()) {
    DCHECK_EQ(edge_to_end.from(), graph()->end());
    edge_to_end.UpdateTo(mcgraph()->Dead());
  }
  callee_end->Kill();
  call->Kill();
  Revisit(graph()->end());
}

void WasmInliner::InlineCall(Node* call, Node* callee_start, Node* callee_end,
                             const wasm::FunctionSig* inlinee_sig,
                             size_t subgraph_min_node_id) {
  DCHECK(call->opcode() == IrOpcode::kCall);

  // 0) Before doing anything, if {call} has an exception handler, collect all
  // unhandled calls in the subgraph.
  Node* handler = nullptr;
  std::vector<Node*> unhandled_subcalls;
  if (NodeProperties::IsExceptionalCall(call, &handler)) {
    AllNodes subgraph_nodes(zone(), callee_end, graph());
    for (Node* node : subgraph_nodes.reachable) {
      if (node->id() >= subgraph_min_node_id &&
          !node->op()->HasProperty(Operator::kNoThrow) &&
          !NodeProperties::IsExceptionalCall(node)) {
        unhandled_subcalls.push_back(node);
      }
    }
  }

  // 1) Rewire function entry.
  RewireFunctionEntry(call, callee_start);

  // 2) Handle all graph terminators for the callee.
  NodeVector return_nodes(zone());
  for (Node* const input : callee_end->inputs()) {
    DCHECK(IrOpcode::IsGraphTerminator(input->opcode()));
    switch (input->opcode()) {
      case IrOpcode::kReturn:
        // Returns are collected to be rewired into the caller graph later.
        return_nodes.push_back(input);
        break;
      case IrOpcode::kDeoptimize:
      case IrOpcode::kTerminate:
      case IrOpcode::kThrow:
        NodeProperties::MergeControlToEnd(graph(), common(), input);
        Revisit(graph()->end());
        break;
      case IrOpcode::kTailCall: {
        // A tail call in the callee inlined in a regular call in the caller has
        // to be transformed into a regular call, and then returned from the
        // inlinee. It will then be handled like any other return.
        auto descriptor = CallDescriptorOf(input->op());
        NodeProperties::ChangeOp(input, common()->Call(descriptor));
        int return_arity = static_cast<int>(inlinee_sig->return_count());
        NodeVector return_inputs(zone());
        // The first input of a return node is always the 0 constant.
        return_inputs.push_back(graph()->NewNode(common()->Int32Constant(0)));
        if (return_arity == 1) {
          return_inputs.push_back(input);
        } else if (return_arity > 1) {
          for (int i = 0; i < return_arity; i++) {
            return_inputs.push_back(
                graph()->NewNode(common()->Projection(i), input, input));
          }
        }

        // Add effect and control inputs.
        return_inputs.push_back(input->op()->EffectOutputCount() > 0
                                    ? input
                                    : NodeProperties::GetEffectInput(input));
        return_inputs.push_back(input->op()->ControlOutputCount() > 0
                                    ? input
                                    : NodeProperties::GetControlInput(input));

        Node* ret = graph()->NewNode(common()->Return(return_arity),
                                     static_cast<int>(return_inputs.size()),
                                     return_inputs.data());
        return_nodes.push_back(ret);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  callee_end->Kill();

  // 3) Rewire unhandled calls to the handler.
  std::vector<Node*> on_exception_nodes;
  for (Node* subcall : unhandled_subcalls) {
    Node* on_success = graph()->NewNode(common()->IfSuccess(), subcall);
    NodeProperties::ReplaceUses(subcall, subcall, subcall, on_success);
    NodeProperties::ReplaceControlInput(on_success, subcall);
    Node* on_exception =
        graph()->NewNode(common()->IfException(), subcall, subcall);
    on_exception_nodes.push_back(on_exception);
  }

  int subcall_count = static_cast<int>(on_exception_nodes.size());

  if (subcall_count > 0) {
    Node* control_output =
        graph()->NewNode(common()->Merge(subcall_count), subcall_count,
                         on_exception_nodes.data());
    on_exception_nodes.push_back(control_output);
    Node* value_output = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, subcall_count),
        subcall_count + 1, on_exception_nodes.data());
    Node* effect_output =
        graph()->NewNode(common()->EffectPhi(subcall_count), subcall_count + 1,
                         on_exception_nodes.data());
    ReplaceWithValue(handler, value_output, effect_output, control_output);
  } else if (handler != nullptr) {
    // Nothing in the inlined function can throw. Remove the handler.
    ReplaceWithValue(handler, mcgraph()->Dead(), mcgraph()->Dead(),
                     mcgraph()->Dead());
  }

  if (return_nodes.size() > 0) {
    /* 4) Collect all return site value, effect, and control inputs into phis
     * and merges. */
    int const return_count = static_cast<int>(return_nodes.size());
    NodeVector controls(zone());
    NodeVector effects(zone());
    for (Node* const return_node : return_nodes) {
      controls.push_back(NodeProperties::GetControlInput(return_node));
      effects.push_back(NodeProperties::GetEffectInput(return_node));
    }
    Node* control_output = graph()->NewNode(common()->Merge(return_count),
                                            return_count, &controls.front());
    effects.push_back(control_output);
    Node* effect_output =
        graph()->NewNode(common()->EffectPhi(return_count),
                         static_cast<int>(effects.size()), &effects.front());

    // The first input of a return node is discarded. This is because Wasm
    // functions always return an additional 0 constant as a first return value.
    DCHECK(
        Int32Matcher(NodeProperties::GetValueInput(return_nodes[0], 0)).Is(0));
    int const return_arity = return_nodes[0]->op()->ValueInputCount() - 1;
    NodeVector values(zone());
    for (int i = 0; i < return_arity; i++) {
      NodeVector ith_values(zone());
      for (Node* const return_node : return_nodes) {
        Node* value = NodeProperties::GetValueInput(return_node, i + 1);
        ith_values.push_back(value);
      }
      ith_values.push_back(control_output);
      // Find the correct machine representation for the return values from the
      // inlinee signature.
      MachineRepresentation repr =
          inlinee_sig->GetReturn(i).machine_representation();
      Node* ith_value_output = graph()->NewNode(
          common()->Phi(repr, return_count),
          static_cast<int>(ith_values.size()), &ith_values.front());
      values.push_back(ith_value_output);
    }
    for (Node* return_node : return_nodes) return_node->Kill();

    if (return_arity == 0) {
      // Void function, no value uses.
      ReplaceWithValue(call, mcgraph()->Dead(), effect_output, control_output);
    } else if (return_arity == 1) {
      // One return value. Just replace value uses of the call node with it.
      ReplaceWithValue(call, values[0], effect_output, control_output);
    } else {
      // Multiple returns. We have to find the projections of the call node and
      // replace them with the returned values.
      for (Edge use_edge : call->use_edges()) {
        if (NodeProperties::IsValueEdge(use_edge)) {
          Node* use = use_edge.from();
          DCHECK_EQ(use->opcode(), IrOpcode::kProjection);
          ReplaceWithValue(use, values[ProjectionIndexOf(use->op())]);
        }
      }
      // All value inputs are replaced by the above loop, so it is ok to use
      // Dead() as a dummy for value replacement.
      ReplaceWithValue(call, mcgraph()->Dead(), effect_output, control_output);
    }
  } else {
    // The callee can never return. The call node and all its uses are dead.
    ReplaceWithValue(call, mcgraph()->Dead(), mcgraph()->Dead(),
                     mcgraph()->Dead());
  }
}

const wasm::WasmModule* WasmInliner::module() const { return env_->module; }

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
