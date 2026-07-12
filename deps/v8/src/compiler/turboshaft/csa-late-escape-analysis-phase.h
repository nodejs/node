// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_CSA_LATE_ESCAPE_ANALYSIS_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_CSA_LATE_ESCAPE_ANALYSIS_PHASE_H_

#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

struct CsaLateEscapeAnalysisPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(CsaLateEscapeAnalysis)

  void Run(PipelineData* data, Zone* temp_zone);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_CSA_LATE_ESCAPE_ANALYSIS_PHASE_H_
