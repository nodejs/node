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

// Parent class for classes that provide heuristics on how to inline in wasm.
class WasmInliningHeuristics {
 public:
  virtual bool DoInline(SourcePosition position,
                        uint32_t function_index) const = 0;
};

// A simple inlining heuristic that inlines all function calls to a set of given
// function indices.
class InlineByIndex : public WasmInliningHeuristics {
 public:
  explicit InlineByIndex(uint32_t function_index)
      : WasmInliningHeuristics(), function_indices_(function_index) {}
  InlineByIndex(std::initializer_list<uint32_t> function_indices)
      : WasmInliningHeuristics(), function_indices_(function_indices) {}

  bool DoInline(SourcePosition position,
                uint32_t function_index) const override {
    return function_indices_.count(function_index) > 0;
  }

 private:
  std::unordered_set<uint32_t> function_indices_;
};

// The WasmInliner provides the core graph inlining machinery for Webassembly
// graphs. Note that this class only deals with the mechanics of how to inline
// one graph into another; heuristics that decide what and how much to inline
// are provided by {WasmInliningHeuristics}.
class WasmInliner final : public AdvancedReducer {
 public:
  WasmInliner(Editor* editor, wasm::CompilationEnv* env,
              SourcePositionTable* source_positions,
              NodeOriginTable* node_origins, MachineGraph* mcgraph,
              const wasm::WireBytesStorage* wire_bytes,
              const WasmInliningHeuristics* heuristics)
      : AdvancedReducer(editor),
        env_(env),
        source_positions_(source_positions),
        node_origins_(node_origins),
        mcgraph_(mcgraph),
        wire_bytes_(wire_bytes),
        heuristics_(heuristics) {}

  const char* reducer_name() const override { return "WasmInliner"; }

  Reduction Reduce(Node* node) final;

 private:
  Zone* zone() const { return mcgraph_->zone(); }
  CommonOperatorBuilder* common() const { return mcgraph_->common(); }
  Graph* graph() const { return mcgraph_->graph(); }
  MachineGraph* mcgraph() const { return mcgraph_; }
  const wasm::WasmModule* module() const;
  const wasm::WasmFunction* inlinee() const;

  Reduction ReduceCall(Node* call);
  Reduction InlineCall(Node* call, Node* callee_start, Node* callee_end,
                       const wasm::FunctionSig* inlinee_sig,
                       size_t subgraph_min_node_id);
  Reduction InlineTailCall(Node* call, Node* callee_start, Node* callee_end);
  void RewireFunctionEntry(Node* call, Node* callee_start);

  wasm::CompilationEnv* const env_;
  SourcePositionTable* const source_positions_;
  NodeOriginTable* const node_origins_;
  MachineGraph* const mcgraph_;
  const wasm::WireBytesStorage* const wire_bytes_;
  const WasmInliningHeuristics* const heuristics_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_INLINING_H_
