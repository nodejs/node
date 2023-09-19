// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/recreate-schedule-phase.h"

namespace v8::internal::compiler::turboshaft {

RecreateScheduleResult RecreateSchedulePhase::Run(PipelineData* data,
                                                  Zone* temp_zone,
                                                  Linkage* linkage) {
  return RecreateSchedule(data->graph(), data->broker(),
                          linkage->GetIncomingDescriptor(), data->graph_zone(),
                          temp_zone, data->source_positions(),
                          data->node_origins());
}

}  // namespace v8::internal::compiler::turboshaft
