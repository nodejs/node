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

// -------------------------------------------------------------------------
// VirtualFrame implementation.

#define __ ACCESS_MASM(masm())


// On entry to a function, the virtual frame already contains the
// receiver and the parameters.  All initial frame elements are in
// memory.
VirtualFrame::VirtualFrame()
    : elements_(parameter_count() + local_count() + kPreallocatedElements),
      stack_pointer_(parameter_count()) {  // 0-based index of TOS.
  for (int i = 0; i <= stack_pointer_; i++) {
    elements_.Add(FrameElement::MemoryElement());
  }
  for (int i = 0; i < RegisterAllocator::kNumRegisters; i++) {
    register_locations_[i] = kIllegalIndex;
  }
}


void VirtualFrame::SyncElementBelowStackPointer(int index) {
  UNREACHABLE();
}


void VirtualFrame::SyncElementByPushing(int index) {
  UNREACHABLE();
}


void VirtualFrame::SyncRange(int begin, int end) {
  // All elements are in memory on ARM (ie, synced).
#ifdef DEBUG
  for (int i = begin; i <= end; i++) {
    ASSERT(elements_[i].is_synced());
  }
#endif
}


void VirtualFrame::MergeTo(VirtualFrame* expected) {
  Comment cmnt(masm(), "[ Merge frame");
  // We should always be merging the code generator's current frame to an
  // expected frame.
  ASSERT(cgen()->frame() == this);

  // Adjust the stack pointer upward (toward the top of the virtual
  // frame) if necessary.
  if (stack_pointer_ < expected->stack_pointer_) {
    int difference = expected->stack_pointer_ - stack_pointer_;
    stack_pointer_ = expected->stack_pointer_;
    __ sub(sp, sp, Operand(difference * kPointerSize));
  }

  MergeMoveRegistersToMemory(expected);
  MergeMoveRegistersToRegisters(expected);
  MergeMoveMemoryToRegisters(expected);

  // Fix any sync bit problems from the bottom-up, stopping when we
  // hit the stack pointer or the top of the frame if the stack
  // pointer is floating above the frame.
  int limit = Min(static_cast<int>(stack_pointer_), element_count() - 1);
  for (int i = 0; i <= limit; i++) {
    FrameElement source = elements_[i];
    FrameElement target = expected->elements_[i];
    if (source.is_synced() && !target.is_synced()) {
      elements_[i].clear_sync();
    } else if (!source.is_synced() && target.is_synced()) {
      SyncElementAt(i);
    }
  }

  // Adjust the stack point downard if necessary.
  if (stack_pointer_ > expected->stack_pointer_) {
    int difference = stack_pointer_ - expected->stack_pointer_;
    stack_pointer_ = expected->stack_pointer_;
    __ add(sp, sp, Operand(difference * kPointerSize));
  }

  // At this point, the frames should be identical.
  ASSERT(Equals(expected));
}


void VirtualFrame::MergeMoveRegistersToMemory(VirtualFrame* expected) {
  ASSERT(stack_pointer_ >= expected->stack_pointer_);

  // Move registers, constants, and copies to memory.  Perform moves
  // from the top downward in the frame in order to leave the backing
  // stores of copies in registers.
  // On ARM, all elements are in memory.

#ifdef DEBUG
  int start = Min(static_cast<int>(stack_pointer_), element_count() - 1);
  for (int i = start; i >= 0; i--) {
    ASSERT(elements_[i].is_memory());
    ASSERT(expected->elements_[i].is_memory());
  }
#endif
}


void VirtualFrame::MergeMoveRegistersToRegisters(VirtualFrame* expected) {
}


void VirtualFrame::MergeMoveMemoryToRegisters(VirtualFrame* expected) {
}


