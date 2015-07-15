// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_PPC

#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/jsregexp.h"
#include "src/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


static void InitializeArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  Address deopt_handler =
      Runtime::FunctionForId(Runtime::kArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(r3, deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE, PASS_ARGUMENTS);
  }
}


static void InitializeInternalArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  Address deopt_handler =
      Runtime::FunctionForId(Runtime::kInternalArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(r3, deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE, PASS_ARGUMENTS);
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


static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cond, Strength strength);
static void EmitSmiNonsmiComparison(MacroAssembler* masm, Register lhs,
                                    Register rhs, Label* lhs_not_nan,
                                    Label* slow, bool strict);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm, Register lhs,
                                           Register rhs);


void HydrogenCodeStub::GenerateLightweightMiss(MacroAssembler* masm,
                                               ExternalReference miss) {
  // Update the static counter each time a new code stub is generated.
  isolate()->counters()->code_stubs()->Increment();

  CallInterfaceDescriptor descriptor = GetCallInterfaceDescriptor();
  int param_count = descriptor.GetRegisterParameterCount();
  {
    // Call the runtime system in a fresh internal frame.
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    DCHECK(param_count == 0 ||
           r3.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor.GetRegisterParameter(i));
    }
    __ CallExternalReference(miss, param_count);
  }

  __ Ret();
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done, fastpath_done;
  Register input_reg = source();
  Register result_reg = destination();
  DCHECK(is_truncating());

  int double_offset = offset();

  // Immediate values for this stub fit in instructions, so it's safe to use ip.
  Register scratch = GetRegisterThatIsNotOneOf(input_reg, result_reg);
  Register scratch_low =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch, scratch_low);
  DoubleRegister double_scratch = kScratchDoubleReg;

  __ push(scratch);
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += kPointerSize;

  if (!skip_fastpath()) {
    // Load double input.
    __ lfd(double_scratch, MemOperand(input_reg, double_offset));

    // Do fast-path convert from double to int.
    __ ConvertDoubleToInt64(double_scratch,
#if !V8_TARGET_ARCH_PPC64
                            scratch,
#endif
                            result_reg, d0);

// Test for overflow
#if V8_TARGET_ARCH_PPC64
    __ TestIfInt32(result_reg, r0);
#else
    __ TestIfInt32(scratch, result_reg, r0);
#endif
    __ beq(&fastpath_done);
  }

  __ Push(scratch_high, scratch_low);
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += 2 * kPointerSize;

  __ lwz(scratch_high,
         MemOperand(input_reg, double_offset + Register::kExponentOffset));
  __ lwz(scratch_low,
         MemOperand(input_reg, double_offset + Register::kMantissaOffset));

  __ ExtractBitMask(scratch, scratch_high, HeapNumber::kExponentMask);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is a *PPC* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ subi(scratch, scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ cmpi(scratch, Operand(83));
  __ bge(&out_of_range);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ subfic(scratch, scratch, Operand(51));
  __ cmpi(scratch, Operand::Zero());
  __ ble(&only_low);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ srw(scratch_low, scratch_low, scratch);
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ subfic(scratch, scratch, Operand(32));
  __ ExtractBitMask(result_reg, scratch_high, HeapNumber::kMantissaMask);
  // Set the implicit 1 before the mantissa part in scratch_high.
  STATIC_ASSERT(HeapNumber::kMantissaBitsInTopWord >= 16);
  __ oris(result_reg, result_reg,
          Operand(1 << ((HeapNumber::kMantissaBitsInTopWord) - 16)));
  __ slw(r0, result_reg, scratch);
  __ orx(result_reg, scratch_low, r0);
  __ b(&negate);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ neg(scratch, scratch);
  __ slw(result_reg, scratch_low, scratch);

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xffffffff and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xffffffff) + 1 = 0 - result.
  __ srawi(r0, scratch_high, 31);
#if V8_TARGET_ARCH_PPC64
  __ srdi(r0, r0, Operand(32));
#endif
  __ xor_(result_reg, result_reg, r0);
  __ srwi(r0, scratch_high, Operand(31));
  __ add(result_reg, result_reg, r0);

  __ bind(&done);
  __ Pop(scratch_high, scratch_low);

  __ bind(&fastpath_done);
  __ pop(scratch);

  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cond, Strength strength) {
  Label not_identical;
  Label heap_number, return_equal;
  __ cmp(r3, r4);
  __ bne(&not_identical);

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis.  If it's not a heap number, then return equal.
  if (cond == lt || cond == gt) {
    // Call runtime on identical JSObjects.
    __ CompareObjectType(r3, r7, r7, FIRST_SPEC_OBJECT_TYPE);
    __ bge(slow);
    // Call runtime on identical symbols since we need to throw a TypeError.
    __ cmpi(r7, Operand(SYMBOL_TYPE));
    __ beq(slow);
    if (is_strong(strength)) {
      // Call the runtime on anything that is converted in the semantics, since
      // we need to throw a TypeError. Smis have already been ruled out.
      __ cmpi(r7, Operand(HEAP_NUMBER_TYPE));
      __ beq(&return_equal);
      __ andi(r0, r7, Operand(kIsNotStringMask));
      __ bne(slow, cr0);
    }
  } else {
    __ CompareObjectType(r3, r7, r7, HEAP_NUMBER_TYPE);
    __ beq(&heap_number);
    // Comparing JS objects with <=, >= is complicated.
    if (cond != eq) {
      __ cmpi(r7, Operand(FIRST_SPEC_OBJECT_TYPE));
      __ bge(slow);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ cmpi(r7, Operand(SYMBOL_TYPE));
      __ beq(slow);
      if (is_strong(strength)) {
        // Call the runtime on anything that is converted in the semantics,
        // since we need to throw a TypeError. Smis and heap numbers have
        // already been ruled out.
        __ andi(r0, r7, Operand(kIsNotStringMask));
        __ bne(slow, cr0);
      }
      // Normally here we fall through to return_equal, but undefined is
      // special: (undefined == undefined) == true, but
      // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
      if (cond == le || cond == ge) {
        __ cmpi(r7, Operand(ODDBALL_TYPE));
        __ bne(&return_equal);
        __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
        __ cmp(r3, r5);
        __ bne(&return_equal);
        if (cond == le) {
          // undefined <= undefined should fail.
          __ li(r3, Operand(GREATER));
        } else {
          // undefined >= undefined should fail.
          __ li(r3, Operand(LESS));
        }
        __ Ret();
      }
    }
  }

  __ bind(&return_equal);
  if (cond == lt) {
    __ li(r3, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cond == gt) {
    __ li(r3, Operand(LESS));  // Things aren't greater than themselves.
  } else {
    __ li(r3, Operand(EQUAL));  // Things are <=, >=, ==, === themselves.
  }
  __ Ret();

  // For less and greater we don't have to check for NaN since the result of
  // x < x is false regardless.  For the others here is some code to check
  // for NaN.
  if (cond != lt && cond != gt) {
    __ bind(&heap_number);
    // It is a heap number, so return non-equal if it's NaN and equal if it's
    // not NaN.

    // The representation of NaN values has all exponent bits (52..62) set,
    // and not all mantissa bits (0..51) clear.
    // Read top bits of double representation (second word of value).
    __ lwz(r5, FieldMemOperand(r3, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    STATIC_ASSERT(HeapNumber::kExponentMask == 0x7ff00000u);
    __ ExtractBitMask(r6, r5, HeapNumber::kExponentMask);
    __ cmpli(r6, Operand(0x7ff));
    __ bne(&return_equal);

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ slwi(r5, r5, Operand(HeapNumber::kNonMantissaBitsInTopWord));
    // Or with all low-bits of mantissa.
    __ lwz(r6, FieldMemOperand(r3, HeapNumber::kMantissaOffset));
    __ orx(r3, r6, r5);
    __ cmpi(r3, Operand::Zero());
    // For equal we already have the right value in r3:  Return zero (equal)
    // if all bits in mantissa are zero (it's an Infinity) and non-zero if
    // not (it's a NaN).  For <= and >= we need to load r0 with the failing
    // value if it's a NaN.
    if (cond != eq) {
      if (CpuFeatures::IsSupported(ISELECT)) {
        __ li(r4, Operand((cond == le) ? GREATER : LESS));
        __ isel(eq, r3, r3, r4);
      } else {
        Label not_equal;
        __ bne(&not_equal);
        // All-zero means Infinity means equal.
        __ Ret();
        __ bind(&not_equal);
        if (cond == le) {
          __ li(r3, Operand(GREATER));  // NaN <= NaN should fail.
        } else {
          __ li(r3, Operand(LESS));  // NaN >= NaN should fail.
        }
      }
    }
    __ Ret();
  }
  // No fall through here.

  __ bind(&not_identical);
}


// See comment at call site.
static void EmitSmiNonsmiComparison(MacroAssembler* masm, Register lhs,
                                    Register rhs, Label* lhs_not_nan,
                                    Label* slow, bool strict) {
  DCHECK((lhs.is(r3) && rhs.is(r4)) || (lhs.is(r4) && rhs.is(r3)));

  Label rhs_is_smi;
  __ JumpIfSmi(rhs, &rhs_is_smi);

  // Lhs is a Smi.  Check whether the rhs is a heap number.
  __ CompareObjectType(rhs, r6, r7, HEAP_NUMBER_TYPE);
  if (strict) {
    // If rhs is not a number and lhs is a Smi then strict equality cannot
    // succeed.  Return non-equal
    // If rhs is r3 then there is already a non zero value in it.
    Label skip;
    __ beq(&skip);
    if (!rhs.is(r3)) {
      __ mov(r3, Operand(NOT_EQUAL));
    }
    __ Ret();
    __ bind(&skip);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ bne(slow);
  }

  // Lhs is a smi, rhs is a number.
  // Convert lhs to a double in d7.
  __ SmiToDouble(d7, lhs);
  // Load the double from rhs, tagged HeapNumber r3, to d6.
  __ lfd(d6, FieldMemOperand(rhs, HeapNumber::kValueOffset));

  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a smi.
  __ b(lhs_not_nan);

  __ bind(&rhs_is_smi);
  // Rhs is a smi.  Check whether the non-smi lhs is a heap number.
  __ CompareObjectType(lhs, r7, r7, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs is not a number and rhs is a smi then strict equality cannot
    // succeed.  Return non-equal.
    // If lhs is r3 then there is already a non zero value in it.
    Label skip;
    __ beq(&skip);
    if (!lhs.is(r3)) {
      __ mov(r3, Operand(NOT_EQUAL));
    }
    __ Ret();
    __ bind(&skip);
  } else {
    // Smi compared non-strictly with a non-smi non-heap-number.  Call
    // the runtime.
    __ bne(slow);
  }

  // Rhs is a smi, lhs is a heap number.
  // Load the double from lhs, tagged HeapNumber r4, to d7.
  __ lfd(d7, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  // Convert rhs to a double in d6.
  __ SmiToDouble(d6, rhs);
  // Fall through to both_loaded_as_doubles.
}


// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm, Register lhs,
                                           Register rhs) {
  DCHECK((lhs.is(r3) && rhs.is(r4)) || (lhs.is(r4) && rhs.is(r3)));

  // If either operand is a JS object or an oddball value, then they are
  // not equal since their pointers are different.
  // There is no test for undetectability in strict equality.
  STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);
  Label first_non_object;
  // Get the type of the first operand into r5 and compare it with
  // FIRST_SPEC_OBJECT_TYPE.
  __ CompareObjectType(rhs, r5, r5, FIRST_SPEC_OBJECT_TYPE);
  __ blt(&first_non_object);

  // Return non-zero (r3 is not zero)
  Label return_not_equal;
  __ bind(&return_not_equal);
  __ Ret();

  __ bind(&first_non_object);
  // Check for oddballs: true, false, null, undefined.
  __ cmpi(r5, Operand(ODDBALL_TYPE));
  __ beq(&return_not_equal);

  __ CompareObjectType(lhs, r6, r6, FIRST_SPEC_OBJECT_TYPE);
  __ bge(&return_not_equal);

  // Check for oddballs: true, false, null, undefined.
  __ cmpi(r6, Operand(ODDBALL_TYPE));
  __ beq(&return_not_equal);

  // Now that we have the types we might as well check for
  // internalized-internalized.
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ orx(r5, r5, r6);
  __ andi(r0, r5, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ beq(&return_not_equal, cr0);
}


// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm, Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers, Label* slow) {
  DCHECK((lhs.is(r3) && rhs.is(r4)) || (lhs.is(r4) && rhs.is(r3)));

  __ CompareObjectType(rhs, r6, r5, HEAP_NUMBER_TYPE);
  __ bne(not_heap_numbers);
  __ LoadP(r5, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ cmp(r5, r6);
  __ bne(slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  __ lfd(d6, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  __ lfd(d7, FieldMemOperand(lhs, HeapNumber::kValueOffset));

  __ b(both_loaded_as_doubles);
}


// Fast negative check for internalized-to-internalized equality.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register lhs, Register rhs,
                                                     Label* possible_strings,
                                                     Label* not_both_strings) {
  DCHECK((lhs.is(r3) && rhs.is(r4)) || (lhs.is(r4) && rhs.is(r3)));

  // r5 is object type of rhs.
  Label object_test;
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ andi(r0, r5, Operand(kIsNotStringMask));
  __ bne(&object_test, cr0);
  __ andi(r0, r5, Operand(kIsNotInternalizedMask));
  __ bne(possible_strings, cr0);
  __ CompareObjectType(lhs, r6, r6, FIRST_NONSTRING_TYPE);
  __ bge(not_both_strings);
  __ andi(r0, r6, Operand(kIsNotInternalizedMask));
  __ bne(possible_strings, cr0);

  // Both are internalized.  We already checked they weren't the same pointer
  // so they are not equal.
  __ li(r3, Operand(NOT_EQUAL));
  __ Ret();

  __ bind(&object_test);
  __ cmpi(r5, Operand(FIRST_SPEC_OBJECT_TYPE));
  __ blt(not_both_strings);
  __ CompareObjectType(lhs, r5, r6, FIRST_SPEC_OBJECT_TYPE);
  __ blt(not_both_strings);
  // If both objects are undetectable, they are equal. Otherwise, they
  // are not equal, since they are different objects and an object is not
  // equal to undefined.
  __ LoadP(r6, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ lbz(r5, FieldMemOperand(r5, Map::kBitFieldOffset));
  __ lbz(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ and_(r3, r5, r6);
  __ andi(r3, r3, Operand(1 << Map::kIsUndetectable));
  __ xori(r3, r3, Operand(1 << Map::kIsUndetectable));
  __ Ret();
}


static void CompareICStub_CheckInputType(MacroAssembler* masm, Register input,
                                         Register scratch,
                                         CompareICState::State expected,
                                         Label* fail) {
  Label ok;
  if (expected == CompareICState::SMI) {
    __ JumpIfNotSmi(input, fail);
  } else if (expected == CompareICState::NUMBER) {
    __ JumpIfSmi(input, &ok);
    __ CheckMap(input, scratch, Heap::kHeapNumberMapRootIndex, fail,
                DONT_DO_SMI_CHECK);
  }
  // We could be strict about internalized/non-internalized here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ bind(&ok);
}


// On entry r4 and r5 are the values to be compared.
// On exit r3 is 0, positive or negative to indicate the result of
// the comparison.
void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Register lhs = r4;
  Register rhs = r3;
  Condition cc = GetCondition();

  Label miss;
  CompareICStub_CheckInputType(masm, lhs, r5, left(), &miss);
  CompareICStub_CheckInputType(masm, rhs, r6, right(), &miss);

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, lhs_not_nan;

  Label not_two_smis, smi_done;
  __ orx(r5, r4, r3);
  __ JumpIfNotSmi(r5, &not_two_smis);
  __ SmiUntag(r4);
  __ SmiUntag(r3);
  __ sub(r3, r4, r3);
  __ Ret();
  __ bind(&not_two_smis);

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical.  Either returns the answer
  // or goes to slow.  Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, &slow, cc, strength());

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
  __ and_(r5, lhs, rhs);
  __ JumpIfNotSmi(r5, &not_smis);
  // One operand is a smi.  EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to lhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison.  The double values of the numbers have been loaded
  // into d7 and d6.
  EmitSmiNonsmiComparison(masm, lhs, rhs, &lhs_not_nan, &slow, strict());

  __ bind(&both_loaded_as_doubles);
  // The arguments have been converted to doubles and stored in d6 and d7
  __ bind(&lhs_not_nan);
  Label no_nan;
  __ fcmpu(d7, d6);

  Label nan, equal, less_than;
  __ bunordered(&nan);
  if (CpuFeatures::IsSupported(ISELECT)) {
    DCHECK(EQUAL == 0);
    __ li(r4, Operand(GREATER));
    __ li(r5, Operand(LESS));
    __ isel(eq, r3, r0, r4);
    __ isel(lt, r3, r5, r3);
    __ Ret();
  } else {
    __ beq(&equal);
    __ blt(&less_than);
    __ li(r3, Operand(GREATER));
    __ Ret();
    __ bind(&equal);
    __ li(r3, Operand(EQUAL));
    __ Ret();
    __ bind(&less_than);
    __ li(r3, Operand(LESS));
    __ Ret();
  }

  __ bind(&nan);
  // If one of the sides was a NaN then the v flag is set.  Load r3 with
  // whatever it takes to make the comparison fail, since comparisons with NaN
  // always fail.
  if (cc == lt || cc == le) {
    __ li(r3, Operand(GREATER));
  } else {
    __ li(r3, Operand(LESS));
  }
  __ Ret();

  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi.  The objects are in rhs_ and lhs_.
  if (strict()) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs, rhs);
  }

  Label check_for_internalized_strings;
  Label flat_string_check;
  // Check for heap-number-heap-number comparison.  Can jump to slow case,
  // or load both doubles into r3, r4, r5, r6 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to
  // check_for_internalized_strings.
  // In this case r5 will contain the type of rhs_.  Never falls through.
  EmitCheckForTwoHeapNumbers(masm, lhs, rhs, &both_loaded_as_doubles,
                             &check_for_internalized_strings,
                             &flat_string_check);

  __ bind(&check_for_internalized_strings);
  // In the strict case the EmitStrictTwoHeapObjectCompare already took care of
  // internalized strings.
  if (cc == eq && !strict()) {
    // Returns an answer for two internalized strings or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that r5 is the type of rhs_ on entry.
    EmitCheckForInternalizedStringsOrObjects(masm, lhs, rhs, &flat_string_check,
                                             &slow);
  }

  // Check for both being sequential one-byte strings,
  // and inline if that is the case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialOneByteStrings(lhs, rhs, r5, r6, &slow);

  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, r5,
                      r6);
  if (cc == eq) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, r5, r6);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, r5, r6, r7);
  }
  // Never falls through to here.

  __ bind(&slow);

  __ Push(lhs, rhs);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
  if (cc == eq) {
    native = strict() ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    native =
        is_strong(strength()) ? Builtins::COMPARE_STRONG : Builtins::COMPARE;
    int ncr;  // NaN compare result
    if (cc == lt || cc == le) {
      ncr = GREATER;
    } else {
      DCHECK(cc == gt || cc == ge);  // remaining cases
      ncr = LESS;
    }
    __ LoadSmiLiteral(r3, Smi::FromInt(ncr));
    __ push(r3);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(native, JUMP_FUNCTION);

  __ bind(&miss);
  GenerateMiss(masm);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ mflr(r0);
  __ MultiPush(kJSCallerSaved | r0.bit());
  if (save_doubles()) {
    __ SaveFPRegs(sp, 0, DoubleRegister::kNumVolatileRegisters);
  }
  const int argument_count = 1;
  const int fp_argument_count = 0;
  const Register scratch = r4;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, fp_argument_count, scratch);
  __ mov(r3, Operand(ExternalReference::isolate_address(isolate())));
  __ CallCFunction(ExternalReference::store_buffer_overflow_function(isolate()),
                   argument_count);
  if (save_doubles()) {
    __ RestoreFPRegs(sp, 0, DoubleRegister::kNumVolatileRegisters);
  }
  __ MultiPop(kJSCallerSaved | r0.bit());
  __ mtlr(r0);
  __ Ret();
}


void StoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ PushSafepointRegisters();
  __ blr();
}


void RestoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ PopSafepointRegisters();
  __ blr();
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register base = r4;
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(r5));
  const Register heapnumbermap = r8;
  const Register heapnumber = r3;
  const DoubleRegister double_base = d1;
  const DoubleRegister double_exponent = d2;
  const DoubleRegister double_result = d3;
  const DoubleRegister double_scratch = d0;
  const Register scratch = r11;
  const Register scratch2 = r10;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack to double registers.
    __ LoadP(base, MemOperand(sp, 1 * kPointerSize));
    __ LoadP(exponent, MemOperand(sp, 0 * kPointerSize));

    __ LoadRoot(heapnumbermap, Heap::kHeapNumberMapRootIndex);

    __ UntagAndJumpIfSmi(scratch, base, &base_is_smi);
    __ LoadP(scratch, FieldMemOperand(base, JSObject::kMapOffset));
    __ cmp(scratch, heapnumbermap);
    __ bne(&call_runtime);

    __ lfd(double_base, FieldMemOperand(base, HeapNumber::kValueOffset));
    __ b(&unpack_exponent);

    __ bind(&base_is_smi);
    __ ConvertIntToDouble(scratch, double_base);
    __ bind(&unpack_exponent);

    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);
    __ LoadP(scratch, FieldMemOperand(exponent, JSObject::kMapOffset));
    __ cmp(scratch, heapnumbermap);
    __ bne(&call_runtime);

    __ lfd(double_exponent,
           FieldMemOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type() == TAGGED) {
    // Base is already in double_base.
    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ lfd(double_exponent,
           FieldMemOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type() != INTEGER) {
    // Detect integer exponents stored as double.
    __ TryDoubleToInt32Exact(scratch, double_exponent, scratch2,
                             double_scratch);
    __ beq(&int_exponent);

    if (exponent_type() == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label not_plus_half, not_minus_inf1, not_minus_inf2;

      // Test for 0.5.
      __ LoadDoubleLiteral(double_scratch, 0.5, scratch);
      __ fcmpu(double_exponent, double_scratch);
      __ bne(&not_plus_half);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      __ LoadDoubleLiteral(double_scratch, -V8_INFINITY, scratch);
      __ fcmpu(double_base, double_scratch);
      __ bne(&not_minus_inf1);
      __ fneg(double_result, double_scratch);
      __ b(&done);
      __ bind(&not_minus_inf1);

      // Add +0 to convert -0 to +0.
      __ fadd(double_scratch, double_base, kDoubleRegZero);
      __ fsqrt(double_result, double_scratch);
      __ b(&done);

      __ bind(&not_plus_half);
      __ LoadDoubleLiteral(double_scratch, -0.5, scratch);
      __ fcmpu(double_exponent, double_scratch);
      __ bne(&call_runtime);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      __ LoadDoubleLiteral(double_scratch, -V8_INFINITY, scratch);
      __ fcmpu(double_base, double_scratch);
      __ bne(&not_minus_inf2);
      __ fmr(double_result, kDoubleRegZero);
      __ b(&done);
      __ bind(&not_minus_inf2);

      // Add +0 to convert -0 to +0.
      __ fadd(double_scratch, double_base, kDoubleRegZero);
      __ LoadDoubleLiteral(double_result, 1.0, scratch);
      __ fsqrt(double_scratch, double_scratch);
      __ fdiv(double_result, double_result, double_scratch);
      __ b(&done);
    }

    __ mflr(r0);
    __ push(r0);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 0, 2);
    }
    __ pop(r0);
    __ mtlr(r0);
    __ MovFromFloatResult(double_result);
    __ b(&done);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);

  // Get two copies of exponent in the registers scratch and exponent.
  if (exponent_type() == INTEGER) {
    __ mr(scratch, exponent);
  } else {
    // Exponent has previously been stored into scratch as untagged integer.
    __ mr(exponent, scratch);
  }
  __ fmr(double_scratch, double_base);  // Back up base.
  __ li(scratch2, Operand(1));
  __ ConvertIntToDouble(scratch2, double_result);

  // Get absolute value of exponent.
  __ cmpi(scratch, Operand::Zero());
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ neg(scratch2, scratch);
    __ isel(lt, scratch, scratch2, scratch);
  } else {
    Label positive_exponent;
    __ bge(&positive_exponent);
    __ neg(scratch, scratch);
    __ bind(&positive_exponent);
  }

  Label while_true, no_carry, loop_end;
  __ bind(&while_true);
  __ andi(scratch2, scratch, Operand(1));
  __ beq(&no_carry, cr0);
  __ fmul(double_result, double_result, double_scratch);
  __ bind(&no_carry);
  __ ShiftRightArithImm(scratch, scratch, 1, SetRC);
  __ beq(&loop_end, cr0);
  __ fmul(double_scratch, double_scratch, double_scratch);
  __ b(&while_true);
  __ bind(&loop_end);

  __ cmpi(exponent, Operand::Zero());
  __ bge(&done);

  __ li(scratch2, Operand(1));
  __ ConvertIntToDouble(scratch2, double_scratch);
  __ fdiv(double_result, double_scratch, double_result);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ fcmpu(double_result, kDoubleRegZero);
  __ bne(&done);
  // double_exponent may not containe the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ ConvertIntToDouble(exponent, double_exponent);

  // Returning or bailing out.
  Counters* counters = isolate()->counters();
  if (exponent_type() == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMathPowRT, 2, 1);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in exponent.
    __ bind(&done);
    __ AllocateHeapNumber(heapnumber, scratch, scratch2, heapnumbermap,
                          &call_runtime);
    __ stfd(double_result,
            FieldMemOperand(heapnumber, HeapNumber::kValueOffset));
    DCHECK(heapnumber.is(r3));
    __ IncrementCounter(counters->math_pow(), 1, scratch, scratch2);
    __ Ret(2);
  } else {
    __ mflr(r0);
    __ push(r0);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 0, 2);
    }
    __ pop(r0);
    __ mtlr(r0);
    __ MovFromFloatResult(double_result);

    __ bind(&done);
    __ IncrementCounter(counters->math_pow(), 1, scratch, scratch2);
    __ Ret();
  }
}


bool CEntryStub::NeedsImmovableCode() { return true; }


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  CreateWeakCellStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  StoreRegistersStateStub::GenerateAheadOfTime(isolate);
  RestoreRegistersStateStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
  TypeofStub::GenerateAheadOfTime(isolate);
}


void StoreRegistersStateStub::GenerateAheadOfTime(Isolate* isolate) {
  StoreRegistersStateStub stub(isolate);
  stub.GetCode();
}


void RestoreRegistersStateStub::GenerateAheadOfTime(Isolate* isolate) {
  RestoreRegistersStateStub stub(isolate);
  stub.GetCode();
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
  // Generate if not already in cache.
  SaveFPRegsMode mode = kSaveFPRegs;
  CEntryStub(isolate, 1, mode).GetCode();
  StoreBufferOverflowStub(isolate, mode).GetCode();
  isolate->set_fp_stubs_generated(true);
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r3: number of arguments including receiver
  // r4: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  __ mr(r15, r4);

  // Compute the argv pointer.
  __ ShiftLeftImm(r4, r3, Operand(kPointerSizeLog2));
  __ add(r4, r4, sp);
  __ subi(r4, r4, Operand(kPointerSize));

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

// PPC LINUX ABI:
#if !ABI_RETURNS_OBJECT_PAIRS_IN_REGS
  // Pass buffer for return value on stack if necessary
  if (result_size() > 1) {
    DCHECK_EQ(2, result_size());
    arg_stack_space += 2;
  }
#endif

  __ EnterExitFrame(save_doubles(), arg_stack_space);

  // Store a copy of argc in callee-saved registers for later.
  __ mr(r14, r3);

  // r3, r14: number of arguments including receiver  (C callee-saved)
  // r4: pointer to the first argument
  // r15: pointer to builtin function  (C callee-saved)

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r5;
#if !ABI_RETURNS_OBJECT_PAIRS_IN_REGS
  if (result_size() > 1) {
    // The return value is 16-byte non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument.
    __ mr(r5, r4);
    __ mr(r4, r3);
    __ addi(r3, sp, Operand((kStackFrameExtraParamSlot + 1) * kPointerSize));
    isolate_reg = r6;
  }
#endif

  // Call C built-in.
  __ mov(isolate_reg, Operand(ExternalReference::isolate_address(isolate())));

#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // Native AIX/PPC64 Linux use a function descriptor.
  __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(r15, kPointerSize));
  __ LoadP(ip, MemOperand(r15, 0));  // Instruction address
  Register target = ip;
#elif ABI_TOC_ADDRESSABILITY_VIA_IP
  __ Move(ip, r15);
  Register target = ip;
#else
  Register target = r15;
#endif

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  Label after_call;
  __ mov_label_addr(r0, &after_call);
  __ StoreP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  __ Call(target);
  __ bind(&after_call);

#if !ABI_RETURNS_OBJECT_PAIRS_IN_REGS
  // If return value is on the stack, pop it to registers.
  if (result_size() > 1) {
    __ LoadP(r4, MemOperand(r3, kPointerSize));
    __ LoadP(r3, MemOperand(r3));
  }
#endif

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r3, Heap::kExceptionRootIndex);
  __ beq(&exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address(
        Isolate::kPendingExceptionAddress, isolate());

    __ mov(r5, Operand(pending_exception_address));
    __ LoadP(r5, MemOperand(r5));
    __ CompareRoot(r5, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay);
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r3:r4: result
  // sp: stack pointer
  // fp: frame pointer
  // r14: still holds argc (callee-saved).
  __ LeaveExitFrame(save_doubles(), r14, true);
  __ blr();

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

  // Ask the runtime for help to determine the handler. This will set r3 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, r3);
    __ li(r3, Operand::Zero());
    __ li(r4, Operand::Zero());
    __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));
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
  __ cmpi(cp, Operand::Zero());
  __ beq(&skip);
  __ StoreP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  // Compute the handler entry address and jump to it.
  ConstantPoolUnavailableScope constant_pool_unavailable(masm);
  __ mov(r4, Operand(pending_handler_code_address));
  __ LoadP(r4, MemOperand(r4));
  __ mov(r5, Operand(pending_handler_offset_address));
  __ LoadP(r5, MemOperand(r5));
  __ addi(r4, r4, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start
  if (FLAG_enable_embedded_constant_pool) {
    __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r4);
  }
  __ add(ip, r4, r5);
  __ Jump(ip);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  // r3: code entry
  // r4: function
  // r5: receiver
  // r6: argc
  // [sp+0]: argv

  Label invoke, handler_entry, exit;

