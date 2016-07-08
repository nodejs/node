// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/ia32/code-stubs-ia32.h"
#include "src/ia32/frames-ia32.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


static void InitializeArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- allocation site with elements kind
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(eax, deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  }
}


static void InitializeInternalArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // eax -- number of arguments
  // edi -- constructor function
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kInternalArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(eax, deopt_handler, constant_stack_parameter_count,
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
           eax.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor.GetRegisterParameter(i));
    }
    __ CallExternalReference(miss, param_count);
  }

  __ ret(0);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ pushad();
  if (save_doubles()) {
    __ sub(esp, Immediate(kDoubleSize * XMMRegister::kMaxNumRegisters));
    for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      __ movsd(Operand(esp, i * kDoubleSize), reg);
    }
  }
  const int argument_count = 1;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, ecx);
  __ mov(Operand(esp, 0 * kPointerSize),
         Immediate(ExternalReference::isolate_address(isolate())));
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()),
      argument_count);
  if (save_doubles()) {
    for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      __ movsd(reg, Operand(esp, i * kDoubleSize));
    }
    __ add(esp, Immediate(kDoubleSize * XMMRegister::kMaxNumRegisters));
  }
  __ popad();
  __ ret(0);
}


class FloatingPointHelper : public AllStatic {
 public:
  enum ArgLocation {
    ARGS_ON_STACK,
    ARGS_IN_REGISTERS
  };

  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand in register number. Returns operand as floating point number
  // on FPU stack.
  static void LoadFloatOperand(MacroAssembler* masm, Register number);

  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in eax, operand_2 in edx; falls through on float
  // operands, jumps to the non_float label otherwise.
  static void CheckFloatOperands(MacroAssembler* masm,
                                 Label* non_float,
                                 Register scratch);

  // Test if operands are numbers (smi or HeapNumber objects), and load
  // them into xmm0 and xmm1 if they are.  Jump to label not_numbers if
  // either operand is not a number.  Operands are in edx and eax.
  // Leaves operands unchanged.
  static void LoadSSE2Operands(MacroAssembler* masm, Label* not_numbers);
};


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Register input_reg = this->source();
  Register final_result_reg = this->destination();
  DCHECK(is_truncating());

  Label check_negative, process_64_bits, done, done_no_stash;

  int double_offset = offset();

  // Account for return address and saved regs if input is esp.
  if (input_reg.is(esp)) double_offset += 3 * kPointerSize;

  MemOperand mantissa_operand(MemOperand(input_reg, double_offset));
  MemOperand exponent_operand(MemOperand(input_reg,
                                         double_offset + kDoubleSize / 2));

  Register scratch1;
  {
    Register scratch_candidates[3] = { ebx, edx, edi };
    for (int i = 0; i < 3; i++) {
      scratch1 = scratch_candidates[i];
      if (!final_result_reg.is(scratch1) && !input_reg.is(scratch1)) break;
    }
  }
  // Since we must use ecx for shifts below, use some other register (eax)
  // to calculate the result if ecx is the requested return register.
  Register result_reg = final_result_reg.is(ecx) ? eax : final_result_reg;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead for
  // the result.
  Register save_reg = final_result_reg.is(ecx) ? eax : ecx;
  __ push(scratch1);
  __ push(save_reg);

  bool stash_exponent_copy = !input_reg.is(esp);
  __ mov(scratch1, mantissa_operand);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    // Load x87 register with heap number.
    __ fld_d(mantissa_operand);
  }
  __ mov(ecx, exponent_operand);
  if (stash_exponent_copy) __ push(ecx);

  __ and_(ecx, HeapNumber::kExponentMask);
  __ shr(ecx, HeapNumber::kExponentShift);
  __ lea(result_reg, MemOperand(ecx, -HeapNumber::kExponentBias));
  __ cmp(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits);

  // Result is entirely in lower 32-bits of mantissa
  int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
  if (CpuFeatures::IsSupported(SSE3)) {
    __ fstp(0);
  }
  __ sub(ecx, Immediate(delta));
  __ xor_(result_reg, result_reg);
  __ cmp(ecx, Immediate(31));
  __ j(above, &done);
  __ shl_cl(scratch1);
  __ jmp(&check_negative);

  __ bind(&process_64_bits);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    if (stash_exponent_copy) {
      // Already a copy of the exponent on the stack, overwrite it.
      STATIC_ASSERT(kDoubleSize == 2 * kPointerSize);
      __ sub(esp, Immediate(kDoubleSize / 2));
    } else {
      // Reserve space for 64 bit answer.
      __ sub(esp, Immediate(kDoubleSize));  // Nolint.
    }
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(result_reg, Operand(esp, 0));  // Load low word of answer as result
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&done_no_stash);
  } else {
    // Result must be extracted from shifted 32-bit mantissa
    __ sub(ecx, Immediate(delta));
    __ neg(ecx);
    if (stash_exponent_copy) {
      __ mov(result_reg, MemOperand(esp, 0));
    } else {
      __ mov(result_reg, exponent_operand);
    }
    __ and_(result_reg,
            Immediate(static_cast<uint32_t>(Double::kSignificandMask >> 32)));
    __ add(result_reg,
           Immediate(static_cast<uint32_t>(Double::kHiddenBit >> 32)));
    __ shrd(result_reg, scratch1);
    __ shr_cl(result_reg);
    __ test(ecx, Immediate(32));
    __ cmov(not_equal, scratch1, result_reg);
  }

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ mov(result_reg, scratch1);
  __ neg(result_reg);
  if (stash_exponent_copy) {
    __ cmp(MemOperand(esp, 0), Immediate(0));
  } else {
    __ cmp(exponent_operand, Immediate(0));
  }
    __ cmov(greater, result_reg, scratch1);

  // Restore registers
  __ bind(&done);
  if (stash_exponent_copy) {
    __ add(esp, Immediate(kDoubleSize / 2));
  }
  __ bind(&done_no_stash);
  if (!final_result_reg.is(result_reg)) {
    DCHECK(final_result_reg.is(ecx));
    __ mov(final_result_reg, result_reg);
  }
  __ pop(save_reg);
  __ pop(scratch1);
  __ ret(0);
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  Label load_smi, done;

  __ JumpIfSmi(number, &load_smi, Label::kNear);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done, Label::kNear);

  __ bind(&load_smi);
  __ SmiUntag(number);
  __ push(number);
  __ fild_s(Operand(esp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm,
                                           Label* not_numbers) {
  Label load_smi_edx, load_eax, load_smi_eax, load_float_eax, done;
  // Load operand in edx into xmm0, or branch to not_numbers.
  __ JumpIfSmi(edx, &load_smi_edx, Label::kNear);
  Factory* factory = masm->isolate()->factory();
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset), factory->heap_number_map());
  __ j(not_equal, not_numbers);  // Argument in edx is not a number.
  __ movsd(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
  __ bind(&load_eax);
  // Load operand in eax into xmm1, or branch to not_numbers.
  __ JumpIfSmi(eax, &load_smi_eax, Label::kNear);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset), factory->heap_number_map());
  __ j(equal, &load_float_eax, Label::kNear);
  __ jmp(not_numbers);  // Argument in eax is not a number.
  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ Cvtsi2sd(xmm0, edx);
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);
  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ Cvtsi2sd(xmm1, eax);
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.
  __ jmp(&done, Label::kNear);
  __ bind(&load_float_eax);
  __ movsd(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ bind(&done);
}


