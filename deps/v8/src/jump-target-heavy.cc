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

namespace v8 {
namespace internal {


void JumpTarget::Jump(Result* arg) {
  ASSERT(cgen()->has_valid_frame());

  cgen()->frame()->Push(arg);
  DoJump();
}


void JumpTarget::Branch(Condition cc, Result* arg, Hint hint) {
  ASSERT(cgen()->has_valid_frame());

  // We want to check that non-frame registers at the call site stay in
  // the same registers on the fall-through branch.
#ifdef DEBUG
  Result::Type arg_type = arg->type();
  Register arg_reg = arg->is_register() ? arg->reg() : no_reg;
#endif

  cgen()->frame()->Push(arg);
  DoBranch(cc, hint);
  *arg = cgen()->frame()->Pop();

  ASSERT(arg->type() == arg_type);
  ASSERT(!arg->is_register() || arg->reg().is(arg_reg));
}


void JumpTarget::Branch(Condition cc, Result* arg0, Result* arg1, Hint hint) {
  ASSERT(cgen()->has_valid_frame());

  // We want to check that non-frame registers at the call site stay in
  // the same registers on the fall-through branch.
#ifdef DEBUG
  Result::Type arg0_type = arg0->type();
  Register arg0_reg = arg0->is_register() ? arg0->reg() : no_reg;
  Result::Type arg1_type = arg1->type();
  Register arg1_reg = arg1->is_register() ? arg1->reg() : no_reg;
#endif

  cgen()->frame()->Push(arg0);
  cgen()->frame()->Push(arg1);
  DoBranch(cc, hint);
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();

  ASSERT(arg0->type() == arg0_type);
  ASSERT(!arg0->is_register() || arg0->reg().is(arg0_reg));
  ASSERT(arg1->type() == arg1_type);
  ASSERT(!arg1->is_register() || arg1->reg().is(arg1_reg));
}


void BreakTarget::Branch(Condition cc, Result* arg, Hint hint) {
  ASSERT(cgen()->has_valid_frame());

  int count = cgen()->frame()->height() - expected_height_;
  if (count > 0) {
    // We negate and branch here rather than using DoBranch's negate
    // and branch.  This gives us a hook to remove statement state
    // from the frame.
    JumpTarget fall_through;
    // Branch to fall through will not negate, because it is a
    // forward-only target.
    fall_through.Branch(NegateCondition(cc), NegateHint(hint));
    Jump(arg);  // May emit merge code here.
    fall_through.Bind();
  } else {
#ifdef DEBUG
    Result::Type arg_type = arg->type();
    Register arg_reg = arg->is_register() ? arg->reg() : no_reg;
#endif
    cgen()->frame()->Push(arg);
    DoBranch(cc, hint);
    *arg = cgen()->frame()->Pop();
    ASSERT(arg->type() == arg_type);
    ASSERT(!arg->is_register() || arg->reg().is(arg_reg));
  }
}


void JumpTarget::Bind(Result* arg) {
  if (cgen()->has_valid_frame()) {
    cgen()->frame()->Push(arg);
  }
  DoBind();
  *arg = cgen()->frame()->Pop();
}


void JumpTarget::Bind(Result* arg0, Result* arg1) {
  if (cgen()->has_valid_frame()) {
    cgen()->frame()->Push(arg0);
    cgen()->frame()->Push(arg1);
  }
  DoBind();
  *arg1 = cgen()->frame()->Pop();
  *arg0 = cgen()->frame()->Pop();
}


void JumpTarget::ComputeEntryFrame() {
  // Given: a collection of frames reaching by forward CFG edges and
  // the directionality of the block.  Compute: an entry frame for the
  // block.

  Counters::compute_entry_frame.Increment();
#ifdef DEBUG
  if (compiling_deferred_code_) {
    ASSERT(reaching_frames_.length() > 1);
    VirtualFrame* frame = reaching_frames_[0];
    bool all_identical = true;
    for (int i = 1; i < reaching_frames_.length(); i++) {
      if (!frame->Equals(reaching_frames_[i])) {
        all_identical = false;
        break;
      }
    }
    ASSERT(!all_identical || all_identical);
  }
#endif

  // Choose an initial frame.
  VirtualFrame* initial_frame = reaching_frames_[0];

  // A list of pointers to frame elements in the entry frame.  NULL
  // indicates that the element has not yet been determined.
  int length = initial_frame->element_count();
  ZoneList<FrameElement*> elements(length);

  // Initially populate the list of elements based on the initial
  // frame.
  for (int i = 0; i < length; i++) {
    FrameElement element = initial_frame->elements_[i];
    // We do not allow copies or constants in bidirectional frames.
    if (direction_ == BIDIRECTIONAL) {
      if (element.is_constant() || element.is_copy()) {
        elements.Add(NULL);
        continue;
      }
    }
    elements.Add(&initial_frame->elements_[i]);
  }

  // Compute elements based on the other reaching frames.
  if (reaching_frames_.length() > 1) {
    for (int i = 0; i < length; i++) {
      FrameElement* element = elements[i];
      for (int j = 1; j < reaching_frames_.length(); j++) {
        // Element computation is monotonic: new information will not
        // change our decision about undetermined or invalid elements.
        if (element == NULL || !element->is_valid()) break;

        FrameElement* other = &reaching_frames_[j]->elements_[i];
        element = element->Combine(other);
        if (element != NULL && !element->is_copy()) {
          ASSERT(other != NULL);
          // We overwrite the number information of one of the incoming frames.
          // This is safe because we only use the frame for emitting merge code.
          // The number information of incoming frames is not used anymore.
          element->set_type_info(TypeInfo::Combine(element->type_info(),
                                                   other->type_info()));
        }
      }
      elements[i] = element;
    }
  }

  // Build the new frame.  A freshly allocated frame has memory elements
  // for the parameters and some platform-dependent elements (e.g.,
  // return address).  Replace those first.
  entry_frame_ = new VirtualFrame();
  int index = 0;
  for (; index < entry_frame_->element_count(); index++) {
    FrameElement* target = elements[index];
    // If the element is determined, set it now.  Count registers.  Mark
    // elements as copied exactly when they have a copy.  Undetermined
    // elements are initially recorded as if in memory.
    if (target != NULL) {
      entry_frame_->elements_[index] = *target;
      InitializeEntryElement(index, target);
    }
  }
  // Then fill in the rest of the frame with new elements.
  for (; index < length; index++) {
    FrameElement* target = elements[index];
    if (target == NULL) {
      entry_frame_->elements_.Add(
          FrameElement::MemoryElement(TypeInfo::Uninitialized()));
    } else {
      entry_frame_->elements_.Add(*target);
      InitializeEntryElement(index, target);
    }
  }

  // Allocate any still-undetermined frame elements to registers or
  // memory, from the top down.
  for (int i = length - 1; i >= 0; i--) {
    if (elements[i] == NULL) {
      // Loop over all the reaching frames to check whether the element
      // is synced on all frames and to count the registers it occupies.
      bool is_synced = true;
      RegisterFile candidate_registers;
      int best_count = kMinInt;
      int best_reg_num = RegisterAllocator::kInvalidRegister;
      TypeInfo info = TypeInfo::Uninitialized();

      for (int j = 0; j < reaching_frames_.length(); j++) {
        FrameElement element = reaching_frames_[j]->elements_[i];
        if (direction_ == BIDIRECTIONAL) {
          info = TypeInfo::Unknown();
        } else if (!element.is_copy()) {
          info = TypeInfo::Combine(info, element.type_info());
        } else {
          // New elements will not be copies, so get number information from
          // backing element in the reaching frame.
          info = TypeInfo::Combine(info,
            reaching_frames_[j]->elements_[element.index()].type_info());
        }
        is_synced = is_synced && element.is_synced();
        if (element.is_register() && !entry_frame_->is_used(element.reg())) {
          // Count the register occurrence and remember it if better
          // than the previous best.
          int num = RegisterAllocator::ToNumber(element.reg());
          candidate_registers.Use(num);
          if (candidate_registers.count(num) > best_count) {
            best_count = candidate_registers.count(num);
            best_reg_num = num;
          }
        }
      }

      // We must have a number type information now (not for copied elements).
      ASSERT(entry_frame_->elements_[i].is_copy()
             || !info.IsUninitialized());

      // If the value is synced on all frames, put it in memory.  This
      // costs nothing at the merge code but will incur a
      // memory-to-register move when the value is needed later.
      if (is_synced) {
        // Already recorded as a memory element.
        // Set combined number info.
        entry_frame_->elements_[i].set_type_info(info);
        continue;
      }

      // Try to put it in a register.  If there was no best choice
      // consider any free register.
      if (best_reg_num == RegisterAllocator::kInvalidRegister) {
        for (int j = 0; j < RegisterAllocator::kNumRegisters; j++) {
          if (!entry_frame_->is_used(j)) {
            best_reg_num = j;
            break;
          }
        }
      }

      if (best_reg_num != RegisterAllocator::kInvalidRegister) {
        // If there was a register choice, use it.  Preserve the copied
        // flag on the element.
        bool is_copied = entry_frame_->elements_[i].is_copied();
        Register reg = RegisterAllocator::ToRegister(best_reg_num);
        entry_frame_->elements_[i] =
            FrameElement::RegisterElement(reg, FrameElement::NOT_SYNCED,
                                          TypeInfo::Uninitialized());
        if (is_copied) entry_frame_->elements_[i].set_copied();
        entry_frame_->set_register_location(reg, i);
      }
      // Set combined number info.
      entry_frame_->elements_[i].set_type_info(info);
    }
  }

  // If we have incoming backward edges assert we forget all number information.
#ifdef DEBUG
  if (direction_ == BIDIRECTIONAL) {
    for (int i = 0; i < length; ++i) {
      if (!entry_frame_->elements_[i].is_copy()) {
        ASSERT(entry_frame_->elements_[i].type_info().IsUnknown());
      }
    }
  }
#endif

  // The stack pointer is at the highest synced element or the base of
  // the expression stack.
  int stack_pointer = length - 1;
  while (stack_pointer >= entry_frame_->expression_base_index() &&
         !entry_frame_->elements_[stack_pointer].is_synced()) {
    stack_pointer--;
  }
  entry_frame_->stack_pointer_ = stack_pointer;
}


DeferredCode::DeferredCode()
    : masm_(CodeGeneratorScope::Current()->masm()),
      statement_position_(masm_->current_statement_position()),
      position_(masm_->current_position()) {
  ASSERT(statement_position_ != RelocInfo::kNoPosition);
  ASSERT(position_ != RelocInfo::kNoPosition);

  CodeGeneratorScope::Current()->AddDeferred(this);
#ifdef DEBUG
  comment_ = "";
#endif

  // Copy the register locations from the code generator's frame.
  // These are the registers that will be spilled on entry to the
  // deferred code and restored on exit.
  VirtualFrame* frame = CodeGeneratorScope::Current()->frame();
  int sp_offset = frame->fp_relative(frame->stack_pointer_);
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    int loc = frame->register_location(i);
    if (loc == VirtualFrame::kIllegalIndex) {
      registers_[i] = kIgnore;
    } else if (frame->elements_[loc].is_synced()) {
      // Needs to be restored on exit but not saved on entry.
      registers_[i] = frame->fp_relative(loc) | kSyncedFlag;
    } else {
      int offset = frame->fp_relative(loc);
      registers_[i] = (offset < sp_offset) ? kPush : offset;
    }
  }
}

} }  // namespace v8::internal
