// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/api-arguments.h"
#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/counters.h"
#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/heap/heap-inl.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/objects/regexp-match-info.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

#include "src/arm64/code-stubs-arm64.h"  // Cannot be the first include.

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ Mov(x5, Operand(x0, LSL, kPointerSizeLog2));
  __ Str(x1, MemOperand(jssp, x5));
  __ Push(x1);
  __ Push(x2);
  __ Add(x0, x0, Operand(3));
  __ TailCallRuntime(Runtime::kNewArray);
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label done;
  Register input = source();
  Register result = destination();
  DCHECK(is_truncating());

  DCHECK(result.Is64Bits());
  DCHECK(jssp.Is(masm->StackPointer()));

  int double_offset = offset();

  DoubleRegister double_scratch = d0;  // only used if !skip_fastpath()
  Register scratch1 = GetAllocatableRegisterThatIsNotOneOf(input, result);
  Register scratch2 =
      GetAllocatableRegisterThatIsNotOneOf(input, result, scratch1);

  __ Push(scratch1, scratch2);
  // Account for saved regs if input is jssp.
  if (input.is(jssp)) double_offset += 2 * kPointerSize;

  if (!skip_fastpath()) {
    __ Push(double_scratch);
    if (input.is(jssp)) double_offset += 1 * kDoubleSize;
    __ Ldr(double_scratch, MemOperand(input, double_offset));
    // Try to convert with a FPU convert instruction.  This handles all
    // non-saturating cases.
    __ TryConvertDoubleToInt64(result, double_scratch, &done);
    __ Fmov(result, double_scratch);
  } else {
    __ Ldr(result, MemOperand(input, double_offset));
  }

  // If we reach here we need to manually convert the input to an int32.

  // Extract the exponent.
  Register exponent = scratch1;
  __ Ubfx(exponent, result, HeapNumber::kMantissaBits,
          HeapNumber::kExponentBits);

  // It the exponent is >= 84 (kMantissaBits + 32), the result is always 0 since
  // the mantissa gets shifted completely out of the int32_t result.
  __ Cmp(exponent, HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 32);
  __ CzeroX(result, ge);
  __ B(ge, &done);

  // The Fcvtzs sequence handles all cases except where the conversion causes
  // signed overflow in the int64_t target. Since we've already handled
  // exponents >= 84, we can guarantee that 63 <= exponent < 84.

  if (masm->emit_debug_code()) {
    __ Cmp(exponent, HeapNumber::kExponentBias + 63);
    // Exponents less than this should have been handled by the Fcvt case.
    __ Check(ge, kUnexpectedValue);
  }

  // Isolate the mantissa bits, and set the implicit '1'.
  Register mantissa = scratch2;
  __ Ubfx(mantissa, result, 0, HeapNumber::kMantissaBits);
  __ Orr(mantissa, mantissa, 1UL << HeapNumber::kMantissaBits);

  // Negate the mantissa if necessary.
  __ Tst(result, kXSignMask);
  __ Cneg(mantissa, mantissa, ne);

  // Shift the mantissa bits in the correct place. We know that we have to shift
  // it left here, because exponent >= 63 >= kMantissaBits.
  __ Sub(exponent, exponent,
         HeapNumber::kExponentBias + HeapNumber::kMantissaBits);
  __ Lsl(result, mantissa, exponent);

  __ Bind(&done);
  if (!skip_fastpath()) {
    __ Pop(double_scratch);
  }
  __ Pop(scratch2, scratch1);
  __ Ret();
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  CPURegList saved_regs = kCallerSaved;
  CPURegList saved_fp_regs = kCallerSavedV;

  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.

  // We don't care if MacroAssembler scratch registers are corrupted.
  saved_regs.Remove(*(masm->TmpList()));
  saved_fp_regs.Remove(*(masm->FPTmpList()));

  __ PushCPURegList(saved_regs);
  if (save_doubles()) {
    __ PushCPURegList(saved_fp_regs);
  }

  AllowExternalCallThatCantCauseGC scope(masm);
  __ Mov(x0, ExternalReference::isolate_address(isolate()));
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()), 1, 0);

  if (save_doubles()) {
    __ PopCPURegList(saved_fp_regs);
  }
  __ PopCPURegList(saved_regs);
  __ Ret();
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub1(isolate, kDontSaveFPRegs);
  stub1.GetCode();
  StoreBufferOverflowStub stub2(isolate, kSaveFPRegs);
  stub2.GetCode();
}


void MathPowStub::Generate(MacroAssembler* masm) {
  // Stack on entry:
  // jssp[0]: Exponent (as a tagged value).
  // jssp[1]: Base (as a tagged value).
  //
  // The (tagged) result will be returned in x0, as a heap number.

  Register exponent_tagged = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent_tagged.is(x11));
  Register exponent_integer = MathPowIntegerDescriptor::exponent();
  DCHECK(exponent_integer.is(x12));
  Register saved_lr = x19;
  VRegister result_double = d0;
  VRegister base_double = d0;
  VRegister exponent_double = d1;
  VRegister base_double_copy = d2;
  VRegister scratch1_double = d6;
  VRegister scratch0_double = d7;

  // A fast-path for integer exponents.
  Label exponent_is_smi, exponent_is_integer;
  // Allocate a heap number for the result, and return it.
  Label done;

  // Unpack the inputs.
  if (exponent_type() == TAGGED) {
    __ JumpIfSmi(exponent_tagged, &exponent_is_smi);
    __ Ldr(exponent_double,
           FieldMemOperand(exponent_tagged, HeapNumber::kValueOffset));
  }

  // Handle double (heap number) exponents.
  if (exponent_type() != INTEGER) {
    // Detect integer exponents stored as doubles and handle those in the
    // integer fast-path.
    __ TryRepresentDoubleAsInt64(exponent_integer, exponent_double,
                                 scratch0_double, &exponent_is_integer);

    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ Mov(saved_lr, lr);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 0, 2);
      __ Mov(lr, saved_lr);
      __ B(&done);
    }

    // Handle SMI exponents.
    __ Bind(&exponent_is_smi);
    //  x10   base_tagged       The tagged base (input).
    //  x11   exponent_tagged   The tagged exponent (input).
    //  d1    base_double       The base as a double.
    __ SmiUntag(exponent_integer, exponent_tagged);
  }

  __ Bind(&exponent_is_integer);
  //  x10   base_tagged       The tagged base (input).
  //  x11   exponent_tagged   The tagged exponent (input).
  //  x12   exponent_integer  The exponent as an integer.
  //  d1    base_double       The base as a double.

  // Find abs(exponent). For negative exponents, we can find the inverse later.
  Register exponent_abs = x13;
  __ Cmp(exponent_integer, 0);
  __ Cneg(exponent_abs, exponent_integer, mi);
  //  x13   exponent_abs      The value of abs(exponent_integer).

  // Repeatedly multiply to calculate the power.
  //  result = 1.0;
  //  For each bit n (exponent_integer{n}) {
  //    if (exponent_integer{n}) {
  //      result *= base;
  //    }
  //    base *= base;
  //    if (remaining bits in exponent_integer are all zero) {
  //      break;
  //    }
  //  }
  Label power_loop, power_loop_entry, power_loop_exit;
  __ Fmov(scratch1_double, base_double);
  __ Fmov(base_double_copy, base_double);
  __ Fmov(result_double, 1.0);
  __ B(&power_loop_entry);

  __ Bind(&power_loop);
  __ Fmul(scratch1_double, scratch1_double, scratch1_double);
  __ Lsr(exponent_abs, exponent_abs, 1);
  __ Cbz(exponent_abs, &power_loop_exit);

  __ Bind(&power_loop_entry);
  __ Tbz(exponent_abs, 0, &power_loop);
  __ Fmul(result_double, result_double, scratch1_double);
  __ B(&power_loop);

  __ Bind(&power_loop_exit);

  // If the exponent was positive, result_double holds the result.
  __ Tbz(exponent_integer, kXSignBit, &done);

  // The exponent was negative, so find the inverse.
  __ Fmov(scratch0_double, 1.0);
  __ Fdiv(result_double, scratch0_double, result_double);
  // ECMA-262 only requires Math.pow to return an 'implementation-dependent
  // approximation' of base^exponent. However, mjsunit/math-pow uses Math.pow
  // to calculate the subnormal value 2^-1074. This method of calculating
  // negative powers doesn't work because 2^1074 overflows to infinity. To
  // catch this corner-case, we bail out if the result was 0. (This can only
  // occur if the divisor is infinity or the base is zero.)
  __ Fcmp(result_double, 0.0);
  __ B(&done, ne);

  AllowExternalCallThatCantCauseGC scope(masm);
  __ Mov(saved_lr, lr);
  __ Fmov(base_double, base_double_copy);
  __ Scvtf(exponent_double, exponent_integer);
  __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                   0, 2);
  __ Mov(lr, saved_lr);
  __ Bind(&done);
  __ Ret();
}

