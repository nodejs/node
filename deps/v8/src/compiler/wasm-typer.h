// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WASM_TYPER_H_
#define V8_COMPILER_WASM_TYPER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/compiler/graph-reducer.h"
#include "src/compiler/wasm-graph-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

class MachineGraph;

// Recomputes wasm-gc types along the graph to assign the narrowest possible
// type to each node.
// Specifically, struct field accesses, array element accesses, phis, type
// casts, and type guards are retyped.
// Types in loops are computed to a fixed point.
class WasmTyper final : public AdvancedReducer {
 public:
  WasmTyper(Editor* editor, MachineGraph* mcgraph, uint32_t function_index);

  const char* reducer_name() const override { return "WasmTyper"; }

  Reduction Reduce(Node* node) final;

 private:
  uint32_t function_index_;
  Zone* graph_zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_TYPER_H_
