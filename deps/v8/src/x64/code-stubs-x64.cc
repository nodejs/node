// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/api-arguments.h"
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
#include "src/objects-inl.h"
#include "src/objects/regexp-match-info.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ popq(rcx);
  __ movq(MemOperand(rsp, rax, times_8, 0), rdi);
  __ pushq(rdi);
  __ pushq(rbx);
  __ pushq(rcx);
  __ addq(rax, Immediate(3));
  __ TailCallRuntime(Runtime::kNewArray);
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
    Register final_result_reg = this->destination();

    Label check_negative, process_64_bits, done;

    // Account for return address and saved regs.
    const int kArgumentOffset = 3 * kRegisterSize;

    MemOperand mantissa_operand(MemOperand(rsp, kArgumentOffset));
    MemOperand exponent_operand(
        MemOperand(rsp, kArgumentOffset + kDoubleSize / 2));

    Register scratch1 = no_reg;
    Register scratch_candidates[3] = { rbx, rdx, rdi };
    for (int i = 0; i < 3; i++) {
      scratch1 = scratch_candidates[i];
      if (final_result_reg != scratch1) break;
    }

    // Since we must use rcx for shifts below, use some other register (rax)
    // to calculate the result if ecx is the requested return register.
    Register result_reg = final_result_reg == rcx ? rax : final_result_reg;
    // Save ecx if it isn't the return register and therefore volatile, or if it
    // is the return register, then save the temp register we use in its stead
    // for the result.
    Register save_reg = final_result_reg == rcx ? rax : rcx;
    __ pushq(scratch1);
    __ pushq(save_reg);

    __ movl(scratch1, mantissa_operand);
    __ Movsd(kScratchDoubleReg, mantissa_operand);
    __ movl(rcx, exponent_operand);

    __ andl(rcx, Immediate(HeapNumber::kExponentMask));
    __ shrl(rcx, Immediate(HeapNumber::kExponentShift));
    __ leal(result_reg, MemOperand(rcx, -HeapNumber::kExponentBias));
    __ cmpl(result_reg, Immediate(HeapNumber::kMantissaBits));
    __ j(below, &process_64_bits);

    // Result is entirely in lower 32-bits of mantissa
    int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
    __ subl(rcx, Immediate(delta));
    __ xorl(result_reg, result_reg);
    __ cmpl(rcx, Immediate(31));
    __ j(above, &done);
    __ shll_cl(scratch1);
    __ jmp(&check_negative);

    __ bind(&process_64_bits);
    __ Cvttsd2siq(result_reg, kScratchDoubleReg);
    __ jmp(&done, Label::kNear);

    // If the double was negative, negate the integer result.
    __ bind(&check_negative);
    __ movl(result_reg, scratch1);
    __ negl(result_reg);
    __ cmpl(exponent_operand, Immediate(0));
    __ cmovl(greater, result_reg, scratch1);

    // Restore registers
    __ bind(&done);
    if (final_result_reg != result_reg) {
      DCHECK(final_result_reg == rcx);
      __ movl(final_result_reg, result_reg);
    }
    __ popq(save_reg);
    __ popq(scratch1);
    __ ret(0);
}

