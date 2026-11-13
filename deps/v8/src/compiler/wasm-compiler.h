// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WASM_COMPILER_H_
#define V8_COMPILER_WASM_COMPILER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <memory>
#include <utility>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything else from src/compiler here!
#include "src/codegen/compiler.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/runtime/runtime.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone.h"

namespace v8::internal {
struct AssemblerOptions;

namespace compiler {
// Forward declarations for some compiler data structures.
class MachineGraph;
class Node;
class NodeOriginTable;
class SourcePositionTable;
}  // namespace compiler

namespace wasm {
class WireBytesStorage;
enum Suspend : int;
enum CallOrigin { kCalledFromWasm, kCalledFromJS };
}  // namespace wasm

namespace compiler {

// Compiles an import call wrapper, which allows Wasm to call imports.
V8_EXPORT_PRIVATE wasm::WasmCompilationResult CompileWasmImportCallWrapper(
    wasm::ImportCallKind, const wasm::CanonicalSig*, int expected_arity,
    wasm::Suspend);

V8_EXPORT_PRIVATE wasm::WasmCompilationResult CompileWasmStackEntryWrapper(
    const wasm::CanonicalSig* sig);

// Compiles a host call wrapper, which allows Wasm to call host functions.
wasm::WasmCompilationResult CompileWasmCapiCallWrapper(
    const wasm::CanonicalSig*);

// Compiles a wrapper to call a Fast API function from Wasm.
wasm::WasmCompilationResult CompileWasmJSFastCallWrapper(
    const wasm::CanonicalSig*, DirectHandle<JSReceiver> callable);

// Returns a TurboshaftCompilationJob object for a JS to Wasm wrapper.
std::unique_ptr<OptimizedCompilationJob> NewJSToWasmCompilationJob(
    Isolate* isolate, const wasm::CanonicalSig* sig);

// Compiles a stub with C++ linkage, to be called from Execution::CallWasm,
// which knows how to feed it its parameters.
V8_EXPORT_PRIVATE Handle<Code> CompileCWasmEntry(Isolate*,
                                                 const wasm::CanonicalSig*);

constexpr MachineType kCWasmEntrySigTypes[] = {
    MachineType::Pointer(),    // return
    MachineType::Uint32(),     // target
    MachineType::AnyTagged(),  // object_ref
    MachineType::Pointer(),    // argv
    MachineType::Pointer()};   // c_entry_fp

constexpr MachineSignature kCWasmEntrySig(1, 4, kCWasmEntrySigTypes);

constexpr const MachineSignature* CWasmEntrySignature() {
  return &kCWasmEntrySig;
}

struct WasmCompilationData {
  explicit WasmCompilationData(const wasm::FunctionBody& func_body)
      : func_body(func_body) {}

  size_t body_size() { return func_body.end - func_body.start; }

  const wasm::FunctionBody& func_body;
  const wasm::WireBytesStorage* wire_bytes_storage;
  NodeOriginTable* node_origins{nullptr};
  std::unique_ptr<wasm::AssumptionsJournal> assumptions{};
  SourcePositionTable* source_positions{nullptr};
  int func_index;
  wasm::Validation validate_callees{wasm::kAlreadyValidated};
};

bool TryWasmInlining(int fct_index, wasm::NativeModule* native_module,
                     int inlining_id, MachineGraph* mcgraph,
                     SourcePositionTable* spt);

void BuildInlinedJSToWasmWrapper(Zone* zone, MachineGraph* mcgraph,
                                 const wasm::CanonicalSig* signature,
                                 Isolate* isolate, Node* frame_state);

AssemblerOptions WasmAssemblerOptions();
AssemblerOptions WasmStubAssemblerOptions();

template <typename T>
Signature<MachineRepresentation>* CreateMachineSignature(
    Zone* zone, const Signature<T>* sig, wasm::CallOrigin origin);

}  // namespace compiler
}  // namespace v8::internal

#endif  // V8_COMPILER_WASM_COMPILER_H_
