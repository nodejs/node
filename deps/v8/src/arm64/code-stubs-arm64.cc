// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/isolate.h"
#include "src/jsregexp.h"
#include "src/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


static void InitializeArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // cp: context
  // x1: function
  // x2: allocation site with elements kind
  // x0: number of arguments to the constructor function
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(x0, deopt_handler, constant_stack_parameter_count,
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


static void InitializeInternalArrayConstructorDescriptor(
    Isolate* isolate, CodeStubDescriptor* descriptor,
    int constant_stack_parameter_count) {
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kInternalArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    descriptor->Initialize(deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    descriptor->Initialize(x0, deopt_handler, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE, PASS_ARGUMENTS);
  }
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
  int param_count = descriptor.GetEnvironmentParameterCount();
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    DCHECK((param_count == 0) ||
           x0.Is(descriptor.GetEnvironmentParameterRegister(param_count - 1)));

    // Push arguments
    MacroAssembler::PushPopQueue queue(masm);
    for (int i = 0; i < param_count; ++i) {
      queue.Queue(descriptor.GetEnvironmentParameterRegister(i));
    }
    queue.PushQueued();

    __ CallExternalReference(miss, param_count);
  }

  __ Ret();
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


// See call site for description.
static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Register left,
                                          Register right,
                                          Register scratch,
                                          FPRegister double_scratch,
                                          Label* slow,
                                          Condition cond) {
  DCHECK(!AreAliased(left, right, scratch));
  Label not_identical, return_equal, heap_number;
  Register result = x0;

  __ Cmp(right, left);
  __ B(ne, &not_identical);

  // Test for NaN. Sadly, we can't just compare to factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis.  If it's not a heap number, then return equal.
  if ((cond == lt) || (cond == gt)) {
    __ JumpIfObjectType(right, scratch, scratch, FIRST_SPEC_OBJECT_TYPE, slow,
                        ge);
  } else if (cond == eq) {
    __ JumpIfHeapNumber(right, &heap_number);
  } else {
    Register right_type = scratch;
    __ JumpIfObjectType(right, right_type, right_type, HEAP_NUMBER_TYPE,
                        &heap_number);
    // Comparing JS objects with <=, >= is complicated.
    __ Cmp(right_type, FIRST_SPEC_OBJECT_TYPE);
    __ B(ge, slow);
    // Normally here we fall through to return_equal, but undefined is
    // special: (undefined == undefined) == true, but
    // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
    if ((cond == le) || (cond == ge)) {
      __ Cmp(right_type, ODDBALL_TYPE);
      __ B(ne, &return_equal);
      __ JumpIfNotRoot(right, Heap::kUndefinedValueRootIndex, &return_equal);
      if (cond == le) {
        // undefined <= undefined should fail.
        __ Mov(result, GREATER);
      } else {
        // undefined >= undefined should fail.
        __ Mov(result, LESS);
      }
      __ Ret();
    }
  }

  __ Bind(&return_equal);
  if (cond == lt) {
    __ Mov(result, GREATER);  // Things aren't less than themselves.
  } else if (cond == gt) {
    __ Mov(result, LESS);     // Things aren't greater than themselves.
  } else {
    __ Mov(result, EQUAL);    // Things are <=, >=, ==, === themselves.
  }
  __ Ret();

  // Cases lt and gt have been handled earlier, and case ne is never seen, as
  // it is handled in the parser (see Parser::ParseBinaryExpression). We are
  // only concerned with cases ge, le and eq here.
  if ((cond != lt) && (cond != gt)) {
    DCHECK((cond == ge) || (cond == le) || (cond == eq));
    __ Bind(&heap_number);
    // Left and right are identical pointers to a heap number object. Return
    // non-equal if the heap number is a NaN, and equal otherwise. Comparing
    // the number to itself will set the overflow flag iff the number is NaN.
    __ Ldr(double_scratch, FieldMemOperand(right, HeapNumber::kValueOffset));
    __ Fcmp(double_scratch, double_scratch);
    __ B(vc, &return_equal);  // Not NaN, so treat as normal heap number.

    if (cond == le) {
      __ Mov(result, GREATER);
    } else {
      __ Mov(result, LESS);
    }
    __ Ret();
  }

  // No fall through here.
  if (FLAG_debug_code) {
    __ Unreachable();
  }

  __ Bind(&not_identical);
}


// See call site for description.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register left,
                                           Register right,
                                           Register left_type,
                                           Register right_type,
                                           Register scratch) {
  DCHECK(!AreAliased(left, right, left_type, right_type, scratch));

  if (masm->emit_debug_code()) {
    // We assume that the arguments are not identical.
    __ Cmp(left, right);
    __ Assert(ne, kExpectedNonIdenticalObjects);
  }

  // If either operand is a JS object or an oddball value, then they are not
  // equal since their pointers are different.
  // There is no test for undetectability in strict equality.
  STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);
  Label right_non_object;

  __ Cmp(right_type, FIRST_SPEC_OBJECT_TYPE);
  __ B(lt, &right_non_object);

  // Return non-zero - x0 already contains a non-zero pointer.
  DCHECK(left.is(x0) || right.is(x0));
  Label return_not_equal;
  __ Bind(&return_not_equal);
  __ Ret();

  __ Bind(&right_non_object);

  // Check for oddballs: true, false, null, undefined.
  __ Cmp(right_type, ODDBALL_TYPE);

  // If right is not ODDBALL, test left. Otherwise, set eq condition.
  __ Ccmp(left_type, ODDBALL_TYPE, ZFlag, ne);

  // If right or left is not ODDBALL, test left >= FIRST_SPEC_OBJECT_TYPE.
  // Otherwise, right or left is ODDBALL, so set a ge condition.
  __ Ccmp(left_type, FIRST_SPEC_OBJECT_TYPE, NVFlag, ne);

  __ B(ge, &return_not_equal);

  // Internalized strings are unique, so they can only be equal if they are the
  // same object. We have already tested that case, so if left and right are
  // both internalized strings, they cannot be equal.
  STATIC_ASSERT((kInternalizedTag == 0) && (kStringTag == 0));
  __ Orr(scratch, left_type, right_type);
  __ TestAndBranchIfAllClear(
      scratch, kIsNotStringMask | kIsNotInternalizedMask, &return_not_equal);
}


// See call site for description.
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register left,
                                    Register right,
                                    FPRegister left_d,
                                    FPRegister right_d,
                                    Label* slow,
                                    bool strict) {
  DCHECK(!AreAliased(left_d, right_d));
  DCHECK((left.is(x0) && right.is(x1)) ||
         (right.is(x0) && left.is(x1)));
  Register result = x0;

  Label right_is_smi, done;
  __ JumpIfSmi(right, &right_is_smi);

  // Left is the smi. Check whether right is a heap number.
  if (strict) {
    // If right is not a number and left is a smi, then strict equality cannot
    // succeed. Return non-equal.
    Label is_heap_number;
    __ JumpIfHeapNumber(right, &is_heap_number);
    // Register right is a non-zero pointer, which is a valid NOT_EQUAL result.
    if (!right.is(result)) {
      __ Mov(result, NOT_EQUAL);
    }
    __ Ret();
    __ Bind(&is_heap_number);
  } else {
    // Smi compared non-strictly with a non-smi, non-heap-number. Call the
    // runtime.
    __ JumpIfNotHeapNumber(right, slow);
  }

  // Left is the smi. Right is a heap number. Load right value into right_d, and
  // convert left smi into double in left_d.
  __ Ldr(right_d, FieldMemOperand(right, HeapNumber::kValueOffset));
  __ SmiUntagToDouble(left_d, left);
  __ B(&done);

  __ Bind(&right_is_smi);
  // Right is a smi. Check whether the non-smi left is a heap number.
  if (strict) {
    // If left is not a number and right is a smi then strict equality cannot
    // succeed. Return non-equal.
    Label is_heap_number;
    __ JumpIfHeapNumber(left, &is_heap_number);
    // Register left is a non-zero pointer, which is a valid NOT_EQUAL result.
    if (!left.is(result)) {
      __ Mov(result, NOT_EQUAL);
    }
    __ Ret();
    __ Bind(&is_heap_number);
  } else {
    // Smi compared non-strictly with a non-smi, non-heap-number. Call the
    // runtime.
    __ JumpIfNotHeapNumber(left, slow);
  }

  // Right is the smi. Left is a heap number. Load left value into left_d, and
  // convert right smi into double in right_d.
  __ Ldr(left_d, FieldMemOperand(left, HeapNumber::kValueOffset));
  __ SmiUntagToDouble(right_d, right);

  // Fall through to both_loaded_as_doubles.
  __ Bind(&done);
}


// Fast negative check for internalized-to-internalized equality.
// See call site for description.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register left,
                                                     Register right,
                                                     Register left_map,
                                                     Register right_map,
                                                     Register left_type,
                                                     Register right_type,
                                                     Label* possible_strings,
                                                     Label* not_both_strings) {
  DCHECK(!AreAliased(left, right, left_map, right_map, left_type, right_type));
  Register result = x0;

  Label object_test;
  STATIC_ASSERT((kInternalizedTag == 0) && (kStringTag == 0));
  // TODO(all): reexamine this branch sequence for optimisation wrt branch
  // prediction.
  __ Tbnz(right_type, MaskToBit(kIsNotStringMask), &object_test);
  __ Tbnz(right_type, MaskToBit(kIsNotInternalizedMask), possible_strings);
  __ Tbnz(left_type, MaskToBit(kIsNotStringMask), not_both_strings);
  __ Tbnz(left_type, MaskToBit(kIsNotInternalizedMask), possible_strings);

  // Both are internalized. We already checked that they weren't the same
  // pointer, so they are not equal.
  __ Mov(result, NOT_EQUAL);
  __ Ret();

  __ Bind(&object_test);

  __ Cmp(right_type, FIRST_SPEC_OBJECT_TYPE);

  // If right >= FIRST_SPEC_OBJECT_TYPE, test left.
  // Otherwise, right < FIRST_SPEC_OBJECT_TYPE, so set lt condition.
  __ Ccmp(left_type, FIRST_SPEC_OBJECT_TYPE, NFlag, ge);

  __ B(lt, not_both_strings);

  // If both objects are undetectable, they are equal. Otherwise, they are not
  // equal, since they are different objects and an object is not equal to
  // undefined.

  // Returning here, so we can corrupt right_type and left_type.
  Register right_bitfield = right_type;
  Register left_bitfield = left_type;
  __ Ldrb(right_bitfield, FieldMemOperand(right_map, Map::kBitFieldOffset));
  __ Ldrb(left_bitfield, FieldMemOperand(left_map, Map::kBitFieldOffset));
  __ And(result, right_bitfield, left_bitfield);
  __ And(result, result, 1 << Map::kIsUndetectable);
  __ Eor(result, result, 1 << Map::kIsUndetectable);
  __ Ret();
}


static void CompareICStub_CheckInputType(MacroAssembler* masm, Register input,
                                         CompareICState::State expected,
                                         Label* fail) {
  Label ok;
  if (expected == CompareICState::SMI) {
    __ JumpIfNotSmi(input, fail);
  } else if (expected == CompareICState::NUMBER) {
    __ JumpIfSmi(input, &ok);
    __ JumpIfNotHeapNumber(input, fail);
  }
  // We could be strict about internalized/non-internalized here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ Bind(&ok);
}