void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent == rdx);
  const Register scratch = rcx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ movp(scratch, Immediate(1));
  __ Cvtlsi2sd(double_result, scratch);

  if (exponent_type() == TAGGED) {
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiToInteger32(exponent, exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ Movsd(double_exponent, FieldOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type() != INTEGER) {
    Label fast_power, try_arithmetic_simplification;
    // Detect integer exponents stored as double.
    __ DoubleToI(exponent, double_exponent, double_scratch,
                 TREAT_MINUS_ZERO_AS_ZERO, &try_arithmetic_simplification,
                 &try_arithmetic_simplification,
                 &try_arithmetic_simplification);
    __ jmp(&int_exponent);

    __ bind(&try_arithmetic_simplification);
    __ Cvttsd2si(exponent, double_exponent);
    // Skip to runtime if possibly NaN (indicated by the indefinite integer).
    __ cmpl(exponent, Immediate(0x1));
    __ j(overflow, &call_runtime);

    // Using FPU instructions to calculate power.
    Label fast_power_failed;
    __ bind(&fast_power);
    __ fnclex();  // Clear flags to catch exceptions later.
    // Transfer (B)ase and (E)xponent onto the FPU register stack.
    __ subp(rsp, Immediate(kDoubleSize));
    __ Movsd(Operand(rsp, 0), double_exponent);
    __ fld_d(Operand(rsp, 0));  // E
    __ Movsd(Operand(rsp, 0), double_base);
    __ fld_d(Operand(rsp, 0));  // B, E

    // Exponent is in st(1) and base is in st(0)
    // B ^ E = (2^(E * log2(B)) - 1) + 1 = (2^X - 1) + 1 for X = E * log2(B)
    // FYL2X calculates st(1) * log2(st(0))
    __ fyl2x();    // X
    __ fld(0);     // X, X
    __ frndint();  // rnd(X), X
    __ fsub(1);    // rnd(X), X-rnd(X)
    __ fxch(1);    // X - rnd(X), rnd(X)
    // F2XM1 calculates 2^st(0) - 1 for -1 < st(0) < 1
    __ f2xm1();    // 2^(X-rnd(X)) - 1, rnd(X)
    __ fld1();     // 1, 2^(X-rnd(X)) - 1, rnd(X)
    __ faddp(1);   // 2^(X-rnd(X)), rnd(X)
    // FSCALE calculates st(0) * 2^st(1)
    __ fscale();   // 2^X, rnd(X)
    __ fstp(1);
    // Bail out to runtime in case of exceptions in the status word.
    __ fnstsw_ax();
    __ testb(rax, Immediate(0x5F));  // Check for all but precision exception.
    __ j(not_zero, &fast_power_failed, Label::kNear);
    __ fstp_d(Operand(rsp, 0));
    __ Movsd(double_result, Operand(rsp, 0));
    __ addp(rsp, Immediate(kDoubleSize));
    __ jmp(&done);

    __ bind(&fast_power_failed);
    __ fninit();
    __ addp(rsp, Immediate(kDoubleSize));
    __ jmp(&call_runtime);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  // Back up exponent as we need to check if exponent is negative later.
  __ movp(scratch, exponent);  // Back up exponent.
  __ Movsd(double_scratch, double_base);     // Back up base.
  __ Movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, while_false;
  __ testl(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ negl(scratch);
  __ bind(&no_neg);

  __ j(zero, &while_false, Label::kNear);
  __ shrl(scratch, Immediate(1));
  // Above condition means CF==0 && ZF==0.  This means that the
  // bit that has been shifted out is 0 and the result is not 0.
  __ j(above, &while_true, Label::kNear);
  __ Movsd(double_result, double_scratch);
  __ j(zero, &while_false, Label::kNear);

  __ bind(&while_true);
  __ shrl(scratch, Immediate(1));
  __ Mulsd(double_scratch, double_scratch);
  __ j(above, &while_true, Label::kNear);
  __ Mulsd(double_result, double_scratch);
  __ j(not_zero, &while_true);

  __ bind(&while_false);
  // If the exponent is negative, return 1/result.
  __ testl(exponent, exponent);
  __ j(greater, &done);
  __ Divsd(double_scratch2, double_result);
  __ Movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ Xorpd(double_scratch2, double_scratch2);
  __ Ucomisd(double_scratch2, double_result);
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // input was a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ Cvtlsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  __ bind(&call_runtime);
  // Move base to the correct argument register.  Exponent is already in xmm1.
  __ Movsd(xmm0, double_base);
  DCHECK(double_exponent == xmm1);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(2);
    __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                     2);
  }
  // Return value is in xmm0.
  __ Movsd(double_result, xmm0);

  __ bind(&done);
  __ ret(0);
}

Movability CEntryStub::NeedsImmovableCode() { return kMovable; }

void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  // It is important that the store buffer overflow stubs are generated first.
  CommonArrayConstructorStub::GenerateStubsAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
  CEntryStub save_doubles(isolate, 1, kSaveFPRegs);
  save_doubles.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer of calling JS frame (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (restored)
  //
  // If argv_in_register():
  // r15: pointer to the first argument

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

#ifdef _WIN64
  // Windows 64-bit ABI passes arguments in rcx, rdx, r8, r9. It requires the
  // stack to be aligned to 16 bytes. It only allows a single-word to be
  // returned in register rax. Larger return sizes must be written to an address
  // passed as a hidden first argument.
  const Register kCCallArg0 = rcx;
  const Register kCCallArg1 = rdx;
  const Register kCCallArg2 = r8;
  const Register kCCallArg3 = r9;
  const int kArgExtraStackSpace = 2;
  const int kMaxRegisterResultSize = 1;
#else
  // GCC / Clang passes arguments in rdi, rsi, rdx, rcx, r8, r9. Simple results
  // are returned in rax, and a struct of two pointers are returned in rax+rdx.
  // Larger return sizes must be written to an address passed as a hidden first
  // argument.
  const Register kCCallArg0 = rdi;
  const Register kCCallArg1 = rsi;
  const Register kCCallArg2 = rdx;
  const Register kCCallArg3 = rcx;
  const int kArgExtraStackSpace = 0;
  const int kMaxRegisterResultSize = 2;
#endif  // _WIN64

  // Enter the exit frame that transitions from JavaScript to C++.
  int arg_stack_space =
      kArgExtraStackSpace +
      (result_size() <= kMaxRegisterResultSize ? 0 : result_size());
  if (argv_in_register()) {
    DCHECK(!save_doubles());
    DCHECK(!is_builtin_exit());
    __ EnterApiExitFrame(arg_stack_space);
    // Move argc into r14 (argv is already in r15).
    __ movp(r14, rax);
  } else {
    __ EnterExitFrame(
        arg_stack_space, save_doubles(),
        is_builtin_exit() ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);
  }

  // rbx: pointer to builtin function  (C callee-saved).
  // rbp: frame pointer of exit frame  (restored after C call).
  // rsp: stack pointer (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: argv pointer (C callee-saved).

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }

  // Call C function. The arguments object will be created by stubs declared by
  // DECLARE_RUNTIME_FUNCTION().
  if (result_size() <= kMaxRegisterResultSize) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax), or a register pair (rax, rdx).
    __ movp(kCCallArg0, r14);  // argc.
    __ movp(kCCallArg1, r15);  // argv.
    __ Move(kCCallArg2, ExternalReference::isolate_address(isolate()));
  } else {
    DCHECK_LE(result_size(), 2);
    // Pass a pointer to the result location as the first argument.
    __ leap(kCCallArg0, StackSpaceOperand(kArgExtraStackSpace));
    // Pass a pointer to the Arguments object as the second argument.
    __ movp(kCCallArg1, r14);  // argc.
    __ movp(kCCallArg2, r15);  // argv.
    __ Move(kCCallArg3, ExternalReference::isolate_address(isolate()));
  }
  __ call(rbx);

  if (result_size() > kMaxRegisterResultSize) {
    // Read result values stored on stack. Result is stored
    // above the the two Arguments object slots on Win64.
    DCHECK_LE(result_size(), 2);
    __ movq(kReturnRegister0, StackSpaceOperand(kArgExtraStackSpace + 0));
    __ movq(kReturnRegister1, StackSpaceOperand(kArgExtraStackSpace + 1));
  }
  // Result is in rax or rdx:rax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(rax, Heap::kExceptionRootIndex);
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    __ LoadRoot(r14, Heap::kTheHoleValueRootIndex);
    ExternalReference pending_exception_address(
        IsolateAddressId::kPendingExceptionAddress, isolate());
    Operand pending_exception_operand =
        masm->ExternalOperand(pending_exception_address);
    __ cmpp(r14, pending_exception_operand);
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles(), !argv_in_register());
  __ ret(0);

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

  // Ask the runtime for help to determine the handler. This will set rax to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ movp(arg_reg_1, Immediate(0));  // argc.
    __ movp(arg_reg_2, Immediate(0));  // argv.
    __ Move(arg_reg_3, ExternalReference::isolate_address(isolate()));
    __ PrepareCallCFunction(3);
    __ CallCFunction(find_handler, 3);
  }
  // Retrieve the handler context, SP and FP.
  __ movp(rsi, masm->ExternalOperand(pending_handler_context_address));
  __ movp(rsp, masm->ExternalOperand(pending_handler_sp_address));
  __ movp(rbp, masm->ExternalOperand(pending_handler_fp_address));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (rsi == 0) for non-JS frames.
  Label skip;
  __ testp(rsi, rsi);
  __ j(zero, &skip, Label::kNear);
  __ movp(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
  __ bind(&skip);

  // Compute the handler entry address and jump to it.
  __ movp(rdi, masm->ExternalOperand(pending_handler_entrypoint_address));
  __ jmp(rdi);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  {  // NOLINT. Scope block confuses linter.
    MacroAssembler::NoRootArrayScope uninitialized_root_register(masm);
    // Set up frame.
    __ pushq(rbp);
    __ movp(rbp, rsp);

    // Push the stack frame type.
    __ Push(Immediate(StackFrame::TypeToMarker(type())));  // context slot
    ExternalReference context_address(IsolateAddressId::kContextAddress,
                                      isolate());
    __ Load(kScratchRegister, context_address);
    __ Push(kScratchRegister);  // context
    // Save callee-saved registers (X64/X32/Win64 calling conventions).
    __ pushq(r12);
    __ pushq(r13);
    __ pushq(r14);
    __ pushq(r15);
#ifdef _WIN64
    __ pushq(rdi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
    __ pushq(rsi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
#endif
    __ pushq(rbx);

#ifdef _WIN64
    // On Win64 XMM6-XMM15 are callee-save
    __ subp(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0), xmm6);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1), xmm7);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2), xmm8);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3), xmm9);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4), xmm10);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5), xmm11);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6), xmm12);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7), xmm13);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8), xmm14);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9), xmm15);
