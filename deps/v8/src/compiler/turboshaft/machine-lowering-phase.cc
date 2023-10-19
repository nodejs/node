// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/machine-lowering-phase.h"

#include "src/compiler/turboshaft/fast-api-call-reducer.h"
#include "src/compiler/turboshaft/machine-lowering-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/heap/factory-inl.h"

namespace v8::internal::compiler::turboshaft {

void MachineLoweringPhase::Run(Zone* temp_zone) {
  if (v8_flags.turboshaft_machine_lowering_opt) {
    turboshaft::OptimizationPhase<
        turboshaft::VariableReducer, turboshaft::MachineLoweringReducer,
        turboshaft::FastApiCallReducer, turboshaft::RequiredOptimizationReducer,
        turboshaft::SelectLoweringReducer,
        turboshaft::MachineOptimizationReducerSignallingNanImpossible>::
        Run(temp_zone);
  } else {
    turboshaft::OptimizationPhase<
        turboshaft::VariableReducer, turboshaft::MachineLoweringReducer,
        turboshaft::FastApiCallReducer, turboshaft::RequiredOptimizationReducer,
        turboshaft::SelectLoweringReducer>::Run(temp_zone);
  }
}

}  // namespace v8::internal::compiler::turboshaft
