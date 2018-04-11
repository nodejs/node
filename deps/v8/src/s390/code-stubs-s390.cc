// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/api-arguments.h"
#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

#include "src/s390/code-stubs-s390.h"  // Cannot be the first include.

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ ShiftLeftP(r1, r2, Operand(kPointerSizeLog2));
  __ StoreP(r3, MemOperand(sp, r1));
  __ push(r3);
  __ push(r4);
  __ AddP(r2, r2, Operand(3));
  __ TailCallRuntime(Runtime::kNewArray);
}

void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done, fastpath_done;
  Register result_reg = destination();

  // Immediate values for this stub fit in instructions, so it's safe to use ip.
  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch_low = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(result_reg, scratch, scratch_low);
  DoubleRegister double_scratch = kScratchDoubleReg;

  __ push(scratch);
  // Account for saved regs.
  int argument_offset = 1 * kPointerSize;

  // Load double input.
  __ LoadDouble(double_scratch, MemOperand(sp, argument_offset));

  // Do fast-path convert from double to int.
  __ ConvertDoubleToInt64(result_reg, double_scratch);

  // Test for overflow
  __ TestIfInt32(result_reg);
  __ beq(&fastpath_done, Label::kNear);

  __ Push(scratch_high, scratch_low);
  // Account for saved regs.
  argument_offset += 2 * kPointerSize;

  __ LoadlW(scratch_high,
            MemOperand(sp, argument_offset + Register::kExponentOffset));
  __ LoadlW(scratch_low,
            MemOperand(sp, argument_offset + Register::kMantissaOffset));

  __ ExtractBitMask(scratch, scratch_high, HeapNumber::kExponentMask);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is a *S390* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ SubP(scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ CmpP(scratch, Operand(83));
  __ bge(&out_of_range, Label::kNear);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ Load(r0, Operand(51));
  __ SubP(scratch, r0, scratch);
  __ CmpP(scratch, Operand::Zero());
  __ ble(&only_low, Label::kNear);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ ShiftRight(scratch_low, scratch_low, scratch);
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ Load(r0, Operand(32));
  __ SubP(scratch, r0, scratch);
  __ ExtractBitMask(result_reg, scratch_high, HeapNumber::kMantissaMask);
  // Set the implicit 1 before the mantissa part in scratch_high.
  STATIC_ASSERT(HeapNumber::kMantissaBitsInTopWord >= 16);
  __ Load(r0, Operand(1 << ((HeapNumber::kMantissaBitsInTopWord)-16)));
  __ ShiftLeftP(r0, r0, Operand(16));
  __ OrP(result_reg, result_reg, r0);
  __ ShiftLeft(r0, result_reg, scratch);
  __ OrP(result_reg, scratch_low, r0);
  __ b(&negate, Label::kNear);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done, Label::kNear);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ LoadComplementRR(scratch, scratch);
  __ ShiftLeft(result_reg, scratch_low, scratch);

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xFFFFFFFF and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xFFFFFFFF) + 1 = 0 - result.
  __ ShiftRightArith(r0, scratch_high, Operand(31));
#if V8_TARGET_ARCH_S390X
  __ lgfr(r0, r0);
  __ ShiftRightP(r0, r0, Operand(32));
#endif
  __ XorP(result_reg, r0);
  __ ShiftRight(r0, scratch_high, Operand(31));
  __ AddP(result_reg, r0);

  __ bind(&done);
  __ Pop(scratch_high, scratch_low);

  __ bind(&fastpath_done);
  __ pop(scratch);

  __ Ret();
}

