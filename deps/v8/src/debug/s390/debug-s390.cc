// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_S390

#include "src/debug/debug.h"

#include "src/codegen.h"
#include "src/debug/liveedit.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void EmitDebugBreakSlot(MacroAssembler* masm) {
  Label check_size;
  __ bind(&check_size);
  //   oill r3, 0
  //   oill r3, 0
  __ nop(Assembler::DEBUG_BREAK_NOP);
  __ nop(Assembler::DEBUG_BREAK_NOP);

  //   lr r0, r0    64-bit only
  //   lr r0, r0    64-bit only
  //   lr r0, r0    64-bit only
  for (int i = 8; i < Assembler::kDebugBreakSlotLength; i += 2) {
    __ nop();
  }
  DCHECK_EQ(Assembler::kDebugBreakSlotLength,
            masm->SizeOfCodeGeneratedSince(&check_size));
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
  CodePatcher patcher(isolate, pc, Assembler::kDebugBreakSlotLength);
  // Patch the code changing the debug break slot code from
  //
  //   oill r3, 0
  //   oill r3, 0
  //   oill r3, 0   64-bit only
  //   lr r0, r0    64-bit only
  //
  // to a call to the debug break code, using a FIXED_SEQUENCE.
  //
  //   iilf r14, <address>   6-bytes
  //   basr r14, r14A        2-bytes
  //
  // The 64bit sequence has an extra iihf.
  //
  //   iihf r14, <high 32-bits address>    6-bytes
  //   iilf r14, <lower 32-bits address>   6-bytes
  //   basr r14, r14                       2-bytes
  patcher.masm()->mov(v8::internal::r14,
                      Operand(reinterpret_cast<intptr_t>(code->entry())));
  patcher.masm()->basr(v8::internal::r14, v8::internal::r14);
}

bool DebugCodegen::DebugBreakSlotIsPatched(Address pc) {
  Instr current_instr = Assembler::instr_at(pc);
  return !Assembler::IsNop(current_instr, Assembler::DEBUG_BREAK_NOP);
}

void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push arguments for DebugBreak call.
    if (mode == SAVE_RESULT_REGISTER) {
      // Break on return.
      __ push(r2);
    } else {
      // Non-return breaks.
      __ Push(masm->isolate()->factory()->the_hole_value());
    }
    __ mov(r2, Operand(1));
    __ mov(r3,
           Operand(ExternalReference(
               Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        Register reg = {JSCallerSavedCode(i)};
        // Do not clobber r2 if mode is SAVE_RESULT_REGISTER. It will
        // contain return value of the function.
        if (!(reg.is(r2) && (mode == SAVE_RESULT_REGISTER))) {
          __ mov(reg, Operand(kDebugZapValue));
        }
      }
    }
    // Leave the internal frame.
  }
  __ MaybeDropFrames();

  // Return to caller.
  __ Ret();
}

void DebugCodegen::GenerateHandleDebuggerStatement(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kHandleDebuggerStatement, 0);
  }
  __ MaybeDropFrames();

  // Return to caller.
  __ Ret();
}

void DebugCodegen::GenerateFrameDropperTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Drop to the target frame specified by r3.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.

  __ LoadRR(fp, r3);
  __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LeaveFrame(StackFrame::INTERNAL);
  __ LoadP(r2, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(
      r2, FieldMemOperand(r2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadRR(r4, r2);

  ParameterCount dummy1(r4);
  ParameterCount dummy2(r2);
  __ InvokeFunction(r3, dummy1, dummy2, JUMP_FUNCTION,
                    CheckDebugStepCallWrapper());
}

const bool LiveEdit::kFrameDropperSupported = true;

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
