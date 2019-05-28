// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_JS_TO_WASM_WRAPPER_CACHE_H_
#define V8_WASM_JS_TO_WASM_WRAPPER_CACHE_H_

#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

class JSToWasmWrapperCache {
 public:
  Handle<Code> GetOrCompileJSToWasmWrapper(Isolate* isolate, FunctionSig* sig,
                                           bool is_import) {
    std::pair<bool, FunctionSig> key(is_import, *sig);
    Handle<Code>& cached = cache_[key];
    if (cached.is_null()) {
      cached = compiler::CompileJSToWasmWrapper(isolate, sig, is_import)
                   .ToHandleChecked();
    }
    return cached;
  }

 private:
  // We generate different code for calling imports than calling wasm functions
  // in this module. Both are cached separately.
  using CacheKey = std::pair<bool, FunctionSig>;
  std::unordered_map<CacheKey, Handle<Code>, base::hash<CacheKey>> cache_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_JS_TO_WASM_WRAPPER_CACHE_H_
