// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if defined(V8_TARGET_ARCH_ARM)

#include "codegen-inl.h"
#include "jump-target-inl.h"
#include "register-allocator-inl.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// JumpTarget implementation.

#define __ ACCESS_MASM(cgen()->masm())

void JumpTarget::DoJump() {
  ASSERT(cgen()->has_valid_frame());
  // Live non-frame registers are not allowed at unconditional jumps
  // because we have no way of invalidating the corresponding results
  // which are still live in the C++ code.
  ASSERT(cgen()->HasValidEntryRegisters());

  if (entry_frame_set_) {
    // There already a frame expectation at the target.
    cgen()->frame()->MergeTo(&entry_frame_);
    cgen()->DeleteFrame();
  } else {
    // Clone the current frame to use as the expected one at the target.
    set_entry_frame(cgen()->frame());
    RegisterFile empty;
    cgen()->SetFrame(NULL, &empty);
  }
  __ jmp(&entry_label_);
}


void JumpTarget::DoBranch(Condition cc, Hint ignored) {
  ASSERT(cgen()->has_valid_frame());

  if (entry_frame_set_) {
    // Backward branch.  We have an expected frame to merge to on the
    // backward edge.
    cgen()->frame()->MergeTo(&entry_frame_, cc);
  } else {
    // Clone the current frame to use as the expected one at the target.
    set_entry_frame(cgen()->frame());
  }
  __ b(cc, &entry_label_);
  if (cc == al) {
    cgen()->DeleteFrame();
  }
}


void JumpTarget::Call() {
  // Call is used to push the address of the catch block on the stack as
  // a return address when compiling try/catch and try/finally.  We
  // fully spill the frame before making the call.  The expected frame
  // at the label (which should be the only one) is the spilled current
  // frame plus an in-memory return address.  The "fall-through" frame
  // at the return site is the spilled current frame.
  ASSERT(cgen()->has_valid_frame());
  // There are no non-frame references across the call.
  ASSERT(cgen()->HasValidEntryRegisters());
  ASSERT(!is_linked());

  // Calls are always 'forward' so we use a copy of the current frame (plus
  // one for a return address) as the expected frame.
  ASSERT(!entry_frame_set_);
  VirtualFrame target_frame = *cgen()->frame();
  target_frame.Adjust(1);
  set_entry_frame(&target_frame);

  __ bl(&entry_label_);
}


void JumpTarget::DoBind() {
  ASSERT(!is_bound());

  // Live non-frame registers are not allowed at the start of a basic
  // block.
  ASSERT(!cgen()->has_valid_frame() || cgen()->HasValidEntryRegisters());

  if (cgen()->has_valid_frame()) {
    // If there is a current frame we can use it on the fall through.
    if (!entry_frame_set_) {
      entry_frame_ = *cgen()->frame();
      entry_frame_set_ = true;
    } else {
      cgen()->frame()->MergeTo(&entry_frame_);
    }
  } else {
    // If there is no current frame we must have an entry frame which we can
    // copy.
    ASSERT(entry_frame_set_);
    RegisterFile empty;
    cgen()->SetFrame(new VirtualFrame(&entry_frame_), &empty);
  }

  __ bind(&entry_label_);
}


#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
