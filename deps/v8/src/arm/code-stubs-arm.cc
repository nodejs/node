// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/code-stubs.h"
#include "src/api-arguments.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

#include "src/arm/code-stubs-arm.h"

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

void FastArrayPushStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  Address deopt_handler = Runtime::FunctionForId(Runtime::kArrayPush)->entry;
  descriptor->Initialize(r0, deopt_handler, -1, JS_FUNCTION_STUB_MODE);
}

void FastFunctionBindStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  Address deopt_handler = Runtime::FunctionForId(Runtime::kFunctionBind)->entry;
  descriptor->Initialize(r0, deopt_handler, -1, JS_FUNCTION_STUB_MODE);
}

static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cond);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
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
           r0.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor.GetRegisterParameter(i));
    }
    __ CallExternalReference(miss, param_count);
  }

  __ Ret();
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done;
  Register input_reg = source();
  Register result_reg = destination();
  DCHECK(is_truncating());

  int double_offset = offset();
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += 3 * kPointerSize;

  Register scratch = GetRegisterThatIsNotOneOf(input_reg, result_reg);
  Register scratch_low =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch, scratch_low);
  LowDwVfpRegister double_scratch = kScratchDoubleReg;

  __ Push(scratch_high, scratch_low, scratch);

  if (!skip_fastpath()) {
    // Load double input.
    __ vldr(double_scratch, MemOperand(input_reg, double_offset));
    __ vmov(scratch_low, scratch_high, double_scratch);

    // Do fast-path convert from double to int.
    __ vcvt_s32_f64(double_scratch.low(), double_scratch);
    __ vmov(result_reg, double_scratch.low());

    // If result is not saturated (0x7fffffff or 0x80000000), we are done.
    __ sub(scratch, result_reg, Operand(1));
    __ cmp(scratch, Operand(0x7ffffffe));
    __ b(lt, &done);
  } else {
    // We've already done MacroAssembler::TryFastTruncatedDoubleToILoad, so we
    // know exponent > 31, so we can skip the vcvt_s32_f64 which will saturate.
    if (double_offset == 0) {
      __ ldm(ia, input_reg, scratch_low.bit() | scratch_high.bit());
    } else {
      __ ldr(scratch_low, MemOperand(input_reg, double_offset));
      __ ldr(scratch_high, MemOperand(input_reg, double_offset + kIntSize));
    }
  }

  __ Ubfx(scratch, scratch_high,
         HeapNumber::kExponentShift, HeapNumber::kExponentBits);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is an *ARM* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ sub(scratch, scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ cmp(scratch, Operand(83));
  __ b(ge, &out_of_range);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ rsb(scratch, scratch, Operand(51), SetCC);
  __ b(ls, &only_low);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ mov(scratch_low, Operand(scratch_low, LSR, scratch));
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ rsb(scratch, scratch, Operand(32));
  __ Ubfx(result_reg, scratch_high,
          0, HeapNumber::kMantissaBitsInTopWord);
  // Set the implicit 1 before the mantissa part in scratch_high.
  __ orr(result_reg, result_reg,
         Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  __ orr(result_reg, scratch_low, Operand(result_reg, LSL, scratch));
  __ b(&negate);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ rsb(scratch, scratch, Operand::Zero());
  __ mov(result_reg, Operand(scratch_low, LSL, scratch));

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xffffffff and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xffffffff) + 1 = 0 - result.
  __ eor(result_reg, result_reg, Operand(scratch_high, ASR, 31));
  __ add(result_reg, result_reg, Operand(scratch_high, LSR, 31));

  __ bind(&done);

  __ Pop(scratch_high, scratch_low, scratch);
  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cond) {
  Label not_identical;
  Label heap_number, return_equal;
  __ cmp(r0, r1);
  __ b(ne, &not_identical);

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis.  If it's not a heap number, then return equal.
  if (cond == lt || cond == gt) {
    // Call runtime on identical JSObjects.
    __ CompareObjectType(r0, r4, r4, FIRST_JS_RECEIVER_TYPE);
    __ b(ge, slow);
    // Call runtime on identical symbols since we need to throw a TypeError.
    __ cmp(r4, Operand(SYMBOL_TYPE));
    __ b(eq, slow);
    // Call runtime on identical SIMD values since we must throw a TypeError.
    __ cmp(r4, Operand(SIMD128_VALUE_TYPE));
    __ b(eq, slow);
  } else {
    __ CompareObjectType(r0, r4, r4, HEAP_NUMBER_TYPE);
    __ b(eq, &heap_number);
    // Comparing JS objects with <=, >= is complicated.
    if (cond != eq) {
      __ cmp(r4, Operand(FIRST_JS_RECEIVER_TYPE));
      __ b(ge, slow);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ cmp(r4, Operand(SYMBOL_TYPE));
      __ b(eq, slow);
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ cmp(r4, Operand(SIMD128_VALUE_TYPE));
      __ b(eq, slow);
      // Normally here we fall through to return_equal, but undefined is
      // special: (undefined == undefined) == true, but
      // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
      if (cond == le || cond == ge) {
        __ cmp(r4, Operand(ODDBALL_TYPE));
        __ b(ne, &return_equal);
        __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
        __ cmp(r0, r2);
        __ b(ne, &return_equal);
        if (cond == le) {
          // undefined <= undefined should fail.
          __ mov(r0, Operand(GREATER));
        } else  {
          // undefined >= undefined should fail.
          __ mov(r0, Operand(LESS));
        }
        __ Ret();
      }
    }
  }

  __ bind(&return_equal);
  if (cond == lt) {
    __ mov(r0, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cond == gt) {
    __ mov(r0, Operand(LESS));     // Things aren't greater than themselves.
  } else {
    __ mov(r0, Operand(EQUAL));    // Things are <=, >=, ==, === themselves.
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
    __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    __ Sbfx(r3, r2, HeapNumber::kExponentShift, HeapNumber::kExponentBits);
    // NaNs have all-one exponents so they sign extend to -1.
    __ cmp(r3, Operand(-1));
    __ b(ne, &return_equal);

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ mov(r2, Operand(r2, LSL, HeapNumber::kNonMantissaBitsInTopWord));
    // Or with all low-bits of mantissa.
    __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
    __ orr(r0, r3, Operand(r2), SetCC);
    // For equal we already have the right value in r0:  Return zero (equal)
    // if all bits in mantissa are zero (it's an Infinity) and non-zero if
    // not (it's a NaN).  For <= and >= we need to load r0 with the failing
    // value if it's a NaN.
    if (cond != eq) {
      // All-zero means Infinity means equal.
      __ Ret(eq);
      if (cond == le) {
        __ mov(r0, Operand(GREATER));  // NaN <= NaN should fail.
      } else {
        __ mov(r0, Operand(LESS));     // NaN >= NaN should fail.
      }
    }
    __ Ret();
  }
  // No fall through here.

  __ bind(&not_identical);
}


// See comment at call site.
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict) {
  DCHECK((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  Label rhs_is_smi;
  __ JumpIfSmi(rhs, &rhs_is_smi);

  // Lhs is a Smi.  Check whether the rhs is a heap number.
  __ CompareObjectType(rhs, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If rhs is not a number and lhs is a Smi then strict equality cannot
    // succeed.  Return non-equal
    // If rhs is r0 then there is already a non zero value in it.
    if (!rhs.is(r0)) {
      __ mov(r0, Operand(NOT_EQUAL), LeaveCC, ne);
    }
    __ Ret(ne);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Lhs is a smi, rhs is a number.
  // Convert lhs to a double in d7.
  __ SmiToDouble(d7, lhs);
  // Load the double from rhs, tagged HeapNumber r0, to d6.
  __ vldr(d6, rhs, HeapNumber::kValueOffset - kHeapObjectTag);

  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a smi.
  __ jmp(lhs_not_nan);

  __ bind(&rhs_is_smi);
  // Rhs is a smi.  Check whether the non-smi lhs is a heap number.
  __ CompareObjectType(lhs, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs is not a number and rhs is a smi then strict equality cannot
    // succeed.  Return non-equal.
    // If lhs is r0 then there is already a non zero value in it.
    if (!lhs.is(r0)) {
      __ mov(r0, Operand(NOT_EQUAL), LeaveCC, ne);
    }
    __ Ret(ne);
  } else {
    // Smi compared non-strictly with a non-smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Rhs is a smi, lhs is a heap number.
  // Load the double from lhs, tagged HeapNumber r1, to d7.
  __ vldr(d7, lhs, HeapNumber::kValueOffset - kHeapObjectTag);
  // Convert rhs to a double in d6              .
  __ SmiToDouble(d6, rhs);
  // Fall through to both_loaded_as_doubles.
}


// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs) {
    DCHECK((lhs.is(r0) && rhs.is(r1)) ||
           (lhs.is(r1) && rhs.is(r0)));

    // If either operand is a JS object or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    Label first_non_object;
    // Get the type of the first operand into r2 and compare it with
    // FIRST_JS_RECEIVER_TYPE.
    __ CompareObjectType(rhs, r2, r2, FIRST_JS_RECEIVER_TYPE);
    __ b(lt, &first_non_object);

    // Return non-zero (r0 is not zero)
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ Ret();

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ cmp(r2, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    __ CompareObjectType(lhs, r3, r3, FIRST_JS_RECEIVER_TYPE);
    __ b(ge, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ cmp(r3, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    // Now that we have the types we might as well check for
    // internalized-internalized.
    STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
    __ orr(r2, r2, Operand(r3));
    __ tst(r2, Operand(kIsNotStringMask | kIsNotInternalizedMask));
    __ b(eq, &return_not_equal);
}


// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  DCHECK((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  __ CompareObjectType(rhs, r3, r2, HEAP_NUMBER_TYPE);
  __ b(ne, not_heap_numbers);
  __ ldr(r2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ cmp(r2, r3);
  __ b(ne, slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  __ vldr(d6, rhs, HeapNumber::kValueOffset - kHeapObjectTag);
  __ vldr(d7, lhs, HeapNumber::kValueOffset - kHeapObjectTag);
  __ jmp(both_loaded_as_doubles);
}


// Fast negative check for internalized-to-internalized equality or receiver
// equality. Also handles the undetectable receiver to null/undefined
// comparison.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register lhs, Register rhs,
                                                     Label* possible_strings,
                                                     Label* runtime_call) {
  DCHECK((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  // r2 is object type of rhs.
  Label object_test, return_equal, return_unequal, undetectable;
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ tst(r2, Operand(kIsNotStringMask));
  __ b(ne, &object_test);
  __ tst(r2, Operand(kIsNotInternalizedMask));
  __ b(ne, possible_strings);
  __ CompareObjectType(lhs, r3, r3, FIRST_NONSTRING_TYPE);
  __ b(ge, runtime_call);
  __ tst(r3, Operand(kIsNotInternalizedMask));
  __ b(ne, possible_strings);

  // Both are internalized. We already checked they weren't the same pointer so
  // they are not equal. Return non-equal by returning the non-zero object
  // pointer in r0.
  __ Ret();

  __ bind(&object_test);
  __ ldr(r2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ ldr(r3, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ ldrb(r4, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ ldrb(r5, FieldMemOperand(r3, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsUndetectable));
  __ b(ne, &undetectable);
  __ tst(r5, Operand(1 << Map::kIsUndetectable));
  __ b(ne, &return_unequal);

  __ CompareInstanceType(r2, r2, FIRST_JS_RECEIVER_TYPE);
  __ b(lt, runtime_call);
  __ CompareInstanceType(r3, r3, FIRST_JS_RECEIVER_TYPE);
  __ b(lt, runtime_call);

  __ bind(&return_unequal);
  // Return non-equal by returning the non-zero object pointer in r0.
  __ Ret();

  __ bind(&undetectable);
  __ tst(r5, Operand(1 << Map::kIsUndetectable));
  __ b(eq, &return_unequal);

  // If both sides are JSReceivers, then the result is false according to
  // the HTML specification, which says that only comparisons with null or
  // undefined are affected by special casing for document.all.
  __ CompareInstanceType(r2, r2, ODDBALL_TYPE);
  __ b(eq, &return_equal);
  __ CompareInstanceType(r3, r3, ODDBALL_TYPE);
  __ b(ne, &return_unequal);

  __ bind(&return_equal);
  __ mov(r0, Operand(EQUAL));
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


// On entry r1 and r2 are the values to be compared.
// On exit r0 is 0, positive or negative to indicate the result of
// the comparison.
void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Register lhs = r1;
  Register rhs = r0;
  Condition cc = GetCondition();

  Label miss;
  CompareICStub_CheckInputType(masm, lhs, r2, left(), &miss);
  CompareICStub_CheckInputType(masm, rhs, r3, right(), &miss);

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, lhs_not_nan;

  Label not_two_smis, smi_done;
  __ orr(r2, r1, r0);
  __ JumpIfNotSmi(r2, &not_two_smis);
  __ mov(r1, Operand(r1, ASR, 1));
  __ sub(r0, r1, Operand(r0, ASR, 1));
  __ Ret();
  __ bind(&not_two_smis);

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical.  Either returns the answer
  // or goes to slow.  Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, &slow, cc);

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
  __ and_(r2, lhs, Operand(rhs));
  __ JumpIfNotSmi(r2, &not_smis);
  // One operand is a smi.  EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to lhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison. The double values of the numbers have been loaded into d7 (lhs)
  // and d6 (rhs).
  EmitSmiNonsmiComparison(masm, lhs, rhs, &lhs_not_nan, &slow, strict());

  __ bind(&both_loaded_as_doubles);
  // The arguments have been converted to doubles and stored in d6 and d7.
  __ bind(&lhs_not_nan);
  Label no_nan;
  __ VFPCompareAndSetFlags(d7, d6);
  Label nan;
  __ b(vs, &nan);
  __ mov(r0, Operand(EQUAL), LeaveCC, eq);
  __ mov(r0, Operand(LESS), LeaveCC, lt);
  __ mov(r0, Operand(GREATER), LeaveCC, gt);
  __ Ret();

  __ bind(&nan);
  // If one of the sides was a NaN then the v flag is set.  Load r0 with
  // whatever it takes to make the comparison fail, since comparisons with NaN
  // always fail.
  if (cc == lt || cc == le) {
    __ mov(r0, Operand(GREATER));
  } else {
    __ mov(r0, Operand(LESS));
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
  // or load both doubles into r0, r1, r2, r3 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to
  // check_for_internalized_strings.
  // In this case r2 will contain the type of rhs_.  Never falls through.
  EmitCheckForTwoHeapNumbers(masm,
                             lhs,
                             rhs,
                             &both_loaded_as_doubles,
                             &check_for_internalized_strings,
                             &flat_string_check);

  __ bind(&check_for_internalized_strings);
  // In the strict case the EmitStrictTwoHeapObjectCompare already took care of
  // internalized strings.
  if (cc == eq && !strict()) {
    // Returns an answer for two internalized strings or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that r2 is the type of rhs_ on entry.
    EmitCheckForInternalizedStringsOrObjects(
        masm, lhs, rhs, &flat_string_check, &slow);
  }

  // Check for both being sequential one-byte strings,
  // and inline if that is the case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialOneByteStrings(lhs, rhs, r2, r3, &slow);

  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, r2,
                      r3);
  if (cc == eq) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, r2, r3, r4);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, r2, r3, r4,
                                                    r5);
  }
  // Never falls through to here.

  __ bind(&slow);

  if (cc == eq) {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(lhs, rhs);
      __ CallRuntime(strict() ? Runtime::kStrictEqual : Runtime::kEqual);
    }
    // Turn true into 0 and false into some non-zero value.
    STATIC_ASSERT(EQUAL == 0);
    __ LoadRoot(r1, Heap::kTrueValueRootIndex);
    __ sub(r0, r0, r1);
    __ Ret();
  } else {
    __ Push(lhs, rhs);
    int ncr;  // NaN compare result
    if (cc == lt || cc == le) {
      ncr = GREATER;
    } else {
      DCHECK(cc == gt || cc == ge);  // remaining cases
      ncr = LESS;
    }
    __ mov(r0, Operand(Smi::FromInt(ncr)));
    __ push(r0);

    // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
    // tagged as a small integer.
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ stm(db_w, sp, kCallerSaved | lr.bit());

  const Register scratch = r1;

  if (save_doubles()) {
    __ SaveFPRegs(sp, scratch);
  }
  const int argument_count = 1;
  const int fp_argument_count = 0;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, fp_argument_count, scratch);
  __ mov(r0, Operand(ExternalReference::isolate_address(isolate())));
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()),
      argument_count);
  if (save_doubles()) {
    __ RestoreFPRegs(sp, scratch);
  }
  __ ldm(ia_w, sp, kCallerSaved | pc.bit());  // Also pop pc to get Ret(0).
}

void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(r2));
  const LowDwVfpRegister double_base = d0;
  const LowDwVfpRegister double_exponent = d1;
  const LowDwVfpRegister double_result = d2;
  const LowDwVfpRegister double_scratch = d3;
  const SwVfpRegister single_scratch = s6;
  const Register scratch = r9;
  const Register scratch2 = r4;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == TAGGED) {
    // Base is already in double_base.
    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ vldr(double_exponent,
            FieldMemOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type() != INTEGER) {
    // Detect integer exponents stored as double.
    __ TryDoubleToInt32Exact(scratch, double_exponent, double_scratch);
    __ b(eq, &int_exponent);

    __ push(lr);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()), 0, 2);
    }
    __ pop(lr);
    __ MovFromFloatResult(double_result);
    __ b(&done);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);

  // Get two copies of exponent in the registers scratch and exponent.
  if (exponent_type() == INTEGER) {
    __ mov(scratch, exponent);
  } else {
    // Exponent has previously been stored into scratch as untagged integer.
    __ mov(exponent, scratch);
  }
  __ vmov(double_scratch, double_base);  // Back up base.
  __ vmov(double_result, 1.0, scratch2);

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
  __ vmov(double_scratch, 1.0, scratch);
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
    __ PrepareCallCFunction(0, 2, scratch);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                     0, 2);
  }
  __ pop(lr);
  __ MovFromFloatResult(double_result);

  __ bind(&done);
  __ Ret();
}

