// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/code-stubs.h"
#include "src/api-arguments.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/mips64/code-stubs-mips64.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ dsll(t9, a0, kPointerSizeLog2);
  __ Daddu(t9, sp, t9);
  __ Sd(a1, MemOperand(t9, 0));
  __ Push(a1);
  __ Push(a2);
  __ Daddu(a0, a0, 3);
  __ TailCallRuntime(Runtime::kNewArray);
}

static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cc);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* rhs_not_nan,
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
    FrameScope scope(masm, StackFrame::INTERNAL);
    DCHECK((param_count == 0) ||
           a0.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments, adjust sp.
    __ Dsubu(sp, sp, Operand(param_count * kPointerSize));
    for (int i = 0; i < param_count; ++i) {
      // Store argument to stack.
      __ Sd(descriptor.GetRegisterParameter(i),
            MemOperand(sp, (param_count - 1 - i) * kPointerSize));
    }
    __ CallExternalReference(miss, param_count);
  }

  __ Ret();
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done;
  Register input_reg = source();
  Register result_reg = destination();

  int double_offset = offset();
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += 3 * kPointerSize;

  Register scratch =
      GetRegisterThatIsNotOneOf(input_reg, result_reg);
  Register scratch2 =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch);
  Register scratch3 =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch, scratch2);
  DoubleRegister double_scratch = kLithiumScratchDouble;

  __ Push(scratch, scratch2, scratch3);
  if (!skip_fastpath()) {
    // Load double input.
    __ Ldc1(double_scratch, MemOperand(input_reg, double_offset));

    // Clear cumulative exception flags and save the FCSR.
    __ cfc1(scratch2, FCSR);
    __ ctc1(zero_reg, FCSR);

    // Try a conversion to a signed integer.
    __ Trunc_w_d(double_scratch, double_scratch);
    // Move the converted value into the result register.
    __ mfc1(scratch3, double_scratch);

    // Retrieve and restore the FCSR.
    __ cfc1(scratch, FCSR);
    __ ctc1(scratch2, FCSR);

    // Check for overflow and NaNs.
    __ And(
        scratch, scratch,
        kFCSROverflowFlagMask | kFCSRUnderflowFlagMask
           | kFCSRInvalidOpFlagMask);
    // If we had no exceptions then set result_reg and we are done.
    Label error;
    __ Branch(&error, ne, scratch, Operand(zero_reg));
    __ Move(result_reg, scratch3);
    __ Branch(&done);
    __ bind(&error);
  }

  // Load the double value and perform a manual truncation.
  Register input_high = scratch2;
  Register input_low = scratch3;

  __ Lw(input_low,
        MemOperand(input_reg, double_offset + Register::kMantissaOffset));
  __ Lw(input_high,
        MemOperand(input_reg, double_offset + Register::kExponentOffset));

  Label normal_exponent, restore_sign;
  // Extract the biased exponent in result.
  __ Ext(result_reg,
         input_high,
         HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  __ Subu(scratch, result_reg, HeapNumber::kExponentMask);
  __ Movz(result_reg, zero_reg, scratch);
  __ Branch(&done, eq, scratch, Operand(zero_reg));

  // Express exponent as delta to (number of mantissa bits + 31).
  __ Subu(result_reg,
          result_reg,
          Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31));

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  __ Branch(&normal_exponent, le, result_reg, Operand(zero_reg));
  __ mov(result_reg, zero_reg);
  __ Branch(&done);

  __ bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  __ Addu(scratch, result_reg, Operand(kShiftBase + HeapNumber::kMantissaBits));

  // Save the sign.
  Register sign = result_reg;
  result_reg = no_reg;
  __ And(sign, input_high, Operand(HeapNumber::kSignMask));

  // On ARM shifts > 31 bits are valid and will result in zero. On MIPS we need
  // to check for this specific case.
  Label high_shift_needed, high_shift_done;
  __ Branch(&high_shift_needed, lt, scratch, Operand(32));
  __ mov(input_high, zero_reg);
  __ Branch(&high_shift_done);
  __ bind(&high_shift_needed);

  // Set the implicit 1 before the mantissa part in input_high.
  __ Or(input_high,
        input_high,
        Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  __ sllv(input_high, input_high, scratch);

  __ bind(&high_shift_done);

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done;
  __ li(at, 32);
  __ subu(scratch, at, scratch);
  __ Branch(&pos_shift, ge, scratch, Operand(zero_reg));

  // Negate scratch.
  __ Subu(scratch, zero_reg, scratch);
  __ sllv(input_low, input_low, scratch);
  __ Branch(&shift_done);

  __ bind(&pos_shift);
  __ srlv(input_low, input_low, scratch);

  __ bind(&shift_done);
  __ Or(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  __ mov(scratch, sign);
  result_reg = sign;
  sign = no_reg;
  __ Subu(result_reg, zero_reg, input_high);
  __ Movz(result_reg, input_high, scratch);

  __ bind(&done);

  __ Pop(scratch, scratch2, scratch3);
  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm, Label* slow,
                                          Condition cc) {
  Label not_identical;
  Label heap_number, return_equal;
  Register exp_mask_reg = t1;

  __ Branch(&not_identical, ne, a0, Operand(a1));

  __ li(exp_mask_reg, Operand(HeapNumber::kExponentMask));

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis. If it's not a heap number, then return equal.
  __ GetObjectType(a0, t0, t0);
  if (cc == less || cc == greater) {
    // Call runtime on identical JSObjects.
    __ Branch(slow, greater, t0, Operand(FIRST_JS_RECEIVER_TYPE));
    // Call runtime on identical symbols since we need to throw a TypeError.
    __ Branch(slow, eq, t0, Operand(SYMBOL_TYPE));
  } else {
    __ Branch(&heap_number, eq, t0, Operand(HEAP_NUMBER_TYPE));
    // Comparing JS objects with <=, >= is complicated.
    if (cc != eq) {
      __ Branch(slow, greater, t0, Operand(FIRST_JS_RECEIVER_TYPE));
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ Branch(slow, eq, t0, Operand(SYMBOL_TYPE));
      // Normally here we fall through to return_equal, but undefined is
      // special: (undefined == undefined) == true, but
      // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
      if (cc == less_equal || cc == greater_equal) {
        __ Branch(&return_equal, ne, t0, Operand(ODDBALL_TYPE));
        __ LoadRoot(a6, Heap::kUndefinedValueRootIndex);
        __ Branch(&return_equal, ne, a0, Operand(a6));
        DCHECK(is_int16(GREATER) && is_int16(LESS));
        __ Ret(USE_DELAY_SLOT);
        if (cc == le) {
          // undefined <= undefined should fail.
          __ li(v0, Operand(GREATER));
        } else  {
          // undefined >= undefined should fail.
          __ li(v0, Operand(LESS));
        }
      }
    }
  }

  __ bind(&return_equal);
  DCHECK(is_int16(GREATER) && is_int16(LESS));
  __ Ret(USE_DELAY_SLOT);
  if (cc == less) {
    __ li(v0, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cc == greater) {
    __ li(v0, Operand(LESS));     // Things aren't greater than themselves.
  } else {
    __ mov(v0, zero_reg);         // Things are <=, >=, ==, === themselves.
  }
  // For less and greater we don't have to check for NaN since the result of
  // x < x is false regardless.  For the others here is some code to check
  // for NaN.
  if (cc != lt && cc != gt) {
    __ bind(&heap_number);
    // It is a heap number, so return non-equal if it's NaN and equal if it's
    // not NaN.

    // The representation of NaN values has all exponent bits (52..62) set,
    // and not all mantissa bits (0..51) clear.
    // Read top bits of double representation (second word of value).
    __ Lwu(a6, FieldMemOperand(a0, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    __ And(a7, a6, Operand(exp_mask_reg));
    // If all bits not set (ne cond), then not a NaN, objects are equal.
    __ Branch(&return_equal, ne, a7, Operand(exp_mask_reg));

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ sll(a6, a6, HeapNumber::kNonMantissaBitsInTopWord);
    // Or with all low-bits of mantissa.
    __ Lwu(a7, FieldMemOperand(a0, HeapNumber::kMantissaOffset));
    __ Or(v0, a7, Operand(a6));
    // For equal we already have the right value in v0:  Return zero (equal)
    // if all bits in mantissa are zero (it's an Infinity) and non-zero if
    // not (it's a NaN).  For <= and >= we need to load v0 with the failing
    // value if it's a NaN.
    if (cc != eq) {
      // All-zero means Infinity means equal.
      __ Ret(eq, v0, Operand(zero_reg));
      DCHECK(is_int16(GREATER) && is_int16(LESS));
      __ Ret(USE_DELAY_SLOT);
      if (cc == le) {
        __ li(v0, Operand(GREATER));  // NaN <= NaN should fail.
      } else {
        __ li(v0, Operand(LESS));     // NaN >= NaN should fail.
      }
    }
  }
  // No fall through here.

  __ bind(&not_identical);
}


static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* both_loaded_as_doubles,
                                    Label* slow,
                                    bool strict) {
  DCHECK((lhs.is(a0) && rhs.is(a1)) ||
         (lhs.is(a1) && rhs.is(a0)));

  Label lhs_is_smi;
  __ JumpIfSmi(lhs, &lhs_is_smi);
  // Rhs is a Smi.
  // Check whether the non-smi is a heap number.
  __ GetObjectType(lhs, t0, t0);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed. Return non-equal (lhs is already not zero).
    __ Ret(USE_DELAY_SLOT, ne, t0, Operand(HEAP_NUMBER_TYPE));
    __ mov(v0, lhs);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number. Call
    // the runtime.
    __ Branch(slow, ne, t0, Operand(HEAP_NUMBER_TYPE));
  }
  // Rhs is a smi, lhs is a number.
  // Convert smi rhs to double.
  __ SmiUntag(at, rhs);
  __ mtc1(at, f14);
  __ cvt_d_w(f14, f14);
  __ Ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));

  // We now have both loaded as doubles.
  __ jmp(both_loaded_as_doubles);

  __ bind(&lhs_is_smi);
  // Lhs is a Smi.  Check whether the non-smi is a heap number.
  __ GetObjectType(rhs, t0, t0);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed. Return non-equal.
    __ Ret(USE_DELAY_SLOT, ne, t0, Operand(HEAP_NUMBER_TYPE));
    __ li(v0, Operand(1));
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number. Call
    // the runtime.
    __ Branch(slow, ne, t0, Operand(HEAP_NUMBER_TYPE));
  }

  // Lhs is a smi, rhs is a number.
  // Convert smi lhs to double.
  __ SmiUntag(at, lhs);
  __ mtc1(at, f12);
  __ cvt_d_w(f12, f12);
  __ Ldc1(f14, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  // Fall through to both_loaded_as_doubles.
}


