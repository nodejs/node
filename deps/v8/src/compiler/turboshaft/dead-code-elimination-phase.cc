// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/dead-code-elimination-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/dead-code-elimination-reducer.h"

namespace v8::internal::compiler::turboshaft {

void DeadCodeEliminationPhase::Run(PipelineData* data, Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(data->broker(), DEBUG_BOOL);

  turboshaft::OptimizationPhase<turboshaft::DeadCodeEliminationReducer>::Run(
      data->isolate(), &data->graph(), temp_zone, data->node_origins());
}

}  // namespace v8::internal::compiler::turboshaft
