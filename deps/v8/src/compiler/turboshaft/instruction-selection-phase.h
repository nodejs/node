// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_PHASE_H_

#include "src/compiler/turboshaft/phase.h"

namespace v8::internal::compiler::turboshaft {

struct InstructionSelectionPhase {
  DECL_TURBOSHAFT_PHASE_CONSTANTS(InstructionSelection)

  base::Optional<BailoutReason> Run(Zone* temp_zone,
                                    const CallDescriptor* call_descriptor,
                                    Linkage* linkage, CodeTracer* code_tracer);
};

// Disable printing a default turboshaft graph as this phase produces an
// instruction seqeuence rather than a new graph.
template <>
struct produces_printable_graph<InstructionSelectionPhase>
    : public std::false_type {};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_PHASE_H_
