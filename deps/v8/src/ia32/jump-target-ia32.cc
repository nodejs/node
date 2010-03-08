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
    // Backward jump.  There is an expected frame to merge to.
    ASSERT(direction_ == BIDIRECTIONAL);
    cgen()->frame()->PrepareMergeTo(entry_frame_);
    cgen()->frame()->MergeTo(entry_frame_);
    cgen()->DeleteFrame();
    __ jmp(&entry_label_);
  } else if (entry_frame_ != NULL) {
    // Forward jump with a preconfigured entry frame.  Assert the
    // current frame matches the expected one and jump to the block.
    ASSERT(cgen()->frame()->Equals(entry_frame_));
    cgen()->DeleteFrame();
    __ jmp(&entry_label_);
  } else {
    // Forward jump.  Remember the current frame and emit a jump to
    // its merge code.
    AddReachingFrame(cgen()->frame());
    RegisterFile empty;
    cgen()->SetFrame(NULL, &empty);
    __ jmp(&merge_labels_.last());
  }
}


void JumpTarget::DoBranch(Condition cc, Hint hint) {
  ASSERT(cgen() != NULL);
  ASSERT(cgen()->has_valid_frame());

  if (is_bound()) {
    ASSERT(direction_ == BIDIRECTIONAL);
    // Backward branch.  We have an expected frame to merge to on the
    // backward edge.

    // Swap the current frame for a copy (we do the swapping to get
    // the off-frame registers off the fall through) to use for the
    // branch.
    VirtualFrame* fall_through_frame = cgen()->frame();
    VirtualFrame* branch_frame = new VirtualFrame(fall_through_frame);
    RegisterFile non_frame_registers;
    cgen()->SetFrame(branch_frame, &non_frame_registers);

    // Check if we can avoid merge code.
    cgen()->frame()->PrepareMergeTo(entry_frame_);
    if (cgen()->frame()->Equals(entry_frame_)) {
      // Branch right in to the block.
      cgen()->DeleteFrame();
      __ j(cc, &entry_label_, hint);
      cgen()->SetFrame(fall_through_frame, &non_frame_registers);
      return;
    }

    // Check if we can reuse existing merge code.
    for (int i = 0; i < reaching_frames_.length(); i++) {
      if (reaching_frames_[i] != NULL &&
          cgen()->frame()->Equals(reaching_frames_[i])) {
        // Branch to the merge code.
        cgen()->DeleteFrame();
        __ j(cc, &merge_labels_[i], hint);
        cgen()->SetFrame(fall_through_frame, &non_frame_registers);
        return;
      }
    }

    // To emit the merge code here, we negate the condition and branch
    // around the merge code on the fall through path.
    Label original_fall_through;
    __ j(NegateCondition(cc), &original_fall_through, NegateHint(hint));
    cgen()->frame()->MergeTo(entry_frame_);
    cgen()->DeleteFrame();
    __ jmp(&entry_label_);
    cgen()->SetFrame(fall_through_frame, &non_frame_registers);
    __ bind(&original_fall_through);

  } else if (entry_frame_ != NULL) {
    // Forward branch with a preconfigured entry frame.  Assert the
    // current frame matches the expected one and branch to the block.
    ASSERT(cgen()->frame()->Equals(entry_frame_));
    // Explicitly use the macro assembler instead of __ as forward
    // branches are expected to be a fixed size (no inserted
    // coverage-checking instructions please).  This is used in
    // Reference::GetValue.
    cgen()->masm()->j(cc, &entry_label_, hint);

  } else {
    // Forward branch.  A copy of the current frame is remembered and
    // a branch to the merge code is emitted.  Explicitly use the
    // macro assembler instead of __ as forward branches are expected
    // to be a fixed size (no inserted coverage-checking instructions
    // please).  This is used in Reference::GetValue.
    AddReachingFrame(new VirtualFrame(cgen()->frame()));
    cgen()->masm()->j(cc, &merge_labels_.last(), hint);
  }
}


void JumpTarget::Call() {
  // Call is used to push the address of the catch block on the stack as
  // a return address when compiling try/catch and try/finally.  We
  // fully spill the frame before making the call.  The expected frame
  // at the label (which should be the only one) is the spilled current
  // frame plus an in-memory return address.  The "fall-through" frame
  // at the return site is the spilled current frame.
  ASSERT(cgen() != NULL);
  ASSERT(cgen()->has_valid_frame());
  // There are no non-frame references across the call.
  ASSERT(cgen()->HasValidEntryRegisters());
  ASSERT(!is_linked());

  cgen()->frame()->SpillAll();
  VirtualFrame* target_frame = new VirtualFrame(cgen()->frame());
  target_frame->Adjust(1);
  // We do not expect a call with a preconfigured entry frame.
  ASSERT(entry_frame_ == NULL);
  AddReachingFrame(target_frame);
  __ call(&merge_labels_.last());
}