void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent == r4);
  const DoubleRegister double_base = d1;
  const DoubleRegister double_exponent = d2;
  const DoubleRegister double_result = d3;
  const DoubleRegister double_scratch = d0;
  const Register scratch = r1;
  const Register scratch2 = r9;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == TAGGED) {
    // Base is already in double_base.
    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ LoadDouble(double_exponent,
                  FieldMemOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type() != INTEGER) {
    // Detect integer exponents stored as double.
    __ TryDoubleToInt32Exact(scratch, double_exponent, scratch2,
                             double_scratch);
    __ beq(&int_exponent, Label::kNear);

    __ push(r14);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 0, 2);
    }
    __ pop(r14);
    __ MovFromFloatResult(double_result);
    __ b(&done);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);

  // Get two copies of exponent in the registers scratch and exponent.
  if (exponent_type() == INTEGER) {
    __ LoadRR(scratch, exponent);
  } else {
    // Exponent has previously been stored into scratch as untagged integer.
    __ LoadRR(exponent, scratch);
  }
  __ ldr(double_scratch, double_base);  // Back up base.
  __ LoadImmP(scratch2, Operand(1));
  __ ConvertIntToDouble(double_result, scratch2);

  // Get absolute value of exponent.
  Label positive_exponent;
  __ CmpP(scratch, Operand::Zero());
  __ bge(&positive_exponent, Label::kNear);
  __ LoadComplementRR(scratch, scratch);
  __ bind(&positive_exponent);

  Label while_true, no_carry, loop_end;
  __ bind(&while_true);
  __ mov(scratch2, Operand(1));
  __ AndP(scratch2, scratch);
  __ beq(&no_carry, Label::kNear);
  __ mdbr(double_result, double_scratch);
  __ bind(&no_carry);
  __ ShiftRightP(scratch, scratch, Operand(1));
  __ LoadAndTestP(scratch, scratch);
  __ beq(&loop_end, Label::kNear);
  __ mdbr(double_scratch, double_scratch);
  __ b(&while_true);
  __ bind(&loop_end);

  __ CmpP(exponent, Operand::Zero());
  __ bge(&done);

  // get 1/double_result:
  __ ldr(double_scratch, double_result);
  __ LoadImmP(scratch2, Operand(1));
  __ ConvertIntToDouble(double_result, scratch2);
  __ ddbr(double_result, double_scratch);

  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ lzdr(kDoubleRegZero);
  __ cdbr(double_result, kDoubleRegZero);
  __ bne(&done, Label::kNear);
  // double_exponent may not containe the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ ConvertIntToDouble(double_exponent, exponent);

  // Returning or bailing out.
  __ push(r14);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(0, 2, scratch);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(
        ExternalReference::power_double_double_function(isolate()), 0, 2);
  }
  __ pop(r14);
  __ MovFromFloatResult(double_result);

  __ bind(&done);
  __ Ret();
}

Movability CEntryStub::NeedsImmovableCode() { return kImmovable; }

void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  CommonArrayConstructorStub::GenerateStubsAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
}

void CodeStub::GenerateFPStubs(Isolate* isolate) {
  SaveFPRegsMode mode = kSaveFPRegs;
  CEntryStub(isolate, 1, mode).GetCode();
}

void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
  CEntryStub save_doubles(isolate, 1, kSaveFPRegs);
  save_doubles.GetCode();
}

