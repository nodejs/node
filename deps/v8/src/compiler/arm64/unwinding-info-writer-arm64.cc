// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/arm64/unwinding-info-writer-arm64.h"
#include "src/compiler/instruction.h"

namespace v8 {
namespace internal {
namespace compiler {

void UnwindingInfoWriter::BeginInstructionBlock(int pc_offset,
                                                const InstructionBlock* block) {
  if (!enabled()) return;

  block_will_exit_ = false;

  DCHECK_LT(block->rpo_number().ToInt(),
            static_cast<int>(block_initial_states_.size()));
  const BlockInitialState* initial_state =
      block_initial_states_[block->rpo_number().ToInt()];
  if (initial_state) {
    if (initial_state->saved_lr_ != saved_lr_) {
      eh_frame_writer_.AdvanceLocation(pc_offset);
      if (initial_state->saved_lr_) {
        eh_frame_writer_.RecordRegisterSavedToStack(lr, kPointerSize);
        eh_frame_writer_.RecordRegisterSavedToStack(fp, 0);
      } else {
        eh_frame_writer_.RecordRegisterFollowsInitialRule(lr);
      }
      saved_lr_ = initial_state->saved_lr_;
    }
  } else {
    // The entry block always lacks an explicit initial state.
    // The exit block may lack an explicit state, if it is only reached by
    //   the block ending in a ret.
    // All the other blocks must have an explicit initial state.
    DCHECK(block->predecessors().empty() || block->successors().empty());
  }
}

void UnwindingInfoWriter::EndInstructionBlock(const InstructionBlock* block) {
  if (!enabled() || block_will_exit_) return;

  for (const RpoNumber& successor : block->successors()) {
    int successor_index = successor.ToInt();
    DCHECK_LT(successor_index, static_cast<int>(block_initial_states_.size()));
    const BlockInitialState* existing_state =
        block_initial_states_[successor_index];

    // If we already had an entry for this BB, check that the values are the
    // same we are trying to insert.
    if (existing_state) {
      DCHECK_EQ(existing_state->saved_lr_, saved_lr_);
    } else {
      block_initial_states_[successor_index] =
          new (zone_) BlockInitialState(saved_lr_);
    }
  }
}

void UnwindingInfoWriter::MarkFrameConstructed(int at_pc) {
  if (!enabled()) return;

  // Regardless of the type of frame constructed, the relevant part of the
  // layout is always the one in the diagram:
  //
  // |   ....   |         higher addresses
  // +----------+               ^
  // |    LR    |               |            |
  // +----------+               |            |
  // | saved FP |               |            |
  // +----------+ <-- FP                     v
  // |   ....   |                       stack growth
  //
  // The LR is pushed on the stack, and we can record this fact at the end of
  // the construction, since the LR itself is not modified in the process.
  eh_frame_writer_.AdvanceLocation(at_pc);
  eh_frame_writer_.RecordRegisterSavedToStack(lr, kPointerSize);
  eh_frame_writer_.RecordRegisterSavedToStack(fp, 0);
  saved_lr_ = true;
}

void UnwindingInfoWriter::MarkFrameDeconstructed(int at_pc) {
  if (!enabled()) return;

  // The lr is restored by the last operation in LeaveFrame().
  eh_frame_writer_.AdvanceLocation(at_pc);
  eh_frame_writer_.RecordRegisterFollowsInitialRule(lr);
  saved_lr_ = false;
}

void UnwindingInfoWriter::MarkLinkRegisterOnTopOfStack(int pc_offset,
                                                       const Register& sp) {
  if (!enabled()) return;

  eh_frame_writer_.AdvanceLocation(pc_offset);
  eh_frame_writer_.SetBaseAddressRegisterAndOffset(sp, 0);
  eh_frame_writer_.RecordRegisterSavedToStack(lr, 0);
}

void UnwindingInfoWriter::MarkPopLinkRegisterFromTopOfStack(int pc_offset) {
  if (!enabled()) return;

  eh_frame_writer_.AdvanceLocation(pc_offset);
  eh_frame_writer_.SetBaseAddressRegisterAndOffset(fp, 0);
  eh_frame_writer_.RecordRegisterFollowsInitialRule(lr);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