void FloatingPointHelper::CheckFloatOperands(MacroAssembler* masm,
                                             Label* non_float,
                                             Register scratch) {
  Label test_other, done;
  // Test if both operands are floats or smi -> scratch=k_is_float;
  // Otherwise scratch = k_not_float.
  __ JumpIfSmi(edx, &test_other, Label::kNear);
  __ mov(scratch, FieldOperand(edx, HeapObject::kMapOffset));
  Factory* factory = masm->isolate()->factory();
  __ cmp(scratch, factory->heap_number_map());
  __ j(not_equal, non_float);  // argument in edx is not a number -> NaN

  __ bind(&test_other);
  __ JumpIfSmi(eax, &done, Label::kNear);
  __ mov(scratch, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(scratch, factory->heap_number_map());
  __ j(not_equal, non_float);  // argument in eax is not a number -> NaN

  // Fall-through: Both operands are numbers.
  __ bind(&done);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  Factory* factory = isolate()->factory();
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(eax));
  const Register base = edx;
  const Register scratch = ecx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ mov(scratch, Immediate(1));
  __ Cvtsi2sd(double_result, scratch);

  if (exponent_type() == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack.
    __ mov(base, Operand(esp, 2 * kPointerSize));
    __ mov(exponent, Operand(esp, 1 * kPointerSize));

    __ JumpIfSmi(base, &base_is_smi, Label::kNear);
    __ cmp(FieldOperand(base, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(not_equal, &call_runtime);

    __ movsd(double_base, FieldOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ SmiUntag(base);
    __ Cvtsi2sd(double_base, base);

    __ bind(&unpack_exponent);
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiUntag(exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ cmp(FieldOperand(exponent, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(not_equal, &call_runtime);
    __ movsd(double_exponent,
              FieldOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type() == TAGGED) {
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiUntag(exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ movsd(double_exponent,
              FieldOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type() != INTEGER) {
    Label fast_power, try_arithmetic_simplification;
    __ DoubleToI(exponent, double_exponent, double_scratch,
                 TREAT_MINUS_ZERO_AS_ZERO, &try_arithmetic_simplification,
                 &try_arithmetic_simplification,
                 &try_arithmetic_simplification);
    __ jmp(&int_exponent);

    __ bind(&try_arithmetic_simplification);
    // Skip to runtime if possibly NaN (indicated by the indefinite integer).
    __ cvttsd2si(exponent, Operand(double_exponent));
    __ cmp(exponent, Immediate(0x1));
    __ j(overflow, &call_runtime);

    if (exponent_type() == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label continue_sqrt, continue_rsqrt, not_plus_half;
      // Test for 0.5.
      // Load double_scratch with 0.5.
      __ mov(scratch, Immediate(0x3F000000u));
      __ movd(double_scratch, scratch);
      __ cvtss2sd(double_scratch, double_scratch);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &not_plus_half, Label::kNear);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      // According to IEEE-754, single-precision -Infinity has the highest
      // 9 bits set and the lowest 23 bits cleared.
      __ mov(scratch, 0xFF800000u);
      __ movd(double_scratch, scratch);
      __ cvtss2sd(double_scratch, double_scratch);
      __ ucomisd(double_base, double_scratch);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_sqrt, Label::kNear);
      __ j(carry, &continue_sqrt, Label::kNear);

      // Set result to Infinity in the special case.
      __ xorps(double_result, double_result);
      __ subsd(double_result, double_scratch);
      __ jmp(&done);

      __ bind(&continue_sqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ xorps(double_scratch, double_scratch);
      __ addsd(double_scratch, double_base);  // Convert -0 to +0.
      __ sqrtsd(double_result, double_scratch);
      __ jmp(&done);

      // Test for -0.5.
      __ bind(&not_plus_half);
      // Load double_exponent with -0.5 by substracting 1.
      __ subsd(double_scratch, double_result);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &fast_power, Label::kNear);

      // Calculates reciprocal of square root of base.  Check for the special
      // case of Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      // According to IEEE-754, single-precision -Infinity has the highest
      // 9 bits set and the lowest 23 bits cleared.
      __ mov(scratch, 0xFF800000u);
      __ movd(double_scratch, scratch);
      __ cvtss2sd(double_scratch, double_scratch);
      __ ucomisd(double_base, double_scratch);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_rsqrt, Label::kNear);
      __ j(carry, &continue_rsqrt, Label::kNear);

      // Set result to 0 in the special case.
      __ xorps(double_result, double_result);
      __ jmp(&done);

      __ bind(&continue_rsqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ xorps(double_exponent, double_exponent);
      __ addsd(double_exponent, double_base);  // Convert -0 to +0.
      __ sqrtsd(double_exponent, double_exponent);
      __ divsd(double_result, double_exponent);
      __ jmp(&done);
    }

    // Using FPU instructions to calculate power.
    Label fast_power_failed;
    __ bind(&fast_power);
    __ fnclex();  // Clear flags to catch exceptions later.
    // Transfer (B)ase and (E)xponent onto the FPU register stack.
    __ sub(esp, Immediate(kDoubleSize));
    __ movsd(Operand(esp, 0), double_exponent);
    __ fld_d(Operand(esp, 0));  // E
    __ movsd(Operand(esp, 0), double_base);
    __ fld_d(Operand(esp, 0));  // B, E

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
    __ fstp(1);    // 2^X
    // Bail out to runtime in case of exceptions in the status word.
    __ fnstsw_ax();
    __ test_b(eax, 0x5F);  // We check for all but precision exception.
    __ j(not_zero, &fast_power_failed, Label::kNear);
    __ fstp_d(Operand(esp, 0));
    __ movsd(double_result, Operand(esp, 0));
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&done);

    __ bind(&fast_power_failed);
    __ fninit();
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&call_runtime);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  __ mov(scratch, exponent);  // Back up exponent.
  __ movsd(double_scratch, double_base);  // Back up base.
  __ movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, while_false;
  __ test(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ neg(scratch);
  __ bind(&no_neg);

  __ j(zero, &while_false, Label::kNear);
  __ shr(scratch, 1);
  // Above condition means CF==0 && ZF==0.  This means that the
  // bit that has been shifted out is 0 and the result is not 0.
  __ j(above, &while_true, Label::kNear);
  __ movsd(double_result, double_scratch);
  __ j(zero, &while_false, Label::kNear);

  __ bind(&while_true);
  __ shr(scratch, 1);
  __ mulsd(double_scratch, double_scratch);
  __ j(above, &while_true, Label::kNear);
  __ mulsd(double_result, double_scratch);
  __ j(not_zero, &while_true);

  __ bind(&while_false);
  // scratch has the original value of the exponent - if the exponent is
  // negative, return 1/result.
  __ test(exponent, exponent);
  __ j(positive, &done);
  __ divsd(double_scratch2, double_result);
  __ movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ xorps(double_scratch2, double_scratch2);
  __ ucomisd(double_scratch2, double_result);  // Result cannot be NaN.
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // exponent is a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ Cvtsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  if (exponent_type() == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMathPowRT);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in exponent.
    __ bind(&done);
    __ AllocateHeapNumber(eax, scratch, base, &call_runtime);
    __ movsd(FieldOperand(eax, HeapNumber::kValueOffset), double_result);
    __ ret(2 * kPointerSize);
  } else {
    __ bind(&call_runtime);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(4, scratch);
      __ movsd(Operand(esp, 0 * kDoubleSize), double_base);
      __ movsd(Operand(esp, 1 * kDoubleSize), double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 4);
    }
    // Return value is in st(0) on ia32.
    // Store it into the (fixed) result register.
    __ sub(esp, Immediate(kDoubleSize));
    __ fstp_d(Operand(esp, 0));
    __ movsd(double_result, Operand(esp, 0));
    __ add(esp, Immediate(kDoubleSize));

    __ bind(&done);
    __ ret(0);
  }
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // With careful management, we won't have to save slot and vector on
  // the stack. Simply handle the possibly missing case first.
  // TODO(mvstanton): this code can be more efficient.
  __ cmp(FieldOperand(receiver, JSFunction::kPrototypeOrInitialMapOffset),
         Immediate(isolate()->factory()->the_hole_value()));
  __ j(equal, &miss);
  __ TryGetFunctionPrototype(receiver, eax, ebx, &miss);
  __ ret(0);

  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void LoadIndexedInterceptorStub::Generate(MacroAssembler* masm) {
  // Return address is on the stack.
  Label slow;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();
  Register scratch = eax;
  DCHECK(!scratch.is(receiver) && !scratch.is(key));

  // Check that the key is an array index, that is Uint32.
  __ test(key, Immediate(kSmiTagMask | kSmiSignMask));
  __ j(not_zero, &slow);

  // Everything is fine, call runtime.
  __ pop(scratch);
  __ push(receiver);  // receiver
  __ push(key);       // key
  __ push(scratch);   // return address

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
  Register scratch = edi;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  Register result = eax;
  DCHECK(!result.is(scratch));
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
  //  esp[0]: return address
  //  esp[4]: last_match_info (expected JSArray)
  //  esp[8]: previous index
  //  esp[12]: subject string
  //  esp[16]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;
  Factory* factory = isolate()->factory();

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ mov(ebx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ test(ebx, ebx);
  __ j(zero, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &runtime);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ecx);
  __ j(not_equal, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Check(not_zero, kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CmpObjectType(ecx, FIXED_ARRAY_TYPE, ebx);
    __ Check(equal, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // ecx: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ mov(ebx, FieldOperand(ecx, JSRegExp::kDataTagOffset));
  __ cmp(ebx, Immediate(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ j(not_equal, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // Multiplying by 2 comes for free since edx is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmp(edx, Isolate::kJSRegexpStaticOffsetsVectorSize - 2);
  __ j(above, &runtime);

  // Reset offset for possibly sliced string.
  __ Move(edi, Immediate(0));
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ JumpIfSmi(eax, &runtime);
  __ mov(edx, eax);  // Make a copy of the original subject string.

  // eax: subject string
  // edx: subject string
  // ecx: RegExp data (FixedArray)
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
  // (1) Sequential two byte?  If yes, go to (9).
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));

  __ and_(ebx, kIsNotStringMask |
               kStringRepresentationMask |
               kStringEncodingMask |
               kShortExternalStringMask);
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).

  // (2) Sequential one byte?  If yes, go to (5).
  // Any other sequential string must be one byte.
  __ and_(ebx, Immediate(kIsNotStringMask |
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
  __ cmp(ebx, Immediate(kExternalStringTag));
  __ j(greater_equal, &not_seq_nor_cons);  // Go to (6).

  // (4) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ cmp(FieldOperand(eax, ConsString::kSecondOffset), factory->empty_string());
  __ j(not_equal, &runtime);
  __ mov(eax, FieldOperand(eax, ConsString::kFirstOffset));
  __ jmp(&check_underlying);

  // eax: sequential subject string (or look-alike, external string)
  // edx: original subject string
  // ecx: RegExp data (FixedArray)
  // (5) One byte sequential.  Load regexp code for one byte.
  __ bind(&seq_one_byte_string);
  // Load previous index and check range before edx is overwritten.  We have
  // to use edx instead of eax here because it might have been only made to
  // look like a sequential string when it actually is an external string.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ JumpIfNotSmi(ebx, &runtime);
  __ cmp(ebx, FieldOperand(edx, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataOneByteCodeOffset));
  __ Move(ecx, Immediate(1));  // Type is one byte.

  // (E) Carry on.  String handling is done.
  __ bind(&check_code);
  // edx: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(edx, &runtime);

  // eax: subject string
  // ebx: previous index (smi)
  // edx: code
  // ecx: encoding of subject string (1 if one_byte, 0 if two_byte);
  // All checks done. Now push arguments for native regexp code.
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->regexp_entry_native(), 1);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 9;
  __ EnterApiExitFrame(kRegExpExecuteArguments);

  // Argument 9: Pass current isolate address.
  __ mov(Operand(esp, 8 * kPointerSize),
      Immediate(ExternalReference::isolate_address(isolate())));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ mov(Operand(esp, 7 * kPointerSize), Immediate(1));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ mov(esi, Operand::StaticVariable(address_of_regexp_stack_memory_address));
  __ add(esi, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ mov(Operand(esp, 6 * kPointerSize), esi);

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  __ mov(Operand(esp, 5 * kPointerSize), Immediate(0));

  // Argument 5: static offsets vector buffer.
  __ mov(Operand(esp, 4 * kPointerSize),
         Immediate(ExternalReference::address_of_static_offsets_vector(
             isolate())));

  // Argument 2: Previous index.
  __ SmiUntag(ebx);
  __ mov(Operand(esp, 1 * kPointerSize), ebx);

  // Argument 1: Original subject string.
  // The original subject is in the previous stack frame. Therefore we have to
  // use ebp, which points exactly to one pointer size below the previous esp.
  // (Because creating a new stack frame pushes the previous ebp onto the stack
  // and thereby moves up esp by one kPointerSize.)
  __ mov(esi, Operand(ebp, kSubjectOffset + kPointerSize));
  __ mov(Operand(esp, 0 * kPointerSize), esi);

  // esi: original subject string
  // eax: underlying subject string
  // ebx: previous index
  // ecx: encoding of subject string (1 if one_byte 0 if two_byte);
  // edx: code
  // Argument 4: End of string data
  // Argument 3: Start of string data
  // Prepare start and end index of the input.
  // Load the length from the original sliced string if that is the case.
  __ mov(esi, FieldOperand(esi, String::kLengthOffset));
  __ add(esi, edi);  // Calculate input end wrt offset.
  __ SmiUntag(edi);
  __ add(ebx, edi);  // Calculate input start wrt offset.

  // ebx: start index of the input string
  // esi: end index of the input string
  Label setup_two_byte, setup_rest;
  __ test(ecx, ecx);
  __ j(zero, &setup_two_byte, Label::kNear);
  __ SmiUntag(esi);
  __ lea(ecx, FieldOperand(eax, esi, times_1, SeqOneByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_1, SeqOneByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.
  __ jmp(&setup_rest, Label::kNear);

  __ bind(&setup_two_byte);
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);  // esi is smi (powered by 2).
  __ lea(ecx, FieldOperand(eax, esi, times_1, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_2, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.

  __ bind(&setup_rest);

  // Locate the code entry and call it.
  __ add(edx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(edx);

  // Drop arguments and come back to JS mode.
  __ LeaveApiExitFrame(true);

  // Check the result.
  Label success;
  __ cmp(eax, 1);
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ j(equal, &success);
  Label failure;
  __ cmp(eax, NativeRegExpMacroAssembler::FAILURE);
  __ j(equal, &failure);
  __ cmp(eax, NativeRegExpMacroAssembler::EXCEPTION);
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate());
  __ mov(edx, Immediate(isolate()->factory()->the_hole_value()));
  __ mov(eax, Operand::StaticVariable(pending_exception));
  __ cmp(edx, eax);
  __ j(equal, &runtime);

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  __ bind(&failure);
  // For failure to match, return null.
  __ mov(eax, factory->null_value());
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(edx, Immediate(2));  // edx was a smi.

  // edx: Number of capture registers
  // Load last_match_info which is still known to be a fast case JSArray.
  // Check that the fourth object is a JSArray object.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ JumpIfSmi(eax, &runtime);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ mov(ebx, FieldOperand(eax, JSArray::kElementsOffset));
  __ mov(eax, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(eax, factory->fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ mov(eax, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ SmiUntag(eax);
  __ sub(eax, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmp(edx, eax);
  __ j(greater, &runtime);

  // ebx: last_match_info backing store (FixedArray)
  // edx: number of capture registers
  // Store the capture count.
  __ SmiTag(edx);  // Number of capture registers to smi.
  __ mov(FieldOperand(ebx, RegExpImpl::kLastCaptureCountOffset), edx);
  __ SmiUntag(edx);  // Number of capture registers back from smi.
  // Store last subject and last input.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(ecx, eax);
  __ mov(FieldOperand(ebx, RegExpImpl::kLastSubjectOffset), eax);
  __ RecordWriteField(ebx,
                      RegExpImpl::kLastSubjectOffset,
                      eax,
                      edi,
                      kDontSaveFPRegs);
  __ mov(eax, ecx);
  __ mov(FieldOperand(ebx, RegExpImpl::kLastInputOffset), eax);
  __ RecordWriteField(ebx,
                      RegExpImpl::kLastInputOffset,
                      eax,
                      edi,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ mov(ecx, Immediate(address_of_static_offsets_vector));

  // ebx: last_match_info backing store (FixedArray)
  // ecx: offsets vector
  // edx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ sub(edx, Immediate(1));
  __ j(negative, &done, Label::kNear);
  // Read the value from the static offsets vector buffer.
  __ mov(edi, Operand(ecx, edx, times_int_size, 0));
  __ SmiTag(edi);
  // Store the smi value in the last match info.
  __ mov(FieldOperand(ebx,
                      edx,
                      times_pointer_size,
                      RegExpImpl::kFirstCaptureOffset),
                      edi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ ret(4 * kPointerSize);

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
  // Reload instance type.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ test_b(ebx, kIsIndirectStringMask);
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  __ mov(eax, FieldOperand(eax, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ sub(eax, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // (8) Is the external string one byte?  If yes, go to (5).
  __ test_b(ebx, kStringEncodingMask);
  __ j(not_zero, &seq_one_byte_string);  // Go to (5).

  // eax: sequential subject string (or look-alike, external string)
  // edx: original subject string
  // ecx: RegExp data (FixedArray)
  // (9) Two byte sequential.  Load regexp code for two byte. Go to (E).
  __ bind(&seq_two_byte_string);
  // Load previous index and check range before edx is overwritten.  We have
  // to use edx instead of eax here because it might have been only made to
  // look like a sequential string when it actually is an external string.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ JumpIfNotSmi(ebx, &runtime);
  __ cmp(ebx, FieldOperand(edx, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataUC16CodeOffset));
  __ Move(ecx, Immediate(0));  // Type is two byte.
  __ jmp(&check_code);  // Go to (E).

  // (10) Not a string or a short external string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  // Catch non-string subject or short external string.
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ test(ebx, Immediate(kIsNotStringMask | kShortExternalStringTag));
  __ j(not_zero, &runtime);

  // (11) Sliced string.  Replace subject with parent.  Go to (1).
  // Load offset into edi and replace subject string with parent.
  __ mov(edi, FieldOperand(eax, SlicedString::kOffsetOffset));
  __ mov(eax, FieldOperand(eax, SlicedString::kParentOffset));
  __ jmp(&check_underlying);  // Go to (1).
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
    __ cmp(FieldOperand(input, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->heap_number_map()));
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
  __ mov(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ test(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, label);
}


void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Label runtime_call, check_unequal_objects;
  Condition cc = GetCondition();

  Label miss;
  CheckInputType(masm, edx, left(), &miss);
  CheckInputType(masm, eax, right(), &miss);

  // Compare two smis.
  Label non_smi, smi_done;
  __ mov(ecx, edx);
  __ or_(ecx, eax);
  __ JumpIfNotSmi(ecx, &non_smi, Label::kNear);
  __ sub(edx, eax);  // Return on the result of the subtraction.
  __ j(no_overflow, &smi_done, Label::kNear);
  __ not_(edx);  // Correct sign in case of overflow. edx is never 0 here.
  __ bind(&smi_done);
  __ mov(eax, edx);
  __ ret(0);
  __ bind(&non_smi);

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Identical objects can be compared fast, but there are some tricky cases
  // for NaN and undefined.
  Label generic_heap_number_comparison;
  {
    Label not_identical;
    __ cmp(eax, edx);
    __ j(not_equal, &not_identical);

    if (cc != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      __ cmp(edx, isolate()->factory()->undefined_value());
      Label check_for_nan;
      __ j(not_equal, &check_for_nan, Label::kNear);
      __ Move(eax, Immediate(Smi::FromInt(NegativeComparisonResult(cc))));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Compare heap numbers in a general way,
    // to handle NaNs correctly.
    __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
           Immediate(isolate()->factory()->heap_number_map()));
    __ j(equal, &generic_heap_number_comparison, Label::kNear);
    if (cc != equal) {
      __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
      __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
      // Call runtime on identical JSObjects.  Otherwise return equal.
      __ cmpb(ecx, static_cast<uint8_t>(FIRST_JS_RECEIVER_TYPE));
      __ j(above_equal, &runtime_call, Label::kFar);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ cmpb(ecx, static_cast<uint8_t>(SYMBOL_TYPE));
      __ j(equal, &runtime_call, Label::kFar);
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ cmpb(ecx, static_cast<uint8_t>(SIMD128_VALUE_TYPE));
      __ j(equal, &runtime_call, Label::kFar);
    }
    __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
    __ ret(0);


    __ bind(&not_identical);
  }

  // Strict equality can quickly decide whether objects are equal.
  // Non-strict object equality is slower, so it is handled later in the stub.
  if (cc == equal && strict()) {
    Label slow;  // Fallthrough label.
    Label not_smis;
    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    // If either is a Smi (we know that not both are), then they can only
    // be equal if the other is a HeapNumber. If so, use the slow case.
    STATIC_ASSERT(kSmiTag == 0);
    DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
    __ mov(ecx, Immediate(kSmiTagMask));
    __ and_(ecx, eax);
    __ test(ecx, edx);
    __ j(not_zero, &not_smis, Label::kNear);
    // One operand is a smi.

    // Check whether the non-smi is a heap number.
    STATIC_ASSERT(kSmiTagMask == 1);
    // ecx still holds eax & kSmiTag, which is either zero or one.
    __ sub(ecx, Immediate(0x01));
    __ mov(ebx, edx);
    __ xor_(ebx, eax);
    __ and_(ebx, ecx);  // ebx holds either 0 or eax ^ edx.
    __ xor_(ebx, eax);
    // if eax was smi, ebx is now edx, else eax.

    // Check if the non-smi operand is a heap number.
    __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
           Immediate(isolate()->factory()->heap_number_map()));
    // If heap number, handle it in the slow case.
    __ j(equal, &slow, Label::kNear);
    // Return non-equal (ebx is not zero)
    __ mov(eax, ebx);
    __ ret(0);

    __ bind(&not_smis);
    // If either operand is a JSObject or an oddball value, then they are not
    // equal since their pointers are different
    // There is no test for undetectability in strict equality.

    // Get the type of the first operand.
    // If the first object is a JS object, we have done pointer comparison.
    Label first_non_object;
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(below, &first_non_object, Label::kNear);

    // Return non-zero (eax is not zero)
    Label return_not_equal;
    STATIC_ASSERT(kHeapObjectTag != 0);
    __ bind(&return_not_equal);
    __ ret(0);

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    __ CmpObjectType(edx, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(above_equal, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    // Fall through to the general case.
    __ bind(&slow);
  }

  // Generate the number comparison code.
  Label non_number_comparison;
  Label unordered;
  __ bind(&generic_heap_number_comparison);

  FloatingPointHelper::LoadSSE2Operands(masm, &non_number_comparison);
  __ ucomisd(xmm0, xmm1);
  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);

  __ mov(eax, 0);  // equal
  __ mov(ecx, Immediate(Smi::FromInt(1)));
  __ cmov(above, eax, ecx);
  __ mov(ecx, Immediate(Smi::FromInt(-1)));
  __ cmov(below, eax, ecx);
  __ ret(0);

  // If one of the numbers was NaN, then the result is always false.
  // The cc is never not-equal.
  __ bind(&unordered);
  DCHECK(cc != not_equal);
  if (cc == less || cc == less_equal) {
    __ mov(eax, Immediate(Smi::FromInt(1)));
  } else {
    __ mov(eax, Immediate(Smi::FromInt(-1)));
  }
  __ ret(0);

  // The number comparison code did not provide a valid result.
  __ bind(&non_number_comparison);

  // Fast negative check for internalized-to-internalized equality.
  Label check_for_strings;
  if (cc == equal) {
    BranchIfNotInternalizedString(masm, &check_for_strings, eax, ecx);
    BranchIfNotInternalizedString(masm, &check_for_strings, edx, ecx);

    // We've already checked for object identity, so if both operands
    // are internalized they aren't equal. Register eax already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialOneByteStrings(edx, eax, ecx, ebx,
                                           &check_unequal_objects);

  // Inline comparison of one-byte strings.
  if (cc == equal) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, edx, eax, ecx, ebx);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, edx, eax, ecx, ebx,
                                                    edi);
  }
#ifdef DEBUG
  __ Abort(kUnexpectedFallThroughFromStringComparison);
#endif

  __ bind(&check_unequal_objects);
  if (cc == equal && !strict()) {
    // Non-strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    Label return_unequal, undetectable;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ lea(ecx, Operand(eax, edx, times_1, 0));
    __ test(ecx, Immediate(kSmiTagMask));
    __ j(not_zero, &runtime_call, Label::kNear);

    __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
    __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));

    __ test_b(FieldOperand(ebx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(not_zero, &undetectable, Label::kNear);
    __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(not_zero, &return_unequal, Label::kNear);

    __ CmpInstanceType(ebx, FIRST_JS_RECEIVER_TYPE);
    __ j(below, &runtime_call, Label::kNear);
    __ CmpInstanceType(ecx, FIRST_JS_RECEIVER_TYPE);
    __ j(below, &runtime_call, Label::kNear);

    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in eax.
    __ ret(0);  // eax, edx were pushed

    __ bind(&undetectable);
    __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(zero, &return_unequal, Label::kNear);
    __ Move(eax, Immediate(EQUAL));
    __ ret(0);  // eax, edx were pushed
  }
  __ bind(&runtime_call);

  if (cc == equal) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(edx);
      __ Push(eax);
      __ CallRuntime(strict() ? Runtime::kStrictEqual : Runtime::kEqual);
    }
    // Turn true into 0 and false into some non-zero value.
    STATIC_ASSERT(EQUAL == 0);
    __ sub(eax, Immediate(isolate()->factory()->true_value()));
    __ Ret();
  } else {
    // Push arguments below the return address.
    __ pop(ecx);
    __ push(edx);
    __ push(eax);
    __ push(Immediate(Smi::FromInt(NegativeComparisonResult(cc))));
    __ push(ecx);
    // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
    // tagged as a small integer.
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // eax : number of arguments to the construct function
  // ebx : feedback vector
  // edx : slot in feedback vector (Smi)
  // edi : the function to call

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Number-of-arguments register must be smi-tagged to call out.
    __ SmiTag(eax);
    __ push(eax);
    __ push(edi);
    __ push(edx);
    __ push(ebx);

    __ CallStub(stub);

    __ pop(ebx);
    __ pop(edx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // eax : number of arguments to the construct function
  // ebx : feedback vector
  // edx : slot in feedback vector (Smi)
  // edi : the function to call
  Isolate* isolate = masm->isolate();
  Label initialize, done, miss, megamorphic, not_array_function;

  // Load the cache state into ecx.
  __ mov(ecx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if ecx is a WeakCell or a Symbol, but it's harmless to read
  // at this position in a symbol (see static asserts in
  // type-feedback-vector.h).
  Label check_allocation_site;
  __ cmp(edi, FieldOperand(ecx, WeakCell::kValueOffset));
  __ j(equal, &done, Label::kFar);
  __ CompareRoot(ecx, Heap::kmegamorphic_symbolRootIndex);
  __ j(equal, &done, Label::kFar);
  __ CompareRoot(FieldOperand(ecx, HeapObject::kMapOffset),
                 Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &check_allocation_site);

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ JumpIfSmi(FieldOperand(ecx, WeakCell::kValueOffset), &initialize);
  __ jmp(&megamorphic);

  __ bind(&check_allocation_site);
  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the slot either some other function or an
  // AllocationSite.
  __ CompareRoot(FieldOperand(ecx, 0), Heap::kAllocationSiteMapRootIndex);
  __ j(not_equal, &miss);

  // Make sure the function is the Array() function
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(not_equal, &megamorphic);
  __ jmp(&done, Label::kFar);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(ecx, Heap::kuninitialized_symbolRootIndex);
  __ j(equal, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ mov(
      FieldOperand(ebx, edx, times_half_pointer_size, FixedArray::kHeaderSize),
      Immediate(TypeFeedbackVector::MegamorphicSentinel(isolate)));
  __ jmp(&done, Label::kFar);

  // An uninitialized cache is patched with the function or sentinel to
  // indicate the ElementsKind if function is the Array constructor.
  __ bind(&initialize);
  // Make sure the function is the Array() function
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(not_equal, &not_array_function);

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the
  // slot.
  CreateAllocationSiteStub create_stub(isolate);
  CallStubInRecordCallTarget(masm, &create_stub);
  __ jmp(&done);

  __ bind(&not_array_function);
  CreateWeakCellStub weak_cell_stub(isolate);
  CallStubInRecordCallTarget(masm, &weak_cell_stub);
  __ bind(&done);
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // eax : number of arguments
  // ebx : feedback vector
  // edx : slot in feedback vector (Smi, for RecordCallTarget)
  // edi : constructor function

  Label non_function;
  // Check that function is not a smi.
  __ JumpIfSmi(edi, &non_function);
  // Check that function is a JSFunction.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &non_function);

  GenerateRecordCallTarget(masm);

  Label feedback_register_initialized;
  // Put the AllocationSite from the feedback vector into ebx, or undefined.
  __ mov(ebx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));
  Handle<Map> allocation_site_map = isolate()->factory()->allocation_site_map();
  __ cmp(FieldOperand(ebx, 0), Immediate(allocation_site_map));
  __ j(equal, &feedback_register_initialized);
  __ mov(ebx, isolate()->factory()->undefined_value());
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(ebx);

  // Pass new target to construct stub.
  __ mov(edx, edi);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kConstructStubOffset));
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ jmp(ecx);

  __ bind(&non_function);
  __ mov(edx, edi);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}


void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // edi - function
  // edx - slot id
  // ebx - vector
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(not_equal, miss);

  __ mov(eax, arg_count());
  // Reload ecx.
  __ mov(ecx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));

  // Increment the call count for monomorphic function calls.
  __ add(FieldOperand(ebx, edx, times_half_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize),
         Immediate(Smi::FromInt(CallICNexus::kCallCountIncrement)));

  __ mov(ebx, ecx);
  __ mov(edx, edi);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);

  // Unreachable.
}


void CallICStub::Generate(MacroAssembler* masm) {
  // edi - function
  // edx - slot id
  // ebx - vector
  Isolate* isolate = masm->isolate();
  Label extra_checks_or_miss, call, call_function;
  int argc = arg_count();
  ParameterCount actual(argc);

  // The checks. First, does edi match the recorded monomorphic target?
  __ mov(ecx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));

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

  __ cmp(edi, FieldOperand(ecx, WeakCell::kValueOffset));
  __ j(not_equal, &extra_checks_or_miss);

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(edi, &extra_checks_or_miss);

  // Increment the call count for monomorphic function calls.
  __ add(FieldOperand(ebx, edx, times_half_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize),
         Immediate(Smi::FromInt(CallICNexus::kCallCountIncrement)));

  __ bind(&call_function);
  __ Set(eax, argc);
  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ cmp(ecx, Immediate(TypeFeedbackVector::MegamorphicSentinel(isolate)));
  __ j(equal, &call);

  // Check if we have an allocation site.
  __ CompareRoot(FieldOperand(ecx, HeapObject::kMapOffset),
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

  __ cmp(ecx, Immediate(TypeFeedbackVector::UninitializedSentinel(isolate)));
  __ j(equal, &uninitialized);

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(ecx);
  __ CmpObjectType(ecx, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &miss);
  __ mov(
      FieldOperand(ebx, edx, times_half_pointer_size, FixedArray::kHeaderSize),
      Immediate(TypeFeedbackVector::MegamorphicSentinel(isolate)));

  __ bind(&call);
  __ Set(eax, argc);
  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(edi, &miss);

  // Goto miss case if we do not have a function.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &miss);

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(equal, &miss);

  // Make sure the function belongs to the same native context.
  __ mov(ecx, FieldOperand(edi, JSFunction::kContextOffset));
  __ mov(ecx, ContextOperand(ecx, Context::NATIVE_CONTEXT_INDEX));
  __ cmp(ecx, NativeContextOperand());
  __ j(not_equal, &miss);

  // Initialize the call counter.
  __ mov(FieldOperand(ebx, edx, times_half_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize),
         Immediate(Smi::FromInt(CallICNexus::kCallCountIncrement)));

  // Store the function. Use a stub since we need a frame for allocation.
  // ebx - vector
  // edx - slot
  // edi - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(isolate);
    __ push(edi);
    __ CallStub(&create_stub);
    __ pop(edi);
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

  // Push the function and feedback info.
  __ push(edi);
  __ push(ebx);
  __ push(edx);

  // Call the entry.
  __ CallRuntime(Runtime::kCallIC_Miss);

  // Move result to edi and exit the internal frame.
  __ mov(edi, eax);
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
  // Generate if not already in cache.
  CEntryStub(isolate, 1, kSaveFPRegs).GetCode();
  isolate->set_fp_stubs_generated(true);
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // eax: number of arguments including receiver
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)
  //
  // If argv_in_register():
  // ecx: pointer to the first argument

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Reserve space on the stack for the three arguments passed to the call. If
  // result size is greater than can be returned in registers, also reserve
  // space for the hidden argument for the result location, and space for the
  // result itself.
  int arg_stack_space = result_size() < 3 ? 3 : 4 + result_size();

  // Enter the exit frame that transitions from JavaScript to C++.
  if (argv_in_register()) {
    DCHECK(!save_doubles());
    __ EnterApiExitFrame(arg_stack_space);

    // Move argc and argv into the correct registers.
    __ mov(esi, ecx);
    __ mov(edi, eax);
  } else {
    __ EnterExitFrame(arg_stack_space, save_doubles());
  }

  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

  // Result returned in eax, or eax+edx if result size is 2.

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }
  // Call C function.
  if (result_size() <= 2) {
    __ mov(Operand(esp, 0 * kPointerSize), edi);  // argc.
    __ mov(Operand(esp, 1 * kPointerSize), esi);  // argv.
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
  } else {
    DCHECK_EQ(3, result_size());
    // Pass a pointer to the result location as the first argument.
    __ lea(eax, Operand(esp, 4 * kPointerSize));
    __ mov(Operand(esp, 0 * kPointerSize), eax);
    __ mov(Operand(esp, 1 * kPointerSize), edi);  // argc.
    __ mov(Operand(esp, 2 * kPointerSize), esi);  // argv.
    __ mov(Operand(esp, 3 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
  }
  __ call(ebx);

  if (result_size() > 2) {
    DCHECK_EQ(3, result_size());
#ifndef _WIN32
    // Restore the "hidden" argument on the stack which was popped by caller.
    __ sub(esp, Immediate(kPointerSize));
#endif
    // Read result values stored on stack. Result is stored above the arguments.
    __ mov(kReturnRegister0, Operand(esp, 4 * kPointerSize));
    __ mov(kReturnRegister1, Operand(esp, 5 * kPointerSize));
    __ mov(kReturnRegister2, Operand(esp, 6 * kPointerSize));
  }
  // Result is in eax, edx:eax or edi:edx:eax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ cmp(eax, isolate()->factory()->exception());
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    __ push(edx);
    __ mov(edx, Immediate(isolate()->factory()->the_hole_value()));
    Label okay;
    ExternalReference pending_exception_address(
        Isolate::kPendingExceptionAddress, isolate());
    __ cmp(edx, Operand::StaticVariable(pending_exception_address));
    // Cannot use check here as it attempts to generate call into runtime.
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
    __ pop(edx);
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

  // Ask the runtime for help to determine the handler. This will set eax to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, eax);
    __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));  // argc.
    __ mov(Operand(esp, 1 * kPointerSize), Immediate(0));  // argv.
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ mov(esi, Operand::StaticVariable(pending_handler_context_address));
  __ mov(esp, Operand::StaticVariable(pending_handler_sp_address));
  __ mov(ebp, Operand::StaticVariable(pending_handler_fp_address));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (esi == 0) for non-JS frames.
  Label skip;
  __ test(esi, esi);
  __ j(zero, &skip, Label::kNear);
  __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
  __ bind(&skip);

  // Compute the handler entry address and jump to it.
  __ mov(edi, Operand::StaticVariable(pending_handler_code_address));
  __ mov(edx, Operand::StaticVariable(pending_handler_offset_address));
  __ lea(edi, FieldOperand(edi, edx, times_1, Code::kHeaderSize));
  __ jmp(edi);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Set up frame.
  __ push(ebp);
  __ mov(ebp, esp);

  // Push marker in two places.
  int marker = type();
  __ push(Immediate(Smi::FromInt(marker)));  // context slot
  __ push(Immediate(Smi::FromInt(marker)));  // function slot
  // Save callee-saved registers (C calling conventions).
  __ push(edi);
  __ push(esi);
  __ push(ebx);

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate());
  __ push(Operand::StaticVariable(c_entry_fp));

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ cmp(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ j(not_equal, &not_outermost_js, Label::kNear);
  __ mov(Operand::StaticVariable(js_entry_sp), ebp);
  __ push(Immediate(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ jmp(&invoke, Label::kNear);
  __ bind(&not_outermost_js);
  __ push(Immediate(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate());
  __ mov(Operand::StaticVariable(pending_exception), eax);
  __ mov(eax, Immediate(isolate()->factory()->exception()));
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Clear any pending exceptions.
  __ mov(edx, Immediate(isolate()->factory()->the_hole_value()));
  __ mov(Operand::StaticVariable(pending_exception), edx);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. Notice that we cannot store a
  // reference to the trampoline code directly in this stub, because the
  // builtin stubs may not have been generated yet.
  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate());
    __ mov(edx, Immediate(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate());
    __ mov(edx, Immediate(entry));
  }
  __ mov(edx, Operand(edx, 0));  // deref address
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ call(edx);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ pop(ebx);
  __ cmp(ebx, Immediate(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ j(not_equal, &not_outermost_js_2);
  __ mov(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  __ pop(Operand::StaticVariable(ExternalReference(
      Isolate::kCEntryFPAddress, isolate())));

  // Restore callee-saved registers (C calling conventions).
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  __ add(esp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(ebp);
  __ ret(0);
}


void InstanceOfStub::Generate(MacroAssembler* masm) {
  Register const object = edx;                       // Object (lhs).
  Register const function = eax;                     // Function (rhs).
  Register const object_map = ecx;                   // Map of {object}.
  Register const function_map = ebx;                 // Map of {function}.
  Register const function_prototype = function_map;  // Prototype of {function}.
  Register const scratch = edi;

  DCHECK(object.is(InstanceOfDescriptor::LeftRegister()));
  DCHECK(function.is(InstanceOfDescriptor::RightRegister()));

  // Check if {object} is a smi.
  Label object_is_smi;
  __ JumpIfSmi(object, &object_is_smi, Label::kNear);

  // Lookup the {function} and the {object} map in the global instanceof cache.
  // Note: This is safe because we clear the global instanceof cache whenever
  // we change the prototype of any object.
  Label fast_case, slow_case;
  __ mov(object_map, FieldOperand(object, HeapObject::kMapOffset));
  __ CompareRoot(function, scratch, Heap::kInstanceofCacheFunctionRootIndex);
  __ j(not_equal, &fast_case, Label::kNear);
  __ CompareRoot(object_map, scratch, Heap::kInstanceofCacheMapRootIndex);
  __ j(not_equal, &fast_case, Label::kNear);
  __ LoadRoot(eax, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(0);

  // If {object} is a smi we can safely return false if {function} is a JS
  // function, otherwise we have to miss to the runtime and throw an exception.
  __ bind(&object_is_smi);
  __ JumpIfSmi(function, &slow_case);
  __ CmpObjectType(function, JS_FUNCTION_TYPE, function_map);
  __ j(not_equal, &slow_case);
  __ LoadRoot(eax, Heap::kFalseValueRootIndex);
  __ ret(0);

  // Fast-case: The {function} must be a valid JSFunction.
  __ bind(&fast_case);
  __ JumpIfSmi(function, &slow_case);
  __ CmpObjectType(function, JS_FUNCTION_TYPE, function_map);
  __ j(not_equal, &slow_case);

  // Go to the runtime if the function is not a constructor.
  __ test_b(FieldOperand(function_map, Map::kBitFieldOffset),
            static_cast<uint8_t>(1 << Map::kIsConstructor));
  __ j(zero, &slow_case);

  // Ensure that {function} has an instance prototype.
  __ test_b(FieldOperand(function_map, Map::kBitFieldOffset),
            static_cast<uint8_t>(1 << Map::kHasNonInstancePrototype));
  __ j(not_zero, &slow_case);

  // Get the "prototype" (or initial map) of the {function}.
  __ mov(function_prototype,
         FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  __ AssertNotSmi(function_prototype);

  // Resolve the prototype if the {function} has an initial map.  Afterwards the
  // {function_prototype} will be either the JSReceiver prototype object or the
  // hole value, which means that no instances of the {function} were created so
  // far and hence we should return false.
  Label function_prototype_valid;
  Register const function_prototype_map = scratch;
  __ CmpObjectType(function_prototype, MAP_TYPE, function_prototype_map);
  __ j(not_equal, &function_prototype_valid, Label::kNear);
  __ mov(function_prototype,
         FieldOperand(function_prototype, Map::kPrototypeOffset));
  __ bind(&function_prototype_valid);
  __ AssertNotSmi(function_prototype);

  // Update the global instanceof cache with the current {object} map and
  // {function}.  The cached answer will be set when it is known below.
  __ StoreRoot(function, scratch, Heap::kInstanceofCacheFunctionRootIndex);
  __ StoreRoot(object_map, scratch, Heap::kInstanceofCacheMapRootIndex);

  // Loop through the prototype chain looking for the {function} prototype.
  // Assume true, and change to false if not found.
  Label done, loop, fast_runtime_fallback;
  __ mov(eax, isolate()->factory()->true_value());
  __ bind(&loop);

  // Check if the object needs to be access checked.
  __ test_b(FieldOperand(object_map, Map::kBitFieldOffset),
            1 << Map::kIsAccessCheckNeeded);
  __ j(not_zero, &fast_runtime_fallback, Label::kNear);
  // Check if the current object is a Proxy.
  __ CmpInstanceType(object_map, JS_PROXY_TYPE);
  __ j(equal, &fast_runtime_fallback, Label::kNear);

  __ mov(object, FieldOperand(object_map, Map::kPrototypeOffset));
  __ cmp(object, function_prototype);
  __ j(equal, &done, Label::kNear);
  __ mov(object_map, FieldOperand(object, HeapObject::kMapOffset));
  __ cmp(object, isolate()->factory()->null_value());
  __ j(not_equal, &loop);
  __ mov(eax, isolate()->factory()->false_value());

  __ bind(&done);
  __ StoreRoot(eax, scratch, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(0);

  // Found Proxy or access check needed: Call the runtime.
  __ bind(&fast_runtime_fallback);
  __ PopReturnAddressTo(scratch);
  __ Push(object);
  __ Push(function_prototype);
  __ PushReturnAddressFrom(scratch);
  // Invalidate the instanceof cache.
  __ Move(eax, Immediate(Smi::FromInt(0)));
  __ StoreRoot(eax, scratch, Heap::kInstanceofCacheFunctionRootIndex);
  __ TailCallRuntime(Runtime::kHasInPrototypeChain);

  // Slow-case: Call the %InstanceOf runtime function.
  __ bind(&slow_case);
  __ PopReturnAddressTo(scratch);
  __ Push(object);
  __ Push(function);
  __ PushReturnAddressFrom(scratch);
  __ TailCallRuntime(Runtime::kInstanceOf);
}


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  STATIC_ASSERT(kSmiTag == 0);
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
    __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ test(result_, Immediate(kIsNotStringMask));
    __ j(not_zero, receiver_not_string_);
  }

  // If the index is non-smi trigger the non-smi case.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ cmp(index_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  __ SmiUntag(index_);

  Factory* factory = masm->isolate()->factory();
  StringCharLoadGenerator::Generate(
      masm, factory, object_, index_, result_, &call_runtime_);

  __ SmiTag(result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm, EmbedMode embed_mode,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              masm->isolate()->factory()->heap_number_map(),
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ push(LoadWithVectorDescriptor::VectorRegister());
    __ push(LoadDescriptor::SlotRegister());
  }
  __ push(object_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero);
  } else {
    DCHECK(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi);
  }
  if (!index_.is(eax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ mov(index_, eax);
  }
  __ pop(object_);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ pop(LoadDescriptor::SlotRegister());
    __ pop(LoadWithVectorDescriptor::VectorRegister());
  }
  // Reload the instance type.
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ SmiTag(index_);
  __ push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  DCHECK(base::bits::IsPowerOfTwo32(String::kMaxOneByteCharCodeU + 1));
  __ test(code_, Immediate(kSmiTagMask |
                           ((~String::kMaxOneByteCharCodeU) << kSmiTagSize)));
  __ j(not_zero, &slow_case_);

  Factory* factory = masm->isolate()->factory();
  __ Move(result_, Immediate(factory->single_character_string_cache()));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiShiftSize == 0);
  // At this point code register contains smi tagged one byte char code.
  __ mov(result_, FieldOperand(result_,
                               code_, times_half_pointer_size,
                               FixedArray::kHeaderSize));
  __ cmp(result_, factory->undefined_value());
  __ j(equal, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kStringCharFromCode);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          Register scratch,
                                          String::Encoding encoding) {
  DCHECK(!scratch.is(dest));
  DCHECK(!scratch.is(src));
  DCHECK(!scratch.is(count));

  // Nothing to do for zero characters.
  Label done;
  __ test(count, count);
  __ j(zero, &done);

  // Make count the number of bytes to copy.
  if (encoding == String::TWO_BYTE_ENCODING) {
    __ shl(count, 1);
  }

  Label loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(src, 0));
  __ mov_b(Operand(dest, 0), scratch);
  __ inc(src);
  __ inc(dest);
  __ dec(count);
  __ j(not_zero, &loop);

  __ bind(&done);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: to
  //  esp[8]: from
  //  esp[12]: string

  // Make sure first argument is a string.
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &runtime);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ebx);
  __ j(NegateCondition(is_string), &runtime);

  // eax: string
  // ebx: instance type

  // Calculate length of sub string using the smi values.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));  // To index.
  __ JumpIfNotSmi(ecx, &runtime);
  __ mov(edx, Operand(esp, 2 * kPointerSize));  // From index.
  __ JumpIfNotSmi(edx, &runtime);
  __ sub(ecx, edx);
  __ cmp(ecx, FieldOperand(eax, String::kLengthOffset));
  Label not_original_string;
  // Shorter than original string's length: an actual substring.
  __ j(below, &not_original_string, Label::kNear);
  // Longer than original string's length or negative: unsafe arguments.
  __ j(above, &runtime);
  // Return original string.
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);
  __ bind(&not_original_string);

  Label single_char;
  __ cmp(ecx, Immediate(Smi::FromInt(1)));
  __ j(equal, &single_char);

  // eax: string
  // ebx: instance type
  // ecx: sub string length (smi)
  // edx: from index (smi)
  // Deal with different string types: update the index if necessary
  // and put the underlying string into edi.
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ test(ebx, Immediate(kIsIndirectStringMask));
  __ j(zero, &seq_or_external_string, Label::kNear);

  Factory* factory = isolate()->factory();
  __ test(ebx, Immediate(kSlicedNotConsMask));
  __ j(not_zero, &sliced_string, Label::kNear);
  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  __ cmp(FieldOperand(eax, ConsString::kSecondOffset),
         factory->empty_string());
  __ j(not_equal, &runtime);
  __ mov(edi, FieldOperand(eax, ConsString::kFirstOffset));
  // Update instance type.
  __ mov(ebx, FieldOperand(edi, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and adjust start index by offset.
  __ add(edx, FieldOperand(eax, SlicedString::kOffsetOffset));
  __ mov(edi, FieldOperand(eax, SlicedString::kParentOffset));
  // Update instance type.
  __ mov(ebx, FieldOperand(edi, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the expected register.
  __ mov(edi, eax);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // edi: underlying subject string
    // ebx: instance type of underlying subject string
    // edx: adjusted start index (smi)
    // ecx: length (smi)
    __ cmp(ecx, Immediate(Smi::FromInt(SlicedString::kMinLength)));
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
    __ test(ebx, Immediate(kStringEncodingMask));
    __ j(zero, &two_byte_slice, Label::kNear);
    __ AllocateOneByteSlicedString(eax, ebx, no_reg, &runtime);
    __ jmp(&set_slice_header, Label::kNear);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(eax, ebx, no_reg, &runtime);
    __ bind(&set_slice_header);
    __ mov(FieldOperand(eax, SlicedString::kLengthOffset), ecx);
    __ mov(FieldOperand(eax, SlicedString::kHashFieldOffset),
           Immediate(String::kEmptyHashField));
    __ mov(FieldOperand(eax, SlicedString::kParentOffset), edi);
    __ mov(FieldOperand(eax, SlicedString::kOffsetOffset), edx);
    __ IncrementCounter(counters->sub_string_native(), 1);
    __ ret(3 * kPointerSize);

    __ bind(&copy_routine);
  }

  // edi: underlying subject string
  // ebx: instance type of underlying subject string
  // edx: adjusted start index (smi)
  // ecx: length (smi)
  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label two_byte_sequential, runtime_drop_two, sequential_string;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test_b(ebx, kExternalStringTag);
  __ j(zero, &sequential_string);

  // Handle external string.
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ test_b(ebx, kShortExternalStringMask);
  __ j(not_zero, &runtime);
  __ mov(edi, FieldOperand(edi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ sub(edi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&sequential_string);
  // Stash away (adjusted) index and (underlying) string.
  __ push(edx);
  __ push(edi);
  __ SmiUntag(ecx);
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ test_b(ebx, kStringEncodingMask);
  __ j(zero, &two_byte_sequential);

  // Sequential one byte string.  Allocate the result.
  __ AllocateOneByteString(eax, ecx, ebx, edx, edi, &runtime_drop_two);

  // eax: result string
  // ecx: result string length
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(edi, Immediate(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ pop(edx);
  __ pop(ebx);
  __ SmiUntag(ebx);
  __ lea(edx, FieldOperand(edx, ebx, times_1, SeqOneByteString::kHeaderSize));

  // eax: result string
  // ecx: result length
  // edi: first character of result
  // edx: character of sub string start
  StringHelper::GenerateCopyCharacters(
      masm, edi, edx, ecx, ebx, String::ONE_BYTE_ENCODING);
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);

  __ bind(&two_byte_sequential);
  // Sequential two-byte string.  Allocate the result.
  __ AllocateTwoByteString(eax, ecx, ebx, edx, edi, &runtime_drop_two);

  // eax: result string
  // ecx: result string length
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(edi,
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ pop(edx);
  __ pop(ebx);
  // As from is a smi it is 2 times the value which matches the size of a two
  // byte character.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ lea(edx, FieldOperand(edx, ebx, times_1, SeqTwoByteString::kHeaderSize));

  // eax: result string
  // ecx: result length
  // edi: first character of result
  // edx: character of sub string start
  StringHelper::GenerateCopyCharacters(
      masm, edi, edx, ecx, ebx, String::TWO_BYTE_ENCODING);
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);

  // Drop pushed values on the stack before tail call.
  __ bind(&runtime_drop_two);
  __ Drop(2);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString);

  __ bind(&single_char);
  // eax: string
  // ebx: instance type
  // ecx: sub string length (smi)
  // edx: from index (smi)
  StringCharAtGenerator generator(eax, edx, ecx, eax, &runtime, &runtime,
                                  &runtime, STRING_INDEX_IS_NUMBER,
                                  RECEIVER_IS_STRING);
  generator.GenerateFast(masm);
  __ ret(3 * kPointerSize);
  generator.SkipSlow(masm, &runtime);
}


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in eax.
  Label not_smi;
  __ JumpIfNotSmi(eax, &not_smi, Label::kNear);
  __ Ret();
  __ bind(&not_smi);

  Label not_heap_number;
  __ CompareMap(eax, masm->isolate()->factory()->heap_number_map());
  __ j(not_equal, &not_heap_number, Label::kNear);
  __ Ret();
  __ bind(&not_heap_number);

  Label not_string, slow_string;
  __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, edi);
  // eax: object
  // edi: object map
  __ j(above_equal, &not_string, Label::kNear);
  // Check if string has a cached array index.
  __ test(FieldOperand(eax, String::kHashFieldOffset),
          Immediate(String::kContainsCachedArrayIndexMask));
  __ j(not_zero, &slow_string, Label::kNear);
  __ mov(eax, FieldOperand(eax, String::kHashFieldOffset));
  __ IndexFromHash(eax, eax);
  __ Ret();
  __ bind(&slow_string);
  __ pop(ecx);   // Pop return address.
  __ push(eax);  // Push argument.
  __ push(ecx);  // Push return address.
  __ TailCallRuntime(Runtime::kStringToNumber);
  __ bind(&not_string);

  Label not_oddball;
  __ CmpInstanceType(edi, ODDBALL_TYPE);
  __ j(not_equal, &not_oddball, Label::kNear);
  __ mov(eax, FieldOperand(eax, Oddball::kToNumberOffset));
  __ Ret();
  __ bind(&not_oddball);

  __ pop(ecx);   // Pop return address.
  __ push(eax);  // Push argument.
  __ push(ecx);  // Push return address.
  __ TailCallRuntime(Runtime::kToNumber);
}


void ToLengthStub::Generate(MacroAssembler* masm) {
  // The ToLength stub takes on argument in eax.
  Label not_smi, positive_smi;
  __ JumpIfNotSmi(eax, &not_smi, Label::kNear);
  STATIC_ASSERT(kSmiTag == 0);
  __ test(eax, eax);
  __ j(greater_equal, &positive_smi, Label::kNear);
  __ xor_(eax, eax);
  __ bind(&positive_smi);
  __ Ret();
  __ bind(&not_smi);

  __ pop(ecx);   // Pop return address.
  __ push(eax);  // Push argument.
  __ push(ecx);  // Push return address.
  __ TailCallRuntime(Runtime::kToLength);
}


void ToStringStub::Generate(MacroAssembler* masm) {
  // The ToString stub takes one argument in eax.
  Label is_number;
  __ JumpIfSmi(eax, &is_number, Label::kNear);

  Label not_string;
  __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, edi);
  // eax: receiver
  // edi: receiver map
  __ j(above_equal, &not_string, Label::kNear);
  __ Ret();
  __ bind(&not_string);

  Label not_heap_number;
  __ CompareMap(eax, masm->isolate()->factory()->heap_number_map());
  __ j(not_equal, &not_heap_number, Label::kNear);
  __ bind(&is_number);
  NumberToStringStub stub(isolate());
  __ TailCallStub(&stub);
  __ bind(&not_heap_number);

  Label not_oddball;
  __ CmpInstanceType(edi, ODDBALL_TYPE);
  __ j(not_equal, &not_oddball, Label::kNear);
  __ mov(eax, FieldOperand(eax, Oddball::kToStringOffset));
  __ Ret();
  __ bind(&not_oddball);

  __ pop(ecx);   // Pop return address.
  __ push(eax);  // Push argument.
  __ push(ecx);  // Push return address.
  __ TailCallRuntime(Runtime::kToString);
}


void ToNameStub::Generate(MacroAssembler* masm) {
  // The ToName stub takes one argument in eax.
  Label is_number;
  __ JumpIfSmi(eax, &is_number, Label::kNear);

  Label not_name;
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ CmpObjectType(eax, LAST_NAME_TYPE, edi);
  // eax: receiver
  // edi: receiver map
  __ j(above, &not_name, Label::kNear);
  __ Ret();
  __ bind(&not_name);

  Label not_heap_number;
  __ CompareMap(eax, masm->isolate()->factory()->heap_number_map());
  __ j(not_equal, &not_heap_number, Label::kNear);
  __ bind(&is_number);
  NumberToStringStub stub(isolate());
  __ TailCallStub(&stub);
  __ bind(&not_heap_number);

  Label not_oddball;
  __ CmpInstanceType(edi, ODDBALL_TYPE);
  __ j(not_equal, &not_oddball, Label::kNear);
  __ mov(eax, FieldOperand(eax, Oddball::kToStringOffset));
  __ Ret();
  __ bind(&not_oddball);

  __ pop(ecx);   // Pop return address.
  __ push(eax);  // Push argument.
  __ push(ecx);  // Push return address.
  __ TailCallRuntime(Runtime::kToName);
}


void StringHelper::GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                                   Register left,
                                                   Register right,
                                                   Register scratch1,
                                                   Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ mov(length, FieldOperand(left, String::kLengthOffset));
  __ cmp(length, FieldOperand(right, String::kLengthOffset));
  __ j(equal, &check_zero_length, Label::kNear);
  __ bind(&strings_not_equal);
  __ Move(eax, Immediate(Smi::FromInt(NOT_EQUAL)));
  __ ret(0);

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ test(length, length);
  __ j(not_zero, &compare_chars, Label::kNear);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  // Compare characters.
  __ bind(&compare_chars);
  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2,
                                  &strings_not_equal, Label::kNear);

  // Characters are equal.
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_compare_native(), 1);

  // Find minimum length.
  Label left_shorter;
  __ mov(scratch1, FieldOperand(left, String::kLengthOffset));
  __ mov(scratch3, scratch1);
  __ sub(scratch3, FieldOperand(right, String::kLengthOffset));

  Register length_delta = scratch3;

  __ j(less_equal, &left_shorter, Label::kNear);
  // Right string is shorter. Change scratch1 to be length of right string.
  __ sub(scratch1, length_delta);
  __ bind(&left_shorter);

  Register min_length = scratch1;

  // If either length is zero, just compare lengths.
  Label compare_lengths;
  __ test(min_length, min_length);
  __ j(zero, &compare_lengths, Label::kNear);

  // Compare characters.
  Label result_not_equal;
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  &result_not_equal, Label::kNear);

  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  __ test(length_delta, length_delta);
  Label length_not_equal;
  __ j(not_zero, &length_not_equal, Label::kNear);

  // Result is EQUAL.
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  Label result_greater;
  Label result_less;
  __ bind(&length_not_equal);
  __ j(greater, &result_greater, Label::kNear);
  __ jmp(&result_less, Label::kNear);
  __ bind(&result_not_equal);
  __ j(above, &result_greater, Label::kNear);
  __ bind(&result_less);

  // Result is LESS.
  __ Move(eax, Immediate(Smi::FromInt(LESS)));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(eax, Immediate(Smi::FromInt(GREATER)));
  __ ret(0);
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch, Label* chars_not_equal,
    Label::Distance chars_not_equal_near) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ lea(left,
         FieldOperand(left, length, times_1, SeqOneByteString::kHeaderSize));
  __ lea(right,
         FieldOperand(right, length, times_1, SeqOneByteString::kHeaderSize));
  __ neg(length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(left, index, times_1, 0));
  __ cmpb(scratch, Operand(right, index, times_1, 0));
  __ j(not_equal, chars_not_equal, chars_not_equal_near);
  __ inc(index);
  __ j(not_zero, &loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : left string
  //  -- eax    : right string
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertString(edx);
  __ AssertString(eax);

  Label not_same;
  __ cmp(edx, eax);
  __ j(not_equal, &not_same, Label::kNear);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1);
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential one-byte strings.
  Label runtime;
  __ JumpIfNotBothSequentialOneByteStrings(edx, eax, ecx, ebx, &runtime);

  // Compare flat one-byte strings.
  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1);
  StringHelper::GenerateCompareFlatOneByteStrings(masm, edx, eax, ecx, ebx,
                                                  edi);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ PopReturnAddressTo(ecx);
  __ Push(edx);
  __ Push(eax);
  __ PushReturnAddressFrom(ecx);
  __ TailCallRuntime(Runtime::kStringCompare);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : left
  //  -- eax    : right
  //  -- esp[0] : return address
  // -----------------------------------

  // Load ecx with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ mov(ecx, handle(isolate()->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_equal, kExpectedAllocationSite);
    __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
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

  __ JumpIfSmi(edx, &miss, miss_distance);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ JumpIfSmi(eax, &miss, miss_distance);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ JumpIfNotRoot(ecx, Heap::kBooleanMapRootIndex, &miss, miss_distance);
  __ JumpIfNotRoot(ebx, Heap::kBooleanMapRootIndex, &miss, miss_distance);
  if (!Token::IsEqualityOp(op())) {
    __ mov(eax, FieldOperand(eax, Oddball::kToNumberOffset));
    __ AssertSmi(eax);
    __ mov(edx, FieldOperand(edx, Oddball::kToNumberOffset));
    __ AssertSmi(edx);
    __ push(eax);
    __ mov(eax, edx);
    __ pop(edx);
  }
  __ sub(eax, edx);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ mov(ecx, edx);
  __ or_(ecx, eax);
  __ JumpIfNotSmi(ecx, &miss, Label::kNear);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ sub(eax, edx);
  } else {
    Label done;
    __ sub(edx, eax);
    __ j(no_overflow, &done, Label::kNear);
    // Correct sign of result in case of overflow.
    __ not_(edx);
    __ bind(&done);
    __ mov(eax, edx);
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
    __ JumpIfNotSmi(edx, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(eax, &miss);
  }

  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(eax, &right_smi, Label::kNear);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         isolate()->factory()->heap_number_map());
  __ j(not_equal, &maybe_undefined1, Label::kNear);
  __ movsd(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ jmp(&left, Label::kNear);
  __ bind(&right_smi);
  __ mov(ecx, eax);  // Can't clobber eax because we can still jump away.
  __ SmiUntag(ecx);
  __ Cvtsi2sd(xmm1, ecx);

  __ bind(&left);
  __ JumpIfSmi(edx, &left_smi, Label::kNear);
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         isolate()->factory()->heap_number_map());
  __ j(not_equal, &maybe_undefined2, Label::kNear);
  __ movsd(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
  __ jmp(&done);
  __ bind(&left_smi);
  __ mov(ecx, edx);  // Can't clobber edx because we can still jump away.
  __ SmiUntag(ecx);
  __ Cvtsi2sd(xmm0, ecx);

  __ bind(&done);
  // Compare operands.
  __ ucomisd(xmm0, xmm1);

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);

  // Return a result of -1, 0, or 1, based on EFLAGS.
  // Performing mov, because xor would destroy the flag register.
  __ mov(eax, 0);  // equal
  __ mov(ecx, Immediate(Smi::FromInt(1)));
  __ cmov(above, eax, ecx);
  __ mov(ecx, Immediate(Smi::FromInt(-1)));
  __ cmov(below, eax, ecx);
  __ ret(0);

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ cmp(eax, Immediate(isolate()->factory()->undefined_value()));
    __ j(not_equal, &miss);
    __ JumpIfSmi(edx, &unordered);
    __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ecx);
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ cmp(edx, Immediate(isolate()->factory()->undefined_value()));
    __ j(equal, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  DCHECK(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;

  // Check that both operands are heap objects.
  Label miss;
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss, Label::kNear);

  // Check that both operands are internalized strings.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ or_(tmp1, tmp2);
  __ test(tmp1, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, &miss, Label::kNear);

  // Internalized strings are compared by identity.
  Label done;
  __ cmp(left, right);
  // Make sure eax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(eax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  DCHECK(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;

  // Check that both operands are heap objects.
  Label miss;
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss, Label::kNear);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss, Label::kNear);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss, Label::kNear);

  // Unique names are compared by identity.
  Label done;
  __ cmp(left, right);
  // Make sure eax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(eax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
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
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;
  Register tmp3 = edi;

  // Check that both operands are heap objects.
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  __ mov(tmp3, tmp1);
  STATIC_ASSERT(kNotStringTag != 0);
  __ or_(tmp3, tmp2);
  __ test(tmp3, Immediate(kIsNotStringMask));
  __ j(not_zero, &miss);

  // Fast check for identical strings.
  Label not_same;
  __ cmp(left, right);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  // Handle not identical strings.
  __ bind(&not_same);

  // Check that both strings are internalized. If they are, we're done
  // because we already know they are not identical.  But in the case of
  // non-equality compare, we still need to determine the order. We
  // also know they are both strings.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kInternalizedTag == 0);
    __ or_(tmp1, tmp2);
    __ test(tmp1, Immediate(kIsNotInternalizedMask));
    __ j(not_zero, &do_compare, Label::kNear);
    // Make sure eax is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(eax));
    __ ret(0);
    __ bind(&do_compare);
  }

  // Check that both strings are sequential one-byte.
  Label runtime;
  __ JumpIfNotBothSequentialOneByteStrings(left, right, tmp1, tmp2, &runtime);

  // Compare flat one byte strings. Returns when done.
  if (equality) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, left, right, tmp1,
                                                  tmp2);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, left, right, tmp1,
                                                    tmp2, tmp3);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ pop(tmp1);  // Return address.
  __ push(left);
  __ push(right);
  __ push(tmp1);
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
  __ mov(ecx, edx);
  __ and_(ecx, eax);
  __ JumpIfSmi(ecx, &miss, Label::kNear);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
  __ j(below, &miss, Label::kNear);
  __ CmpObjectType(edx, FIRST_JS_RECEIVER_TYPE, ecx);
  __ j(below, &miss, Label::kNear);

  DCHECK_EQ(equal, GetCondition());
  __ sub(eax, edx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ mov(ecx, edx);
  __ and_(ecx, eax);
  __ JumpIfSmi(ecx, &miss, Label::kNear);

  __ GetWeakValue(edi, cell);
  __ cmp(edi, FieldOperand(eax, HeapObject::kMapOffset));
  __ j(not_equal, &miss, Label::kNear);
  __ cmp(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ j(not_equal, &miss, Label::kNear);

  if (Token::IsEqualityOp(op())) {
    __ sub(eax, edx);
    __ ret(0);
  } else {
    __ PopReturnAddressTo(ecx);
    __ Push(edx);
    __ Push(eax);
    __ Push(Immediate(Smi::FromInt(NegativeComparisonResult(GetCondition()))));
    __ PushReturnAddressFrom(ecx);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(edx);  // Preserve edx and eax.
    __ push(eax);
    __ push(edx);  // And also use them as the arguments.
    __ push(eax);
    __ push(Immediate(Smi::FromInt(op())));
    __ CallRuntime(Runtime::kCompareIC_Miss);
    // Compute the entry point of the rewritten stub.
    __ lea(edi, FieldOperand(eax, Code::kHeaderSize));
    __ pop(eax);
    __ pop(edx);
  }

  // Do a tail call to the rewritten stub.
  __ jmp(edi);
}


// Helper function used to check that the dictionary doesn't contain
// the property. This function may return false negatives, so miss_label
// must always call a backup property check that is complete.
// This function is safe to call if the receiver has fast properties.
// Name must be a unique name and receiver must be a heap object.
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
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r0;
    // Capacity is smi 2^n.
    __ mov(index, FieldOperand(properties, kCapacityOffset));
    __ dec(index);
    __ and_(index,
            Immediate(Smi::FromInt(name->Hash() +
                                   NameDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index, Operand(index, index, times_2, 0));  // index *= 3.
    Register entity_name = r0;
    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ mov(entity_name, Operand(properties, index, times_half_pointer_size,
                                kElementsStartOffset - kHeapObjectTag));
    __ cmp(entity_name, masm->isolate()->factory()->undefined_value());
    __ j(equal, done);

    // Stop if found the property.
    __ cmp(entity_name, Handle<Name>(name));
    __ j(equal, miss);

    Label good;
    // Check for the hole and skip.
    __ cmp(entity_name, masm->isolate()->factory()->the_hole_value());
    __ j(equal, &good, Label::kNear);

    // Check if the entry name is not a unique name.
    __ mov(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
    __ JumpIfNotUniqueNameInstanceType(
        FieldOperand(entity_name, Map::kInstanceTypeOffset), miss);
    __ bind(&good);
  }

  NameDictionaryLookupStub stub(masm->isolate(), properties, r0, r0,
                                NEGATIVE_LOOKUP);
  __ push(Immediate(Handle<Object>(name)));
  __ push(Immediate(name->Hash()));
  __ CallStub(&stub);
  __ test(r0, r0);
  __ j(not_zero, miss);
  __ jmp(done);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found leaving the
// index into the dictionary in |r0|. Jump to the |miss| label
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

  __ mov(r1, FieldOperand(elements, kCapacityOffset));
  __ shr(r1, kSmiTagSize);  // convert smi to int
  __ dec(r1);

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(r0, FieldOperand(name, Name::kHashFieldOffset));
    __ shr(r0, Name::kHashShift);
    if (i > 0) {
      __ add(r0, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(r0, r1);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(r0, Operand(r0, r0, times_2, 0));  // r0 = r0 * 3

    // Check if the key is identical to the name.
    __ cmp(name, Operand(elements,
                         r0,
                         times_4,
                         kElementsStartOffset - kHeapObjectTag));
    __ j(equal, done);
  }

  NameDictionaryLookupStub stub(masm->isolate(), elements, r1, r0,
                                POSITIVE_LOOKUP);
  __ push(name);
  __ mov(r0, FieldOperand(name, Name::kHashFieldOffset));
  __ shr(r0, Name::kHashShift);
  __ push(r0);
  __ CallStub(&stub);

  __ test(r1, r1);
  __ j(zero, miss);
  __ jmp(done);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Stack frame on entry:
  //  esp[0 * kPointerSize]: return address.
  //  esp[1 * kPointerSize]: key's hash.
  //  esp[2 * kPointerSize]: key.
  // Registers:
  //  dictionary_: NameDictionary to probe.
  //  result_: used as scratch.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  Register scratch = result();

  __ mov(scratch, FieldOperand(dictionary(), kCapacityOffset));
  __ dec(scratch);
  __ SmiUntag(scratch);
  __ push(scratch);

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(scratch, Operand(esp, 2 * kPointerSize));
    if (i > 0) {
      __ add(scratch, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(esp, 0));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index(), Operand(scratch, scratch, times_2, 0));  // index *= 3.

    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ mov(scratch, Operand(dictionary(), index(), times_pointer_size,
                            kElementsStartOffset - kHeapObjectTag));
    __ cmp(scratch, isolate()->factory()->undefined_value());
    __ j(equal, &not_in_dictionary);

    // Stop if found the property.
    __ cmp(scratch, Operand(esp, 3 * kPointerSize));
    __ j(equal, &in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // If we hit a key that is not a unique name during negative
      // lookup we have to bailout as this key might be equal to the
      // key we are looking for.

      // Check if the entry name is not a unique name.
      __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
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
    __ mov(result(), Immediate(0));
    __ Drop(1);
    __ ret(2 * kPointerSize);
  }

  __ bind(&in_dictionary);
  __ mov(result(), Immediate(1));
  __ Drop(1);
  __ ret(2 * kPointerSize);

  __ bind(&not_in_dictionary);
  __ mov(result(), Immediate(0));
  __ Drop(1);
  __ ret(2 * kPointerSize);
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub(isolate, kDontSaveFPRegs);
  stub.GetCode();
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

    __ mov(regs_.scratch0(), Operand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),  // Value.
                           regs_.scratch0(),
                           &dont_need_remembered_set);

    __ JumpIfInNewSpace(regs_.object(), regs_.scratch0(),
                        &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm,
        kUpdateRememberedSetOnNoNeedToInformIncrementalMarker,
        mode);
    InformIncrementalMarker(masm);
    regs_.Restore(masm);
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);

    __ bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm,
      kReturnOnNoNeedToInformIncrementalMarker,
      mode);
  InformIncrementalMarker(masm);
  regs_.Restore(masm);
  __ ret(0);
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode());
  int argument_count = 3;
  __ PrepareCallCFunction(argument_count, regs_.scratch0());
  __ mov(Operand(esp, 0 * kPointerSize), regs_.object());
  __ mov(Operand(esp, 1 * kPointerSize), regs_.address());  // Slot.
  __ mov(Operand(esp, 2 * kPointerSize),
         Immediate(ExternalReference::isolate_address(isolate())));

  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(
      ExternalReference::incremental_marking_record_write_function(isolate()),
      argument_count);

  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode());
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label object_is_black, need_incremental, need_incremental_pop_object;

  __ mov(regs_.scratch0(), Immediate(~Page::kPageAlignmentMask));
  __ and_(regs_.scratch0(), regs_.object());
  __ mov(regs_.scratch1(),
         Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset));
  __ sub(regs_.scratch1(), Immediate(1));
  __ mov(Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset),
         regs_.scratch1());
  __ j(negative, &need_incremental);

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(),
                 regs_.scratch0(),
                 regs_.scratch1(),
                 &object_is_black,
                 Label::kNear);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&object_is_black);

  // Get the value from the slot.
  __ mov(regs_.scratch0(), Operand(regs_.address(), 0));

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
                     not_zero,
                     &ensure_not_white,
                     Label::kNear);

    __ jmp(&need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need an extra register for this, so we push the object register
  // temporarily.
  __ push(regs_.object());
  __ JumpIfWhite(regs_.scratch0(),  // The value.
                 regs_.scratch1(),  // Scratch.
                 regs_.object(),    // Scratch.
                 &need_incremental_pop_object, Label::kNear);
  __ pop(regs_.object());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&need_incremental_pop_object);
  __ pop(regs_.object());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ mov(ebx, MemOperand(ebp, parameter_count_offset));
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ pop(ecx);
  int additional_offset =
      function_mode() == JS_FUNCTION_STUB_MODE ? kPointerSize : 0;
  __ lea(esp, MemOperand(esp, ebx, times_pointer_size, additional_offset));
  __ jmp(ecx);  // Return to IC Miss stub, continuation still on stack.
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


