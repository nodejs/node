// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/ppc/assembler-ppc.h"
#include "src/codegen/ppc/register-ppc.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/ppc/maglev-assembler-ppc-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());

  // Deopt when result would be -0.
  __ CmpS32(value, Operand::Zero(), r0);
  __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);

  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    // On CPUs that do not support overflow detection, always deopt.
    __ neg(out, value, SetOE);
    __ MoveToCrFromXer(cr0);

    // Output register must not be a register input into the eager deopt info.
    DCHECK_REGLIST_EMPTY(RegList{out} &
                         GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
    __ EmitEagerDeoptIf(overflow32, DeoptimizeReason::kOverflow, this);
  } else {
    // For PPC8, we don't have direct overflow detection for negation.
    // We need to check for the special overflow case: value == INT32_MIN.
    // If so, deopt; otherwise, perform negation.
    __ CmpS32(value, Operand(0x80000000), r0);
    __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);
    __ neg(out, value);
  }

  __ extsw(out, out);
}

void Int32AbsWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register out = ToRegister(result());
  Label done;

  __ CmpS32(out, Operand::Zero(), r0);
  __ bge(&done);

  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    __ neg(out, out, SetOE);
    __ MoveToCrFromXer(cr0);
    DCHECK_REGLIST_EMPTY(RegList{out} &
                         GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
    __ EmitEagerDeoptIf(overflow32, DeoptimizeReason::kOverflow, this);
  } else {
    // For PPC8, we don't have direct overflow detection for negation.
    // We need to check for the special overflow case: value == INT32_MIN.
    // If so, deopt; otherwise, perform negation.
    __ CmpS32(out, Operand(0x80000000), r0);
    __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);
    __ neg(out, out);
  }

  __ bind(&done);
  __ extsw(out, out);
}

void Int32Increment::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Increment::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ AddS32(out, value, Operand(1));
}

void Int32Decrement::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Decrement::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ SubS32(out, value, Operand(1));
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    __ li(r0, Operand(1));
    __ add(out, value, r0, SetOE);
    __ MoveToCrFromXer(cr0);

    // Output register must not be a register input into the eager deopt info.
    DCHECK_REGLIST_EMPTY(RegList{out} &
                         GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
    __ EmitEagerDeoptIf(overflow32, DeoptimizeReason::kOverflow, this);
  } else {
    // For PPC8, we don't have direct overflow detection for addition.
    // We need to check for the special overflow case: value == INT32_MAX.
    // If so, deopt; otherwise, perform increment.
    __ CmpS32(value, Operand(0x7fffffff), r0);
    __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);
    __ addi(out, value, Operand(1));
  }

  __ extsw(out, out);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    __ li(r0, Operand(-1));
    __ add(out, value, r0, SetOE);
    __ MoveToCrFromXer(cr0);

    // Output register must not be a register input into the eager deopt info.
    DCHECK_REGLIST_EMPTY(RegList{out} &
                         GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
    __ EmitEagerDeoptIf(overflow32, DeoptimizeReason::kOverflow, this);
  } else {
    // For PPC8, we don't have direct overflow detection for subtraction.
    // We need to check for the special overflow case: value == INT32_MIN.
    // If so, deopt; otherwise, perform decrement.
    __ CmpS32(value, Operand(0x80000000), r0);
    __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);
    __ addi(out, value, Operand(-1));
  }

  __ extsw(out, out);
}

int BuiltinStringFromCharCode::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void BuiltinStringFromCharCode::SetValueLocationConstraints() {
  if (CharCodeInput().node()->Is<Int32Constant>()) {
    UseAny(CharCodeInput());
  } else {
    UseAndClobberRegister(CharCodeInput());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  Register result_string = ToRegister(result());
  if (Int32Constant* constant =
          CharCodeInput().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value() & 0xFFFF;
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      // Ensure that {result_string} never aliases {scratch}, otherwise the
      // store will fail.
      bool reallocate_result = (scratch == result_string);
      if (reallocate_result) {
        result_string = temps.AcquireScratch();
      }
      DCHECK(scratch != result_string);
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ Move(scratch, char_code);
      __ StoreU16(scratch,
                  FieldMemOperand(result_string,
                                  OFFSET_OF_DATA_START(SeqTwoByteString)),
                  r0);
      if (reallocate_result) {
        __ Move(ToRegister(result()), result_string);
      }
    }
  } else {
    __ StringFromCharCode(register_snapshot(), nullptr, result_string,
                          ToRegister(CharCodeInput()), scratch,
                          MaglevAssembler::CharCodeMaskMode::kMustApplyMask);
  }
}

void InlinedAllocation::SetValueLocationConstraints() {
  UseRegister(AllocationBlockInput());
  if (offset() == 0) {
    DefineSameAsFirst(this);
  } else {
    DefineAsRegister(this);
  }
}

void InlinedAllocation::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  if (offset() != 0) {
    __ AddS64(ToRegister(result()), ToRegister(AllocationBlockInput()),
              Operand(offset()), r0);
  }
}

void ArgumentsLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void ArgumentsLength::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register argc = ToRegister(result());
  __ LoadU64(argc, MemOperand(fp, StandardFrameConstants::kArgCOffset), r0);
  __ SubS64(argc, argc, Operand(1));  // Remove receiver.
}

void RestLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void RestLength::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register length = ToRegister(result());
  Label done;
  __ LoadU64(length, MemOperand(fp, StandardFrameConstants::kArgCOffset), r0);
  __ SubS32(length, length, Operand(formal_parameter_count() + 1), r0, SetRC);
  __ bge(&done);
  __ Move(length, 0);
  __ bind(&done);
  __ UncheckedSmiTagInt32(length);
}

int CheckedObjectToIndex::MaxCallStackArgs() const { return 0; }

void CheckedIntPtrToInt32::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void CheckedIntPtrToInt32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(ValueInput());
  Label* deopt = __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32);

  __ CmpS64(input_reg, Operand(std::numeric_limits<int32_t>::max()), r0);
  __ bgt(deopt);
  __ CmpS64(input_reg, Operand(std::numeric_limits<int32_t>::min()), r0);
  __ blt(deopt);
}

void CheckFloat64SameValue::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  set_temporaries_needed((value().get_scalar() == 0) ? 1 : 0);
  set_double_temporaries_needed(value().is_nan() ? 0 : 1);
}

void CheckFloat64SameValue::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Label* fail = __ GetDeoptLabel(this, deoptimize_reason());
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  DoubleRegister double_scratch = temps.AcquireScratchDouble();
  DoubleRegister target = ToDoubleRegister(ValueInput());
  if (value().is_nan()) {
    __ JumpIfNotNan(target, fail);
  } else {
    __ Move(double_scratch, value());
    __ CompareFloat64AndJumpIf(double_scratch, target, kNotEqual, fail, fail);
    if (value().get_scalar() == 0) {  // If value is +0.0 or -0.0.
      Register scratch = temps.AcquireScratch();
      __ MovDoubleToInt64(scratch, target);
      __ CmpU64(scratch, Operand(0), r0);
      __ JumpIf(value().get_bits() == 0 ? kNotEqual : kEqual, fail);
    }
  }
}
void Int32Add::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Int32Add::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());
  __ add(out, left, right);
  __ extsw(out, out);
}

void Int32Subtract::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32Subtract::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());
  __ sub(out, left, right);
  __ extsw(out, out);
}

void Int32Multiply::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32Multiply::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ mullw(out, left, right);

  // Making sure that the 32-bit output is zero-extended.
  __ ZeroExtWord32(out, out);
}

void Int32MultiplyOverflownBits::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Int32MultiplyOverflownBits::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ mulhw(out, left, right);

  // Making sure that the 32-bit output is zero-extended.
  __ ZeroExtWord32(out, out);
}

void Int32Divide::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32Divide::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());
  ZoneLabelRef do_division(masm), done(masm);

  // TODO(leszeks): peephole optimise division by a constant.

  __ CmpS32(right, Operand(0), r0);
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef do_division, ZoneLabelRef done,
         Register right, Register left, Register out) {
        Label right_is_neg;
        // Truncated value of anything divided by 0 is 0.
        __ bne(&right_is_neg);
        __ mov(out, Operand::Zero());
        __ b(*done);

        // Return -left if right = -1.
        // This avoids a hardware exception if left = INT32_MIN.
        // Int32Divide returns a truncated value and according to
        // ecma262#sec-toint32, the truncated value of INT32_MIN
        // is INT32_MIN.
        __ bind(&right_is_neg);
        __ CmpS32(right, Operand(-1), r0);
        __ bne(*do_division);
        __ neg(out, left);
        __ b(*done);
      },
      do_division, done, right, left, out);

  __ bind(*do_division);
  __ divw(out, left, right);
  __ bind(*done);
  __ extsw(out, out);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    __ add(out, left, right, SetOE);
    __ MoveToCrFromXer(cr0);
    // The output register shouldn't be a register input into the eager deopt
    // info.
    DCHECK_REGLIST_EMPTY(RegList{out} &
                         GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
    __ EmitEagerDeoptIf(overflow32, DeoptimizeReason::kOverflow, this);
    __ extsw(out, out);
  } else {
    // For PPC8, we don't have direct overflow detection for addition.
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    Register temp = temps.AcquireScratch();
    Register temp2 = temps.AcquireScratch();
    __ extsw(temp, left);
    __ extsw(temp2, right);
    __ add(temp, temp, temp2);
    __ extsw(out, temp);
    __ CmpS64(temp, out);
    __ EmitEagerDeoptIf(ne, DeoptimizeReason::kOverflow, this);
  }
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());
  if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
    __ sub(out, left, right, SetOE);
    __ MoveToCrFromXer(cr0);
    // The output register shouldn't be a register input into the eager deopt
    // info.
    DCHECK_REGLIST_EMPTY(RegList{out} &
                         GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
    __ EmitEagerDeoptIf(overflow32, DeoptimizeReason::kOverflow, this);
    __ extsw(out, out);
  } else {
    // For PPC8, we don't have direct overflow detection for subtraction.
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    Register temp = temps.AcquireScratch();
    Register temp2 = temps.AcquireScratch();
    __ extsw(temp, left);
    __ extsw(temp2, right);
    __ sub(temp, temp, temp2);
    __ extsw(out, temp);
    __ CmpS64(temp, out);
    __ EmitEagerDeoptIf(ne, DeoptimizeReason::kOverflow, this);
  }
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise multiplication by a constant.

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register temp = temps.AcquireScratch();
  __ mullw(out, left, right, SetOE, SetRC);

  DCHECK_REGLIST_EMPTY(RegList{temp, out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);

  // Making sure that the 32-bit output is zero-extended.
  __ ZeroExtWord32(out, out);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ CmpS32(out, Operand::Zero(), r0);
  __ bne(&end);

  __ xor_(temp, left, right);
  __ CmpS32(temp, Operand::Zero(), r0);
  // If one of them is negative, we must have a -0 result, which is non-int32,
  // so deopt.
  __ EmitEagerDeoptIf(lt, DeoptimizeReason::kOverflow, this);

  __ bind(&end);
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise division by a constant.

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ CmpS32(right, Operand::Zero(), r0);
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register left,
         Register right, Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // TODO(leszeks): Using kNotInt32 here, but in same places
        // kDivisionByZerokMinusZero/kMinusZero/kOverflow would be better. Right
        // now all eager deopts in a node have to be the same -- we should allow
        // a node to emit multiple eager deopts with different reasons.
        Label* deopt = __ GetDeoptLabel(node, DeoptimizeReason::kNotInt32);

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        __ JumpIf(eq, deopt);

        // Check if {left} is zero, as that would produce minus zero.
        __ CmpS32(left, Operand::Zero(), r0);
        __ JumpIf(eq, deopt);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ CmpS32(left, Operand(kMinInt), r0);
        __ JumpIf(ne, *done);
        __ CmpS32(right, Operand(-1), r0);
        __ JumpIf(ne, *done);
        __ JumpToDeopt(deopt);
      },
      done, left, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  __ divw(out, left, right, LeaveOE, SetRC);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kNotInt32, this);

  // Check that the remainder is zero.
  __ mullw(r0, out, right);
  __ sub(r0, left, r0, LeaveOE, SetRC);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, this);

  __ extsw(out, out);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseAndClobberRegister(LeftInput());
  UseAndClobberRegister(RightInput());
  DefineAsRegister(this);
}
void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // If AreAliased(lhs, rhs):
  //   deopt if lhs < 0  // Minus zero.
  //   0

  // Using same algorithm as in EffectControlLinearizer:
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let lhs_abs = -lsh in
  //     let res = lhs_abs % rhs in
  //     deopt if res == 0
  //     -res
  //   else
  //     let msk = rhs - 1 in
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs

  Register lhs = ToRegister(LeftInput());
  Register rhs = ToRegister(RightInput());
  Register out = ToRegister(result());

  static constexpr DeoptimizeReason deopt_reason =
      DeoptimizeReason::kDivisionByZero;

  if (lhs == rhs) {
    // For the modulus algorithm described above, lhs and rhs must not alias
    // each other.
    __ CmpS32(lhs, Operand::Zero(), r0);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(lt, deopt_reason, this);
    __ Move(out, 0);
    return;
  }

  DCHECK_NE(lhs, rhs);

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);
  __ CmpS32(rhs, Operand(0), r0);
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ neg(rhs, rhs, LeaveOE, SetRC);
        __ bne(*rhs_checked);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      rhs_checked, rhs, this);
  __ bind(*rhs_checked);

  __ CmpS32(lhs, Operand(0), r0);
  __ JumpToDeferredIf(
      lt,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Register out, Int32ModulusWithOverflow* node) {
        __ neg(lhs, lhs);
        __ ModU32(out, lhs, rhs);
        __ neg(out, out, LeaveOE, SetRC);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
        // only allows one deopt reason per IR.
        __ bne(*done);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      done, lhs, rhs, out, this);

  Label rhs_not_power_of_2;
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register mask = temps.AcquireScratch();
  __ addi(mask, rhs, Operand(-1));
  __ and_(r0, mask, rhs, SetRC);
  __ JumpIf(ne, &rhs_not_power_of_2);

  // {rhs} is power of 2.
  __ and_(out, mask, lhs, SetRC);
  __ Jump(*done);
  // {mask} can be reused from now on.
  temps.IncludeScratch(mask);

  __ bind(&rhs_not_power_of_2);
  __ ModU32(out, lhs, rhs);
  __ bind(*done);
  __ extsw(out, out);
}