void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r2: number of arguments including receiver
  // r3: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_in_register():
  // r4: pointer to the first argument
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  __ LoadRR(r7, r3);

  if (argv_in_register()) {
    // Move argv into the correct register.
    __ LoadRR(r3, r4);
  } else {
    // Compute the argv pointer.
    __ ShiftLeftP(r3, r2, Operand(kPointerSizeLog2));
    __ lay(r3, MemOperand(r3, sp, -kPointerSize));
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

  // Pass buffer for return value on stack if necessary
  bool needs_return_buffer =
      result_size() == 2 && !ABI_RETURNS_OBJECTPAIR_IN_REGS;
  if (needs_return_buffer) {
    arg_stack_space += result_size();
  }

#if V8_TARGET_ARCH_S390X
  // 64-bit linux pass Argument object by reference not value
  arg_stack_space += 2;
#endif

  __ EnterExitFrame(save_doubles(), arg_stack_space, is_builtin_exit()
                                           ? StackFrame::BUILTIN_EXIT
                                           : StackFrame::EXIT);

  // Store a copy of argc, argv in callee-saved registers for later.
  __ LoadRR(r6, r2);
  __ LoadRR(r8, r3);
  // r2, r6: number of arguments including receiver  (C callee-saved)
  // r3, r8: pointer to the first argument
  // r7: pointer to builtin function  (C callee-saved)

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r4;
  if (needs_return_buffer) {
    // The return value is 16-byte non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument in R2.  Shfit original parameters
    // by one register each.
    __ LoadRR(r4, r3);
    __ LoadRR(r3, r2);
    __ la(r2, MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kPointerSize));
    isolate_reg = r5;
  }
  // Call C built-in.
  __ mov(isolate_reg, Operand(ExternalReference::isolate_address(isolate())));

  Register target = r7;

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  {
    Label return_label;
    __ larl(r14, &return_label);  // Generate the return addr of call later.
    __ StoreP(r14, MemOperand(sp, kStackFrameRASlot * kPointerSize));

    // zLinux ABI requires caller's frame to have sufficient space for callee
    // preserved regsiter save area.
    // __ lay(sp, MemOperand(sp, -kCalleeRegisterSaveAreaSize));
    __ b(target);
    __ bind(&return_label);
    // __ la(sp, MemOperand(sp, +kCalleeRegisterSaveAreaSize));
  }

  // If return value is on the stack, pop it to registers.
  if (needs_return_buffer) {
    __ LoadP(r3, MemOperand(r2, kPointerSize));
    __ LoadP(r2, MemOperand(r2));
  }

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r2, Heap::kExceptionRootIndex);
  __ beq(&exception_returned, Label::kNear);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address(
        IsolateAddressId::kPendingExceptionAddress, isolate());
    __ mov(r1, Operand(pending_exception_address));
    __ LoadP(r1, MemOperand(r1));
    __ CompareRoot(r1, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay, Label::kNear);
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r2:r3: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_in_register()
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // r6: still holds argc (callee-saved).
                      : r6;
  __ LeaveExitFrame(save_doubles(), argc);
  __ b(r14);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address(
      IsolateAddressId::kPendingHandlerContextAddress, isolate());
  ExternalReference pending_handler_entrypoint_address(
      IsolateAddressId::kPendingHandlerEntrypointAddress, isolate());
  ExternalReference pending_handler_fp_address(
      IsolateAddressId::kPendingHandlerFPAddress, isolate());
  ExternalReference pending_handler_sp_address(
      IsolateAddressId::kPendingHandlerSPAddress, isolate());

  // Ask the runtime for help to determine the handler. This will set r3 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, r2);
    __ LoadImmP(r2, Operand::Zero());
    __ LoadImmP(r3, Operand::Zero());
    __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ mov(cp, Operand(pending_handler_context_address));
  __ LoadP(cp, MemOperand(cp));
  __ mov(sp, Operand(pending_handler_sp_address));
  __ LoadP(sp, MemOperand(sp));
  __ mov(fp, Operand(pending_handler_fp_address));
  __ LoadP(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label skip;
  __ CmpP(cp, Operand::Zero());
  __ beq(&skip, Label::kNear);
  __ StoreP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  // Reset the masking register.
  if (FLAG_branch_load_poisoning) {
    __ ResetSpeculationPoisonRegister();
  }

  // Compute the handler entry address and jump to it.
  __ mov(r3, Operand(pending_handler_entrypoint_address));
  __ LoadP(r3, MemOperand(r3));
  __ Jump(r3);
}

