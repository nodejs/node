// Copyright 2009 the V8 project authors. All rights reserved.
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
#include "scopes.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

// -------------------------------------------------------------------------
// VirtualFrame implementation.

// On entry to a function, the virtual frame already contains the receiver,
// the parameters, and a return address.  All frame elements are in memory.
VirtualFrame::VirtualFrame()
    : elements_(parameter_count() + local_count() + kPreallocatedElements),
      stack_pointer_(parameter_count() + 1) {  // 0-based index of TOS.
  for (int i = 0; i <= stack_pointer_; i++) {
    elements_.Add(FrameElement::MemoryElement());
  }
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    register_locations_[i] = kIllegalIndex;
  }
}


void VirtualFrame::Enter() {
  // Registers live on entry to a JS frame:
  //   rsp: stack pointer, points to return address from this function.
  //   rbp: base pointer, points to previous JS, ArgumentsAdaptor, or
  //        Trampoline frame.
  //   rsi: context of this function call.
  //   rdi: pointer to this function object.
  Comment cmnt(masm(), "[ Enter JS frame");

#ifdef DEBUG
  // Verify that rdi contains a JS function.  The following code
  // relies on rax being available for use.
  __ testq(rdi, Immediate(kSmiTagMask));
  __ Check(not_zero,
           "VirtualFrame::Enter - rdi is not a function (smi check).");
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rax);
  __ Check(equal,
           "VirtualFrame::Enter - rdi is not a function (map check).");
#endif

  EmitPush(rbp);

  __ movq(rbp, rsp);

  // Store the context in the frame.  The context is kept in rsi and a
  // copy is stored in the frame.  The external reference to rsi
  // remains.
  EmitPush(rsi);

  // Store the function in the frame.  The frame owns the register
  // reference now (ie, it can keep it in rdi or spill it later).
  Push(rdi);
  // SyncElementAt(element_count() - 1);
  cgen()->allocator()->Unuse(rdi);
}


void VirtualFrame::Exit() {
  Comment cmnt(masm(), "[ Exit JS frame");
  // Record the location of the JS exit code for patching when setting
  // break point.
  __ RecordJSReturn();

  // Avoid using the leave instruction here, because it is too
  // short. We need the return sequence to be a least the size of a
  // call instruction to support patching the exit code in the
  // debugger. See VisitReturnStatement for the full return sequence.
  // TODO(X64): A patched call will be very long now.  Make sure we
  // have enough room.
  __ movq(rsp, rbp);
  stack_pointer_ = frame_pointer();
  for (int i = element_count() - 1; i > stack_pointer_; i--) {
    FrameElement last = elements_.RemoveLast();
    if (last.is_register()) {
      Unuse(last.reg());
    }
  }

  EmitPop(rbp);
}


void VirtualFrame::EmitPop(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  stack_pointer_--;
  elements_.RemoveLast();
  __ pop(reg);
}


void VirtualFrame::EmitPop(const Operand& operand) {
  ASSERT(stack_pointer_ == element_count() - 1);
  stack_pointer_--;
  elements_.RemoveLast();
  __ pop(operand);
}


void VirtualFrame::EmitPush(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(reg);
}


void VirtualFrame::EmitPush(const Operand& operand) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(operand);
}


void VirtualFrame::EmitPush(Immediate immediate) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(immediate);
}


void VirtualFrame::Drop(int a) {
  UNIMPLEMENTED();
}

int VirtualFrame::InvalidateFrameSlotAt(int a) {
  UNIMPLEMENTED();
  return -1;
}

void VirtualFrame::MergeTo(VirtualFrame* a) {
  UNIMPLEMENTED();
}

Result VirtualFrame::Pop() {
  UNIMPLEMENTED();
  return Result(NULL);
}

Result VirtualFrame::RawCallStub(CodeStub* a) {
  UNIMPLEMENTED();
  return Result(NULL);
}

void VirtualFrame::SyncElementBelowStackPointer(int a) {
  UNIMPLEMENTED();
}

void VirtualFrame::SyncElementByPushing(int a) {
  UNIMPLEMENTED();
}

void VirtualFrame::SyncRange(int a, int b) {
  UNIMPLEMENTED();
}


#undef __

} }  // namespace v8::internal