#define DEF_BITWISE_BINOP(Instruction, opcode)                   \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(LeftInput());                                    \
    UseRegister(RightInput());                                   \
    DefineAsRegister(this);                                      \
  }                                                              \
                                                                 \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(LeftInput());                     \
    Register right = ToRegister(RightInput());                   \
    Register out = ToRegister(result());                         \
    __ opcode(out, left, right);                                 \
    __ extsw(out, out);                                          \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_)
DEF_BITWISE_BINOP(Int32BitwiseOr, orx)
DEF_BITWISE_BINOP(Int32BitwiseXor, xor_)
#undef DEF_BITWISE_BINOP

#define DEF_SHIFT_BINOP(Instruction, opcode)                     \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(LeftInput());                                    \
    if (RightInput().node()->Is<Int32Constant>()) {              \
      UseAny(RightInput());                                      \
    } else {                                                     \
      UseRegister(RightInput());                                 \
    }                                                            \
    DefineAsRegister(this);                                      \
  }                                                              \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(LeftInput());                     \
    Register out = ToRegister(result());                         \
    if (Int32Constant* constant =                                \
            RightInput().node()->TryCast<Int32Constant>()) {     \
      uint32_t shift = constant->value() & 31;                   \
      if (shift == 0) {                                          \
        __ Move(out, left);                                      \
        return;                                                  \
      }                                                          \
      __ opcode(out, left, Operand(shift));                      \
      __ extsw(out, out);                                        \
    } else {                                                     \
      MaglevAssembler::TemporaryRegisterScope temps(masm);       \
      Register scratch = temps.AcquireScratch();                 \
      Register right = ToRegister(RightInput());                 \
      __ andi(scratch, right, Operand(31));                      \
      __ opcode(out, left, scratch);                             \
      __ extsw(out, out);                                        \
    }                                                            \
  }
DEF_SHIFT_BINOP(Int32ShiftLeft, ShiftLeftU32)
DEF_SHIFT_BINOP(Int32ShiftRight, ShiftRightS32)
DEF_SHIFT_BINOP(Int32ShiftRightLogical, ShiftRightU32)
#undef DEF_SHIFT_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ notx(out, value);
  __ extsw(out, out);
}

