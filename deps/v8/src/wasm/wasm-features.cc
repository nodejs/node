// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-features.h"
#include "src/flags.h"
#include "src/handles-inl.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {
namespace wasm {

#define COMMA ,
#define SPACE
#define DO_UNION(feat, desc, val) dst->feat |= src.feat;
#define FLAG_REF(feat, desc, val) FLAG_experimental_wasm_##feat

void UnionFeaturesInto(WasmFeatures* dst, const WasmFeatures& src) {
  FOREACH_WASM_FEATURE(DO_UNION, SPACE);
}

WasmFeatures WasmFeaturesFromFlags() {
  return WasmFeatures{FOREACH_WASM_FEATURE(FLAG_REF, COMMA)};
}

WasmFeatures WasmFeaturesFromIsolate(Isolate* isolate) {
  WasmFeatures features = WasmFeaturesFromFlags();
  features.threads |=
      isolate->AreWasmThreadsEnabled(handle(isolate->context(), isolate));
  return features;
}

#undef DO_UNION
#undef FLAG_REF
#undef SPACE
#undef COMMA
}  // namespace wasm
}  // namespace internal
}  // namespace v8
