// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-simd-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/wasm-shuffle-reducer.h"

namespace v8::internal::compiler::turboshaft {

void WasmSimdPhase::Run(PipelineData* data, Zone* temp_zone) {
  WasmShuffleAnalyzer analyzer(temp_zone, data->graph());

  if (analyzer.ShouldReduce()) {
    data->set_wasm_shuffle_analyzer(&analyzer);
    CopyingPhase<WasmShuffleReducer>::Run(data, temp_zone);
    data->clear_wasm_shuffle_analyzer();
  }
}

}  // namespace v8::internal::compiler::turboshaft
