// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/debug/debug.h"

#include "src/assembler-inl.h"
#include "src/codegen.h"
#include "src/debug/liveedit.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

namespace {
void EmitDebugBreakSlot(Assembler* assembler) {
  Label check_size;
  assembler->bind(&check_size);
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    assembler->nop(MacroAssembler::DEBUG_BREAK_NOP);
  }
  DCHECK_EQ(Assembler::kDebugBreakSlotInstructions,
            assembler->InstructionsGeneratedSince(&check_size));
}
}  // anonymous namespace

void DebugCodegen::GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  Assembler::BlockConstPoolScope block_const_pool(masm);
  masm->RecordDebugBreakSlot(mode);
  EmitDebugBreakSlot(masm);
}


void DebugCodegen::ClearDebugBreakSlot(Isolate* isolate, Address pc) {
  PatchingAssembler patcher(Assembler::IsolateData(isolate), pc,
                            Assembler::kDebugBreakSlotInstructions);
  EmitDebugBreakSlot(&patcher);
  patcher.FlushICache(isolate);
}


void DebugCodegen::PatchDebugBreakSlot(Isolate* isolate, Address pc,
                                       Handle<Code> code) {
  DCHECK(code->is_debug_stub());
  PatchingAssembler patcher(Assembler::IsolateData(isolate), pc,
                            Assembler::kDebugBreakSlotInstructions);
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
  patcher.ldr(ip, MemOperand(v8::internal::pc, 0));
  patcher.b(&skip_constant);
  patcher.Emit(code->entry());
  patcher.bind(&skip_constant);
  patcher.blx(ip);
  patcher.FlushICache(isolate);
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
  // - Drop to the target frame specified by r1.
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.
  __ mov(fp, r1);
  __ ldr(r1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LeaveFrame(StackFrame::INTERNAL);

  __ ldr(r0, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r0,
         FieldMemOperand(r0, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(r2, r0);

  ParameterCount dummy1(r2);
  ParameterCount dummy2(r0);
  __ InvokeFunction(r1, dummy1, dummy2, JUMP_FUNCTION,
                    CheckDebugStepCallWrapper());
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
