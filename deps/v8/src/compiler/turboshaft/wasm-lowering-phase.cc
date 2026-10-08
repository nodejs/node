// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-lowering-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/wasm-lowering-reducer.h"
#include "src/handles/handles-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/heap-object-inl.h"

namespace v8::internal::compiler::turboshaft {

void WasmLoweringPhase::Run(PipelineData* data, Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(data->broker(),
                              v8_flags.turboshaft_trace_reduction);
  // Also run the MachineOptimizationReducer as it can help the late load
  // elimination that follows this phase eliminate more loads.
  CopyingPhase<WasmLoweringReducer, MachineOptimizationReducer>::Run(data,
                                                                     temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
