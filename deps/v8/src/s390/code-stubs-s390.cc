// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

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
#include "src/s390/code-stubs-s390.h"

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

void FastArrayPushStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  Address deopt_handler = Runtime::FunctionForId(Runtime::kArrayPush)->entry;
  descriptor->Initialize(r2, deopt_handler, -1, JS_FUNCTION_STUB_MODE);
}

void FastFunctionBindStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  Address deopt_handler = Runtime::FunctionForId(Runtime::kFunctionBind)->entry;
  descriptor->Initialize(r2, deopt_handler, -1, JS_FUNCTION_STUB_MODE);
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
    FrameScope scope(masm, StackFrame::INTERNAL);
    DCHECK(param_count == 0 ||
           r2.is(descriptor.GetRegisterParameter(param_count - 1)));
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
    __ LoadDouble(double_scratch, MemOperand(input_reg, double_offset));

    // Do fast-path convert from double to int.
    __ ConvertDoubleToInt64(double_scratch,
#if !V8_TARGET_ARCH_S390X
                            scratch,
#endif
                            result_reg, d0);

// Test for overflow
#if V8_TARGET_ARCH_S390X
    __ TestIfInt32(result_reg, r0);
#else
    __ TestIfInt32(scratch, result_reg, r0);
#endif
    __ beq(&fastpath_done, Label::kNear);
  }

  __ Push(scratch_high, scratch_low);
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += 2 * kPointerSize;

  __ LoadlW(scratch_high,
            MemOperand(input_reg, double_offset + Register::kExponentOffset));
  __ LoadlW(scratch_low,
            MemOperand(input_reg, double_offset + Register::kMantissaOffset));

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
  // Input_high ASR 31 equals 0xffffffff and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xffffffff) + 1 = 0 - result.
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

// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cond) {
  Label not_identical;
  Label heap_number, return_equal;
  __ CmpP(r2, r3);
  __ bne(&not_identical);

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis.  If it's not a heap number, then return equal.
  if (cond == lt || cond == gt) {
    // Call runtime on identical JSObjects.
    __ CompareObjectType(r2, r6, r6, FIRST_JS_RECEIVER_TYPE);
    __ bge(slow);
    // Call runtime on identical symbols since we need to throw a TypeError.
    __ CmpP(r6, Operand(SYMBOL_TYPE));
    __ beq(slow);
    // Call runtime on identical SIMD values since we must throw a TypeError.
    __ CmpP(r6, Operand(SIMD128_VALUE_TYPE));
    __ beq(slow);
  } else {
    __ CompareObjectType(r2, r6, r6, HEAP_NUMBER_TYPE);
    __ beq(&heap_number);
    // Comparing JS objects with <=, >= is complicated.
    if (cond != eq) {
      __ CmpP(r6, Operand(FIRST_JS_RECEIVER_TYPE));
      __ bge(slow);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ CmpP(r6, Operand(SYMBOL_TYPE));
      __ beq(slow);
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ CmpP(r6, Operand(SIMD128_VALUE_TYPE));
      __ beq(slow);
      // Normally here we fall through to return_equal, but undefined is
      // special: (undefined == undefined) == true, but
      // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
      if (cond == le || cond == ge) {
        __ CmpP(r6, Operand(ODDBALL_TYPE));
        __ bne(&return_equal);
        __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
        __ bne(&return_equal);
        if (cond == le) {
          // undefined <= undefined should fail.
          __ LoadImmP(r2, Operand(GREATER));
        } else {
          // undefined >= undefined should fail.
          __ LoadImmP(r2, Operand(LESS));
        }
        __ Ret();
      }
    }
  }

  __ bind(&return_equal);
  if (cond == lt) {
    __ LoadImmP(r2, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cond == gt) {
    __ LoadImmP(r2, Operand(LESS));  // Things aren't greater than themselves.
  } else {
    __ LoadImmP(r2, Operand(EQUAL));  // Things are <=, >=, ==, === themselves
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
    __ LoadlW(r4, FieldMemOperand(r2, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    STATIC_ASSERT(HeapNumber::kExponentMask == 0x7ff00000u);
    __ ExtractBitMask(r5, r4, HeapNumber::kExponentMask);
    __ CmpLogicalP(r5, Operand(0x7ff));
    __ bne(&return_equal);

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ sll(r4, Operand(HeapNumber::kNonMantissaBitsInTopWord));
    // Or with all low-bits of mantissa.
    __ LoadlW(r5, FieldMemOperand(r2, HeapNumber::kMantissaOffset));
    __ OrP(r2, r5, r4);
    __ CmpP(r2, Operand::Zero());
    // For equal we already have the right value in r2:  Return zero (equal)
    // if all bits in mantissa are zero (it's an Infinity) and non-zero if
    // not (it's a NaN).  For <= and >= we need to load r0 with the failing
    // value if it's a NaN.
    if (cond != eq) {
      Label not_equal;
      __ bne(&not_equal, Label::kNear);
      // All-zero means Infinity means equal.
      __ Ret();
      __ bind(&not_equal);
      if (cond == le) {
        __ LoadImmP(r2, Operand(GREATER));  // NaN <= NaN should fail.
      } else {
        __ LoadImmP(r2, Operand(LESS));  // NaN >= NaN should fail.
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
  DCHECK((lhs.is(r2) && rhs.is(r3)) || (lhs.is(r3) && rhs.is(r2)));

  Label rhs_is_smi;
  __ JumpIfSmi(rhs, &rhs_is_smi);

  // Lhs is a Smi.  Check whether the rhs is a heap number.
  __ CompareObjectType(rhs, r5, r6, HEAP_NUMBER_TYPE);
  if (strict) {
    // If rhs is not a number and lhs is a Smi then strict equality cannot
    // succeed.  Return non-equal
    // If rhs is r2 then there is already a non zero value in it.
    Label skip;
    __ beq(&skip, Label::kNear);
    if (!rhs.is(r2)) {
      __ mov(r2, Operand(NOT_EQUAL));
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
  // Load the double from rhs, tagged HeapNumber r2, to d6.
  __ LoadDouble(d6, FieldMemOperand(rhs, HeapNumber::kValueOffset));

  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a smi.
  __ b(lhs_not_nan);

  __ bind(&rhs_is_smi);
  // Rhs is a smi.  Check whether the non-smi lhs is a heap number.
  __ CompareObjectType(lhs, r6, r6, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs is not a number and rhs is a smi then strict equality cannot
    // succeed.  Return non-equal.
    // If lhs is r2 then there is already a non zero value in it.
    Label skip;
    __ beq(&skip, Label::kNear);
    if (!lhs.is(r2)) {
      __ mov(r2, Operand(NOT_EQUAL));
    }
    __ Ret();
    __ bind(&skip);
  } else {
    // Smi compared non-strictly with a non-smi non-heap-number.  Call
    // the runtime.
    __ bne(slow);
  }

  // Rhs is a smi, lhs is a heap number.
  // Load the double from lhs, tagged HeapNumber r3, to d7.
  __ LoadDouble(d7, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  // Convert rhs to a double in d6.
  __ SmiToDouble(d6, rhs);
  // Fall through to both_loaded_as_doubles.
}

// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm, Register lhs,
                                           Register rhs) {
  DCHECK((lhs.is(r2) && rhs.is(r3)) || (lhs.is(r3) && rhs.is(r2)));

  // If either operand is a JS object or an oddball value, then they are
  // not equal since their pointers are different.
  // There is no test for undetectability in strict equality.
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Label first_non_object;
  // Get the type of the first operand into r4 and compare it with
  // FIRST_JS_RECEIVER_TYPE.
  __ CompareObjectType(rhs, r4, r4, FIRST_JS_RECEIVER_TYPE);
  __ blt(&first_non_object, Label::kNear);

  // Return non-zero (r2 is not zero)
  Label return_not_equal;
  __ bind(&return_not_equal);
  __ Ret();

  __ bind(&first_non_object);
  // Check for oddballs: true, false, null, undefined.
  __ CmpP(r4, Operand(ODDBALL_TYPE));
  __ beq(&return_not_equal);

  __ CompareObjectType(lhs, r5, r5, FIRST_JS_RECEIVER_TYPE);
  __ bge(&return_not_equal);

  // Check for oddballs: true, false, null, undefined.
  __ CmpP(r5, Operand(ODDBALL_TYPE));
  __ beq(&return_not_equal);

  // Now that we have the types we might as well check for
  // internalized-internalized.
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ OrP(r4, r4, r5);
  __ AndP(r0, r4, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ beq(&return_not_equal);
}

// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm, Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers, Label* slow) {
  DCHECK((lhs.is(r2) && rhs.is(r3)) || (lhs.is(r3) && rhs.is(r2)));

  __ CompareObjectType(rhs, r5, r4, HEAP_NUMBER_TYPE);
  __ bne(not_heap_numbers);
  __ LoadP(r4, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ CmpP(r4, r5);
  __ bne(slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  __ LoadDouble(d6, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  __ LoadDouble(d7, FieldMemOperand(lhs, HeapNumber::kValueOffset));

  __ b(both_loaded_as_doubles);
}

// Fast negative check for internalized-to-internalized equality or receiver
// equality. Also handles the undetectable receiver to null/undefined
// comparison.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register lhs, Register rhs,
                                                     Label* possible_strings,
                                                     Label* runtime_call) {
  DCHECK((lhs.is(r2) && rhs.is(r3)) || (lhs.is(r3) && rhs.is(r2)));

  // r4 is object type of rhs.
  Label object_test, return_equal, return_unequal, undetectable;
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ mov(r0, Operand(kIsNotStringMask));
  __ AndP(r0, r4);
  __ bne(&object_test, Label::kNear);
  __ mov(r0, Operand(kIsNotInternalizedMask));
  __ AndP(r0, r4);
  __ bne(possible_strings);
  __ CompareObjectType(lhs, r5, r5, FIRST_NONSTRING_TYPE);
  __ bge(runtime_call);
  __ mov(r0, Operand(kIsNotInternalizedMask));
  __ AndP(r0, r5);
  __ bne(possible_strings);

  // Both are internalized. We already checked they weren't the same pointer so
  // they are not equal. Return non-equal by returning the non-zero object
  // pointer in r2.
  __ Ret();

  __ bind(&object_test);
  __ LoadP(r4, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ LoadP(r5, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ LoadlB(r6, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ LoadlB(r7, FieldMemOperand(r5, Map::kBitFieldOffset));
  __ AndP(r0, r6, Operand(1 << Map::kIsUndetectable));
  __ bne(&undetectable);
  __ AndP(r0, r7, Operand(1 << Map::kIsUndetectable));
  __ bne(&return_unequal);

  __ CompareInstanceType(r4, r4, FIRST_JS_RECEIVER_TYPE);
  __ blt(runtime_call);
  __ CompareInstanceType(r5, r5, FIRST_JS_RECEIVER_TYPE);
  __ blt(runtime_call);

  __ bind(&return_unequal);
  // Return non-equal by returning the non-zero object pointer in r2.
  __ Ret();

  __ bind(&undetectable);
  __ AndP(r0, r7, Operand(1 << Map::kIsUndetectable));
  __ beq(&return_unequal);

  // If both sides are JSReceivers, then the result is false according to
  // the HTML specification, which says that only comparisons with null or
  // undefined are affected by special casing for document.all.
  __ CompareInstanceType(r4, r4, ODDBALL_TYPE);
  __ beq(&return_equal);
  __ CompareInstanceType(r5, r5, ODDBALL_TYPE);
  __ bne(&return_unequal);

  __ bind(&return_equal);
  __ LoadImmP(r2, Operand(EQUAL));
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

// On entry r3 and r4 are the values to be compared.
// On exit r2 is 0, positive or negative to indicate the result of
// the comparison.
void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Register lhs = r3;
  Register rhs = r2;
  Condition cc = GetCondition();

  Label miss;
  CompareICStub_CheckInputType(masm, lhs, r4, left(), &miss);
  CompareICStub_CheckInputType(masm, rhs, r5, right(), &miss);

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, lhs_not_nan;

  Label not_two_smis, smi_done;
  __ OrP(r4, r3, r2);
  __ JumpIfNotSmi(r4, &not_two_smis);
  __ SmiUntag(r3);
  __ SmiUntag(r2);
  __ SubP(r2, r3, r2);
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
  __ AndP(r4, lhs, rhs);
  __ JumpIfNotSmi(r4, &not_smis);
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
  __ cdbr(d7, d6);

  Label nan, equal, less_than;
  __ bunordered(&nan);
  __ beq(&equal, Label::kNear);
  __ blt(&less_than, Label::kNear);
  __ LoadImmP(r2, Operand(GREATER));
  __ Ret();
  __ bind(&equal);
  __ LoadImmP(r2, Operand(EQUAL));
  __ Ret();
  __ bind(&less_than);
  __ LoadImmP(r2, Operand(LESS));
  __ Ret();

  __ bind(&nan);
  // If one of the sides was a NaN then the v flag is set.  Load r2 with
  // whatever it takes to make the comparison fail, since comparisons with NaN
  // always fail.
  if (cc == lt || cc == le) {
    __ LoadImmP(r2, Operand(GREATER));
  } else {
    __ LoadImmP(r2, Operand(LESS));
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
  // or load both doubles into r2, r3, r4, r5 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to
  // check_for_internalized_strings.
  // In this case r4 will contain the type of rhs_.  Never falls through.
  EmitCheckForTwoHeapNumbers(masm, lhs, rhs, &both_loaded_as_doubles,
                             &check_for_internalized_strings,
                             &flat_string_check);

  __ bind(&check_for_internalized_strings);
  // In the strict case the EmitStrictTwoHeapObjectCompare already took care of
  // internalized strings.
  if (cc == eq && !strict()) {
    // Returns an answer for two internalized strings or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that r4 is the type of rhs_ on entry.
    EmitCheckForInternalizedStringsOrObjects(masm, lhs, rhs, &flat_string_check,
                                             &slow);
  }

  // Check for both being sequential one-byte strings,
  // and inline if that is the case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialOneByteStrings(lhs, rhs, r4, r5, &slow);

  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, r4,
                      r5);
  if (cc == eq) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, r4, r5);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, r4, r5, r6);
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
    __ LoadRoot(r3, Heap::kTrueValueRootIndex);
    __ SubP(r2, r2, r3);
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
    __ LoadSmiLiteral(r2, Smi::FromInt(ncr));
    __ push(r2);

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
  __ MultiPush(kJSCallerSaved | r14.bit());
  if (save_doubles()) {
    __ MultiPushDoubles(kCallerSavedDoubles);
  }
  const int argument_count = 1;
  const int fp_argument_count = 0;
  const Register scratch = r3;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, fp_argument_count, scratch);
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
  __ CallCFunction(ExternalReference::store_buffer_overflow_function(isolate()),
                   argument_count);
  if (save_doubles()) {
    __ MultiPopDoubles(kCallerSavedDoubles);
  }
  __ MultiPop(kJSCallerSaved | r14.bit());
  __ Ret();
}

void StoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ PushSafepointRegisters();
  __ b(r14);
}

void RestoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ PopSafepointRegisters();
  __ b(r14);
}

void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(r4));
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
  __ ConvertIntToDouble(scratch2, double_result);

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
  __ ConvertIntToDouble(scratch2, double_result);
  __ ddbr(double_result, double_scratch);

  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ lzdr(kDoubleRegZero);
  __ cdbr(double_result, kDoubleRegZero);
  __ bne(&done, Label::kNear);
  // double_exponent may not containe the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ ConvertIntToDouble(exponent, double_exponent);

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
      result_size() > 2 ||
      (result_size() == 2 && !ABI_RETURNS_OBJECTPAIR_IN_REGS);
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
    if (result_size() > 2) __ LoadP(r4, MemOperand(r2, 2 * kPointerSize));
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
        Isolate::kPendingExceptionAddress, isolate());
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
  Register argc;
  if (argv_in_register()) {
    // We don't want to pop arguments so set argc to no_reg.
    argc = no_reg;
  } else {
    // r6: still holds argc (callee-saved).
    argc = r6;
  }
  __ LeaveExitFrame(save_doubles(), argc, true);
  __ b(r14);

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

  // Compute the handler entry address and jump to it.
  __ mov(r3, Operand(pending_handler_code_address));
  __ LoadP(r3, MemOperand(r3));
  __ mov(r4, Operand(pending_handler_offset_address));
  __ LoadP(r4, MemOperand(r4));
  __ AddP(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start
  __ AddP(ip, r3, r4);
  __ Jump(ip);
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

  int marker = type();
  __ LoadSmiLiteral(r9, Smi::FromInt(marker));
  __ LoadSmiLiteral(r8, Smi::FromInt(marker));
  // Save copies of the top frame descriptor on the stack.
  __ mov(r7, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ LoadP(r7, MemOperand(r7));
  __ StoreMultipleP(r7, r10, MemOperand(sp, kPointerSize));
  // Set up frame pointer for the frame to be pushed.
  // Need to add kPointerSize, because sp has one extra
  // frame already for the frame type being pushed later.
  __ lay(fp,
         MemOperand(sp, -EntryFrameConstants::kCallerFPOffset + kPointerSize));

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ mov(r7, Operand(ExternalReference(js_entry_sp)));
  __ LoadAndTestP(r8, MemOperand(r7));
  __ bne(&non_outermost_js, Label::kNear);
  __ StoreP(fp, MemOperand(r7));
  __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont, Label::kNear);
  __ bind(&non_outermost_js);
  __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME));

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
  __ mov(ip, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));

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
  __ AddP(ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  Label return_addr;
  // __ basr(r14, ip);
  __ larl(r14, &return_addr);
  __ b(ip);
  __ bind(&return_addr);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // r2 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r7);
  __ CmpSmiLiteral(r7, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME), r0);
  __ bne(&non_outermost_js_2, Label::kNear);
  __ mov(r8, Operand::Zero());
  __ mov(r7, Operand(ExternalReference(js_entry_sp)));
  __ StoreP(r8, MemOperand(r7));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r5);
  __ mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
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

void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // Ensure that the vector and slot registers won't be clobbered before
  // calling the miss handler.
  DCHECK(!AreAliased(r6, r7, LoadWithVectorDescriptor::VectorRegister(),
                     LoadWithVectorDescriptor::SlotRegister()));

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, r6,
                                                          r7, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}

void LoadIndexedStringStub::Generate(MacroAssembler* masm) {
  // Return address is in lr.
  Label miss;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register index = LoadDescriptor::NameRegister();
  Register scratch = r7;
  Register result = r2;
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
#else   // V8_INTERPRETED_REGEXP

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
  Register subject = r6;
  Register regexp_data = r7;
  Register last_match_info_elements = r8;
  Register code = r9;

  __ CleanseP(r14);

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
  __ mov(r2, Operand(address_of_regexp_stack_memory_size));
  __ LoadAndTestP(r2, MemOperand(r2));
  __ beq(&runtime);

  // Check that the first argument is a JSRegExp object.
  __ LoadP(r2, MemOperand(sp, kJSRegExpOffset));
  __ JumpIfSmi(r2, &runtime);
  __ CompareObjectType(r2, r3, r3, JS_REGEXP_TYPE);
  __ bne(&runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ LoadP(regexp_data, FieldMemOperand(r2, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ TestIfSmi(regexp_data);
    __ Check(ne, kUnexpectedTypeForRegExpDataFixedArrayExpected, cr0);
    __ CompareObjectType(regexp_data, r2, r2, FIXED_ARRAY_TYPE);
    __ Check(eq, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ LoadP(r2, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  // DCHECK(Smi::FromInt(JSRegExp::IRREGEXP) < (char *)0xffffu);
  __ CmpSmiLiteral(r2, Smi::FromInt(JSRegExp::IRREGEXP), r0);
  __ bne(&runtime);

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ LoadP(r4,
           FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // SmiToShortArrayOffset accomplishes the multiplication by 2 and
  // SmiUntag (which is a nop for 32-bit).
  __ SmiToShortArrayOffset(r4, r4);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ CmpLogicalP(r4, Operand(Isolate::kJSRegexpStaticOffsetsVectorSize - 2));
  __ bgt(&runtime);

  // Reset offset for possibly sliced string.
  __ LoadImmP(ip, Operand::Zero());
  __ LoadP(subject, MemOperand(sp, kSubjectOffset));
  __ JumpIfSmi(subject, &runtime);
  __ LoadRR(r5, subject);  // Make a copy of the original subject string.
  // subject: subject string
  // r5: subject string
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
  __ LoadP(r2, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ LoadlB(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));

  // (1) Sequential string?  If yes, go to (4).

  STATIC_ASSERT((kIsNotStringMask | kStringRepresentationMask |
                 kShortExternalStringMask) == 0x93);
  __ mov(r3, Operand(kIsNotStringMask | kStringRepresentationMask |
                     kShortExternalStringMask));
  __ AndP(r3, r2);
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ beq(&seq_string, Label::kNear);  // Go to (4).

  // (2) Sequential or cons? If not, go to (5).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  STATIC_ASSERT(kExternalStringTag < 0xffffu);
  __ CmpP(r3, Operand(kExternalStringTag));
  __ bge(&not_seq_nor_cons);  // Go to (5).

  // (3) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ LoadP(r2, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ CompareRoot(r2, Heap::kempty_stringRootIndex);
  __ bne(&runtime);
  __ LoadP(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  __ b(&check_underlying);

  // (4) Sequential string.  Load regexp code according to encoding.
  __ bind(&seq_string);
  // subject: sequential subject string (or look-alike, external string)
  // r5: original subject string
  // Load previous index and check range before r5 is overwritten.  We have to
  // use r5 instead of subject here because subject might have been only made
  // to look like a sequential string when it actually is an external string.
  __ LoadP(r3, MemOperand(sp, kPreviousIndexOffset));
  __ JumpIfNotSmi(r3, &runtime);
  __ LoadP(r5, FieldMemOperand(r5, String::kLengthOffset));
  __ CmpLogicalP(r5, r3);
  __ ble(&runtime);
  __ SmiUntag(r3);

  STATIC_ASSERT(4 == kOneByteStringTag);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  STATIC_ASSERT(kStringEncodingMask == 4);
  __ ExtractBitMask(r5, r2, kStringEncodingMask, SetRC);
  __ beq(&encoding_type_UC16, Label::kNear);
  __ LoadP(code,
           FieldMemOperand(regexp_data, JSRegExp::kDataOneByteCodeOffset));
  __ b(&br_over, Label::kNear);
  __ bind(&encoding_type_UC16);
  __ LoadP(code, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset));
  __ bind(&br_over);

  // (E) Carry on.  String handling is done.
  // code: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(code, &runtime);

  // r3: previous index
  // r5: encoding of subject string (1 if one_byte, 0 if two_byte);
  // code: Address of generated regexp code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(isolate()->counters()->regexp_entry_native(), 1, r2, r4);

  // Isolates: note we add an additional parameter here (isolate pointer).
  const int kRegExpExecuteArguments = 10;
  const int kParameterRegisters = 5;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers.

  // Argument 10 (in stack parameter area): Pass current isolate address.
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
  __ StoreP(r2, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize +
                                   4 * kPointerSize));

  // Argument 9 is a dummy that reserves the space used for
  // the return address added by the ExitFrame in native calls.
  __ mov(r2, Operand::Zero());
  __ StoreP(r2, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize +
                                   3 * kPointerSize));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ mov(r2, Operand(1));
  __ StoreP(r2, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize +
                                   2 * kPointerSize));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ mov(r2, Operand(address_of_regexp_stack_memory_address));
  __ LoadP(r2, MemOperand(r2, 0));
  __ mov(r1, Operand(address_of_regexp_stack_memory_size));
  __ LoadP(r1, MemOperand(r1, 0));
  __ AddP(r2, r1);
  __ StoreP(r2, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize +
                                   1 * kPointerSize));

  // Argument 6: Set the number of capture registers to zero to force
  // global egexps to behave as non-global.  This does not affect non-global
  // regexps.
  __ mov(r2, Operand::Zero());
  __ StoreP(r2, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize +
                                   0 * kPointerSize));

  // Argument 1 (r2): Subject string.
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to 15 pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize and
  // 13 registers saved on the stack previously)
  __ LoadP(r2, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));

  // Argument 2 (r3): Previous index.
  // Already there
  __ AddP(r1, subject, Operand(SeqString::kHeaderSize - kHeapObjectTag));

  // Argument 5 (r6): static offsets vector buffer.
  __ mov(
      r6,
      Operand(ExternalReference::address_of_static_offsets_vector(isolate())));

  // For arguments 4 (r5) and 3 (r4) get string length, calculate start of data
  // and calculate the shift of the index (0 for one-byte and 1 for two byte).
  __ XorP(r5, Operand(1));
  // If slice offset is not 0, load the length from the original sliced string.
  // Argument 3, r4: Start of string data
  // Prepare start and end index of the input.
  __ ShiftLeftP(ip, ip, r5);
  __ AddP(ip, r1, ip);
  __ ShiftLeftP(r4, r3, r5);
  __ AddP(r4, ip, r4);

  // Argument 4, r5: End of string data
  __ LoadP(r1, FieldMemOperand(r2, String::kLengthOffset));
  __ SmiUntag(r1);
  __ ShiftLeftP(r0, r1, r5);
  __ AddP(r5, ip, r0);

  // Locate the code entry and call it.
  __ AddP(code, Operand(Code::kHeaderSize - kHeapObjectTag));

  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm, code);

  __ LeaveExitFrame(false, no_reg, true);

  // r2: result (int32)
  // subject: subject string -- needed to reload
  __ LoadP(subject, MemOperand(sp, kSubjectOffset));

  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)
  // Check the result.
  Label success;
  __ Cmp32(r2, Operand(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ beq(&success);
  Label failure;
  __ Cmp32(r2, Operand(NativeRegExpMacroAssembler::FAILURE));
  __ beq(&failure);
  __ Cmp32(r2, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ bne(&runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ mov(r3, Operand(isolate()->factory()->the_hole_value()));
  __ mov(r4, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));
  __ LoadP(r2, MemOperand(r4, 0));
  __ CmpP(r2, r3);
  __ beq(&runtime);

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  __ bind(&failure);
  // For failure and exception return null.
  __ mov(r2, Operand(isolate()->factory()->null_value()));
  __ la(sp, MemOperand(sp, (4 * kPointerSize)));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ LoadP(r3,
           FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  // SmiToShortArrayOffset accomplishes the multiplication by 2 and
  // SmiUntag (which is a nop for 32-bit).
  __ SmiToShortArrayOffset(r3, r3);
  __ AddP(r3, Operand(2));

  __ LoadP(r2, MemOperand(sp, kLastMatchInfoOffset));
  __ JumpIfSmi(r2, &runtime);
  __ CompareObjectType(r2, r4, r4, JS_OBJECT_TYPE);
  __ bne(&runtime);
  // Check that the object has fast elements.
  __ LoadP(last_match_info_elements,
           FieldMemOperand(r2, JSArray::kElementsOffset));
  __ LoadP(r2,
           FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ CompareRoot(r2, Heap::kFixedArrayMapRootIndex);
  __ bne(&runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ LoadP(
      r2, FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ AddP(r4, r3, Operand(RegExpImpl::kLastMatchOverhead));
  __ SmiUntag(r0, r2);
  __ CmpP(r4, r0);
  __ bgt(&runtime);

  // r3: number of capture registers
  // subject: subject string
  // Store the capture count.
  __ SmiTag(r4, r3);
  __ StoreP(r4, FieldMemOperand(last_match_info_elements,
                                RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ StoreP(subject, FieldMemOperand(last_match_info_elements,
                                     RegExpImpl::kLastSubjectOffset));
  __ LoadRR(r4, subject);
  __ RecordWriteField(last_match_info_elements, RegExpImpl::kLastSubjectOffset,
                      subject, r9, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ LoadRR(subject, r4);
  __ StoreP(subject, FieldMemOperand(last_match_info_elements,
                                     RegExpImpl::kLastInputOffset));
  __ RecordWriteField(last_match_info_elements, RegExpImpl::kLastInputOffset,
                      subject, r9, kLRHasNotBeenSaved, kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ mov(r4, Operand(address_of_static_offsets_vector));

  // r3: number of capture registers
  // r4: offsets vector
  Label next_capture;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ AddP(
      r2, last_match_info_elements,
      Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag - kPointerSize));
  __ AddP(r4, Operand(-kIntSize));  // bias down for lwzu
  __ bind(&next_capture);
  // Read the value from the static offsets vector buffer.
  __ ly(r5, MemOperand(r4, kIntSize));
  __ lay(r4, MemOperand(r4, kIntSize));
  // Store the smi value in the last match info.
  __ SmiTag(r5);
  __ StoreP(r5, MemOperand(r2, kPointerSize));
  __ lay(r2, MemOperand(r2, kPointerSize));
  __ BranchOnCount(r3, &next_capture);

  // Return last match info.
  __ LoadP(r2, MemOperand(sp, kLastMatchInfoOffset));
  __ la(sp, MemOperand(sp, (4 * kPointerSize)));
  __ Ret();

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec);

  // Deferred code for string handling.
  // (5) Long external string? If not, go to (7).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set.
  __ bgt(&not_long_external, Label::kNear);  // Go to (7).

  // (6) External string.  Make it, offset-wise, look like a sequential string.
  __ bind(&external_string);
  __ LoadP(r2, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ LoadlB(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    STATIC_ASSERT(kIsIndirectStringMask == 1);
    __ tmll(r2, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound, cr0);
  }
  __ LoadP(subject,
           FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ SubP(subject, subject,
          Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ b(&seq_string);  // Go to (4).

  // (7) Short external string or not a string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag != 0);
  __ mov(r0, Operand(kIsNotStringMask | kShortExternalStringMask));
  __ AndP(r0, r3);
  __ bne(&runtime);

  // (8) Sliced string.  Replace subject with parent.  Go to (4).
  // Load offset into ip and replace subject string with parent.
  __ LoadP(ip, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ SmiUntag(ip);
  __ LoadP(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ b(&check_underlying);  // Go to (4).
#endif  // V8_INTERPRETED_REGEXP
}

static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // r2 : number of arguments to the construct function
  // r3 : the function to call
  // r4 : feedback vector
  // r5 : slot in feedback vector (Smi)
  FrameScope scope(masm, StackFrame::INTERNAL);

  // Number-of-arguments register must be smi-tagged to call out.
  __ SmiTag(r2);
  __ Push(r5, r4, r3, r2);
  __ Push(cp);

  __ CallStub(stub);

  __ Pop(cp);
  __ Pop(r5, r4, r3, r2);
  __ SmiUntag(r2);
}

static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // r2 : number of arguments to the construct function
  // r3 : the function to call
  // r4 : feedback vector
  // r5 : slot in feedback vector (Smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*TypeFeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  const int count_offset = FixedArray::kHeaderSize + kPointerSize;

  // Load the cache state into r7.
  __ SmiToPtrArrayOffset(r7, r5);
  __ AddP(r7, r4, r7);
  __ LoadP(r7, FieldMemOperand(r7, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if r7 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in type-feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = r8;
  Register weak_value = r9;
  __ LoadP(weak_value, FieldMemOperand(r7, WeakCell::kValueOffset));
  __ CmpP(r3, weak_value);
  __ beq(&done, Label::kNear);
  __ CompareRoot(r7, Heap::kmegamorphic_symbolRootIndex);
  __ beq(&done, Label::kNear);
  __ LoadP(feedback_map, FieldMemOperand(r7, HeapObject::kMapOffset));
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
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r7);
  __ CmpP(r3, r7);
  __ bne(&megamorphic);
  __ b(&done, Label::kNear);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(r7, Heap::kuninitialized_symbolRootIndex);
  __ beq(&initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ SmiToPtrArrayOffset(r7, r5);
  __ AddP(r7, r4, r7);
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ StoreP(ip, FieldMemOperand(r7, FixedArray::kHeaderSize), r0);
  __ jmp(&done);

  // An uninitialized cache is patched with the function
  __ bind(&initialize);

  // Make sure the function is the Array() function.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r7);
  __ CmpP(r3, r7);
  __ bne(&not_array_function);

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the
  // slot.
  CreateAllocationSiteStub create_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &create_stub);
  __ b(&done, Label::kNear);

  __ bind(&not_array_function);

  CreateWeakCellStub weak_cell_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &weak_cell_stub);

  __ bind(&done);

  // Increment the call count for all function calls.
  __ SmiToPtrArrayOffset(r7, r5);
  __ AddP(r7, r4, r7);

  __ LoadP(r6, FieldMemOperand(r7, count_offset));
  __ AddSmiLiteral(r6, r6, Smi::FromInt(1), r0);
  __ StoreP(r6, FieldMemOperand(r7, count_offset), r0);
}

void CallConstructStub::Generate(MacroAssembler* masm) {
  // r2 : number of arguments
  // r3 : the function to call
  // r4 : feedback vector
  // r5 : slot in feedback vector (Smi, for RecordCallTarget)

  Label non_function;
  // Check that the function is not a smi.
  __ JumpIfSmi(r3, &non_function);
  // Check that the function is a JSFunction.
  __ CompareObjectType(r3, r7, r7, JS_FUNCTION_TYPE);
  __ bne(&non_function);

  GenerateRecordCallTarget(masm);

  __ SmiToPtrArrayOffset(r7, r5);
  __ AddP(r7, r4, r7);
  // Put the AllocationSite from the feedback vector into r4, or undefined.
  __ LoadP(r4, FieldMemOperand(r7, FixedArray::kHeaderSize));
  __ LoadP(r7, FieldMemOperand(r4, AllocationSite::kMapOffset));
  __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
  Label feedback_register_initialized;
  __ beq(&feedback_register_initialized);
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(r4, r7);

  // Pass function as new target.
  __ LoadRR(r5, r3);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kConstructStubOffset));
  __ AddP(ip, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);

  __ bind(&non_function);
  __ LoadRR(r5, r3);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// Note: feedback_vector and slot are clobbered after the call.
static void IncrementCallCount(MacroAssembler* masm, Register feedback_vector,
                               Register slot, Register temp) {
  const int count_offset = FixedArray::kHeaderSize + kPointerSize;
  __ SmiToPtrArrayOffset(temp, slot);
  __ AddP(feedback_vector, feedback_vector, temp);
  __ LoadP(slot, FieldMemOperand(feedback_vector, count_offset));
  __ AddSmiLiteral(slot, slot, Smi::FromInt(1), temp);
  __ StoreP(slot, FieldMemOperand(feedback_vector, count_offset), temp);
}

void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // r3 - function
  // r5 - slot id
  // r4 - vector
  // r6 - allocation site (loaded from vector[slot])
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r7);
  __ CmpP(r3, r7);
  __ bne(miss);

  __ mov(r2, Operand(arg_count()));

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, r4, r5, r1);

  __ LoadRR(r4, r6);
  __ LoadRR(r5, r3);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);
}

void CallICStub::Generate(MacroAssembler* masm) {
  // r3 - function
  // r5 - slot id (Smi)
  // r4 - vector
  Label extra_checks_or_miss, call, call_function, call_count_incremented;
  int argc = arg_count();
  ParameterCount actual(argc);

  // The checks. First, does r3 match the recorded monomorphic target?
  __ SmiToPtrArrayOffset(r8, r5);
  __ AddP(r8, r4, r8);
  __ LoadP(r6, FieldMemOperand(r8, FixedArray::kHeaderSize));

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

  __ LoadP(r7, FieldMemOperand(r6, WeakCell::kValueOffset));
  __ CmpP(r3, r7);
  __ bne(&extra_checks_or_miss, Label::kNear);

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(r3, &extra_checks_or_miss);

  __ bind(&call_function);

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, r4, r5, r1);

  __ mov(r2, Operand(argc));
  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ CompareRoot(r6, Heap::kmegamorphic_symbolRootIndex);
  __ beq(&call);

  // Verify that r6 contains an AllocationSite
  __ LoadP(r7, FieldMemOperand(r6, HeapObject::kMapOffset));
  __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
  __ bne(&not_allocation_site);

  // We have an allocation site.
  HandleArrayCase(masm, &miss);

  __ bind(&not_allocation_site);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ b(&miss);
  }

  __ CompareRoot(r6, Heap::kuninitialized_symbolRootIndex);
  __ beq(&uninitialized);

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(r6);
  __ CompareObjectType(r6, r7, r7, JS_FUNCTION_TYPE);
  __ bne(&miss);
  __ LoadRoot(ip, Heap::kmegamorphic_symbolRootIndex);
  __ StoreP(ip, FieldMemOperand(r8, FixedArray::kHeaderSize), r0);

  __ bind(&call);

  // Increment the call count for megamorphic function calls.
  IncrementCallCount(masm, r4, r5, r1);

  __ bind(&call_count_incremented);
  __ mov(r2, Operand(argc));
  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(r3, &miss);

  // Goto miss case if we do not have a function.
  __ CompareObjectType(r3, r6, r6, JS_FUNCTION_TYPE);
  __ bne(&miss);

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, r6);
  __ CmpP(r3, r6);
  __ beq(&miss);

  // Make sure the function belongs to the same native context.
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kContextOffset));
  __ LoadP(r6, ContextMemOperand(r6, Context::NATIVE_CONTEXT_INDEX));
  __ LoadP(ip, NativeContextMemOperand());
  __ CmpP(r6, ip);
  __ bne(&miss);

  // Store the function. Use a stub since we need a frame for allocation.
  // r4 - vector
  // r5 - slot
  // r3 - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(masm->isolate());
    __ Push(r4);
    __ Push(r5);
    __ Push(cp, r3);
    __ CallStub(&create_stub);
    __ Pop(cp, r3);
    __ Pop(r5);
    __ Pop(r4);
  }

  __ b(&call_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  __ b(&call_count_incremented);
}

void CallICStub::GenerateMiss(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);

  // Push the function and feedback info.
  __ Push(r3, r4, r5);

  // Call the entry.
  __ CallRuntime(Runtime::kCallIC_Miss);

  // Move result to r3 and exit the internal frame.
  __ LoadRR(r3, r2);
}

