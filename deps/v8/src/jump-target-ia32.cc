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
#include "register-allocator-inl.h"

namespace v8 { namespace internal {

// -------------------------------------------------------------------------
// JumpTarget implementation.

#define __ masm_->

void JumpTarget::DoJump() {
  ASSERT(cgen_ != NULL);
  ASSERT(cgen_->has_valid_frame());
  // Live non-frame registers are not allowed at unconditional jumps
  // because we have no way of invalidating the corresponding results
  // which are still live in the C++ code.
  ASSERT(cgen_->HasValidEntryRegisters());

  if (is_bound()) {
    // Backward jump.  There is an expected frame to merge to.
    ASSERT(direction_ == BIDIRECTIONAL);
    cgen_->frame()->MergeTo(entry_frame_);
    cgen_->DeleteFrame();
    __ jmp(&entry_label_);
  } else {
    // Forward jump.  The current frame is added to the end of the list
    // of frames reaching the target block and a jump to the merge code
    // is emitted.
    AddReachingFrame(cgen_->frame());
    RegisterFile empty;
    cgen_->SetFrame(NULL, &empty);
    __ jmp(&merge_labels_.last());
  }

  is_linked_ = !is_bound_;
}


void JumpTarget::DoBranch(Condition cc, Hint hint) {
  ASSERT(cgen_ != NULL);
  ASSERT(cgen_->has_valid_frame());

  if (is_bound()) {
    ASSERT(direction_ == BIDIRECTIONAL);
    // Backward branch.  We have an expected frame to merge to on the
    // backward edge.

    // Swap the current frame for a copy (we do the swapping to get
    // the off-frame registers off the fall through) to use for the
    // branch.
    VirtualFrame* fall_through_frame = cgen_->frame();
    VirtualFrame* branch_frame = new VirtualFrame(fall_through_frame);
    RegisterFile non_frame_registers = RegisterAllocator::Reserved();
    cgen_->SetFrame(branch_frame, &non_frame_registers);

    // Check if we can avoid merge code.
    cgen_->frame()->PrepareMergeTo(entry_frame_);
    if (cgen_->frame()->Equals(entry_frame_)) {
      // Branch right in to the block.
      cgen_->DeleteFrame();
      __ j(cc, &entry_label_, hint);
      cgen_->SetFrame(fall_through_frame, &non_frame_registers);
      return;
    }

    // Check if we can reuse existing merge code.
    for (int i = 0; i < reaching_frames_.length(); i++) {
      if (reaching_frames_[i] != NULL &&
          cgen_->frame()->Equals(reaching_frames_[i])) {
        // Branch to the merge code.
        cgen_->DeleteFrame();
        __ j(cc, &merge_labels_[i], hint);
        cgen_->SetFrame(fall_through_frame, &non_frame_registers);
        return;
      }
    }

    // To emit the merge code here, we negate the condition and branch
    // around the merge code on the fall through path.
    Label original_fall_through;
    __ j(NegateCondition(cc), &original_fall_through, NegateHint(hint));
    cgen_->frame()->MergeTo(entry_frame_);
    cgen_->DeleteFrame();
    __ jmp(&entry_label_);
    cgen_->SetFrame(fall_through_frame, &non_frame_registers);
    __ bind(&original_fall_through);

  } else {
    // Forward branch.  A copy of the current frame is added to the end
    // of the list of frames reaching the target block and a branch to
    // the merge code is emitted.
    AddReachingFrame(new VirtualFrame(cgen_->frame()));
    __ j(cc, &merge_labels_.last(), hint);
    is_linked_ = true;
  }
}


void JumpTarget::Call() {
  // Call is used to push the address of the catch block on the stack as
  // a return address when compiling try/catch and try/finally.  We
  // fully spill the frame before making the call.  The expected frame
  // at the label (which should be the only one) is the spilled current
  // frame plus an in-memory return address.  The "fall-through" frame
  // at the return site is the spilled current frame.
  ASSERT(cgen_ != NULL);
  ASSERT(cgen_->has_valid_frame());
  // There are no non-frame references across the call.
  ASSERT(cgen_->HasValidEntryRegisters());
  ASSERT(!is_linked());

  cgen_->frame()->SpillAll();
  VirtualFrame* target_frame = new VirtualFrame(cgen_->frame());
  target_frame->Adjust(1);
  AddReachingFrame(target_frame);
  __ call(&merge_labels_.last());

  is_linked_ = !is_bound_;
}


void JumpTarget::DoBind(int mergable_elements) {
  ASSERT(cgen_ != NULL);
  ASSERT(!is_bound());

  // Live non-frame registers are not allowed at the start of a basic
  // block.
  ASSERT(!cgen_->has_valid_frame() || cgen_->HasValidEntryRegisters());

  if (direction_ == FORWARD_ONLY) {
    // A simple case: no forward jumps and no possible backward jumps.
    if (!is_linked()) {
      // The stack pointer can be floating above the top of the
      // virtual frame before the bind.  Afterward, it should not.
      ASSERT(cgen_->has_valid_frame());
      VirtualFrame* frame = cgen_->frame();
      int difference =
          frame->stack_pointer_ - (frame->elements_.length() - 1);
      if (difference > 0) {
        frame->stack_pointer_ -= difference;
        __ add(Operand(esp), Immediate(difference * kPointerSize));
      }

      is_bound_ = true;
      return;
    }

    // Another simple case: no fall through, a single forward jump,
    // and no possible backward jumps.
    if (!cgen_->has_valid_frame() && reaching_frames_.length() == 1) {
      // Pick up the only reaching frame, take ownership of it, and
      // use it for the block about to be emitted.
      VirtualFrame* frame = reaching_frames_[0];
      RegisterFile reserved = RegisterAllocator::Reserved();
      cgen_->SetFrame(frame, &reserved);
      reaching_frames_[0] = NULL;
      __ bind(&merge_labels_[0]);

      // The stack pointer can be floating above the top of the
      // virtual frame before the bind.  Afterward, it should not.
      int difference =
          frame->stack_pointer_ - (frame->elements_.length() - 1);
      if (difference > 0) {
        frame->stack_pointer_ -= difference;
        __ add(Operand(esp), Immediate(difference * kPointerSize));
      }

      is_linked_ = false;
      is_bound_ = true;
      return;
    }
  }

  // If there is a current frame, record it as the fall-through.  It
  // is owned by the reaching frames for now.
  bool had_fall_through = false;
  if (cgen_->has_valid_frame()) {
    had_fall_through = true;
    AddReachingFrame(cgen_->frame());
    RegisterFile empty;
    cgen_->SetFrame(NULL, &empty);
  }

  // Compute the frame to use for entry to the block.
  ComputeEntryFrame(mergable_elements);

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
          if (cgen_->has_valid_frame()) {
            cgen_->DeleteFrame();
            __ jmp(&entry_label_);
          }
          // Pick up the frame for this block.  Assume ownership if
          // there cannot be backward jumps.
          RegisterFile reserved = RegisterAllocator::Reserved();
          if (direction_ == BIDIRECTIONAL) {
            cgen_->SetFrame(new VirtualFrame(frame), &reserved);
          } else {
            cgen_->SetFrame(frame, &reserved);
            reaching_frames_[i] = NULL;
          }
          __ bind(&merge_labels_[i]);

          // Loop over the remaining (non-null) reaching frames,
          // looking for any that can share merge code with this one.
          for (int j = 0; j < i; j++) {
            VirtualFrame* other = reaching_frames_[j];
            if (other != NULL && other->Equals(cgen_->frame())) {
              // Set the reaching frame element to null to avoid
              // processing it later, and then bind its entry label.
              delete other;
              reaching_frames_[j] = NULL;
              __ bind(&merge_labels_[j]);
            }
          }

          // Emit the merge code.
          cgen_->frame()->MergeTo(entry_frame_);
        } else if (i == reaching_frames_.length() - 1 && had_fall_through) {
          // If this is the fall through frame, and it didn't need
          // merge code, we need to pick up the frame so we can jump
          // around subsequent merge blocks if necessary.
          RegisterFile reserved = RegisterAllocator::Reserved();
          cgen_->SetFrame(frame, &reserved);
          reaching_frames_[i] = NULL;
        }
      }
    }

    // The code generator may not have a current frame if there was no
    // fall through and none of the reaching frames needed merging.
    // In that case, clone the entry frame as the current frame.
    if (!cgen_->has_valid_frame()) {
      RegisterFile reserved_registers = RegisterAllocator::Reserved();
      cgen_->SetFrame(new VirtualFrame(entry_frame_), &reserved_registers);
    }

    // There is certainly a current frame equal to the entry frame.
    // Bind the entry frame label.
    __ bind(&entry_label_);

    // There may be unprocessed reaching frames that did not need
    // merge code.  They will have unbound merge labels.  Bind their
    // merge labels to be the same as the entry label and deallocate
    // them.
    for (int i = 0; i < reaching_frames_.length(); i++) {
      if (!merge_labels_[i].is_bound()) {
        delete reaching_frames_[i];
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
    RegisterFile reserved = RegisterAllocator::Reserved();
    cgen_->SetFrame(new VirtualFrame(reaching_frames_[0]), &reserved);
    __ bind(&merge_labels_[0]);
    cgen_->frame()->MergeTo(entry_frame_);
    __ bind(&entry_label_);
  }

  is_linked_ = false;
  is_bound_ = true;
}

#undef __


} }  // namespace v8::internal
