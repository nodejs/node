// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-in-js-inlining-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/simplified-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/compiler/turboshaft/wasm-in-js-inlining-reducer-inl.h"

namespace v8::internal::compiler::turboshaft {

void WasmInJSInliningPhase::Run(PipelineData* data, Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(data->broker(), DEBUG_BOOL);

  // GVN (ValueNumberingReducer) is run here to deduplicate Wasm operations
  // across multiple inlined functions. This can also help with subsequent
  // WasmGC typed optimizations or Wasm load elimination.
  CopyingPhase<WasmInJSInliningReducer, SimplifiedOptimizationReducer,
               ValueNumberingReducer>::Run(data, temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
