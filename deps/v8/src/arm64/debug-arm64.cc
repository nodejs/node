// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/codegen.h"
#include "src/debug.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)

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
      debug_info_->GetIsolate()->builtins()->Return_DebugBreak()->entry();

  // The first instruction of a patched return sequence must be a load literal
  // loading the address of the debug break return code.
  patcher.ldr_pcrel(ip0, (3 * kInstructionSize) >> kLoadLiteralScaleLog2);
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
  DCHECK(RelocInfo::IsJSReturn(rinfo->rmode()));
  return rinfo->IsPatchedReturnSequence();
}


bool BreakLocationIterator::IsDebugBreakAtSlot() {
  DCHECK(IsDebugBreakSlot());
  // Check whether the debug break slot instructions have been patched.
  return rinfo()->IsPatchedDebugBreakSlotSequence();
}


void BreakLocationIterator::SetDebugBreakAtSlot() {
  // Patch the code emitted by DebugCodegen::GenerateSlots, changing the debug
  // break slot code from
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
      debug_info_->GetIsolate()->builtins()->Slot_DebugBreak()->entry();

  // The first instruction of a patched debug break slot must be a load literal
  // loading the address of the debug break slot code.
  patcher.ldr_pcrel(ip0, (2 * kInstructionSize) >> kLoadLiteralScaleLog2);
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
  DCHECK(IsDebugBreakSlot());
  rinfo()->PatchCode(original_rinfo()->pc(),
                     Assembler::kDebugBreakSlotInstructions);
}


static void Generate_DebugBreakCallHelper(MacroAssembler* masm,
                                          RegList object_regs,
                                          RegList non_object_regs,
                                          Register scratch) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Load padding words on stack.
    __ Mov(scratch, Smi::FromInt(LiveEdit::kFramePaddingValue));
    __ PushMultipleTimes(scratch, LiveEdit::kFramePaddingInitialSize);
    __ Mov(scratch, Smi::FromInt(LiveEdit::kFramePaddingInitialSize));
    __ Push(scratch);

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
    DCHECK((~kCallerSaved.list() & object_regs) == 0);
    DCHECK((~kCallerSaved.list() & non_object_regs) == 0);
    DCHECK((object_regs & non_object_regs) == 0);
    DCHECK((scratch.Bit() & object_regs) == 0);
    DCHECK((scratch.Bit() & non_object_regs) == 0);
    DCHECK((masm->TmpList()->list() & (object_regs | non_object_regs)) == 0);
    STATIC_ASSERT(kSmiValueSize == 32);

    CPURegList non_object_list =
        CPURegList(CPURegister::kRegister, kXRegSizeInBits, non_object_regs);
    while (!non_object_list.IsEmpty()) {
      // Store each non-object register as two SMIs.
      Register reg = Register(non_object_list.PopLowestIndex());
      __ Lsr(scratch, reg, 32);
      __ SmiTagAndPush(scratch, reg);

      // Stack:
      //  jssp[12]: reg[63:32]
      //  jssp[8]: 0x00000000 (SMI tag & padding)
      //  jssp[4]: reg[31:0]
      //  jssp[0]: 0x00000000 (SMI tag & padding)
      STATIC_ASSERT(kSmiTag == 0);
      STATIC_ASSERT(static_cast<unsigned>(kSmiShift) == kWRegSizeInBits);
    }

    if (object_regs != 0) {
      __ PushXRegList(object_regs);
    }

#ifdef DEBUG
    __ RecordComment("// Calling from debug break to runtime - come in - over");
#endif
    __ Mov(x0, 0);  // No arguments.
    __ Mov(x1, ExternalReference::debug_break(masm->isolate()));

    CEntryStub stub(masm->isolate(), 1);
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

    // Don't bother removing padding bytes pushed on the stack
    // as the frame is going to be restored right away.

    // Leave the internal frame.
  }

  // Now that the break point has been handled, resume normal execution by
  // jumping to the target address intended by the caller and that was
  // overwritten by the address of DebugBreakXXX.
  ExternalReference after_break_target =
      ExternalReference::debug_after_break_target_address(masm->isolate());
  __ Mov(scratch, after_break_target);
  __ Ldr(scratch, MemOperand(scratch));
  __ Br(scratch);
}


