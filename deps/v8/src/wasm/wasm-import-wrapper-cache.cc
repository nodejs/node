// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-import-wrapper-cache.h"

#include <vector>

#include "src/counters.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

WasmImportWrapperCache::~WasmImportWrapperCache() {
  std::vector<WasmCode*> ptrs;
  ptrs.reserve(entry_map_.size());
  for (auto& e : entry_map_) ptrs.push_back(e.second);
  WasmCode::DecrementRefCount(VectorOf(ptrs));
}

WasmCode* WasmImportWrapperCache::GetOrCompile(
    WasmEngine* wasm_engine, Counters* counters,
    compiler::WasmImportCallKind kind, FunctionSig* sig) {
  base::MutexGuard lock(&mutex_);
  CacheKey key(static_cast<uint8_t>(kind), *sig);
  WasmCode*& cached = entry_map_[key];
  if (cached == nullptr) {
    // TODO(wasm): no need to hold the lock while compiling an import wrapper.
    bool source_positions = native_module_->module()->origin == kAsmJsOrigin;
    // Keep the {WasmCode} alive until we explicitly call {IncRef}.
    WasmCodeRefScope code_ref_scope;
    cached = compiler::CompileWasmImportCallWrapper(
        wasm_engine, native_module_, kind, sig, source_positions);
    cached->IncRef();
    counters->wasm_generated_code_size()->Increment(
        cached->instructions().length());
    counters->wasm_reloc_size()->Increment(cached->reloc_info().length());
  }
  return cached;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
