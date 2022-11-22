// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-interpreter-frame-state.h"

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph.h"

namespace v8 {
namespace internal {
namespace maglev {

// static
MergePointInterpreterFrameState*
MergePointInterpreterFrameState::NewForCatchBlock(
    const MaglevCompilationUnit& unit,
    const compiler::BytecodeLivenessState* liveness, int handler_offset,
    interpreter::Register context_register, Graph* graph, bool is_inline) {
  Zone* const zone = unit.zone();
  MergePointInterpreterFrameState* state =
      zone->New<MergePointInterpreterFrameState>(
          unit, 0, 0, nullptr, BasicBlockType::kExceptionHandlerStart,
          liveness);
  auto& frame_state = state->frame_state_;
  // If the accumulator is live, the ExceptionPhi associated to it is the
  // first one in the block. That ensures it gets kReturnValue0 in the
  // register allocator. See
  // StraightForwardRegisterAllocator::AllocateRegisters.
  if (frame_state.liveness()->AccumulatorIsLive()) {
    frame_state.accumulator(unit) = state->NewExceptionPhi(
        zone, interpreter::Register::virtual_accumulator(), handler_offset);
  }
  frame_state.ForEachParameter(
      unit, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = state->NewExceptionPhi(zone, reg, handler_offset);
      });
  frame_state.context(unit) =
      state->NewExceptionPhi(zone, context_register, handler_offset);
  frame_state.ForEachLocal(
      unit, [&](ValueNode*& entry, interpreter::Register reg) {
        entry = state->NewExceptionPhi(zone, reg, handler_offset);
      });
  state->known_node_aspects_ = zone->New<KnownNodeAspects>(zone);
  return state;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
