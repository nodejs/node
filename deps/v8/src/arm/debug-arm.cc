// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if defined(V8_TARGET_ARCH_ARM)

#include "codegen-inl.h"
#include "debug.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT
bool BreakLocationIterator::IsDebugBreakAtReturn() {
  return Debug::IsDebugBreakAtReturn(rinfo());
}


void BreakLocationIterator::SetDebugBreakAtReturn() {
  // Patch the code changing the return from JS function sequence from
  //   mov sp, fp
  //   ldmia sp!, {fp, lr}
  //   add sp, sp, #4
  //   bx lr
  // to a call to the debug break return code.
  // #if USE_BLX
  //   ldr ip, [pc, #0]
  //   blx ip
  // #else
  //   mov lr, pc
  //   ldr pc, [pc, #-4]
  // #endif
  //   <debug break return code entry point address>
  //   bktp 0
  CodePatcher patcher(rinfo()->pc(), Assembler::kJSReturnSequenceInstructions);
#ifdef USE_BLX
  patcher.masm()->ldr(v8::internal::ip, MemOperand(v8::internal::pc, 0));
  patcher.masm()->blx(v8::internal::ip);
#else
  patcher.masm()->mov(v8::internal::lr, v8::internal::pc);
  patcher.masm()->ldr(v8::internal::pc, MemOperand(v8::internal::pc, -4));
#endif
  patcher.Emit(Debug::debug_break_return()->entry());
  patcher.masm()->bkpt(0);
}


// Restore the JS frame exit code.
void BreakLocationIterator::ClearDebugBreakAtReturn() {
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kJSReturnSequenceInstructions);
}


// A debug break in the frame exit code is identified by the JS frame exit code
// having been patched with a call instruction.
bool Debug::IsDebugBreakAtReturn(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  return rinfo->IsPatchedReturnSequence();
}


bool BreakLocationIterator::IsDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  // Check whether the debug break slot instructions have been patched.
  return rinfo()->IsPatchedDebugBreakSlotSequence();
}


void BreakLocationIterator::SetDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  // Patch the code changing the debug break slot code from
  //   mov r2, r2
  //   mov r2, r2
  //   mov r2, r2
  // to a call to the debug break slot code.
  // #if USE_BLX
  //   ldr ip, [pc, #0]
  //   blx ip
  // #else
  //   mov lr, pc
  //   ldr pc, [pc, #-4]
  // #endif
  //   <debug break slot code entry point address>
  CodePatcher patcher(rinfo()->pc(), Assembler::kDebugBreakSlotInstructions);
#ifdef USE_BLX
  patcher.masm()->ldr(v8::internal::ip, MemOperand(v8::internal::pc, 0));
  patcher.masm()->blx(v8::internal::ip);
#else
  patcher.masm()->mov(v8::internal::lr, v8::internal::pc);
  patcher.masm()->ldr(v8::internal::pc, MemOperand(v8::internal::pc, -4));
#endif
  patcher.Emit(Debug::debug_break_slot()->entry());
}


void BreakLocationIterator::ClearDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kDebugBreakSlotInstructions);
}


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs) {
  __ EnterInternalFrame();

  // Store the registers containing live values on the expression stack to
  // make sure that these are correctly updated during GC. Non object values
  // are stored as a smi causing it to be untouched by GC.
  ASSERT((object_regs & ~kJSCallerSaved) == 0);
  ASSERT((non_object_regs & ~kJSCallerSaved) == 0);
  ASSERT((object_regs & non_object_regs) == 0);
  if ((object_regs | non_object_regs) != 0) {
    for (int i = 0; i < kNumJSCallerSaved; i++) {
      int r = JSCallerSavedCode(i);
      Register reg = { r };
      if ((non_object_regs & (1 << r)) != 0) {
        if (FLAG_debug_code) {
          __ tst(reg, Operand(0xc0000000));
          __ Assert(eq, "Unable to encode value as smi");
        }
        __ mov(reg, Operand(reg, LSL, kSmiTagSize));
      }
    }
    __ stm(db_w, sp, object_regs | non_object_regs);
  }

#ifdef DEBUG
  __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
  __ mov(r0, Operand(0, RelocInfo::NONE));  // no arguments
  __ mov(r1, Operand(ExternalReference::debug_break()));

  CEntryStub ceb(1);
  __ CallStub(&ceb);

  // Restore the register values from the expression stack.
  if ((object_regs | non_object_regs) != 0) {
    __ ldm(ia_w, sp, object_regs | non_object_regs);
    for (int i = 0; i < kNumJSCallerSaved; i++) {
      int r = JSCallerSavedCode(i);
      Register reg = { r };
      if ((non_object_regs & (1 << r)) != 0) {
        __ mov(reg, Operand(reg, LSR, kSmiTagSize));
      }
      if (FLAG_debug_code &&
          (((object_regs |non_object_regs) & (1 << r)) == 0)) {
        __ mov(reg, Operand(kDebugZapValue));
      }
    }
  }

  __ LeaveInternalFrame();

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  __ mov(ip, Operand(ExternalReference(Debug_Address::AfterBreakTarget())));
  __ ldr(ip, MemOperand(ip));
  __ Jump(ip);
}


void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC load (from ic-arm.cc).
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- [sp]  : receiver
  // -----------------------------------
  // Registers r0 and r2 contain objects that need to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, r0.bit() | r2.bit(), 0);
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC store (from ic-arm.cc).
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  // Registers r0, r1, and r2 contain objects that need to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, r0.bit() | r1.bit() | r2.bit(), 0);
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  Generate_DebugBreakCallHelper(masm, r0.bit() | r1.bit(), 0);
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  Generate_DebugBreakCallHelper(masm, r0.bit() | r1.bit() | r2.bit(), 0);
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC call (from ic-arm.cc)
  // ----------- S t a t e -------------
  //  -- r2     : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r2.bit(), 0);
}


void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  // Calling convention for construct call (from builtins-arm.cc)
  //  -- r0     : number of arguments (not smi)
  //  -- r1     : constructor function
  Generate_DebugBreakCallHelper(masm, r1.bit(), r0.bit());
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, r0.bit(), 0);
}


void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  No registers used on entry.
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, 0, 0);
}


void Debug::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  Assembler::BlockConstPoolScope block_const_pool(masm);
  Label check_codesize;
  __ bind(&check_codesize);
  __ RecordDebugBreakSlot();
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(MacroAssembler::DEBUG_BREAK_NOP);
  }
  ASSERT_EQ(Assembler::kDebugBreakSlotInstructions,
            masm->InstructionsGeneratedSince(&check_codesize));
}


void Debug::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0, 0);
}


void Debug::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  masm->Abort("LiveEdit frame dropping is not supported on arm");
}


void Debug::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  masm->Abort("LiveEdit frame dropping is not supported on arm");
}

const bool Debug::kFrameDropperSupported = false;

#undef __



#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
