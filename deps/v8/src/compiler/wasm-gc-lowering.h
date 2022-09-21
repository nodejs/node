// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_GC_LOWERING_H_
#define V8_COMPILER_WASM_GC_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/wasm-graph-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

class MachineGraph;
class WasmGraphAssembler;

class WasmGCLowering final : public AdvancedReducer {
 public:
  WasmGCLowering(Editor* editor, MachineGraph* mcgraph);

  const char* reducer_name() const override { return "WasmGCLowering"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceWasmTypeCheck(Node* node);
  Reduction ReduceWasmTypeCast(Node* node);
  Reduction ReduceAssertNotNull(Node* node);
  Reduction ReduceNull(Node* node);
  Reduction ReduceIsNull(Node* node);
  Reduction ReduceIsNotNull(Node* node);
  Reduction ReduceRttCanon(Node* node);
  Reduction ReduceTypeGuard(Node* node);
  Reduction ReduceWasmExternInternalize(Node* node);
  Reduction ReduceWasmExternExternalize(Node* node);
  Node* RootNode(RootIndex index);
  Node* Null();
  WasmGraphAssembler gasm_;
  Node* dead_;
  Node* instance_node_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_GC_LOWERING_H_