void JumpTarget::DoBind() {
  ASSERT(cgen() != NULL);
  ASSERT(!is_bound());

  // Live non-frame registers are not allowed at the start of a basic
  // block.
  ASSERT(!cgen()->has_valid_frame() || cgen()->HasValidEntryRegisters());

  // Fast case: the jump target was manually configured with an entry
  // frame to use.
  if (entry_frame_ != NULL) {
    // Assert no reaching frames to deal with.
    ASSERT(reaching_frames_.is_empty());
    ASSERT(!cgen()->has_valid_frame());

    RegisterFile empty;
    if (direction_ == BIDIRECTIONAL) {
      // Copy the entry frame so the original can be used for a
      // possible backward jump.
      cgen()->SetFrame(new VirtualFrame(entry_frame_), &empty);
    } else {
      // Take ownership of the entry frame.
      cgen()->SetFrame(entry_frame_, &empty);
      entry_frame_ = NULL;
    }
    __ bind(&entry_label_);
    return;
  }

  if (!is_linked()) {
    ASSERT(cgen()->has_valid_frame());
    if (direction_ == FORWARD_ONLY) {
      // Fast case: no forward jumps and no possible backward jumps.
      // The stack pointer can be floating above the top of the
      // virtual frame before the bind.  Afterward, it should not.
      VirtualFrame* frame = cgen()->frame();
      int difference = frame->stack_pointer_ - (frame->element_count() - 1);
      if (difference > 0) {
        frame->stack_pointer_ -= difference;
        __ add(Operand(esp), Immediate(difference * kPointerSize));
      }
    } else {
      ASSERT(direction_ == BIDIRECTIONAL);
      // Fast case: no forward jumps, possible backward ones.  Remove
      // constants and copies above the watermark on the fall-through
      // frame and use it as the entry frame.
      cgen()->frame()->MakeMergable();
      entry_frame_ = new VirtualFrame(cgen()->frame());
    }
    __ bind(&entry_label_);
    return;
  }

  if (direction_ == FORWARD_ONLY &&
      !cgen()->has_valid_frame() &&
      reaching_frames_.length() == 1) {
    // Fast case: no fall-through, a single forward jump, and no
    // possible backward jumps.  Pick up the only reaching frame, take
    // ownership of it, and use it for the block about to be emitted.
    VirtualFrame* frame = reaching_frames_[0];
    RegisterFile empty;
    cgen()->SetFrame(frame, &empty);
    reaching_frames_[0] = NULL;
    __ bind(&merge_labels_[0]);

    // The stack pointer can be floating above the top of the
    // virtual frame before the bind.  Afterward, it should not.
    int difference = frame->stack_pointer_ - (frame->element_count() - 1);
    if (difference > 0) {
      frame->stack_pointer_ -= difference;
      __ add(Operand(esp), Immediate(difference * kPointerSize));
    }

    __ bind(&entry_label_);
    return;
  }

  // If there is a current frame, record it as the fall-through.  It
  // is owned by the reaching frames for now.
  bool had_fall_through = false;
  if (cgen()->has_valid_frame()) {
    had_fall_through = true;
    AddReachingFrame(cgen()->frame());  // Return value ignored.
    RegisterFile empty;
    cgen()->SetFrame(NULL, &empty);
  }

  // Compute the frame to use for entry to the block.
  ComputeEntryFrame();

  // Some moves required to merge to an expected frame require purely
  // frame state changes, and do not require any code generation.
  // Perform those first to increase the possibility of finding equal
  // frames below.
  for (int i = 0; i < reaching_frames_.length(); i++) {
    if (reaching_frames_[i] != NULL) {
      reaching_frames_[i]->PrepareMergeTo(entry_frame_);
    }
  }

  if (is_linked()) {
    // There were forward jumps.  Handle merging the reaching frames
    // to the entry frame.

    // Loop over the (non-null) reaching frames and process any that
    // need merge code.  Iterate backwards through the list to handle
    // the fall-through frame first.  Set frames that will be
    // processed after 'i' to NULL if we want to avoid processing
    // them.
    for (int i = reaching_frames_.length() - 1; i >= 0; i--) {
      VirtualFrame* frame = reaching_frames_[i];

      if (frame != NULL) {
        // Does the frame (probably) need merge code?
        if (!frame->Equals(entry_frame_)) {
          // We could have a valid frame as the fall through to the
          // binding site or as the fall through from a previous merge
          // code block.  Jump around the code we are about to
          // generate.
          if (cgen()->has_valid_frame()) {
            cgen()->DeleteFrame();
            __ jmp(&entry_label_);
          }
          // Pick up the frame for this block.  Assume ownership if
          // there cannot be backward jumps.
          RegisterFile empty;
          if (direction_ == BIDIRECTIONAL) {
            cgen()->SetFrame(new VirtualFrame(frame), &empty);
          } else {
            cgen()->SetFrame(frame, &empty);
            reaching_frames_[i] = NULL;
          }
          __ bind(&merge_labels_[i]);

          // Loop over the remaining (non-null) reaching frames,
          // looking for any that can share merge code with this one.
          for (int j = 0; j < i; j++) {
            VirtualFrame* other = reaching_frames_[j];
            if (other != NULL && other->Equals(cgen()->frame())) {
              // Set the reaching frame element to null to avoid
              // processing it later, and then bind its entry label.
              reaching_frames_[j] = NULL;
              __ bind(&merge_labels_[j]);
            }
          }

          // Emit the merge code.
          cgen()->frame()->MergeTo(entry_frame_);
        } else if (i == reaching_frames_.length() - 1 && had_fall_through) {
          // If this is the fall through frame, and it didn't need
          // merge code, we need to pick up the frame so we can jump
          // around subsequent merge blocks if necessary.
          RegisterFile empty;
          cgen()->SetFrame(frame, &empty);
          reaching_frames_[i] = NULL;
        }
      }
    }

    // The code generator may not have a current frame if there was no
    // fall through and none of the reaching frames needed merging.
    // In that case, clone the entry frame as the current frame.
    if (!cgen()->has_valid_frame()) {
      RegisterFile empty;
      cgen()->SetFrame(new VirtualFrame(entry_frame_), &empty);
    }

    // There may be unprocessed reaching frames that did not need
    // merge code.  They will have unbound merge labels.  Bind their
    // merge labels to be the same as the entry label and deallocate
    // them.
    for (int i = 0; i < reaching_frames_.length(); i++) {
      if (!merge_labels_[i].is_bound()) {
        reaching_frames_[i] = NULL;
        __ bind(&merge_labels_[i]);
      }
    }

    // There are non-NULL reaching frames with bound labels for each
    // merge block, but only on backward targets.
  } else {
    // There were no forward jumps.  There must be a current frame and
    // this must be a bidirectional target.
    ASSERT(reaching_frames_.length() == 1);
    ASSERT(reaching_frames_[0] != NULL);
    ASSERT(direction_ == BIDIRECTIONAL);

    // Use a copy of the reaching frame so the original can be saved
    // for possible reuse as a backward merge block.
    RegisterFile empty;
    cgen()->SetFrame(new VirtualFrame(reaching_frames_[0]), &empty);
    __ bind(&merge_labels_[0]);
    cgen()->frame()->MergeTo(entry_frame_);
  }

  __ bind(&entry_label_);
}