static void HandleArrayCases(MacroAssembler* masm, Register receiver,
                             Register key, Register vector, Register slot,
                             Register feedback, bool is_polymorphic,
                             Label* miss) {
  // feedback initially contains the feedback array
  Label next, next_loop, prepare_next;
  Label load_smi_map, compare_map;
  Label start_polymorphic;

  __ push(receiver);
  __ push(vector);

  Register receiver_map = receiver;
  Register cached_map = vector;

  // Receiver might not be a heap object.
  __ JumpIfSmi(receiver, &load_smi_map);
  __ mov(receiver_map, FieldOperand(receiver, 0));
  __ bind(&compare_map);
  __ mov(cached_map, FieldOperand(feedback, FixedArray::OffsetOfElementAt(0)));

  // A named keyed load might have a 2 element array, all other cases can count
  // on an array with at least 2 {map, handler} pairs, so they can go right
  // into polymorphic array handling.
  __ cmp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, is_polymorphic ? &start_polymorphic : &next);

  // found, now call handler.
  Register handler = feedback;
  __ mov(handler, FieldOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ pop(vector);
  __ pop(receiver);
  __ lea(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);

  if (!is_polymorphic) {
    __ bind(&next);
    __ cmp(FieldOperand(feedback, FixedArray::kLengthOffset),
           Immediate(Smi::FromInt(2)));
    __ j(not_equal, &start_polymorphic);
    __ pop(vector);
    __ pop(receiver);
    __ jmp(miss);
  }

  // Polymorphic, we have to loop from 2 to N
  __ bind(&start_polymorphic);
  __ push(key);
  Register counter = key;
  __ mov(counter, Immediate(Smi::FromInt(2)));
  __ bind(&next_loop);
  __ mov(cached_map, FieldOperand(feedback, counter, times_half_pointer_size,
                                  FixedArray::kHeaderSize));
  __ cmp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &prepare_next);
  __ mov(handler, FieldOperand(feedback, counter, times_half_pointer_size,
                               FixedArray::kHeaderSize + kPointerSize));
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ lea(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);

  __ bind(&prepare_next);
  __ add(counter, Immediate(Smi::FromInt(2)));
  __ cmp(counter, FieldOperand(feedback, FixedArray::kLengthOffset));
  __ j(less, &next_loop);

  // We exhausted our array of map handler pairs.
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ jmp(miss);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


