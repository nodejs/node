// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-features.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"

namespace v8 {
namespace internal {
namespace wasm {


void UnionFeaturesInto(WasmFeatures* dst, const WasmFeatures& src) {
#define DO_UNION(feat, desc, val) dst->feat |= src.feat;
  FOREACH_WASM_FEATURE(DO_UNION);
#undef DO_UNION
}

WasmFeatures WasmFeaturesFromFlags() {
#define FLAG_REF(feat, desc, val) FLAG_experimental_wasm_##feat,
  return WasmFeatures(FOREACH_WASM_FEATURE(FLAG_REF){});
#undef FLAG_REF
}

WasmFeatures WasmFeaturesFromIsolate(Isolate* isolate) {
  WasmFeatures features = WasmFeaturesFromFlags();
  features.threads |=
      isolate->AreWasmThreadsEnabled(handle(isolate->context(), isolate));
  return features;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
