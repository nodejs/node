// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/x64/unwinding-info-writer-x64.h"
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
    if (!initial_state->register_.is(eh_frame_writer_.base_register()) &&
        initial_state->offset_ != eh_frame_writer_.base_offset()) {
      eh_frame_writer_.AdvanceLocation(pc_offset);
      eh_frame_writer_.SetBaseAddressRegisterAndOffset(initial_state->register_,
                                                       initial_state->offset_);
    } else if (!initial_state->register_.is(eh_frame_writer_.base_register())) {
      eh_frame_writer_.AdvanceLocation(pc_offset);
      eh_frame_writer_.SetBaseAddressRegister(initial_state->register_);
    } else if (initial_state->offset_ != eh_frame_writer_.base_offset()) {
      eh_frame_writer_.AdvanceLocation(pc_offset);
      eh_frame_writer_.SetBaseAddressOffset(initial_state->offset_);
    }

    tracking_fp_ = initial_state->tracking_fp_;
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
      DCHECK(existing_state->register_.is(eh_frame_writer_.base_register()));
      DCHECK_EQ(existing_state->offset_, eh_frame_writer_.base_offset());
      DCHECK_EQ(existing_state->tracking_fp_, tracking_fp_);
    } else {
      block_initial_states_[successor_index] = new (zone_)
          BlockInitialState(eh_frame_writer_.base_register(),
                            eh_frame_writer_.base_offset(), tracking_fp_);
    }
  }
}

void UnwindingInfoWriter::MarkFrameConstructed(int pc_base) {
  if (!enabled()) return;

  // push rbp
  eh_frame_writer_.AdvanceLocation(pc_base + 1);
  eh_frame_writer_.IncreaseBaseAddressOffset(kInt64Size);
  // <base address> points at the bottom of the current frame on x64 and
  // <base register> is rsp, which points to the top of the frame by definition.
  // Thus, the distance between <base address> and the top is -<base offset>.
  int top_of_stack = -eh_frame_writer_.base_offset();
  eh_frame_writer_.RecordRegisterSavedToStack(rbp, top_of_stack);

  // mov rbp, rsp
  eh_frame_writer_.AdvanceLocation(pc_base + 4);
  eh_frame_writer_.SetBaseAddressRegister(rbp);

  tracking_fp_ = true;
}

void UnwindingInfoWriter::MarkFrameDeconstructed(int pc_base) {
  if (!enabled()) return;

  // mov rsp, rbp
  eh_frame_writer_.AdvanceLocation(pc_base + 3);
  eh_frame_writer_.SetBaseAddressRegister(rsp);

  // pop rbp
  eh_frame_writer_.AdvanceLocation(pc_base + 4);
  eh_frame_writer_.IncreaseBaseAddressOffset(-kInt64Size);

  tracking_fp_ = false;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
