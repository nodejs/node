// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

#include "src/code-stubs.h"
#include "src/api-arguments.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/mips/code-stubs-mips.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ sll(t9, a0, kPointerSizeLog2);
  __ Addu(t9, sp, t9);
  __ sw(a1, MemOperand(t9, 0));
  __ Push(a1);
  __ Push(a2);
  __ Addu(a0, a0, Operand(3));
  __ TailCallRuntime(Runtime::kNewArray);
}

void FastArrayPushStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  Address deopt_handler = Runtime::FunctionForId(Runtime::kArrayPush)->entry;
  descriptor->Initialize(a0, deopt_handler, -1, JS_FUNCTION_STUB_MODE);
}

void FastFunctionBindStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  Address deopt_handler = Runtime::FunctionForId(Runtime::kFunctionBind)->entry;
  descriptor->Initialize(a0, deopt_handler, -1, JS_FUNCTION_STUB_MODE);
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
    DCHECK(param_count == 0 ||
           a0.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments, adjust sp.
    __ Subu(sp, sp, Operand(param_count * kPointerSize));
    for (int i = 0; i < param_count; ++i) {
      // Store argument to stack.
      __ sw(descriptor.GetRegisterParameter(i),
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
    __ ldc1(double_scratch, MemOperand(input_reg, double_offset));

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

  __ lw(input_low,
      MemOperand(input_reg, double_offset + Register::kMantissaOffset));
  __ lw(input_high,
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
  Register exp_mask_reg = t5;

  __ Branch(&not_identical, ne, a0, Operand(a1));

  __ li(exp_mask_reg, Operand(HeapNumber::kExponentMask));

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis. If it's not a heap number, then return equal.
  __ GetObjectType(a0, t4, t4);
  if (cc == less || cc == greater) {
    // Call runtime on identical JSObjects.
    __ Branch(slow, greater, t4, Operand(FIRST_JS_RECEIVER_TYPE));
    // Call runtime on identical symbols since we need to throw a TypeError.
    __ Branch(slow, eq, t4, Operand(SYMBOL_TYPE));
    // Call runtime on identical SIMD values since we must throw a TypeError.
    __ Branch(slow, eq, t4, Operand(SIMD128_VALUE_TYPE));
  } else {
    __ Branch(&heap_number, eq, t4, Operand(HEAP_NUMBER_TYPE));
    // Comparing JS objects with <=, >= is complicated.
    if (cc != eq) {
      __ Branch(slow, greater, t4, Operand(FIRST_JS_RECEIVER_TYPE));
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ Branch(slow, eq, t4, Operand(SYMBOL_TYPE));
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ Branch(slow, eq, t4, Operand(SIMD128_VALUE_TYPE));
      // Normally here we fall through to return_equal, but undefined is
      // special: (undefined == undefined) == true, but
      // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
      if (cc == less_equal || cc == greater_equal) {
        __ Branch(&return_equal, ne, t4, Operand(ODDBALL_TYPE));
        __ LoadRoot(t2, Heap::kUndefinedValueRootIndex);
        __ Branch(&return_equal, ne, a0, Operand(t2));
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
    __ lw(t2, FieldMemOperand(a0, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    __ And(t3, t2, Operand(exp_mask_reg));
    // If all bits not set (ne cond), then not a NaN, objects are equal.
    __ Branch(&return_equal, ne, t3, Operand(exp_mask_reg));

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ sll(t2, t2, HeapNumber::kNonMantissaBitsInTopWord);
    // Or with all low-bits of mantissa.
    __ lw(t3, FieldMemOperand(a0, HeapNumber::kMantissaOffset));
    __ Or(v0, t3, Operand(t2));
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
  __ GetObjectType(lhs, t4, t4);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed. Return non-equal (lhs is already not zero).
    __ Ret(USE_DELAY_SLOT, ne, t4, Operand(HEAP_NUMBER_TYPE));
    __ mov(v0, lhs);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number. Call
    // the runtime.
    __ Branch(slow, ne, t4, Operand(HEAP_NUMBER_TYPE));
  }

  // Rhs is a smi, lhs is a number.
  // Convert smi rhs to double.
  __ sra(at, rhs, kSmiTagSize);
  __ mtc1(at, f14);
  __ cvt_d_w(f14, f14);
  __ ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));

  // We now have both loaded as doubles.
  __ jmp(both_loaded_as_doubles);

  __ bind(&lhs_is_smi);
  // Lhs is a Smi.  Check whether the non-smi is a heap number.
  __ GetObjectType(rhs, t4, t4);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed. Return non-equal.
    __ Ret(USE_DELAY_SLOT, ne, t4, Operand(HEAP_NUMBER_TYPE));
    __ li(v0, Operand(1));
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number. Call
    // the runtime.
    __ Branch(slow, ne, t4, Operand(HEAP_NUMBER_TYPE));
  }

  // Lhs is a smi, rhs is a number.
  // Convert smi lhs to double.
  __ sra(at, lhs, kSmiTagSize);
  __ mtc1(at, f12);
  __ cvt_d_w(f12, f12);
  __ ldc1(f14, FieldMemOperand(rhs, HeapNumber::kValueOffset));
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
  __ lw(a2, FieldMemOperand(rhs, HeapObject::kMapOffset));
  // If first was a heap number & second wasn't, go to slow case.
  __ Branch(slow, ne, a3, Operand(a2));

  // Both are heap numbers. Load them up then jump to the code we have
  // for that.
  __ ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  __ ldc1(f14, FieldMemOperand(rhs, HeapNumber::kValueOffset));

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
  __ lw(a2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ lw(a3, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(a2, Map::kBitFieldOffset));
  __ lbu(t1, FieldMemOperand(a3, Map::kBitFieldOffset));
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
  __ sra(a1, a1, 1);
  __ sra(a0, a0, 1);
  __ Ret(USE_DELAY_SLOT);
  __ subu(v0, a1, a0);
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
  __ And(t2, lhs, Operand(rhs));
  __ JumpIfNotSmi(t2, &not_smis, t0);
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
  __ li(t0, Operand(LESS));
  __ li(t1, Operand(GREATER));
  __ li(t2, Operand(EQUAL));

  // Check if either rhs or lhs is NaN.
  __ BranchF(NULL, &nan, eq, f12, f14);

  // Check if LESS condition is satisfied. If true, move conditionally
  // result to v0.
  if (!IsMipsArchVariant(kMips32r6)) {
    __ c(OLT, D, f12, f14);
    __ Movt(v0, t0);
    // Use previous check to store conditionally to v0 oposite condition
    // (GREATER). If rhs is equal to lhs, this will be corrected in next
    // check.
    __ Movf(v0, t1);
    // Check if EQUAL condition is satisfied. If true, move conditionally
    // result to v0.
    __ c(EQ, D, f12, f14);
    __ Movt(v0, t2);
  } else {
    Label skip;
    __ BranchF(USE_DELAY_SLOT, &skip, NULL, lt, f12, f14);
    __ mov(v0, t0);  // Return LESS as result.

    __ BranchF(USE_DELAY_SLOT, &skip, NULL, eq, f12, f14);
    __ mov(v0, t2);  // Return EQUAL as result.

    __ mov(v0, t1);  // Return GREATER as result.
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
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, a2, a3, t0);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, a2, a3, t0,
                                                    t1);
  }
  // Never falls through to here.

  __ bind(&slow);
  if (cc == eq) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(lhs, rhs);
      __ CallRuntime(strict() ? Runtime::kStrictEqual : Runtime::kEqual);
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
  const Register scratch = t5;
  const Register scratch2 = t3;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == TAGGED) {
    // Base is already in double_base.
    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ ldc1(double_exponent,
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
  __ Subu(scratch, zero_reg, scratch);
  // Check when Subu overflows and we get negative result
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

  __ sra(scratch, scratch, 1);

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
  isolate->set_fp_stubs_generated(true);
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
    __ Lsa(s1, sp, a0, kPointerSizeLog2);
    __ Subu(s1, s1, kPointerSize);
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
    __ Subu(sp, sp, Operand(result_stack_size));

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
    if (kArchVariant >= kMips32r6) {
      __ addiupc(ra, kNumInstructionsToJump + 1);
    } else {
      // This branch-and-link sequence is needed to find the current PC on mips
      // before r6, saved to the ra register.
      __ bal(&find_ra);  // bal exposes branch delay slot.
      __ Addu(ra, ra, kNumInstructionsToJump * Instruction::kInstrSize);
    }
    __ bind(&find_ra);

    // This spot was reserved in EnterExitFrame.
    __ sw(ra, MemOperand(sp, result_stack_size));
    // Stack space reservation moved to the branch delay slot below.
    // Stack is still aligned.

    // Call the C routine.
    __ mov(t9, s2);  // Function pointer to t9 to conform to ABI for PIC.
    __ jalr(t9);
    // Set up sp in the delay slot.
    __ addiu(sp, sp, -kCArgsSlotsSize);
    // Make sure the stored 'ra' points to this position.
    DCHECK_EQ(kNumInstructionsToJump,
              masm->InstructionsGeneratedSince(&find_ra));
  }
  if (result_size() > 2) {
    DCHECK_EQ(3, result_size());
    // Read result values stored on stack.
    __ lw(a0, MemOperand(v0, 2 * kPointerSize));
    __ lw(v1, MemOperand(v0, 1 * kPointerSize));
    __ lw(v0, MemOperand(v0, 0 * kPointerSize));
  }
  // Result returned in v0, v1:v0 or a0:v1:v0 - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ LoadRoot(t0, Heap::kExceptionRootIndex);
  __ Branch(&exception_returned, eq, t0, Operand(v0));

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    ExternalReference pending_exception_address(
        Isolate::kPendingExceptionAddress, isolate());
    __ li(a2, Operand(pending_exception_address));
    __ lw(a2, MemOperand(a2));
    __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ Branch(&okay, eq, t0, Operand(a2));
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
  __ lw(cp, MemOperand(cp));
  __ li(sp, Operand(pending_handler_sp_address));
  __ lw(sp, MemOperand(sp));
  __ li(fp, Operand(pending_handler_fp_address));
  __ lw(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label zero;
  __ Branch(&zero, eq, cp, Operand(zero_reg));
  __ sw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&zero);

  // Compute the handler entry address and jump to it.
  __ li(a1, Operand(pending_handler_code_address));
  __ lw(a1, MemOperand(a1));
  __ li(a2, Operand(pending_handler_offset_address));
  __ lw(a2, MemOperand(a2));
  __ Addu(a1, a1, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Addu(t9, a1, a2);
  __ Jump(t9);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Isolate* isolate = masm->isolate();

  // Registers:
  // a0: entry address
  // a1: function
  // a2: receiver
  // a3: argc
  //
  // Stack:
  // 4 args slots
  // args

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Save callee saved registers on the stack.
  __ MultiPush(kCalleeSaved | ra.bit());

  // Save callee-saved FPU registers.
  __ MultiPushFPU(kCalleeSavedFPU);
  // Set up the reserved register for 0.0.
  __ Move(kDoubleRegZero, 0.0);


  // Load argv in s0 register.
  int offset_to_argv = (kNumCalleeSaved + 1) * kPointerSize;
  offset_to_argv += kNumCalleeSavedFPU * kDoubleSize;

  __ InitializeRootRegister();
  __ lw(s0, MemOperand(sp, offset_to_argv + kCArgsSlotsSize));

  // We build an EntryFrame.
  __ li(t3, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  int marker = type();
  __ li(t2, Operand(Smi::FromInt(marker)));
  __ li(t1, Operand(Smi::FromInt(marker)));
  __ li(t0, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                      isolate)));
  __ lw(t0, MemOperand(t0));
  __ Push(t3, t2, t1, t0);
  // Set up frame pointer for the frame to be pushed.
  __ addiu(fp, sp, -EntryFrameConstants::kCallerFPOffset);

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
  // 4 args slots
  // args

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate);
  __ li(t1, Operand(ExternalReference(js_entry_sp)));
  __ lw(t2, MemOperand(t1));
  __ Branch(&non_outermost_js, ne, t2, Operand(zero_reg));
  __ sw(fp, MemOperand(t1));
  __ li(t0, Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  Label cont;
  __ b(&cont);
  __ nop();   // Branch delay slot nop.
  __ bind(&non_outermost_js);
  __ li(t0, Operand(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));
  __ bind(&cont);
  __ push(t0);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ li(t0, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      isolate)));
  __ sw(v0, MemOperand(t0));  // We come back from 'invoke'. result is in v0.
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
  // 4 args slots
  // args

  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate);
    __ li(t0, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, masm->isolate());
    __ li(t0, Operand(entry));
  }
  __ lw(t9, MemOperand(t0));  // Deref address.

  // Call JSEntryTrampoline.
  __ addiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  __ Call(t9);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // v0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(t1);
  __ Branch(&non_outermost_js_2,
            ne,
            t1,
            Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ li(t1, Operand(ExternalReference(js_entry_sp)));
  __ sw(zero_reg, MemOperand(t1));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(t1);
  __ li(t0, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                      isolate)));
  __ sw(t1, MemOperand(t0));

  // Reset the stack to the callee saved registers.
  __ addiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Restore callee-saved fpu registers.
  __ MultiPopFPU(kCalleeSavedFPU);

  // Restore callee saved registers from the stack.
  __ MultiPop(kCalleeSaved | ra.bit());
  // Return.
  __ Jump(ra);
}


