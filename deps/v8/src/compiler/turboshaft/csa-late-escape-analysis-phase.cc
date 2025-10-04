// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/csa-late-escape-analysis-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/late-escape-analysis-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/numbers/conversions-inl.h"

namespace v8::internal::compiler::turboshaft {

void CsaLateEscapeAnalysisPhase::Run(PipelineData* data, Zone* temp_zone) {
  CopyingPhase<LateEscapeAnalysisReducer, MachineOptimizationReducer,
               ValueNumberingReducer>::Run(data, temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
