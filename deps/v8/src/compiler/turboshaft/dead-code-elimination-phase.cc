// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/dead-code-elimination-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/dead-code-elimination-reducer.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/compiler/turboshaft/wasm-js-lowering-reducer.h"
#endif

namespace v8::internal::compiler::turboshaft {

void DeadCodeEliminationPhase::Run(Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(PipelineData::Get().broker(), DEBUG_BOOL);

  turboshaft::OptimizationPhase<turboshaft::DeadCodeEliminationReducer
#if V8_ENABLE_WEBASSEMBLY
                                ,
                                turboshaft::WasmJSLoweringReducer
#endif
                                >::Run(temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
