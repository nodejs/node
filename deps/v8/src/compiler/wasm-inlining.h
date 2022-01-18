// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_INLINING_H_
#define V8_COMPILER_WASM_INLINING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"

namespace v8 {
namespace internal {

namespace wasm {
struct CompilationEnv;
struct WasmModule;
struct WasmFunction;
class WireBytesStorage;
}  // namespace wasm

class BytecodeOffset;
class OptimizedCompilationInfo;

namespace compiler {

class NodeOriginTable;
class SourcePositionTable;

// The WasmInliner provides the core graph inlining machinery for Webassembly
// graphs. Note that this class only deals with the mechanics of how to inline
// one graph into another; heuristics that decide what and how much to inline
// are provided by {WasmInliningHeuristics}.
class WasmInliner final : public AdvancedReducer {
 public:
  WasmInliner(Editor* editor, wasm::CompilationEnv* env,
              uint32_t function_index, SourcePositionTable* source_positions,
              NodeOriginTable* node_origins, MachineGraph* mcgraph,
              const wasm::WireBytesStorage* wire_bytes)
      : AdvancedReducer(editor),
        env_(env),
        function_index_(function_index),
        source_positions_(source_positions),
        node_origins_(node_origins),
        mcgraph_(mcgraph),
        wire_bytes_(wire_bytes),
        initial_graph_size_(mcgraph->graph()->NodeCount()),
        current_graph_size_(initial_graph_size_),
        inlining_candidates_() {}

  const char* reducer_name() const override { return "WasmInliner"; }

  Reduction Reduce(Node* node) final;
  void Finalize() final;

  static bool any_inlining_impossible(size_t initial_graph_size) {
    return size_limit(initial_graph_size) - initial_graph_size <
           kMinimumFunctionNodeCount;
  }

 private:
  struct CandidateInfo {
    Node* node;
    uint32_t inlinee_index;
    bool is_speculative_call_ref;
    int call_count;
    int wire_byte_size;
  };

  struct LexicographicOrdering {
    // Returns if c1 should be prioritized less than c2.
    bool operator()(CandidateInfo& c1, CandidateInfo& c2) {
      if (c1.is_speculative_call_ref && !c2.is_speculative_call_ref) {
        return false;
      }
      if (c2.is_speculative_call_ref && !c1.is_speculative_call_ref) {
        return true;
      }
      if (c1.call_count > c2.call_count) return false;
      if (c2.call_count > c1.call_count) return true;
      return c1.wire_byte_size > c2.wire_byte_size;
    }
  };

  // TODO(manoskouk): This has not been found to be useful, but something
  // similar may be tried again in the future.
  // struct AdvancedOrdering {
  //  // Returns if c1 should be prioritized less than c2.
  //  bool operator()(CandidateInfo& c1, CandidateInfo& c2) {
  //    if (c1.is_speculative_call_ref && c2.is_speculative_call_ref) {
  //      if (c1.call_count > c2.call_count) return false;
  //      if (c2.call_count > c1.call_count) return true;
  //      return c1.wire_byte_size > c2.wire_byte_size;
  //    }
  //    if (!c1.is_speculative_call_ref && !c2.is_speculative_call_ref) {
  //      return c1.wire_byte_size > c2.wire_byte_size;
  //    }
  //
  //    constexpr int kAssumedCallCountForDirectCalls = 3;
  //
  //    int c1_call_count = c1.is_speculative_call_ref
  //                            ? c1.call_count
  //                            : kAssumedCallCountForDirectCalls;
  //    int c2_call_count = c2.is_speculative_call_ref
  //                            ? c2.call_count
  //                            : kAssumedCallCountForDirectCalls;
  //
  //    return static_cast<float>(c1_call_count) / c1.wire_byte_size <
  //           static_cast<float>(c2_call_count) / c2.wire_byte_size;
  //  }
  //};

  Zone* zone() const { return mcgraph_->zone(); }
  CommonOperatorBuilder* common() const { return mcgraph_->common(); }
  Graph* graph() const { return mcgraph_->graph(); }
  MachineGraph* mcgraph() const { return mcgraph_; }
  const wasm::WasmModule* module() const;

  // A limit to the size of the inlined graph as a function of its initial size.
  static size_t size_limit(size_t initial_graph_size) {
    return initial_graph_size +
           std::min(FLAG_wasm_inlining_max_size,
                    FLAG_wasm_inlining_budget_factor / initial_graph_size);
  }

  // The smallest size in TF nodes any meaningful wasm function can have
  // (start, instance parameter, end).
  static constexpr size_t kMinimumFunctionNodeCount = 3;

  Reduction ReduceCall(Node* call);
  void InlineCall(Node* call, Node* callee_start, Node* callee_end,
                  const wasm::FunctionSig* inlinee_sig,
                  size_t subgraph_min_node_id);
  void InlineTailCall(Node* call, Node* callee_start, Node* callee_end);
  void RewireFunctionEntry(Node* call, Node* callee_start);

  wasm::CompilationEnv* const env_;
  uint32_t function_index_;
  SourcePositionTable* const source_positions_;
  NodeOriginTable* const node_origins_;
  MachineGraph* const mcgraph_;
  const wasm::WireBytesStorage* const wire_bytes_;
  const size_t initial_graph_size_;
  size_t current_graph_size_;
  std::priority_queue<CandidateInfo, std::vector<CandidateInfo>,
                      LexicographicOrdering>
      inlining_candidates_;
  std::unordered_set<Node*> seen_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_INLINING_H_