void VirtualFrame::Enter() {
  Comment cmnt(masm(), "[ Enter JS frame");

#ifdef DEBUG
  // Verify that r1 contains a JS function.  The following code relies
  // on r2 being available for use.
  { Label map_check, done;
    __ tst(r1, Operand(kSmiTagMask));
    __ b(ne, &map_check);
    __ stop("VirtualFrame::Enter - r1 is not a function (smi check).");
    __ bind(&map_check);
    __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
    __ cmp(r2, Operand(JS_FUNCTION_TYPE));
    __ b(eq, &done);
    __ stop("VirtualFrame::Enter - r1 is not a function (map check).");
    __ bind(&done);
  }
#endif  // DEBUG

  // We are about to push four values to the frame.
  Adjust(4);
  __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
  // Adjust FP to point to saved FP.
  __ add(fp, sp, Operand(2 * kPointerSize));
  cgen()->allocator()->Unuse(r1);
  cgen()->allocator()->Unuse(lr);
}


void VirtualFrame::Exit() {
  Comment cmnt(masm(), "[ Exit JS frame");
  // Drop the execution stack down to the frame pointer and restore the caller
  // frame pointer and return address.
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
}


void VirtualFrame::AllocateStackSlots() {
  int count = local_count();
  if (count > 0) {
    Comment cmnt(masm(), "[ Allocate space for locals");
    Adjust(count);
      // Initialize stack slots with 'undefined' value.
    __ mov(ip, Operand(Factory::undefined_value()));
    for (int i = 0; i < count; i++) {
      __ push(ip);
    }
  }
}


void VirtualFrame::SaveContextRegister() {
  UNIMPLEMENTED();
}


void VirtualFrame::RestoreContextRegister() {
  UNIMPLEMENTED();
}


void VirtualFrame::PushReceiverSlotAddress() {
  UNIMPLEMENTED();
}


int VirtualFrame::InvalidateFrameSlotAt(int index) {
  UNIMPLEMENTED();
  return kIllegalIndex;
}


void VirtualFrame::TakeFrameSlotAt(int index) {
  UNIMPLEMENTED();
}


void VirtualFrame::StoreToFrameSlotAt(int index) {
  UNIMPLEMENTED();
}


void VirtualFrame::PushTryHandler(HandlerType type) {
  // Grow the expression stack by handler size less one (the return address
  // is already pushed by a call instruction).
  Adjust(kHandlerSize - 1);
  __ PushTryHandler(IN_JAVASCRIPT, type);
}


