// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/assembler.h"
#include "src/codegen.h"
#include "src/debug/debug.h"


namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void EmitDebugBreakSlot(MacroAssembler* masm) {
  Label check_codesize;
  __ bind(&check_codesize);
  __ Nop(Assembler::kDebugBreakSlotLength);
  DCHECK_EQ(Assembler::kDebugBreakSlotLength,
            masm->SizeOfCodeGeneratedSince(&check_codesize));
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode,
                                int call_argc) {
  // Generate enough nop's to make space for a call instruction.
  masm->RecordDebugBreakSlot(mode, call_argc);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Address pc) {
  CodePatcher patcher(pc, Assembler::kDebugBreakSlotLength);
  EmitDebugBreakSlot(patcher.masm());
}


void DebugCodegen::PatchDebugBreakSlot(Address pc, Handle<Code> code) {
  DCHECK_EQ(Code::BUILTIN, code->kind());
  static const int kSize = Assembler::kDebugBreakSlotLength;
  CodePatcher patcher(pc, kSize);
  Label check_codesize;
  patcher.masm()->bind(&check_codesize);
  patcher.masm()->movp(kScratchRegister, reinterpret_cast<void*>(code->entry()),
                       Assembler::RelocInfoNone());
  patcher.masm()->call(kScratchRegister);
  // Check that the size of the code generated is as expected.
  DCHECK_EQ(kSize, patcher.masm()->SizeOfCodeGeneratedSince(&check_codesize));
}


void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    for (int i = 0; i < LiveEdit::kFramePaddingInitialSize; i++) {
      __ Push(Smi::FromInt(LiveEdit::kFramePaddingValue));
    }
    __ Push(Smi::FromInt(LiveEdit::kFramePaddingInitialSize));

    if (mode == SAVE_RESULT_REGISTER) __ Push(rax);

    __ Set(rax, 0);  // No arguments (argc == 0).
    __ Move(rbx, ExternalReference(Runtime::FunctionForId(Runtime::kDebugBreak),
                                   masm->isolate()));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; ++i) {
        Register reg = {JSCallerSavedCode(i)};
        __ Set(reg, kDebugZapValue);
      }
    }

    if (mode == SAVE_RESULT_REGISTER) __ Pop(rax);

    // Read current padding counter and skip corresponding number of words.
    __ Pop(kScratchRegister);
    __ SmiToInteger32(kScratchRegister, kScratchRegister);
    __ leap(rsp, Operand(rsp, kScratchRegister, times_pointer_size, 0));

    // Get rid of the internal frame.
  }

  // This call did not replace a call , so there will be an unwanted
  // return address left on the stack. Here we get rid of that.
  __ addp(rsp, Immediate(kPCOnStackSize));

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference::debug_after_break_target_address(masm->isolate());
  __ Move(kScratchRegister, after_break_target);
  __ Jump(Operand(kScratchRegister, 0));
}


void DebugCodegen::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  masm->ret(0);
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference::debug_restarter_frame_function_pointer_address(
          masm->isolate());
  __ Move(rax, restarter_frame_function_slot);
  __ movp(Operand(rax, 0), Immediate(0));

  // We do not know our frame height, but set rsp based on rbp.
  __ leap(rsp, Operand(rbp, -1 * kPointerSize));

  __ Pop(rdi);  // Function.
  __ popq(rbp);

  // Load context from the function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Get function code.
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rdx, FieldOperand(rdx, SharedFunctionInfo::kCodeOffset));
  __ leap(rdx, FieldOperand(rdx, Code::kHeaderSize));

  // Re-run JSFunction, rdi is function, rsi is context.
  __ jmp(rdx);
}

const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
