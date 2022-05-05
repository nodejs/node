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
struct WasmLoopInfo;

// The WasmInliner provides the core graph inlining machinery for Webassembly
// graphs.
class WasmInliner final : public AdvancedReducer {
 public:
  WasmInliner(Editor* editor, wasm::CompilationEnv* env,
              uint32_t function_index, SourcePositionTable* source_positions,
              NodeOriginTable* node_origins, MachineGraph* mcgraph,
              const wasm::WireBytesStorage* wire_bytes,
              std::vector<WasmLoopInfo>* loop_infos, const char* debug_name)
      : AdvancedReducer(editor),
        env_(env),
        function_index_(function_index),
        source_positions_(source_positions),
        node_origins_(node_origins),
        mcgraph_(mcgraph),
        wire_bytes_(wire_bytes),
        loop_infos_(loop_infos),
        debug_name_(debug_name),
        initial_graph_size_(mcgraph->graph()->NodeCount()),
        current_graph_size_(initial_graph_size_),
        inlining_candidates_() {}

  const char* reducer_name() const override { return "WasmInliner"; }

  Reduction Reduce(Node* node) final;
  void Finalize() final;

  static bool graph_size_allows_inlining(size_t initial_graph_size) {
    return initial_graph_size < 5000;
  }

 private:
  struct CandidateInfo {
    Node* node;
    uint32_t inlinee_index;
    int call_count;
    int wire_byte_size;
  };

  struct LexicographicOrdering {
    // Returns if c1 should be prioritized less than c2.
    bool operator()(CandidateInfo& c1, CandidateInfo& c2) {
      if (c1.call_count > c2.call_count) return false;
      if (c2.call_count > c1.call_count) return true;
      return c1.wire_byte_size > c2.wire_byte_size;
    }
  };

  uint32_t FindOriginatingFunction(Node* call);

  Zone* zone() const { return mcgraph_->zone(); }
  CommonOperatorBuilder* common() const { return mcgraph_->common(); }
  Graph* graph() const { return mcgraph_->graph(); }
  MachineGraph* mcgraph() const { return mcgraph_; }
  const wasm::WasmModule* module() const;

  Reduction ReduceCall(Node* call);
  void InlineCall(Node* call, Node* callee_start, Node* callee_end,
                  const wasm::FunctionSig* inlinee_sig,
                  size_t subgraph_min_node_id);
  void InlineTailCall(Node* call, Node* callee_start, Node* callee_end);
  void RewireFunctionEntry(Node* call, Node* callee_start);

  int GetCallCount(Node* call);

  void Trace(Node* call, int inlinee, const char* decision);
  void Trace(const CandidateInfo& candidate, const char* decision);

  wasm::CompilationEnv* const env_;
  uint32_t function_index_;
  SourcePositionTable* const source_positions_;
  NodeOriginTable* const node_origins_;
  MachineGraph* const mcgraph_;
  const wasm::WireBytesStorage* const wire_bytes_;
  std::vector<WasmLoopInfo>* const loop_infos_;
  const char* debug_name_;
  const size_t initial_graph_size_;
  size_t current_graph_size_;
  std::priority_queue<CandidateInfo, std::vector<CandidateInfo>,
                      LexicographicOrdering>
      inlining_candidates_;
  std::unordered_set<Node*> seen_;
  std::vector<uint32_t> inlined_functions_;
  // Stores the graph size before an inlining was performed, to make it
  // possible to map back from nodes to the function they came from.
  // Guaranteed to have the same length as {inlined_functions_}.
  std::vector<uint32_t> first_node_id_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_INLINING_H_
