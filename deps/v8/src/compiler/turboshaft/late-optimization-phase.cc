// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/late-optimization-phase.h"

#include "src/compiler/turboshaft/branch-elimination-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/numbers/conversions-inl.h"

namespace v8::internal::compiler::turboshaft {

void LateOptimizationPhase::Run(PipelineData* data, Zone* temp_zone) {
  // TODO(dmercadier,tebbi): add missing CommonOperatorReducer.
  turboshaft::OptimizationPhase<
      turboshaft::VariableReducer, turboshaft::BranchEliminationReducer,
      turboshaft::SelectLoweringReducer,
      turboshaft::MachineOptimizationReducerSignallingNanImpossible,
      turboshaft::ValueNumberingReducer>::Run(data->isolate(), &data->graph(),
                                              temp_zone, data->node_origins());
}

}  // namespace v8::internal::compiler::turboshaft
