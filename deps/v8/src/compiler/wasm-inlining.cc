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
  if (v8_flags.trace_wasm_inlining) PrintF(__VA_ARGS__)

void WasmInliner::Trace(Node* call, int inlinee, const char* decision) {
  TRACE("[function %d: considering node %d, call to %d: %s]\n", function_index_,
        call->id(), inlinee, decision);
}

int WasmInliner::GetCallCount(Node* call) {
  if (!v8_flags.wasm_speculative_inlining) return 0;
  return mcgraph()->GetCallCount(call->id());
}

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
  if (info.rmode() != RelocInfo::WASM_CALL) {
    Trace(call, inlinee_index, "not a wasm call");
    return NoChange();
  }
  if (inlinee_index < module()->num_imported_functions) {
    Trace(call, inlinee_index, "imported function");
    return NoChange();
  }

  // We limit the times a function can be inlined to avoid repeatedly inlining
  // recursive calls. Since we only check here (and not in {Finalize}), it is
  // possible to exceed this limit if we find a large number of calls in a
  // single pass.
  constexpr int kMaximumInlinedCallsPerFunction = 3;
  if (function_inlining_count_[inlinee_index] >=
      kMaximumInlinedCallsPerFunction) {
    Trace(call, inlinee_index,
          "too many inlined calls to (recursive?) function");
    return NoChange();
  }

  CHECK_LT(inlinee_index, module()->functions.size());
  const wasm::WasmFunction* inlinee = &module()->functions[inlinee_index];
  base::Vector<const byte> function_bytes = wire_bytes_->GetCode(inlinee->code);

  int call_count = GetCallCount(call);

  int wire_byte_size = static_cast<int>(function_bytes.size());
  int min_count_for_inlining = wire_byte_size / 2;

  // If liftoff ran and collected call counts, only inline calls that have been
  // invoked often, except for truly tiny functions.
  if (v8_flags.liftoff && v8_flags.wasm_speculative_inlining &&
      wire_byte_size >= 12 && call_count < min_count_for_inlining) {
    Trace(call, inlinee_index, "not called often enough");
    return NoChange();
  }

  Trace(call, inlinee_index, "adding to inlining candidates!");

  CandidateInfo candidate{call, inlinee_index, call_count,
                          function_bytes.length()};

  inlining_candidates_.push(candidate);
  return NoChange();
}

bool SmallEnoughToInline(size_t current_graph_size, uint32_t candidate_size) {
  if (WasmInliner::graph_size_allows_inlining(current_graph_size +
                                              candidate_size)) {
    return true;
  }
  // For truly tiny functions, let's be a bit more generous.
  return candidate_size <= 12 &&
         WasmInliner::graph_size_allows_inlining(current_graph_size - 100);
}

void WasmInliner::Trace(const CandidateInfo& candidate, const char* decision) {
  TRACE(
      "  [function %d: considering candidate {@%d, index=%d, count=%d, "
      "size=%d}: %s]\n",
      function_index_, candidate.node->id(), candidate.inlinee_index,
      candidate.call_count, candidate.wire_byte_size, decision);
}