void JSEntryStub::Generate(MacroAssembler* masm) {
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv

  Label invoke, handler_entry, exit;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

// saving floating point registers
#if V8_TARGET_ARCH_S390X
  // 64bit ABI requires f8 to f15 be saved
  __ lay(sp, MemOperand(sp, -8 * kDoubleSize));
  __ std(d8, MemOperand(sp));
  __ std(d9, MemOperand(sp, 1 * kDoubleSize));
  __ std(d10, MemOperand(sp, 2 * kDoubleSize));
  __ std(d11, MemOperand(sp, 3 * kDoubleSize));
  __ std(d12, MemOperand(sp, 4 * kDoubleSize));
  __ std(d13, MemOperand(sp, 5 * kDoubleSize));
  __ std(d14, MemOperand(sp, 6 * kDoubleSize));
  __ std(d15, MemOperand(sp, 7 * kDoubleSize));
#else
  // 31bit ABI requires you to store f4 and f6:
  // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
  __ lay(sp, MemOperand(sp, -2 * kDoubleSize));
  __ std(d4, MemOperand(sp));
  __ std(d6, MemOperand(sp, kDoubleSize));
#endif

  // zLinux ABI
  //    Incoming parameters:
  //          r2: code entry
  //          r3: function
  //          r4: receiver
  //          r5: argc
  //          r6: argv
  //    Requires us to save the callee-preserved registers r6-r13
  //    General convention is to also save r14 (return addr) and
  //    sp/r15 as well in a single STM/STMG
  __ lay(sp, MemOperand(sp, -10 * kPointerSize));
  __ StoreMultipleP(r6, sp, MemOperand(sp, 0));

  // Set up the reserved register for 0.0.
  // __ LoadDoubleLiteral(kDoubleRegZero, 0.0, r0);

  // Push a frame with special values setup to mark it as an entry frame.
  //   Bad FP (-1)
  //   SMI Marker
  //   SMI Marker
  //   kCEntryFPAddress
  //   Frame type
  __ lay(sp, MemOperand(sp, -5 * kPointerSize));

  // Push a bad frame pointer to fail if it is used.
  __ LoadImmP(r10, Operand(-1));

  StackFrame::Type marker = type();
  __ Load(r9, Operand(StackFrame::TypeToMarker(marker)));
  __ Load(r8, Operand(StackFrame::TypeToMarker(marker)));
  // Save copies of the top frame descriptor on the stack.
  __ mov(r7, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                       isolate())));
  __ LoadP(r7, MemOperand(r7));
  __ StoreMultipleP(r7, r10, MemOperand(sp, kPointerSize));
  // Set up frame pointer for the frame to be pushed.
  // Need to add kPointerSize, because sp has one extra
  // frame already for the frame type being pushed later.
  __ lay(fp,
         MemOperand(sp, -EntryFrameConstants::kCallerFPOffset + kPointerSize));

  __ InitializeRootRegister();

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ mov(r7, Operand(ExternalReference(js_entry_sp)));
  __ LoadAndTestP(r8, MemOperand(r7));
  __ bne(&non_outermost_js, Label::kNear);
  __ StoreP(fp, MemOperand(r7));
  __ Load(ip, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont, Label::kNear);
  __ bind(&non_outermost_js);
  __ Load(ip, Operand(StackFrame::INNER_JSENTRY_FRAME));

  __ bind(&cont);
  __ StoreP(ip, MemOperand(sp));  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke, Label::kNear);

  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference(
                 IsolateAddressId::kPendingExceptionAddress, isolate())));

  __ StoreP(r2, MemOperand(ip));
  __ LoadRoot(r2, Heap::kExceptionRootIndex);
  __ b(&exit, Label::kNear);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r2-r6.
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the b(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv
  __ Call(EntryTrampoline(), RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();
  __ bind(&exit);  // r2 holds result

  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r7);
  __ CmpP(r7, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ bne(&non_outermost_js_2, Label::kNear);
  __ mov(r8, Operand::Zero());
  __ mov(r7, Operand(ExternalReference(js_entry_sp)));
  __ StoreP(r8, MemOperand(r7));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r5);
  __ mov(ip, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                       isolate())));
  __ StoreP(r5, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ lay(sp, MemOperand(sp, -EntryFrameConstants::kCallerFPOffset));

  // Reload callee-saved preserved regs, return address reg (r14) and sp
  __ LoadMultipleP(r6, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 10 * kPointerSize));

