// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/pipelines.h"

#include "src/compiler/pipeline-data-inl.h"
#include "src/compiler/turboshaft/recreate-schedule-phase.h"

namespace v8::internal::compiler::turboshaft {

void Pipeline::RecreateTurbofanGraph(compiler::TFPipelineData* turbofan_data,
                                     Linkage* linkage) {
  Run<turboshaft::RecreateSchedulePhase>(turbofan_data, linkage);
  TraceSchedule(turbofan_data->info(), turbofan_data, turbofan_data->schedule(),
                turboshaft::RecreateSchedulePhase::phase_name());
}

}  // namespace v8::internal::compiler::turboshaft
