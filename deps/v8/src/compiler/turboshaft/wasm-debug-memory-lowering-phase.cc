// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef DEBUG
#include "src/compiler/turboshaft/wasm-debug-memory-lowering-phase.h"

#include "src/compiler/turboshaft/memory-optimization-reducer.h"
#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

void WasmDebugMemoryLoweringPhase::Run(PipelineData* data, Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(data->broker(),
                              v8_flags.turboshaft_trace_reduction);
  CopyingPhase<MemoryOptimizationReducer>::Run(data, temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // DEBUG
