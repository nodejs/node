// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/signature-map.h"

namespace v8 {
namespace internal {
namespace wasm {

uint32_t SignatureMap::FindOrInsert(FunctionSig* sig) {
  auto pos = map_.find(sig);
  if (pos != map_.end()) {
    return pos->second;
  } else {
    uint32_t index = next_++;
    map_[sig] = index;
    return index;
  }
}

int32_t SignatureMap::Find(FunctionSig* sig) const {
  auto pos = map_.find(sig);
  if (pos != map_.end()) {
    return static_cast<int32_t>(pos->second);
  } else {
    return -1;
  }
}

bool SignatureMap::CompareFunctionSigs::operator()(FunctionSig* a,
                                                   FunctionSig* b) const {
  if (a == b) return false;
  if (a->return_count() < b->return_count()) return true;
  if (a->return_count() > b->return_count()) return false;
  if (a->parameter_count() < b->parameter_count()) return true;
  if (a->parameter_count() > b->parameter_count()) return false;
  for (size_t r = 0; r < a->return_count(); r++) {
    if (a->GetReturn(r) < b->GetReturn(r)) return true;
    if (a->GetReturn(r) > b->GetReturn(r)) return false;
  }
  for (size_t p = 0; p < a->parameter_count(); p++) {
    if (a->GetParam(p) < b->GetParam(p)) return true;
    if (a->GetParam(p) > b->GetParam(p)) return false;
  }
  return false;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
