// Copyright 2012 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_X64)

#include "assembler.h"
#include "codegen.h"
#include "debug.h"


namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT

bool BreakLocationIterator::IsDebugBreakAtReturn()  {
  return Debug::IsDebugBreakAtReturn(rinfo());
}


// Patch the JS frame exit code with a debug break call. See
// CodeGenerator::VisitReturnStatement and VirtualFrame::Exit in codegen-x64.cc
// for the precise return instructions sequence.
void BreakLocationIterator::SetDebugBreakAtReturn()  {
  ASSERT(Assembler::kJSReturnSequenceLength >=
         Assembler::kCallInstructionLength);
  rinfo()->PatchCodeWithCall(
      Isolate::Current()->debug()->debug_break_return()->entry(),
      Assembler::kJSReturnSequenceLength - Assembler::kCallInstructionLength);
}


// Restore the JS frame exit code.
void BreakLocationIterator::ClearDebugBreakAtReturn() {
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kJSReturnSequenceLength);
}


// A debug break in the frame exit code is identified by the JS frame exit code
// having been patched with a call instruction.
bool Debug::IsDebugBreakAtReturn(v8::internal::RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  return rinfo->IsPatchedReturnSequence();
}


bool BreakLocationIterator::IsDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  // Check whether the debug break slot instructions have been patched.
  return !Assembler::IsNop(rinfo()->pc());
}


void BreakLocationIterator::SetDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  rinfo()->PatchCodeWithCall(
      Isolate::Current()->debug()->debug_break_slot()->entry(),
      Assembler::kDebugBreakSlotLength - Assembler::kCallInstructionLength);
}


void BreakLocationIterator::ClearDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  rinfo()->PatchCode(original_rinfo()->pc(), Assembler::kDebugBreakSlotLength);
}

const bool Debug::FramePaddingLayout::kIsSupported = true;


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs,
                                          bool convert_call_to_jmp) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    for (int i = 0; i < Debug::FramePaddingLayout::kInitialSize; i++) {
      __ Push(Smi::FromInt(Debug::FramePaddingLayout::kPaddingValue));
    }
    __ Push(Smi::FromInt(Debug::FramePaddingLayout::kInitialSize));

    // Store the registers containing live values on the expression stack to
    // make sure that these are correctly updated during GC. Non object values
    // are stored as as two smis causing it to be untouched by GC.
    ASSERT((object_regs & ~kJSCallerSaved) == 0);
    ASSERT((non_object_regs & ~kJSCallerSaved) == 0);
    ASSERT((object_regs & non_object_regs) == 0);
    for (int i = 0; i < kNumJSCallerSaved; i++) {
      int r = JSCallerSavedCode(i);
      Register reg = { r };
      ASSERT(!reg.is(kScratchRegister));
      if ((object_regs & (1 << r)) != 0) {
        __ push(reg);
      }
      // Store the 64-bit value as two smis.
      if ((non_object_regs & (1 << r)) != 0) {
        __ movq(kScratchRegister, reg);
        __ Integer32ToSmi(reg, reg);
        __ push(reg);
        __ sar(kScratchRegister, Immediate(32));
        __ Integer32ToSmi(kScratchRegister, kScratchRegister);
        __ push(kScratchRegister);
      }
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ Set(rax, 0);  // No arguments (argc == 0).
    __ movq(rbx, ExternalReference::debug_break(masm->isolate()));

    CEntryStub ceb(1);
    __ CallStub(&ceb);

    // Restore the register values from the expression stack.
    for (int i = kNumJSCallerSaved - 1; i >= 0; i--) {
      int r = JSCallerSavedCode(i);
      Register reg = { r };
      if (FLAG_debug_code) {
        __ Set(reg, kDebugZapValue);
      }
      if ((object_regs & (1 << r)) != 0) {
        __ pop(reg);
      }
      // Reconstruct the 64-bit value from two smis.
      if ((non_object_regs & (1 << r)) != 0) {
        __ pop(kScratchRegister);
        __ SmiToInteger32(kScratchRegister, kScratchRegister);
        __ shl(kScratchRegister, Immediate(32));
        __ pop(reg);
        __ SmiToInteger32(reg, reg);
        __ or_(reg, kScratchRegister);
      }
    }

    // Read current padding counter and skip corresponding number of words.
    __ pop(kScratchRegister);
    __ SmiToInteger32(kScratchRegister, kScratchRegister);
    __ lea(rsp, Operand(rsp, kScratchRegister, times_pointer_size, 0));

    // Get rid of the internal frame.
  }

  // If this call did not replace a call but patched other code then there will
  // be an unwanted return address left on the stack. Here we get rid of that.
  if (convert_call_to_jmp) {
    __ addq(rsp, Immediate(kPointerSize));
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference(Debug_Address::AfterBreakTarget(), masm->isolate());
  __ movq(kScratchRegister, after_break_target);
  __ jmp(Operand(kScratchRegister, 0));
}