void BreakTarget::Jump() {
  // Drop leftover statement state from the frame before merging, without
  // emitting code.
  ASSERT(cgen()->has_valid_frame());
  int count = cgen()->frame()->height() - expected_height_;
  cgen()->frame()->ForgetElements(count);
  DoJump();
}


void BreakTarget::Jump(Result* arg) {
  // Drop leftover statement state from the frame before merging, without
  // emitting code.
  ASSERT(cgen()->has_valid_frame());
  int count = cgen()->frame()->height() - expected_height_;
  cgen()->frame()->ForgetElements(count);
  cgen()->frame()->Push(arg);
  DoJump();
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
  // Drop leftover statement state from the frame before merging, even on
  // the fall through.  This is so we can bind the return target with state
  // on the frame.
  if (cgen()->has_valid_frame()) {
    int count = cgen()->frame()->height() - expected_height_;
    cgen()->frame()->ForgetElements(count);
  }
  DoBind();
}


void BreakTarget::Bind(Result* arg) {
#ifdef DEBUG
  // All the forward-reaching frames should have been adjusted at the
  // jumps to this target.
  for (int i = 0; i < reaching_frames_.length(); i++) {
    ASSERT(reaching_frames_[i] == NULL ||
           reaching_frames_[i]->height() == expected_height_ + 1);
  }
#endif
  // Drop leftover statement state from the frame before merging, even on
  // the fall through.  This is so we can bind the return target with state
  // on the frame.
  if (cgen()->has_valid_frame()) {
    int count = cgen()->frame()->height() - expected_height_;
    cgen()->frame()->ForgetElements(count);
    cgen()->frame()->Push(arg);
  }
  DoBind();
  *arg = cgen()->frame()->Pop();
}


#undef __


} }  // namespace v8::internal
