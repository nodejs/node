// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

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


void DebugCodegen::GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the trampoline pool in the debug break slot code.
  Assembler::BlockTrampolinePoolScope block_pool(masm);
  masm->RecordDebugBreakSlot(mode);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Isolate* isolate, Address pc) {
  CodePatcher patcher(isolate, pc, Assembler::kDebugBreakSlotInstructions);
  EmitDebugBreakSlot(patcher.masm());
}


void DebugCodegen::PatchDebugBreakSlot(Isolate* isolate, Address pc,
                                       Handle<Code> code) {
  DCHECK(code->is_debug_stub());
  CodePatcher patcher(isolate, pc, Assembler::kDebugBreakSlotInstructions);
  // Patch the code changing the debug break slot code from:
  //   nop(DEBUG_BREAK_NOP) - nop(1) is sll(zero_reg, zero_reg, 1)
  //   nop(DEBUG_BREAK_NOP)
  //   nop(DEBUG_BREAK_NOP)
  //   nop(DEBUG_BREAK_NOP)
  // to a call to the debug break slot code.
  //   li t9, address   (lui t9 / ori t9 instruction pair)
  //   call t9          (jalr t9 / nop instruction pair)
  patcher.masm()->li(v8::internal::t9,
                     Operand(reinterpret_cast<int32_t>(code->entry())));
  patcher.masm()->Call(v8::internal::t9);
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

    // Load padding words on stack.
    __ li(at, Operand(Smi::FromInt(LiveEdit::kFramePaddingValue)));
    __ Subu(sp, sp,
            Operand(kPointerSize * LiveEdit::kFramePaddingInitialSize));
    for (int i = LiveEdit::kFramePaddingInitialSize - 1; i >= 0; i--) {
      __ sw(at, MemOperand(sp, kPointerSize * i));
    }
    __ li(at, Operand(Smi::FromInt(LiveEdit::kFramePaddingInitialSize)));
    __ push(at);

    // Push arguments for DebugBreak call.
    if (mode == SAVE_RESULT_REGISTER) {
      // Break on return.
      __ push(v0);
    } else {
      // Non-return breaks.
      __ Push(masm->isolate()->factory()->the_hole_value());
    }
    __ PrepareCEntryArgs(1);
    __ PrepareCEntryFunction(ExternalReference(
        Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate()));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        Register reg = {JSCallerSavedCode(i)};
        // Do not clobber v0 if mode is SAVE_RESULT_REGISTER. It will
        // contain return value of the function returned by DebugBreak.
        if (!(reg.is(v0) && (mode == SAVE_RESULT_REGISTER))) {
          __ li(reg, kDebugZapValue);
        }
      }
    }

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
  __ lw(t9, MemOperand(t9));
  __ Jump(t9);
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  // We do not know our frame height, but set sp based on fp.
  __ lw(a1, MemOperand(fp, FrameDropperFrameConstants::kFunctionOffset));

  // Pop return address and frame.
  __ LeaveFrame(StackFrame::INTERNAL);

  ParameterCount dummy(0);
  __ FloodFunctionIfStepping(a1, no_reg, dummy, dummy);

  // Load context from the function.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Clear new.target as a safety measure.
  __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);

  // Get function code.
  __ lw(at, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(at, FieldMemOperand(at, SharedFunctionInfo::kCodeOffset));
  __ Addu(t9, at, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Re-run JSFunction, a1 is function, cp is context.
  __ Jump(t9);
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS
