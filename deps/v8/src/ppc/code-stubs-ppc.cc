// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/code-stubs.h"
#include "src/api-arguments.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/ppc/code-stubs-ppc.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ ShiftLeftImm(r0, r3, Operand(kPointerSizeLog2));
  __ StorePX(r4, MemOperand(sp, r0));
  __ push(r4);
  __ push(r5);
  __ addi(r3, r3, Operand(3));
  __ TailCallRuntime(Runtime::kNewArray);
}

static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cond);
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
                                          Condition cond) {
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
    __ CompareObjectType(r3, r7, r7, FIRST_JS_RECEIVER_TYPE);
    __ bge(slow);
    // Call runtime on identical symbols since we need to throw a TypeError.
    __ cmpi(r7, Operand(SYMBOL_TYPE));
    __ beq(slow);
  } else {
    __ CompareObjectType(r3, r7, r7, HEAP_NUMBER_TYPE);
    __ beq(&heap_number);
    // Comparing JS objects with <=, >= is complicated.
    if (cond != eq) {
      __ cmpi(r7, Operand(FIRST_JS_RECEIVER_TYPE));
      __ bge(slow);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ cmpi(r7, Operand(SYMBOL_TYPE));
      __ beq(slow);
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
        // All-zero means Infinity means equal.
        __ Ret(eq);
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
    if (!rhs.is(r3)) {
      Label skip;
      __ beq(&skip);
      __ mov(r3, Operand(NOT_EQUAL));
      __ Ret();
      __ bind(&skip);
    } else {
      __ Ret(ne);
    }
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
    if (!lhs.is(r3)) {
      Label skip;
      __ beq(&skip);
      __ mov(r3, Operand(NOT_EQUAL));
      __ Ret();
      __ bind(&skip);
    } else {
      __ Ret(ne);
    }
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
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Label first_non_object;
  // Get the type of the first operand into r5 and compare it with
  // FIRST_JS_RECEIVER_TYPE.
  __ CompareObjectType(rhs, r5, r5, FIRST_JS_RECEIVER_TYPE);
  __ blt(&first_non_object);

  // Return non-zero (r3 is not zero)
  Label return_not_equal;
  __ bind(&return_not_equal);
  __ Ret();

  __ bind(&first_non_object);
  // Check for oddballs: true, false, null, undefined.
  __ cmpi(r5, Operand(ODDBALL_TYPE));
  __ beq(&return_not_equal);

  __ CompareObjectType(lhs, r6, r6, FIRST_JS_RECEIVER_TYPE);
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

// Fast negative check for internalized-to-internalized equality or receiver
// equality. Also handles the undetectable receiver to null/undefined
// comparison.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register lhs, Register rhs,
                                                     Label* possible_strings,
                                                     Label* runtime_call) {
  DCHECK((lhs.is(r3) && rhs.is(r4)) || (lhs.is(r4) && rhs.is(r3)));

  // r5 is object type of rhs.
  Label object_test, return_equal, return_unequal, undetectable;
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ andi(r0, r5, Operand(kIsNotStringMask));
  __ bne(&object_test, cr0);
  __ andi(r0, r5, Operand(kIsNotInternalizedMask));
  __ bne(possible_strings, cr0);
  __ CompareObjectType(lhs, r6, r6, FIRST_NONSTRING_TYPE);
  __ bge(runtime_call);
  __ andi(r0, r6, Operand(kIsNotInternalizedMask));
  __ bne(possible_strings, cr0);

  // Both are internalized. We already checked they weren't the same pointer so
  // they are not equal. Return non-equal by returning the non-zero object
  // pointer in r3.
  __ Ret();

  __ bind(&object_test);
  __ LoadP(r5, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ LoadP(r6, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ lbz(r7, FieldMemOperand(r5, Map::kBitFieldOffset));
  __ lbz(r8, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ andi(r0, r7, Operand(1 << Map::kIsUndetectable));
  __ bne(&undetectable, cr0);
  __ andi(r0, r8, Operand(1 << Map::kIsUndetectable));
  __ bne(&return_unequal, cr0);

  __ CompareInstanceType(r5, r5, FIRST_JS_RECEIVER_TYPE);
  __ blt(runtime_call);
  __ CompareInstanceType(r6, r6, FIRST_JS_RECEIVER_TYPE);
  __ blt(runtime_call);

  __ bind(&return_unequal);
  // Return non-equal by returning the non-zero object pointer in r3.
  __ Ret();

  __ bind(&undetectable);
  __ andi(r0, r8, Operand(1 << Map::kIsUndetectable));
  __ beq(&return_unequal, cr0);

  // If both sides are JSReceivers, then the result is false according to
  // the HTML specification, which says that only comparisons with null or
  // undefined are affected by special casing for document.all.
  __ CompareInstanceType(r5, r5, ODDBALL_TYPE);
  __ beq(&return_equal);
  __ CompareInstanceType(r6, r6, ODDBALL_TYPE);
  __ bne(&return_unequal);

  __ bind(&return_equal);
  __ li(r3, Operand(EQUAL));
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
  EmitIdenticalObjectComparison(masm, &slow, cc);

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK_EQ(static_cast<Smi*>(0), Smi::kZero);
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

  if (cc == eq) {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(cp);
      __ Call(strict() ? isolate()->builtins()->StrictEqual()
                       : isolate()->builtins()->Equal(),
              RelocInfo::CODE_TARGET);
      __ Pop(cp);
    }
    // Turn true into 0 and false into some non-zero value.
    STATIC_ASSERT(EQUAL == 0);
    __ LoadRoot(r4, Heap::kTrueValueRootIndex);
    __ sub(r3, r3, r4);
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
    __ LoadSmiLiteral(r3, Smi::FromInt(ncr));
    __ push(r3);

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
  __ mflr(r0);
  __ MultiPush(kJSCallerSaved | r0.bit());
  if (save_doubles()) {
    __ MultiPushDoubles(kCallerSavedDoubles);
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
    __ MultiPopDoubles(kCallerSavedDoubles);
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
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(r5));
  const DoubleRegister double_base = d1;
  const DoubleRegister double_exponent = d2;
  const DoubleRegister double_result = d3;
  const DoubleRegister double_scratch = d0;
  const Register scratch = r11;
  const Register scratch2 = r10;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == TAGGED) {
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
  __ ShiftRightImm(scratch, scratch, Operand(1), SetRC);
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
  __ Ret();
}


bool CEntryStub::NeedsImmovableCode() { return true; }


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  CommonArrayConstructorStub::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  CreateWeakCellStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  StoreRegistersStateStub::GenerateAheadOfTime(isolate);
  RestoreRegistersStateStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
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
  //
  // If argv_in_register():
  // r5: pointer to the first argument
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  __ mr(r15, r4);

  if (argv_in_register()) {
    // Move argv into the correct register.
    __ mr(r4, r5);
  } else {
    // Compute the argv pointer.
    __ ShiftLeftImm(r4, r3, Operand(kPointerSizeLog2));
    __ add(r4, r4, sp);
    __ subi(r4, r4, Operand(kPointerSize));
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

  // Pass buffer for return value on stack if necessary
  bool needs_return_buffer =
      result_size() > 2 ||
      (result_size() == 2 && !ABI_RETURNS_OBJECT_PAIRS_IN_REGS);
  if (needs_return_buffer) {
    arg_stack_space += result_size();
  }

  __ EnterExitFrame(save_doubles(), arg_stack_space, is_builtin_exit()
                                           ? StackFrame::BUILTIN_EXIT
                                           : StackFrame::EXIT);

  // Store a copy of argc in callee-saved registers for later.
  __ mr(r14, r3);

  // r3, r14: number of arguments including receiver  (C callee-saved)
  // r4: pointer to the first argument
  // r15: pointer to builtin function  (C callee-saved)

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r5;
  if (needs_return_buffer) {
    // The return value is a non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument.
    __ mr(r5, r4);
    __ mr(r4, r3);
    __ addi(r3, sp, Operand((kStackFrameExtraParamSlot + 1) * kPointerSize));
    isolate_reg = r6;
  }

  // Call C built-in.
  __ mov(isolate_reg, Operand(ExternalReference::isolate_address(isolate())));

  Register target = r15;
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX/PPC64BE Linux use a function descriptor.
    __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(r15, kPointerSize));
    __ LoadP(ip, MemOperand(r15, 0));  // Instruction address
    target = ip;
  } else if (ABI_CALL_VIA_IP) {
    __ Move(ip, r15);
    target = ip;
  }

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  Label after_call;
  __ mov_label_addr(r0, &after_call);
  __ StoreP(r0, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  __ Call(target);
  __ bind(&after_call);

  // If return value is on the stack, pop it to registers.
  if (needs_return_buffer) {
    if (result_size() > 2) __ LoadP(r5, MemOperand(r3, 2 * kPointerSize));
    __ LoadP(r4, MemOperand(r3, kPointerSize));
    __ LoadP(r3, MemOperand(r3));
  }

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

    __ mov(r6, Operand(pending_exception_address));
    __ LoadP(r6, MemOperand(r6));
    __ CompareRoot(r6, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay);
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r3:r4: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc;
  if (argv_in_register()) {
    // We don't want to pop arguments so set argc to no_reg.
    argc = no_reg;
  } else {
    // r14: still holds argc (callee-saved).
    argc = r14;
  }
  __ LeaveExitFrame(save_doubles(), argc, true);
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

  // Save callee-saved double registers.
  __ MultiPushDoubles(kCalleeSavedDoubles);
  // Set up the reserved register for 0.0.
  __ LoadDoubleLiteral(kDoubleRegZero, 0.0, r0);

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
  StackFrame::Type marker = type();
  __ mov(r0, Operand(StackFrame::TypeToMarker(marker)));
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
  __ mov(ip, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ bind(&non_outermost_js);
  __ mov(ip, Operand(StackFrame::INNER_JSENTRY_FRAME));
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
  __ cmpi(r8, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
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

  // Restore callee-saved double registers.
  __ MultiPopDoubles(kCalleeSavedDoubles);

  // Restore callee-saved registers.
  __ MultiPop(kCalleeSaved);

  // Return
  __ LoadP(r0, MemOperand(sp, kStackFrameLRSlot * kPointerSize));
  __ mtlr(r0);
  __ blr();
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // r3 : number of arguments to the construct function
  // r4 : the function to call
  // r5 : feedback vector
  // r6 : slot in feedback vector (Smi)
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

  // Number-of-arguments register must be smi-tagged to call out.
  __ SmiTag(r3);
  __ Push(r6, r5, r4, r3);
  __ Push(cp);

  __ CallStub(stub);

  __ Pop(cp);
  __ Pop(r6, r5, r4, r3);
  __ SmiUntag(r3);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // r3 : number of arguments to the construct function
  // r4 : the function to call
  // r5 : feedback vector
  // r6 : slot in feedback vector (Smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*FeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*FeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  const int count_offset = FixedArray::kHeaderSize + kPointerSize;

  // Load the cache state into r8.
  __ SmiToPtrArrayOffset(r8, r6);
  __ add(r8, r5, r8);
  __ LoadP(r8, FieldMemOperand(r8, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if r8 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = r9;
  Register weak_value = r10;
  __ LoadP(weak_value, FieldMemOperand(r8, WeakCell::kValueOffset));
  __ cmp(r4, weak_value);
  __ beq(&done);
  __ CompareRoot(r8, Heap::kmegamorphic_symbolRootIndex);
  __ beq(&done);
  __ LoadP(feedback_map, FieldMemOperand(r8, HeapObject::kMapOffset));
  __ CompareRoot(feedback_map, Heap::kWeakCellMapRootIndex);
  __ bne(&check_allocation_site);

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ JumpIfSmi(weak_value, &initialize);
  __ b(&megamorphic);

  __ bind(&check_allocation_site);
  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the slot either some other function or an
  // AllocationSite.
  __ CompareRoot(feedback_map, Heap::kAllocationSiteMapRootIndex);
  __ bne(&miss);

  // Make sure the function is the Array() function
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r8);
  __ cmp(r4, r8);
  __ bne(&megamorphic);
  __ b(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(r8, Heap::kuninitialized_symbolRootIndex);
  __ beq(&initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ SmiToPtrArrayOffset(r8, r6);
  __ add(r8, r5, r8);
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ StoreP(ip, FieldMemOperand(r8, FixedArray::kHeaderSize), r0);
  __ jmp(&done);

  // An uninitialized cache is patched with the function
  __ bind(&initialize);

  // Make sure the function is the Array() function.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r8);
  __ cmp(r4, r8);
  __ bne(&not_array_function);

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
  __ SmiToPtrArrayOffset(r8, r6);
  __ add(r8, r5, r8);

  __ LoadP(r7, FieldMemOperand(r8, count_offset));
  __ AddSmiLiteral(r7, r7, Smi::FromInt(1), r0);
  __ StoreP(r7, FieldMemOperand(r8, count_offset), r0);
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // r3 : number of arguments
  // r4 : the function to call
  // r5 : feedback vector
  // r6 : slot in feedback vector (Smi, for RecordCallTarget)

  Label non_function;
  // Check that the function is not a smi.
  __ JumpIfSmi(r4, &non_function);
  // Check that the function is a JSFunction.
  __ CompareObjectType(r4, r8, r8, JS_FUNCTION_TYPE);
  __ bne(&non_function);

  GenerateRecordCallTarget(masm);

  __ SmiToPtrArrayOffset(r8, r6);
  __ add(r8, r5, r8);
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

  __ AssertUndefinedOrAllocationSite(r5, r8);

  // Pass function as new target.
  __ mr(r6, r4);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kConstructStubOffset));
  __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);

  __ bind(&non_function);
  __ mr(r6, r4);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
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
  __ CallRuntime(Runtime::kNumberToSmi);
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
  __ CallRuntime(Runtime::kStringCharCodeAtRT);
  __ Move(result_, r3);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
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


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r4    : left
  //  -- r3    : right
  //  -- lr    : return address
  // -----------------------------------

  // Load r5 with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ Move(r5, isolate()->factory()->undefined_value());

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


void CompareICStub::GenerateBooleans(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::BOOLEAN, state());
  Label miss;

  __ CheckMap(r4, r5, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  __ CheckMap(r3, r6, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  if (!Token::IsEqualityOp(op())) {
    __ LoadP(r4, FieldMemOperand(r4, Oddball::kToNumberOffset));
    __ AssertSmi(r4);
    __ LoadP(r3, FieldMemOperand(r3, Oddball::kToNumberOffset));
    __ AssertSmi(r3);
  }
  __ sub(r3, r4, r3);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
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
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
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
    // Make sure r3 is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(r3));
    __ Ret(eq, cr0);
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
  if (equality) {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(left, right);
      __ CallRuntime(Runtime::kStringEqual);
    }
    __ LoadRoot(r4, Heap::kTrueValueRootIndex);
    __ sub(r3, r3, r4);
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
  __ and_(r5, r4, r3);
  __ JumpIfSmi(r5, &miss);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ CompareObjectType(r3, r5, r5, FIRST_JS_RECEIVER_TYPE);
  __ blt(&miss);
  __ CompareObjectType(r4, r5, r5, FIRST_JS_RECEIVER_TYPE);
  __ blt(&miss);

  DCHECK(GetCondition() == eq);
  __ sub(r3, r3, r4);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
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

  if (Token::IsEqualityOp(op())) {
    __ sub(r3, r3, r4);
    __ Ret();
  } else {
    if (op() == Token::LT || op() == Token::LTE) {
      __ LoadSmiLiteral(r5, Smi::FromInt(GREATER));
    } else {
      __ LoadSmiLiteral(r5, Smi::FromInt(LESS));
    }
    __ Push(r4, r3, r5);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4, r3);
    __ Push(r4, r3);
    __ LoadSmiLiteral(r0, Smi::FromInt(op()));
    __ push(r0);
    __ CallRuntime(Runtime::kCompareIC_Miss);
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
  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    // AIX/PPC64BE Linux use a function descriptor.
    __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(target, kPointerSize));
    __ LoadP(ip, MemOperand(target, 0));  // Instruction address
  } else {
    // ip needs to be set for DirectCEentryStub::Generate, and also
    // for ABI_CALL_VIA_IP.
    __ Move(ip, target);
  }

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
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
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
  __ LoadP(r4, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ addi(r4, r4, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ slwi(r4, r4, Operand(kPointerSizeLog2));
  __ add(sp, sp, r4);
  __ Ret();
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
#else
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  ExternalReference entry_hook = ExternalReference(
      &dispatcher, ExternalReference::BUILTIN_CALL, isolate());

  // It additionally takes an isolate as a third parameter
  __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));
#endif

  __ mov(ip, Operand(entry_hook));

  if (ABI_USES_FUNCTION_DESCRIPTORS) {
    __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(ip, kPointerSize));
    __ LoadP(ip, MemOperand(ip, 0));
  }
  // ip set above, so nothing more to do for ABI_CALL_VIA_IP.

  // PPC LINUX ABI:
  __ li(r0, Operand::Zero());
  __ StorePU(r0, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));

  __ Call(ip);

  __ addi(sp, sp, Operand(kNumRequiredStackFrameSlots * kPointerSize));

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
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

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


void CommonArrayConstructorStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayNArgumentsConstructorStub stub(isolate);
  stub.GetCode();
  ElementsKind kinds[2] = {FAST_ELEMENTS, FAST_HOLEY_ELEMENTS};
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
  __ cmpi(r3, Operand::Zero());
  __ bne(&not_zero_case);
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ bind(&not_zero_case);
  __ cmpi(r3, Operand(1));
  __ bgt(&not_one_case);
  CreateArrayDispatchOneArgument(masm, mode);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : argc (only if argument_count() == ANY)
  //  -- r4 : constructor
  //  -- r5 : AllocationSite or undefined
  //  -- r6 : new target
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

  // Enter the context of the Array function.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

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
  __ ShiftLeftImm(r0, r3, Operand(kPointerSizeLog2));
  __ StorePX(r4, MemOperand(sp, r0));
  __ addi(r3, r3, Operand(3));
  __ Push(r6, r5);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(MacroAssembler* masm,
                                                ElementsKind kind) {
  __ cmpli(r3, Operand(1));

  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0, lt);

  ArrayNArgumentsConstructorStub stubN(isolate());
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
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

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

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                  : callee
  //  -- r7                  : call_data
  //  -- r5                  : holder
  //  -- r4                  : api_function_address
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
  STATIC_ASSERT(FCA::kNewTargetIndex == 7);
  STATIC_ASSERT(FCA::kArgsLength == 8);

  // new target
  __ PushRoot(Heap::kUndefinedValueRootIndex);

  // context save
  __ push(context);
  if (!is_lazy()) {
    // load context from callee
    __ LoadP(context, FieldMemOperand(callee, JSFunction::kContextOffset));
  }

  // callee
  __ push(callee);

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
  __ mr(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // PPC LINUX ABI:
  //
  // Create 4 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-3] FunctionCallbackInfo
  const int kApiStackSpace = 4;
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
  // FunctionCallbackInfo::values_
  __ addi(ip, scratch, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ StoreP(ip, MemOperand(r3, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ li(ip, Operand(argc()));
  __ stw(ip, MemOperand(r3, 2 * kPointerSize));

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
  MemOperand length_operand =
      MemOperand(sp, kFunctionCallbackInfoOffset + 2 * kPointerSize);
  MemOperand* stack_space_operand = &length_operand;
  stack_space = argc() + FCA::kArgsLength + 1;
  stack_space_operand = NULL;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_operand, return_value_operand,
                           &context_restore_operand);
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
  Register scratch = r7;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r5;

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
  __ mr(r3, sp);                               // r3 = Handle<Name>
  __ addi(r4, r3, Operand(1 * kPointerSize));  // r4 = v8::PCI::args_

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
    __ StoreP(r3, MemOperand(sp, arg0Slot * kPointerSize));
    __ addi(r3, sp, Operand(arg0Slot * kPointerSize));
  }

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ StoreP(r4, MemOperand(sp, accessorInfoSlot * kPointerSize));
  __ addi(r4, sp, Operand(accessorInfoSlot * kPointerSize));
  // r4 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  __ LoadP(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ LoadP(api_function_address,
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

#endif  // V8_TARGET_ARCH_PPC
