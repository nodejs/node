// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_H_
#define V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_H_

#include "src/base/macros.h"

namespace v8::internal {
class AccountingAllocator;

namespace compiler {
class NodeOriginTable;
namespace turboshaft {
class Graph;
}
}  // namespace compiler

namespace wasm {
struct FunctionBody;
class WasmFeatures;
struct WasmModule;

V8_EXPORT_PRIVATE bool BuildTSGraph(AccountingAllocator* allocator,
                                    const WasmFeatures& enabled,
                                    const WasmModule* module,
                                    WasmFeatures* detected,
                                    const FunctionBody& body,
                                    compiler::turboshaft::Graph& graph,
                                    compiler::NodeOriginTable* node_origins);
}  // namespace wasm
}  // namespace v8::internal

#endif  // V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_H_
