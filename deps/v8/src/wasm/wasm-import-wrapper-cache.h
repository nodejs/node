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
  struct CacheKey {
    CacheKey(const compiler::WasmImportCallKind& _kind, const FunctionSig* _sig,
             int _expected_arity)
        : kind(_kind),
          signature(_sig),
          expected_arity(_expected_arity == kDontAdaptArgumentsSentinel
                             ? 0
                             : _expected_arity) {}

    bool operator==(const CacheKey& rhs) const {
      return kind == rhs.kind && signature == rhs.signature &&
             expected_arity == rhs.expected_arity;
    }

    compiler::WasmImportCallKind kind;
    const FunctionSig* signature;
    int expected_arity;
  };

  class CacheKeyHash {
   public:
    size_t operator()(const CacheKey& key) const {
      return base::hash_combine(static_cast<uint8_t>(key.kind), key.signature,
                                key.expected_arity);
    }
  };

  // Helper class to modify the cache under a lock.
  class V8_NODISCARD ModificationScope {
   public:
    explicit ModificationScope(WasmImportWrapperCache* cache)
        : cache_(cache), guard_(&cache->mutex_) {}

    V8_EXPORT_PRIVATE WasmCode*& operator[](const CacheKey& key);

   private:
    WasmImportWrapperCache* const cache_;
    base::MutexGuard guard_;
  };

  // Not thread-safe, use ModificationScope to get exclusive write access to the
  // cache.
  V8_EXPORT_PRIVATE WasmCode*& operator[](const CacheKey& key);

  // Thread-safe. Assumes the key exists in the map.
  V8_EXPORT_PRIVATE WasmCode* Get(compiler::WasmImportCallKind kind,
                                  const FunctionSig* sig,
                                  int expected_arity) const;

  ~WasmImportWrapperCache();

 private:
  mutable base::Mutex mutex_;
  std::unordered_map<CacheKey, WasmCode*, CacheKeyHash> entry_map_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
