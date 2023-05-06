// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/machine-lowering-phase.h"

#include "src/compiler/turboshaft/machine-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/heap/factory-inl.h"

namespace v8::internal::compiler::turboshaft {

void MachineLoweringPhase::Run(PipelineData* data, Zone* temp_zone) {
  turboshaft::OptimizationPhase<turboshaft::MachineLoweringReducer,
                                turboshaft::VariableReducer>::
      Run(data->isolate(), &data->graph(), temp_zone, data->node_origins(),
          std::tuple{turboshaft::MachineLoweringReducerArgs{
              data->isolate()->factory(), data->isolate()}});
}

}  // namespace v8::internal::compiler::turboshaft