void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Register lhs = x1;
  Register rhs = x0;
  Register result = x0;
  Condition cond = GetCondition();

  Label miss;
  CompareICStub_CheckInputType(masm, lhs, left(), &miss);
  CompareICStub_CheckInputType(masm, rhs, right(), &miss);

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles;
  Label not_two_smis, smi_done;
  __ JumpIfEitherNotSmi(lhs, rhs, &not_two_smis);
  __ SmiUntag(lhs);
  __ Sub(result, lhs, Operand::UntagSmi(rhs));
  __ Ret();

  __ Bind(&not_two_smis);

  // NOTICE! This code is only reached after a smi-fast-case check, so it is
  // certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical. Either returns the answer
  // or goes to slow. Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, lhs, rhs, x10, d0, &slow, cond);

  // If either is a smi (we know that at least one is not a smi), then they can
  // only be strictly equal if the other is a HeapNumber.
  __ JumpIfBothNotSmi(lhs, rhs, &not_smis);

  // Exactly one operand is a smi. EmitSmiNonsmiComparison generates code that
  // can:
  //  1) Return the answer.
  //  2) Branch to the slow case.
  //  3) Fall through to both_loaded_as_doubles.
  // In case 3, we have found out that we were dealing with a number-number
  // comparison. The double values of the numbers have been loaded, right into
  // rhs_d, left into lhs_d.
  FPRegister rhs_d = d0;
  FPRegister lhs_d = d1;
  EmitSmiNonsmiComparison(masm, lhs, rhs, lhs_d, rhs_d, &slow, strict());

  __ Bind(&both_loaded_as_doubles);
  // The arguments have been converted to doubles and stored in rhs_d and
  // lhs_d.
  Label nan;
  __ Fcmp(lhs_d, rhs_d);
  __ B(vs, &nan);  // Overflow flag set if either is NaN.
  STATIC_ASSERT((LESS == -1) && (EQUAL == 0) && (GREATER == 1));
  __ Cset(result, gt);  // gt => 1, otherwise (lt, eq) => 0 (EQUAL).
  __ Csinv(result, result, xzr, ge);  // lt => -1, gt => 1, eq => 0.
  __ Ret();

  __ Bind(&nan);
  // Left and/or right is a NaN. Load the result register with whatever makes
  // the comparison fail, since comparisons with NaN always fail (except ne,
  // which is filtered out at a higher level.)
  DCHECK(cond != ne);
  if ((cond == lt) || (cond == le)) {
    __ Mov(result, GREATER);
  } else {
    __ Mov(result, LESS);
  }
  __ Ret();

  __ Bind(&not_smis);
  // At this point we know we are dealing with two different objects, and
  // neither of them is a smi. The objects are in rhs_ and lhs_.

  // Load the maps and types of the objects.
  Register rhs_map = x10;
  Register rhs_type = x11;
  Register lhs_map = x12;
  Register lhs_type = x13;
  __ Ldr(rhs_map, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ Ldr(lhs_map, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ Ldrb(rhs_type, FieldMemOperand(rhs_map, Map::kInstanceTypeOffset));
  __ Ldrb(lhs_type, FieldMemOperand(lhs_map, Map::kInstanceTypeOffset));

  if (strict()) {
    // This emits a non-equal return sequence for some object types, or falls
    // through if it was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs, rhs, lhs_type, rhs_type, x14);
  }

  Label check_for_internalized_strings;
  Label flat_string_check;
  // Check for heap number comparison. Branch to earlier double comparison code
  // if they are heap numbers, otherwise, branch to internalized string check.
  __ Cmp(rhs_type, HEAP_NUMBER_TYPE);
  __ B(ne, &check_for_internalized_strings);
  __ Cmp(lhs_map, rhs_map);

  // If maps aren't equal, lhs_ and rhs_ are not heap numbers. Branch to flat
  // string check.
  __ B(ne, &flat_string_check);

  // Both lhs_ and rhs_ are heap numbers. Load them and branch to the double
  // comparison code.
  __ Ldr(lhs_d, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  __ Ldr(rhs_d, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  __ B(&both_loaded_as_doubles);

  __ Bind(&check_for_internalized_strings);
  // In the strict case, the EmitStrictTwoHeapObjectCompare already took care
  // of internalized strings.
  if ((cond == eq) && !strict()) {
    // Returns an answer for two internalized strings or two detectable objects.
    // Otherwise branches to the string case or not both strings case.
    EmitCheckForInternalizedStringsOrObjects(masm, lhs, rhs, lhs_map, rhs_map,
                                             lhs_type, rhs_type,
                                             &flat_string_check, &slow);
  }

  // Check for both being sequential one-byte strings,
  // and inline if that is the case.
  __ Bind(&flat_string_check);
  __ JumpIfBothInstanceTypesAreNotSequentialOneByte(lhs_type, rhs_type, x14,
                                                    x15, &slow);

  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, x10,
                      x11);
  if (cond == eq) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, x10, x11,
                                                  x12);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, x10, x11,
                                                    x12, x13);
  }

  // Never fall through to here.
  if (FLAG_debug_code) {
    __ Unreachable();
  }

  __ Bind(&slow);

  __ Push(lhs, rhs);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
  if (cond == eq) {
    native = strict() ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    native = Builtins::COMPARE;
    int ncr;  // NaN compare result
    if ((cond == lt) || (cond == le)) {
      ncr = GREATER;
    } else {
      DCHECK((cond == gt) || (cond == ge));  // remaining cases
      ncr = LESS;
    }
    __ Mov(x10, Smi::FromInt(ncr));
    __ Push(x10);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(native, JUMP_FUNCTION);

  __ Bind(&miss);
  GenerateMiss(masm);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  CPURegList saved_regs = kCallerSaved;
  CPURegList saved_fp_regs = kCallerSavedFP;

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


void StoreRegistersStateStub::Generate(MacroAssembler* masm) {
  MacroAssembler::NoUseRealAbortsScope no_use_real_aborts(masm);
  UseScratchRegisterScope temps(masm);
  Register saved_lr = temps.UnsafeAcquire(to_be_pushed_lr());
  Register return_address = temps.AcquireX();
  __ Mov(return_address, lr);
  // Restore lr with the value it had before the call to this stub (the value
  // which must be pushed).
  __ Mov(lr, saved_lr);
  __ PushSafepointRegisters();
  __ Ret(return_address);
}


void RestoreRegistersStateStub::Generate(MacroAssembler* masm) {
  MacroAssembler::NoUseRealAbortsScope no_use_real_aborts(masm);
  UseScratchRegisterScope temps(masm);
  Register return_address = temps.AcquireX();
  // Preserve the return address (lr will be clobbered by the pop).
  __ Mov(return_address, lr);
  __ PopSafepointRegisters();
  __ Ret(return_address);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  // Stack on entry:
  // jssp[0]: Exponent (as a tagged value).
  // jssp[1]: Base (as a tagged value).
  //
  // The (tagged) result will be returned in x0, as a heap number.

  Register result_tagged = x0;
  Register base_tagged = x10;
  Register exponent_tagged = MathPowTaggedDescriptor::exponent();
  DCHECK(exponent_tagged.is(x11));
  Register exponent_integer = MathPowIntegerDescriptor::exponent();
  DCHECK(exponent_integer.is(x12));
  Register scratch1 = x14;
  Register scratch0 = x15;
  Register saved_lr = x19;
  FPRegister result_double = d0;
  FPRegister base_double = d0;
  FPRegister exponent_double = d1;
  FPRegister base_double_copy = d2;
  FPRegister scratch1_double = d6;
  FPRegister scratch0_double = d7;

  // A fast-path for integer exponents.
  Label exponent_is_smi, exponent_is_integer;
  // Bail out to runtime.
  Label call_runtime;
  // Allocate a heap number for the result, and return it.
  Label done;

  // Unpack the inputs.
  if (exponent_type() == ON_STACK) {
    Label base_is_smi;
    Label unpack_exponent;

    __ Pop(exponent_tagged, base_tagged);

    __ JumpIfSmi(base_tagged, &base_is_smi);
    __ JumpIfNotHeapNumber(base_tagged, &call_runtime);
    // base_tagged is a heap number, so load its double value.
    __ Ldr(base_double, FieldMemOperand(base_tagged, HeapNumber::kValueOffset));
    __ B(&unpack_exponent);
    __ Bind(&base_is_smi);
    // base_tagged is a SMI, so untag it and convert it to a double.
    __ SmiUntagToDouble(base_double, base_tagged);

    __ Bind(&unpack_exponent);
    //  x10   base_tagged       The tagged base (input).
    //  x11   exponent_tagged   The tagged exponent (input).
    //  d1    base_double       The base as a double.
    __ JumpIfSmi(exponent_tagged, &exponent_is_smi);
    __ JumpIfNotHeapNumber(exponent_tagged, &call_runtime);
    // exponent_tagged is a heap number, so load its double value.
    __ Ldr(exponent_double,
           FieldMemOperand(exponent_tagged, HeapNumber::kValueOffset));
  } else if (exponent_type() == TAGGED) {
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

    if (exponent_type() == ON_STACK) {
      FPRegister  half_double = d3;
      FPRegister  minus_half_double = d4;
      // Detect square root case. Crankshaft detects constant +/-0.5 at compile
      // time and uses DoMathPowHalf instead. We then skip this check for
      // non-constant cases of +/-0.5 as these hardly occur.

      __ Fmov(minus_half_double, -0.5);
      __ Fmov(half_double, 0.5);
      __ Fcmp(minus_half_double, exponent_double);
      __ Fccmp(half_double, exponent_double, NZFlag, ne);
      // Condition flags at this point:
      //    0.5;  nZCv    // Identified by eq && pl
      //   -0.5:  NZcv    // Identified by eq && mi
      //  other:  ?z??    // Identified by ne
      __ B(ne, &call_runtime);

      // The exponent is 0.5 or -0.5.

      // Given that exponent is known to be either 0.5 or -0.5, the following
      // special cases could apply (according to ECMA-262 15.8.2.13):
      //
      //  base.isNaN():                   The result is NaN.
      //  (base == +INFINITY) || (base == -INFINITY)
      //    exponent == 0.5:              The result is +INFINITY.
      //    exponent == -0.5:             The result is +0.
      //  (base == +0) || (base == -0)
      //    exponent == 0.5:              The result is +0.
      //    exponent == -0.5:             The result is +INFINITY.
      //  (base < 0) && base.isFinite():  The result is NaN.
      //
      // Fsqrt (and Fdiv for the -0.5 case) can handle all of those except
      // where base is -INFINITY or -0.

      // Add +0 to base. This has no effect other than turning -0 into +0.
      __ Fadd(base_double, base_double, fp_zero);
      // The operation -0+0 results in +0 in all cases except where the
      // FPCR rounding mode is 'round towards minus infinity' (RM). The
      // ARM64 simulator does not currently simulate FPCR (where the rounding
      // mode is set), so test the operation with some debug code.
      if (masm->emit_debug_code()) {
        UseScratchRegisterScope temps(masm);
        Register temp = temps.AcquireX();
        __ Fneg(scratch0_double, fp_zero);
        // Verify that we correctly generated +0.0 and -0.0.
        //  bits(+0.0) = 0x0000000000000000
        //  bits(-0.0) = 0x8000000000000000
        __ Fmov(temp, fp_zero);
        __ CheckRegisterIsClear(temp, kCouldNotGenerateZero);
        __ Fmov(temp, scratch0_double);
        __ Eor(temp, temp, kDSignMask);
        __ CheckRegisterIsClear(temp, kCouldNotGenerateNegativeZero);
        // Check that -0.0 + 0.0 == +0.0.
        __ Fadd(scratch0_double, scratch0_double, fp_zero);
        __ Fmov(temp, scratch0_double);
        __ CheckRegisterIsClear(temp, kExpectedPositiveZero);
      }

      // If base is -INFINITY, make it +INFINITY.
      //  * Calculate base - base: All infinities will become NaNs since both
      //    -INFINITY+INFINITY and +INFINITY-INFINITY are NaN in ARM64.
      //  * If the result is NaN, calculate abs(base).
      __ Fsub(scratch0_double, base_double, base_double);
      __ Fcmp(scratch0_double, 0.0);
      __ Fabs(scratch1_double, base_double);
      __ Fcsel(base_double, scratch1_double, base_double, vs);

      // Calculate the square root of base.
      __ Fsqrt(result_double, base_double);
      __ Fcmp(exponent_double, 0.0);
      __ B(ge, &done);  // Finish now for exponents of 0.5.
      // Find the inverse for exponents of -0.5.
      __ Fmov(scratch0_double, 1.0);
      __ Fdiv(result_double, scratch0_double, result_double);
      __ B(&done);
    }

    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ Mov(saved_lr, lr);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()),
          0, 2);
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

  if (exponent_type() == ON_STACK) {
    // Bail out to runtime code.
    __ Bind(&call_runtime);
    // Put the arguments back on the stack.
    __ Push(base_tagged, exponent_tagged);
    __ TailCallRuntime(Runtime::kMathPowRT, 2, 1);

    // Return.
    __ Bind(&done);
    __ AllocateHeapNumber(result_tagged, &call_runtime, scratch0, scratch1,
                          result_double);
    DCHECK(result_tagged.is(x0));
    __ IncrementCounter(
        isolate()->counters()->math_pow(), 1, scratch0, scratch1);
    __ Ret();
  } else {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ Mov(saved_lr, lr);
    __ Fmov(base_double, base_double_copy);
    __ Scvtf(exponent_double, exponent_integer);
    __ CallCFunction(
        ExternalReference::power_double_double_function(isolate()),
        0, 2);
    __ Mov(lr, saved_lr);
    __ Bind(&done);
    __ IncrementCounter(
        isolate()->counters()->math_pow(), 1, scratch0, scratch1);
    __ Ret();
  }
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  // It is important that the following stubs are generated in this order
  // because pregenerated stubs can only call other pregenerated stubs.
  // RecordWriteStub uses StoreBufferOverflowStub, which in turn uses
  // CEntryStub.
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  StoreRegistersStateStub::GenerateAheadOfTime(isolate);
  RestoreRegistersStateStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
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
  __ Add(temp_argv, jssp, Operand(x0, LSL, kPointerSizeLog2));
  //  - Adjust for the receiver.
  __ Sub(temp_argv, temp_argv, 1 * kPointerSize);

  // Enter the exit frame. Reserve three slots to preserve x21-x23 callee-saved
  // registers.
  FrameScope scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(save_doubles(), x10, 3);
  DCHECK(csp.Is(__ StackPointer()));

  // Poke callee-saved registers into reserved space.
  __ Poke(argv, 1 * kPointerSize);
  __ Poke(argc, 2 * kPointerSize);
  __ Poke(target, 3 * kPointerSize);

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

  //  x0    result      The return code from the call.
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
  // Pop or drop the remaining stack slots and return from the stub.
  //         jssp[24]:    Arguments array (of size argc), including receiver.
  //         jssp[16]:    Preserved x23 (used for target).
  //         jssp[8]:     Preserved x22 (used for argc).
  //         jssp[0]:     Preserved x21 (used for argv).
  __ Drop(x11);
  __ AssertFPCRState();
  __ Ret();

  // The stack pointer is still csp if we aren't returning, and the frame
  // hasn't changed (except for the return address).
  __ SetStackPointer(csp);

  // Handling of exception.
  __ Bind(&exception_returned);

  // Retrieve the pending exception.
  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, isolate());
  const Register& exception = result;
  const Register& exception_address = x11;
  __ Mov(exception_address, Operand(pending_exception_address));
  __ Ldr(exception, MemOperand(exception_address));

  // Clear the pending exception.
  __ Mov(x10, Operand(isolate()->factory()->the_hole_value()));
  __ Str(x10, MemOperand(exception_address));

  //  x0    exception   The exception descriptor.
  //  x21   argv
  //  x22   argc
  //  x23   target

  // Special handling of termination exceptions, which are uncatchable by
  // JavaScript code.
  Label throw_termination_exception;
  __ Cmp(exception, Operand(isolate()->factory()->termination_exception()));
  __ B(eq, &throw_termination_exception);

  // We didn't execute a return case, so the stack frame hasn't been updated
  // (except for the return address slot). However, we don't need to initialize
  // jssp because the throw method will immediately overwrite it when it
  // unwinds the stack.
  __ SetStackPointer(jssp);

  ASM_LOCATION("Throw normal");
  __ Mov(argv, 0);
  __ Mov(argc, 0);
  __ Mov(target, 0);
  __ Throw(x0, x10, x11, x12, x13);

  __ Bind(&throw_termination_exception);
  ASM_LOCATION("Throw termination");
  __ Mov(argv, 0);
  __ Mov(argc, 0);
  __ Mov(target, 0);
  __ ThrowUncatchable(x0, x10, x11, x12, x13);
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

  // Configure the FPCR. We don't restore it, so this is technically not allowed
  // according to AAPCS64. However, we only set default-NaN mode and this will
  // be harmless for most C code. Also, it works for ARM.
  __ ConfigureFPCR();

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Set up the reserved register for 0.0.
  __ Fmov(fp_zero, 0.0);

  // Build an entry frame (see layout below).
  int marker = type();
  int64_t bad_frame_pointer = -1L;  // Bad frame pointer to fail if it is used.
  __ Mov(x13, bad_frame_pointer);
  __ Mov(x12, Smi::FromInt(marker));
  __ Mov(x11, ExternalReference(Isolate::kCEntryFPAddress, isolate()));
  __ Ldr(x10, MemOperand(x11));

  __ Push(x13, xzr, x12, x10);
  // Set up fp.
  __ Sub(fp, jssp, EntryFrameConstants::kCallerFPOffset);

  // Push the JS entry frame marker. Also set js_entry_sp if this is the
  // outermost JS call.
  Label non_outermost_js, done;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ Mov(x10, ExternalReference(js_entry_sp));
  __ Ldr(x11, MemOperand(x10));
  __ Cbnz(x11, &non_outermost_js);
  __ Str(fp, MemOperand(x10));
  __ Mov(x12, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ Push(x12);
  __ B(&done);
  __ Bind(&non_outermost_js);
  // We spare one instruction by pushing xzr since the marker is 0.
  DCHECK(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME) == NULL);
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
    __ Mov(x10, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                          isolate())));
  }
  __ Str(code_entry, MemOperand(x10));
  __ LoadRoot(x0, Heap::kExceptionRootIndex);
  __ B(&exit);

  // Invoke: Link this frame into the handler chain.  There's only one
  // handler block in this code object, so its index is 0.
  __ Bind(&invoke);
  __ PushTryHandler(StackHandler::JS_ENTRY, 0);
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the B(&invoke) above, which
  // restores all callee-saved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ Mov(x10, Operand(isolate()->factory()->the_hole_value()));
  __ Mov(x11, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                        isolate())));
  __ Str(x10, MemOperand(x11));

  // Invoke the function by calling through the JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // x0: code entry.
  // x1: function.
  // x2: receiver.
  // x3: argc.
  // x4: argv.
  ExternalReference entry(type() == StackFrame::ENTRY_CONSTRUCT
                              ? Builtins::kJSConstructEntryTrampoline
                              : Builtins::kJSEntryTrampoline,
                          isolate());
  __ Mov(x10, entry);

  // Call the JSEntryTrampoline.
  __ Ldr(x11, MemOperand(x10));  // Dereference the address.
  __ Add(x12, x11, Code::kHeaderSize - kHeapObjectTag);
  __ Blr(x12);

  // Unlink this frame from the handler chain.
  __ PopTryHandler();


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
  __ Cmp(x10, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ B(ne, &non_outermost_js_2);
  __ Mov(x11, ExternalReference(js_entry_sp));
  __ Str(xzr, MemOperand(x11));
  __ Bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ Pop(x10);
  __ Mov(x11, ExternalReference(Isolate::kCEntryFPAddress, isolate()));
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


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();

  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, x10,
                                                          x11, &miss);

  __ Bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void InstanceofStub::Generate(MacroAssembler* masm) {
  // Stack on entry:
  // jssp[0]: function.
  // jssp[8]: object.
  //
  // Returns result in x0. Zero indicates instanceof, smi 1 indicates not
  // instanceof.

  Register result = x0;
  Register function = right();
  Register object = left();
  Register scratch1 = x6;
  Register scratch2 = x7;
  Register res_true = x8;
  Register res_false = x9;
  // Only used if there was an inline map check site. (See
  // LCodeGen::DoInstanceOfKnownGlobal().)
  Register map_check_site = x4;
  // Delta for the instructions generated between the inline map check and the
  // instruction setting the result.
  const int32_t kDeltaToLoadBoolResult = 4 * kInstructionSize;

  Label not_js_object, slow;

  if (!HasArgsInRegisters()) {
    __ Pop(function, object);
  }

  if (ReturnTrueFalseObject()) {
    __ LoadTrueFalseRoots(res_true, res_false);
  } else {
    // This is counter-intuitive, but correct.
    __ Mov(res_true, Smi::FromInt(0));
    __ Mov(res_false, Smi::FromInt(1));
  }

  // Check that the left hand side is a JS object and load its map as a side
  // effect.
  Register map = x12;
  __ JumpIfSmi(object, &not_js_object);
  __ IsObjectJSObjectType(object, map, scratch2, &not_js_object);

  // If there is a call site cache, don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck() && !ReturnTrueFalseObject()) {
    Label miss;
    __ JumpIfNotRoot(function, Heap::kInstanceofCacheFunctionRootIndex, &miss);
    __ JumpIfNotRoot(map, Heap::kInstanceofCacheMapRootIndex, &miss);
    __ LoadRoot(result, Heap::kInstanceofCacheAnswerRootIndex);
    __ Ret();
    __ Bind(&miss);
  }

  // Get the prototype of the function.
  Register prototype = x13;
  __ TryGetFunctionPrototype(function, prototype, scratch2, &slow,
                             MacroAssembler::kMissOnBoundFunction);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(prototype, &slow);
  __ IsObjectJSObjectType(prototype, scratch1, scratch2, &slow);

  // Update the global instanceof or call site inlined cache with the current
  // map and function. The cached answer will be set when it is known below.
  if (HasCallSiteInlineCheck()) {
    // Patch the (relocated) inlined map check.
    __ GetRelocatedValueLocation(map_check_site, scratch1);
    // We have a cell, so need another level of dereferencing.
    __ Ldr(scratch1, MemOperand(scratch1));
    __ Str(map, FieldMemOperand(scratch1, Cell::kValueOffset));
  } else {
    __ StoreRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
    __ StoreRoot(map, Heap::kInstanceofCacheMapRootIndex);
  }

  Label return_true, return_result;
  Register smi_value = scratch1;
  {
    // Loop through the prototype chain looking for the function prototype.
    Register chain_map = x1;
    Register chain_prototype = x14;
    Register null_value = x15;
    Label loop;
    __ Ldr(chain_prototype, FieldMemOperand(map, Map::kPrototypeOffset));
    __ LoadRoot(null_value, Heap::kNullValueRootIndex);
    // Speculatively set a result.
    __ Mov(result, res_false);
    if (!HasCallSiteInlineCheck() && ReturnTrueFalseObject()) {
      // Value to store in the cache cannot be an object.
      __ Mov(smi_value, Smi::FromInt(1));
    }

    __ Bind(&loop);

    // If the chain prototype is the object prototype, return true.
    __ Cmp(chain_prototype, prototype);
    __ B(eq, &return_true);

    // If the chain prototype is null, we've reached the end of the chain, so
    // return false.
    __ Cmp(chain_prototype, null_value);
    __ B(eq, &return_result);

    // Otherwise, load the next prototype in the chain, and loop.
    __ Ldr(chain_map, FieldMemOperand(chain_prototype, HeapObject::kMapOffset));
    __ Ldr(chain_prototype, FieldMemOperand(chain_map, Map::kPrototypeOffset));
    __ B(&loop);
  }

  // Return sequence when no arguments are on the stack.
  // We cannot fall through to here.
  __ Bind(&return_true);
  __ Mov(result, res_true);
  if (!HasCallSiteInlineCheck() && ReturnTrueFalseObject()) {
    // Value to store in the cache cannot be an object.
    __ Mov(smi_value, Smi::FromInt(0));
  }
  __ Bind(&return_result);
  if (HasCallSiteInlineCheck()) {
    DCHECK(ReturnTrueFalseObject());
    __ Add(map_check_site, map_check_site, kDeltaToLoadBoolResult);
    __ GetRelocatedValueLocation(map_check_site, scratch2);
    __ Str(result, MemOperand(scratch2));
  } else {
    Register cached_value = ReturnTrueFalseObject() ? smi_value : result;
    __ StoreRoot(cached_value, Heap::kInstanceofCacheAnswerRootIndex);
  }
  __ Ret();

  Label object_not_null, object_not_null_or_smi;

  __ Bind(&not_js_object);
  Register object_type = x14;
  //   x0   result        result return register (uninit)
  //   x10  function      pointer to function
  //   x11  object        pointer to object
  //   x14  object_type   type of object (uninit)

  // Before null, smi and string checks, check that the rhs is a function.
  // For a non-function rhs, an exception must be thrown.
  __ JumpIfSmi(function, &slow);
  __ JumpIfNotObjectType(
      function, scratch1, object_type, JS_FUNCTION_TYPE, &slow);

  __ Mov(result, res_false);

  // Null is not instance of anything.
  __ Cmp(object_type, Operand(isolate()->factory()->null_value()));
  __ B(ne, &object_not_null);
  __ Ret();

  __ Bind(&object_not_null);
  // Smi values are not instances of anything.
  __ JumpIfNotSmi(object, &object_not_null_or_smi);
  __ Ret();

  __ Bind(&object_not_null_or_smi);
  // String values are not instances of anything.
  __ IsObjectJSStringType(object, scratch2, &slow);
  __ Ret();

  // Slow-case. Tail call builtin.
  __ Bind(&slow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Arguments have either been passed into registers or have been previously
    // popped. We need to push them before calling builtin.
    __ Push(object, function);
    __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_FUNCTION);
  }
  if (ReturnTrueFalseObject()) {
    // Reload true/false because they were clobbered in the builtin call.
    __ LoadTrueFalseRoots(res_true, res_false);
    __ Cmp(result, 0);
    __ Csel(result, res_true, res_false, eq);
  }
  __ Ret();
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  Register arg_count = ArgumentsAccessReadDescriptor::parameter_count();
  Register key = ArgumentsAccessReadDescriptor::index();
  DCHECK(arg_count.is(x0));
  DCHECK(key.is(x1));

  // The displacement is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement =
      StandardFrameConstants::kCallerSPOffset - kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(key, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Register local_fp = x11;
  Register caller_fp = x11;
  Register caller_ctx = x12;
  Label skip_adaptor;
  __ Ldr(caller_fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(caller_ctx, MemOperand(caller_fp,
                                StandardFrameConstants::kContextOffset));
  __ Cmp(caller_ctx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ Csel(local_fp, fp, caller_fp, ne);
  __ B(ne, &skip_adaptor);

  // Load the actual arguments limit found in the arguments adaptor frame.
  __ Ldr(arg_count, MemOperand(caller_fp,
                               ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Bind(&skip_adaptor);

  // Check index against formal parameters count limit. Use unsigned comparison
  // to get negative check for free: branch if key < 0 or key >= arg_count.
  __ Cmp(key, arg_count);
  __ B(hs, &slow);

  // Read the argument from the stack and return it.
  __ Sub(x10, arg_count, key);
  __ Add(x10, local_fp, Operand::UntagSmiAndScale(x10, kPointerSizeLog2));
  __ Ldr(x0, MemOperand(x10, kDisplacement));
  __ Ret();

  // Slow case: handle non-smi or out-of-bounds access to arguments by calling
  // the runtime system.
  __ Bind(&slow);
  __ Push(key);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewSloppySlow(MacroAssembler* masm) {
  // Stack layout on entry.
  //  jssp[0]:  number of parameters (tagged)
  //  jssp[8]:  address of receiver argument
  //  jssp[16]: function

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  Register caller_fp = x10;
  __ Ldr(caller_fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  // Load and untag the context.
  __ Ldr(w11, UntagSmiMemOperand(caller_fp,
                                 StandardFrameConstants::kContextOffset));
  __ Cmp(w11, StackFrame::ARGUMENTS_ADAPTOR);
  __ B(ne, &runtime);

  // Patch the arguments.length and parameters pointer in the current frame.
  __ Ldr(x11, MemOperand(caller_fp,
                         ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Poke(x11, 0 * kXRegSize);
  __ Add(x10, caller_fp, Operand::UntagSmiAndScale(x11, kPointerSizeLog2));
  __ Add(x10, x10, StandardFrameConstants::kCallerSPOffset);
  __ Poke(x10, 1 * kXRegSize);

  __ Bind(&runtime);
  __ TailCallRuntime(Runtime::kNewSloppyArguments, 3, 1);
}


void ArgumentsAccessStub::GenerateNewSloppyFast(MacroAssembler* masm) {
  // Stack layout on entry.
  //  jssp[0]:  number of parameters (tagged)
  //  jssp[8]:  address of receiver argument
  //  jssp[16]: function
  //
  // Returns pointer to result object in x0.

  // Note: arg_count_smi is an alias of param_count_smi.
  Register arg_count_smi = x3;
  Register param_count_smi = x3;
  Register param_count = x7;
  Register recv_arg = x14;
  Register function = x4;
  __ Pop(param_count_smi, recv_arg, function);
  __ SmiUntag(param_count, param_count_smi);

  // Check if the calling frame is an arguments adaptor frame.
  Register caller_fp = x11;
  Register caller_ctx = x12;
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ Ldr(caller_fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(caller_ctx, MemOperand(caller_fp,
                                StandardFrameConstants::kContextOffset));
  __ Cmp(caller_ctx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ B(eq, &adaptor_frame);

  // No adaptor, parameter count = argument count.

  //   x1   mapped_params number of mapped params, min(params, args) (uninit)
  //   x2   arg_count     number of function arguments (uninit)
  //   x3   arg_count_smi number of function arguments (smi)
  //   x4   function      function pointer
  //   x7   param_count   number of function parameters
  //   x11  caller_fp     caller's frame pointer
  //   x14  recv_arg      pointer to receiver arguments

  Register arg_count = x2;
  __ Mov(arg_count, param_count);
  __ B(&try_allocate);

  // We have an adaptor frame. Patch the parameters pointer.
  __ Bind(&adaptor_frame);
  __ Ldr(arg_count_smi,
         MemOperand(caller_fp,
                    ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(arg_count, arg_count_smi);
  __ Add(x10, caller_fp, Operand(arg_count, LSL, kPointerSizeLog2));
  __ Add(recv_arg, x10, StandardFrameConstants::kCallerSPOffset);

  // Compute the mapped parameter count = min(param_count, arg_count)
  Register mapped_params = x1;
  __ Cmp(param_count, arg_count);
  __ Csel(mapped_params, param_count, arg_count, lt);

  __ Bind(&try_allocate);

  //   x0   alloc_obj     pointer to allocated objects: param map, backing
  //                      store, arguments (uninit)
  //   x1   mapped_params number of mapped parameters, min(params, args)
  //   x2   arg_count     number of function arguments
  //   x3   arg_count_smi number of function arguments (smi)
  //   x4   function      function pointer
  //   x7   param_count   number of function parameters
  //   x10  size          size of objects to allocate (uninit)
  //   x14  recv_arg      pointer to receiver arguments

  // Compute the size of backing store, parameter map, and arguments object.
  // 1. Parameter map, has two extra words containing context and backing
  // store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;

  // Calculate the parameter map size, assuming it exists.
  Register size = x10;
  __ Mov(size, Operand(mapped_params, LSL, kPointerSizeLog2));
  __ Add(size, size, kParameterMapHeaderSize);

  // If there are no mapped parameters, set the running size total to zero.
  // Otherwise, use the parameter map size calculated earlier.
  __ Cmp(mapped_params, 0);
  __ CzeroX(size, eq);

  // 2. Add the size of the backing store and arguments object.
  __ Add(size, size, Operand(arg_count, LSL, kPointerSizeLog2));
  __ Add(size, size,
         FixedArray::kHeaderSize + Heap::kSloppyArgumentsObjectSize);

  // Do the allocation of all three objects in one go. Assign this to x0, as it
  // will be returned to the caller.
  Register alloc_obj = x0;
  __ Allocate(size, alloc_obj, x11, x12, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current (global) context.

  //   x0   alloc_obj       pointer to allocated objects (param map, backing
  //                        store, arguments)
  //   x1   mapped_params   number of mapped parameters, min(params, args)
  //   x2   arg_count       number of function arguments
  //   x3   arg_count_smi   number of function arguments (smi)
  //   x4   function        function pointer
  //   x7   param_count     number of function parameters
  //   x11  sloppy_args_map offset to args (or aliased args) map (uninit)
  //   x14  recv_arg        pointer to receiver arguments

  Register global_object = x10;
  Register global_ctx = x10;
  Register sloppy_args_map = x11;
  Register aliased_args_map = x10;
  __ Ldr(global_object, GlobalObjectMemOperand());
  __ Ldr(global_ctx, FieldMemOperand(global_object,
                                     GlobalObject::kNativeContextOffset));

  __ Ldr(sloppy_args_map,
         ContextMemOperand(global_ctx, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
  __ Ldr(aliased_args_map,
         ContextMemOperand(global_ctx, Context::ALIASED_ARGUMENTS_MAP_INDEX));
  __ Cmp(mapped_params, 0);
  __ CmovX(sloppy_args_map, aliased_args_map, ne);

  // Copy the JS object part.
  __ Str(sloppy_args_map, FieldMemOperand(alloc_obj, JSObject::kMapOffset));
  __ LoadRoot(x10, Heap::kEmptyFixedArrayRootIndex);
  __ Str(x10, FieldMemOperand(alloc_obj, JSObject::kPropertiesOffset));
  __ Str(x10, FieldMemOperand(alloc_obj, JSObject::kElementsOffset));

  // Set up the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  const int kCalleeOffset = JSObject::kHeaderSize +
                            Heap::kArgumentsCalleeIndex * kPointerSize;
  __ AssertNotSmi(function);
  __ Str(function, FieldMemOperand(alloc_obj, kCalleeOffset));

  // Use the length and set that as an in-object property.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  const int kLengthOffset = JSObject::kHeaderSize +
                            Heap::kArgumentsLengthIndex * kPointerSize;
  __ Str(arg_count_smi, FieldMemOperand(alloc_obj, kLengthOffset));

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, "elements" will point there, otherwise
  // it will point to the backing store.

  //   x0   alloc_obj     pointer to allocated objects (param map, backing
  //                      store, arguments)
  //   x1   mapped_params number of mapped parameters, min(params, args)
  //   x2   arg_count     number of function arguments
  //   x3   arg_count_smi number of function arguments (smi)
  //   x4   function      function pointer
  //   x5   elements      pointer to parameter map or backing store (uninit)
  //   x6   backing_store pointer to backing store (uninit)
  //   x7   param_count   number of function parameters
  //   x14  recv_arg      pointer to receiver arguments

  Register elements = x5;
  __ Add(elements, alloc_obj, Heap::kSloppyArgumentsObjectSize);
  __ Str(elements, FieldMemOperand(alloc_obj, JSObject::kElementsOffset));

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ Cmp(mapped_params, 0);
  // Set up backing store address, because it is needed later for filling in
  // the unmapped arguments.
  Register backing_store = x6;
  __ CmovX(backing_store, elements, eq);
  __ B(eq, &skip_parameter_map);

  __ LoadRoot(x10, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ Str(x10, FieldMemOperand(elements, FixedArray::kMapOffset));
  __ Add(x10, mapped_params, 2);
  __ SmiTag(x10);
  __ Str(x10, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ Str(cp, FieldMemOperand(elements,
                             FixedArray::kHeaderSize + 0 * kPointerSize));
  __ Add(x10, elements, Operand(mapped_params, LSL, kPointerSizeLog2));
  __ Add(x10, x10, kParameterMapHeaderSize);
  __ Str(x10, FieldMemOperand(elements,
                              FixedArray::kHeaderSize + 1 * kPointerSize));

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. Then index the context,
  // where parameters are stored in reverse order, at:
  //
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS + parameter_count - 1
  //
  // The mapped parameter thus needs to get indices:
  //
  //   MIN_CONTEXT_SLOTS + parameter_count - 1 ..
  //     MIN_CONTEXT_SLOTS + parameter_count - mapped_parameter_count
  //
  // We loop from right to left.

  //   x0   alloc_obj     pointer to allocated objects (param map, backing
  //                      store, arguments)
  //   x1   mapped_params number of mapped parameters, min(params, args)
  //   x2   arg_count     number of function arguments
  //   x3   arg_count_smi number of function arguments (smi)
  //   x4   function      function pointer
  //   x5   elements      pointer to parameter map or backing store (uninit)
  //   x6   backing_store pointer to backing store (uninit)
  //   x7   param_count   number of function parameters
  //   x11  loop_count    parameter loop counter (uninit)
  //   x12  index         parameter index (smi, uninit)
  //   x13  the_hole      hole value (uninit)
  //   x14  recv_arg      pointer to receiver arguments

  Register loop_count = x11;
  Register index = x12;
  Register the_hole = x13;
  Label parameters_loop, parameters_test;
  __ Mov(loop_count, mapped_params);
  __ Add(index, param_count, static_cast<int>(Context::MIN_CONTEXT_SLOTS));
  __ Sub(index, index, mapped_params);
  __ SmiTag(index);
  __ LoadRoot(the_hole, Heap::kTheHoleValueRootIndex);
  __ Add(backing_store, elements, Operand(loop_count, LSL, kPointerSizeLog2));
  __ Add(backing_store, backing_store, kParameterMapHeaderSize);

  __ B(&parameters_test);

  __ Bind(&parameters_loop);
  __ Sub(loop_count, loop_count, 1);
  __ Mov(x10, Operand(loop_count, LSL, kPointerSizeLog2));
  __ Add(x10, x10, kParameterMapHeaderSize - kHeapObjectTag);
  __ Str(index, MemOperand(elements, x10));
  __ Sub(x10, x10, kParameterMapHeaderSize - FixedArray::kHeaderSize);
  __ Str(the_hole, MemOperand(backing_store, x10));
  __ Add(index, index, Smi::FromInt(1));
  __ Bind(&parameters_test);
  __ Cbnz(loop_count, &parameters_loop);

  __ Bind(&skip_parameter_map);
  // Copy arguments header and remaining slots (if there are any.)
  __ LoadRoot(x10, Heap::kFixedArrayMapRootIndex);
  __ Str(x10, FieldMemOperand(backing_store, FixedArray::kMapOffset));
  __ Str(arg_count_smi, FieldMemOperand(backing_store,
                                        FixedArray::kLengthOffset));

  //   x0   alloc_obj     pointer to allocated objects (param map, backing
  //                      store, arguments)
  //   x1   mapped_params number of mapped parameters, min(params, args)
  //   x2   arg_count     number of function arguments
  //   x4   function      function pointer
  //   x3   arg_count_smi number of function arguments (smi)
  //   x6   backing_store pointer to backing store (uninit)
  //   x14  recv_arg      pointer to receiver arguments

  Label arguments_loop, arguments_test;
  __ Mov(x10, mapped_params);
  __ Sub(recv_arg, recv_arg, Operand(x10, LSL, kPointerSizeLog2));
  __ B(&arguments_test);

  __ Bind(&arguments_loop);
  __ Sub(recv_arg, recv_arg, kPointerSize);
  __ Ldr(x11, MemOperand(recv_arg));
  __ Add(x12, backing_store, Operand(x10, LSL, kPointerSizeLog2));
  __ Str(x11, FieldMemOperand(x12, FixedArray::kHeaderSize));
  __ Add(x10, x10, 1);

  __ Bind(&arguments_test);
  __ Cmp(x10, arg_count);
  __ B(lt, &arguments_loop);

  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ Bind(&runtime);
  __ Push(function, recv_arg, arg_count_smi);
  __ TailCallRuntime(Runtime::kNewSloppyArguments, 3, 1);
}


void LoadIndexedInterceptorStub::Generate(MacroAssembler* masm) {
  // Return address is in lr.
  Label slow;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();

  // Check that the key is an array index, that is Uint32.
  __ TestAndBranchIfAnySet(key, kSmiTagMask | kSmiSignMask, &slow);

  // Everything is fine, call runtime.
  __ Push(receiver, key);
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kLoadElementWithInterceptor),
                        masm->isolate()),
      2, 1);

  __ Bind(&slow);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::KEYED_LOAD_IC));
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // Stack layout on entry.
  //  jssp[0]:  number of parameters (tagged)
  //  jssp[8]:  address of receiver argument
  //  jssp[16]: function
  //
  // Returns pointer to result object in x0.

  // Get the stub arguments from the frame, and make an untagged copy of the
  // parameter count.
  Register param_count_smi = x1;
  Register params = x2;
  Register function = x3;
  Register param_count = x13;
  __ Pop(param_count_smi, params, function);
  __ SmiUntag(param_count, param_count_smi);

  // Test if arguments adaptor needed.
  Register caller_fp = x11;
  Register caller_ctx = x12;
  Label try_allocate, runtime;
  __ Ldr(caller_fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(caller_ctx, MemOperand(caller_fp,
                                StandardFrameConstants::kContextOffset));
  __ Cmp(caller_ctx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ B(ne, &try_allocate);

  //   x1   param_count_smi   number of parameters passed to function (smi)
  //   x2   params            pointer to parameters
  //   x3   function          function pointer
  //   x11  caller_fp         caller's frame pointer
  //   x13  param_count       number of parameters passed to function

  // Patch the argument length and parameters pointer.
  __ Ldr(param_count_smi,
         MemOperand(caller_fp,
                    ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(param_count, param_count_smi);
  __ Add(x10, caller_fp, Operand(param_count, LSL, kPointerSizeLog2));
  __ Add(params, x10, StandardFrameConstants::kCallerSPOffset);

  // Try the new space allocation. Start out with computing the size of the
  // arguments object and the elements array in words.
  Register size = x10;
  __ Bind(&try_allocate);
  __ Add(size, param_count, FixedArray::kHeaderSize / kPointerSize);
  __ Cmp(param_count, 0);
  __ CzeroX(size, eq);
  __ Add(size, size, Heap::kStrictArgumentsObjectSize / kPointerSize);

  // Do the allocation of both objects in one go. Assign this to x0, as it will
  // be returned to the caller.
  Register alloc_obj = x0;
  __ Allocate(size, alloc_obj, x11, x12, &runtime,
              static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));

  // Get the arguments boilerplate from the current (native) context.
  Register global_object = x10;
  Register global_ctx = x10;
  Register strict_args_map = x4;
  __ Ldr(global_object, GlobalObjectMemOperand());
  __ Ldr(global_ctx, FieldMemOperand(global_object,
                                     GlobalObject::kNativeContextOffset));
  __ Ldr(strict_args_map,
         ContextMemOperand(global_ctx, Context::STRICT_ARGUMENTS_MAP_INDEX));

  //   x0   alloc_obj         pointer to allocated objects: parameter array and
  //                          arguments object
  //   x1   param_count_smi   number of parameters passed to function (smi)
  //   x2   params            pointer to parameters
  //   x3   function          function pointer
  //   x4   strict_args_map   offset to arguments map
  //   x13  param_count       number of parameters passed to function
  __ Str(strict_args_map, FieldMemOperand(alloc_obj, JSObject::kMapOffset));
  __ LoadRoot(x5, Heap::kEmptyFixedArrayRootIndex);
  __ Str(x5, FieldMemOperand(alloc_obj, JSObject::kPropertiesOffset));
  __ Str(x5, FieldMemOperand(alloc_obj, JSObject::kElementsOffset));

  // Set the smi-tagged length as an in-object property.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  const int kLengthOffset = JSObject::kHeaderSize +
                            Heap::kArgumentsLengthIndex * kPointerSize;
  __ Str(param_count_smi, FieldMemOperand(alloc_obj, kLengthOffset));

  // If there are no actual arguments, we're done.
  Label done;
  __ Cbz(param_count, &done);

  // Set up the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  Register elements = x5;
  __ Add(elements, alloc_obj, Heap::kStrictArgumentsObjectSize);
  __ Str(elements, FieldMemOperand(alloc_obj, JSObject::kElementsOffset));
  __ LoadRoot(x10, Heap::kFixedArrayMapRootIndex);
  __ Str(x10, FieldMemOperand(elements, FixedArray::kMapOffset));
  __ Str(param_count_smi, FieldMemOperand(elements, FixedArray::kLengthOffset));

  //   x0   alloc_obj         pointer to allocated objects: parameter array and
  //                          arguments object
  //   x1   param_count_smi   number of parameters passed to function (smi)
  //   x2   params            pointer to parameters
  //   x3   function          function pointer
  //   x4   array             pointer to array slot (uninit)
  //   x5   elements          pointer to elements array of alloc_obj
  //   x13  param_count       number of parameters passed to function

  // Copy the fixed array slots.
  Label loop;
  Register array = x4;
  // Set up pointer to first array slot.
  __ Add(array, elements, FixedArray::kHeaderSize - kHeapObjectTag);

  __ Bind(&loop);
  // Pre-decrement the parameters pointer by kPointerSize on each iteration.
  // Pre-decrement in order to skip receiver.
  __ Ldr(x10, MemOperand(params, -kPointerSize, PreIndex));
  // Post-increment elements by kPointerSize on each iteration.
  __ Str(x10, MemOperand(array, kPointerSize, PostIndex));
  __ Sub(param_count, param_count, 1);
  __ Cbnz(param_count, &loop);

  // Return from stub.
  __ Bind(&done);
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ Bind(&runtime);
  __ Push(function, params, param_count_smi);
  __ TailCallRuntime(Runtime::kNewStrictArguments, 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExecRT, 4, 1);
#else  // V8_INTERPRETED_REGEXP

  // Stack frame on entry.
  //  jssp[0]: last_match_info (expected JSArray)
  //  jssp[8]: previous index
  //  jssp[16]: subject string
  //  jssp[24]: JSRegExp object
  Label runtime;

  // Use of registers for this function.

  // Variable registers:
  //   x10-x13                                  used as scratch registers
  //   w0       string_type                     type of subject string
  //   x2       jsstring_length                 subject string length
  //   x3       jsregexp_object                 JSRegExp object
  //   w4       string_encoding                 Latin1 or UC16
  //   w5       sliced_string_offset            if the string is a SlicedString
  //                                            offset to the underlying string
  //   w6       string_representation           groups attributes of the string:
  //                                              - is a string
  //                                              - type of the string
  //                                              - is a short external string
  Register string_type = w0;
  Register jsstring_length = x2;
  Register jsregexp_object = x3;
  Register string_encoding = w4;
  Register sliced_string_offset = w5;
  Register string_representation = w6;

  // These are in callee save registers and will be preserved by the call
  // to the native RegExp code, as this code is called using the normal
  // C calling convention. When calling directly from generated code the
  // native RegExp code will not do a GC and therefore the content of
  // these registers are safe to use after the call.

  //   x19       subject                        subject string
  //   x20       regexp_data                    RegExp data (FixedArray)
  //   x21       last_match_info_elements       info relative to the last match
  //                                            (FixedArray)
  //   x22       code_object                    generated regexp code
  Register subject = x19;
  Register regexp_data = x20;
  Register last_match_info_elements = x21;
  Register code_object = x22;

  // TODO(jbramley): Is it necessary to preserve these? I don't think ARM does.
  CPURegList used_callee_saved_registers(subject,
                                         regexp_data,
                                         last_match_info_elements,
                                         code_object);
  __ PushCPURegList(used_callee_saved_registers);

  // Stack frame.
  //  jssp[0] : x19
  //  jssp[8] : x20
  //  jssp[16]: x21
  //  jssp[24]: x22
  //  jssp[32]: last_match_info (JSArray)
  //  jssp[40]: previous index
  //  jssp[48]: subject string
  //  jssp[56]: JSRegExp object

  const int kLastMatchInfoOffset = 4 * kPointerSize;
  const int kPreviousIndexOffset = 5 * kPointerSize;
  const int kSubjectOffset = 6 * kPointerSize;
  const int kJSRegExpOffset = 7 * kPointerSize;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ Mov(x10, address_of_regexp_stack_memory_size);
  __ Ldr(x10, MemOperand(x10));
  __ Cbz(x10, &runtime);

  // Check that the first argument is a JSRegExp object.
  DCHECK(jssp.Is(__ StackPointer()));
  __ Peek(jsregexp_object, kJSRegExpOffset);
  __ JumpIfSmi(jsregexp_object, &runtime);
  __ JumpIfNotObjectType(jsregexp_object, x10, x10, JS_REGEXP_TYPE, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ Ldr(regexp_data, FieldMemOperand(jsregexp_object, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    STATIC_ASSERT(kSmiTag == 0);
    __ Tst(regexp_data, kSmiTagMask);
    __ Check(ne, kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CompareObjectType(regexp_data, x10, x10, FIXED_ARRAY_TYPE);
    __ Check(eq, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ Ldr(x10, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ Cmp(x10, Smi::FromInt(JSRegExp::IRREGEXP));
  __ B(ne, &runtime);

  // Check that the number of captures fit in the static offsets vector buffer.
  // We have always at least one capture for the whole match, plus additional
  // ones due to capturing parentheses. A capture takes 2 registers.
  // The number of capture registers then is (number_of_captures + 1) * 2.
  __ Ldrsw(x10,
           UntagSmiFieldMemOperand(regexp_data,
                                   JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  //             number_of_captures * 2 <= offsets vector size - 2
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ Add(x10, x10, x10);
  __ Cmp(x10, Isolate::kJSRegexpStaticOffsetsVectorSize - 2);
  __ B(hi, &runtime);

  // Initialize offset for possibly sliced string.
  __ Mov(sliced_string_offset, 0);

  DCHECK(jssp.Is(__ StackPointer()));
  __ Peek(subject, kSubjectOffset);
  __ JumpIfSmi(subject, &runtime);

  __ Ldr(x10, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ Ldrb(string_type, FieldMemOperand(x10, Map::kInstanceTypeOffset));

  __ Ldr(jsstring_length, FieldMemOperand(subject, String::kLengthOffset));

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

  Label check_underlying;   // (4)
  Label seq_string;         // (5)
  Label not_seq_nor_cons;   // (6)
  Label external_string;    // (7)
  Label not_long_external;  // (8)

  // (1) Sequential string?  If yes, go to (5).
  __ And(string_representation,
         string_type,
         kIsNotStringMask |
             kStringRepresentationMask |
             kShortExternalStringMask);
  // We depend on the fact that Strings of type
  // SeqString and not ShortExternalString are defined
  // by the following pattern:
  //   string_type: 0XX0 XX00
  //                ^  ^   ^^
  //                |  |   ||
  //                |  |   is a SeqString
  //                |  is not a short external String
  //                is a String
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ Cbz(string_representation, &seq_string);  // Go to (5).

  // (2) Anything but sequential or cons?  If yes, go to (6).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  __ Cmp(string_representation, kExternalStringTag);
  __ B(ge, &not_seq_nor_cons);  // Go to (6).

  // (3) Cons string.  Check that it's flat.
  __ Ldr(x10, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ JumpIfNotRoot(x10, Heap::kempty_stringRootIndex, &runtime);
  // Replace subject with first string.
  __ Ldr(subject, FieldMemOperand(subject, ConsString::kFirstOffset));

  // (4) Is subject external?  If yes, go to (7).
  __ Bind(&check_underlying);
  // Reload the string type.
  __ Ldr(x10, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ Ldrb(string_type, FieldMemOperand(x10, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSeqStringTag == 0);
  // The underlying external string is never a short external string.
  STATIC_ASSERT(ExternalString::kMaxShortLength < ConsString::kMinLength);
  STATIC_ASSERT(ExternalString::kMaxShortLength < SlicedString::kMinLength);
  __ TestAndBranchIfAnySet(string_type.X(),
                           kStringRepresentationMask,
                           &external_string);  // Go to (7).

  // (5) Sequential string.  Load regexp code according to encoding.
  __ Bind(&seq_string);

  // Check that the third argument is a positive smi less than the subject
  // string length. A negative value will be greater (unsigned comparison).
  DCHECK(jssp.Is(__ StackPointer()));
  __ Peek(x10, kPreviousIndexOffset);
  __ JumpIfNotSmi(x10, &runtime);
  __ Cmp(jsstring_length, x10);
  __ B(ls, &runtime);

  // Argument 2 (x1): We need to load argument 2 (the previous index) into x1
  // before entering the exit frame.
  __ SmiUntag(x1, x10);

  // The third bit determines the string encoding in string_type.
  STATIC_ASSERT(kOneByteStringTag == 0x04);
  STATIC_ASSERT(kTwoByteStringTag == 0x00);
  STATIC_ASSERT(kStringEncodingMask == 0x04);

  // Find the code object based on the assumptions above.
  // kDataOneByteCodeOffset and kDataUC16CodeOffset are adjacent, adds an offset
  // of kPointerSize to reach the latter.
  DCHECK_EQ(JSRegExp::kDataOneByteCodeOffset + kPointerSize,
            JSRegExp::kDataUC16CodeOffset);
  __ Mov(x10, kPointerSize);
  // We will need the encoding later: Latin1 = 0x04
  //                                  UC16   = 0x00
  __ Ands(string_encoding, string_type, kStringEncodingMask);
  __ CzeroX(x10, ne);
  __ Add(x10, regexp_data, x10);
  __ Ldr(code_object, FieldMemOperand(x10, JSRegExp::kDataOneByteCodeOffset));

  // (E) Carry on.  String handling is done.

  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(code_object, &runtime);

  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(isolate()->counters()->regexp_entry_native(), 1,
                      x10,
                      x11);

  // Isolates: note we add an additional parameter here (isolate pointer).
  __ EnterExitFrame(false, x10, 1);
  DCHECK(csp.Is(__ StackPointer()));

  // We have 9 arguments to pass to the regexp code, therefore we have to pass
  // one on the stack and the rest as registers.

  // Note that the placement of the argument on the stack isn't standard
  // AAPCS64:
  // csp[0]: Space for the return address placed by DirectCEntryStub.
  // csp[8]: Argument 9, the current isolate address.

  __ Mov(x10, ExternalReference::isolate_address(isolate()));
  __ Poke(x10, kPointerSize);

  Register length = w11;
  Register previous_index_in_bytes = w12;
  Register start = x13;

  // Load start of the subject string.
  __ Add(start, subject, SeqString::kHeaderSize - kHeapObjectTag);
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to two pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and decrements sp by 2 * kPointerSize.)
  __ Ldr(subject, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));
  __ Ldr(length, UntagSmiFieldMemOperand(subject, String::kLengthOffset));

  // Handle UC16 encoding, two bytes make one character.
  //   string_encoding: if Latin1: 0x04
  //                    if UC16:   0x00
  STATIC_ASSERT(kStringEncodingMask == 0x04);
  __ Ubfx(string_encoding, string_encoding, 2, 1);
  __ Eor(string_encoding, string_encoding, 1);
  //   string_encoding: if Latin1: 0
  //                    if UC16:   1

  // Convert string positions from characters to bytes.
  // Previous index is in x1.
  __ Lsl(previous_index_in_bytes, w1, string_encoding);
  __ Lsl(length, length, string_encoding);
  __ Lsl(sliced_string_offset, sliced_string_offset, string_encoding);

  // Argument 1 (x0): Subject string.
  __ Mov(x0, subject);

  // Argument 2 (x1): Previous index, already there.

  // Argument 3 (x2): Get the start of input.
  // Start of input = start of string + previous index + substring offset
  //                                                     (0 if the string
  //                                                      is not sliced).
  __ Add(w10, previous_index_in_bytes, sliced_string_offset);
  __ Add(x2, start, Operand(w10, UXTW));

  // Argument 4 (x3):
  // End of input = start of input + (length of input - previous index)
  __ Sub(w10, length, previous_index_in_bytes);
  __ Add(x3, x2, Operand(w10, UXTW));

  // Argument 5 (x4): static offsets vector buffer.
  __ Mov(x4, ExternalReference::address_of_static_offsets_vector(isolate()));

  // Argument 6 (x5): Set the number of capture registers to zero to force
  // global regexps to behave as non-global. This stub is not used for global
  // regexps.
  __ Mov(x5, 0);

  // Argument 7 (x6): Start (high end) of backtracking stack memory area.
  __ Mov(x10, address_of_regexp_stack_memory_address);
  __ Ldr(x10, MemOperand(x10));
  __ Mov(x11, address_of_regexp_stack_memory_size);
  __ Ldr(x11, MemOperand(x11));
  __ Add(x6, x10, x11);

  // Argument 8 (x7): Indicate that this is a direct call from JavaScript.
  __ Mov(x7, 1);

  // Locate the code entry and call it.
  __ Add(code_object, code_object, Code::kHeaderSize - kHeapObjectTag);
  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm, code_object);

  __ LeaveExitFrame(false, x10, true);

  // The generated regexp code returns an int32 in w0.
  Label failure, exception;
  __ CompareAndBranch(w0, NativeRegExpMacroAssembler::FAILURE, eq, &failure);
  __ CompareAndBranch(w0,
                      NativeRegExpMacroAssembler::EXCEPTION,
                      eq,
                      &exception);
  __ CompareAndBranch(w0, NativeRegExpMacroAssembler::RETRY, eq, &runtime);

  // Success: process the result from the native regexp code.
  Register number_of_capture_registers = x12;

  // Calculate number of capture registers (number_of_captures + 1) * 2
  // and store it in the last match info.
  __ Ldrsw(x10,
           UntagSmiFieldMemOperand(regexp_data,
                                   JSRegExp::kIrregexpCaptureCountOffset));
  __ Add(x10, x10, x10);
  __ Add(number_of_capture_registers, x10, 2);

  // Check that the fourth object is a JSArray object.
  DCHECK(jssp.Is(__ StackPointer()));
  __ Peek(x10, kLastMatchInfoOffset);
  __ JumpIfSmi(x10, &runtime);
  __ JumpIfNotObjectType(x10, x11, x11, JS_ARRAY_TYPE, &runtime);

  // Check that the JSArray is the fast case.
  __ Ldr(last_match_info_elements,
         FieldMemOperand(x10, JSArray::kElementsOffset));
  __ Ldr(x10,
         FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ JumpIfNotRoot(x10, Heap::kFixedArrayMapRootIndex, &runtime);

  // Check that the last match info has space for the capture registers and the
  // additional information (overhead).
  //     (number_of_captures + 1) * 2 + overhead <= last match info size
  //     (number_of_captures * 2) + 2 + overhead <= last match info size
  //      number_of_capture_registers + overhead <= last match info size
  __ Ldrsw(x10,
           UntagSmiFieldMemOperand(last_match_info_elements,
                                   FixedArray::kLengthOffset));
  __ Add(x11, number_of_capture_registers, RegExpImpl::kLastMatchOverhead);
  __ Cmp(x11, x10);
  __ B(gt, &runtime);

  // Store the capture count.
  __ SmiTag(x10, number_of_capture_registers);
  __ Str(x10,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ Str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  // Use x10 as the subject string in order to only need
  // one RecordWriteStub.
  __ Mov(x10, subject);
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastSubjectOffset,
                      x10,
                      x11,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs);
  __ Str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ Mov(x10, subject);
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastInputOffset,
                      x10,
                      x11,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs);

  Register last_match_offsets = x13;
  Register offsets_vector_index = x14;
  Register current_offset = x15;

  // Get the static offsets vector filled by the native regexp code
  // and fill the last match info.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ Mov(offsets_vector_index, address_of_static_offsets_vector);

  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // iterates down to zero (inclusive).
  __ Add(last_match_offsets,
         last_match_info_elements,
         RegExpImpl::kFirstCaptureOffset - kHeapObjectTag);
  __ Bind(&next_capture);
  __ Subs(number_of_capture_registers, number_of_capture_registers, 2);
  __ B(mi, &done);
  // Read two 32 bit values from the static offsets vector buffer into
  // an X register
  __ Ldr(current_offset,
         MemOperand(offsets_vector_index, kWRegSize * 2, PostIndex));
  // Store the smi values in the last match info.
  __ SmiTag(x10, current_offset);
  // Clearing the 32 bottom bits gives us a Smi.
  STATIC_ASSERT(kSmiTag == 0);
  __ Bic(x11, current_offset, kSmiShiftMask);
  __ Stp(x10,
         x11,
         MemOperand(last_match_offsets, kXRegSize * 2, PostIndex));
  __ B(&next_capture);
  __ Bind(&done);

  // Return last match info.
  __ Peek(x0, kLastMatchInfoOffset);
  __ PopCPURegList(used_callee_saved_registers);
  // Drop the 4 arguments of the stub from the stack.
  __ Drop(4);
  __ Ret();

  __ Bind(&exception);
  Register exception_value = x0;
  // A stack overflow (on the backtrack stack) may have occured
  // in the RegExp code but no exception has been created yet.
  // If there is no pending exception, handle that in the runtime system.
  __ Mov(x10, Operand(isolate()->factory()->the_hole_value()));
  __ Mov(x11,
         Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                   isolate())));
  __ Ldr(exception_value, MemOperand(x11));
  __ Cmp(x10, exception_value);
  __ B(eq, &runtime);

  __ Str(x10, MemOperand(x11));  // Clear pending exception.

  // Check if the exception is a termination. If so, throw as uncatchable.
  Label termination_exception;
  __ JumpIfRoot(exception_value,
                Heap::kTerminationExceptionRootIndex,
                &termination_exception);

  __ Throw(exception_value, x10, x11, x12, x13);

  __ Bind(&termination_exception);
  __ ThrowUncatchable(exception_value, x10, x11, x12, x13);

  __ Bind(&failure);
  __ Mov(x0, Operand(isolate()->factory()->null_value()));
  __ PopCPURegList(used_callee_saved_registers);
  // Drop the 4 arguments of the stub from the stack.
  __ Drop(4);
  __ Ret();

  __ Bind(&runtime);
  __ PopCPURegList(used_callee_saved_registers);
  __ TailCallRuntime(Runtime::kRegExpExecRT, 4, 1);

  // Deferred code for string handling.
  // (6) Not a long external string?  If yes, go to (8).
  __ Bind(&not_seq_nor_cons);
  // Compare flags are still set.
  __ B(ne, &not_long_external);  // Go to (8).

  // (7) External string. Make it, offset-wise, look like a sequential string.
  __ Bind(&external_string);
  if (masm->emit_debug_code()) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ Ldr(x10, FieldMemOperand(subject, HeapObject::kMapOffset));
    __ Ldrb(x10, FieldMemOperand(x10, Map::kInstanceTypeOffset));
    __ Tst(x10, kIsIndirectStringMask);
    __ Check(eq, kExternalStringExpectedButNotFound);
    __ And(x10, x10, kStringRepresentationMask);
    __ Cmp(x10, 0);
    __ Check(ne, kExternalStringExpectedButNotFound);
  }
  __ Ldr(subject,
         FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Sub(subject, subject, SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ B(&seq_string);    // Go to (5).

  // (8) If this is a short external string or not a string, bail out to
  // runtime.
  __ Bind(&not_long_external);
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ TestAndBranchIfAnySet(string_representation,
                           kShortExternalStringMask | kIsNotStringMask,
                           &runtime);

  // (9) Sliced string. Replace subject with parent.
  __ Ldr(sliced_string_offset,
         UntagSmiFieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ Ldr(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ B(&check_underlying);    // Go to (4).
#endif
}


static void GenerateRecordCallTarget(MacroAssembler* masm,
                                     Register argc,
                                     Register function,
                                     Register feedback_vector,
                                     Register index,
                                     Register scratch1,
                                     Register scratch2) {
  ASM_LOCATION("GenerateRecordCallTarget");
  DCHECK(!AreAliased(scratch1, scratch2,
                     argc, function, feedback_vector, index));
  // Cache the called function in a feedback vector slot. Cache states are
  // uninitialized, monomorphic (indicated by a JSFunction), and megamorphic.
  //  argc :            number of arguments to the construct function
  //  function :        the function to call
  //  feedback_vector : the feedback vector
  //  index :           slot in feedback vector (smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*TypeFeedbackVector::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*TypeFeedbackVector::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  // Load the cache state.
  __ Add(scratch1, feedback_vector,
         Operand::UntagSmiAndScale(index, kPointerSizeLog2));
  __ Ldr(scratch1, FieldMemOperand(scratch1, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ Cmp(scratch1, function);
  __ B(eq, &done);

  if (!FLAG_pretenuring_call_new) {
    // If we came here, we need to see if we are the array function.
    // If we didn't have a matching function, and we didn't find the megamorph
    // sentinel, then we have in the slot either some other function or an
    // AllocationSite. Do a map check on the object in scratch1 register.
    __ Ldr(scratch2, FieldMemOperand(scratch1, AllocationSite::kMapOffset));
    __ JumpIfNotRoot(scratch2, Heap::kAllocationSiteMapRootIndex, &miss);

    // Make sure the function is the Array() function
    __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, scratch1);
    __ Cmp(function, scratch1);
    __ B(ne, &megamorphic);
    __ B(&done);
  }

  __ Bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ JumpIfRoot(scratch1, Heap::kUninitializedSymbolRootIndex, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ Bind(&megamorphic);
  __ Add(scratch1, feedback_vector,
         Operand::UntagSmiAndScale(index, kPointerSizeLog2));
  __ LoadRoot(scratch2, Heap::kMegamorphicSymbolRootIndex);
  __ Str(scratch2, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  __ B(&done);

  // An uninitialized cache is patched with the function or sentinel to
  // indicate the ElementsKind if function is the Array constructor.
  __ Bind(&initialize);

  if (!FLAG_pretenuring_call_new) {
    // Make sure the function is the Array() function
    __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, scratch1);
    __ Cmp(function, scratch1);
    __ B(ne, &not_array_function);

    // The target function is the Array constructor,
    // Create an AllocationSite if we don't already have it, store it in the
    // slot.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      CreateAllocationSiteStub create_stub(masm->isolate());

      // Arguments register must be smi-tagged to call out.
      __ SmiTag(argc);
      __ Push(argc, function, feedback_vector, index);

      // CreateAllocationSiteStub expect the feedback vector in x2 and the slot
      // index in x3.
      DCHECK(feedback_vector.Is(x2) && index.Is(x3));
      __ CallStub(&create_stub);

      __ Pop(index, feedback_vector, function, argc);
      __ SmiUntag(argc);
    }
    __ B(&done);

    __ Bind(&not_array_function);
  }

  // An uninitialized cache is patched with the function.

  __ Add(scratch1, feedback_vector,
         Operand::UntagSmiAndScale(index, kPointerSizeLog2));
  __ Add(scratch1, scratch1, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Str(function, MemOperand(scratch1, 0));

  __ Push(function);
  __ RecordWrite(feedback_vector, scratch1, function, kLRHasNotBeenSaved,
                 kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Pop(function);

  __ Bind(&done);
}


static void EmitContinueIfStrictOrNative(MacroAssembler* masm, Label* cont) {
  // Do not transform the receiver for strict mode functions.
  __ Ldr(x3, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w4, FieldMemOperand(x3, SharedFunctionInfo::kCompilerHintsOffset));
  __ Tbnz(w4, SharedFunctionInfo::kStrictModeFunction, cont);

  // Do not transform the receiver for native (Compilerhints already in x3).
  __ Tbnz(w4, SharedFunctionInfo::kNative, cont);
}


static void EmitSlowCase(MacroAssembler* masm,
                         int argc,
                         Register function,
                         Register type,
                         Label* non_function) {
  // Check for function proxy.
  // x10 : function type.
  __ CompareAndBranch(type, JS_FUNCTION_PROXY_TYPE, ne, non_function);
  __ Push(function);  // put proxy as additional argument
  __ Mov(x0, argc + 1);
  __ Mov(x2, 0);
  __ GetBuiltinFunction(x1, Builtins::CALL_FUNCTION_PROXY);
  {
    Handle<Code> adaptor =
        masm->isolate()->builtins()->ArgumentsAdaptorTrampoline();
    __ Jump(adaptor, RelocInfo::CODE_TARGET);
  }

  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ Bind(non_function);
  __ Poke(function, argc * kXRegSize);
  __ Mov(x0, argc);  // Set up the number of arguments.
  __ Mov(x2, 0);
  __ GetBuiltinFunction(function, Builtins::CALL_NON_FUNCTION);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


static void EmitWrapCase(MacroAssembler* masm, int argc, Label* cont) {
  // Wrap the receiver and patch it back onto the stack.
  { FrameScope frame_scope(masm, StackFrame::INTERNAL);
    __ Push(x1, x3);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ Pop(x1);
  }
  __ Poke(x0, argc * kPointerSize);
  __ B(cont);
}


static void CallFunctionNoFeedback(MacroAssembler* masm,
                                   int argc, bool needs_checks,
                                   bool call_as_method) {
  // x1  function    the function to call
  Register function = x1;
  Register type = x4;
  Label slow, non_function, wrap, cont;

  // TODO(jbramley): This function has a lot of unnamed registers. Name them,
  // and tidy things up a bit.

  if (needs_checks) {
    // Check that the function is really a JavaScript function.
    __ JumpIfSmi(function, &non_function);

    // Goto slow case if we do not have a function.
    __ JumpIfNotObjectType(function, x10, type, JS_FUNCTION_TYPE, &slow);
  }

  // Fast-case: Invoke the function now.
  // x1  function  pushed function
  ParameterCount actual(argc);

  if (call_as_method) {
    if (needs_checks) {
      EmitContinueIfStrictOrNative(masm, &cont);
    }

    // Compute the receiver in sloppy mode.
    __ Peek(x3, argc * kPointerSize);

    if (needs_checks) {
      __ JumpIfSmi(x3, &wrap);
      __ JumpIfObjectType(x3, x10, type, FIRST_SPEC_OBJECT_TYPE, &wrap, lt);
    } else {
      __ B(&wrap);
    }

    __ Bind(&cont);
  }

  __ InvokeFunction(function,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper());
  if (needs_checks) {
    // Slow-case: Non-function called.
    __ Bind(&slow);
    EmitSlowCase(masm, argc, function, type, &non_function);
  }

  if (call_as_method) {
    __ Bind(&wrap);
    EmitWrapCase(masm, argc, &cont);
  }
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  ASM_LOCATION("CallFunctionStub::Generate");
  CallFunctionNoFeedback(masm, argc(), NeedsChecks(), CallAsMethod());
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  ASM_LOCATION("CallConstructStub::Generate");
  // x0 : number of arguments
  // x1 : the function to call
  // x2 : feedback vector
  // x3 : slot in feedback vector (smi) (if r2 is not the megamorphic symbol)
  Register function = x1;
  Label slow, non_function_call;

  // Check that the function is not a smi.
  __ JumpIfSmi(function, &non_function_call);
  // Check that the function is a JSFunction.
  Register object_type = x10;
  __ JumpIfNotObjectType(function, object_type, object_type, JS_FUNCTION_TYPE,
                         &slow);

  if (RecordCallTarget()) {
    GenerateRecordCallTarget(masm, x0, function, x2, x3, x4, x5);

    __ Add(x5, x2, Operand::UntagSmiAndScale(x3, kPointerSizeLog2));
    if (FLAG_pretenuring_call_new) {
      // Put the AllocationSite from the feedback vector into x2.
      // By adding kPointerSize we encode that we know the AllocationSite
      // entry is at the feedback vector slot given by x3 + 1.
      __ Ldr(x2, FieldMemOperand(x5, FixedArray::kHeaderSize + kPointerSize));
    } else {
    Label feedback_register_initialized;
      // Put the AllocationSite from the feedback vector into x2, or undefined.
      __ Ldr(x2, FieldMemOperand(x5, FixedArray::kHeaderSize));
      __ Ldr(x5, FieldMemOperand(x2, AllocationSite::kMapOffset));
      __ JumpIfRoot(x5, Heap::kAllocationSiteMapRootIndex,
                    &feedback_register_initialized);
      __ LoadRoot(x2, Heap::kUndefinedValueRootIndex);
      __ bind(&feedback_register_initialized);
    }

    __ AssertUndefinedOrAllocationSite(x2, x5);
  }

  // Jump to the function-specific construct stub.
  Register jump_reg = x4;
  Register shared_func_info = jump_reg;
  Register cons_stub = jump_reg;
  Register cons_stub_code = jump_reg;
  __ Ldr(shared_func_info,
         FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(cons_stub,
         FieldMemOperand(shared_func_info,
                         SharedFunctionInfo::kConstructStubOffset));
  __ Add(cons_stub_code, cons_stub, Code::kHeaderSize - kHeapObjectTag);
  __ Br(cons_stub_code);

  Label do_call;
  __ Bind(&slow);
  __ Cmp(object_type, JS_FUNCTION_PROXY_TYPE);
  __ B(ne, &non_function_call);
  __ GetBuiltinFunction(x1, Builtins::CALL_FUNCTION_PROXY_AS_CONSTRUCTOR);
  __ B(&do_call);

  __ Bind(&non_function_call);
  __ GetBuiltinFunction(x1, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);

  __ Bind(&do_call);
  // Set expected number of arguments to zero (not changing x0).
  __ Mov(x2, 0);
  __ Jump(isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


static void EmitLoadTypeFeedbackVector(MacroAssembler* masm, Register vector) {
  __ Ldr(vector, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Ldr(vector, FieldMemOperand(vector,
                                 JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(vector, FieldMemOperand(vector,
                                 SharedFunctionInfo::kFeedbackVectorOffset));
}


void CallIC_ArrayStub::Generate(MacroAssembler* masm) {
  // x1 - function
  // x3 - slot id
  Label miss;
  Register function = x1;
  Register feedback_vector = x2;
  Register index = x3;
  Register scratch = x4;

  EmitLoadTypeFeedbackVector(masm, feedback_vector);

  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, scratch);
  __ Cmp(function, scratch);
  __ B(ne, &miss);

  __ Mov(x0, Operand(arg_count()));

  __ Add(scratch, feedback_vector,
         Operand::UntagSmiAndScale(index, kPointerSizeLog2));
  __ Ldr(scratch, FieldMemOperand(scratch, FixedArray::kHeaderSize));

  // Verify that scratch contains an AllocationSite
  Register map = x5;
  __ Ldr(map, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ JumpIfNotRoot(map, Heap::kAllocationSiteMapRootIndex, &miss);

  Register allocation_site = feedback_vector;
  __ Mov(allocation_site, scratch);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);

  __ bind(&miss);
  GenerateMiss(masm);

  // The slow case, we need this no matter what to complete a call after a miss.
  CallFunctionNoFeedback(masm,
                         arg_count(),
                         true,
                         CallAsMethod());

  __ Unreachable();
}


void CallICStub::Generate(MacroAssembler* masm) {
  ASM_LOCATION("CallICStub");

  // x1 - function
  // x3 - slot id (Smi)
  Label extra_checks_or_miss, slow_start;
  Label slow, non_function, wrap, cont;
  Label have_js_function;
  int argc = arg_count();
  ParameterCount actual(argc);

  Register function = x1;
  Register feedback_vector = x2;
  Register index = x3;
  Register type = x4;

  EmitLoadTypeFeedbackVector(masm, feedback_vector);

  // The checks. First, does x1 match the recorded monomorphic target?
  __ Add(x4, feedback_vector,
         Operand::UntagSmiAndScale(index, kPointerSizeLog2));
  __ Ldr(x4, FieldMemOperand(x4, FixedArray::kHeaderSize));

  __ Cmp(x4, function);
  __ B(ne, &extra_checks_or_miss);

  __ bind(&have_js_function);
  if (CallAsMethod()) {
    EmitContinueIfStrictOrNative(masm, &cont);

    // Compute the receiver in sloppy mode.
    __ Peek(x3, argc * kPointerSize);

    __ JumpIfSmi(x3, &wrap);
    __ JumpIfObjectType(x3, x10, type, FIRST_SPEC_OBJECT_TYPE, &wrap, lt);

    __ Bind(&cont);
  }

  __ InvokeFunction(function,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper());

  __ bind(&slow);
  EmitSlowCase(masm, argc, function, type, &non_function);

  if (CallAsMethod()) {
    __ bind(&wrap);
    EmitWrapCase(masm, argc, &cont);
  }

  __ bind(&extra_checks_or_miss);
  Label miss;

  __ JumpIfRoot(x4, Heap::kMegamorphicSymbolRootIndex, &slow_start);
  __ JumpIfRoot(x4, Heap::kUninitializedSymbolRootIndex, &miss);

  if (!FLAG_trace_ic) {
    // We are going megamorphic. If the feedback is a JSFunction, it is fine
    // to handle it here. More complex cases are dealt with in the runtime.
    __ AssertNotSmi(x4);
    __ JumpIfNotObjectType(x4, x5, x5, JS_FUNCTION_TYPE, &miss);
    __ Add(x4, feedback_vector,
           Operand::UntagSmiAndScale(index, kPointerSizeLog2));
    __ LoadRoot(x5, Heap::kMegamorphicSymbolRootIndex);
    __ Str(x5, FieldMemOperand(x4, FixedArray::kHeaderSize));
    __ B(&slow_start);
  }

  // We are here because tracing is on or we are going monomorphic.
  __ bind(&miss);
  GenerateMiss(masm);

  // the slow case
  __ bind(&slow_start);

  // Check that the function is really a JavaScript function.
  __ JumpIfSmi(function, &non_function);

  // Goto slow case if we do not have a function.
  __ JumpIfNotObjectType(function, x10, type, JS_FUNCTION_TYPE, &slow);
  __ B(&have_js_function);
}


void CallICStub::GenerateMiss(MacroAssembler* masm) {
  ASM_LOCATION("CallICStub[Miss]");

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ Peek(x4, (arg_count() + 1) * kPointerSize);

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push the receiver and the function and feedback info.
    __ Push(x4, x1, x2, x3);

    // Call the entry.
    IC::UtilityId id = GetICState() == DEFAULT ? IC::kCallIC_Miss
                                               : IC::kCallIC_Customization_Miss;

    ExternalReference miss = ExternalReference(IC_Utility(id),
                                               masm->isolate());
    __ CallExternalReference(miss, 4);

    // Move result to edi and exit the internal frame.
    __ Mov(x1, x0);
  }
}


void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ Ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ Ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));

  // If the receiver is not a string trigger the non-string case.
  __ TestAndBranchIfAnySet(result_, kIsNotStringMask, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);

  __ Bind(&got_smi_index_);
  // Check for index out of range.
  __ Ldrsw(result_, UntagSmiFieldMemOperand(object_, String::kLengthOffset));
  __ Cmp(result_, Operand::UntagSmi(index_));
  __ B(ls, index_out_of_range_);

  __ SmiUntag(index_);

  StringCharLoadGenerator::Generate(masm,
                                    object_,
                                    index_.W(),
                                    result_,
                                    &call_runtime_);
  __ SmiTag(result_);
  __ Bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

  __ Bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ JumpIfNotHeapNumber(index_, index_not_number_);
  call_helper.BeforeCall(masm);
  // Save object_ on the stack and pass index_ as argument for runtime call.
  __ Push(object_, index_);
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    DCHECK(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.
  __ Mov(index_, x0);
  __ Pop(object_);
  // Reload the instance type.
  __ Ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ Ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);

  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ B(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ Bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ SmiTag(index_);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT, 2);
  __ Mov(result_, x0);
  call_helper.AfterCall(masm);
  __ B(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  __ JumpIfNotSmi(code_, &slow_case_);
  __ Cmp(code_, Smi::FromInt(String::kMaxOneByteCharCode));
  __ B(hi, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged one-byte char code.
  __ Add(result_, result_, Operand::UntagSmiAndScale(code_, kPointerSizeLog2));
  __ Ldr(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ JumpIfRoot(result_, Heap::kUndefinedValueRootIndex, &slow_case_);
  __ Bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ Bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ Push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  __ Mov(result_, x0);
  call_helper.AfterCall(masm);
  __ B(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  // Inputs are in x0 (lhs) and x1 (rhs).
  DCHECK(state() == CompareICState::SMI);
  ASM_LOCATION("CompareICStub[Smis]");
  Label miss;
  // Bail out (to 'miss') unless both x0 and x1 are smis.
  __ JumpIfEitherNotSmi(x0, x1, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    __ Sub(x0, x0, x1);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(x1);
    __ Sub(x0, x1, Operand::UntagSmi(x0));
  }
  __ Ret();

  __ Bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateNumbers(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::NUMBER);
  ASM_LOCATION("CompareICStub[HeapNumbers]");

  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss, handle_lhs, values_in_d_regs;
  Label untag_rhs, untag_lhs;

  Register result = x0;
  Register rhs = x0;
  Register lhs = x1;
  FPRegister rhs_d = d0;
  FPRegister lhs_d = d1;

  if (left() == CompareICState::SMI) {
    __ JumpIfNotSmi(lhs, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(rhs, &miss);
  }

  __ SmiUntagToDouble(rhs_d, rhs, kSpeculativeUntag);
  __ SmiUntagToDouble(lhs_d, lhs, kSpeculativeUntag);

  // Load rhs if it's a heap number.
  __ JumpIfSmi(rhs, &handle_lhs);
  __ JumpIfNotHeapNumber(rhs, &maybe_undefined1);
  __ Ldr(rhs_d, FieldMemOperand(rhs, HeapNumber::kValueOffset));

  // Load lhs if it's a heap number.
  __ Bind(&handle_lhs);
  __ JumpIfSmi(lhs, &values_in_d_regs);
  __ JumpIfNotHeapNumber(lhs, &maybe_undefined2);
  __ Ldr(lhs_d, FieldMemOperand(lhs, HeapNumber::kValueOffset));

  __ Bind(&values_in_d_regs);
  __ Fcmp(lhs_d, rhs_d);
  __ B(vs, &unordered);  // Overflow flag set if either is NaN.
  STATIC_ASSERT((LESS == -1) && (EQUAL == 0) && (GREATER == 1));
  __ Cset(result, gt);  // gt => 1, otherwise (lt, eq) => 0 (EQUAL).
  __ Csinv(result, result, xzr, ge);  // lt => -1, gt => 1, eq => 0.
  __ Ret();

  __ Bind(&unordered);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ Bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ JumpIfNotRoot(rhs, Heap::kUndefinedValueRootIndex, &miss);
    __ JumpIfSmi(lhs, &unordered);
    __ JumpIfNotHeapNumber(lhs, &maybe_undefined2);
    __ B(&unordered);
  }

  __ Bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ JumpIfRoot(lhs, Heap::kUndefinedValueRootIndex, &unordered);
  }

  __ Bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  ASM_LOCATION("CompareICStub[InternalizedStrings]");
  Label miss;

  Register result = x0;
  Register rhs = x0;
  Register lhs = x1;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(lhs, rhs, &miss);

  // Check that both operands are internalized strings.
  Register rhs_map = x10;
  Register lhs_map = x11;
  Register rhs_type = x10;
  Register lhs_type = x11;
  __ Ldr(lhs_map, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ Ldr(rhs_map, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ Ldrb(lhs_type, FieldMemOperand(lhs_map, Map::kInstanceTypeOffset));
  __ Ldrb(rhs_type, FieldMemOperand(rhs_map, Map::kInstanceTypeOffset));

  STATIC_ASSERT((kInternalizedTag == 0) && (kStringTag == 0));
  __ Orr(x12, lhs_type, rhs_type);
  __ TestAndBranchIfAnySet(
      x12, kIsNotStringMask | kIsNotInternalizedMask, &miss);

  // Internalized strings are compared by identity.
  STATIC_ASSERT(EQUAL == 0);
  __ Cmp(lhs, rhs);
  __ Cset(result, ne);
  __ Ret();

  __ Bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  ASM_LOCATION("CompareICStub[UniqueNames]");
  DCHECK(GetCondition() == eq);
  Label miss;

  Register result = x0;
  Register rhs = x0;
  Register lhs = x1;

  Register lhs_instance_type = w2;
  Register rhs_instance_type = w3;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(lhs, rhs, &miss);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ Ldr(x10, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ Ldr(x11, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ Ldrb(lhs_instance_type, FieldMemOperand(x10, Map::kInstanceTypeOffset));
  __ Ldrb(rhs_instance_type, FieldMemOperand(x11, Map::kInstanceTypeOffset));

  // To avoid a miss, each instance type should be either SYMBOL_TYPE or it
  // should have kInternalizedTag set.
  __ JumpIfNotUniqueNameInstanceType(lhs_instance_type, &miss);
  __ JumpIfNotUniqueNameInstanceType(rhs_instance_type, &miss);

  // Unique names are compared by identity.
  STATIC_ASSERT(EQUAL == 0);
  __ Cmp(lhs, rhs);
  __ Cset(result, ne);
  __ Ret();

  __ Bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  ASM_LOCATION("CompareICStub[Strings]");

  Label miss;

  bool equality = Token::IsEqualityOp(op());

  Register result = x0;
  Register rhs = x0;
  Register lhs = x1;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(rhs, lhs, &miss);

  // Check that both operands are strings.
  Register rhs_map = x10;
  Register lhs_map = x11;
  Register rhs_type = x10;
  Register lhs_type = x11;
  __ Ldr(lhs_map, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ Ldr(rhs_map, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ Ldrb(lhs_type, FieldMemOperand(lhs_map, Map::kInstanceTypeOffset));
  __ Ldrb(rhs_type, FieldMemOperand(rhs_map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ Orr(x12, lhs_type, rhs_type);
  __ Tbnz(x12, MaskToBit(kIsNotStringMask), &miss);

  // Fast check for identical strings.
  Label not_equal;
  __ Cmp(lhs, rhs);
  __ B(ne, &not_equal);
  __ Mov(result, EQUAL);
  __ Ret();

  __ Bind(&not_equal);
  // Handle not identical strings

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We know they are both
  // strings.
  if (equality) {
    DCHECK(GetCondition() == eq);
    STATIC_ASSERT(kInternalizedTag == 0);
    Label not_internalized_strings;
    __ Orr(x12, lhs_type, rhs_type);
    __ TestAndBranchIfAnySet(
        x12, kIsNotInternalizedMask, &not_internalized_strings);
    // Result is in rhs (x0), and not EQUAL, as rhs is not a smi.
    __ Ret();
    __ Bind(&not_internalized_strings);
  }

  // Check that both strings are sequential one-byte.
  Label runtime;
  __ JumpIfBothInstanceTypesAreNotSequentialOneByte(lhs_type, rhs_type, x12,
                                                    x13, &runtime);

  // Compare flat one-byte strings. Returns when done.
  if (equality) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, lhs, rhs, x10, x11,
                                                  x12);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, lhs, rhs, x10, x11,
                                                    x12, x13);
  }

  // Handle more complex cases in runtime.
  __ Bind(&runtime);
  __ Push(lhs, rhs);
  if (equality) {
    __ TailCallRuntime(Runtime::kStringEquals, 2, 1);
  } else {
    __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
  }

  __ Bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateObjects(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::OBJECT);
  ASM_LOCATION("CompareICStub[Objects]");

  Label miss;

  Register result = x0;
  Register rhs = x0;
  Register lhs = x1;

  __ JumpIfEitherSmi(rhs, lhs, &miss);

  __ JumpIfNotObjectType(rhs, x10, x10, JS_OBJECT_TYPE, &miss);
  __ JumpIfNotObjectType(lhs, x10, x10, JS_OBJECT_TYPE, &miss);

  DCHECK(GetCondition() == eq);
  __ Sub(result, rhs, lhs);
  __ Ret();

  __ Bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownObjects(MacroAssembler* masm) {
  ASM_LOCATION("CompareICStub[KnownObjects]");

  Label miss;

  Register result = x0;
  Register rhs = x0;
  Register lhs = x1;

  __ JumpIfEitherSmi(rhs, lhs, &miss);

  Register rhs_map = x10;
  Register lhs_map = x11;
  __ Ldr(rhs_map, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ Ldr(lhs_map, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ Cmp(rhs_map, Operand(known_map_));
  __ B(ne, &miss);
  __ Cmp(lhs_map, Operand(known_map_));
  __ B(ne, &miss);

  __ Sub(result, rhs, lhs);
  __ Ret();

  __ Bind(&miss);
  GenerateMiss(masm);
}


// This method handles the case where a compare stub had the wrong
// implementation. It calls a miss handler, which re-writes the stub. All other
// CompareICStub::Generate* methods should fall back into this one if their
// operands were not the expected types.
void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  ASM_LOCATION("CompareICStub[Miss]");

  Register stub_entry = x11;
  {
    ExternalReference miss =
      ExternalReference(IC_Utility(IC::kCompareIC_Miss), isolate());

    FrameScope scope(masm, StackFrame::INTERNAL);
    Register op = x10;
    Register left = x1;
    Register right = x0;
    // Preserve some caller-saved registers.
    __ Push(x1, x0, lr);
    // Push the arguments.
    __ Mov(op, Smi::FromInt(this->op()));
    __ Push(left, right, op);

    // Call the miss handler. This also pops the arguments.
    __ CallExternalReference(miss, 3);

    // Compute the entry point of the rewritten stub.
    __ Add(stub_entry, x0, Code::kHeaderSize - kHeapObjectTag);
    // Restore caller-saved registers.
    __ Pop(lr, x0, x1);
  }

  // Tail-call to the new stub.
  __ Jump(stub_entry);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  ASM_LOCATION("SubStringStub::Generate");
  Label runtime;

  // Stack frame on entry.
  //  lr: return address
  //  jssp[0]:  substring "to" offset
  //  jssp[8]:  substring "from" offset
  //  jssp[16]: pointer to string object

  // This stub is called from the native-call %_SubString(...), so
  // nothing can be assumed about the arguments. It is tested that:
  //  "string" is a sequential string,
  //  both "from" and "to" are smis, and
  //  0 <= from <= to <= string.length (in debug mode.)
  // If any of these assumptions fail, we call the runtime system.

  static const int kToOffset = 0 * kPointerSize;
  static const int kFromOffset = 1 * kPointerSize;
  static const int kStringOffset = 2 * kPointerSize;

  Register to = x0;
  Register from = x15;
  Register input_string = x10;
  Register input_length = x11;
  Register input_type = x12;
  Register result_string = x0;
  Register result_length = x1;
  Register temp = x3;

  __ Peek(to, kToOffset);
  __ Peek(from, kFromOffset);

  // Check that both from and to are smis. If not, jump to runtime.
  __ JumpIfEitherNotSmi(from, to, &runtime);
  __ SmiUntag(from);
  __ SmiUntag(to);

  // Calculate difference between from and to. If to < from, branch to runtime.
  __ Subs(result_length, to, from);
  __ B(mi, &runtime);

  // Check from is positive.
  __ Tbnz(from, kWSignBit, &runtime);

  // Make sure first argument is a string.
  __ Peek(input_string, kStringOffset);
  __ JumpIfSmi(input_string, &runtime);
  __ IsObjectJSStringType(input_string, input_type, &runtime);

  Label single_char;
  __ Cmp(result_length, 1);
  __ B(eq, &single_char);

  // Short-cut for the case of trivial substring.
  Label return_x0;
  __ Ldrsw(input_length,
           UntagSmiFieldMemOperand(input_string, String::kLengthOffset));

  __ Cmp(result_length, input_length);
  __ CmovX(x0, input_string, eq);
  // Return original string.
  __ B(eq, &return_x0);

  // Longer than original string's length or negative: unsafe arguments.
  __ B(hi, &runtime);

  // Shorter than original string's length: an actual substring.

  //   x0   to               substring end character offset
  //   x1   result_length    length of substring result
  //   x10  input_string     pointer to input string object
  //   x10  unpacked_string  pointer to unpacked string object
  //   x11  input_length     length of input string
  //   x12  input_type       instance type of input string
  //   x15  from             substring start character offset

  // Deal with different string types: update the index if necessary and put
  // the underlying string into register unpacked_string.
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  Label update_instance_type;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);

  // Test for string types, and branch/fall through to appropriate unpacking
  // code.
  __ Tst(input_type, kIsIndirectStringMask);
  __ B(eq, &seq_or_external_string);
  __ Tst(input_type, kSlicedNotConsMask);
  __ B(ne, &sliced_string);

  Register unpacked_string = input_string;

  // Cons string. Check whether it is flat, then fetch first part.
  __ Ldr(temp, FieldMemOperand(input_string, ConsString::kSecondOffset));
  __ JumpIfNotRoot(temp, Heap::kempty_stringRootIndex, &runtime);
  __ Ldr(unpacked_string,
         FieldMemOperand(input_string, ConsString::kFirstOffset));
  __ B(&update_instance_type);

  __ Bind(&sliced_string);
  // Sliced string. Fetch parent and correct start index by offset.
  __ Ldrsw(temp,
           UntagSmiFieldMemOperand(input_string, SlicedString::kOffsetOffset));
  __ Add(from, from, temp);
  __ Ldr(unpacked_string,
         FieldMemOperand(input_string, SlicedString::kParentOffset));

  __ Bind(&update_instance_type);
  __ Ldr(temp, FieldMemOperand(unpacked_string, HeapObject::kMapOffset));
  __ Ldrb(input_type, FieldMemOperand(temp, Map::kInstanceTypeOffset));
  // Now control must go to &underlying_unpacked. Since the no code is generated
  // before then we fall through instead of generating a useless branch.

  __ Bind(&seq_or_external_string);
  // Sequential or external string. Registers unpacked_string and input_string
  // alias, so there's nothing to do here.
  // Note that if code is added here, the above code must be updated.

  //   x0   result_string    pointer to result string object (uninit)
  //   x1   result_length    length of substring result
  //   x10  unpacked_string  pointer to unpacked string object
  //   x11  input_length     length of input string
  //   x12  input_type       instance type of input string
  //   x15  from             substring start character offset
  __ Bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    __ Cmp(result_length, SlicedString::kMinLength);
    // Short slice. Copy instead of slicing.
    __ B(lt, &copy_routine);
    // Allocate new sliced string. At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string. It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyway due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ Tbz(input_type, MaskToBit(kStringEncodingMask), &two_byte_slice);
    __ AllocateOneByteSlicedString(result_string, result_length, x3, x4,
                                   &runtime);
    __ B(&set_slice_header);

    __ Bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(result_string, result_length, x3, x4,
                                   &runtime);

    __ Bind(&set_slice_header);
    __ SmiTag(from);
    __ Str(from, FieldMemOperand(result_string, SlicedString::kOffsetOffset));
    __ Str(unpacked_string,
           FieldMemOperand(result_string, SlicedString::kParentOffset));
    __ B(&return_x0);

    __ Bind(&copy_routine);
  }

  //   x0   result_string    pointer to result string object (uninit)
  //   x1   result_length    length of substring result
  //   x10  unpacked_string  pointer to unpacked string object
  //   x11  input_length     length of input string
  //   x12  input_type       instance type of input string
  //   x13  unpacked_char0   pointer to first char of unpacked string (uninit)
  //   x13  substring_char0  pointer to first char of substring (uninit)
  //   x14  result_char0     pointer to first char of result (uninit)
  //   x15  from             substring start character offset
  Register unpacked_char0 = x13;
  Register substring_char0 = x13;
  Register result_char0 = x14;
  Label two_byte_sequential, sequential_string, allocate_result;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);

  __ Tst(input_type, kExternalStringTag);
  __ B(eq, &sequential_string);

  __ Tst(input_type, kShortExternalStringTag);
  __ B(ne, &runtime);
  __ Ldr(unpacked_char0,
         FieldMemOperand(unpacked_string, ExternalString::kResourceDataOffset));
  // unpacked_char0 points to the first character of the underlying string.
  __ B(&allocate_result);

  __ Bind(&sequential_string);
  // Locate first character of underlying subject string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Add(unpacked_char0, unpacked_string,
         SeqOneByteString::kHeaderSize - kHeapObjectTag);

  __ Bind(&allocate_result);
  // Sequential one-byte string. Allocate the result.
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ Tbz(input_type, MaskToBit(kStringEncodingMask), &two_byte_sequential);

  // Allocate and copy the resulting one-byte string.
  __ AllocateOneByteString(result_string, result_length, x3, x4, x5, &runtime);

  // Locate first character of substring to copy.
  __ Add(substring_char0, unpacked_char0, from);

  // Locate first character of result.
  __ Add(result_char0, result_string,
         SeqOneByteString::kHeaderSize - kHeapObjectTag);

  STATIC_ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  __ CopyBytes(result_char0, substring_char0, result_length, x3, kCopyLong);
  __ B(&return_x0);

  // Allocate and copy the resulting two-byte string.
  __ Bind(&two_byte_sequential);
  __ AllocateTwoByteString(result_string, result_length, x3, x4, x5, &runtime);

  // Locate first character of substring to copy.
  __ Add(substring_char0, unpacked_char0, Operand(from, LSL, 1));

  // Locate first character of result.
  __ Add(result_char0, result_string,
         SeqTwoByteString::kHeaderSize - kHeapObjectTag);

  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  __ Add(result_length, result_length, result_length);
  __ CopyBytes(result_char0, substring_char0, result_length, x3, kCopyLong);

  __ Bind(&return_x0);
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1, x3, x4);
  __ Drop(3);
  __ Ret();

  __ Bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);

  __ bind(&single_char);
  // x1: result_length
  // x10: input_string
  // x12: input_type
  // x15: from (untagged)
  __ SmiTag(from);
  StringCharAtGenerator generator(
      input_string, from, result_length, x0,
      &runtime, &runtime, &runtime, STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm);
  __ Drop(3);
  __ Ret();
  generator.SkipSlow(masm, &runtime);
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


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  Counters* counters = isolate()->counters();

  // Stack frame on entry.
  //  sp[0]: right string
  //  sp[8]: left string
  Register right = x10;
  Register left = x11;
  Register result = x0;
  __ Pop(right, left);

  Label not_same;
  __ Subs(result, right, left);
  __ B(ne, &not_same);
  STATIC_ASSERT(EQUAL == 0);
  __ IncrementCounter(counters->string_compare_native(), 1, x3, x4);
  __ Ret();

  __ Bind(&not_same);

  // Check that both objects are sequential one-byte strings.
  __ JumpIfEitherIsNotSequentialOneByteStrings(left, right, x12, x13, &runtime);

  // Compare flat one-byte strings natively. Remove arguments from stack first,
  // as this function will generate a return.
  __ IncrementCounter(counters->string_compare_native(), 1, x3, x4);
  StringHelper::GenerateCompareFlatOneByteStrings(masm, left, right, x12, x13,
                                                  x14, x15);

  __ Bind(&runtime);

  // Push arguments back on to the stack.
  //  sp[0] = right string
  //  sp[8] = left string.
  __ Push(left, right);

  // Call the runtime.
  // Returns -1 (less), 0 (equal), or 1 (greater) tagged as a small integer.
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x1    : left
  //  -- x0    : right
  //  -- lr    : return address
  // -----------------------------------

  // Load x2 with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ LoadObject(x2, handle(isolate()->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ AssertNotSmi(x2, kExpectedAllocationSite);
    __ Ldr(x10, FieldMemOperand(x2, HeapObject::kMapOffset));
    __ AssertRegisterIsRoot(x10, Heap::kAllocationSiteMapRootIndex,
                            kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state());
  __ TailCallStub(&stub);
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

    __ CheckPageFlagSet(regs_.object(), val, 1 << MemoryChunk::SCAN_ON_SCAVENGE,
                        &dont_need_remembered_set);

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


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_scratch;

  Register mem_chunk = regs_.scratch0();
  Register counter = regs_.scratch1();
  __ Bic(mem_chunk, regs_.object(), Page::kPageAlignmentMask);
  __ Ldr(counter,
         MemOperand(mem_chunk, MemoryChunk::kWriteBarrierCounterOffset));
  __ Subs(counter, counter, 1);
  __ Str(counter,
         MemOperand(mem_chunk, MemoryChunk::kWriteBarrierCounterOffset));
  __ B(mi, &need_incremental);

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
  __ EnsureNotWhite(val,
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


void StoreArrayLiteralElementStub::Generate(MacroAssembler* masm) {
  // x0     value            element value to store
  // x3     index_smi        element index as smi
  // sp[0]  array_index_smi  array literal index in function as smi
  // sp[1]  array            array literal

  Register value = x0;
  Register index_smi = x3;

  Register array = x1;
  Register array_map = x2;
  Register array_index_smi = x4;
  __ PeekPair(array_index_smi, array, 0);
  __ Ldr(array_map, FieldMemOperand(array, JSObject::kMapOffset));

  Label double_elements, smi_element, fast_elements, slow_elements;
  Register bitfield2 = x10;
  __ Ldrb(bitfield2, FieldMemOperand(array_map, Map::kBitField2Offset));

  // Jump if array's ElementsKind is not FAST*_SMI_ELEMENTS, FAST_ELEMENTS or
  // FAST_HOLEY_ELEMENTS.
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  __ Cmp(bitfield2, Map::kMaximumBitField2FastHoleyElementValue);
  __ B(hi, &double_elements);

  __ JumpIfSmi(value, &smi_element);

  // Jump if array's ElementsKind is not FAST_ELEMENTS or FAST_HOLEY_ELEMENTS.
  __ Tbnz(bitfield2, MaskToBit(FAST_ELEMENTS << Map::ElementsKindBits::kShift),
          &fast_elements);

  // Store into the array literal requires an elements transition. Call into
  // the runtime.
  __ Bind(&slow_elements);
  __ Push(array, index_smi, value);
  __ Ldr(x10, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Ldr(x11, FieldMemOperand(x10, JSFunction::kLiteralsOffset));
  __ Push(x11, array_index_smi);
  __ TailCallRuntime(Runtime::kStoreArrayLiteralElement, 5, 1);

  // Array literal has ElementsKind of FAST_*_ELEMENTS and value is an object.
  __ Bind(&fast_elements);
  __ Ldr(x10, FieldMemOperand(array, JSObject::kElementsOffset));
  __ Add(x11, x10, Operand::UntagSmiAndScale(index_smi, kPointerSizeLog2));
  __ Add(x11, x11, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Str(value, MemOperand(x11));
  // Update the write barrier for the array store.
  __ RecordWrite(x10, x11, value, kLRHasNotBeenSaved, kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Ret();

  // Array literal has ElementsKind of FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS,
  // and value is Smi.
  __ Bind(&smi_element);
  __ Ldr(x10, FieldMemOperand(array, JSObject::kElementsOffset));
  __ Add(x11, x10, Operand::UntagSmiAndScale(index_smi, kPointerSizeLog2));
  __ Str(value, FieldMemOperand(x11, FixedArray::kHeaderSize));
  __ Ret();

  __ Bind(&double_elements);
  __ Ldr(x10, FieldMemOperand(array, JSObject::kElementsOffset));
  __ StoreNumberToDoubleElements(value, index_smi, x10, x11, d0,
                                 &slow_elements);
  __ Ret();
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ Call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ Ldr(x1, MemOperand(fp, parameter_count_offset));
  if (function_mode() == JS_FUNCTION_STUB_MODE) {
    __ Add(x1, x1, 1);
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ Drop(x1);
  // Return to IC Miss stub, continuation still on stack.
  __ Ret();
}


void LoadICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, VectorLoadICDescriptor::VectorRegister());
  VectorLoadStub stub(isolate(), state());
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void KeyedLoadICTrampolineStub::Generate(MacroAssembler* masm) {
  EmitLoadTypeFeedbackVector(masm, VectorLoadICDescriptor::VectorRegister());
  VectorKeyedLoadStub stub(isolate());
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


static unsigned int GetProfileEntryHookCallSize(MacroAssembler* masm) {
  // The entry hook is a "BumpSystemStackPointer" instruction (sub),
  // followed by a "Push lr" instruction, followed by a call.
  unsigned int size =
      Assembler::kCallSizeWithRelocation + (2 * kInstructionSize);
  if (CpuFeatures::IsSupported(ALWAYS_ALIGN_CSP)) {
    // If ALWAYS_ALIGN_CSP then there will be an extra bic instruction in
    // "BumpSystemStackPointer".
    size += kInstructionSize;
  }
  return size;
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
           GetProfileEntryHookCallSize(masm));

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
  __ Sub(x0, lr, GetProfileEntryHookCallSize(masm));

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


// Probe the name dictionary in the 'elements' register.
// Jump to the 'done' label if a property with the given name is found.
// Jump to the 'miss' label otherwise.
//
// If lookup was successful 'scratch2' will be equal to elements + 4 * index.
// 'elements' and 'name' registers are preserved on miss.
void NameDictionaryLookupStub::GeneratePositiveLookup(
    MacroAssembler* masm,
    Label* miss,
    Label* done,
    Register elements,
    Register name,
    Register scratch1,
    Register scratch2) {
  DCHECK(!AreAliased(elements, name, scratch1, scratch2));

  // Assert that name contains a string.
  __ AssertName(name);

  // Compute the capacity mask.
  __ Ldrsw(scratch1, UntagSmiFieldMemOperand(elements, kCapacityOffset));
  __ Sub(scratch1, scratch1, 1);

  // Generate an unrolled loop that performs a few probes before giving up.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ Ldr(scratch2, FieldMemOperand(name, Name::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
          1 << (32 - Name::kHashFieldOffset));
      __ Add(scratch2, scratch2, Operand(
          NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ And(scratch2, scratch1, Operand(scratch2, LSR, Name::kHashShift));

    // Scale the index by multiplying by the element size.
    DCHECK(NameDictionary::kEntrySize == 3);
    __ Add(scratch2, scratch2, Operand(scratch2, LSL, 1));

    // Check if the key is identical to the name.
    UseScratchRegisterScope temps(masm);
    Register scratch3 = temps.AcquireX();
    __ Add(scratch2, elements, Operand(scratch2, LSL, kPointerSizeLog2));
    __ Ldr(scratch3, FieldMemOperand(scratch2, kElementsStartOffset));
    __ Cmp(name, scratch3);
    __ B(eq, done);
  }

  // The inlined probes didn't find the entry.
  // Call the complete stub to scan the whole dictionary.

  CPURegList spill_list(CPURegister::kRegister, kXRegSizeInBits, 0, 6);
  spill_list.Combine(lr);
  spill_list.Remove(scratch1);
  spill_list.Remove(scratch2);

  __ PushCPURegList(spill_list);

  if (name.is(x0)) {
    DCHECK(!elements.is(x1));
    __ Mov(x1, name);
    __ Mov(x0, elements);
  } else {
    __ Mov(x0, elements);
    __ Mov(x1, name);
  }

  Label not_found;
  NameDictionaryLookupStub stub(masm->isolate(), POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ Cbz(x0, &not_found);
  __ Mov(scratch2, x2);  // Move entry index into scratch2.
  __ PopCPURegList(spill_list);
  __ B(done);

  __ Bind(&not_found);
  __ PopCPURegList(spill_list);
  __ B(miss);
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
    DCHECK(NameDictionary::kEntrySize == 3);
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

  __ Ldr(x0, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
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
    DCHECK(NameDictionary::kEntrySize == 3);
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

  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // Is the low bit set? If so, the array is holey.
    __ Tbnz(kind, 0, &normal_sequence);
  }

  // Look at the last argument.
  // TODO(jbramley): What does a 0 argument represent?
  __ Peek(x10, 0);
  __ Cbz(x10, &normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(masm->isolate(),
                                                  holey_initial,
                                                  DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ Bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(masm->isolate(),
                                            initial,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
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
    __ Ldr(x11, FieldMemOperand(allocation_site,
                                AllocationSite::kTransitionInfoOffset));
    __ Add(x11, x11, Smi::FromInt(kFastElementsKindPackedToHoley));
    __ Str(x11, FieldMemOperand(allocation_site,
                                AllocationSite::kTransitionInfoOffset));

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
  Register argc = x0;
  if (argument_count() == ANY) {
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
  ASM_LOCATION("ArrayConstructorStub::Generate");
  // ----------- S t a t e -------------
  //  -- x0 : argc (only if argument_count() == ANY)
  //  -- x1 : constructor
  //  -- x2 : AllocationSite or undefined
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------
  Register constructor = x1;
  Register allocation_site = x2;

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

  Register kind = x3;
  Label no_info;
  // Get the elements kind and case on that.
  __ JumpIfRoot(allocation_site, Heap::kUndefinedValueRootIndex, &no_info);

  __ Ldrsw(kind,
           UntagSmiFieldMemOperand(allocation_site,
                                   AllocationSite::kTransitionInfoOffset));
  __ And(kind, kind, AllocationSite::ElementsKindBits::kMask);
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ Bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);
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
  InternalArrayNArgumentsConstructorStub stubN(isolate(), kind);
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
    __ Cmp(x3, FAST_ELEMENTS);
    __ Ccmp(x3, FAST_HOLEY_ELEMENTS, ZFlag, ne);
    __ Assert(eq, kInvalidElementsKindForInternalArrayOrInternalPackedArray);
  }

  Label fast_elements_case;
  __ CompareAndBranch(kind, FAST_ELEMENTS, eq, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ Bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
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
  // -----------------------------------

  Register callee = x0;
  Register call_data = x4;
  Register holder = x2;
  Register api_function_address = x1;
  Register context = cp;

  int argc = this->argc();
  bool is_store = this->is_store();
  bool call_data_undefined = this->call_data_undefined();

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kArgsLength == 7);

  // FunctionCallbackArguments: context, callee and call data.
  __ Push(context, callee, call_data);

  // Load context from callee
  __ Ldr(context, FieldMemOperand(callee, JSFunction::kContextOffset));

  if (!call_data_undefined) {
    __ LoadRoot(call_data, Heap::kUndefinedValueRootIndex);
  }
  Register isolate_reg = x5;
  __ Mov(isolate_reg, ExternalReference::isolate_address(isolate()));

  // FunctionCallbackArguments:
  //    return value, return value default, isolate, holder.
  __ Push(call_data, call_data, isolate_reg, holder);

  // Prepare arguments.
  Register args = x6;
  __ Mov(args, masm->StackPointer());

  // Allocate the v8::Arguments structure in the arguments' space, since it's
  // not controlled by GC.
  const int kApiStackSpace = 4;

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
  __ Add(x10, args, Operand((FCA::kArgsLength - 1 + argc) * kPointerSize));
  __ Stp(args, x10, MemOperand(x0, 0 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc and
  // FunctionCallbackInfo::is_construct_call = 0
  __ Mov(x10, argc);
  __ Stp(x10, xzr, MemOperand(x0, 2 * kPointerSize));

  const int kStackUnwindSpace = argc + FCA::kArgsLength + 1;
  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  MemOperand context_restore_operand(
      fp, (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument
  int return_value_offset = 0;
  if (is_store) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);

  const int spill_offset = 1 + kApiStackSpace;
  __ CallApiFunctionAndReturn(api_function_address,
                              thunk_ref,
                              kStackUnwindSpace,
                              spill_offset,
                              return_value_operand,
                              &context_restore_operand);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- sp[0]                  : name
  //  -- sp[8 - kArgsLength*8]  : PropertyCallbackArguments object
  //  -- ...
  //  -- x2                     : api_function_address
  // -----------------------------------

  Register api_function_address = ApiGetterDescriptor::function_address();
  DCHECK(api_function_address.is(x2));

  __ Mov(x0, masm->StackPointer());  // x0 = Handle<Name>
  __ Add(x1, x0, 1 * kPointerSize);  // x1 = PCA

  const int kApiStackSpace = 1;

  // Allocate space for CallApiFunctionAndReturn can store some scratch
  // registeres on the stack.
  const int kCallApiFunctionSpillSpace = 4;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, x10, kApiStackSpace + kCallApiFunctionSpillSpace);

  // Create PropertyAccessorInfo instance on the stack above the exit frame with
  // x1 (internal::Object** args_) as the data.
  __ Poke(x1, 1 * kPointerSize);
  __ Add(x1, masm->StackPointer(), 1 * kPointerSize);  // x1 = AccessorInfo&

  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  const int spill_offset = 1 + kApiStackSpace;
  __ CallApiFunctionAndReturn(api_function_address,
                              thunk_ref,
                              kStackUnwindSpace,
                              spill_offset,
                              MemOperand(fp, 6 * kPointerSize),
                              NULL);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
