// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_PPC

#include "src/codegen.h"
#include "src/debug.h"

namespace v8 {
namespace internal {

void BreakLocation::SetDebugBreakAtReturn() {
  // Patch the code changing the return from JS function sequence from
  //
  //   LeaveFrame
  //   blr
  //
  // to a call to the debug break return code.
  // this uses a FIXED_SEQUENCE to load an address constant
  //
  //   mov r0, <address>
  //   mtlr r0
  //   blrl
  //   bkpt
  //
  CodePatcher patcher(pc(), Assembler::kJSReturnSequenceInstructions);
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(patcher.masm());
  patcher.masm()->mov(
      v8::internal::r0,
      Operand(reinterpret_cast<intptr_t>(debug_info_->GetIsolate()
                                             ->builtins()
                                             ->Return_DebugBreak()
                                             ->entry())));
  patcher.masm()->mtctr(v8::internal::r0);
  patcher.masm()->bctrl();
  patcher.masm()->bkpt(0);
}


void BreakLocation::SetDebugBreakAtSlot() {
  DCHECK(IsDebugBreakSlot());
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
  CodePatcher patcher(pc(), Assembler::kDebugBreakSlotInstructions);
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(patcher.masm());
  patcher.masm()->mov(
      v8::internal::r0,
      Operand(reinterpret_cast<intptr_t>(
          debug_info_->GetIsolate()->builtins()->Slot_DebugBreak()->entry())));
  patcher.masm()->mtctr(v8::internal::r0);
  patcher.masm()->bctrl();
}


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ LoadSmiLiteral(ip, Smi::FromInt(LiveEdit::kFramePaddingValue));
    for (int i = 0; i < LiveEdit::kFramePaddingInitialSize; i++) {
      __ push(ip);
    }
    __ LoadSmiLiteral(ip, Smi::FromInt(LiveEdit::kFramePaddingInitialSize));
    __ push(ip);

    // Store the registers containing live values on the expression stack to
    // make sure that these are correctly updated during GC. Non object values
    // are stored as a smi causing it to be untouched by GC.
    DCHECK((object_regs & ~kJSCallerSaved) == 0);
    DCHECK((non_object_regs & ~kJSCallerSaved) == 0);
    DCHECK((object_regs & non_object_regs) == 0);
    if ((object_regs | non_object_regs) != 0) {
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        int r = JSCallerSavedCode(i);
        Register reg = {r};
        if ((non_object_regs & (1 << r)) != 0) {
          if (FLAG_debug_code) {
            __ TestUnsignedSmiCandidate(reg, r0);
            __ Assert(eq, kUnableToEncodeValueAsSmi, cr0);
          }
          __ SmiTag(reg);
        }
      }
      __ MultiPush(object_regs | non_object_regs);
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ mov(r3, Operand::Zero());  // no arguments
    __ mov(r4, Operand(ExternalReference::debug_break(masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    // Restore the register values from the expression stack.
    if ((object_regs | non_object_regs) != 0) {
      __ MultiPop(object_regs | non_object_regs);
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        int r = JSCallerSavedCode(i);
        Register reg = {r};
        if ((non_object_regs & (1 << r)) != 0) {
          __ SmiUntag(reg);
        }
        if (FLAG_debug_code &&
            (((object_regs | non_object_regs) & (1 << r)) == 0)) {
          __ mov(reg, Operand(kDebugZapValue));
        }
      }
    }

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


void DebugCodegen::GenerateCallICStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallICStub
  // ----------- S t a t e -------------
  //  -- r4 : function
  //  -- r6 : slot in feedback array (smi)
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r4.bit() | r6.bit(), 0);
}


void DebugCodegen::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC load (from ic-ppc.cc).
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();
  RegList regs = receiver.bit() | name.bit();
  if (FLAG_vector_ics) {
    regs |= VectorLoadICTrampolineDescriptor::SlotRegister().bit();
  }
  Generate_DebugBreakCallHelper(masm, regs, 0);
}


void DebugCodegen::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC store (from ic-ppc.cc).
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Generate_DebugBreakCallHelper(masm, receiver.bit() | name.bit() | value.bit(),
                                0);
}


void DebugCodegen::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for keyed IC load (from ic-ppc.cc).
  GenerateLoadICDebugBreak(masm);
}


void DebugCodegen::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC keyed store call (from ic-ppc.cc).
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Generate_DebugBreakCallHelper(masm, receiver.bit() | name.bit() | value.bit(),
                                0);
}


void DebugCodegen::GenerateCompareNilICDebugBreak(MacroAssembler* masm) {
  // Register state for CompareNil IC
  // ----------- S t a t e -------------
  //  -- r3    : value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r3.bit(), 0);
}


void DebugCodegen::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r3 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, r3.bit(), 0);
}


void DebugCodegen::GenerateCallFunctionStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-ppc.cc).
  // ----------- S t a t e -------------
  //  -- r4 : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r4.bit(), 0);
}


void DebugCodegen::GenerateCallConstructStubDebugBreak(MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-ppc.cc)
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments (not smi)
  //  -- r4     : constructor function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r4.bit(), r3.bit());
}


void DebugCodegen::GenerateCallConstructStubRecordDebugBreak(
    MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-ppc.cc)
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments (not smi)
  //  -- r4     : constructor function
  //  -- r5     : feedback array
  //  -- r6     : feedback slot (smi)
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r4.bit() | r5.bit() | r6.bit(), r3.bit());
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the trampoline pool in the debug break slot code.
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm);
  Label check_codesize;
  __ bind(&check_codesize);
  __ RecordDebugBreakSlot();
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(MacroAssembler::DEBUG_BREAK_NOP);
  }
  DCHECK_EQ(Assembler::kDebugBreakSlotInstructions,
            masm->InstructionsGeneratedSince(&check_codesize));
}


void DebugCodegen::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0, 0);
}


void DebugCodegen::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  __ Ret();
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference::debug_restarter_frame_function_pointer_address(
          masm->isolate());
  __ mov(ip, Operand(restarter_frame_function_slot));
  __ li(r4, Operand::Zero());
  __ StoreP(r4, MemOperand(ip, 0));

  // Load the function pointer off of our current stack frame.
  __ LoadP(r4, MemOperand(fp, StandardFrameConstants::kConstantPoolOffset -
                                  kPointerSize));

  // Pop return address and frame
  __ LeaveFrame(StackFrame::INTERNAL);

  // Load context from the function.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // Get function code.
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Re-run JSFunction, r4 is function, cp is context.
  __ Jump(ip);
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_PPC
