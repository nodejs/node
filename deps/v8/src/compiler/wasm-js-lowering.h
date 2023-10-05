// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_JS_LOWERING_H_
#define V8_COMPILER_WASM_JS_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/compiler/wasm-graph-assembler.h"

namespace v8::internal::compiler {

class SourcePositionTable;

// This reducer is part of the JavaScript pipeline and contains lowering of
// wasm nodes (from inlined wasm functions).
//
// The reducer replaces all TrapIf / TrapUnless nodes with a conditional goto to
// deferred code containing a call to the trap builtin.
class WasmJSLowering final : public AdvancedReducer {
 public:
  WasmJSLowering(Editor* editor, MachineGraph* mcgraph,
                 SourcePositionTable* source_position_table);

  const char* reducer_name() const override { return "WasmJSLowering"; }
  Reduction Reduce(Node* node) final;

 private:
  WasmGraphAssembler gasm_;
  const MachineGraph* mcgraph_;
  SourcePositionTable* source_position_table_;
};

}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_WASM_JS_LOWERING_H_