static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs) {
    // If either operand is a JS object or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    Label first_non_object;
    // Get the type of the first operand into a2 and compare it with
    // FIRST_JS_RECEIVER_TYPE.
    __ GetObjectType(lhs, a2, a2);
    __ Branch(&first_non_object, less, a2, Operand(FIRST_JS_RECEIVER_TYPE));

    // Return non-zero.
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ Ret(USE_DELAY_SLOT);
    __ li(v0, Operand(1));

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ Branch(&return_not_equal, eq, a2, Operand(ODDBALL_TYPE));

    __ GetObjectType(rhs, a3, a3);
    __ Branch(&return_not_equal, greater, a3, Operand(FIRST_JS_RECEIVER_TYPE));

    // Check for oddballs: true, false, null, undefined.
    __ Branch(&return_not_equal, eq, a3, Operand(ODDBALL_TYPE));

    // Now that we have the types we might as well check for
    // internalized-internalized.
    STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
    __ Or(a2, a2, Operand(a3));
    __ And(at, a2, Operand(kIsNotStringMask | kIsNotInternalizedMask));
    __ Branch(&return_not_equal, eq, at, Operand(zero_reg));
}


static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  __ GetObjectType(lhs, a3, a2);
  __ Branch(not_heap_numbers, ne, a2, Operand(HEAP_NUMBER_TYPE));
  __ Ld(a2, FieldMemOperand(rhs, HeapObject::kMapOffset));
  // If first was a heap number & second wasn't, go to slow case.
  __ Branch(slow, ne, a3, Operand(a2));

  // Both are heap numbers. Load them up then jump to the code we have
  // for that.
  __ Ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  __ Ldc1(f14, FieldMemOperand(rhs, HeapNumber::kValueOffset));

  __ jmp(both_loaded_as_doubles);
}


// Fast negative check for internalized-to-internalized equality.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register lhs, Register rhs,
                                                     Label* possible_strings,
                                                     Label* runtime_call) {
  DCHECK((lhs.is(a0) && rhs.is(a1)) ||
         (lhs.is(a1) && rhs.is(a0)));

  // a2 is object type of rhs.
  Label object_test, return_equal, return_unequal, undetectable;
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ And(at, a2, Operand(kIsNotStringMask));
  __ Branch(&object_test, ne, at, Operand(zero_reg));
  __ And(at, a2, Operand(kIsNotInternalizedMask));
  __ Branch(possible_strings, ne, at, Operand(zero_reg));
  __ GetObjectType(rhs, a3, a3);
  __ Branch(runtime_call, ge, a3, Operand(FIRST_NONSTRING_TYPE));
  __ And(at, a3, Operand(kIsNotInternalizedMask));
  __ Branch(possible_strings, ne, at, Operand(zero_reg));

  // Both are internalized. We already checked they weren't the same pointer so
  // they are not equal. Return non-equal by returning the non-zero object
  // pointer in v0.
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);  // In delay slot.

  __ bind(&object_test);
  __ Ld(a2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ Ld(a3, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ Lbu(t0, FieldMemOperand(a2, Map::kBitFieldOffset));
  __ Lbu(t1, FieldMemOperand(a3, Map::kBitFieldOffset));
  __ And(at, t0, Operand(1 << Map::kIsUndetectable));
  __ Branch(&undetectable, ne, at, Operand(zero_reg));
  __ And(at, t1, Operand(1 << Map::kIsUndetectable));
  __ Branch(&return_unequal, ne, at, Operand(zero_reg));

  __ GetInstanceType(a2, a2);
  __ Branch(runtime_call, lt, a2, Operand(FIRST_JS_RECEIVER_TYPE));
  __ GetInstanceType(a3, a3);
  __ Branch(runtime_call, lt, a3, Operand(FIRST_JS_RECEIVER_TYPE));

  __ bind(&return_unequal);
  // Return non-equal by returning the non-zero object pointer in v0.
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);  // In delay slot.

  __ bind(&undetectable);
  __ And(at, t1, Operand(1 << Map::kIsUndetectable));
  __ Branch(&return_unequal, eq, at, Operand(zero_reg));

  // If both sides are JSReceivers, then the result is false according to
  // the HTML specification, which says that only comparisons with null or
  // undefined are affected by special casing for document.all.
  __ GetInstanceType(a2, a2);
  __ Branch(&return_equal, eq, a2, Operand(ODDBALL_TYPE));
  __ GetInstanceType(a3, a3);
  __ Branch(&return_unequal, ne, a3, Operand(ODDBALL_TYPE));

  __ bind(&return_equal);
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(EQUAL));  // In delay slot.
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
  // We could be strict about internalized/string here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ bind(&ok);
}


