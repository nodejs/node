// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/debug/debug.h"

#include "src/codegen.h"
#include "src/debug/liveedit.h"
#include "src/ia32/frames-ia32.h"

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


void DebugCodegen::GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode) {
  // Generate enough nop's to make space for a call instruction.
  masm->RecordDebugBreakSlot(mode);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Isolate* isolate, Address pc) {
  CodePatcher patcher(isolate, pc, Assembler::kDebugBreakSlotLength);
  EmitDebugBreakSlot(patcher.masm());
}


void DebugCodegen::PatchDebugBreakSlot(Isolate* isolate, Address pc,
                                       Handle<Code> code) {
  DCHECK(code->is_debug_stub());
  static const int kSize = Assembler::kDebugBreakSlotLength;
  CodePatcher patcher(isolate, pc, kSize);

  // Add a label for checking the size of the code used for returning.
  Label check_codesize;
  patcher.masm()->bind(&check_codesize);
  patcher.masm()->call(code->entry(), RelocInfo::NONE32);
  // Check that the size of the code generated is as expected.
  DCHECK_EQ(kSize, patcher.masm()->SizeOfCodeGeneratedSince(&check_codesize));
}

bool DebugCodegen::DebugBreakSlotIsPatched(Address pc) {
  return !Assembler::IsNop(pc);
}

void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push arguments for DebugBreak call.
    if (mode == SAVE_RESULT_REGISTER) {
      // Break on return.
      __ push(eax);
    } else {
      // Non-return breaks.
      __ Push(masm->isolate()->factory()->the_hole_value());
    }
    __ Move(eax, Immediate(1));
    __ mov(ebx,
           Immediate(ExternalReference(
               Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; ++i) {
        Register reg = {JSCallerSavedCode(i)};
        // Do not clobber eax if mode is SAVE_RESULT_REGISTER. It will
        // contain return value of the function.
        if (!(reg.is(eax) && (mode == SAVE_RESULT_REGISTER))) {
          __ Move(reg, Immediate(kDebugZapValue));
        }
      }
    }
    // Get rid of the internal frame.
  }

  __ MaybeDropFrames();

  // Return to caller.
  __ ret(0);
}

void DebugCodegen::GenerateHandleDebuggerStatement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kHandleDebuggerStatement, 0);
  }
  __ MaybeDropFrames();

  // Return to caller.
  __ ret(0);
}

void DebugCodegen::GenerateFrameDropperTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Drop to the target frame specified by ebx.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ mov(ebp, ebx);
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ leave();

  __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx,
         FieldOperand(ebx, SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount dummy(ebx);
  __ InvokeFunction(edi, dummy, dummy, JUMP_FUNCTION,
                    CheckDebugStepCallWrapper());
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
