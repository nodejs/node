// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame.h"

#include "src/compiler/linkage.h"

namespace v8 {
namespace internal {
namespace compiler {

Frame::Frame(int fixed_frame_size_in_slots)
    : fixed_slot_count_(fixed_frame_size_in_slots),
      frame_slot_count_(fixed_frame_size_in_slots),
      spill_slot_count_(0),
      return_slot_count_(0),
      allocated_registers_(nullptr),
      allocated_double_registers_(nullptr) {}

int Frame::AlignFrame(int alignment) {
  int alignment_slots = alignment / kSystemPointerSize;
  // We have to align return slots separately, because they are claimed
  // separately on the stack.
  int return_delta =
      alignment_slots - (return_slot_count_ & (alignment_slots - 1));
  if (return_delta != alignment_slots) {
    frame_slot_count_ += return_delta;
  }
  int delta = alignment_slots - (frame_slot_count_ & (alignment_slots - 1));
  if (delta != alignment_slots) {
    frame_slot_count_ += delta;
    if (spill_slot_count_ != 0) {
      spill_slot_count_ += delta;
    }
  }
  return delta;
}

void FrameAccessState::MarkHasFrame(bool state) {
  has_frame_ = state;
  SetFrameAccessToDefault();
}

void FrameAccessState::SetFrameAccessToDefault() {
  if (has_frame() && !FLAG_turbo_sp_frame_access) {
    SetFrameAccessToFP();
  } else {
    SetFrameAccessToSP();
  }
}


FrameOffset FrameAccessState::GetFrameOffset(int spill_slot) const {
  const int frame_offset = FrameSlotToFPOffset(spill_slot);
  if (access_frame_with_fp()) {
    return FrameOffset::FromFramePointer(frame_offset);
  } else {
    // No frame. Retrieve all parameters relative to stack pointer.
    int sp_offset = frame_offset + GetSPToFPOffset();
    return FrameOffset::FromStackPointer(sp_offset);
  }
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