// On entry a1 and a2 are the values to be compared.
// On exit a0 is 0, positive or negative to indicate the result of
// the comparison.
void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Register lhs = a1;
  Register rhs = a0;
  Condition cc = GetCondition();

  Label miss;
  CompareICStub_CheckInputType(masm, lhs, a2, left(), &miss);
  CompareICStub_CheckInputType(masm, rhs, a3, right(), &miss);

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles;

  Label not_two_smis, smi_done;
  __ Or(a2, a1, a0);
  __ JumpIfNotSmi(a2, &not_two_smis);
  __ SmiUntag(a1);
  __ SmiUntag(a0);

  __ Ret(USE_DELAY_SLOT);
  __ dsubu(v0, a1, a0);
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
  __ And(a6, lhs, Operand(rhs));
  __ JumpIfNotSmi(a6, &not_smis, a4);
  // One operand is a smi. EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to rhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison and the numbers have been loaded into f12 and f14 as doubles,
  // or in GP registers (a0, a1, a2, a3) depending on the presence of the FPU.
  EmitSmiNonsmiComparison(masm, lhs, rhs,
                          &both_loaded_as_doubles, &slow, strict());

  __ bind(&both_loaded_as_doubles);
  // f12, f14 are the double representations of the left hand side
  // and the right hand side if we have FPU. Otherwise a2, a3 represent
  // left hand side and a0, a1 represent right hand side.

  Label nan;
  __ li(a4, Operand(LESS));
  __ li(a5, Operand(GREATER));
  __ li(a6, Operand(EQUAL));

  // Check if either rhs or lhs is NaN.
  __ BranchF(NULL, &nan, eq, f12, f14);

  // Check if LESS condition is satisfied. If true, move conditionally
  // result to v0.
  if (kArchVariant != kMips64r6) {
    __ c(OLT, D, f12, f14);
    __ Movt(v0, a4);
    // Use previous check to store conditionally to v0 oposite condition
    // (GREATER). If rhs is equal to lhs, this will be corrected in next
    // check.
    __ Movf(v0, a5);
    // Check if EQUAL condition is satisfied. If true, move conditionally
    // result to v0.
    __ c(EQ, D, f12, f14);
    __ Movt(v0, a6);
  } else {
    Label skip;
    __ BranchF(USE_DELAY_SLOT, &skip, NULL, lt, f12, f14);
    __ mov(v0, a4);  // Return LESS as result.

    __ BranchF(USE_DELAY_SLOT, &skip, NULL, eq, f12, f14);
    __ mov(v0, a6);  // Return EQUAL as result.

    __ mov(v0, a5);  // Return GREATER as result.
    __ bind(&skip);
  }
  __ Ret();

  __ bind(&nan);
  // NaN comparisons always fail.
  // Load whatever we need in v0 to make the comparison fail.
  DCHECK(is_int16(GREATER) && is_int16(LESS));
  __ Ret(USE_DELAY_SLOT);
  if (cc == lt || cc == le) {
    __ li(v0, Operand(GREATER));
  } else {
    __ li(v0, Operand(LESS));
  }


  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi. The objects are in lhs_ and rhs_.
  if (strict()) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs, rhs);
  }

  Label check_for_internalized_strings;
  Label flat_string_check;
  // Check for heap-number-heap-number comparison. Can jump to slow case,
  // or load both doubles and jump to the code that handles
  // that case. If the inputs are not doubles then jumps to
  // check_for_internalized_strings.
  // In this case a2 will contain the type of lhs_.
  EmitCheckForTwoHeapNumbers(masm,
                             lhs,
                             rhs,
                             &both_loaded_as_doubles,
                             &check_for_internalized_strings,
                             &flat_string_check);

  __ bind(&check_for_internalized_strings);
  if (cc == eq && !strict()) {
    // Returns an answer for two internalized strings or two
    // detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that a2 is the type of lhs_ on entry.
    EmitCheckForInternalizedStringsOrObjects(
        masm, lhs, rhs, &flat_string_check, &slow);
  }

  // Check for both being sequential one-byte strings,
  // and inline if that is the case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialOneByteStrings(lhs, rhs, a2, a3, &slow);

  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, a2,
                      a3);
  if (cc == eq) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, a2, a3, a4);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, a2, a3, a4,
                                                    a5);
  }
  // Never falls through to here.

  __ bind(&slow);
  if (cc == eq) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(cp);
      __ Call(strict() ? isolate()->builtins()->StrictEqual()
                       : isolate()->builtins()->Equal(),
              RelocInfo::CODE_TARGET);
      __ Pop(cp);
    }
    // Turn true into 0 and false into some non-zero value.
    STATIC_ASSERT(EQUAL == 0);
    __ LoadRoot(a0, Heap::kTrueValueRootIndex);
    __ Ret(USE_DELAY_SLOT);
    __ subu(v0, v0, a0);  // In delay slot.
  } else {
    // Prepare for call to builtin. Push object pointers, a0 (lhs) first,
    // a1 (rhs) second.
    __ Push(lhs, rhs);
    int ncr;  // NaN compare result.
    if (cc == lt || cc == le) {
      ncr = GREATER;
    } else {
      DCHECK(cc == gt || cc == ge);  // Remaining cases.
      ncr = LESS;
    }
    __ li(a0, Operand(Smi::FromInt(ncr)));
    __ push(a0);

    // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
    // tagged as a small integer.
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void StoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ mov(t9, ra);
  __ pop(ra);
  __ PushSafepointRegisters();
  __ Jump(t9);
}


void RestoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ mov(t9, ra);
  __ pop(ra);
  __ PopSafepointRegisters();
  __ Jump(t9);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ MultiPush(kJSCallerSaved | ra.bit());
  if (save_doubles()) {
    __ MultiPushFPU(kCallerSavedFPU);
  }
  const int argument_count = 1;
  const int fp_argument_count = 0;
  const Register scratch = a1;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, fp_argument_count, scratch);
  __ li(a0, Operand(ExternalReference::isolate_address(isolate())));
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()),
      argument_count);
  if (save_doubles()) {
    __ MultiPopFPU(kCallerSavedFPU);
  }

  __ MultiPop(kJSCallerSaved | ra.bit());
  __ Ret();
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(a2));
  const DoubleRegister double_base = f2;
  const DoubleRegister double_exponent = f4;
  const DoubleRegister double_result = f0;
  const DoubleRegister double_scratch = f6;
  const FPURegister single_scratch = f8;
  const Register scratch = t1;
  const Register scratch2 = a7;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == TAGGED) {
    // Base is already in double_base.
    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ Ldc1(double_exponent,
            FieldMemOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type() != INTEGER) {
    Label int_exponent_convert;
    // Detect integer exponents stored as double.
    __ EmitFPUTruncate(kRoundToMinusInf,
                       scratch,
                       double_exponent,
                       at,
                       double_scratch,
                       scratch2,
                       kCheckForInexactConversion);
    // scratch2 == 0 means there was no conversion error.
    __ Branch(&int_exponent_convert, eq, scratch2, Operand(zero_reg));

    __ push(ra);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch2);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()),
          0, 2);
    }
    __ pop(ra);
    __ MovFromFloatResult(double_result);
    __ jmp(&done);

    __ bind(&int_exponent_convert);
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

  __ mov_d(double_scratch, double_base);  // Back up base.
  __ Move(double_result, 1.0);

  // Get absolute value of exponent.
  Label positive_exponent, bail_out;
  __ Branch(&positive_exponent, ge, scratch, Operand(zero_reg));
  __ Dsubu(scratch, zero_reg, scratch);
  // Check when Dsubu overflows and we get negative result
  // (happens only when input is MIN_INT).
  __ Branch(&bail_out, gt, zero_reg, Operand(scratch));
  __ bind(&positive_exponent);
  __ Assert(ge, kUnexpectedNegativeValue, scratch, Operand(zero_reg));

  Label while_true, no_carry, loop_end;
  __ bind(&while_true);

  __ And(scratch2, scratch, 1);

  __ Branch(&no_carry, eq, scratch2, Operand(zero_reg));
  __ mul_d(double_result, double_result, double_scratch);
  __ bind(&no_carry);

  __ dsra(scratch, scratch, 1);

  __ Branch(&loop_end, eq, scratch, Operand(zero_reg));
  __ mul_d(double_scratch, double_scratch, double_scratch);

  __ Branch(&while_true);

  __ bind(&loop_end);

  __ Branch(&done, ge, exponent, Operand(zero_reg));
  __ Move(double_scratch, 1.0);
  __ div_d(double_result, double_scratch, double_result);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ BranchF(&done, NULL, ne, double_result, kDoubleRegZero);

  // double_exponent may not contain the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ bind(&bail_out);
  __ mtc1(exponent, single_scratch);
  __ cvt_d_w(double_exponent, single_scratch);

  // Returning or bailing out.
  __ push(ra);
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(0, 2, scratch);
    __ MovToFloatParameters(double_base, double_exponent);
    __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                     0, 2);
  }
  __ pop(ra);
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
  // Called from JavaScript; parameters are on stack as if calling JS function
  // a0: number of arguments including receiver
  // a1: pointer to builtin function
  // fp: frame pointer    (restored after C call)
  // sp: stack pointer    (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_in_register():
  // a2: pointer to the first argument

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  if (argv_in_register()) {
    // Move argv into the correct register.
    __ mov(s1, a2);
  } else {
    // Compute the argv pointer in a callee-saved register.
    __ Dlsa(s1, sp, a0, kPointerSizeLog2);
    __ Dsubu(s1, s1, kPointerSize);
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(save_doubles(), 0, is_builtin_exit()
                                           ? StackFrame::BUILTIN_EXIT
                                           : StackFrame::EXIT);

  // s0: number of arguments  including receiver (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  // Prepare arguments for C routine.
  // a0 = argc
  __ mov(s0, a0);
  __ mov(s2, a1);

  // We are calling compiled C/C++ code. a0 and a1 hold our two arguments. We
  // also need to reserve the 4 argument slots on the stack.

  __ AssertStackIsAligned();

  int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  int result_stack_size;
  if (result_size() <= 2) {
    // a0 = argc, a1 = argv, a2 = isolate
    __ li(a2, Operand(ExternalReference::isolate_address(isolate())));
    __ mov(a1, s1);
    result_stack_size = 0;
  } else {
    DCHECK_EQ(3, result_size());
    // Allocate additional space for the result.
    result_stack_size =
        ((result_size() * kPointerSize) + frame_alignment_mask) &
        ~frame_alignment_mask;
    __ Dsubu(sp, sp, Operand(result_stack_size));

    // a0 = hidden result argument, a1 = argc, a2 = argv, a3 = isolate.
    __ li(a3, Operand(ExternalReference::isolate_address(isolate())));
    __ mov(a2, s1);
    __ mov(a1, a0);
    __ mov(a0, sp);
  }

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  { Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm);
    int kNumInstructionsToJump = 4;
    Label find_ra;
    // Adjust the value in ra to point to the correct return location, 2nd
    // instruction past the real call into C code (the jalr(t9)), and push it.
    // This is the return address of the exit frame.
    if (kArchVariant >= kMips64r6) {
      __ addiupc(ra, kNumInstructionsToJump + 1);
    } else {
      // This branch-and-link sequence is needed to find the current PC on mips
      // before r6, saved to the ra register.
      __ bal(&find_ra);  // bal exposes branch delay slot.
      __ Daddu(ra, ra, kNumInstructionsToJump * Instruction::kInstrSize);
    }
    __ bind(&find_ra);

    // This spot was reserved in EnterExitFrame.
    __ Sd(ra, MemOperand(sp, result_stack_size));
    // Stack space reservation moved to the branch delay slot below.
    // Stack is still aligned.

    // Call the C routine.
    __ mov(t9, s2);  // Function pointer to t9 to conform to ABI for PIC.
    __ jalr(t9);
    // Set up sp in the delay slot.
    __ daddiu(sp, sp, -kCArgsSlotsSize);
    // Make sure the stored 'ra' points to this position.
    DCHECK_EQ(kNumInstructionsToJump,
              masm->InstructionsGeneratedSince(&find_ra));
  }
  if (result_size() > 2) {
    DCHECK_EQ(3, result_size());
    // Read result values stored on stack.
    __ Ld(a0, MemOperand(v0, 2 * kPointerSize));
    __ Ld(v1, MemOperand(v0, 1 * kPointerSize));
    __ Ld(v0, MemOperand(v0, 0 * kPointerSize));
  }
  // Result returned in v0, v1:v0 or a0:v1:v0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ LoadRoot(a4, Heap::kExceptionRootIndex);
  __ Branch(&exception_returned, eq, a4, Operand(v0));

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address(
        Isolate::kPendingExceptionAddress, isolate());
    __ li(a2, Operand(pending_exception_address));
    __ Ld(a2, MemOperand(a2));
    __ LoadRoot(a4, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ Branch(&okay, eq, a4, Operand(a2));
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // v0:v1: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc;
  if (argv_in_register()) {
    // We don't want to pop arguments so set argc to no_reg.
    argc = no_reg;
  } else {
    // s0: still holds argc (callee-saved).
    argc = s0;
  }
  __ LeaveExitFrame(save_doubles(), argc, true, EMIT_RETURN);

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

  // Ask the runtime for help to determine the handler. This will set v0 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, a0);
    __ mov(a0, zero_reg);
    __ mov(a1, zero_reg);
    __ li(a2, Operand(ExternalReference::isolate_address(isolate())));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ li(cp, Operand(pending_handler_context_address));
  __ Ld(cp, MemOperand(cp));
  __ li(sp, Operand(pending_handler_sp_address));
  __ Ld(sp, MemOperand(sp));
  __ li(fp, Operand(pending_handler_fp_address));
  __ Ld(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label zero;
  __ Branch(&zero, eq, cp, Operand(zero_reg));
  __ Sd(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&zero);

  // Compute the handler entry address and jump to it.
  __ li(a1, Operand(pending_handler_code_address));
  __ Ld(a1, MemOperand(a1));
  __ li(a2, Operand(pending_handler_offset_address));
  __ Ld(a2, MemOperand(a2));
  __ Daddu(a1, a1, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Daddu(t9, a1, a2);
  __ Jump(t9);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Isolate* isolate = masm->isolate();

  // TODO(plind): unify the ABI description here.
  // Registers:
  // a0: entry address
  // a1: function
  // a2: receiver
  // a3: argc
  // a4 (a4): on mips64

  // Stack:
  // 0 arg slots on mips64 (4 args slots on mips)
  // args -- in a4/a4 on mips64, on stack on mips

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Save callee saved registers on the stack.
  __ MultiPush(kCalleeSaved | ra.bit());

  // Save callee-saved FPU registers.
  __ MultiPushFPU(kCalleeSavedFPU);
  // Set up the reserved register for 0.0.
  __ Move(kDoubleRegZero, 0.0);

  // Load argv in s0 register.
  __ mov(s0, a4);  // 5th parameter in mips64 a4 (a4) register.

  __ InitializeRootRegister();

  // We build an EntryFrame.
  __ li(a7, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  StackFrame::Type marker = type();
  __ li(a6, Operand(StackFrame::TypeToMarker(marker)));
  __ li(a5, Operand(StackFrame::TypeToMarker(marker)));
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate);
  __ li(a4, Operand(c_entry_fp));
  __ Ld(a4, MemOperand(a4));
  __ Push(a7, a6, a5, a4);
  // Set up frame pointer for the frame to be pushed.
  __ daddiu(fp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: receiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // caller fp          |
  // function slot      | entry frame
  // context slot       |
  // bad fp (0xff...f)  |
  // callee saved registers + ra
  // [ O32: 4 args slots]
  // args

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate);
  __ li(a5, Operand(ExternalReference(js_entry_sp)));
  __ Ld(a6, MemOperand(a5));
  __ Branch(&non_outermost_js, ne, a6, Operand(zero_reg));
  __ Sd(fp, MemOperand(a5));
  __ li(a4, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont);
  __ nop();   // Branch delay slot nop.
  __ bind(&non_outermost_js);
  __ li(a4, Operand(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);
  __ push(a4);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ li(a4, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      isolate)));
  __ Sd(v0, MemOperand(a4));  // We come back from 'invoke'. result is in v0.
  __ LoadRoot(v0, Heap::kExceptionRootIndex);
  __ b(&exit);  // b exposes branch delay slot.
  __ nop();   // Branch delay slot nop.

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bal(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: receiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // handler frame
  // entry frame
  // callee saved registers + ra
  // [ O32: 4 args slots]
  // args

  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate);
    __ li(a4, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, masm->isolate());
    __ li(a4, Operand(entry));
  }
  __ Ld(t9, MemOperand(a4));  // Deref address.
  // Call JSEntryTrampoline.
  __ daddiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  __ Call(t9);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // v0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(a5);
  __ Branch(&non_outermost_js_2, ne, a5,
            Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ li(a5, Operand(ExternalReference(js_entry_sp)));
  __ Sd(zero_reg, MemOperand(a5));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(a5);
  __ li(a4, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                      isolate)));
  __ Sd(a5, MemOperand(a4));

  // Reset the stack to the callee saved registers.
  __ daddiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Restore callee-saved fpu registers.
  __ MultiPopFPU(kCalleeSavedFPU);

  // Restore callee saved registers from the stack.
  __ MultiPop(kCalleeSaved | ra.bit());
  // Return.
  __ Jump(ra);
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // a0 : number of arguments to the construct function
  // a2 : feedback vector
  // a3 : slot in feedback vector (Smi)
  // a1 : the function to call
  FrameScope scope(masm, StackFrame::INTERNAL);
  const RegList kSavedRegs = 1 << 4 |  // a0
                             1 << 5 |  // a1
                             1 << 6 |  // a2
                             1 << 7 |  // a3
                             1 << cp.code();

  // Number-of-arguments register must be smi-tagged to call out.
  __ SmiTag(a0);
  __ MultiPush(kSavedRegs);

  __ CallStub(stub);

  __ MultiPop(kSavedRegs);
  __ SmiUntag(a0);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // a0 : number of arguments to the construct function
  // a1 : the function to call
  // a2 : feedback vector
  // a3 : slot in feedback vector (Smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*FeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*FeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  // Load the cache state into a5.
  __ dsrl(a5, a3, 32 - kPointerSizeLog2);
  __ Daddu(a5, a2, Operand(a5));
  __ Ld(a5, FieldMemOperand(a5, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if a5 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = a6;
  Register weak_value = t0;
  __ Ld(weak_value, FieldMemOperand(a5, WeakCell::kValueOffset));
  __ Branch(&done, eq, a1, Operand(weak_value));
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&done, eq, a5, Operand(at));
  __ Ld(feedback_map, FieldMemOperand(a5, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kWeakCellMapRootIndex);
  __ Branch(&check_allocation_site, ne, feedback_map, Operand(at));

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ JumpIfSmi(weak_value, &initialize);
  __ jmp(&megamorphic);

  __ bind(&check_allocation_site);
  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the slot either some other function or an
  // AllocationSite.
  __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
  __ Branch(&miss, ne, feedback_map, Operand(at));

  // Make sure the function is the Array() function
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, a5);
  __ Branch(&megamorphic, ne, a1, Operand(a5));
  __ jmp(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ LoadRoot(at, Heap::kuninitialized_symbolRootIndex);
  __ Branch(&initialize, eq, a5, Operand(at));
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ dsrl(a5, a3, 32 - kPointerSizeLog2);
  __ Daddu(a5, a2, Operand(a5));
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Sd(at, FieldMemOperand(a5, FixedArray::kHeaderSize));
  __ jmp(&done);

  // An uninitialized cache is patched with the function.
  __ bind(&initialize);
  // Make sure the function is the Array() function.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, a5);
  __ Branch(&not_array_function, ne, a1, Operand(a5));

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the
  // slot.
  CreateAllocationSiteStub create_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &create_stub);
  __ Branch(&done);

  __ bind(&not_array_function);

  CreateWeakCellStub weak_cell_stub(masm->isolate());
  CallStubInRecordCallTarget(masm, &weak_cell_stub);

  __ bind(&done);

  // Increment the call count for all function calls.
  __ SmiScale(a4, a3, kPointerSizeLog2);
  __ Daddu(a5, a2, Operand(a4));
  __ Ld(a4, FieldMemOperand(a5, FixedArray::kHeaderSize + kPointerSize));
  __ Daddu(a4, a4, Operand(Smi::FromInt(1)));
  __ Sd(a4, FieldMemOperand(a5, FixedArray::kHeaderSize + kPointerSize));
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // a0 : number of arguments
  // a1 : the function to call
  // a2 : feedback vector
  // a3 : slot in feedback vector (Smi, for RecordCallTarget)

  Label non_function;
  // Check that the function is not a smi.
  __ JumpIfSmi(a1, &non_function);
  // Check that the function is a JSFunction.
  __ GetObjectType(a1, a5, a5);
  __ Branch(&non_function, ne, a5, Operand(JS_FUNCTION_TYPE));

  GenerateRecordCallTarget(masm);

  __ dsrl(at, a3, 32 - kPointerSizeLog2);
  __ Daddu(a5, a2, at);
  Label feedback_register_initialized;
  // Put the AllocationSite from the feedback vector into a2, or undefined.
  __ Ld(a2, FieldMemOperand(a5, FixedArray::kHeaderSize));
  __ Ld(a5, FieldMemOperand(a2, AllocationSite::kMapOffset));
  __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
  __ Branch(&feedback_register_initialized, eq, a5, Operand(at));
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(a2, a5);

  // Pass function as new target.
  __ mov(a3, a1);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ Ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kConstructStubOffset));
  __ Daddu(at, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);

  __ bind(&non_function);
  __ mov(a3, a1);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}


