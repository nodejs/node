// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-inlining.h"

#include "src/compiler/node-matchers.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction WasmInliner::Reduce(Node* node) {
  if (node->opcode() == IrOpcode::kCall) {
    return ReduceCall(node);
  } else {
    return NoChange();
  }
}

// TODO(12166): Abstract over a heuristics provider.
Reduction WasmInliner::ReduceCall(Node* call) {
  Node* callee = NodeProperties::GetValueInput(call, 0);
  IrOpcode::Value reloc_opcode = mcgraph_->machine()->Is32()
                                     ? IrOpcode::kRelocatableInt32Constant
                                     : IrOpcode::kRelocatableInt64Constant;
  if (callee->opcode() != reloc_opcode) return NoChange();
  auto info = OpParameter<RelocatablePtrConstantInfo>(callee->op());
  if (static_cast<uint32_t>(info.value()) != inlinee_index_) return NoChange();

  CHECK_LT(inlinee_index_, module()->functions.size());
  const wasm::WasmFunction* function = &module()->functions[inlinee_index_];
  base::Vector<const byte> function_bytes =
      wire_bytes_->GetCode(function->code);
  const wasm::FunctionBody inlinee_body(function->sig, function->code.offset(),
                                        function_bytes.begin(),
                                        function_bytes.end());
  wasm::WasmFeatures detected;
  WasmGraphBuilder builder(env_, zone(), mcgraph_, inlinee_body.sig, spt_);
  std::vector<WasmLoopInfo> infos;

  wasm::DecodeResult result;
  Node* inlinee_start;
  Node* inlinee_end;
  {
    Graph::SubgraphScope scope(graph());
    result = wasm::BuildTFGraph(zone()->allocator(), env_->enabled_features,
                                module(), &builder, &detected, inlinee_body,
                                &infos, node_origins_, inlinee_index_,
                                wasm::kDoNotInstrumentEndpoints);
    inlinee_start = graph()->start();
    inlinee_end = graph()->end();
  }

  if (result.failed()) return NoChange();
  return InlineCall(call, inlinee_start, inlinee_end);
}

// TODO(12166): Handle exceptions and tail calls.
Reduction WasmInliner::InlineCall(Node* call, Node* callee_start,
                                  Node* callee_end) {
  DCHECK_EQ(call->opcode(), IrOpcode::kCall);

  /* 1) Rewire callee formal parameters to the call-site real parameters. Rewire
   * effect and control dependencies of callee's start node with the respective
   * inputs of the call node.
   */
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
        break;
    }
  }

  /* 2) Rewire uses of the call node to the return values of the callee. Since
   * there might be multiple return nodes in the callee, we have to create Merge
   * and Phi nodes for them.
   */
  NodeVector return_nodes(zone());
  for (Node* const input : callee_end->inputs()) {
    DCHECK(IrOpcode::IsGraphTerminator(input->opcode()));
    switch (input->opcode()) {
      case IrOpcode::kReturn:
        return_nodes.push_back(input);
        break;
      case IrOpcode::kDeoptimize:
      case IrOpcode::kTerminate:
      case IrOpcode::kThrow:
        NodeProperties::MergeControlToEnd(graph(), common(), input);
        Revisit(graph()->end());
        break;
      case IrOpcode::kTailCall:
        // TODO(12166): A tail call in the inlined function has to be
        // transformed into a regular call in the caller function.
        UNIMPLEMENTED();
      default:
        UNREACHABLE();
    }
  }

  if (return_nodes.size() > 0) {
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
      const wasm::WasmFunction* function = &module()->functions[inlinee_index_];
      MachineRepresentation repr =
          function->sig->GetReturn(i).machine_representation();
      Node* ith_value_output = graph()->NewNode(
          common()->Phi(repr, return_count),
          static_cast<int>(ith_values.size()), &ith_values.front());
      values.push_back(ith_value_output);
    }

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
    return Replace(mcgraph()->Dead());
  } else {
    // The callee can never return. The call node and all its uses are dead.
    ReplaceWithValue(call, mcgraph()->Dead(), mcgraph()->Dead(),
                     mcgraph()->Dead());
    return Changed(call);
  }
}

const wasm::WasmModule* WasmInliner::module() const { return env_->module; }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
