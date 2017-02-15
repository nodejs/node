// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/debug/debug.h"

#include "src/codegen.h"
#include "src/debug/liveedit.h"

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
  // the constant pool in the debug break slot code.
  Assembler::BlockConstPoolScope block_const_pool(masm);
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
  // Patch the code changing the debug break slot code from
  //   mov r2, r2
  //   mov r2, r2
  //   mov r2, r2
  //   mov r2, r2
  // to a call to the debug break slot code.
  //   ldr ip, [pc, #0]
  //   b skip
  //   <debug break slot code entry point address>
  //   skip:
  //   blx ip
  Label skip_constant;
  patcher.masm()->ldr(ip, MemOperand(v8::internal::pc, 0));
  patcher.masm()->b(&skip_constant);
  patcher.Emit(code->entry());
  patcher.masm()->bind(&skip_constant);
  patcher.masm()->blx(ip);
}

bool DebugCodegen::DebugBreakSlotIsPatched(Address pc) {
  Instr current_instr = Assembler::instr_at(pc);
  return !Assembler::IsNop(current_instr, Assembler::DEBUG_BREAK_NOP);
}

void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ mov(ip, Operand(Smi::FromInt(LiveEdit::kFramePaddingValue)));
    for (int i = 0; i < LiveEdit::kFramePaddingInitialSize; i++) {
      __ push(ip);
    }
    __ mov(ip, Operand(Smi::FromInt(LiveEdit::kFramePaddingInitialSize)));
    __ push(ip);

    // Push arguments for DebugBreak call.
    if (mode == SAVE_RESULT_REGISTER) {
      // Break on return.
      __ push(r0);
    } else {
      // Non-return breaks.
      __ Push(masm->isolate()->factory()->the_hole_value());
    }
    __ mov(r0, Operand(1));
    __ mov(r1,
           Operand(ExternalReference(
               Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        Register reg = {JSCallerSavedCode(i)};
        // Do not clobber r0 if mode is SAVE_RESULT_REGISTER. It will
        // contain return value of the function.
        if (!(reg.is(r0) && (mode == SAVE_RESULT_REGISTER))) {
          __ mov(reg, Operand(kDebugZapValue));
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
  __ mov(ip, Operand(after_break_target));
  __ ldr(ip, MemOperand(ip));
  __ Jump(ip);
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  // Load the function pointer off of our current stack frame.
  __ ldr(r1, MemOperand(fp, FrameDropperFrameConstants::kFunctionOffset));

  // Pop return address, frame and constant pool pointer (if
  // FLAG_enable_embedded_constant_pool).
  __ LeaveFrame(StackFrame::INTERNAL);

  ParameterCount dummy(0);
  __ FloodFunctionIfStepping(r1, no_reg, dummy, dummy);

  { ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    // Load context from the function.
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    // Clear new.target as a safety measure.
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);

    // Get function code.
    __ ldr(ip, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
    __ add(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Re-run JSFunction, r1 is function, cp is context.
    __ Jump(ip);
  }
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
