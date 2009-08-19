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

#include "codegen-inl.h"
#include "debug.h"


namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT

// A debug break in the frame exit code is identified by a call instruction.
bool BreakLocationIterator::IsDebugBreakAtReturn() {
  // Opcode E8 is call.
  return Debug::IsDebugBreakAtReturn(rinfo());
}


// Patch the JS frame exit code with a debug break call. See
// CodeGenerator::VisitReturnStatement and VirtualFrame::Exit in codegen-ia32.cc
// for the precise return instructions sequence.
void BreakLocationIterator::SetDebugBreakAtReturn() {
  ASSERT(Debug::kIa32JSReturnSequenceLength >=
         Debug::kIa32CallInstructionLength);
  rinfo()->PatchCodeWithCall(Debug::debug_break_return_entry()->entry(),
      Debug::kIa32JSReturnSequenceLength - Debug::kIa32CallInstructionLength);
}


// Restore the JS frame exit code.
void BreakLocationIterator::ClearDebugBreakAtReturn() {
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Debug::kIa32JSReturnSequenceLength);
}


// Check whether the JS frame exit code has been patched with a debug break.
bool Debug::IsDebugBreakAtReturn(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  // Opcode E8 is call.
  return (*(rinfo->pc()) == 0xE8);
}


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList pointer_regs,
                                          bool convert_call_to_jmp) {
  // Save the content of all general purpose registers in memory. This copy in
  // memory is later pushed onto the JS expression stack for the fake JS frame
  // generated and also to the C frame generated on top of that. In the JS
  // frame ONLY the registers containing pointers will be pushed on the
  // expression stack. This causes the GC to update these pointers so that
  // they will have the correct value when returning from the debugger.
  __ SaveRegistersToMemory(kJSCallerSaved);

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Store the registers containing object pointers on the expression stack to
  // make sure that these are correctly updated during GC.
  __ PushRegistersFromMemory(pointer_regs);

#ifdef DEBUG
  __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
  __ Set(eax, Immediate(0));  // no arguments
  __ mov(ebx, Immediate(ExternalReference::debug_break()));

  CEntryDebugBreakStub ceb;
  __ CallStub(&ceb);

  // Restore the register values containing object pointers from the expression
  // stack in the reverse order as they where pushed.
  __ PopRegistersToMemory(pointer_regs);

  // Get rid of the internal frame.
  __ LeaveInternalFrame();

  // If this call did not replace a call but patched other code then there will
  // be an unwanted return address left on the stack. Here we get rid of that.
  if (convert_call_to_jmp) {
    __ pop(eax);
  }

  // Finally restore all registers.
  __ RestoreRegistersFromMemory(kJSCallerSaved);

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference(Debug_Address::AfterBreakTarget());
  __ jmp(Operand::StaticVariable(after_break_target));
}


void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for IC load call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- ecx    : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, ecx.bit(), false);
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // REgister state for IC store call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, eax.bit() | ecx.bit(), false);
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  No registers used on entry.
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, 0, false);
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax    : value
  // -----------------------------------
  // Register eax contains an object that needs to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, eax.bit(), false);
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC call call (from ic-ia32.cc)
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  // -----------------------------------
  // The number of arguments in eax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, 0, false);
}


void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-ia32.cc).
  // eax is the actual number of arguments not encoded as a smi see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- eax: number of arguments
  // -----------------------------------
  // The number of arguments in eax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, 0, false);
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax: return value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, eax.bit(), true);
}


void Debug::GenerateReturnDebugBreakEntry(MacroAssembler* masm) {
  // OK to clobber ebx as we are returning from a JS function through the code
  // generated by CodeGenerator::GenerateReturnSequence()
  ExternalReference debug_break_return =
      ExternalReference(Debug_Address::DebugBreakReturn());
  __ mov(ebx, Operand::StaticVariable(debug_break_return));
  __ add(Operand(ebx), Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(Operand(ebx));
}


void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  // Register state for stub CallFunction (from CallFunctionStub in ic-ia32.cc).
  // ----------- S t a t e -------------
  //  No registers used on entry.
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, 0, false);
}


#undef __

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal
