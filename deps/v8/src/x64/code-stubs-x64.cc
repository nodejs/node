// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"
#include "src/x64/code-stubs-x64.h"

namespace v8 {
namespace internal {


static void InitializeArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(rax, deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  }
}


static void InitializeInternalArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kInternalArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(rax, deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  }
}


void ArrayNoArgumentConstructorStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(isolate(), descriptor, 0);
}


void ArraySingleArgumentConstructorStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(isolate(), descriptor, 1);
}


void ArrayNArgumentsConstructorStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(isolate(), descriptor, -1);
}


void InternalArrayNoArgumentConstructorStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(isolate(), descriptor, 0);
}


void InternalArraySingleArgumentConstructorStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(isolate(), descriptor, 1);
}


void InternalArrayNArgumentsConstructorStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(isolate(), descriptor, -1);
}


#define __ ACCESS_MASM(masm)


void HydrogenCodeStub::GenerateLightweightMiss(MacroAssembler* masm,
                                               ExternalReference miss) {
  // Update the static counter each time a new code stub is generated.
  isolate()->counters()->code_stubs()->Increment();

  CallInterfaceDescriptor descriptor = GetCallInterfaceDescriptor();
  int param_count = descriptor.GetRegisterParameterCount();
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    DCHECK(param_count == 0 ||
           rax.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ Push(descriptor.GetRegisterParameter(i));
    }
    __ CallExternalReference(miss, param_count);
  }

  __ Ret();
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  __ PushCallerSaved(save_doubles() ? kSaveFPRegs : kDontSaveFPRegs);
  const int argument_count = 1;
  __ PrepareCallCFunction(argument_count);
  __ LoadAddress(arg_reg_1,
                 ExternalReference::isolate_address(isolate()));

  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()),
      argument_count);
  __ PopCallerSaved(save_doubles() ? kSaveFPRegs : kDontSaveFPRegs);
  __ ret(0);
}


class FloatingPointHelper : public AllStatic {
 public:
  enum ConvertUndefined {
    CONVERT_UNDEFINED_TO_ZERO,
    BAILOUT_ON_UNDEFINED
  };
  // Load the operands from rdx and rax into xmm0 and xmm1, as doubles.
  // If the operands are not both numbers, jump to not_numbers.
  // Leaves rdx and rax unchanged.  SmiOperands assumes both are smis.
  // NumberOperands assumes both are smis or heap numbers.
  static void LoadSSE2UnknownOperands(MacroAssembler* masm,
                                      Label* not_numbers);
};


void DoubleToIStub::Generate(MacroAssembler* masm) {
    Register input_reg = this->source();
    Register final_result_reg = this->destination();
    DCHECK(is_truncating());

    Label check_negative, process_64_bits, done;

    int double_offset = offset();

    // Account for return address and saved regs if input is rsp.
    if (input_reg.is(rsp)) double_offset += 3 * kRegisterSize;

    MemOperand mantissa_operand(MemOperand(input_reg, double_offset));
    MemOperand exponent_operand(MemOperand(input_reg,
                                           double_offset + kDoubleSize / 2));

    Register scratch1;
    Register scratch_candidates[3] = { rbx, rdx, rdi };
    for (int i = 0; i < 3; i++) {
      scratch1 = scratch_candidates[i];
      if (!final_result_reg.is(scratch1) && !input_reg.is(scratch1)) break;
    }

    // Since we must use rcx for shifts below, use some other register (rax)
    // to calculate the result if ecx is the requested return register.
    Register result_reg = final_result_reg.is(rcx) ? rax : final_result_reg;
    // Save ecx if it isn't the return register and therefore volatile, or if it
    // is the return register, then save the temp register we use in its stead
    // for the result.
    Register save_reg = final_result_reg.is(rcx) ? rax : rcx;
    __ pushq(scratch1);
    __ pushq(save_reg);

    bool stash_exponent_copy = !input_reg.is(rsp);
    __ movl(scratch1, mantissa_operand);
    __ Movsd(xmm0, mantissa_operand);
    __ movl(rcx, exponent_operand);
    if (stash_exponent_copy) __ pushq(rcx);

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
    __ Cvttsd2siq(result_reg, xmm0);
    __ jmp(&done, Label::kNear);

    // If the double was negative, negate the integer result.
    __ bind(&check_negative);
    __ movl(result_reg, scratch1);
    __ negl(result_reg);
    if (stash_exponent_copy) {
        __ cmpl(MemOperand(rsp, 0), Immediate(0));
    } else {
        __ cmpl(exponent_operand, Immediate(0));
    }
    __ cmovl(greater, result_reg, scratch1);

    // Restore registers
    __ bind(&done);
    if (stash_exponent_copy) {
        __ addp(rsp, Immediate(kDoubleSize));
    }
    if (!final_result_reg.is(result_reg)) {
        DCHECK(final_result_reg.is(rcx));
        __ movl(final_result_reg, result_reg);
    }
    __ popq(save_reg);
    __ popq(scratch1);
    __ ret(0);
}


void FloatingPointHelper::LoadSSE2UnknownOperands(MacroAssembler* masm,
                                                  Label* not_numbers) {
  Label load_smi_rdx, load_nonsmi_rax, load_smi_rax, load_float_rax, done;
  // Load operand in rdx into xmm0, or branch to not_numbers.
  __ LoadRoot(rcx, Heap::kHeapNumberMapRootIndex);
  __ JumpIfSmi(rdx, &load_smi_rdx);
  __ cmpp(FieldOperand(rdx, HeapObject::kMapOffset), rcx);
  __ j(not_equal, not_numbers);  // Argument in rdx is not a number.
  __ Movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  // Load operand in rax into xmm1, or branch to not_numbers.
  __ JumpIfSmi(rax, &load_smi_rax);

  __ bind(&load_nonsmi_rax);
  __ cmpp(FieldOperand(rax, HeapObject::kMapOffset), rcx);
  __ j(not_equal, not_numbers);
  __ Movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_rdx);
  __ SmiToInteger32(kScratchRegister, rdx);
  __ Cvtlsi2sd(xmm0, kScratchRegister);
  __ JumpIfNotSmi(rax, &load_nonsmi_rax);

  __ bind(&load_smi_rax);
  __ SmiToInteger32(kScratchRegister, rax);
  __ Cvtlsi2sd(xmm1, kScratchRegister);
  __ bind(&done);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(rdx));
  const Register base = rax;
  const Register scratch = rcx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ movp(scratch, Immediate(1));
  __ Cvtlsi2sd(double_result, scratch);

  if (exponent_type() == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack.
    StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER);
    __ movp(base, args.GetArgumentOperand(0));
    __ movp(exponent, args.GetArgumentOperand(1));
    __ JumpIfSmi(base, &base_is_smi, Label::kNear);
    __ CompareRoot(FieldOperand(base, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);

    __ Movsd(double_base, FieldOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ SmiToInteger32(base, base);
    __ Cvtlsi2sd(double_base, base);
    __ bind(&unpack_exponent);

    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiToInteger32(exponent, exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ CompareRoot(FieldOperand(exponent, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);
    __ Movsd(double_exponent, FieldOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type() == TAGGED) {
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

    if (exponent_type() == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label continue_sqrt, continue_rsqrt, not_plus_half;
      // Test for 0.5.
      // Load double_scratch with 0.5.
      __ movq(scratch, V8_UINT64_C(0x3FE0000000000000));
      __ Movq(double_scratch, scratch);
      // Already ruled out NaNs for exponent.
      __ Ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &not_plus_half, Label::kNear);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      // According to IEEE-754, double-precision -Infinity has the highest
      // 12 bits set and the lowest 52 bits cleared.
      __ movq(scratch, V8_UINT64_C(0xFFF0000000000000));
      __ Movq(double_scratch, scratch);
      __ Ucomisd(double_scratch, double_base);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_sqrt, Label::kNear);
      __ j(carry, &continue_sqrt, Label::kNear);

      // Set result to Infinity in the special case.
      __ Xorpd(double_result, double_result);
      __ Subsd(double_result, double_scratch);
      __ jmp(&done);

      __ bind(&continue_sqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ Xorpd(double_scratch, double_scratch);
      __ Addsd(double_scratch, double_base);  // Convert -0 to 0.
      __ Sqrtsd(double_result, double_scratch);
      __ jmp(&done);

      // Test for -0.5.
      __ bind(&not_plus_half);
      // Load double_scratch with -0.5 by substracting 1.
      __ Subsd(double_scratch, double_result);
      // Already ruled out NaNs for exponent.
      __ Ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &fast_power, Label::kNear);

      // Calculates reciprocal of square root of base.  Check for the special
      // case of Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      // According to IEEE-754, double-precision -Infinity has the highest
      // 12 bits set and the lowest 52 bits cleared.
      __ movq(scratch, V8_UINT64_C(0xFFF0000000000000));
      __ Movq(double_scratch, scratch);
      __ Ucomisd(double_scratch, double_base);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_rsqrt, Label::kNear);
      __ j(carry, &continue_rsqrt, Label::kNear);

      // Set result to 0 in the special case.
      __ Xorpd(double_result, double_result);
      __ jmp(&done);

      __ bind(&continue_rsqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ Xorpd(double_exponent, double_exponent);
      __ Addsd(double_exponent, double_base);  // Convert -0 to +0.
      __ Sqrtsd(double_exponent, double_exponent);
      __ Divsd(double_result, double_exponent);
      __ jmp(&done);
    }

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
  if (exponent_type() == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMathPowRT);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in rax.
    __ bind(&done);
    __ AllocateHeapNumber(rax, rcx, &call_runtime);
    __ Movsd(FieldOperand(rax, HeapNumber::kValueOffset), double_result);
    __ ret(2 * kPointerSize);
  } else {
    __ bind(&call_runtime);
    // Move base to the correct argument register.  Exponent is already in xmm1.
    __ Movsd(xmm0, double_base);
    DCHECK(double_exponent.is(xmm1));
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(2);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 2);
    }
    // Return value is in xmm0.
    __ Movsd(double_result, xmm0);

    __ bind(&done);
    __ ret(0);
  }
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // Ensure that the vector and slot registers won't be clobbered before
  // calling the miss handler.
  DCHECK(!AreAliased(r8, r9, LoadWithVectorDescriptor::VectorRegister(),
                     LoadDescriptor::SlotRegister()));

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, r8,
                                                          r9, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void LoadIndexedInterceptorStub::Generate(MacroAssembler* masm) {
  // Return address is on the stack.
  Label slow;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();
  Register scratch = rax;
  DCHECK(!scratch.is(receiver) && !scratch.is(key));

  // Check that the key is an array index, that is Uint32.
  STATIC_ASSERT(kSmiValueSize <= 32);
  __ JumpUnlessNonNegativeSmi(key, &slow);

  // Everything is fine, call runtime.
  __ PopReturnAddressTo(scratch);
  __ Push(receiver);  // receiver
  __ Push(key);       // key
  __ PushReturnAddressFrom(scratch);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kLoadElementWithInterceptor);

  __ bind(&slow);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::KEYED_LOAD_IC));
}


