// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BRANCH_HINT_MAP_H_
#define V8_WASM_BRANCH_HINT_MAP_H_

#include <unordered_map>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

namespace wasm {

enum class WasmBranchHint : uint8_t {
  kNoHint = 0,
  kUnlikely = 1,
  kLikely = 2,
};

class V8_EXPORT_PRIVATE BranchHintMap {
 public:
  void insert(uint32_t offset, WasmBranchHint hint) {
    map_.emplace(offset, hint);
  }
  WasmBranchHint GetHintFor(uint32_t offset) const {
    auto it = map_.find(offset);
    if (it == map_.end()) {
      return WasmBranchHint::kNoHint;
    }
    return it->second;
  }

 private:
  std::unordered_map<uint32_t, WasmBranchHint> map_;
};

using BranchHintInfo = std::unordered_map<uint32_t, BranchHintMap>;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BRANCH_HINT_MAP_H_