void Float64Add::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ AddF64(out, left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ SubF64(out, left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ MulF64(out, left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ DivF64(out, left, right);
}

void Float64Modulus::SetValueLocationConstraints() {
  UseFixed(LeftInput(), d1);
  UseFixed(RightInput(), d2);
  DefineSameAsFirst(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush({r3, r4, r5, r6, r7, r8, r9, r10, r11});
  __ PrepareCallCFunction(0, 2);
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
  __ MultiPop({r3, r4, r5, r6, r7, r8, r9, r10, r11});
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ fneg(out, value);
}

void Float64Abs::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ fabs(out, in);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  if (kind_ == Kind::kNearest) {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireScratchDouble();
    DoubleRegister temp2 = temps.AcquireScratchDouble();
    // frin rounds to even on tie, while JS expects it to round towards
    // +Infinity. Fix the difference by checking if we rounded down by exactly
    // 0.5, and if so, round to the other side.
    __ fmr(temp, in);
    __ frin(out, in);
    __ fsub(temp, temp, out);
    __ Move(temp2, 0.5);
    __ fcmpu(temp, temp2);
    Label done;
    __ JumpIf(ne, &done, Label::kNear);
    __ fadd(out, temp2, out);
    __ fadd(out, temp2, out);
    // Add fcpsgn make sure -0.5 rounds to -0.0 instead of 0.0
    __ fcpsgn(out, in, out);
    __ bind(&done);
  } else if (kind_ == Kind::kCeil) {
    __ frip(out, in);
  } else if (kind_ == Kind::kFloor) {
    __ frim(out, in);
  }
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(LeftInput(), d1);
  UseFixed(RightInput(), d2);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush({r3, r4, r5, r6, r7, r8, r9, r10, r11});
  __ PrepareCallCFunction(0, 2);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 0, 2);
  __ MultiPop({r3, r4, r5, r6, r7, r8, r9, r10, r11});
}

void Float64Min::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Float64Min::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ MinF64(out, left, right);
}

void Float64Max::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Float64Max::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ MaxF64(out, left, right);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(ValueInput(), d1);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush({r3, r4, r5, r6, r7, r8, r9, r10, r11});
  __ PrepareCallCFunction(0, 1);
  __ CallCFunction(ieee_function_ref(), 0, 1);
  __ MultiPop({r3, r4, r5, r6, r7, r8, r9, r10, r11});
}
int Float64Ieee754Binary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Binary::SetValueLocationConstraints() {
  UseFixed(LeftInput(), d1);
  UseFixed(RightInput(), d2);
  DefineSameAsFirst(this);
}
void Float64Ieee754Binary::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush({r3, r4, r5, r6, r7, r8, r9, r10, r11});
  __ PrepareCallCFunction(0, 2);
  __ CallCFunction(ieee_function_ref(), 0, 2);
  __ MultiPop({r3, r4, r5, r6, r7, r8, r9, r10, r11});
}

void Float64Sqrt::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void Float64Sqrt::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister result_register = ToDoubleRegister(result());
  __ fsqrt(result_register, value);
}

void LoadTypedArrayLength::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void LoadTypedArrayLength::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register object = ToRegister(ValueInput());
  Register result_register = ToRegister(result());
  if (v8_flags.debug_code) {
    __ AssertObjectType(object, JS_TYPED_ARRAY_TYPE,
                        AbortReason::kUnexpectedValue);
  }

  __ LoadBoundedSizeFromObject(result_register, object,
                               JSTypedArray::kRawByteLengthOffset);
  int shift_size = ElementsKindToShiftSize(elements_kind_);
  if (shift_size > 0) {
    // TODO(leszeks): Merge this shift with the one in LoadBoundedSize.
    DCHECK(shift_size == 1 || shift_size == 2 || shift_size == 3);
    __ ShiftRightU64(result_register, result_register, Operand(shift_size));
  }
}

int CheckJSDataViewBounds::MaxCallStackArgs() const { return 1; }
void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(IndexInput());
  UseRegister(ByteLengthInput());
  int element_size = compiler::ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    set_temporaries_needed(1);
  }
}
void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register index = ToRegister(IndexInput());
  Register byte_length = ToRegister(ByteLengthInput());

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register limit = byte_length;

  int element_size = compiler::ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    limit = temps.Acquire();
    __ SubS64(limit, byte_length, Operand(element_size - 1), r0, LeaveOE,
              SetRC);
    __ EmitEagerDeoptIf(lt, DeoptimizeReason::kOutOfBounds, this);
  }
  __ CmpS32(index, limit);
  __ EmitEagerDeoptIf(ge, DeoptimizeReason::kOutOfBounds, this);
}

void ChangeFloat64ToHoleyFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void ChangeFloat64ToHoleyFloat64::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  // A Float64 value could contain a NaN with the bit pattern that has a special
  // interpretation in the HoleyFloat64 representation, so we need to canicalize
  // those before changing representation.
  __ CanonicalizeNaN(value, value);
}

void HoleyFloat64ToSilencedFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void HoleyFloat64ToSilencedFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  // The hole value is a signalling NaN, so just silence it to get the
  // float64 value.
  __ CanonicalizeNaN(ToDoubleRegister(this->result()),
                     ToDoubleRegister(ValueInput()));
}

