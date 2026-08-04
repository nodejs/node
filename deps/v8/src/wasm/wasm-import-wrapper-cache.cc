// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-import-wrapper-cache.h"

#include <vector>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/flush-instruction-cache.h"
#include "src/common/code-memory-access-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/std-object-sizes.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-code-pointer-table-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-wrapper-cache-inl.h"

namespace v8::internal::wasm {

WasmWrapperHandle::WasmWrapperHandle(WasmCode* code, Address addr,
                                     uint64_t signature_hash)
    : code_pointer_(
          GetProcessWideWasmCodePointerTable()->AllocateAndInitializeEntry(
              addr, signature_hash)),
      code_(code) {
  if (code) code->IncRef();
}

WasmWrapperHandle::WasmWrapperHandle(Address addr, uint64_t signature_hash)
    : code_pointer_(
          GetProcessWideWasmCodePointerTable()->AllocateAndInitializeEntry(
              addr, signature_hash)) {}

WasmWrapperHandle::~WasmWrapperHandle() {
  WasmCode* wasm_code = code_.load(std::memory_order_acquire);
  if (wasm_code) {
    bool should_free = wasm_code->DecRef();
    USE(should_free);
    DCHECK(!should_free);
  }
  GetProcessWideWasmCodePointerTable()->FreeEntry(code_pointer_);
}

std::optional<Builtin> WasmImportWrapperCache::BuiltinForWrapper(
    const CacheKey& key) {
  if (key.kind != ImportCallKind::kJSFunction) return {};

  if (!IsJSCompatibleSignature(key.sig)) {
    return Builtin::kWasmToJsWrapperInvalidSig;
  }

  if (UseGenericWasmToJSWrapper(key.kind, key.sig, key.suspend)) {
    return Builtin::kWasmToJsWrapperAsm;
  }

  return {};
}

std::shared_ptr<WasmWrapperHandle> WasmImportWrapperCache::Get(
    Isolate* isolate, const CacheKey& cache_key) {
  {
    base::MutexGuard lock(&mutex_);

    // If we have an entry already, just return that.
    auto it = entry_map_.find(cache_key);
    if (it != entry_map_.end()) {
      std::shared_ptr<WasmWrapperHandle> handle = it->second.lock();
      if (handle) {
        return handle;
      }
    }

    // Check if we should use a builtin.
    std::optional<Builtin> builtin = BuiltinForWrapper(cache_key);
    if (builtin) {
      Address call_target = Builtins::EmbeddedEntryOf(*builtin);
      static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
      std::shared_ptr<WasmWrapperHandle> wrapper_handle =
          std::make_shared<WasmWrapperHandle>(call_target,
                                              cache_key.sig->signature_hash());
      entry_map_[cache_key] = wrapper_handle;
      return wrapper_handle;
    }
  }

  // Fall back to compile a new wrapper.
  auto [result, code_kind] = CompileWrapper(isolate, cache_key);
  return CacheCompiledWrapper(isolate, std::move(result), code_kind, cache_key,
                              nullptr);
}

#ifdef DEBUG
bool WasmImportWrapperCache::IsCompiledWrapper(WasmCodePointer code_pointer) {
  base::MutexGuard lock(&mutex_);
  for (auto& [cache_key, weak_handle] : entry_map_) {
    std::shared_ptr<WasmWrapperHandle> handle = weak_handle.lock();
    if (!handle) continue;

    if (handle->code_pointer() == code_pointer) {
      return handle->has_code();
    }
  }
  UNREACHABLE();
}
#endif

bool WasmImportWrapperCache::HasCodeForTesting(ImportCallKind kind,
                                               const CanonicalSig* sig,
                                               int expected_arity,
                                               Suspend suspend) {
  base::MutexGuard lock(&mutex_);

  CacheKey cache_key(kind, sig, expected_arity, suspend);
  auto it = entry_map_.find(cache_key);
  if (it == entry_map_.end()) return false;
  auto handle = it->second.lock();
  if (!handle) return false;
  return handle->has_code();
}

std::pair<WasmCompilationResult, WasmCode::Kind>
WasmImportWrapperCache::CompileWrapper(Isolate* isolate,
                                       const CacheKey& cache_key) {
#if !V8_ENABLE_TURBOFAN
  UNREACHABLE();
#else
  wasm::WasmCompilationResult result;
  wasm::WasmCode::Kind code_kind;
  switch (cache_key.kind) {
    case ImportCallKind::kLinkError:
    case ImportCallKind::kWasmToWasm:
    case ImportCallKind::kWasmToJSFastApi:
      UNREACHABLE();
    case ImportCallKind::kJSFunction:
    case ImportCallKind::kUseCallBuiltin:
    case ImportCallKind::kRuntimeTypeError:
      result = compiler::CompileWasmImportCallWrapper(
          cache_key.kind, cache_key.sig, cache_key.expected_arity,
          cache_key.suspend);
      code_kind = WasmCode::Kind::kWasmToJsWrapper;
      break;
    case ImportCallKind::kWasmToCapi:
      result = compiler::CompileWasmCapiCallWrapper(cache_key.sig);
      code_kind = wasm::WasmCode::Kind::kWasmToCapiWrapper;
      break;
  }
  return {std::move(result), code_kind};
#endif
}

#ifdef V8_ENABLE_TURBOFAN
std::shared_ptr<WasmWrapperHandle>
WasmImportWrapperCache::CompileWasmJsFastCallWrapper(
    Isolate* isolate, DirectHandle<JSReceiver> callable,
    const wasm::CanonicalSig* sig) {
  // Note: the wrapper we're about to compile is specific to this
  // instantiation, so it cannot be shared.
  WasmCompilationResult result =
      compiler::CompileWasmJSFastCallWrapper(sig, callable);

  std::shared_ptr<WasmWrapperHandle> wrapper_handle =
      std::make_shared<WasmWrapperHandle>(kNullAddress, sig->signature_hash());
  WasmCode* wasm_code;
  {
    ModificationScope cache_scope(this);
    wasm_code = cache_scope.AddWrapper(std::move(result),
                                       WasmCode::Kind::kWasmToJsWrapper,
                                       sig->signature_hash(), wrapper_handle);
  }
  // To avoid lock order inversion, code printing must happen after the
  // end of the {cache_scope}.
  wasm_code->MaybePrint();
  isolate->counters()->wasm_generated_code_size()->Increment(
      wasm_code->instructions().length());
  isolate->counters()->wasm_reloc_size()->Increment(
      wasm_code->reloc_info().length());
  if (GetWasmEngine()->LogWrapperCode(wasm_code)) {
    // Log the code immediately in the current isolate.
    GetWasmEngine()->LogOutstandingCodesForIsolate(isolate);
  }

  return wrapper_handle;
}
#endif

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    WasmWrapperCache<ImportWrapperCacheKey>;

}  // namespace v8::internal::wasm
