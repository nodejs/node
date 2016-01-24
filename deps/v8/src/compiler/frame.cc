// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame.h"

#include "src/compiler/linkage.h"
#include "src/compiler/register-allocator.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

Frame::Frame(int fixed_frame_size_in_slots, const CallDescriptor* descriptor)
    : needs_frame_((descriptor != nullptr) &&
                   descriptor->RequiresFrameAsIncoming()),
      frame_slot_count_(fixed_frame_size_in_slots),
      callee_saved_slot_count_(0),
      spill_slot_count_(0),
      allocated_registers_(nullptr),
      allocated_double_registers_(nullptr) {}


void FrameAccessState::SetFrameAccessToDefault() {
  if (frame()->needs_frame() && !FLAG_turbo_sp_frame_access) {
    SetFrameAccessToFP();
  } else {
    SetFrameAccessToSP();
  }
}


FrameOffset FrameAccessState::GetFrameOffset(int spill_slot) const {
  const int offset =
      (StandardFrameConstants::kFixedSlotCountAboveFp - spill_slot - 1) *
      kPointerSize;
  if (access_frame_with_fp()) {
    DCHECK(frame()->needs_frame());
    return FrameOffset::FromFramePointer(offset);
  } else {
    // No frame. Retrieve all parameters relative to stack pointer.
    int sp_offset =
        offset + ((frame()->GetSpToFpSlotCount() + sp_delta()) * kPointerSize);
    return FrameOffset::FromStackPointer(sp_offset);
  }
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
