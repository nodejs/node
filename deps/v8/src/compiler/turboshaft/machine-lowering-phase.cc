// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/machine-lowering-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/dataview-lowering-reducer.h"
#include "src/compiler/turboshaft/fast-api-call-lowering-reducer.h"
#include "src/compiler/turboshaft/machine-lowering-reducer-inl.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"

namespace v8::internal::compiler::turboshaft {

void MachineLoweringPhase::Run(Zone* temp_zone) {
  CopyingPhase<DataViewLoweringReducer, MachineLoweringReducer,
               FastApiCallLoweringReducer, SelectLoweringReducer,
               MachineOptimizationReducer>::Run(temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
