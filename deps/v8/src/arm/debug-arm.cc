// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM

#include "src/codegen.h"
#include "src/debug.h"

namespace v8 {
namespace internal {

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
  //   ldr ip, [pc, #0]
  //   blx ip
  //   <debug break return code entry point address>
  //   bkpt 0
  CodePatcher patcher(rinfo()->pc(), Assembler::kJSReturnSequenceInstructions);
  patcher.masm()->ldr(v8::internal::ip, MemOperand(v8::internal::pc, 0));
  patcher.masm()->blx(v8::internal::ip);
  patcher.Emit(
      debug_info_->GetIsolate()->builtins()->Return_DebugBreak()->entry());
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
  DCHECK(RelocInfo::IsJSReturn(rinfo->rmode()));
  return rinfo->IsPatchedReturnSequence();
}


bool BreakLocationIterator::IsDebugBreakAtSlot() {
  DCHECK(IsDebugBreakSlot());
  // Check whether the debug break slot instructions have been patched.
  return rinfo()->IsPatchedDebugBreakSlotSequence();
}


void BreakLocationIterator::SetDebugBreakAtSlot() {
  DCHECK(IsDebugBreakSlot());
  // Patch the code changing the debug break slot code from
  //   mov r2, r2
  //   mov r2, r2
  //   mov r2, r2
  // to a call to the debug break slot code.
  //   ldr ip, [pc, #0]
  //   blx ip
  //   <debug break slot code entry point address>
  CodePatcher patcher(rinfo()->pc(), Assembler::kDebugBreakSlotInstructions);
  patcher.masm()->ldr(v8::internal::ip, MemOperand(v8::internal::pc, 0));
  patcher.masm()->blx(v8::internal::ip);
  patcher.Emit(
      debug_info_->GetIsolate()->builtins()->Slot_DebugBreak()->entry());
}


void BreakLocationIterator::ClearDebugBreakAtSlot() {
  DCHECK(IsDebugBreakSlot());
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kDebugBreakSlotInstructions);
}


#define __ ACCESS_MASM(masm)


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ mov(ip, Operand(Smi::FromInt(LiveEdit::kFramePaddingValue)));
    for (int i = 0; i < LiveEdit::kFramePaddingInitialSize; i++) {
      __ push(ip);
    }
    __ mov(ip, Operand(Smi::FromInt(LiveEdit::kFramePaddingInitialSize)));
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
        Register reg = { r };
        if ((non_object_regs & (1 << r)) != 0) {
          if (FLAG_debug_code) {
            __ tst(reg, Operand(0xc0000000));
            __ Assert(eq, kUnableToEncodeValueAsSmi);
          }
          __ SmiTag(reg);
        }
      }
      __ stm(db_w, sp, object_regs | non_object_regs);
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ mov(r0, Operand::Zero());  // no arguments
    __ mov(r1, Operand(ExternalReference::debug_break(masm->isolate())));

    CEntryStub ceb(masm->isolate(), 1);
    __ CallStub(&ceb);

    // Restore the register values from the expression stack.
    if ((object_regs | non_object_regs) != 0) {
      __ ldm(ia_w, sp, object_regs | non_object_regs);
      for (int i = 0; i < kNumJSCallerSaved; i++) {
        int r = JSCallerSavedCode(i);
        Register reg = { r };
        if ((non_object_regs & (1 << r)) != 0) {
          __ SmiUntag(reg);
        }
        if (FLAG_debug_code &&
            (((object_regs |non_object_regs) & (1 << r)) == 0)) {
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
  __ ldr(ip, MemOperand(ip));
  __ Jump(ip);
}


void DebugCodegen::GenerateCallICStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallICStub
  // ----------- S t a t e -------------
  //  -- r1 : function
  //  -- r3 : slot in feedback array (smi)
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r1.bit() | r3.bit(), 0);
}


void DebugCodegen::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC load (from ic-arm.cc).
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();
  RegList regs = receiver.bit() | name.bit();
  if (FLAG_vector_ics) {
    regs |= VectorLoadICTrampolineDescriptor::SlotRegister().bit();
  }
  Generate_DebugBreakCallHelper(masm, regs, 0);
}


void DebugCodegen::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC store (from ic-arm.cc).
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Generate_DebugBreakCallHelper(
      masm, receiver.bit() | name.bit() | value.bit(), 0);
}


void DebugCodegen::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for keyed IC load (from ic-arm.cc).
  GenerateLoadICDebugBreak(masm);
}


void DebugCodegen::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC keyed store call (from ic-arm.cc).
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Generate_DebugBreakCallHelper(
      masm, receiver.bit() | name.bit() | value.bit(), 0);
}


void DebugCodegen::GenerateCompareNilICDebugBreak(MacroAssembler* masm) {
  // Register state for CompareNil IC
  // ----------- S t a t e -------------
  //  -- r0    : value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r0.bit(), 0);
}


void DebugCodegen::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, r0.bit(), 0);
}


void DebugCodegen::GenerateCallFunctionStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-arm.cc).
  // ----------- S t a t e -------------
  //  -- r1 : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r1.bit(), 0);
}


void DebugCodegen::GenerateCallConstructStubDebugBreak(MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-arm.cc)
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments (not smi)
  //  -- r1     : constructor function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r1.bit(), r0.bit());
}


void DebugCodegen::GenerateCallConstructStubRecordDebugBreak(
    MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-arm.cc)
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments (not smi)
  //  -- r1     : constructor function
  //  -- r2     : feedback array
  //  -- r3     : feedback slot (smi)
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, r1.bit() | r2.bit() | r3.bit(), r0.bit());
}


void DebugCodegen::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  Assembler::BlockConstPoolScope block_const_pool(masm);
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
  __ mov(r1, Operand::Zero());
  __ str(r1, MemOperand(ip, 0));

  // Load the function pointer off of our current stack frame.
  __ ldr(r1, MemOperand(fp,
         StandardFrameConstants::kConstantPoolOffset - kPointerSize));

  // Pop return address, frame and constant pool pointer (if
  // FLAG_enable_ool_constant_pool).
  __ LeaveFrame(StackFrame::INTERNAL);

  { ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    // Load context from the function.
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    // Get function code.
    __ ldr(ip, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
    __ add(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Re-run JSFunction, r1 is function, cp is context.
    __ Jump(ip);
  }
}


const bool LiveEdit::kFrameDropperSupported = true;

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