// StringCharCodeAtGenerator.
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  DCHECK(!a4.is(index_));
  DCHECK(!a4.is(result_));
  DCHECK(!a4.is(object_));

  // If the receiver is a smi trigger the non-string case.
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ Ld(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
    __ Lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ And(a4, result_, Operand(kIsNotStringMask));
    __ Branch(receiver_not_string_, ne, a4, Operand(zero_reg));
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);

  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ Ld(a4, FieldMemOperand(object_, String::kLengthOffset));
  __ Branch(index_out_of_range_, ls, a4, Operand(index_));

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
  // Consumed by runtime conversion function:
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Push(LoadWithVectorDescriptor::VectorRegister(),
            LoadWithVectorDescriptor::SlotRegister(), object_, index_);
  } else {
    __ Push(object_, index_);
  }
  __ CallRuntime(Runtime::kNumberToSmi);

  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.

  __ Move(index_, v0);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ Pop(LoadWithVectorDescriptor::VectorRegister(),
           LoadWithVectorDescriptor::SlotRegister(), object_);
  } else {
    __ pop(object_);
  }
  // Reload the instance type.
  __ Ld(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ Lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ Branch(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ SmiTag(index_);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT);

  __ Move(result_, v0);

  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}

void StringHelper::GenerateFlatOneByteStringEquals(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ Ld(length, FieldMemOperand(left, String::kLengthOffset));
  __ Ld(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Branch(&check_zero_length, eq, length, Operand(scratch2));
  __ bind(&strings_not_equal);
  // Can not put li in delayslot, it has multi instructions.
  __ li(v0, Operand(Smi::FromInt(NOT_EQUAL)));
  __ Ret();

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&compare_chars, ne, length, Operand(zero_reg));
  DCHECK(is_int16((intptr_t)Smi::FromInt(EQUAL)));
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(Smi::FromInt(EQUAL)));

  // Compare characters.
  __ bind(&compare_chars);

  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2, scratch3,
                                  v0, &strings_not_equal);

  // Characters are equal.
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3, Register scratch4) {
  Label result_not_equal, compare_lengths;
  // Find minimum length and length difference.
  __ Ld(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ Ld(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Dsubu(scratch3, scratch1, Operand(scratch2));
  Register length_delta = scratch3;
  __ slt(scratch4, scratch2, scratch1);
  __ Movn(scratch1, scratch2, scratch4);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&compare_lengths, eq, min_length, Operand(zero_reg));

  // Compare loop.
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  scratch4, v0, &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ bind(&compare_lengths);
  DCHECK(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use length_delta as result if it's zero.
  __ mov(scratch2, length_delta);
  __ mov(scratch4, zero_reg);
  __ mov(v0, zero_reg);

  __ bind(&result_not_equal);
  // Conditionally update the result based either on length_delta or
  // the last comparion performed in the loop above.
  Label ret;
  __ Branch(&ret, eq, scratch2, Operand(scratch4));
  __ li(v0, Operand(Smi::FromInt(GREATER)));
  __ Branch(&ret, gt, scratch2, Operand(scratch4));
  __ li(v0, Operand(Smi::FromInt(LESS)));
  __ bind(&ret);
  __ Ret();
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch1, Register scratch2, Register scratch3,
    Label* chars_not_equal) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ Daddu(scratch1, length,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ Daddu(left, left, Operand(scratch1));
  __ Daddu(right, right, Operand(scratch1));
  __ Dsubu(length, zero_reg, length);
  Register index = length;  // index = -length;


  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ Daddu(scratch3, left, index);
  __ Lbu(scratch1, MemOperand(scratch3));
  __ Daddu(scratch3, right, index);
  __ Lbu(scratch2, MemOperand(scratch3));
  __ Branch(chars_not_equal, ne, scratch1, Operand(scratch2));
  __ Daddu(index, index, 1);
  __ Branch(&loop, ne, index, Operand(zero_reg));
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a1    : left
  //  -- a0    : right
  //  -- ra    : return address
  // -----------------------------------

  // Load a2 with the allocation site. We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ li(a2, isolate()->factory()->undefined_value());

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ And(at, a2, Operand(kSmiTagMask));
    __ Assert(ne, kExpectedAllocationSite, at, Operand(zero_reg));
    __ Ld(a4, FieldMemOperand(a2, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
    __ Assert(eq, kExpectedAllocationSite, a4, Operand(at));
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state());
  __ TailCallStub(&stub);
}


void CompareICStub::GenerateBooleans(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::BOOLEAN, state());
  Label miss;

  __ CheckMap(a1, a2, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  __ CheckMap(a0, a3, Heap::kBooleanMapRootIndex, &miss, DO_SMI_CHECK);
  if (!Token::IsEqualityOp(op())) {
    __ Ld(a1, FieldMemOperand(a1, Oddball::kToNumberOffset));
    __ AssertSmi(a1);
    __ Ld(a0, FieldMemOperand(a0, Oddball::kToNumberOffset));
    __ AssertSmi(a0);
  }
  __ Ret(USE_DELAY_SLOT);
  __ Dsubu(v0, a1, a0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ Or(a2, a1, a0);
  __ JumpIfNotSmi(a2, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    __ Ret(USE_DELAY_SLOT);
    __ Dsubu(v0, a0, a1);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(a1);
    __ SmiUntag(a0);
    __ Ret(USE_DELAY_SLOT);
    __ Dsubu(v0, a1, a0);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateNumbers(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::NUMBER);

  Label generic_stub;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;

  if (left() == CompareICState::SMI) {
    __ JumpIfNotSmi(a1, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(a0, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved.
  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(a0, &right_smi);
  __ CheckMap(a0, a2, Heap::kHeapNumberMapRootIndex, &maybe_undefined1,
              DONT_DO_SMI_CHECK);
  __ Dsubu(a2, a0, Operand(kHeapObjectTag));
  __ Ldc1(f2, MemOperand(a2, HeapNumber::kValueOffset));
  __ Branch(&left);
  __ bind(&right_smi);
  __ SmiUntag(a2, a0);  // Can't clobber a0 yet.
  FPURegister single_scratch = f6;
  __ mtc1(a2, single_scratch);
  __ cvt_d_w(f2, single_scratch);

  __ bind(&left);
  __ JumpIfSmi(a1, &left_smi);
  __ CheckMap(a1, a2, Heap::kHeapNumberMapRootIndex, &maybe_undefined2,
              DONT_DO_SMI_CHECK);
  __ Dsubu(a2, a1, Operand(kHeapObjectTag));
  __ Ldc1(f0, MemOperand(a2, HeapNumber::kValueOffset));
  __ Branch(&done);
  __ bind(&left_smi);
  __ SmiUntag(a2, a1);  // Can't clobber a1 yet.
  single_scratch = f8;
  __ mtc1(a2, single_scratch);
  __ cvt_d_w(f0, single_scratch);

  __ bind(&done);

  // Return a result of -1, 0, or 1, or use CompareStub for NaNs.
  Label fpu_eq, fpu_lt;
  // Test if equal, and also handle the unordered/NaN case.
  __ BranchF(&fpu_eq, &unordered, eq, f0, f2);

  // Test if less (unordered case is already handled).
  __ BranchF(&fpu_lt, NULL, lt, f0, f2);

  // Otherwise it's greater, so just fall thru, and return.
  DCHECK(is_int16(GREATER) && is_int16(EQUAL) && is_int16(LESS));
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(GREATER));

  __ bind(&fpu_eq);
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(EQUAL));

  __ bind(&fpu_lt);
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(LESS));

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(&miss, ne, a0, Operand(at));
    __ JumpIfSmi(a1, &unordered);
    __ GetObjectType(a1, a2, a2);
    __ Branch(&maybe_undefined2, ne, a2, Operand(HEAP_NUMBER_TYPE));
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(&unordered, eq, a1, Operand(at));
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = a1;
  Register right = a0;
  Register tmp1 = a2;
  Register tmp2 = a3;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are internalized strings.
  __ Ld(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ Ld(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ Lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ Lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ Or(tmp1, tmp1, Operand(tmp2));
  __ And(at, tmp1, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ Branch(&miss, ne, at, Operand(zero_reg));

  // Make sure a0 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(a0));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(v0, right);
  // Internalized strings are compared by identity.
  __ Ret(ne, left, Operand(right));
  DCHECK(is_int16(EQUAL));
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(Smi::FromInt(EQUAL)));

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  DCHECK(GetCondition() == eq);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = a1;
  Register right = a0;
  Register tmp1 = a2;
  Register tmp2 = a3;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ Ld(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ Ld(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ Lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ Lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss);

  // Use a0 as result
  __ mov(v0, a0);

  // Unique names are compared by identity.
  Label done;
  __ Branch(&done, ne, left, Operand(right));
  // Make sure a0 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(a0));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
  __ bind(&done);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  Label miss;

  bool equality = Token::IsEqualityOp(op());

  // Registers containing left and right operands respectively.
  Register left = a1;
  Register right = a0;
  Register tmp1 = a2;
  Register tmp2 = a3;
  Register tmp3 = a4;
  Register tmp4 = a5;
  Register tmp5 = a6;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ Ld(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ Ld(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ Lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ Lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ Or(tmp3, tmp1, tmp2);
  __ And(tmp5, tmp3, Operand(kIsNotStringMask));
  __ Branch(&miss, ne, tmp5, Operand(zero_reg));

  // Fast check for identical strings.
  Label left_ne_right;
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&left_ne_right, ne, left, Operand(right));
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, zero_reg);  // In the delay slot.
  __ bind(&left_ne_right);

  // Handle not identical strings.

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We know they are both
  // strings.
  if (equality) {
    DCHECK(GetCondition() == eq);
    STATIC_ASSERT(kInternalizedTag == 0);
    __ Or(tmp3, tmp1, Operand(tmp2));
    __ And(tmp5, tmp3, Operand(kIsNotInternalizedMask));
    Label is_symbol;
    __ Branch(&is_symbol, ne, tmp5, Operand(zero_reg));
    // Make sure a0 is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(a0));
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a0);  // In the delay slot.
    __ bind(&is_symbol);
  }

  // Check that both strings are sequential one_byte.
  Label runtime;
  __ JumpIfBothInstanceTypesAreNotSequentialOneByte(tmp1, tmp2, tmp3, tmp4,
                                                    &runtime);

  // Compare flat one_byte strings. Returns when done.
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
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(left, right);
      __ CallRuntime(Runtime::kStringEqual);
    }
    __ LoadRoot(a0, Heap::kTrueValueRootIndex);
    __ Ret(USE_DELAY_SLOT);
    __ Subu(v0, v0, a0);  // In delay slot.
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
  __ And(a2, a1, Operand(a0));
  __ JumpIfSmi(a2, &miss);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ GetObjectType(a0, a2, a2);
  __ Branch(&miss, lt, a2, Operand(FIRST_JS_RECEIVER_TYPE));
  __ GetObjectType(a1, a2, a2);
  __ Branch(&miss, lt, a2, Operand(FIRST_JS_RECEIVER_TYPE));

  DCHECK_EQ(eq, GetCondition());
  __ Ret(USE_DELAY_SLOT);
  __ dsubu(v0, a0, a1);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ And(a2, a1, a0);
  __ JumpIfSmi(a2, &miss);
  __ GetWeakValue(a4, cell);
  __ Ld(a2, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ Ld(a3, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&miss, ne, a2, Operand(a4));
  __ Branch(&miss, ne, a3, Operand(a4));

  if (Token::IsEqualityOp(op())) {
    __ Ret(USE_DELAY_SLOT);
    __ dsubu(v0, a0, a1);
  } else {
    if (op() == Token::LT || op() == Token::LTE) {
      __ li(a2, Operand(Smi::FromInt(GREATER)));
    } else {
      __ li(a2, Operand(Smi::FromInt(LESS)));
    }
    __ Push(a1, a0, a2);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a0);
    __ Push(ra, a1, a0);
    __ li(a4, Operand(Smi::FromInt(op())));
    __ daddiu(sp, sp, -kPointerSize);
    __ CallRuntime(Runtime::kCompareIC_Miss, 3, kDontSaveFPRegs,
                   USE_DELAY_SLOT);
    __ Sd(a4, MemOperand(sp));  // In the delay slot.
    // Compute the entry point of the rewritten stub.
    __ Daddu(a2, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
    // Restore registers.
    __ Pop(a1, a0, ra);
  }
  __ Jump(a2);
}


void DirectCEntryStub::Generate(MacroAssembler* masm) {
  // Make place for arguments to fit C calling convention. Most of the callers
  // of DirectCEntryStub::GenerateCall are using EnterExitFrame/LeaveExitFrame
  // so they handle stack restoring and we don't have to do that here.
  // Any caller of DirectCEntryStub::GenerateCall must take care of dropping
  // kCArgsSlotsSize stack space after the call.
  __ daddiu(sp, sp, -kCArgsSlotsSize);
  // Place the return address on the stack, making the call
  // GC safe. The RegExp backend also relies on this.
  __ Sd(ra, MemOperand(sp, kCArgsSlotsSize));
  __ Call(t9);  // Call the C++ function.
  __ Ld(t9, MemOperand(sp, kCArgsSlotsSize));

  if (FLAG_debug_code && FLAG_enable_slow_asserts) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC.
    // Dereference the address and check for this.
    __ Uld(a4, MemOperand(t9));
    __ Assert(ne, kReceivedInvalidReturnAddress, a4,
        Operand(reinterpret_cast<uint64_t>(kZapValue)));
  }
  __ Jump(t9);
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    Register target) {
  intptr_t loc =
      reinterpret_cast<intptr_t>(GetCode().location());
  __ Move(t9, target);
  __ li(at, Operand(loc, RelocInfo::CODE_TARGET), CONSTANT_SIZE);
  __ Call(at);
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
    __ SmiLoadUntag(index, FieldMemOperand(properties, kCapacityOffset));
    __ Dsubu(index, index, Operand(1));
    __ And(index, index,
           Operand(name->Hash() + NameDictionary::GetProbeOffset(i)));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ Dlsa(index, index, index, 1);  // index *= 3.

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    Register tmp = properties;

    __ Dlsa(tmp, properties, index, kPointerSizeLog2);
    __ Ld(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    DCHECK(!tmp.is(entity_name));
    __ LoadRoot(tmp, Heap::kUndefinedValueRootIndex);
    __ Branch(done, eq, entity_name, Operand(tmp));

    // Load the hole ready for use below:
    __ LoadRoot(tmp, Heap::kTheHoleValueRootIndex);

    // Stop if found the property.
    __ Branch(miss, eq, entity_name, Operand(Handle<Name>(name)));

    Label good;
    __ Branch(&good, eq, entity_name, Operand(tmp));

    // Check if the entry name is not a unique name.
    __ Ld(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ Lbu(entity_name, FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ Ld(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask =
      (ra.bit() | a6.bit() | a5.bit() | a4.bit() | a3.bit() |
       a2.bit() | a1.bit() | a0.bit() | v0.bit());

  __ MultiPush(spill_mask);
  __ Ld(a0, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ li(a1, Operand(Handle<Name>(name)));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  __ mov(at, v0);
  __ MultiPop(spill_mask);

  __ Branch(done, eq, at, Operand(zero_reg));
  __ Branch(miss, ne, at, Operand(zero_reg));
}

void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Registers:
  //  result: NameDictionary to probe
  //  a1: key
  //  dictionary: NameDictionary to probe.
  //  index: will hold an index of entry if lookup is successful.
  //         might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Register result = v0;
  Register dictionary = a0;
  Register key = a1;
  Register index = a2;
  Register mask = a3;
  Register hash = a4;
  Register undefined = a5;
  Register entry_key = a6;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ Ld(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ SmiUntag(mask);
  __ Dsubu(mask, mask, Operand(1));

  __ Lwu(hash, FieldMemOperand(key, Name::kHashFieldOffset));

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
      __ Daddu(index, hash, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    } else {
      __ mov(index, hash);
    }
    __ dsrl(index, index, Name::kHashShift);
    __ And(index, mask, index);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    // index *= 3.
    __ Dlsa(index, index, index, 1);

    STATIC_ASSERT(kSmiTagSize == 1);
    __ Dlsa(index, dictionary, index, kPointerSizeLog2);
    __ Ld(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ Branch(&not_in_dictionary, eq, entry_key, Operand(undefined));

    // Stop if found the property.
    __ Branch(&in_dictionary, eq, entry_key, Operand(key));

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ Ld(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ Lbu(entry_key, FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(entry_key, &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ Ret(USE_DELAY_SLOT);
    __ mov(result, zero_reg);
  }

  __ bind(&in_dictionary);
  __ Ret(USE_DELAY_SLOT);
  __ li(result, 1);

  __ bind(&not_in_dictionary);
  __ Ret(USE_DELAY_SLOT);
  __ mov(result, zero_reg);
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

  // The first two branch+nop instructions are generated with labels so as to
  // get the offset fixed up correctly by the bind(Label*) call.  We patch it
  // back and forth between a "bne zero_reg, zero_reg, ..." (a nop in this
  // position) and the "beq zero_reg, zero_reg, ..." when we start and stop
  // incremental heap marking.
  // See RecordWriteStub::Patch for details.
  __ beq(zero_reg, zero_reg, &skip_to_incremental_noncompacting);
  __ nop();
  __ beq(zero_reg, zero_reg, &skip_to_incremental_compacting);
  __ nop();

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object(),
                           address(),
                           value(),
                           save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  }
  __ Ret();

  __ bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);

  // Initial mode of the stub is expected to be STORE_BUFFER_ONLY.
  // Will be checked in IncrementalMarking::ActivateGeneratedStub.

  PatchBranchIntoNop(masm, 0);
  PatchBranchIntoNop(masm, 2 * Assembler::kInstrSize);
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ Ld(regs_.scratch0(), MemOperand(regs_.address(), 0));
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
    __ RememberedSetHelper(object(),
                           address(),
                           value(),
                           save_fp_regs_mode(),
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
      a0.is(regs_.address()) ? regs_.scratch0() : regs_.address();
  DCHECK(!address.is(regs_.object()));
  DCHECK(!address.is(a0));
  __ Move(address, regs_.address());
  __ Move(a0, regs_.object());
  __ Move(a1, address);
  __ li(a2, Operand(ExternalReference::isolate_address(isolate())));

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
    __ RememberedSetHelper(object(),
                           address(),
                           value(),
                           save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ bind(&on_black);

  // Get the value from the slot.
  __ Ld(regs_.scratch0(), MemOperand(regs_.address(), 0));

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
    __ RememberedSetHelper(object(),
                           address(),
                           value(),
                           save_fp_regs_mode(),
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
  __ Ld(a1, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ Daddu(a1, a1, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ dsll(a1, a1, kPointerSizeLog2);
  __ Ret(USE_DELAY_SLOT);
  __ Daddu(sp, sp, a1);
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    ProfileEntryHookStub stub(masm->isolate());
    __ push(ra);
    __ CallStub(&stub);
    __ pop(ra);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // The entry hook is a "push ra" instruction, followed by a call.
  // Note: on MIPS "push" is 2 instruction
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + (2 * Assembler::kInstrSize);

  // This should contain all kJSCallerSaved registers.
  const RegList kSavedRegs =
     kJSCallerSaved |  // Caller saved registers.
     s5.bit();         // Saved stack pointer.

  // We also save ra, so the count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = kNumJSCallerSaved + 2;

  // Save all caller-save registers as this may be called from anywhere.
  __ MultiPush(kSavedRegs | ra.bit());

  // Compute the function's address for the first argument.
  __ Dsubu(a0, ra, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ Daddu(a1, sp, Operand(kNumSavedRegs * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ mov(s5, sp);
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    __ And(sp, sp, Operand(-frame_alignment));
  }

  __ Dsubu(sp, sp, kCArgsSlotsSize);
#if defined(V8_HOST_ARCH_MIPS) || defined(V8_HOST_ARCH_MIPS64)
  int64_t entry_hook =
      reinterpret_cast<int64_t>(isolate()->function_entry_hook());
  __ li(t9, Operand(entry_hook));
#else
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  // It additionally takes an isolate as a third parameter.
  __ li(a2, Operand(ExternalReference::isolate_address(isolate())));

  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ li(t9, Operand(ExternalReference(&dispatcher,
                                      ExternalReference::BUILTIN_CALL,
                                      isolate())));
#endif
  // Call C function through t9 to conform ABI for PIC.
  __ Call(t9);

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ mov(sp, s5);
  } else {
    __ Daddu(sp, sp, kCArgsSlotsSize);
  }

  // Also pop ra to get Ret(0).
  __ MultiPop(kSavedRegs | ra.bit());
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
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      T stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq, a3, Operand(kind));
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // a2 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // a3 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // a0 - number of arguments
  // a1 - constructor?
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
    __ And(at, a3, Operand(1));
    __ Branch(&normal_sequence, ne, at, Operand(zero_reg));
  }
  // look at the first argument
  __ Ld(a5, MemOperand(sp, 0));
  __ Branch(&normal_sequence, eq, a5, Operand(zero_reg));

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
    __ Daddu(a3, a3, Operand(1));

    if (FLAG_debug_code) {
      __ Ld(a5, FieldMemOperand(a2, 0));
      __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite, a5, Operand(at));
    }

    // Save the resulting elements kind in type info. We can't just store a3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ Ld(a4, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));
    __ Daddu(a4, a4, Operand(Smi::FromInt(kFastElementsKindPackedToHoley)));
    __ Sd(a4, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));

    __ bind(&normal_sequence);
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq, a3, Operand(kind));
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
    // For internal arrays we only need a few things.
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
  __ And(at, a0, a0);
  __ Branch(&not_zero_case, ne, at, Operand(zero_reg));
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ bind(&not_zero_case);
  __ Branch(&not_one_case, gt, a0, Operand(1));
  CreateArrayDispatchOneArgument(masm, mode);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argc (only if argument_count() == ANY)
  //  -- a1 : constructor
  //  -- a2 : AllocationSite or undefined
  //  -- a3 : new target
  //  -- sp[0] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ Ld(a4, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ SmiTst(a4, at);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction,
        at, Operand(zero_reg));
    __ GetObjectType(a4, a4, a5);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction,
        a5, Operand(MAP_TYPE));

    // We should either have undefined in a2 or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(a2, a4);
  }

  // Enter the context of the Array function.
  __ Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  Label subclassing;
  __ Branch(&subclassing, ne, a1, Operand(a3));

  Label no_info;
  // Get the elements kind and case on that.
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&no_info, eq, a2, Operand(at));

  __ Ld(a3, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(a3);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ And(a3, a3, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing.
  __ bind(&subclassing);
  __ Dlsa(at, sp, a0, kPointerSizeLog2);
  __ Sd(a1, MemOperand(at));
  __ li(at, Operand(3));
  __ Daddu(a0, a0, at);
  __ Push(a3, a2);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {

  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0, lo, a0, Operand(1));

  ArrayNArgumentsConstructorStub stubN(isolate());
  __ TailCallStub(&stubN, hi, a0, Operand(1));

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument.
    __ Ld(at, MemOperand(sp, 0));

    InternalArraySingleArgumentConstructorStub
        stub1_holey(isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey, ne, at, Operand(zero_reg));
  }

  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argc
  //  -- a1 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ Ld(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ SmiTst(a3, at);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction,
        at, Operand(zero_reg));
    __ GetObjectType(a3, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction,
        a4, Operand(MAP_TYPE));
  }

  // Figure out the right elements kind.
  __ Ld(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into a3. We only need the first byte,
  // but the following bit field extraction takes care of that anyway.
  __ Lbu(a3, FieldMemOperand(a3, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(a3);

  if (FLAG_debug_code) {
    Label done;
    __ Branch(&done, eq, a3, Operand(FAST_ELEMENTS));
    __ Assert(
        eq, kInvalidElementsKindForInternalArrayOrInternalPackedArray,
        a3, Operand(FAST_HOLEY_ELEMENTS));
    __ bind(&done);
  }

  Label fast_elements_case;
  __ Branch(&fast_elements_case, eq, a3, Operand(FAST_ELEMENTS));
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}

static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}


// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  stack_space
// - space to be unwound on exit (includes the call JS arguments space and
// the additional space allocated for the fast call).
static void CallApiFunctionAndReturn(
    MacroAssembler* masm, Register function_address,
    ExternalReference thunk_ref, int stack_space, int32_t stack_space_offset,
    MemOperand return_value_operand, MemOperand* context_restore_operand) {
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  DCHECK(function_address.is(a1) || function_address.is(a2));

  Label profiler_disabled;
  Label end_profiler_check;
  __ li(t9, Operand(ExternalReference::is_profiling_address(isolate)));
  __ Lb(t9, MemOperand(t9, 0));
  __ Branch(&profiler_disabled, eq, t9, Operand(zero_reg));

  // Additional parameter is the address of the actual callback.
  __ li(t9, Operand(thunk_ref));
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  __ mov(t9, function_address);
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  __ li(s3, Operand(next_address));
  __ Ld(s0, MemOperand(s3, kNextOffset));
  __ Ld(s1, MemOperand(s3, kLimitOffset));
  __ Lw(s2, MemOperand(s3, kLevelOffset));
  __ Addu(s2, s2, Operand(1));
  __ Sw(s2, MemOperand(s3, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, a0);
    __ li(a0, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub(isolate);
  stub.GenerateCall(masm, t9);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, a0);
    __ li(a0, Operand(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // Load value from ReturnValue.
  __ Ld(v0, return_value_operand);
  __ bind(&return_value_loaded);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ Sd(s0, MemOperand(s3, kNextOffset));
  if (__ emit_debug_code()) {
    __ Lw(a1, MemOperand(s3, kLevelOffset));
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall, a1, Operand(s2));
  }
  __ Subu(s2, s2, Operand(1));
  __ Sw(s2, MemOperand(s3, kLevelOffset));
  __ Ld(at, MemOperand(s3, kLimitOffset));
  __ Branch(&delete_allocated_handles, ne, s1, Operand(at));

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ Ld(cp, *context_restore_operand);
  }
  if (stack_space_offset != kInvalidStackOffset) {
    DCHECK(kCArgsSlotsSize == 0);
    __ Ld(s0, MemOperand(sp, stack_space_offset));
  } else {
    __ li(s0, Operand(stack_space));
  }
  __ LeaveExitFrame(false, s0, !restore_context, NO_EMIT_RETURN,
                    stack_space_offset != kInvalidStackOffset);

  // Check if the function scheduled an exception.
  __ LoadRoot(a4, Heap::kTheHoleValueRootIndex);
  __ li(at, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ Ld(a5, MemOperand(at));
  __ Branch(&promote_scheduled_exception, ne, a4, Operand(a5));

  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ Sd(s1, MemOperand(s3, kLimitOffset));
  __ mov(s0, v0);
  __ mov(a0, v0);
  __ PrepareCallCFunction(1, s1);
  __ li(a0, Operand(ExternalReference::isolate_address(isolate)));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ mov(v0, s0);
  __ jmp(&leave_exit_frame);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                  : callee
  //  -- a4                  : call_data
  //  -- a2                  : holder
  //  -- a1                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 8]   : first argument
  //  -- sp[argc * 8]        : receiver
  // -----------------------------------

  Register callee = a0;
  Register call_data = a4;
  Register holder = a2;
  Register api_function_address = a1;
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

  // Save context, callee and call data.
  __ Push(context, callee, call_data);
  if (!is_lazy()) {
    // Load context from callee.
    __ Ld(context, FieldMemOperand(callee, JSFunction::kContextOffset));
  }

  Register scratch = call_data;
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  // Push return value and default return value.
  __ Push(scratch, scratch);
  __ li(scratch, Operand(ExternalReference::isolate_address(masm->isolate())));
  // Push isolate and holder.
  __ Push(scratch, holder);

  // Prepare arguments.
  __ mov(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 3;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(!api_function_address.is(a0) && !scratch.is(a0));
  // a0 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ Daddu(a0, sp, Operand(1 * kPointerSize));
  // FunctionCallbackInfo::implicit_args_
  __ Sd(scratch, MemOperand(a0, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ Daddu(at, scratch,
           Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ Sd(at, MemOperand(a0, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  // Stored as int field, 32-bit integers within struct on stack always left
  // justified by n64 ABI.
  __ li(at, Operand(argc()));
  __ Sw(at, MemOperand(a0, 2 * kPointerSize));

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  MemOperand context_restore_operand(
      fp, (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument.
  int return_value_offset = 0;
  if (is_store()) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);
  int stack_space = 0;
  int32_t stack_space_offset = 3 * kPointerSize;
  stack_space = argc() + FCA::kArgsLength + 1;
  // TODO(adamk): Why are we clobbering this immediately?
  stack_space_offset = kInvalidStackOffset;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_offset, return_value_operand,
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
  Register scratch = a4;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = a2;

  // Here and below +1 is for name() pushed after the args_ array.
  typedef PropertyCallbackArguments PCA;
  __ Dsubu(sp, sp, (PCA::kArgsLength + 1) * kPointerSize);
  __ Sd(receiver, MemOperand(sp, (PCA::kThisIndex + 1) * kPointerSize));
  __ Ld(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ Sd(scratch, MemOperand(sp, (PCA::kDataIndex + 1) * kPointerSize));
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ Sd(scratch, MemOperand(sp, (PCA::kReturnValueOffset + 1) * kPointerSize));
  __ Sd(scratch, MemOperand(sp, (PCA::kReturnValueDefaultValueIndex + 1) *
                                    kPointerSize));
  __ li(scratch, Operand(ExternalReference::isolate_address(isolate())));
  __ Sd(scratch, MemOperand(sp, (PCA::kIsolateIndex + 1) * kPointerSize));
  __ Sd(holder, MemOperand(sp, (PCA::kHolderIndex + 1) * kPointerSize));
  // should_throw_on_error -> false
  DCHECK(Smi::kZero == nullptr);
  __ Sd(zero_reg,
        MemOperand(sp, (PCA::kShouldThrowOnErrorIndex + 1) * kPointerSize));
  __ Ld(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ Sd(scratch, MemOperand(sp, 0 * kPointerSize));

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mov(a0, sp);                               // a0 = Handle<Name>
  __ Daddu(a1, a0, Operand(1 * kPointerSize));  // a1 = v8::PCI::args_

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ Sd(a1, MemOperand(sp, 1 * kPointerSize));
  __ Daddu(a1, sp, Operand(1 * kPointerSize));
  // a1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  __ Ld(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ Ld(api_function_address,
        FieldMemOperand(scratch, Foreign::kForeignAddressOffset));

  // +3 is to skip prolog, return address and name handle.
  MemOperand return_value_operand(
      fp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, kInvalidStackOffset,
                           return_value_operand, NULL);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
