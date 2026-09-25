// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_CSA_EFFECTS_COMPUTATION_H_
#define V8_COMPILER_TURBOSHAFT_CSA_EFFECTS_COMPUTATION_H_

#include "src/compiler/turboshaft/phase.h"

// This phase computes which builtins can allocate, and make this information
// available to the JavaScript pipeline, in order to prevent bugs caused by
// "forgetting to mark a Call as CanAllocate". Currently, this is just used for
// DCHECKing the CanAllocate flag of builtin calls in Turboshaft and Maglev, but
// we could also use this to automatically set the CanAllocate property for
// builtins (ie, in turboshaft/builtin-call-descriptors.h).
//
// The idea of the analysis is simple: a builtin can allocate if
//
//   1- its graph contains AllocateOp
//   2- or, it calls a runtime function that can allocate
//   3- or, it calls a builtin that can allocate.
//
// 1. and 2. are trivial to compute: we just iterate the graph and look at all
// operations for AllocateOp or runtime calls (and then we use
// Runtime::kCanTriggerGC).
// And to compute 3, when compiling each builtin, we record all of the builtins
// that they call. Once we've done this for all builtins, we call
// BuiltinsEffectsAnalyzer::Finalize, which compute which builtins can allocate
// based on whether the builtins they call can or can't allocate.
//
// Note that for now, we're just interested in which builtins can allocate, but,
// long-term, we could expand this analysis to compute other side-effects for
// builtins, in particular reads and writes for parameters and arbitrary memory.

namespace v8::internal::compiler::turboshaft {

struct CsaEffectsComputationPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(CsaEffectsComputation)

  void Run(PipelineData* data, Zone* temp_zone);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_CSA_EFFECTS_COMPUTATION_H_