// saving floating point registers
#if V8_TARGET_ARCH_S390X
  // 64bit ABI requires f8 to f15 be saved
  __ ld(d8, MemOperand(sp));
  __ ld(d9, MemOperand(sp, 1 * kDoubleSize));
  __ ld(d10, MemOperand(sp, 2 * kDoubleSize));
  __ ld(d11, MemOperand(sp, 3 * kDoubleSize));
  __ ld(d12, MemOperand(sp, 4 * kDoubleSize));
  __ ld(d13, MemOperand(sp, 5 * kDoubleSize));
  __ ld(d14, MemOperand(sp, 6 * kDoubleSize));
  __ ld(d15, MemOperand(sp, 7 * kDoubleSize));
  __ la(sp, MemOperand(sp, 8 * kDoubleSize));
#else
  // 31bit ABI requires you to store f4 and f6:
  // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
  __ ld(d4, MemOperand(sp));
  __ ld(d6, MemOperand(sp, kDoubleSize));
  __ la(sp, MemOperand(sp, 2 * kDoubleSize));
#endif

  __ b(r14);
}

// This stub is paired with DirectCEntryStub::GenerateCall
void DirectCEntryStub::Generate(MacroAssembler* masm) {
  __ CleanseP(r14);

  __ b(ip);  // Callee will return to R14 directly
}

void DirectCEntryStub::GenerateCall(MacroAssembler* masm, Register target) {
#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // Native AIX/S390X Linux use a function descriptor.
  __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(target, kPointerSize));
  __ LoadP(target, MemOperand(target, 0));  // Instruction address
#else
  // ip needs to be set for DirectCEentryStub::Generate, and also
  // for ABI_CALL_VIA_IP.
  __ Move(ip, target);
#endif

  __ call(GetCode(), RelocInfo::CODE_TARGET);  // Call the stub.
}

void ProfileEntryHookStub::MaybeCallEntryHookDelayed(TurboAssembler* tasm,
                                                     Zone* zone) {
  if (tasm->isolate()->function_entry_hook() != nullptr) {
    PredictableCodeSizeScope predictable(tasm,
#if V8_TARGET_ARCH_S390X
                                         40);
#elif V8_HOST_ARCH_S390
                                         36);
#else
                                         32);
#endif
    tasm->CleanseP(r14);
    tasm->Push(r14, ip);
    tasm->CallStubDelayed(new (zone) ProfileEntryHookStub(nullptr));
    tasm->Pop(r14, ip);
  }
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != nullptr) {
    PredictableCodeSizeScope predictable(masm,
#if V8_TARGET_ARCH_S390X
                                         40);
#elif V8_HOST_ARCH_S390
                                         36);
#else
                                         32);
#endif
    ProfileEntryHookStub stub(masm->isolate());
    __ CleanseP(r14);
    __ Push(r14, ip);
    __ CallStub(&stub);  // BRASL
    __ Pop(r14, ip);
  }
}

void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
// The entry hook is a "push lr" instruction (LAY+ST/STG), followed by a call.
#if V8_TARGET_ARCH_S390X
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 18;  // LAY + STG * 2
#elif V8_HOST_ARCH_S390
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 18;  // NILH + LAY + ST * 2
#else
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 14;  // LAY + ST * 2
#endif

  // This should contain all kJSCallerSaved registers.
  const RegList kSavedRegs = kJSCallerSaved |  // Caller saved registers.
                             r7.bit();         // Saved stack pointer.

  // We also save r14+ip, so count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = kNumJSCallerSaved + 3;

  // Save all caller-save registers as this may be called from anywhere.
  __ CleanseP(r14);
  __ LoadRR(ip, r14);
  __ MultiPush(kSavedRegs | ip.bit());

  // Compute the function's address for the first argument.

  __ SubP(r2, ip, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is two slots above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ lay(r3, MemOperand(sp, kNumSavedRegs * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ LoadRR(r7, sp);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    __ ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
  }

#if !defined(USE_SIMULATOR)
  uintptr_t entry_hook =
      reinterpret_cast<uintptr_t>(isolate()->function_entry_hook());
  __ mov(ip, Operand(entry_hook));

#if ABI_USES_FUNCTION_DESCRIPTORS
  // Function descriptor
  __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(ip, kPointerSize));
  __ LoadP(ip, MemOperand(ip, 0));
// ip already set.
#endif
#endif

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  __ LoadImmP(r0, Operand::Zero());
  __ lay(sp, MemOperand(sp, -kCalleeRegisterSaveAreaSize -
                                kNumRequiredStackFrameSlots * kPointerSize));
  __ StoreP(r0, MemOperand(sp));
#if defined(USE_SIMULATOR)
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  // It additionally takes an isolate as a third parameter
  __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));

  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ mov(ip, Operand(ExternalReference(
                 &dispatcher, ExternalReference::BUILTIN_CALL, isolate())));