void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for IC load call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rax.bit() | rcx.bit(), 0, false);
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Register state for IC store call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  // -----------------------------------
  Generate_DebugBreakCallHelper(
      masm, rax.bit() | rcx.bit() | rdx.bit(), 0, false);
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax     : key
  //  -- rdx     : receiver
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rax.bit() | rdx.bit(), 0, false);
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  // -----------------------------------
  Generate_DebugBreakCallHelper(
      masm, rax.bit() | rcx.bit() | rdx.bit(), 0, false);
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Register state for IC call call (from ic-x64.cc)
  // ----------- S t a t e -------------
  //  -- rcx: function name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rcx.bit(), 0, false);
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax: return value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rax.bit(), 0, true);
}


void Debug::GenerateCallFunctionStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-x64.cc).
  // ----------- S t a t e -------------
  //  -- rdi : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rdi.bit(), 0, false);
}


void Debug::GenerateCallFunctionStubRecordDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-x64.cc).
  // ----------- S t a t e -------------
  //  -- rdi : function
  //  -- rbx: cache cell for call target
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rbx.bit() | rdi.bit(), 0, false);
}


void Debug::GenerateCallConstructStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallConstructStub (from code-stubs-x64.cc).
  // rax is the actual number of arguments not encoded as a smi, see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  // -----------------------------------
  // The number of arguments in rax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, rdi.bit(), rax.bit(), false);
}


void Debug::GenerateCallConstructStubRecordDebugBreak(MacroAssembler* masm) {
  // Register state for CallConstructStub (from code-stubs-x64.cc).
  // rax is the actual number of arguments not encoded as a smi, see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rbx: cache cell for call target
  // -----------------------------------
  // The number of arguments in rax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, rbx.bit() | rdi.bit(), rax.bit(), false);
}


void Debug::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction.
  Label check_codesize;
  __ bind(&check_codesize);
  __ RecordDebugBreakSlot();
  __ Nop(Assembler::kDebugBreakSlotLength);
  ASSERT_EQ(Assembler::kDebugBreakSlotLength,
            masm->SizeOfCodeGeneratedSince(&check_codesize));
}


void Debug::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0, 0, true);
}


void Debug::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  masm->ret(0);
}


void Debug::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference(Debug_Address::RestarterFrameFunctionPointer(),
                        masm->isolate());
  __ movq(rax, restarter_frame_function_slot);
  __ movq(Operand(rax, 0), Immediate(0));

  // We do not know our frame height, but set rsp based on rbp.
  __ lea(rsp, Operand(rbp, -1 * kPointerSize));

  __ pop(rdi);  // Function.
  __ pop(rbp);

  // Load context from the function.
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Get function code.
  __ movq(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(rdx, FieldOperand(rdx, SharedFunctionInfo::kCodeOffset));
  __ lea(rdx, FieldOperand(rdx, Code::kHeaderSize));

  // Re-run JSFunction, rdi is function, rsi is context.
  __ jmp(rdx);
}

const bool Debug::kFrameDropperSupported = true;

#undef __

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