void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  // It is important that the following stubs are generated in this order
  // because pregenerated stubs can only call other pregenerated stubs.
  // RecordWriteStub uses StoreBufferOverflowStub, which in turn uses
  // CEntryStub.
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  CommonArrayConstructorStub::GenerateStubsAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
  // Floating-point code doesn't get special handling in ARM64, so there's
  // nothing to do here.
  USE(isolate);
}


bool CEntryStub::NeedsImmovableCode() {
  // CEntryStub stores the return address on the stack before calling into
  // C++ code. In some cases, the VM accesses this address, but it is not used
  // when the C++ code returns to the stub because LR holds the return address
  // in AAPCS64. If the stub is moved (perhaps during a GC), we could end up
  // returning to dead code.
  // TODO(jbramley): Whilst this is the only analysis that makes sense, I can't
  // find any comment to confirm this, and I don't hit any crashes whatever
  // this function returns. The anaylsis should be properly confirmed.
  return true;
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
  CEntryStub stub_fp(isolate, 1, kSaveFPRegs);
  stub_fp.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // The Abort mechanism relies on CallRuntime, which in turn relies on
  // CEntryStub, so until this stub has been generated, we have to use a
  // fall-back Abort mechanism.
  //
  // Note that this stub must be generated before any use of Abort.
  MacroAssembler::NoUseRealAbortsScope no_use_real_aborts(masm);

  ASM_LOCATION("CEntryStub::Generate entry");
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Register parameters:
  //    x0: argc (including receiver, untagged)
  //    x1: target
  // If argv_in_register():
  //    x11: argv (pointer to first argument)
  //
  // The stack on entry holds the arguments and the receiver, with the receiver
  // at the highest address:
  //
  //    jssp]argc-1]: receiver
  //    jssp[argc-2]: arg[argc-2]
  //    ...           ...
  //    jssp[1]:      arg[1]
  //    jssp[0]:      arg[0]
  //
  // The arguments are in reverse order, so that arg[argc-2] is actually the
  // first argument to the target function and arg[0] is the last.
  DCHECK(jssp.Is(__ StackPointer()));
  const Register& argc_input = x0;
  const Register& target_input = x1;

  // Calculate argv, argc and the target address, and store them in
  // callee-saved registers so we can retry the call without having to reload
  // these arguments.
  // TODO(jbramley): If the first call attempt succeeds in the common case (as
  // it should), then we might be better off putting these parameters directly
  // into their argument registers, rather than using callee-saved registers and
  // preserving them on the stack.
  const Register& argv = x21;
  const Register& argc = x22;
  const Register& target = x23;

  // Derive argv from the stack pointer so that it points to the first argument
  // (arg[argc-2]), or just below the receiver in case there are no arguments.
  //  - Adjust for the arg[] array.
  Register temp_argv = x11;
  if (!argv_in_register()) {
    __ Add(temp_argv, jssp, Operand(x0, LSL, kPointerSizeLog2));
    //  - Adjust for the receiver.
    __ Sub(temp_argv, temp_argv, 1 * kPointerSize);
  }

  // Reserve three slots to preserve x21-x23 callee-saved registers. If the
  // result size is too large to be returned in registers then also reserve
  // space for the return value.
  int extra_stack_space = 3 + (result_size() <= 2 ? 0 : result_size());
  // Enter the exit frame.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(
      save_doubles(), x10, extra_stack_space,
      is_builtin_exit() ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);
  DCHECK(csp.Is(__ StackPointer()));

  // Poke callee-saved registers into reserved space.
  __ Poke(argv, 1 * kPointerSize);
  __ Poke(argc, 2 * kPointerSize);
  __ Poke(target, 3 * kPointerSize);

  if (result_size() > 2) {
    // Save the location of the return value into x8 for call.
    __ Add(x8, __ StackPointer(), Operand(4 * kPointerSize));
  }

  // We normally only keep tagged values in callee-saved registers, as they
  // could be pushed onto the stack by called stubs and functions, and on the
  // stack they can confuse the GC. However, we're only calling C functions
  // which can push arbitrary data onto the stack anyway, and so the GC won't
  // examine that part of the stack.
  __ Mov(argc, argc_input);
  __ Mov(target, target_input);
  __ Mov(argv, temp_argv);

  // x21 : argv
  // x22 : argc
  // x23 : call target
  //
  // The stack (on entry) holds the arguments and the receiver, with the
  // receiver at the highest address:
  //
  //         argv[8]:     receiver
  // argv -> argv[0]:     arg[argc-2]
  //         ...          ...
  //         argv[...]:   arg[1]
  //         argv[...]:   arg[0]
  //
  // Immediately below (after) this is the exit frame, as constructed by
  // EnterExitFrame:
  //         fp[8]:    CallerPC (lr)
  //   fp -> fp[0]:    CallerFP (old fp)
  //         fp[-8]:   Space reserved for SPOffset.
  //         fp[-16]:  CodeObject()
  //         csp[...]: Saved doubles, if saved_doubles is true.
  //         csp[32]:  Alignment padding, if necessary.
  //         csp[24]:  Preserved x23 (used for target).
  //         csp[16]:  Preserved x22 (used for argc).
  //         csp[8]:   Preserved x21 (used for argv).
  //  csp -> csp[0]:   Space reserved for the return address.
  //
  // After a successful call, the exit frame, preserved registers (x21-x23) and
  // the arguments (including the receiver) are dropped or popped as
  // appropriate. The stub then returns.
  //
  // After an unsuccessful call, the exit frame and suchlike are left
  // untouched, and the stub either throws an exception by jumping to one of
  // the exception_returned label.

  DCHECK(csp.Is(__ StackPointer()));

  // Prepare AAPCS64 arguments to pass to the builtin.
  __ Mov(x0, argc);
  __ Mov(x1, argv);
  __ Mov(x2, ExternalReference::isolate_address(isolate()));

  Label return_location;
  __ Adr(x12, &return_location);
  __ Poke(x12, 0);

  if (__ emit_debug_code()) {
    // Verify that the slot below fp[kSPOffset]-8 points to the return location
    // (currently in x12).
    UseScratchRegisterScope temps(masm);
    Register temp = temps.AcquireX();
    __ Ldr(temp, MemOperand(fp, ExitFrameConstants::kSPOffset));
    __ Ldr(temp, MemOperand(temp, -static_cast<int64_t>(kXRegSize)));
    __ Cmp(temp, x12);
    __ Check(eq, kReturnAddressNotFoundInFrame);
  }

  // Call the builtin.
  __ Blr(target);
  __ Bind(&return_location);

  if (result_size() > 2) {
    DCHECK_EQ(3, result_size());
    // Read result values stored on stack.
    __ Ldr(x0, MemOperand(__ StackPointer(), 4 * kPointerSize));
    __ Ldr(x1, MemOperand(__ StackPointer(), 5 * kPointerSize));
    __ Ldr(x2, MemOperand(__ StackPointer(), 6 * kPointerSize));
  }
  // Result returned in x0, x1:x0 or x2:x1:x0 - do not destroy these registers!

  //  x0    result0      The return code from the call.
  //  x1    result1      For calls which return ObjectPair or ObjectTriple.
  //  x2    result2      For calls which return ObjectTriple.
  //  x21   argv
  //  x22   argc
  //  x23   target
  const Register& result = x0;

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(result, Heap::kExceptionRootIndex);
  __ B(eq, &exception_returned);

  // The call succeeded, so unwind the stack and return.

  // Restore callee-saved registers x21-x23.
  __ Mov(x11, argc);

  __ Peek(argv, 1 * kPointerSize);
  __ Peek(argc, 2 * kPointerSize);
  __ Peek(target, 3 * kPointerSize);

  __ LeaveExitFrame(save_doubles(), x10, true);
  DCHECK(jssp.Is(__ StackPointer()));
  if (!argv_in_register()) {
    // Drop the remaining stack slots and return from the stub.
    __ Drop(x11);
  }
  __ AssertFPCRState();
  __ Ret();

  // The stack pointer is still csp if we aren't returning, and the frame
  // hasn't changed (except for the return address).
  __ SetStackPointer(csp);

  // Handling of exception.
  __ Bind(&exception_returned);

  ExternalReference pending_handler_context_address(
      IsolateAddressId::kPendingHandlerContextAddress, isolate());
  ExternalReference pending_handler_code_address(
      IsolateAddressId::kPendingHandlerCodeAddress, isolate());
  ExternalReference pending_handler_offset_address(
      IsolateAddressId::kPendingHandlerOffsetAddress, isolate());
  ExternalReference pending_handler_fp_address(
      IsolateAddressId::kPendingHandlerFPAddress, isolate());
  ExternalReference pending_handler_sp_address(
      IsolateAddressId::kPendingHandlerSPAddress, isolate());

  // Ask the runtime for help to determine the handler. This will set x0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  DCHECK(csp.Is(masm->StackPointer()));
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Mov(x0, 0);  // argc.
    __ Mov(x1, 0);  // argv.
    __ Mov(x2, ExternalReference::isolate_address(isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // We didn't execute a return case, so the stack frame hasn't been updated
  // (except for the return address slot). However, we don't need to initialize
  // jssp because the throw method will immediately overwrite it when it
  // unwinds the stack.
  __ SetStackPointer(jssp);

  // Retrieve the handler context, SP and FP.
  __ Mov(cp, Operand(pending_handler_context_address));
  __ Ldr(cp, MemOperand(cp));
  __ Mov(jssp, Operand(pending_handler_sp_address));
  __ Ldr(jssp, MemOperand(jssp));
  __ Mov(csp, jssp);
  __ Mov(fp, Operand(pending_handler_fp_address));
  __ Ldr(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label not_js_frame;
  __ Cbz(cp, &not_js_frame);
  __ Str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ Bind(&not_js_frame);

  // Compute the handler entry address and jump to it.
  __ Mov(x10, Operand(pending_handler_code_address));
  __ Ldr(x10, MemOperand(x10));
  __ Mov(x11, Operand(pending_handler_offset_address));
  __ Ldr(x11, MemOperand(x11));
  __ Add(x10, x10, Code::kHeaderSize - kHeapObjectTag);
  __ Add(x10, x10, x11);
  __ Br(x10);
}


// This is the entry point from C++. 5 arguments are provided in x0-x4.
// See use of the CALL_GENERATED_CODE macro for example in src/execution.cc.
// Input:
//   x0: code entry.
//   x1: function.
//   x2: receiver.
//   x3: argc.
//   x4: argv.
// Output:
//   x0: result.
void JSEntryStub::Generate(MacroAssembler* masm) {
  DCHECK(jssp.Is(__ StackPointer()));
  Register code_entry = x0;

  // Enable instruction instrumentation. This only works on the simulator, and
  // will have no effect on the model or real hardware.
  __ EnableInstrumentation();

  Label invoke, handler_entry, exit;

  // Push callee-saved registers and synchronize the system stack pointer (csp)
  // and the JavaScript stack pointer (jssp).
  //
  // We must not write to jssp until after the PushCalleeSavedRegisters()
  // call, since jssp is itself a callee-saved register.
  __ SetStackPointer(csp);
  __ PushCalleeSavedRegisters();
  __ Mov(jssp, csp);
  __ SetStackPointer(jssp);

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Set up the reserved register for 0.0.
  __ Fmov(fp_zero, 0.0);

  // Build an entry frame (see layout below).
  StackFrame::Type marker = type();
  int64_t bad_frame_pointer = -1L;  // Bad frame pointer to fail if it is used.
  __ Mov(x13, bad_frame_pointer);
  __ Mov(x12, StackFrame::TypeToMarker(marker));
  __ Mov(x11, ExternalReference(IsolateAddressId::kCEntryFPAddress, isolate()));
  __ Ldr(x10, MemOperand(x11));

  __ Push(x13, x12, xzr, x10);
  // Set up fp.
  __ Sub(fp, jssp, EntryFrameConstants::kCallerFPOffset);

  // Push the JS entry frame marker. Also set js_entry_sp if this is the
  // outermost JS call.
  Label non_outermost_js, done;
  ExternalReference js_entry_sp(IsolateAddressId::kJSEntrySPAddress, isolate());
  __ Mov(x10, ExternalReference(js_entry_sp));
  __ Ldr(x11, MemOperand(x10));
  __ Cbnz(x11, &non_outermost_js);
  __ Str(fp, MemOperand(x10));
  __ Mov(x12, StackFrame::OUTERMOST_JSENTRY_FRAME);
  __ Push(x12);
  __ B(&done);
  __ Bind(&non_outermost_js);
  // We spare one instruction by pushing xzr since the marker is 0.
  DCHECK(StackFrame::INNER_JSENTRY_FRAME == 0);
  __ Push(xzr);
  __ Bind(&done);

  // The frame set up looks like this:
  // jssp[0] : JS entry frame marker.
  // jssp[1] : C entry FP.
  // jssp[2] : stack frame marker.
  // jssp[3] : stack frmae marker.
  // jssp[4] : bad frame pointer 0xfff...ff   <- fp points here.


  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ B(&invoke);

  // Prevent the constant pool from being emitted between the record of the
  // handler_entry position and the first instruction of the sequence here.
  // There is no risk because Assembler::Emit() emits the instruction before
  // checking for constant pool emission, but we do not want to depend on
  // that.
  {
    Assembler::BlockPoolsScope block_pools(masm);
    __ bind(&handler_entry);
    handler_offset_ = handler_entry.pos();
    // Caught exception: Store result (exception) in the pending exception
    // field in the JSEnv and return a failure sentinel. Coming in here the
    // fp will be invalid because the PushTryHandler below sets it to 0 to
    // signal the existence of the JSEntry frame.
    __ Mov(x10, Operand(ExternalReference(
                    IsolateAddressId::kPendingExceptionAddress, isolate())));
  }
  __ Str(code_entry, MemOperand(x10));
  __ LoadRoot(x0, Heap::kExceptionRootIndex);
  __ B(&exit);

  // Invoke: Link this frame into the handler chain.
  __ Bind(&invoke);
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the B(&invoke) above, which
  // restores all callee-saved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through the JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // x0: code entry.
  // x1: function.
  // x2: receiver.
  // x3: argc.
  // x4: argv.

  if (type() == StackFrame::CONSTRUCT_ENTRY) {
    __ Call(BUILTIN_CODE(isolate(), JSConstructEntryTrampoline),
            RelocInfo::CODE_TARGET);
  } else {
    __ Call(BUILTIN_CODE(isolate(), JSEntryTrampoline), RelocInfo::CODE_TARGET);
  }

  // Unlink this frame from the handler chain.
  __ PopStackHandler();


  __ Bind(&exit);
  // x0 holds the result.
  // The stack pointer points to the top of the entry frame pushed on entry from
  // C++ (at the beginning of this stub):
  // jssp[0] : JS entry frame marker.
  // jssp[1] : C entry FP.
  // jssp[2] : stack frame marker.
  // jssp[3] : stack frmae marker.
  // jssp[4] : bad frame pointer 0xfff...ff   <- fp points here.

  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ Pop(x10);
  __ Cmp(x10, StackFrame::OUTERMOST_JSENTRY_FRAME);
  __ B(ne, &non_outermost_js_2);
  __ Mov(x11, ExternalReference(js_entry_sp));
  __ Str(xzr, MemOperand(x11));
  __ Bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ Pop(x10);
  __ Mov(x11, ExternalReference(IsolateAddressId::kCEntryFPAddress, isolate()));
  __ Str(x10, MemOperand(x11));

  // Reset the stack to the callee saved registers.
  __ Drop(-EntryFrameConstants::kCallerFPOffset, kByteSizeInBytes);
  // Restore the callee-saved registers and return.
  DCHECK(jssp.Is(__ StackPointer()));
  __ Mov(csp, jssp);
  __ SetStackPointer(csp);
  __ PopCalleeSavedRegisters();
  // After this point, we must not modify jssp because it is a callee-saved
  // register which we have just restored.
  __ Ret();
}


void StringHelper::GenerateFlatOneByteStringEquals(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  DCHECK(!AreAliased(left, right, scratch1, scratch2, scratch3));
  Register result = x0;
  Register left_length = scratch1;
  Register right_length = scratch2;

  // Compare lengths. If lengths differ, strings can't be equal. Lengths are
  // smis, and don't need to be untagged.
  Label strings_not_equal, check_zero_length;
  __ Ldr(left_length, FieldMemOperand(left, String::kLengthOffset));
  __ Ldr(right_length, FieldMemOperand(right, String::kLengthOffset));
  __ Cmp(left_length, right_length);
  __ B(eq, &check_zero_length);

  __ Bind(&strings_not_equal);
  __ Mov(result, Smi::FromInt(NOT_EQUAL));
  __ Ret();

  // Check if the length is zero. If so, the strings must be equal (and empty.)
  Label compare_chars;
  __ Bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ Cbnz(left_length, &compare_chars);
  __ Mov(result, Smi::FromInt(EQUAL));
  __ Ret();

  // Compare characters. Falls through if all characters are equal.
  __ Bind(&compare_chars);
  GenerateOneByteCharsCompareLoop(masm, left, right, left_length, scratch2,
                                  scratch3, &strings_not_equal);

  // Characters in strings are equal.
  __ Mov(result, Smi::FromInt(EQUAL));
  __ Ret();
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3, Register scratch4) {
  DCHECK(!AreAliased(left, right, scratch1, scratch2, scratch3, scratch4));
  Label result_not_equal, compare_lengths;

  // Find minimum length and length difference.
  Register length_delta = scratch3;
  __ Ldr(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ Ldr(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Subs(length_delta, scratch1, scratch2);

  Register min_length = scratch1;
  __ Csel(min_length, scratch2, scratch1, gt);
  __ Cbz(min_length, &compare_lengths);

  // Compare loop.
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  scratch4, &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ Bind(&compare_lengths);

  DCHECK(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));

  // Use length_delta as result if it's zero.
  Register result = x0;
  __ Subs(result, length_delta, 0);

  __ Bind(&result_not_equal);
  Register greater = x10;
  Register less = x11;
  __ Mov(greater, Smi::FromInt(GREATER));
  __ Mov(less, Smi::FromInt(LESS));
  __ CmovX(result, greater, gt);
  __ CmovX(result, less, lt);
  __ Ret();
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch1, Register scratch2, Label* chars_not_equal) {
  DCHECK(!AreAliased(left, right, length, scratch1, scratch2));

  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ Add(scratch1, length, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ Add(left, left, scratch1);
  __ Add(right, right, scratch1);

  Register index = length;
  __ Neg(index, length);  // index = -length;

  // Compare loop
  Label loop;
  __ Bind(&loop);
  __ Ldrb(scratch1, MemOperand(left, index));
  __ Ldrb(scratch2, MemOperand(right, index));
  __ Cmp(scratch1, scratch2);
  __ B(ne, chars_not_equal);
  __ Add(index, index, 1);
  __ Cbnz(index, &loop);
}


RecordWriteStub::RegisterAllocation::RegisterAllocation(Register object,
                                                        Register address,
                                                        Register scratch)
    : object_(object),
      address_(address),
      scratch0_(scratch),
      saved_regs_(kCallerSaved),
      saved_fp_regs_(kCallerSavedV) {
  DCHECK(!AreAliased(scratch, object, address));

  // The SaveCallerSaveRegisters method needs to save caller-saved
  // registers, but we don't bother saving MacroAssembler scratch registers.
  saved_regs_.Remove(MacroAssembler::DefaultTmpList());
  saved_fp_regs_.Remove(MacroAssembler::DefaultFPTmpList());

  // We would like to require more scratch registers for this stub,
  // but the number of registers comes down to the ones used in
  // FullCodeGen::SetVar(), which is architecture independent.
  // We allocate 2 extra scratch registers that we'll save on the stack.
  CPURegList pool_available = GetValidRegistersForAllocation();
  CPURegList used_regs(object, address, scratch);
  pool_available.Remove(used_regs);
  scratch1_ = Register(pool_available.PopLowestIndex());
  scratch2_ = Register(pool_available.PopLowestIndex());

  // The scratch registers will be restored by other means so we don't need
  // to save them with the other caller saved registers.
  saved_regs_.Remove(scratch0_);
  saved_regs_.Remove(scratch1_);
  saved_regs_.Remove(scratch2_);
}

void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  // We need some extra registers for this stub, they have been allocated
  // but we need to save them before using them.
  regs_.Save(masm);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    Register val = regs_.scratch0();
    __ Ldr(val, MemOperand(regs_.address()));
    __ JumpIfNotInNewSpace(val, &dont_need_remembered_set);

    __ JumpIfInNewSpace(regs_.object(), &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm, kUpdateRememberedSetOnNoNeedToInformIncrementalMarker, mode);
    InformIncrementalMarker(masm);
    regs_.Restore(masm);  // Restore the extra scratch registers we used.

    __ RememberedSetHelper(object(), address(),
                           value(),  // scratch1
                           save_fp_regs_mode(), MacroAssembler::kReturnAtEnd);

    __ Bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm, kReturnOnNoNeedToInformIncrementalMarker, mode);
  InformIncrementalMarker(masm);
  regs_.Restore(masm);  // Restore the extra scratch registers we used.
  __ Ret();
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode());
  Register address =
    x0.Is(regs_.address()) ? regs_.scratch0() : regs_.address();
  DCHECK(!address.Is(regs_.object()));
  DCHECK(!address.Is(x0));
  __ Mov(address, regs_.address());
  __ Mov(x0, regs_.object());
  __ Mov(x1, address);
  __ Mov(x2, ExternalReference::isolate_address(isolate()));

  AllowExternalCallThatCantCauseGC scope(masm);
  ExternalReference function =
      ExternalReference::incremental_marking_record_write_function(
          isolate());
  __ CallCFunction(function, 3, 0);

  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode());
}

void RecordWriteStub::Activate(Code* code) {
  code->GetHeap()->incremental_marking()->ActivateGeneratedStub(code);
}

void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label need_incremental;
  Label need_incremental_pop_scratch;

#ifndef V8_CONCURRENT_MARKING
  Label on_black;
  // If the object is not black we don't have to inform the incremental marker.
  __ JumpIfBlack(regs_.object(), regs_.scratch0(), regs_.scratch1(), &on_black);

  regs_.Restore(masm);  // Restore the extra scratch registers we used.
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(),
                           value(),  // scratch1
                           save_fp_regs_mode(), MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ Bind(&on_black);
#endif

  // Get the value from the slot.
  Register val = regs_.scratch0();
  __ Ldr(val, MemOperand(regs_.address()));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlagClear(val, regs_.scratch1(),
                          MemoryChunk::kEvacuationCandidateMask,
                          &ensure_not_white);

    __ CheckPageFlagClear(regs_.object(),
                          regs_.scratch1(),
                          MemoryChunk::kSkipEvacuationSlotsRecordingMask,
                          &need_incremental);

    __ Bind(&ensure_not_white);
  }

  // We need extra registers for this, so we push the object and the address
  // register temporarily.
  __ Push(regs_.address(), regs_.object());
  __ JumpIfWhite(val,
                 regs_.scratch1(),  // Scratch.
                 regs_.object(),    // Scratch.
                 regs_.address(),   // Scratch.
                 regs_.scratch2(),  // Scratch.
                 &need_incremental_pop_scratch);
  __ Pop(regs_.object(), regs_.address());

  regs_.Restore(masm);  // Restore the extra scratch registers we used.
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(),
                           value(),  // scratch1
                           save_fp_regs_mode(), MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ Bind(&need_incremental_pop_scratch);
  __ Pop(regs_.object(), regs_.address());

  __ Bind(&need_incremental);
  // Fall through when we need to inform the incremental marker.
}


void RecordWriteStub::Generate(MacroAssembler* masm) {
  Label skip_to_incremental_noncompacting;
  Label skip_to_incremental_compacting;

  // We patch these two first instructions back and forth between a nop and
  // real branch when we start and stop incremental heap marking.
  // Initially the stub is expected to be in STORE_BUFFER_ONLY mode, so 2 nops
  // are generated.
  // See RecordWriteStub::Patch for details.
  {
    InstructionAccurateScope scope(masm, 2);
    __ adr(xzr, &skip_to_incremental_noncompacting);
    __ adr(xzr, &skip_to_incremental_compacting);
  }

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object(), address(),
                           value(),  // scratch1
                           save_fp_regs_mode(), MacroAssembler::kReturnAtEnd);
  }
  __ Ret();

  __ Bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ Bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);
}


// The entry hook is a "BumpSystemStackPointer" instruction (sub), followed by
// a "Push lr" instruction, followed by a call.
static const unsigned int kProfileEntryHookCallSize =
    Assembler::kCallSizeWithRelocation + (2 * kInstructionSize);

void ProfileEntryHookStub::MaybeCallEntryHookDelayed(TurboAssembler* tasm,
                                                     Zone* zone) {
  if (tasm->isolate()->function_entry_hook() != NULL) {
    Assembler::BlockConstPoolScope no_const_pools(tasm);
    DontEmitDebugCodeScope no_debug_code(tasm);
    Label entry_hook_call_start;
    tasm->Bind(&entry_hook_call_start);
    tasm->Push(lr);
    tasm->CallStubDelayed(new (zone) ProfileEntryHookStub(nullptr));
    DCHECK(tasm->SizeOfCodeGeneratedSince(&entry_hook_call_start) ==
           kProfileEntryHookCallSize);
    tasm->Pop(lr);
  }
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    ProfileEntryHookStub stub(masm->isolate());
    Assembler::BlockConstPoolScope no_const_pools(masm);
    DontEmitDebugCodeScope no_debug_code(masm);
    Label entry_hook_call_start;
    __ Bind(&entry_hook_call_start);
    __ Push(lr);
    __ CallStub(&stub);
    DCHECK(masm->SizeOfCodeGeneratedSince(&entry_hook_call_start) ==
           kProfileEntryHookCallSize);
    __ Pop(lr);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  MacroAssembler::NoUseRealAbortsScope no_use_real_aborts(masm);

  // Save all kCallerSaved registers (including lr), since this can be called
  // from anywhere.
  // TODO(jbramley): What about FP registers?
  __ PushCPURegList(kCallerSaved);
  DCHECK(kCallerSaved.IncludesAliasOf(lr));
  const int kNumSavedRegs = kCallerSaved.Count();

  // Compute the function's address as the first argument.
  __ Sub(x0, lr, kProfileEntryHookCallSize);

#if V8_HOST_ARCH_ARM64
  uintptr_t entry_hook =
      reinterpret_cast<uintptr_t>(isolate()->function_entry_hook());
  __ Mov(x10, entry_hook);
#else
  // Under the simulator we need to indirect the entry hook through a trampoline
  // function at a known address.
  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ Mov(x10, Operand(ExternalReference(&dispatcher,
                                        ExternalReference::BUILTIN_CALL,
                                        isolate())));
  // It additionally takes an isolate as a third parameter
  __ Mov(x2, ExternalReference::isolate_address(isolate()));
#endif

  // The caller's return address is above the saved temporaries.
  // Grab its location for the second argument to the hook.
  __ Add(x1, __ StackPointer(), kNumSavedRegs * kPointerSize);

  {
    // Create a dummy frame, as CallCFunction requires this.
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallCFunction(x10, 2, 0);
  }

  __ PopCPURegList(kCallerSaved);
  __ Ret();
}


