// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_STACK_WRAPPER_CACHE_H_
#define V8_WASM_WASM_STACK_WRAPPER_CACHE_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/wasm/wasm-wrapper-cache.h"

namespace v8::internal::wasm {

struct StackEntryWrapperCacheKey {
  explicit StackEntryWrapperCacheKey(const CanonicalSig* sig) : sig(sig) {}

  bool operator==(const StackEntryWrapperCacheKey& rhs) const = default;

  const CanonicalSig* sig;

  class Hash {
   public:
    size_t operator()(const StackEntryWrapperCacheKey& key) const {
#if V8_HASHES_COLLIDE
      if (v8_flags.hashes_collide) return base::kCollidingHash;
#endif  // V8_HASHES_COLLIDE
      return key.sig->index().index;
    }
  };
};

class WasmStackEntryWrapperCache
    : public WasmWrapperCache<StackEntryWrapperCacheKey> {
 protected:
  using CacheKey = StackEntryWrapperCacheKey;

  std::pair<WasmCompilationResult, WasmCode::Kind> CompileWrapper(
      Isolate* isolate, const CacheKey& cache_key) override;
};

// Override WasmWrapperCache::EstimateCurrentMemoryConsumption if the derived
// class size diverges.
static_assert(sizeof(WasmWrapperCache<StackEntryWrapperCacheKey>) ==
              sizeof(WasmStackEntryWrapperCache));

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    WasmWrapperCache<StackEntryWrapperCacheKey>;

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_STACK_WRAPPER_CACHE_H_
