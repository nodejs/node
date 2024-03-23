// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_H_
#define V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_H_

#include "src/base/macros.h"
#include "src/zone/zone-containers.h"

namespace v8::internal {
class AccountingAllocator;
struct WasmInliningPosition;

namespace compiler {
class NodeOriginTable;
namespace turboshaft {
class Graph;
}
}  // namespace compiler

namespace wasm {
class AssumptionsJournal;
struct FunctionBody;
class WasmFeatures;
struct WasmModule;
class WireBytesStorage;

V8_EXPORT_PRIVATE bool BuildTSGraph(
    AccountingAllocator* allocator, WasmFeatures enabled,
    const WasmModule* module, WasmFeatures* detected,
    compiler::turboshaft::Graph& graph, const FunctionBody& func_body,
    const WireBytesStorage* wire_bytes, AssumptionsJournal* assumptions,
    ZoneVector<WasmInliningPosition>* inlining_positions, int func_index);
}  // namespace wasm
}  // namespace v8::internal

#endif  // V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_H_