#endif

    // Set up the roots and smi constant registers.
    // Needs to be done before any further smi loads.
    __ InitializeRootRegister();
  }

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(IsolateAddressId::kCEntryFPAddress, isolate());
  {
    Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ Push(c_entry_fp_operand);
  }

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ Load(rax, js_entry_sp);
  __ testp(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ Push(Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ movp(rax, rbp);
  __ Store(js_entry_sp, rax);
  Label cont;
  __ jmp(&cont);
  __ bind(&not_outermost_js);
  __ Push(Immediate(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(
      IsolateAddressId::kPendingExceptionAddress, isolate());
  __ Store(pending_exception, rax);
  __ LoadRoot(rax, Heap::kExceptionRootIndex);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. We load the address from an
  // external reference instead of inlining the call target address directly
  // in the code, because the builtin stubs may not have been generated yet
  // at the time this code is generated.
  if (type() == StackFrame::CONSTRUCT_ENTRY) {
    __ Call(BUILTIN_CODE(isolate(), JSConstructEntryTrampoline),
            RelocInfo::CODE_TARGET);
  } else {
    __ Call(BUILTIN_CODE(isolate(), JSEntryTrampoline), RelocInfo::CODE_TARGET);
  }

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ Pop(rbx);
  __ cmpp(rbx, Immediate(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ j(not_equal, &not_outermost_js_2);
  __ Move(kScratchRegister, js_entry_sp);
  __ movp(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  { Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ Pop(c_entry_fp_operand);
  }

  // Restore callee-saved registers (X64 conventions).
#ifdef _WIN64
  // On Win64 XMM6-XMM15 are callee-save
  __ movdqu(xmm6, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0));
  __ movdqu(xmm7, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1));
  __ movdqu(xmm8, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2));
  __ movdqu(xmm9, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3));
  __ movdqu(xmm10, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4));
  __ movdqu(xmm11, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5));
  __ movdqu(xmm12, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6));
  __ movdqu(xmm13, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7));
  __ movdqu(xmm14, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8));
  __ movdqu(xmm15, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9));
  __ addp(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
#endif

  __ popq(rbx);
#ifdef _WIN64
  // Callee save on in Win64 ABI, arguments/volatile in AMD64 ABI.
  __ popq(rsi);
  __ popq(rdi);
#endif
  __ popq(r15);
  __ popq(r14);
  __ popq(r13);
  __ popq(r12);
  __ addp(rsp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ popq(rbp);
  __ ret(0);
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != nullptr) {
    ProfileEntryHookStub stub(masm->isolate());
    masm->CallStub(&stub);
  }
}

