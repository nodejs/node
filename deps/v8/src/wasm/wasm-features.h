// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_FEATURES_H_
#define V8_WASM_WASM_FEATURES_H_

// The feature flags are declared in their own header.
#include "src/base/macros.h"
#include "src/wasm/wasm-feature-flags.h"

// All features, including features that do not have flags.
#define FOREACH_WASM_FEATURE FOREACH_WASM_FEATURE_FLAG

namespace v8 {
namespace internal {
class Isolate;
namespace wasm {

#define COMMA ,
#define SPACE
#define DECL_FIELD(feat, desc, val) bool feat = false;
#define JUST_TRUE(feat, desc, val) true
#define JUST_FALSE(feat, desc, val) false
#define DECL_PARAM(feat, desc, val) bool p##feat
#define DO_INIT(feat, desc, val) feat(p##feat)

// Enabled or detected features.
struct WasmFeatures {
  FOREACH_WASM_FEATURE(DECL_FIELD, SPACE)

  constexpr WasmFeatures() = default;

  explicit constexpr WasmFeatures(FOREACH_WASM_FEATURE(DECL_PARAM, COMMA))
      : FOREACH_WASM_FEATURE(DO_INIT, COMMA) {}
};

static constexpr WasmFeatures kAllWasmFeatures{
    FOREACH_WASM_FEATURE(JUST_TRUE, COMMA)};

static constexpr WasmFeatures kNoWasmFeatures{
    FOREACH_WASM_FEATURE(JUST_FALSE, COMMA)};

#undef JUST_TRUE
#undef JUST_FALSE
#undef DECL_FIELD
#undef DECL_PARAM
#undef DO_INIT
#undef COMMA
#undef SPACE

static constexpr WasmFeatures kAsmjsWasmFeatures = kNoWasmFeatures;

V8_EXPORT_PRIVATE WasmFeatures WasmFeaturesFromFlags();

// Enables features based on both commandline flags and the isolate.
// Precondition: A valid context must be set in {isolate->context()}.
V8_EXPORT_PRIVATE WasmFeatures WasmFeaturesFromIsolate(Isolate* isolate);

V8_EXPORT_PRIVATE void UnionFeaturesInto(WasmFeatures* dst,
                                         const WasmFeatures& src);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_FEATURES_H_
