// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/typed-optimizations-phase.h"

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/type-inference-reducer.h"
#include "src/compiler/turboshaft/typed-optimizations-reducer.h"

namespace v8::internal::compiler::turboshaft {

void TypedOptimizationsPhase::Run(PipelineData* data, Zone* temp_zone) {
#ifdef DEBUG
  UnparkedScopeIfNeeded scope(data->broker(), v8_flags.turboshaft_trace_typing);
#endif

  turboshaft::TypeInferenceReducerArgs typing_args{
      data->isolate(),
      turboshaft::TypeInferenceReducerArgs::InputGraphTyping::kPrecise,
      turboshaft::TypeInferenceReducerArgs::OutputGraphTyping::kNone};

  turboshaft::OptimizationPhase<
      turboshaft::TypedOptimizationsReducer,
      turboshaft::TypeInferenceReducer>::Run(data->isolate(), &data->graph(),
                                             temp_zone, data->node_origins(),
                                             {typing_args});
}

}  // namespace v8::internal::compiler::turboshaft