void ProfileEntryHookStub::MaybeCallEntryHookDelayed(TurboAssembler* tasm,
                                                     Zone* zone) {
  if (tasm->isolate()->function_entry_hook() != nullptr) {
    tasm->CallStubDelayed(new (zone) ProfileEntryHookStub(nullptr));
  }
}

void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // This stub can be called from essentially anywhere, so it needs to save
  // all volatile and callee-save registers.
  const size_t kNumSavedRegisters = 2;
  __ pushq(arg_reg_1);
  __ pushq(arg_reg_2);

  // Calculate the original stack pointer and store it in the second arg.
  __ leap(arg_reg_2,
         Operand(rsp, kNumSavedRegisters * kRegisterSize + kPCOnStackSize));

  // Calculate the function address to the first arg.
  __ movp(arg_reg_1, Operand(rsp, kNumSavedRegisters * kRegisterSize));
  __ subp(arg_reg_1, Immediate(Assembler::kShortCallInstructionLength));

  // Save the remainder of the volatile registers.
  masm->PushCallerSaved(kSaveFPRegs, arg_reg_1, arg_reg_2);

  // Call the entry hook function.
  __ Move(rax, FUNCTION_ADDR(isolate()->function_entry_hook()),
          Assembler::RelocInfoNone());

  AllowExternalCallThatCantCauseGC scope(masm);

  const int kArgumentCount = 2;
  __ PrepareCallCFunction(kArgumentCount);
  __ CallCFunction(rax, kArgumentCount);

  // Restore volatile regs.
  masm->PopCallerSaved(kSaveFPRegs, arg_reg_1, arg_reg_2);
  __ popq(arg_reg_2);
  __ popq(arg_reg_1);

  __ Ret();
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
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmpl(rdx, Immediate(kind));
      __ j(not_equal, &next);
      T stub(masm->isolate(), kind);
      __ TailCallStub(&stub);
      __ bind(&next);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // rbx - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // rdx - kind (if mode != DISABLE_ALLOCATION_SITES)
  // rax - number of arguments
  // rdi - constructor?
  // rsp[0] - return address
  // rsp[8] - last argument

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
    __ testb(rdx, Immediate(1));
    __ j(not_zero, &normal_sequence);

    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the slot).
    __ incl(rdx);

    if (FLAG_debug_code) {
      Handle<Map> allocation_site_map =
          masm->isolate()->factory()->allocation_site_map();
      __ Cmp(FieldOperand(rbx, 0), allocation_site_map);
      __ Assert(equal, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ SmiAddConstant(
        FieldOperand(rbx, AllocationSite::kTransitionInfoOrBoilerplateOffset),
        Smi::FromInt(kFastElementsKindPackedToHoley));

    __ bind(&normal_sequence);
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmpl(rdx, Immediate(kind));
      __ j(not_equal, &next);
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub);
      __ bind(&next);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
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
    MacroAssembler* masm, AllocationSiteOverrideMode mode) {
  Label not_zero_case, not_one_case;
  __ testp(rax, rax);
  __ j(not_zero, &not_zero_case);
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ bind(&not_zero_case);
  __ cmpl(rax, Immediate(1));
  __ j(greater, &not_one_case);
  CreateArrayDispatchOneArgument(masm, mode);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rbx    : AllocationSite or undefined
  //  -- rdi    : constructor
  //  -- rdx    : new target
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ movp(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rcx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rcx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in rbx or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(rbx);
  }

  // Enter the context of the Array function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  Label subclassing;
  __ cmpp(rdi, rdx);
  __ j(not_equal, &subclassing);

  Label no_info;
  // If the feedback vector is the undefined value call an array constructor
  // that doesn't use AllocationSites.
  __ CompareRoot(rbx, Heap::kUndefinedValueRootIndex);
  __ j(equal, &no_info);

  // Only look at the lower 16 bits of the transition info.
  __ movp(rdx, FieldOperand(
                   rbx, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  __ SmiToInteger32(rdx, rdx);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ andp(rdx, Immediate(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing
  __ bind(&subclassing);
  StackArgumentsAccessor args(rsp, rax);
  __ movp(args.GetReceiverOperand(), rdi);
  __ addp(rax, Immediate(3));
  __ PopReturnAddressTo(rcx);
  __ Push(rdx);
  __ Push(rbx);
  __ PushReturnAddressFrom(rcx);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  Label not_zero_case, not_one_case;
  Label normal_sequence;

  __ testp(rax, rax);
  __ j(not_zero, &not_zero_case);
  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0);

  __ bind(&not_zero_case);
  __ cmpl(rax, Immediate(1));
  __ j(greater, &not_one_case);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
    __ movp(rcx, args.GetArgumentOperand(0));
    __ testp(rcx, rcx);
    __ j(zero, &normal_sequence);

    InternalArraySingleArgumentConstructorStub
        stub1_holey(isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey);
  }

  __ bind(&normal_sequence);
  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stubN(isolate());
  __ TailCallStub(&stubN);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rdi    : constructor
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ movp(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a nullptr and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rcx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rcx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ movp(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following masking takes care of that anyway.
  __ movzxbp(rcx, FieldOperand(rcx, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(rcx);

  if (FLAG_debug_code) {
    Label done;
    __ cmpl(rcx, Immediate(PACKED_ELEMENTS));
    __ j(equal, &done);
    __ cmpl(rcx, Immediate(HOLEY_ELEMENTS));
    __ Assert(equal,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmpl(rcx, Immediate(PACKED_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateCase(masm, HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, PACKED_ELEMENTS);
}

static int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}

// Prepares stack to put arguments (aligns and so on).  WIN64 calling convention
// requires to put the pointer to the return value slot into rcx (rcx must be
// preserverd until CallApiFunctionAndReturn). Clobbers rax.  Allocates
// arg_stack_space * kPointerSize inside the exit frame (not GCed) accessible
// via StackSpaceOperand.
static void PrepareCallApiFunction(MacroAssembler* masm, int arg_stack_space) {
  __ EnterApiExitFrame(arg_stack_space);
}


// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers r14, r15, rbx and
// caller-save registers.  Restores context.  On return removes
// stack_space * kPointerSize (GCed).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     Register thunk_last_arg, int stack_space,
                                     Operand* stack_space_operand,
                                     Operand return_value_operand) {
  Label prologue;
  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label write_back;

  Isolate* isolate = masm->isolate();
  Factory* factory = isolate->factory();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = Offset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = Offset(
      ExternalReference::handle_scope_level_address(isolate), next_address);
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate);

  DCHECK(rdx == function_address || r8 == function_address);
  // Allocate HandleScope in callee-save registers.
  Register prev_next_address_reg = r14;
  Register prev_limit_reg = rbx;
  Register base_reg = r15;
  __ Move(base_reg, next_address);
  __ movp(prev_next_address_reg, Operand(base_reg, kNextOffset));
  __ movp(prev_limit_reg, Operand(base_reg, kLimitOffset));
  __ addl(Operand(base_reg, kLevelOffset), Immediate(1));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1);
    __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label profiler_disabled;
  Label end_profiler_check;
  __ Move(rax, ExternalReference::is_profiling_address(isolate));
  __ cmpb(Operand(rax, 0), Immediate(0));
  __ j(zero, &profiler_disabled);

  // Third parameter is the address of the actual getter function.
  __ Move(thunk_last_arg, function_address);
  __ Move(rax, thunk_ref);
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  // Call the api function!
  __ Move(rax, function_address);

  __ bind(&end_profiler_check);

  // Call the api function!
  __ call(rax);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1);
    __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  // Load the value from ReturnValue
  __ movp(rax, return_value_operand);
  __ bind(&prologue);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ subl(Operand(base_reg, kLevelOffset), Immediate(1));
  __ movp(Operand(base_reg, kNextOffset), prev_next_address_reg);
  __ cmpp(prev_limit_reg, Operand(base_reg, kLimitOffset));
  __ j(not_equal, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  if (stack_space_operand != nullptr) {
    __ movp(rbx, *stack_space_operand);
  }
  __ LeaveApiExitFrame();

  // Check if the function scheduled an exception.
  __ Move(rdi, scheduled_exception_address);
  __ Cmp(Operand(rdi, 0), factory->the_hole_value());
  __ j(not_equal, &promote_scheduled_exception);

#if DEBUG
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = rax;
  Register map = rcx;

  __ JumpIfSmi(return_value, &ok, Label::kNear);
  __ movp(map, FieldOperand(return_value, HeapObject::kMapOffset));

  __ CmpInstanceType(map, LAST_NAME_TYPE);
  __ j(below_equal, &ok, Label::kNear);

  __ CmpInstanceType(map, FIRST_JS_RECEIVER_TYPE);
  __ j(above_equal, &ok, Label::kNear);

  __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, Heap::kUndefinedValueRootIndex);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, Heap::kTrueValueRootIndex);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, Heap::kFalseValueRootIndex);
  __ j(equal, &ok, Label::kNear);

  __ CompareRoot(return_value, Heap::kNullValueRootIndex);
  __ j(equal, &ok, Label::kNear);

  __ Abort(kAPICallReturnedInvalidObject);

  __ bind(&ok);
#endif

  if (stack_space_operand != nullptr) {
    DCHECK_EQ(stack_space, 0);
    __ PopReturnAddressTo(rcx);
    __ addq(rsp, rbx);
    __ jmp(rcx);
  } else {
    __ ret(stack_space * kPointerSize);
  }

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ movp(Operand(base_reg, kLimitOffset), prev_limit_reg);
  __ movp(prev_limit_reg, rax);
  __ LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate));
  __ LoadAddress(rax,
                 ExternalReference::delete_handle_scope_extensions(isolate));
  __ call(rax);
  __ movp(rax, prev_limit_reg);
  __ jmp(&leave_exit_frame);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rbx                 : call_data
  //  -- rcx                 : holder
  //  -- rdx                 : api_function_address
  //  -- rsi                 : context
  //  -- rax                 : number of arguments if argc is a register
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : last argument
  //  -- ...
  //  -- rsp[argc * 8]       : first argument
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  Register call_data = rbx;
  Register holder = rcx;
  Register api_function_address = rdx;
  Register return_address = r8;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kArgsLength == 6);
  STATIC_ASSERT(FCA::kNewTargetIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  __ PopReturnAddressTo(return_address);

  // new target
  __ PushRoot(Heap::kUndefinedValueRootIndex);

  // call data
  __ Push(call_data);

  // return value
  __ PushRoot(Heap::kUndefinedValueRootIndex);
  // return value default
  __ PushRoot(Heap::kUndefinedValueRootIndex);
  // isolate
  Register scratch = call_data;
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Push(scratch);
  // holder
  __ Push(holder);

  int argc = this->argc();

  __ movp(scratch, rsp);
  // Push return address back on stack.
  __ PushReturnAddressFrom(return_address);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 3;

  PrepareCallApiFunction(masm, kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_.
  __ movp(StackSpaceOperand(0), scratch);
  __ addp(scratch, Immediate((argc + FCA::kArgsLength - 1) * kPointerSize));
  // FunctionCallbackInfo::values_.
  __ movp(StackSpaceOperand(1), scratch);
  // FunctionCallbackInfo::length_.
  __ Set(StackSpaceOperand(2), argc);

#if defined(__MINGW64__) || defined(_WIN64)
  Register arguments_arg = rcx;
  Register callback_arg = rdx;
#else
  Register arguments_arg = rdi;
  Register callback_arg = rsi;
#endif

  // It's okay if api_function_address == callback_arg
  // but not arguments_arg
  DCHECK(api_function_address != arguments_arg);

  // v8::InvocationCallback's argument.
  __ leap(arguments_arg, StackSpaceOperand(0));

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  // Accessor for FunctionCallbackInfo and first js arg.
  StackArgumentsAccessor args_from_rbp(rbp, FCA::kArgsLength + 1,
                                       ARGUMENTS_DONT_CONTAIN_RECEIVER);
  Operand return_value_operand = args_from_rbp.GetArgumentOperand(
      FCA::kArgsLength - FCA::kReturnValueOffset);
  const int stack_space = argc + FCA::kArgsLength + 1;
  Operand* stack_space_operand = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, callback_arg,
                           stack_space, stack_space_operand,
                           return_value_operand);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
#if defined(__MINGW64__) || defined(_WIN64)
  Register getter_arg = r8;
  Register accessor_info_arg = rdx;
  Register name_arg = rcx;
#else
  Register getter_arg = rdx;
  Register accessor_info_arg = rsi;
  Register name_arg = rdi;
#endif
  Register api_function_address = r8;
  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = rax;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

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

  // Insert additional parameters into the stack frame above return address.
  __ PopReturnAddressTo(scratch);
  __ Push(receiver);
  __ Push(FieldOperand(callback, AccessorInfo::kDataOffset));
  __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
  __ Push(kScratchRegister);  // return value
  __ Push(kScratchRegister);  // return value default
  __ PushAddress(ExternalReference::isolate_address(isolate()));
  __ Push(holder);
  __ Push(Smi::kZero);  // should_throw_on_error -> false
  __ Push(FieldOperand(callback, AccessorInfo::kNameOffset));
  __ PushReturnAddressFrom(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Allocate v8::PropertyCallbackInfo in non-GCed stack space.
  const int kArgStackSpace = 1;

  // Load address of v8::PropertyAccessorInfo::args_ array.
  __ leap(scratch, Operand(rsp, 2 * kPointerSize));

  PrepareCallApiFunction(masm, kArgStackSpace);
  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  Operand info_object = StackSpaceOperand(0);
  __ movp(info_object, scratch);

  __ leap(name_arg, Operand(scratch, -kPointerSize));
  // The context register (rsi) has been saved in PrepareCallApiFunction and
  // could be used to pass arguments.
  __ leap(accessor_info_arg, info_object);

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  // It's okay if api_function_address == getter_arg
  // but not accessor_info_arg or name_arg
  DCHECK(api_function_address != accessor_info_arg);
  DCHECK(api_function_address != name_arg);
  __ movp(scratch, FieldOperand(callback, AccessorInfo::kJsGetterOffset));
  __ movp(api_function_address,
          FieldOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  Operand return_value_operand(
      rbp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, getter_arg,
                           kStackUnwindSpace, nullptr, return_value_operand);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
