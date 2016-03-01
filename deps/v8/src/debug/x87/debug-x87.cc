// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X87

#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/x87/frames-x87.h"

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
  DCHECK_EQ(Code::BUILTIN, code->kind());
  static const int kSize = Assembler::kDebugBreakSlotLength;
  CodePatcher patcher(isolate, pc, kSize);

  // Add a label for checking the size of the code used for returning.
  Label check_codesize;
  patcher.masm()->bind(&check_codesize);
  patcher.masm()->call(code->entry(), RelocInfo::NONE32);
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
      __ push(Immediate(Smi::FromInt(LiveEdit::kFramePaddingValue)));
    }
    __ push(Immediate(Smi::FromInt(LiveEdit::kFramePaddingInitialSize)));

    if (mode == SAVE_RESULT_REGISTER) __ push(eax);

    __ Move(eax, Immediate(0));  // No arguments.
    __ mov(ebx,
           Immediate(ExternalReference(
               Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; ++i) {
        Register reg = {JSCallerSavedCode(i)};
        __ Move(reg, Immediate(kDebugZapValue));
      }
    }

    if (mode == SAVE_RESULT_REGISTER) __ pop(eax);

    __ pop(ebx);
    // We divide stored value by 2 (untagging) and multiply it by word's size.
    STATIC_ASSERT(kSmiTagSize == 1 && kSmiShiftSize == 0);
    __ lea(esp, Operand(esp, ebx, times_half_pointer_size, 0));

    // Get rid of the internal frame.
  }

  // This call did not replace a call , so there will be an unwanted
  // return address left on the stack. Here we get rid of that.
  __ add(esp, Immediate(kPointerSize));

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference::debug_after_break_target_address(masm->isolate());
  __ jmp(Operand::StaticVariable(after_break_target));
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  // We do not know our frame height, but set esp based on ebp.
  __ lea(esp, Operand(ebp, -1 * kPointerSize));

  __ pop(edi);  // Function.
  __ pop(ebp);

  ParameterCount dummy(0);
  __ FloodFunctionIfStepping(edi, no_reg, dummy, dummy);

  // Load context from the function.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Clear new.target register as a safety measure.
  __ mov(edx, masm->isolate()->factory()->undefined_value());

  // Get function code.
  __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kCodeOffset));
  __ lea(ebx, FieldOperand(ebx, Code::kHeaderSize));

  // Re-run JSFunction, edi is function, esi is context.
  __ jmp(ebx);
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87
