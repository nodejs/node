// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
#define V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_

#include "src/base/platform/mutex.h"
#include "src/compiler/wasm-compiler.h"

namespace v8 {
namespace internal {

class Counters;

namespace wasm {

class WasmCode;
class WasmEngine;

using FunctionSig = Signature<ValueType>;

// Implements a cache for import wrappers.
class WasmImportWrapperCache {
 public:
  using CacheKey = std::pair<compiler::WasmImportCallKind, FunctionSig*>;

  class CacheKeyHash {
   public:
    size_t operator()(const CacheKey& key) const {
      return base::hash_combine(static_cast<uint8_t>(key.first), *key.second);
    }
  };

  // Helper class to modify the cache under a lock.
  class ModificationScope {
   public:
    explicit ModificationScope(WasmImportWrapperCache* cache)
        : cache_(cache), guard_(&cache->mutex_) {}

    V8_EXPORT_PRIVATE WasmCode*& operator[](const CacheKey& key);

   private:
    WasmImportWrapperCache* const cache_;
    base::MutexGuard guard_;
  };

  // Assumes the key exists in the map.
  V8_EXPORT_PRIVATE WasmCode* Get(compiler::WasmImportCallKind kind,
                                  FunctionSig* sig) const;

  ~WasmImportWrapperCache();

 private:
  base::Mutex mutex_;
  std::unordered_map<CacheKey, WasmCode*, CacheKeyHash> entry_map_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
