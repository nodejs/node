// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_GRAPH_BUILDER_INTERFACE_H_
#define V8_WASM_GRAPH_BUILDER_INTERFACE_H_

#include "src/wasm/decoder.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

namespace compiler {  // external declarations from compiler.
class NodeOriginTable;
class WasmGraphBuilder;
struct WasmLoopInfo;
}  // namespace compiler

namespace wasm {

struct FunctionBody;
class WasmFeatures;
struct WasmModule;

enum InlinedStatus {
  // Inlined function whose call node has IfSuccess/IfException outputs.
  kInlinedHandledCall,
  // Inlined function whose call node does not have IfSuccess/IfException
  // outputs.
  kInlinedNonHandledCall,
  // Not an inlined call.
  kRegularFunction
};

V8_EXPORT_PRIVATE DecodeResult
BuildTFGraph(AccountingAllocator* allocator, const WasmFeatures& enabled,
             const WasmModule* module, compiler::WasmGraphBuilder* builder,
             WasmFeatures* detected, const FunctionBody& body,
             std::vector<compiler::WasmLoopInfo>* loop_infos,
             compiler::NodeOriginTable* node_origins, int func_index,
             InlinedStatus inlined_status);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_GRAPH_BUILDER_INTERFACE_H_