void DirectCEntryStub::Generate(MacroAssembler* masm) {
  // When calling into C++ code the stack pointer must be csp.
  // Therefore this code must use csp for peek/poke operations when the
  // stub is generated. When the stub is called
  // (via DirectCEntryStub::GenerateCall), the caller must setup an ExitFrame
  // and configure the stack pointer *before* doing the call.
  const Register old_stack_pointer = __ StackPointer();
  __ SetStackPointer(csp);

  // Put return address on the stack (accessible to GC through exit frame pc).
  __ Poke(lr, 0);
  // Call the C++ function.
  __ Blr(x10);
  // Return to calling code.
  __ Peek(lr, 0);
  __ AssertFPCRState();
  __ Ret();

  __ SetStackPointer(old_stack_pointer);
}

void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    Register target) {
  // Make sure the caller configured the stack pointer (see comment in
  // DirectCEntryStub::Generate).
  DCHECK(csp.Is(__ StackPointer()));

  intptr_t code =
      reinterpret_cast<intptr_t>(GetCode().location());
  __ Mov(lr, Operand(code, RelocInfo::CODE_TARGET));
  __ Mov(x10, target);
  // Branch to the stub.
  __ Blr(lr);
}

void NameDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register receiver,
                                                      Register properties,
                                                      Handle<Name> name,
                                                      Register scratch0) {
  DCHECK(!AreAliased(receiver, properties, scratch0));
  DCHECK(name->IsUniqueName());
  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the hole value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // scratch0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = scratch0;
    // Capacity is smi 2^n.
    __ Ldrsw(index, UntagSmiFieldMemOperand(properties, kCapacityOffset));
    __ Sub(index, index, 1);
    __ And(index, index, name->Hash() + NameDictionary::GetProbeOffset(i));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ Add(index, index, Operand(index, LSL, 1));  // index *= 3.

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    Register tmp = index;
    __ Add(tmp, properties, Operand(index, LSL, kPointerSizeLog2));
    __ Ldr(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    __ JumpIfRoot(entity_name, Heap::kUndefinedValueRootIndex, done);

    // Stop if found the property.
    __ Cmp(entity_name, Operand(name));
    __ B(eq, miss);

    Label good;
    __ JumpIfRoot(entity_name, Heap::kTheHoleValueRootIndex, &good);

    // Check if the entry name is not a unique name.
    __ Ldr(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ Ldrb(entity_name,
            FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ Bind(&good);
  }

  CPURegList spill_list(CPURegister::kRegister, kXRegSizeInBits, 0, 6);
  spill_list.Combine(lr);
  spill_list.Remove(scratch0);  // Scratch registers don't need to be preserved.

  __ PushCPURegList(spill_list);

  __ Ldr(x0, FieldMemOperand(receiver, JSObject::kPropertiesOrHashOffset));
  __ Mov(x1, Operand(name));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  // Move stub return value to scratch0. Note that scratch0 is not included in
  // spill_list and won't be clobbered by PopCPURegList.
  __ Mov(scratch0, x0);
  __ PopCPURegList(spill_list);

  __ Cbz(scratch0, done);
  __ B(miss);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false. That means
  // we cannot call anything that could cause a GC from this stub.
  //
  // Arguments are in x0 and x1:
  //   x0: property dictionary.
  //   x1: the name of the property we are looking for.
  //
  // Return value is in x0 and is zero if lookup failed, non zero otherwise.
  // If the lookup is successful, x2 will contains the index of the entry.

  Register result = x0;
  Register dictionary = x0;
  Register key = x1;
  Register index = x2;
  Register mask = x3;
  Register hash = x4;
  Register undefined = x5;
  Register entry_key = x6;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ Ldrsw(mask, UntagSmiFieldMemOperand(dictionary, kCapacityOffset));
  __ Sub(mask, mask, 1);

  __ Ldr(hash, FieldMemOperand(key, Name::kHashFieldOffset));
  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    // Capacity is smi 2^n.
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ Add(index, hash,
             NameDictionary::GetProbeOffset(i) << Name::kHashShift);
    } else {
      __ Mov(index, hash);
    }
    __ And(index, mask, Operand(index, LSR, Name::kHashShift));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ Add(index, index, Operand(index, LSL, 1));  // index *= 3.

    __ Add(index, dictionary, Operand(index, LSL, kPointerSizeLog2));
    __ Ldr(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ Cmp(entry_key, undefined);
    __ B(eq, &not_in_dictionary);

    // Stop if found the property.
    __ Cmp(entry_key, key);
    __ B(eq, &in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ Ldr(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ Ldrb(entry_key, FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(entry_key, &maybe_in_dictionary);
    }
  }

  __ Bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup, probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ Mov(result, 0);
    __ Ret();
  }

  __ Bind(&in_dictionary);
  __ Mov(result, 1);
  __ Ret();

  __ Bind(&not_in_dictionary);
  __ Mov(result, 0);
  __ Ret();
}


template<class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  ASM_LOCATION("CreateArrayDispatch");
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(masm->isolate(), GetInitialFastElementsKind(), mode);
     __ TailCallStub(&stub);

  } else if (mode == DONT_OVERRIDE) {
    Register kind = x3;
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind candidate_kind = GetFastElementsKindFromSequenceIndex(i);
      // TODO(jbramley): Is this the best way to handle this? Can we make the
      // tail calls conditional, rather than hopping over each one?
      __ CompareAndBranch(kind, candidate_kind, ne, &next);
      T stub(masm->isolate(), candidate_kind);
      __ TailCallStub(&stub);
      __ Bind(&next);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);

  } else {
    UNREACHABLE();
  }
}


