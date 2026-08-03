// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WASM_INLINING_INTO_JS_H_
#define V8_COMPILER_WASM_INLINING_INTO_JS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/base/vector.h"
#include "src/common/globals.h"

namespace v8::internal {
class Zone;

namespace wasm {
struct FunctionBody;
struct WasmModule;
}  // namespace wasm

namespace compiler {
class MachineGraph;
class Node;
class SourcePositionTable;

// The WasmIntoJsInliner provides support for inlining very small wasm functions
// which only contain very specific supported instructions into JS.
class WasmIntoJSInliner {
 public:
  static bool TryInlining(Zone* zone, const wasm::WasmModule* module,
                          MachineGraph* mcgraph, const wasm::FunctionBody& body,
                          base::Vector<const uint8_t> bytes,
                          SourcePositionTable* source_position_table,
                          int inlining_id);
};

}  // namespace compiler
}  // namespace v8::internal

#endif  // V8_COMPILER_WASM_INLINING_INTO_JS_H_
