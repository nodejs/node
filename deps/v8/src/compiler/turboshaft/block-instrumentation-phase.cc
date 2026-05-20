// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/block-instrumentation-phase.h"

#include "src/compiler/turboshaft/block-instrumentation-reducer.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/load-store-simplification-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"

namespace v8::internal::compiler::turboshaft {

void BlockInstrumentationPhase::Run(PipelineData* data, Zone* temp_zone) {
  // BlockInstrumentationReducer introduces loads & stores that are not
  // normalized. We thus always add LoadStoreSimplificationReducer to the stack
  // to make sure that those loads & stores get normalized.
  CopyingPhase<BlockInstrumentationReducer, LoadStoreSimplificationReducer,
               ValueNumberingReducer>::Run(data, temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
