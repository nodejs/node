// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/tag-untag-lowering-phase.h"

#include "src/compiler/turboshaft/tag-untag-lowering-reducer.h"

namespace v8::internal::compiler::turboshaft {

void TagUntagLoweringPhase::Run(PipelineData* data, Zone* temp_zone) {
  turboshaft::OptimizationPhase<turboshaft::TagUntagLoweringReducer>::Run(
      data->isolate(), &data->graph(), temp_zone, data->node_origins());
}

}  // namespace v8::internal::compiler::turboshaft
