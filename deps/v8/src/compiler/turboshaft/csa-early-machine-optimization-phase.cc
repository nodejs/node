// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/csa-early-machine-optimization-phase.h"

#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/value-numbering-reducer.h"
#include "src/objects/heap-object-inl.h"

namespace v8::internal::compiler::turboshaft {

void CsaEarlyMachineOptimizationPhase::Run(PipelineData* data,
                                           Zone* temp_zone) {
  CopyingPhase<MachineOptimizationReducer, ValueNumberingReducer>::Run(
      data, temp_zone);
}

}  // namespace v8::internal::compiler::turboshaft
