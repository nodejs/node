// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/assembler.h"
#include "src/codegen.h"
#include "src/debug.h"


namespace v8 {
namespace internal {

// Patch the code at the current PC with a call to the target address.
// Additional guard int3 instructions can be added if required.
void PatchCodeWithCall(Address pc, Address target, int guard_bytes) {
  int code_size = Assembler::kCallSequenceLength + guard_bytes;

  // Create a code patcher.
  CodePatcher patcher(pc, code_size);

// Add a label for checking the size of the code used for returning.
#ifdef DEBUG
  Label check_codesize;
  patcher.masm()->bind(&check_codesize);
#endif

  // Patch the code.
  patcher.masm()->movp(kScratchRegister, reinterpret_cast<void*>(target),
                       Assembler::RelocInfoNone());
  patcher.masm()->call(kScratchRegister);

  // Check that the size of the code generated is as expected.
  DCHECK_EQ(Assembler::kCallSequenceLength,
            patcher.masm()->SizeOfCodeGeneratedSince(&check_codesize));

  // Add the requested number of int3 instructions after the call.
  for (int i = 0; i < guard_bytes; i++) {
    patcher.masm()->int3();
  }

  CpuFeatures::FlushICache(pc, code_size);
}


// Patch the JS frame exit code with a debug break call. See
// CodeGenerator::VisitReturnStatement and VirtualFrame::Exit in codegen-x64.cc
// for the precise return instructions sequence.
void BreakLocation::SetDebugBreakAtReturn() {
  DCHECK(Assembler::kJSReturnSequenceLength >= Assembler::kCallSequenceLength);
  PatchCodeWithCall(
      pc(), debug_info_->GetIsolate()->builtins()->Return_DebugBreak()->entry(),
      Assembler::kJSReturnSequenceLength - Assembler::kCallSequenceLength);
}


void BreakLocation::SetDebugBreakAtSlot() {
  DCHECK(IsDebugBreakSlot());
  PatchCodeWithCall(
      pc(), debug_info_->GetIsolate()->builtins()->Slot_DebugBreak()->entry(),
      Assembler::kDebugBreakSlotLength - Assembler::kCallSequenceLength);
}


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs,
                                          bool convert_call_to_jmp) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    for (int i = 0; i < LiveEdit::kFramePaddingInitialSize; i++) {
      __ Push(Smi::FromInt(LiveEdit::kFramePaddingValue));
    }
    __ Push(Smi::FromInt(LiveEdit::kFramePaddingInitialSize));

    // Store the registers containing live values on the expression stack to
    // make sure that these are correctly updated during GC. Non object values
    // are stored as as two smis causing it to be untouched by GC.
    DCHECK((object_regs & ~kJSCallerSaved) == 0);
    DCHECK((non_object_regs & ~kJSCallerSaved) == 0);
    DCHECK((object_regs & non_object_regs) == 0);
    for (int i = 0; i < kNumJSCallerSaved; i++) {
      int r = JSCallerSavedCode(i);
      Register reg = { r };
      DCHECK(!reg.is(kScratchRegister));
      if ((object_regs & (1 << r)) != 0) {
        __ Push(reg);
      }
      if ((non_object_regs & (1 << r)) != 0) {
        __ PushRegisterAsTwoSmis(reg);
      }
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ Set(rax, 0);  // No arguments (argc == 0).
    __ Move(rbx, ExternalReference::debug_break(masm->isolate()));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    // Restore the register values from the expression stack.
    for (int i = kNumJSCallerSaved - 1; i >= 0; i--) {
      int r = JSCallerSavedCode(i);
      Register reg = { r };
      if (FLAG_debug_code) {
        __ Set(reg, kDebugZapValue);
      }
      if ((object_regs & (1 << r)) != 0) {
        __ Pop(reg);
      }
      // Reconstruct the 64-bit value from two smis.
      if ((non_object_regs & (1 << r)) != 0) {
        __ PopRegisterAsTwoSmis(reg);
      }
    }

    // Read current padding counter and skip corresponding number of words.
    __ Pop(kScratchRegister);
    __ SmiToInteger32(kScratchRegister, kScratchRegister);
    __ leap(rsp, Operand(rsp, kScratchRegister, times_pointer_size, 0));

    // Get rid of the internal frame.
  }

  // If this call did not replace a call but patched other code then there will
  // be an unwanted return address left on the stack. Here we get rid of that.
  if (convert_call_to_jmp) {
    __ addp(rsp, Immediate(kPCOnStackSize));
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference::debug_after_break_target_address(masm->isolate());
  __ Move(kScratchRegister, after_break_target);
  __ Jump(Operand(kScratchRegister, 0));
}


void DebugCodegen::GenerateCallICStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallICStub
  // ----------- S t a t e -------------
  //  -- rdx    : type feedback slot (smi)
  //  -- rdi    : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rdx.bit() | rdi.bit(), 0, false);
}


void DebugCodegen::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // Register state just before return from JS function (from codegen-x64.cc).
  // ----------- S t a t e -------------
  //  -- rax: return value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rax.bit(), 0, true);
}


void DebugCodegen::GenerateCallFunctionStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-x64.cc).
  // ----------- S t a t e -------------
  //  -- rdi : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, rdi.bit(), 0, false);
}


void DebugCodegen::GenerateCallConstructStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallConstructStub (from code-stubs-x64.cc).
  // rax is the actual number of arguments not encoded as a smi, see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  // -----------------------------------
  // The number of arguments in rax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, rdi.bit(), rax.bit(), false);
}


void DebugCodegen::GenerateCallConstructStubRecordDebugBreak(
    MacroAssembler* masm) {
  // Register state for CallConstructStub (from code-stubs-x64.cc).
  // rax is the actual number of arguments not encoded as a smi, see comment
  // above IC call.
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rbx: feedback array
  //  -- rdx: feedback slot (smi)
  // -----------------------------------
  // The number of arguments in rax is not smi encoded.
  Generate_DebugBreakCallHelper(masm, rbx.bit() | rdx.bit() | rdi.bit(),
                                rax.bit(), false);
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction.
  Label check_codesize;
  __ bind(&check_codesize);
  __ RecordDebugBreakSlot();
  __ Nop(Assembler::kDebugBreakSlotLength);
  DCHECK_EQ(Assembler::kDebugBreakSlotLength,
            masm->SizeOfCodeGeneratedSince(&check_codesize));
}


void DebugCodegen::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0, 0, true);
}


void DebugCodegen::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  masm->ret(0);
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference::debug_restarter_frame_function_pointer_address(
          masm->isolate());
  __ Move(rax, restarter_frame_function_slot);
  __ movp(Operand(rax, 0), Immediate(0));

  // We do not know our frame height, but set rsp based on rbp.
  __ leap(rsp, Operand(rbp, -1 * kPointerSize));

  __ Pop(rdi);  // Function.
  __ popq(rbp);

  // Load context from the function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Get function code.
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rdx, FieldOperand(rdx, SharedFunctionInfo::kCodeOffset));
  __ leap(rdx, FieldOperand(rdx, Code::kHeaderSize));

  // Re-run JSFunction, rdi is function, rsi is context.
  __ jmp(rdx);
}

const bool LiveEdit::kFrameDropperSupported = true;

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
