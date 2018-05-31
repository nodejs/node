// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/api-arguments.h"
#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/double.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/heap/heap-inl.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/objects/regexp-match-info.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

#include "src/arm/code-stubs-arm.h"  // Cannot be the first include.

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ lsl(r5, r0, Operand(kPointerSizeLog2));
  __ str(r1, MemOperand(sp, r5));
  __ Push(r1);
  __ Push(r2);
  __ add(r0, r0, Operand(3));
  __ TailCallRuntime(Runtime::kNewArray);
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label negate, done;
  Register result_reg = destination();

  UseScratchRegisterScope temps(masm);
  Register double_low = GetRegisterThatIsNotOneOf(result_reg);
  Register double_high = GetRegisterThatIsNotOneOf(result_reg, double_low);
  LowDwVfpRegister double_scratch = temps.AcquireLowD();

  // Save the old values from these temporary registers on the stack.
  __ Push(double_high, double_low);

  // Account for saved regs.
  const int kArgumentOffset = 2 * kPointerSize;

  // Load double input.
  __ vldr(double_scratch, MemOperand(sp, kArgumentOffset));
  __ vmov(double_low, double_high, double_scratch);
  // Try to convert with a FPU convert instruction. This handles all
  // non-saturating cases.
  __ TryInlineTruncateDoubleToI(result_reg, double_scratch, &done);

  Register scratch = temps.Acquire();
  __ Ubfx(scratch, double_high, HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is an *ARM* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ sub(scratch, scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83). If the exponent is
  // greater than this, the conversion is out of range, so return zero.
  __ cmp(scratch, Operand(83));
  __ mov(result_reg, Operand::Zero(), LeaveCC, ge);
  __ b(ge, &done);

  // If we reach this code, 30 <= exponent <= 83.
  // `TryInlineTruncateDoubleToI` above will have truncated any double with an
  // exponent lower than 30.
  if (masm->emit_debug_code()) {
    // Scratch is exponent - 1.
    __ cmp(scratch, Operand(30 - 1));
    __ Check(ge, AbortReason::kUnexpectedValue);
  }

  // We don't have to handle cases where 0 <= exponent <= 20 for which we would
  // need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ rsb(scratch, scratch, Operand(51), SetCC);

  // 52 <= exponent <= 83, shift only double_low.
  // On entry, scratch contains: 52 - exponent.
  __ rsb(scratch, scratch, Operand::Zero(), LeaveCC, ls);
  __ mov(result_reg, Operand(double_low, LSL, scratch), LeaveCC, ls);
  __ b(ls, &negate);

  // 21 <= exponent <= 51, shift double_low and double_high
  // to generate the result.
  __ mov(double_low, Operand(double_low, LSR, scratch));
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ rsb(scratch, scratch, Operand(32));
  __ Ubfx(result_reg, double_high, 0, HeapNumber::kMantissaBitsInTopWord);
  // Set the implicit 1 before the mantissa part in double_high.
  __ orr(result_reg, result_reg,
         Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  __ orr(result_reg, double_low, Operand(result_reg, LSL, scratch));

  __ bind(&negate);
  // If input was positive, double_high ASR 31 equals 0 and
  // double_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xFFFFFFFF and double_high LSR 31 equals 1.
  // New result = (result eor 0xFFFFFFFF) + 1 = 0 - result.
  __ eor(result_reg, result_reg, Operand(double_high, ASR, 31));
  __ add(result_reg, result_reg, Operand(double_high, LSR, 31));

  __ bind(&done);

  // Restore registers corrupted in this routine and return.
  __ Pop(double_high, double_low);
  __ Ret();
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent == r2);
  const LowDwVfpRegister double_base = d0;
  const LowDwVfpRegister double_exponent = d1;
  const LowDwVfpRegister double_result = d2;
  const LowDwVfpRegister double_scratch = d3;
  const SwVfpRegister single_scratch = s6;
  const Register scratch = r9;
  const Register scratch2 = r4;

  Label call_runtime, done, int_exponent;

  // Detect integer exponents stored as double.
  __ TryDoubleToInt32Exact(scratch, double_exponent, double_scratch);
  __ b(eq, &int_exponent);

  __ push(lr);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(0, 2);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                     0, 2);
  }
  __ pop(lr);
  __ MovFromFloatResult(double_result);
  __ b(&done);

  // Calculate power with integer exponent.
  __ bind(&int_exponent);

  // Get two copies of exponent in the registers scratch and exponent.
  // Exponent has previously been stored into scratch as untagged integer.
  __ mov(exponent, scratch);

  __ vmov(double_scratch, double_base);  // Back up base.
  __ vmov(double_result, Double(1.0), scratch2);

  // Get absolute value of exponent.
  __ cmp(scratch, Operand::Zero());
  __ rsb(scratch, scratch, Operand::Zero(), LeaveCC, mi);

  Label while_true;
  __ bind(&while_true);
  __ mov(scratch, Operand(scratch, LSR, 1), SetCC);
  __ vmul(double_result, double_result, double_scratch, cs);
  __ vmul(double_scratch, double_scratch, double_scratch, ne);
  __ b(ne, &while_true);

  __ cmp(exponent, Operand::Zero());
  __ b(ge, &done);
  __ vmov(double_scratch, Double(1.0), scratch);
  __ vdiv(double_result, double_scratch, double_result);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ VFPCompareAndSetFlags(double_result, 0.0);
  __ b(ne, &done);
  // double_exponent may not containe the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ vmov(single_scratch, exponent);
  __ vcvt_f64_s32(double_exponent, single_scratch);

  // Returning or bailing out.
  __ push(lr);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(0, 2);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                     0, 2);
  }
  __ pop(lr);
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
  // Generate if not already in cache.
  SaveFPRegsMode mode = kSaveFPRegs;
  CEntryStub(isolate, 1, mode).GetCode();
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r0: number of arguments including receiver
  // r1: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_in_register():
  // r2: pointer to the first argument
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  __ mov(r5, Operand(r1));

  if (argv_in_register()) {
    // Move argv into the correct register.
    __ mov(r1, Operand(r2));
  } else {
    // Compute the argv pointer in a callee-saved register.
    __ add(r1, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ sub(r1, r1, Operand(kPointerSize));
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(save_doubles(), 0, is_builtin_exit()
                                           ? StackFrame::BUILTIN_EXIT
                                           : StackFrame::EXIT);

  // Store a copy of argc in callee-saved registers for later.
  __ mov(r4, Operand(r0));

  // r0, r4: number of arguments including receiver  (C callee-saved)
  // r1: pointer to the first argument (C callee-saved)
  // r5: pointer to builtin function  (C callee-saved)

#if V8_HOST_ARCH_ARM
  int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (FLAG_debug_code) {
    if (frame_alignment > kPointerSize) {
      Label alignment_as_expected;
      DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
      __ tst(sp, Operand(frame_alignment_mask));
      __ b(eq, &alignment_as_expected);
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      __ stop("Unexpected alignment");
      __ bind(&alignment_as_expected);
    }
  }
#endif

  // Call C built-in.
  // r0 = argc, r1 = argv, r2 = isolate
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  // Compute the return address in lr to return to after the jump below. Pc is
  // already at '+ 8' from the current instruction but return is after three
  // instructions so add another 4 to pc to get the return address.
  {
    // Prevent literal pool emission before return address.
    Assembler::BlockConstPoolScope block_const_pool(masm);
    __ add(lr, pc, Operand(4));
    __ str(lr, MemOperand(sp));
    __ Call(r5);
  }

  // Result returned in r0 or r1:r0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r0, Heap::kExceptionRootIndex);
  __ b(eq, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address(
        IsolateAddressId::kPendingExceptionAddress, isolate());
    __ mov(r3, Operand(pending_exception_address));
    __ ldr(r3, MemOperand(r3));
    __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ b(eq, &okay);
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r0:r1: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_in_register()
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // Callee-saved register r4 still holds argc.
                      : r4;
  __ LeaveExitFrame(save_doubles(), argc);
  __ mov(pc, lr);

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

  // Ask the runtime for help to determine the handler. This will set r0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0);
    __ mov(r0, Operand(0));
    __ mov(r1, Operand(0));
    __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ mov(cp, Operand(pending_handler_context_address));
  __ ldr(cp, MemOperand(cp));
  __ mov(sp, Operand(pending_handler_sp_address));
  __ ldr(sp, MemOperand(sp));
  __ mov(fp, Operand(pending_handler_fp_address));
  __ ldr(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  __ cmp(cp, Operand(0));
  __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset), ne);

  // Reset the masking register. This is done independent of the underlying
  // feature flag {FLAG_branch_load_poisoning} to make the snapshot work with
  // both configurations. It is safe to always do this, because the underlying
  // register is caller-saved and can be arbitrarily clobbered.
  __ ResetSpeculationPoisonRegister();

  // Compute the handler entry address and jump to it.
  ConstantPoolUnavailableScope constant_pool_unavailable(masm);
  __ mov(r1, Operand(pending_handler_entrypoint_address));
  __ ldr(r1, MemOperand(r1));
  __ Jump(r1);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // [sp+0]: argv

  Label invoke, handler_entry, exit;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Called from C, so do not pop argc and args on exit (preserve sp)
  // No need to save register-passed args
  // Save callee-saved registers (incl. cp and fp), sp, and lr
  __ stm(db_w, sp, kCalleeSaved | lr.bit());

  // Save callee-saved vfp registers.
  __ vstm(db_w, sp, kFirstCalleeSavedDoubleReg, kLastCalleeSavedDoubleReg);
  // Set up the reserved register for 0.0.
  __ vmov(kDoubleRegZero, Double(0.0));

  __ InitializeRootRegister();

  // Get address of argv, see stm above.
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc

  // Set up argv in r4.
  int offset_to_argv = (kNumCalleeSaved + 1) * kPointerSize;
  offset_to_argv += kNumDoubleCalleeSaved * kDoubleSize;
  __ ldr(r4, MemOperand(sp, offset_to_argv));

  // Push a frame with special values setup to mark it as an entry frame.
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  StackFrame::Type marker = type();
  __ mov(r7, Operand(StackFrame::TypeToMarker(marker)));
  __ mov(r6, Operand(StackFrame::TypeToMarker(marker)));
  __ mov(r5, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                       isolate())));
  __ ldr(r5, MemOperand(r5));
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();

    // Push a bad frame pointer to fail if it is used.
    __ mov(scratch, Operand(-1));
    __ stm(db_w, sp, r5.bit() | r6.bit() | r7.bit() | scratch.bit());
  }

  Register scratch = r6;

  // Set up frame pointer for the frame to be pushed.
  __ add(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ mov(r5, Operand(ExternalReference(js_entry_sp)));
  __ ldr(scratch, MemOperand(r5));
  __ cmp(scratch, Operand::Zero());
  __ b(ne, &non_outermost_js);
  __ str(fp, MemOperand(r5));
  __ mov(scratch, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ bind(&non_outermost_js);
  __ mov(scratch, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(scratch);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);

  // Block literal pool emission whilst taking the position of the handler
  // entry. This avoids making the assumption that literal pools are always
  // emitted after an instruction is emitted, rather than before.
  {
    Assembler::BlockConstPoolScope block_const_pool(masm);
    __ bind(&handler_entry);
    handler_offset_ = handler_entry.pos();
    // Caught exception: Store result (exception) in the pending exception
    // field in the JSEnv and return a failure sentinel.  Coming in here the
    // fp will be invalid because the PushStackHandler below sets it to 0 to
    // signal the existence of the JSEntry frame.
    __ mov(scratch,
           Operand(ExternalReference(IsolateAddressId::kPendingExceptionAddress,
                                     isolate())));
  }
  __ str(r0, MemOperand(scratch));
  __ LoadRoot(r0, Heap::kExceptionRootIndex);
  __ b(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r0-r4, r5-r6 are available.
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bl(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  __ Call(EntryTrampoline(), RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // r0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r5);
  __ cmp(r5, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ b(ne, &non_outermost_js_2);
  __ mov(r6, Operand::Zero());
  __ mov(r5, Operand(ExternalReference(js_entry_sp)));
  __ str(r6, MemOperand(r5));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r3);
  __ mov(scratch, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                            isolate())));
  __ str(r3, MemOperand(scratch));

  // Reset the stack to the callee saved registers.
  __ add(sp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // Restore callee-saved registers and return.
#ifdef DEBUG
  if (FLAG_debug_code) {
    __ mov(lr, Operand(pc));
  }
#endif

  // Restore callee-saved vfp registers.
  __ vldm(ia_w, sp, kFirstCalleeSavedDoubleReg, kLastCalleeSavedDoubleReg);

  __ ldm(ia_w, sp, kCalleeSaved | pc.bit());
}

void DirectCEntryStub::Generate(MacroAssembler* masm) {
  // Place the return address on the stack, making the call
  // GC safe. The RegExp backend also relies on this.
  __ str(lr, MemOperand(sp, 0));
  __ blx(ip);  // Call the C++ function.
  __ ldr(pc, MemOperand(sp, 0));
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    Register target) {
  intptr_t code =
      reinterpret_cast<intptr_t>(GetCode().location());
  __ Move(ip, target);
  __ mov(lr, Operand(code, RelocInfo::CODE_TARGET));
  __ blx(lr);  // Call the stub.
}


void ProfileEntryHookStub::MaybeCallEntryHookDelayed(TurboAssembler* tasm,
                                                     Zone* zone) {
  if (tasm->isolate()->function_entry_hook() != nullptr) {
    tasm->MaybeCheckConstPool();
    PredictableCodeSizeScope predictable(
        tasm, tasm->CallStubSize() + 2 * Assembler::kInstrSize);
    tasm->push(lr);
    tasm->CallStubDelayed(new (zone) ProfileEntryHookStub(nullptr));
    tasm->pop(lr);
  }
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != nullptr) {
    ProfileEntryHookStub stub(masm->isolate());
    masm->MaybeCheckConstPool();
    PredictableCodeSizeScope predictable(
        masm, masm->CallStubSize() + 2 * Assembler::kInstrSize);
    __ push(lr);
    __ CallStub(&stub);
    __ pop(lr);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // The entry hook is a "push lr" instruction, followed by a call.
  const int32_t kReturnAddressDistanceFromFunctionStart =
      3 * Assembler::kInstrSize;

  // This should contain all kCallerSaved registers.
  const RegList kSavedRegs =
      1 <<  0 |  // r0
      1 <<  1 |  // r1
      1 <<  2 |  // r2
      1 <<  3 |  // r3
      1 <<  5 |  // r5
      1 <<  9;   // r9
  // We also save lr, so the count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = 7;

  DCHECK_EQ(kCallerSaved & kSavedRegs, kCallerSaved);

  // Save all caller-save registers as this may be called from anywhere.
  __ stm(db_w, sp, kSavedRegs | lr.bit());

  // Compute the function's address for the first argument.
  __ sub(r0, lr, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ add(r1, sp, Operand(kNumSavedRegs * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ mov(r5, sp);
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    __ and_(sp, sp, Operand(-frame_alignment));
  }

  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();

#if V8_HOST_ARCH_ARM
    int32_t entry_hook =
        reinterpret_cast<int32_t>(isolate()->function_entry_hook());
    __ mov(scratch, Operand(entry_hook));
#else
    // Under the simulator we need to indirect the entry hook through a
    // trampoline function at a known address.
    // It additionally takes an isolate as a third parameter
    __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));

    ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
    __ mov(scratch,
           Operand(ExternalReference(
               &dispatcher, ExternalReference::BUILTIN_CALL, isolate())));
#endif
    __ Call(scratch);
  }

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ mov(sp, r5);
  }

  // Also pop pc to get Ret(0).
  __ ldm(ia_w, sp, kSavedRegs | pc.bit());
}


template<class T>
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
      __ cmp(r3, Operand(kind));
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
  // r2 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // r3 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // r0 - number of arguments
  // r1 - constructor?
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

    ArraySingleArgumentConstructorStub stub_holey(masm->isolate(),
                                                  holey_initial,
                                                  DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);
  } else if (mode == DONT_OVERRIDE) {
    // is the low bit set? If so, we are holey and that is good.
    Label normal_sequence;
    __ tst(r3, Operand(1));
    __ b(ne, &normal_sequence);

    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the slot).
    __ add(r3, r3, Operand(1));

    if (FLAG_debug_code) {
      __ ldr(r5, FieldMemOperand(r2, 0));
      __ CompareRoot(r5, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, AbortReason::kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ ldr(r4, FieldMemOperand(
                   r2, AllocationSite::kTransitionInfoOrBoilerplateOffset));
    __ add(r4, r4, Operand(Smi::FromInt(kFastElementsKindPackedToHoley)));
    __ str(r4, FieldMemOperand(
                   r2, AllocationSite::kTransitionInfoOrBoilerplateOffset));

    __ bind(&normal_sequence);
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(r3, Operand(kind));
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(AbortReason::kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


template<class T>
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
  ArrayConstructorStubAheadOfTimeHelper<ArraySingleArgumentConstructorStub>(
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
    MacroAssembler* masm,
    AllocationSiteOverrideMode mode) {
  Label not_zero_case, not_one_case;
  __ tst(r0, r0);
  __ b(ne, &not_zero_case);
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ bind(&not_zero_case);
  __ cmp(r0, Operand(1));
  __ b(gt, &not_one_case);
  CreateArrayDispatchOneArgument(masm, mode);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argc (only if argument_count() == ANY)
  //  -- r1 : constructor
  //  -- r2 : AllocationSite or undefined
  //  -- r3 : new target
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ tst(r4, Operand(kSmiTagMask));
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r4, r4, r5, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in r2 or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(r2, r4);
  }

  // Enter the context of the Array function.
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  Label subclassing;
  __ cmp(r3, r1);
  __ b(ne, &subclassing);

  Label no_info;
  // Get the elements kind and case on that.
  __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
  __ b(eq, &no_info);

  __ ldr(r3, FieldMemOperand(
                 r2, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  __ SmiUntag(r3);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ and_(r3, r3, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  __ bind(&subclassing);
  __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  __ add(r0, r0, Operand(3));
  __ Push(r3, r2);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  __ cmp(r0, Operand(1));

  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0, lo);

  ArrayNArgumentsConstructorStub stubN(isolate());
  __ TailCallStub(&stubN, hi);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ ldr(r3, MemOperand(sp, 0));
    __ cmp(r3, Operand::Zero());

    InternalArraySingleArgumentConstructorStub
        stub1_holey(isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey, ne);
  }

  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : argc
  //  -- r1 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ ldr(r3, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    __ tst(r3, Operand(kSmiTagMask));
    __ Assert(ne, AbortReason::kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r3, r3, r4, MAP_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following bit field extraction takes care of that anyway.
  __ ldr(r3, FieldMemOperand(r3, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(r3);

  if (FLAG_debug_code) {
    Label done;
    __ cmp(r3, Operand(PACKED_ELEMENTS));
    __ b(eq, &done);
    __ cmp(r3, Operand(HOLEY_ELEMENTS));
    __ Assert(
        eq,
        AbortReason::kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmp(r3, Operand(PACKED_ELEMENTS));
  __ b(eq, &fast_elements_case);
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

  DCHECK(function_address == r1 || function_address == r2);

  Label profiler_disabled;
  Label end_profiler_check;
  __ mov(r9, Operand(ExternalReference::is_profiling_address(isolate)));
  __ ldrb(r9, MemOperand(r9, 0));
  __ cmp(r9, Operand(0));
  __ b(eq, &profiler_disabled);

  // Additional parameter is the address of the actual callback.
  __ mov(r3, Operand(thunk_ref));
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  __ Move(r3, function_address);
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  __ mov(r9, Operand(next_address));
  __ ldr(r4, MemOperand(r9, kNextOffset));
  __ ldr(r5, MemOperand(r9, kLimitOffset));
  __ ldr(r6, MemOperand(r9, kLevelOffset));
  __ add(r6, r6, Operand(1));
  __ str(r6, MemOperand(r9, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1);
    __ mov(r0, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate);
  stub.GenerateCall(masm, r3);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1);
    __ mov(r0, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  __ ldr(r0, return_value_operand);
  __ bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ str(r4, MemOperand(r9, kNextOffset));
  if (__ emit_debug_code()) {
    __ ldr(r1, MemOperand(r9, kLevelOffset));
    __ cmp(r1, r6);
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ sub(r6, r6, Operand(1));
  __ str(r6, MemOperand(r9, kLevelOffset));
  __ ldr(r6, MemOperand(r9, kLimitOffset));
  __ cmp(r5, r6);
  __ b(ne, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand != nullptr) {
    __ ldr(r4, *stack_space_operand);
  } else {
    __ mov(r4, Operand(stack_space));
  }
  __ LeaveExitFrame(false, r4, stack_space_operand != nullptr);

  // Check if the function scheduled an exception.
  __ LoadRoot(r4, Heap::kTheHoleValueRootIndex);
  __ mov(r6, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ ldr(r5, MemOperand(r6));
  __ cmp(r4, r5);
  __ b(ne, &promote_scheduled_exception);

  __ mov(pc, lr);

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ str(r5, MemOperand(r9, kLimitOffset));
  __ mov(r4, r0);
  __ PrepareCallCFunction(1);
  __ mov(r0, Operand(ExternalReference::isolate_address(isolate)));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ mov(r0, r4);
  __ jmp(&leave_exit_frame);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4                  : call_data
  //  -- r2                  : holder
  //  -- r1                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 4]  : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register call_data = r4;
  Register holder = r2;
  Register api_function_address = r1;

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

  Register scratch0 = call_data;
  Register scratch1 = r5;
  __ LoadRoot(scratch0, Heap::kUndefinedValueRootIndex);
  // return value
  __ push(scratch0);
  // return value default
  __ push(scratch0);
  // isolate
  __ mov(scratch1,
         Operand(ExternalReference::isolate_address(masm->isolate())));
  __ push(scratch1);
  // holder
  __ push(holder);

  // Prepare arguments.
  __ mov(scratch0, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 3;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(api_function_address != r0 && scratch0 != r0);
  // r0 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ add(r0, sp, Operand(1 * kPointerSize));
  // FunctionCallbackInfo::implicit_args_
  __ str(scratch0, MemOperand(r0, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ add(scratch1, scratch0,
         Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ str(scratch1, MemOperand(r0, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ mov(scratch0, Operand(argc()));
  __ str(scratch0, MemOperand(r0, 2 * kPointerSize));

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
  Register scratch = r4;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r2;

  __ push(receiver);
  // Push data from AccessorInfo.
  __ ldr(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ push(scratch);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ Push(scratch, scratch);
  __ mov(scratch, Operand(ExternalReference::isolate_address(isolate())));
  __ Push(scratch, holder);
  __ Push(Smi::kZero);  // should_throw_on_error -> false
  __ ldr(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);
  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mov(r0, sp);                             // r0 = Handle<Name>
  __ add(r1, r0, Operand(1 * kPointerSize));  // r1 = v8::PCI::args_

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ str(r1, MemOperand(sp, 1 * kPointerSize));
  __ add(r1, sp, Operand(1 * kPointerSize));  // r1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  __ ldr(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ ldr(api_function_address,
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

#endif  // V8_TARGET_ARCH_ARM
