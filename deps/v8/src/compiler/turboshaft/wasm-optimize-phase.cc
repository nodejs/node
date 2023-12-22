// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-optimize-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/branch-elimination-reducer.h"
#include "src/compiler/turboshaft/late-load-elimination-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/memory-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/compiler/turboshaft/wasm-lowering-reducer.h"
#include "src/numbers/conversions-inl.h"

namespace v8::internal::compiler::turboshaft {

void WasmOptimizePhase::Run(Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(PipelineData::Get().broker(),
                              v8_flags.turboshaft_trace_reduction);
  // TODO(14108): Add more reducers as needed.
  // Note: The MemoryOptimizationReducer should run after any optimization that
  // might introduce new allocations. Therefore, if at any point in time the
  // WasmLoweringReducer started to lower to allocations, it should be moved to
  // a separate (prior) phase.
  OptimizationPhase<
      WasmLoweringReducer, MachineOptimizationReducerSignallingNanPossible,
      MemoryOptimizationReducer, VariableReducer, RequiredOptimizationReducer,
      BranchEliminationReducer, LateLoadEliminationReducer,
      ValueNumberingReducer>::Run(temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
