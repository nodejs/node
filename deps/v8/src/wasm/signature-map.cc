// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/signature-map.h"

#include "src/signature.h"

namespace v8 {
namespace internal {
namespace wasm {

uint32_t SignatureMap::FindOrInsert(const FunctionSig& sig) {
  CHECK(!frozen_);
  auto pos = map_.find(sig);
  if (pos != map_.end()) {
    return pos->second;
  } else {
    uint32_t index = next_++;
    map_[sig] = index;
    return index;
  }
}

int32_t SignatureMap::Find(const FunctionSig& sig) const {
  auto pos = map_.find(sig);
  if (pos != map_.end()) {
    return static_cast<int32_t>(pos->second);
  } else {
    return -1;
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