void Float64ToSilencedFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void Float64ToSilencedFloat64::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // The hole value is a signalling NaN, so just silence it to get the
  // float64 value.
  __ CanonicalizeNaN(ToDoubleRegister(this->result()),
                     ToDoubleRegister(ValueInput()));
}

void UnsafeFloat64ToHoleyFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void UnsafeFloat64ToHoleyFloat64::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {}

namespace {

enum class ReduceInterruptBudgetType { kLoop, kReturn };

void HandleInterruptsAndTiering(MaglevAssembler* masm, ZoneLabelRef done,
                                Node* node, ReduceInterruptBudgetType type,
                                Register scratch0) {
  if (v8_flags.verify_write_barriers) {
    // The safepoint/interrupt might trigger GC.
    __ ResetLastYoungAllocation();
  }

  // For loops, first check for interrupts. Don't do this for returns, as we
  // can't lazy deopt to the end of a return.
  if (type == ReduceInterruptBudgetType::kLoop) {
    Label next;
    // Here, we only care about interrupts since we've already guarded against
    // real stack overflows on function entry.
    {
      Register stack_limit = scratch0;
      __ LoadStackLimit(stack_limit, StackLimitKind::kInterruptStackLimit, r0);
      __ CmpU64(sp, stack_limit);
      __ bgt(&next);
    }

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ LoadU64(function,
                 MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ Push(function);
      // Move into kContextRegister after the load into scratch0, just in case
      // scratch0 happens to be kContextRegister.
      __ Move(kContextRegister, masm->native_context().object());
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ b(*done);  // All done, continue.
    __ bind(&next);
  }

  // No pending interrupts. Call into the TieringManager if needed.
  {
    SaveRegisterStateForCall save_register_state(masm,
                                                 node->register_snapshot());
    Register function = scratch0;
    __ LoadU64(function,
               MemOperand(fp, StandardFrameConstants::kFunctionOffset));
    __ Push(function);
    // Move into kContextRegister after the load into scratch0, just in case
    // scratch0 happens to be kContextRegister.
    __ Move(kContextRegister, masm->native_context().object());
    // Note: must not cause a lazy deopt!
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
    save_register_state.DefineSafepoint();
  }
  __ b(*done);
}

void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   Register feedback_cell,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register budget = temps.AcquireScratch();
  __ LoadU32(
      budget,
      FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset), r0);
  __ SubS32(budget, budget, Operand(amount), r0);
  __ StoreU32(
      budget,
      FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset), r0);
  ZoneLabelRef done(masm);
  __ CmpS32(budget, Operand(0), r0);
  __ JumpToDeferredIf(lt, HandleInterruptsAndTiering, done, node, type, budget);
  __ bind(*done);
}

}  // namespace

int ReduceInterruptBudgetForLoop::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForLoop::SetValueLocationConstraints() {
  UseRegister(FeedbackCellInput());
}
void ReduceInterruptBudgetForLoop::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ToRegister(FeedbackCellInput()),
                                ReduceInterruptBudgetType::kLoop, amount());
}

int ReduceInterruptBudgetForReturn::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForReturn::SetValueLocationConstraints() {
  UseRegister(FeedbackCellInput());
  set_temporaries_needed(1);
}
void ReduceInterruptBudgetForReturn::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ToRegister(FeedbackCellInput()),
                                ReduceInterruptBudgetType::kReturn, amount());
}

// ---
// Control nodes
// ---
void Return::SetValueLocationConstraints() {
  UseFixed(ValueInput(), kReturnRegister0);
}
void Return::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  DCHECK_EQ(ToRegister(ValueInput()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size =
      masm->compilation_info()->toplevel_compilation_unit()->parameter_count();

  // We're not going to continue execution, so we can use an arbitrary register
  // here instead of relying on temporaries from the register allocator.
  Register actual_params_size = r7;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to reuse the
  // incoming argc's register (if it's still valid).
  __ LoadU64(actual_params_size,
             MemOperand(fp, StandardFrameConstants::kArgCOffset), r0);

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label drop_dynamic_arg_size;
  __ CmpS32(actual_params_size, Operand(formal_params_size), r0);
  __ bgt(&drop_dynamic_arg_size);
  __ mov(actual_params_size, Operand(formal_params_size));
  __ bind(&drop_dynamic_arg_size);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(actual_params_size);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