void WasmInliner::Finalize() {
  TRACE("function %d %s: going though inlining candidates...\n",
        function_index_, debug_name_);
  if (inlining_candidates_.empty()) return;
  while (!inlining_candidates_.empty()) {
    CandidateInfo candidate = inlining_candidates_.top();
    inlining_candidates_.pop();
    Node* call = candidate.node;
    if (call->IsDead()) {
      Trace(candidate, "dead node");
      continue;
    }
    // We could build the candidate's graph first and consider its node count,
    // but it turns out that wire byte size and node count are quite strongly
    // correlated, at about 1.16 nodes per wire byte (measured for J2Wasm).
    if (!SmallEnoughToInline(current_graph_size_, candidate.wire_byte_size)) {
      Trace(candidate, "not enough inlining budget");
      continue;
    }
    const wasm::WasmFunction* inlinee =
        &module()->functions[candidate.inlinee_index];

    const wasm::FunctionSig* lowered_sig =
        mcgraph_->machine()->Is64() ? inlinee->sig
                                    : GetI32Sig(zone(), inlinee->sig);

    DCHECK_EQ(lowered_sig->parameter_count(),
              call->op()->ValueInputCount() - 2);
#if DEBUG
    // The two first parameters in the call are the function and instance, and
    // then come the wasm function parameters.
    for (uint32_t i = 0; i < lowered_sig->parameter_count(); i++) {
      if (!NodeProperties::IsTyped(call->InputAt(i + 2))) continue;
      wasm::TypeInModule param_type =
          NodeProperties::GetType(call->InputAt(i + 2)).AsWasm();
      CHECK(IsSubtypeOf(param_type.type, lowered_sig->GetParam(i),
                        param_type.module, module()));
    }
#endif

    base::Vector<const byte> function_bytes =
        wire_bytes_->GetCode(inlinee->code);

    wasm::WasmFeatures detected;
    std::vector<WasmLoopInfo> inlinee_loop_infos;

    size_t subgraph_min_node_id = graph()->NodeCount();
    Node* inlinee_start;
    Node* inlinee_end;
    const wasm::FunctionBody inlinee_body(inlinee->sig, inlinee->code.offset(),
                                          function_bytes.begin(),
                                          function_bytes.end());
    WasmGraphBuilder builder(env_, zone(), mcgraph_, inlinee_body.sig,
                             source_positions_);
    {
      Graph::SubgraphScope scope(graph());
      wasm::DecodeResult result = wasm::BuildTFGraph(
          zone()->allocator(), env_->enabled_features, module(), &builder,
          &detected, inlinee_body, &inlinee_loop_infos, node_origins_,
          candidate.inlinee_index,
          NodeProperties::IsExceptionalCall(call)
              ? wasm::kInlinedHandledCall
              : wasm::kInlinedNonHandledCall);
      if (result.ok()) {
        builder.LowerInt64(WasmGraphBuilder::kCalledFromWasm);
        inlinee_start = graph()->start();
        inlinee_end = graph()->end();
      } else {
        // Otherwise report failure.
        Trace(candidate, "failed to compile");
        return;
      }
    }

    size_t additional_nodes = graph()->NodeCount() - subgraph_min_node_id;
    Trace(candidate, "inlining!");
    current_graph_size_ += additional_nodes;
    DCHECK_GE(function_inlining_count_[candidate.inlinee_index], 0);
    function_inlining_count_[candidate.inlinee_index]++;

    if (call->opcode() == IrOpcode::kCall) {
      InlineCall(call, inlinee_start, inlinee_end, lowered_sig,
                 subgraph_min_node_id);
    } else {
      InlineTailCall(call, inlinee_start, inlinee_end);
    }
    call->Kill();
    loop_infos_->insert(loop_infos_->end(), inlinee_loop_infos.begin(),
                        inlinee_loop_infos.end());
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
          // Projections pointing to the inlinee start are floating control.
          // They should point to the graph's start.
          edge.UpdateTo(use->opcode() == IrOpcode::kProjection
                            ? graph()->start()
                            : control);
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
  DCHECK_EQ(call->opcode(), IrOpcode::kTailCall);
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

namespace {
// graph-builder-interface generates a dangling exception handler for each
// throwing call in the inlinee. This might be followed by a LoopExit node.
Node* DanglingHandler(Node* call) {
  Node* if_exception = nullptr;
  for (Node* use : call->uses()) {
    if (use->opcode() == IrOpcode::kIfException) {
      if_exception = use;
      break;
    }
  }
  DCHECK_NOT_NULL(if_exception);

  // If this handler is dangling, return it.
  if (if_exception->UseCount() == 0) return if_exception;

  for (Node* use : if_exception->uses()) {
    // Otherwise, look for a LoopExit use of this handler.
    if (use->opcode() == IrOpcode::kLoopExit) {
      for (Node* loop_exit_use : use->uses()) {
        if (loop_exit_use->opcode() != IrOpcode::kLoopExitEffect &&
            loop_exit_use->opcode() != IrOpcode::kLoopExitValue) {
          // This LoopExit has a use other than LoopExitEffect/Value, so it is
          // not dangling.
          return nullptr;
        }
      }
      return use;
    }
  }

  return nullptr;
}
}  // namespace

void WasmInliner::InlineCall(Node* call, Node* callee_start, Node* callee_end,
                             const wasm::FunctionSig* inlinee_sig,
                             size_t subgraph_min_node_id) {
  DCHECK_EQ(call->opcode(), IrOpcode::kCall);

  // 0) Before doing anything, if {call} has an exception handler, collect all
  // unhandled calls in the subgraph.
  Node* handler = nullptr;
  std::vector<Node*> dangling_handlers;
  if (NodeProperties::IsExceptionalCall(call, &handler)) {
    AllNodes subgraph_nodes(zone(), callee_end, graph());
    for (Node* node : subgraph_nodes.reachable) {
      if (node->id() >= subgraph_min_node_id &&
          !node->op()->HasProperty(Operator::kNoThrow)) {
        Node* dangling_handler = DanglingHandler(node);
        if (dangling_handler != nullptr) {
          dangling_handlers.push_back(dangling_handler);
        }
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
  int handler_count = static_cast<int>(dangling_handlers.size());

  if (handler_count > 0) {
    Node* control_output =
        graph()->NewNode(common()->Merge(handler_count), handler_count,
                         dangling_handlers.data());
    std::vector<Node*> effects;
    std::vector<Node*> values;
    for (Node* control : dangling_handlers) {
      if (control->opcode() == IrOpcode::kIfException) {
        effects.push_back(control);
        values.push_back(control);
      } else {
        DCHECK_EQ(control->opcode(), IrOpcode::kLoopExit);
        Node* if_exception = control->InputAt(0);
        DCHECK_EQ(if_exception->opcode(), IrOpcode::kIfException);
        effects.push_back(graph()->NewNode(common()->LoopExitEffect(),
                                           if_exception, control));
        values.push_back(graph()->NewNode(
            common()->LoopExitValue(MachineRepresentation::kTagged),
            if_exception, control));
      }
    }

    effects.push_back(control_output);
    values.push_back(control_output);
    Node* value_output = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, handler_count),
        handler_count + 1, values.data());
    Node* effect_output = graph()->NewNode(common()->EffectPhi(handler_count),
                                           handler_count + 1, effects.data());
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
#if DEBUG
    for (Node* const return_node : return_nodes) {
      // 3 = effect, control, first 0 return value.
      CHECK_EQ(return_arity, return_node->InputCount() - 3);
    }
#endif
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
          // Other nodes are unreachable leftovers from Int32Lowering.
          if (use->opcode() == IrOpcode::kProjection) {
            ReplaceWithValue(use, values[ProjectionIndexOf(use->op())]);
          } else {
            DCHECK(mcgraph()->machine()->Is32());
          }
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
