// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_BUILD_GRAPH_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_BUILD_GRAPH_PHASE_H_

#include "src/codegen/bailout-reason.h"
#include "src/compiler/linkage.h"
#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

struct BuildGraphPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(BuildGraph)

  base::Optional<BailoutReason> Run(Zone* temp_zone, Linkage* linkage);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_BUILD_GRAPH_PHASE_H_