// StringCharCodeAtGenerator
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ LoadP(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
    __ LoadlB(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ mov(r0, Operand(kIsNotStringMask));
    __ AndP(r0, result_);
    __ bne(receiver_not_string_);
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ LoadP(ip, FieldMemOperand(object_, String::kLengthOffset));
  __ CmpLogicalP(ip, index_);
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
  __ Move(index_, r2);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Pop(LoadWithVectorDescriptor::VectorRegister(),
           LoadWithVectorDescriptor::SlotRegister(), object_);
  } else {
    __ pop(object_);
  }
  // Reload the instance type.
  __ LoadP(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ LoadlB(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
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
  __ Move(result_, r2);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}

// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  DCHECK(base::bits::IsPowerOfTwo32(String::kMaxOneByteCharCodeU + 1));
  __ LoadSmiLiteral(r0, Smi::FromInt(~String::kMaxOneByteCharCodeU));
  __ OrP(r0, r0, Operand(kSmiTagMask));
  __ AndP(r0, code_, r0);
  __ bne(&slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged one-byte char code.
  __ LoadRR(r0, code_);
  __ SmiToPtrArrayOffset(code_, code_);
  __ AddP(result_, code_);
  __ LoadRR(code_, r0);
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
  __ CallRuntime(Runtime::kStringCharFromCode);
  __ Move(result_, r2);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}

enum CopyCharactersFlags { COPY_ASCII = 1, DEST_ALWAYS_ALIGNED = 2 };

void StringHelper::GenerateCopyCharacters(MacroAssembler* masm, Register dest,
                                          Register src, Register count,
                                          Register scratch,
                                          String::Encoding encoding) {
  if (FLAG_debug_code) {
    // Check that destination is word aligned.
    __ mov(r0, Operand(kPointerAlignmentMask));
    __ AndP(r0, dest);
    __ Check(eq, kDestinationOfCopyNotAligned, cr0);
  }

  // Nothing to do for zero characters.
  Label done;
  if (encoding == String::TWO_BYTE_ENCODING) {
    // double the length
    __ AddP(count, count, count);
    __ beq(&done, Label::kNear);
  } else {
    __ CmpP(count, Operand::Zero());
    __ beq(&done, Label::kNear);
  }

  // Copy count bytes from src to dst.
  Label byte_loop;
  // TODO(joransiu): Convert into MVC loop
  __ bind(&byte_loop);
  __ LoadlB(scratch, MemOperand(src));
  __ la(src, MemOperand(src, 1));
  __ stc(scratch, MemOperand(dest));
  __ la(dest, MemOperand(dest, 1));
  __ BranchOnCount(count, &byte_loop);

  __ bind(&done);
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
  __ CmpP(length, scratch2);
  __ beq(&check_zero_length);
  __ bind(&strings_not_equal);
  __ LoadSmiLiteral(r2, Smi::FromInt(NOT_EQUAL));
  __ Ret();

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ CmpP(length, Operand::Zero());
  __ bne(&compare_chars);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();

  // Compare characters.
  __ bind(&compare_chars);
  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2,
                                  &strings_not_equal);

  // Characters are equal.
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();
}

void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Label skip, result_not_equal, compare_lengths;
  // Find minimum length and length difference.
  __ LoadP(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ LoadP(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ SubP(scratch3, scratch1, scratch2 /*, LeaveOE, SetRC*/);
  // Removing RC looks okay here.
  Register length_delta = scratch3;
  __ ble(&skip, Label::kNear);
  __ LoadRR(scratch1, scratch2);
  __ bind(&skip);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ CmpP(min_length, Operand::Zero());
  __ beq(&compare_lengths);

  // Compare loop.
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ bind(&compare_lengths);
  DCHECK(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use length_delta as result if it's zero.
  __ LoadRR(r2, length_delta);
  __ CmpP(length_delta, Operand::Zero());
  __ bind(&result_not_equal);
  // Conditionally update the result based either on length_delta or
  // the last comparion performed in the loop above.
  Label less_equal, equal;
  __ ble(&less_equal);
  __ LoadSmiLiteral(r2, Smi::FromInt(GREATER));
  __ Ret();
  __ bind(&less_equal);
  __ beq(&equal);
  __ LoadSmiLiteral(r2, Smi::FromInt(LESS));
  __ bind(&equal);
  __ Ret();
}

void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch1, Label* chars_not_equal) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ AddP(scratch1, length,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ AddP(left, scratch1);
  __ AddP(right, scratch1);
  __ LoadComplementRR(length, length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ LoadlB(scratch1, MemOperand(left, index));
  __ LoadlB(r0, MemOperand(right, index));
  __ CmpP(scratch1, r0);
  __ bne(chars_not_equal);
  __ AddP(index, Operand(1));
  __ CmpP(index, Operand::Zero());
  __ bne(&loop);
}

void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3    : left
  //  -- r2    : right
  // r3: second string
  // -----------------------------------

  // Load r4 with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ Move(r4, isolate()->factory()->undefined_value());

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ TestIfSmi(r4);
    __ Assert(ne, kExpectedAllocationSite, cr0);
    __ push(r4);
    __ LoadP(r4, FieldMemOperand(r4, HeapObject::kMapOffset));
    __ CompareRoot(r4, Heap::kAllocationSiteMapRootIndex);
    __ pop(r4);
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

  __ CheckMap(r3, r4, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  __ CheckMap(r2, r5, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  if (!Token::IsEqualityOp(op())) {
    __ LoadP(r3, FieldMemOperand(r3, Oddball::kToNumberOffset));
    __ AssertSmi(r3);
    __ LoadP(r2, FieldMemOperand(r2, Oddball::kToNumberOffset));
    __ AssertSmi(r2);
  }
  __ SubP(r2, r3, r2);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}

void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ OrP(r4, r3, r2);
  __ JumpIfNotSmi(r4, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    // __ sub(r2, r2, r3, SetCC);
    __ SubP(r2, r2, r3);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(r3);
    __ SmiUntag(r2);
    __ SubP(r2, r3, r2);
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
    __ JumpIfNotSmi(r3, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(r2, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved.
  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(r2, &right_smi);
  __ CheckMap(r2, r4, Heap::kHeapNumberMapRootIndex, &maybe_undefined1,
              DONT_DO_SMI_CHECK);
  __ LoadDouble(d1, FieldMemOperand(r2, HeapNumber::kValueOffset));
  __ b(&left);
  __ bind(&right_smi);
  __ SmiToDouble(d1, r2);

  __ bind(&left);
  __ JumpIfSmi(r3, &left_smi);
  __ CheckMap(r3, r4, Heap::kHeapNumberMapRootIndex, &maybe_undefined2,
              DONT_DO_SMI_CHECK);
  __ LoadDouble(d0, FieldMemOperand(r3, HeapNumber::kValueOffset));
  __ b(&done);
  __ bind(&left_smi);
  __ SmiToDouble(d0, r3);

  __ bind(&done);

  // Compare operands
  __ cdbr(d0, d1);

  // Don't base result on status bits when a NaN is involved.
  __ bunordered(&unordered);

  // Return a result of -1, 0, or 1, based on status bits.
  __ beq(&equal);
  __ blt(&less_than);
  //  assume greater than
  __ LoadImmP(r2, Operand(GREATER));
  __ Ret();
  __ bind(&equal);
  __ LoadImmP(r2, Operand(EQUAL));
  __ Ret();
  __ bind(&less_than);
  __ LoadImmP(r2, Operand(LESS));
  __ Ret();

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
    __ bne(&miss);
    __ JumpIfSmi(r3, &unordered);
    __ CompareObjectType(r3, r4, r4, HEAP_NUMBER_TYPE);
    __ bne(&maybe_undefined2);
    __ b(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
    __ beq(&unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}

void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  Label miss, not_equal;

  // Registers containing left and right operands respectively.
  Register left = r3;
  Register right = r2;
  Register tmp1 = r4;
  Register tmp2 = r5;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are symbols.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ LoadlB(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ LoadlB(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ OrP(tmp1, tmp1, tmp2);
  __ AndP(r0, tmp1, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ bne(&miss);

  // Internalized strings are compared by identity.
  __ CmpP(left, right);
  __ bne(&not_equal);
  // Make sure r2 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r2));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
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
  Register left = r3;
  Register right = r2;
  Register tmp1 = r4;
  Register tmp2 = r5;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ LoadlB(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ LoadlB(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss);

  // Unique names are compared by identity.
  __ CmpP(left, right);
  __ bne(&miss);
  // Make sure r2 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r2));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}

void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  Label miss, not_identical, is_symbol;

  bool equality = Token::IsEqualityOp(op());

  // Registers containing left and right operands respectively.
  Register left = r3;
  Register right = r2;
  Register tmp1 = r4;
  Register tmp2 = r5;
  Register tmp3 = r6;
  Register tmp4 = r7;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ LoadlB(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ LoadlB(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ OrP(tmp3, tmp1, tmp2);
  __ AndP(r0, tmp3, Operand(kIsNotStringMask));
  __ bne(&miss);

  // Fast check for identical strings.
  __ CmpP(left, right);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ bne(&not_identical);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();
  __ bind(&not_identical);

  // Handle not identical strings.

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We know they are both
  // strings.
  if (equality) {
    DCHECK(GetCondition() == eq);
    STATIC_ASSERT(kInternalizedTag == 0);
    __ OrP(tmp3, tmp1, tmp2);
    __ AndP(r0, tmp3, Operand(kIsNotInternalizedMask));
    __ bne(&is_symbol);
    // Make sure r2 is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(r2));
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
  if (equality) {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(left, right);
      __ CallRuntime(Runtime::kStringEqual);
    }
    __ LoadRoot(r3, Heap::kTrueValueRootIndex);
    __ SubP(r2, r2, r3);
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
  __ AndP(r4, r3, r2);
  __ JumpIfSmi(r4, &miss);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ CompareObjectType(r2, r4, r4, FIRST_JS_RECEIVER_TYPE);
  __ blt(&miss);
  __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
  __ blt(&miss);

  DCHECK(GetCondition() == eq);
  __ SubP(r2, r2, r3);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}

void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ AndP(r4, r3, r2);
  __ JumpIfSmi(r4, &miss);
  __ GetWeakValue(r6, cell);
  __ LoadP(r4, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadP(r5, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ CmpP(r4, r6);
  __ bne(&miss);
  __ CmpP(r5, r6);
  __ bne(&miss);

  if (Token::IsEqualityOp(op())) {
    __ SubP(r2, r2, r3);
    __ Ret();
  } else {
    if (op() == Token::LT || op() == Token::LTE) {
      __ LoadSmiLiteral(r4, Smi::FromInt(GREATER));
    } else {
      __ LoadSmiLiteral(r4, Smi::FromInt(LESS));
    }
    __ Push(r3, r2, r4);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}

void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r2);
    __ Push(r3, r2);
    __ LoadSmiLiteral(r0, Smi::FromInt(op()));
    __ push(r0);
    __ CallRuntime(Runtime::kCompareIC_Miss);
    // Compute the entry point of the rewritten stub.
    __ AddP(r4, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
    // Restore registers.
    __ Pop(r3, r2);
  }

  __ JumpToJSEntry(r4);
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
    __ SubP(index, Operand(1));
    __ LoadSmiLiteral(
        ip, Smi::FromInt(name->Hash() + NameDictionary::GetProbeOffset(i)));
    __ AndP(index, ip);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ ShiftLeftP(ip, index, Operand(1));
    __ AddP(index, ip);  // index *= 3.

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    Register tmp = properties;
    __ SmiToPtrArrayOffset(ip, index);
    __ AddP(tmp, properties, ip);
    __ LoadP(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    DCHECK(!tmp.is(entity_name));
    __ CompareRoot(entity_name, Heap::kUndefinedValueRootIndex);
    __ beq(done);

    // Stop if found the property.
    __ CmpP(entity_name, Operand(Handle<Name>(name)));
    __ beq(miss);

    Label good;
    __ CompareRoot(entity_name, Heap::kTheHoleValueRootIndex);
    __ beq(&good);

    // Check if the entry name is not a unique name.
    __ LoadP(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ LoadlB(entity_name,
              FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ LoadP(properties,
             FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask = (r0.bit() | r8.bit() | r7.bit() | r6.bit() | r5.bit() |
                          r4.bit() | r3.bit() | r2.bit());

  __ LoadRR(r0, r14);
  __ MultiPush(spill_mask);

  __ LoadP(r2, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ mov(r3, Operand(Handle<Name>(name)));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  __ CmpP(r2, Operand::Zero());

  __ MultiPop(spill_mask);  // MultiPop does not touch condition flags
  __ LoadRR(r14, r0);

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
  __ SubP(scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ LoadlW(scratch2, FieldMemOperand(name, String::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ AddP(scratch2,
              Operand(NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ srl(scratch2, Operand(String::kHashShift));
    __ AndP(scratch2, scratch1);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.
    __ ShiftLeftP(ip, scratch2, Operand(1));
    __ AddP(scratch2, ip);

    // Check if the key is identical to the name.
    __ ShiftLeftP(ip, scratch2, Operand(kPointerSizeLog2));
    __ AddP(scratch2, elements, ip);
    __ LoadP(ip, FieldMemOperand(scratch2, kElementsStartOffset));
    __ CmpP(name, ip);
    __ beq(done);
  }

  const int spill_mask = (r0.bit() | r8.bit() | r7.bit() | r6.bit() | r5.bit() |
                          r4.bit() | r3.bit() | r2.bit()) &
                         ~(scratch1.bit() | scratch2.bit());

  __ LoadRR(r0, r14);
  __ MultiPush(spill_mask);
  if (name.is(r2)) {
    DCHECK(!elements.is(r3));
    __ LoadRR(r3, name);
    __ LoadRR(r2, elements);
  } else {
    __ LoadRR(r2, elements);
    __ LoadRR(r3, name);
  }
  NameDictionaryLookupStub stub(masm->isolate(), POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ LoadRR(r1, r2);
  __ LoadRR(scratch2, r4);
  __ MultiPop(spill_mask);
  __ LoadRR(r14, r0);

  __ CmpP(r1, Operand::Zero());
  __ bne(done);
  __ beq(miss);
}

void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Registers:
  //  result: NameDictionary to probe
  //  r3: key
  //  dictionary: NameDictionary to probe.
  //  index: will hold an index of entry if lookup is successful.
  //         might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Register result = r2;
  Register dictionary = r2;
  Register key = r3;
  Register index = r4;
  Register mask = r5;
  Register hash = r6;
  Register undefined = r7;
  Register entry_key = r8;
  Register scratch = r8;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ LoadP(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ SmiUntag(mask);
  __ SubP(mask, Operand(1));

  __ LoadlW(hash, FieldMemOperand(key, String::kHashFieldOffset));

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
      __ AddP(index, hash,
              Operand(NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    } else {
      __ LoadRR(index, hash);
    }
    __ ShiftRight(r0, index, Operand(String::kHashShift));
    __ AndP(index, r0, mask);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ ShiftLeftP(scratch, index, Operand(1));
    __ AddP(index, scratch);  // index *= 3.

    __ ShiftLeftP(scratch, index, Operand(kPointerSizeLog2));
    __ AddP(index, dictionary, scratch);
    __ LoadP(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ CmpP(entry_key, undefined);
    __ beq(&not_in_dictionary);

    // Stop if found the property.
    __ CmpP(entry_key, key);
    __ beq(&in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ LoadP(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ LoadlB(entry_key,
                FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(entry_key, &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ LoadImmP(result, Operand::Zero());
    __ Ret();
  }

  __ bind(&in_dictionary);
  __ LoadImmP(result, Operand(1));
  __ Ret();

  __ bind(&not_in_dictionary);
  __ LoadImmP(result, Operand::Zero());
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
  __ b(CC_NOP, &skip_to_incremental_noncompacting);
  __ b(CC_NOP, &skip_to_incremental_compacting);

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
  // patching not required on S390 as the initial path is effectively NOP
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
      r2.is(regs_.address()) ? regs_.scratch0() : regs_.address();
  DCHECK(!address.is(regs_.object()));
  DCHECK(!address.is(r2));
  __ LoadRR(address, regs_.address());
  __ LoadRR(r2, regs_.object());
  __ LoadRR(r3, address);
  __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));

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
  __ LoadP(r3, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ AddP(r3, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ ShiftLeftP(r3, r3, Operand(kPointerSizeLog2));
  __ la(sp, MemOperand(r3, sp));
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
  __ EmitLoadTypeFeedbackVector(r4);
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

  __ LoadP(cached_map,
           FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(0)));
  __ LoadP(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ CmpP(receiver_map, cached_map);
  __ bne(&start_polymorphic, Label::kNear);
  // found, now call handler.
  Register handler = feedback;
  __ LoadP(handler,
           FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ AddP(ip, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
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
  __ AddP(too_far, feedback, r0);
  __ AddP(too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ AddP(pointer_reg, feedback,
          Operand(FixedArray::OffsetOfElementAt(2) - kHeapObjectTag));

  __ bind(&next_loop);
  __ LoadP(cached_map, MemOperand(pointer_reg));
  __ LoadP(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ CmpP(receiver_map, cached_map);
  __ bne(&prepare_next, Label::kNear);
  __ LoadP(handler, MemOperand(pointer_reg, kPointerSize));
  __ AddP(ip, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);

  __ bind(&prepare_next);
  __ AddP(pointer_reg, Operand(kPointerSize * 2));
  __ CmpP(pointer_reg, too_far);
  __ blt(&next_loop, Label::kNear);

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
  __ CmpP(cached_map, receiver_map);
  __ bne(try_array);
  Register handler = feedback;
  __ SmiToPtrArrayOffset(r1, slot);
  __ LoadP(handler,
           FieldMemOperand(r1, vector, FixedArray::kHeaderSize + kPointerSize));
  __ AddP(ip, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);
}

void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // r3
  Register name = LoadWithVectorDescriptor::NameRegister();          // r4
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // r5
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // r2
  Register feedback = r6;
  Register receiver_map = r7;
  Register scratch1 = r8;

  __ SmiToPtrArrayOffset(r1, slot);
  __ LoadP(feedback, FieldMemOperand(r1, vector, FixedArray::kHeaderSize));

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
  __ bne(&not_array, Label::kNear);
  HandleArrayCases(masm, feedback, receiver_map, scratch1, r9, true, &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ bne(&miss);
  masm->isolate()->load_stub_cache()->GenerateProbe(
      masm, receiver, name, feedback, receiver_map, scratch1, r9);

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
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // r3
  Register key = LoadWithVectorDescriptor::NameRegister();           // r4
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // r5
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // r2
  Register feedback = r6;
  Register receiver_map = r7;
  Register scratch1 = r8;

  __ SmiToPtrArrayOffset(r1, slot);
  __ LoadP(feedback, FieldMemOperand(r1, vector, FixedArray::kHeaderSize));

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
  HandleArrayCases(masm, feedback, receiver_map, scratch1, r9, true, &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ bne(&try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ CmpP(key, feedback);
  __ bne(&miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ SmiToPtrArrayOffset(r1, slot);
  __ LoadP(feedback,
           FieldMemOperand(r1, vector, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, r9, false, &miss);

  __ bind(&miss);
  KeyedLoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ b(&compare_map);
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
  Register receiver = StoreWithVectorDescriptor::ReceiverRegister();  // r3
  Register key = StoreWithVectorDescriptor::NameRegister();           // r4
  Register vector = StoreWithVectorDescriptor::VectorRegister();      // r5
  Register slot = StoreWithVectorDescriptor::SlotRegister();          // r6
  DCHECK(StoreWithVectorDescriptor::ValueRegister().is(r2));          // r2
  Register feedback = r7;
  Register receiver_map = r8;
  Register scratch1 = r9;

  __ SmiToPtrArrayOffset(r0, slot);
  __ AddP(feedback, vector, r0);
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

  Register scratch2 = ip;
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, true,
                   &miss);

  __ bind(&not_array);
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ bne(&miss);
  masm->isolate()->store_stub_cache()->GenerateProbe(
      masm, receiver, key, feedback, receiver_map, scratch1, scratch2);

  __ bind(&miss);
  StoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ b(&compare_map);
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
  __ LoadP(too_far, FieldMemOperand(feedback, FixedArray::kLengthOffset));

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
  __ SmiToPtrArrayOffset(r0, too_far);
  __ AddP(too_far, feedback, r0);
  __ AddP(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ AddP(pointer_reg, feedback,
          Operand(FixedArray::OffsetOfElementAt(0) - kHeapObjectTag));

  __ bind(&next_loop);
  __ LoadP(cached_map, MemOperand(pointer_reg));
  __ LoadP(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ CmpP(receiver_map, cached_map);
  __ bne(&prepare_next);
  // Is it a transitioning store?
  __ LoadP(too_far, MemOperand(pointer_reg, kPointerSize));
  __ CompareRoot(too_far, Heap::kUndefinedValueRootIndex);
  __ bne(&transition_call);
  __ LoadP(pointer_reg, MemOperand(pointer_reg, kPointerSize * 2));
  __ AddP(ip, pointer_reg, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);

  __ bind(&transition_call);
  __ LoadP(too_far, FieldMemOperand(too_far, WeakCell::kValueOffset));
  __ JumpIfSmi(too_far, miss);

  __ LoadP(receiver_map, MemOperand(pointer_reg, kPointerSize * 2));

  // Load the map into the correct register.
  DCHECK(feedback.is(StoreTransitionDescriptor::MapRegister()));
  __ LoadRR(feedback, too_far);

  __ AddP(ip, receiver_map, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip);

  __ bind(&prepare_next);
  __ AddP(pointer_reg, pointer_reg, Operand(kPointerSize * 3));
  __ CmpLogicalP(pointer_reg, too_far);
  __ blt(&next_loop);

  // We exhausted our array of map handler pairs.
  __ b(miss);
}

void KeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = StoreWithVectorDescriptor::ReceiverRegister();  // r3
  Register key = StoreWithVectorDescriptor::NameRegister();           // r4
  Register vector = StoreWithVectorDescriptor::VectorRegister();      // r5
  Register slot = StoreWithVectorDescriptor::SlotRegister();          // r6
  DCHECK(StoreWithVectorDescriptor::ValueRegister().is(r2));          // r2
  Register feedback = r7;
  Register receiver_map = r8;
  Register scratch1 = r9;

  __ SmiToPtrArrayOffset(r0, slot);
  __ AddP(feedback, vector, r0);
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

  Register scratch2 = ip;

  HandlePolymorphicStoreCase(masm, feedback, receiver_map, scratch1, scratch2,
                             &miss);

  __ bind(&not_array);
  // Is it generic?
  __ CompareRoot(feedback, Heap::kmegamorphic_symbolRootIndex);
  __ bne(&try_poly_name);
  Handle<Code> megamorphic_stub =
      KeyedStoreIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ CmpP(key, feedback);
  __ bne(&miss);
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ SmiToPtrArrayOffset(r0, slot);
  __ AddP(feedback, vector, r0);
  __ LoadP(feedback,
           FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, false,
                   &miss);

  __ bind(&miss);
  KeyedStoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ b(&compare_map);
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
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
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
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
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ AndP(r0, r5, Operand(1));
    __ bne(&normal_sequence);
  }

  // look at the first argument
  __ LoadP(r7, MemOperand(sp, 0));
  __ CmpP(r7, Operand::Zero());
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
    __ AddP(r5, r5, Operand(1));
    if (FLAG_debug_code) {
      __ LoadP(r7, FieldMemOperand(r4, 0));
      __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r5
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ LoadP(r6, FieldMemOperand(r4, AllocationSite::kTransitionInfoOffset));
    __ AddSmiLiteral(r6, r6, Smi::FromInt(kFastElementsKindPackedToHoley), r0);
    __ StoreP(r6, FieldMemOperand(r4, AllocationSite::kTransitionInfoOffset));

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
  if (argument_count() == ANY) {
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
    // Will both indicate a NULL and a Smi.
    __ TestIfSmi(r6);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r6, r6, r7, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);

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

  __ LoadP(r5, FieldMemOperand(r4, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(r5);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ AndP(r5, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  __ bind(&subclassing);
  switch (argument_count()) {
    case ANY:
    case MORE_THAN_ONE:
      __ ShiftLeftP(r1, r2, Operand(kPointerSizeLog2));
      __ StoreP(r3, MemOperand(sp, r1));
      __ AddP(r2, r2, Operand(3));
      break;
    case NONE:
      __ StoreP(r3, MemOperand(sp, 0 * kPointerSize));
      __ LoadImmP(r2, Operand(3));
      break;
    case ONE:
      __ StoreP(r3, MemOperand(sp, 1 * kPointerSize));
      __ LoadImmP(r2, Operand(4));
      break;
  }

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
    // Will both indicate a NULL and a Smi.
    __ TestIfSmi(r5);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r5, r6, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the map's "bit field 2" into |result|.
  __ LoadlB(r5, FieldMemOperand(r5, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(r5);

  if (FLAG_debug_code) {
    Label done;
    __ CmpP(r5, Operand(FAST_ELEMENTS));
    __ beq(&done);
    __ CmpP(r5, Operand(FAST_HOLEY_ELEMENTS));
    __ Assert(eq, kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ CmpP(r5, Operand(FAST_ELEMENTS));
  __ beq(&fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}

void FastNewObjectStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : target
  //  -- r5 : new target
  //  -- cp : context
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r3);
  __ AssertReceiver(r5);

  // Verify that the new target is a JSFunction.
  Label new_object;
  __ CompareObjectType(r5, r4, r4, JS_FUNCTION_TYPE);
  __ bne(&new_object);

  // Load the initial map and verify that it's in fact a map.
  __ LoadP(r4, FieldMemOperand(r5, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(r4, &new_object);
  __ CompareObjectType(r4, r2, r2, MAP_TYPE);
  __ bne(&new_object);

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  __ LoadP(r2, FieldMemOperand(r4, Map::kConstructorOrBackPointerOffset));
  __ CmpP(r2, r3);
  __ bne(&new_object);

  // Allocate the JSObject on the heap.
  Label allocate, done_allocate;
  __ LoadlB(r6, FieldMemOperand(r4, Map::kInstanceSizeOffset));
  __ Allocate(r6, r2, r7, r8, &allocate, SIZE_IN_WORDS);
  __ bind(&done_allocate);

  // Initialize the JSObject fields.
  __ StoreP(r4, FieldMemOperand(r2, JSObject::kMapOffset));
  __ LoadRoot(r5, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kPropertiesOffset));
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kElementsOffset));
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ AddP(r3, r2, Operand(JSObject::kHeaderSize - kHeapObjectTag));

  // ----------- S t a t e -------------
  //  -- r2 : result (tagged)
  //  -- r3 : result fields (untagged)
  //  -- r7 : result end (untagged)
  //  -- r4 : initial map
  //  -- cp : context
  //  -- lr : return address
  // -----------------------------------

  // Perform in-object slack tracking if requested.
  Label slack_tracking;
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  __ LoadRoot(r8, Heap::kUndefinedValueRootIndex);
  __ LoadlW(r5, FieldMemOperand(r4, Map::kBitField3Offset));
  __ DecodeField<Map::ConstructionCounter>(r9, r5);
  __ LoadAndTestP(r9, r9);
  __ bne(&slack_tracking);
  {
    // Initialize all in-object fields with undefined.
    __ InitializeFieldsWithFiller(r3, r7, r8);

    __ Ret();
  }
  __ bind(&slack_tracking);
  {
    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    __ Add32(r5, r5, Operand(-(1 << Map::ConstructionCounter::kShift)));
    __ StoreW(r5, FieldMemOperand(r4, Map::kBitField3Offset));

    // Initialize the in-object fields with undefined.
    __ LoadlB(r6, FieldMemOperand(r4, Map::kUnusedPropertyFieldsOffset));
    __ ShiftLeftP(r6, r6, Operand(kPointerSizeLog2));
    __ SubP(r6, r7, r6);
    __ InitializeFieldsWithFiller(r3, r6, r8);

    // Initialize the remaining (reserved) fields with one pointer filler map.
    __ LoadRoot(r8, Heap::kOnePointerFillerMapRootIndex);
    __ InitializeFieldsWithFiller(r3, r7, r8);

    // Check if we can finalize the instance size.
    __ CmpP(r9, Operand(Map::kSlackTrackingCounterEnd));
    __ Ret(ne);

    // Finalize the instance size.
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r2, r4);
      __ CallRuntime(Runtime::kFinalizeInstanceSize);
      __ Pop(r2);
    }
    __ Ret();
  }

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    STATIC_ASSERT(kSmiTag == 0);
    __ ShiftLeftP(r6, r6,
                  Operand(kPointerSizeLog2 + kSmiTagSize + kSmiShiftSize));
    __ Push(r4, r6);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(r4);
  }
  __ LoadlB(r7, FieldMemOperand(r4, Map::kInstanceSizeOffset));
  __ ShiftLeftP(r7, r7, Operand(kPointerSizeLog2));
  __ AddP(r7, r2, r7);
  __ SubP(r7, r7, Operand(kHeapObjectTag));
  __ b(&done_allocate);

  // Fall back to %NewObject.
  __ bind(&new_object);
  __ Push(r3, r5);
  __ TailCallRuntime(Runtime::kNewObject);
}

void FastNewRestParameterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r3);

  // Make r4 point to the JavaScript frame.
  __ LoadRR(r4, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ LoadP(r4, MemOperand(r4, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ LoadP(ip, MemOperand(r4, StandardFrameConstants::kFunctionOffset));
    __ CmpP(ip, r3);
    __ b(&ok, Label::kNear);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ LoadP(r4, MemOperand(r4, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(ip, MemOperand(r4, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpSmiLiteral(ip, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&no_rest_parameters);

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ LoadP(r2, MemOperand(r4, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ LoadP(r5, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadW(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
#if V8_TARGET_ARCH_S390X
  __ SmiTag(r5);
#endif
  __ SubP(r2, r2, r5);
  __ bgt(&rest_parameters);

  // Return an empty rest parameter array.
  __ bind(&no_rest_parameters);
  {
    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- lr : return address
    // -----------------------------------

    // Allocate an empty rest parameter array.
    Label allocate, done_allocate;
    __ Allocate(JSArray::kSize, r2, r3, r4, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the rest parameter array in r0.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, r3);
    __ StoreP(r3, FieldMemOperand(r2, JSArray::kMapOffset), r0);
    __ LoadRoot(r3, Heap::kEmptyFixedArrayRootIndex);
    __ StoreP(r3, FieldMemOperand(r2, JSArray::kPropertiesOffset), r0);
    __ StoreP(r3, FieldMemOperand(r2, JSArray::kElementsOffset), r0);
    __ LoadImmP(r3, Operand::Zero());
    __ StoreP(r3, FieldMemOperand(r2, JSArray::kLengthOffset), r0);
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret();

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(Smi::FromInt(JSArray::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace);
    }
    __ b(&done_allocate);
  }

  __ bind(&rest_parameters);
  {
    // Compute the pointer to the first rest parameter (skippping the receiver).
    __ SmiToPtrArrayOffset(r8, r2);
    __ AddP(r4, r4, r8);
    __ AddP(r4, r4, Operand(StandardFrameConstants::kCallerSPOffset));

    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- r2 : number of rest parameters (tagged)
    //  -- r3 : function
    //  -- r4 : pointer just past first rest parameters
    //  -- r8 : size of rest parameters
    //  -- lr : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ mov(r9, Operand(JSArray::kSize + FixedArray::kHeaderSize));
    __ AddP(r9, r9, r8);
    __ Allocate(r9, r5, r6, r7, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the elements array in r5.
    __ LoadRoot(r3, Heap::kFixedArrayMapRootIndex);
    __ StoreP(r3, FieldMemOperand(r5, FixedArray::kMapOffset), r0);
    __ StoreP(r2, FieldMemOperand(r5, FixedArray::kLengthOffset), r0);
    __ AddP(r6, r5,
            Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
    {
      Label loop;
      __ SmiUntag(r1, r2);
      // __ mtctr(r0);
      __ bind(&loop);
      __ lay(r4, MemOperand(r4, -kPointerSize));
      __ LoadP(ip, MemOperand(r4));
      __ la(r6, MemOperand(r6, kPointerSize));
      __ StoreP(ip, MemOperand(r6));
      // __ bdnz(&loop);
      __ BranchOnCount(r1, &loop);
      __ AddP(r6, r6, Operand(kPointerSize));
    }

    // Setup the rest parameter array in r6.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, r3);
    __ StoreP(r3, MemOperand(r6, JSArray::kMapOffset));
    __ LoadRoot(r3, Heap::kEmptyFixedArrayRootIndex);
    __ StoreP(r3, MemOperand(r6, JSArray::kPropertiesOffset));
    __ StoreP(r5, MemOperand(r6, JSArray::kElementsOffset));
    __ StoreP(r2, MemOperand(r6, JSArray::kLengthOffset));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ AddP(r2, r6, Operand(kHeapObjectTag));
    __ Ret();

    // Fall back to %AllocateInNewSpace (if not too big).
    Label too_big_for_new_space;
    __ bind(&allocate);
    __ CmpP(r9, Operand(kMaxRegularHeapObjectSize));
    __ bgt(&too_big_for_new_space);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(r9);
      __ Push(r2, r4, r9);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ LoadRR(r5, r2);
      __ Pop(r2, r4);
    }
    __ b(&done_allocate);

    // Fall back to %NewRestParameter.
    __ bind(&too_big_for_new_space);
    __ push(r3);
    __ TailCallRuntime(Runtime::kNewRestParameter);
  }
}

void FastNewSloppyArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r3);

  // Make r9 point to the JavaScript frame.
  __ LoadRR(r9, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ LoadP(r9, MemOperand(r9, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ LoadP(ip, MemOperand(r9, StandardFrameConstants::kFunctionOffset));
    __ CmpP(ip, r3);
    __ beq(&ok, Label::kNear);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadW(
      r4, FieldMemOperand(r4, SharedFunctionInfo::kFormalParameterCountOffset));
#if V8_TARGET_ARCH_S390X
  __ SmiTag(r4);
#endif
  __ SmiToPtrArrayOffset(r5, r4);
  __ AddP(r5, r9, r5);
  __ AddP(r5, r5, Operand(StandardFrameConstants::kCallerSPOffset));

  // r3 : function
  // r4 : number of parameters (tagged)
  // r5 : parameters pointer
  // r9 : JavaScript frame pointer
  // Registers used over whole function:
  // r7 : arguments count (tagged)
  // r8 : mapped parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ LoadP(r6, MemOperand(r9, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r2, MemOperand(r6, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpSmiLiteral(r2, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor_frame);

  // No adaptor, parameter count = argument count.
  __ LoadRR(r7, r4);
  __ LoadRR(r8, r4);
  __ b(&try_allocate);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ LoadP(r7, MemOperand(r6, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiToPtrArrayOffset(r5, r7);
  __ AddP(r5, r5, r6);
  __ AddP(r5, r5, Operand(StandardFrameConstants::kCallerSPOffset));

  // r7 = argument count (tagged)
  // r8 = parameter count (tagged)
  // Compute the mapped parameter count = min(r4, r7) in r8.
  __ CmpP(r4, r7);
  Label skip;
  __ LoadRR(r8, r4);
  __ blt(&skip);
  __ LoadRR(r8, r7);
  __ bind(&skip);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  __ CmpSmiLiteral(r8, Smi::FromInt(0), r0);
  Label skip2, skip3;
  __ bne(&skip2);
  __ LoadImmP(r1, Operand::Zero());
  __ b(&skip3);
  __ bind(&skip2);
  __ SmiToPtrArrayOffset(r1, r8);
  __ AddP(r1, r1, Operand(kParameterMapHeaderSize));
  __ bind(&skip3);

  // 2. Backing store.
  __ SmiToPtrArrayOffset(r6, r7);
  __ AddP(r1, r1, r6);
  __ AddP(r1, r1, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ AddP(r1, r1, Operand(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(r1, r2, r1, r6, &runtime, NO_ALLOCATION_FLAGS);

  // r2 = address of new object(s) (tagged)
  // r4 = argument count (smi-tagged)
  // Get the arguments boilerplate from the current native context into r3.
  const int kNormalOffset =
      Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);

  __ LoadP(r6, NativeContextMemOperand());
  __ CmpP(r8, Operand::Zero());
  Label skip4, skip5;
  __ bne(&skip4);
  __ LoadP(r6, MemOperand(r6, kNormalOffset));
  __ b(&skip5);
  __ bind(&skip4);
  __ LoadP(r6, MemOperand(r6, kAliasedOffset));
  __ bind(&skip5);

  // r2 = address of new object (tagged)
  // r4 = argument count (smi-tagged)
  // r6 = address of arguments map (tagged)
  // r8 = mapped parameter count (tagged)
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kMapOffset), r0);
  __ LoadRoot(r1, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r1, FieldMemOperand(r2, JSObject::kPropertiesOffset), r0);
  __ StoreP(r1, FieldMemOperand(r2, JSObject::kElementsOffset), r0);

  // Set up the callee in-object property.
  __ AssertNotSmi(r3);
  __ StoreP(r3, FieldMemOperand(r2, JSSloppyArgumentsObject::kCalleeOffset),
            r0);

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(r7);
  __ StoreP(r7, FieldMemOperand(r2, JSSloppyArgumentsObject::kLengthOffset),
            r0);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, r6 will point there, otherwise
  // it will point to the backing store.
  __ AddP(r6, r2, Operand(JSSloppyArgumentsObject::kSize));
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kElementsOffset), r0);

  // r2 = address of new object (tagged)
  // r4 = argument count (tagged)
  // r6 = address of parameter map or backing store (tagged)
  // r8 = mapped parameter count (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ CmpSmiLiteral(r8, Smi::FromInt(0), r0);
  Label skip6;
  __ bne(&skip6);
  // Move backing store address to r3, because it is
  // expected there when filling in the unmapped arguments.
  __ LoadRR(r3, r6);
  __ b(&skip_parameter_map);
  __ bind(&skip6);

  __ LoadRoot(r7, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ StoreP(r7, FieldMemOperand(r6, FixedArray::kMapOffset), r0);
  __ AddSmiLiteral(r7, r8, Smi::FromInt(2), r0);
  __ StoreP(r7, FieldMemOperand(r6, FixedArray::kLengthOffset), r0);
  __ StoreP(cp, FieldMemOperand(r6, FixedArray::kHeaderSize + 0 * kPointerSize),
            r0);
  __ SmiToPtrArrayOffset(r7, r8);
  __ AddP(r7, r7, r6);
  __ AddP(r7, r7, Operand(kParameterMapHeaderSize));
  __ StoreP(r7, FieldMemOperand(r6, FixedArray::kHeaderSize + 1 * kPointerSize),
            r0);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop;
  __ LoadRR(r7, r8);
  __ AddSmiLiteral(r1, r4, Smi::FromInt(Context::MIN_CONTEXT_SLOTS), r0);
  __ SubP(r1, r1, r8);
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ SmiToPtrArrayOffset(r3, r7);
  __ AddP(r3, r3, r6);
  __ AddP(r3, r3, Operand(kParameterMapHeaderSize));

  // r3 = address of backing store (tagged)
  // r6 = address of parameter map (tagged)
  // r7 = temporary scratch (a.o., for address calculation)
  // r9 = temporary scratch (a.o., for address calculation)
  // ip = the hole value
  __ SmiUntag(r7);
  __ push(r4);
  __ LoadRR(r4, r7);
  __ ShiftLeftP(r7, r7, Operand(kPointerSizeLog2));
  __ AddP(r9, r3, r7);
  __ AddP(r7, r6, r7);
  __ AddP(r9, r9, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ AddP(r7, r7, Operand(kParameterMapHeaderSize - kHeapObjectTag));

  __ bind(&parameters_loop);
  __ StoreP(r1, MemOperand(r7, -kPointerSize));
  __ lay(r7, MemOperand(r7, -kPointerSize));
  __ StoreP(ip, MemOperand(r9, -kPointerSize));
  __ lay(r9, MemOperand(r9, -kPointerSize));
  __ AddSmiLiteral(r1, r1, Smi::FromInt(1), r0);
  __ BranchOnCount(r4, &parameters_loop);
  __ pop(r4);

  // Restore r7 = argument count (tagged).
  __ LoadP(r7, FieldMemOperand(r2, JSSloppyArgumentsObject::kLengthOffset));

  __ bind(&skip_parameter_map);
  // r2 = address of new object (tagged)
  // r3 = address of backing store (tagged)
  // r7 = argument count (tagged)
  // r8 = mapped parameter count (tagged)
  // r1 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(r1, Heap::kFixedArrayMapRootIndex);
  __ StoreP(r1, FieldMemOperand(r3, FixedArray::kMapOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, FixedArray::kLengthOffset), r0);
  __ SubP(r1, r7, r8);
  __ Ret(eq);

  Label arguments_loop;
  __ SmiUntag(r1);
  __ LoadRR(r4, r1);

  __ SmiToPtrArrayOffset(r0, r8);
  __ SubP(r5, r5, r0);
  __ AddP(r1, r3, r0);
  __ AddP(r1, r1,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));

  __ bind(&arguments_loop);
  __ LoadP(r6, MemOperand(r5, -kPointerSize));
  __ lay(r5, MemOperand(r5, -kPointerSize));
  __ StoreP(r6, MemOperand(r1, kPointerSize));
  __ la(r1, MemOperand(r1, kPointerSize));
  __ BranchOnCount(r4, &arguments_loop);

  // Return.
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // r7 = argument count (tagged)
  __ bind(&runtime);
  __ Push(r3, r5, r7);
  __ TailCallRuntime(Runtime::kNewSloppyArguments);
}

void FastNewStrictArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- lr : return address
  // -----------------------------------
  __ AssertFunction(r3);

  // Make r4 point to the JavaScript frame.
  __ LoadRR(r4, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ LoadP(r4, MemOperand(r4, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ LoadP(ip, MemOperand(r4, StandardFrameConstants::kFunctionOffset));
    __ CmpP(ip, r3);
    __ beq(&ok, Label::kNear);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ LoadP(r5, MemOperand(r4, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(ip, MemOperand(r5, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpSmiLiteral(ip, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&arguments_adaptor);
  {
    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadW(r2, FieldMemOperand(
                     r6, SharedFunctionInfo::kFormalParameterCountOffset));
#if V8_TARGET_ARCH_S390X
    __ SmiTag(r2);
#endif
    __ SmiToPtrArrayOffset(r8, r2);
    __ AddP(r4, r4, r8);
  }
  __ b(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    __ LoadP(r2, MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ SmiToPtrArrayOffset(r8, r2);
    __ AddP(r4, r5, r8);
  }
  __ bind(&arguments_done);
  __ AddP(r4, r4, Operand(StandardFrameConstants::kCallerSPOffset));

  // ----------- S t a t e -------------
  //  -- cp : context
  //  -- r2 : number of rest parameters (tagged)
  //  -- r3 : function
  //  -- r4 : pointer just past first rest parameters
  //  -- r8 : size of rest parameters
  //  -- lr : return address
  // -----------------------------------

  // Allocate space for the strict arguments object plus the backing store.
  Label allocate, done_allocate;
  __ mov(r9, Operand(JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize));
  __ AddP(r9, r9, r8);
  __ Allocate(r9, r5, r6, r7, &allocate, NO_ALLOCATION_FLAGS);
  __ bind(&done_allocate);

  // Setup the elements array in r5.
  __ LoadRoot(r3, Heap::kFixedArrayMapRootIndex);
  __ StoreP(r3, FieldMemOperand(r5, FixedArray::kMapOffset), r0);
  __ StoreP(r2, FieldMemOperand(r5, FixedArray::kLengthOffset), r0);
  __ AddP(r6, r5,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
  {
    Label loop, done_loop;
    __ SmiUntag(r1, r2);
    __ LoadAndTestP(r1, r1);
    __ beq(&done_loop);
    __ bind(&loop);
    __ lay(r4, MemOperand(r4, -kPointerSize));
    __ LoadP(ip, MemOperand(r4));
    __ la(r6, MemOperand(r6, kPointerSize));
    __ StoreP(ip, MemOperand(r6));
    __ BranchOnCount(r1, &loop);
    __ bind(&done_loop);
    __ AddP(r6, r6, Operand(kPointerSize));
  }

  // Setup the rest parameter array in r6.
  __ LoadNativeContextSlot(Context::STRICT_ARGUMENTS_MAP_INDEX, r3);
  __ StoreP(r3, MemOperand(r6, JSStrictArgumentsObject::kMapOffset));
  __ LoadRoot(r3, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r3, MemOperand(r6, JSStrictArgumentsObject::kPropertiesOffset));
  __ StoreP(r5, MemOperand(r6, JSStrictArgumentsObject::kElementsOffset));
  __ StoreP(r2, MemOperand(r6, JSStrictArgumentsObject::kLengthOffset));
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ AddP(r2, r6, Operand(kHeapObjectTag));
  __ Ret();

  // Fall back to %AllocateInNewSpace (if not too big).
  Label too_big_for_new_space;
  __ bind(&allocate);
  __ CmpP(r9, Operand(kMaxRegularHeapObjectSize));
  __ bgt(&too_big_for_new_space);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(r9);
    __ Push(r2, r4, r9);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ LoadRR(r5, r2);
    __ Pop(r2, r4);
  }
  __ b(&done_allocate);

  // Fall back to %NewStrictArguments.
  __ bind(&too_big_for_new_space);
  __ push(r3);
  __ TailCallRuntime(Runtime::kNewStrictArguments);
}

void StoreGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register value = r2;
  Register slot = r4;

  Register cell = r3;
  Register cell_details = r5;
  Register cell_value = r6;
  Register cell_value_map = r7;
  Register scratch = r8;

  Register context = cp;
  Register context_temp = cell;

  Label fast_heapobject_case, fast_smi_case, slow_case;

  if (FLAG_debug_code) {
    __ CompareRoot(value, Heap::kTheHoleValueRootIndex);
    __ Check(ne, kUnexpectedValue);
  }

  // Go up the context chain to the script context.
  for (int i = 0; i < depth(); i++) {
    __ LoadP(context_temp, ContextMemOperand(context, Context::PREVIOUS_INDEX));
    context = context_temp;
  }

  // Load the PropertyCell at the specified slot.
  __ ShiftLeftP(r0, slot, Operand(kPointerSizeLog2));
  __ AddP(cell, context, r0);
  __ LoadP(cell, ContextMemOperand(cell));

  // Load PropertyDetails for the cell (actually only the cell_type and kind).
  __ LoadP(cell_details, FieldMemOperand(cell, PropertyCell::kDetailsOffset));
  __ SmiUntag(cell_details);
  __ AndP(cell_details, cell_details,
          Operand(PropertyDetails::PropertyCellTypeField::kMask |
                  PropertyDetails::KindField::kMask |
                  PropertyDetails::kAttributesReadOnlyMask));

  // Check if PropertyCell holds mutable data.
  Label not_mutable_data;
  __ CmpP(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                    PropertyCellType::kMutable) |
                                PropertyDetails::KindField::encode(kData)));
  __ bne(&not_mutable_data);
  __ JumpIfSmi(value, &fast_smi_case);

  __ bind(&fast_heapobject_case);
  __ StoreP(value, FieldMemOperand(cell, PropertyCell::kValueOffset), r0);
  // RecordWriteField clobbers the value register, so we copy it before the
  // call.
  __ LoadRR(r5, value);
  __ RecordWriteField(cell, PropertyCell::kValueOffset, r5, scratch,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Ret();

  __ bind(&not_mutable_data);
  // Check if PropertyCell value matches the new value (relevant for Constant,
  // ConstantType and Undefined cells).
  Label not_same_value;
  __ LoadP(cell_value, FieldMemOperand(cell, PropertyCell::kValueOffset));
  __ CmpP(cell_value, value);
  __ bne(&not_same_value);

  // Make sure the PropertyCell is not marked READ_ONLY.
  __ AndP(r0, cell_details, Operand(PropertyDetails::kAttributesReadOnlyMask));
  __ bne(&slow_case);

  if (FLAG_debug_code) {
    Label done;
    // This can only be true for Constant, ConstantType and Undefined cells,
    // because we never store the_hole via this stub.
    __ CmpP(cell_details,
            Operand(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kConstant) |
                    PropertyDetails::KindField::encode(kData)));
    __ beq(&done);
    __ CmpP(cell_details,
            Operand(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kConstantType) |
                    PropertyDetails::KindField::encode(kData)));
    __ beq(&done);
    __ CmpP(cell_details,
            Operand(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kUndefined) |
                    PropertyDetails::KindField::encode(kData)));
    __ Check(eq, kUnexpectedValue);
    __ bind(&done);
  }
  __ Ret();
  __ bind(&not_same_value);

  // Check if PropertyCell contains data with constant type (and is not
  // READ_ONLY).
  __ CmpP(cell_details, Operand(PropertyDetails::PropertyCellTypeField::encode(
                                    PropertyCellType::kConstantType) |
                                PropertyDetails::KindField::encode(kData)));
  __ bne(&slow_case);

  // Now either both old and new values must be smis or both must be heap
  // objects with same map.
  Label value_is_heap_object;
  __ JumpIfNotSmi(value, &value_is_heap_object);
  __ JumpIfNotSmi(cell_value, &slow_case);
  // Old and new values are smis, no need for a write barrier here.
  __ bind(&fast_smi_case);
  __ StoreP(value, FieldMemOperand(cell, PropertyCell::kValueOffset), r0);
  __ Ret();

  __ bind(&value_is_heap_object);
  __ JumpIfSmi(cell_value, &slow_case);

  __ LoadP(cell_value_map, FieldMemOperand(cell_value, HeapObject::kMapOffset));
  __ LoadP(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  __ CmpP(cell_value_map, scratch);
  __ beq(&fast_heapobject_case);

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

  // Additional parameter is the address of the actual callback.
  DCHECK(function_address.is(r3) || function_address.is(r4));
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
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ SubP(r8, Operand(1));
  __ StoreW(r8, MemOperand(r9, kLevelOffset));
  __ CmpP(r7, MemOperand(r9, kLimitOffset));
  __ bne(&delete_allocated_handles, Label::kNear);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ LoadP(cp, *context_restore_operand);
  }
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand != NULL) {
    __ l(r6, *stack_space_operand);
  } else {
    __ mov(r6, Operand(stack_space));
  }
  __ LeaveExitFrame(false, r6, !restore_context, stack_space_operand != NULL);

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
  //  -- r2                  : callee
  //  -- r6                  : call_data
  //  -- r4                  : holder
  //  -- r3                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 4]   : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register callee = r2;
  Register call_data = r6;
  Register holder = r4;
  Register api_function_address = r3;
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

  DCHECK(!api_function_address.is(r2) && !scratch.is(r2));
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
  __ Push(Smi::FromInt(0));  // should_throw_on_error -> false
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
                           kStackUnwindSpace, NULL, return_value_operand, NULL);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