void LoadIndexedStringStub::Generate(MacroAssembler* masm) {
  // Return address is on the stack.
  Label miss;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register index = LoadDescriptor::NameRegister();
  Register scratch = rdi;
  Register result = rax;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  DCHECK(!scratch.is(LoadWithVectorDescriptor::VectorRegister()) &&
         result.is(LoadDescriptor::SlotRegister()));

  // StringCharAtGenerator doesn't use the result register until it's passed
  // the different miss possibilities. If it did, we would have a conflict
  // when FLAG_vector_ics is true.
  StringCharAtGenerator char_at_generator(receiver, index, scratch, result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &miss,  // When index out of range.
                                          STRING_INDEX_IS_ARRAY_INDEX,
                                          RECEIVER_IS_STRING);
  char_at_generator.GenerateFast(masm);
  __ ret(0);

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, PART_OF_IC_HANDLER, call_helper);

  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::KEYED_LOAD_IC));
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec);
#else  // V8_INTERPRETED_REGEXP

  // Stack frame on entry.
  //  rsp[0]  : return address
  //  rsp[8]  : last_match_info (expected JSArray)
  //  rsp[16] : previous index
  //  rsp[24] : subject string
  //  rsp[32] : JSRegExp object

  enum RegExpExecStubArgumentIndices {
    JS_REG_EXP_OBJECT_ARGUMENT_INDEX,
    SUBJECT_STRING_ARGUMENT_INDEX,
    PREVIOUS_INDEX_ARGUMENT_INDEX,
    LAST_MATCH_INFO_ARGUMENT_INDEX,
    REG_EXP_EXEC_ARGUMENT_COUNT
  };

  StackArgumentsAccessor args(rsp, REG_EXP_EXEC_ARGUMENT_COUNT,
                              ARGUMENTS_DONT_CONTAIN_RECEIVER);
  Label runtime;
  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ Load(kScratchRegister, address_of_regexp_stack_memory_size);
  __ testp(kScratchRegister, kScratchRegister);
  __ j(zero, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ movp(rax, args.GetArgumentOperand(JS_REG_EXP_OBJECT_ARGUMENT_INDEX));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ movp(rax, FieldOperand(rax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    Condition is_smi = masm->CheckSmi(rax);
    __ Check(NegateCondition(is_smi),
        kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CmpObjectType(rax, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Check(equal, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // rax: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ SmiToInteger32(rbx, FieldOperand(rax, JSRegExp::kDataTagOffset));
  __ cmpl(rbx, Immediate(JSRegExp::IRREGEXP));
  __ j(not_equal, &runtime);

  // rax: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ SmiToInteger32(rdx,
                    FieldOperand(rax, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or              number_of_captures <= offsets vector size / 2 - 1
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmpl(rdx, Immediate(Isolate::kJSRegexpStaticOffsetsVectorSize / 2 - 1));
  __ j(above, &runtime);

  // Reset offset for possibly sliced string.
  __ Set(r14, 0);
  __ movp(rdi, args.GetArgumentOperand(SUBJECT_STRING_ARGUMENT_INDEX));
  __ JumpIfSmi(rdi, &runtime);
  __ movp(r15, rdi);  // Make a copy of the original subject string.
  // rax: RegExp data (FixedArray)
  // rdi: subject string
  // r15: subject string
  // Handle subject string according to its encoding and representation:
  // (1) Sequential two byte?  If yes, go to (9).
  // (2) Sequential one byte?  If yes, go to (5).
  // (3) Sequential or cons?  If not, go to (6).
  // (4) Cons string.  If the string is flat, replace subject with first string
  //     and go to (1). Otherwise bail out to runtime.
  // (5) One byte sequential.  Load regexp code for one byte.
  // (E) Carry on.
  /// [...]

  // Deferred code at the end of the stub:
  // (6) Long external string?  If not, go to (10).
  // (7) External string.  Make it, offset-wise, look like a sequential string.
  // (8) Is the external string one byte?  If yes, go to (5).
  // (9) Two byte sequential.  Load regexp code for two byte. Go to (E).
  // (10) Short external string or not a string?  If yes, bail out to runtime.
  // (11) Sliced string.  Replace subject with parent. Go to (1).

  Label seq_one_byte_string /* 5 */, seq_two_byte_string /* 9 */,
      external_string /* 7 */, check_underlying /* 1 */,
      not_seq_nor_cons /* 6 */, check_code /* E */, not_long_external /* 10 */;

  __ bind(&check_underlying);
  __ movp(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));

  // (1) Sequential two byte?  If yes, go to (9).
  __ andb(rbx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kStringEncodingMask |
                         kShortExternalStringMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).

  // (2) Sequential one byte?  If yes, go to (5).
  // Any other sequential string must be one byte.
  __ andb(rbx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kShortExternalStringMask));
  __ j(zero, &seq_one_byte_string, Label::kNear);  // Go to (5).

  // (3) Sequential or cons?  If not, go to (6).
  // We check whether the subject string is a cons, since sequential strings
  // have already been covered.
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  __ cmpp(rbx, Immediate(kExternalStringTag));
  __ j(greater_equal, &not_seq_nor_cons);  // Go to (6).

  // (4) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ CompareRoot(FieldOperand(rdi, ConsString::kSecondOffset),
                 Heap::kempty_stringRootIndex);
  __ j(not_equal, &runtime);
  __ movp(rdi, FieldOperand(rdi, ConsString::kFirstOffset));
  __ jmp(&check_underlying);

  // (5) One byte sequential.  Load regexp code for one byte.
  __ bind(&seq_one_byte_string);
  // rax: RegExp data (FixedArray)
  __ movp(r11, FieldOperand(rax, JSRegExp::kDataOneByteCodeOffset));
  __ Set(rcx, 1);  // Type is one byte.

  // (E) Carry on.  String handling is done.
  __ bind(&check_code);
  // r11: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // smi (code flushing support)
  __ JumpIfSmi(r11, &runtime);

  // rdi: sequential subject string (or look-alike, external string)
  // r15: original subject string
  // rcx: encoding of subject string (1 if one_byte, 0 if two_byte);
  // r11: code
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  // We have to use r15 instead of rdi to load the length because rdi might
  // have been only made to look like a sequential string when it actually
  // is an external string.
  __ movp(rbx, args.GetArgumentOperand(PREVIOUS_INDEX_ARGUMENT_INDEX));
  __ JumpIfNotSmi(rbx, &runtime);
  __ SmiCompare(rbx, FieldOperand(r15, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ SmiToInteger64(rbx, rbx);

  // rdi: subject string
  // rbx: previous index
  // rcx: encoding of subject string (1 if one_byte 0 if two_byte);
  // r11: code
  // All checks done. Now push arguments for native regexp code.
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->regexp_entry_native(), 1);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 9;
  int argument_slots_on_stack =
      masm->ArgumentStackSlotsForCFunctionCall(kRegExpExecuteArguments);
  __ EnterApiExitFrame(argument_slots_on_stack);

  // Argument 9: Pass current isolate address.
  __ LoadAddress(kScratchRegister,
                 ExternalReference::isolate_address(isolate()));
  __ movq(Operand(rsp, (argument_slots_on_stack - 1) * kRegisterSize),
          kScratchRegister);

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ movq(Operand(rsp, (argument_slots_on_stack - 2) * kRegisterSize),
          Immediate(1));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ Move(kScratchRegister, address_of_regexp_stack_memory_address);
  __ movp(r9, Operand(kScratchRegister, 0));
  __ Move(kScratchRegister, address_of_regexp_stack_memory_size);
  __ addp(r9, Operand(kScratchRegister, 0));
  __ movq(Operand(rsp, (argument_slots_on_stack - 3) * kRegisterSize), r9);

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  // Argument 6 is passed in r9 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 4) * kRegisterSize),
          Immediate(0));
#else
  __ Set(r9, 0);
#endif

  // Argument 5: static offsets vector buffer.
  __ LoadAddress(
      r8, ExternalReference::address_of_static_offsets_vector(isolate()));
  // Argument 5 passed in r8 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 5) * kRegisterSize), r8);
#endif

  // rdi: subject string
  // rbx: previous index
  // rcx: encoding of subject string (1 if one_byte 0 if two_byte);
  // r11: code
  // r14: slice offset
  // r15: original subject string

  // Argument 2: Previous index.
  __ movp(arg_reg_2, rbx);

  // Argument 4: End of string data
  // Argument 3: Start of string data
  Label setup_two_byte, setup_rest, got_length, length_not_from_slice;
  // Prepare start and end index of the input.
  // Load the length from the original sliced string if that is the case.
  __ addp(rbx, r14);
  __ SmiToInteger32(arg_reg_3, FieldOperand(r15, String::kLengthOffset));
  __ addp(r14, arg_reg_3);  // Using arg3 as scratch.

  // rbx: start index of the input
  // r14: end index of the input
  // r15: original subject string
  __ testb(rcx, rcx);  // Last use of rcx as encoding of subject string.
  __ j(zero, &setup_two_byte, Label::kNear);
  __ leap(arg_reg_4,
         FieldOperand(rdi, r14, times_1, SeqOneByteString::kHeaderSize));
  __ leap(arg_reg_3,
         FieldOperand(rdi, rbx, times_1, SeqOneByteString::kHeaderSize));
  __ jmp(&setup_rest, Label::kNear);
  __ bind(&setup_two_byte);
  __ leap(arg_reg_4,
         FieldOperand(rdi, r14, times_2, SeqTwoByteString::kHeaderSize));
  __ leap(arg_reg_3,
         FieldOperand(rdi, rbx, times_2, SeqTwoByteString::kHeaderSize));
  __ bind(&setup_rest);

  // Argument 1: Original subject string.
  // The original subject is in the previous stack frame. Therefore we have to
  // use rbp, which points exactly to one pointer size below the previous rsp.
  // (Because creating a new stack frame pushes the previous rbp onto the stack
  // and thereby moves up rsp by one kPointerSize.)
  __ movp(arg_reg_1, r15);

  // Locate the code entry and call it.
  __ addp(r11, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(r11);

  __ LeaveApiExitFrame(true);

  // Check the result.
  Label success;
  Label exception;
  __ cmpl(rax, Immediate(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ j(equal, &success, Label::kNear);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::EXCEPTION));
  __ j(equal, &exception);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::FAILURE));
  // If none of the above, it can only be retry.
  // Handle that in the runtime system.
  __ j(not_equal, &runtime);

  // For failure return null.
  __ LoadRoot(rax, Heap::kNullValueRootIndex);
  __ ret(REG_EXP_EXEC_ARGUMENT_COUNT * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ movp(rax, args.GetArgumentOperand(JS_REG_EXP_OBJECT_ARGUMENT_INDEX));
  __ movp(rcx, FieldOperand(rax, JSRegExp::kDataOffset));
  __ SmiToInteger32(rax,
                    FieldOperand(rcx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ leal(rdx, Operand(rax, rax, times_1, 2));

  // rdx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ movp(r15, args.GetArgumentOperand(LAST_MATCH_INFO_ARGUMENT_INDEX));
  __ JumpIfSmi(r15, &runtime);
  __ CmpObjectType(r15, JS_ARRAY_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ movp(rbx, FieldOperand(r15, JSArray::kElementsOffset));
  __ movp(rax, FieldOperand(rbx, HeapObject::kMapOffset));
  __ CompareRoot(rax, Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information. Ensure no overflow in add.
  STATIC_ASSERT(FixedArray::kMaxLength < kMaxInt - FixedArray::kLengthOffset);
  __ SmiToInteger32(rax, FieldOperand(rbx, FixedArray::kLengthOffset));
  __ subl(rax, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmpl(rdx, rax);
  __ j(greater, &runtime);

  // rbx: last_match_info backing store (FixedArray)
  // rdx: number of capture registers
  // Store the capture count.
  __ Integer32ToSmi(kScratchRegister, rdx);
  __ movp(FieldOperand(rbx, RegExpImpl::kLastCaptureCountOffset),
          kScratchRegister);
  // Store last subject and last input.
  __ movp(rax, args.GetArgumentOperand(SUBJECT_STRING_ARGUMENT_INDEX));
  __ movp(FieldOperand(rbx, RegExpImpl::kLastSubjectOffset), rax);
  __ movp(rcx, rax);
  __ RecordWriteField(rbx,
                      RegExpImpl::kLastSubjectOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs);
  __ movp(rax, rcx);
  __ movp(FieldOperand(rbx, RegExpImpl::kLastInputOffset), rax);
  __ RecordWriteField(rbx,
                      RegExpImpl::kLastInputOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  __ LoadAddress(
      rcx, ExternalReference::address_of_static_offsets_vector(isolate()));

  // rbx: last_match_info backing store (FixedArray)
  // rcx: offsets vector
  // rdx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ subp(rdx, Immediate(1));
  __ j(negative, &done, Label::kNear);
  // Read the value from the static offsets vector buffer and make it a smi.
  __ movl(rdi, Operand(rcx, rdx, times_int_size, 0));
  __ Integer32ToSmi(rdi, rdi);
  // Store the smi value in the last match info.
  __ movp(FieldOperand(rbx,
                       rdx,
                       times_pointer_size,
                       RegExpImpl::kFirstCaptureOffset),
          rdi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ movp(rax, r15);
  __ ret(REG_EXP_EXEC_ARGUMENT_COUNT * kPointerSize);

  __ bind(&exception);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, isolate());
  Operand pending_exception_operand =
      masm->ExternalOperand(pending_exception_address, rbx);
  __ movp(rax, pending_exception_operand);
  __ LoadRoot(rdx, Heap::kTheHoleValueRootIndex);
  __ cmpp(rax, rdx);
  __ j(equal, &runtime);

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec);

  // Deferred code for string handling.
  // (6) Long external string?  If not, go to (10).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set from (3).
  __ j(greater, &not_long_external, Label::kNear);  // Go to (10).

  // (7) External string.  Short external strings have been ruled out.
  __ bind(&external_string);
  __ movp(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ testb(rbx, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  __ movp(rdi, FieldOperand(rdi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ subp(rdi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // (8) Is the external string one byte?  If yes, go to (5).
  __ testb(rbx, Immediate(kStringEncodingMask));
  __ j(not_zero, &seq_one_byte_string);  // Go to (5).

  // rdi: subject string (flat two-byte)
  // rax: RegExp data (FixedArray)
  // (9) Two byte sequential.  Load regexp code for two byte.  Go to (E).
  __ bind(&seq_two_byte_string);
  __ movp(r11, FieldOperand(rax, JSRegExp::kDataUC16CodeOffset));
  __ Set(rcx, 0);  // Type is two byte.
  __ jmp(&check_code);  // Go to (E).

  // (10) Not a string or a short external string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  // Catch non-string subject or short external string.
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ testb(rbx, Immediate(kIsNotStringMask | kShortExternalStringMask));
  __ j(not_zero, &runtime);

  // (11) Sliced string.  Replace subject with parent. Go to (1).
  // Load offset into r14 and replace subject string with parent.
  __ SmiToInteger32(r14, FieldOperand(rdi, SlicedString::kOffsetOffset));
  __ movp(rdi, FieldOperand(rdi, SlicedString::kParentOffset));
  __ jmp(&check_underlying);
#endif  // V8_INTERPRETED_REGEXP
}


static int NegativeComparisonResult(Condition cc) {
  DCHECK(cc != equal);
  DCHECK((cc == less) || (cc == less_equal)
      || (cc == greater) || (cc == greater_equal));
  return (cc == greater || cc == greater_equal) ? LESS : GREATER;
}


static void CheckInputType(MacroAssembler* masm, Register input,
                           CompareICState::State expected, Label* fail) {
  Label ok;
  if (expected == CompareICState::SMI) {
    __ JumpIfNotSmi(input, fail);
  } else if (expected == CompareICState::NUMBER) {
    __ JumpIfSmi(input, &ok);
    __ CompareMap(input, masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, fail);
  }
  // We could be strict about internalized/non-internalized here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ bind(&ok);
}


static void BranchIfNotInternalizedString(MacroAssembler* masm,
                                          Label* label,
                                          Register object,
                                          Register scratch) {
  __ JumpIfSmi(object, label);
  __ movp(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzxbp(scratch,
             FieldOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ testb(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, label);
}


void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Label runtime_call, check_unequal_objects, done;
  Condition cc = GetCondition();
  Factory* factory = isolate()->factory();

  Label miss;
  CheckInputType(masm, rdx, left(), &miss);
  CheckInputType(masm, rax, right(), &miss);

  // Compare two smis.
  Label non_smi, smi_done;
  __ JumpIfNotBothSmi(rax, rdx, &non_smi);
  __ subp(rdx, rax);
  __ j(no_overflow, &smi_done);
  __ notp(rdx);  // Correct sign in case of overflow. rdx cannot be 0 here.
  __ bind(&smi_done);
  __ movp(rax, rdx);
  __ ret(0);
  __ bind(&non_smi);

  // The compare stub returns a positive, negative, or zero 64-bit integer
  // value in rax, corresponding to result of comparing the two inputs.
  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Two identical objects are equal unless they are both NaN or undefined.
  {
    Label not_identical;
    __ cmpp(rax, rdx);
    __ j(not_equal, &not_identical, Label::kNear);

    if (cc != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
      Label check_for_nan;
      __ j(not_equal, &check_for_nan, Label::kNear);
      __ Set(rax, NegativeComparisonResult(cc));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
    // so we do the second best thing - test it ourselves.
    Label heap_number;
    // If it's not a heap number, then return equal for (in)equality operator.
    __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(equal, &heap_number, Label::kNear);
    if (cc != equal) {
      __ movp(rcx, FieldOperand(rax, HeapObject::kMapOffset));
      __ movzxbl(rcx, FieldOperand(rcx, Map::kInstanceTypeOffset));
      // Call runtime on identical objects.  Otherwise return equal.
      __ cmpb(rcx, Immediate(static_cast<uint8_t>(FIRST_JS_RECEIVER_TYPE)));
      __ j(above_equal, &runtime_call, Label::kFar);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ cmpb(rcx, Immediate(static_cast<uint8_t>(SYMBOL_TYPE)));
      __ j(equal, &runtime_call, Label::kFar);
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ cmpb(rcx, Immediate(static_cast<uint8_t>(SIMD128_VALUE_TYPE)));
      __ j(equal, &runtime_call, Label::kFar);
    }
    __ Set(rax, EQUAL);
    __ ret(0);

    __ bind(&heap_number);
    // It is a heap number, so return  equal if it's not NaN.
    // For NaN, return 1 for every condition except greater and
    // greater-equal.  Return -1 for them, so the comparison yields
    // false for all conditions except not-equal.
    __ Set(rax, EQUAL);
    __ Movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
    __ Ucomisd(xmm0, xmm0);
    __ setcc(parity_even, rax);
    // rax is 0 for equal non-NaN heapnumbers, 1 for NaNs.
    if (cc == greater_equal || cc == greater) {
      __ negp(rax);
    }
    __ ret(0);

    __ bind(&not_identical);
  }

  if (cc == equal) {  // Both strict and non-strict.
    Label slow;  // Fallthrough label.

    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    if (strict()) {
      // If either is a Smi (we know that not both are), then they can only
      // be equal if the other is a HeapNumber. If so, use the slow case.
      {
        Label not_smis;
        __ SelectNonSmi(rbx, rax, rdx, &not_smis);

        // Check if the non-smi operand is a heap number.
        __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
               factory->heap_number_map());
        // If heap number, handle it in the slow case.
        __ j(equal, &slow);
        // Return non-equal.  ebx (the lower half of rbx) is not zero.
        __ movp(rax, rbx);
        __ ret(0);

        __ bind(&not_smis);
      }

      // If either operand is a JSObject or an oddball value, then they are not
      // equal since their pointers are different
      // There is no test for undetectability in strict equality.

      // If the first object is a JS object, we have done pointer comparison.
      STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
      Label first_non_object;
      __ CmpObjectType(rax, FIRST_JS_RECEIVER_TYPE, rcx);
      __ j(below, &first_non_object, Label::kNear);
      // Return non-zero (rax (not rax) is not zero)
      Label return_not_equal;
      STATIC_ASSERT(kHeapObjectTag != 0);
      __ bind(&return_not_equal);
      __ ret(0);

      __ bind(&first_non_object);
      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      __ CmpObjectType(rdx, FIRST_JS_RECEIVER_TYPE, rcx);
      __ j(above_equal, &return_not_equal);

      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      // Fall through to the general case.
    }
    __ bind(&slow);
  }

  // Generate the number comparison code.
  Label non_number_comparison;
  Label unordered;
  FloatingPointHelper::LoadSSE2UnknownOperands(masm, &non_number_comparison);
  __ xorl(rax, rax);
  __ xorl(rcx, rcx);
  __ Ucomisd(xmm0, xmm1);

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);
  // Return a result of -1, 0, or 1, based on EFLAGS.
  __ setcc(above, rax);
  __ setcc(below, rcx);
  __ subp(rax, rcx);
  __ ret(0);

  // If one of the numbers was NaN, then the result is always false.
  // The cc is never not-equal.
  __ bind(&unordered);
  DCHECK(cc != not_equal);
  if (cc == less || cc == less_equal) {
    __ Set(rax, 1);
  } else {
    __ Set(rax, -1);
  }
  __ ret(0);

  // The number comparison code did not provide a valid result.
  __ bind(&non_number_comparison);

  // Fast negative check for internalized-to-internalized equality.
  Label check_for_strings;
  if (cc == equal) {
    BranchIfNotInternalizedString(
        masm, &check_for_strings, rax, kScratchRegister);
    BranchIfNotInternalizedString(
        masm, &check_for_strings, rdx, kScratchRegister);

    // We've already checked for object identity, so if both operands are
    // internalized strings they aren't equal. Register rax (not rax) already
    // holds a non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialOneByteStrings(rdx, rax, rcx, rbx,
                                           &check_unequal_objects);

  // Inline comparison of one-byte strings.
  if (cc == equal) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, rdx, rax, rcx, rbx);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, rdx, rax, rcx, rbx,
                                                    rdi, r8);
  }

#ifdef DEBUG
  __ Abort(kUnexpectedFallThroughFromStringComparison);
#endif

  __ bind(&check_unequal_objects);
  if (cc == equal && !strict()) {
    // Not strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    Label return_unequal, undetectable;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ leap(rcx, Operand(rax, rdx, times_1, 0));
    __ testb(rcx, Immediate(kSmiTagMask));
    __ j(not_zero, &runtime_call, Label::kNear);

    __ movp(rbx, FieldOperand(rax, HeapObject::kMapOffset));
    __ movp(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(not_zero, &undetectable);
    __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(not_zero, &return_unequal);

    __ CmpInstanceType(rbx, FIRST_JS_RECEIVER_TYPE);
    __ j(below, &runtime_call, Label::kNear);
    __ CmpInstanceType(rcx, FIRST_JS_RECEIVER_TYPE);
    __ j(below, &runtime_call, Label::kNear);

    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in rax.
    __ ret(0);

    __ bind(&undetectable);
    __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal);
    __ Set(rax, EQUAL);
    __ ret(0);
  }
  __ bind(&runtime_call);

  if (cc == equal) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rdx);
      __ Push(rax);
      __ CallRuntime(strict() ? Runtime::kStrictEqual : Runtime::kEqual);
    }
    // Turn true into 0 and false into some non-zero value.
    STATIC_ASSERT(EQUAL == 0);
    __ LoadRoot(rdx, Heap::kTrueValueRootIndex);
    __ subp(rax, rdx);
    __ Ret();
  } else {
    // Push arguments below the return address to prepare jump to builtin.
    __ PopReturnAddressTo(rcx);
    __ Push(rdx);
    __ Push(rax);
    __ Push(Smi::FromInt(NegativeComparisonResult(cc)));
    __ PushReturnAddressFrom(rcx);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // rax : number of arguments to the construct function
  // rbx : feedback vector
  // rdx : slot in feedback vector (Smi)
  // rdi : the function to call
  FrameScope scope(masm, StackFrame::INTERNAL);

  // Number-of-arguments register must be smi-tagged to call out.
  __ Integer32ToSmi(rax, rax);
  __ Push(rax);
  __ Push(rdi);
  __ Integer32ToSmi(rdx, rdx);
  __ Push(rdx);
  __ Push(rbx);

  __ CallStub(stub);

  __ Pop(rbx);
  __ Pop(rdx);
  __ Pop(rdi);
  __ Pop(rax);
  __ SmiToInteger32(rax, rax);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // rax : number of arguments to the construct function
  // rbx : feedback vector
  // rdx : slot in feedback vector (Smi)
  // rdi : the function to call
  Isolate* isolate = masm->isolate();
  Label initialize, done, miss, megamorphic, not_array_function,
      done_no_smi_convert;

  // Load the cache state into r11.
  __ SmiToInteger32(rdx, rdx);
  __ movp(r11,
          FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if r11 is a WeakCell or a Symbol, but it's harmless to read
  // at this position in a symbol (see static asserts in
  // type-feedback-vector.h).
  Label check_allocation_site;
  __ cmpp(rdi, FieldOperand(r11, WeakCell::kValueOffset));
  __ j(equal, &done, Label::kFar);
  __ CompareRoot(r11, Heap::kmegamorphic_symbolRootIndex);
  __ j(equal, &done, Label::kFar);
  __ CompareRoot(FieldOperand(r11, HeapObject::kMapOffset),
                 Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &check_allocation_site);

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ CheckSmi(FieldOperand(r11, WeakCell::kValueOffset));
  __ j(equal, &initialize);
  __ jmp(&megamorphic);

  __ bind(&check_allocation_site);
  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the slot either some other function or an
  // AllocationSite.
  __ CompareRoot(FieldOperand(r11, 0), Heap::kAllocationSiteMapRootIndex);
  __ j(not_equal, &miss);

  // Make sure the function is the Array() function
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r11);
  __ cmpp(rdi, r11);
  __ j(not_equal, &megamorphic);
  __ jmp(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(r11, Heap::kuninitialized_symbolRootIndex);
  __ j(equal, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ Move(FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize),
          TypeFeedbackVector::MegamorphicSentinel(isolate));
  __ jmp(&done);

  // An uninitialized cache is patched with the function or sentinel to
  // indicate the ElementsKind if function is the Array constructor.
  __ bind(&initialize);

  // Make sure the function is the Array() function
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r11);
  __ cmpp(rdi, r11);
  __ j(not_equal, &not_array_function);

  CreateAllocationSiteStub create_stub(isolate);
  CallStubInRecordCallTarget(masm, &create_stub);
  __ jmp(&done_no_smi_convert);

  __ bind(&not_array_function);
  CreateWeakCellStub weak_cell_stub(isolate);
  CallStubInRecordCallTarget(masm, &weak_cell_stub);
  __ jmp(&done_no_smi_convert);

  __ bind(&done);
  __ Integer32ToSmi(rdx, rdx);

  __ bind(&done_no_smi_convert);
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // rax : number of arguments
  // rbx : feedback vector
  // rdx : slot in feedback vector (Smi)
  // rdi : constructor function

  Label non_function;
  // Check that the constructor is not a smi.
  __ JumpIfSmi(rdi, &non_function);
  // Check that constructor is a JSFunction.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, r11);
  __ j(not_equal, &non_function);

  GenerateRecordCallTarget(masm);

  __ SmiToInteger32(rdx, rdx);
  Label feedback_register_initialized;
  // Put the AllocationSite from the feedback vector into rbx, or undefined.
  __ movp(rbx,
          FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize));
  __ CompareRoot(FieldOperand(rbx, 0), Heap::kAllocationSiteMapRootIndex);
  __ j(equal, &feedback_register_initialized, Label::kNear);
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(rbx);

  // Pass new target to construct stub.
  __ movp(rdx, rdi);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kConstructStubOffset));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);

  __ bind(&non_function);
  __ movp(rdx, rdi);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}


void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // rdi - function
  // rdx - slot id
  // rbx - vector
  // rcx - allocation site (loaded from vector[slot]).
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r8);
  __ cmpp(rdi, r8);
  __ j(not_equal, miss);

  __ movp(rax, Immediate(arg_count()));

  // Increment the call count for monomorphic function calls.
  __ SmiAddConstant(FieldOperand(rbx, rdx, times_pointer_size,
                                 FixedArray::kHeaderSize + kPointerSize),
                    Smi::FromInt(CallICNexus::kCallCountIncrement));

  __ movp(rbx, rcx);
  __ movp(rdx, rdi);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);
}


void CallICStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- rdi - function
  // -- rdx - slot id
  // -- rbx - vector
  // -----------------------------------
  Isolate* isolate = masm->isolate();
  Label extra_checks_or_miss, call, call_function;
  int argc = arg_count();
  StackArgumentsAccessor args(rsp, argc);
  ParameterCount actual(argc);

  // The checks. First, does rdi match the recorded monomorphic target?
  __ SmiToInteger32(rdx, rdx);
  __ movp(rcx,
          FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize));

  // We don't know that we have a weak cell. We might have a private symbol
  // or an AllocationSite, but the memory is safe to examine.
  // AllocationSite::kTransitionInfoOffset - contains a Smi or pointer to
  // FixedArray.
  // WeakCell::kValueOffset - contains a JSFunction or Smi(0)
  // Symbol::kHashFieldSlot - if the low bit is 1, then the hash is not
  // computed, meaning that it can't appear to be a pointer. If the low bit is
  // 0, then hash is computed, but the 0 bit prevents the field from appearing
  // to be a pointer.
  STATIC_ASSERT(WeakCell::kSize >= kPointerSize);
  STATIC_ASSERT(AllocationSite::kTransitionInfoOffset ==
                    WeakCell::kValueOffset &&
                WeakCell::kValueOffset == Symbol::kHashFieldSlot);

  __ cmpp(rdi, FieldOperand(rcx, WeakCell::kValueOffset));
  __ j(not_equal, &extra_checks_or_miss);

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(rdi, &extra_checks_or_miss);

  // Increment the call count for monomorphic function calls.
  __ SmiAddConstant(FieldOperand(rbx, rdx, times_pointer_size,
                                 FixedArray::kHeaderSize + kPointerSize),
                    Smi::FromInt(CallICNexus::kCallCountIncrement));

  __ bind(&call_function);
  __ Set(rax, argc);
  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ Cmp(rcx, TypeFeedbackVector::MegamorphicSentinel(isolate));
  __ j(equal, &call);

  // Check if we have an allocation site.
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kAllocationSiteMapRootIndex);
  __ j(not_equal, &not_allocation_site);

  // We have an allocation site.
  HandleArrayCase(masm, &miss);

  __ bind(&not_allocation_site);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ jmp(&miss);
  }

  __ Cmp(rcx, TypeFeedbackVector::UninitializedSentinel(isolate));
  __ j(equal, &uninitialized);

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(rcx);
  __ CmpObjectType(rcx, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &miss);
  __ Move(FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize),
          TypeFeedbackVector::MegamorphicSentinel(isolate));

  __ bind(&call);
  __ Set(rax, argc);
  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(rdi, &miss);

  // Goto miss case if we do not have a function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &miss);

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, rcx);
  __ cmpp(rdi, rcx);
  __ j(equal, &miss);

  // Make sure the function belongs to the same native context.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kContextOffset));
  __ movp(rcx, ContextOperand(rcx, Context::NATIVE_CONTEXT_INDEX));
  __ cmpp(rcx, NativeContextOperand());
  __ j(not_equal, &miss);

  // Initialize the call counter.
  __ Move(FieldOperand(rbx, rdx, times_pointer_size,
                       FixedArray::kHeaderSize + kPointerSize),
          Smi::FromInt(CallICNexus::kCallCountIncrement));

  // Store the function. Use a stub since we need a frame for allocation.
  // rbx - vector
  // rdx - slot (needs to be in smi form)
  // rdi - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(isolate);

    __ Integer32ToSmi(rdx, rdx);
    __ Push(rdi);
    __ CallStub(&create_stub);
    __ Pop(rdi);
  }

  __ jmp(&call_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  __ jmp(&call);

  // Unreachable
  __ int3();
}


