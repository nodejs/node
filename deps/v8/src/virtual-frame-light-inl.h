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

#ifndef V8_VIRTUAL_FRAME_LIGHT_INL_H_
#define V8_VIRTUAL_FRAME_LIGHT_INL_H_

#include "codegen.h"
#include "register-allocator.h"
#include "scopes.h"
#include "type-info.h"

#include "codegen-inl.h"
#include "jump-target-light-inl.h"

namespace v8 {
namespace internal {

VirtualFrame::VirtualFrame(InvalidVirtualFrameInitializer* dummy)
    : element_count_(0),
      top_of_stack_state_(NO_TOS_REGISTERS),
      register_allocation_map_(0) { }


// On entry to a function, the virtual frame already contains the receiver,
// the parameters, and a return address.  All frame elements are in memory.
VirtualFrame::VirtualFrame()
    : element_count_(parameter_count() + 2),
      top_of_stack_state_(NO_TOS_REGISTERS),
      register_allocation_map_(0) { }


// When cloned, a frame is a deep copy of the original.
VirtualFrame::VirtualFrame(VirtualFrame* original)
    : element_count_(original->element_count()),
      top_of_stack_state_(original->top_of_stack_state_),
      register_allocation_map_(original->register_allocation_map_) { }


bool VirtualFrame::Equals(const VirtualFrame* other) {
  ASSERT(element_count() == other->element_count());
  if (top_of_stack_state_ != other->top_of_stack_state_) return false;
  if (register_allocation_map_ != other->register_allocation_map_) return false;

  return true;
}


void VirtualFrame::PrepareForReturn() {
  SpillAll();
}


VirtualFrame::RegisterAllocationScope::RegisterAllocationScope(
    CodeGenerator* cgen)
  : cgen_(cgen),
    old_is_spilled_(SpilledScope::is_spilled_) {
  SpilledScope::is_spilled_ = false;
  if (old_is_spilled_) {
    VirtualFrame* frame = cgen->frame();
    if (frame != NULL) {
      frame->AssertIsSpilled();
    }
  }
}


VirtualFrame::RegisterAllocationScope::~RegisterAllocationScope() {
  SpilledScope::is_spilled_ = old_is_spilled_;
  if (old_is_spilled_) {
    VirtualFrame* frame = cgen_->frame();
    if (frame != NULL) {
      frame->SpillAll();
    }
  }
}


CodeGenerator* VirtualFrame::cgen() const {
  return CodeGeneratorScope::Current();
}


MacroAssembler* VirtualFrame::masm() { return cgen()->masm(); }


void VirtualFrame::CallStub(CodeStub* stub, int arg_count) {
  if (arg_count != 0) Forget(arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  masm()->CallStub(stub);
}


int VirtualFrame::parameter_count() const {
  return cgen()->scope()->num_parameters();
}


int VirtualFrame::local_count() const {
  return cgen()->scope()->num_stack_slots();
}


int VirtualFrame::frame_pointer() const { return parameter_count() + 3; }


int VirtualFrame::context_index() { return frame_pointer() - 1; }


int VirtualFrame::function_index() { return frame_pointer() - 2; }


int VirtualFrame::local0_index() const { return frame_pointer() + 2; }


int VirtualFrame::fp_relative(int index) {
  ASSERT(index < element_count());
  ASSERT(frame_pointer() < element_count());  // FP is on the frame.
  return (frame_pointer() - index) * kPointerSize;
}


int VirtualFrame::expression_base_index() const {
  return local0_index() + local_count();
}


int VirtualFrame::height() const {
  return element_count() - expression_base_index();
}


MemOperand VirtualFrame::LocalAt(int index) {
  ASSERT(0 <= index);
  ASSERT(index < local_count());
  return MemOperand(fp, kLocal0Offset - index * kPointerSize);
}

} }  // namespace v8::internal

#endif  // V8_VIRTUAL_FRAME_LIGHT_INL_H_