static void HandleMonomorphicCase(MacroAssembler* masm, Register receiver,
                                  Register key, Register vector, Register slot,
                                  Register weak_cell, Label* miss) {
  // feedback initially contains the feedback array
  Label compare_smi_map;

  // Move the weak map into the weak_cell register.
  Register ic_map = weak_cell;
  __ mov(ic_map, FieldOperand(weak_cell, WeakCell::kValueOffset));

  // Receiver might not be a heap object.
  __ JumpIfSmi(receiver, &compare_smi_map);
  __ cmp(ic_map, FieldOperand(receiver, 0));
  __ j(not_equal, miss);
  Register handler = weak_cell;
  __ mov(handler, FieldOperand(vector, slot, times_half_pointer_size,
                               FixedArray::kHeaderSize + kPointerSize));
  __ lea(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);

  // In microbenchmarks, it made sense to unroll this code so that the call to
  // the handler is duplicated for a HeapObject receiver and a Smi receiver.
  __ bind(&compare_smi_map);
  __ CompareRoot(ic_map, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, miss);
  __ mov(handler, FieldOperand(vector, slot, times_half_pointer_size,
                               FixedArray::kHeaderSize + kPointerSize));
  __ lea(handler, FieldOperand(handler, Code::kHeaderSize));
  __ jmp(handler);
}


