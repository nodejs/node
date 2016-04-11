// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

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
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm);
  masm->RecordDebugBreakSlot(mode);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Isolate* isolate, Address pc) {
  CodePatcher patcher(isolate, pc, Assembler::kDebugBreakSlotInstructions);
  EmitDebugBreakSlot(patcher.masm());
}


void DebugCodegen::PatchDebugBreakSlot(Isolate* isolate, Address pc,
                                       Handle<Code> code) {
  DCHECK_EQ(Code::BUILTIN, code->kind());
  CodePatcher patcher(isolate, pc, Assembler::kDebugBreakSlotInstructions);
  // Patch the code changing the debug break slot code from
  //
  //   ori r3, r3, 0
  //   ori r3, r3, 0
  //   ori r3, r3, 0
  //   ori r3, r3, 0
  //   ori r3, r3, 0
  //
  // to a call to the debug break code, using a FIXED_SEQUENCE.
  //
  //   mov r0, <address>
  //   mtlr r0
  //   blrl
  //
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(patcher.masm());
  patcher.masm()->mov(v8::internal::r0,
                      Operand(reinterpret_cast<intptr_t>(code->entry())));
  patcher.masm()->mtctr(v8::internal::r0);
  patcher.masm()->bctrl();
}


void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ LoadSmiLiteral(ip, Smi::FromInt(LiveEdit::kFramePaddingValue));
    for (int i = 0; i < LiveEdit::kFramePaddingInitialSize; i++) {
      __ push(ip);
    }
    __ LoadSmiLiteral(ip, Smi::FromInt(LiveEdit::kFramePaddingInitialSize));
    __ push(ip);

    if (mode == SAVE_RESULT_REGISTER) __ push(r3);

    __ mov(r3, Operand::Zero());  // no arguments
    __ mov(r4,
           Operand(ExternalReference(
               Runtime::FunctionForId(Runtime::kDebugBreak), masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        Register reg = {JSCallerSavedCode(i)};
        __ mov(reg, Operand(kDebugZapValue));
      }
    }

    if (mode == SAVE_RESULT_REGISTER) __ pop(r3);

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
  __ LoadP(ip, MemOperand(ip));
  __ JumpToJSEntry(ip);
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  // Load the function pointer off of our current stack frame.
  __ LoadP(r4, MemOperand(fp, StandardFrameConstants::kConstantPoolOffset -
                                  kPointerSize));

  // Pop return address and frame
  __ LeaveFrame(StackFrame::INTERNAL);

  ParameterCount dummy(0);
  __ FloodFunctionIfStepping(r4, no_reg, dummy, dummy);

  // Load context from the function.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // Clear new.target as a safety measure.
  __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);

  // Get function code.
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Re-run JSFunction, r4 is function, cp is context.
  __ Jump(ip);
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
