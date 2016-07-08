// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

#include "src/bootstrapper.h"
#include "src/code-stubs.h"
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


static void InitializeArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(a0, deopt_handler, constant_stack_parameter_count,
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
    descriptor->Initialize(a0, deopt_handler, constant_stack_parameter_count,
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
      __ sd(descriptor.GetRegisterParameter(i),
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
    // Call runtime on identical SIMD values since we must throw a TypeError.
    __ Branch(slow, eq, t0, Operand(SIMD128_VALUE_TYPE));
  } else {
    __ Branch(&heap_number, eq, t0, Operand(HEAP_NUMBER_TYPE));
    // Comparing JS objects with <=, >= is complicated.
    if (cc != eq) {
      __ Branch(slow, greater, t0, Operand(FIRST_JS_RECEIVER_TYPE));
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ Branch(slow, eq, t0, Operand(SYMBOL_TYPE));
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ Branch(slow, eq, t0, Operand(SIMD128_VALUE_TYPE));
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
    __ lwu(a6, FieldMemOperand(a0, HeapNumber::kExponentOffset));
    // Test that exponent bits are all set.
    __ And(a7, a6, Operand(exp_mask_reg));
    // If all bits not set (ne cond), then not a NaN, objects are equal.
    __ Branch(&return_equal, ne, a7, Operand(exp_mask_reg));

    // Shift out flag and all exponent bits, retaining only mantissa.
    __ sll(a6, a6, HeapNumber::kNonMantissaBitsInTopWord);
    // Or with all low-bits of mantissa.
    __ lwu(a7, FieldMemOperand(a0, HeapNumber::kMantissaOffset));
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
  __ ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));

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
  __ ld(a2, FieldMemOperand(rhs, HeapObject::kMapOffset));
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
  Label object_test, return_unequal, undetectable;
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
  __ ld(a2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ ld(a3, FieldMemOperand(rhs, HeapObject::kMapOffset));
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
  DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
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
  const Register base = a1;
  const Register exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent.is(a2));
  const Register heapnumbermap = a5;
  const Register heapnumber = v0;
  const DoubleRegister double_base = f2;
  const DoubleRegister double_exponent = f4;
  const DoubleRegister double_result = f0;
  const DoubleRegister double_scratch = f6;
  const FPURegister single_scratch = f8;
  const Register scratch = t1;
  const Register scratch2 = a7;

  Label call_runtime, done, int_exponent;
  if (exponent_type() == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack to double registers.
    __ ld(base, MemOperand(sp, 1 * kPointerSize));
    __ ld(exponent, MemOperand(sp, 0 * kPointerSize));

    __ LoadRoot(heapnumbermap, Heap::kHeapNumberMapRootIndex);

    __ UntagAndJumpIfSmi(scratch, base, &base_is_smi);
    __ ld(scratch, FieldMemOperand(base, JSObject::kMapOffset));
    __ Branch(&call_runtime, ne, scratch, Operand(heapnumbermap));

    __ ldc1(double_base, FieldMemOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent);

    __ bind(&base_is_smi);
    __ mtc1(scratch, single_scratch);
    __ cvt_d_w(double_base, single_scratch);
    __ bind(&unpack_exponent);

    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ ld(scratch, FieldMemOperand(exponent, JSObject::kMapOffset));
    __ Branch(&call_runtime, ne, scratch, Operand(heapnumbermap));
    __ ldc1(double_exponent,
            FieldMemOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type() == TAGGED) {
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

    if (exponent_type() == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label not_plus_half;

      // Test for 0.5.
      __ Move(double_scratch, 0.5);
      __ BranchF(USE_DELAY_SLOT,
                 &not_plus_half,
                 NULL,
                 ne,
                 double_exponent,
                 double_scratch);
      // double_scratch can be overwritten in the delay slot.
      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      __ Move(double_scratch, static_cast<double>(-V8_INFINITY));
      __ BranchF(USE_DELAY_SLOT, &done, NULL, eq, double_base, double_scratch);
      __ neg_d(double_result, double_scratch);

      // Add +0 to convert -0 to +0.
      __ add_d(double_scratch, double_base, kDoubleRegZero);
      __ sqrt_d(double_result, double_scratch);
      __ jmp(&done);

      __ bind(&not_plus_half);
      __ Move(double_scratch, -0.5);
      __ BranchF(USE_DELAY_SLOT,
                 &call_runtime,
                 NULL,
                 ne,
                 double_exponent,
                 double_scratch);
      // double_scratch can be overwritten in the delay slot.
      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      __ Move(double_scratch, static_cast<double>(-V8_INFINITY));
      __ BranchF(USE_DELAY_SLOT, &done, NULL, eq, double_base, double_scratch);
      __ Move(double_result, kDoubleRegZero);

      // Add +0 to convert -0 to +0.
      __ add_d(double_scratch, double_base, kDoubleRegZero);
      __ Move(double_result, 1.);
      __ sqrt_d(double_scratch, double_scratch);
      __ div_d(double_result, double_result, double_scratch);
      __ jmp(&done);
    }

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
  Label positive_exponent;
  __ Branch(&positive_exponent, ge, scratch, Operand(zero_reg));
  __ Dsubu(scratch, zero_reg, scratch);
  __ bind(&positive_exponent);

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
  __ mtc1(exponent, single_scratch);
  __ cvt_d_w(double_exponent, single_scratch);

  // Returning or bailing out.
  if (exponent_type() == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMathPowRT);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in exponent.
    __ bind(&done);
    __ AllocateHeapNumber(
        heapnumber, scratch, scratch2, heapnumbermap, &call_runtime);
    __ sdc1(double_result,
            FieldMemOperand(heapnumber, HeapNumber::kValueOffset));
    DCHECK(heapnumber.is(v0));
    __ DropAndRet(2);
  } else {
    __ push(ra);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()),
          0, 2);
    }
    __ pop(ra);
    __ MovFromFloatResult(double_result);

    __ bind(&done);
    __ Ret();
  }
}


bool CEntryStub::NeedsImmovableCode() {
  return true;
}


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
  __ EnterExitFrame(save_doubles());

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
    __ sd(ra, MemOperand(sp, result_stack_size));
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
    __ ld(a0, MemOperand(v0, 2 * kPointerSize));
    __ ld(v1, MemOperand(v0, 1 * kPointerSize));
    __ ld(v0, MemOperand(v0, 0 * kPointerSize));
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
    __ ld(a2, MemOperand(a2));
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
  __ ld(cp, MemOperand(cp));
  __ li(sp, Operand(pending_handler_sp_address));
  __ ld(sp, MemOperand(sp));
  __ li(fp, Operand(pending_handler_fp_address));
  __ ld(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label zero;
  __ Branch(&zero, eq, cp, Operand(zero_reg));
  __ sd(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&zero);

  // Compute the handler entry address and jump to it.
  __ li(a1, Operand(pending_handler_code_address));
  __ ld(a1, MemOperand(a1));
  __ li(a2, Operand(pending_handler_offset_address));
  __ ld(a2, MemOperand(a2));
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
  int marker = type();
  __ li(a6, Operand(Smi::FromInt(marker)));
  __ li(a5, Operand(Smi::FromInt(marker)));
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate);
  __ li(a4, Operand(c_entry_fp));
  __ ld(a4, MemOperand(a4));
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
  __ ld(a6, MemOperand(a5));
  __ Branch(&non_outermost_js, ne, a6, Operand(zero_reg));
  __ sd(fp, MemOperand(a5));
  __ li(a4, Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  Label cont;
  __ b(&cont);
  __ nop();   // Branch delay slot nop.
  __ bind(&non_outermost_js);
  __ li(a4, Operand(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));
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
  __ sd(v0, MemOperand(a4));  // We come back from 'invoke'. result is in v0.
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

  // Clear any pending exceptions.
  __ LoadRoot(a5, Heap::kTheHoleValueRootIndex);
  __ li(a4, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      isolate)));
  __ sd(a5, MemOperand(a4));

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
  __ ld(t9, MemOperand(a4));  // Deref address.
  // Call JSEntryTrampoline.
  __ daddiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  __ Call(t9);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);  // v0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(a5);
  __ Branch(&non_outermost_js_2,
            ne,
            a5,
            Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ li(a5, Operand(ExternalReference(js_entry_sp)));
  __ sd(zero_reg, MemOperand(a5));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(a5);
  __ li(a4, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                      isolate)));
  __ sd(a5, MemOperand(a4));

  // Reset the stack to the callee saved registers.
  __ daddiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

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
  Register scratch = a5;
  Register result = v0;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  DCHECK(!scratch.is(LoadWithVectorDescriptor::VectorRegister()));

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