void CallICStub::GenerateMiss(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);

  // Push the receiver and the function and feedback info.
  __ Push(rdi);
  __ Push(rbx);
  __ Integer32ToSmi(rdx, rdx);
  __ Push(rdx);

  // Call the entry.
  __ CallRuntime(Runtime::kCallIC_Miss);

  // Move result to edi and exit the internal frame.
  __ movp(rdi, rax);
}


bool CEntryStub::NeedsImmovableCode() {
  return false;
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  // It is important that the store buffer overflow stubs are generated first.
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  CreateWeakCellStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
  TypeofStub::GenerateAheadOfTime(isolate);
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
    __ EnterApiExitFrame(arg_stack_space);
    // Move argc into r14 (argv is already in r15).
    __ movp(r14, rax);
  } else {
    __ EnterExitFrame(arg_stack_space, save_doubles());
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
    DCHECK_LE(result_size(), 3);
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
    DCHECK_LE(result_size(), 3);
    __ movq(kReturnRegister0, StackSpaceOperand(kArgExtraStackSpace + 0));
    __ movq(kReturnRegister1, StackSpaceOperand(kArgExtraStackSpace + 1));
    if (result_size() > 2) {
      __ movq(kReturnRegister2, StackSpaceOperand(kArgExtraStackSpace + 2));
    }
  }
  // Result is in rax, rdx:rax or r8:rdx:rax - do not destroy these registers!

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
        Isolate::kPendingExceptionAddress, isolate());
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
      Isolate::kPendingHandlerContextAddress, isolate());
  ExternalReference pending_handler_code_address(
      Isolate::kPendingHandlerCodeAddress, isolate());
  ExternalReference pending_handler_offset_address(
      Isolate::kPendingHandlerOffsetAddress, isolate());
  ExternalReference pending_handler_fp_address(
      Isolate::kPendingHandlerFPAddress, isolate());
  ExternalReference pending_handler_sp_address(
      Isolate::kPendingHandlerSPAddress, isolate());

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
  __ movp(rdi, masm->ExternalOperand(pending_handler_code_address));
  __ movp(rdx, masm->ExternalOperand(pending_handler_offset_address));
  __ leap(rdi, FieldOperand(rdi, rdx, times_1, Code::kHeaderSize));
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

    // Push the stack frame type marker twice.
    int marker = type();
    // Scratch register is neither callee-save, nor an argument register on any
    // platform. It's free to use at this point.
    // Cannot use smi-register for loading yet.
    __ Move(kScratchRegister, Smi::FromInt(marker), Assembler::RelocInfoNone());
    __ Push(kScratchRegister);  // context slot
    __ Push(kScratchRegister);  // function slot
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
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate());
  {
    Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ Push(c_entry_fp_operand);
  }

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ Load(rax, js_entry_sp);
  __ testp(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ Push(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ movp(rax, rbp);
  __ Store(js_entry_sp, rax);
  Label cont;
  __ jmp(&cont);
  __ bind(&not_outermost_js);
  __ Push(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate());
  __ Store(pending_exception, rax);
  __ LoadRoot(rax, Heap::kExceptionRootIndex);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Clear any pending exceptions.
  __ LoadRoot(rax, Heap::kTheHoleValueRootIndex);
  __ Store(pending_exception, rax);

  // Fake a receiver (NULL).
  __ Push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. We load the address from an
  // external reference instead of inlining the call target address directly
  // in the code, because the builtin stubs may not have been generated yet
  // at the time this code is generated.
  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate());
    __ Load(rax, construct_entry);
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate());
    __ Load(rax, entry);
  }
  __ leap(kScratchRegister, FieldOperand(rax, Code::kHeaderSize));
  __ call(kScratchRegister);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ Pop(rbx);
  __ Cmp(rbx, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
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


void InstanceOfStub::Generate(MacroAssembler* masm) {
  Register const object = rdx;              // Object (lhs).
  Register const function = rax;            // Function (rhs).
  Register const object_map = rcx;          // Map of {object}.
  Register const function_map = r8;         // Map of {function}.
  Register const function_prototype = rdi;  // Prototype of {function}.

  DCHECK(object.is(InstanceOfDescriptor::LeftRegister()));
  DCHECK(function.is(InstanceOfDescriptor::RightRegister()));

  // Check if {object} is a smi.
  Label object_is_smi;
  __ JumpIfSmi(object, &object_is_smi, Label::kNear);

  // Lookup the {function} and the {object} map in the global instanceof cache.
  // Note: This is safe because we clear the global instanceof cache whenever
  // we change the prototype of any object.
  Label fast_case, slow_case;
  __ movp(object_map, FieldOperand(object, HeapObject::kMapOffset));
  __ CompareRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
  __ j(not_equal, &fast_case, Label::kNear);
  __ CompareRoot(object_map, Heap::kInstanceofCacheMapRootIndex);
  __ j(not_equal, &fast_case, Label::kNear);
  __ LoadRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(0);

  // If {object} is a smi we can safely return false if {function} is a JS
  // function, otherwise we have to miss to the runtime and throw an exception.
  __ bind(&object_is_smi);
  __ JumpIfSmi(function, &slow_case);
  __ CmpObjectType(function, JS_FUNCTION_TYPE, function_map);
  __ j(not_equal, &slow_case);
  __ LoadRoot(rax, Heap::kFalseValueRootIndex);
  __ ret(0);

  // Fast-case: The {function} must be a valid JSFunction.
  __ bind(&fast_case);
  __ JumpIfSmi(function, &slow_case);
  __ CmpObjectType(function, JS_FUNCTION_TYPE, function_map);
  __ j(not_equal, &slow_case);

  // Go to the runtime if the function is not a constructor.
  __ testb(FieldOperand(function_map, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &slow_case);

  // Ensure that {function} has an instance prototype.
  __ testb(FieldOperand(function_map, Map::kBitFieldOffset),
           Immediate(1 << Map::kHasNonInstancePrototype));
  __ j(not_zero, &slow_case);

  // Get the "prototype" (or initial map) of the {function}.
  __ movp(function_prototype,
          FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  __ AssertNotSmi(function_prototype);

  // Resolve the prototype if the {function} has an initial map.  Afterwards the
  // {function_prototype} will be either the JSReceiver prototype object or the
  // hole value, which means that no instances of the {function} were created so
  // far and hence we should return false.
  Label function_prototype_valid;
  Register const function_prototype_map = kScratchRegister;
  __ CmpObjectType(function_prototype, MAP_TYPE, function_prototype_map);
  __ j(not_equal, &function_prototype_valid, Label::kNear);
  __ movp(function_prototype,
          FieldOperand(function_prototype, Map::kPrototypeOffset));
  __ bind(&function_prototype_valid);
  __ AssertNotSmi(function_prototype);

  // Update the global instanceof cache with the current {object} map and
  // {function}.  The cached answer will be set when it is known below.
  __ StoreRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
  __ StoreRoot(object_map, Heap::kInstanceofCacheMapRootIndex);

  // Loop through the prototype chain looking for the {function} prototype.
  // Assume true, and change to false if not found.
  Label done, loop, fast_runtime_fallback;
  __ LoadRoot(rax, Heap::kTrueValueRootIndex);
  __ bind(&loop);

  __ testb(FieldOperand(object_map, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &fast_runtime_fallback, Label::kNear);
  __ CmpInstanceType(object_map, JS_PROXY_TYPE);
  __ j(equal, &fast_runtime_fallback, Label::kNear);

  __ movp(object, FieldOperand(object_map, Map::kPrototypeOffset));
  __ cmpp(object, function_prototype);
  __ j(equal, &done, Label::kNear);
  __ CompareRoot(object, Heap::kNullValueRootIndex);
  __ movp(object_map, FieldOperand(object, HeapObject::kMapOffset));
  __ j(not_equal, &loop);
  __ LoadRoot(rax, Heap::kFalseValueRootIndex);
  __ bind(&done);
  __ StoreRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(0);

  // Found Proxy or access check needed: Call the runtime.
  __ bind(&fast_runtime_fallback);
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(object);
  __ Push(function_prototype);
  __ PushReturnAddressFrom(kScratchRegister);
  // Invalidate the instanceof cache.
  __ Move(rax, Smi::FromInt(0));
  __ StoreRoot(rax, Heap::kInstanceofCacheFunctionRootIndex);
  __ TailCallRuntime(Runtime::kHasInPrototypeChain);

  // Slow-case: Call the %InstanceOf runtime function.
  __ bind(&slow_case);
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(object);
  __ Push(function);
  __ PushReturnAddressFrom(kScratchRegister);
  __ TailCallRuntime(Runtime::kInstanceOf);
}


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ movp(result_, FieldOperand(object_, HeapObject::kMapOffset));
    __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ testb(result_, Immediate(kIsNotStringMask));
    __ j(not_zero, receiver_not_string_);
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ SmiCompare(index_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  __ SmiToInteger32(index_, index_);

  StringCharLoadGenerator::Generate(
      masm, object_, index_, result_, &call_runtime_);

  __ Integer32ToSmi(result_, result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm, EmbedMode embed_mode,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

  Factory* factory = masm->isolate()->factory();
  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              factory->heap_number_map(),
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Push(LoadWithVectorDescriptor::VectorRegister());
    __ Push(LoadDescriptor::SlotRegister());
  }
  __ Push(object_);
  __ Push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero);
  } else {
    DCHECK(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi);
  }
  if (!index_.is(rax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ movp(index_, rax);
  }
  __ Pop(object_);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Pop(LoadDescriptor::SlotRegister());
    __ Pop(LoadWithVectorDescriptor::VectorRegister());
  }
  // Reload the instance type.
  __ movp(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ Push(object_);
  __ Integer32ToSmi(index_, index_);
  __ Push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT);
  if (!result_.is(rax)) {
    __ movp(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  __ JumpIfNotSmi(code_, &slow_case_);
  __ SmiCompare(code_, Smi::FromInt(String::kMaxOneByteCharCode));
  __ j(above, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  SmiIndex index = masm->SmiToIndex(kScratchRegister, code_, kPointerSizeLog2);
  __ movp(result_, FieldOperand(result_, index.reg, index.scale,
                                FixedArray::kHeaderSize));
  __ CompareRoot(result_, Heap::kUndefinedValueRootIndex);
  __ j(equal, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ Push(code_);
  __ CallRuntime(Runtime::kStringCharFromCode);
  if (!result_.is(rax)) {
    __ movp(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          String::Encoding encoding) {
  // Nothing to do for zero characters.
  Label done;
  __ testl(count, count);
  __ j(zero, &done, Label::kNear);

  // Make count the number of bytes to copy.
  if (encoding == String::TWO_BYTE_ENCODING) {
    STATIC_ASSERT(2 == sizeof(uc16));
    __ addl(count, count);
  }

  // Copy remaining characters.
  Label loop;
  __ bind(&loop);
  __ movb(kScratchRegister, Operand(src, 0));
  __ movb(Operand(dest, 0), kScratchRegister);
  __ incp(src);
  __ incp(dest);
  __ decl(count);
  __ j(not_zero, &loop);

  __ bind(&done);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]  : return address
  //  rsp[8]  : to
  //  rsp[16] : from
  //  rsp[24] : string

  enum SubStringStubArgumentIndices {
    STRING_ARGUMENT_INDEX,
    FROM_ARGUMENT_INDEX,
    TO_ARGUMENT_INDEX,
    SUB_STRING_ARGUMENT_COUNT
  };

  StackArgumentsAccessor args(rsp, SUB_STRING_ARGUMENT_COUNT,
                              ARGUMENTS_DONT_CONTAIN_RECEIVER);

  // Make sure first argument is a string.
  __ movp(rax, args.GetArgumentOperand(STRING_ARGUMENT_INDEX));
  STATIC_ASSERT(kSmiTag == 0);
  __ testl(rax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);

  // rax: string
  // rbx: instance type
  // Calculate length of sub string using the smi values.
  __ movp(rcx, args.GetArgumentOperand(TO_ARGUMENT_INDEX));
  __ movp(rdx, args.GetArgumentOperand(FROM_ARGUMENT_INDEX));
  __ JumpUnlessBothNonNegativeSmi(rcx, rdx, &runtime);

  __ SmiSub(rcx, rcx, rdx);  // Overflow doesn't happen.
  __ cmpp(rcx, FieldOperand(rax, String::kLengthOffset));
  Label not_original_string;
  // Shorter than original string's length: an actual substring.
  __ j(below, &not_original_string, Label::kNear);
  // Longer than original string's length or negative: unsafe arguments.
  __ j(above, &runtime);
  // Return original string.
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(SUB_STRING_ARGUMENT_COUNT * kPointerSize);
  __ bind(&not_original_string);

  Label single_char;
  __ SmiCompare(rcx, Smi::FromInt(1));
  __ j(equal, &single_char);

  __ SmiToInteger32(rcx, rcx);

  // rax: string
  // rbx: instance type
  // rcx: sub string length
  // rdx: from index (smi)
  // Deal with different string types: update the index if necessary
  // and put the underlying string into edi.
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ testb(rbx, Immediate(kIsIndirectStringMask));
  __ j(zero, &seq_or_external_string, Label::kNear);

  __ testb(rbx, Immediate(kSlicedNotConsMask));
  __ j(not_zero, &sliced_string, Label::kNear);
  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  __ CompareRoot(FieldOperand(rax, ConsString::kSecondOffset),
                 Heap::kempty_stringRootIndex);
  __ j(not_equal, &runtime);
  __ movp(rdi, FieldOperand(rax, ConsString::kFirstOffset));
  // Update instance type.
  __ movp(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and correct start index by offset.
  __ addp(rdx, FieldOperand(rax, SlicedString::kOffsetOffset));
  __ movp(rdi, FieldOperand(rax, SlicedString::kParentOffset));
  // Update instance type.
  __ movp(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the correct register.
  __ movp(rdi, rax);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // rdi: underlying subject string
    // rbx: instance type of underlying subject string
    // rdx: adjusted start index (smi)
    // rcx: length
    // If coming from the make_two_character_string path, the string
    // is too short to be sliced anyways.
    __ cmpp(rcx, Immediate(SlicedString::kMinLength));
    // Short slice.  Copy instead of slicing.
    __ j(less, &copy_routine);
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ testb(rbx, Immediate(kStringEncodingMask));
    __ j(zero, &two_byte_slice, Label::kNear);
    __ AllocateOneByteSlicedString(rax, rbx, r14, &runtime);
    __ jmp(&set_slice_header, Label::kNear);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(rax, rbx, r14, &runtime);
    __ bind(&set_slice_header);
    __ Integer32ToSmi(rcx, rcx);
    __ movp(FieldOperand(rax, SlicedString::kLengthOffset), rcx);
    __ movp(FieldOperand(rax, SlicedString::kHashFieldOffset),
           Immediate(String::kEmptyHashField));
    __ movp(FieldOperand(rax, SlicedString::kParentOffset), rdi);
    __ movp(FieldOperand(rax, SlicedString::kOffsetOffset), rdx);
    __ IncrementCounter(counters->sub_string_native(), 1);
    __ ret(3 * kPointerSize);

    __ bind(&copy_routine);
  }

  // rdi: underlying subject string
  // rbx: instance type of underlying subject string
  // rdx: adjusted start index (smi)
  // rcx: length
  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label two_byte_sequential, sequential_string;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(rbx, Immediate(kExternalStringTag));
  __ j(zero, &sequential_string);

  // Handle external string.
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ testb(rbx, Immediate(kShortExternalStringMask));
  __ j(not_zero, &runtime);
  __ movp(rdi, FieldOperand(rdi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ subp(rdi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&sequential_string);
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ testb(rbx, Immediate(kStringEncodingMask));
  __ j(zero, &two_byte_sequential);

  // Allocate the result.
  __ AllocateOneByteString(rax, rcx, r11, r14, r15, &runtime);

  // rax: result string
  // rcx: result string length
  {  // Locate character of sub string start.
    SmiIndex smi_as_index = masm->SmiToIndex(rdx, rdx, times_1);
    __ leap(r14, Operand(rdi, smi_as_index.reg, smi_as_index.scale,
                        SeqOneByteString::kHeaderSize - kHeapObjectTag));
  }
  // Locate first character of result.
  __ leap(rdi, FieldOperand(rax, SeqOneByteString::kHeaderSize));

  // rax: result string
  // rcx: result length
  // r14: first character of result
  // rsi: character of sub string start
  StringHelper::GenerateCopyCharacters(
      masm, rdi, r14, rcx, String::ONE_BYTE_ENCODING);
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(SUB_STRING_ARGUMENT_COUNT * kPointerSize);

  __ bind(&two_byte_sequential);
  // Allocate the result.
  __ AllocateTwoByteString(rax, rcx, r11, r14, r15, &runtime);

  // rax: result string
  // rcx: result string length
  {  // Locate character of sub string start.
    SmiIndex smi_as_index = masm->SmiToIndex(rdx, rdx, times_2);
    __ leap(r14, Operand(rdi, smi_as_index.reg, smi_as_index.scale,
                        SeqOneByteString::kHeaderSize - kHeapObjectTag));
  }
  // Locate first character of result.
  __ leap(rdi, FieldOperand(rax, SeqTwoByteString::kHeaderSize));

  // rax: result string
  // rcx: result length
  // rdi: first character of result
  // r14: character of sub string start
  StringHelper::GenerateCopyCharacters(
      masm, rdi, r14, rcx, String::TWO_BYTE_ENCODING);
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(SUB_STRING_ARGUMENT_COUNT * kPointerSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString);

  __ bind(&single_char);
  // rax: string
  // rbx: instance type
  // rcx: sub string length (smi)
  // rdx: from index (smi)
  StringCharAtGenerator generator(rax, rdx, rcx, rax, &runtime, &runtime,
                                  &runtime, STRING_INDEX_IS_NUMBER,
                                  RECEIVER_IS_STRING);
  generator.GenerateFast(masm);
  __ ret(SUB_STRING_ARGUMENT_COUNT * kPointerSize);
  generator.SkipSlow(masm, &runtime);
}


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in rax.
  Label not_smi;
  __ JumpIfNotSmi(rax, &not_smi, Label::kNear);
  __ Ret();
  __ bind(&not_smi);

  Label not_heap_number;
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &not_heap_number, Label::kNear);
  __ Ret();
  __ bind(&not_heap_number);

  Label not_string, slow_string;
  __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdi);
  // rax: object
  // rdi: object map
  __ j(above_equal, &not_string, Label::kNear);
  // Check if string has a cached array index.
  __ testl(FieldOperand(rax, String::kHashFieldOffset),
           Immediate(String::kContainsCachedArrayIndexMask));
  __ j(not_zero, &slow_string, Label::kNear);
  __ movl(rax, FieldOperand(rax, String::kHashFieldOffset));
  __ IndexFromHash(rax, rax);
  __ Ret();
  __ bind(&slow_string);
  __ PopReturnAddressTo(rcx);     // Pop return address.
  __ Push(rax);                   // Push argument.
  __ PushReturnAddressFrom(rcx);  // Push return address.
  __ TailCallRuntime(Runtime::kStringToNumber);
  __ bind(&not_string);

  Label not_oddball;
  __ CmpInstanceType(rdi, ODDBALL_TYPE);
  __ j(not_equal, &not_oddball, Label::kNear);
  __ movp(rax, FieldOperand(rax, Oddball::kToNumberOffset));
  __ Ret();
  __ bind(&not_oddball);

  __ PopReturnAddressTo(rcx);     // Pop return address.
  __ Push(rax);                   // Push argument.
  __ PushReturnAddressFrom(rcx);  // Push return address.
  __ TailCallRuntime(Runtime::kToNumber);
}


void ToLengthStub::Generate(MacroAssembler* masm) {
  // The ToLength stub takes on argument in rax.
  Label not_smi, positive_smi;
  __ JumpIfNotSmi(rax, &not_smi, Label::kNear);
  STATIC_ASSERT(kSmiTag == 0);
  __ testp(rax, rax);
  __ j(greater_equal, &positive_smi, Label::kNear);
  __ xorl(rax, rax);
  __ bind(&positive_smi);
  __ Ret();
  __ bind(&not_smi);

  __ PopReturnAddressTo(rcx);     // Pop return address.
  __ Push(rax);                   // Push argument.
  __ PushReturnAddressFrom(rcx);  // Push return address.
  __ TailCallRuntime(Runtime::kToLength);
}


void ToStringStub::Generate(MacroAssembler* masm) {
  // The ToString stub takes one argument in rax.
  Label is_number;
  __ JumpIfSmi(rax, &is_number, Label::kNear);

  Label not_string;
  __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdi);
  // rax: receiver
  // rdi: receiver map
  __ j(above_equal, &not_string, Label::kNear);
  __ Ret();
  __ bind(&not_string);

  Label not_heap_number;
  __ CompareRoot(rdi, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &not_heap_number, Label::kNear);
  __ bind(&is_number);
  NumberToStringStub stub(isolate());
  __ TailCallStub(&stub);
  __ bind(&not_heap_number);

  Label not_oddball;
  __ CmpInstanceType(rdi, ODDBALL_TYPE);
  __ j(not_equal, &not_oddball, Label::kNear);
  __ movp(rax, FieldOperand(rax, Oddball::kToStringOffset));
  __ Ret();
  __ bind(&not_oddball);

  __ PopReturnAddressTo(rcx);     // Pop return address.
  __ Push(rax);                   // Push argument.
  __ PushReturnAddressFrom(rcx);  // Push return address.
  __ TailCallRuntime(Runtime::kToString);
}


void ToNameStub::Generate(MacroAssembler* masm) {
  // The ToName stub takes one argument in rax.
  Label is_number;
  __ JumpIfSmi(rax, &is_number, Label::kNear);

  Label not_name;
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ CmpObjectType(rax, LAST_NAME_TYPE, rdi);
  // rax: receiver
  // rdi: receiver map
  __ j(above, &not_name, Label::kNear);
  __ Ret();
  __ bind(&not_name);

  Label not_heap_number;
  __ CompareRoot(rdi, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &not_heap_number, Label::kNear);
  __ bind(&is_number);
  NumberToStringStub stub(isolate());
  __ TailCallStub(&stub);
  __ bind(&not_heap_number);

  Label not_oddball;
  __ CmpInstanceType(rdi, ODDBALL_TYPE);
  __ j(not_equal, &not_oddball, Label::kNear);
  __ movp(rax, FieldOperand(rax, Oddball::kToStringOffset));
  __ Ret();
  __ bind(&not_oddball);

  __ PopReturnAddressTo(rcx);     // Pop return address.
  __ Push(rax);                   // Push argument.
  __ PushReturnAddressFrom(rcx);  // Push return address.
  __ TailCallRuntime(Runtime::kToName);
}


void StringHelper::GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                                   Register left,
                                                   Register right,
                                                   Register scratch1,
                                                   Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label check_zero_length;
  __ movp(length, FieldOperand(left, String::kLengthOffset));
  __ SmiCompare(length, FieldOperand(right, String::kLengthOffset));
  __ j(equal, &check_zero_length, Label::kNear);
  __ Move(rax, Smi::FromInt(NOT_EQUAL));
  __ ret(0);

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ SmiTest(length);
  __ j(not_zero, &compare_chars, Label::kNear);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Compare characters.
  __ bind(&compare_chars);
  Label strings_not_equal;
  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2,
                                  &strings_not_equal, Label::kNear);

  // Characters are equal.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Characters are not equal.
  __ bind(&strings_not_equal);
  __ Move(rax, Smi::FromInt(NOT_EQUAL));
  __ ret(0);
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3, Register scratch4) {
  // Ensure that you can always subtract a string length from a non-negative
  // number (e.g. another length).
  STATIC_ASSERT(String::kMaxLength < 0x7fffffff);

  // Find minimum length and length difference.
  __ movp(scratch1, FieldOperand(left, String::kLengthOffset));
  __ movp(scratch4, scratch1);
  __ SmiSub(scratch4,
            scratch4,
            FieldOperand(right, String::kLengthOffset));
  // Register scratch4 now holds left.length - right.length.
  const Register length_difference = scratch4;
  Label left_shorter;
  __ j(less, &left_shorter, Label::kNear);
  // The right string isn't longer that the left one.
  // Get the right string's length by subtracting the (non-negative) difference
  // from the left string's length.
  __ SmiSub(scratch1, scratch1, length_difference);
  __ bind(&left_shorter);
  // Register scratch1 now holds Min(left.length, right.length).
  const Register min_length = scratch1;

  Label compare_lengths;
  // If min-length is zero, go directly to comparing lengths.
  __ SmiTest(min_length);
  __ j(zero, &compare_lengths, Label::kNear);

  // Compare loop.
  Label result_not_equal;
  GenerateOneByteCharsCompareLoop(
      masm, left, right, min_length, scratch2, &result_not_equal,
      // In debug-code mode, SmiTest below might push
      // the target label outside the near range.
      Label::kFar);

  // Completed loop without finding different characters.
  // Compare lengths (precomputed).
  __ bind(&compare_lengths);
  __ SmiTest(length_difference);
  Label length_not_equal;
  __ j(not_zero, &length_not_equal, Label::kNear);

  // Result is EQUAL.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  Label result_greater;
  Label result_less;
  __ bind(&length_not_equal);
  __ j(greater, &result_greater, Label::kNear);
  __ jmp(&result_less, Label::kNear);
  __ bind(&result_not_equal);
  // Unequal comparison of left to right, either character or length.
  __ j(above, &result_greater, Label::kNear);
  __ bind(&result_less);

  // Result is LESS.
  __ Move(rax, Smi::FromInt(LESS));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(rax, Smi::FromInt(GREATER));
  __ ret(0);
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch, Label* chars_not_equal, Label::Distance near_jump) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiToInteger32(length, length);
  __ leap(left,
         FieldOperand(left, length, times_1, SeqOneByteString::kHeaderSize));
  __ leap(right,
         FieldOperand(right, length, times_1, SeqOneByteString::kHeaderSize));
  __ negq(length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ movb(scratch, Operand(left, index, times_1, 0));
  __ cmpb(scratch, Operand(right, index, times_1, 0));
  __ j(not_equal, chars_not_equal, near_jump);
  __ incq(index);
  __ j(not_zero, &loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdx    : left string
  //  -- rax    : right string
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertString(rdx);
  __ AssertString(rax);

  // Check for identity.
  Label not_same;
  __ cmpp(rdx, rax);
  __ j(not_equal, &not_same, Label::kNear);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1);
  __ Ret();

  __ bind(&not_same);

  // Check that both are sequential one-byte strings.
  Label runtime;
  __ JumpIfNotBothSequentialOneByteStrings(rdx, rax, rcx, rbx, &runtime);

  // Inline comparison of one-byte strings.
  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1);
  StringHelper::GenerateCompareFlatOneByteStrings(masm, rdx, rax, rcx, rbx, rdi,
                                                  r8);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ PopReturnAddressTo(rcx);
  __ Push(rdx);
  __ Push(rax);
  __ PushReturnAddressFrom(rcx);
  __ TailCallRuntime(Runtime::kStringCompare);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdx    : left
  //  -- rax    : right
  //  -- rsp[0] : return address
  // -----------------------------------

  // Load rcx with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ Move(rcx, handle(isolate()->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ testb(rcx, Immediate(kSmiTagMask));
    __ Assert(not_equal, kExpectedAllocationSite);
    __ Cmp(FieldOperand(rcx, HeapObject::kMapOffset),
           isolate()->factory()->allocation_site_map());
    __ Assert(equal, kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state());
  __ TailCallStub(&stub);
}


void CompareICStub::GenerateBooleans(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::BOOLEAN, state());
  Label miss;
  Label::Distance const miss_distance =
      masm->emit_debug_code() ? Label::kFar : Label::kNear;

  __ JumpIfSmi(rdx, &miss, miss_distance);
  __ movp(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ JumpIfSmi(rax, &miss, miss_distance);
  __ movp(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ JumpIfNotRoot(rcx, Heap::kBooleanMapRootIndex, &miss, miss_distance);
  __ JumpIfNotRoot(rbx, Heap::kBooleanMapRootIndex, &miss, miss_distance);
  if (!Token::IsEqualityOp(op())) {
    __ movp(rax, FieldOperand(rax, Oddball::kToNumberOffset));
    __ AssertSmi(rax);
    __ movp(rdx, FieldOperand(rdx, Oddball::kToNumberOffset));
    __ AssertSmi(rdx);
    __ pushq(rax);
    __ movq(rax, rdx);
    __ popq(rdx);
  }
  __ subp(rax, rdx);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ JumpIfNotBothSmi(rdx, rax, &miss, Label::kNear);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ subp(rax, rdx);
  } else {
    Label done;
    __ subp(rdx, rax);
    __ j(no_overflow, &done, Label::kNear);
    // Correct sign of result in case of overflow.
    __ notp(rdx);
    __ bind(&done);
    __ movp(rax, rdx);
  }
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateNumbers(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::NUMBER);

  Label generic_stub;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;

  if (left() == CompareICState::SMI) {
    __ JumpIfNotSmi(rdx, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(rax, &miss);
  }

  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(rax, &right_smi, Label::kNear);
  __ CompareMap(rax, isolate()->factory()->heap_number_map());
  __ j(not_equal, &maybe_undefined1, Label::kNear);
  __ Movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
  __ jmp(&left, Label::kNear);
  __ bind(&right_smi);
  __ SmiToInteger32(rcx, rax);  // Can't clobber rax yet.
  __ Cvtlsi2sd(xmm1, rcx);

  __ bind(&left);
  __ JumpIfSmi(rdx, &left_smi, Label::kNear);
  __ CompareMap(rdx, isolate()->factory()->heap_number_map());
  __ j(not_equal, &maybe_undefined2, Label::kNear);
  __ Movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  __ jmp(&done);
  __ bind(&left_smi);
  __ SmiToInteger32(rcx, rdx);  // Can't clobber rdx yet.
  __ Cvtlsi2sd(xmm0, rcx);

  __ bind(&done);
  // Compare operands
  __ Ucomisd(xmm0, xmm1);

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);

  // Return a result of -1, 0, or 1, based on EFLAGS.
  // Performing mov, because xor would destroy the flag register.
  __ movl(rax, Immediate(0));
  __ movl(rcx, Immediate(0));
  __ setcc(above, rax);  // Add one to zero if carry clear and not equal.
  __ sbbp(rax, rcx);  // Subtract one if below (aka. carry set).
  __ ret(0);

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ Cmp(rax, isolate()->factory()->undefined_value());
    __ j(not_equal, &miss);
    __ JumpIfSmi(rdx, &unordered);
    __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rcx);
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ Cmp(rdx, isolate()->factory()->undefined_value());
    __ j(equal, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  DCHECK(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;

  // Check that both operands are heap objects.
  Label miss;
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss, Label::kNear);

  // Check that both operands are internalized strings.
  __ movp(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movp(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbp(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbp(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ orp(tmp1, tmp2);
  __ testb(tmp1, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, &miss, Label::kNear);

  // Internalized strings are compared by identity.
  Label done;
  __ cmpp(left, right);
  // Make sure rax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(rax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  DCHECK(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;

  // Check that both operands are heap objects.
  Label miss;
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss, Label::kNear);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ movp(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movp(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbp(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbp(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss, Label::kNear);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss, Label::kNear);

  // Unique names are compared by identity.
  Label done;
  __ cmpp(left, right);
  // Make sure rax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(rax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  Label miss;

  bool equality = Token::IsEqualityOp(op());

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;
  Register tmp3 = rdi;

  // Check that both operands are heap objects.
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ movp(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movp(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbp(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbp(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  __ movp(tmp3, tmp1);
  STATIC_ASSERT(kNotStringTag != 0);
  __ orp(tmp3, tmp2);
  __ testb(tmp3, Immediate(kIsNotStringMask));
  __ j(not_zero, &miss);

  // Fast check for identical strings.
  Label not_same;
  __ cmpp(left, right);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Handle not identical strings.
  __ bind(&not_same);

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We also know they are both
  // strings.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kInternalizedTag == 0);
    __ orp(tmp1, tmp2);
    __ testb(tmp1, Immediate(kIsNotInternalizedMask));
    __ j(not_zero, &do_compare, Label::kNear);
    // Make sure rax is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(rax));
    __ ret(0);
    __ bind(&do_compare);
  }

  // Check that both strings are sequential one-byte.
  Label runtime;
  __ JumpIfNotBothSequentialOneByteStrings(left, right, tmp1, tmp2, &runtime);

  // Compare flat one-byte strings. Returns when done.
  if (equality) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, left, right, tmp1,
                                                  tmp2);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(
        masm, left, right, tmp1, tmp2, tmp3, kScratchRegister);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ PopReturnAddressTo(tmp1);
  __ Push(left);
  __ Push(right);
  __ PushReturnAddressFrom(tmp1);
  if (equality) {
    __ TailCallRuntime(Runtime::kStringEquals);
  } else {
    __ TailCallRuntime(Runtime::kStringCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateReceivers(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::RECEIVER, state());
  Label miss;
  Condition either_smi = masm->CheckEitherSmi(rdx, rax);
  __ j(either_smi, &miss, Label::kNear);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ CmpObjectType(rax, FIRST_JS_RECEIVER_TYPE, rcx);
  __ j(below, &miss, Label::kNear);
  __ CmpObjectType(rdx, FIRST_JS_RECEIVER_TYPE, rcx);
  __ j(below, &miss, Label::kNear);

  DCHECK_EQ(equal, GetCondition());
  __ subp(rax, rdx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  Condition either_smi = masm->CheckEitherSmi(rdx, rax);
  __ j(either_smi, &miss, Label::kNear);

  __ GetWeakValue(rdi, cell);
  __ cmpp(FieldOperand(rdx, HeapObject::kMapOffset), rdi);
  __ j(not_equal, &miss, Label::kNear);
  __ cmpp(FieldOperand(rax, HeapObject::kMapOffset), rdi);
  __ j(not_equal, &miss, Label::kNear);

  if (Token::IsEqualityOp(op())) {
    __ subp(rax, rdx);
    __ ret(0);
  } else {
    __ PopReturnAddressTo(rcx);
    __ Push(rdx);
    __ Push(rax);
    __ Push(Smi::FromInt(NegativeComparisonResult(GetCondition())));
    __ PushReturnAddressFrom(rcx);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdx);
    __ Push(rax);
    __ Push(rdx);
    __ Push(rax);
    __ Push(Smi::FromInt(op()));
    __ CallRuntime(Runtime::kCompareIC_Miss);

    // Compute the entry point of the rewritten stub.
    __ leap(rdi, FieldOperand(rax, Code::kHeaderSize));
    __ Pop(rax);
    __ Pop(rdx);
  }

  // Do a tail call to the rewritten stub.
  __ jmp(rdi);
}


void NameDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register properties,
                                                      Handle<Name> name,
                                                      Register r0) {
  DCHECK(name->IsUniqueName());
  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the hole value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // r0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r0;
    // Capacity is smi 2^n.
    __ SmiToInteger32(index, FieldOperand(properties, kCapacityOffset));
    __ decl(index);
    __ andp(index,
            Immediate(name->Hash() + NameDictionary::GetProbeOffset(i)));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ leap(index, Operand(index, index, times_2, 0));  // index *= 3.

    Register entity_name = r0;
    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ movp(entity_name, Operand(properties,
                                 index,
                                 times_pointer_size,
                                 kElementsStartOffset - kHeapObjectTag));
    __ Cmp(entity_name, masm->isolate()->factory()->undefined_value());
    __ j(equal, done);

    // Stop if found the property.
    __ Cmp(entity_name, Handle<Name>(name));
    __ j(equal, miss);

    Label good;
    // Check for the hole and skip.
    __ CompareRoot(entity_name, Heap::kTheHoleValueRootIndex);
    __ j(equal, &good, Label::kNear);

    // Check if the entry name is not a unique name.
    __ movp(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
    __ JumpIfNotUniqueNameInstanceType(
        FieldOperand(entity_name, Map::kInstanceTypeOffset), miss);
    __ bind(&good);
  }

  NameDictionaryLookupStub stub(masm->isolate(), properties, r0, r0,
                                NEGATIVE_LOOKUP);
  __ Push(Handle<Object>(name));
  __ Push(Immediate(name->Hash()));
  __ CallStub(&stub);
  __ testp(r0, r0);
  __ j(not_zero, miss);
  __ jmp(done);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found leaving the
// index into the dictionary in |r1|. Jump to the |miss| label
// otherwise.
void NameDictionaryLookupStub::GeneratePositiveLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register elements,
                                                      Register name,
                                                      Register r0,
                                                      Register r1) {
  DCHECK(!elements.is(r0));
  DCHECK(!elements.is(r1));
  DCHECK(!name.is(r0));
  DCHECK(!name.is(r1));

  __ AssertName(name);

  __ SmiToInteger32(r0, FieldOperand(elements, kCapacityOffset));
  __ decl(r0);

  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movl(r1, FieldOperand(name, Name::kHashFieldOffset));
    __ shrl(r1, Immediate(Name::kHashShift));
    if (i > 0) {
      __ addl(r1, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ andp(r1, r0);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ leap(r1, Operand(r1, r1, times_2, 0));  // r1 = r1 * 3

    // Check if the key is identical to the name.
    __ cmpp(name, Operand(elements, r1, times_pointer_size,
                          kElementsStartOffset - kHeapObjectTag));
    __ j(equal, done);
  }

  NameDictionaryLookupStub stub(masm->isolate(), elements, r0, r1,
                                POSITIVE_LOOKUP);
  __ Push(name);
  __ movl(r0, FieldOperand(name, Name::kHashFieldOffset));
  __ shrl(r0, Immediate(Name::kHashShift));
  __ Push(r0);
  __ CallStub(&stub);

  __ testp(r0, r0);
  __ j(zero, miss);
  __ jmp(done);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Stack frame on entry:
  //  rsp[0 * kPointerSize] : return address.
  //  rsp[1 * kPointerSize] : key's hash.
  //  rsp[2 * kPointerSize] : key.
  // Registers:
  //  dictionary_: NameDictionary to probe.
  //  result_: used as scratch.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  Register scratch = result();

  __ SmiToInteger32(scratch, FieldOperand(dictionary(), kCapacityOffset));
  __ decl(scratch);
  __ Push(scratch);

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER,
                              kPointerSize);
  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movp(scratch, args.GetArgumentOperand(1));
    if (i > 0) {
      __ addl(scratch, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ andp(scratch, Operand(rsp, 0));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ leap(index(), Operand(scratch, scratch, times_2, 0));  // index *= 3.

    // Having undefined at this place means the name is not contained.
    __ movp(scratch, Operand(dictionary(), index(), times_pointer_size,
                             kElementsStartOffset - kHeapObjectTag));

    __ Cmp(scratch, isolate()->factory()->undefined_value());
    __ j(equal, &not_in_dictionary);

    // Stop if found the property.
    __ cmpp(scratch, args.GetArgumentOperand(0));
    __ j(equal, &in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // If we hit a key that is not a unique name during negative
      // lookup we have to bailout as this key might be equal to the
      // key we are looking for.

      // Check if the entry name is not a unique name.
      __ movp(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
      __ JumpIfNotUniqueNameInstanceType(
          FieldOperand(scratch, Map::kInstanceTypeOffset),
          &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ movp(scratch, Immediate(0));
    __ Drop(1);
    __ ret(2 * kPointerSize);
  }

  __ bind(&in_dictionary);
  __ movp(scratch, Immediate(1));
  __ Drop(1);
  __ ret(2 * kPointerSize);

  __ bind(&not_in_dictionary);
  __ movp(scratch, Immediate(0));
  __ Drop(1);
  __ ret(2 * kPointerSize);
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub1(isolate, kDontSaveFPRegs);
  stub1.GetCode();
  StoreBufferOverflowStub stub2(isolate, kSaveFPRegs);
  stub2.GetCode();
}


// Takes the input in 3 registers: address_ value_ and object_.  A pointer to
// the value has just been written into the object, now this stub makes sure
// we keep the GC informed.  The word in the object where the value has been
// written is in the address register.
void RecordWriteStub::Generate(MacroAssembler* masm) {
  Label skip_to_incremental_noncompacting;
  Label skip_to_incremental_compacting;

  // The first two instructions are generated with labels so as to get the
  // offset fixed up correctly by the bind(Label*) call.  We patch it back and
  // forth between a compare instructions (a nop in this position) and the
  // real branch when we start and stop incremental heap marking.
  // See RecordWriteStub::Patch for details.
  __ jmp(&skip_to_incremental_noncompacting, Label::kNear);
  __ jmp(&skip_to_incremental_compacting, Label::kFar);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);

  // Initial mode of the stub is expected to be STORE_BUFFER_ONLY.
  // Will be checked in IncrementalMarking::ActivateGeneratedStub.
  masm->set_byte_at(0, kTwoByteNopInstruction);
  masm->set_byte_at(2, kFiveByteNopInstruction);
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ movp(regs_.scratch0(), Operand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),
                           regs_.scratch0(),
                           &dont_need_remembered_set);

    __ JumpIfInNewSpace(regs_.object(), regs_.scratch0(),
                        &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm, kUpdateRememberedSetOnNoNeedToInformIncrementalMarker, mode);
    InformIncrementalMarker(masm);
    regs_.Restore(masm);
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);

    __ bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm, kReturnOnNoNeedToInformIncrementalMarker, mode);
  InformIncrementalMarker(masm);
  regs_.Restore(masm);
  __ ret(0);
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode());
  Register address =
      arg_reg_1.is(regs_.address()) ? kScratchRegister : regs_.address();
  DCHECK(!address.is(regs_.object()));
  DCHECK(!address.is(arg_reg_1));
  __ Move(address, regs_.address());
  __ Move(arg_reg_1, regs_.object());
  // TODO(gc) Can we just set address arg2 in the beginning?
  __ Move(arg_reg_2, address);
  __ LoadAddress(arg_reg_3,
                 ExternalReference::isolate_address(isolate()));
  int argument_count = 3;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count);
  __ CallCFunction(
      ExternalReference::incremental_marking_record_write_function(isolate()),
      argument_count);
  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode());
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_object;

  __ movp(regs_.scratch0(), Immediate(~Page::kPageAlignmentMask));
  __ andp(regs_.scratch0(), regs_.object());
  __ movp(regs_.scratch1(),
         Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset));
  __ subp(regs_.scratch1(), Immediate(1));
  __ movp(Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset),
         regs_.scratch1());
  __ j(negative, &need_incremental);

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(),
                 regs_.scratch0(),
                 regs_.scratch1(),
                 &on_black,
                 Label::kNear);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&on_black);

  // Get the value from the slot.
  __ movp(regs_.scratch0(), Operand(regs_.address(), 0));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlag(regs_.scratch0(),  // Contains value.
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kEvacuationCandidateMask,
                     zero,
                     &ensure_not_white,
                     Label::kNear);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kSkipEvacuationSlotsRecordingMask,
                     zero,
                     &need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need an extra register for this, so we push the object register
  // temporarily.
  __ Push(regs_.object());
  __ JumpIfWhite(regs_.scratch0(),  // The value.
                 regs_.scratch1(),  // Scratch.
                 regs_.object(),    // Scratch.
                 &need_incremental_pop_object, Label::kNear);
  __ Pop(regs_.object());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&need_incremental_pop_object);
  __ Pop(regs_.object());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ Call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ movp(rbx, MemOperand(rbp, parameter_count_offset));
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ PopReturnAddressTo(rcx);
  int additional_offset =
      function_mode() == JS_FUNCTION_STUB_MODE ? kPointerSize : 0;
  __ leap(rsp, MemOperand(rsp, rbx, times_pointer_size, additional_offset));
  __ jmp(rcx);  // Return to IC Miss stub, continuation still on stack.
}


void LoadICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(LoadWithVectorDescriptor::VectorRegister());
  LoadICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


void KeyedLoadICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(LoadWithVectorDescriptor::VectorRegister());
  KeyedLoadICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


static void HandleArrayCases(MacroAssembler* masm, Register feedback,
                             Register receiver_map, Register scratch1,
                             Register scratch2, Register scratch3,
                             bool is_polymorphic, Label* miss) {
  // feedback initially contains the feedback array
  Label next_loop, prepare_next;
  Label start_polymorphic;

  Register counter = scratch1;
  Register length = scratch2;
  Register cached_map = scratch3;

  __ movp(cached_map, FieldOperand(feedback, FixedArray::OffsetOfElementAt(0)));
  __ cmpp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &start_polymorphic);

  // found, now call handler.
  Register handler = feedback;
  __ movp(handler, FieldOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ leap(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);

  // Polymorphic, we have to loop from 2 to N
  __ bind(&start_polymorphic);
  __ SmiToInteger32(length, FieldOperand(feedback, FixedArray::kLengthOffset));
  if (!is_polymorphic) {
    // If the IC could be monomorphic we have to make sure we don't go past the
    // end of the feedback array.
    __ cmpl(length, Immediate(2));
    __ j(equal, miss);
  }
  __ movl(counter, Immediate(2));

  __ bind(&next_loop);
  __ movp(cached_map, FieldOperand(feedback, counter, times_pointer_size,
                                   FixedArray::kHeaderSize));
  __ cmpp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &prepare_next);
  __ movp(handler, FieldOperand(feedback, counter, times_pointer_size,
                                FixedArray::kHeaderSize + kPointerSize));
  __ leap(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);

  __ bind(&prepare_next);
  __ addl(counter, Immediate(2));
  __ cmpl(counter, length);
  __ j(less, &next_loop);

  // We exhausted our array of map handler pairs.
  __ jmp(miss);
}


static void HandleMonomorphicCase(MacroAssembler* masm, Register receiver,
                                  Register receiver_map, Register feedback,
                                  Register vector, Register integer_slot,
                                  Label* compare_map, Label* load_smi_map,
                                  Label* try_array) {
  __ JumpIfSmi(receiver, load_smi_map);
  __ movp(receiver_map, FieldOperand(receiver, 0));

  __ bind(compare_map);
  __ cmpp(receiver_map, FieldOperand(feedback, WeakCell::kValueOffset));
  __ j(not_equal, try_array);
  Register handler = feedback;
  __ movp(handler, FieldOperand(vector, integer_slot, times_pointer_size,
                                FixedArray::kHeaderSize + kPointerSize));
  __ leap(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);
}


void LoadICStub::Generate(MacroAssembler* masm) { GenerateImpl(masm, false); }


void LoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // rdx
  Register name = LoadWithVectorDescriptor::NameRegister();          // rcx
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // rbx
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // rax
  Register feedback = rdi;
  Register integer_slot = r8;
  Register receiver_map = r9;

  __ SmiToInteger32(integer_slot, slot);
  __ movp(feedback, FieldOperand(vector, integer_slot, times_pointer_size,
                                 FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector,
                        integer_slot, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ CompareRoot(FieldOperand(feedback, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);
  HandleArrayCases(masm, feedback, receiver_map, integer_slot, r11, r15, true,
                   &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &miss);
  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, Code::LOAD_IC, code_flags, receiver, name, feedback, no_reg);

  __ bind(&miss);
  LoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


void KeyedLoadICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void KeyedLoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void KeyedLoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // rdx
  Register key = LoadWithVectorDescriptor::NameRegister();           // rcx
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // rbx
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // rax
  Register feedback = rdi;
  Register integer_slot = r8;
  Register receiver_map = r9;

  __ SmiToInteger32(integer_slot, slot);
  __ movp(feedback, FieldOperand(vector, integer_slot, times_pointer_size,
                                 FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector,
                        integer_slot, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ CompareRoot(FieldOperand(feedback, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);

  // We have a polymorphic element handler.
  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);
  HandleArrayCases(masm, feedback, receiver_map, integer_slot, r11, r15, true,
                   &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ jmp(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmpp(key, feedback);
  __ j(not_equal, &miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ movp(feedback, FieldOperand(vector, integer_slot, times_pointer_size,
                                 FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, integer_slot, r11, r15, false,
                   &miss);

  __ bind(&miss);
  KeyedLoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


void VectorStoreICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(VectorStoreICDescriptor::VectorRegister());
  VectorStoreICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


void VectorKeyedStoreICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(VectorStoreICDescriptor::VectorRegister());
  VectorKeyedStoreICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


void VectorStoreICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void VectorStoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void VectorStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = VectorStoreICDescriptor::ReceiverRegister();  // rdx
  Register key = VectorStoreICDescriptor::NameRegister();           // rcx
  Register vector = VectorStoreICDescriptor::VectorRegister();      // rbx
  Register slot = VectorStoreICDescriptor::SlotRegister();          // rdi
  DCHECK(VectorStoreICDescriptor::ValueRegister().is(rax));         // rax
  Register feedback = r8;
  Register integer_slot = r9;
  Register receiver_map = r11;
  DCHECK(!AreAliased(feedback, integer_slot, vector, slot, receiver_map));

  __ SmiToInteger32(integer_slot, slot);
  __ movp(feedback, FieldOperand(vector, integer_slot, times_pointer_size,
                                 FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector,
                        integer_slot, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ CompareRoot(FieldOperand(feedback, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);
  HandleArrayCases(masm, feedback, receiver_map, integer_slot, r14, r15, true,
                   &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &miss);

  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::STORE_IC, code_flags,
                                               receiver, key, feedback, no_reg);

  __ bind(&miss);
  StoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


void VectorKeyedStoreICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void VectorKeyedStoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


static void HandlePolymorphicKeyedStoreCase(MacroAssembler* masm,
                                            Register receiver_map,
                                            Register feedback, Register scratch,
                                            Register scratch1,
                                            Register scratch2, Label* miss) {
  // feedback initially contains the feedback array
  Label next, next_loop, prepare_next;
  Label transition_call;

  Register cached_map = scratch;
  Register counter = scratch1;
  Register length = scratch2;

  // Polymorphic, we have to loop from 0 to N - 1
  __ movp(counter, Immediate(0));
  __ movp(length, FieldOperand(feedback, FixedArray::kLengthOffset));
  __ SmiToInteger32(length, length);

  __ bind(&next_loop);
  __ movp(cached_map, FieldOperand(feedback, counter, times_pointer_size,
                                   FixedArray::kHeaderSize));
  __ cmpp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &prepare_next);
  __ movp(cached_map, FieldOperand(feedback, counter, times_pointer_size,
                                   FixedArray::kHeaderSize + kPointerSize));
  __ CompareRoot(cached_map, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &transition_call);
  __ movp(feedback, FieldOperand(feedback, counter, times_pointer_size,
                                 FixedArray::kHeaderSize + 2 * kPointerSize));
  __ leap(feedback, FieldOperand(feedback, Code::kHeaderSize));
  __ jmp(feedback);

  __ bind(&transition_call);
  DCHECK(receiver_map.is(VectorStoreTransitionDescriptor::MapRegister()));
  __ movp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  // The weak cell may have been cleared.
  __ JumpIfSmi(receiver_map, miss);
  // Get the handler in value.
  __ movp(feedback, FieldOperand(feedback, counter, times_pointer_size,
                                 FixedArray::kHeaderSize + 2 * kPointerSize));
  __ leap(feedback, FieldOperand(feedback, Code::kHeaderSize));
  __ jmp(feedback);

  __ bind(&prepare_next);
  __ addl(counter, Immediate(3));
  __ cmpl(counter, length);
  __ j(less, &next_loop);

  // We exhausted our array of map handler pairs.
  __ jmp(miss);
}


void VectorKeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = VectorStoreICDescriptor::ReceiverRegister();  // rdx
  Register key = VectorStoreICDescriptor::NameRegister();           // rcx
  Register vector = VectorStoreICDescriptor::VectorRegister();      // rbx
  Register slot = VectorStoreICDescriptor::SlotRegister();          // rdi
  DCHECK(VectorStoreICDescriptor::ValueRegister().is(rax));         // rax
  Register feedback = r8;
  Register integer_slot = r9;
  Register receiver_map = r11;
  DCHECK(!AreAliased(feedback, integer_slot, vector, slot, receiver_map));

  __ SmiToInteger32(integer_slot, slot);
  __ movp(feedback, FieldOperand(vector, integer_slot, times_pointer_size,
                                 FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector,
                        integer_slot, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ CompareRoot(FieldOperand(feedback, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);
  HandlePolymorphicKeyedStoreCase(masm, receiver_map, feedback, integer_slot,
                                  r15, r14, &miss);

  __ bind(&not_array);
  Label try_poly_name;
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &try_poly_name);

  Handle<Code> megamorphic_stub =
      KeyedStoreIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ jmp(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmpp(key, feedback);
  __ j(not_equal, &miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ movp(feedback, FieldOperand(vector, integer_slot, times_pointer_size,
                                 FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, integer_slot, r14, r15, false,
                   &miss);

  __ bind(&miss);
  KeyedStoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


void CallICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(rbx);
  CallICStub stub(isolate(), state());
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    ProfileEntryHookStub stub(masm->isolate());
    masm->CallStub(&stub);
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
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
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
  Handle<Object> undefined_sentinel(
      masm->isolate()->heap()->undefined_value(),
      masm->isolate());

  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ testb(rdx, Immediate(1));
    __ j(not_zero, &normal_sequence);
  }

  // look at the first argument
  StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movp(rcx, args.GetArgumentOperand(0));
  __ testp(rcx, rcx);
  __ j(zero, &normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(masm->isolate(),
                                                  holey_initial,
                                                  DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(masm->isolate(),
                                            initial,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
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
    __ SmiAddConstant(FieldOperand(rbx, AllocationSite::kTransitionInfoOffset),
                      Smi::FromInt(kFastElementsKindPackedToHoley));

    __ bind(&normal_sequence);
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
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
  int to_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(isolate, kind);
    stub.GetCode();
    if (AllocationSite::GetMode(kind) != DONT_TRACK_ALLOCATION_SITE) {
      T stub1(isolate, kind, DISABLE_ALLOCATION_SITES);
      stub1.GetCode();
    }
  }
}


void ArrayConstructorStubBase::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArraySingleArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArrayNArgumentsConstructorStub>(
      isolate);
}


void InternalArrayConstructorStubBase::GenerateStubsAheadOfTime(
    Isolate* isolate) {
  ElementsKind kinds[2] = { FAST_ELEMENTS, FAST_HOLEY_ELEMENTS };
  for (int i = 0; i < 2; i++) {
    // For internal arrays we only need a few things
    InternalArrayNoArgumentConstructorStub stubh1(isolate, kinds[i]);
    stubh1.GetCode();
    InternalArraySingleArgumentConstructorStub stubh2(isolate, kinds[i]);
    stubh2.GetCode();
    InternalArrayNArgumentsConstructorStub stubh3(isolate, kinds[i]);
    stubh3.GetCode();
  }
}


void ArrayConstructorStub::GenerateDispatchToArrayStub(
    MacroAssembler* masm,
    AllocationSiteOverrideMode mode) {
  if (argument_count() == ANY) {
    Label not_zero_case, not_one_case;
    __ testp(rax, rax);
    __ j(not_zero, &not_zero_case);
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ cmpl(rax, Immediate(1));
    __ j(greater, &not_one_case);
    CreateArrayDispatchOneArgument(masm, mode);

    __ bind(&not_one_case);
    CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm, mode);
  } else if (argument_count() == NONE) {
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);
  } else if (argument_count() == ONE) {
    CreateArrayDispatchOneArgument(masm, mode);
  } else if (argument_count() == MORE_THAN_ONE) {
    CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm, mode);
  } else {
    UNREACHABLE();
  }
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
    // Will both indicate a NULL and a Smi.
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
  __ movp(rdx, FieldOperand(rbx, AllocationSite::kTransitionInfoOffset));
  __ SmiToInteger32(rdx, rdx);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ andp(rdx, Immediate(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing
  __ bind(&subclassing);
  switch (argument_count()) {
    case ANY:
    case MORE_THAN_ONE: {
      StackArgumentsAccessor args(rsp, rax);
      __ movp(args.GetReceiverOperand(), rdi);
      __ addp(rax, Immediate(3));
      break;
    }
    case NONE: {
      StackArgumentsAccessor args(rsp, 0);
      __ movp(args.GetReceiverOperand(), rdi);
      __ Set(rax, 3);
      break;
    }
    case ONE: {
      StackArgumentsAccessor args(rsp, 1);
      __ movp(args.GetReceiverOperand(), rdi);
      __ Set(rax, 4);
      break;
    }
  }
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
  InternalArrayNArgumentsConstructorStub stubN(isolate(), kind);
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
    // Will both indicate a NULL and a Smi.
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
    __ cmpl(rcx, Immediate(FAST_ELEMENTS));
    __ j(equal, &done);
    __ cmpl(rcx, Immediate(FAST_HOLEY_ELEMENTS));
    __ Assert(equal,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmpl(rcx, Immediate(FAST_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


void FastNewObjectStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdi    : target
  //  -- rdx    : new target
  //  -- rsi    : context
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertFunction(rdi);
  __ AssertReceiver(rdx);

  // Verify that the new target is a JSFunction.
  Label new_object;
  __ CmpObjectType(rdx, JS_FUNCTION_TYPE, rbx);
  __ j(not_equal, &new_object);

  // Load the initial map and verify that it's in fact a map.
  __ movp(rcx, FieldOperand(rdx, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(rcx, &new_object);
  __ CmpObjectType(rcx, MAP_TYPE, rbx);
  __ j(not_equal, &new_object);

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  __ cmpp(rdi, FieldOperand(rcx, Map::kConstructorOrBackPointerOffset));
  __ j(not_equal, &new_object);

  // Allocate the JSObject on the heap.
  Label allocate, done_allocate;
  __ movzxbl(rbx, FieldOperand(rcx, Map::kInstanceSizeOffset));
  __ leal(rbx, Operand(rbx, times_pointer_size, 0));
  __ Allocate(rbx, rax, rdi, no_reg, &allocate, NO_ALLOCATION_FLAGS);
  __ bind(&done_allocate);

  // Initialize the JSObject fields.
  __ movp(Operand(rax, JSObject::kMapOffset), rcx);
  __ LoadRoot(rbx, Heap::kEmptyFixedArrayRootIndex);
  __ movp(Operand(rax, JSObject::kPropertiesOffset), rbx);
  __ movp(Operand(rax, JSObject::kElementsOffset), rbx);
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ leap(rbx, Operand(rax, JSObject::kHeaderSize));

  // ----------- S t a t e -------------
  //  -- rax    : result (untagged)
  //  -- rbx    : result fields (untagged)
  //  -- rdi    : result end (untagged)
  //  -- rcx    : initial map
  //  -- rsi    : context
  //  -- rsp[0] : return address
  // -----------------------------------

  // Perform in-object slack tracking if requested.
  Label slack_tracking;
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  __ LoadRoot(r11, Heap::kUndefinedValueRootIndex);
  __ testl(FieldOperand(rcx, Map::kBitField3Offset),
           Immediate(Map::ConstructionCounter::kMask));
  __ j(not_zero, &slack_tracking, Label::kNear);
  {
    // Initialize all in-object fields with undefined.
    __ InitializeFieldsWithFiller(rbx, rdi, r11);

    // Add the object tag to make the JSObject real.
    STATIC_ASSERT(kHeapObjectTag == 1);
    __ incp(rax);
    __ Ret();
  }
  __ bind(&slack_tracking);
  {
    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    __ subl(FieldOperand(rcx, Map::kBitField3Offset),
            Immediate(1 << Map::ConstructionCounter::kShift));

    // Initialize the in-object fields with undefined.
    __ movzxbl(rdx, FieldOperand(rcx, Map::kUnusedPropertyFieldsOffset));
    __ negp(rdx);
    __ leap(rdx, Operand(rdi, rdx, times_pointer_size, 0));
    __ InitializeFieldsWithFiller(rbx, rdx, r11);

    // Initialize the remaining (reserved) fields with one pointer filler map.
    __ LoadRoot(r11, Heap::kOnePointerFillerMapRootIndex);
    __ InitializeFieldsWithFiller(rdx, rdi, r11);

    // Add the object tag to make the JSObject real.
    STATIC_ASSERT(kHeapObjectTag == 1);
    __ incp(rax);

    // Check if we can finalize the instance size.
    Label finalize;
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);
    __ testl(FieldOperand(rcx, Map::kBitField3Offset),
             Immediate(Map::ConstructionCounter::kMask));
    __ j(zero, &finalize, Label::kNear);
    __ Ret();

    // Finalize the instance size.
    __ bind(&finalize);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rax);
      __ Push(rcx);
      __ CallRuntime(Runtime::kFinalizeInstanceSize);
      __ Pop(rax);
    }
    __ Ret();
  }

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Integer32ToSmi(rbx, rbx);
    __ Push(rcx);
    __ Push(rbx);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(rcx);
  }
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ decp(rax);
  __ movzxbl(rbx, FieldOperand(rcx, Map::kInstanceSizeOffset));
  __ leap(rdi, Operand(rax, rbx, times_pointer_size, 0));
  __ jmp(&done_allocate);

  // Fall back to %NewObject.
  __ bind(&new_object);
  __ PopReturnAddressTo(rcx);
  __ Push(rdi);
  __ Push(rdx);
  __ PushReturnAddressFrom(rcx);
  __ TailCallRuntime(Runtime::kNewObject);
}


void FastNewRestParameterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdi    : function
  //  -- rsi    : context
  //  -- rbp    : frame pointer
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertFunction(rdi);

  // For Ignition we need to skip all possible handler/stub frames until
  // we reach the JavaScript frame for the function (similar to what the
  // runtime fallback implementation does). So make rdx point to that
  // JavaScript frame.
  {
    Label loop, loop_entry;
    __ movp(rdx, rbp);
    __ jmp(&loop_entry, Label::kNear);
    __ bind(&loop);
    __ movp(rdx, Operand(rdx, StandardFrameConstants::kCallerFPOffset));
    __ bind(&loop_entry);
    __ cmpp(rdi, Operand(rdx, StandardFrameConstants::kMarkerOffset));
    __ j(not_equal, &loop);
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ movp(rbx, Operand(rdx, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(rbx, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &no_rest_parameters, Label::kNear);

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ LoadSharedFunctionInfoSpecialField(
      rcx, rcx, SharedFunctionInfo::kFormalParameterCountOffset);
  __ SmiToInteger32(
      rax, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ subl(rax, rcx);
  __ j(greater, &rest_parameters);

  // Return an empty rest parameter array.
  __ bind(&no_rest_parameters);
  {
    // ----------- S t a t e -------------
    //  -- rsi    : context
    //  -- rsp[0] : return address
    // -----------------------------------

    // Allocate an empty rest parameter array.
    Label allocate, done_allocate;
    __ Allocate(JSArray::kSize, rax, rdx, rcx, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Setup the rest parameter array in rax.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, rcx);
    __ movp(FieldOperand(rax, JSArray::kMapOffset), rcx);
    __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
    __ movp(FieldOperand(rax, JSArray::kPropertiesOffset), rcx);
    __ movp(FieldOperand(rax, JSArray::kElementsOffset), rcx);
    __ movp(FieldOperand(rax, JSArray::kLengthOffset), Immediate(0));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret();

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(Smi::FromInt(JSArray::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace);
    }
    __ jmp(&done_allocate);
  }

  __ bind(&rest_parameters);
  {
    // Compute the pointer to the first rest parameter (skippping the receiver).
    __ leap(rbx, Operand(rbx, rax, times_pointer_size,
                         StandardFrameConstants::kCallerSPOffset -
                             1 * kPointerSize));

    // ----------- S t a t e -------------
    //  -- rsi    : context
    //  -- rax    : number of rest parameters
    //  -- rbx    : pointer to first rest parameters
    //  -- rsp[0] : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ leal(rcx, Operand(rax, times_pointer_size,
                         JSArray::kSize + FixedArray::kHeaderSize));
    __ Allocate(rcx, rdx, rdi, no_reg, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Compute the arguments.length in rdi.
    __ Integer32ToSmi(rdi, rax);

    // Setup the elements array in rdx.
    __ LoadRoot(rcx, Heap::kFixedArrayMapRootIndex);
    __ movp(FieldOperand(rdx, FixedArray::kMapOffset), rcx);
    __ movp(FieldOperand(rdx, FixedArray::kLengthOffset), rdi);
    {
      Label loop, done_loop;
      __ Set(rcx, 0);
      __ bind(&loop);
      __ cmpl(rcx, rax);
      __ j(equal, &done_loop, Label::kNear);
      __ movp(kScratchRegister, Operand(rbx, 0 * kPointerSize));
      __ movp(
          FieldOperand(rdx, rcx, times_pointer_size, FixedArray::kHeaderSize),
          kScratchRegister);
      __ subp(rbx, Immediate(1 * kPointerSize));
      __ addl(rcx, Immediate(1));
      __ jmp(&loop);
      __ bind(&done_loop);
    }

    // Setup the rest parameter array in rax.
    __ leap(rax,
            Operand(rdx, rax, times_pointer_size, FixedArray::kHeaderSize));
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, rcx);
    __ movp(FieldOperand(rax, JSArray::kMapOffset), rcx);
    __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
    __ movp(FieldOperand(rax, JSArray::kPropertiesOffset), rcx);
    __ movp(FieldOperand(rax, JSArray::kElementsOffset), rdx);
    __ movp(FieldOperand(rax, JSArray::kLengthOffset), rdi);
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret();

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Integer32ToSmi(rax, rax);
      __ Integer32ToSmi(rcx, rcx);
      __ Push(rax);
      __ Push(rbx);
      __ Push(rcx);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ movp(rdx, rax);
      __ Pop(rbx);
      __ Pop(rax);
      __ SmiToInteger32(rax, rax);
    }
    __ jmp(&done_allocate);
  }
}


void FastNewSloppyArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdi    : function
  //  -- rsi    : context
  //  -- rbp    : frame pointer
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertFunction(rdi);

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ LoadSharedFunctionInfoSpecialField(
      rcx, rcx, SharedFunctionInfo::kFormalParameterCountOffset);
  __ leap(rdx, Operand(rbp, rcx, times_pointer_size,
                       StandardFrameConstants::kCallerSPOffset));
  __ Integer32ToSmi(rcx, rcx);

  // rcx : number of parameters (tagged)
  // rdx : parameters pointer
  // rdi : function
  // rsp[0] : return address
  // Registers used over the whole function:
  //  rbx: the mapped parameter count (untagged)
  //  rax: the allocated object (tagged).
  Factory* factory = isolate()->factory();

  __ SmiToInteger64(rbx, rcx);
  // rbx = parameter count (untagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ movp(rax, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movp(r8, Operand(rax, StandardFrameConstants::kContextOffset));
  __ Cmp(r8, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // No adaptor, parameter count = argument count.
  __ movp(r11, rbx);
  __ jmp(&try_allocate, Label::kNear);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ SmiToInteger64(
      r11, Operand(rax, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ leap(rdx, Operand(rax, r11, times_pointer_size,
                       StandardFrameConstants::kCallerSPOffset));

  // rbx = parameter count (untagged)
  // r11 = argument count (untagged)
  // Compute the mapped parameter count = min(rbx, r11) in rbx.
  __ cmpp(rbx, r11);
  __ j(less_equal, &try_allocate, Label::kNear);
  __ movp(rbx, r11);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  Label no_parameter_map;
  __ xorp(r8, r8);
  __ testp(rbx, rbx);
  __ j(zero, &no_parameter_map, Label::kNear);
  __ leap(r8, Operand(rbx, times_pointer_size, kParameterMapHeaderSize));
  __ bind(&no_parameter_map);

  // 2. Backing store.
  __ leap(r8, Operand(r8, r11, times_pointer_size, FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ addp(r8, Immediate(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(r8, rax, r9, no_reg, &runtime, TAG_OBJECT);

  // rax = address of new object(s) (tagged)
  // r11 = argument count (untagged)
  // Get the arguments map from the current native context into r9.
  Label has_mapped_parameters, instantiate;
  __ movp(r9, NativeContextOperand());
  __ testp(rbx, rbx);
  __ j(not_zero, &has_mapped_parameters, Label::kNear);

  const int kIndex = Context::SLOPPY_ARGUMENTS_MAP_INDEX;
  __ movp(r9, Operand(r9, Context::SlotOffset(kIndex)));
  __ jmp(&instantiate, Label::kNear);

  const int kAliasedIndex = Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX;
  __ bind(&has_mapped_parameters);
  __ movp(r9, Operand(r9, Context::SlotOffset(kAliasedIndex)));
  __ bind(&instantiate);

  // rax = address of new object (tagged)
  // rbx = mapped parameter count (untagged)
  // r11 = argument count (untagged)
  // r9 = address of arguments map (tagged)
  __ movp(FieldOperand(rax, JSObject::kMapOffset), r9);
  __ LoadRoot(kScratchRegister, Heap::kEmptyFixedArrayRootIndex);
  __ movp(FieldOperand(rax, JSObject::kPropertiesOffset), kScratchRegister);
  __ movp(FieldOperand(rax, JSObject::kElementsOffset), kScratchRegister);

  // Set up the callee in-object property.
  __ AssertNotSmi(rdi);
  __ movp(FieldOperand(rax, JSSloppyArgumentsObject::kCalleeOffset), rdi);

  // Use the length (smi tagged) and set that as an in-object property too.
  // Note: r11 is tagged from here on.
  __ Integer32ToSmi(r11, r11);
  __ movp(FieldOperand(rax, JSSloppyArgumentsObject::kLengthOffset), r11);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, rdi will point there, otherwise to the
  // backing store.
  __ leap(rdi, Operand(rax, JSSloppyArgumentsObject::kSize));
  __ movp(FieldOperand(rax, JSObject::kElementsOffset), rdi);

  // rax = address of new object (tagged)
  // rbx = mapped parameter count (untagged)
  // r11 = argument count (tagged)
  // rdi = address of parameter map or backing store (tagged)

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ testp(rbx, rbx);
  __ j(zero, &skip_parameter_map);

  __ LoadRoot(kScratchRegister, Heap::kSloppyArgumentsElementsMapRootIndex);
  // rbx contains the untagged argument count. Add 2 and tag to write.
  __ movp(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);
  __ Integer64PlusConstantToSmi(r9, rbx, 2);
  __ movp(FieldOperand(rdi, FixedArray::kLengthOffset), r9);
  __ movp(FieldOperand(rdi, FixedArray::kHeaderSize + 0 * kPointerSize), rsi);
  __ leap(r9, Operand(rdi, rbx, times_pointer_size, kParameterMapHeaderSize));
  __ movp(FieldOperand(rdi, FixedArray::kHeaderSize + 1 * kPointerSize), r9);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;

  // Load tagged parameter count into r9.
  __ Integer32ToSmi(r9, rbx);
  __ Move(r8, Smi::FromInt(Context::MIN_CONTEXT_SLOTS));
  __ addp(r8, rcx);
  __ subp(r8, r9);
  __ movp(rcx, rdi);
  __ leap(rdi, Operand(rdi, rbx, times_pointer_size, kParameterMapHeaderSize));
  __ SmiToInteger64(r9, r9);
  // r9 = loop variable (untagged)
  // r8 = mapping index (tagged)
  // rcx = address of parameter map (tagged)
  // rdi = address of backing store (tagged)
  __ jmp(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ subp(r9, Immediate(1));
  __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
  __ movp(FieldOperand(rcx, r9, times_pointer_size, kParameterMapHeaderSize),
          r8);
  __ movp(FieldOperand(rdi, r9, times_pointer_size, FixedArray::kHeaderSize),
          kScratchRegister);
  __ SmiAddConstant(r8, r8, Smi::FromInt(1));
  __ bind(&parameters_test);
  __ testp(r9, r9);
  __ j(not_zero, &parameters_loop, Label::kNear);

  __ bind(&skip_parameter_map);

  // r11 = argument count (tagged)
  // rdi = address of backing store (tagged)
  // Copy arguments header and remaining slots (if there are any).
  __ Move(FieldOperand(rdi, FixedArray::kMapOffset),
          factory->fixed_array_map());
  __ movp(FieldOperand(rdi, FixedArray::kLengthOffset), r11);

  Label arguments_loop, arguments_test;
  __ movp(r8, rbx);
  // Untag r11 for the loop below.
  __ SmiToInteger64(r11, r11);
  __ leap(kScratchRegister, Operand(r8, times_pointer_size, 0));
  __ subp(rdx, kScratchRegister);
  __ jmp(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ subp(rdx, Immediate(kPointerSize));
  __ movp(r9, Operand(rdx, 0));
  __ movp(FieldOperand(rdi, r8,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          r9);
  __ addp(r8, Immediate(1));

  __ bind(&arguments_test);
  __ cmpp(r8, r11);
  __ j(less, &arguments_loop, Label::kNear);

  // Return.
  __ ret(0);

  // Do the runtime call to allocate the arguments object.
  // r11 = argument count (untagged)
  __ bind(&runtime);
  __ Integer32ToSmi(r11, r11);
  __ PopReturnAddressTo(rax);
  __ Push(rdi);  // Push function.
  __ Push(rdx);  // Push parameters pointer.
  __ Push(r11);  // Push parameter count.
  __ PushReturnAddressFrom(rax);
  __ TailCallRuntime(Runtime::kNewSloppyArguments);
}


void FastNewStrictArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rdi    : function
  //  -- rsi    : context
  //  -- rbp    : frame pointer
  //  -- rsp[0] : return address
  // -----------------------------------
  __ AssertFunction(rdi);

  // For Ignition we need to skip all possible handler/stub frames until
  // we reach the JavaScript frame for the function (similar to what the
  // runtime fallback implementation does). So make rdx point to that
  // JavaScript frame.
  {
    Label loop, loop_entry;
    __ movp(rdx, rbp);
    __ jmp(&loop_entry, Label::kNear);
    __ bind(&loop);
    __ movp(rdx, Operand(rdx, StandardFrameConstants::kCallerFPOffset));
    __ bind(&loop_entry);
    __ cmpp(rdi, Operand(rdx, StandardFrameConstants::kMarkerOffset));
    __ j(not_equal, &loop);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ movp(rbx, Operand(rdx, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(rbx, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ movp(rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ LoadSharedFunctionInfoSpecialField(
        rax, rax, SharedFunctionInfo::kFormalParameterCountOffset);
    __ leap(rbx, Operand(rdx, rax, times_pointer_size,
                         StandardFrameConstants::kCallerSPOffset -
                             1 * kPointerSize));
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    __ SmiToInteger32(
        rax, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ leap(rbx, Operand(rbx, rax, times_pointer_size,
                         StandardFrameConstants::kCallerSPOffset -
                             1 * kPointerSize));
  }
  __ bind(&arguments_done);

  // ----------- S t a t e -------------
  //  -- rax    : number of arguments
  //  -- rbx    : pointer to the first argument
  //  -- rsi    : context
  //  -- rsp[0] : return address
  // -----------------------------------

  // Allocate space for the strict arguments object plus the backing store.
  Label allocate, done_allocate;
  __ leal(rcx, Operand(rax, times_pointer_size, JSStrictArgumentsObject::kSize +
                                                    FixedArray::kHeaderSize));
  __ Allocate(rcx, rdx, rdi, no_reg, &allocate, TAG_OBJECT);
  __ bind(&done_allocate);

  // Compute the arguments.length in rdi.
  __ Integer32ToSmi(rdi, rax);

  // Setup the elements array in rdx.
  __ LoadRoot(rcx, Heap::kFixedArrayMapRootIndex);
  __ movp(FieldOperand(rdx, FixedArray::kMapOffset), rcx);
  __ movp(FieldOperand(rdx, FixedArray::kLengthOffset), rdi);
  {
    Label loop, done_loop;
    __ Set(rcx, 0);
    __ bind(&loop);
    __ cmpl(rcx, rax);
    __ j(equal, &done_loop, Label::kNear);
    __ movp(kScratchRegister, Operand(rbx, 0 * kPointerSize));
    __ movp(
        FieldOperand(rdx, rcx, times_pointer_size, FixedArray::kHeaderSize),
        kScratchRegister);
    __ subp(rbx, Immediate(1 * kPointerSize));
    __ addl(rcx, Immediate(1));
    __ jmp(&loop);
    __ bind(&done_loop);
  }

  // Setup the strict arguments object in rax.
  __ leap(rax,
          Operand(rdx, rax, times_pointer_size, FixedArray::kHeaderSize));
  __ LoadNativeContextSlot(Context::STRICT_ARGUMENTS_MAP_INDEX, rcx);
  __ movp(FieldOperand(rax, JSStrictArgumentsObject::kMapOffset), rcx);
  __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
  __ movp(FieldOperand(rax, JSStrictArgumentsObject::kPropertiesOffset), rcx);
  __ movp(FieldOperand(rax, JSStrictArgumentsObject::kElementsOffset), rdx);
  __ movp(FieldOperand(rax, JSStrictArgumentsObject::kLengthOffset), rdi);
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ Ret();

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Integer32ToSmi(rax, rax);
    __ Integer32ToSmi(rcx, rcx);
    __ Push(rax);
    __ Push(rbx);
    __ Push(rcx);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ movp(rdx, rax);
    __ Pop(rbx);
    __ Pop(rax);
    __ SmiToInteger32(rax, rax);
  }
  __ jmp(&done_allocate);
}


void LoadGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = rsi;
  Register slot_reg = rbx;
  Register result_reg = rax;
  Label slow_case;

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ movp(rdi, ContextOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = rdi;
  }

  // Load the PropertyCell value at the specified slot.
  __ movp(result_reg, ContextOperand(context_reg, slot_reg));
  __ movp(result_reg, FieldOperand(result_reg, PropertyCell::kValueOffset));

  // Check that value is not the_hole.
  __ CompareRoot(result_reg, Heap::kTheHoleValueRootIndex);
  __ j(equal, &slow_case, Label::kNear);
  __ Ret();

  // Fallback to the runtime.
  __ bind(&slow_case);
  __ Integer32ToSmi(slot_reg, slot_reg);
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(slot_reg);
  __ Push(kScratchRegister);
  __ TailCallRuntime(Runtime::kLoadGlobalViaContext);
}


void StoreGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = rsi;
  Register slot_reg = rbx;
  Register value_reg = rax;
  Register cell_reg = r8;
  Register cell_details_reg = rdx;
  Register cell_value_reg = r9;
  Label fast_heapobject_case, fast_smi_case, slow_case;

  if (FLAG_debug_code) {
    __ CompareRoot(value_reg, Heap::kTheHoleValueRootIndex);
    __ Check(not_equal, kUnexpectedValue);
  }

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ movp(rdi, ContextOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = rdi;
  }

  // Load the PropertyCell at the specified slot.
  __ movp(cell_reg, ContextOperand(context_reg, slot_reg));

  // Load PropertyDetails for the cell (actually only the cell_type, kind and
  // READ_ONLY bit of attributes).
  __ SmiToInteger32(cell_details_reg,
                    FieldOperand(cell_reg, PropertyCell::kDetailsOffset));
  __ andl(cell_details_reg,
          Immediate(PropertyDetails::PropertyCellTypeField::kMask |
                    PropertyDetails::KindField::kMask |
                    PropertyDetails::kAttributesReadOnlyMask));

  // Check if PropertyCell holds mutable data.
  Label not_mutable_data;
  __ cmpl(cell_details_reg,
          Immediate(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kMutable) |
                    PropertyDetails::KindField::encode(kData)));
  __ j(not_equal, &not_mutable_data);
  __ JumpIfSmi(value_reg, &fast_smi_case);
  __ bind(&fast_heapobject_case);
  __ movp(FieldOperand(cell_reg, PropertyCell::kValueOffset), value_reg);
  __ RecordWriteField(cell_reg, PropertyCell::kValueOffset, value_reg,
                      cell_value_reg, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // RecordWriteField clobbers the value register, so we need to reload.
  __ movp(value_reg, FieldOperand(cell_reg, PropertyCell::kValueOffset));
  __ Ret();
  __ bind(&not_mutable_data);

  // Check if PropertyCell value matches the new value (relevant for Constant,
  // ConstantType and Undefined cells).
  Label not_same_value;
  __ movp(cell_value_reg, FieldOperand(cell_reg, PropertyCell::kValueOffset));
  __ cmpp(cell_value_reg, value_reg);
  __ j(not_equal, &not_same_value,
       FLAG_debug_code ? Label::kFar : Label::kNear);
  // Make sure the PropertyCell is not marked READ_ONLY.
  __ testl(cell_details_reg,
           Immediate(PropertyDetails::kAttributesReadOnlyMask));
  __ j(not_zero, &slow_case);
  if (FLAG_debug_code) {
    Label done;
    // This can only be true for Constant, ConstantType and Undefined cells,
    // because we never store the_hole via this stub.
    __ cmpl(cell_details_reg,
            Immediate(PropertyDetails::PropertyCellTypeField::encode(
                          PropertyCellType::kConstant) |
                      PropertyDetails::KindField::encode(kData)));
    __ j(equal, &done);
    __ cmpl(cell_details_reg,
            Immediate(PropertyDetails::PropertyCellTypeField::encode(
                          PropertyCellType::kConstantType) |
                      PropertyDetails::KindField::encode(kData)));
    __ j(equal, &done);
    __ cmpl(cell_details_reg,
            Immediate(PropertyDetails::PropertyCellTypeField::encode(
                          PropertyCellType::kUndefined) |
                      PropertyDetails::KindField::encode(kData)));
    __ Check(equal, kUnexpectedValue);
    __ bind(&done);
  }
  __ Ret();
  __ bind(&not_same_value);

  // Check if PropertyCell contains data with constant type (and is not
  // READ_ONLY).
  __ cmpl(cell_details_reg,
          Immediate(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kConstantType) |
                    PropertyDetails::KindField::encode(kData)));
  __ j(not_equal, &slow_case, Label::kNear);

  // Now either both old and new values must be SMIs or both must be heap
  // objects with same map.
  Label value_is_heap_object;
  __ JumpIfNotSmi(value_reg, &value_is_heap_object, Label::kNear);
  __ JumpIfNotSmi(cell_value_reg, &slow_case, Label::kNear);
  // Old and new values are SMIs, no need for a write barrier here.
  __ bind(&fast_smi_case);
  __ movp(FieldOperand(cell_reg, PropertyCell::kValueOffset), value_reg);
  __ Ret();
  __ bind(&value_is_heap_object);
  __ JumpIfSmi(cell_value_reg, &slow_case, Label::kNear);
  Register cell_value_map_reg = cell_value_reg;
  __ movp(cell_value_map_reg,
          FieldOperand(cell_value_reg, HeapObject::kMapOffset));
  __ cmpp(cell_value_map_reg, FieldOperand(value_reg, HeapObject::kMapOffset));
  __ j(equal, &fast_heapobject_case);

  // Fallback to the runtime.
  __ bind(&slow_case);
  __ Integer32ToSmi(slot_reg, slot_reg);
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(slot_reg);
  __ Push(value_reg);
  __ Push(kScratchRegister);
  __ TailCallRuntime(is_strict(language_mode())
                         ? Runtime::kStoreGlobalViaContext_Strict
                         : Runtime::kStoreGlobalViaContext_Sloppy);
}


static int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}


// Prepares stack to put arguments (aligns and so on).  WIN64 calling
// convention requires to put the pointer to the return value slot into
// rcx (rcx must be preserverd until CallApiFunctionAndReturn).  Saves
// context (rsi).  Clobbers rax.  Allocates arg_stack_space * kPointerSize
// inside the exit frame (not GCed) accessible via StackSpaceOperand.
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
                                     Operand return_value_operand,
                                     Operand* context_restore_operand) {
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

  DCHECK(rdx.is(function_address) || r8.is(function_address));
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
  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ movp(rsi, *context_restore_operand);
  }
  if (stack_space_operand != nullptr) {
    __ movp(rbx, *stack_space_operand);
  }
  __ LeaveApiExitFrame(!restore_context);

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

static void CallApiFunctionStubHelper(MacroAssembler* masm,
                                      const ParameterCount& argc,
                                      bool return_first_arg,
                                      bool call_data_undefined, bool is_lazy) {
  // ----------- S t a t e -------------
  //  -- rdi                 : callee
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

  Register callee = rdi;
  Register call_data = rbx;
  Register holder = rcx;
  Register api_function_address = rdx;
  Register context = rsi;
  Register return_address = r8;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kArgsLength == 7);

  DCHECK(argc.is_immediate() || rax.is(argc.reg()));

  __ PopReturnAddressTo(return_address);

  // context save
  __ Push(context);

  // callee
  __ Push(callee);

  // call data
  __ Push(call_data);
  Register scratch = call_data;
  if (!call_data_undefined) {
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  }
  // return value
  __ Push(scratch);
  // return value default
  __ Push(scratch);
  // isolate
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Push(scratch);
  // holder
  __ Push(holder);

  __ movp(scratch, rsp);
  // Push return address back on stack.
  __ PushReturnAddressFrom(return_address);

  if (!is_lazy) {
    // load context from callee
    __ movp(context, FieldOperand(callee, JSFunction::kContextOffset));
  }

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  PrepareCallApiFunction(masm, kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_.
  __ movp(StackSpaceOperand(0), scratch);
  if (argc.is_immediate()) {
    __ addp(scratch, Immediate((argc.immediate() + FCA::kArgsLength - 1) *
                               kPointerSize));
    // FunctionCallbackInfo::values_.
    __ movp(StackSpaceOperand(1), scratch);
    // FunctionCallbackInfo::length_.
    __ Set(StackSpaceOperand(2), argc.immediate());
    // FunctionCallbackInfo::is_construct_call_.
    __ Set(StackSpaceOperand(3), 0);
  } else {
    __ leap(scratch, Operand(scratch, argc.reg(), times_pointer_size,
                             (FCA::kArgsLength - 1) * kPointerSize));
    // FunctionCallbackInfo::values_.
    __ movp(StackSpaceOperand(1), scratch);
    // FunctionCallbackInfo::length_.
    __ movp(StackSpaceOperand(2), argc.reg());
    // FunctionCallbackInfo::is_construct_call_.
    __ leap(argc.reg(), Operand(argc.reg(), times_pointer_size,
                                (FCA::kArgsLength + 1) * kPointerSize));
    __ movp(StackSpaceOperand(3), argc.reg());
  }

#if defined(__MINGW64__) || defined(_WIN64)
  Register arguments_arg = rcx;
  Register callback_arg = rdx;
#else
  Register arguments_arg = rdi;
  Register callback_arg = rsi;
#endif

  // It's okay if api_function_address == callback_arg
  // but not arguments_arg
  DCHECK(!api_function_address.is(arguments_arg));

  // v8::InvocationCallback's argument.
  __ leap(arguments_arg, StackSpaceOperand(0));

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  // Accessor for FunctionCallbackInfo and first js arg.
  StackArgumentsAccessor args_from_rbp(rbp, FCA::kArgsLength + 1,
                                       ARGUMENTS_DONT_CONTAIN_RECEIVER);
  Operand context_restore_operand = args_from_rbp.GetArgumentOperand(
      FCA::kArgsLength - FCA::kContextSaveIndex);
  Operand is_construct_call_operand = StackSpaceOperand(3);
  Operand return_value_operand = args_from_rbp.GetArgumentOperand(
      return_first_arg ? 0 : FCA::kArgsLength - FCA::kReturnValueOffset);
  int stack_space = 0;
  Operand* stack_space_operand = &is_construct_call_operand;
  if (argc.is_immediate()) {
    stack_space = argc.immediate() + FCA::kArgsLength + 1;
    stack_space_operand = nullptr;
  }
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, callback_arg,
                           stack_space, stack_space_operand,
                           return_value_operand, &context_restore_operand);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
  bool call_data_undefined = this->call_data_undefined();
  CallApiFunctionStubHelper(masm, ParameterCount(rax), false,
                            call_data_undefined, false);
}


void CallApiAccessorStub::Generate(MacroAssembler* masm) {
  bool is_store = this->is_store();
  int argc = this->argc();
  bool call_data_undefined = this->call_data_undefined();
  bool is_lazy = this->is_lazy();
  CallApiFunctionStubHelper(masm, ParameterCount(argc), is_store,
                            call_data_undefined, is_lazy);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rsp[0]                          : return address
  //  -- rsp[8]                          : name
  //  -- rsp[16 .. (16 + kArgsLength*8)] : v8::PropertyCallbackInfo::args_
  //  -- ...
  //  -- r8                              : api_function_address
  // -----------------------------------

#if defined(__MINGW64__) || defined(_WIN64)
  Register getter_arg = r8;
  Register accessor_info_arg = rdx;
  Register name_arg = rcx;
#else
  Register getter_arg = rdx;
  Register accessor_info_arg = rsi;
  Register name_arg = rdi;
#endif
  Register api_function_address = ApiGetterDescriptor::function_address();
  DCHECK(api_function_address.is(r8));
  Register scratch = rax;

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
  DCHECK(!api_function_address.is(accessor_info_arg) &&
         !api_function_address.is(name_arg));

  // +3 is to skip prolog, return address and name handle.
  Operand return_value_operand(
      rbp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, getter_arg,
                           kStackUnwindSpace, nullptr, return_value_operand,
                           NULL);
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