// Called from C
  __ function_descriptor();

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // PPC LINUX ABI:
  // preserve LR in pre-reserved slot in caller's frame
  __ mflr(r0);
  __ StoreP(r0, MemOperand(sp, kStackFrameLRSlot * kPointerSize));

  // Save callee saved registers on the stack.
  __ MultiPush(kCalleeSaved);

  // Floating point regs FPR0 - FRP13 are volatile
  // FPR14-FPR31 are non-volatile, but sub-calls will save them for us

  //  int offset_to_argv = kPointerSize * 22; // matches (22*4) above
  //  __ lwz(r7, MemOperand(sp, offset_to_argv));

  // Push a frame with special values setup to mark it as an entry frame.
  // r3: code entry
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  __ li(r0, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ push(r0);
  if (FLAG_enable_embedded_constant_pool) {
    __ li(kConstantPoolRegister, Operand::Zero());
    __ push(kConstantPoolRegister);
  }
  int marker = type();
  __ LoadSmiLiteral(r0, Smi::FromInt(marker));
  __ push(r0);
  __ push(r0);
  // Save copies of the top frame descriptor on the stack.
  __ mov(r8, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ LoadP(r0, MemOperand(r8));
  __ push(r0);

  // Set up frame pointer for the frame to be pushed.
  __ addi(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ mov(r8, Operand(ExternalReference(js_entry_sp)));
  __ LoadP(r9, MemOperand(r8));
  __ cmpi(r9, Operand::Zero());
  __ bne(&non_outermost_js);
  __ StoreP(fp, MemOperand(r8));
  __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ bind(&non_outermost_js);
  __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(ip);  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke);

  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));

  __ StoreP(r3, MemOperand(ip));
  __ LoadRoot(r3, Heap::kExceptionRootIndex);
  __ b(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r3-r7.
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the b(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ mov(r8, Operand(isolate()->factory()->the_hole_value()));
  __ mov(ip, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));
  __ StoreP(r8, MemOperand(ip));

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r3: code entry
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate());
    __ mov(ip, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate());
    __ mov(ip, Operand(entry));
  }
  __ LoadP(ip, MemOperand(ip));  // deref address

  // Branch and link to JSEntryTrampoline.
  // the address points to the start of the code object, skip the header
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ mtctr(ip);
  __ bctrl();  // make the call

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // r3 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r8);
  __ CmpSmiLiteral(r8, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME), r0);
  __ bne(&non_outermost_js_2);
  __ mov(r9, Operand::Zero());
  __ mov(r8, Operand(ExternalReference(js_entry_sp)));
  __ StoreP(r9, MemOperand(r8));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r6);
  __ mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ StoreP(r6, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ addi(sp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

// Restore callee-saved registers and return.
#ifdef DEBUG
  if (FLAG_debug_code) {
    Label here;
    __ b(&here, SetLK);
    __ bind(&here);
  }
#endif

  __ MultiPop(kCalleeSaved);

  __ LoadP(r0, MemOperand(sp, kStackFrameLRSlot * kPointerSize));
  __ mtctr(r0);
  __ bctr();
}


// Uses registers r3 to r7.
// Expected input (depending on whether args are in registers or on the stack):
// * object: r3 or at sp + 1 * kPointerSize.
// * function: r4 or at sp.
//
// An inlined call site may have been generated before calling this stub.
// In this case the offset to the inline site to patch is passed in r8.
// (See LCodeGen::DoInstanceOfKnownGlobal)
void InstanceofStub::Generate(MacroAssembler* masm) {
  // Call site inlining and patching implies arguments in registers.
  DCHECK(HasArgsInRegisters() || !HasCallSiteInlineCheck());

  // Fixed register usage throughout the stub:
  const Register object = r3;     // Object (lhs).
  Register map = r6;              // Map of the object.
  const Register function = r4;   // Function (rhs).
  const Register prototype = r7;  // Prototype of the function.
  // The map_check_delta was stored in r8
  // The bool_load_delta was stored in r9
  //   (See LCodeGen::DoDeferredLInstanceOfKnownGlobal).
  const Register map_check_delta = r8;
  const Register bool_load_delta = r9;
  const Register inline_site = r10;
  const Register scratch = r5;
  Register scratch3 = no_reg;
  Label slow, loop, is_instance, is_not_instance, not_js_object;

  if (!HasArgsInRegisters()) {
    __ LoadP(object, MemOperand(sp, 1 * kPointerSize));
    __ LoadP(function, MemOperand(sp, 0));
  }

  // Check that the left hand is a JS object and load map.
  __ JumpIfSmi(object, &not_js_object);
  __ IsObjectJSObjectType(object, map, scratch, &not_js_object);

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck() && !ReturnTrueFalseObject()) {
    Label miss;
    __ CompareRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
    __ bne(&miss);
    __ CompareRoot(map, Heap::kInstanceofCacheMapRootIndex);
    __ bne(&miss);
    __ LoadRoot(r3, Heap::kInstanceofCacheAnswerRootIndex);
    __ Ret(HasArgsInRegisters() ? 0 : 2);

    __ bind(&miss);
  }

  // Get the prototype of the function.
  __ TryGetFunctionPrototype(function, prototype, scratch, &slow, true);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(prototype, &slow);
  __ IsObjectJSObjectType(prototype, scratch, scratch, &slow);

  // Update the global instanceof or call site inlined cache with the current
  // map and function. The cached answer will be set when it is known below.
  if (!HasCallSiteInlineCheck()) {
    __ StoreRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
    __ StoreRoot(map, Heap::kInstanceofCacheMapRootIndex);
  } else {
    DCHECK(HasArgsInRegisters());
    // Patch the (relocated) inlined map check.

    const Register offset = map_check_delta;
    __ mflr(inline_site);
    __ sub(inline_site, inline_site, offset);
    // Get the map location in offset and patch it.
    __ GetRelocatedValue(inline_site, offset, scratch);
    __ StoreP(map, FieldMemOperand(offset, Cell::kValueOffset), r0);

    __ mr(r11, map);
    __ RecordWriteField(offset, Cell::kValueOffset, r11, function,
                        kLRHasNotBeenSaved, kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  }

  // Register mapping: r6 is object map and r7 is function prototype.
  // Get prototype of object into r5.
  __ LoadP(scratch, FieldMemOperand(map, Map::kPrototypeOffset));

  // We don't need map any more. Use it as a scratch register.
  scratch3 = map;
  map = no_reg;

  // Loop through the prototype chain looking for the function prototype.
  __ LoadRoot(scratch3, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ cmp(scratch, prototype);
  __ beq(&is_instance);
  __ cmp(scratch, scratch3);
  __ beq(&is_not_instance);
  __ LoadP(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ LoadP(scratch, FieldMemOperand(scratch, Map::kPrototypeOffset));
  __ b(&loop);
  Factory* factory = isolate()->factory();

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ LoadSmiLiteral(r3, Smi::FromInt(0));
    __ StoreRoot(r3, Heap::kInstanceofCacheAnswerRootIndex);
    if (ReturnTrueFalseObject()) {
      __ Move(r3, factory->true_value());
    }
  } else {
    // Patch the call site to return true.
    __ LoadRoot(r3, Heap::kTrueValueRootIndex);
    __ add(inline_site, inline_site, bool_load_delta);
    // Get the boolean result location in scratch and patch it.
    __ SetRelocatedValue(inline_site, scratch, r3);

    if (!ReturnTrueFalseObject()) {
      __ LoadSmiLiteral(r3, Smi::FromInt(0));
    }
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    __ LoadSmiLiteral(r3, Smi::FromInt(1));
    __ StoreRoot(r3, Heap::kInstanceofCacheAnswerRootIndex);
    if (ReturnTrueFalseObject()) {
      __ Move(r3, factory->false_value());
    }
  } else {
    // Patch the call site to return false.
    __ LoadRoot(r3, Heap::kFalseValueRootIndex);
    __ add(inline_site, inline_site, bool_load_delta);
    // Get the boolean result location in scratch and patch it.
    __ SetRelocatedValue(inline_site, scratch, r3);

    if (!ReturnTrueFalseObject()) {
      __ LoadSmiLiteral(r3, Smi::FromInt(1));
    }
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  Label object_not_null, object_not_null_or_smi;
  __ bind(&not_js_object);
  // Before null, smi and string value checks, check that the rhs is a function
  // as for a non-function rhs an exception needs to be thrown.
  __ JumpIfSmi(function, &slow);
  __ CompareObjectType(function, scratch3, scratch, JS_FUNCTION_TYPE);
  __ bne(&slow);

  // Null is not instance of anything.
  __ Cmpi(object, Operand(isolate()->factory()->null_value()), r0);
  __ bne(&object_not_null);
  if (ReturnTrueFalseObject()) {
    __ Move(r3, factory->false_value());
  } else {
    __ LoadSmiLiteral(r3, Smi::FromInt(1));
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null);
  // Smi values are not instances of anything.
  __ JumpIfNotSmi(object, &object_not_null_or_smi);
  if (ReturnTrueFalseObject()) {
    __ Move(r3, factory->false_value());
  } else {
    __ LoadSmiLiteral(r3, Smi::FromInt(1));
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null_or_smi);
  // String values are not instances of anything.
  __ IsObjectJSStringType(object, scratch, &slow);
  if (ReturnTrueFalseObject()) {
    __ Move(r3, factory->false_value());
  } else {
    __ LoadSmiLiteral(r3, Smi::FromInt(1));
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  // Slow-case.  Tail call builtin.
  __ bind(&slow);
  if (!ReturnTrueFalseObject()) {
    if (HasArgsInRegisters()) {
      __ Push(r3, r4);
    }
    __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
  } else {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r3, r4);
      __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_FUNCTION);
    }
    if (CpuFeatures::IsSupported(ISELECT)) {
      __ cmpi(r3, Operand::Zero());
      __ LoadRoot(r3, Heap::kTrueValueRootIndex);
      __ LoadRoot(r4, Heap::kFalseValueRootIndex);
      __ isel(eq, r3, r3, r4);
    } else {
      Label true_value, done;
      __ cmpi(r3, Operand::Zero());
      __ beq(&true_value);

      __ LoadRoot(r3, Heap::kFalseValueRootIndex);
      __ b(&done);

      __ bind(&true_value);
      __ LoadRoot(r3, Heap::kTrueValueRootIndex);

      __ bind(&done);
    }
    __ Ret(HasArgsInRegisters() ? 0 : 2);
  }
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // Ensure that the vector and slot registers won't be clobbered before
  // calling the miss handler.
  DCHECK(!AreAliased(r7, r8, LoadWithVectorDescriptor::VectorRegister(),
                     LoadWithVectorDescriptor::SlotRegister()));

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, r7,
                                                          r8, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void LoadIndexedStringStub::Generate(MacroAssembler* masm) {
  // Return address is in lr.
  Label miss;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register index = LoadDescriptor::NameRegister();
  Register scratch = r8;
  Register result = r3;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  DCHECK(!scratch.is(LoadWithVectorDescriptor::VectorRegister()) &&
         result.is(LoadWithVectorDescriptor::SlotRegister()));

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
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, PART_OF_IC_HANDLER, call_helper);

  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::KEYED_LOAD_IC));
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The displacement is the offset of the last parameter (if any)
  // relative to the frame pointer.
  const int kDisplacement =
      StandardFrameConstants::kCallerSPOffset - kPointerSize;
  DCHECK(r4.is(ArgumentsAccessReadDescriptor::index()));
  DCHECK(r3.is(ArgumentsAccessReadDescriptor::parameter_count()));

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(r4, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r6, MemOperand(r5, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r6, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor);

  // Check index against formal parameters count limit passed in
  // through register r3. Use unsigned comparison to get negative
  // check for free.
  __ cmpl(r4, r3);
  __ bge(&slow);

  // Read the argument from the stack and return it.
  __ sub(r6, r3, r4);
  __ SmiToPtrArrayOffset(r6, r6);
  __ add(r6, fp, r6);
  __ LoadP(r3, MemOperand(r6, kDisplacement));
  __ blr();

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ LoadP(r3, MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmpl(r4, r3);
  __ bge(&slow);

  // Read the argument from the adaptor frame and return it.
  __ sub(r6, r3, r4);
  __ SmiToPtrArrayOffset(r6, r6);
  __ add(r6, r5, r6);
  __ LoadP(r3, MemOperand(r6, kDisplacement));
  __ blr();

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ push(r4);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewSloppySlow(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[1] : receiver displacement
  // sp[2] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ LoadP(r6, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r5, MemOperand(r6, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r5, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&runtime);

  // Patch the arguments.length and the parameters pointer in the current frame.
  __ LoadP(r5, MemOperand(r6, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ StoreP(r5, MemOperand(sp, 0 * kPointerSize));
  __ SmiToPtrArrayOffset(r5, r5);
  __ add(r6, r6, r5);
  __ addi(r6, r6, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r6, MemOperand(sp, 1 * kPointerSize));

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewSloppyArguments, 3, 1);
}


void ArgumentsAccessStub::GenerateNewSloppyFast(MacroAssembler* masm) {
  // Stack layout:
  //  sp[0] : number of parameters (tagged)
  //  sp[1] : address of receiver argument
  //  sp[2] : function
  // Registers used over whole function:
  //  r9 : allocated object (tagged)
  //  r11 : mapped parameter count (tagged)

  __ LoadP(r4, MemOperand(sp, 0 * kPointerSize));
  // r4 = parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ LoadP(r6, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r5, MemOperand(r6, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r5, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor_frame);

  // No adaptor, parameter count = argument count.
  __ mr(r5, r4);
  __ b(&try_allocate);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ LoadP(r5, MemOperand(r6, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiToPtrArrayOffset(r7, r5);
  __ add(r6, r6, r7);
  __ addi(r6, r6, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r6, MemOperand(sp, 1 * kPointerSize));

  // r4 = parameter count (tagged)
  // r5 = argument count (tagged)
  // Compute the mapped parameter count = min(r4, r5) in r4.
  __ cmp(r4, r5);
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ isel(lt, r4, r4, r5);
  } else {
    Label skip;
    __ blt(&skip);
    __ mr(r4, r5);
    __ bind(&skip);
  }

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  __ CmpSmiLiteral(r4, Smi::FromInt(0), r0);
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ SmiToPtrArrayOffset(r11, r4);
    __ addi(r11, r11, Operand(kParameterMapHeaderSize));
    __ isel(eq, r11, r0, r11);
  } else {
    Label skip2, skip3;
    __ bne(&skip2);
    __ li(r11, Operand::Zero());
    __ b(&skip3);
    __ bind(&skip2);
    __ SmiToPtrArrayOffset(r11, r4);
    __ addi(r11, r11, Operand(kParameterMapHeaderSize));
    __ bind(&skip3);
  }

  // 2. Backing store.
  __ SmiToPtrArrayOffset(r7, r5);
  __ add(r11, r11, r7);
  __ addi(r11, r11, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ addi(r11, r11, Operand(Heap::kSloppyArgumentsObjectSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(r11, r3, r6, r7, &runtime, TAG_OBJECT);

  // r3 = address of new object(s) (tagged)
  // r5 = argument count (smi-tagged)
  // Get the arguments boilerplate from the current native context into r4.
  const int kNormalOffset =
      Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);

  __ LoadP(r7,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(r7, FieldMemOperand(r7, GlobalObject::kNativeContextOffset));
  __ cmpi(r4, Operand::Zero());
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ LoadP(r11, MemOperand(r7, kNormalOffset));
    __ LoadP(r7, MemOperand(r7, kAliasedOffset));
    __ isel(eq, r7, r11, r7);
  } else {
    Label skip4, skip5;
    __ bne(&skip4);
    __ LoadP(r7, MemOperand(r7, kNormalOffset));
    __ b(&skip5);
    __ bind(&skip4);
    __ LoadP(r7, MemOperand(r7, kAliasedOffset));
    __ bind(&skip5);
  }

  // r3 = address of new object (tagged)
  // r4 = mapped parameter count (tagged)
  // r5 = argument count (smi-tagged)
  // r7 = address of arguments map (tagged)
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kMapOffset), r0);
  __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r6, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
  __ StoreP(r6, FieldMemOperand(r3, JSObject::kElementsOffset), r0);

  // Set up the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  __ LoadP(r6, MemOperand(sp, 2 * kPointerSize));
  __ AssertNotSmi(r6);
  const int kCalleeOffset =
      JSObject::kHeaderSize + Heap::kArgumentsCalleeIndex * kPointerSize;
  __ StoreP(r6, FieldMemOperand(r3, kCalleeOffset), r0);

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(r5);
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  const int kLengthOffset =
      JSObject::kHeaderSize + Heap::kArgumentsLengthIndex * kPointerSize;
  __ StoreP(r5, FieldMemOperand(r3, kLengthOffset), r0);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, r7 will point there, otherwise
  // it will point to the backing store.
  __ addi(r7, r3, Operand(Heap::kSloppyArgumentsObjectSize));
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kElementsOffset), r0);

  // r3 = address of new object (tagged)
  // r4 = mapped parameter count (tagged)
  // r5 = argument count (tagged)
  // r7 = address of parameter map or backing store (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ CmpSmiLiteral(r4, Smi::FromInt(0), r0);
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ isel(eq, r6, r7, r6);
    __ beq(&skip_parameter_map);
  } else {
    Label skip6;
    __ bne(&skip6);
    // Move backing store address to r6, because it is
    // expected there when filling in the unmapped arguments.
    __ mr(r6, r7);
    __ b(&skip_parameter_map);
    __ bind(&skip6);
  }

  __ LoadRoot(r9, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ StoreP(r9, FieldMemOperand(r7, FixedArray::kMapOffset), r0);
  __ AddSmiLiteral(r9, r4, Smi::FromInt(2), r0);
  __ StoreP(r9, FieldMemOperand(r7, FixedArray::kLengthOffset), r0);
  __ StoreP(cp, FieldMemOperand(r7, FixedArray::kHeaderSize + 0 * kPointerSize),
            r0);
  __ SmiToPtrArrayOffset(r9, r4);
  __ add(r9, r7, r9);
  __ addi(r9, r9, Operand(kParameterMapHeaderSize));
  __ StoreP(r9, FieldMemOperand(r7, FixedArray::kHeaderSize + 1 * kPointerSize),
            r0);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ mr(r9, r4);
  __ LoadP(r11, MemOperand(sp, 0 * kPointerSize));
  __ AddSmiLiteral(r11, r11, Smi::FromInt(Context::MIN_CONTEXT_SLOTS), r0);
  __ sub(r11, r11, r4);
  __ LoadRoot(r10, Heap::kTheHoleValueRootIndex);
  __ SmiToPtrArrayOffset(r6, r9);
  __ add(r6, r7, r6);
  __ addi(r6, r6, Operand(kParameterMapHeaderSize));

  // r9 = loop variable (tagged)
  // r4 = mapping index (tagged)
  // r6 = address of backing store (tagged)
  // r7 = address of parameter map (tagged)
  // r8 = temporary scratch (a.o., for address calculation)
  // r10 = the hole value
  __ b(&parameters_test);

  __ bind(&parameters_loop);
  __ SubSmiLiteral(r9, r9, Smi::FromInt(1), r0);
  __ SmiToPtrArrayOffset(r8, r9);
  __ addi(r8, r8, Operand(kParameterMapHeaderSize - kHeapObjectTag));
  __ StorePX(r11, MemOperand(r8, r7));
  __ subi(r8, r8, Operand(kParameterMapHeaderSize - FixedArray::kHeaderSize));
  __ StorePX(r10, MemOperand(r8, r6));
  __ AddSmiLiteral(r11, r11, Smi::FromInt(1), r0);
  __ bind(&parameters_test);
  __ CmpSmiLiteral(r9, Smi::FromInt(0), r0);
  __ bne(&parameters_loop);

  __ bind(&skip_parameter_map);
  // r5 = argument count (tagged)
  // r6 = address of backing store (tagged)
  // r8 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(r8, Heap::kFixedArrayMapRootIndex);
  __ StoreP(r8, FieldMemOperand(r6, FixedArray::kMapOffset), r0);
  __ StoreP(r5, FieldMemOperand(r6, FixedArray::kLengthOffset), r0);

  Label arguments_loop, arguments_test;
  __ mr(r11, r4);
  __ LoadP(r7, MemOperand(sp, 1 * kPointerSize));
  __ SmiToPtrArrayOffset(r8, r11);
  __ sub(r7, r7, r8);
  __ b(&arguments_test);

  __ bind(&arguments_loop);
  __ subi(r7, r7, Operand(kPointerSize));
  __ LoadP(r9, MemOperand(r7, 0));
  __ SmiToPtrArrayOffset(r8, r11);
  __ add(r8, r6, r8);
  __ StoreP(r9, FieldMemOperand(r8, FixedArray::kHeaderSize), r0);
  __ AddSmiLiteral(r11, r11, Smi::FromInt(1), r0);

  __ bind(&arguments_test);
  __ cmp(r11, r5);
  __ blt(&arguments_loop);

  // Return and remove the on-stack parameters.
  __ addi(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // r5 = argument count (tagged)
  __ bind(&runtime);
  __ StoreP(r5, MemOperand(sp, 0 * kPointerSize));  // Patch argument count.
  __ TailCallRuntime(Runtime::kNewSloppyArguments, 3, 1);
}


void LoadIndexedInterceptorStub::Generate(MacroAssembler* masm) {
  // Return address is in lr.
  Label slow;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();

  // Check that the key is an array index, that is Uint32.
  __ TestIfPositiveSmi(key, r0);
  __ bne(&slow, cr0);

  // Everything is fine, call runtime.
  __ Push(receiver, key);  // Receiver, key.

  // Perform tail call to the entry.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kLoadElementWithInterceptor),
                        masm->isolate()),
      2, 1);

  __ bind(&slow);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::KEYED_LOAD_IC));
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[4] : receiver displacement
  // sp[8] : function
  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r6, MemOperand(r5, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r6, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor_frame);

  // Get the length from the frame.
  __ LoadP(r4, MemOperand(sp, 0));
  __ b(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ LoadP(r4, MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ StoreP(r4, MemOperand(sp, 0));
  __ SmiToPtrArrayOffset(r6, r4);
  __ add(r6, r5, r6);
  __ addi(r6, r6, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r6, MemOperand(sp, 1 * kPointerSize));

  // Try the new space allocation. Start out with computing the size
  // of the arguments object and the elements array in words.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ cmpi(r4, Operand::Zero());
  __ beq(&add_arguments_object);
  __ SmiUntag(r4);
  __ addi(r4, r4, Operand(FixedArray::kHeaderSize / kPointerSize));
  __ bind(&add_arguments_object);
  __ addi(r4, r4, Operand(Heap::kStrictArgumentsObjectSize / kPointerSize));

  // Do the allocation of both objects in one go.
  __ Allocate(r4, r3, r5, r6, &runtime,
              static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));

  // Get the arguments boilerplate from the current native context.
  __ LoadP(r7,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(r7, FieldMemOperand(r7, GlobalObject::kNativeContextOffset));
  __ LoadP(
      r7,
      MemOperand(r7, Context::SlotOffset(Context::STRICT_ARGUMENTS_MAP_INDEX)));

  __ StoreP(r7, FieldMemOperand(r3, JSObject::kMapOffset), r0);
  __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r6, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
  __ StoreP(r6, FieldMemOperand(r3, JSObject::kElementsOffset), r0);

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ LoadP(r4, MemOperand(sp, 0 * kPointerSize));
  __ AssertSmi(r4);
  __ StoreP(r4,
            FieldMemOperand(r3, JSObject::kHeaderSize +
                                    Heap::kArgumentsLengthIndex * kPointerSize),
            r0);

  // If there are no actual arguments, we're done.
  Label done;
  __ cmpi(r4, Operand::Zero());
  __ beq(&done);

  // Get the parameters pointer from the stack.
  __ LoadP(r5, MemOperand(sp, 1 * kPointerSize));

  // Set up the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ addi(r7, r3, Operand(Heap::kStrictArgumentsObjectSize));
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kElementsOffset), r0);
  __ LoadRoot(r6, Heap::kFixedArrayMapRootIndex);
  __ StoreP(r6, FieldMemOperand(r7, FixedArray::kMapOffset), r0);
  __ StoreP(r4, FieldMemOperand(r7, FixedArray::kLengthOffset), r0);
  // Untag the length for the loop.
  __ SmiUntag(r4);

  // Copy the fixed array slots.
  Label loop;
  // Set up r7 to point just prior to the first array slot.
  __ addi(r7, r7,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
  __ mtctr(r4);
  __ bind(&loop);
  // Pre-decrement r5 with kPointerSize on each iteration.
  // Pre-decrement in order to skip receiver.
  __ LoadPU(r6, MemOperand(r5, -kPointerSize));
  // Pre-increment r7 with kPointerSize on each iteration.
  __ StorePU(r6, MemOperand(r7, kPointerSize));
  __ bdnz(&loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ addi(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewStrictArguments, 3, 1);
}


void RestParamAccessStub::GenerateNew(MacroAssembler* masm) {
  // Stack layout on entry.
  //  sp[0] : language mode
  //  sp[4] : index of rest parameter
  //  sp[8] : number of parameters
  //  sp[12] : receiver displacement

  Label runtime;
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r6, MemOperand(r5, StandardFrameConstants::kContextOffset));
  __ CmpSmiLiteral(r6, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&runtime);

  // Patch the arguments.length and the parameters pointer.
  __ LoadP(r4, MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ StoreP(r4, MemOperand(sp, 2 * kPointerSize));
  __ SmiToPtrArrayOffset(r6, r4);
  __ add(r6, r5, r6);
  __ addi(r6, r6, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r6, MemOperand(sp, 3 * kPointerSize));

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewRestParam, 4, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
// Just jump directly to runtime if native RegExp is not selected at compile
// time or if regexp entry in generated code is turned off runtime switch or
// at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#else  // V8_INTERPRETED_REGEXP

  // Stack frame on entry.
  //  sp[0]: last_match_info (expected JSArray)
  //  sp[4]: previous index
  //  sp[8]: subject string
  //  sp[12]: JSRegExp object

  const int kLastMatchInfoOffset = 0 * kPointerSize;
  const int kPreviousIndexOffset = 1 * kPointerSize;
  const int kSubjectOffset = 2 * kPointerSize;
  const int kJSRegExpOffset = 3 * kPointerSize;

  Label runtime, br_over, encoding_type_UC16;

  // Allocation of registers for this function. These are in callee save
  // registers and will be preserved by the call to the native RegExp code, as
  // this code is called using the normal C calling convention. When calling
  // directly from generated code the native RegExp code will not do a GC and
  // therefore the content of these registers are safe to use after the call.
  Register subject = r14;
  Register regexp_data = r15;
  Register last_match_info_elements = r16;
  Register code = r17;

  // Ensure register assigments are consistent with callee save masks
  DCHECK(subject.bit() & kCalleeSaved);
  DCHECK(regexp_data.bit() & kCalleeSaved);
  DCHECK(last_match_info_elements.bit() & kCalleeSaved);
  DCHECK(code.bit() & kCalleeSaved);

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ mov(r3, Operand(address_of_regexp_stack_memory_size));
  __ LoadP(r3, MemOperand(r3, 0));
  __ cmpi(r3, Operand::Zero());
  __ beq(&runtime);

  // Check that the first argument is a JSRegExp object.
  __ LoadP(r3, MemOperand(sp, kJSRegExpOffset));
  __ JumpIfSmi(r3, &runtime);
  __ CompareObjectType(r3, r4, r4, JS_REGEXP_TYPE);
  __ bne(&runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ LoadP(regexp_data, FieldMemOperand(r3, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ TestIfSmi(regexp_data, r0);
    __ Check(ne, kUnexpectedTypeForRegExpDataFixedArrayExpected, cr0);
    __ CompareObjectType(regexp_data, r3, r3, FIXED_ARRAY_TYPE);
    __ Check(eq, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ LoadP(r3, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  // DCHECK(Smi::FromInt(JSRegExp::IRREGEXP) < (char *)0xffffu);
  __ CmpSmiLiteral(r3, Smi::FromInt(JSRegExp::IRREGEXP), r0);
  __ bne(&runtime);

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ LoadP(r5,
           FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // SmiToShortArrayOffset accomplishes the multiplication by 2 and
  // SmiUntag (which is a nop for 32-bit).
  __ SmiToShortArrayOffset(r5, r5);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmpli(r5, Operand(Isolate::kJSRegexpStaticOffsetsVectorSize - 2));
  __ bgt(&runtime);

  // Reset offset for possibly sliced string.
  __ li(r11, Operand::Zero());
  __ LoadP(subject, MemOperand(sp, kSubjectOffset));
  __ JumpIfSmi(subject, &runtime);
  __ mr(r6, subject);  // Make a copy of the original subject string.
  __ LoadP(r3, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbz(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
  // subject: subject string
  // r6: subject string
  // r3: subject string instance type
  // regexp_data: RegExp data (FixedArray)
  // Handle subject string according to its encoding and representation:
  // (1) Sequential string?  If yes, go to (5).
  // (2) Anything but sequential or cons?  If yes, go to (6).
  // (3) Cons string.  If the string is flat, replace subject with first string.
  //     Otherwise bailout.
  // (4) Is subject external?  If yes, go to (7).
  // (5) Sequential string.  Load regexp code according to encoding.
  // (E) Carry on.
  /// [...]

  // Deferred code at the end of the stub:
  // (6) Not a long external string?  If yes, go to (8).
  // (7) External string.  Make it, offset-wise, look like a sequential string.
  //     Go to (5).
  // (8) Short external string or not a string?  If yes, bail out to runtime.
  // (9) Sliced string.  Replace subject with parent.  Go to (4).

  Label seq_string /* 5 */, external_string /* 7 */, check_underlying /* 4 */,
      not_seq_nor_cons /* 6 */, not_long_external /* 8 */;

  // (1) Sequential string?  If yes, go to (5).
  STATIC_ASSERT((kIsNotStringMask | kStringRepresentationMask |
                 kShortExternalStringMask) == 0x93);
  __ andi(r4, r3, Operand(kIsNotStringMask | kStringRepresentationMask |
                          kShortExternalStringMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ beq(&seq_string, cr0);  // Go to (5).

  // (2) Anything but sequential or cons?  If yes, go to (6).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  STATIC_ASSERT(kExternalStringTag < 0xffffu);
  __ cmpi(r4, Operand(kExternalStringTag));
  __ bge(&not_seq_nor_cons);  // Go to (6).

  // (3) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ LoadP(r3, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ CompareRoot(r3, Heap::kempty_stringRootIndex);
  __ bne(&runtime);
  __ LoadP(subject, FieldMemOperand(subject, ConsString::kFirstOffset));

  // (4) Is subject external?  If yes, go to (7).
  __ bind(&check_underlying);
  __ LoadP(r3, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbz(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSeqStringTag == 0);
  STATIC_ASSERT(kStringRepresentationMask == 3);
  __ andi(r0, r3, Operand(kStringRepresentationMask));
  // The underlying external string is never a short external string.
  STATIC_ASSERT(ExternalString::kMaxShortLength < ConsString::kMinLength);
  STATIC_ASSERT(ExternalString::kMaxShortLength < SlicedString::kMinLength);
  __ bne(&external_string, cr0);  // Go to (7).

  // (5) Sequential string.  Load regexp code according to encoding.
  __ bind(&seq_string);
  // subject: sequential subject string (or look-alike, external string)
  // r6: original subject string
  // Load previous index and check range before r6 is overwritten.  We have to
  // use r6 instead of subject here because subject might have been only made
  // to look like a sequential string when it actually is an external string.
  __ LoadP(r4, MemOperand(sp, kPreviousIndexOffset));
  __ JumpIfNotSmi(r4, &runtime);
  __ LoadP(r6, FieldMemOperand(r6, String::kLengthOffset));
  __ cmpl(r6, r4);
  __ ble(&runtime);
  __ SmiUntag(r4);

  STATIC_ASSERT(4 == kOneByteStringTag);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  STATIC_ASSERT(kStringEncodingMask == 4);
  __ ExtractBitMask(r6, r3, kStringEncodingMask, SetRC);
  __ beq(&encoding_type_UC16, cr0);
  __ LoadP(code,
           FieldMemOperand(regexp_data, JSRegExp::kDataOneByteCodeOffset));
  __ b(&br_over);
  __ bind(&encoding_type_UC16);
  __ LoadP(code, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset));
  __ bind(&br_over);

  // (E) Carry on.  String handling is done.
  // code: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(code, &runtime);

  // r4: previous index
  // r6: encoding of subject string (1 if one_byte, 0 if two_byte);
  // code: Address of generated regexp code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(isolate()->counters()->regexp_entry_native(), 1, r3, r5);

  // Isolates: note we add an additional parameter here (isolate pointer).
  const int kRegExpExecuteArguments = 10;
  const int kParameterRegisters = 8;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers.

  // Argument 10 (in stack parameter area): Pass current isolate address.
  __ mov(r3, Operand(ExternalReference::isolate_address(isolate())));
  __ StoreP(r3, MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kPointerSize));

  // Argument 9 is a dummy that reserves the space used for
  // the return address added by the ExitFrame in native calls.

  // Argument 8 (r10): Indicate that this is a direct call from JavaScript.
  __ li(r10, Operand(1));

  // Argument 7 (r9): Start (high end) of backtracking stack memory area.
  __ mov(r3, Operand(address_of_regexp_stack_memory_address));
  __ LoadP(r3, MemOperand(r3, 0));
  __ mov(r5, Operand(address_of_regexp_stack_memory_size));
  __ LoadP(r5, MemOperand(r5, 0));
  __ add(r9, r3, r5);

  // Argument 6 (r8): Set the number of capture registers to zero to force
  // global egexps to behave as non-global.  This does not affect non-global
  // regexps.
  __ li(r8, Operand::Zero());

  // Argument 5 (r7): static offsets vector buffer.
  __ mov(
      r7,
      Operand(ExternalReference::address_of_static_offsets_vector(isolate())));

  // For arguments 4 (r6) and 3 (r5) get string length, calculate start of data
  // and calculate the shift of the index (0 for one-byte and 1 for two-byte).
  __ addi(r18, subject, Operand(SeqString::kHeaderSize - kHeapObjectTag));
  __ xori(r6, r6, Operand(1));
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to two pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize.)
  __ LoadP(subject, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));
  // If slice offset is not 0, load the length from the original sliced string.
  // Argument 4, r6: End of string data
  // Argument 3, r5: Start of string data
  // Prepare start and end index of the input.
  __ ShiftLeft_(r11, r11, r6);
  __ add(r11, r18, r11);
  __ ShiftLeft_(r5, r4, r6);
  __ add(r5, r11, r5);

  __ LoadP(r18, FieldMemOperand(subject, String::kLengthOffset));
  __ SmiUntag(r18);
  __ ShiftLeft_(r6, r18, r6);
  __ add(r6, r11, r6);

  // Argument 2 (r4): Previous index.
  // Already there

  // Argument 1 (r3): Subject string.
  __ mr(r3, subject);

  // Locate the code entry and call it.
  __ addi(code, code, Operand(Code::kHeaderSize - kHeapObjectTag));


#if ABI_USES_FUNCTION_DESCRIPTORS && defined(USE_SIMULATOR)
  // Even Simulated AIX/PPC64 Linux uses a function descriptor for the
  // RegExp routine.  Extract the instruction address here since
  // DirectCEntryStub::GenerateCall will not do it for calls out to
  // what it thinks is C code compiled for the simulator/host
  // platform.
  __ LoadP(code, MemOperand(code, 0));  // Instruction address
#endif

  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm, code);

  __ LeaveExitFrame(false, no_reg, true);

  // r3: result (int32)
  // subject: subject string (callee saved)
  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)
  // Check the result.
  Label success;
  __ cmpwi(r3, Operand(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ beq(&success);
  Label failure;
  __ cmpwi(r3, Operand(NativeRegExpMacroAssembler::FAILURE));
  __ beq(&failure);
  __ cmpwi(r3, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ bne(&runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ mov(r4, Operand(isolate()->factory()->the_hole_value()));
  __ mov(r5, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));
  __ LoadP(r3, MemOperand(r5, 0));
  __ cmp(r3, r4);
  __ beq(&runtime);

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow, 4, 1);

  __ bind(&failure);
  // For failure and exception return null.
  __ mov(r3, Operand(isolate()->factory()->null_value()));
  __ addi(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ LoadP(r4,
           FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  // SmiToShortArrayOffset accomplishes the multiplication by 2 and
  // SmiUntag (which is a nop for 32-bit).
  __ SmiToShortArrayOffset(r4, r4);
  __ addi(r4, r4, Operand(2));

  __ LoadP(r3, MemOperand(sp, kLastMatchInfoOffset));
  __ JumpIfSmi(r3, &runtime);
  __ CompareObjectType(r3, r5, r5, JS_ARRAY_TYPE);
  __ bne(&runtime);
  // Check that the JSArray is in fast case.
  __ LoadP(last_match_info_elements,
           FieldMemOperand(r3, JSArray::kElementsOffset));
  __ LoadP(r3,
           FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ CompareRoot(r3, Heap::kFixedArrayMapRootIndex);
  __ bne(&runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ LoadP(
      r3, FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ addi(r5, r4, Operand(RegExpImpl::kLastMatchOverhead));
  __ SmiUntag(r0, r3);
  __ cmp(r5, r0);
  __ bgt(&runtime);

  // r4: number of capture registers
  // subject: subject string
  // Store the capture count.
  __ SmiTag(r5, r4);
  __ StoreP(r5, FieldMemOperand(last_match_info_elements,
                                RegExpImpl::kLastCaptureCountOffset),
            r0);
  // Store last subject and last input.
  __ StoreP(subject, FieldMemOperand(last_match_info_elements,
                                     RegExpImpl::kLastSubjectOffset),
            r0);
  __ mr(r5, subject);
  __ RecordWriteField(last_match_info_elements, RegExpImpl::kLastSubjectOffset,
                      subject, r10, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ mr(subject, r5);
  __ StoreP(subject, FieldMemOperand(last_match_info_elements,
                                     RegExpImpl::kLastInputOffset),
            r0);
  __ RecordWriteField(last_match_info_elements, RegExpImpl::kLastInputOffset,
                      subject, r10, kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ mov(r5, Operand(address_of_static_offsets_vector));

  // r4: number of capture registers
  // r5: offsets vector
  Label next_capture;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ addi(
      r3, last_match_info_elements,
      Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag - kPointerSize));
  __ addi(r5, r5, Operand(-kIntSize));  // bias down for lwzu
  __ mtctr(r4);
  __ bind(&next_capture);
  // Read the value from the static offsets vector buffer.
  __ lwzu(r6, MemOperand(r5, kIntSize));
  // Store the smi value in the last match info.
  __ SmiTag(r6);
  __ StorePU(r6, MemOperand(r3, kPointerSize));
  __ bdnz(&next_capture);

  // Return last match info.
  __ LoadP(r3, MemOperand(sp, kLastMatchInfoOffset));
  __ addi(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);

  // Deferred code for string handling.
  // (6) Not a long external string?  If yes, go to (8).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set.
  __ bgt(&not_long_external);  // Go to (8).

  // (7) External string.  Make it, offset-wise, look like a sequential string.
  __ bind(&external_string);
  __ LoadP(r3, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbz(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    STATIC_ASSERT(kIsIndirectStringMask == 1);
    __ andi(r0, r3, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound, cr0);
  }
  __ LoadP(subject,
           FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ subi(subject, subject,
          Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ b(&seq_string);  // Go to (5).

  // (8) Short external string or not a string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag != 0);
  __ andi(r0, r4, Operand(kIsNotStringMask | kShortExternalStringMask));
  __ bne(&runtime, cr0);

  // (9) Sliced string.  Replace subject with parent.  Go to (4).
  // Load offset into r11 and replace subject string with parent.
  __ LoadP(r11, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ SmiUntag(r11);
  __ LoadP(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ b(&check_underlying);  // Go to (4).
#endif  // V8_INTERPRETED_REGEXP
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // r3 : number of arguments to the construct function
  // r5 : Feedback vector
  // r6 : slot in feedback vector (Smi)
  // r4 : the function to call
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

  // Number-of-arguments register must be smi-tagged to call out.
  __ SmiTag(r3);
  __ Push(r6, r5, r4, r3);

  __ CallStub(stub);

  __ Pop(r6, r5, r4, r3);
  __ SmiUntag(r3);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // r3 : number of arguments to the construct function
  // r4 : the function to call
  // r5 : Feedback vector
  // r6 : slot in feedback vector (Smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*TypeFeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  // Load the cache state into r7.
  __ SmiToPtrArrayOffset(r7, r6);
  __ add(r7, r5, r7);
  __ LoadP(r7, FieldMemOperand(r7, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if r7 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in type-feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = r8;
  Register weak_value = r9;
  __ LoadP(weak_value, FieldMemOperand(r7, WeakCell::kValueOffset));
  __ cmp(r4, weak_value);
  __ beq(&done);
  __ CompareRoot(r7, Heap::kmegamorphic_symbolRootIndex);
  __ beq(&done);
  __ LoadP(feedback_map, FieldMemOperand(r7, HeapObject::kMapOffset));
  __ CompareRoot(feedback_map, Heap::kWeakCellMapRootIndex);
  __ bne(FLAG_pretenuring_call_new ? &miss : &check_allocation_site);

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ JumpIfSmi(weak_value, &initialize);
  __ b(&megamorphic);

  if (!FLAG_pretenuring_call_new) {
    __ bind(&check_allocation_site);
    // If we came here, we need to see if we are the array function.
    // If we didn't have a matching function, and we didn't find the megamorph
    // sentinel, then we have in the slot either some other function or an
    // AllocationSite.
    __ CompareRoot(feedback_map, Heap::kAllocationSiteMapRootIndex);
    __ bne(&miss);

    // Make sure the function is the Array() function
    __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r7);
    __ cmp(r4, r7);
    __ bne(&megamorphic);
    __ b(&done);
  }

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(r7, Heap::kuninitialized_symbolRootIndex);
  __ beq(&initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ SmiToPtrArrayOffset(r7, r6);
  __ add(r7, r5, r7);
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ StoreP(ip, FieldMemOperand(r7, FixedArray::kHeaderSize), r0);
  __ jmp(&done);

  // An uninitialized cache is patched with the function
  __ bind(&initialize);

  if (!FLAG_pretenuring_call_new) {
    // Make sure the function is the Array() function.
    __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r7);
    __ cmp(r4, r7);
    __ bne(&not_array_function);

    // The target function is the Array constructor,
    // Create an AllocationSite if we don't already have it, store it in the
    // slot.
    CreateAllocationSiteStub create_stub(masm->isolate());
    CallStubInRecordCallTarget(masm, &create_stub);
    __ b(&done);

    __ bind(&not_array_function);
  }

  CreateWeakCellStub create_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &create_stub);
  __ bind(&done);
}


static void EmitContinueIfStrictOrNative(MacroAssembler* masm, Label* cont) {
  // Do not transform the receiver for strict mode functions and natives.
  __ LoadP(r6, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ lwz(r7, FieldMemOperand(r6, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestBit(r7,
#if V8_TARGET_ARCH_PPC64
             SharedFunctionInfo::kStrictModeFunction,
#else
             SharedFunctionInfo::kStrictModeFunction + kSmiTagSize,
#endif
             r0);
  __ bne(cont, cr0);

  // Do not transform the receiver for native.
  __ TestBit(r7,
#if V8_TARGET_ARCH_PPC64
             SharedFunctionInfo::kNative,
#else
             SharedFunctionInfo::kNative + kSmiTagSize,
#endif
             r0);
  __ bne(cont, cr0);
}


static void EmitSlowCase(MacroAssembler* masm, int argc, Label* non_function) {
  // Check for function proxy.
  STATIC_ASSERT(JS_FUNCTION_PROXY_TYPE < 0xffffu);
  __ cmpi(r7, Operand(JS_FUNCTION_PROXY_TYPE));
  __ bne(non_function);
  __ push(r4);  // put proxy as additional argument
  __ li(r3, Operand(argc + 1));
  __ li(r5, Operand::Zero());
  __ GetBuiltinFunction(r4, Builtins::CALL_FUNCTION_PROXY);
  {
    Handle<Code> adaptor =
        masm->isolate()->builtins()->ArgumentsAdaptorTrampoline();
    __ Jump(adaptor, RelocInfo::CODE_TARGET);
  }

  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ bind(non_function);
  __ StoreP(r4, MemOperand(sp, argc * kPointerSize), r0);
  __ li(r3, Operand(argc));  // Set up the number of arguments.
  __ li(r5, Operand::Zero());
  __ GetBuiltinFunction(r4, Builtins::CALL_NON_FUNCTION);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


static void EmitWrapCase(MacroAssembler* masm, int argc, Label* cont) {
  // Wrap the receiver and patch it back onto the stack.
  {
    FrameAndConstantPoolScope frame_scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r6);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ pop(r4);
  }
  __ StoreP(r3, MemOperand(sp, argc * kPointerSize), r0);
  __ b(cont);
}


static void CallFunctionNoFeedback(MacroAssembler* masm, int argc,
                                   bool needs_checks, bool call_as_method) {
  // r4 : the function to call
  Label slow, non_function, wrap, cont;

  if (needs_checks) {
    // Check that the function is really a JavaScript function.
    // r4: pushed function (to be verified)
    __ JumpIfSmi(r4, &non_function);

    // Goto slow case if we do not have a function.
    __ CompareObjectType(r4, r7, r7, JS_FUNCTION_TYPE);
    __ bne(&slow);
  }

  // Fast-case: Invoke the function now.
  // r4: pushed function
  ParameterCount actual(argc);

  if (call_as_method) {
    if (needs_checks) {
      EmitContinueIfStrictOrNative(masm, &cont);
    }

    // Compute the receiver in sloppy mode.
    __ LoadP(r6, MemOperand(sp, argc * kPointerSize), r0);

    if (needs_checks) {
      __ JumpIfSmi(r6, &wrap);
      __ CompareObjectType(r6, r7, r7, FIRST_SPEC_OBJECT_TYPE);
      __ blt(&wrap);
    } else {
      __ b(&wrap);
    }

    __ bind(&cont);
  }

  __ InvokeFunction(r4, actual, JUMP_FUNCTION, NullCallWrapper());

  if (needs_checks) {
    // Slow-case: Non-function called.
    __ bind(&slow);
    EmitSlowCase(masm, argc, &non_function);
  }

  if (call_as_method) {
    __ bind(&wrap);
    EmitWrapCase(masm, argc, &cont);
  }
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  CallFunctionNoFeedback(masm, argc(), NeedsChecks(), CallAsMethod());
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // r3 : number of arguments
  // r4 : the function to call
  // r5 : feedback vector
  // r6 : (only if r5 is not the megamorphic symbol) slot in feedback
  //      vector (Smi)
  Label slow, non_function_call;

  // Check that the function is not a smi.
  __ JumpIfSmi(r4, &non_function_call);
  // Check that the function is a JSFunction.
  __ CompareObjectType(r4, r7, r7, JS_FUNCTION_TYPE);
  __ bne(&slow);

  if (RecordCallTarget()) {
    GenerateRecordCallTarget(masm);

    __ SmiToPtrArrayOffset(r8, r6);
    __ add(r8, r5, r8);
    if (FLAG_pretenuring_call_new) {
      // Put the AllocationSite from the feedback vector into r5.
      // By adding kPointerSize we encode that we know the AllocationSite
      // entry is at the feedback vector slot given by r6 + 1.
      __ LoadP(r5, FieldMemOperand(r8, FixedArray::kHeaderSize + kPointerSize));
    } else {
      // Put the AllocationSite from the feedback vector into r5, or undefined.
      __ LoadP(r5, FieldMemOperand(r8, FixedArray::kHeaderSize));
      __ LoadP(r8, FieldMemOperand(r5, AllocationSite::kMapOffset));
      __ CompareRoot(r8, Heap::kAllocationSiteMapRootIndex);
      if (CpuFeatures::IsSupported(ISELECT)) {
        __ LoadRoot(r8, Heap::kUndefinedValueRootIndex);
        __ isel(eq, r5, r5, r8);
      } else {
        Label feedback_register_initialized;
        __ beq(&feedback_register_initialized);
        __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
        __ bind(&feedback_register_initialized);
      }
    }

    __ AssertUndefinedOrAllocationSite(r5, r8);
  }

  // Pass function as original constructor.
  if (IsSuperConstructorCall()) {
    __ ShiftLeftImm(r7, r3, Operand(kPointerSizeLog2));
    __ addi(r7, r7, Operand(kPointerSize));
    __ LoadPX(r6, MemOperand(sp, r7));
  } else {
    __ mr(r6, r4);
  }

  // Jump to the function-specific construct stub.
  Register jmp_reg = r7;
  __ LoadP(jmp_reg, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(jmp_reg,
           FieldMemOperand(jmp_reg, SharedFunctionInfo::kConstructStubOffset));
  __ addi(ip, jmp_reg, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);

  // r3: number of arguments
  // r4: called object
  // r7: object type
  Label do_call;
  __ bind(&slow);
  STATIC_ASSERT(JS_FUNCTION_PROXY_TYPE < 0xffffu);
  __ cmpi(r7, Operand(JS_FUNCTION_PROXY_TYPE));
  __ bne(&non_function_call);
  __ GetBuiltinFunction(r4, Builtins::CALL_FUNCTION_PROXY_AS_CONSTRUCTOR);
  __ b(&do_call);

  __ bind(&non_function_call);
  __ GetBuiltinFunction(r4, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ bind(&do_call);
  // Set expected number of arguments to zero (not changing r3).
  __ li(r5, Operand::Zero());
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


static void EmitLoadTypeFeedbackVector(MacroAssembler* masm, Register vector) {
  __ LoadP(vector, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadP(vector,
           FieldMemOperand(vector, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(vector,
           FieldMemOperand(vector, SharedFunctionInfo::kFeedbackVectorOffset));
}


void CallIC_ArrayStub::Generate(MacroAssembler* masm) {
  // r4 - function
  // r6 - slot id
  // r5 - vector
  Label miss;
  int argc = arg_count();
  ParameterCount actual(argc);

  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r7);
  __ cmp(r4, r7);
  __ bne(&miss);

  __ mov(r3, Operand(arg_count()));
  __ SmiToPtrArrayOffset(r9, r6);
  __ add(r9, r5, r9);
  __ LoadP(r7, FieldMemOperand(r9, FixedArray::kHeaderSize));

  // Verify that r7 contains an AllocationSite
  __ LoadP(r8, FieldMemOperand(r7, HeapObject::kMapOffset));
  __ CompareRoot(r8, Heap::kAllocationSiteMapRootIndex);
  __ bne(&miss);

  // Increment the call count for monomorphic function calls.
  const int count_offset = FixedArray::kHeaderSize + kPointerSize;
  __ LoadP(r6, FieldMemOperand(r9, count_offset));
  __ AddSmiLiteral(r6, r6, Smi::FromInt(CallICNexus::kCallCountIncrement), r0);
  __ StoreP(r6, FieldMemOperand(r9, count_offset), r0);

  __ mr(r5, r7);
  __ mr(r6, r4);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);

  __ bind(&miss);
  GenerateMiss(masm);

  // The slow case, we need this no matter what to complete a call after a miss.
  CallFunctionNoFeedback(masm, arg_count(), true, CallAsMethod());

  // Unreachable.
  __ stop("Unexpected code address");
}


void CallICStub::Generate(MacroAssembler* masm) {
  // r4 - function
  // r6 - slot id (Smi)
  // r5 - vector
  const int with_types_offset =
      FixedArray::OffsetOfElementAt(TypeFeedbackVector::kWithTypesIndex);
  const int generic_offset =
      FixedArray::OffsetOfElementAt(TypeFeedbackVector::kGenericCountIndex);
  Label extra_checks_or_miss, slow_start;
  Label slow, non_function, wrap, cont;
  Label have_js_function;
  int argc = arg_count();
  ParameterCount actual(argc);

  // The checks. First, does r4 match the recorded monomorphic target?
  __ SmiToPtrArrayOffset(r9, r6);
  __ add(r9, r5, r9);
  __ LoadP(r7, FieldMemOperand(r9, FixedArray::kHeaderSize));

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

  __ LoadP(r8, FieldMemOperand(r7, WeakCell::kValueOffset));
  __ cmp(r4, r8);
  __ bne(&extra_checks_or_miss);

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(r4, &extra_checks_or_miss);

  // Increment the call count for monomorphic function calls.
  const int count_offset = FixedArray::kHeaderSize + kPointerSize;
  __ LoadP(r6, FieldMemOperand(r9, count_offset));
  __ AddSmiLiteral(r6, r6, Smi::FromInt(CallICNexus::kCallCountIncrement), r0);
  __ StoreP(r6, FieldMemOperand(r9, count_offset), r0);

  __ bind(&have_js_function);
  if (CallAsMethod()) {
    EmitContinueIfStrictOrNative(masm, &cont);
    // Compute the receiver in sloppy mode.
    __ LoadP(r6, MemOperand(sp, argc * kPointerSize), r0);

    __ JumpIfSmi(r6, &wrap);
    __ CompareObjectType(r6, r7, r7, FIRST_SPEC_OBJECT_TYPE);
    __ blt(&wrap);

    __ bind(&cont);
  }

  __ InvokeFunction(r4, actual, JUMP_FUNCTION, NullCallWrapper());

  __ bind(&slow);
  EmitSlowCase(masm, argc, &non_function);

  if (CallAsMethod()) {
    __ bind(&wrap);
    EmitWrapCase(masm, argc, &cont);
  }

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss;

  __ CompareRoot(r7, Heap::kmegamorphic_symbolRootIndex);
  __ beq(&slow_start);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ b(&miss);
  }

  __ CompareRoot(r7, Heap::kuninitialized_symbolRootIndex);
  __ beq(&uninitialized);

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(r7);
  __ CompareObjectType(r7, r8, r8, JS_FUNCTION_TYPE);
  __ bne(&miss);
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ StoreP(ip, FieldMemOperand(r9, FixedArray::kHeaderSize), r0);
  // We have to update statistics for runtime profiling.
  __ LoadP(r7, FieldMemOperand(r5, with_types_offset));
  __ SubSmiLiteral(r7, r7, Smi::FromInt(1), r0);
  __ StoreP(r7, FieldMemOperand(r5, with_types_offset), r0);
  __ LoadP(r7, FieldMemOperand(r5, generic_offset));
  __ AddSmiLiteral(r7, r7, Smi::FromInt(1), r0);
  __ StoreP(r7, FieldMemOperand(r5, generic_offset), r0);
  __ b(&slow_start);

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(r4, &miss);

  // Goto miss case if we do not have a function.
  __ CompareObjectType(r4, r7, r7, JS_FUNCTION_TYPE);
  __ bne(&miss);

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r7);
  __ cmp(r4, r7);
  __ beq(&miss);

  // Update stats.
  __ LoadP(r7, FieldMemOperand(r5, with_types_offset));
  __ AddSmiLiteral(r7, r7, Smi::FromInt(1), r0);
  __ StoreP(r7, FieldMemOperand(r5, with_types_offset), r0);

  // Initialize the call counter.
  __ LoadSmiLiteral(r8, Smi::FromInt(CallICNexus::kCallCountIncrement));
  __ StoreP(r8, FieldMemOperand(r9, count_offset), r0);

  // Store the function. Use a stub since we need a frame for allocation.
  // r5 - vector
  // r6 - slot
  // r4 - function
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(masm->isolate());
    __ Push(r4);
    __ CallStub(&create_stub);
    __ Pop(r4);
  }

  __ b(&have_js_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  // the slow case
  __ bind(&slow_start);
  // Check that the function is really a JavaScript function.
  // r4: pushed function (to be verified)
  __ JumpIfSmi(r4, &non_function);

  // Goto slow case if we do not have a function.
  __ CompareObjectType(r4, r7, r7, JS_FUNCTION_TYPE);
  __ bne(&slow);
  __ b(&have_js_function);
}


void CallICStub::GenerateMiss(MacroAssembler* masm) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

  // Push the function and feedback info.
  __ Push(r4, r5, r6);

  // Call the entry.
  IC::UtilityId id = GetICState() == DEFAULT ? IC::kCallIC_Miss
                                             : IC::kCallIC_Customization_Miss;

  ExternalReference miss = ExternalReference(IC_Utility(id), masm->isolate());
  __ CallExternalReference(miss, 3);

  // Move result to r4 and exit the internal frame.
  __ mr(r4, r3);
}


// StringCharCodeAtGenerator
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ LoadP(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
    __ lbz(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ andi(r0, result_, Operand(kIsNotStringMask));
    __ bne(receiver_not_string_, cr0);
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ LoadP(ip, FieldMemOperand(object_, String::kLengthOffset));
  __ cmpl(ip, index_);
  __ ble(index_out_of_range_);

  __ SmiUntag(index_);

  StringCharLoadGenerator::Generate(masm, object_, index_, result_,
                                    &call_runtime_);

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
  __ CheckMap(index_, result_, Heap::kHeapNumberMapRootIndex, index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Push(LoadWithVectorDescriptor::VectorRegister(),
            LoadWithVectorDescriptor::SlotRegister(), object_, index_);
  } else {
    // index_ is consumed by runtime conversion function.
    __ Push(object_, index_);
  }
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    DCHECK(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.
  __ Move(index_, r3);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Pop(LoadWithVectorDescriptor::VectorRegister(),
           LoadWithVectorDescriptor::SlotRegister(), object_);
  } else {
    __ pop(object_);
  }
  // Reload the instance type.
  __ LoadP(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ lbz(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ b(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ SmiTag(index_);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT, 2);
  __ Move(result_, r3);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  DCHECK(base::bits::IsPowerOfTwo32(String::kMaxOneByteCharCode + 1));
  __ LoadSmiLiteral(r0, Smi::FromInt(~String::kMaxOneByteCharCode));
  __ ori(r0, r0, Operand(kSmiTagMask));
  __ and_(r0, code_, r0);
  __ cmpi(r0, Operand::Zero());
  __ bne(&slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged one-byte char code.
  __ mr(r0, code_);
  __ SmiToPtrArrayOffset(code_, code_);
  __ add(result_, result_, code_);
  __ mr(code_, r0);
  __ LoadP(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ CompareRoot(result_, Heap::kUndefinedValueRootIndex);
  __ beq(&slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  __ Move(result_, r3);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


enum CopyCharactersFlags { COPY_ONE_BYTE = 1, DEST_ALWAYS_ALIGNED = 2 };


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm, Register dest,
                                          Register src, Register count,
                                          Register scratch,
                                          String::Encoding encoding) {
  if (FLAG_debug_code) {
    // Check that destination is word aligned.
    __ andi(r0, dest, Operand(kPointerAlignmentMask));
    __ Check(eq, kDestinationOfCopyNotAligned, cr0);
  }

  // Nothing to do for zero characters.
  Label done;
  if (encoding == String::TWO_BYTE_ENCODING) {
    // double the length
    __ add(count, count, count, LeaveOE, SetRC);
    __ beq(&done, cr0);
  } else {
    __ cmpi(count, Operand::Zero());
    __ beq(&done);
  }

  // Copy count bytes from src to dst.
  Label byte_loop;
  __ mtctr(count);
  __ bind(&byte_loop);
  __ lbz(scratch, MemOperand(src));
  __ addi(src, src, Operand(1));
  __ stb(scratch, MemOperand(dest));
  __ addi(dest, dest, Operand(1));
  __ bdnz(&byte_loop);

  __ bind(&done);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  lr: return address
  //  sp[0]: to
  //  sp[4]: from
  //  sp[8]: string

  // This stub is called from the native-call %_SubString(...), so
  // nothing can be assumed about the arguments. It is tested that:
  //  "string" is a sequential string,
  //  both "from" and "to" are smis, and
  //  0 <= from <= to <= string.length.
  // If any of these assumptions fail, we call the runtime system.

  const int kToOffset = 0 * kPointerSize;
  const int kFromOffset = 1 * kPointerSize;
  const int kStringOffset = 2 * kPointerSize;

  __ LoadP(r5, MemOperand(sp, kToOffset));
  __ LoadP(r6, MemOperand(sp, kFromOffset));

  // If either to or from had the smi tag bit set, then fail to generic runtime
  __ JumpIfNotSmi(r5, &runtime);
  __ JumpIfNotSmi(r6, &runtime);
  __ SmiUntag(r5);
  __ SmiUntag(r6, SetRC);
  // Both r5 and r6 are untagged integers.

  // We want to bailout to runtime here if From is negative.
  __ blt(&runtime, cr0);  // From < 0.

  __ cmpl(r6, r5);
  __ bgt(&runtime);  // Fail if from > to.
  __ sub(r5, r5, r6);

  // Make sure first argument is a string.
  __ LoadP(r3, MemOperand(sp, kStringOffset));
  __ JumpIfSmi(r3, &runtime);
  Condition is_string = masm->IsObjectStringType(r3, r4);
  __ b(NegateCondition(is_string), &runtime, cr0);

  Label single_char;
  __ cmpi(r5, Operand(1));
  __ b(eq, &single_char);

  // Short-cut for the case of trivial substring.
  Label return_r3;
  // r3: original string
  // r5: result string length
  __ LoadP(r7, FieldMemOperand(r3, String::kLengthOffset));
  __ SmiUntag(r0, r7);
  __ cmpl(r5, r0);
  // Return original string.
  __ beq(&return_r3);
  // Longer than original string's length or negative: unsafe arguments.
  __ bgt(&runtime);
  // Shorter than original string's length: an actual substring.

  // Deal with different string types: update the index if necessary
  // and put the underlying string into r8.
  // r3: original string
  // r4: instance type
  // r5: length
  // r6: from index (untagged)
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ andi(r0, r4, Operand(kIsIndirectStringMask));
  __ beq(&seq_or_external_string, cr0);

  __ andi(r0, r4, Operand(kSlicedNotConsMask));
  __ bne(&sliced_string, cr0);
  // Cons string.  Check whether it is flat, then fetch first part.
  __ LoadP(r8, FieldMemOperand(r3, ConsString::kSecondOffset));
  __ CompareRoot(r8, Heap::kempty_stringRootIndex);
  __ bne(&runtime);
  __ LoadP(r8, FieldMemOperand(r3, ConsString::kFirstOffset));
  // Update instance type.
  __ LoadP(r4, FieldMemOperand(r8, HeapObject::kMapOffset));
  __ lbz(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  __ b(&underlying_unpacked);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and correct start index by offset.
  __ LoadP(r8, FieldMemOperand(r3, SlicedString::kParentOffset));
  __ LoadP(r7, FieldMemOperand(r3, SlicedString::kOffsetOffset));
  __ SmiUntag(r4, r7);
  __ add(r6, r6, r4);  // Add offset to index.
  // Update instance type.
  __ LoadP(r4, FieldMemOperand(r8, HeapObject::kMapOffset));
  __ lbz(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  __ b(&underlying_unpacked);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the expected register.
  __ mr(r8, r3);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // r8: underlying subject string
    // r4: instance type of underlying subject string
    // r5: length
    // r6: adjusted start index (untagged)
    __ cmpi(r5, Operand(SlicedString::kMinLength));
    // Short slice.  Copy instead of slicing.
    __ blt(&copy_routine);
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ andi(r0, r4, Operand(kStringEncodingMask));
    __ beq(&two_byte_slice, cr0);
    __ AllocateOneByteSlicedString(r3, r5, r9, r10, &runtime);
    __ b(&set_slice_header);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(r3, r5, r9, r10, &runtime);
    __ bind(&set_slice_header);
    __ SmiTag(r6);
    __ StoreP(r8, FieldMemOperand(r3, SlicedString::kParentOffset), r0);
    __ StoreP(r6, FieldMemOperand(r3, SlicedString::kOffsetOffset), r0);
    __ b(&return_r3);

    __ bind(&copy_routine);
  }

  // r8: underlying subject string
  // r4: instance type of underlying subject string
  // r5: length
  // r6: adjusted start index (untagged)
  Label two_byte_sequential, sequential_string, allocate_result;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ andi(r0, r4, Operand(kExternalStringTag));
  __ beq(&sequential_string, cr0);

  // Handle external string.
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ andi(r0, r4, Operand(kShortExternalStringTag));
  __ bne(&runtime, cr0);
  __ LoadP(r8, FieldMemOperand(r8, ExternalString::kResourceDataOffset));
  // r8 already points to the first character of underlying string.
  __ b(&allocate_result);

  __ bind(&sequential_string);
  // Locate first character of underlying subject string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ addi(r8, r8, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&allocate_result);
  // Sequential acii string.  Allocate the result.
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ andi(r0, r4, Operand(kStringEncodingMask));
  __ beq(&two_byte_sequential, cr0);

  // Allocate and copy the resulting one-byte string.
  __ AllocateOneByteString(r3, r5, r7, r9, r10, &runtime);

  // Locate first character of substring to copy.
  __ add(r8, r8, r6);
  // Locate first character of result.
  __ addi(r4, r3, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  // r3: result string
  // r4: first character of result string
  // r5: result string length
  // r8: first character of substring to copy
  STATIC_ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharacters(masm, r4, r8, r5, r6,
                                       String::ONE_BYTE_ENCODING);
  __ b(&return_r3);

  // Allocate and copy the resulting two-byte string.
  __ bind(&two_byte_sequential);
  __ AllocateTwoByteString(r3, r5, r7, r9, r10, &runtime);

  // Locate first character of substring to copy.
  __ ShiftLeftImm(r4, r6, Operand(1));
  __ add(r8, r8, r4);
  // Locate first character of result.
  __ addi(r4, r3, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // r3: result string.
  // r4: first character of result.
  // r5: result length.
  // r8: first character of substring to copy.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharacters(masm, r4, r8, r5, r6,
                                       String::TWO_BYTE_ENCODING);

  __ bind(&return_r3);
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1, r6, r7);
  __ Drop(3);
  __ Ret();

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubStringRT, 3, 1);

  __ bind(&single_char);
  // r3: original string
  // r4: instance type
  // r5: length
  // r6: from index (untagged)
  __ SmiTag(r6, r6);
  StringCharAtGenerator generator(r3, r6, r5, r3, &runtime, &runtime, &runtime,
                                  STRING_INDEX_IS_NUMBER, RECEIVER_IS_STRING);
  generator.GenerateFast(masm);
  __ Drop(3);
  __ Ret();
  generator.SkipSlow(masm, &runtime);
}


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in r3.
  Label not_smi;
  __ JumpIfNotSmi(r3, &not_smi);
  __ blr();
  __ bind(&not_smi);

  Label not_heap_number;
  __ LoadP(r4, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ lbz(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  // r3: object
  // r4: instance type.
  __ cmpi(r4, Operand(HEAP_NUMBER_TYPE));
  __ bne(&not_heap_number);
  __ blr();
  __ bind(&not_heap_number);

  Label not_string, slow_string;
  __ cmpli(r4, Operand(FIRST_NONSTRING_TYPE));
  __ bge(&not_string);
  // Check if string has a cached array index.
  __ lwz(r5, FieldMemOperand(r3, String::kHashFieldOffset));
  __ And(r0, r5, Operand(String::kContainsCachedArrayIndexMask), SetRC);
  __ bne(&slow_string, cr0);
  __ IndexFromHash(r5, r3);
  __ blr();
  __ bind(&slow_string);
  __ push(r3);  // Push argument.
  __ TailCallRuntime(Runtime::kStringToNumber, 1, 1);
  __ bind(&not_string);

  Label not_oddball;
  __ cmpi(r4, Operand(ODDBALL_TYPE));
  __ bne(&not_oddball);
  __ LoadP(r3, FieldMemOperand(r3, Oddball::kToNumberOffset));
  __ blr();
  __ bind(&not_oddball);

  __ push(r3);  // Push argument.
  __ InvokeBuiltin(Builtins::TO_NUMBER, JUMP_FUNCTION);
}


void StringHelper::GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                                   Register left,
                                                   Register right,
                                                   Register scratch1,
                                                   Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ LoadP(length, FieldMemOperand(left, String::kLengthOffset));
  __ LoadP(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ cmp(length, scratch2);
  __ beq(&check_zero_length);
  __ bind(&strings_not_equal);
  __ LoadSmiLiteral(r3, Smi::FromInt(NOT_EQUAL));
  __ Ret();

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ cmpi(length, Operand::Zero());
  __ bne(&compare_chars);
  __ LoadSmiLiteral(r3, Smi::FromInt(EQUAL));
  __ Ret();

  // Compare characters.
  __ bind(&compare_chars);
  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2,
                                  &strings_not_equal);

  // Characters are equal.
  __ LoadSmiLiteral(r3, Smi::FromInt(EQUAL));
  __ Ret();
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Label result_not_equal, compare_lengths;
  // Find minimum length and length difference.
  __ LoadP(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ LoadP(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ sub(scratch3, scratch1, scratch2, LeaveOE, SetRC);
  Register length_delta = scratch3;
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ isel(gt, scratch1, scratch2, scratch1, cr0);
  } else {
    Label skip;
    __ ble(&skip, cr0);
    __ mr(scratch1, scratch2);
    __ bind(&skip);
  }
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ cmpi(min_length, Operand::Zero());
  __ beq(&compare_lengths);

  // Compare loop.
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ bind(&compare_lengths);
  DCHECK(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use length_delta as result if it's zero.
  __ mr(r3, length_delta);
  __ cmpi(r3, Operand::Zero());
  __ bind(&result_not_equal);
  // Conditionally update the result based either on length_delta or
  // the last comparion performed in the loop above.
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ LoadSmiLiteral(r4, Smi::FromInt(GREATER));
    __ LoadSmiLiteral(r5, Smi::FromInt(LESS));
    __ isel(eq, r3, r0, r4);
    __ isel(lt, r3, r5, r3);
    __ Ret();
  } else {
    Label less_equal, equal;
    __ ble(&less_equal);
    __ LoadSmiLiteral(r3, Smi::FromInt(GREATER));
    __ Ret();
    __ bind(&less_equal);
    __ beq(&equal);
    __ LoadSmiLiteral(r3, Smi::FromInt(LESS));
    __ bind(&equal);
    __ Ret();
  }
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch1, Label* chars_not_equal) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ addi(scratch1, length,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ add(left, left, scratch1);
  __ add(right, right, scratch1);
  __ subfic(length, length, Operand::Zero());
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ lbzx(scratch1, MemOperand(left, index));
  __ lbzx(r0, MemOperand(right, index));
  __ cmp(scratch1, r0);
  __ bne(chars_not_equal);
  __ addi(index, index, Operand(1));
  __ cmpi(index, Operand::Zero());
  __ bne(&loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  Counters* counters = isolate()->counters();

  // Stack frame on entry.
  //  sp[0]: right string
  //  sp[4]: left string
  __ LoadP(r3, MemOperand(sp));  // Load right in r3, left in r4.
  __ LoadP(r4, MemOperand(sp, kPointerSize));

  Label not_same;
  __ cmp(r3, r4);
  __ bne(&not_same);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r3, Smi::FromInt(EQUAL));
  __ IncrementCounter(counters->string_compare_native(), 1, r4, r5);
  __ addi(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential one-byte strings.
  __ JumpIfNotBothSequentialOneByteStrings(r4, r3, r5, r6, &runtime);

  // Compare flat one-byte strings natively. Remove arguments from stack first.
  __ IncrementCounter(counters->string_compare_native(), 1, r5, r6);
  __ addi(sp, sp, Operand(2 * kPointerSize));
  StringHelper::GenerateCompareFlatOneByteStrings(masm, r4, r3, r5, r6, r7);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompareRT, 2, 1);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4    : left
  //  -- r3    : right
  //  -- lr    : return address
  // -----------------------------------

  // Load r5 with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ Move(r5, handle(isolate()->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ TestIfSmi(r5, r0);
    __ Assert(ne, kExpectedAllocationSite, cr0);
    __ push(r5);
    __ LoadP(r5, FieldMemOperand(r5, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kAllocationSiteMapRootIndex);
    __ cmp(r5, ip);
    __ pop(r5);
    __ Assert(eq, kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state());
  __ TailCallStub(&stub);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ orx(r5, r4, r3);
  __ JumpIfNotSmi(r5, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    // __ sub(r3, r3, r4, SetCC);
    __ sub(r3, r3, r4);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(r4);
    __ SmiUntag(r3);
    __ sub(r3, r4, r3);
  }
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateNumbers(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::NUMBER);

  Label generic_stub;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;
  Label equal, less_than;

  if (left() == CompareICState::SMI) {
    __ JumpIfNotSmi(r4, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(r3, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved.
  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(r3, &right_smi);
  __ CheckMap(r3, r5, Heap::kHeapNumberMapRootIndex, &maybe_undefined1,
              DONT_DO_SMI_CHECK);
  __ lfd(d1, FieldMemOperand(r3, HeapNumber::kValueOffset));
  __ b(&left);
  __ bind(&right_smi);
  __ SmiToDouble(d1, r3);

  __ bind(&left);
  __ JumpIfSmi(r4, &left_smi);
  __ CheckMap(r4, r5, Heap::kHeapNumberMapRootIndex, &maybe_undefined2,
              DONT_DO_SMI_CHECK);
  __ lfd(d0, FieldMemOperand(r4, HeapNumber::kValueOffset));
  __ b(&done);
  __ bind(&left_smi);
  __ SmiToDouble(d0, r4);

  __ bind(&done);

  // Compare operands
  __ fcmpu(d0, d1);

  // Don't base result on status bits when a NaN is involved.
  __ bunordered(&unordered);

  // Return a result of -1, 0, or 1, based on status bits.
  if (CpuFeatures::IsSupported(ISELECT)) {
    DCHECK(EQUAL == 0);
    __ li(r4, Operand(GREATER));
    __ li(r5, Operand(LESS));
    __ isel(eq, r3, r0, r4);
    __ isel(lt, r3, r5, r3);
    __ Ret();
  } else {
    __ beq(&equal);
    __ blt(&less_than);
    //  assume greater than
    __ li(r3, Operand(GREATER));
    __ Ret();
    __ bind(&equal);
    __ li(r3, Operand(EQUAL));
    __ Ret();
    __ bind(&less_than);
    __ li(r3, Operand(LESS));
    __ Ret();
  }

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), strength(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
    __ bne(&miss);
    __ JumpIfSmi(r4, &unordered);
    __ CompareObjectType(r4, r5, r5, HEAP_NUMBER_TYPE);
    __ bne(&maybe_undefined2);
    __ b(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ CompareRoot(r4, Heap::kUndefinedValueRootIndex);
    __ beq(&unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  Label miss, not_equal;

  // Registers containing left and right operands respectively.
  Register left = r4;
  Register right = r3;
  Register tmp1 = r5;
  Register tmp2 = r6;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are symbols.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbz(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbz(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ orx(tmp1, tmp1, tmp2);
  __ andi(r0, tmp1, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ bne(&miss, cr0);

  // Internalized strings are compared by identity.
  __ cmp(left, right);
  __ bne(&not_equal);
  // Make sure r3 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r3));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r3, Smi::FromInt(EQUAL));
  __ bind(&not_equal);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  DCHECK(GetCondition() == eq);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = r4;
  Register right = r3;
  Register tmp1 = r5;
  Register tmp2 = r6;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbz(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbz(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss);

  // Unique names are compared by identity.
  __ cmp(left, right);
  __ bne(&miss);
  // Make sure r3 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r3));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r3, Smi::FromInt(EQUAL));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  Label miss, not_identical, is_symbol;

  bool equality = Token::IsEqualityOp(op());

  // Registers containing left and right operands respectively.
  Register left = r4;
  Register right = r3;
  Register tmp1 = r5;
  Register tmp2 = r6;
  Register tmp3 = r7;
  Register tmp4 = r8;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbz(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbz(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ orx(tmp3, tmp1, tmp2);
  __ andi(r0, tmp3, Operand(kIsNotStringMask));
  __ bne(&miss, cr0);

  // Fast check for identical strings.
  __ cmp(left, right);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ bne(&not_identical);
  __ LoadSmiLiteral(r3, Smi::FromInt(EQUAL));
  __ Ret();
  __ bind(&not_identical);

  // Handle not identical strings.

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We know they are both
  // strings.
  if (equality) {
    DCHECK(GetCondition() == eq);
    STATIC_ASSERT(kInternalizedTag == 0);
    __ orx(tmp3, tmp1, tmp2);
    __ andi(r0, tmp3, Operand(kIsNotInternalizedMask));
    __ bne(&is_symbol, cr0);
    // Make sure r3 is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(r3));
    __ Ret();
    __ bind(&is_symbol);
  }

  // Check that both strings are sequential one-byte.
  Label runtime;
  __ JumpIfBothInstanceTypesAreNotSequentialOneByte(tmp1, tmp2, tmp3, tmp4,
                                                    &runtime);

  // Compare flat one-byte strings. Returns when done.
  if (equality) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, left, right, tmp1,
                                                  tmp2);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, left, right, tmp1,
                                                    tmp2, tmp3);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ Push(left, right);
  if (equality) {
    __ TailCallRuntime(Runtime::kStringEquals, 2, 1);
  } else {
    __ TailCallRuntime(Runtime::kStringCompareRT, 2, 1);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateObjects(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::OBJECT);
  Label miss;
  __ and_(r5, r4, r3);
  __ JumpIfSmi(r5, &miss);

  __ CompareObjectType(r3, r5, r5, JS_OBJECT_TYPE);
  __ bne(&miss);
  __ CompareObjectType(r4, r5, r5, JS_OBJECT_TYPE);
  __ bne(&miss);

  DCHECK(GetCondition() == eq);
  __ sub(r3, r3, r4);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownObjects(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ and_(r5, r4, r3);
  __ JumpIfSmi(r5, &miss);
  __ GetWeakValue(r7, cell);
  __ LoadP(r5, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadP(r6, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ cmp(r5, r7);
  __ bne(&miss);
  __ cmp(r6, r7);
  __ bne(&miss);

  __ sub(r3, r3, r4);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    ExternalReference miss =
        ExternalReference(IC_Utility(IC::kCompareIC_Miss), isolate());

    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r3);
    __ Push(r4, r3);
    __ LoadSmiLiteral(r0, Smi::FromInt(op()));
    __ push(r0);
    __ CallExternalReference(miss, 3);
    // Compute the entry point of the rewritten stub.
    __ addi(r5, r3, Operand(Code::kHeaderSize - kHeapObjectTag));
    // Restore registers.
    __ Pop(r4, r3);
  }

  __ JumpToJSEntry(r5);
}


// This stub is paired with DirectCEntryStub::GenerateCall
void DirectCEntryStub::Generate(MacroAssembler* masm) {
  // Place the return address on the stack, making the call
  // GC safe. The RegExp backend also relies on this.
  __ mflr(r0);
  __ StoreP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  __ Call(ip);  // Call the C++ function.
  __ LoadP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  __ mtlr(r0);
  __ blr();
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm, Register target) {
#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // Native AIX/PPC64 Linux use a function descriptor.
  __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(target, kPointerSize));
  __ LoadP(ip, MemOperand(target, 0));  // Instruction address
#else
  // ip needs to be set for DirectCEentryStub::Generate, and also
  // for ABI_TOC_ADDRESSABILITY_VIA_IP.
  __ Move(ip, target);
#endif

  intptr_t code = reinterpret_cast<intptr_t>(GetCode().location());
  __ mov(r0, Operand(code, RelocInfo::CODE_TARGET));
  __ Call(r0);  // Call the stub.
}


void NameDictionaryLookupStub::GenerateNegativeLookup(
    MacroAssembler* masm, Label* miss, Label* done, Register receiver,
    Register properties, Handle<Name> name, Register scratch0) {
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
    __ LoadP(index, FieldMemOperand(properties, kCapacityOffset));
    __ subi(index, index, Operand(1));
    __ LoadSmiLiteral(
        ip, Smi::FromInt(name->Hash() + NameDictionary::GetProbeOffset(i)));
    __ and_(index, index, ip);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ ShiftLeftImm(ip, index, Operand(1));
    __ add(index, index, ip);  // index *= 3.

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    Register tmp = properties;
    __ SmiToPtrArrayOffset(ip, index);
    __ add(tmp, properties, ip);
    __ LoadP(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    DCHECK(!tmp.is(entity_name));
    __ LoadRoot(tmp, Heap::kUndefinedValueRootIndex);
    __ cmp(entity_name, tmp);
    __ beq(done);

    // Load the hole ready for use below:
    __ LoadRoot(tmp, Heap::kTheHoleValueRootIndex);

    // Stop if found the property.
    __ Cmpi(entity_name, Operand(Handle<Name>(name)), r0);
    __ beq(miss);

    Label good;
    __ cmp(entity_name, tmp);
    __ beq(&good);

    // Check if the entry name is not a unique name.
    __ LoadP(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ lbz(entity_name, FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ LoadP(properties,
             FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask = (r0.bit() | r9.bit() | r8.bit() | r7.bit() | r6.bit() |
                          r5.bit() | r4.bit() | r3.bit());

  __ mflr(r0);
  __ MultiPush(spill_mask);

  __ LoadP(r3, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ mov(r4, Operand(Handle<Name>(name)));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  __ cmpi(r3, Operand::Zero());

  __ MultiPop(spill_mask);  // MultiPop does not touch condition flags
  __ mtlr(r0);

  __ beq(done);
  __ bne(miss);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found. Jump to
// the |miss| label otherwise.
// If lookup was successful |scratch2| will be equal to elements + 4 * index.
void NameDictionaryLookupStub::GeneratePositiveLookup(
    MacroAssembler* masm, Label* miss, Label* done, Register elements,
    Register name, Register scratch1, Register scratch2) {
  DCHECK(!elements.is(scratch1));
  DCHECK(!elements.is(scratch2));
  DCHECK(!name.is(scratch1));
  DCHECK(!name.is(scratch2));

  __ AssertName(name);

  // Compute the capacity mask.
  __ LoadP(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ SmiUntag(scratch1);  // convert smi to int
  __ subi(scratch1, scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ lwz(scratch2, FieldMemOperand(name, Name::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ addi(scratch2, scratch2,
              Operand(NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ srwi(scratch2, scratch2, Operand(Name::kHashShift));
    __ and_(scratch2, scratch1, scratch2);

    // Scale the index by multiplying by the element size.
    DCHECK(NameDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.
    __ ShiftLeftImm(ip, scratch2, Operand(1));
    __ add(scratch2, scratch2, ip);

    // Check if the key is identical to the name.
    __ ShiftLeftImm(ip, scratch2, Operand(kPointerSizeLog2));
    __ add(scratch2, elements, ip);
    __ LoadP(ip, FieldMemOperand(scratch2, kElementsStartOffset));
    __ cmp(name, ip);
    __ beq(done);
  }

  const int spill_mask = (r0.bit() | r9.bit() | r8.bit() | r7.bit() | r6.bit() |
                          r5.bit() | r4.bit() | r3.bit()) &
                         ~(scratch1.bit() | scratch2.bit());

  __ mflr(r0);
  __ MultiPush(spill_mask);
  if (name.is(r3)) {
    DCHECK(!elements.is(r4));
    __ mr(r4, name);
    __ mr(r3, elements);
  } else {
    __ mr(r3, elements);
    __ mr(r4, name);
  }
  NameDictionaryLookupStub stub(masm->isolate(), POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ cmpi(r3, Operand::Zero());
  __ mr(scratch2, r5);
  __ MultiPop(spill_mask);
  __ mtlr(r0);

  __ bne(done);
  __ beq(miss);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Registers:
  //  result: NameDictionary to probe
  //  r4: key
  //  dictionary: NameDictionary to probe.
  //  index: will hold an index of entry if lookup is successful.
  //         might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Register result = r3;
  Register dictionary = r3;
  Register key = r4;
  Register index = r5;
  Register mask = r6;
  Register hash = r7;
  Register undefined = r8;
  Register entry_key = r9;
  Register scratch = r9;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ LoadP(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ SmiUntag(mask);
  __ subi(mask, mask, Operand(1));

  __ lwz(hash, FieldMemOperand(key, Name::kHashFieldOffset));

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
      __ addi(index, hash,
              Operand(NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    } else {
      __ mr(index, hash);
    }
    __ srwi(r0, index, Operand(Name::kHashShift));
    __ and_(index, mask, r0);

    // Scale the index by multiplying by the entry size.
    DCHECK(NameDictionary::kEntrySize == 3);
    __ ShiftLeftImm(scratch, index, Operand(1));
    __ add(index, index, scratch);  // index *= 3.

    __ ShiftLeftImm(scratch, index, Operand(kPointerSizeLog2));
    __ add(index, dictionary, scratch);
    __ LoadP(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ cmp(entry_key, undefined);
    __ beq(&not_in_dictionary);

    // Stop if found the property.
    __ cmp(entry_key, key);
    __ beq(&in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ LoadP(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ lbz(entry_key, FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(entry_key, &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ li(result, Operand::Zero());
    __ Ret();
  }

  __ bind(&in_dictionary);
  __ li(result, Operand(1));
  __ Ret();

  __ bind(&not_in_dictionary);
  __ li(result, Operand::Zero());
  __ Ret();
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub1(isolate, kDontSaveFPRegs);
  stub1.GetCode();
  // Hydrogen code stubs need stub2 at snapshot time.
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

  // The first two branch instructions are generated with labels so as to
  // get the offset fixed up correctly by the bind(Label*) call.  We patch
  // it back and forth between branch condition True and False
  // when we start and stop incremental heap marking.
  // See RecordWriteStub::Patch for details.

  // Clear the bit, branch on True for NOP action initially
  __ crclr(Assembler::encode_crbit(cr2, CR_LT));
  __ blt(&skip_to_incremental_noncompacting, cr2);
  __ blt(&skip_to_incremental_compacting, cr2);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  }
  __ Ret();

  __ bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);

  // Initial mode of the stub is expected to be STORE_BUFFER_ONLY.
  // Will be checked in IncrementalMarking::ActivateGeneratedStub.
  // patching not required on PPC as the initial path is effectively NOP
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ LoadP(regs_.scratch0(), MemOperand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),  // Value.
                           regs_.scratch0(), &dont_need_remembered_set);

    __ CheckPageFlag(regs_.object(), regs_.scratch0(),
                     1 << MemoryChunk::SCAN_ON_SCAVENGE, ne,
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
  __ Ret();
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode());
  int argument_count = 3;
  __ PrepareCallCFunction(argument_count, regs_.scratch0());
  Register address =
      r3.is(regs_.address()) ? regs_.scratch0() : regs_.address();
  DCHECK(!address.is(regs_.object()));
  DCHECK(!address.is(r3));
  __ mr(address, regs_.address());
  __ mr(r3, regs_.object());
  __ mr(r4, address);
  __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));

  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(
      ExternalReference::incremental_marking_record_write_function(isolate()),
      argument_count);
  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode());
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm, OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_scratch;

  DCHECK((~Page::kPageAlignmentMask & 0xffff) == 0);
  __ lis(r0, Operand((~Page::kPageAlignmentMask >> 16)));
  __ and_(regs_.scratch0(), regs_.object(), r0);
  __ LoadP(
      regs_.scratch1(),
      MemOperand(regs_.scratch0(), MemoryChunk::kWriteBarrierCounterOffset));
  __ subi(regs_.scratch1(), regs_.scratch1(), Operand(1));
  __ StoreP(
      regs_.scratch1(),
      MemOperand(regs_.scratch0(), MemoryChunk::kWriteBarrierCounterOffset));
  __ cmpi(regs_.scratch1(), Operand::Zero());  // PPC, we could do better here
  __ blt(&need_incremental);

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(), regs_.scratch0(), regs_.scratch1(), &on_black);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ bind(&on_black);

  // Get the value from the slot.
  __ LoadP(regs_.scratch0(), MemOperand(regs_.address(), 0));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlag(regs_.scratch0(),  // Contains value.
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kEvacuationCandidateMask, eq,
                     &ensure_not_white);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kSkipEvacuationSlotsRecordingMask, eq,
                     &need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need extra registers for this, so we push the object and the address
  // register temporarily.
  __ Push(regs_.object(), regs_.address());
  __ EnsureNotWhite(regs_.scratch0(),  // The value.
                    regs_.scratch1(),  // Scratch.
                    regs_.object(),    // Scratch.
                    regs_.address(),   // Scratch.
                    &need_incremental_pop_scratch);
  __ Pop(regs_.object(), regs_.address());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ bind(&need_incremental_pop_scratch);
  __ Pop(regs_.object(), regs_.address());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StoreArrayLiteralElementStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3    : element value to store
  //  -- r6    : element index as smi
  //  -- sp[0] : array literal index in function as smi
  //  -- sp[4] : array literal
  // clobbers r3, r5, r7
  // -----------------------------------

  Label element_done;
  Label double_elements;
  Label smi_element;
  Label slow_elements;
  Label fast_elements;

  // Get array literal index, array literal and its map.
  __ LoadP(r7, MemOperand(sp, 0 * kPointerSize));
  __ LoadP(r4, MemOperand(sp, 1 * kPointerSize));
  __ LoadP(r5, FieldMemOperand(r4, JSObject::kMapOffset));

  __ CheckFastElements(r5, r8, &double_elements);
  // FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS
  __ JumpIfSmi(r3, &smi_element);
  __ CheckFastSmiElements(r5, r8, &fast_elements);

  // Store into the array literal requires a elements transition. Call into
  // the runtime.
  __ bind(&slow_elements);
  // call.
  __ Push(r4, r6, r3);
  __ LoadP(r8, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadP(r8, FieldMemOperand(r8, JSFunction::kLiteralsOffset));
  __ Push(r8, r7);
  __ TailCallRuntime(Runtime::kStoreArrayLiteralElement, 5, 1);

  // Array literal has ElementsKind of FAST_*_ELEMENTS and value is an object.
  __ bind(&fast_elements);
  __ LoadP(r8, FieldMemOperand(r4, JSObject::kElementsOffset));
  __ SmiToPtrArrayOffset(r9, r6);
  __ add(r9, r8, r9);
#if V8_TARGET_ARCH_PPC64
  // add due to offset alignment requirements of StorePU
  __ addi(r9, r9, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ StoreP(r3, MemOperand(r9));
#else
  __ StorePU(r3, MemOperand(r9, FixedArray::kHeaderSize - kHeapObjectTag));
#endif
  // Update the write barrier for the array store.
  __ RecordWrite(r8, r9, r3, kLRHasNotBeenSaved, kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Ret();

  // Array literal has ElementsKind of FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS,
  // and value is Smi.
  __ bind(&smi_element);
  __ LoadP(r8, FieldMemOperand(r4, JSObject::kElementsOffset));
  __ SmiToPtrArrayOffset(r9, r6);
  __ add(r9, r8, r9);
  __ StoreP(r3, FieldMemOperand(r9, FixedArray::kHeaderSize), r0);
  __ Ret();

  // Array literal has ElementsKind of FAST_DOUBLE_ELEMENTS.
  __ bind(&double_elements);
  __ LoadP(r8, FieldMemOperand(r4, JSObject::kElementsOffset));
  __ StoreNumberToDoubleElements(r3, r6, r8, r9, d0, &slow_elements);
  __ Ret();
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ Call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ LoadP(r4, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ addi(r4, r4, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ slwi(r4, r4, Operand(kPointerSizeLog2));
  __ add(sp, sp, r4);
  __ Ret();
}


void LoadICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, LoadWithVectorDescriptor::VectorRegister());
  LoadICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


void KeyedLoadICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, LoadWithVectorDescriptor::VectorRegister());
  KeyedLoadICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


void CallICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, r5);
  CallICStub stub(isolate(), state());
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void CallIC_ArrayTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, r5);
  CallIC_ArrayStub stub(isolate(), state());
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void LoadICStub::Generate(MacroAssembler* masm) { GenerateImpl(masm, false); }


void LoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


static void HandleArrayCases(MacroAssembler* masm, Register receiver,
                             Register key, Register vector, Register slot,
                             Register feedback, Register receiver_map,
                             Register scratch1, Register scratch2,
                             bool is_polymorphic, Label* miss) {
  // feedback initially contains the feedback array
  Label next_loop, prepare_next;
  Label start_polymorphic;

  Register cached_map = scratch1;

  __ LoadP(cached_map,
           FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(0)));
  __ LoadP(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ cmp(receiver_map, cached_map);
  __ bne(&start_polymorphic);
  // found, now call handler.
  Register handler = feedback;
  __ LoadP(handler,
           FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ addi(ip, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);


  Register length = scratch2;
  __ bind(&start_polymorphic);
  __ LoadP(length, FieldMemOperand(feedback, FixedArray::kLengthOffset));
  if (!is_polymorphic) {
    // If the IC could be monomorphic we have to make sure we don't go past the
    // end of the feedback array.
    __ CmpSmiLiteral(length, Smi::FromInt(2), r0);
    __ beq(miss);
  }

  Register too_far = length;
  Register pointer_reg = feedback;

  // +-----+------+------+-----+-----+ ... ----+
  // | map | len  | wm0  | h0  | wm1 |      hN |
  // +-----+------+------+-----+-----+ ... ----+
  //                 0      1     2        len-1
  //                              ^              ^
  //                              |              |
  //                         pointer_reg      too_far
  //                         aka feedback     scratch2
  // also need receiver_map
  // use cached_map (scratch1) to look in the weak map values.
  __ SmiToPtrArrayOffset(r0, length);
  __ add(too_far, feedback, r0);
  __ addi(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ addi(pointer_reg, feedback,
          Operand(FixedArray::OffsetOfElementAt(2) - kHeapObjectTag));

  __ bind(&next_loop);
  __ LoadP(cached_map, MemOperand(pointer_reg));
  __ LoadP(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ cmp(receiver_map, cached_map);
  __ bne(&prepare_next);
  __ LoadP(handler, MemOperand(pointer_reg, kPointerSize));
  __ addi(ip, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);

  __ bind(&prepare_next);
  __ addi(pointer_reg, pointer_reg, Operand(kPointerSize * 2));
  __ cmp(pointer_reg, too_far);
  __ blt(&next_loop);

  // We exhausted our array of map handler pairs.
  __ b(miss);
}


static void HandleMonomorphicCase(MacroAssembler* masm, Register receiver,
                                  Register receiver_map, Register feedback,
                                  Register vector, Register slot,
                                  Register scratch, Label* compare_map,
                                  Label* load_smi_map, Label* try_array) {
  __ JumpIfSmi(receiver, load_smi_map);
  __ LoadP(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ bind(compare_map);
  Register cached_map = scratch;
  // Move the weak map into the weak_cell register.
  __ LoadP(cached_map, FieldMemOperand(feedback, WeakCell::kValueOffset));
  __ cmp(cached_map, receiver_map);
  __ bne(try_array);
  Register handler = feedback;
  __ SmiToPtrArrayOffset(r0, slot);
  __ add(handler, vector, r0);
  __ LoadP(handler,
           FieldMemOperand(handler, FixedArray::kHeaderSize + kPointerSize));
  __ addi(ip, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);
}


void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // r4
  Register name = LoadWithVectorDescriptor::NameRegister();          // r5
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // r6
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // r3
  Register feedback = r7;
  Register receiver_map = r8;
  Register scratch1 = r9;

  __ SmiToPtrArrayOffset(r0, slot);
  __ add(feedback, vector, r0);
  __ LoadP(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ LoadP(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  __ bne(&not_array);
  HandleArrayCases(masm, receiver, name, vector, slot, feedback, receiver_map,
                   scratch1, r10, true, &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ bne(&miss);
  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::LOAD_IC, code_flags,
                                               false, receiver, name, feedback,
                                               receiver_map, scratch1, r10);

  __ bind(&miss);
  LoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ b(&compare_map);
}


void KeyedLoadICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void KeyedLoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void KeyedLoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // r4
  Register key = LoadWithVectorDescriptor::NameRegister();           // r5
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // r6
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // r3
  Register feedback = r7;
  Register receiver_map = r8;
  Register scratch1 = r9;

  __ SmiToPtrArrayOffset(r0, slot);
  __ add(feedback, vector, r0);
  __ LoadP(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ LoadP(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  __ bne(&not_array);

  // We have a polymorphic element handler.
  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);
  HandleArrayCases(masm, receiver, key, vector, slot, feedback, receiver_map,
                   scratch1, r10, true, &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ bne(&try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmp(key, feedback);
  __ bne(&miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ SmiToPtrArrayOffset(r0, slot);
  __ add(feedback, vector, r0);
  __ LoadP(feedback,
           FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, receiver, key, vector, slot, feedback, receiver_map,
                   scratch1, r10, false, &miss);

  __ bind(&miss);
  KeyedLoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ b(&compare_map);
}


void VectorStoreICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, VectorStoreICDescriptor::VectorRegister());
  VectorStoreICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}


void VectorKeyedStoreICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, VectorStoreICDescriptor::VectorRegister());
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
  Label miss;

  // TODO(mvstanton): Implement.
  __ bind(&miss);
  StoreIC::GenerateMiss(masm);
}


void VectorKeyedStoreICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void VectorKeyedStoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


void VectorKeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Label miss;

  // TODO(mvstanton): Implement.
  __ bind(&miss);
  KeyedStoreIC::GenerateMiss(masm);
}


void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    PredictableCodeSizeScope predictable(masm,
#if V8_TARGET_ARCH_PPC64
                                         14 * Assembler::kInstrSize);
#else
                                         11 * Assembler::kInstrSize);
#endif
    ProfileEntryHookStub stub(masm->isolate());
    __ mflr(r0);
    __ Push(r0, ip);
    __ CallStub(&stub);
    __ Pop(r0, ip);
    __ mtlr(r0);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // The entry hook is a "push lr, ip" instruction, followed by a call.
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 3 * Assembler::kInstrSize;

  // This should contain all kJSCallerSaved registers.
  const RegList kSavedRegs = kJSCallerSaved |  // Caller saved registers.
                             r15.bit();        // Saved stack pointer.

  // We also save lr, so the count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = kNumJSCallerSaved + 2;

  // Save all caller-save registers as this may be called from anywhere.
  __ mflr(ip);
  __ MultiPush(kSavedRegs | ip.bit());

  // Compute the function's address for the first argument.
  __ subi(r3, ip, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is two slots above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ addi(r4, sp, Operand((kNumSavedRegs + 1) * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ mr(r15, sp);
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
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
#elif ABI_TOC_ADDRESSABILITY_VIA_IP
// ip set above, so nothing to do.
#endif

  // PPC LINUX ABI:
  __ li(r0, Operand::Zero());
  __ StorePU(r0, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));
#else
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  // It additionally takes an isolate as a third parameter
  __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));

  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ mov(ip, Operand(ExternalReference(
                 &dispatcher, ExternalReference::BUILTIN_CALL, isolate())));
#endif
  __ Call(ip);

#if !defined(USE_SIMULATOR)
  __ addi(sp, sp, Operand(kNumRequiredStackFrameSlots * kPointerSize));
#endif

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ mr(sp, r15);
  }

  // Also pop lr to get Ret(0).
  __ MultiPop(kSavedRegs | ip.bit());
  __ mtlr(ip);
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
      __ Cmpi(r6, Operand(kind), r0);
      T stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // r5 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // r6 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // r3 - number of arguments
  // r4 - constructor?
  // sp[0] - last argument
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    DCHECK(FAST_SMI_ELEMENTS == 0);
    DCHECK(FAST_HOLEY_SMI_ELEMENTS == 1);
    DCHECK(FAST_ELEMENTS == 2);
    DCHECK(FAST_HOLEY_ELEMENTS == 3);
    DCHECK(FAST_DOUBLE_ELEMENTS == 4);
    DCHECK(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ andi(r0, r6, Operand(1));
    __ bne(&normal_sequence, cr0);
  }

  // look at the first argument
  __ LoadP(r8, MemOperand(sp, 0));
  __ cmpi(r8, Operand::Zero());
  __ beq(&normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(
        masm->isolate(), holey_initial, DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(masm->isolate(), initial,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the slot).
    __ addi(r6, r6, Operand(1));

    if (FLAG_debug_code) {
      __ LoadP(r8, FieldMemOperand(r5, 0));
      __ CompareRoot(r8, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r6
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ LoadP(r7, FieldMemOperand(r5, AllocationSite::kTransitionInfoOffset));
    __ AddSmiLiteral(r7, r7, Smi::FromInt(kFastElementsKindPackedToHoley), r0);
    __ StoreP(r7, FieldMemOperand(r5, AllocationSite::kTransitionInfoOffset),
              r0);

    __ bind(&normal_sequence);
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ mov(r0, Operand(kind));
      __ cmp(r6, r0);
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
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
  ElementsKind kinds[2] = {FAST_ELEMENTS, FAST_HOLEY_ELEMENTS};
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
    MacroAssembler* masm, AllocationSiteOverrideMode mode) {
  if (argument_count() == ANY) {
    Label not_zero_case, not_one_case;
    __ cmpi(r3, Operand::Zero());
    __ bne(&not_zero_case);
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ cmpi(r3, Operand(1));
    __ bgt(&not_one_case);
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
  //  -- r3 : argc (only if argument_count() == ANY)
  //  -- r4 : constructor
  //  -- r5 : AllocationSite or undefined
  //  -- r6 : original constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r7, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ TestIfSmi(r7, r0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r7, r7, r8, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in r5 or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(r5, r7);
  }

  Label subclassing;
  __ cmp(r6, r4);
  __ bne(&subclassing);

  Label no_info;
  // Get the elements kind and case on that.
  __ CompareRoot(r5, Heap::kUndefinedValueRootIndex);
  __ beq(&no_info);

  __ LoadP(r6, FieldMemOperand(r5, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(r6);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ And(r6, r6, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  __ bind(&subclassing);
  __ push(r4);
  __ push(r6);

  // Adjust argc.
  switch (argument_count()) {
    case ANY:
    case MORE_THAN_ONE:
      __ addi(r3, r3, Operand(2));
      break;
    case NONE:
      __ li(r3, Operand(2));
      break;
    case ONE:
      __ li(r3, Operand(3));
      break;
  }

  __ JumpToExternalReference(
      ExternalReference(Runtime::kArrayConstructorWithSubclassing, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(MacroAssembler* masm,
                                                ElementsKind kind) {
  __ cmpli(r3, Operand(1));

  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0, lt);

  InternalArrayNArgumentsConstructorStub stubN(isolate(), kind);
  __ TailCallStub(&stubN, gt);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ LoadP(r6, MemOperand(sp, 0));
    __ cmpi(r6, Operand::Zero());

    InternalArraySingleArgumentConstructorStub stub1_holey(
        isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey, ne);
  }

  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argc
  //  -- r4 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r6, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ TestIfSmi(r6, r0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r6, r6, r7, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ LoadP(r6, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the map's "bit field 2" into |result|.
  __ lbz(r6, FieldMemOperand(r6, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(r6);

  if (FLAG_debug_code) {
    Label done;
    __ cmpi(r6, Operand(FAST_ELEMENTS));
    __ beq(&done);
    __ cmpi(r6, Operand(FAST_HOLEY_ELEMENTS));
    __ Assert(eq, kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmpi(r6, Operand(FAST_ELEMENTS));
  __ beq(&fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
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
                                     MemOperand return_value_operand,
                                     MemOperand* context_restore_operand) {
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  // Additional parameter is the address of the actual callback.
  DCHECK(function_address.is(r4) || function_address.is(r5));
  Register scratch = r6;

  __ mov(scratch, Operand(ExternalReference::is_profiling_address(isolate)));
  __ lbz(scratch, MemOperand(scratch, 0));
  __ cmpi(scratch, Operand::Zero());

  if (CpuFeatures::IsSupported(ISELECT)) {
    __ mov(scratch, Operand(thunk_ref));
    __ isel(eq, scratch, function_address, scratch);
  } else {
    Label profiler_disabled;
    Label end_profiler_check;
    __ beq(&profiler_disabled);
    __ mov(scratch, Operand(thunk_ref));
    __ b(&end_profiler_check);
    __ bind(&profiler_disabled);
    __ mr(scratch, function_address);
    __ bind(&end_profiler_check);
  }

  // Allocate HandleScope in callee-save registers.
  // r17 - next_address
  // r14 - next_address->kNextOffset
  // r15 - next_address->kLimitOffset
  // r16 - next_address->kLevelOffset
  __ mov(r17, Operand(next_address));
  __ LoadP(r14, MemOperand(r17, kNextOffset));
  __ LoadP(r15, MemOperand(r17, kLimitOffset));
  __ lwz(r16, MemOperand(r17, kLevelOffset));
  __ addi(r16, r16, Operand(1));
  __ stw(r16, MemOperand(r17, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, r3);
    __ mov(r3, Operand(ExternalReference::isolate_address(isolate)));
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
    __ PrepareCallCFunction(1, r3);
    __ mov(r3, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  __ LoadP(r3, return_value_operand);
  __ bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ StoreP(r14, MemOperand(r17, kNextOffset));
  if (__ emit_debug_code()) {
    __ lwz(r4, MemOperand(r17, kLevelOffset));
    __ cmp(r4, r16);
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ subi(r16, r16, Operand(1));
  __ stw(r16, MemOperand(r17, kLevelOffset));
  __ LoadP(r0, MemOperand(r17, kLimitOffset));
  __ cmp(r15, r0);
  __ bne(&delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ LoadP(cp, *context_restore_operand);
  }
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand != NULL) {
    __ lwz(r14, *stack_space_operand);
  } else {
    __ mov(r14, Operand(stack_space));
  }
  __ LeaveExitFrame(false, r14, !restore_context, stack_space_operand != NULL);

  // Check if the function scheduled an exception.
  __ LoadRoot(r14, Heap::kTheHoleValueRootIndex);
  __ mov(r15, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ LoadP(r15, MemOperand(r15));
  __ cmp(r14, r15);
  __ bne(&promote_scheduled_exception);

  __ blr();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException, 0, 1);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ StoreP(r15, MemOperand(r17, kLimitOffset));
  __ mr(r14, r3);
  __ PrepareCallCFunction(1, r15);
  __ mov(r3, Operand(ExternalReference::isolate_address(isolate)));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ mr(r3, r14);
  __ b(&leave_exit_frame);
}


static void CallApiFunctionStubHelper(MacroAssembler* masm,
                                      const ParameterCount& argc,
                                      bool return_first_arg,
                                      bool call_data_undefined) {
  // ----------- S t a t e -------------
  //  -- r3                  : callee
  //  -- r7                  : call_data
  //  -- r5                  : holder
  //  -- r4                  : api_function_address
  //  -- r6                  : number of arguments if argc is a register
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 4]   : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register callee = r3;
  Register call_data = r7;
  Register holder = r5;
  Register api_function_address = r4;
  Register context = cp;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kArgsLength == 7);

  DCHECK(argc.is_immediate() || r3.is(argc.reg()));

  // context save
  __ push(context);
  // load context from callee
  __ LoadP(context, FieldMemOperand(callee, JSFunction::kContextOffset));

  // callee
  __ push(callee);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  if (!call_data_undefined) {
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  }
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
  __ mr(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // PPC LINUX ABI:
  //
  // Create 5 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-4] FunctionCallbackInfo
  const int kApiStackSpace = 5;
  const int kFunctionCallbackInfoOffset =
      (kStackFrameExtraParamSlot + 1) * kPointerSize;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(!api_function_address.is(r3) && !scratch.is(r3));
  // r3 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ addi(r3, sp, Operand(kFunctionCallbackInfoOffset));
  // FunctionCallbackInfo::implicit_args_
  __ StoreP(scratch, MemOperand(r3, 0 * kPointerSize));
  if (argc.is_immediate()) {
    // FunctionCallbackInfo::values_
    __ addi(ip, scratch,
            Operand((FCA::kArgsLength - 1 + argc.immediate()) * kPointerSize));
    __ StoreP(ip, MemOperand(r3, 1 * kPointerSize));
    // FunctionCallbackInfo::length_ = argc
    __ li(ip, Operand(argc.immediate()));
    __ stw(ip, MemOperand(r3, 2 * kPointerSize));
    // FunctionCallbackInfo::is_construct_call_ = 0
    __ li(ip, Operand::Zero());
    __ stw(ip, MemOperand(r3, 2 * kPointerSize + kIntSize));
  } else {
    __ ShiftLeftImm(ip, argc.reg(), Operand(kPointerSizeLog2));
    __ addi(ip, ip, Operand((FCA::kArgsLength - 1) * kPointerSize));
    // FunctionCallbackInfo::values_
    __ add(r0, scratch, ip);
    __ StoreP(r0, MemOperand(r3, 1 * kPointerSize));
    // FunctionCallbackInfo::length_ = argc
    __ stw(argc.reg(), MemOperand(r3, 2 * kPointerSize));
    // FunctionCallbackInfo::is_construct_call_
    __ stw(ip, MemOperand(r3, 2 * kPointerSize + kIntSize));
  }

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  MemOperand context_restore_operand(
      fp, (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument
  int return_value_offset = 0;
  if (return_first_arg) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);
  int stack_space = 0;
  MemOperand is_construct_call_operand =
      MemOperand(sp, kFunctionCallbackInfoOffset + 2 * kPointerSize + kIntSize);
  MemOperand* stack_space_operand = &is_construct_call_operand;
  if (argc.is_immediate()) {
    stack_space = argc.immediate() + FCA::kArgsLength + 1;
    stack_space_operand = NULL;
  }
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_operand, return_value_operand,
                           &context_restore_operand);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
  bool call_data_undefined = this->call_data_undefined();
  CallApiFunctionStubHelper(masm, ParameterCount(r6), false,
                            call_data_undefined);
}


void CallApiAccessorStub::Generate(MacroAssembler* masm) {
  bool is_store = this->is_store();
  int argc = this->argc();
  bool call_data_undefined = this->call_data_undefined();
  CallApiFunctionStubHelper(masm, ParameterCount(argc), is_store,
                            call_data_undefined);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- sp[0]                  : name
  //  -- sp[4 - kArgsLength*4]  : PropertyCallbackArguments object
  //  -- ...
  //  -- r5                     : api_function_address
  // -----------------------------------

  Register api_function_address = ApiGetterDescriptor::function_address();
  DCHECK(api_function_address.is(r5));

  __ mr(r3, sp);                               // r0 = Handle<Name>
  __ addi(r4, r3, Operand(1 * kPointerSize));  // r4 = PCA

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
#if ABI_PASSES_HANDLES_IN_REGS
  const int kAccessorInfoSlot = kStackFrameExtraParamSlot + 1;
  const int kApiStackSpace = 2;
#else
  const int kArg0Slot = kStackFrameExtraParamSlot + 1;
  const int kAccessorInfoSlot = kArg0Slot + 1;
  const int kApiStackSpace = 3;
#endif

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

#if !ABI_PASSES_HANDLES_IN_REGS
  // pass 1st arg by reference
  __ StoreP(r3, MemOperand(sp, kArg0Slot * kPointerSize));
  __ addi(r3, sp, Operand(kArg0Slot * kPointerSize));
#endif

  // Create PropertyAccessorInfo instance on the stack above the exit frame with
  // r4 (internal::Object** args_) as the data.
  __ StoreP(r4, MemOperand(sp, kAccessorInfoSlot * kPointerSize));
  // r4 = AccessorInfo&
  __ addi(r4, sp, Operand(kAccessorInfoSlot * kPointerSize));

  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, NULL,
                           MemOperand(fp, 6 * kPointerSize), NULL);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