void InstanceOfStub::Generate(MacroAssembler* masm) {
  Register const object = a1;              // Object (lhs).
  Register const function = a0;            // Function (rhs).
  Register const object_map = a2;          // Map of {object}.
  Register const function_map = a3;        // Map of {function}.
  Register const function_prototype = a4;  // Prototype of {function}.
  Register const scratch = a5;

  DCHECK(object.is(InstanceOfDescriptor::LeftRegister()));
  DCHECK(function.is(InstanceOfDescriptor::RightRegister()));

  // Check if {object} is a smi.
  Label object_is_smi;
  __ JumpIfSmi(object, &object_is_smi);

  // Lookup the {function} and the {object} map in the global instanceof cache.
  // Note: This is safe because we clear the global instanceof cache whenever
  // we change the prototype of any object.
  Label fast_case, slow_case;
  __ ld(object_map, FieldMemOperand(object, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kInstanceofCacheFunctionRootIndex);
  __ Branch(&fast_case, ne, function, Operand(at));
  __ LoadRoot(at, Heap::kInstanceofCacheMapRootIndex);
  __ Branch(&fast_case, ne, object_map, Operand(at));
  __ Ret(USE_DELAY_SLOT);
  __ LoadRoot(v0, Heap::kInstanceofCacheAnswerRootIndex);  // In delay slot.

  // If {object} is a smi we can safely return false if {function} is a JS
  // function, otherwise we have to miss to the runtime and throw an exception.
  __ bind(&object_is_smi);
  __ JumpIfSmi(function, &slow_case);
  __ GetObjectType(function, function_map, scratch);
  __ Branch(&slow_case, ne, scratch, Operand(JS_FUNCTION_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ LoadRoot(v0, Heap::kFalseValueRootIndex);  // In delay slot.

  // Fast-case: The {function} must be a valid JSFunction.
  __ bind(&fast_case);
  __ JumpIfSmi(function, &slow_case);
  __ GetObjectType(function, function_map, scratch);
  __ Branch(&slow_case, ne, scratch, Operand(JS_FUNCTION_TYPE));

  // Go to the runtime if the function is not a constructor.
  __ lbu(scratch, FieldMemOperand(function_map, Map::kBitFieldOffset));
  __ And(at, scratch, Operand(1 << Map::kIsConstructor));
  __ Branch(&slow_case, eq, at, Operand(zero_reg));

  // Ensure that {function} has an instance prototype.
  __ And(at, scratch, Operand(1 << Map::kHasNonInstancePrototype));
  __ Branch(&slow_case, ne, at, Operand(zero_reg));

  // Get the "prototype" (or initial map) of the {function}.
  __ ld(function_prototype,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  __ AssertNotSmi(function_prototype);

  // Resolve the prototype if the {function} has an initial map.  Afterwards the
  // {function_prototype} will be either the JSReceiver prototype object or the
  // hole value, which means that no instances of the {function} were created so
  // far and hence we should return false.
  Label function_prototype_valid;
  __ GetObjectType(function_prototype, scratch, scratch);
  __ Branch(&function_prototype_valid, ne, scratch, Operand(MAP_TYPE));
  __ ld(function_prototype,
        FieldMemOperand(function_prototype, Map::kPrototypeOffset));
  __ bind(&function_prototype_valid);
  __ AssertNotSmi(function_prototype);

  // Update the global instanceof cache with the current {object} map and
  // {function}.  The cached answer will be set when it is known below.
  __ StoreRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
  __ StoreRoot(object_map, Heap::kInstanceofCacheMapRootIndex);

  // Loop through the prototype chain looking for the {function} prototype.
  // Assume true, and change to false if not found.
  Register const object_instance_type = function_map;
  Register const map_bit_field = function_map;
  Register const null = scratch;
  Register const result = v0;

  Label done, loop, fast_runtime_fallback;
  __ LoadRoot(result, Heap::kTrueValueRootIndex);
  __ LoadRoot(null, Heap::kNullValueRootIndex);
  __ bind(&loop);

  // Check if the object needs to be access checked.
  __ lbu(map_bit_field, FieldMemOperand(object_map, Map::kBitFieldOffset));
  __ And(map_bit_field, map_bit_field, Operand(1 << Map::kIsAccessCheckNeeded));
  __ Branch(&fast_runtime_fallback, ne, map_bit_field, Operand(zero_reg));
  // Check if the current object is a Proxy.
  __ lbu(object_instance_type,
         FieldMemOperand(object_map, Map::kInstanceTypeOffset));
  __ Branch(&fast_runtime_fallback, eq, object_instance_type,
            Operand(JS_PROXY_TYPE));

  __ ld(object, FieldMemOperand(object_map, Map::kPrototypeOffset));
  __ Branch(&done, eq, object, Operand(function_prototype));
  __ Branch(USE_DELAY_SLOT, &loop, ne, object, Operand(null));
  __ ld(object_map,
        FieldMemOperand(object, HeapObject::kMapOffset));  // In delay slot.
  __ LoadRoot(result, Heap::kFalseValueRootIndex);
  __ bind(&done);
  __ Ret(USE_DELAY_SLOT);
  __ StoreRoot(result,
               Heap::kInstanceofCacheAnswerRootIndex);  // In delay slot.

  // Found Proxy or access check needed: Call the runtime
  __ bind(&fast_runtime_fallback);
  __ Push(object, function_prototype);
  // Invalidate the instanceof cache.
  DCHECK(Smi::FromInt(0) == 0);
  __ StoreRoot(zero_reg, Heap::kInstanceofCacheFunctionRootIndex);
  __ TailCallRuntime(Runtime::kHasInPrototypeChain);

  // Slow-case: Call the %InstanceOf runtime function.
  __ bind(&slow_case);
  __ Push(object, function);
  __ TailCallRuntime(Runtime::kInstanceOf);
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // Ensure that the vector and slot registers won't be clobbered before
  // calling the miss handler.
  DCHECK(!AreAliased(a4, a5, LoadWithVectorDescriptor::VectorRegister(),
                     LoadWithVectorDescriptor::SlotRegister()));

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, a4,
                                                          a5, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void LoadIndexedInterceptorStub::Generate(MacroAssembler* masm) {
  // Return address is in ra.
  Label slow;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();

  // Check that the key is an array index, that is Uint32.
  __ And(t0, key, Operand(kSmiTagMask | kSmiSignMask));
  __ Branch(&slow, ne, t0, Operand(zero_reg));

  // Everything is fine, call runtime.
  __ Push(receiver, key);  // Receiver, key.

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kLoadElementWithInterceptor);

  __ bind(&slow);
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
  // MIPS - using s0..s2, since we are not using CEntry Stub.
  Register subject = s0;
  Register regexp_data = s1;
  Register last_match_info_elements = s2;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(
          isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ li(a0, Operand(address_of_regexp_stack_memory_size));
  __ ld(a0, MemOperand(a0, 0));
  __ Branch(&runtime, eq, a0, Operand(zero_reg));

  // Check that the first argument is a JSRegExp object.
  __ ld(a0, MemOperand(sp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(a0, &runtime);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&runtime, ne, a1, Operand(JS_REGEXP_TYPE));

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ ld(regexp_data, FieldMemOperand(a0, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ SmiTst(regexp_data, a4);
    __ Check(nz,
             kUnexpectedTypeForRegExpDataFixedArrayExpected,
             a4,
             Operand(zero_reg));
    __ GetObjectType(regexp_data, a0, a0);
    __ Check(eq,
             kUnexpectedTypeForRegExpDataFixedArrayExpected,
             a0,
             Operand(FIXED_ARRAY_TYPE));
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ ld(a0, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ Branch(&runtime, ne, a0, Operand(Smi::FromInt(JSRegExp::IRREGEXP)));

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ ld(a2,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // Or          number_of_captures     <= offsets vector size / 2 - 1
  // Multiplying by 2 comes for free since a2 is smi-tagged.
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  int temp = Isolate::kJSRegexpStaticOffsetsVectorSize / 2 - 1;
  __ Branch(&runtime, hi, a2, Operand(Smi::FromInt(temp)));

  // Reset offset for possibly sliced string.
  __ mov(t0, zero_reg);
  __ ld(subject, MemOperand(sp, kSubjectOffset));
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

  Label check_underlying;   // (1)
  Label seq_string;         // (4)
  Label not_seq_nor_cons;   // (5)
  Label external_string;    // (6)
  Label not_long_external;  // (7)

  __ bind(&check_underlying);
  __ ld(a2, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbu(a0, FieldMemOperand(a2, Map::kInstanceTypeOffset));

  // (1) Sequential string?  If yes, go to (4).
  __ And(a1,
         a0,
         Operand(kIsNotStringMask |
                 kStringRepresentationMask |
                 kShortExternalStringMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ Branch(&seq_string, eq, a1, Operand(zero_reg));  // Go to (4).

  // (2) Sequential or cons?  If not, go to (5).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  // Go to (5).
  __ Branch(&not_seq_nor_cons, ge, a1, Operand(kExternalStringTag));

  // (3) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ ld(a0, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ LoadRoot(a1, Heap::kempty_stringRootIndex);
  __ Branch(&runtime, ne, a0, Operand(a1));
  __ ld(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  __ jmp(&check_underlying);

  // (4) Sequential string.  Load regexp code according to encoding.
  __ bind(&seq_string);
  // subject: sequential subject string (or look-alike, external string)
  // a3: original subject string
  // Load previous index and check range before a3 is overwritten.  We have to
  // use a3 instead of subject here because subject might have been only made
  // to look like a sequential string when it actually is an external string.
  __ ld(a1, MemOperand(sp, kPreviousIndexOffset));
  __ JumpIfNotSmi(a1, &runtime);
  __ ld(a3, FieldMemOperand(a3, String::kLengthOffset));
  __ Branch(&runtime, ls, a3, Operand(a1));
  __ SmiUntag(a1);

  STATIC_ASSERT(kStringEncodingMask == 4);
  STATIC_ASSERT(kOneByteStringTag == 4);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ And(a0, a0, Operand(kStringEncodingMask));  // Non-zero for one_byte.
  __ ld(t9, FieldMemOperand(regexp_data, JSRegExp::kDataOneByteCodeOffset));
  __ dsra(a3, a0, 2);  // a3 is 1 for one_byte, 0 for UC16 (used below).
  __ ld(a5, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset));
  __ Movz(t9, a5, a0);  // If UC16 (a0 is 0), replace t9 w/kDataUC16CodeOffset.

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
  const int kParameterRegisters = 8;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers, meaning we
  // treat the return address as argument 5. Thus every argument after that
  // needs to be shifted back by 1. Since DirectCEntryStub will handle
  // allocating space for the c argument slots, we don't need to calculate
  // that into the argument positions on the stack. This is how the stack will
  // look (sp meaning the value of sp at this moment):
  // Abi n64:
  //   [sp + 1] - Argument 9
  //   [sp + 0] - saved ra
  // Abi O32:
  //   [sp + 5] - Argument 9
  //   [sp + 4] - Argument 8
  //   [sp + 3] - Argument 7
  //   [sp + 2] - Argument 6
  //   [sp + 1] - Argument 5
  //   [sp + 0] - saved ra

  // Argument 9: Pass current isolate address.
  __ li(a0, Operand(ExternalReference::isolate_address(isolate())));
  __ sd(a0, MemOperand(sp, 1 * kPointerSize));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ li(a7, Operand(1));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ li(a0, Operand(address_of_regexp_stack_memory_address));
  __ ld(a0, MemOperand(a0, 0));
  __ li(a2, Operand(address_of_regexp_stack_memory_size));
  __ ld(a2, MemOperand(a2, 0));
  __ daddu(a6, a0, a2);

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global. This does not affect non-global regexps.
  __ mov(a5, zero_reg);

  // Argument 5: static offsets vector buffer.
  __ li(
      a4,
      Operand(ExternalReference::address_of_static_offsets_vector(isolate())));

  // For arguments 4 and 3 get string length, calculate start of string data
  // and calculate the shift of the index (0 for one_byte and 1 for two byte).
  __ Daddu(t2, subject, Operand(SeqString::kHeaderSize - kHeapObjectTag));
  __ Xor(a3, a3, Operand(1));  // 1 for 2-byte str, 0 for 1-byte.
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to two pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize.)
  __ ld(subject, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));
  // If slice offset is not 0, load the length from the original sliced string.
  // Argument 4, a3: End of string data
  // Argument 3, a2: Start of string data
  // Prepare start and end index of the input.
  __ dsllv(t1, t0, a3);
  __ daddu(t0, t2, t1);
  __ dsllv(t1, a1, a3);
  __ daddu(a2, t0, t1);

  __ ld(t2, FieldMemOperand(subject, String::kLengthOffset));

  __ SmiUntag(t2);
  __ dsllv(t1, t2, a3);
  __ daddu(a3, t0, t1);
  // Argument 2 (a1): Previous index.
  // Already there

  // Argument 1 (a0): Subject string.
  __ mov(a0, subject);

  // Locate the code entry and call it.
  __ Daddu(t9, t9, Operand(Code::kHeaderSize - kHeapObjectTag));
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
  __ ld(v0, MemOperand(a2, 0));
  __ Branch(&runtime, eq, v0, Operand(a1));

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  __ bind(&failure);
  // For failure and exception return null.
  __ li(v0, Operand(isolate()->factory()->null_value()));
  __ DropAndRet(4);

  // Process the result from the native regexp code.
  __ bind(&success);

  __ lw(a1, UntagSmiFieldMemOperand(
      regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ Daddu(a1, a1, Operand(1));
  __ dsll(a1, a1, 1);  // Multiply by 2.

  __ ld(a0, MemOperand(sp, kLastMatchInfoOffset));
  __ JumpIfSmi(a0, &runtime);
  __ GetObjectType(a0, a2, a2);
  __ Branch(&runtime, ne, a2, Operand(JS_ARRAY_TYPE));
  // Check that the JSArray is in fast case.
  __ ld(last_match_info_elements,
        FieldMemOperand(a0, JSArray::kElementsOffset));
  __ ld(a0, FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&runtime, ne, a0, Operand(at));
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ ld(a0,
        FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ Daddu(a2, a1, Operand(RegExpImpl::kLastMatchOverhead));

  __ SmiUntag(at, a0);
  __ Branch(&runtime, gt, a2, Operand(at));

  // a1: number of capture registers
  // subject: subject string
  // Store the capture count.
  __ SmiTag(a2, a1);  // To smi.
  __ sd(a2, FieldMemOperand(last_match_info_elements,
                             RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ sd(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  __ mov(a2, subject);
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastSubjectOffset,
                      subject,
                      a7,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs);
  __ mov(subject, a2);
  __ sd(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastInputOffset,
                      subject,
                      a7,
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
  __ Daddu(a0,
         last_match_info_elements,
         Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag));
  __ bind(&next_capture);
  __ Dsubu(a1, a1, Operand(1));
  __ Branch(&done, lt, a1, Operand(zero_reg));
  // Read the value from the static offsets vector buffer.
  __ lw(a3, MemOperand(a2, 0));
  __ daddiu(a2, a2, kIntSize);
  // Store the smi value in the last match info.
  __ SmiTag(a3);
  __ sd(a3, MemOperand(a0, 0));
  __ Branch(&next_capture, USE_DELAY_SLOT);
  __ daddiu(a0, a0, kPointerSize);  // In branch delay slot.

  __ bind(&done);

  // Return last match info.
  __ ld(v0, MemOperand(sp, kLastMatchInfoOffset));
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
  __ ld(a0, FieldMemOperand(subject, HeapObject::kMapOffset));
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
  __ ld(subject,
        FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Dsubu(subject,
          subject,
          SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ jmp(&seq_string);  // Go to (4).

  // (7) Short external string or not a string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ And(at, a1, Operand(kIsNotStringMask | kShortExternalStringMask));
  __ Branch(&runtime, ne, at, Operand(zero_reg));

  // (8) Sliced string.  Replace subject with parent.  Go to (4).
  // Load offset into t0 and replace subject string with parent.
  __ ld(t0, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ SmiUntag(t0);
  __ ld(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ jmp(&check_underlying);  // Go to (1).
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
                             1 << 7;   // a3


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

  // Load the cache state into a5.
  __ dsrl(a5, a3, 32 - kPointerSizeLog2);
  __ Daddu(a5, a2, Operand(a5));
  __ ld(a5, FieldMemOperand(a5, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if a5 is a WeakCell or a Symbol, but it's harmless to read at
  // this position in a symbol (see static asserts in type-feedback-vector.h).
  Label check_allocation_site;
  Register feedback_map = a6;
  Register weak_value = t0;
  __ ld(weak_value, FieldMemOperand(a5, WeakCell::kValueOffset));
  __ Branch(&done, eq, a1, Operand(weak_value));
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&done, eq, a5, Operand(at));
  __ ld(feedback_map, FieldMemOperand(a5, HeapObject::kMapOffset));
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
  __ sd(at, FieldMemOperand(a5, FixedArray::kHeaderSize));
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
  __ ld(a2, FieldMemOperand(a5, FixedArray::kHeaderSize));
  __ ld(a5, FieldMemOperand(a2, AllocationSite::kMapOffset));
  __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
  __ Branch(&feedback_register_initialized, eq, a5, Operand(at));
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(a2, a5);

  // Pass function as new target.
  __ mov(a3, a1);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kConstructStubOffset));
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
    __ ld(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
    __ lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ And(a4, result_, Operand(kIsNotStringMask));
    __ Branch(receiver_not_string_, ne, a4, Operand(zero_reg));
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);

  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ ld(a4, FieldMemOperand(object_, String::kLengthOffset));
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


void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // a1 - function
  // a3 - slot id
  // a2 - vector
  // a4 - allocation site (loaded from vector[slot])
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, at);
  __ Branch(miss, ne, a1, Operand(at));

  __ li(a0, Operand(arg_count()));

  // Increment the call count for monomorphic function calls.
  __ dsrl(t0, a3, 32 - kPointerSizeLog2);
  __ Daddu(a3, a2, Operand(t0));
  __ ld(t0, FieldMemOperand(a3, FixedArray::kHeaderSize + kPointerSize));
  __ Daddu(t0, t0, Operand(Smi::FromInt(CallICNexus::kCallCountIncrement)));
  __ sd(t0, FieldMemOperand(a3, FixedArray::kHeaderSize + kPointerSize));

  __ mov(a2, a4);
  __ mov(a3, a1);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);
}


void CallICStub::Generate(MacroAssembler* masm) {
  // a1 - function
  // a3 - slot id (Smi)
  // a2 - vector
  Label extra_checks_or_miss, call, call_function;
  int argc = arg_count();
  ParameterCount actual(argc);

  // The checks. First, does r1 match the recorded monomorphic target?
  __ dsrl(a4, a3, 32 - kPointerSizeLog2);
  __ Daddu(a4, a2, Operand(a4));
  __ ld(a4, FieldMemOperand(a4, FixedArray::kHeaderSize));

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

  __ ld(a5, FieldMemOperand(a4, WeakCell::kValueOffset));
  __ Branch(&extra_checks_or_miss, ne, a1, Operand(a5));

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(a1, &extra_checks_or_miss);

  // Increment the call count for monomorphic function calls.
  __ dsrl(t0, a3, 32 - kPointerSizeLog2);
  __ Daddu(a3, a2, Operand(t0));
  __ ld(t0, FieldMemOperand(a3, FixedArray::kHeaderSize + kPointerSize));
  __ Daddu(t0, t0, Operand(Smi::FromInt(CallICNexus::kCallCountIncrement)));
  __ sd(t0, FieldMemOperand(a3, FixedArray::kHeaderSize + kPointerSize));

  __ bind(&call_function);
  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET, al, zero_reg, Operand(zero_reg),
          USE_DELAY_SLOT);
  __ li(a0, Operand(argc));  // In delay slot.

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&call, eq, a4, Operand(at));

  // Verify that a4 contains an AllocationSite
  __ ld(a5, FieldMemOperand(a4, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
  __ Branch(&not_allocation_site, ne, a5, Operand(at));

  HandleArrayCase(masm, &miss);

  __ bind(&not_allocation_site);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ Branch(&miss);
  }

  __ LoadRoot(at, Heap::kuninitialized_symbolRootIndex);
  __ Branch(&uninitialized, eq, a4, Operand(at));

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(a4);
  __ GetObjectType(a4, a5, a5);
  __ Branch(&miss, ne, a5, Operand(JS_FUNCTION_TYPE));
  __ dsrl(a4, a3, 32 - kPointerSizeLog2);
  __ Daddu(a4, a2, Operand(a4));
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ sd(at, FieldMemOperand(a4, FixedArray::kHeaderSize));

  __ bind(&call);
  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET, al, zero_reg, Operand(zero_reg),
          USE_DELAY_SLOT);
  __ li(a0, Operand(argc));  // In delay slot.

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(a1, &miss);

  // Goto miss case if we do not have a function.
  __ GetObjectType(a1, a4, a4);
  __ Branch(&miss, ne, a4, Operand(JS_FUNCTION_TYPE));

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, a4);
  __ Branch(&miss, eq, a1, Operand(a4));

  // Make sure the function belongs to the same native context.
  __ ld(t0, FieldMemOperand(a1, JSFunction::kContextOffset));
  __ ld(t0, ContextMemOperand(t0, Context::NATIVE_CONTEXT_INDEX));
  __ ld(t1, NativeContextMemOperand());
  __ Branch(&miss, ne, t0, Operand(t1));

  // Initialize the call counter.
  __ dsrl(at, a3, 32 - kPointerSizeLog2);
  __ Daddu(at, a2, Operand(at));
  __ li(t0, Operand(Smi::FromInt(CallICNexus::kCallCountIncrement)));
  __ sd(t0, FieldMemOperand(at, FixedArray::kHeaderSize + kPointerSize));

  // Store the function. Use a stub since we need a frame for allocation.
  // a2 - vector
  // a3 - slot
  // a1 - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(masm->isolate());
    __ Push(a1);
    __ CallStub(&create_stub);
    __ Pop(a1);
  }

  __ Branch(&call_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  __ Branch(&call);
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
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero);
  } else {
    DCHECK(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi);
  }

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
  __ ld(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
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
  __ SmiTag(index_);
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
  __ JumpIfNotSmi(code_, &slow_case_);
  __ Branch(&slow_case_, hi, code_,
            Operand(Smi::FromInt(String::kMaxOneByteCharCode)));

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged one_byte char code.
  __ SmiScale(at, code_, kPointerSizeLog2);
  __ Daddu(result_, result_, at);
  __ ld(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&slow_case_, eq, result_, Operand(at));
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
    __ Daddu(count, count, count);
  }

  Register limit = count;  // Read until dest equals this.
  __ Daddu(limit, dest, Operand(count));

  Label loop_entry, loop;
  // Copy bytes from src to dest until dest hits limit.
  __ Branch(&loop_entry);
  __ bind(&loop);
  __ lbu(scratch, MemOperand(src));
  __ daddiu(src, src, 1);
  __ sb(scratch, MemOperand(dest));
  __ daddiu(dest, dest, 1);
  __ bind(&loop_entry);
  __ Branch(&loop, lt, dest, Operand(limit));

  __ bind(&done);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;
  // Stack frame on entry.
  //  ra: return address
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

  __ ld(a2, MemOperand(sp, kToOffset));
  __ ld(a3, MemOperand(sp, kFromOffset));

  STATIC_ASSERT(kSmiTag == 0);

  // Utilize delay slots. SmiUntag doesn't emit a jump, everything else is
  // safe in this case.
  __ JumpIfNotSmi(a2, &runtime);
  __ JumpIfNotSmi(a3, &runtime);
  // Both a2 and a3 are untagged integers.

  __ SmiUntag(a2, a2);
  __ SmiUntag(a3, a3);
  __ Branch(&runtime, lt, a3, Operand(zero_reg));  // From < 0.

  __ Branch(&runtime, gt, a3, Operand(a2));  // Fail if from > to.
  __ Dsubu(a2, a2, a3);

  // Make sure first argument is a string.
  __ ld(v0, MemOperand(sp, kStringOffset));
  __ JumpIfSmi(v0, &runtime);
  __ ld(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kInstanceTypeOffset));
  __ And(a4, a1, Operand(kIsNotStringMask));

  __ Branch(&runtime, ne, a4, Operand(zero_reg));

  Label single_char;
  __ Branch(&single_char, eq, a2, Operand(1));

  // Short-cut for the case of trivial substring.
  Label return_v0;
  // v0: original string
  // a2: result string length
  __ ld(a4, FieldMemOperand(v0, String::kLengthOffset));
  __ SmiUntag(a4);
  // Return original string.
  __ Branch(&return_v0, eq, a2, Operand(a4));
  // Longer than original string's length or negative: unsafe arguments.
  __ Branch(&runtime, hi, a2, Operand(a4));
  // Shorter than original string's length: an actual substring.

  // Deal with different string types: update the index if necessary
  // and put the underlying string into a5.
  // v0: original string
  // a1: instance type
  // a2: length
  // a3: from index (untagged)
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ And(a4, a1, Operand(kIsIndirectStringMask));
  __ Branch(USE_DELAY_SLOT, &seq_or_external_string, eq, a4, Operand(zero_reg));
  // a4 is used as a scratch register and can be overwritten in either case.
  __ And(a4, a1, Operand(kSlicedNotConsMask));
  __ Branch(&sliced_string, ne, a4, Operand(zero_reg));
  // Cons string.  Check whether it is flat, then fetch first part.
  __ ld(a5, FieldMemOperand(v0, ConsString::kSecondOffset));
  __ LoadRoot(a4, Heap::kempty_stringRootIndex);
  __ Branch(&runtime, ne, a5, Operand(a4));
  __ ld(a5, FieldMemOperand(v0, ConsString::kFirstOffset));
  // Update instance type.
  __ ld(a1, FieldMemOperand(a5, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and correct start index by offset.
  __ ld(a5, FieldMemOperand(v0, SlicedString::kParentOffset));
  __ ld(a4, FieldMemOperand(v0, SlicedString::kOffsetOffset));
  __ SmiUntag(a4);  // Add offset to index.
  __ Daddu(a3, a3, a4);
  // Update instance type.
  __ ld(a1, FieldMemOperand(a5, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the expected register.
  __ mov(a5, v0);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // a5: underlying subject string
    // a1: instance type of underlying subject string
    // a2: length
    // a3: adjusted start index (untagged)
    // Short slice.  Copy instead of slicing.
    __ Branch(&copy_routine, lt, a2, Operand(SlicedString::kMinLength));
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ And(a4, a1, Operand(kStringEncodingMask));
    __ Branch(&two_byte_slice, eq, a4, Operand(zero_reg));
    __ AllocateOneByteSlicedString(v0, a2, a6, a7, &runtime);
    __ jmp(&set_slice_header);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(v0, a2, a6, a7, &runtime);
    __ bind(&set_slice_header);
    __ SmiTag(a3);
    __ sd(a5, FieldMemOperand(v0, SlicedString::kParentOffset));
    __ sd(a3, FieldMemOperand(v0, SlicedString::kOffsetOffset));
    __ jmp(&return_v0);

    __ bind(&copy_routine);
  }

  // a5: underlying subject string
  // a1: instance type of underlying subject string
  // a2: length
  // a3: adjusted start index (untagged)
  Label two_byte_sequential, sequential_string, allocate_result;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ And(a4, a1, Operand(kExternalStringTag));
  __ Branch(&sequential_string, eq, a4, Operand(zero_reg));

  // Handle external string.
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ And(a4, a1, Operand(kShortExternalStringTag));
  __ Branch(&runtime, ne, a4, Operand(zero_reg));
  __ ld(a5, FieldMemOperand(a5, ExternalString::kResourceDataOffset));
  // a5 already points to the first character of underlying string.
  __ jmp(&allocate_result);

  __ bind(&sequential_string);
  // Locate first character of underlying subject string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Daddu(a5, a5, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&allocate_result);
  // Sequential acii string.  Allocate the result.
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ And(a4, a1, Operand(kStringEncodingMask));
  __ Branch(&two_byte_sequential, eq, a4, Operand(zero_reg));

  // Allocate and copy the resulting one_byte string.
  __ AllocateOneByteString(v0, a2, a4, a6, a7, &runtime);

  // Locate first character of substring to copy.
  __ Daddu(a5, a5, a3);

  // Locate first character of result.
  __ Daddu(a1, v0, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  // v0: result string
  // a1: first character of result string
  // a2: result string length
  // a5: first character of substring to copy
  STATIC_ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharacters(
      masm, a1, a5, a2, a3, String::ONE_BYTE_ENCODING);
  __ jmp(&return_v0);

  // Allocate and copy the resulting two-byte string.
  __ bind(&two_byte_sequential);
  __ AllocateTwoByteString(v0, a2, a4, a6, a7, &runtime);

  // Locate first character of substring to copy.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ Dlsa(a5, a5, a3, 1);
  // Locate first character of result.
  __ Daddu(a1, v0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // v0: result string.
  // a1: first character of result.
  // a2: result length.
  // a5: first character of substring to copy.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharacters(
      masm, a1, a5, a2, a3, String::TWO_BYTE_ENCODING);

  __ bind(&return_v0);
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1, a3, a4);
  __ DropAndRet(3);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString);

  __ bind(&single_char);
  // v0: original string
  // a1: instance type
  // a2: length
  // a3: from index (untagged)
  __ SmiTag(a3);
  StringCharAtGenerator generator(v0, a3, a2, v0, &runtime, &runtime, &runtime,
                                  STRING_INDEX_IS_NUMBER, RECEIVER_IS_STRING);
  generator.GenerateFast(masm);
  __ DropAndRet(3);
  generator.SkipSlow(masm, &runtime);
}


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in a0.
  Label not_smi;
  __ JumpIfNotSmi(a0, &not_smi);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
  __ bind(&not_smi);

  Label not_heap_number;
  __ ld(a1, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kInstanceTypeOffset));
  // a0: object
  // a1: instance type.
  __ Branch(&not_heap_number, ne, a1, Operand(HEAP_NUMBER_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
  __ bind(&not_heap_number);

  Label not_string, slow_string;
  __ Branch(&not_string, hs, a1, Operand(FIRST_NONSTRING_TYPE));
  // Check if string has a cached array index.
  __ lwu(a2, FieldMemOperand(a0, String::kHashFieldOffset));
  __ And(at, a2, Operand(String::kContainsCachedArrayIndexMask));
  __ Branch(&slow_string, ne, at, Operand(zero_reg));
  __ IndexFromHash(a2, a0);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
  __ bind(&slow_string);
  __ push(a0);  // Push argument.
  __ TailCallRuntime(Runtime::kStringToNumber);
  __ bind(&not_string);

  Label not_oddball;
  __ Branch(&not_oddball, ne, a1, Operand(ODDBALL_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ ld(v0, FieldMemOperand(a0, Oddball::kToNumberOffset));
  __ bind(&not_oddball);

  __ push(a0);  // Push argument.
  __ TailCallRuntime(Runtime::kToNumber);
}


void ToLengthStub::Generate(MacroAssembler* masm) {
  // The ToLength stub takes on argument in a0.
  Label not_smi, positive_smi;
  __ JumpIfNotSmi(a0, &not_smi);
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&positive_smi, ge, a0, Operand(zero_reg));
  __ mov(a0, zero_reg);
  __ bind(&positive_smi);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
  __ bind(&not_smi);

  __ push(a0);  // Push argument.
  __ TailCallRuntime(Runtime::kToLength);
}


void ToStringStub::Generate(MacroAssembler* masm) {
  // The ToString stub takes on argument in a0.
  Label is_number;
  __ JumpIfSmi(a0, &is_number);

  Label not_string;
  __ GetObjectType(a0, a1, a1);
  // a0: receiver
  // a1: receiver instance type
  __ Branch(&not_string, ge, a1, Operand(FIRST_NONSTRING_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
  __ bind(&not_string);

  Label not_heap_number;
  __ Branch(&not_heap_number, ne, a1, Operand(HEAP_NUMBER_TYPE));
  __ bind(&is_number);
  NumberToStringStub stub(isolate());
  __ TailCallStub(&stub);
  __ bind(&not_heap_number);

  Label not_oddball;
  __ Branch(&not_oddball, ne, a1, Operand(ODDBALL_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ ld(v0, FieldMemOperand(a0, Oddball::kToStringOffset));
  __ bind(&not_oddball);

  __ push(a0);  // Push argument.
  __ TailCallRuntime(Runtime::kToString);
}


void ToNameStub::Generate(MacroAssembler* masm) {
  // The ToName stub takes on argument in a0.
  Label is_number;
  __ JumpIfSmi(a0, &is_number);

  Label not_name;
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ GetObjectType(a0, a1, a1);
  // a0: receiver
  // a1: receiver instance type
  __ Branch(&not_name, gt, a1, Operand(LAST_NAME_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
  __ bind(&not_name);

  Label not_heap_number;
  __ Branch(&not_heap_number, ne, a1, Operand(HEAP_NUMBER_TYPE));
  __ bind(&is_number);
  NumberToStringStub stub(isolate());
  __ TailCallStub(&stub);
  __ bind(&not_heap_number);

  Label not_oddball;
  __ Branch(&not_oddball, ne, a1, Operand(ODDBALL_TYPE));
  __ Ret(USE_DELAY_SLOT);
  __ ld(v0, FieldMemOperand(a0, Oddball::kToStringOffset));
  __ bind(&not_oddball);

  __ push(a0);  // Push argument.
  __ TailCallRuntime(Runtime::kToName);
}


void StringHelper::GenerateFlatOneByteStringEquals(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ ld(length, FieldMemOperand(left, String::kLengthOffset));
  __ ld(scratch2, FieldMemOperand(right, String::kLengthOffset));
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
  __ ld(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ ld(scratch2, FieldMemOperand(right, String::kLengthOffset));
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
  __ lbu(scratch1, MemOperand(scratch3));
  __ Daddu(scratch3, right, index);
  __ lbu(scratch2, MemOperand(scratch3));
  __ Branch(chars_not_equal, ne, scratch1, Operand(scratch2));
  __ Daddu(index, index, 1);
  __ Branch(&loop, ne, index, Operand(zero_reg));
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a1    : left
  //  -- a0    : right
  //  -- ra    : return address
  // -----------------------------------
  __ AssertString(a1);
  __ AssertString(a0);

  Label not_same;
  __ Branch(&not_same, ne, a0, Operand(a1));
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, a1,
                      a2);
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential one-byte strings.
  Label runtime;
  __ JumpIfNotBothSequentialOneByteStrings(a1, a0, a2, a3, &runtime);

  // Compare flat ASCII strings natively.
  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, a2,
                      a3);
  StringHelper::GenerateCompareFlatOneByteStrings(masm, a1, a0, a2, a3, t0, t1);

  __ bind(&runtime);
  __ Push(a1, a0);
  __ TailCallRuntime(Runtime::kStringCompare);
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
  __ li(a2, handle(isolate()->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ And(at, a2, Operand(kSmiTagMask));
    __ Assert(ne, kExpectedAllocationSite, at, Operand(zero_reg));
    __ ld(a4, FieldMemOperand(a2, HeapObject::kMapOffset));
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
    __ ld(a1, FieldMemOperand(a1, Oddball::kToNumberOffset));
    __ AssertSmi(a1);
    __ ld(a0, FieldMemOperand(a0, Oddball::kToNumberOffset));
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
  __ Dsubu(a2, a1, Operand(kHeapObjectTag));
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
  __ ld(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ld(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
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
  __ ld(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ld(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
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
  Register tmp3 = a4;
  Register tmp4 = a5;
  Register tmp5 = a6;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ ld(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ld(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
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
  __ Push(left, right);
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
  __ ld(a2, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ ld(a3, FieldMemOperand(a1, HeapObject::kMapOffset));
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
    __ sd(a4, MemOperand(sp));  // In the delay slot.
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
  __ sd(ra, MemOperand(sp, kCArgsSlotsSize));
  __ Call(t9);  // Call the C++ function.
  __ ld(t9, MemOperand(sp, kCArgsSlotsSize));

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
    __ ld(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

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
    __ ld(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
    __ lbu(entity_name,
           FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueNameInstanceType(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ ld(properties,
          FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask =
      (ra.bit() | a6.bit() | a5.bit() | a4.bit() | a3.bit() |
       a2.bit() | a1.bit() | a0.bit() | v0.bit());

  __ MultiPush(spill_mask);
  __ ld(a0, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
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
  __ ld(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ SmiUntag(scratch1);
  __ Dsubu(scratch1, scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ lwu(scratch2, FieldMemOperand(name, Name::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ Daddu(scratch2, scratch2, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ dsrl(scratch2, scratch2, Name::kHashShift);
    __ And(scratch2, scratch1, scratch2);

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.
    __ Dlsa(scratch2, scratch2, scratch2, 1);

    // Check if the key is identical to the name.
    __ Dlsa(scratch2, elements, scratch2, kPointerSizeLog2);
    __ ld(at, FieldMemOperand(scratch2, kElementsStartOffset));
    __ Branch(done, eq, name, Operand(at));
  }

  const int spill_mask =
      (ra.bit() | a6.bit() | a5.bit() | a4.bit() |
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
  Register hash = a4;
  Register undefined = a5;
  Register entry_key = a6;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ ld(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ SmiUntag(mask);
  __ Dsubu(mask, mask, Operand(1));

  __ lwu(hash, FieldMemOperand(key, Name::kHashFieldOffset));

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
    __ ld(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ Branch(&not_in_dictionary, eq, entry_key, Operand(undefined));

    // Stop if found the property.
    __ Branch(&in_dictionary, eq, entry_key, Operand(key));

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ ld(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
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

    __ ld(regs_.scratch0(), MemOperand(regs_.address(), 0));
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
  __ ld(regs_.scratch1(),
        MemOperand(regs_.scratch0(),
                   MemoryChunk::kWriteBarrierCounterOffset));
  __ Dsubu(regs_.scratch1(), regs_.scratch1(), Operand(1));
  __ sd(regs_.scratch1(),
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
  __ ld(regs_.scratch0(), MemOperand(regs_.address(), 0));

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
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ ld(a1, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ Daddu(a1, a1, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ dsll(a1, a1, kPointerSizeLog2);
  __ Ret(USE_DELAY_SLOT);
  __ Daddu(sp, sp, a1);
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

  __ ld(cached_map,
        FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(0)));
  __ ld(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ Branch(&start_polymorphic, ne, receiver_map, Operand(cached_map));
  // found, now call handler.
  Register handler = feedback;
  __ ld(handler, FieldMemOperand(feedback, FixedArray::OffsetOfElementAt(1)));
  __ Daddu(t9, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  Register length = scratch2;
  __ bind(&start_polymorphic);
  __ ld(length, FieldMemOperand(feedback, FixedArray::kLengthOffset));
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
  __ SmiScale(too_far, length, kPointerSizeLog2);
  __ Daddu(too_far, feedback, Operand(too_far));
  __ Daddu(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Daddu(pointer_reg, feedback,
           Operand(FixedArray::OffsetOfElementAt(2) - kHeapObjectTag));

  __ bind(&next_loop);
  __ ld(cached_map, MemOperand(pointer_reg));
  __ ld(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ Branch(&prepare_next, ne, receiver_map, Operand(cached_map));
  __ ld(handler, MemOperand(pointer_reg, kPointerSize));
  __ Daddu(t9, handler, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  __ bind(&prepare_next);
  __ Daddu(pointer_reg, pointer_reg, Operand(kPointerSize * 2));
  __ Branch(&next_loop, lt, pointer_reg, Operand(too_far));

  // We exhausted our array of map handler pairs.
  __ Branch(miss);
}


static void HandleMonomorphicCase(MacroAssembler* masm, Register receiver,
                                  Register receiver_map, Register feedback,
                                  Register vector, Register slot,
                                  Register scratch, Label* compare_map,
                                  Label* load_smi_map, Label* try_array) {
  __ JumpIfSmi(receiver, load_smi_map);
  __ ld(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ bind(compare_map);
  Register cached_map = scratch;
  // Move the weak map into the weak_cell register.
  __ ld(cached_map, FieldMemOperand(feedback, WeakCell::kValueOffset));
  __ Branch(try_array, ne, cached_map, Operand(receiver_map));
  Register handler = feedback;
  __ SmiScale(handler, slot, kPointerSizeLog2);
  __ Daddu(handler, vector, Operand(handler));
  __ ld(handler,
        FieldMemOperand(handler, FixedArray::kHeaderSize + kPointerSize));
  __ Daddu(t9, handler, Code::kHeaderSize - kHeapObjectTag);
  __ Jump(t9);
}


void LoadICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = LoadWithVectorDescriptor::ReceiverRegister();  // a1
  Register name = LoadWithVectorDescriptor::NameRegister();          // a2
  Register vector = LoadWithVectorDescriptor::VectorRegister();      // a3
  Register slot = LoadWithVectorDescriptor::SlotRegister();          // a0
  Register feedback = a4;
  Register receiver_map = a5;
  Register scratch1 = a6;

  __ SmiScale(feedback, slot, kPointerSizeLog2);
  __ Daddu(feedback, vector, Operand(feedback));
  __ ld(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ ld(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&not_array, ne, scratch1, Operand(at));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, a7, true, &miss);

  __ bind(&not_array);
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&miss, ne, feedback, Operand(at));
  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::LOAD_IC, code_flags,
                                               receiver, name, feedback,
                                               receiver_map, scratch1, a7);

  __ bind(&miss);
  LoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ Branch(&compare_map);
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
  Register feedback = a4;
  Register receiver_map = a5;
  Register scratch1 = a6;

  __ SmiScale(feedback, slot, kPointerSizeLog2);
  __ Daddu(feedback, vector, Operand(feedback));
  __ ld(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ ld(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ Branch(&not_array, ne, scratch1, Operand(at));
  // We have a polymorphic element handler.
  __ JumpIfNotSmi(key, &miss);

  Label polymorphic, try_poly_name;
  __ bind(&polymorphic);
  HandleArrayCases(masm, feedback, receiver_map, scratch1, a7, true, &miss);

  __ bind(&not_array);
  // Is it generic?
  __ LoadRoot(at, Heap::kmegamorphic_symbolRootIndex);
  __ Branch(&try_poly_name, ne, feedback, Operand(at));
  Handle<Code> megamorphic_stub =
      KeyedLoadIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ Branch(&miss, ne, key, Operand(feedback));
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ SmiScale(feedback, slot, kPointerSizeLog2);
  __ Daddu(feedback, vector, Operand(feedback));
  __ ld(feedback,
        FieldMemOperand(feedback, FixedArray::kHeaderSize + kPointerSize));
  HandleArrayCases(masm, feedback, receiver_map, scratch1, a7, false, &miss);

  __ bind(&miss);
  KeyedLoadIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);
  __ Branch(&compare_map);
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
  Register receiver = VectorStoreICDescriptor::ReceiverRegister();  // a1
  Register key = VectorStoreICDescriptor::NameRegister();           // a2
  Register vector = VectorStoreICDescriptor::VectorRegister();      // a3
  Register slot = VectorStoreICDescriptor::SlotRegister();          // a4
  DCHECK(VectorStoreICDescriptor::ValueRegister().is(a0));          // a0
  Register feedback = a5;
  Register receiver_map = a6;
  Register scratch1 = a7;

  __ SmiScale(scratch1, slot, kPointerSizeLog2);
  __ Daddu(feedback, vector, Operand(scratch1));
  __ ld(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  // Is it a fixed array?
  __ bind(&try_array);
  __ ld(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ Branch(&not_array, ne, scratch1, Heap::kFixedArrayMapRootIndex);

  Register scratch2 = t0;
  HandleArrayCases(masm, feedback, receiver_map, scratch1, scratch2, true,
                   &miss);

  __ bind(&not_array);
  __ Branch(&miss, ne, feedback, Heap::kmegamorphic_symbolRootIndex);
  Code::Flags code_flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, Code::STORE_IC, code_flags, receiver, key, feedback, receiver_map,
      scratch1, scratch2);

  __ bind(&miss);
  StoreIC::GenerateMiss(masm);

  __ bind(&load_smi_map);
  __ Branch(USE_DELAY_SLOT, &compare_map);
  __ LoadRoot(receiver_map, Heap::kHeapNumberMapRootIndex);  // In delay slot.
}


void VectorKeyedStoreICStub::Generate(MacroAssembler* masm) {
  GenerateImpl(masm, false);
}


void VectorKeyedStoreICStub::GenerateForTrampoline(MacroAssembler* masm) {
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

  __ ld(too_far, FieldMemOperand(feedback, FixedArray::kLengthOffset));

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
  __ SmiScale(too_far, too_far, kPointerSizeLog2);
  __ Daddu(too_far, feedback, Operand(too_far));
  __ Daddu(too_far, too_far, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Daddu(pointer_reg, feedback,
           Operand(FixedArray::OffsetOfElementAt(0) - kHeapObjectTag));

  __ bind(&next_loop);
  __ ld(cached_map, MemOperand(pointer_reg));
  __ ld(cached_map, FieldMemOperand(cached_map, WeakCell::kValueOffset));
  __ Branch(&prepare_next, ne, receiver_map, Operand(cached_map));
  // Is it a transitioning store?
  __ ld(too_far, MemOperand(pointer_reg, kPointerSize));
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&transition_call, ne, too_far, Operand(at));

  __ ld(pointer_reg, MemOperand(pointer_reg, kPointerSize * 2));
  __ Daddu(t9, pointer_reg, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  __ bind(&transition_call);
  __ ld(too_far, FieldMemOperand(too_far, WeakCell::kValueOffset));
  __ JumpIfSmi(too_far, miss);

  __ ld(receiver_map, MemOperand(pointer_reg, kPointerSize * 2));
  // Load the map into the correct register.
  DCHECK(feedback.is(VectorStoreTransitionDescriptor::MapRegister()));
  __ Move(feedback, too_far);
  __ Daddu(t9, receiver_map, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t9);

  __ bind(&prepare_next);
  __ Daddu(pointer_reg, pointer_reg, Operand(kPointerSize * 3));
  __ Branch(&next_loop, lt, pointer_reg, Operand(too_far));

  // We exhausted our array of map handler pairs.
  __ Branch(miss);
}


void VectorKeyedStoreICStub::GenerateImpl(MacroAssembler* masm, bool in_frame) {
  Register receiver = VectorStoreICDescriptor::ReceiverRegister();  // a1
  Register key = VectorStoreICDescriptor::NameRegister();           // a2
  Register vector = VectorStoreICDescriptor::VectorRegister();      // a3
  Register slot = VectorStoreICDescriptor::SlotRegister();          // a4
  DCHECK(VectorStoreICDescriptor::ValueRegister().is(a0));          // a0
  Register feedback = a5;
  Register receiver_map = a6;
  Register scratch1 = a7;

  __ SmiScale(scratch1, slot, kPointerSizeLog2);
  __ Daddu(feedback, vector, Operand(scratch1));
  __ ld(feedback, FieldMemOperand(feedback, FixedArray::kHeaderSize));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  Label try_array, load_smi_map, compare_map;
  Label not_array, miss;
  HandleMonomorphicCase(masm, receiver, receiver_map, feedback, vector, slot,
                        scratch1, &compare_map, &load_smi_map, &try_array);

  __ bind(&try_array);
  // Is it a fixed array?
  __ ld(scratch1, FieldMemOperand(feedback, HeapObject::kMapOffset));
  __ Branch(&not_array, ne, scratch1, Heap::kFixedArrayMapRootIndex);

  // We have a polymorphic element handler.
  Label try_poly_name;

  Register scratch2 = t0;

  HandlePolymorphicStoreCase(masm, feedback, receiver_map, scratch1, scratch2,
                             &miss);

  __ bind(&not_array);
  // Is it generic?
  __ Branch(&try_poly_name, ne, feedback, Heap::kmegamorphic_symbolRootIndex);
  Handle<Code> megamorphic_stub =
      KeyedStoreIC::ChooseMegamorphicStub(masm->isolate(), GetExtraICState());
  __ Jump(megamorphic_stub, RelocInfo::CODE_TARGET);

  __ bind(&try_poly_name);
  // We might have a name in feedback, and a fixed array in the next slot.
  __ Branch(&miss, ne, key, Operand(feedback));
  // If the name comparison succeeded, we know we have a fixed array with
  // at least one map/handler pair.
  __ SmiScale(scratch1, slot, kPointerSizeLog2);
  __ Daddu(feedback, vector, Operand(scratch1));
  __ ld(feedback,
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
  __ ld(a5, MemOperand(sp, 0));
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
      __ ld(a5, FieldMemOperand(a2, 0));
      __ LoadRoot(at, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite, a5, Operand(at));
    }

    // Save the resulting elements kind in type info. We can't just store a3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ ld(a4, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));
    __ Daddu(a4, a4, Operand(Smi::FromInt(kFastElementsKindPackedToHoley)));
    __ sd(a4, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));


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
    // For internal arrays we only need a few things.
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
    __ And(at, a0, a0);
    __ Branch(&not_zero_case, ne, at, Operand(zero_reg));
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ Branch(&not_one_case, gt, a0, Operand(1));
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
    __ ld(a4, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
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
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  Label subclassing;
  __ Branch(&subclassing, ne, a1, Operand(a3));

  Label no_info;
  // Get the elements kind and case on that.
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&no_info, eq, a2, Operand(at));

  __ ld(a3, FieldMemOperand(a2, AllocationSite::kTransitionInfoOffset));
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
      __ Dlsa(at, sp, a0, kPointerSizeLog2);
      __ sd(a1, MemOperand(at));
      __ li(at, Operand(3));
      __ Daddu(a0, a0, at);
      break;
    case NONE:
      __ sd(a1, MemOperand(sp, 0 * kPointerSize));
      __ li(a0, Operand(3));
      break;
    case ONE:
      __ sd(a1, MemOperand(sp, 1 * kPointerSize));
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

  InternalArrayNArgumentsConstructorStub stubN(isolate(), kind);
  __ TailCallStub(&stubN, hi, a0, Operand(1));

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument.
    __ ld(at, MemOperand(sp, 0));

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
    __ ld(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ SmiTst(a3, at);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction,
        at, Operand(zero_reg));
    __ GetObjectType(a3, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction,
        a4, Operand(MAP_TYPE));
  }

  // Figure out the right elements kind.
  __ ld(a3, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));

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
  __ ld(a2, FieldMemOperand(a3, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(a2, &new_object);
  __ GetObjectType(a2, a0, a0);
  __ Branch(&new_object, ne, a0, Operand(MAP_TYPE));

  // Fall back to runtime if the target differs from the new target's
  // initial map constructor.
  __ ld(a0, FieldMemOperand(a2, Map::kConstructorOrBackPointerOffset));
  __ Branch(&new_object, ne, a0, Operand(a1));

  // Allocate the JSObject on the heap.
  Label allocate, done_allocate;
  __ lbu(a4, FieldMemOperand(a2, Map::kInstanceSizeOffset));
  __ Allocate(a4, v0, a5, a0, &allocate, SIZE_IN_WORDS);
  __ bind(&done_allocate);

  // Initialize the JSObject fields.
  __ sd(a2, MemOperand(v0, JSObject::kMapOffset));
  __ LoadRoot(a3, Heap::kEmptyFixedArrayRootIndex);
  __ sd(a3, MemOperand(v0, JSObject::kPropertiesOffset));
  __ sd(a3, MemOperand(v0, JSObject::kElementsOffset));
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ Daddu(a1, v0, Operand(JSObject::kHeaderSize));

  // ----------- S t a t e -------------
  //  -- v0 : result (untagged)
  //  -- a1 : result fields (untagged)
  //  -- a5 : result end (untagged)
  //  -- a2 : initial map
  //  -- cp : context
  //  -- ra : return address
  // -----------------------------------

  // Perform in-object slack tracking if requested.
  Label slack_tracking;
  STATIC_ASSERT(Map::kNoSlackTracking == 0);
  __ lwu(a3, FieldMemOperand(a2, Map::kBitField3Offset));
  __ And(at, a3, Operand(Map::ConstructionCounter::kMask));
  __ Branch(USE_DELAY_SLOT, &slack_tracking, ne, at, Operand(zero_reg));
  __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);  // In delay slot.
  {
    // Initialize all in-object fields with undefined.
    __ InitializeFieldsWithFiller(a1, a5, a0);

    // Add the object tag to make the JSObject real.
    STATIC_ASSERT(kHeapObjectTag == 1);
    __ Ret(USE_DELAY_SLOT);
    __ Daddu(v0, v0, Operand(kHeapObjectTag));  // In delay slot.
  }
  __ bind(&slack_tracking);
  {
    // Decrease generous allocation count.
    STATIC_ASSERT(Map::ConstructionCounter::kNext == 32);
    __ Subu(a3, a3, Operand(1 << Map::ConstructionCounter::kShift));
    __ sw(a3, FieldMemOperand(a2, Map::kBitField3Offset));

    // Initialize the in-object fields with undefined.
    __ lbu(a4, FieldMemOperand(a2, Map::kUnusedPropertyFieldsOffset));
    __ dsll(a4, a4, kPointerSizeLog2);
    __ Dsubu(a4, a5, a4);
    __ InitializeFieldsWithFiller(a1, a4, a0);

    // Initialize the remaining (reserved) fields with one pointer filler map.
    __ LoadRoot(a0, Heap::kOnePointerFillerMapRootIndex);
    __ InitializeFieldsWithFiller(a1, a5, a0);

    // Check if we can finalize the instance size.
    Label finalize;
    STATIC_ASSERT(Map::kSlackTrackingCounterEnd == 1);
    __ And(a3, a3, Operand(Map::ConstructionCounter::kMask));
    __ Branch(USE_DELAY_SLOT, &finalize, eq, a3, Operand(zero_reg));
    STATIC_ASSERT(kHeapObjectTag == 1);
    __ Daddu(v0, v0, Operand(kHeapObjectTag));  // In delay slot.
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
    __ dsll(a4, a4, kPointerSizeLog2 + kSmiShiftSize + kSmiTagSize);
    __ SmiTag(a4);
    __ Push(a2, a4);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(a2);
  }
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ Dsubu(v0, v0, Operand(kHeapObjectTag));
  __ lbu(a5, FieldMemOperand(a2, Map::kInstanceSizeOffset));
  __ Dlsa(a5, v0, a5, kPointerSizeLog2);
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

  // For Ignition we need to skip all possible handler/stub frames until
  // we reach the JavaScript frame for the function (similar to what the
  // runtime fallback implementation does). So make a2 point to that
  // JavaScript frame.
  {
    Label loop, loop_entry;
    __ Branch(USE_DELAY_SLOT, &loop_entry);
    __ mov(a2, fp);  // In delay slot.
    __ bind(&loop);
    __ ld(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
    __ bind(&loop_entry);
    __ ld(a3, MemOperand(a2, StandardFrameConstants::kMarkerOffset));
    __ Branch(&loop, ne, a1, Operand(a3));
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ ld(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
  __ ld(a3, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&no_rest_parameters, ne, a3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ SmiLoadUntag(
      a0, MemOperand(a2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ ld(a1, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a1,
        FieldMemOperand(a1, SharedFunctionInfo::kFormalParameterCountOffset));
  __ Dsubu(a0, a0, Operand(a1));
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
    __ Allocate(JSArray::kSize, v0, a0, a1, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Setup the rest parameter array in v0.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, a1);
    __ sd(a1, FieldMemOperand(v0, JSArray::kMapOffset));
    __ LoadRoot(a1, Heap::kEmptyFixedArrayRootIndex);
    __ sd(a1, FieldMemOperand(v0, JSArray::kPropertiesOffset));
    __ sd(a1, FieldMemOperand(v0, JSArray::kElementsOffset));
    __ Move(a1, Smi::FromInt(0));
    __ Ret(USE_DELAY_SLOT);
    __ sd(a1, FieldMemOperand(v0, JSArray::kLengthOffset));  // In delay slot
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
    __ Dlsa(a2, a2, a0, kPointerSizeLog2);
    __ Daddu(a2, a2, Operand(StandardFrameConstants::kCallerSPOffset -
                             1 * kPointerSize));

    // ----------- S t a t e -------------
    //  -- cp : context
    //  -- a0 : number of rest parameters
    //  -- a2 : pointer to first rest parameters
    //  -- ra : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ li(a1, Operand(JSArray::kSize + FixedArray::kHeaderSize));
    __ Dlsa(a1, a1, a0, kPointerSizeLog2);
    __ Allocate(a1, v0, a3, a4, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Compute arguments.length in a4.
    __ SmiTag(a4, a0);

    // Setup the elements array in v0.
    __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
    __ sd(at, FieldMemOperand(v0, FixedArray::kMapOffset));
    __ sd(a4, FieldMemOperand(v0, FixedArray::kLengthOffset));
    __ Daddu(a3, v0, Operand(FixedArray::kHeaderSize));
    {
      Label loop, done_loop;
      __ Dlsa(a1, a3, a0, kPointerSizeLog2);
      __ bind(&loop);
      __ Branch(&done_loop, eq, a1, Operand(a3));
      __ ld(at, MemOperand(a2, 0 * kPointerSize));
      __ sd(at, FieldMemOperand(a3, 0 * kPointerSize));
      __ Dsubu(a2, a2, Operand(1 * kPointerSize));
      __ Daddu(a3, a3, Operand(1 * kPointerSize));
      __ Branch(&loop);
      __ bind(&done_loop);
    }

    // Setup the rest parameter array in a3.
    __ LoadNativeContextSlot(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, at);
    __ sd(at, FieldMemOperand(a3, JSArray::kMapOffset));
    __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
    __ sd(at, FieldMemOperand(a3, JSArray::kPropertiesOffset));
    __ sd(v0, FieldMemOperand(a3, JSArray::kElementsOffset));
    __ sd(a4, FieldMemOperand(a3, JSArray::kLengthOffset));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a3);  // In delay slot

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(a0);
      __ SmiTag(a1);
      __ Push(a0, a2, a1);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ Pop(a0, a2);
      __ SmiUntag(a0);
    }
    __ jmp(&done_allocate);
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

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2,
         FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ Lsa(a3, fp, a2, kPointerSizeLog2);
  __ Addu(a3, a3, Operand(StandardFrameConstants::kCallerSPOffset));
  __ SmiTag(a2);

  // a1 : function
  // a2 : number of parameters (tagged)
  // a3 : parameters pointer
  // Registers used over whole function:
  //  a5 : arguments count (tagged)
  //  a6 : mapped parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ ld(a4, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ld(a0, MemOperand(a4, StandardFrameConstants::kContextOffset));
  __ Branch(&adaptor_frame, eq, a0,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // No adaptor, parameter count = argument count.
  __ mov(a5, a2);
  __ Branch(USE_DELAY_SLOT, &try_allocate);
  __ mov(a6, a2);  // In delay slot.

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ ld(a5, MemOperand(a4, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiScale(t2, a5, kPointerSizeLog2);
  __ Daddu(a4, a4, Operand(t2));
  __ Daddu(a3, a4, Operand(StandardFrameConstants::kCallerSPOffset));

  // a5 = argument count (tagged)
  // a6 = parameter count (tagged)
  // Compute the mapped parameter count = min(a6, a5) in a6.
  __ mov(a6, a2);
  __ Branch(&try_allocate, le, a6, Operand(a5));
  __ mov(a6, a5);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  Label param_map_size;
  DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
  __ Branch(USE_DELAY_SLOT, &param_map_size, eq, a6, Operand(zero_reg));
  __ mov(t1, zero_reg);  // In delay slot: param map size = 0 when a6 == 0.
  __ SmiScale(t1, a6, kPointerSizeLog2);
  __ daddiu(t1, t1, kParameterMapHeaderSize);
  __ bind(&param_map_size);

  // 2. Backing store.
  __ SmiScale(t2, a5, kPointerSizeLog2);
  __ Daddu(t1, t1, Operand(t2));
  __ Daddu(t1, t1, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ Daddu(t1, t1, Operand(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(t1, v0, t1, a4, &runtime, TAG_OBJECT);

  // v0 = address of new object(s) (tagged)
  // a2 = argument count (smi-tagged)
  // Get the arguments boilerplate from the current native context into a4.
  const int kNormalOffset =
      Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);

  __ ld(a4, NativeContextMemOperand());
  Label skip2_ne, skip2_eq;
  __ Branch(&skip2_ne, ne, a6, Operand(zero_reg));
  __ ld(a4, MemOperand(a4, kNormalOffset));
  __ bind(&skip2_ne);

  __ Branch(&skip2_eq, eq, a6, Operand(zero_reg));
  __ ld(a4, MemOperand(a4, kAliasedOffset));
  __ bind(&skip2_eq);

  // v0 = address of new object (tagged)
  // a2 = argument count (smi-tagged)
  // a4 = address of arguments map (tagged)
  // a6 = mapped parameter count (tagged)
  __ sd(a4, FieldMemOperand(v0, JSObject::kMapOffset));
  __ LoadRoot(t1, Heap::kEmptyFixedArrayRootIndex);
  __ sd(t1, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sd(t1, FieldMemOperand(v0, JSObject::kElementsOffset));

  // Set up the callee in-object property.
  __ AssertNotSmi(a1);
  __ sd(a1, FieldMemOperand(v0, JSSloppyArgumentsObject::kCalleeOffset));

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(a5);
  __ sd(a5, FieldMemOperand(v0, JSSloppyArgumentsObject::kLengthOffset));

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, a4 will point there, otherwise
  // it will point to the backing store.
  __ Daddu(a4, v0, Operand(JSSloppyArgumentsObject::kSize));
  __ sd(a4, FieldMemOperand(v0, JSObject::kElementsOffset));

  // v0 = address of new object (tagged)
  // a2 = argument count (tagged)
  // a4 = address of parameter map or backing store (tagged)
  // a6 = mapped parameter count (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  Label skip3;
  __ Branch(&skip3, ne, a6, Operand(Smi::FromInt(0)));
  // Move backing store address to a1, because it is
  // expected there when filling in the unmapped arguments.
  __ mov(a1, a4);
  __ bind(&skip3);

  __ Branch(&skip_parameter_map, eq, a6, Operand(Smi::FromInt(0)));

  __ LoadRoot(a5, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ sd(a5, FieldMemOperand(a4, FixedArray::kMapOffset));
  __ Daddu(a5, a6, Operand(Smi::FromInt(2)));
  __ sd(a5, FieldMemOperand(a4, FixedArray::kLengthOffset));
  __ sd(cp, FieldMemOperand(a4, FixedArray::kHeaderSize + 0 * kPointerSize));
  __ SmiScale(t2, a6, kPointerSizeLog2);
  __ Daddu(a5, a4, Operand(t2));
  __ Daddu(a5, a5, Operand(kParameterMapHeaderSize));
  __ sd(a5, FieldMemOperand(a4, FixedArray::kHeaderSize + 1 * kPointerSize));

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ mov(a5, a6);
  __ Daddu(t1, a2, Operand(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ Dsubu(t1, t1, Operand(a6));
  __ LoadRoot(a7, Heap::kTheHoleValueRootIndex);
  __ SmiScale(t2, a5, kPointerSizeLog2);
  __ Daddu(a1, a4, Operand(t2));
  __ Daddu(a1, a1, Operand(kParameterMapHeaderSize));

  // a1 = address of backing store (tagged)
  // a4 = address of parameter map (tagged)
  // a0 = temporary scratch (a.o., for address calculation)
  // t1 = loop variable (tagged)
  // a7 = the hole value
  __ jmp(&parameters_test);

  __ bind(&parameters_loop);
  __ Dsubu(a5, a5, Operand(Smi::FromInt(1)));
  __ SmiScale(a0, a5, kPointerSizeLog2);
  __ Daddu(a0, a0, Operand(kParameterMapHeaderSize - kHeapObjectTag));
  __ Daddu(t2, a4, a0);
  __ sd(t1, MemOperand(t2));
  __ Dsubu(a0, a0, Operand(kParameterMapHeaderSize - FixedArray::kHeaderSize));
  __ Daddu(t2, a1, a0);
  __ sd(a7, MemOperand(t2));
  __ Daddu(t1, t1, Operand(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ Branch(&parameters_loop, ne, a5, Operand(Smi::FromInt(0)));

  // Restore t1 = argument count (tagged).
  __ ld(a5, FieldMemOperand(v0, JSSloppyArgumentsObject::kLengthOffset));

  __ bind(&skip_parameter_map);
  // v0 = address of new object (tagged)
  // a1 = address of backing store (tagged)
  // a5 = argument count (tagged)
  // a6 = mapped parameter count (tagged)
  // t1 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(t1, Heap::kFixedArrayMapRootIndex);
  __ sd(t1, FieldMemOperand(a1, FixedArray::kMapOffset));
  __ sd(a5, FieldMemOperand(a1, FixedArray::kLengthOffset));

  Label arguments_loop, arguments_test;
  __ SmiScale(t2, a6, kPointerSizeLog2);
  __ Dsubu(a3, a3, Operand(t2));
  __ jmp(&arguments_test);

  __ bind(&arguments_loop);
  __ Dsubu(a3, a3, Operand(kPointerSize));
  __ ld(a4, MemOperand(a3, 0));
  __ SmiScale(t2, a6, kPointerSizeLog2);
  __ Daddu(t1, a1, Operand(t2));
  __ sd(a4, FieldMemOperand(t1, FixedArray::kHeaderSize));
  __ Daddu(a6, a6, Operand(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ Branch(&arguments_loop, lt, a6, Operand(a5));

  // Return.
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // a5 = argument count (tagged)
  __ bind(&runtime);
  __ Push(a1, a3, a5);
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

  // For Ignition we need to skip all possible handler/stub frames until
  // we reach the JavaScript frame for the function (similar to what the
  // runtime fallback implementation does). So make a2 point to that
  // JavaScript frame.
  {
    Label loop, loop_entry;
    __ Branch(USE_DELAY_SLOT, &loop_entry);
    __ mov(a2, fp);  // In delay slot.
    __ bind(&loop);
    __ ld(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
    __ bind(&loop_entry);
    __ ld(a3, MemOperand(a2, StandardFrameConstants::kMarkerOffset));
    __ Branch(&loop, ne, a1, Operand(a3));
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ ld(a3, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));
  __ ld(a0, MemOperand(a3, StandardFrameConstants::kContextOffset));
  __ Branch(&arguments_adaptor, eq, a0,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  {
    __ ld(a1, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a0,
          FieldMemOperand(a1, SharedFunctionInfo::kFormalParameterCountOffset));
    __ Dlsa(a2, a2, a0, kPointerSizeLog2);
    __ Daddu(a2, a2, Operand(StandardFrameConstants::kCallerSPOffset -
                             1 * kPointerSize));
  }
  __ Branch(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    __ SmiLoadUntag(
        a0, MemOperand(a3, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ Dlsa(a2, a3, a0, kPointerSizeLog2);
    __ Daddu(a2, a2, Operand(StandardFrameConstants::kCallerSPOffset -
                             1 * kPointerSize));
  }
  __ bind(&arguments_done);

  // ----------- S t a t e -------------
  //  -- cp : context
  //  -- a0 : number of rest parameters
  //  -- a2 : pointer to first rest parameters
  //  -- ra : return address
  // -----------------------------------

  // Allocate space for the rest parameter array plus the backing store.
  Label allocate, done_allocate;
  __ li(a1, Operand(JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize));
  __ Dlsa(a1, a1, a0, kPointerSizeLog2);
  __ Allocate(a1, v0, a3, a4, &allocate, TAG_OBJECT);
  __ bind(&done_allocate);

  // Compute arguments.length in a4.
  __ SmiTag(a4, a0);

  // Setup the elements array in v0.
  __ LoadRoot(at, Heap::kFixedArrayMapRootIndex);
  __ sd(at, FieldMemOperand(v0, FixedArray::kMapOffset));
  __ sd(a4, FieldMemOperand(v0, FixedArray::kLengthOffset));
  __ Daddu(a3, v0, Operand(FixedArray::kHeaderSize));
  {
    Label loop, done_loop;
    __ Dlsa(a1, a3, a0, kPointerSizeLog2);
    __ bind(&loop);
    __ Branch(&done_loop, eq, a1, Operand(a3));
    __ ld(at, MemOperand(a2, 0 * kPointerSize));
    __ sd(at, FieldMemOperand(a3, 0 * kPointerSize));
    __ Dsubu(a2, a2, Operand(1 * kPointerSize));
    __ Daddu(a3, a3, Operand(1 * kPointerSize));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Setup the strict arguments object in a3.
  __ LoadNativeContextSlot(Context::STRICT_ARGUMENTS_MAP_INDEX, at);
  __ sd(at, FieldMemOperand(a3, JSStrictArgumentsObject::kMapOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ sd(at, FieldMemOperand(a3, JSStrictArgumentsObject::kPropertiesOffset));
  __ sd(v0, FieldMemOperand(a3, JSStrictArgumentsObject::kElementsOffset));
  __ sd(a4, FieldMemOperand(a3, JSStrictArgumentsObject::kLengthOffset));
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a3);  // In delay slot

  // Fall back to %AllocateInNewSpace.
  __ bind(&allocate);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(a0);
    __ SmiTag(a1);
    __ Push(a0, a2, a1);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ Pop(a0, a2);
    __ SmiUntag(a0);
  }
  __ jmp(&done_allocate);
}


void LoadGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = cp;
  Register slot_reg = a2;
  Register result_reg = v0;
  Label slow_case;

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ ld(result_reg, ContextMemOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = result_reg;
  }

  // Load the PropertyCell value at the specified slot.
  __ Dlsa(at, context_reg, slot_reg, kPointerSizeLog2);
  __ ld(result_reg, ContextMemOperand(at, 0));
  __ ld(result_reg, FieldMemOperand(result_reg, PropertyCell::kValueOffset));

  // Check that value is not the_hole.
  __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
  __ Branch(&slow_case, eq, result_reg, Operand(at));
  __ Ret();

  // Fallback to the runtime.
  __ bind(&slow_case);
  __ SmiTag(slot_reg);
  __ Push(slot_reg);
  __ TailCallRuntime(Runtime::kLoadGlobalViaContext);
}


void StoreGlobalViaContextStub::Generate(MacroAssembler* masm) {
  Register context_reg = cp;
  Register slot_reg = a2;
  Register value_reg = a0;
  Register cell_reg = a4;
  Register cell_value_reg = a5;
  Register cell_details_reg = a6;
  Label fast_heapobject_case, fast_smi_case, slow_case;

  if (FLAG_debug_code) {
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Check(ne, kUnexpectedValue, value_reg, Operand(at));
  }

  // Go up context chain to the script context.
  for (int i = 0; i < depth(); ++i) {
    __ ld(cell_reg, ContextMemOperand(context_reg, Context::PREVIOUS_INDEX));
    context_reg = cell_reg;
  }

  // Load the PropertyCell at the specified slot.
  __ Dlsa(at, context_reg, slot_reg, kPointerSizeLog2);
  __ ld(cell_reg, ContextMemOperand(at, 0));

  // Load PropertyDetails for the cell (actually only the cell_type and kind).
  __ ld(cell_details_reg,
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
  __ sd(value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ RecordWriteField(cell_reg, PropertyCell::kValueOffset, value_reg,
                      cell_details_reg, kRAHasNotBeenSaved, kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  // RecordWriteField clobbers the value register, so we need to reload.
  __ Ret(USE_DELAY_SLOT);
  __ ld(value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ bind(&not_mutable_data);

  // Check if PropertyCell value matches the new value (relevant for Constant,
  // ConstantType and Undefined cells).
  Label not_same_value;
  __ ld(cell_value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
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
  __ sd(value_reg, FieldMemOperand(cell_reg, PropertyCell::kValueOffset));
  __ bind(&value_is_heap_object);
  __ JumpIfSmi(cell_value_reg, &slow_case);
  Register cell_value_map_reg = cell_value_reg;
  __ ld(cell_value_map_reg,
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
  __ ld(s0, MemOperand(s3, kNextOffset));
  __ ld(s1, MemOperand(s3, kLimitOffset));
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
  __ ld(v0, return_value_operand);
  __ bind(&return_value_loaded);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ sd(s0, MemOperand(s3, kNextOffset));
  if (__ emit_debug_code()) {
    __ lw(a1, MemOperand(s3, kLevelOffset));
    __ Check(eq, kUnexpectedLevelAfterReturnFromApiCall, a1, Operand(s2));
  }
  __ Subu(s2, s2, Operand(1));
  __ sw(s2, MemOperand(s3, kLevelOffset));
  __ ld(at, MemOperand(s3, kLimitOffset));
  __ Branch(&delete_allocated_handles, ne, s1, Operand(at));

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ ld(cp, *context_restore_operand);
  }
  if (stack_space_offset != kInvalidStackOffset) {
    DCHECK(kCArgsSlotsSize == 0);
    __ ld(s0, MemOperand(sp, stack_space_offset));
  } else {
    __ li(s0, Operand(stack_space));
  }
  __ LeaveExitFrame(false, s0, !restore_context, NO_EMIT_RETURN,
                    stack_space_offset != kInvalidStackOffset);

  // Check if the function scheduled an exception.
  __ LoadRoot(a4, Heap::kTheHoleValueRootIndex);
  __ li(at, Operand(ExternalReference::scheduled_exception_address(isolate)));
  __ ld(a5, MemOperand(at));
  __ Branch(&promote_scheduled_exception, ne, a4, Operand(a5));

  __ Ret();

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ sd(s1, MemOperand(s3, kLimitOffset));
  __ mov(s0, v0);
  __ mov(a0, v0);
  __ PrepareCallCFunction(1, s1);
  __ li(a0, Operand(ExternalReference::isolate_address(isolate)));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate),
                   1);
  __ mov(v0, s0);
  __ jmp(&leave_exit_frame);
}

static void CallApiFunctionStubHelper(MacroAssembler* masm,
                                      const ParameterCount& argc,
                                      bool return_first_arg,
                                      bool call_data_undefined, bool is_lazy) {
  // ----------- S t a t e -------------
  //  -- a0                  : callee
  //  -- a4                  : call_data
  //  -- a2                  : holder
  //  -- a1                  : api_function_address
  //  -- a3                  : number of arguments if argc is a register
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
  STATIC_ASSERT(FCA::kArgsLength == 7);

  DCHECK(argc.is_immediate() || a3.is(argc.reg()));

  // Save context, callee and call data.
  __ Push(context, callee, call_data);
  if (!is_lazy) {
    // Load context from callee.
    __ ld(context, FieldMemOperand(callee, JSFunction::kContextOffset));
  }

  Register scratch = call_data;
  if (!call_data_undefined) {
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
  const int kApiStackSpace = 4;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(!api_function_address.is(a0) && !scratch.is(a0));
  // a0 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ Daddu(a0, sp, Operand(1 * kPointerSize));
  // FunctionCallbackInfo::implicit_args_
  __ sd(scratch, MemOperand(a0, 0 * kPointerSize));
  if (argc.is_immediate()) {
    // FunctionCallbackInfo::values_
    __ Daddu(at, scratch,
             Operand((FCA::kArgsLength - 1 + argc.immediate()) * kPointerSize));
    __ sd(at, MemOperand(a0, 1 * kPointerSize));
    // FunctionCallbackInfo::length_ = argc
    // Stored as int field, 32-bit integers within struct on stack always left
    // justified by n64 ABI.
    __ li(at, Operand(argc.immediate()));
    __ sw(at, MemOperand(a0, 2 * kPointerSize));
    // FunctionCallbackInfo::is_construct_call_ = 0
    __ sw(zero_reg, MemOperand(a0, 2 * kPointerSize + kIntSize));
  } else {
    // FunctionCallbackInfo::values_
    __ dsll(at, argc.reg(), kPointerSizeLog2);
    __ Daddu(at, at, scratch);
    __ Daddu(at, at, Operand((FCA::kArgsLength - 1) * kPointerSize));
    __ sd(at, MemOperand(a0, 1 * kPointerSize));
    // FunctionCallbackInfo::length_ = argc
    // Stored as int field, 32-bit integers within struct on stack always left
    // justified by n64 ABI.
    __ sw(argc.reg(), MemOperand(a0, 2 * kPointerSize));
    // FunctionCallbackInfo::is_construct_call_
    __ Daddu(argc.reg(), argc.reg(), Operand(FCA::kArgsLength + 1));
    __ dsll(at, argc.reg(), kPointerSizeLog2);
    __ sw(at, MemOperand(a0, 2 * kPointerSize + kIntSize));
  }

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  MemOperand context_restore_operand(
      fp, (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument.
  int return_value_offset = 0;
  if (return_first_arg) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);
  int stack_space = 0;
  int32_t stack_space_offset = 4 * kPointerSize;
  if (argc.is_immediate()) {
    stack_space = argc.immediate() + FCA::kArgsLength + 1;
    stack_space_offset = kInvalidStackOffset;
  }
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref, stack_space,
                           stack_space_offset, return_value_operand,
                           &context_restore_operand);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
  bool call_data_undefined = this->call_data_undefined();
  CallApiFunctionStubHelper(masm, ParameterCount(a3), false,
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
  //  -- sp[0]                        : name
  //  -- sp[8 .. (8 + kArgsLength*8)] : v8::PropertyCallbackInfo::args_
  //  -- ...
  //  -- a2                           : api_function_address
  // -----------------------------------

  Register api_function_address = ApiGetterDescriptor::function_address();
  DCHECK(api_function_address.is(a2));

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
  __ sd(a1, MemOperand(sp, 1 * kPointerSize));
  __ Daddu(a1, sp, Operand(1 * kPointerSize));
  // a1 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

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
