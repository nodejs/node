// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/codegen.h"
#include "src/debug/debug.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void EmitDebugBreakSlot(MacroAssembler* masm) {
  Label check_size;
  __ bind(&check_size);
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(MacroAssembler::DEBUG_BREAK_NOP);
  }
  DCHECK_EQ(Assembler::kDebugBreakSlotInstructions,
            masm->InstructionsGeneratedSince(&check_size));
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode,
                                int call_argc) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the trampoline pool in the debug break slot code.
  Assembler::BlockTrampolinePoolScope block_pool(masm);
  masm->RecordDebugBreakSlot(mode, call_argc);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Address pc) {
  CodePatcher patcher(pc, Assembler::kDebugBreakSlotInstructions);
  EmitDebugBreakSlot(patcher.masm());
}


void DebugCodegen::PatchDebugBreakSlot(Address pc, Handle<Code> code) {
  DCHECK_EQ(Code::BUILTIN, code->kind());
  CodePatcher patcher(pc, Assembler::kDebugBreakSlotInstructions);
  // Patch the code changing the debug break slot code from:
  //   nop(DEBUG_BREAK_NOP) - nop(1) is sll(zero_reg, zero_reg, 1)
  //   nop(DEBUG_BREAK_NOP)
  //   nop(DEBUG_BREAK_NOP)
  //   nop(DEBUG_BREAK_NOP)
  //   nop(DEBUG_BREAK_NOP)
  //   nop(DEBUG_BREAK_NOP)
  // to a call to the debug break slot code.
  //   li t9, address   (4-instruction sequence on mips64)
  //   call t9          (jalr t9 / nop instruction pair)
  patcher.masm()->li(v8::internal::t9,
                     Operand(reinterpret_cast<int64_t>(code->entry())),
                     ADDRESS_LOAD);
  patcher.masm()->Call(v8::internal::t9);
}


void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ li(at, Operand(Smi::FromInt(LiveEdit::kFramePaddingValue)));
    __ Dsubu(sp, sp,
            Operand(kPointerSize * LiveEdit::kFramePaddingInitialSize));
    for (int i = LiveEdit::kFramePaddingInitialSize - 1; i >= 0; i--) {
      __ sd(at, MemOperand(sp, kPointerSize * i));
    }
    __ li(at, Operand(Smi::FromInt(LiveEdit::kFramePaddingInitialSize)));
    __ push(at);

    if (mode == SAVE_RESULT_REGISTER) __ push(v0);

    __ PrepareCEntryArgs(0);  // No arguments.
    __ PrepareCEntryFunction(ExternalReference(
        Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate()));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        Register reg = {JSCallerSavedCode(i)};
        __ li(reg, kDebugZapValue);
      }
    }

    if (mode == SAVE_RESULT_REGISTER) __ pop(v0);

    // Don't bother removing padding bytes pushed on the stack
    // as the frame is going to be restored right away.

    // Leave the internal frame.
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference::debug_after_break_target_address(masm->isolate());
  __ li(t9, Operand(after_break_target));
  __ ld(t9, MemOperand(t9));
  __ Jump(t9);
}


void DebugCodegen::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  __ Ret();
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference::debug_restarter_frame_function_pointer_address(
          masm->isolate());
  __ li(at, Operand(restarter_frame_function_slot));
  __ sw(zero_reg, MemOperand(at, 0));

  // We do not know our frame height, but set sp based on fp.
  __ Dsubu(sp, fp, Operand(kPointerSize));

  __ Pop(ra, fp, a1);  // Return address, Frame, Function.

  // Load context from the function.
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Get function code.
  __ ld(at, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(at, FieldMemOperand(at, SharedFunctionInfo::kCodeOffset));
  __ Daddu(t9, at, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Re-run JSFunction, a1 is function, cp is context.
  __ Jump(t9);
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
