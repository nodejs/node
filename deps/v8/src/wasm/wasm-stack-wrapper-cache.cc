// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-stack-wrapper-cache.h"

#include "src/base/logging.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/wasm-wrapper-cache-inl.h"

namespace v8::internal::wasm {
std::pair<WasmCompilationResult, WasmCode::Kind>
WasmStackEntryWrapperCache::CompileWrapper(Isolate* isolate,
                                           const CacheKey& cache_key) {
#if V8_ENABLE_TURBOFAN
  wasm::WasmCompilationResult result =
      compiler::CompileWasmStackEntryWrapper(cache_key.sig);
  return {std::move(result), WasmCode::kWasmStackEntryWrapper};
#else
  UNREACHABLE();
#endif
}

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    WasmWrapperCache<StackEntryWrapperCacheKey>;
}  // namespace v8::internal::wasm