void LoadICStub::Generate(MacroAssembler* masm) { GenerateImpl(masm, false); }


void LoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // edx
  Register name = LoadWithVectorDescriptor::NameRegister();          // ecx
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // ebx
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // eax
  Register scratch = edi;
  __ mov(scratch, FieldOperand(vector, slot, times_half_pointer_size,
                               FixedArray::kHeaderSize));

  // Is it a weak cell?
  Label try_array;
  Label not_array, smi_key, key_okay, miss;
  __ CompareRoot(FieldOperand(scratch, 0), Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &try_array);
  HandleMonomorphicCase(masm, receiver, name, vector, slot, scratch, &miss);

  // Is it a fixed array?
  __ bind(&try_array);
  __ CompareRoot(FieldOperand(scratch, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);
  HandleArrayCases(masm, receiver, name, vector, slot, scratch, true, &miss);

  __ bind(&not_array);
  __ CompareRoot(scratch, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &miss);
  __ push(slot);
  __ push(vector);
  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::LOAD_IC, code_flags,
                                               receiver, name, vector, scratch);
  __ pop(vector);
  __ pop(slot);

  __ bind(&miss);
  LoadIC::GenerateMiss(masm);
}


void KeyedLoadICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void KeyedLoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void KeyedLoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // edx
  Register key = LoadWithVectorDescriptor::NameRegister();           // ecx
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // ebx
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // eax
  Register feedback = edi;
  __ mov(feedback, FieldOperand(vector, slot, times_half_pointer_size,
                                FixedArray::kHeaderSize));
  // Is it a weak cell?
  Label try_array;
  Label not_array, smi_key, key_okay, miss;
  __ CompareRoot(FieldOperand(feedback, 0), Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &try_array);
  HandleMonomorphicCase(masm, receiver, key, vector, slot, feedback, &miss);

  __ bind(&try_array);
  // Is it a fixed array?
  __ CompareRoot(FieldOperand(feedback, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);

  // We have a polymorphic element handler.
  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);
  HandleArrayCases(masm, receiver, key, vector, slot, feedback, true, &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ jmp(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmp(key, feedback);
  __ j(not_equal, &miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ mov(feedback, FieldOperand(vector, slot, times_half_pointer_size,
                                FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, receiver, key, vector, slot, feedback, false, &miss);

  __ bind(&miss);
  KeyedLoadIC::GenerateMiss(masm);
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


// value is on the stack already.
static void HandlePolymorphicStoreCase(MacroAssembler* masm, Register receiver,
                                       Register key, Register vector,
                                       Register slot, Register feedback,
                                       bool is_polymorphic, Label* miss) {
  // feedback initially contains the feedback array
  Label next, next_loop, prepare_next;
  Label load_smi_map, compare_map;
  Label start_polymorphic;
  Label pop_and_miss;
  ExternalReference virtual_register =
      ExternalReference::virtual_handler_register(masm->isolate());

  __ push(receiver);
  __ push(vector);

  Register receiver_map = receiver;
  Register cached_map = vector;

  // Receiver might not be a heap object.
  __ JumpIfSmi(receiver, &load_smi_map);
  __ mov(receiver_map, FieldOperand(receiver, 0));
  __ bind(&compare_map);
  __ mov(cached_map, FieldOperand(feedback, FixedArray::OffsetOfElementAt(0)));

  // A named keyed store might have a 2 element array, all other cases can count
  // on an array with at least 2 {map, handler} pairs, so they can go right
  // into polymorphic array handling.
  __ cmp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &start_polymorphic);

  // found, now call handler.
  Register handler = feedback;
  DCHECK(handler.is(VectorStoreICDescriptor::ValueRegister()));
  __ mov(handler, FieldOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ pop(vector);
  __ pop(receiver);
  __ lea(handler, FieldOperand(handler, Code::kHeaderSize));
  __ mov(Operand::StaticVariable(virtual_register), handler);
  __ pop(handler);  // Pop "value".
  __ jmp(Operand::StaticVariable(virtual_register));

  // Polymorphic, we have to loop from 2 to N
  __ bind(&start_polymorphic);
  __ push(key);
  Register counter = key;
  __ mov(counter, Immediate(Smi::FromInt(2)));

  if (!is_polymorphic) {
    // If is_polymorphic is false, we may only have a two element array.
    // Check against length now in that case.
    __ cmp(counter, FieldOperand(feedback, FixedArray::kLengthOffset));
    __ j(greater_equal, &pop_and_miss);
  }

  __ bind(&next_loop);
  __ mov(cached_map, FieldOperand(feedback, counter, times_half_pointer_size,
                                  FixedArray::kHeaderSize));
  __ cmp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &prepare_next);
  __ mov(handler, FieldOperand(feedback, counter, times_half_pointer_size,
                               FixedArray::kHeaderSize + kPointerSize));
  __ lea(handler, FieldOperand(handler, Code::kHeaderSize));
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ mov(Operand::StaticVariable(virtual_register), handler);
  __ pop(handler);  // Pop "value".
  __ jmp(Operand::StaticVariable(virtual_register));

  __ bind(&prepare_next);
  __ add(counter, Immediate(Smi::FromInt(2)));
  __ cmp(counter, FieldOperand(feedback, FixedArray::kLengthOffset));
  __ j(less, &next_loop);

  // We exhausted our array of map handler pairs.
  __ bind(&pop_and_miss);
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ jmp(miss);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


static void HandleMonomorphicStoreCase(MacroAssembler* masm, Register receiver,
                                       Register key, Register vector,
                                       Register slot, Register weak_cell,
                                       Label* miss) {
  // The store ic value is on the stack.
  DCHECK(weak_cell.is(VectorStoreICDescriptor::ValueRegister()));
  ExternalReference virtual_register =
      ExternalReference::virtual_handler_register(masm->isolate());

  // feedback initially contains the feedback array
  Label compare_smi_map;

  // Move the weak map into the weak_cell register.
  Register ic_map = weak_cell;
  __ mov(ic_map, FieldOperand(weak_cell, WeakCell::kValueOffset));

  // Receiver might not be a heap object.
  __ JumpIfSmi(receiver, &compare_smi_map);
  __ cmp(ic_map, FieldOperand(receiver, 0));
  __ j(not_equal, miss);
  __ mov(weak_cell, FieldOperand(vector, slot, times_half_pointer_size,
                                 FixedArray::kHeaderSize + kPointerSize));
  __ lea(weak_cell, FieldOperand(weak_cell, Code::kHeaderSize));
  // Put the store ic value back in it's register.
  __ mov(Operand::StaticVariable(virtual_register), weak_cell);
  __ pop(weak_cell);  // Pop "value".
  // jump to the handler.
  __ jmp(Operand::StaticVariable(virtual_register));

  // In microbenchmarks, it made sense to unroll this code so that the call to
  // the handler is duplicated for a HeapObject receiver and a Smi receiver.
  __ bind(&compare_smi_map);
  __ CompareRoot(ic_map, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, miss);
  __ mov(weak_cell, FieldOperand(vector, slot, times_half_pointer_size,
                                 FixedArray::kHeaderSize + kPointerSize));
  __ lea(weak_cell, FieldOperand(weak_cell, Code::kHeaderSize));
  __ mov(Operand::StaticVariable(virtual_register), weak_cell);
  __ pop(weak_cell);  // Pop "value".
  // jump to the handler.
  __ jmp(Operand::StaticVariable(virtual_register));
}


void VectorStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = VectorStoreICDescriptor::ReceiverRegister();  // edx
  Register key = VectorStoreICDescriptor::NameRegister();           // ecx
  Register value = VectorStoreICDescriptor::ValueRegister();        // eax
  Register vector = VectorStoreICDescriptor::VectorRegister();      // ebx
  Register slot = VectorStoreICDescriptor::SlotRegister();          // edi
  Label miss;

  __ push(value);

  Register scratch = value;
  __ mov(scratch, FieldOperand(vector, slot, times_half_pointer_size,
                               FixedArray::kHeaderSize));

  // Is it a weak cell?
  Label try_array;
  Label not_array, smi_key, key_okay;
  __ CompareRoot(FieldOperand(scratch, 0), Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &try_array);
  HandleMonomorphicStoreCase(masm, receiver, key, vector, slot, scratch, &miss);

  // Is it a fixed array?
  __ bind(&try_array);
  __ CompareRoot(FieldOperand(scratch, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);
  HandlePolymorphicStoreCase(masm, receiver, key, vector, slot, scratch, true,
                             &miss);

  __ bind(&not_array);
  __ CompareRoot(scratch, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &miss);

  __ pop(value);
  __ push(slot);
  __ push(vector);
  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::STORE_IC, code_flags,
                                               receiver, key, slot, no_reg);
  __ pop(vector);
  __ pop(slot);
  Label no_pop_miss;
  __ jmp(&no_pop_miss);

  __ bind(&miss);
  __ pop(value);
  __ bind(&no_pop_miss);
  StoreIC::GenerateMiss(masm);
}


void VectorKeyedStoreICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void VectorKeyedStoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


static void HandlePolymorphicKeyedStoreCase(MacroAssembler* masm,
                                            Register receiver, Register key,
                                            Register vector, Register slot,
                                            Register feedback, Label* miss) {
  // feedback initially contains the feedback array
  Label next, next_loop, prepare_next;
  Label load_smi_map, compare_map;
  Label transition_call;
  Label pop_and_miss;
  ExternalReference virtual_register =
      ExternalReference::virtual_handler_register(masm->isolate());
  ExternalReference virtual_slot =
      ExternalReference::virtual_slot_register(masm->isolate());

  __ push(receiver);
  __ push(vector);

  Register receiver_map = receiver;
  Register cached_map = vector;
  Register value = StoreDescriptor::ValueRegister();

  // Receiver might not be a heap object.
  __ JumpIfSmi(receiver, &load_smi_map);
  __ mov(receiver_map, FieldOperand(receiver, 0));
  __ bind(&compare_map);

  // Polymorphic, we have to loop from 0 to N - 1
  __ push(key);
  // Current stack layout:
  // - esp[0]    -- key
  // - esp[4]    -- vector
  // - esp[8]    -- receiver
  // - esp[12]   -- value
  // - esp[16]   -- return address
  //
  // Required stack layout for handler call:
  // - esp[0]    -- return address
  // - receiver, key, value, vector, slot in registers.
  // - handler in virtual register.
  Register counter = key;
  __ mov(counter, Immediate(Smi::FromInt(0)));
  __ bind(&next_loop);
  __ mov(cached_map, FieldOperand(feedback, counter, times_half_pointer_size,
                                  FixedArray::kHeaderSize));
  __ cmp(receiver_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  __ j(not_equal, &prepare_next);
  __ mov(cached_map, FieldOperand(feedback, counter, times_half_pointer_size,
                                  FixedArray::kHeaderSize + kPointerSize));
  __ CompareRoot(cached_map, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &transition_call);
  __ mov(feedback, FieldOperand(feedback, counter, times_half_pointer_size,
                                FixedArray::kHeaderSize + 2 * kPointerSize));
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ lea(feedback, FieldOperand(feedback, Code::kHeaderSize));
  __ mov(Operand::StaticVariable(virtual_register), feedback);
  __ pop(value);
  __ jmp(Operand::StaticVariable(virtual_register));

  __ bind(&transition_call);
  // Current stack layout:
  // - esp[0]    -- key
  // - esp[4]    -- vector
  // - esp[8]    -- receiver
  // - esp[12]   -- value
  // - esp[16]   -- return address
  //
  // Required stack layout for handler call:
  // - esp[0]    -- return address
  // - receiver, key, value, map, vector in registers.
  // - handler and slot in virtual registers.
  __ mov(Operand::StaticVariable(virtual_slot), slot);
  __ mov(feedback, FieldOperand(feedback, counter, times_half_pointer_size,
                                FixedArray::kHeaderSize + 2 * kPointerSize));
  __ lea(feedback, FieldOperand(feedback, Code::kHeaderSize));
  __ mov(Operand::StaticVariable(virtual_register), feedback);

  __ mov(cached_map, FieldOperand(cached_map, WeakCell::kValueOffset));
  // The weak cell may have been cleared.
  __ JumpIfSmi(cached_map, &pop_and_miss);
  DCHECK(!cached_map.is(VectorStoreTransitionDescriptor::MapRegister()));
  __ mov(VectorStoreTransitionDescriptor::MapRegister(), cached_map);

  // Pop key into place.
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ pop(value);
  __ jmp(Operand::StaticVariable(virtual_register));

  __ bind(&prepare_next);
  __ add(counter, Immediate(Smi::FromInt(3)));
  __ cmp(counter, FieldOperand(feedback, FixedArray::kLengthOffset));
  __ j(less, &next_loop);

  // We exhausted our array of map handler pairs.
  __ bind(&pop_and_miss);
  __ pop(key);
  __ pop(vector);
  __ pop(receiver);
  __ jmp(miss);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


void VectorKeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = VectorStoreICDescriptor::ReceiverRegister();  // edx
  Register key = VectorStoreICDescriptor::NameRegister();           // ecx
  Register value = VectorStoreICDescriptor::ValueRegister();        // eax
  Register vector = VectorStoreICDescriptor::VectorRegister();      // ebx
  Register slot = VectorStoreICDescriptor::SlotRegister();          // edi
  Label miss;

  __ push(value);

  Register scratch = value;
  __ mov(scratch, FieldOperand(vector, slot, times_half_pointer_size,
                               FixedArray::kHeaderSize));

  // Is it a weak cell?
  Label try_array;
  Label not_array, smi_key, key_okay;
  __ CompareRoot(FieldOperand(scratch, 0), Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &try_array);
  HandleMonomorphicStoreCase(masm, receiver, key, vector, slot, scratch, &miss);

  // Is it a fixed array?
  __ bind(&try_array);
  __ CompareRoot(FieldOperand(scratch, 0), Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &not_array);
  HandlePolymorphicKeyedStoreCase(masm, receiver, key, vector, slot, scratch,
                                  &miss);

  __ bind(&not_array);
  Label try_poly_name;
  __ CompareRoot(scratch, Heap::kmegamorphic_symbolRootIndex);
  __ j(not_equal, &try_poly_name);

  __ pop(value);

  Handle<Code> megamorphic_stub =
      KeyedStoreIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ jmp(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmp(key, scratch);
  __ j(not_equal, &miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ mov(scratch, FieldOperand(vector, slot, times_half_pointer_size,
                               FixedArray::kHeaderSize + kPointerSize));
  HandlePolymorphicStoreCase(masm, receiver, key, vector, slot, scratch, false,
                             &miss);

  __ bind(&miss);
  __ pop(value);
  KeyedStoreIC::GenerateMiss(masm);
}


void CallICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(ebx);
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
  // Save volatile registers.
  const int kNumSavedRegisters = 3;
  __ push(eax);
  __ push(ecx);
  __ push(edx);

  // Calculate and push the original stack pointer.
  __ lea(eax, Operand(esp, (kNumSavedRegisters + 1) * kPointerSize));
  __ push(eax);

  // Retrieve our return address and use it to calculate the calling
  // function's address.
  __ mov(eax, Operand(esp, (kNumSavedRegisters + 1) * kPointerSize));
  __ sub(eax, Immediate(Assembler::kCallInstructionLength));
  __ push(eax);

  // Call the entry hook.
  DCHECK(isolate()->function_entry_hook() != NULL);
  __ call(FUNCTION_ADDR(isolate()->function_entry_hook()),
          RelocInfo::RUNTIME_ENTRY);
  __ add(esp, Immediate(2 * kPointerSize));

  // Restore ecx.
  __ pop(edx);
  __ pop(ecx);
  __ pop(eax);

  __ ret(0);
}


template<class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(masm->isolate(),
           GetInitialFastElementsKind(),
           mode);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(edx, kind);
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
  // ebx - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // edx - kind (if mode != DISABLE_ALLOCATION_SITES)
  // eax - number of arguments
  // edi - constructor?
  // esp[0] - return address
  // esp[4] - last argument
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ test_b(edx, 1);
    __ j(not_zero, &normal_sequence);
  }

  // look at the first argument
  __ mov(ecx, Operand(esp, kPointerSize));
  __ test(ecx, ecx);
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
    // Fix kind and retry.
    __ inc(edx);

    if (FLAG_debug_code) {
      Handle<Map> allocation_site_map =
          masm->isolate()->factory()->allocation_site_map();
      __ cmp(FieldOperand(ebx, 0), Immediate(allocation_site_map));
      __ Assert(equal, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ add(FieldOperand(ebx, AllocationSite::kTransitionInfoOffset),
           Immediate(Smi::FromInt(kFastElementsKindPackedToHoley)));

    __ bind(&normal_sequence);
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(edx, kind);
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
    __ test(eax, eax);
    __ j(not_zero, &not_zero_case);
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ cmp(eax, 1);
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
  //  -- eax : argc (only if argument_count() is ANY or MORE_THAN_ONE)
  //  -- ebx : AllocationSite or undefined
  //  -- edi : constructor
  //  -- edx : Original constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in ebx or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(ebx);
  }

  Label subclassing;

  // Enter the context of the Array function.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  __ cmp(edx, edi);
  __ j(not_equal, &subclassing);

  Label no_info;
  // If the feedback vector is the undefined value call an array constructor
  // that doesn't use AllocationSites.
  __ cmp(ebx, isolate()->factory()->undefined_value());
  __ j(equal, &no_info);

  // Only look at the lower 16 bits of the transition info.
  __ mov(edx, FieldOperand(ebx, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(edx);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ and_(edx, Immediate(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing.
  __ bind(&subclassing);
  switch (argument_count()) {
    case ANY:
    case MORE_THAN_ONE:
      __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), edi);
      __ add(eax, Immediate(3));
      break;
    case NONE:
      __ mov(Operand(esp, 1 * kPointerSize), edi);
      __ mov(eax, Immediate(3));
      break;
    case ONE:
      __ mov(Operand(esp, 2 * kPointerSize), edi);
      __ mov(eax, Immediate(4));
      break;
  }
  __ PopReturnAddressTo(ecx);
  __ Push(edx);
  __ Push(ebx);
  __ PushReturnAddressFrom(ecx);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  Label not_zero_case, not_one_case;
  Label normal_sequence;

  __ test(eax, eax);
  __ j(not_zero, &not_zero_case);
  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0);

  __ bind(&not_zero_case);
  __ cmp(eax, 1);
  __ j(greater, &not_one_case);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ mov(ecx, Operand(esp, kPointerSize));
    __ test(ecx, ecx);
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
  //  -- eax : argc
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following masking takes care of that anyway.
  __ mov(ecx, FieldOperand(ecx, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(ecx);

  if (FLAG_debug_code) {
    Label done;
    __ cmp(ecx, Immediate(FAST_ELEMENTS));
    __ j(equal, &done);
    __ cmp(ecx, Immediate(FAST_HOLEY_ELEMENTS));
    __ Assert(equal,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmp(ecx, Immediate(FAST_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


void FastNewObjectStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : target
  //  -- edx    : new target
  //  -- esi    : context
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);
  __ AssertReceiver(edx);

  // Verify that the new target is a JSFunction.
  Label new_object;
  __ CmpObjectType(edx, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &new_object);

  // Load the initial map and verify that it's in fact a map.
  __ mov(ecx, FieldOperand(edx, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(ecx, &new_object);
  __ CmpObjectType(ecx, MAP_TYPE, ebx);
  __ j(not_equal, &new_object);

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  __ cmp(edi, FieldOperand(ecx, Map::kConstructorOrBackPointerOffset));
  __ j(not_equal, &new_object);

  // Allocate the JSObject on the heap.
  Label allocate, done_allocate;
  __ movzx_b(ebx, FieldOperand(ecx, Map::kInstanceSizeOffset));
  __ lea(ebx, Operand(ebx, times_pointer_size, 0));
  __ Allocate(ebx, eax, edi, no_reg, &allocate, NO_ALLOCATION_FLAGS);
  __ bind(&done_allocate);

  // Initialize the JSObject fields.
  __ mov(Operand(eax, JSObject::kMapOffset), ecx);
  __ mov(Operand(eax, JSObject::kPropertiesOffset),
         masm->isolate()->factory()->empty_fixed_array());
  __ mov(Operand(eax, JSObject::kElementsOffset),
         masm->isolate()->factory()->empty_fixed_array());
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ lea(ebx, Operand(eax, JSObject::kHeaderSize));

  // ----------- S t a t e -------------
  //  -- eax    : result (untagged)
  //  -- ebx    : result fields (untagged)
  //  -- edi    : result end (untagged)
  //  -- ecx    : initial map
  //  -- esi    : context
  //  -- esp[0] : return address
  // -----------------------------------

  // Perform in-object slack tracking if requested.
  Label slack_tracking;
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  __ test(FieldOperand(ecx, Map::kBitField3Offset),
          Immediate(Map::ConstructionCounter::kMask));
  __ j(not_zero, &slack_tracking, Label::kNear);
  {
    // Initialize all in-object fields with undefined.
    __ LoadRoot(edx, Heap::kUndefinedValueRootIndex);
    __ InitializeFieldsWithFiller(ebx, edi, edx);

    // Add the object tag to make the JSObject real.
    STATIC_ASSERT(kHeapObjectTag == 1);
    __ inc(eax);
    __ Ret();
  }
  __ bind(&slack_tracking);
  {
    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    __ sub(FieldOperand(ecx, Map::kBitField3Offset),
           Immediate(1 << Map::ConstructionCounter::kShift));

    // Initialize the in-object fields with undefined.
    __ movzx_b(edx, FieldOperand(ecx, Map::kUnusedPropertyFieldsOffset));
    __ neg(edx);
    __ lea(edx, Operand(edi, edx, times_pointer_size, 0));
    __ LoadRoot(edi, Heap::kUndefinedValueRootIndex);
    __ InitializeFieldsWithFiller(ebx, edx, edi);

    // Initialize the remaining (reserved) fields with one pointer filler map.
    __ movzx_b(edx, FieldOperand(ecx, Map::kUnusedPropertyFieldsOffset));
    __ lea(edx, Operand(ebx, edx, times_pointer_size, 0));
    __ LoadRoot(edi, Heap::kOnePointerFillerMapRootIndex);
    __ InitializeFieldsWithFiller(ebx, edx, edi);

    // Add the object tag to make the JSObject real.
    STATIC_ASSERT(kHeapObjectTag == 1);
    __ inc(eax);

    // Check if we can finalize the instance size.
    Label finalize;
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);
    __ test(FieldOperand(ecx, Map::kBitField3Offset),
            Immediate(Map::ConstructionCounter::kMask));
    __ j(zero, &finalize, Label::kNear);
    __ Ret();

    // Finalize the instance size.
    __ bind(&finalize);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(eax);
      __ Push(ecx);
      __ CallRuntime(Runtime::kFinalizeInstanceSize);
      __ Pop(eax);
    }
    __ Ret();
  }

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(ebx);
    __ Push(ecx);
    __ Push(ebx);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(ecx);
  }
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ dec(eax);
  __ movzx_b(ebx, FieldOperand(ecx, Map::kInstanceSizeOffset));
  __ lea(edi, Operand(eax, ebx, times_pointer_size, 0));
  __ jmp(&done_allocate);

  // Fall back to %NewObject.
  __ bind(&new_object);
  __ PopReturnAddressTo(ecx);
  __ Push(edi);
  __ Push(edx);
  __ PushReturnAddressFrom(ecx);
  __ TailCallRuntime(Runtime::kNewObject);
}


void FastNewRestParameterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : function
  //  -- esi    : context
  //  -- ebp    : frame pointer
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);

  // For Ignition we need to skip all possible handler/stub frames until
  // we reach the JavaScript frame for the function (similar to what the
  // runtime fallback implementation does). So make edx point to that
  // JavaScript frame.
  {
    Label loop, loop_entry;
    __ mov(edx, ebp);
    __ jmp(&loop_entry, Label::kNear);
    __ bind(&loop);
    __ mov(edx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
    __ bind(&loop_entry);
    __ cmp(edi, Operand(edx, StandardFrameConstants::kMarkerOffset));
    __ j(not_equal, &loop);
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ mov(ebx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &no_rest_parameters, Label::kNear);

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ sub(eax,
         FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ j(greater, &rest_parameters);

  // Return an empty rest parameter array.
  __ bind(&no_rest_parameters);
  {
    // ----------- S t a t e -------------
    //  -- esi    : context
    //  -- esp[0] : return address
    // -----------------------------------

    // Allocate an empty rest parameter array.
    Label allocate, done_allocate;
    __ Allocate(JSArray::kSize, eax, edx, ecx, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Setup the rest parameter array in rax.
    __ LoadGlobalFunction(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, ecx);
    __ mov(FieldOperand(eax, JSArray::kMapOffset), ecx);
    __ mov(ecx, isolate()->factory()->empty_fixed_array());
    __ mov(FieldOperand(eax, JSArray::kPropertiesOffset), ecx);
    __ mov(FieldOperand(eax, JSArray::kElementsOffset), ecx);
    __ mov(FieldOperand(eax, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(0)));
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
    __ lea(ebx,
           Operand(ebx, eax, times_half_pointer_size,
                   StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));

    // ----------- S t a t e -------------
    //  -- esi    : context
    //  -- eax    : number of rest parameters (tagged)
    //  -- ebx    : pointer to first rest parameters
    //  -- esp[0] : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ lea(ecx, Operand(eax, times_half_pointer_size,
                        JSArray::kSize + FixedArray::kHeaderSize));
    __ Allocate(ecx, edx, edi, no_reg, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Setup the elements array in edx.
    __ mov(FieldOperand(edx, FixedArray::kMapOffset),
           isolate()->factory()->fixed_array_map());
    __ mov(FieldOperand(edx, FixedArray::kLengthOffset), eax);
    {
      Label loop, done_loop;
      __ Move(ecx, Smi::FromInt(0));
      __ bind(&loop);
      __ cmp(ecx, eax);
      __ j(equal, &done_loop, Label::kNear);
      __ mov(edi, Operand(ebx, 0 * kPointerSize));
      __ mov(FieldOperand(edx, ecx, times_half_pointer_size,
                          FixedArray::kHeaderSize),
             edi);
      __ sub(ebx, Immediate(1 * kPointerSize));
      __ add(ecx, Immediate(Smi::FromInt(1)));
      __ jmp(&loop);
      __ bind(&done_loop);
    }

    // Setup the rest parameter array in edi.
    __ lea(edi,
           Operand(edx, eax, times_half_pointer_size, FixedArray::kHeaderSize));
    __ LoadGlobalFunction(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, ecx);
    __ mov(FieldOperand(edi, JSArray::kMapOffset), ecx);
    __ mov(FieldOperand(edi, JSArray::kPropertiesOffset),
           isolate()->factory()->empty_fixed_array());
    __ mov(FieldOperand(edi, JSArray::kElementsOffset), edx);
    __ mov(FieldOperand(edi, JSArray::kLengthOffset), eax);
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ mov(eax, edi);
    __ Ret();

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(ecx);
      __ Push(eax);
      __ Push(ebx);
      __ Push(ecx);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ mov(edx, eax);
      __ Pop(ebx);
      __ Pop(eax);
    }
    __ jmp(&done_allocate);
  }
}


void FastNewSloppyArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : function
  //  -- esi    : context
  //  -- ebp    : frame pointer
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx,
         FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ lea(edx, Operand(ebp, ecx, times_half_pointer_size,
                      StandardFrameConstants::kCallerSPOffset));

  // ecx : number of parameters (tagged)
  // edx : parameters pointer
  // edi : function
  // esp[0] : return address

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(eax, Operand(ebx, StandardFrameConstants::kContextOffset));
  __ cmp(eax, Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor_frame, Label::kNear);

  // No adaptor, parameter count = argument count.
  __ mov(ebx, ecx);
  __ push(ecx);
  __ jmp(&try_allocate, Label::kNear);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ mov(ebx, ecx);
  __ push(ecx);
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ lea(edx, Operand(edx, ecx, times_2,
                      StandardFrameConstants::kCallerSPOffset));

  // ebx = parameter count (tagged)
  // ecx = argument count (smi-tagged)
  // Compute the mapped parameter count = min(ebx, ecx) in ebx.
  __ cmp(ebx, ecx);
  __ j(less_equal, &try_allocate, Label::kNear);
  __ mov(ebx, ecx);

  // Save mapped parameter count and function.
  __ bind(&try_allocate);
  __ push(edi);
  __ push(ebx);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  Label no_parameter_map;
  __ test(ebx, ebx);
  __ j(zero, &no_parameter_map, Label::kNear);
  __ lea(ebx, Operand(ebx, times_2, kParameterMapHeaderSize));
  __ bind(&no_parameter_map);

  // 2. Backing store.
  __ lea(ebx, Operand(ebx, ecx, times_2, FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ add(ebx, Immediate(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(ebx, eax, edi, no_reg, &runtime, TAG_OBJECT);

  // eax = address of new object(s) (tagged)
  // ecx = argument count (smi-tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[4] = function
  // esp[8] = parameter count (tagged)
  // Get the arguments map from the current native context into edi.
  Label has_mapped_parameters, instantiate;
  __ mov(edi, NativeContextOperand());
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ test(ebx, ebx);
  __ j(not_zero, &has_mapped_parameters, Label::kNear);
  __ mov(
      edi,
      Operand(edi, Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX)));
  __ jmp(&instantiate, Label::kNear);

  __ bind(&has_mapped_parameters);
  __ mov(edi, Operand(edi, Context::SlotOffset(
                               Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX)));
  __ bind(&instantiate);

  // eax = address of new object (tagged)
  // ebx = mapped parameter count (tagged)
  // ecx = argument count (smi-tagged)
  // edi = address of arguments map (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[4] = function
  // esp[8] = parameter count (tagged)
  // Copy the JS object part.
  __ mov(FieldOperand(eax, JSObject::kMapOffset), edi);
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset),
         masm->isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(eax, JSObject::kElementsOffset),
         masm->isolate()->factory()->empty_fixed_array());

  // Set up the callee in-object property.
  STATIC_ASSERT(JSSloppyArgumentsObject::kCalleeIndex == 1);
  __ mov(edi, Operand(esp, 1 * kPointerSize));
  __ AssertNotSmi(edi);
  __ mov(FieldOperand(eax, JSSloppyArgumentsObject::kCalleeOffset), edi);

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(ecx);
  __ mov(FieldOperand(eax, JSSloppyArgumentsObject::kLengthOffset), ecx);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, edi will point there, otherwise to the
  // backing store.
  __ lea(edi, Operand(eax, JSSloppyArgumentsObject::kSize));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), edi);

  // eax = address of new object (tagged)
  // ebx = mapped parameter count (tagged)
  // ecx = argument count (tagged)
  // edx = address of receiver argument
  // edi = address of parameter map or backing store (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[4] = function
  // esp[8] = parameter count (tagged)
  // Free two registers.
  __ push(edx);
  __ push(eax);

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ test(ebx, ebx);
  __ j(zero, &skip_parameter_map);

  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(isolate()->factory()->sloppy_arguments_elements_map()));
  __ lea(eax, Operand(ebx, reinterpret_cast<intptr_t>(Smi::FromInt(2))));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), eax);
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize + 0 * kPointerSize), esi);
  __ lea(eax, Operand(edi, ebx, times_2, kParameterMapHeaderSize));
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize + 1 * kPointerSize), eax);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ push(ecx);
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  __ mov(ebx, Immediate(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ add(ebx, Operand(esp, 5 * kPointerSize));
  __ sub(ebx, eax);
  __ mov(ecx, isolate()->factory()->the_hole_value());
  __ mov(edx, edi);
  __ lea(edi, Operand(edi, eax, times_2, kParameterMapHeaderSize));
  // eax = loop variable (tagged)
  // ebx = mapping index (tagged)
  // ecx = the hole value
  // edx = address of parameter map (tagged)
  // edi = address of backing store (tagged)
  // esp[0] = argument count (tagged)
  // esp[4] = address of new object (tagged)
  // esp[8] = address of receiver argument
  // esp[12] = mapped parameter count (tagged)
  // esp[16] = function
  // esp[20] = parameter count (tagged)
  __ jmp(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ sub(eax, Immediate(Smi::FromInt(1)));
  __ mov(FieldOperand(edx, eax, times_2, kParameterMapHeaderSize), ebx);
  __ mov(FieldOperand(edi, eax, times_2, FixedArray::kHeaderSize), ecx);
  __ add(ebx, Immediate(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ test(eax, eax);
  __ j(not_zero, &parameters_loop, Label::kNear);
  __ pop(ecx);

  __ bind(&skip_parameter_map);

  // ecx = argument count (tagged)
  // edi = address of backing store (tagged)
  // esp[0] = address of new object (tagged)
  // esp[4] = address of receiver argument
  // esp[8] = mapped parameter count (tagged)
  // esp[12] = function
  // esp[16] = parameter count (tagged)
  // Copy arguments header and remaining slots (if there are any).
  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(isolate()->factory()->fixed_array_map()));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), ecx);

  Label arguments_loop, arguments_test;
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ sub(edx, ebx);  // Is there a smarter way to do negative scaling?
  __ sub(edx, ebx);
  __ jmp(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ sub(edx, Immediate(kPointerSize));
  __ mov(eax, Operand(edx, 0));
  __ mov(FieldOperand(edi, ebx, times_2, FixedArray::kHeaderSize), eax);
  __ add(ebx, Immediate(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ cmp(ebx, ecx);
  __ j(less, &arguments_loop, Label::kNear);

  // Restore.
  __ pop(eax);  // Address of arguments object.
  __ Drop(4);

  // Return.
  __ ret(0);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ pop(eax);   // Remove saved mapped parameter count.
  __ pop(edi);   // Pop saved function.
  __ pop(eax);   // Remove saved parameter count.
  __ pop(eax);   // Pop return address.
  __ push(edi);  // Push function.
  __ push(edx);  // Push parameters pointer.
  __ push(ecx);  // Push parameter count.
  __ push(eax);  // Push return address.
  __ TailCallRuntime(Runtime::kNewSloppyArguments);
}


void FastNewStrictArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : function
  //  -- esi    : context
  //  -- ebp    : frame pointer
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);

  // For Ignition we need to skip all possible handler/stub frames until
  // we reach the JavaScript frame for the function (similar to what the
  // runtime fallback implementation does). So make edx point to that
  // JavaScript frame.
  {
    Label loop, loop_entry;
    __ mov(edx, ebp);
    __ jmp(&loop_entry, Label::kNear);
    __ bind(&loop);
    __ mov(edx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
    __ bind(&loop_entry);
    __ cmp(edi, Operand(edx, StandardFrameConstants::kMarkerOffset));
    __ j(not_equal, &loop);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ mov(ebx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(eax,
           FieldOperand(eax, SharedFunctionInfo::kFormalParameterCountOffset));
    __ lea(ebx,
           Operand(edx, eax, times_half_pointer_size,
                   StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ lea(ebx,
           Operand(ebx, eax, times_half_pointer_size,
                   StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));
  }
  __ bind(&arguments_done);

  // ----------- S t a t e -------------
  //  -- eax    : number of arguments (tagged)
  //  -- ebx    : pointer to the first argument
  //  -- esi    : context
  //  -- esp[0] : return address
  // -----------------------------------

  // Allocate space for the strict arguments object plus the backing store.
  Label allocate, done_allocate;
  __ lea(ecx,
         Operand(eax, times_half_pointer_size,
                 JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize));
  __ Allocate(ecx, edx, edi, no_reg, &allocate, TAG_OBJECT);
  __ bind(&done_allocate);

  // Setup the elements array in edx.
  __ mov(FieldOperand(edx, FixedArray::kMapOffset),
         isolate()->factory()->fixed_array_map());
  __ mov(FieldOperand(edx, FixedArray::kLengthOffset), eax);
  {
    Label loop, done_loop;
    __ Move(ecx, Smi::FromInt(0));
    __ bind(&loop);
    __ cmp(ecx, eax);
    __ j(equal, &done_loop, Label::kNear);
    __ mov(edi, Operand(ebx, 0 * kPointerSize));
    __ mov(FieldOperand(edx, ecx, times_half_pointer_size,
                        FixedArray::kHeaderSize),
           edi);
    __ sub(ebx, Immediate(1 * kPointerSize));
    __ add(ecx, Immediate(Smi::FromInt(1)));
    __ jmp(&loop);
    __ bind(&done_loop);
  }

  // Setup the rest parameter array in edi.
  __ lea(edi,
         Operand(edx, eax, times_half_pointer_size, FixedArray::kHeaderSize));
  __ LoadGlobalFunction(Context::STRICT_ARGUMENTS_MAP_INDEX, ecx);
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kMapOffset), ecx);
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kPropertiesOffset),
         isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kElementsOffset), edx);
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kLengthOffset), eax);
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ mov(eax, edi);
  __ Ret();

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(ecx);
    __ Push(eax);
    __ Push(ebx);
    __ Push(ecx);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ mov(edx, eax);
    __ Pop(ebx);
    __ Pop(eax);
  }
  __ jmp(&done_allocate);
}


void LoadGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = esi;
  Register slot_reg = ebx;
  Register result_reg = eax;
  Label slow_case;

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ mov(result_reg, ContextOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = result_reg;
  }

  // Load the PropertyCell value at the specified slot.
  __ mov(result_reg, ContextOperand(context_reg, slot_reg));
  __ mov(result_reg, FieldOperand(result_reg, PropertyCell::kValueOffset));

  // Check that value is not the_hole.
  __ CompareRoot(result_reg, Heap::kTheHoleValueRootIndex);
  __ j(equal, &slow_case, Label::kNear);
  __ Ret();

  // Fallback to the runtime.
  __ bind(&slow_case);
  __ SmiTag(slot_reg);
  __ Pop(result_reg);  // Pop return address.
  __ Push(slot_reg);
  __ Push(result_reg);  // Push return address.
  __ TailCallRuntime(Runtime::kLoadGlobalViaContext);
}


void StoreGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = esi;
  Register slot_reg = ebx;
  Register value_reg = eax;
  Register cell_reg = edi;
  Register cell_details_reg = edx;
  Register cell_value_reg = ecx;
  Label fast_heapobject_case, fast_smi_case, slow_case;

  if (FLAG_debug_code) {
    __ CompareRoot(value_reg, Heap::kTheHoleValueRootIndex);
    __ Check(not_equal, kUnexpectedValue);
  }

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ mov(cell_reg, ContextOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = cell_reg;
  }

  // Load the PropertyCell at the specified slot.
  __ mov(cell_reg, ContextOperand(context_reg, slot_reg));

  // Load PropertyDetails for the cell (actually only the cell_type and kind).
  __ mov(cell_details_reg,
         FieldOperand(cell_reg, PropertyCell::kDetailsOffset));
  __ SmiUntag(cell_details_reg);
  __ and_(cell_details_reg,
          Immediate(PropertyDetails::PropertyCellTypeField::kMask |
                    PropertyDetails::KindField::kMask |
                    PropertyDetails::kAttributesReadOnlyMask));

  // Check if PropertyCell holds mutable data.
  Label not_mutable_data;
  __ cmp(cell_details_reg,
         Immediate(PropertyDetails::PropertyCellTypeField::encode(
                       PropertyCellType::kMutable) |
                   PropertyDetails::KindField::encode(kData)));
  __ j(not_equal, &not_mutable_data);
  __ JumpIfSmi(value_reg, &fast_smi_case);
  __ bind(&fast_heapobject_case);
  __ mov(FieldOperand(cell_reg, PropertyCell::kValueOffset), value_reg);
  __ RecordWriteField(cell_reg, PropertyCell::kValueOffset, value_reg,
                      cell_details_reg, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // RecordWriteField clobbers the value register, so we need to reload.
  __ mov(value_reg, FieldOperand(cell_reg, PropertyCell::kValueOffset));
  __ Ret();
  __ bind(&not_mutable_data);

  // Check if PropertyCell value matches the new value (relevant for Constant,
  // ConstantType and Undefined cells).
  Label not_same_value;
  __ mov(cell_value_reg, FieldOperand(cell_reg, PropertyCell::kValueOffset));
  __ cmp(cell_value_reg, value_reg);
  __ j(not_equal, &not_same_value,
       FLAG_debug_code ? Label::kFar : Label::kNear);
  // Make sure the PropertyCell is not marked READ_ONLY.
  __ test(cell_details_reg,
          Immediate(PropertyDetails::kAttributesReadOnlyMask));
  __ j(not_zero, &slow_case);
  if (FLAG_debug_code) {
    Label done;
    // This can only be true for Constant, ConstantType and Undefined cells,
    // because we never store the_hole via this stub.
    __ cmp(cell_details_reg,
           Immediate(PropertyDetails::PropertyCellTypeField::encode(
                         PropertyCellType::kConstant) |
                     PropertyDetails::KindField::encode(kData)));
    __ j(equal, &done);
    __ cmp(cell_details_reg,
           Immediate(PropertyDetails::PropertyCellTypeField::encode(
                         PropertyCellType::kConstantType) |
                     PropertyDetails::KindField::encode(kData)));
    __ j(equal, &done);
    __ cmp(cell_details_reg,
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
  __ cmp(cell_details_reg,
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
  __ mov(FieldOperand(cell_reg, PropertyCell::kValueOffset), value_reg);
  __ Ret();
  __ bind(&value_is_heap_object);
  __ JumpIfSmi(cell_value_reg, &slow_case, Label::kNear);
  Register cell_value_map_reg = cell_value_reg;
  __ mov(cell_value_map_reg,
         FieldOperand(cell_value_reg, HeapObject::kMapOffset));
  __ cmp(cell_value_map_reg, FieldOperand(value_reg, HeapObject::kMapOffset));
  __ j(equal, &fast_heapobject_case);

  // Fallback to the runtime.
  __ bind(&slow_case);
  __ SmiTag(slot_reg);
  __ Pop(cell_reg);  // Pop return address.
  __ Push(slot_reg);
  __ Push(value_reg);
  __ Push(cell_reg);  // Push return address.
  __ TailCallRuntime(is_strict(language_mode())
                         ? Runtime::kStoreGlobalViaContext_Strict
                         : Runtime::kStoreGlobalViaContext_Sloppy);
}


// Generates an Operand for saving parameters after PrepareCallApiFunction.
static Operand ApiParameterOperand(int index) {
  return Operand(esp, index * kPointerSize);
}


// Prepares stack to put arguments (aligns and so on). Reserves
// space for return value if needed (assumes the return value is a handle).
// Arguments must be stored in ApiParameterOperand(0), ApiParameterOperand(1)
// etc. Saves context (esi). If space was reserved for return value then
// stores the pointer to the reserved slot into esi.
static void PrepareCallApiFunction(MacroAssembler* masm, int argc) {
  __ EnterApiExitFrame(argc);
  if (__ emit_debug_code()) {
    __ mov(esi, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}


// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers ebx, edi and
// caller-save registers.  Restores context.  On return removes
// stack_space * kPointerSize (GCed).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     Operand thunk_last_arg, int stack_space,
                                     Operand* stack_space_operand,
                                     Operand return_value_operand,
                                     Operand* context_restore_operand) {
  Isolate* isolate = masm->isolate();

  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  ExternalReference limit_address =
      ExternalReference::handle_scope_limit_address(isolate);
  ExternalReference level_address =
      ExternalReference::handle_scope_level_address(isolate);

  DCHECK(edx.is(function_address));
  // Allocate HandleScope in callee-save registers.
  __ mov(ebx, Operand::StaticVariable(next_address));
  __ mov(edi, Operand::StaticVariable(limit_address));
  __ add(Operand::StaticVariable(level_address), Immediate(1));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, eax);
    __ mov(Operand(esp, 0),
           Immediate(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }


  Label profiler_disabled;
  Label end_profiler_check;
  __ mov(eax, Immediate(ExternalReference::is_profiling_address(isolate)));
  __ cmpb(Operand(eax, 0), 0);
  __ j(zero, &profiler_disabled);

  // Additional parameter is the address of the actual getter function.
  __ mov(thunk_last_arg, function_address);
  // Call the api function.
  __ mov(eax, Immediate(thunk_ref));
  __ call(eax);
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  // Call the api function.
  __ call(function_address);
  __ bind(&end_profiler_check);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, eax);
    __ mov(Operand(esp, 0),
           Immediate(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label prologue;
  // Load the value from ReturnValue
  __ mov(eax, return_value_operand);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  __ bind(&prologue);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ mov(Operand::StaticVariable(next_address), ebx);
  __ sub(Operand::StaticVariable(level_address), Immediate(1));
  __ Assert(above_equal, kInvalidHandleScopeLevel);
  __ cmp(edi, Operand::StaticVariable(limit_address));
  __ j(not_equal, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ mov(esi, *context_restore_operand);
  }
  if (stack_space_operand != nullptr) {
    __ mov(ebx, *stack_space_operand);
  }
  __ LeaveApiExitFrame(!restore_context);

  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate);
  __ cmp(Operand::StaticVariable(scheduled_exception_address),
         Immediate(isolate->factory()->the_hole_value()));
  __ j(not_equal, &promote_scheduled_exception);

#if DEBUG
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = eax;
  Register map = ecx;

  __ JumpIfSmi(return_value, &ok, Label::kNear);
  __ mov(map, FieldOperand(return_value, HeapObject::kMapOffset));

  __ CmpInstanceType(map, LAST_NAME_TYPE);
  __ j(below_equal, &ok, Label::kNear);

  __ CmpInstanceType(map, FIRST_JS_RECEIVER_TYPE);
  __ j(above_equal, &ok, Label::kNear);

  __ cmp(map, isolate->factory()->heap_number_map());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->undefined_value());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->true_value());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->false_value());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->null_value());
  __ j(equal, &ok, Label::kNear);

  __ Abort(kAPICallReturnedInvalidObject);

  __ bind(&ok);
#endif

  if (stack_space_operand != nullptr) {
    DCHECK_EQ(0, stack_space);
    __ pop(ecx);
    __ add(esp, ebx);
    __ jmp(ecx);
  } else {
    __ ret(stack_space * kPointerSize);
  }

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  ExternalReference delete_extensions =
      ExternalReference::delete_handle_scope_extensions(isolate);
  __ bind(&delete_allocated_handles);
  __ mov(Operand::StaticVariable(limit_address), edi);
  __ mov(edi, eax);
  __ mov(Operand(esp, 0),
         Immediate(ExternalReference::isolate_address(isolate)));
  __ mov(eax, Immediate(delete_extensions));
  __ call(eax);
  __ mov(eax, edi);
  __ jmp(&leave_exit_frame);
}

