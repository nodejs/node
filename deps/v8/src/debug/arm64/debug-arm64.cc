// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/debug/debug.h"

#include "src/arm64/frames-arm64.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/codegen.h"
#include "src/debug/liveedit.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void EmitDebugBreakSlot(Assembler* masm) {
  Label check_size;
  __ bind(&check_size);
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(Assembler::DEBUG_BREAK_NOP);
  }
  DCHECK_EQ(Assembler::kDebugBreakSlotInstructions,
            static_cast<int>(masm->InstructionsGeneratedSince(&check_size)));
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  InstructionAccurateScope scope(masm, Assembler::kDebugBreakSlotInstructions);
  masm->RecordDebugBreakSlot(mode);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Isolate* isolate, Address pc) {
  PatchingAssembler patcher(isolate, pc,
                            Assembler::kDebugBreakSlotInstructions);
  EmitDebugBreakSlot(&patcher);
}


void DebugCodegen::PatchDebugBreakSlot(Isolate* isolate, Address pc,
                                       Handle<Code> code) {
  DCHECK(code->is_debug_stub());
  PatchingAssembler patcher(isolate, pc,
                            Assembler::kDebugBreakSlotInstructions);
  // Patch the code emitted by DebugCodegen::GenerateSlots, changing the debug
  // break slot code from
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  // to a call to the debug slot code.
  //   ldr ip0, [pc, #(2 * kInstructionSize)]
  //   blr ip0
  //   b skip
  //   <debug break slot code entry point address (64 bits)>
  //   skip:

  Label skip_constant;
  // The first instruction of a patched debug break slot must be a load literal
  // loading the address of the debug break slot code.
  patcher.ldr_pcrel(ip0, (2 * kInstructionSize) >> kLoadLiteralScaleLog2);
  patcher.b(&skip_constant);
  patcher.dc64(reinterpret_cast<int64_t>(code->entry()));
  patcher.bind(&skip_constant);
  // TODO(all): check the following is correct.
  // The debug break slot code will push a frame and call statically compiled
  // code. By using blr,  this call site will be registered in the frame.
  // The debugger can now iterate on the frames to find this call.
  patcher.blr(ip0);
}

bool DebugCodegen::DebugBreakSlotIsPatched(Address pc) {
  Instruction* current_instr = reinterpret_cast<Instruction*>(pc);
  return !current_instr->IsNop(Assembler::DEBUG_BREAK_NOP);
}

void DebugCodegen::GenerateDebugBreakStub(MacroAssembler* masm,
                                          DebugBreakCallHelperMode mode) {
  __ RecordComment("Debug break");
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push arguments for DebugBreak call.
    if (mode == SAVE_RESULT_REGISTER) {
      // Break on return.
      __ Push(x0);
    } else {
      // Non-return breaks.
      __ Push(masm->isolate()->factory()->the_hole_value());
    }
    __ Mov(x0, 1);
    __ Mov(x1, ExternalReference(Runtime::FunctionForId(Runtime::kDebugBreak),
                                 masm->isolate()));

    CEntryStub stub(masm->isolate(), 1);
    __ CallStub(&stub);

    if (FLAG_debug_code) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        Register reg = Register::XRegFromCode(JSCallerSavedCode(i));
        // Do not clobber x0 if mode is SAVE_RESULT_REGISTER. It will
        // contain return value of the function.
        if (!(reg.is(x0) && (mode == SAVE_RESULT_REGISTER))) {
          __ Mov(reg, Operand(kDebugZapValue));
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
  // - Drop to the target frame specified by x1.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ Mov(fp, x1);
  __ AssertStackConsistency();
  __ Ldr(x1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  __ Mov(masm->StackPointer(), Operand(fp));
  __ Pop(fp, lr);  // Frame, Return address.

  __ Ldr(x0, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x0,
         FieldMemOperand(x0, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(x2, x0);

  ParameterCount dummy1(x2);
  ParameterCount dummy2(x0);
  __ InvokeFunction(x1, dummy1, dummy2, JUMP_FUNCTION,
                    CheckDebugStepCallWrapper());
}


const bool LiveEdit::kFrameDropperSupported = true;

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