#endif
  __ Call(ip);

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  __ la(sp, MemOperand(sp, kCalleeRegisterSaveAreaSize +
                               kNumRequiredStackFrameSlots * kPointerSize));

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ LoadRR(sp, r7);
  }

  // Also pop lr to get Ret(0).
  __ MultiPop(kSavedRegs | ip.bit());
  __ LoadRR(r14, ip);
  __ Ret();
}

template <class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(masm->isolate(), GetInitialFastElementsKind(), mode);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ CmpP(r5, Operand(kind));
      T stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(AbortReason::kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}

static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // r4 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // r5 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // r2 - number of arguments
  // r3 - constructor?
  // sp[0] - last argument
  STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(PACKED_ELEMENTS == 2);
  STATIC_ASSERT(HOLEY_ELEMENTS == 3);
  STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS == 4);
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == 5);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(
        masm->isolate(), holey_initial, DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);
  } else if (mode == DONT_OVERRIDE) {
    Label normal_sequence;
    // is the low bit set? If so, we are holey and that is good.
    __ AndP(r0, r5, Operand(1));
    __ bne(&normal_sequence);

    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the slot).
    __ AddP(r5, r5, Operand(1));
    if (FLAG_debug_code) {
      __ LoadP(r7, FieldMemOperand(r4, 0));
      __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, AbortReason::kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r5
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ LoadP(r6, FieldMemOperand(
                     r4, AllocationSite::kTransitionInfoOrBoilerplateOffset));
    __ AddSmiLiteral(r6, r6, Smi::FromInt(kFastElementsKindPackedToHoley), r0);
    __ StoreP(r6, FieldMemOperand(
                      r4, AllocationSite::kTransitionInfoOrBoilerplateOffset));

    __ bind(&normal_sequence);
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ CmpP(r5, Operand(kind));
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(AbortReason::kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}

template <class T>
static void ArrayConstructorStubAheadOfTimeHelper(Isolate* isolate) {
  int to_index =
      GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(isolate, kind);
    stub.GetCode();
    if (AllocationSite::ShouldTrack(kind)) {
      T stub1(isolate, kind, DISABLE_ALLOCATION_SITES);
      stub1.GetCode();
    }
  }
}

void CommonArrayConstructorStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayNArgumentsConstructorStub stub(isolate);
  stub.GetCode();
  ElementsKind kinds[2] = {PACKED_ELEMENTS, HOLEY_ELEMENTS};
  for (int i = 0; i < 2; i++) {
    // For internal arrays we only need a few things
    InternalArrayNoArgumentConstructorStub stubh1(isolate, kinds[i]);
    stubh1.GetCode();
    InternalArraySingleArgumentConstructorStub stubh2(isolate, kinds[i]);
    stubh2.GetCode();
  }
}

