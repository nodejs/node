// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-import-wrapper-cache.h"

#include <vector>

#include "src/logging/counters.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

WasmCode*& WasmImportWrapperCache::ModificationScope::operator[](
    const CacheKey& key) {
  return cache_->entry_map_[key];
}

WasmCode*& WasmImportWrapperCache::operator[](
    const WasmImportWrapperCache::CacheKey& key) {
  return entry_map_[key];
}

WasmCode* WasmImportWrapperCache::Get(compiler::WasmImportCallKind kind,
                                      FunctionSig* sig) const {
  base::MutexGuard lock(&mutex_);
  auto it = entry_map_.find({kind, sig});
  DCHECK(it != entry_map_.end());
  return it->second;
}

WasmImportWrapperCache::~WasmImportWrapperCache() {
  std::vector<WasmCode*> ptrs;
  ptrs.reserve(entry_map_.size());
  for (auto& e : entry_map_) {
    if (e.second) {
      ptrs.push_back(e.second);
    }
  }
  WasmCode::DecrementRefCount(VectorOf(ptrs));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