// TODO(jbramley): If this needs to be a special case, make it a proper template
// specialization, and not a separate function.
static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  ASM_LOCATION("CreateArrayDispatchOneArgument");
  // x0 - argc
  // x1 - constructor?
  // x2 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // x3 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // sp[0] - last argument

  Register allocation_site = x2;
  Register kind = x3;

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
    // Is the low bit set? If so, the array is holey.
    Label normal_sequence;
    __ Tbnz(kind, 0, &normal_sequence);

    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the slot).
    __ Orr(kind, kind, 1);

    if (FLAG_debug_code) {
      __ Ldr(x10, FieldMemOperand(allocation_site, 0));
      __ JumpIfNotRoot(x10, Heap::kAllocationSiteMapRootIndex,
                       &normal_sequence);
      __ Assert(eq, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store 'kind'
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field; upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ Ldr(x11,
           FieldMemOperand(allocation_site,
                           AllocationSite::kTransitionInfoOrBoilerplateOffset));
    __ Add(x11, x11, Smi::FromInt(kFastElementsKindPackedToHoley));
    __ Str(x11,
           FieldMemOperand(allocation_site,
                           AllocationSite::kTransitionInfoOrBoilerplateOffset));

    __ Bind(&normal_sequence);
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind candidate_kind = GetFastElementsKindFromSequenceIndex(i);
      __ CompareAndBranch(kind, candidate_kind, ne, &next);
      ArraySingleArgumentConstructorStub stub(masm->isolate(), candidate_kind);
      __ TailCallStub(&stub);
      __ Bind(&next);
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
    MacroAssembler* masm,
    AllocationSiteOverrideMode mode) {
  Register argc = x0;
  Label zero_case, n_case;
  __ Cbz(argc, &zero_case);
  __ Cmp(argc, 1);
  __ B(ne, &n_case);

  // One argument.
  CreateArrayDispatchOneArgument(masm, mode);

  __ Bind(&zero_case);
  // No arguments.
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ Bind(&n_case);
  // N arguments.
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  ASM_LOCATION("ArrayConstructorStub::Generate");
  // ----------- S t a t e -------------
  //  -- x0 : argc (only if argument_count() is ANY or MORE_THAN_ONE)
  //  -- x1 : constructor
  //  -- x2 : AllocationSite or undefined
  //  -- x3 : new target
  //  -- sp[0] : last argument
  // -----------------------------------
  Register constructor = x1;
  Register allocation_site = x2;
  Register new_target = x3;

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    Label unexpected_map, map_ok;
    // Initial map for the builtin Array function should be a map.
    __ Ldr(x10, FieldMemOperand(constructor,
                                JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ JumpIfSmi(x10, &unexpected_map);
    __ JumpIfObjectType(x10, x10, x11, MAP_TYPE, &map_ok);
    __ Bind(&unexpected_map);
    __ Abort(kUnexpectedInitialMapForArrayFunction);
    __ Bind(&map_ok);

    // We should either have undefined in the allocation_site register or a
    // valid AllocationSite.
    __ AssertUndefinedOrAllocationSite(allocation_site, x10);
  }

  // Enter the context of the Array function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  Label subclassing;
  __ Cmp(new_target, constructor);
  __ B(ne, &subclassing);

  Register kind = x3;
  Label no_info;
  // Get the elements kind and case on that.
  __ JumpIfRoot(allocation_site, Heap::kUndefinedValueRootIndex, &no_info);

  __ Ldrsw(kind, UntagSmiFieldMemOperand(
                     allocation_site,
                     AllocationSite::kTransitionInfoOrBoilerplateOffset));
  __ And(kind, kind, AllocationSite::ElementsKindBits::kMask);
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ Bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing support.
  __ Bind(&subclassing);
  __ Poke(constructor, Operand(x0, LSL, kPointerSizeLog2));
  __ Add(x0, x0, Operand(3));
  __ Push(new_target, allocation_site);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  Label zero_case, n_case;
  Register argc = x0;

  __ Cbz(argc, &zero_case);
  __ CompareAndBranch(argc, 1, ne, &n_case);

  // One argument.
  if (IsFastPackedElementsKind(kind)) {
    Label packed_case;

    // We might need to create a holey array; look at the first argument.
    __ Peek(x10, 0);
    __ Cbz(x10, &packed_case);

    InternalArraySingleArgumentConstructorStub
        stub1_holey(isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey);

    __ Bind(&packed_case);
  }
  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);

  __ Bind(&zero_case);
  // No arguments.
  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0);

  __ Bind(&n_case);
  // N arguments.
  ArrayNArgumentsConstructorStub stubN(isolate());
  __ TailCallStub(&stubN);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argc
  //  -- x1 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  Register constructor = x1;

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    Label unexpected_map, map_ok;
    // Initial map for the builtin Array function should be a map.
    __ Ldr(x10, FieldMemOperand(constructor,
                                JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ JumpIfSmi(x10, &unexpected_map);
    __ JumpIfObjectType(x10, x10, x11, MAP_TYPE, &map_ok);
    __ Bind(&unexpected_map);
    __ Abort(kUnexpectedInitialMapForArrayFunction);
    __ Bind(&map_ok);
  }

  Register kind = w3;
  // Figure out the right elements kind
  __ Ldr(x10, FieldMemOperand(constructor,
                              JSFunction::kPrototypeOrInitialMapOffset));

  // Retrieve elements_kind from map.
  __ LoadElementsKindFromMap(kind, x10);

  if (FLAG_debug_code) {
    Label done;
    __ Cmp(x3, PACKED_ELEMENTS);
    __ Ccmp(x3, HOLEY_ELEMENTS, ZFlag, ne);
    __ Assert(eq, kInvalidElementsKindForInternalArrayOrInternalPackedArray);
  }

  Label fast_elements_case;
  __ CompareAndBranch(kind, PACKED_ELEMENTS, eq, &fast_elements_case);
  GenerateCase(masm, HOLEY_ELEMENTS);

  __ Bind(&fast_elements_case);
  GenerateCase(masm, PACKED_ELEMENTS);
}

// The number of register that CallApiFunctionAndReturn will need to save on
// the stack. The space for these registers need to be allocated in the
// ExitFrame before calling CallApiFunctionAndReturn.
static const int kCallApiFunctionSpillSpace = 4;


static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return static_cast<int>(ref0.address() - ref1.address());
}


