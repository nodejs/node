// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/build-graph-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/graph-builder.h"

namespace v8::internal::compiler::turboshaft {

base::Optional<BailoutReason> BuildGraphPhase::Run(PipelineData* data,
                                                   Zone* temp_zone,
                                                   Linkage* linkage) {
  Schedule* schedule = data->schedule();
  data->reset_schedule();
  DCHECK_NOT_NULL(schedule);
  data->CreateTurboshaftGraph();

  UnparkedScopeIfNeeded scope(data->broker());

  if (auto bailout = turboshaft::BuildGraph(
          data->broker(), schedule, data->isolate(), data->graph_zone(),
          temp_zone, &data->graph(), linkage, data->source_positions(),
          data->node_origins())) {
    return bailout;
  }
  return {};
}

}  // namespace v8::internal::compiler::turboshaft
