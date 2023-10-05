// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/build-graph-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/graph-builder.h"
#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

base::Optional<BailoutReason> BuildGraphPhase::Run(Zone* temp_zone,
                                                   Linkage* linkage) {
  Schedule* schedule = PipelineData::Get().schedule();
  PipelineData::Get().reset_schedule();
  DCHECK_NOT_NULL(schedule);

  UnparkedScopeIfNeeded scope(PipelineData::Get().broker());

  if (auto bailout = turboshaft::BuildGraph(schedule, temp_zone, linkage)) {
    return bailout;
  }
  return {};
}

}  // namespace v8::internal::compiler::turboshaft