bool CEntryStub::NeedsImmovableCode() {
  return true;
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  CommonArrayConstructorStub::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  CreateWeakCellStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
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

  int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
#if V8_HOST_ARCH_ARM
  if (FLAG_debug_code) {
    if (frame_alignment > kPointerSize) {
      Label alignment_as_expected;
      DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
      __ tst(sp, Operand(frame_alignment_mask));
      __ b(eq, &alignment_as_expected);
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      __ stop("Unexpected alignment");
      __ bind(&alignment_as_expected);
    }
  }
#endif

  // Call C built-in.
  int result_stack_size;
  if (result_size() <= 2) {
    // r0 = argc, r1 = argv, r2 = isolate
    __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
    result_stack_size = 0;
  } else {
    DCHECK_EQ(3, result_size());
    // Allocate additional space for the result.
    result_stack_size =
        ((result_size() * kPointerSize) + frame_alignment_mask) &
        ~frame_alignment_mask;
    __ sub(sp, sp, Operand(result_stack_size));

    // r0 = hidden result argument, r1 = argc, r2 = argv, r3 = isolate.
    __ mov(r3, Operand(ExternalReference::isolate_address(isolate())));
    __ mov(r2, Operand(r1));
    __ mov(r1, Operand(r0));
    __ mov(r0, Operand(sp));
  }

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
    __ str(lr, MemOperand(sp, result_stack_size));
    __ Call(r5);
  }
  if (result_size() > 2) {
    DCHECK_EQ(3, result_size());
    // Read result values stored on stack.
    __ ldr(r2, MemOperand(sp, 2 * kPointerSize));
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));
    __ ldr(r0, MemOperand(sp, 0 * kPointerSize));
  }
  // Result returned in r0, r1:r0 or r2:r1:r0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r0, Heap::kExceptionRootIndex);
  __ b(eq, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address(
        Isolate::kPendingExceptionAddress, isolate());
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
  Register argc;
  if (argv_in_register()) {
    // We don't want to pop arguments so set argc to no_reg.
    argc = no_reg;
  } else {
    // Callee-saved register r4 still holds argc.
    argc = r4;
  }
  __ LeaveExitFrame(save_doubles(), argc, true);
  __ mov(pc, lr);

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

  // Ask the runtime for help to determine the handler. This will set r0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, r0);
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

  // Compute the handler entry address and jump to it.
  ConstantPoolUnavailableScope constant_pool_unavailable(masm);
  __ mov(r1, Operand(pending_handler_code_address));
  __ ldr(r1, MemOperand(r1));
  __ mov(r2, Operand(pending_handler_offset_address));
  __ ldr(r2, MemOperand(r2));
  __ add(r1, r1, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start
  if (FLAG_enable_embedded_constant_pool) {
    __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r1);
  }
  __ add(pc, r1, r2);
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
  __ vmov(kDoubleRegZero, 0.0);

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
  int marker = type();
  if (FLAG_enable_embedded_constant_pool) {
    __ mov(r8, Operand::Zero());
  }
  __ mov(r7, Operand(Smi::FromInt(marker)));
  __ mov(r6, Operand(Smi::FromInt(marker)));
  __ mov(r5,
         Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ ldr(r5, MemOperand(r5));
  __ mov(ip, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  __ stm(db_w, sp, r5.bit() | r6.bit() | r7.bit() |
                       (FLAG_enable_embedded_constant_pool ? r8.bit() : 0) |
                       ip.bit());

  // Set up frame pointer for the frame to be pushed.
  __ add(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ mov(r5, Operand(ExternalReference(js_entry_sp)));
  __ ldr(r6, MemOperand(r5));
  __ cmp(r6, Operand::Zero());
  __ b(ne, &non_outermost_js);
  __ str(fp, MemOperand(r5));
  __ mov(ip, Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  Label cont;
  __ b(&cont);
  __ bind(&non_outermost_js);
  __ mov(ip, Operand(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));
  __ bind(&cont);
  __ push(ip);

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
    __ mov(ip, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                         isolate())));
  }
  __ str(r0, MemOperand(ip));
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
  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate());
    __ mov(ip, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate());
    __ mov(ip, Operand(entry));
  }
  __ ldr(ip, MemOperand(ip));  // deref address
  __ add(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Branch and link to JSEntryTrampoline.
  __ Call(ip);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // r0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r5);
  __ cmp(r5, Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ b(ne, &non_outermost_js_2);
  __ mov(r6, Operand::Zero());
  __ mov(r5, Operand(ExternalReference(js_entry_sp)));
  __ str(r6, MemOperand(r5));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r3);
  __ mov(ip,
         Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ str(r3, MemOperand(ip));

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


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // Ensure that the vector and slot registers won't be clobbered before
  // calling the miss handler.
  DCHECK(!AreAliased(r4, r5, LoadWithVectorDescriptor::VectorRegister(),
                     LoadWithVectorDescriptor::SlotRegister()));

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, r4,
                                                          r5, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void LoadIndexedStringStub::Generate(MacroAssembler* masm) {
  // Return address is in lr.
  Label miss;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register index = LoadDescriptor::NameRegister();
  Register scratch = r5;
  Register result = r0;
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
                                          RECEIVER_IS_STRING);
  char_at_generator.GenerateFast(masm);
  __ Ret();

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
  //  sp[0]: last_match_info (expected JSArray)
  //  sp[4]: previous index
  //  sp[8]: subject string
  //  sp[12]: JSRegExp object

  const int kLastMatchInfoOffset = 0 * kPointerSize;
  const int kPreviousIndexOffset = 1 * kPointerSize;
  const int kSubjectOffset = 2 * kPointerSize;
  const int kJSRegExpOffset = 3 * kPointerSize;

  Label runtime;
  // Allocation of registers for this function. These are in callee save
  // registers and will be preserved by the call to the native RegExp code, as
  // this code is called using the normal C calling convention. When calling
  // directly from generated code the native RegExp code will not do a GC and
  // therefore the content of these registers are safe to use after the call.
  Register subject = r4;
  Register regexp_data = r5;
  Register last_match_info_elements = no_reg;  // will be r6;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ mov(r0, Operand(address_of_regexp_stack_memory_size));
  __ ldr(r0, MemOperand(r0, 0));
  __ cmp(r0, Operand::Zero());
  __ b(eq, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ ldr(r0, MemOperand(sp, kJSRegExpOffset));
  __ JumpIfSmi(r0, &runtime);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
  __ b(ne, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ ldr(regexp_data, FieldMemOperand(r0, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ SmiTst(regexp_data);
    __ Check(ne, kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CompareObjectType(regexp_data, r0, r0, FIXED_ARRAY_TYPE);
    __ Check(eq, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ ldr(r0, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ cmp(r0, Operand(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ b(ne, &runtime);

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ ldr(r2,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // Multiplying by 2 comes for free since r2 is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmp(r2, Operand(Isolate::kJSRegexpStaticOffsetsVectorSize - 2));
  __ b(hi, &runtime);

  // Reset offset for possibly sliced string.
  __ mov(r9, Operand::Zero());
  __ ldr(subject, MemOperand(sp, kSubjectOffset));
  __ JumpIfSmi(subject, &runtime);
  __ mov(r3, subject);  // Make a copy of the original subject string.
  // subject: subject string
  // r3: subject string
  // regexp_data: RegExp data (FixedArray)
  // Handle subject string according to its encoding and representation:
  // (1) Sequential string?  If yes, go to (4).
  // (2) Sequential or cons?  If not, go to (5).
  // (3) Cons string.  If the string is flat, replace subject with first string
  //     and go to (1). Otherwise bail out to runtime.
  // (4) Sequential string.  Load regexp code according to encoding.
  // (E) Carry on.
  /// [...]

  // Deferred code at the end of the stub:
  // (5) Long external string?  If not, go to (7).
  // (6) External string.  Make it, offset-wise, look like a sequential string.
  //     Go to (4).
  // (7) Short external string or not a string?  If yes, bail out to runtime.
  // (8) Sliced string.  Replace subject with parent.  Go to (1).

  Label seq_string /* 4 */, external_string /* 6 */, check_underlying /* 1 */,
      not_seq_nor_cons /* 5 */, not_long_external /* 7 */;

  __ bind(&check_underlying);
  __ ldr(r0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));

  // (1) Sequential string?  If yes, go to (4).
  __ and_(r1,
          r0,
          Operand(kIsNotStringMask |
                  kStringRepresentationMask |
                  kShortExternalStringMask),
          SetCC);
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ b(eq, &seq_string);  // Go to (4).

  // (2) Sequential or cons?  If not, go to (5).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  __ cmp(r1, Operand(kExternalStringTag));
  __ b(ge, &not_seq_nor_cons);  // Go to (5).

  // (3) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ ldr(r0, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ CompareRoot(r0, Heap::kempty_stringRootIndex);
  __ b(ne, &runtime);
  __ ldr(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  __ jmp(&check_underlying);

  // (4) Sequential string.  Load regexp code according to encoding.
  __ bind(&seq_string);
  // subject: sequential subject string (or look-alike, external string)
  // r3: original subject string
  // Load previous index and check range before r3 is overwritten.  We have to
  // use r3 instead of subject here because subject might have been only made
  // to look like a sequential string when it actually is an external string.
  __ ldr(r1, MemOperand(sp, kPreviousIndexOffset));
  __ JumpIfNotSmi(r1, &runtime);
  __ ldr(r3, FieldMemOperand(r3, String::kLengthOffset));
  __ cmp(r3, Operand(r1));
  __ b(ls, &runtime);
  __ SmiUntag(r1);

  STATIC_ASSERT(4 == kOneByteStringTag);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ and_(r0, r0, Operand(kStringEncodingMask));
  __ mov(r3, Operand(r0, ASR, 2), SetCC);
  __ ldr(r6, FieldMemOperand(regexp_data, JSRegExp::kDataOneByteCodeOffset),
         ne);
  __ ldr(r6, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset), eq);

  // (E) Carry on.  String handling is done.
  // r6: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(r6, &runtime);

  // r1: previous index
  // r3: encoding of subject string (1 if one_byte, 0 if two_byte);
  // r6: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(isolate()->counters()->regexp_entry_native(), 1, r0, r2);

  // Isolates: note we add an additional parameter here (isolate pointer).
  const int kRegExpExecuteArguments = 9;
  const int kParameterRegisters = 4;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers.

  // Argument 9 (sp[20]): Pass current isolate address.
  __ mov(r0, Operand(ExternalReference::isolate_address(isolate())));
  __ str(r0, MemOperand(sp, 5 * kPointerSize));

  // Argument 8 (sp[16]): Indicate that this is a direct call from JavaScript.
  __ mov(r0, Operand(1));
  __ str(r0, MemOperand(sp, 4 * kPointerSize));

  // Argument 7 (sp[12]): Start (high end) of backtracking stack memory area.
  __ mov(r0, Operand(address_of_regexp_stack_memory_address));
  __ ldr(r0, MemOperand(r0, 0));
  __ mov(r2, Operand(address_of_regexp_stack_memory_size));
  __ ldr(r2, MemOperand(r2, 0));
  __ add(r0, r0, Operand(r2));
  __ str(r0, MemOperand(sp, 3 * kPointerSize));

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  __ mov(r0, Operand::Zero());
  __ str(r0, MemOperand(sp, 2 * kPointerSize));

  // Argument 5 (sp[4]): static offsets vector buffer.
  __ mov(r0,
         Operand(ExternalReference::address_of_static_offsets_vector(
             isolate())));
  __ str(r0, MemOperand(sp, 1 * kPointerSize));

  // For arguments 4 and 3 get string length, calculate start of string data and
  // calculate the shift of the index (0 for one-byte and 1 for two-byte).
  __ add(r7, subject, Operand(SeqString::kHeaderSize - kHeapObjectTag));
  __ eor(r3, r3, Operand(1));
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to two pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize.)
  __ ldr(subject, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));
  // If slice offset is not 0, load the length from the original sliced string.
  // Argument 4, r3: End of string data
  // Argument 3, r2: Start of string data
  // Prepare start and end index of the input.
  __ add(r9, r7, Operand(r9, LSL, r3));
  __ add(r2, r9, Operand(r1, LSL, r3));

  __ ldr(r7, FieldMemOperand(subject, String::kLengthOffset));
  __ SmiUntag(r7);
  __ add(r3, r9, Operand(r7, LSL, r3));

  // Argument 2 (r1): Previous index.
  // Already there

  // Argument 1 (r0): Subject string.
  __ mov(r0, subject);

  // Locate the code entry and call it.
  __ add(r6, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm, r6);

  __ LeaveExitFrame(false, no_reg, true);

  last_match_info_elements = r6;

  // r0: result
  // subject: subject string (callee saved)
  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)
  // Check the result.
  Label success;
  __ cmp(r0, Operand(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ b(eq, &success);
  Label failure;
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::FAILURE));
  __ b(eq, &failure);
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ b(ne, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ mov(r1, Operand(isolate()->factory()->the_hole_value()));
  __ mov(r2, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));
  __ ldr(r0, MemOperand(r2, 0));
  __ cmp(r0, r1);
  __ b(eq, &runtime);

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  __ bind(&failure);
  // For failure and exception return null.
  __ mov(r0, Operand(isolate()->factory()->null_value()));
  __ add(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ ldr(r1,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  // Multiplying by 2 comes for free since r1 is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(r1, r1, Operand(2));  // r1 was a smi.

  __ ldr(r0, MemOperand(sp, kLastMatchInfoOffset));
  __ JumpIfSmi(r0, &runtime);
  __ CompareObjectType(r0, r2, r2, JS_OBJECT_TYPE);
  __ b(ne, &runtime);
  // Check that the object has fast elements.
  __ ldr(last_match_info_elements,
         FieldMemOperand(r0, JSArray::kElementsOffset));
  __ ldr(r0, FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ CompareRoot(r0, Heap::kFixedArrayMapRootIndex);
  __ b(ne, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ ldr(r0,
         FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ add(r2, r1, Operand(RegExpImpl::kLastMatchOverhead));
  __ cmp(r2, Operand::SmiUntag(r0));
  __ b(gt, &runtime);

  // r1: number of capture registers
  // r4: subject string
  // Store the capture count.
  __ SmiTag(r2, r1);
  __ str(r2, FieldMemOperand(last_match_info_elements,
                             RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  __ mov(r2, subject);
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastSubjectOffset,
                      subject,
                      r3,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs);
  __ mov(subject, r2);
  __ str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastInputOffset,
                      subject,
                      r3,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ mov(r2, Operand(address_of_static_offsets_vector));

  // r1: number of capture registers
  // r2: offsets vector
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ add(r0,
         last_match_info_elements,
         Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag));
  __ bind(&next_capture);
  __ sub(r1, r1, Operand(1), SetCC);
  __ b(mi, &done);
  // Read the value from the static offsets vector buffer.
  __ ldr(r3, MemOperand(r2, kPointerSize, PostIndex));
  // Store the smi value in the last match info.
  __ SmiTag(r3);
  __ str(r3, MemOperand(r0, kPointerSize, PostIndex));
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ ldr(r0, MemOperand(sp, kLastMatchInfoOffset));
  __ add(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec);

  // Deferred code for string handling.
  // (5) Long external string?  If not, go to (7).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set.
  __ b(gt, &not_long_external);  // Go to (7).

  // (6) External string.  Make it, offset-wise, look like a sequential string.
  __ bind(&external_string);
  __ ldr(r0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ tst(r0, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound);
  }
  __ ldr(subject,
         FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ sub(subject,
         subject,
         Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ jmp(&seq_string);  // Go to (4).

  // (7) Short external string or not a string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ tst(r1, Operand(kIsNotStringMask | kShortExternalStringMask));
  __ b(ne, &runtime);

  // (8) Sliced string.  Replace subject with parent.  Go to (4).
  // Load offset into r9 and replace subject string with parent.
  __ ldr(r9, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ SmiUntag(r9);
  __ ldr(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ jmp(&check_underlying);  // Go to (4).
#endif  // V8_INTERPRETED_REGEXP
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // r0 : number of arguments to the construct function
  // r1 : the function to call
  // r2 : feedback vector
  // r3 : slot in feedback vector (Smi)
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

  // Number-of-arguments register must be smi-tagged to call out.
  __ SmiTag(r0);
  __ Push(r3, r2, r1, r0);
  __ Push(cp);

  __ CallStub(stub);

  __ Pop(cp);
  __ Pop(r3, r2, r1, r0);
  __ SmiUntag(r0);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // r0 : number of arguments to the construct function
  // r1 : the function to call
  // r2 : feedback vector
  // r3 : slot in feedback vector (Smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*TypeFeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  // Load the cache state into r5.
  __ add(r5, r2, Operand::PointerOffsetFromSmiKey(r3));
  __ ldr(r5, FieldMemOperand(r5, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if r5 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in type-feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = r6;
  Register weak_value = r9;
  __ ldr(weak_value, FieldMemOperand(r5, WeakCell::kValueOffset));
  __ cmp(r1, weak_value);
  __ b(eq, &done);
  __ CompareRoot(r5, Heap::kmegamorphic_symbolRootIndex);
  __ b(eq, &done);
  __ ldr(feedback_map, FieldMemOperand(r5, HeapObject::kMapOffset));
  __ CompareRoot(feedback_map, Heap::kWeakCellMapRootIndex);
  __ b(ne, &check_allocation_site);

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ JumpIfSmi(weak_value, &initialize);
  __ jmp(&megamorphic);

  __ bind(&check_allocation_site);
  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the slot either some other function or an
  // AllocationSite.
  __ CompareRoot(feedback_map, Heap::kAllocationSiteMapRootIndex);
  __ b(ne, &miss);

  // Make sure the function is the Array() function
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r5);
  __ cmp(r1, r5);
  __ b(ne, &megamorphic);
  __ jmp(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(r5, Heap::kuninitialized_symbolRootIndex);
  __ b(eq, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ add(r5, r2, Operand::PointerOffsetFromSmiKey(r3));
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ str(ip, FieldMemOperand(r5, FixedArray::kHeaderSize));
  __ jmp(&done);

  // An uninitialized cache is patched with the function
  __ bind(&initialize);

  // Make sure the function is the Array() function
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r5);
  __ cmp(r1, r5);
  __ b(ne, &not_array_function);

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the
  // slot.
  CreateAllocationSiteStub create_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &create_stub);
  __ b(&done);

  __ bind(&not_array_function);
  CreateWeakCellStub weak_cell_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &weak_cell_stub);

  __ bind(&done);

  // Increment the call count for all function calls.
  __ add(r5, r2, Operand::PointerOffsetFromSmiKey(r3));
  __ add(r5, r5, Operand(FixedArray::kHeaderSize + kPointerSize));
  __ ldr(r4, FieldMemOperand(r5, 0));
  __ add(r4, r4, Operand(Smi::FromInt(1)));
  __ str(r4, FieldMemOperand(r5, 0));
}

void CallConstructStub::Generate(MacroAssembler* masm) {
  // r0 : number of arguments
  // r1 : the function to call
  // r2 : feedback vector
  // r3 : slot in feedback vector (Smi, for RecordCallTarget)

  Label non_function;
  // Check that the function is not a smi.
  __ JumpIfSmi(r1, &non_function);
  // Check that the function is a JSFunction.
  __ CompareObjectType(r1, r5, r5, JS_FUNCTION_TYPE);
  __ b(ne, &non_function);

  GenerateRecordCallTarget(masm);

  __ add(r5, r2, Operand::PointerOffsetFromSmiKey(r3));
  Label feedback_register_initialized;
  // Put the AllocationSite from the feedback vector into r2, or undefined.
  __ ldr(r2, FieldMemOperand(r5, FixedArray::kHeaderSize));
  __ ldr(r5, FieldMemOperand(r2, AllocationSite::kMapOffset));
  __ CompareRoot(r5, Heap::kAllocationSiteMapRootIndex);
  __ b(eq, &feedback_register_initialized);
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(r2, r5);

  // Pass function as new target.
  __ mov(r3, r1);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kConstructStubOffset));
  __ add(pc, r4, Operand(Code::kHeaderSize - kHeapObjectTag));

  __ bind(&non_function);
  __ mov(r3, r1);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// Note: feedback_vector and slot are clobbered after the call.
static void IncrementCallCount(MacroAssembler* masm, Register feedback_vector,
                               Register slot) {
  __ add(feedback_vector, feedback_vector,
         Operand::PointerOffsetFromSmiKey(slot));
  __ add(feedback_vector, feedback_vector,
         Operand(FixedArray::kHeaderSize + kPointerSize));
  __ ldr(slot, FieldMemOperand(feedback_vector, 0));
  __ add(slot, slot, Operand(Smi::FromInt(1)));
  __ str(slot, FieldMemOperand(feedback_vector, 0));
}

void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // r1 - function
  // r3 - slot id
  // r2 - vector
  // r4 - allocation site (loaded from vector[slot])
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r5);
  __ cmp(r1, r5);
  __ b(ne, miss);

  __ mov(r0, Operand(arg_count()));

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, r2, r3);

  __ mov(r2, r4);
  __ mov(r3, r1);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);
}


void CallICStub::Generate(MacroAssembler* masm) {
  // r1 - function
  // r3 - slot id (Smi)
  // r2 - vector
  Label extra_checks_or_miss, call, call_function, call_count_incremented;
  int argc = arg_count();
  ParameterCount actual(argc);

  // The checks. First, does r1 match the recorded monomorphic target?
  __ add(r4, r2, Operand::PointerOffsetFromSmiKey(r3));
  __ ldr(r4, FieldMemOperand(r4, FixedArray::kHeaderSize));

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

  __ ldr(r5, FieldMemOperand(r4, WeakCell::kValueOffset));
  __ cmp(r1, r5);
  __ b(ne, &extra_checks_or_miss);

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(r1, &extra_checks_or_miss);

  __ bind(&call_function);

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, r2, r3);

  __ mov(r0, Operand(argc));
  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ CompareRoot(r4, Heap::kmegamorphic_symbolRootIndex);
  __ b(eq, &call);

  // Verify that r4 contains an AllocationSite
  __ ldr(r5, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ CompareRoot(r5, Heap::kAllocationSiteMapRootIndex);
  __ b(ne, &not_allocation_site);

  // We have an allocation site.
  HandleArrayCase(masm, &miss);

  __ bind(&not_allocation_site);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ jmp(&miss);
  }

  __ CompareRoot(r4, Heap::kuninitialized_symbolRootIndex);
  __ b(eq, &uninitialized);

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(r4);
  __ CompareObjectType(r4, r5, r5, JS_FUNCTION_TYPE);
  __ b(ne, &miss);
  __ add(r4, r2, Operand::PointerOffsetFromSmiKey(r3));
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ str(ip, FieldMemOperand(r4, FixedArray::kHeaderSize));

  __ bind(&call);

  // Increment the call count for megamorphic function calls.
  IncrementCallCount(masm, r2, r3);

  __ bind(&call_count_incremented);
  __ mov(r0, Operand(argc));
  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(r1, &miss);

  // Goto miss case if we do not have a function.
  __ CompareObjectType(r1, r4, r4, JS_FUNCTION_TYPE);
  __ b(ne, &miss);

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r4);
  __ cmp(r1, r4);
  __ b(eq, &miss);

  // Make sure the function belongs to the same native context.
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kContextOffset));
  __ ldr(r4, ContextMemOperand(r4, Context::NATIVE_CONTEXT_INDEX));
  __ ldr(ip, NativeContextMemOperand());
  __ cmp(r4, ip);
  __ b(ne, &miss);

  // Store the function. Use a stub since we need a frame for allocation.
  // r2 - vector
  // r3 - slot
  // r1 - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(masm->isolate());
    __ Push(r2);
    __ Push(r3);
    __ Push(cp, r1);
    __ CallStub(&create_stub);
    __ Pop(cp, r1);
    __ Pop(r3);
    __ Pop(r2);
  }

  __ jmp(&call_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  __ jmp(&call_count_incremented);
}


void CallICStub::GenerateMiss(MacroAssembler* masm) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

  // Push the receiver and the function and feedback info.
  __ Push(r1, r2, r3);

  // Call the entry.
  __ CallRuntime(Runtime::kCallIC_Miss);

  // Move result to edi and exit the internal frame.
  __ mov(r1, r0);
}


// StringCharCodeAtGenerator
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
    __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ tst(result_, Operand(kIsNotStringMask));
    __ b(ne, receiver_not_string_);
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ ldr(ip, FieldMemOperand(object_, String::kLengthOffset));
  __ cmp(ip, Operand(index_));
  __ b(ls, index_out_of_range_);

  __ SmiUntag(index_);

  StringCharLoadGenerator::Generate(masm,
                                    object_,
                                    index_,
                                    result_,
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
  __ CheckMap(index_,
              result_,
              Heap::kHeapNumberMapRootIndex,
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Push(LoadWithVectorDescriptor::VectorRegister(),
            LoadWithVectorDescriptor::SlotRegister(), object_, index_);
  } else {
    // index_ is consumed by runtime conversion function.
    __ Push(object_, index_);
  }
  __ CallRuntime(Runtime::kNumberToSmi);
  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.
  __ Move(index_, r0);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Pop(LoadWithVectorDescriptor::VectorRegister(),
           LoadWithVectorDescriptor::SlotRegister(), object_);
  } else {
    __ pop(object_);
  }
  // Reload the instance type.
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
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
  __ SmiTag(index_);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT);
  __ Move(result_, r0);
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
  __ tst(code_, Operand(kSmiTagMask |
                        ((~String::kMaxOneByteCharCodeU) << kSmiTagSize)));
  __ b(ne, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged one-byte char code.
  __ add(result_, result_, Operand::PointerOffsetFromSmiKey(code_));
  __ ldr(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ CompareRoot(result_, Heap::kUndefinedValueRootIndex);
  __ b(eq, &slow_case_);
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
  __ Move(result_, r0);
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


enum CopyCharactersFlags { COPY_ONE_BYTE = 1, DEST_ALWAYS_ALIGNED = 2 };


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          Register scratch,
                                          String::Encoding encoding) {
  if (FLAG_debug_code) {
    // Check that destination is word aligned.
    __ tst(dest, Operand(kPointerAlignmentMask));
    __ Check(eq, kDestinationOfCopyNotAligned);
  }

  // Assumes word reads and writes are little endian.
  // Nothing to do for zero characters.
  Label done;
  if (encoding == String::TWO_BYTE_ENCODING) {
    __ add(count, count, Operand(count), SetCC);
  }

  Register limit = count;  // Read until dest equals this.
  __ add(limit, dest, Operand(count));

  Label loop_entry, loop;
  // Copy bytes from src to dest until dest hits limit.
  __ b(&loop_entry);
  __ bind(&loop);
  __ ldrb(scratch, MemOperand(src, 1, PostIndex), lt);
  __ strb(scratch, MemOperand(dest, 1, PostIndex));
  __ bind(&loop_entry);
  __ cmp(dest, Operand(limit));
  __ b(lt, &loop);

  __ bind(&done);
}


void StringHelper::GenerateFlatOneByteStringEquals(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ ldr(length, FieldMemOperand(left, String::kLengthOffset));
  __ ldr(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ cmp(length, scratch2);
  __ b(eq, &check_zero_length);
  __ bind(&strings_not_equal);
  __ mov(r0, Operand(Smi::FromInt(NOT_EQUAL)));
  __ Ret();

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ cmp(length, Operand::Zero());
  __ b(ne, &compare_chars);
  __ mov(r0, Operand(Smi::FromInt(EQUAL)));
  __ Ret();

  // Compare characters.
  __ bind(&compare_chars);
  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2, scratch3,
                                  &strings_not_equal);

  // Characters are equal.
  __ mov(r0, Operand(Smi::FromInt(EQUAL)));
  __ Ret();
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3, Register scratch4) {
  Label result_not_equal, compare_lengths;
  // Find minimum length and length difference.
  __ ldr(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ ldr(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ sub(scratch3, scratch1, Operand(scratch2), SetCC);
  Register length_delta = scratch3;
  __ mov(scratch1, scratch2, LeaveCC, gt);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ cmp(min_length, Operand::Zero());
  __ b(eq, &compare_lengths);

  // Compare loop.
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  scratch4, &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ bind(&compare_lengths);
  DCHECK(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use length_delta as result if it's zero.
  __ mov(r0, Operand(length_delta), SetCC);
  __ bind(&result_not_equal);
  // Conditionally update the result based either on length_delta or
  // the last comparion performed in the loop above.
  __ mov(r0, Operand(Smi::FromInt(GREATER)), LeaveCC, gt);
  __ mov(r0, Operand(Smi::FromInt(LESS)), LeaveCC, lt);
  __ Ret();
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch1, Register scratch2, Label* chars_not_equal) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ add(scratch1, length,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ add(left, left, Operand(scratch1));
  __ add(right, right, Operand(scratch1));
  __ rsb(length, length, Operand::Zero());
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ ldrb(scratch1, MemOperand(left, index));
  __ ldrb(scratch2, MemOperand(right, index));
  __ cmp(scratch1, scratch2);
  __ b(ne, chars_not_equal);
  __ add(index, index, Operand(1), SetCC);
  __ b(ne, &loop);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1    : left
  //  -- r0    : right
  //  -- lr    : return address
  // -----------------------------------

  // Load r2 with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ Move(r2, isolate()->factory()->undefined_value());

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, kExpectedAllocationSite);
    __ push(r2);
    __ ldr(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kAllocationSiteMapRootIndex);
    __ cmp(r2, ip);
    __ pop(r2);
    __ Assert(eq, kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state());
  __ TailCallStub(&stub);
}


void CompareICStub::GenerateBooleans(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::BOOLEAN, state());
  Label miss;

  __ CheckMap(r1, r2, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  __ CheckMap(r0, r3, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  if (!Token::IsEqualityOp(op())) {
    __ ldr(r1, FieldMemOperand(r1, Oddball::kToNumberOffset));
    __ AssertSmi(r1);
    __ ldr(r0, FieldMemOperand(r0, Oddball::kToNumberOffset));
    __ AssertSmi(r0);
  }
  __ sub(r0, r1, r0);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ orr(r2, r1, r0);
  __ JumpIfNotSmi(r2, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    __ sub(r0, r0, r1, SetCC);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(r1);
    __ sub(r0, r1, Operand::SmiUntag(r0));
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

  if (left() == CompareICState::SMI) {
    __ JumpIfNotSmi(r1, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(r0, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved.
  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(r0, &right_smi);
  __ CheckMap(r0, r2, Heap::kHeapNumberMapRootIndex, &maybe_undefined1,
              DONT_DO_SMI_CHECK);
  __ sub(r2, r0, Operand(kHeapObjectTag));
  __ vldr(d1, r2, HeapNumber::kValueOffset);
  __ b(&left);
  __ bind(&right_smi);
  __ SmiToDouble(d1, r0);

  __ bind(&left);
  __ JumpIfSmi(r1, &left_smi);
  __ CheckMap(r1, r2, Heap::kHeapNumberMapRootIndex, &maybe_undefined2,
              DONT_DO_SMI_CHECK);
  __ sub(r2, r1, Operand(kHeapObjectTag));
  __ vldr(d0, r2, HeapNumber::kValueOffset);
  __ b(&done);
  __ bind(&left_smi);
  __ SmiToDouble(d0, r1);

  __ bind(&done);
  // Compare operands.
  __ VFPCompareAndSetFlags(d0, d1);

  // Don't base result on status bits when a NaN is involved.
  __ b(vs, &unordered);

  // Return a result of -1, 0, or 1, based on status bits.
  __ mov(r0, Operand(EQUAL), LeaveCC, eq);
  __ mov(r0, Operand(LESS), LeaveCC, lt);
  __ mov(r0, Operand(GREATER), LeaveCC, gt);
  __ Ret();

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
    __ b(ne, &miss);
    __ JumpIfSmi(r1, &unordered);
    __ CompareObjectType(r1, r2, r2, HEAP_NUMBER_TYPE);
    __ b(ne, &maybe_undefined2);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ CompareRoot(r1, Heap::kUndefinedValueRootIndex);
    __ b(eq, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = r1;
  Register right = r0;
  Register tmp1 = r2;
  Register tmp2 = r3;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are internalized strings.
  __ ldr(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ldr(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ ldrb(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ ldrb(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ orr(tmp1, tmp1, Operand(tmp2));
  __ tst(tmp1, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ b(ne, &miss);

  // Internalized strings are compared by identity.
  __ cmp(left, right);
  // Make sure r0 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r0));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(r0, Operand(Smi::FromInt(EQUAL)), LeaveCC, eq);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  DCHECK(GetCondition() == eq);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = r1;
  Register right = r0;
  Register tmp1 = r2;
  Register tmp2 = r3;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ ldr(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ldr(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ ldrb(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ ldrb(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss);

  // Unique names are compared by identity.
  __ cmp(left, right);
  // Make sure r0 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r0));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(r0, Operand(Smi::FromInt(EQUAL)), LeaveCC, eq);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  Label miss;

  bool equality = Token::IsEqualityOp(op());

  // Registers containing left and right operands respectively.
  Register left = r1;
  Register right = r0;
  Register tmp1 = r2;
  Register tmp2 = r3;
  Register tmp3 = r4;
  Register tmp4 = r5;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ ldr(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ldr(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ ldrb(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ ldrb(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ orr(tmp3, tmp1, tmp2);
  __ tst(tmp3, Operand(kIsNotStringMask));
  __ b(ne, &miss);

  // Fast check for identical strings.
  __ cmp(left, right);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(r0, Operand(Smi::FromInt(EQUAL)), LeaveCC, eq);
  __ Ret(eq);

  // Handle not identical strings.

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We know they are both
  // strings.
  if (equality) {
    DCHECK(GetCondition() == eq);
    STATIC_ASSERT(kInternalizedTag == 0);
    __ orr(tmp3, tmp1, Operand(tmp2));
    __ tst(tmp3, Operand(kIsNotInternalizedMask));
    // Make sure r0 is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(r0));
    __ Ret(eq);
  }

  // Check that both strings are sequential one-byte.
  Label runtime;
  __ JumpIfBothInstanceTypesAreNotSequentialOneByte(tmp1, tmp2, tmp3, tmp4,
                                                    &runtime);

  // Compare flat one-byte strings. Returns when done.
  if (equality) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, left, right, tmp1, tmp2,
                                                  tmp3);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, left, right, tmp1,
                                                    tmp2, tmp3, tmp4);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  if (equality) {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(left, right);
      __ CallRuntime(Runtime::kStringEqual);
    }
    __ LoadRoot(r1, Heap::kTrueValueRootIndex);
    __ sub(r0, r0, r1);
    __ Ret();
  } else {
    __ Push(left, right);
    __ TailCallRuntime(Runtime::kStringCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateReceivers(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::RECEIVER, state());
  Label miss;
  __ and_(r2, r1, Operand(r0));
  __ JumpIfSmi(r2, &miss);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ CompareObjectType(r0, r2, r2, FIRST_JS_RECEIVER_TYPE);
  __ b(lt, &miss);
  __ CompareObjectType(r1, r2, r2, FIRST_JS_RECEIVER_TYPE);
  __ b(lt, &miss);

  DCHECK(GetCondition() == eq);
  __ sub(r0, r0, Operand(r1));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ and_(r2, r1, Operand(r0));
  __ JumpIfSmi(r2, &miss);
  __ GetWeakValue(r4, cell);
  __ ldr(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldr(r3, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ cmp(r2, r4);
  __ b(ne, &miss);
  __ cmp(r3, r4);
  __ b(ne, &miss);

  if (Token::IsEqualityOp(op())) {
    __ sub(r0, r0, Operand(r1));
    __ Ret();
  } else {
    if (op() == Token::LT || op() == Token::LTE) {
      __ mov(r2, Operand(Smi::FromInt(GREATER)));
    } else {
      __ mov(r2, Operand(Smi::FromInt(LESS)));
    }
    __ Push(r1, r0, r2);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1, r0);
    __ Push(lr, r1, r0);
    __ mov(ip, Operand(Smi::FromInt(op())));
    __ push(ip);
    __ CallRuntime(Runtime::kCompareIC_Miss);
    // Compute the entry point of the rewritten stub.
    __ add(r2, r0, Operand(Code::kHeaderSize - kHeapObjectTag));
    // Restore registers.
    __ pop(lr);
    __ Pop(r1, r0);
  }

  __ Jump(r2);
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


void NameDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register receiver,
                                                      Register properties,
                                                      Handle<Name> name,
                                                      Register scratch0) {
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
    __ ldr(index, FieldMemOperand(properties, kCapacityOffset));
    __ sub(index, index, Operand(1));
    __ and_(index, index, Operand(
        Smi::FromInt(name->Hash() + NameDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ add(index, index, Operand(index, LSL, 1));  // index *= 3.

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    Register tmp = properties;
    __ add(tmp, properties, Operand(index, LSL, 1));
    __ ldr(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    DCHECK(!tmp.is(entity_name));
    __ LoadRoot(tmp, Heap::kUndefinedValueRootIndex);
    __ cmp(entity_name, tmp);
    __ b(eq, done);

    // Load the hole ready for use below:
    __ LoadRoot(tmp, Heap::kTheHoleValueRootIndex);

    // Stop if found the property.
    __ cmp(entity_name, Operand(Handle<Name>(name)));
    __ b(eq, miss);

    Label good;
    __ cmp(entity_name, tmp);
    __ b(eq, &good);

    // Check if the entry name is not a unique name.
    __ ldr(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ ldrb(entity_name,
            FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ ldr(properties,
           FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask =
      (lr.bit() | r6.bit() | r5.bit() | r4.bit() | r3.bit() |
       r2.bit() | r1.bit() | r0.bit());

  __ stm(db_w, sp, spill_mask);
  __ ldr(r0, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ mov(r1, Operand(Handle<Name>(name)));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  __ cmp(r0, Operand::Zero());
  __ ldm(ia_w, sp, spill_mask);

  __ b(eq, done);
  __ b(ne, miss);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found. Jump to
// the |miss| label otherwise.
// If lookup was successful |scratch2| will be equal to elements + 4 * index.
void NameDictionaryLookupStub::GeneratePositiveLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register elements,
                                                      Register name,
                                                      Register scratch1,
                                                      Register scratch2) {
  DCHECK(!elements.is(scratch1));
  DCHECK(!elements.is(scratch2));
  DCHECK(!name.is(scratch1));
  DCHECK(!name.is(scratch2));

  __ AssertName(name);

  // Compute the capacity mask.
  __ ldr(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ SmiUntag(scratch1);
  __ sub(scratch1, scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ ldr(scratch2, FieldMemOperand(name, Name::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ add(scratch2, scratch2, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ and_(scratch2, scratch1, Operand(scratch2, LSR, Name::kHashShift));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.
    __ add(scratch2, scratch2, Operand(scratch2, LSL, 1));

    // Check if the key is identical to the name.
    __ add(scratch2, elements, Operand(scratch2, LSL, 2));
    __ ldr(ip, FieldMemOperand(scratch2, kElementsStartOffset));
    __ cmp(name, Operand(ip));
    __ b(eq, done);
  }

  const int spill_mask =
      (lr.bit() | r6.bit() | r5.bit() | r4.bit() |
       r3.bit() | r2.bit() | r1.bit() | r0.bit()) &
      ~(scratch1.bit() | scratch2.bit());

  __ stm(db_w, sp, spill_mask);
  if (name.is(r0)) {
    DCHECK(!elements.is(r1));
    __ Move(r1, name);
    __ Move(r0, elements);
  } else {
    __ Move(r0, elements);
    __ Move(r1, name);
  }
  NameDictionaryLookupStub stub(masm->isolate(), POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ cmp(r0, Operand::Zero());
  __ mov(scratch2, Operand(r2));
  __ ldm(ia_w, sp, spill_mask);

  __ b(ne, done);
  __ b(eq, miss);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Registers:
  //  result: NameDictionary to probe
  //  r1: key
  //  dictionary: NameDictionary to probe.
  //  index: will hold an index of entry if lookup is successful.
  //         might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Register result = r0;
  Register dictionary = r0;
  Register key = r1;
  Register index = r2;
  Register mask = r3;
  Register hash = r4;
  Register undefined = r5;
  Register entry_key = r6;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ ldr(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ SmiUntag(mask);
  __ sub(mask, mask, Operand(1));

  __ ldr(hash, FieldMemOperand(key, Name::kHashFieldOffset));

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
      __ add(index, hash, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    } else {
      __ mov(index, Operand(hash));
    }
    __ and_(index, mask, Operand(index, LSR, Name::kHashShift));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ add(index, index, Operand(index, LSL, 1));  // index *= 3.

    STATIC_ASSERT(kSmiTagSize == 1);
    __ add(index, dictionary, Operand(index, LSL, 2));
    __ ldr(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ cmp(entry_key, Operand(undefined));
    __ b(eq, &not_in_dictionary);

    // Stop if found the property.
    __ cmp(entry_key, Operand(key));
    __ b(eq, &in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ ldr(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ ldrb(entry_key,
              FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(entry_key, &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ mov(result, Operand::Zero());
    __ Ret();
  }

  __ bind(&in_dictionary);
  __ mov(result, Operand(1));
  __ Ret();

  __ bind(&not_in_dictionary);
  __ mov(result, Operand::Zero());
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

  // The first two instructions are generated with labels so as to get the
  // offset fixed up correctly by the bind(Label*) call.  We patch it back and
  // forth between a compare instructions (a nop in this position) and the
  // real branch when we start and stop incremental heap marking.
  // See RecordWriteStub::Patch for details.
  {
    // Block literal pool emission, as the position of these two instructions
    // is assumed by the patching code.
    Assembler::BlockConstPoolScope block_const_pool(masm);
    __ b(&skip_to_incremental_noncompacting);
    __ b(&skip_to_incremental_compacting);
  }

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
  DCHECK(Assembler::GetBranchOffset(masm->instr_at(0)) < (1 << 12));
  DCHECK(Assembler::GetBranchOffset(masm->instr_at(4)) < (1 << 12));
  PatchBranchIntoNop(masm, 0);
  PatchBranchIntoNop(masm, Assembler::kInstrSize);
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ ldr(regs_.scratch0(), MemOperand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),  // Value.
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
  __ Ret();
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode());
  int argument_count = 3;
  __ PrepareCallCFunction(argument_count, regs_.scratch0());
  Register address =
      r0.is(regs_.address()) ? regs_.scratch0() : regs_.address();
  DCHECK(!address.is(regs_.object()));
  DCHECK(!address.is(r0));
  __ Move(address, regs_.address());
  __ Move(r0, regs_.object());
  __ Move(r1, address);
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));

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
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_scratch;

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
  __ ldr(regs_.scratch0(), MemOperand(regs_.address(), 0));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlag(regs_.scratch0(),  // Contains value.
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kEvacuationCandidateMask,
                     eq,
                     &ensure_not_white);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kSkipEvacuationSlotsRecordingMask,
                     eq,
                     &need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need extra registers for this, so we push the object and the address
  // register temporarily.
  __ Push(regs_.object(), regs_.address());
  __ JumpIfWhite(regs_.scratch0(),  // The value.
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


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ Call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrameConstants::kArgumentsLengthOffset;
  __ ldr(r1, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ add(r1, r1, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ mov(r1, Operand(r1, LSL, kPointerSizeLog2));
  __ add(sp, sp, r1);
  __ Ret();
}


void LoadICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(LoadWithVectorDescriptor::VectorRegister());
  LoadICStub stub(isolate());
  stub.GenerateForTrampoline(masm);
}


void KeyedLoadICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(LoadWithVectorDescriptor::VectorRegister());
  KeyedLoadICStub stub(isolate());
  stub.GenerateForTrampoline(masm);
}


void CallICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(r2);
  CallICStub stub(isolate(), state());
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void LoadICStub::Generate(MacroAssembler* masm) { GenerateImpl(masm, false); }


void LoadICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


static void HandleArrayCases(MacroAssembler* masm, Register feedback,
                             Register receiver_map, Register scratch1,
                             Register scratch2, bool is_polymorphic,
                             Label* miss) {
  // feedback initially contains the feedback array
  Label next_loop, prepare_next;
  Label start_polymorphic;

  Register cached_map = scratch1;

  __ ldr(cached_map,
         FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(0)));
  __ ldr(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ cmp(receiver_map, cached_map);
  __ b(ne, &start_polymorphic);
  // found, now call handler.
  Register handler = feedback;
  __ ldr(handler, FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ add(pc, handler, Operand(Code::kHeaderSize - kHeapObjectTag));


  Register length = scratch2;
  __ bind(&start_polymorphic);
  __ ldr(length, FieldMemOperand(feedback, FixedArray::kLengthOffset));
  if (!is_polymorphic) {
    // If the IC could be monomorphic we have to make sure we don't go past the
    // end of the feedback array.
    __ cmp(length, Operand(Smi::FromInt(2)));
    __ b(eq, miss);
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
  __ add(too_far, feedback, Operand::PointerOffsetFromSmiKey(length));
  __ add(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(pointer_reg, feedback,
         Operand(FixedArray::OffsetOfElementAt(2) - kHeapObjectTag));

  __ bind(&next_loop);
  __ ldr(cached_map, MemOperand(pointer_reg));
  __ ldr(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ cmp(receiver_map, cached_map);
  __ b(ne, &prepare_next);
  __ ldr(handler, MemOperand(pointer_reg, kPointerSize));
  __ add(pc, handler, Operand(Code::kHeaderSize - kHeapObjectTag));

  __ bind(&prepare_next);
  __ add(pointer_reg, pointer_reg, Operand(kPointerSize * 2));
  __ cmp(pointer_reg, too_far);
  __ b(lt, &next_loop);

  // We exhausted our array of map handler pairs.
  __ jmp(miss);
}


static void HandleMonomorphicCase(MacroAssembler* masm, Register receiver,
                                  Register receiver_map, Register feedback,
                                  Register vector, Register slot,
                                  Register scratch, Label* compare_map,
                                  Label* load_smi_map, Label* try_array) {
  __ JumpIfSmi(receiver, load_smi_map);
  __ ldr(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ bind(compare_map);
  Register cached_map = scratch;
  // Move the weak map into the weak_cell register.
  __ ldr(cached_map, FieldMemOperand(feedback, WeakCell::kValueOffset));
  __ cmp(cached_map, receiver_map);
  __ b(ne, try_array);
  Register handler = feedback;
  __ add(handler, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(handler,
         FieldMemOperand(handler, FixedArray::kHeaderSize + kPointerSize));
  __ add(pc, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
}


void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // r1
  Register name = LoadWithVectorDescriptor::NameRegister();          // r2
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // r3
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // r0
  Register feedback = r4;
  Register receiver_map = r5;
  Register scratch1 = r6;

  __ add(feedback, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ ldr(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  __ b(ne, &not_array);
  HandleArrayCases(masm, feedback, receiver_map, scratch1, r9, true, &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ b(ne, &miss);
  masm->isolate()->load_stub_cache()->GenerateProbe(
      masm, receiver, name, feedback, receiver_map, scratch1, r9);

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
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // r1
  Register key = LoadWithVectorDescriptor::NameRegister();           // r2
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // r3
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // r0
  Register feedback = r4;
  Register receiver_map = r5;
  Register scratch1 = r6;

  __ add(feedback, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ ldr(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  __ b(ne, &not_array);

  // We have a polymorphic element handler.
  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);
  HandleArrayCases(masm, feedback, receiver_map, scratch1, r9, true, &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ b(ne, &try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmp(key, feedback);
  __ b(ne, &miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ add(feedback, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(feedback,
         FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, r9, false, &miss);

  __ bind(&miss);
  KeyedLoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}

void StoreICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(StoreWithVectorDescriptor::VectorRegister());
  StoreICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}

void KeyedStoreICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadTypeFeedbackVector(StoreWithVectorDescriptor::VectorRegister());
  KeyedStoreICStub stub(isolate(), state());
  stub.GenerateForTrampoline(masm);
}

void StoreICStub::Generate(MacroAssembler* masm) { GenerateImpl(masm, false); }

void StoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}

void StoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = StoreWithVectorDescriptor::ReceiverRegister();  // r1
  Register key = StoreWithVectorDescriptor::NameRegister();           // r2
  Register vector = StoreWithVectorDescriptor::VectorRegister();      // r3
  Register slot = StoreWithVectorDescriptor::SlotRegister();          // r4
  DCHECK(StoreWithVectorDescriptor::ValueRegister().is(r0));          // r0
  Register feedback = r5;
  Register receiver_map = r6;
  Register scratch1 = r9;

  __ add(feedback, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ ldr(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  __ b(ne, &not_array);

  // We are using register r8, which is used for the embedded constant pool
  // when FLAG_enable_embedded_constant_pool is true.
  DCHECK(!FLAG_enable_embedded_constant_pool);
  Register scratch2 = r8;
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, true,
                   &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ b(ne, &miss);
  masm->isolate()->store_stub_cache()->GenerateProbe(
      masm, receiver, key, feedback, receiver_map, scratch1, scratch2);

  __ bind(&miss);
  StoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}

void KeyedStoreICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}

void KeyedStoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
  GenerateImpl(masm, true);
}


static void HandlePolymorphicStoreCase(MacroAssembler* masm, Register feedback,
                                       Register receiver_map, Register scratch1,
                                       Register scratch2, Label* miss) {
  // feedback initially contains the feedback array
  Label next_loop, prepare_next;
  Label start_polymorphic;
  Label transition_call;

  Register cached_map = scratch1;
  Register too_far = scratch2;
  Register pointer_reg = feedback;
  __ ldr(too_far, FieldMemOperand(feedback, FixedArray::kLengthOffset));

  // +-----+------+------+-----+-----+-----+ ... ----+
  // | map | len  | wm0  | wt0 | h0  | wm1 |      hN |
  // +-----+------+------+-----+-----+ ----+ ... ----+
  //                 0      1     2              len-1
  //                 ^                                 ^
  //                 |                                 |
  //             pointer_reg                        too_far
  //             aka feedback                       scratch2
  // also need receiver_map
  // use cached_map (scratch1) to look in the weak map values.
  __ add(too_far, feedback, Operand::PointerOffsetFromSmiKey(too_far));
  __ add(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(pointer_reg, feedback,
         Operand(FixedArray::OffsetOfElementAt(0) - kHeapObjectTag));

  __ bind(&next_loop);
  __ ldr(cached_map, MemOperand(pointer_reg));
  __ ldr(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ cmp(receiver_map, cached_map);
  __ b(ne, &prepare_next);
  // Is it a transitioning store?
  __ ldr(too_far, MemOperand(pointer_reg, kPointerSize));
  __ CompareRoot(too_far, Heap::kUndefinedValueRootIndex);
  __ b(ne, &transition_call);
  __ ldr(pointer_reg, MemOperand(pointer_reg, kPointerSize * 2));
  __ add(pc, pointer_reg, Operand(Code::kHeaderSize - kHeapObjectTag));

  __ bind(&transition_call);
  __ ldr(too_far, FieldMemOperand(too_far, WeakCell::kValueOffset));
  __ JumpIfSmi(too_far, miss);

  __ ldr(receiver_map, MemOperand(pointer_reg, kPointerSize * 2));

  // Load the map into the correct register.
  DCHECK(feedback.is(StoreTransitionDescriptor::MapRegister()));
  __ mov(feedback, too_far);

  __ add(pc, receiver_map, Operand(Code::kHeaderSize - kHeapObjectTag));

  __ bind(&prepare_next);
  __ add(pointer_reg, pointer_reg, Operand(kPointerSize * 3));
  __ cmp(pointer_reg, too_far);
  __ b(lt, &next_loop);

  // We exhausted our array of map handler pairs.
  __ jmp(miss);
}

void KeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = StoreWithVectorDescriptor::ReceiverRegister();  // r1
  Register key = StoreWithVectorDescriptor::NameRegister();           // r2
  Register vector = StoreWithVectorDescriptor::VectorRegister();      // r3
  Register slot = StoreWithVectorDescriptor::SlotRegister();          // r4
  DCHECK(StoreWithVectorDescriptor::ValueRegister().is(r0));          // r0
  Register feedback = r5;
  Register receiver_map = r6;
  Register scratch1 = r9;

  __ add(feedback, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ ldr(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  __ b(ne, &not_array);

  // We have a polymorphic element handler.
  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);

  // We are using register r8, which is used for the embedded constant pool
  // when FLAG_enable_embedded_constant_pool is true.
  DCHECK(!FLAG_enable_embedded_constant_pool);
  Register scratch2 = r8;

  HandlePolymorphicStoreCase(masm, feedback, receiver_map, scratch1, scratch2,
                             &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ b(ne, &try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedStoreIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ cmp(key, feedback);
  __ b(ne, &miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ add(feedback, vector, Operand::PointerOffsetFromSmiKey(slot));
  __ ldr(feedback,
         FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, false,
                   &miss);

  __ bind(&miss);
  KeyedStoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ jmp(&compare_map);
}


void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    ProfileEntryHookStub stub(masm->isolate());
    PredictableCodeSizeScope predictable(masm);
    predictable.ExpectSize(masm->CallStubSize(&stub) +
                           2 * Assembler::kInstrSize);
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

  DCHECK((kCallerSaved & kSavedRegs) == kCallerSaved);

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
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    __ and_(sp, sp, Operand(-frame_alignment));
  }

#if V8_HOST_ARCH_ARM
  int32_t entry_hook =
      reinterpret_cast<int32_t>(isolate()->function_entry_hook());
  __ mov(ip, Operand(entry_hook));
#else
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  // It additionally takes an isolate as a third parameter
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));

  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ mov(ip, Operand(ExternalReference(&dispatcher,
                                       ExternalReference::BUILTIN_CALL,
                                       isolate())));
#endif
  __ Call(ip);

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
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(r3, Operand(kind));
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
  // r2 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // r3 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // r0 - number of arguments
  // r1 - constructor?
  // sp[0] - last argument
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ tst(r3, Operand(1));
    __ b(ne, &normal_sequence);
  }

  // look at the first argument
  __ ldr(r5, MemOperand(sp, 0));
  __ cmp(r5, Operand::Zero());
  __ b(eq, &normal_sequence);

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
    __ add(r3, r3, Operand(1));

    if (FLAG_debug_code) {
      __ ldr(r5, FieldMemOperand(r2, 0));
      __ CompareRoot(r5, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ ldr(r4, FieldMemOperand(r2, AllocationSite::kTransitionInfoOffset));
    __ add(r4, r4, Operand(Smi::FromInt(kFastElementsKindPackedToHoley)));
    __ str(r4, FieldMemOperand(r2, AllocationSite::kTransitionInfoOffset));

    __ bind(&normal_sequence);
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(r3, Operand(kind));
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
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

void CommonArrayConstructorStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArraySingleArgumentConstructorStub>(
      isolate);
  ArrayNArgumentsConstructorStub stub(isolate);
  stub.GetCode();
  ElementsKind kinds[2] = { FAST_ELEMENTS, FAST_HOLEY_ELEMENTS };
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
  if (argument_count() == ANY) {
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
  } else if (argument_count() == NONE) {
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);
  } else if (argument_count() == ONE) {
    CreateArrayDispatchOneArgument(masm, mode);
  } else if (argument_count() == MORE_THAN_ONE) {
    ArrayNArgumentsConstructorStub stub(masm->isolate());
    __ TailCallStub(&stub);
  } else {
    UNREACHABLE();
  }
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
    // Will both indicate a NULL and a Smi.
    __ tst(r4, Operand(kSmiTagMask));
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r4, r4, r5, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);

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

  __ ldr(r3, FieldMemOperand(r2, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(r3);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ and_(r3, r3, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  __ bind(&subclassing);
  switch (argument_count()) {
    case ANY:
    case MORE_THAN_ONE:
      __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
      __ add(r0, r0, Operand(3));
      break;
    case NONE:
      __ str(r1, MemOperand(sp, 0 * kPointerSize));
      __ mov(r0, Operand(3));
      break;
    case ONE:
      __ str(r1, MemOperand(sp, 1 * kPointerSize));
      __ mov(r0, Operand(4));
      break;
  }
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
    // Will both indicate a NULL and a Smi.
    __ tst(r3, Operand(kSmiTagMask));
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r3, r3, r4, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
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
    __ cmp(r3, Operand(FAST_ELEMENTS));
    __ b(eq, &done);
    __ cmp(r3, Operand(FAST_HOLEY_ELEMENTS));
    __ Assert(eq,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmp(r3, Operand(FAST_ELEMENTS));
  __ b(eq, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


void FastNewObjectStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : target
  //  -- r3 : new target
  //  -- cp : context
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r1);
  __ AssertReceiver(r3);

  // Verify that the new target is a JSFunction.
  Label new_object;
  __ CompareObjectType(r3, r2, r2, JS_FUNCTION_TYPE);
  __ b(ne, &new_object);

  // Load the initial map and verify that it's in fact a map.
  __ ldr(r2, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(r2, &new_object);
  __ CompareObjectType(r2, r0, r0, MAP_TYPE);
  __ b(ne, &new_object);

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  __ ldr(r0, FieldMemOperand(r2, Map::kConstructorOrBackPointerOffset));
  __ cmp(r0, r1);
  __ b(ne, &new_object);

  // Allocate the JSObject on the heap.
  Label allocate, done_allocate;
  __ ldrb(r4, FieldMemOperand(r2, Map::kInstanceSizeOffset));
  __ Allocate(r4, r0, r5, r6, &allocate, SIZE_IN_WORDS);
  __ bind(&done_allocate);

  // Initialize the JSObject fields.
  __ str(r2, FieldMemOperand(r0, JSObject::kMapOffset));
  __ LoadRoot(r3, Heap::kEmptyFixedArrayRootIndex);
  __ str(r3, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r3, FieldMemOperand(r0, JSObject::kElementsOffset));
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ add(r1, r0, Operand(JSObject::kHeaderSize - kHeapObjectTag));

  // ----------- S t a t e -------------
  //  -- r0 : result (tagged)
  //  -- r1 : result fields (untagged)
  //  -- r5 : result end (untagged)
  //  -- r2 : initial map
  //  -- cp : context
  //  -- lr : return address
  // -----------------------------------

  // Perform in-object slack tracking if requested.
  Label slack_tracking;
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
  __ ldr(r3, FieldMemOperand(r2, Map::kBitField3Offset));
  __ tst(r3, Operand(Map::ConstructionCounter::kMask));
  __ b(ne, &slack_tracking);
  {
    // Initialize all in-object fields with undefined.
    __ InitializeFieldsWithFiller(r1, r5, r6);
    __ Ret();
  }
  __ bind(&slack_tracking);
  {
    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    __ sub(r3, r3, Operand(1 << Map::ConstructionCounter::kShift));
    __ str(r3, FieldMemOperand(r2, Map::kBitField3Offset));

    // Initialize the in-object fields with undefined.
    __ ldrb(r4, FieldMemOperand(r2, Map::kUnusedPropertyFieldsOffset));
    __ sub(r4, r5, Operand(r4, LSL, kPointerSizeLog2));
    __ InitializeFieldsWithFiller(r1, r4, r6);

    // Initialize the remaining (reserved) fields with one pointer filler map.
    __ LoadRoot(r6, Heap::kOnePointerFillerMapRootIndex);
    __ InitializeFieldsWithFiller(r1, r5, r6);

    // Check if we can finalize the instance size.
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);
    __ tst(r3, Operand(Map::ConstructionCounter::kMask));
    __ Ret(ne);

    // Finalize the instance size.
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r0, r2);
      __ CallRuntime(Runtime::kFinalizeInstanceSize);
      __ Pop(r0);
    }
    __ Ret();
  }

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    __ mov(r4, Operand(r4, LSL, kPointerSizeLog2 + 1));
    __ Push(r2, r4);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(r2);
  }
  __ ldrb(r5, FieldMemOperand(r2, Map::kInstanceSizeOffset));
  __ add(r5, r0, Operand(r5, LSL, kPointerSizeLog2));
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ sub(r5, r5, Operand(kHeapObjectTag));
  __ b(&done_allocate);

  // Fall back to %NewObject.
  __ bind(&new_object);
  __ Push(r1, r3);
  __ TailCallRuntime(Runtime::kNewObject);
}


void FastNewRestParameterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r1);

  // Make r2 point to the JavaScript frame.
  __ mov(r2, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ ldr(r2, MemOperand(r2, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ ldr(ip, MemOperand(r2, StandardFrameConstants::kFunctionOffset));
    __ cmp(ip, r1);
    __ b(eq, &ok);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ ldr(r2, MemOperand(r2, StandardFrameConstants::kCallerFPOffset));
  __ ldr(ip, MemOperand(r2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmp(ip, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &no_rest_parameters);

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r3,
         FieldMemOperand(r3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ sub(r0, r0, r3, SetCC);
  __ b(gt, &rest_parameters);

  // Return an empty rest parameter array.
  __ bind(&no_rest_parameters);
  {
    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- lr : return address
    // -----------------------------------

    // Allocate an empty rest parameter array.
    Label allocate, done_allocate;
    __ Allocate(JSArray::kSize, r0, r1, r2, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the rest parameter array in r0.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, r1);
    __ str(r1, FieldMemOperand(r0, JSArray::kMapOffset));
    __ LoadRoot(r1, Heap::kEmptyFixedArrayRootIndex);
    __ str(r1, FieldMemOperand(r0, JSArray::kPropertiesOffset));
    __ str(r1, FieldMemOperand(r0, JSArray::kElementsOffset));
    __ mov(r1, Operand(0));
    __ str(r1, FieldMemOperand(r0, JSArray::kLengthOffset));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret();

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(Smi::FromInt(JSArray::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace);
    }
    __ jmp(&done_allocate);
  }

  __ bind(&rest_parameters);
  {
    // Compute the pointer to the first rest parameter (skippping the receiver).
    __ add(r2, r2, Operand(r0, LSL, kPointerSizeLog2 - 1));
    __ add(r2, r2,
           Operand(StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));

    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- r0 : number of rest parameters (tagged)
    //  -- r1 : function
    //  -- r2 : pointer to first rest parameters
    //  -- lr : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ mov(r6, Operand(JSArray::kSize + FixedArray::kHeaderSize));
    __ add(r6, r6, Operand(r0, LSL, kPointerSizeLog2 - 1));
    __ Allocate(r6, r3, r4, r5, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the elements array in r3.
    __ LoadRoot(r1, Heap::kFixedArrayMapRootIndex);
    __ str(r1, FieldMemOperand(r3, FixedArray::kMapOffset));
    __ str(r0, FieldMemOperand(r3, FixedArray::kLengthOffset));
    __ add(r4, r3, Operand(FixedArray::kHeaderSize));
    {
      Label loop, done_loop;
      __ add(r1, r4, Operand(r0, LSL, kPointerSizeLog2 - 1));
      __ bind(&loop);
      __ cmp(r4, r1);
      __ b(eq, &done_loop);
      __ ldr(ip, MemOperand(r2, 1 * kPointerSize, NegPostIndex));
      __ str(ip, FieldMemOperand(r4, 0 * kPointerSize));
      __ add(r4, r4, Operand(1 * kPointerSize));
      __ b(&loop);
      __ bind(&done_loop);
    }

    // Setup the rest parameter array in r4.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, r1);
    __ str(r1, FieldMemOperand(r4, JSArray::kMapOffset));
    __ LoadRoot(r1, Heap::kEmptyFixedArrayRootIndex);
    __ str(r1, FieldMemOperand(r4, JSArray::kPropertiesOffset));
    __ str(r3, FieldMemOperand(r4, JSArray::kElementsOffset));
    __ str(r0, FieldMemOperand(r4, JSArray::kLengthOffset));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ mov(r0, r4);
    __ Ret();

    // Fall back to %AllocateInNewSpace (if not too big).
    Label too_big_for_new_space;
    __ bind(&allocate);
    __ cmp(r6, Operand(kMaxRegularHeapObjectSize));
    __ b(gt, &too_big_for_new_space);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(r6);
      __ Push(r0, r2, r6);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ mov(r3, r0);
      __ Pop(r0, r2);
    }
    __ jmp(&done_allocate);

    // Fall back to %NewRestParameter.
    __ bind(&too_big_for_new_space);
    __ push(r1);
    __ TailCallRuntime(Runtime::kNewRestParameter);
  }
}


void FastNewSloppyArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r1);

  // Make r9 point to the JavaScript frame.
  __ mov(r9, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ ldr(r9, MemOperand(r9, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ ldr(ip, MemOperand(r9, StandardFrameConstants::kFunctionOffset));
    __ cmp(ip, r1);
    __ b(eq, &ok);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2,
         FieldMemOperand(r2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ add(r3, r9, Operand(r2, LSL, kPointerSizeLog2 - 1));
  __ add(r3, r3, Operand(StandardFrameConstants::kCallerSPOffset));

  // r1 : function
  // r2 : number of parameters (tagged)
  // r3 : parameters pointer
  // r9 : JavaScript frame pointer
  // Registers used over whole function:
  //  r5 : arguments count (tagged)
  //  r6 : mapped parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ ldr(r4, MemOperand(r9, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r0, MemOperand(r4, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmp(r0, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adaptor_frame);

  // No adaptor, parameter count = argument count.
  __ mov(r5, r2);
  __ mov(r6, r2);
  __ b(&try_allocate);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ ldr(r5, MemOperand(r4, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ add(r4, r4, Operand(r5, LSL, 1));
  __ add(r3, r4, Operand(StandardFrameConstants::kCallerSPOffset));

  // r5 = argument count (tagged)
  // r6 = parameter count (tagged)
  // Compute the mapped parameter count = min(r6, r5) in r6.
  __ mov(r6, r2);
  __ cmp(r6, Operand(r5));
  __ mov(r6, Operand(r5), LeaveCC, gt);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  __ cmp(r6, Operand(Smi::FromInt(0)));
  __ mov(r9, Operand::Zero(), LeaveCC, eq);
  __ mov(r9, Operand(r6, LSL, 1), LeaveCC, ne);
  __ add(r9, r9, Operand(kParameterMapHeaderSize), LeaveCC, ne);

  // 2. Backing store.
  __ add(r9, r9, Operand(r5, LSL, 1));
  __ add(r9, r9, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ add(r9, r9, Operand(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(r9, r0, r9, r4, &runtime, NO_ALLOCATION_FLAGS);

  // r0 = address of new object(s) (tagged)
  // r2 = argument count (smi-tagged)
  // Get the arguments boilerplate from the current native context into r4.
  const int kNormalOffset =
      Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);

  __ ldr(r4, NativeContextMemOperand());
  __ cmp(r6, Operand::Zero());
  __ ldr(r4, MemOperand(r4, kNormalOffset), eq);
  __ ldr(r4, MemOperand(r4, kAliasedOffset), ne);

  // r0 = address of new object (tagged)
  // r2 = argument count (smi-tagged)
  // r4 = address of arguments map (tagged)
  // r6 = mapped parameter count (tagged)
  __ str(r4, FieldMemOperand(r0, JSObject::kMapOffset));
  __ LoadRoot(r9, Heap::kEmptyFixedArrayRootIndex);
  __ str(r9, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r9, FieldMemOperand(r0, JSObject::kElementsOffset));

  // Set up the callee in-object property.
  __ AssertNotSmi(r1);
  __ str(r1, FieldMemOperand(r0, JSSloppyArgumentsObject::kCalleeOffset));

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(r5);
  __ str(r5, FieldMemOperand(r0, JSSloppyArgumentsObject::kLengthOffset));

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, r4 will point there, otherwise
  // it will point to the backing store.
  __ add(r4, r0, Operand(JSSloppyArgumentsObject::kSize));
  __ str(r4, FieldMemOperand(r0, JSObject::kElementsOffset));

  // r0 = address of new object (tagged)
  // r2 = argument count (tagged)
  // r4 = address of parameter map or backing store (tagged)
  // r6 = mapped parameter count (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ cmp(r6, Operand(Smi::FromInt(0)));
  // Move backing store address to r1, because it is
  // expected there when filling in the unmapped arguments.
  __ mov(r1, r4, LeaveCC, eq);
  __ b(eq, &skip_parameter_map);

  __ LoadRoot(r5, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ str(r5, FieldMemOperand(r4, FixedArray::kMapOffset));
  __ add(r5, r6, Operand(Smi::FromInt(2)));
  __ str(r5, FieldMemOperand(r4, FixedArray::kLengthOffset));
  __ str(cp, FieldMemOperand(r4, FixedArray::kHeaderSize + 0 * kPointerSize));
  __ add(r5, r4, Operand(r6, LSL, 1));
  __ add(r5, r5, Operand(kParameterMapHeaderSize));
  __ str(r5, FieldMemOperand(r4, FixedArray::kHeaderSize + 1 * kPointerSize));

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ mov(r5, r6);
  __ add(r9, r2, Operand(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ sub(r9, r9, Operand(r6));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ add(r1, r4, Operand(r5, LSL, 1));
  __ add(r1, r1, Operand(kParameterMapHeaderSize));

  // r1 = address of backing store (tagged)
  // r4 = address of parameter map (tagged), which is also the address of new
  //      object + Heap::kSloppyArgumentsObjectSize (tagged)
  // r0 = temporary scratch (a.o., for address calculation)
  // r5 = loop variable (tagged)
  // ip = the hole value
  __ jmp(&parameters_test);

  __ bind(&parameters_loop);
  __ sub(r5, r5, Operand(Smi::FromInt(1)));
  __ mov(r0, Operand(r5, LSL, 1));
  __ add(r0, r0, Operand(kParameterMapHeaderSize - kHeapObjectTag));
  __ str(r9, MemOperand(r4, r0));
  __ sub(r0, r0, Operand(kParameterMapHeaderSize - FixedArray::kHeaderSize));
  __ str(ip, MemOperand(r1, r0));
  __ add(r9, r9, Operand(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ cmp(r5, Operand(Smi::FromInt(0)));
  __ b(ne, &parameters_loop);

  // Restore r0 = new object (tagged) and r5 = argument count (tagged).
  __ sub(r0, r4, Operand(JSSloppyArgumentsObject::kSize));
  __ ldr(r5, FieldMemOperand(r0, JSSloppyArgumentsObject::kLengthOffset));

  __ bind(&skip_parameter_map);
  // r0 = address of new object (tagged)
  // r1 = address of backing store (tagged)
  // r5 = argument count (tagged)
  // r6 = mapped parameter count (tagged)
  // r9 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(r9, Heap::kFixedArrayMapRootIndex);
  __ str(r9, FieldMemOperand(r1, FixedArray::kMapOffset));
  __ str(r5, FieldMemOperand(r1, FixedArray::kLengthOffset));

  Label arguments_loop, arguments_test;
  __ sub(r3, r3, Operand(r6, LSL, 1));
  __ jmp(&arguments_test);

  __ bind(&arguments_loop);
  __ sub(r3, r3, Operand(kPointerSize));
  __ ldr(r4, MemOperand(r3, 0));
  __ add(r9, r1, Operand(r6, LSL, 1));
  __ str(r4, FieldMemOperand(r9, FixedArray::kHeaderSize));
  __ add(r6, r6, Operand(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ cmp(r6, Operand(r5));
  __ b(lt, &arguments_loop);

  // Return.
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // r0 = address of new object (tagged)
  // r5 = argument count (tagged)
  __ bind(&runtime);
  __ Push(r1, r3, r5);
  __ TailCallRuntime(Runtime::kNewSloppyArguments);
}


void FastNewStrictArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r1 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r1);

  // Make r2 point to the JavaScript frame.
  __ mov(r2, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ ldr(r2, MemOperand(r2, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ ldr(ip, MemOperand(r2, StandardFrameConstants::kFunctionOffset));
    __ cmp(ip, r1);
    __ b(eq, &ok);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kCallerFPOffset));
  __ ldr(ip, MemOperand(r3, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmp(ip, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &arguments_adaptor);
  {
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r0, FieldMemOperand(
                   r4, SharedFunctionInfo::kFormalParameterCountOffset));
    __ add(r2, r2, Operand(r0, LSL, kPointerSizeLog2 - 1));
    __ add(r2, r2,
           Operand(StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));
  }
  __ b(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    __ ldr(r0, MemOperand(r3, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ add(r2, r3, Operand(r0, LSL, kPointerSizeLog2 - 1));
    __ add(r2, r2,
           Operand(StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));
  }
  __ bind(&arguments_done);

  // ----------- S t a t e -------------
  //  -- cp : context
  //  -- r0 : number of rest parameters (tagged)
  //  -- r1 : function
  //  -- r2 : pointer to first rest parameters
  //  -- lr : return address
  // -----------------------------------

  // Allocate space for the strict arguments object plus the backing store.
  Label allocate, done_allocate;
  __ mov(r6, Operand(JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize));
  __ add(r6, r6, Operand(r0, LSL, kPointerSizeLog2 - 1));
  __ Allocate(r6, r3, r4, r5, &allocate, NO_ALLOCATION_FLAGS);
  __ bind(&done_allocate);

  // Setup the elements array in r3.
  __ LoadRoot(r1, Heap::kFixedArrayMapRootIndex);
  __ str(r1, FieldMemOperand(r3, FixedArray::kMapOffset));
  __ str(r0, FieldMemOperand(r3, FixedArray::kLengthOffset));
  __ add(r4, r3, Operand(FixedArray::kHeaderSize));
  {
    Label loop, done_loop;
    __ add(r1, r4, Operand(r0, LSL, kPointerSizeLog2 - 1));
    __ bind(&loop);
    __ cmp(r4, r1);
    __ b(eq, &done_loop);
    __ ldr(ip, MemOperand(r2, 1 * kPointerSize, NegPostIndex));
    __ str(ip, FieldMemOperand(r4, 0 * kPointerSize));
    __ add(r4, r4, Operand(1 * kPointerSize));
    __ b(&loop);
    __ bind(&done_loop);
  }

  // Setup the strict arguments object in r4.
  __ LoadNativeContextSlot(Context::STRICT_ARGUMENTS_MAP_INDEX, r1);
  __ str(r1, FieldMemOperand(r4, JSStrictArgumentsObject::kMapOffset));
  __ LoadRoot(r1, Heap::kEmptyFixedArrayRootIndex);
  __ str(r1, FieldMemOperand(r4, JSStrictArgumentsObject::kPropertiesOffset));
  __ str(r3, FieldMemOperand(r4, JSStrictArgumentsObject::kElementsOffset));
  __ str(r0, FieldMemOperand(r4, JSStrictArgumentsObject::kLengthOffset));
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ mov(r0, r4);
  __ Ret();

  // Fall back to %AllocateInNewSpace (if not too big).
  Label too_big_for_new_space;
  __ bind(&allocate);
  __ cmp(r6, Operand(kMaxRegularHeapObjectSize));
  __ b(gt, &too_big_for_new_space);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(r6);
    __ Push(r0, r2, r6);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ mov(r3, r0);
    __ Pop(r0, r2);
  }
  __ b(&done_allocate);

  // Fall back to %NewStrictArguments.
  __ bind(&too_big_for_new_space);
  __ push(r1);
  __ TailCallRuntime(Runtime::kNewStrictArguments);
}


void StoreGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register value = r0;
  Register slot = r2;

  Register cell = r1;
  Register cell_details = r4;
  Register cell_value = r5;
  Register cell_value_map = r6;
  Register scratch = r9;

  Register context = cp;
  Register context_temp = cell;

  Label fast_heapobject_case, fast_smi_case, slow_case;

  if (FLAG_debug_code) {
    __ CompareRoot(value, Heap::kTheHoleValueRootIndex);
    __ Check(ne, kUnexpectedValue);
  }

  // Go up the context chain to the script context.
  for (int i = 0; i < depth(); i++) {
    __ ldr(context_temp, ContextMemOperand(context, Context::PREVIOUS_INDEX));
    context = context_temp;
  }

  // Load the PropertyCell at the specified slot.
  __ add(cell, context, Operand(slot, LSL, kPointerSizeLog2));
  __ ldr(cell, ContextMemOperand(cell));

  // Load PropertyDetails for the cell (actually only the cell_type and kind).
  __ ldr(cell_details, FieldMemOperand(cell, PropertyCell::kDetailsOffset));
  __ SmiUntag(cell_details);
  __ and_(cell_details, cell_details,
          Operand(PropertyDetails::PropertyCellTypeField::kMask |
                  PropertyDetails::KindField::kMask |
                  PropertyDetails::kAttributesReadOnlyMask));

  // Check if PropertyCell holds mutable data.
  Label not_mutable_data;
  __ cmp(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                   PropertyCellType::kMutable) |
                               PropertyDetails::KindField::encode(kData)));
  __ b(ne, &not_mutable_data);
  __ JumpIfSmi(value, &fast_smi_case);

  __ bind(&fast_heapobject_case);
  __ str(value, FieldMemOperand(cell, PropertyCell::kValueOffset));
  // RecordWriteField clobbers the value register, so we copy it before the
  // call.
  __ mov(r4, Operand(value));
  __ RecordWriteField(cell, PropertyCell::kValueOffset, r4, scratch,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Ret();

  __ bind(&not_mutable_data);
  // Check if PropertyCell value matches the new value (relevant for Constant,
  // ConstantType and Undefined cells).
  Label not_same_value;
  __ ldr(cell_value, FieldMemOperand(cell, PropertyCell::kValueOffset));
  __ cmp(cell_value, value);
  __ b(ne, &not_same_value);

  // Make sure the PropertyCell is not marked READ_ONLY.
  __ tst(cell_details, Operand(PropertyDetails::kAttributesReadOnlyMask));
  __ b(ne, &slow_case);

  if (FLAG_debug_code) {
    Label done;
    // This can only be true for Constant, ConstantType and Undefined cells,
    // because we never store the_hole via this stub.
    __ cmp(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                     PropertyCellType::kConstant) |
                                 PropertyDetails::KindField::encode(kData)));
    __ b(eq, &done);
    __ cmp(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                     PropertyCellType::kConstantType) |
                                 PropertyDetails::KindField::encode(kData)));
    __ b(eq, &done);
    __ cmp(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                     PropertyCellType::kUndefined) |
                                 PropertyDetails::KindField::encode(kData)));
    __ Check(eq, kUnexpectedValue);
    __ bind(&done);
  }
  __ Ret();
  __ bind(&not_same_value);

  // Check if PropertyCell contains data with constant type (and is not
  // READ_ONLY).
  __ cmp(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                   PropertyCellType::kConstantType) |
                               PropertyDetails::KindField::encode(kData)));
  __ b(ne, &slow_case);

  // Now either both old and new values must be smis or both must be heap
  // objects with same map.
  Label value_is_heap_object;
  __ JumpIfNotSmi(value, &value_is_heap_object);
  __ JumpIfNotSmi(cell_value, &slow_case);
  // Old and new values are smis, no need for a write barrier here.
  __ bind(&fast_smi_case);
  __ str(value, FieldMemOperand(cell, PropertyCell::kValueOffset));
  __ Ret();

  __ bind(&value_is_heap_object);
  __ JumpIfSmi(cell_value, &slow_case);

  __ ldr(cell_value_map, FieldMemOperand(cell_value, HeapObject::kMapOffset));
  __ ldr(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  __ cmp(cell_value_map, scratch);
  __ b(eq, &fast_heapobject_case);

  // Fallback to runtime.
  __ bind(&slow_case);
  __ SmiTag(slot);
  __ Push(slot, value);
  __ TailCallRuntime(is_strict(language_mode())
                         ? Runtime::kStoreGlobalViaContext_Strict
                         : Runtime::kStoreGlobalViaContext_Sloppy);
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

  DCHECK(function_address.is(r1) || function_address.is(r2));

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
    __ PrepareCallCFunction(1, r0);
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
    __ PrepareCallCFunction(1, r0);
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
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ sub(r6, r6, Operand(1));
  __ str(r6, MemOperand(r9, kLevelOffset));
  __ ldr(ip, MemOperand(r9, kLimitOffset));
  __ cmp(r5, ip);
  __ b(ne, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ ldr(cp, *context_restore_operand);
  }
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand != NULL) {
    __ ldr(r4, *stack_space_operand);
  } else {
    __ mov(r4, Operand(stack_space));
  }
  __ LeaveExitFrame(false, r4, !restore_context, stack_space_operand != NULL);

  // Check if the function scheduled an exception.
  __ LoadRoot(r4, Heap::kTheHoleValueRootIndex);
  __ mov(ip, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ ldr(r5, MemOperand(ip));
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
  __ PrepareCallCFunction(1, r5);
  __ mov(r0, Operand(ExternalReference::isolate_address(isolate)));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ mov(r0, r4);
  __ jmp(&leave_exit_frame);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                  : callee
  //  -- r4                  : call_data
  //  -- r2                  : holder
  //  -- r1                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 4]   : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register callee = r0;
  Register call_data = r4;
  Register holder = r2;
  Register api_function_address = r1;
  Register context = cp;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kNewTargetIndex == 7);
  STATIC_ASSERT(FCA::kArgsLength == 8);

  // new target
  __ PushRoot(Heap::kUndefinedValueRootIndex);

  // context save
  __ push(context);
  if (!is_lazy()) {
    // load context from callee
    __ ldr(context, FieldMemOperand(callee, JSFunction::kContextOffset));
  }

  // callee
  __ push(callee);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  if (!call_data_undefined()) {
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
  __ mov(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 3;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(!api_function_address.is(r0) && !scratch.is(r0));
  // r0 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ add(r0, sp, Operand(1 * kPointerSize));
  // FunctionCallbackInfo::implicit_args_
  __ str(scratch, MemOperand(r0, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ add(ip, scratch, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ str(ip, MemOperand(r0, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ mov(ip, Operand(argc()));
  __ str(ip, MemOperand(r0, 2 * kPointerSize));

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
  int stack_space = 0;
  MemOperand length_operand = MemOperand(sp, 3 * kPointerSize);
  MemOperand* stack_space_operand = &length_operand;
  stack_space = argc() + FCA::kArgsLength + 1;
  stack_space_operand = NULL;

  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_operand, return_value_operand,
                           &context_restore_operand);
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
  __ Push(Smi::FromInt(0));  // should_throw_on_error -> false
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
                           kStackUnwindSpace, NULL, return_value_operand, NULL);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
