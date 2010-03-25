// Copyright 2010 the V8 project authors. All rights reserved.
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

  if (is_bound()) {
    // Backward jump.  There already a frame expectation at the target.
    ASSERT(direction_ == BIDIRECTIONAL);
    cgen()->frame()->MergeTo(entry_frame_);
    cgen()->DeleteFrame();
  } else {
    // Use the current frame as the expected one at the target if necessary.
    if (entry_frame_ == NULL) {
      entry_frame_ = cgen()->frame();
      RegisterFile empty;
      cgen()->SetFrame(NULL, &empty);
    } else {
      cgen()->frame()->MergeTo(entry_frame_);
      cgen()->DeleteFrame();
    }

    // The predicate is_linked() should be made true.  Its implementation
    // detects the presence of a frame pointer in the reaching_frames_ list.
    if (!is_linked()) {
      reaching_frames_.Add(NULL);
      ASSERT(is_linked());
    }
  }
  __ b(&entry_label_);
  __ nop();   // Branch delay slot nop.
}


void JumpTarget::DoBranch(Condition cc, Hint ignored) {
  UNIMPLEMENTED_MIPS();
}


void JumpTarget::Call() {
  UNIMPLEMENTED_MIPS();
}


void JumpTarget::DoBind() {
  ASSERT(!is_bound());

  // Live non-frame registers are not allowed at the start of a basic
  // block.
  ASSERT(!cgen()->has_valid_frame() || cgen()->HasValidEntryRegisters());

  if (cgen()->has_valid_frame()) {
    // If there is a current frame we can use it on the fall through.
    if (entry_frame_ == NULL) {
      entry_frame_ = new VirtualFrame(cgen()->frame());
    } else {
      ASSERT(cgen()->frame()->Equals(entry_frame_));
    }
  } else {
    // If there is no current frame we must have an entry frame which we can
    // copy.
    ASSERT(entry_frame_ != NULL);
    RegisterFile empty;
    cgen()->SetFrame(new VirtualFrame(entry_frame_), &empty);
  }

  // The predicate is_linked() should be made false.  Its implementation
  // detects the presence (or absence) of frame pointers in the
  // reaching_frames_ list.  If we inserted a bogus frame to make
  // is_linked() true, remove it now.
  if (is_linked()) {
    reaching_frames_.Clear();
  }

  __ bind(&entry_label_);
}


void BreakTarget::Jump() {
  // On ARM we do not currently emit merge code for jumps, so we need to do
  // it explicitly here.  The only merging necessary is to drop extra
  // statement state from the stack.
  ASSERT(cgen()->has_valid_frame());
  int count = cgen()->frame()->height() - expected_height_;
  cgen()->frame()->Drop(count);
  DoJump();
}


void BreakTarget::Jump(Result* arg) {
  UNIMPLEMENTED_MIPS();
}


void BreakTarget::Bind() {
#ifdef DEBUG
  // All the forward-reaching frames should have been adjusted at the
  // jumps to this target.
  for (int i = 0; i < reaching_frames_.length(); i++) {
    ASSERT(reaching_frames_[i] == NULL ||
           reaching_frames_[i]->height() == expected_height_);
  }
#endif
  // Drop leftover statement state from the frame before merging, even
  // on the fall through.  This is so we can bind the return target
  // with state on the frame.
  if (cgen()->has_valid_frame()) {
    int count = cgen()->frame()->height() - expected_height_;
    // On ARM we do not currently emit merge code at binding sites, so we need
    // to do it explicitly here.  The only merging necessary is to drop extra
    // statement state from the stack.
    cgen()->frame()->Drop(count);
  }

  DoBind();
}


void BreakTarget::Bind(Result* arg) {
  UNIMPLEMENTED_MIPS();
}


#undef __


} }  // namespace v8::internal

