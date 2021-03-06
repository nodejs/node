// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/s390/unwinding-info-writer-s390.h"
#include "src/compiler/backend/instruction.h"

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
  if (!initial_state) return;
  if (initial_state->saved_lr_ != saved_lr_) {
    eh_frame_writer_.AdvanceLocation(pc_offset);
    if (initial_state->saved_lr_) {
      eh_frame_writer_.RecordRegisterSavedToStack(r14, kSystemPointerSize);
      eh_frame_writer_.RecordRegisterSavedToStack(fp, 0);
    } else {
      eh_frame_writer_.RecordRegisterFollowsInitialRule(r14);
    }
    saved_lr_ = initial_state->saved_lr_;
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
          zone_->New<BlockInitialState>(saved_lr_);
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
  eh_frame_writer_.RecordRegisterSavedToStack(r14, kSystemPointerSize);
  eh_frame_writer_.RecordRegisterSavedToStack(fp, 0);
  saved_lr_ = true;
}

void UnwindingInfoWriter::MarkFrameDeconstructed(int at_pc) {
  if (!enabled()) return;

  // The lr is restored by the last operation in LeaveFrame().
  eh_frame_writer_.AdvanceLocation(at_pc);
  eh_frame_writer_.RecordRegisterFollowsInitialRule(r14);
  saved_lr_ = false;
}

void UnwindingInfoWriter::MarkLinkRegisterOnTopOfStack(int pc_offset) {
  if (!enabled()) return;

  eh_frame_writer_.AdvanceLocation(pc_offset);
  eh_frame_writer_.SetBaseAddressRegisterAndOffset(sp, 0);
  eh_frame_writer_.RecordRegisterSavedToStack(r14, 0);
}

void UnwindingInfoWriter::MarkPopLinkRegisterFromTopOfStack(int pc_offset) {
  if (!enabled()) return;

  eh_frame_writer_.AdvanceLocation(pc_offset);
  eh_frame_writer_.SetBaseAddressRegisterAndOffset(fp, 0);
  eh_frame_writer_.RecordRegisterFollowsInitialRule(r14);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