static void CallApiFunctionStubHelper(MacroAssembler* masm,
                                      const ParameterCount& argc,
                                      bool return_first_arg,
                                      bool call_data_undefined, bool is_lazy) {
  // ----------- S t a t e -------------
  //  -- edi                 : callee
  //  -- ebx                 : call_data
  //  -- ecx                 : holder
  //  -- edx                 : api_function_address
  //  -- esi                 : context
  //  -- eax                 : number of arguments if argc is a register
  //  --
  //  -- esp[0]              : return address
  //  -- esp[4]              : last argument
  //  -- ...
  //  -- esp[argc * 4]       : first argument
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  Register callee = edi;
  Register call_data = ebx;
  Register holder = ecx;
  Register api_function_address = edx;
  Register context = esi;
  Register return_address = eax;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kArgsLength == 7);

  DCHECK(argc.is_immediate() || eax.is(argc.reg()));

  if (argc.is_immediate()) {
    __ pop(return_address);
    // context save.
    __ push(context);
  } else {
    // pop return address and save context
    __ xchg(context, Operand(esp, 0));
    return_address = context;
  }

  // callee
  __ push(callee);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  if (!call_data_undefined) {
    // return value
    __ push(Immediate(masm->isolate()->factory()->undefined_value()));
    // return value default
    __ push(Immediate(masm->isolate()->factory()->undefined_value()));
  } else {
    // return value
    __ push(scratch);
    // return value default
    __ push(scratch);
  }
  // isolate
  __ push(Immediate(reinterpret_cast<int>(masm->isolate())));
  // holder
  __ push(holder);

  __ mov(scratch, esp);

  // push return address
  __ push(return_address);

  if (!is_lazy) {
    // load context from callee
    __ mov(context, FieldOperand(callee, JSFunction::kContextOffset));
  }

  // API function gets reference to the v8::Arguments. If CPU profiler
  // is enabled wrapper function will be called and we need to pass
  // address of the callback as additional parameter, always allocate
  // space for it.
  const int kApiArgc = 1 + 1;

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  PrepareCallApiFunction(masm, kApiArgc + kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_.
  __ mov(ApiParameterOperand(2), scratch);
  if (argc.is_immediate()) {
    __ add(scratch,
           Immediate((argc.immediate() + FCA::kArgsLength - 1) * kPointerSize));
    // FunctionCallbackInfo::values_.
    __ mov(ApiParameterOperand(3), scratch);
    // FunctionCallbackInfo::length_.
    __ Move(ApiParameterOperand(4), Immediate(argc.immediate()));
    // FunctionCallbackInfo::is_construct_call_.
    __ Move(ApiParameterOperand(5), Immediate(0));
  } else {
    __ lea(scratch, Operand(scratch, argc.reg(), times_pointer_size,
                            (FCA::kArgsLength - 1) * kPointerSize));
    // FunctionCallbackInfo::values_.
    __ mov(ApiParameterOperand(3), scratch);
    // FunctionCallbackInfo::length_.
    __ mov(ApiParameterOperand(4), argc.reg());
    // FunctionCallbackInfo::is_construct_call_.
    __ lea(argc.reg(), Operand(argc.reg(), times_pointer_size,
                               (FCA::kArgsLength + 1) * kPointerSize));
    __ mov(ApiParameterOperand(5), argc.reg());
  }

  // v8::InvocationCallback's argument.
  __ lea(scratch, ApiParameterOperand(2));
  __ mov(ApiParameterOperand(0), scratch);

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  Operand context_restore_operand(ebp,
                                  (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument
  int return_value_offset = 0;
  if (return_first_arg) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  Operand return_value_operand(ebp, return_value_offset * kPointerSize);
  int stack_space = 0;
  Operand is_construct_call_operand = ApiParameterOperand(5);
  Operand* stack_space_operand = &is_construct_call_operand;
  if (argc.is_immediate()) {
    stack_space = argc.immediate() + FCA::kArgsLength + 1;
    stack_space_operand = nullptr;
  }
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           ApiParameterOperand(1), stack_space,
                           stack_space_operand, return_value_operand,
                           &context_restore_operand);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
  bool call_data_undefined = this->call_data_undefined();
  CallApiFunctionStubHelper(masm, ParameterCount(eax), false,
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
  //  -- esp[0]                        : return address
  //  -- esp[4]                        : name
  //  -- esp[8 .. (8 + kArgsLength*4)] : v8::PropertyCallbackInfo::args_
  //  -- ...
  //  -- edx                           : api_function_address
  // -----------------------------------
  DCHECK(edx.is(ApiGetterDescriptor::function_address()));

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Allocate v8::PropertyCallbackInfo object, arguments for callback and
  // space for optional callback address parameter (in case CPU profiler is
  // active) in non-GCed stack space.
  const int kApiArgc = 3 + 1;

  Register api_function_address = edx;
  Register scratch = ebx;

  // Load address of v8::PropertyAccessorInfo::args_ array.
  __ lea(scratch, Operand(esp, 2 * kPointerSize));

  PrepareCallApiFunction(masm, kApiArgc);
  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  Operand info_object = ApiParameterOperand(3);
  __ mov(info_object, scratch);

  __ sub(scratch, Immediate(kPointerSize));
  __ mov(ApiParameterOperand(0), scratch);  // name.
  __ lea(scratch, info_object);
  __ mov(ApiParameterOperand(1), scratch);  // arguments pointer.
  // Reserve space for optional callback address parameter.
  Operand thunk_last_arg = ApiParameterOperand(2);

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  // +3 is to skip prolog, return address and name handle.
  Operand return_value_operand(
      ebp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           thunk_last_arg, kStackUnwindSpace, nullptr,
                           return_value_operand, NULL);
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