// Calls an API function. Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.
// 'stack_space' is the space to be unwound on exit (includes the call JS
// arguments space and the additional space allocated for the fast call).
// 'spill_offset' is the offset from the stack pointer where
// CallApiFunctionAndReturn can spill registers.
static void CallApiFunctionAndReturn(
    MacroAssembler* masm, Register function_address,
    ExternalReference thunk_ref, int stack_space,
    MemOperand* stack_space_operand, int spill_offset,
    MemOperand return_value_operand, MemOperand* context_restore_operand) {
  ASM_LOCATION("CallApiFunctionAndReturn");
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  DCHECK(function_address.is(x1) || function_address.is(x2));

  Label profiler_disabled;
  Label end_profiler_check;
  __ Mov(x10, ExternalReference::is_profiling_address(isolate));
  __ Ldrb(w10, MemOperand(x10));
  __ Cbz(w10, &profiler_disabled);
  __ Mov(x3, thunk_ref);
  __ B(&end_profiler_check);

  __ Bind(&profiler_disabled);
  __ Mov(x3, function_address);
  __ Bind(&end_profiler_check);

  // Save the callee-save registers we are going to use.
  // TODO(all): Is this necessary? ARM doesn't do it.
  STATIC_ASSERT(kCallApiFunctionSpillSpace == 4);
  __ Poke(x19, (spill_offset + 0) * kXRegSize);
  __ Poke(x20, (spill_offset + 1) * kXRegSize);
  __ Poke(x21, (spill_offset + 2) * kXRegSize);
  __ Poke(x22, (spill_offset + 3) * kXRegSize);

  // Allocate HandleScope in callee-save registers.
  // We will need to restore the HandleScope after the call to the API function,
  // by allocating it in callee-save registers they will be preserved by C code.
  Register handle_scope_base = x22;
  Register next_address_reg = x19;
  Register limit_reg = x20;
  Register level_reg = w21;

  __ Mov(handle_scope_base, next_address);
  __ Ldr(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  __ Ldr(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  __ Ldr(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  __ Add(level_reg, level_reg, 1);
  __ Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ Mov(x0, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate);
  stub.GenerateCall(masm, x3);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ Mov(x0, ExternalReference::isolate_address(isolate));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  __ Ldr(x0, return_value_operand);
  __ Bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ Str(next_address_reg, MemOperand(handle_scope_base, kNextOffset));
  if (__ emit_debug_code()) {
    __ Ldr(w1, MemOperand(handle_scope_base, kLevelOffset));
    __ Cmp(w1, level_reg);
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ Sub(level_reg, level_reg, 1);
  __ Str(level_reg, MemOperand(handle_scope_base, kLevelOffset));
  __ Ldr(x1, MemOperand(handle_scope_base, kLimitOffset));
  __ Cmp(limit_reg, x1);
  __ B(ne, &delete_allocated_handles);

  // Leave the API exit frame.
  __ Bind(&leave_exit_frame);
  // Restore callee-saved registers.
  __ Peek(x19, (spill_offset + 0) * kXRegSize);
  __ Peek(x20, (spill_offset + 1) * kXRegSize);
  __ Peek(x21, (spill_offset + 2) * kXRegSize);
  __ Peek(x22, (spill_offset + 3) * kXRegSize);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ Ldr(cp, *context_restore_operand);
  }

  if (stack_space_operand != NULL) {
    __ Ldr(w2, *stack_space_operand);
  }

  __ LeaveExitFrame(false, x1, !restore_context);

  // Check if the function scheduled an exception.
  __ Mov(x5, ExternalReference::scheduled_exception_address(isolate));
  __ Ldr(x5, MemOperand(x5));
  __ JumpIfNotRoot(x5, Heap::kTheHoleValueRootIndex,
                   &promote_scheduled_exception);

  if (stack_space_operand != NULL) {
    __ Drop(x2, 1);
  } else {
    __ Drop(stack_space);
  }
  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ Bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ Bind(&delete_allocated_handles);
  __ Str(limit_reg, MemOperand(handle_scope_base, kLimitOffset));
  // Save the return value in a callee-save register.
  Register saved_result = x19;
  __ Mov(saved_result, x0);
  __ Mov(x0, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ Mov(x0, saved_result);
  __ B(&leave_exit_frame);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                  : callee
  //  -- x4                  : call_data
  //  -- x2                  : holder
  //  -- x1                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1) * 8]  : first argument
  //  -- sp[argc * 8]        : receiver
  //  -- sp[(argc + 1) * 8]  : accessor_holder
  // -----------------------------------

  Register callee = x0;
  Register call_data = x4;
  Register holder = x2;
  Register api_function_address = x1;
  Register context = cp;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kArgsLength == 8);
  STATIC_ASSERT(FCA::kNewTargetIndex == 7);
  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);

  // new target
  __ PushRoot(Heap::kUndefinedValueRootIndex);

  // context, callee and call data.
  __ Push(context, callee, call_data);

  Register scratch = call_data;
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  Register isolate_reg = x5;
  __ Mov(isolate_reg, ExternalReference::isolate_address(masm->isolate()));

  // FunctionCallbackArguments:
  //    return value, return value default, isolate, holder.
  __ Push(scratch, scratch, isolate_reg, holder);

  // Enter a new context
  if (is_lazy()) {
    // ----------- S t a t e -------------------------------------
    //  -- sp[0]                                 : holder
    //  -- ...
    //  -- sp[(FCA::kArgsLength - 1) * 8]        : new_target
    //  -- sp[FCA::kArgsLength * 8]              : last argument
    //  -- ...
    //  -- sp[(FCA::kArgsLength + argc - 1) * 8] : first argument
    //  -- sp[(FCA::kArgsLength + argc) * 8]     : receiver
    //  -- sp[(FCA::kArgsLength + argc + 1) * 8] : accessor_holder
    // -----------------------------------------------------------

    // Load context from accessor_holder
    Register accessor_holder = context;
    Register scratch2 = callee;
    __ Ldr(accessor_holder,
           MemOperand(__ StackPointer(),
                      (FCA::kArgsLength + 1 + argc()) * kPointerSize));
    // Look for the constructor if |accessor_holder| is not a function.
    Label skip_looking_for_constructor;
    __ Ldr(scratch, FieldMemOperand(accessor_holder, HeapObject::kMapOffset));
    __ Ldrb(scratch2, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ Tst(scratch2, Operand(1 << Map::kIsConstructor));
    __ B(ne, &skip_looking_for_constructor);
    __ GetMapConstructor(context, scratch, scratch, scratch2);
    __ bind(&skip_looking_for_constructor);
    __ Ldr(context, FieldMemOperand(context, JSFunction::kContextOffset));
  } else {
    // Load context from callee
    __ Ldr(context, FieldMemOperand(callee, JSFunction::kContextOffset));
  }

  // Prepare arguments.
  Register args = x6;
  __ Mov(args, masm->StackPointer());

  // Allocate the v8::Arguments structure in the arguments' space, since it's
  // not controlled by GC.
  const int kApiStackSpace = 3;

  // Allocate space for CallApiFunctionAndReturn can store some scratch
  // registeres on the stack.
  const int kCallApiFunctionSpillSpace = 4;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, x10, kApiStackSpace + kCallApiFunctionSpillSpace);

  DCHECK(!AreAliased(x0, api_function_address));
  // x0 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ Add(x0, masm->StackPointer(), 1 * kPointerSize);
  // FunctionCallbackInfo::implicit_args_ and FunctionCallbackInfo::values_
  __ Add(x10, args, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ Stp(args, x10, MemOperand(x0, 0 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ Mov(x10, argc());
  __ Str(x10, MemOperand(x0, 2 * kPointerSize));

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  MemOperand context_restore_operand(
      fp, (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument
  int return_value_offset = 0;
  if (is_store()) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);
  const int stack_space = argc() + FCA::kArgsLength + 2;
  MemOperand* stack_space_operand = nullptr;

  const int spill_offset = 1 + kApiStackSpace;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_operand, spill_offset,
                           return_value_operand, &context_restore_operand);
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
  Register scratch = x4;
  Register scratch2 = x5;
  Register scratch3 = x6;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  __ Push(receiver);

  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ Mov(scratch2, Operand(ExternalReference::isolate_address(isolate())));
  __ Ldr(scratch3, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ Push(scratch3, scratch, scratch, scratch2, holder);
  __ Push(Smi::kZero);  // should_throw_on_error -> false
  __ Ldr(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ Push(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ Mov(x0, masm->StackPointer());  // x0 = Handle<Name>
  __ Add(x1, x0, 1 * kPointerSize);  // x1 = v8::PCI::args_

  const int kApiStackSpace = 1;

  // Allocate space for CallApiFunctionAndReturn can store some scratch
  // registeres on the stack.
  const int kCallApiFunctionSpillSpace = 4;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, x10, kApiStackSpace + kCallApiFunctionSpillSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ Poke(x1, 1 * kPointerSize);
  __ Add(x1, masm->StackPointer(), 1 * kPointerSize);
  // x1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  Register api_function_address = x2;
  __ Ldr(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ Ldr(api_function_address,
         FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  const int spill_offset = 1 + kApiStackSpace;
  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, NULL, spill_offset,
                           return_value_operand, NULL);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
