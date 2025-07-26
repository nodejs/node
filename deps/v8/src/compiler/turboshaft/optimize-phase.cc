// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/optimize-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/late-escape-analysis-reducer.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/memory-optimization-reducer.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/pretenuring-propagation-reducer.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/structural-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/numbers/conversions-inl.h"
#include "src/roots/roots-inl.h"

namespace v8::internal::compiler::turboshaft {

void OptimizePhase::Run(PipelineData* data, Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(data->broker(),
                              v8_flags.turboshaft_trace_reduction);
  turboshaft::CopyingPhase<turboshaft::StructuralOptimizationReducer,
                           turboshaft::LateEscapeAnalysisReducer,
                           turboshaft::PretenuringPropagationReducer,
                           turboshaft::MemoryOptimizationReducer,
                           turboshaft::MachineOptimizationReducer,
                           turboshaft::ValueNumberingReducer>::Run(data,
                                                                   temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
