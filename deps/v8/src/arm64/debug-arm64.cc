// Copyright 2013 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_ARM64

#include "codegen.h"
#include "debug.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


#ifdef ENABLE_DEBUGGER_SUPPORT
bool BreakLocationIterator::IsDebugBreakAtReturn() {
  return Debug::IsDebugBreakAtReturn(rinfo());
}


void BreakLocationIterator::SetDebugBreakAtReturn() {
  // Patch the code emitted by FullCodeGenerator::EmitReturnSequence, changing
  // the return from JS function sequence from
  //   mov sp, fp
  //   ldp fp, lr, [sp] #16
  //   lrd ip0, [pc, #(3 * kInstructionSize)]
  //   add sp, sp, ip0
  //   ret
  //   <number of paramters ...
  //    ... plus one (64 bits)>
  // to a call to the debug break return code.
  //   ldr ip0, [pc, #(3 * kInstructionSize)]
  //   blr ip0
  //   hlt kHltBadCode    @ code should not return, catch if it does.
  //   <debug break return code ...
  //    ... entry point address (64 bits)>

  // The patching code must not overflow the space occupied by the return
  // sequence.
  STATIC_ASSERT(Assembler::kJSRetSequenceInstructions >= 5);
  PatchingAssembler patcher(reinterpret_cast<Instruction*>(rinfo()->pc()), 5);
  byte* entry =
      debug_info_->GetIsolate()->debug()->debug_break_return()->entry();

  // The first instruction of a patched return sequence must be a load literal
  // loading the address of the debug break return code.
  patcher.LoadLiteral(ip0, 3 * kInstructionSize);
  // TODO(all): check the following is correct.
  // The debug break return code will push a frame and call statically compiled
  // code. By using blr, even though control will not return after the branch,
  // this call site will be registered in the frame (lr being saved as the pc
  // of the next instruction to execute for this frame). The debugger can now
  // iterate on the frames to find call to debug break return code.
  patcher.blr(ip0);
  patcher.hlt(kHltBadCode);
  patcher.dc64(reinterpret_cast<int64_t>(entry));
}


void BreakLocationIterator::ClearDebugBreakAtReturn() {
  // Reset the code emitted by EmitReturnSequence to its original state.
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kJSRetSequenceInstructions);
}


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
  // Patch the code emitted by Debug::GenerateSlots, changing the debug break
  // slot code from
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  //   mov x0, x0    @ nop DEBUG_BREAK_NOP
  // to a call to the debug slot code.
  //   ldr ip0, [pc, #(2 * kInstructionSize)]
  //   blr ip0
  //   <debug break slot code ...
  //    ... entry point address (64 bits)>

  // TODO(all): consider adding a hlt instruction after the blr as we don't
  // expect control to return here. This implies increasing
  // kDebugBreakSlotInstructions to 5 instructions.

  // The patching code must not overflow the space occupied by the return
  // sequence.
  STATIC_ASSERT(Assembler::kDebugBreakSlotInstructions >= 4);
  PatchingAssembler patcher(reinterpret_cast<Instruction*>(rinfo()->pc()), 4);
  byte* entry =
      debug_info_->GetIsolate()->debug()->debug_break_slot()->entry();

  // The first instruction of a patched debug break slot must be a load literal
  // loading the address of the debug break slot code.
  patcher.LoadLiteral(ip0, 2 * kInstructionSize);
  // TODO(all): check the following is correct.
  // The debug break slot code will push a frame and call statically compiled
  // code. By using blr, event hough control will not return after the branch,
  // this call site will be registered in the frame (lr being saved as the pc
  // of the next instruction to execute for this frame). The debugger can now
  // iterate on the frames to find call to debug break slot code.
  patcher.blr(ip0);
  patcher.dc64(reinterpret_cast<int64_t>(entry));
}


void BreakLocationIterator::ClearDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kDebugBreakSlotInstructions);
}

const bool Debug::FramePaddingLayout::kIsSupported = false;

