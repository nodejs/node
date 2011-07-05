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

#if defined(V8_TARGET_ARCH_MIPS)

#include "codegen-inl.h"
#include "register-allocator-inl.h"
#include "scopes.h"
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// VirtualFrame implementation.

#define __ ACCESS_MASM(masm())

void VirtualFrame::SyncElementBelowStackPointer(int index) {
  UNREACHABLE();
}


void VirtualFrame::SyncElementByPushing(int index) {
  UNREACHABLE();
}


void VirtualFrame::SyncRange(int begin, int end) {
  // All elements are in memory on MIPS (ie, synced).
#ifdef DEBUG
  for (int i = begin; i <= end; i++) {
    ASSERT(elements_[i].is_synced());
  }
#endif
}


void VirtualFrame::MergeTo(VirtualFrame* expected) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::Enter() {
  // TODO(MIPS): Implement DEBUG

  // We are about to push four values to the frame.
  Adjust(4);
  __ MultiPush(ra.bit() | fp.bit() | cp.bit() | a1.bit());
  // Adjust FP to point to saved FP.
  __ addiu(fp, sp, 2 * kPointerSize);
}


void VirtualFrame::Exit() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::AllocateStackSlots() {
  int count = local_count();
  if (count > 0) {
    Comment cmnt(masm(), "[ Allocate space for locals");
    Adjust(count);
      // Initialize stack slots with 'undefined' value.
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ addiu(sp, sp, -count * kPointerSize);
    for (int i = 0; i < count; i++) {
      __ sw(t0, MemOperand(sp, (count-i-1)*kPointerSize));
    }
  }
}


void VirtualFrame::SaveContextRegister() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::RestoreContextRegister() {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::PushReceiverSlotAddress() {
  UNIMPLEMENTED_MIPS();
}


int VirtualFrame::InvalidateFrameSlotAt(int index) {
  return kIllegalIndex;
}


void VirtualFrame::TakeFrameSlotAt(int index) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::StoreToFrameSlotAt(int index) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::PushTryHandler(HandlerType type) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::RawCallStub(CodeStub* stub) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallStub(CodeStub* stub, Result* arg) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallStub(CodeStub* stub, Result* arg0, Result* arg1) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallRuntime(Runtime::Function* f, int arg_count) {
  PrepareForCall(arg_count, arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(f, arg_count);
}


void VirtualFrame::CallRuntime(Runtime::FunctionId id, int arg_count) {
  PrepareForCall(arg_count, arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(id, arg_count);
}


void VirtualFrame::CallAlignedRuntime(Runtime::Function* f, int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallAlignedRuntime(Runtime::FunctionId id, int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::InvokeBuiltin(Builtins::JavaScript id,
                                 InvokeJSFlags flags,
                                 Result* arg_count_register,
                                 int arg_count) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  int dropped_args) {
  switch (code->kind()) {
    case Code::CALL_IC:
      break;
    case Code::FUNCTION:
      UNIMPLEMENTED_MIPS();
      break;
    case Code::KEYED_LOAD_IC:
      UNIMPLEMENTED_MIPS();
      break;
    case Code::LOAD_IC:
      UNIMPLEMENTED_MIPS();
      break;
    case Code::KEYED_STORE_IC:
      UNIMPLEMENTED_MIPS();
      break;
    case Code::STORE_IC:
      UNIMPLEMENTED_MIPS();
      break;
    case Code::BUILTIN:
      UNIMPLEMENTED_MIPS();
      break;
    default:
      UNREACHABLE();
      break;
  }
  Forget(dropped_args);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ Call(code, rmode);
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  Result* arg,
                                  int dropped_args) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::CallCodeObject(Handle<Code> code,
                                  RelocInfo::Mode rmode,
                                  Result* arg0,
                                  Result* arg1,
                                  int dropped_args,
                                  bool set_auto_args_slots) {
  UNIMPLEMENTED_MIPS();
}


void VirtualFrame::Drop(int count) {
  ASSERT(count >= 0);
  ASSERT(height() >= count);
  int num_virtual_elements = (element_count() - 1) - stack_pointer_;

  // Emit code to lower the stack pointer if necessary.
  if (num_virtual_elements < count) {
    int num_dropped = count - num_virtual_elements;
    stack_pointer_ -= num_dropped;
    __ addiu(sp, sp, num_dropped * kPointerSize);
  }

  // Discard elements from the virtual frame and free any registers.
  for (int i = 0; i < count; i++) {
    FrameElement dropped = elements_.RemoveLast();
    if (dropped.is_register()) {
      Unuse(dropped.reg());
    }
  }
}


void VirtualFrame::DropFromVFrameOnly(int count) {
  UNIMPLEMENTED_MIPS();
}


Result VirtualFrame::Pop() {
  UNIMPLEMENTED_MIPS();
  Result res = Result();
  return res;    // UNIMPLEMENTED RETURN
}


void VirtualFrame::EmitPop(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  stack_pointer_--;
  elements_.RemoveLast();
  __ Pop(reg);
}


void VirtualFrame::EmitMultiPop(RegList regs) {
  ASSERT(stack_pointer_ == element_count() - 1);
  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      stack_pointer_--;
      elements_.RemoveLast();
    }
  }
  __ MultiPop(regs);
}


void VirtualFrame::EmitPush(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement(NumberInfo::Unknown()));
  stack_pointer_++;
  __ Push(reg);
}


void VirtualFrame::EmitMultiPush(RegList regs) {
  ASSERT(stack_pointer_ == element_count() - 1);
  for (int16_t i = kNumRegisters; i > 0; i--) {
    if ((regs & (1 << i)) != 0) {
      elements_.Add(FrameElement::MemoryElement(NumberInfo::Unknown()));
      stack_pointer_++;
    }
  }
  __ MultiPush(regs);
}


void VirtualFrame::EmitArgumentSlots(RegList reglist) {
  UNIMPLEMENTED_MIPS();
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
