// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_IA32)

#include "codegen.h"
#include "debug.h"


namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT

bool BreakLocationIterator::IsDebugBreakAtReturn() {
  return Debug::IsDebugBreakAtReturn(rinfo());
}


// Patch the JS frame exit code with a debug break call. See
// CodeGenerator::VisitReturnStatement and VirtualFrame::Exit in codegen-ia32.cc
// for the precise return instructions sequence.
void BreakLocationIterator::SetDebugBreakAtReturn() {
  ASSERT(Assembler::kJSReturnSequenceLength >=
         Assembler::kCallInstructionLength);
  Isolate* isolate = Isolate::Current();
  rinfo()->PatchCodeWithCall(isolate->debug()->debug_break_return()->entry(),
      Assembler::kJSReturnSequenceLength - Assembler::kCallInstructionLength);
}


// Restore the JS frame exit code.
void BreakLocationIterator::ClearDebugBreakAtReturn() {
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kJSReturnSequenceLength);
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
  Isolate* isolate = Isolate::Current();
  rinfo()->PatchCodeWithCall(
      isolate->debug()->debug_break_slot()->entry(),
      Assembler::kDebugBreakSlotLength - Assembler::kCallInstructionLength);
}


void BreakLocationIterator::ClearDebugBreakAtSlot() {
  ASSERT(IsDebugBreakSlot());
  rinfo()->PatchCode(original_rinfo()->pc(), Assembler::kDebugBreakSlotLength);
}


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs,
                                          bool convert_call_to_jmp) {
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Store the registers containing live values on the expression stack to
  // make sure that these are correctly updated during GC. Non object values
  // are stored as a smi causing it to be untouched by GC.
  ASSERT((object_regs & ~kJSCallerSaved) == 0);
  ASSERT((non_object_regs & ~kJSCallerSaved) == 0);
  ASSERT((object_regs & non_object_regs) == 0);
  for (int i = 0; i < kNumJSCallerSaved; i++) {
    int r = JSCallerSavedCode(i);
    Register reg = { r };
    if ((object_regs & (1 << r)) != 0) {
      __ push(reg);
    }
    if ((non_object_regs & (1 << r)) != 0) {
      if (FLAG_debug_code) {
        __ test(reg, Immediate(0xc0000000));
        __ Assert(zero, "Unable to encode value as smi");
      }
      __ SmiTag(reg);
      __ push(reg);
    }
  }

#ifdef DEBUG
  __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
  __ Set(eax, Immediate(0));  // No arguments.
  __ mov(ebx, Immediate(ExternalReference::debug_break(masm->isolate())));

  CEntryStub ceb(1);
  __ CallStub(&ceb);

  // Restore the register values containing object pointers from the expression
  // stack.
  for (int i = kNumJSCallerSaved; --i >= 0;) {
    int r = JSCallerSavedCode(i);
    Register reg = { r };
    if (FLAG_debug_code) {
      __ Set(reg, Immediate(kDebugZapValue));
    }
    if ((object_regs & (1 << r)) != 0) {
      __ pop(reg);
    }
    if ((non_object_regs & (1 << r)) != 0) {
      __ pop(reg);
      __ SmiUntag(reg);
    }
  }

  // Get rid of the internal frame.
  __ LeaveInternalFrame();

  // If this call did not replace a call but patched other code then there will
  // be an unwanted return address left on the stack. Here we get rid of that.
  if (convert_call_to_jmp) {
    __ add(Operand(esp), Immediate(kPointerSize));
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference(Debug_Address::AfterBreakTarget(), masm->isolate());
  __ jmp(Operand::StaticVariable(after_break_target));
}


void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for IC load call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, eax.bit() | ecx.bit(), 0, false);
}


void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Register state for IC store call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  // -----------------------------------
  Generate_DebugBreakCallHelper(
      masm, eax.bit() | ecx.bit() | edx.bit(), 0, false);
}


void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- edx    : receiver
  //  -- eax    : key
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, eax.bit() | edx.bit(), 0, false);
}


void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC load call (from ic-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  // -----------------------------------
  Generate_DebugBreakCallHelper(
      masm, eax.bit() | ecx.bit() | edx.bit(), 0, false);
}


void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  // Register state for keyed IC call call (from ic-ia32.cc)
  // ----------- S t a t e -------------
  //  -- ecx: name
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, ecx.bit(), 0, false);
}


void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-ia32.cc).
  // eax is the actual number of arguments not encoded as a smi see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- eax: number of arguments (not smi)
  //  -- edi: constructor function
  // -----------------------------------
  // The number of arguments in eax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, edi.bit(), eax.bit(), false);
}


void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-ia32.cc).
  // ----------- S t a t e -------------
  //  -- eax: return value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, eax.bit(), 0, true);
}


void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  // Register state for stub CallFunction (from CallFunctionStub in ic-ia32.cc).
  // ----------- S t a t e -------------
  //  No registers used on entry.
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, 0, 0, false);
}


void Debug::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction.
  Label check_codesize;
  __ bind(&check_codesize);
  __ RecordDebugBreakSlot();
  for (int i = 0; i < Assembler::kDebugBreakSlotLength; i++) {
    __ nop();
  }
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
  __ mov(Operand::StaticVariable(restarter_frame_function_slot), Immediate(0));

  // We do not know our frame height, but set esp based on ebp.
  __ lea(esp, Operand(ebp, -1 * kPointerSize));

  __ pop(edi);  // Function.
  __ pop(ebp);

  // Load context from the function.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Get function code.
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(edx, FieldOperand(edx, SharedFunctionInfo::kCodeOffset));
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));

  // Re-run JSFunction, edi is function, esi is context.
  __ jmp(Operand(edx));
}

const bool Debug::kFrameDropperSupported = true;

#undef __

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
