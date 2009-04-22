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

namespace v8 { namespace internal {


// Currently debug break is not supported in frame exit code on ARM.
bool BreakLocationIterator::IsDebugBreakAtReturn() {
  return false;
}


// Currently debug break is not supported in frame exit code on ARM.
void BreakLocationIterator::SetDebugBreakAtReturn() {
  UNIMPLEMENTED();
}


// Currently debug break is not supported in frame exit code on ARM.
void BreakLocationIterator::ClearDebugBreakAtReturn() {
  UNIMPLEMENTED();
}


bool Debug::IsDebugBreakAtReturn(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  // Currently debug break is not supported in frame exit code on ARM.
  return false;
}


#define __ masm->


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList pointer_regs) {
  // Save the content of all general purpose registers in memory. This copy in
  // memory is later pushed onto the JS expression stack for the fake JS frame
  // generated and also to the C frame generated on top of that. In the JS
  // frame ONLY the registers containing pointers will be pushed on the
  // expression stack. This causes the GC to update these  pointers so that
  // they will have the correct value when returning from the debugger.
  __ SaveRegistersToMemory(kJSCallerSaved);

  __ EnterInternalFrame();

  // Store the registers containing object pointers on the expression stack to
  // make sure that these are correctly updated during GC.
  // Use sp as base to push.
  __ CopyRegistersFromMemoryToStack(sp, pointer_regs);

#ifdef DEBUG
  __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
  __ mov(r0, Operand(0));  // no arguments
  __ mov(r1, Operand(ExternalReference::debug_break()));

  CEntryDebugBreakStub ceb;
  __ CallStub(&ceb);

  // Restore the register values containing object pointers from the expression
  // stack in the reverse order as they where pushed.
  // Use sp as base to pop.
  __ CopyRegistersFromStackToMemory(sp, r3, pointer_regs);

  __ LeaveInternalFrame();

  // Inlined ExitJSFrame ends here.

  // Finally restore all registers.
  __ RestoreRegistersFromMemory(kJSCallerSaved);

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
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  // Registers r0 and r2 contain objects that needs to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, r0.bit() | r2.bit());
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC store (from ic-arm.cc).
  // ----------- S t a t e -------------
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  // Registers r0 and r2 contain objects that needs to be pushed on the
  // expression stack of the fake JS frame.
  Generate_DebugBreakCallHelper(masm, r0.bit() | r2.bit());
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Keyed load IC not implemented on ARM.
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Keyed store IC not implemented on ARM.
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC call (from ic-arm.cc)
  // ----------- S t a t e -------------
  //  -- r0: number of arguments
  //  -- r1: receiver
  //  -- lr: return address
  // -----------------------------------
  // Register r1 contains an object that needs to be pushed on the expression
  // stack of the fake JS frame. r0 is the actual number of arguments not
  // encoded as a smi, therefore it cannot be on the expression stack of the
  // fake JS frame as it can easily be an invalid pointer (e.g. 1). r0 will be
  // pushed on the stack of the C frame and restored from there.
  Generate_DebugBreakCallHelper(masm, r1.bit());
}


void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, r0.bit());
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, r0.bit());
}


void Debug::GenerateReturnDebugBreakEntry(MacroAssembler* masm) {
  // Generate nothing as this handling of debug break return is not done this
  // way on ARM  - yet.
}


void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  // Generate nothing as CodeStub CallFunction is not used on ARM.
}


#undef __


} }  // namespace v8::internal
