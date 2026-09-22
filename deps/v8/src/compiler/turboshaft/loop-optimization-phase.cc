// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/loop-optimization-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/loop-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/objects-inl.h"

namespace v8::internal::compiler::turboshaft {

void LoopOptimizationPhase::Run(PipelineData* data, Zone* temp_zone) {
  UnparkedScopeIfNeeded scope(data->broker());
  turboshaft::CopyingPhase<LoopOptimizationReducer, ValueNumberingReducer>::Run(
      data, temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
