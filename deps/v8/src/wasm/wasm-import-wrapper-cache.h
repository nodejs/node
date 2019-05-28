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
  ~WasmImportWrapperCache();

  V8_EXPORT_PRIVATE WasmCode* GetOrCompile(WasmEngine* wasm_engine,
                                           Counters* counters,
                                           compiler::WasmImportCallKind kind,
                                           FunctionSig* sig);

 private:
  friend class NativeModule;
  using CacheKey = std::pair<uint8_t, FunctionSig>;

  mutable base::Mutex mutex_;
  NativeModule* native_module_;
  std::unordered_map<CacheKey, WasmCode*, base::hash<CacheKey>> entry_map_;

  explicit WasmImportWrapperCache(NativeModule* native_module)
      : native_module_(native_module) {}
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_IMPORT_WRAPPER_CACHE_H_