void DebugCodegen::GenerateCallICStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallICStub
  // ----------- S t a t e -------------
  //  -- x1 : function
  //  -- x3 : slot in feedback array
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x1.Bit() | x3.Bit(), 0, x10);
}


void DebugCodegen::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC load (from ic-arm.cc).
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();
  Generate_DebugBreakCallHelper(masm, receiver.Bit() | name.Bit(), 0, x10);
}


void DebugCodegen::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC store (from ic-arm64.cc).
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Generate_DebugBreakCallHelper(
      masm, receiver.Bit() | name.Bit() | value.Bit(), 0, x10);
}


void DebugCodegen::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  // Calling convention for keyed IC load (from ic-arm.cc).
  GenerateLoadICDebugBreak(masm);
}


void DebugCodegen::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  // Calling convention for IC keyed store call (from ic-arm64.cc).
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Generate_DebugBreakCallHelper(
      masm, receiver.Bit() | name.Bit() | value.Bit(), 0, x10);
}


void DebugCodegen::GenerateCompareNilICDebugBreak(MacroAssembler* masm) {
  // Register state for CompareNil IC
  // ----------- S t a t e -------------
  //  -- r0    : value
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x0.Bit(), 0, x10);
}


void DebugCodegen::GenerateReturnDebugBreak(MacroAssembler* masm) {
  // In places other than IC call sites it is expected that r0 is TOS which
  // is an object - this is not generally the case so this should be used with
  // care.
  Generate_DebugBreakCallHelper(masm, x0.Bit(), 0, x10);
}


void DebugCodegen::GenerateCallFunctionStubDebugBreak(MacroAssembler* masm) {
  // Register state for CallFunctionStub (from code-stubs-arm64.cc).
  // ----------- S t a t e -------------
  //  -- x1 : function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x1.Bit(), 0, x10);
}


void DebugCodegen::GenerateCallConstructStubDebugBreak(MacroAssembler* masm) {
  // Calling convention for CallConstructStub (from code-stubs-arm64.cc).
  // ----------- S t a t e -------------
  //  -- x0 : number of arguments (not smi)
  //  -- x1 : constructor function
  // -----------------------------------
  Generate_DebugBreakCallHelper(masm, x1.Bit(), x0.Bit(), x10);
}


void DebugCodegen::GenerateCallConstructStubRecordDebugBreak(
    MacroAssembler* masm) {
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


void DebugCodegen::GenerateSlot(MacroAssembler* masm) {
  // Generate enough nop's to make space for a call instruction. Avoid emitting
  // the constant pool in the debug break slot code.
  InstructionAccurateScope scope(masm, Assembler::kDebugBreakSlotInstructions);

  __ RecordDebugBreakSlot();
  for (int i = 0; i < Assembler::kDebugBreakSlotInstructions; i++) {
    __ nop(Assembler::DEBUG_BREAK_NOP);
  }
}


void DebugCodegen::GenerateSlotDebugBreak(MacroAssembler* masm) {
  // In the places where a debug break slot is inserted no registers can contain
  // object pointers.
  Generate_DebugBreakCallHelper(masm, 0, 0, x10);
}


void DebugCodegen::GeneratePlainReturnLiveEdit(MacroAssembler* masm) {
  __ Ret();
}


void DebugCodegen::GenerateFrameDropperLiveEdit(MacroAssembler* masm) {
  ExternalReference restarter_frame_function_slot =
      ExternalReference::debug_restarter_frame_function_pointer_address(
          masm->isolate());
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();

  __ Mov(scratch, restarter_frame_function_slot);
  __ Str(xzr, MemOperand(scratch));

  // We do not know our frame height, but set sp based on fp.
  __ Sub(masm->StackPointer(), fp, kPointerSize);
  __ AssertStackConsistency();

  __ Pop(x1, fp, lr);  // Function, Frame, Return address.

  // Load context from the function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // Get function code.
  __ Ldr(scratch, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(scratch, FieldMemOperand(scratch, SharedFunctionInfo::kCodeOffset));
  __ Add(scratch, scratch, Code::kHeaderSize - kHeapObjectTag);

  // Re-run JSFunction, x1 is function, cp is context.
  __ Br(scratch);
}


const bool LiveEdit::kFrameDropperSupported = true;

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