void LoadIndexedStringStub::Generate(MacroAssembler* masm) {
  // Return address is in ra.
  Label miss;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register index = LoadDescriptor::NameRegister();
  Register scratch = t1;
  Register result = v0;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  DCHECK(!scratch.is(LoadWithVectorDescriptor::VectorRegister()));

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


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // Ensure that the vector and slot registers won't be clobbered before
  // calling the miss handler.
  DCHECK(!AreAliased(t0, t1, LoadWithVectorDescriptor::VectorRegister(),
                     LoadWithVectorDescriptor::SlotRegister()));

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, t0,
                                                          t1, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
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
  // MIPS - using s0..s2, since we are not using CEntry Stub.
  Register subject = s0;
  Register regexp_data = s1;
  Register last_match_info_elements = s2;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ li(a0, Operand(address_of_regexp_stack_memory_size));
  __ lw(a0, MemOperand(a0, 0));
  __ Branch(&runtime, eq, a0, Operand(zero_reg));

  // Check that the first argument is a JSRegExp object.
  __ lw(a0, MemOperand(sp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(a0, &runtime);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&runtime, ne, a1, Operand(JS_REGEXP_TYPE));

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ lw(regexp_data, FieldMemOperand(a0, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ SmiTst(regexp_data, t0);
    __ Check(nz,
             kUnexpectedTypeForRegExpDataFixedArrayExpected,
             t0,
             Operand(zero_reg));
    __ GetObjectType(regexp_data, a0, a0);
    __ Check(eq,
             kUnexpectedTypeForRegExpDataFixedArrayExpected,
             a0,
             Operand(FIXED_ARRAY_TYPE));
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ lw(a0, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ Branch(&runtime, ne, a0, Operand(Smi::FromInt(JSRegExp::IRREGEXP)));

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ lw(a2,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // Multiplying by 2 comes for free since a2 is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ Branch(
      &runtime, hi, a2, Operand(Isolate::kJSRegexpStaticOffsetsVectorSize - 2));

  // Reset offset for possibly sliced string.
  __ mov(t0, zero_reg);
  __ lw(subject, MemOperand(sp, kSubjectOffset));
  __ JumpIfSmi(subject, &runtime);
  __ mov(a3, subject);  // Make a copy of the original subject string.
  // subject: subject string
  // a3: subject string
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
  __ lw(a0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbu(a0, FieldMemOperand(a0, Map::kInstanceTypeOffset));

  // (1) Sequential string?  If yes, go to (4).
  __ And(a1,
         a0,
         Operand(kIsNotStringMask |
                 kStringRepresentationMask |
                 kShortExternalStringMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ Branch(&seq_string, eq, a1, Operand(zero_reg));  // Go to (5).

  // (2) Sequential or cons?  If not, go to (5).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  // Go to (5).
  __ Branch(&not_seq_nor_cons, ge, a1, Operand(kExternalStringTag));

  // (3) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ lw(a0, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ LoadRoot(a1, Heap::kempty_stringRootIndex);
  __ Branch(&runtime, ne, a0, Operand(a1));
  __ lw(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  __ jmp(&check_underlying);

  // (4) Sequential string.  Load regexp code according to encoding.
  __ bind(&seq_string);
  // subject: sequential subject string (or look-alike, external string)
  // a3: original subject string
  // Load previous index and check range before a3 is overwritten.  We have to
  // use a3 instead of subject here because subject might have been only made
  // to look like a sequential string when it actually is an external string.
  __ lw(a1, MemOperand(sp, kPreviousIndexOffset));
  __ JumpIfNotSmi(a1, &runtime);
  __ lw(a3, FieldMemOperand(a3, String::kLengthOffset));
  __ Branch(&runtime, ls, a3, Operand(a1));
  __ sra(a1, a1, kSmiTagSize);  // Untag the Smi.

  STATIC_ASSERT(kStringEncodingMask == 4);
  STATIC_ASSERT(kOneByteStringTag == 4);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ And(a0, a0, Operand(kStringEncodingMask));  // Non-zero for one-byte.
  __ lw(t9, FieldMemOperand(regexp_data, JSRegExp::kDataOneByteCodeOffset));
  __ sra(a3, a0, 2);  // a3 is 1 for ASCII, 0 for UC16 (used below).
  __ lw(t1, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset));
  __ Movz(t9, t1, a0);  // If UC16 (a0 is 0), replace t9 w/kDataUC16CodeOffset.

  // (E) Carry on.  String handling is done.
  // t9: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(t9, &runtime);

  // a1: previous index
  // a3: encoding of subject string (1 if one_byte, 0 if two_byte);
  // t9: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(isolate()->counters()->regexp_entry_native(),
                      1, a0, a2);

  // Isolates: note we add an additional parameter here (isolate pointer).
  const int kRegExpExecuteArguments = 9;
  const int kParameterRegisters = 4;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers, meaning we
  // treat the return address as argument 5. Thus every argument after that
  // needs to be shifted back by 1. Since DirectCEntryStub will handle
  // allocating space for the c argument slots, we don't need to calculate
  // that into the argument positions on the stack. This is how the stack will
  // look (sp meaning the value of sp at this moment):
  // [sp + 5] - Argument 9
  // [sp + 4] - Argument 8
  // [sp + 3] - Argument 7
  // [sp + 2] - Argument 6
  // [sp + 1] - Argument 5
  // [sp + 0] - saved ra

  // Argument 9: Pass current isolate address.
  // CFunctionArgumentOperand handles MIPS stack argument slots.
  __ li(a0, Operand(ExternalReference::isolate_address(isolate())));
  __ sw(a0, MemOperand(sp, 5 * kPointerSize));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ li(a0, Operand(1));
  __ sw(a0, MemOperand(sp, 4 * kPointerSize));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ li(a0, Operand(address_of_regexp_stack_memory_address));
  __ lw(a0, MemOperand(a0, 0));
  __ li(a2, Operand(address_of_regexp_stack_memory_size));
  __ lw(a2, MemOperand(a2, 0));
  __ addu(a0, a0, a2);
  __ sw(a0, MemOperand(sp, 3 * kPointerSize));

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  __ mov(a0, zero_reg);
  __ sw(a0, MemOperand(sp, 2 * kPointerSize));

  // Argument 5: static offsets vector buffer.
  __ li(a0, Operand(
        ExternalReference::address_of_static_offsets_vector(isolate())));
  __ sw(a0, MemOperand(sp, 1 * kPointerSize));

  // For arguments 4 and 3 get string length, calculate start of string data
  // calculate the shift of the index (0 for one-byte and 1 for two-byte).
  __ Addu(t2, subject, Operand(SeqString::kHeaderSize - kHeapObjectTag));
  __ Xor(a3, a3, Operand(1));  // 1 for 2-byte str, 0 for 1-byte.
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to two pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize.)
  __ lw(subject, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));
  // If slice offset is not 0, load the length from the original sliced string.
  // Argument 4, a3: End of string data
  // Argument 3, a2: Start of string data
  // Prepare start and end index of the input.
  __ sllv(t1, t0, a3);
  __ addu(t0, t2, t1);
  __ sllv(t1, a1, a3);
  __ addu(a2, t0, t1);

  __ lw(t2, FieldMemOperand(subject, String::kLengthOffset));
  __ sra(t2, t2, kSmiTagSize);
  __ sllv(t1, t2, a3);
  __ addu(a3, t0, t1);
  // Argument 2 (a1): Previous index.
  // Already there

  // Argument 1 (a0): Subject string.
  __ mov(a0, subject);

  // Locate the code entry and call it.
  __ Addu(t9, t9, Operand(Code::kHeaderSize - kHeapObjectTag));
  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm, t9);

  __ LeaveExitFrame(false, no_reg, true);

  // v0: result
  // subject: subject string (callee saved)
  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)
  // Check the result.
  Label success;
  __ Branch(&success, eq, v0, Operand(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  Label failure;
  __ Branch(&failure, eq, v0, Operand(NativeRegExpMacroAssembler::FAILURE));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ Branch(&runtime, ne, v0, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ li(a1, Operand(isolate()->factory()->the_hole_value()));
  __ li(a2, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      isolate())));
  __ lw(v0, MemOperand(a2, 0));
  __ Branch(&runtime, eq, v0, Operand(a1));

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  __ bind(&failure);
  // For failure and exception return null.
  __ li(v0, Operand(isolate()->factory()->null_value()));
  __ DropAndRet(4);

  // Process the result from the native regexp code.
  __ bind(&success);
  __ lw(a1,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  // Multiplying by 2 comes for free since r1 is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ Addu(a1, a1, Operand(2));  // a1 was a smi.

  __ lw(a0, MemOperand(sp, kLastMatchInfoOffset));
  __ JumpIfSmi(a0, &runtime);
  __ GetObjectType(a0, a2, a2);
  __ Branch(&runtime, ne, a2, Operand(JS_OBJECT_TYPE));
  // Check that the object has fast elements.
  __ lw(last_match_info_elements,
        FieldMemOperand(a0, JSArray::kElementsOffset));
  __ lw(a0, FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&runtime, ne, a0, Operand(at));
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ lw(a0,
        FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ Addu(a2, a1, Operand(RegExpImpl::kLastMatchOverhead));
  __ sra(at, a0, kSmiTagSize);
  __ Branch(&runtime, gt, a2, Operand(at));

  // a1: number of capture registers
  // subject: subject string
  // Store the capture count.
  __ sll(a2, a1, kSmiTagSize + kSmiShiftSize);  // To smi.
  __ sw(a2, FieldMemOperand(last_match_info_elements,
                             RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ sw(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  __ mov(a2, subject);
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastSubjectOffset,
                      subject,
                      t3,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs);
  __ mov(subject, a2);
  __ sw(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastInputOffset,
                      subject,
                      t3,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ li(a2, Operand(address_of_static_offsets_vector));

  // a1: number of capture registers
  // a2: offsets vector
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wrapping after zero.
  __ Addu(a0,
         last_match_info_elements,
         Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag));
  __ bind(&next_capture);
  __ Subu(a1, a1, Operand(1));
  __ Branch(&done, lt, a1, Operand(zero_reg));
  // Read the value from the static offsets vector buffer.
  __ lw(a3, MemOperand(a2, 0));
  __ addiu(a2, a2, kPointerSize);
  // Store the smi value in the last match info.
  __ sll(a3, a3, kSmiTagSize);  // Convert to Smi.
  __ sw(a3, MemOperand(a0, 0));
  __ Branch(&next_capture, USE_DELAY_SLOT);
  __ addiu(a0, a0, kPointerSize);  // In branch delay slot.

  __ bind(&done);

  // Return last match info.
  __ lw(v0, MemOperand(sp, kLastMatchInfoOffset));
  __ DropAndRet(4);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec);

  // Deferred code for string handling.
  // (5) Long external string?  If not, go to (7).
  __ bind(&not_seq_nor_cons);
  // Go to (7).
  __ Branch(&not_long_external, gt, a1, Operand(kExternalStringTag));

  // (6) External string.  Make it, offset-wise, look like a sequential string.
  __ bind(&external_string);
  __ lw(a0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbu(a0, FieldMemOperand(a0, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ And(at, a0, Operand(kIsIndirectStringMask));
    __ Assert(eq,
              kExternalStringExpectedButNotFound,
              at,
              Operand(zero_reg));
  }
  __ lw(subject,
        FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Subu(subject,
          subject,
          SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ jmp(&seq_string);    // Go to (5).

  // (7) Short external string or not a string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ And(at, a1, Operand(kIsNotStringMask | kShortExternalStringMask));
  __ Branch(&runtime, ne, at, Operand(zero_reg));

  // (8) Sliced string.  Replace subject with parent.  Go to (4).
  // Load offset into t0 and replace subject string with parent.
  __ lw(t0, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ sra(t0, t0, kSmiTagSize);
  __ lw(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ jmp(&check_underlying);  // Go to (4).
#endif  // V8_INTERPRETED_REGEXP
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

  DCHECK_EQ(*TypeFeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  // Load the cache state into t2.
  __ Lsa(t2, a2, a3, kPointerSizeLog2 - kSmiTagSize);
  __ lw(t2, FieldMemOperand(t2, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if t2 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in type-feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = t1;
  Register weak_value = t4;
  __ lw(weak_value, FieldMemOperand(t2, WeakCell::kValueOffset));
  __ Branch(&done, eq, a1, Operand(weak_value));
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&done, eq, t2, Operand(at));
  __ lw(feedback_map, FieldMemOperand(t2, HeapObject::kMapOffset));
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
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, t2);
  __ Branch(&megamorphic, ne, a1, Operand(t2));
  __ jmp(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ LoadRoot(at, Heap::kuninitialized_symbolRootIndex);
  __ Branch(&initialize, eq, t2, Operand(at));
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ Lsa(t2, a2, a3, kPointerSizeLog2 - kSmiTagSize);
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ sw(at, FieldMemOperand(t2, FixedArray::kHeaderSize));
  __ jmp(&done);

  // An uninitialized cache is patched with the function.
  __ bind(&initialize);
  // Make sure the function is the Array() function.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, t2);
  __ Branch(&not_array_function, ne, a1, Operand(t2));

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
  __ Lsa(at, a2, a3, kPointerSizeLog2 - kSmiTagSize);
  __ lw(t0, FieldMemOperand(at, FixedArray::kHeaderSize + kPointerSize));
  __ Addu(t0, t0, Operand(Smi::FromInt(1)));
  __ sw(t0, FieldMemOperand(at, FixedArray::kHeaderSize + kPointerSize));
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
  __ GetObjectType(a1, t1, t1);
  __ Branch(&non_function, ne, t1, Operand(JS_FUNCTION_TYPE));

  GenerateRecordCallTarget(masm);

  __ Lsa(t1, a2, a3, kPointerSizeLog2 - kSmiTagSize);
  Label feedback_register_initialized;
  // Put the AllocationSite from the feedback vector into a2, or undefined.
  __ lw(a2, FieldMemOperand(t1, FixedArray::kHeaderSize));
  __ lw(t1, FieldMemOperand(a2, AllocationSite::kMapOffset));
  __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
  __ Branch(&feedback_register_initialized, eq, t1, Operand(at));
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(a2, t1);

  // Pass function as new target.
  __ mov(a3, a1);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kConstructStubOffset));
  __ Addu(at, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);

  __ bind(&non_function);
  __ mov(a3, a1);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// Note: feedback_vector and slot are clobbered after the call.
static void IncrementCallCount(MacroAssembler* masm, Register feedback_vector,
                               Register slot) {
  __ Lsa(at, feedback_vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(slot, FieldMemOperand(at, FixedArray::kHeaderSize + kPointerSize));
  __ Addu(slot, slot, Operand(Smi::FromInt(1)));
  __ sw(slot, FieldMemOperand(at, FixedArray::kHeaderSize + kPointerSize));
}

void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // a1 - function
  // a3 - slot id
  // a2 - vector
  // t0 - loaded from vector[slot]
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, at);
  __ Branch(miss, ne, a1, Operand(at));

  __ li(a0, Operand(arg_count()));

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, a2, a3);

  __ mov(a2, t0);
  __ mov(a3, a1);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);
}


void CallICStub::Generate(MacroAssembler* masm) {
  // a1 - function
  // a3 - slot id (Smi)
  // a2 - vector
  Label extra_checks_or_miss, call, call_function, call_count_incremented;
  int argc = arg_count();
  ParameterCount actual(argc);

  // The checks. First, does r1 match the recorded monomorphic target?
  __ Lsa(t0, a2, a3, kPointerSizeLog2 - kSmiTagSize);
  __ lw(t0, FieldMemOperand(t0, FixedArray::kHeaderSize));

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

  __ lw(t1, FieldMemOperand(t0, WeakCell::kValueOffset));
  __ Branch(&extra_checks_or_miss, ne, a1, Operand(t1));

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(a1, &extra_checks_or_miss);

  __ bind(&call_function);

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, a2, a3);

  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET, al, zero_reg, Operand(zero_reg),
          USE_DELAY_SLOT);
  __ li(a0, Operand(argc));  // In delay slot.

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&call, eq, t0, Operand(at));

  // Verify that t0 contains an AllocationSite
  __ lw(t1, FieldMemOperand(t0, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
  __ Branch(&not_allocation_site, ne, t1, Operand(at));

  HandleArrayCase(masm, &miss);

  __ bind(&not_allocation_site);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ Branch(&miss);
  }

  __ LoadRoot(at, Heap::kuninitialized_symbolRootIndex);
  __ Branch(&uninitialized, eq, t0, Operand(at));

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(t0);
  __ GetObjectType(t0, t1, t1);
  __ Branch(&miss, ne, t1, Operand(JS_FUNCTION_TYPE));
  __ Lsa(t0, a2, a3, kPointerSizeLog2 - kSmiTagSize);
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ sw(at, FieldMemOperand(t0, FixedArray::kHeaderSize));

  __ bind(&call);
  IncrementCallCount(masm, a2, a3);

  __ bind(&call_count_incremented);

  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET, al, zero_reg, Operand(zero_reg),
          USE_DELAY_SLOT);
  __ li(a0, Operand(argc));  // In delay slot.

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(a1, &miss);

  // Goto miss case if we do not have a function.
  __ GetObjectType(a1, t0, t0);
  __ Branch(&miss, ne, t0, Operand(JS_FUNCTION_TYPE));

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, t0);
  __ Branch(&miss, eq, a1, Operand(t0));

  // Make sure the function belongs to the same native context.
  __ lw(t0, FieldMemOperand(a1, JSFunction::kContextOffset));
  __ lw(t0, ContextMemOperand(t0, Context::NATIVE_CONTEXT_INDEX));
  __ lw(t1, NativeContextMemOperand());
  __ Branch(&miss, ne, t0, Operand(t1));

  // Store the function. Use a stub since we need a frame for allocation.
  // a2 - vector
  // a3 - slot
  // a1 - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(masm->isolate());
    __ Push(a2, a3);
    __ Push(cp, a1);
    __ CallStub(&create_stub);
    __ Pop(cp, a1);
    __ Pop(a2, a3);
  }

  __ Branch(&call_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  __ Branch(&call_count_incremented);
}


void CallICStub::GenerateMiss(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);

  // Push the receiver and the function and feedback info.
  __ Push(a1, a2, a3);

  // Call the entry.
  __ CallRuntime(Runtime::kCallIC_Miss);

  // Move result to a1 and exit the internal frame.
  __ mov(a1, v0);
}


// StringCharCodeAtGenerator.
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  DCHECK(!t0.is(index_));
  DCHECK(!t0.is(result_));
  DCHECK(!t0.is(object_));
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    // If the receiver is a smi trigger the non-string case.
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ lw(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
    __ lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ And(t0, result_, Operand(kIsNotStringMask));
    __ Branch(receiver_not_string_, ne, t0, Operand(zero_reg));
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);

  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ lw(t0, FieldMemOperand(object_, String::kLengthOffset));
  __ Branch(index_out_of_range_, ls, t0, Operand(index_));

  __ sra(index_, index_, kSmiTagSize);

  StringCharLoadGenerator::Generate(masm,
                                    object_,
                                    index_,
                                    result_,
                                    &call_runtime_);

  __ sll(result_, result_, kSmiTagSize);
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
  __ lw(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
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
  __ sll(index_, index_, kSmiTagSize);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT);

  __ Move(result_, v0);

  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.

  DCHECK(!t0.is(result_));
  DCHECK(!t0.is(code_));

  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  DCHECK(base::bits::IsPowerOfTwo32(String::kMaxOneByteCharCodeU + 1));
  __ And(t0, code_, Operand(kSmiTagMask |
                            ((~String::kMaxOneByteCharCodeU) << kSmiTagSize)));
  __ Branch(&slow_case_, ne, t0, Operand(zero_reg));

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged one-byte char code.
  STATIC_ASSERT(kSmiTag == 0);
  __ Lsa(result_, result_, code_, kPointerSizeLog2 - kSmiTagSize);
  __ lw(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
  __ Branch(&slow_case_, eq, result_, Operand(t0));
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
  __ Move(result_, v0);

  call_helper.AfterCall(masm);
  __ Branch(&exit_);

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
    __ And(scratch, dest, Operand(kPointerAlignmentMask));
    __ Check(eq,
             kDestinationOfCopyNotAligned,
             scratch,
             Operand(zero_reg));
  }

  // Assumes word reads and writes are little endian.
  // Nothing to do for zero characters.
  Label done;

  if (encoding == String::TWO_BYTE_ENCODING) {
    __ Addu(count, count, count);
  }

  Register limit = count;  // Read until dest equals this.
  __ Addu(limit, dest, Operand(count));

  Label loop_entry, loop;
  // Copy bytes from src to dest until dest hits limit.
  __ Branch(&loop_entry);
  __ bind(&loop);
  __ lbu(scratch, MemOperand(src));
  __ Addu(src, src, Operand(1));
  __ sb(scratch, MemOperand(dest));
  __ Addu(dest, dest, Operand(1));
  __ bind(&loop_entry);
  __ Branch(&loop, lt, dest, Operand(limit));

  __ bind(&done);
}


void StringHelper::GenerateFlatOneByteStringEquals(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ lw(length, FieldMemOperand(left, String::kLengthOffset));
  __ lw(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Branch(&check_zero_length, eq, length, Operand(scratch2));
  __ bind(&strings_not_equal);
  DCHECK(is_int16(NOT_EQUAL));
  __ Ret(USE_DELAY_SLOT);
  __ li(v0, Operand(Smi::FromInt(NOT_EQUAL)));

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&compare_chars, ne, length, Operand(zero_reg));
  DCHECK(is_int16(EQUAL));
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
  __ lw(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ lw(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Subu(scratch3, scratch1, Operand(scratch2));
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
  __ Addu(scratch1, length,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ Addu(left, left, Operand(scratch1));
  __ Addu(right, right, Operand(scratch1));
  __ Subu(length, zero_reg, length);
  Register index = length;  // index = -length;


  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ Addu(scratch3, left, index);
  __ lbu(scratch1, MemOperand(scratch3));
  __ Addu(scratch3, right, index);
  __ lbu(scratch2, MemOperand(scratch3));
  __ Branch(chars_not_equal, ne, scratch1, Operand(scratch2));
  __ Addu(index, index, 1);
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
    __ lw(t0, FieldMemOperand(a2, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
    __ Assert(eq, kExpectedAllocationSite, t0, Operand(at));
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
    __ lw(a1, FieldMemOperand(a1, Oddball::kToNumberOffset));
    __ AssertSmi(a1);
    __ lw(a0, FieldMemOperand(a0, Oddball::kToNumberOffset));
    __ AssertSmi(a0);
  }
  __ Ret(USE_DELAY_SLOT);
  __ Subu(v0, a1, a0);

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
    __ Subu(v0, a0, a1);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(a1);
    __ SmiUntag(a0);
    __ Ret(USE_DELAY_SLOT);
    __ Subu(v0, a1, a0);
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
  __ Subu(a2, a0, Operand(kHeapObjectTag));
  __ ldc1(f2, MemOperand(a2, HeapNumber::kValueOffset));
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
  __ Subu(a2, a1, Operand(kHeapObjectTag));
  __ ldc1(f0, MemOperand(a2, HeapNumber::kValueOffset));
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
  __ lw(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
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
  __ lw(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));

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
  Register tmp3 = t0;
  Register tmp4 = t1;
  Register tmp5 = t2;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ lw(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
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
  __ subu(v0, a0, a1);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ And(a2, a1, a0);
  __ JumpIfSmi(a2, &miss);
  __ GetWeakValue(t0, cell);
  __ lw(a2, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ lw(a3, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&miss, ne, a2, Operand(t0));
  __ Branch(&miss, ne, a3, Operand(t0));

  if (Token::IsEqualityOp(op())) {
    __ Ret(USE_DELAY_SLOT);
    __ subu(v0, a0, a1);
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
    __ li(t0, Operand(Smi::FromInt(op())));
    __ addiu(sp, sp, -kPointerSize);
    __ CallRuntime(Runtime::kCompareIC_Miss, 3, kDontSaveFPRegs,
                   USE_DELAY_SLOT);
    __ sw(t0, MemOperand(sp));  // In the delay slot.
    // Compute the entry point of the rewritten stub.
    __ Addu(a2, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
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
  __ Subu(sp, sp, Operand(kCArgsSlotsSize));
  // Place the return address on the stack, making the call
  // GC safe. The RegExp backend also relies on this.
  __ sw(ra, MemOperand(sp, kCArgsSlotsSize));
  __ Call(t9);  // Call the C++ function.
  __ lw(t9, MemOperand(sp, kCArgsSlotsSize));

  if (FLAG_debug_code && FLAG_enable_slow_asserts) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC.
    // Dereference the address and check for this.
    __ lw(t0, MemOperand(t9));
    __ Assert(ne, kReceivedInvalidReturnAddress, t0,
        Operand(reinterpret_cast<uint32_t>(kZapValue)));
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
    __ lw(index, FieldMemOperand(properties, kCapacityOffset));
    __ Subu(index, index, Operand(1));
    __ And(index, index, Operand(
        Smi::FromInt(name->Hash() + NameDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ Lsa(index, index, index, 1);

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    Register tmp = properties;
    __ Lsa(tmp, properties, index, 1);
    __ lw(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

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
    __ lw(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ lbu(entity_name,
           FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ lw(properties,
          FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask =
      (ra.bit() | t2.bit() | t1.bit() | t0.bit() | a3.bit() |
       a2.bit() | a1.bit() | a0.bit() | v0.bit());

  __ MultiPush(spill_mask);
  __ lw(a0, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ li(a1, Operand(Handle<Name>(name)));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  __ mov(at, v0);
  __ MultiPop(spill_mask);

  __ Branch(done, eq, at, Operand(zero_reg));
  __ Branch(miss, ne, at, Operand(zero_reg));
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
  __ lw(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ sra(scratch1, scratch1, kSmiTagSize);  // convert smi to int
  __ Subu(scratch1, scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ lw(scratch2, FieldMemOperand(name, Name::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ Addu(scratch2, scratch2, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ srl(scratch2, scratch2, Name::kHashShift);
    __ And(scratch2, scratch1, scratch2);

    // Scale the index by multiplying by the element size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.

    __ Lsa(scratch2, scratch2, scratch2, 1);

    // Check if the key is identical to the name.
    __ Lsa(scratch2, elements, scratch2, 2);
    __ lw(at, FieldMemOperand(scratch2, kElementsStartOffset));
    __ Branch(done, eq, name, Operand(at));
  }

  const int spill_mask =
      (ra.bit() | t2.bit() | t1.bit() | t0.bit() |
       a3.bit() | a2.bit() | a1.bit() | a0.bit() | v0.bit()) &
      ~(scratch1.bit() | scratch2.bit());

  __ MultiPush(spill_mask);
  if (name.is(a0)) {
    DCHECK(!elements.is(a1));
    __ Move(a1, name);
    __ Move(a0, elements);
  } else {
    __ Move(a0, elements);
    __ Move(a1, name);
  }
  NameDictionaryLookupStub stub(masm->isolate(), POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ mov(scratch2, a2);
  __ mov(at, v0);
  __ MultiPop(spill_mask);

  __ Branch(done, ne, at, Operand(zero_reg));
  __ Branch(miss, eq, at, Operand(zero_reg));
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
  Register hash = t0;
  Register undefined = t1;
  Register entry_key = t2;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ lw(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ sra(mask, mask, kSmiTagSize);
  __ Subu(mask, mask, Operand(1));

  __ lw(hash, FieldMemOperand(key, Name::kHashFieldOffset));

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
      __ Addu(index, hash, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    } else {
      __ mov(index, hash);
    }
    __ srl(index, index, Name::kHashShift);
    __ And(index, mask, index);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    // index *= 3.
    __ Lsa(index, index, index, 1);

    STATIC_ASSERT(kSmiTagSize == 1);
    __ Lsa(index, dictionary, index, 2);
    __ lw(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ Branch(&not_in_dictionary, eq, entry_key, Operand(undefined));

    // Stop if found the property.
    __ Branch(&in_dictionary, eq, entry_key, Operand(key));

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ lw(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ lbu(entry_key,
             FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
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

    __ lw(regs_.scratch0(), MemOperand(regs_.address(), 0));
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

  __ And(regs_.scratch0(), regs_.object(), Operand(~Page::kPageAlignmentMask));
  __ lw(regs_.scratch1(),
        MemOperand(regs_.scratch0(),
                   MemoryChunk::kWriteBarrierCounterOffset));
  __ Subu(regs_.scratch1(), regs_.scratch1(), Operand(1));
  __ sw(regs_.scratch1(),
         MemOperand(regs_.scratch0(),
                    MemoryChunk::kWriteBarrierCounterOffset));
  __ Branch(&need_incremental, lt, regs_.scratch1(), Operand(zero_reg));

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
  __ lw(regs_.scratch0(), MemOperand(regs_.address(), 0));

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
  __ lw(a1, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ Addu(a1, a1, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ sll(a1, a1, kPointerSizeLog2);
  __ Ret(USE_DELAY_SLOT);
  __ Addu(sp, sp, a1);
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
  __ EmitLoadTypeFeedbackVector(a2);
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

  __ lw(cached_map,
        FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(0)));
  __ lw(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ Branch(&start_polymorphic, ne, receiver_map, Operand(cached_map));
  // found, now call handler.
  Register handler = feedback;
  __ lw(handler, FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ Addu(t9, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);


  Register length = scratch2;
  __ bind(&start_polymorphic);
  __ lw(length, FieldMemOperand(feedback, FixedArray::kLengthOffset));
  if (!is_polymorphic) {
    // If the IC could be monomorphic we have to make sure we don't go past the
    // end of the feedback array.
    __ Branch(miss, eq, length, Operand(Smi::FromInt(2)));
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
  __ Lsa(too_far, feedback, length, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Addu(pointer_reg, feedback,
          Operand(FixedArray::OffsetOfElementAt(2) - kHeapObjectTag));

  __ bind(&next_loop);
  __ lw(cached_map, MemOperand(pointer_reg));
  __ lw(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ Branch(&prepare_next, ne, receiver_map, Operand(cached_map));
  __ lw(handler, MemOperand(pointer_reg, kPointerSize));
  __ Addu(t9, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  __ bind(&prepare_next);
  __ Addu(pointer_reg, pointer_reg, Operand(kPointerSize * 2));
  __ Branch(&next_loop, lt, pointer_reg, Operand(too_far));

  // We exhausted our array of map handler pairs.
  __ jmp(miss);
}


static void HandleMonomorphicCase(MacroAssembler* masm, Register receiver,
                                  Register receiver_map, Register feedback,
                                  Register vector, Register slot,
                                  Register scratch, Label* compare_map,
                                  Label* load_smi_map, Label* try_array) {
  __ JumpIfSmi(receiver, load_smi_map);
  __ lw(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ bind(compare_map);
  Register cached_map = scratch;
  // Move the weak map into the weak_cell register.
  __ lw(cached_map, FieldMemOperand(feedback, WeakCell::kValueOffset));
  __ Branch(try_array, ne, cached_map, Operand(receiver_map));
  Register handler = feedback;

  __ Lsa(handler, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(handler,
        FieldMemOperand(handler, FixedArray::kHeaderSize + kPointerSize));
  __ Addu(t9, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);
}


void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // a1
  Register name = LoadWithVectorDescriptor::NameRegister();          // a2
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // a3
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // a0
  Register feedback = t0;
  Register receiver_map = t1;
  Register scratch1 = t4;

  __ Lsa(feedback, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ lw(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&not_array, ne, at, Operand(scratch1));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, t5, true, &miss);

  __ bind(&not_array);
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&miss, ne, at, Operand(feedback));
  masm->isolate()->load_stub_cache()->GenerateProbe(
      masm, receiver, name, feedback, receiver_map, scratch1, t5);

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
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // a1
  Register key = LoadWithVectorDescriptor::NameRegister();           // a2
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // a3
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // a0
  Register feedback = t0;
  Register receiver_map = t1;
  Register scratch1 = t4;

  __ Lsa(feedback, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ lw(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&not_array, ne, at, Operand(scratch1));
  // We have a polymorphic element handler.
  __ JumpIfNotSmi(key, &miss);

  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);
  HandleArrayCases(masm, feedback, receiver_map, scratch1, t5, true, &miss);

  __ bind(&not_array);
  // Is it generic?
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&try_poly_name, ne, at, Operand(feedback));
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ Branch(&miss, ne, key, Operand(feedback));
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ Lsa(feedback, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(feedback,
        FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, t5, false, &miss);

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
  Register receiver = StoreWithVectorDescriptor::ReceiverRegister();  // a1
  Register key = StoreWithVectorDescriptor::NameRegister();           // a2
  Register vector = StoreWithVectorDescriptor::VectorRegister();      // a3
  Register slot = StoreWithVectorDescriptor::SlotRegister();          // t0
  DCHECK(StoreWithVectorDescriptor::ValueRegister().is(a0));          // a0
  Register feedback = t1;
  Register receiver_map = t2;
  Register scratch1 = t5;

  __ Lsa(feedback, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ lw(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&not_array, ne, scratch1, Operand(at));

  Register scratch2 = t4;
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, true,
                   &miss);

  __ bind(&not_array);
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&miss, ne, feedback, Operand(at));
  masm->isolate()->store_stub_cache()->GenerateProbe(
      masm, receiver, key, feedback, receiver_map, scratch1, scratch2);

  __ bind(&miss);
  StoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ Branch(USE_DELAY_SLOT, &compare_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);  // In delay slot.
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
  __ lw(too_far, FieldMemOperand(feedback, FixedArray::kLengthOffset));

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
  __ Lsa(too_far, feedback, too_far, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Addu(pointer_reg, feedback,
          Operand(FixedArray::OffsetOfElementAt(0) - kHeapObjectTag));

  __ bind(&next_loop);
  __ lw(cached_map, MemOperand(pointer_reg));
  __ lw(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ Branch(&prepare_next, ne, receiver_map, Operand(cached_map));
  // Is it a transitioning store?
  __ lw(too_far, MemOperand(pointer_reg, kPointerSize));
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&transition_call, ne, too_far, Operand(at));
  __ lw(pointer_reg, MemOperand(pointer_reg, kPointerSize * 2));
  __ Addu(t9, pointer_reg, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  __ bind(&transition_call);
  __ lw(too_far, FieldMemOperand(too_far, WeakCell::kValueOffset));
  __ JumpIfSmi(too_far, miss);

  __ lw(receiver_map, MemOperand(pointer_reg, kPointerSize * 2));

  // Load the map into the correct register.
  DCHECK(feedback.is(StoreTransitionDescriptor::MapRegister()));
  __ mov(feedback, too_far);

  __ Addu(t9, receiver_map, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  __ bind(&prepare_next);
  __ Addu(pointer_reg, pointer_reg, Operand(kPointerSize * 3));
  __ Branch(&next_loop, lt, pointer_reg, Operand(too_far));

  // We exhausted our array of map handler pairs.
  __ jmp(miss);
}

void KeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = StoreWithVectorDescriptor::ReceiverRegister();  // a1
  Register key = StoreWithVectorDescriptor::NameRegister();           // a2
  Register vector = StoreWithVectorDescriptor::VectorRegister();      // a3
  Register slot = StoreWithVectorDescriptor::SlotRegister();          // t0
  DCHECK(StoreWithVectorDescriptor::ValueRegister().is(a0));          // a0
  Register feedback = t1;
  Register receiver_map = t2;
  Register scratch1 = t5;

  __ Lsa(feedback, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ lw(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&not_array, ne, scratch1, Operand(at));

  // We have a polymorphic element handler.
  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);

  Register scratch2 = t4;

  HandlePolymorphicStoreCase(masm, feedback, receiver_map, scratch1, scratch2,
                             &miss);

  __ bind(&not_array);
  // Is it generic?
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&try_poly_name, ne, feedback, Operand(at));
  Handle<Code> megamorphic_stub =
      KeyedStoreIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ Branch(&miss, ne, key, Operand(feedback));
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ Lsa(feedback, vector, slot, kPointerSizeLog2 - kSmiTagSize);
  __ lw(feedback,
        FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, false,
                   &miss);

  __ bind(&miss);
  KeyedStoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ Branch(USE_DELAY_SLOT, &compare_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);  // In delay slot.
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
  __ Subu(a0, ra, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ Addu(a1, sp, Operand(kNumSavedRegs * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ mov(s5, sp);
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    __ And(sp, sp, Operand(-frame_alignment));
  }
  __ Subu(sp, sp, kCArgsSlotsSize);
#if defined(V8_HOST_ARCH_MIPS)
  int32_t entry_hook =
      reinterpret_cast<int32_t>(isolate()->function_entry_hook());
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
    __ Addu(sp, sp, kCArgsSlotsSize);
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
  __ lw(t1, MemOperand(sp, 0));
  __ Branch(&normal_sequence, eq, t1, Operand(zero_reg));

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
    __ Addu(a3, a3, Operand(1));

    if (FLAG_debug_code) {
      __ lw(t1, FieldMemOperand(a2, 0));
      __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite, t1, Operand(at));
    }

    // Save the resulting elements kind in type info. We can't just store a3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ lw(t0, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));
    __ Addu(t0, t0, Operand(Smi::FromInt(kFastElementsKindPackedToHoley)));
    __ sw(t0, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));


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
  if (argument_count() == ANY) {
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
  //  -- a0 : argc (only if argument_count() is ANY or MORE_THAN_ONE)
  //  -- a1 : constructor
  //  -- a2 : AllocationSite or undefined
  //  -- a3 : Original constructor
  //  -- sp[0] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ lw(t0, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ SmiTst(t0, at);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction,
        at, Operand(zero_reg));
    __ GetObjectType(t0, t0, t1);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction,
        t1, Operand(MAP_TYPE));

    // We should either have undefined in a2 or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(a2, t0);
  }

  // Enter the context of the Array function.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  Label subclassing;
  __ Branch(&subclassing, ne, a1, Operand(a3));

  Label no_info;
  // Get the elements kind and case on that.
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&no_info, eq, a2, Operand(at));

  __ lw(a3, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(a3);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ And(a3, a3, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing.
  __ bind(&subclassing);
  switch (argument_count()) {
    case ANY:
    case MORE_THAN_ONE:
      __ Lsa(at, sp, a0, kPointerSizeLog2);
      __ sw(a1, MemOperand(at));
      __ li(at, Operand(3));
      __ addu(a0, a0, at);
      break;
    case NONE:
      __ sw(a1, MemOperand(sp, 0 * kPointerSize));
      __ li(a0, Operand(3));
      break;
    case ONE:
      __ sw(a1, MemOperand(sp, 1 * kPointerSize));
      __ li(a0, Operand(4));
      break;
  }
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
    __ lw(at, MemOperand(sp, 0));

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
    __ lw(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ SmiTst(a3, at);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction,
        at, Operand(zero_reg));
    __ GetObjectType(a3, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction,
        t0, Operand(MAP_TYPE));
  }

  // Figure out the right elements kind.
  __ lw(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into a3. We only need the first byte,
  // but the following bit field extraction takes care of that anyway.
  __ lbu(a3, FieldMemOperand(a3, Map::kBitField2Offset));
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


void FastNewObjectStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a1 : target
  //  -- a3 : new target
  //  -- cp : context
  //  -- ra : return address
  // -----------------------------------
  __ AssertFunction(a1);
  __ AssertReceiver(a3);

  // Verify that the new target is a JSFunction.
  Label new_object;
  __ GetObjectType(a3, a2, a2);
  __ Branch(&new_object, ne, a2, Operand(JS_FUNCTION_TYPE));

  // Load the initial map and verify that it's in fact a map.
  __ lw(a2, FieldMemOperand(a3, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(a2, &new_object);
  __ GetObjectType(a2, a0, a0);
  __ Branch(&new_object, ne, a0, Operand(MAP_TYPE));

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  __ lw(a0, FieldMemOperand(a2, Map::kConstructorOrBackPointerOffset));
  __ Branch(&new_object, ne, a0, Operand(a1));

  // Allocate the JSObject on the heap.
  Label allocate, done_allocate;
  __ lbu(t0, FieldMemOperand(a2, Map::kInstanceSizeOffset));
  __ Allocate(t0, v0, t1, a0, &allocate, SIZE_IN_WORDS);
  __ bind(&done_allocate);

  // Initialize the JSObject fields.
  __ sw(a2, FieldMemOperand(v0, JSObject::kMapOffset));
  __ LoadRoot(a3, Heap::kEmptyFixedArrayRootIndex);
  __ sw(a3, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(a3, FieldMemOperand(v0, JSObject::kElementsOffset));
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ Addu(a1, v0, Operand(JSObject::kHeaderSize - kHeapObjectTag));

  // ----------- S t a t e -------------
  //  -- v0 : result (tagged)
  //  -- a1 : result fields (untagged)
  //  -- t1 : result end (untagged)
  //  -- a2 : initial map
  //  -- cp : context
  //  -- ra : return address
  // -----------------------------------

  // Perform in-object slack tracking if requested.
  Label slack_tracking;
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  __ lw(a3, FieldMemOperand(a2, Map::kBitField3Offset));
  __ And(at, a3, Operand(Map::ConstructionCounter::kMask));
  __ Branch(USE_DELAY_SLOT, &slack_tracking, ne, at, Operand(0));
  __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);  // In delay slot.
  {
    // Initialize all in-object fields with undefined.
    __ InitializeFieldsWithFiller(a1, t1, a0);
    __ Ret();
  }
  __ bind(&slack_tracking);
  {
    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    __ Subu(a3, a3, Operand(1 << Map::ConstructionCounter::kShift));
    __ sw(a3, FieldMemOperand(a2, Map::kBitField3Offset));

    // Initialize the in-object fields with undefined.
    __ lbu(t0, FieldMemOperand(a2, Map::kUnusedPropertyFieldsOffset));
    __ sll(t0, t0, kPointerSizeLog2);
    __ subu(t0, t1, t0);
    __ InitializeFieldsWithFiller(a1, t0, a0);

    // Initialize the remaining (reserved) fields with one pointer filler map.
    __ LoadRoot(a0, Heap::kOnePointerFillerMapRootIndex);
    __ InitializeFieldsWithFiller(a1, t1, a0);

    // Check if we can finalize the instance size.
    Label finalize;
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);
    __ And(a3, a3, Operand(Map::ConstructionCounter::kMask));
    __ Branch(&finalize, eq, a3, Operand(zero_reg));
    __ Ret();

    // Finalize the instance size.
    __ bind(&finalize);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(v0, a2);
      __ CallRuntime(Runtime::kFinalizeInstanceSize);
      __ Pop(v0);
    }
    __ Ret();
  }

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagSize == 1);
    __ sll(t0, t0, kPointerSizeLog2 + kSmiTagSize);
    __ Push(a2, t0);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(a2);
  }
  __ lbu(t1, FieldMemOperand(a2, Map::kInstanceSizeOffset));
  __ Lsa(t1, v0, t1, kPointerSizeLog2);
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ Subu(t1, t1, Operand(kHeapObjectTag));
  __ jmp(&done_allocate);

  // Fall back to %NewObject.
  __ bind(&new_object);
  __ Push(a1, a3);
  __ TailCallRuntime(Runtime::kNewObject);
}


void FastNewRestParameterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a1 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- ra : return address
  // -----------------------------------
  __ AssertFunction(a1);

  // Make a2 point to the JavaScript frame.
  __ mov(a2, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ lw(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ lw(a3, MemOperand(a2, StandardFrameConstants::kFunctionOffset));
    __ Branch(&ok, eq, a1, Operand(a3));
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ lw(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
  __ lw(a3, MemOperand(a2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&no_rest_parameters, ne, a3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ lw(a0, MemOperand(a2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ lw(a3, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3,
        FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ Subu(a0, a0, Operand(a3));
  __ Branch(&rest_parameters, gt, a0, Operand(zero_reg));

  // Return an empty rest parameter array.
  __ bind(&no_rest_parameters);
  {
    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- ra : return address
    // -----------------------------------

    // Allocate an empty rest parameter array.
    Label allocate, done_allocate;
    __ Allocate(JSArray::kSize, v0, a0, a1, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the rest parameter array in v0.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, a1);
    __ sw(a1, FieldMemOperand(v0, JSArray::kMapOffset));
    __ LoadRoot(a1, Heap::kEmptyFixedArrayRootIndex);
    __ sw(a1, FieldMemOperand(v0, JSArray::kPropertiesOffset));
    __ sw(a1, FieldMemOperand(v0, JSArray::kElementsOffset));
    __ Move(a1, Smi::FromInt(0));
    __ Ret(USE_DELAY_SLOT);
    __ sw(a1, FieldMemOperand(v0, JSArray::kLengthOffset));  // In delay slot
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);

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
    __ Lsa(a2, a2, a0, kPointerSizeLog2 - 1);
    __ Addu(a2, a2, Operand(StandardFrameConstants::kCallerSPOffset -
                            1 * kPointerSize));

    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- a0 : number of rest parameters (tagged)
    //  -- a1 : function
    //  -- a2 : pointer to first rest parameters
    //  -- ra : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ li(t0, Operand(JSArray::kSize + FixedArray::kHeaderSize));
    __ Lsa(t0, t0, a0, kPointerSizeLog2 - 1);
    __ Allocate(t0, v0, a3, t1, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the elements array in v0.
    __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
    __ sw(at, FieldMemOperand(v0, FixedArray::kMapOffset));
    __ sw(a0, FieldMemOperand(v0, FixedArray::kLengthOffset));
    __ Addu(a3, v0, Operand(FixedArray::kHeaderSize));
    {
      Label loop, done_loop;
      __ sll(at, a0, kPointerSizeLog2 - 1);
      __ Addu(a1, a3, at);
      __ bind(&loop);
      __ Branch(&done_loop, eq, a1, Operand(a3));
      __ lw(at, MemOperand(a2, 0 * kPointerSize));
      __ sw(at, FieldMemOperand(a3, 0 * kPointerSize));
      __ Subu(a2, a2, Operand(1 * kPointerSize));
      __ Addu(a3, a3, Operand(1 * kPointerSize));
      __ jmp(&loop);
      __ bind(&done_loop);
    }

    // Setup the rest parameter array in a3.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, at);
    __ sw(at, FieldMemOperand(a3, JSArray::kMapOffset));
    __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
    __ sw(at, FieldMemOperand(a3, JSArray::kPropertiesOffset));
    __ sw(v0, FieldMemOperand(a3, JSArray::kElementsOffset));
    __ sw(a0, FieldMemOperand(a3, JSArray::kLengthOffset));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a3);  // In delay slot

    // Fall back to %AllocateInNewSpace (if not too big).
    Label too_big_for_new_space;
    __ bind(&allocate);
    __ Branch(&too_big_for_new_space, gt, t0,
              Operand(kMaxRegularHeapObjectSize));
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(t0);
      __ Push(a0, a2, t0);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ Pop(a0, a2);
    }
    __ jmp(&done_allocate);

    // Fall back to %NewStrictArguments.
    __ bind(&too_big_for_new_space);
    __ Push(a1);
    __ TailCallRuntime(Runtime::kNewStrictArguments);
  }
}


void FastNewSloppyArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a1 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- ra : return address
  // -----------------------------------
  __ AssertFunction(a1);

  // Make t0 point to the JavaScript frame.
  __ mov(t0, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ lw(t0, MemOperand(t0, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ lw(a3, MemOperand(t0, StandardFrameConstants::kFunctionOffset));
    __ Branch(&ok, eq, a1, Operand(a3));
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2,
        FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ Lsa(a3, t0, a2, kPointerSizeLog2 - 1);
  __ Addu(a3, a3, Operand(StandardFrameConstants::kCallerSPOffset));

  // a1 : function
  // a2 : number of parameters (tagged)
  // a3 : parameters pointer
  // t0 : Javascript frame pointer
  // Registers used over whole function:
  //  t1 : arguments count (tagged)
  //  t2 : mapped parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ lw(t0, MemOperand(t0, StandardFrameConstants::kCallerFPOffset));
  __ lw(a0, MemOperand(t0, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&adaptor_frame, eq, a0,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // No adaptor, parameter count = argument count.
  __ mov(t1, a2);
  __ Branch(USE_DELAY_SLOT, &try_allocate);
  __ mov(t2, a2);  // In delay slot.

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ lw(t1, MemOperand(t0, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Lsa(t0, t0, t1, 1);
  __ Addu(a3, t0, Operand(StandardFrameConstants::kCallerSPOffset));

  // t1 = argument count (tagged)
  // t2 = parameter count (tagged)
  // Compute the mapped parameter count = min(t2, t1) in t2.
  __ mov(t2, a2);
  __ Branch(&try_allocate, le, t2, Operand(t1));
  __ mov(t2, t1);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  Label param_map_size;
  DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
  __ Branch(USE_DELAY_SLOT, &param_map_size, eq, t2, Operand(zero_reg));
  __ mov(t5, zero_reg);  // In delay slot: param map size = 0 when t2 == 0.
  __ sll(t5, t2, 1);
  __ addiu(t5, t5, kParameterMapHeaderSize);
  __ bind(&param_map_size);

  // 2. Backing store.
  __ Lsa(t5, t5, t1, 1);
  __ Addu(t5, t5, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ Addu(t5, t5, Operand(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(t5, v0, t5, t0, &runtime, NO_ALLOCATION_FLAGS);

  // v0 = address of new object(s) (tagged)
  // a2 = argument count (smi-tagged)
  // Get the arguments boilerplate from the current native context into t0.
  const int kNormalOffset =
      Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);

  __ lw(t0, NativeContextMemOperand());
  Label skip2_ne, skip2_eq;
  __ Branch(&skip2_ne, ne, t2, Operand(zero_reg));
  __ lw(t0, MemOperand(t0, kNormalOffset));
  __ bind(&skip2_ne);

  __ Branch(&skip2_eq, eq, t2, Operand(zero_reg));
  __ lw(t0, MemOperand(t0, kAliasedOffset));
  __ bind(&skip2_eq);

  // v0 = address of new object (tagged)
  // a2 = argument count (smi-tagged)
  // t0 = address of arguments map (tagged)
  // t2 = mapped parameter count (tagged)
  __ sw(t0, FieldMemOperand(v0, JSObject::kMapOffset));
  __ LoadRoot(t5, Heap::kEmptyFixedArrayRootIndex);
  __ sw(t5, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(t5, FieldMemOperand(v0, JSObject::kElementsOffset));

  // Set up the callee in-object property.
  __ AssertNotSmi(a1);
  __ sw(a1, FieldMemOperand(v0, JSSloppyArgumentsObject::kCalleeOffset));

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(t1);
  __ sw(t1, FieldMemOperand(v0, JSSloppyArgumentsObject::kLengthOffset));

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, t0 will point there, otherwise
  // it will point to the backing store.
  __ Addu(t0, v0, Operand(JSSloppyArgumentsObject::kSize));
  __ sw(t0, FieldMemOperand(v0, JSObject::kElementsOffset));

  // v0 = address of new object (tagged)
  // a2 = argument count (tagged)
  // t0 = address of parameter map or backing store (tagged)
  // t2 = mapped parameter count (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  Label skip3;
  __ Branch(&skip3, ne, t2, Operand(Smi::FromInt(0)));
  // Move backing store address to a1, because it is
  // expected there when filling in the unmapped arguments.
  __ mov(a1, t0);
  __ bind(&skip3);

  __ Branch(&skip_parameter_map, eq, t2, Operand(Smi::FromInt(0)));

  __ LoadRoot(t1, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ sw(t1, FieldMemOperand(t0, FixedArray::kMapOffset));
  __ Addu(t1, t2, Operand(Smi::FromInt(2)));
  __ sw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  __ sw(cp, FieldMemOperand(t0, FixedArray::kHeaderSize + 0 * kPointerSize));
  __ Lsa(t1, t0, t2, 1);
  __ Addu(t1, t1, Operand(kParameterMapHeaderSize));
  __ sw(t1, FieldMemOperand(t0, FixedArray::kHeaderSize + 1 * kPointerSize));

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ mov(t1, t2);
  __ Addu(t5, a2, Operand(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ Subu(t5, t5, Operand(t2));
  __ LoadRoot(t3, Heap::kTheHoleValueRootIndex);
  __ Lsa(a1, t0, t1, 1);
  __ Addu(a1, a1, Operand(kParameterMapHeaderSize));

  // a1 = address of backing store (tagged)
  // t0 = address of parameter map (tagged)
  // a0 = temporary scratch (a.o., for address calculation)
  // t1 = loop variable (tagged)
  // t3 = the hole value
  __ jmp(&parameters_test);

  __ bind(&parameters_loop);
  __ Subu(t1, t1, Operand(Smi::FromInt(1)));
  __ sll(a0, t1, 1);
  __ Addu(a0, a0, Operand(kParameterMapHeaderSize - kHeapObjectTag));
  __ Addu(t6, t0, a0);
  __ sw(t5, MemOperand(t6));
  __ Subu(a0, a0, Operand(kParameterMapHeaderSize - FixedArray::kHeaderSize));
  __ Addu(t6, a1, a0);
  __ sw(t3, MemOperand(t6));
  __ Addu(t5, t5, Operand(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ Branch(&parameters_loop, ne, t1, Operand(Smi::FromInt(0)));

  // t1 = argument count (tagged).
  __ lw(t1, FieldMemOperand(v0, JSSloppyArgumentsObject::kLengthOffset));

  __ bind(&skip_parameter_map);
  // v0 = address of new object (tagged)
  // a1 = address of backing store (tagged)
  // t1 = argument count (tagged)
  // t2 = mapped parameter count (tagged)
  // t5 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(t5, Heap::kFixedArrayMapRootIndex);
  __ sw(t5, FieldMemOperand(a1, FixedArray::kMapOffset));
  __ sw(t1, FieldMemOperand(a1, FixedArray::kLengthOffset));

  Label arguments_loop, arguments_test;
  __ sll(t6, t2, 1);
  __ Subu(a3, a3, Operand(t6));
  __ jmp(&arguments_test);

  __ bind(&arguments_loop);
  __ Subu(a3, a3, Operand(kPointerSize));
  __ lw(t0, MemOperand(a3, 0));
  __ Lsa(t5, a1, t2, 1);
  __ sw(t0, FieldMemOperand(t5, FixedArray::kHeaderSize));
  __ Addu(t2, t2, Operand(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ Branch(&arguments_loop, lt, t2, Operand(t1));

  // Return.
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // t1 = argument count (tagged)
  __ bind(&runtime);
  __ Push(a1, a3, t1);
  __ TailCallRuntime(Runtime::kNewSloppyArguments);
}


void FastNewStrictArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a1 : function
  //  -- cp : context
  //  -- fp : frame pointer
  //  -- ra : return address
  // -----------------------------------
  __ AssertFunction(a1);

  // Make a2 point to the JavaScript frame.
  __ mov(a2, fp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ lw(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ lw(a3, MemOperand(a2, StandardFrameConstants::kFunctionOffset));
    __ Branch(&ok, eq, a1, Operand(a3));
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ lw(a3, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
  __ lw(a0, MemOperand(a3, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&arguments_adaptor, eq, a0,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  {
    __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a0,
          FieldMemOperand(t0, SharedFunctionInfo::kFormalParameterCountOffset));
    __ Lsa(a2, a2, a0, kPointerSizeLog2 - 1);
    __ Addu(a2, a2, Operand(StandardFrameConstants::kCallerSPOffset -
                            1 * kPointerSize));
  }
  __ Branch(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    __ lw(a0, MemOperand(a3, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ Lsa(a2, a3, a0, kPointerSizeLog2 - 1);
    __ Addu(a2, a2, Operand(StandardFrameConstants::kCallerSPOffset -
                            1 * kPointerSize));
  }
  __ bind(&arguments_done);

  // ----------- S t a t e -------------
  //  -- cp : context
  //  -- a0 : number of rest parameters (tagged)
  //  -- a1 : function
  //  -- a2 : pointer to first rest parameters
  //  -- ra : return address
  // -----------------------------------

  // Allocate space for the strict arguments object plus the backing store.
  Label allocate, done_allocate;
  __ li(t0, Operand(JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize));
  __ Lsa(t0, t0, a0, kPointerSizeLog2 - 1);
  __ Allocate(t0, v0, a3, t1, &allocate, NO_ALLOCATION_FLAGS);
  __ bind(&done_allocate);

  // Setup the elements array in v0.
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ sw(at, FieldMemOperand(v0, FixedArray::kMapOffset));
  __ sw(a0, FieldMemOperand(v0, FixedArray::kLengthOffset));
  __ Addu(a3, v0, Operand(FixedArray::kHeaderSize));
  {
    Label loop, done_loop;
    __ sll(at, a0, kPointerSizeLog2 - 1);
    __ Addu(a1, a3, at);
    __ bind(&loop);
    __ Branch(&done_loop, eq, a1, Operand(a3));
    __ lw(at, MemOperand(a2, 0 * kPointerSize));
    __ sw(at, FieldMemOperand(a3, 0 * kPointerSize));
    __ Subu(a2, a2, Operand(1 * kPointerSize));
    __ Addu(a3, a3, Operand(1 * kPointerSize));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Setup the strict arguments object in a3.
  __ LoadNativeContextSlot(Context::STRICT_ARGUMENTS_MAP_INDEX, at);
  __ sw(at, FieldMemOperand(a3, JSStrictArgumentsObject::kMapOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ sw(at, FieldMemOperand(a3, JSStrictArgumentsObject::kPropertiesOffset));
  __ sw(v0, FieldMemOperand(a3, JSStrictArgumentsObject::kElementsOffset));
  __ sw(a0, FieldMemOperand(a3, JSStrictArgumentsObject::kLengthOffset));
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a3);  // In delay slot

  // Fall back to %AllocateInNewSpace (if not too big).
  Label too_big_for_new_space;
  __ bind(&allocate);
  __ Branch(&too_big_for_new_space, gt, t0, Operand(kMaxRegularHeapObjectSize));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(t0);
    __ Push(a0, a2, t0);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(a0, a2);
  }
  __ jmp(&done_allocate);

  // Fall back to %NewStrictArguments.
  __ bind(&too_big_for_new_space);
  __ Push(a1);
  __ TailCallRuntime(Runtime::kNewStrictArguments);
}


void StoreGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = cp;
  Register slot_reg = a2;
  Register value_reg = a0;
  Register cell_reg = t0;
  Register cell_value_reg = t1;
  Register cell_details_reg = t2;
  Label fast_heapobject_case, fast_smi_case, slow_case;

  if (FLAG_debug_code) {
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Check(ne, kUnexpectedValue, value_reg, Operand(at));
  }

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ lw(cell_reg, ContextMemOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = cell_reg;
  }

  // Load the PropertyCell at the specified slot.
  __ Lsa(at, context_reg, slot_reg, kPointerSizeLog2);
  __ lw(cell_reg, ContextMemOperand(at, 0));

  // Load PropertyDetails for the cell (actually only the cell_type and kind).
  __ lw(cell_details_reg,
        FieldMemOperand(cell_reg, PropertyCell::kDetailsOffset));
  __ SmiUntag(cell_details_reg);
  __ And(cell_details_reg, cell_details_reg,
         PropertyDetails::PropertyCellTypeField::kMask |
             PropertyDetails::KindField::kMask |
             PropertyDetails::kAttributesReadOnlyMask);

  // Check if PropertyCell holds mutable data.
  Label not_mutable_data;
  __ Branch(&not_mutable_data, ne, cell_details_reg,
            Operand(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kMutable) |
                    PropertyDetails::KindField::encode(kData)));
  __ JumpIfSmi(value_reg, &fast_smi_case);
  __ bind(&fast_heapobject_case);
  __ sw(value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ RecordWriteField(cell_reg, PropertyCell::kValueOffset, value_reg,
                      cell_details_reg, kRAHasNotBeenSaved, kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // RecordWriteField clobbers the value register, so we need to reload.
  __ Ret(USE_DELAY_SLOT);
  __ lw(value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ bind(&not_mutable_data);

  // Check if PropertyCell value matches the new value (relevant for Constant,
  // ConstantType and Undefined cells).
  Label not_same_value;
  __ lw(cell_value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ Branch(&not_same_value, ne, value_reg, Operand(cell_value_reg));
  // Make sure the PropertyCell is not marked READ_ONLY.
  __ And(at, cell_details_reg, PropertyDetails::kAttributesReadOnlyMask);
  __ Branch(&slow_case, ne, at, Operand(zero_reg));
  if (FLAG_debug_code) {
    Label done;
    // This can only be true for Constant, ConstantType and Undefined cells,
    // because we never store the_hole via this stub.
    __ Branch(&done, eq, cell_details_reg,
              Operand(PropertyDetails::PropertyCellTypeField::encode(
                          PropertyCellType::kConstant) |
                      PropertyDetails::KindField::encode(kData)));
    __ Branch(&done, eq, cell_details_reg,
              Operand(PropertyDetails::PropertyCellTypeField::encode(
                          PropertyCellType::kConstantType) |
                      PropertyDetails::KindField::encode(kData)));
    __ Check(eq, kUnexpectedValue, cell_details_reg,
             Operand(PropertyDetails::PropertyCellTypeField::encode(
                         PropertyCellType::kUndefined) |
                     PropertyDetails::KindField::encode(kData)));
    __ bind(&done);
  }
  __ Ret();
  __ bind(&not_same_value);

  // Check if PropertyCell contains data with constant type (and is not
  // READ_ONLY).
  __ Branch(&slow_case, ne, cell_details_reg,
            Operand(PropertyDetails::PropertyCellTypeField::encode(
                        PropertyCellType::kConstantType) |
                    PropertyDetails::KindField::encode(kData)));

  // Now either both old and new values must be SMIs or both must be heap
  // objects with same map.
  Label value_is_heap_object;
  __ JumpIfNotSmi(value_reg, &value_is_heap_object);
  __ JumpIfNotSmi(cell_value_reg, &slow_case);
  // Old and new values are SMIs, no need for a write barrier here.
  __ bind(&fast_smi_case);
  __ Ret(USE_DELAY_SLOT);
  __ sw(value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ bind(&value_is_heap_object);
  __ JumpIfSmi(cell_value_reg, &slow_case);
  Register cell_value_map_reg = cell_value_reg;
  __ lw(cell_value_map_reg,
        FieldMemOperand(cell_value_reg, HeapObject::kMapOffset));
  __ Branch(&fast_heapobject_case, eq, cell_value_map_reg,
            FieldMemOperand(value_reg, HeapObject::kMapOffset));

  // Fallback to the runtime.
  __ bind(&slow_case);
  __ SmiTag(slot_reg);
  __ Push(slot_reg, value_reg);
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
  __ lb(t9, MemOperand(t9, 0));
  __ Branch(&profiler_disabled, eq, t9, Operand(zero_reg));

  // Additional parameter is the address of the actual callback.
  __ li(t9, Operand(thunk_ref));
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  __ mov(t9, function_address);
  __ bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  __ li(s3, Operand(next_address));
  __ lw(s0, MemOperand(s3, kNextOffset));
  __ lw(s1, MemOperand(s3, kLimitOffset));
  __ lw(s2, MemOperand(s3, kLevelOffset));
  __ Addu(s2, s2, Operand(1));
  __ sw(s2, MemOperand(s3, kLevelOffset));

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
  __ lw(v0, return_value_operand);
  __ bind(&return_value_loaded);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ sw(s0, MemOperand(s3, kNextOffset));
  if (__ emit_debug_code()) {
    __ lw(a1, MemOperand(s3, kLevelOffset));
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall, a1, Operand(s2));
  }
  __ Subu(s2, s2, Operand(1));
  __ sw(s2, MemOperand(s3, kLevelOffset));
  __ lw(at, MemOperand(s3, kLimitOffset));
  __ Branch(&delete_allocated_handles, ne, s1, Operand(at));

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ lw(cp, *context_restore_operand);
  }
  if (stack_space_offset != kInvalidStackOffset) {
    // ExitFrame contains four MIPS argument slots after DirectCEntryStub call
    // so this must be accounted for.
    __ lw(s0, MemOperand(sp, stack_space_offset + kCArgsSlotsSize));
  } else {
    __ li(s0, Operand(stack_space));
  }
  __ LeaveExitFrame(false, s0, !restore_context, NO_EMIT_RETURN,
                    stack_space_offset != kInvalidStackOffset);

  // Check if the function scheduled an exception.
  __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
  __ li(at, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ lw(t1, MemOperand(at));
  __ Branch(&promote_scheduled_exception, ne, t0, Operand(t1));

  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ sw(s1, MemOperand(s3, kLimitOffset));
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
  //  -- t0                  : call_data
  //  -- a2                  : holder
  //  -- a1                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 4]   : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register callee = a0;
  Register call_data = t0;
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
    __ lw(context, FieldMemOperand(callee, JSFunction::kContextOffset));
  }

  Register scratch = call_data;
  if (!call_data_undefined()) {
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  }
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
  __ Addu(a0, sp, Operand(1 * kPointerSize));
  // FunctionCallbackInfo::implicit_args_
  __ sw(scratch, MemOperand(a0, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ Addu(at, scratch, Operand((FCA::kArgsLength - 1 + argc()) * kPointerSize));
  __ sw(at, MemOperand(a0, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ li(at, Operand(argc()));
  __ sw(at, MemOperand(a0, 2 * kPointerSize));

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
  Register scratch = t0;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = a2;

  // Here and below +1 is for name() pushed after the args_ array.
  typedef PropertyCallbackArguments PCA;
  __ Subu(sp, sp, (PCA::kArgsLength + 1) * kPointerSize);
  __ sw(receiver, MemOperand(sp, (PCA::kThisIndex + 1) * kPointerSize));
  __ lw(scratch, FieldMemOperand(callback, AccessorInfo::kDataOffset));
  __ sw(scratch, MemOperand(sp, (PCA::kDataIndex + 1) * kPointerSize));
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ sw(scratch, MemOperand(sp, (PCA::kReturnValueOffset + 1) * kPointerSize));
  __ sw(scratch, MemOperand(sp, (PCA::kReturnValueDefaultValueIndex + 1) *
                                    kPointerSize));
  __ li(scratch, Operand(ExternalReference::isolate_address(isolate())));
  __ sw(scratch, MemOperand(sp, (PCA::kIsolateIndex + 1) * kPointerSize));
  __ sw(holder, MemOperand(sp, (PCA::kHolderIndex + 1) * kPointerSize));
  // should_throw_on_error -> false
  DCHECK(Smi::FromInt(0) == nullptr);
  __ sw(zero_reg,
        MemOperand(sp, (PCA::kShouldThrowOnErrorIndex + 1) * kPointerSize));
  __ lw(scratch, FieldMemOperand(callback, AccessorInfo::kNameOffset));
  __ sw(scratch, MemOperand(sp, 0 * kPointerSize));

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mov(a0, sp);                              // a0 = Handle<Name>
  __ Addu(a1, a0, Operand(1 * kPointerSize));  // a1 = v8::PCI::args_

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ sw(a1, MemOperand(sp, 1 * kPointerSize));
  __ Addu(a1, sp, Operand(1 * kPointerSize));  // a1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  __ lw(scratch, FieldMemOperand(callback, AccessorInfo::kJsGetterOffset));
  __ lw(api_function_address,
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

#endif  // V8_TARGET_ARCH_MIPS
