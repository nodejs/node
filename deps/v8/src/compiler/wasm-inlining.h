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
class WireBytesStorage;
}  // namespace wasm

class BytecodeOffset;
class OptimizedCompilationInfo;

namespace compiler {

class NodeOriginTable;
class SourcePositionTable;

// The WasmInliner provides the core graph inlining machinery for Webassembly
// graphs. Note that this class only deals with the mechanics of how to inline
// one graph into another, heuristics that decide what and how much to inline
// are beyond its scope. As a current placeholder, only a function at specific
// given index {inlinee_index} is inlined.
class WasmInliner final : public AdvancedReducer {
 public:
  WasmInliner(Editor* editor, wasm::CompilationEnv* env,
              SourcePositionTable* spt, NodeOriginTable* node_origins,
              MachineGraph* mcgraph, const wasm::WireBytesStorage* wire_bytes,
              uint32_t inlinee_index)
      : AdvancedReducer(editor),
        env_(env),
        spt_(spt),
        node_origins_(node_origins),
        mcgraph_(mcgraph),
        wire_bytes_(wire_bytes),
        inlinee_index_(inlinee_index) {}

  const char* reducer_name() const override { return "WasmInliner"; }

  Reduction Reduce(Node* node) final;

 private:
  Zone* zone() const { return mcgraph_->zone(); }
  CommonOperatorBuilder* common() const { return mcgraph_->common(); }
  Graph* graph() const { return mcgraph_->graph(); }
  MachineGraph* mcgraph() const { return mcgraph_; }
  const wasm::WasmModule* module() const;

  Reduction ReduceCall(Node* call);
  Reduction InlineCall(Node* call, Node* callee_start, Node* callee_end);

  wasm::CompilationEnv* const env_;
  SourcePositionTable* const spt_;
  NodeOriginTable* const node_origins_;
  MachineGraph* const mcgraph_;
  const wasm::WireBytesStorage* const wire_bytes_;
  const uint32_t inlinee_index_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_INLINING_H_