Result VirtualFrame::RawCallStub(CodeStub* stub) {
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallStub(stub);
  Result result = cgen()->allocator()->Allocate(r0);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::CallStub(CodeStub* stub, Result* arg) {
  PrepareForCall(0, 0);
  arg->Unuse();
  return RawCallStub(stub);
}


Result VirtualFrame::CallStub(CodeStub* stub, Result* arg0, Result* arg1) {
  PrepareForCall(0, 0);
  arg0->Unuse();
  arg1->Unuse();
  return RawCallStub(stub);
}


Result VirtualFrame::CallRuntime(Runtime::Function* f, int arg_count) {
  PrepareForCall(arg_count, arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(f, arg_count);
  Result result = cgen()->allocator()->Allocate(r0);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::CallRuntime(Runtime::FunctionId id, int arg_count) {
  PrepareForCall(arg_count, arg_count);
  ASSERT(cgen()->HasValidEntryRegisters());
  __ CallRuntime(id, arg_count);
  Result result = cgen()->allocator()->Allocate(r0);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeJSFlags flags,
                                   Result* arg_count_register,
                                   int arg_count) {
  ASSERT(arg_count_register->reg().is(r0));
  PrepareForCall(arg_count, arg_count);
  arg_count_register->Unuse();
  __ InvokeBuiltin(id, flags);
  Result result = cgen()->allocator()->Allocate(r0);
  return result;
}


Result VirtualFrame::RawCallCodeObject(Handle<Code> code,
                                       RelocInfo::Mode rmode) {
  ASSERT(cgen()->HasValidEntryRegisters());
  __ Call(code, rmode);
  Result result = cgen()->allocator()->Allocate(r0);
  ASSERT(result.is_valid());
  return result;
}


Result VirtualFrame::CallCodeObject(Handle<Code> code,
                                    RelocInfo::Mode rmode,
                                    int dropped_args) {
  int spilled_args = 0;
  switch (code->kind()) {
    case Code::CALL_IC:
      spilled_args = dropped_args + 1;
      break;
    case Code::FUNCTION:
      spilled_args = dropped_args + 1;
      break;
    case Code::KEYED_LOAD_IC:
      ASSERT(dropped_args == 0);
      spilled_args = 2;
      break;
    default:
      // The other types of code objects are called with values
      // in specific registers, and are handled in functions with
      // a different signature.
      UNREACHABLE();
      break;
  }
  PrepareForCall(spilled_args, dropped_args);
  return RawCallCodeObject(code, rmode);
}


Result VirtualFrame::CallCodeObject(Handle<Code> code,
                                    RelocInfo::Mode rmode,
                                    Result* arg,
                                    int dropped_args) {
  int spilled_args = 0;
  switch (code->kind()) {
    case Code::LOAD_IC:
      ASSERT(arg->reg().is(r2));
      ASSERT(dropped_args == 0);
      spilled_args = 1;
      break;
    case Code::KEYED_STORE_IC:
      ASSERT(arg->reg().is(r0));
      ASSERT(dropped_args == 0);
      spilled_args = 2;
      break;
    default:
      // No other types of code objects are called with values
      // in exactly one register.
      UNREACHABLE();
      break;
  }
  PrepareForCall(spilled_args, dropped_args);
  arg->Unuse();
  return RawCallCodeObject(code, rmode);
}


Result VirtualFrame::CallCodeObject(Handle<Code> code,
                                    RelocInfo::Mode rmode,
                                    Result* arg0,
                                    Result* arg1,
                                    int dropped_args) {
  int spilled_args = 1;
  switch (code->kind()) {
    case Code::STORE_IC:
      ASSERT(arg0->reg().is(r0));
      ASSERT(arg1->reg().is(r2));
      ASSERT(dropped_args == 0);
      spilled_args = 1;
      break;
    case Code::BUILTIN:
      ASSERT(*code == Builtins::builtin(Builtins::JSConstructCall));
      ASSERT(arg0->reg().is(r0));
      ASSERT(arg1->reg().is(r1));
      spilled_args = dropped_args + 1;
      break;
    default:
      // No other types of code objects are called with values
      // in exactly two registers.
      UNREACHABLE();
      break;
  }
  PrepareForCall(spilled_args, dropped_args);
  arg0->Unuse();
  arg1->Unuse();
  return RawCallCodeObject(code, rmode);
}


void VirtualFrame::Drop(int count) {
  ASSERT(height() >= count);
  int num_virtual_elements = (element_count() - 1) - stack_pointer_;

  // Emit code to lower the stack pointer if necessary.
  if (num_virtual_elements < count) {
    int num_dropped = count - num_virtual_elements;
    stack_pointer_ -= num_dropped;
    __ add(sp, sp, Operand(num_dropped * kPointerSize));
  }

  // Discard elements from the virtual frame and free any registers.
  for (int i = 0; i < count; i++) {
    FrameElement dropped = elements_.RemoveLast();
    if (dropped.is_register()) {
      Unuse(dropped.reg());
    }
  }
}


Result VirtualFrame::Pop() {
  UNIMPLEMENTED();
  return Result();
}


void VirtualFrame::EmitPop(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  stack_pointer_--;
  elements_.RemoveLast();
  __ pop(reg);
}


void VirtualFrame::EmitPush(Register reg) {
  ASSERT(stack_pointer_ == element_count() - 1);
  elements_.Add(FrameElement::MemoryElement());
  stack_pointer_++;
  __ push(reg);
}


#undef __

} }  // namespace v8::internal