static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs,
                                          Register scratch) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Any live values (object_regs and non_object_regs) in caller-saved
    // registers (or lr) need to be stored on the stack so that their values are
    // safely preserved for a call into C code.
    //
    // Also:
    //  * object_regs may be modified during the C code by the garbage
    //    collector. Every object register must be a valid tagged pointer or
    //    SMI.
    //
    //  * non_object_regs will be converted to SMIs so that the garbage
    //    collector doesn't try to interpret them as pointers.
    //
    // TODO(jbramley): Why can't this handle callee-saved registers?
    ASSERT((~kCallerSaved.list() & object_regs) == 0);
    ASSERT((~kCallerSaved.list() & non_object_regs) == 0);
    ASSERT((object_regs & non_object_regs) == 0);
    ASSERT((scratch.Bit() & object_regs) == 0);
    ASSERT((scratch.Bit() & non_object_regs) == 0);
    ASSERT((masm->TmpList()->list() & (object_regs | non_object_regs)) == 0);
    STATIC_ASSERT(kSmiValueSize == 32);

    CPURegList non_object_list =
        CPURegList(CPURegister::kRegister, kXRegSizeInBits, non_object_regs);
    while (!non_object_list.IsEmpty()) {
      // Store each non-object register as two SMIs.
      Register reg = Register(non_object_list.PopLowestIndex());
      __ Push(reg);
      __ Poke(wzr, 0);
      __ Push(reg.W(), wzr);
      // Stack:
      //  jssp[12]: reg[63:32]
      //  jssp[8]: 0x00000000 (SMI tag & padding)
      //  jssp[4]: reg[31:0]
      //  jssp[0]: 0x00000000 (SMI tag & padding)
      STATIC_ASSERT((kSmiTag == 0) && (kSmiShift == 32));
    }

    if (object_regs != 0) {
      __ PushXRegList(object_regs);
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ Mov(x0, 0);  // No arguments.
    __ Mov(x1, ExternalReference::debug_break(masm->isolate()));

    CEntryStub stub(1);
    __ CallStub(&stub);

    // Restore the register values from the expression stack.
    if (object_regs != 0) {
      __ PopXRegList(object_regs);
    }

    non_object_list =
        CPURegList(CPURegister::kRegister, kXRegSizeInBits, non_object_regs);
    while (!non_object_list.IsEmpty()) {
      // Load each non-object register from two SMIs.
      // Stack:
      //  jssp[12]: reg[63:32]
      //  jssp[8]: 0x00000000 (SMI tag & padding)
      //  jssp[4]: reg[31:0]
      //  jssp[0]: 0x00000000 (SMI tag & padding)
      Register reg = Register(non_object_list.PopHighestIndex());
      __ Pop(scratch, reg);
      __ Bfxil(reg, scratch, 32, 32);
    }

    // Leave the internal frame.
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target(Debug_Address::AfterBreakTarget(),
                                       masm->isolate());
  __ Mov(scratch, after_break_target);
  __ Ldr(scratch, MemOperand(scratch));
  __ Br(scratch);
}


void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC load (from ic-arm.cc).
  // ----------- S t a t e -------------
  //  -- x2    : name
  //  -- lr    : return address
  //  -- x0    : receiver
  //  -- [sp]  : receiver
  // -----------------------------------
  // Registers x0 and x2 contain objects that need to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, x0.Bit() | x2.Bit(), 0, x10);
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC store (from ic-arm.cc).
  // ----------- S t a t e -------------
  //  -- x0    : value
  //  -- x1    : receiver
  //  -- x2    : name
  //  -- lr    : return address
  // -----------------------------------
  // Registers x0, x1, and x2 contain objects that need to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, x0.Bit() | x1.Bit() | x2.Bit(), 0, x10);
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  Generate_DebugBreakCallHelper(masm, x0.Bit() | x1.Bit(), 0, x10);
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- x0     : value
  //  -- x1     : key
  //  -- x2     : receiver
  //  -- lr     : return address
  Generate_DebugBreakCallHelper(masm, x0.Bit() | x1.Bit() | x2.Bit(), 0, x10);
}


void Debug::GenerateCompareNilICDebugBreak(MacroAssembler* masm) {
  // Register state for CompareNil IC
  // ----------- S t a t e -------------
  //  -- r0    : value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x0.Bit(), 0, x10);
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC call (from ic-arm.cc)
  // ----------- S t a t e -------------
  //  -- x2     : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x2.Bit(), 0, x10);
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, x0.Bit(), 0, x10);
}


void Debug::GenerateCallFunctionStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-arm64.cc).
  // ----------- S t a t e -------------
  //  -- x1 : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x1.Bit(), 0, x10);
}


void Debug::GenerateCallFunctionStubRecordDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-arm64.cc).
  // ----------- S t a t e -------------
  //  -- x1 : function
  //  -- x2 : feedback array
  //  -- x3 : slot in feedback array
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x1.Bit() | x2.Bit() | x3.Bit(), 0, x10);
}


void Debug::GenerateCallConstructStubDebugBreak(MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-arm64.cc).
  // ----------- S t a t e -------------
  //  -- x0 : number of arguments (not smi)
  //  -- x1 : constructor function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x1.Bit(), x0.Bit(), x10);
}


void Debug::GenerateCallConstructStubRecordDebugBreak(MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-arm64.cc).
  // ----------- S t a t e -------------
  //  -- x0 : number of arguments (not smi)
  //  -- x1 : constructor function
  //  -- x2     : feedback array
  //  -- x3     : feedback slot (smi)
  // -----------------------------------
  Generate_DebugBreakCallHelper(
      masm, x1.Bit() | x2.Bit() | x3.Bit(), x0.Bit(), x10);
}


void Debug::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  InstructionAccurateScope scope(masm, Assembler::kDebugBreakSlotInstructions);

  __ RecordDebugBreakSlot();
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(Assembler::DEBUG_BREAK_NOP);
  }
}


void Debug::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0, 0, x10);
}


void Debug::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  masm->Abort(kLiveEditFrameDroppingIsNotSupportedOnARM64);
}


void Debug::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  masm->Abort(kLiveEditFrameDroppingIsNotSupportedOnARM64);
}

const bool Debug::kFrameDropperSupported = false;

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
