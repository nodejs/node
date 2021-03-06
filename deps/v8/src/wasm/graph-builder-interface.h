// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_GRAPH_BUILDER_INTERFACE_H_
#define V8_WASM_GRAPH_BUILDER_INTERFACE_H_

#include "src/wasm/decoder.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

namespace compiler {  // external declarations from compiler.
class NodeOriginTable;
class WasmGraphBuilder;
}  // namespace compiler

namespace wasm {

struct FunctionBody;
class WasmFeatures;
struct WasmModule;

V8_EXPORT_PRIVATE DecodeResult
BuildTFGraph(AccountingAllocator* allocator, const WasmFeatures& enabled,
             const WasmModule* module, compiler::WasmGraphBuilder* builder,
             WasmFeatures* detected, const FunctionBody& body,
             compiler::NodeOriginTable* node_origins);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_GRAPH_BUILDER_INTERFACE_H_