void ArrayConstructorStub::GenerateDispatchToArrayStub(
    MacroAssembler* masm, AllocationSiteOverrideMode mode) {
  Label not_zero_case, not_one_case;
  __ CmpP(r2, Operand::Zero());
  __ bne(&not_zero_case);
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ bind(&not_zero_case);
  __ CmpP(r2, Operand(1));
  __ bgt(&not_one_case);
  CreateArrayDispatchOneArgument(masm, mode);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argc (only if argument_count() == ANY)
  //  -- r3 : constructor
  //  -- r4 : AllocationSite or undefined
  //  -- r5 : new target
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ TestIfSmi(r6);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r6, r6, r7, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in r4 or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(r4, r6);
  }

  // Enter the context of the Array function.
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  Label subclassing;
  __ CmpP(r5, r3);
  __ bne(&subclassing, Label::kNear);

  Label no_info;
  // Get the elements kind and case on that.
  __ CompareRoot(r4, Heap::kUndefinedValueRootIndex);
  __ beq(&no_info);

  __ LoadP(r5, FieldMemOperand(
                   r4, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  __ SmiUntag(r5);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ AndP(r5, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  __ bind(&subclassing);
  __ ShiftLeftP(r1, r2, Operand(kPointerSizeLog2));
  __ StoreP(r3, MemOperand(sp, r1));
  __ AddP(r2, r2, Operand(3));
  __ Push(r5, r4);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}

void InternalArrayConstructorStub::GenerateCase(MacroAssembler* masm,
                                                ElementsKind kind) {
  __ CmpLogicalP(r2, Operand(1));

  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0, lt);

  ArrayNArgumentsConstructorStub stubN(isolate());
  __ TailCallStub(&stubN, gt);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ LoadP(r5, MemOperand(sp, 0));
    __ CmpP(r5, Operand::Zero());

    InternalArraySingleArgumentConstructorStub stub1_holey(
        isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey, ne);
  }

  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);
}

void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argc
  //  -- r3 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ TestIfSmi(r5);
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r5, r6, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the map's "bit field 2" into |result|.
  __ LoadlB(r5, FieldMemOperand(r5, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(r5);

  if (FLAG_debug_code) {
    Label done;
    __ CmpP(r5, Operand(PACKED_ELEMENTS));
    __ beq(&done);
    __ CmpP(r5, Operand(HOLEY_ELEMENTS));
    __ Assert(
        eq,
        AbortReason::kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ CmpP(r5, Operand(PACKED_ELEMENTS));
  __ beq(&fast_elements_case);
  GenerateCase(masm, HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, PACKED_ELEMENTS);
}

static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  stack_space
// - space to be unwound on exit (includes the call JS arguments space and
// the additional space allocated for the fast call).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     int stack_space,
                                     MemOperand* stack_space_operand,
                                     MemOperand return_value_operand) {
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  // Additional parameter is the address of the actual callback.
  DCHECK(function_address == r3 || function_address == r4);
  Register scratch = r5;

  __ mov(scratch, Operand(ExternalReference::is_profiling_address(isolate)));
  __ LoadlB(scratch, MemOperand(scratch, 0));
  __ CmpP(scratch, Operand::Zero());

  Label profiler_disabled;
  Label end_profiler_check;
  __ beq(&profiler_disabled, Label::kNear);
  __ mov(scratch, Operand(thunk_ref));
  __ b(&end_profiler_check, Label::kNear);
  __ bind(&profiler_disabled);
  __ LoadRR(scratch, function_address);
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  // r9 - next_address
  // r6 - next_address->kNextOffset
  // r7 - next_address->kLimitOffset
  // r8 - next_address->kLevelOffset
  __ mov(r9, Operand(next_address));
  __ LoadP(r6, MemOperand(r9, kNextOffset));
  __ LoadP(r7, MemOperand(r9, kLimitOffset));
  __ LoadlW(r8, MemOperand(r9, kLevelOffset));
  __ AddP(r8, Operand(1));
  __ StoreW(r8, MemOperand(r9, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r2);
    __ mov(r2, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate);
  stub.GenerateCall(masm, scratch);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r2);
    __ mov(r2, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  __ LoadP(r2, return_value_operand);
  __ bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ StoreP(r6, MemOperand(r9, kNextOffset));
  if (__ emit_debug_code()) {
    __ LoadlW(r3, MemOperand(r9, kLevelOffset));
    __ CmpP(r3, r8);
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ SubP(r8, Operand(1));
  __ StoreW(r8, MemOperand(r9, kLevelOffset));
  __ CmpP(r7, MemOperand(r9, kLimitOffset));
  __ bne(&delete_allocated_handles, Label::kNear);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand != nullptr) {
    __ l(r6, *stack_space_operand);
  } else {
    __ mov(r6, Operand(stack_space));
  }
  __ LeaveExitFrame(false, r6, stack_space_operand != nullptr);

  // Check if the function scheduled an exception.
  __ mov(r7, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ LoadP(r7, MemOperand(r7));
  __ CompareRoot(r7, Heap::kTheHoleValueRootIndex);
  __ bne(&promote_scheduled_exception, Label::kNear);

  __ b(r14);

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ StoreP(r7, MemOperand(r9, kLimitOffset));
  __ LoadRR(r6, r2);
  __ PrepareCallCFunction(1, r7);
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate)));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ LoadRR(r2, r6);
  __ b(&leave_exit_frame, Label::kNear);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r6                  : call_data
  //  -- r4                  : holder
  //  -- r3                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 4]  : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register call_data = r6;
  Register holder = r4;
  Register api_function_address = r3;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kArgsLength == 6);
  STATIC_ASSERT(FCA::kNewTargetIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  // new target
  __ PushRoot(Heap::kUndefinedValueRootIndex);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  // return value
  __ push(scratch);
  // return value default
  __ push(scratch);
  // isolate
  __ mov(scratch, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ push(scratch);
  // holder
  __ push(holder);

  // Prepare arguments.
  __ LoadRR(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // S390 LINUX ABI:
  //
  // Create 4 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-3] FunctionCallbackInfo
  const int kApiStackSpace = 4;
  const int kFunctionCallbackInfoOffset =
      (kStackFrameExtraParamSlot + 1) * kPointerSize;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(api_function_address != r2 && scratch != r2);
  // r2 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ AddP(r2, sp, Operand(kFunctionCallbackInfoOffset));
  // FunctionCallbackInfo::implicit_args_
  __ StoreP(scratch, MemOperand(r2, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ AddP(ip, scratch, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ StoreP(ip, MemOperand(r2, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ LoadImmP(ip, Operand(argc()));
  __ StoreW(ip, MemOperand(r2, 2 * kPointerSize));

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  // Stores return the first js argument
  int return_value_offset = 2 + FCA::kReturnValueOffset;
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);
  const int stack_space = argc() + FCA::kArgsLength + 1;
  MemOperand* stack_space_operand = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_operand, return_value_operand);
}

void CallApiGetterStub::Generate(MacroAssembler* masm) {
  int arg0Slot = 0;
  int accessorInfoSlot = 0;
  int apiStackSpace = 0;
  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  STATIC_ASSERT(PropertyCallbackArguments::kShouldThrowOnErrorIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 6);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 7);

  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = r6;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r4;

  __ push(receiver);
  // Push data from AccessorInfo.
  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ push(scratch);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ Push(scratch, scratch);
  __ mov(scratch, Operand(ExternalReference::isolate_address(isolate())));
  __ Push(scratch, holder);
  __ Push(Smi::kZero);  // should_throw_on_error -> false
  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ LoadRR(r2, sp);                           // r2 = Handle<Name>
  __ AddP(r3, r2, Operand(1 * kPointerSize));  // r3 = v8::PCI::args_

  // If ABI passes Handles (pointer-sized struct) in a register:
  //
  // Create 2 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] AccessorInfo&
  //
  // Otherwise:
  //
  // Create 3 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] copy of Handle (first arg)
  //    [2] AccessorInfo&
  if (ABI_PASSES_HANDLES_IN_REGS) {
    accessorInfoSlot = kStackFrameExtraParamSlot + 1;
    apiStackSpace = 2;
  } else {
    arg0Slot = kStackFrameExtraParamSlot + 1;
    accessorInfoSlot = arg0Slot + 1;
    apiStackSpace = 3;
  }

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, apiStackSpace);

  if (!ABI_PASSES_HANDLES_IN_REGS) {
    // pass 1st arg by reference
    __ StoreP(r2, MemOperand(sp, arg0Slot * kPointerSize));
    __ AddP(r2, sp, Operand(arg0Slot * kPointerSize));
  }

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ StoreP(r3, MemOperand(sp, accessorInfoSlot * kPointerSize));
  __ AddP(r3, sp, Operand(accessorInfoSlot * kPointerSize));
  // r3 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ LoadP(api_function_address,
           FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, nullptr, return_value_operand);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
