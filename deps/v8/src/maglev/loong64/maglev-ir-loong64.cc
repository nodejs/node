// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/loong64/assembler-loong64-inl.h"
#include "src/codegen/loong64/register-loong64.h"
#include "src/maglev/loong64/maglev-assembler-loong64-inl.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

namespace {

std::optional<int32_t> TryGetAddImmediateInt32ConstantInput(Node* node,
                                                            int index) {
  if (auto res = node->TryGetInt32ConstantInput(index)) {
    if (is_int12(*res)) {
      return res;
    }
  }
  return {};
}

std::optional<int32_t> TryGetLogicalImmediateInt32ConstantInput(Node* node,
                                                                int index) {
  if (auto res = node->TryGetInt32ConstantInput(index)) {
    if (*res <= 0) {
      return {};
    }
    if (is_uint12(*res)) {
      return res;
    }
  }
  return {};
}

}  // namespace

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  DCHECK(!AreAliased(value, out));
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));

  // Deopt when result would be -0.
  // Only Neg(INT32_MIN) will overflow, Neg(0x80000000) = 0x80000000
  // Neg(value) == value iff (value == zero || value == 0x8000000)
  static_assert(Int32NegateWithOverflow::kProperties.can_eager_deopt());
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");

  __ sub_w(out, zero_reg, value);
  // Deopt when Neg(value) == value, in which case value is zero or INT32_MIN.
  __ MacroAssembler::Branch(fail, equal, value, Operand(out));
}

void Int32AbsWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register out = ToRegister(result());

  Label done;
  DCHECK(ToRegister(ValueInput()) == out);
  __ MacroAssembler::Branch(&done, greater_equal, out, Operand(zero_reg),
                            Label::Distance::kNear);

  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");
  __ sub_w(out, zero_reg, out);
  // Deopt when Neg(input) < 0, in this case input is INT32_MIN.
  __ MacroAssembler::Branch(fail, lt, out, Operand(zero_reg));

  __ bind(&done);
}

void Int32Increment::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Increment::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ Add_w(out, value, Operand(1));
}

void Int32Decrement::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Decrement::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ Sub_w(out, value, Operand(1));
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));

  if (AreAliased(value, out)) {
    DCHECK(!AreAliased(value, scratch));
    __ mov(scratch, value);
    value = scratch;
  }
  __ Add_w(out, value, Operand(1));

  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");
  __ MacroAssembler::Branch(fail, le, out, Operand(value));
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));

  if (AreAliased(value, out)) {
    DCHECK(!AreAliased(value, scratch));
    __ mov(scratch, value);
    value = scratch;
  }
  __ Sub_w(out, value, Operand(1));

  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");
  __ MacroAssembler::Branch(fail, ge, out, Operand(value));
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
  Register scratch = temps.Acquire();
  Register result_string = ToRegister(result());
  if (Int32Constant* constant =
          CharCodeInput().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value() & 0xFFFF;
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ li(scratch, char_code);
      __ St_h(scratch, FieldMemOperand(result_string,
                                       OFFSET_OF_DATA_START(SeqTwoByteString)));
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
    __ Add_d(ToRegister(result()), ToRegister(AllocationBlockInput()),
             Operand(offset()));
  }
}

void ArgumentsLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void ArgumentsLength::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register out = ToRegister(result());
  __ Ld_d(out, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Sub_d(out, out, Operand(1));  // Remove receiver.
}

void RestLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void RestLength::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register length = ToRegister(result());
  Label done;
  __ Ld_d(length, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Sub_d(length, length, Operand(formal_parameter_count() + 1));
  __ MacroAssembler::Branch(&done, greater_equal, length, Operand(zero_reg),
                            Label::Distance::kNear);
  __ mov(length, zero_reg);
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
  __ MacroAssembler::Branch(__ GetDeoptLabel(this, DeoptimizeReason::kNotInt32),
                            gt, input_reg,
                            Operand(std::numeric_limits<int32_t>::max()));
  __ MacroAssembler::Branch(__ GetDeoptLabel(this, DeoptimizeReason::kNotInt32),
                            lt, input_reg,
                            Operand(std::numeric_limits<int32_t>::min()));
}

void CheckFloat64SameValue::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  set_temporaries_needed((value().get_scalar() == 0) ? 1 : 0);
  set_double_temporaries_needed(
      value().is_nan() || (value().get_scalar() == 0) ? 0 : 1);
}
void CheckFloat64SameValue::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Label* fail = __ GetDeoptLabel(this, deoptimize_reason());
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  DoubleRegister target = ToDoubleRegister(ValueInput());
  DoubleRegister double_scratch = temps.AcquireScratchDouble();
  if (value().is_nan()) {
    __ JumpIfNotNan(target, fail);
  } else if (value().get_scalar() == 0) {  // If value is +0.0 or -0.0.
    Register scratch = temps.AcquireScratch();
    __ Move(double_scratch, value().get_scalar());
    __ CompareF64(target, double_scratch, CUN);
    __ BranchTrueF(fail);
    __ movfr2gr_d(scratch, target);
    if (value().get_bits() == 0) {
      __ MacroAssembler::Branch(fail, lt, scratch, Operand(zero_reg));
    } else {
      __ MacroAssembler::Branch(fail, ge, scratch, Operand(zero_reg));
    }
  } else {
    __ Move(double_scratch, value().get_scalar());
    __ CompareFloat64AndJumpIf(double_scratch, target, kNotEqual, fail, fail);
  }
}

void Int32Add::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetAddImmediateInt32ConstantInput(this, kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineAsRegister(this);
}

void Int32Add::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register out = ToRegister(result());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    int32_t right = *right_const;
    __ Add_w(out, left, Operand(right));
  } else {
    Register right = ToRegister(RightInput());
    __ Add_w(out, left, Operand(right));
  }
}

void Int32Subtract::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetAddImmediateInt32ConstantInput(this, kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineAsRegister(this);
}

void Int32Subtract::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register out = ToRegister(result());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    int32_t right = *right_const;
    __ Sub_w(out, left, Operand(right));
  } else {
    Register right = ToRegister(RightInput());
    __ Sub_w(out, left, Operand(right));
  }
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
  __ Mul_w(out, left, Operand(right));
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
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();

  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ Mul_w(scratch, left, Operand(right));
  __ srai_d(scratch, scratch, 32);
  __ Mulh_w(out, left, Operand(right));
  __ xor_(out, out, scratch);
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

  // TODO(leszeks): peephole optimise division by a constant.

  // On LOONG64, integer division does not trap on overflow or division by zero.
  // - Division by zero returns Undefined.
  // - Signed overflow (INT_MIN / -1) returns INT_MIN.
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  __ Div_w(scratch, left, Operand(right));
  __ maskeqz(out, scratch, right);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetAddImmediateInt32ConstantInput(this, kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineAsRegister(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register out = ToRegister(result());
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    int32_t right = *right_const;
    if (out == left) {
      __ Move(scratch, left);
      left = scratch;
    }
    __ Add_w(out, left, Operand(right));
    __ RecordComment("-- Jump to eager deopt");
    __ MacroAssembler::Branch(fail, right > 0 ? lt : gt, out, Operand(left));
  } else {
    Register right = ToRegister(RightInput());
    __ Add_d(scratch, left, right);
    __ Add_w(out, left, right);
    __ RecordComment("-- Jump to eager deopt");
    __ MacroAssembler::Branch(fail, ne, scratch, Operand(out));
  }
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetAddImmediateInt32ConstantInput(this, kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineAsRegister(this);
}
void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register out = ToRegister(result());
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    int32_t right = *right_const;
    if (out == left) {
      __ Move(scratch, left);
      left = scratch;
    }
    __ Sub_w(out, left, Operand(right));
    __ RecordComment("-- Jump to eager deopt");
    __ MacroAssembler::Branch(fail, right > 0 ? gt : lt, out, Operand(left));
  } else {
    Register right = ToRegister(RightInput());
    __ Sub_d(scratch, left, right);
    __ Sub_w(out, left, right);
    __ RecordComment("-- Jump to eager deopt");
    __ MacroAssembler::Branch(fail, ne, scratch, Operand(out));
  }
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
  set_temporaries_needed(2);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise multiplication by a constant.

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.AcquireScratch();
  }

  Register scratch = temps.AcquireScratch();
  __ mulw_d_w(res, left, right);
  __ slli_w(scratch, res, 0);

  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");
  __ MacroAssembler::Branch(fail, ne, res, Operand(scratch));

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ MacroAssembler::Branch(&end, ne, res, Operand(zero_reg),
                            Label::Distance::kNear);
  {
    Register maybeNegative = scratch;
    __ add_w(maybeNegative, left, right);
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ RecordComment("-- Jump to eager deopt if the result is negative zero");
    Label* deopt_label = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
    __ MacroAssembler::Branch(deopt_label, lt, maybeNegative,
                              Operand(zero_reg));
  }

  __ bind(&end);
  if (out_alias_input) {
    __ Move(out, res);
  }
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

  ZoneLabelRef done(masm);
  Label* deferred_overflow_checks = __ MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register left,
         Register right, Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // TODO(leszeks): Using kNotInt32 here, but in same places
        // kDivisionByZerokMinusZero/kMinusZero/kOverflow would be better. Right
        // now all eager deopts in a node have to be the same -- we should allow
        // a node to emit multiple eager deopts with different reasons.
        Label* deopt = __ GetDeoptLabel(node, DeoptimizeReason::kNotInt32);

        // Check if {right} is zero.
        __ MacroAssembler::Branch(deopt, eq, right, Operand(zero_reg));

        // Check if {left} is zero, as that would produce minus zero.
        __ MacroAssembler::Branch(deopt, eq, left, Operand(zero_reg));

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ MacroAssembler::Branch(*done, ne, left, Operand(kMinInt));
        __ MacroAssembler::Branch(*done, ne, right, Operand(-1));
        __ JumpToDeopt(deopt);
      },
      done, left, right, this);

  // Check if {right} is positive and not zero.
  __ MacroAssembler::Branch(deferred_overflow_checks, le, right,
                            Operand(zero_reg));
  __ bind(*done);

  // Perform the actual integer division.
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.AcquireScratch();
  }
  // Needs left and right are properly sign-extended.
  __ div_w(res, left, right);

  // Check that the remainder is zero.
  Register temp = temps.AcquireScratch();
  // Needs left and right are properly sign-extended.
  __ mod_w(temp, left, right);
  Label* deopt = __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32);
  __ MacroAssembler::Branch(deopt, ne, temp, Operand(zero_reg));

  __ Move(out, res);
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
  //
  // Using same algorithm as in EffectControlLinearizer:
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let lhs_abs = -lhs in
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

  // For the modulus algorithm described above, lhs and rhs must not alias
  // each other.
  if (lhs == rhs) {
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    Label* deopt = __ GetDeoptLabel(this, DeoptimizeReason::kDivisionByZero);
    __ RecordComment("-- Jump to eager deopt");
    __ MacroAssembler::Branch(deopt, lt, lhs, Operand(zero_reg));
    __ Move(out, zero_reg);
    return;
  }

  DCHECK(!AreAliased(lhs, rhs));

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);

  Label* deferred_rhs_check = __ MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ sub_w(rhs, zero_reg, rhs);
        __ MacroAssembler::Branch(*rhs_checked, ne, rhs, Operand(zero_reg));
        __ EmitEagerDeopt(node, deopt_reason);
      },
      rhs_checked, rhs, this);
  __ MacroAssembler::Branch(deferred_rhs_check, less_equal, rhs,
                            Operand(zero_reg));
  __ bind(*rhs_checked);

  Label* deferred_lhs_check = __ MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Register out, Int32ModulusWithOverflow* node) {
        MaglevAssembler::TemporaryRegisterScope temps(masm);
        Register lhs_abs = temps.AcquireScratch();
        __ sub_w(lhs_abs, zero_reg, lhs);
        Register res = lhs_abs;
        __ mod_w(res, lhs_abs, rhs);
        __ sub_w(out, zero_reg, res);
        __ MacroAssembler::Branch(*done, ne, res, Operand(zero_reg));
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
        // only allows one deopt reason per IR.
        __ EmitEagerDeopt(node, deopt_reason);
      },
      done, lhs, rhs, out, this);
  __ MacroAssembler::Branch(deferred_lhs_check, lt, lhs, Operand(zero_reg));

  Label rhs_not_power_of_2;
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  Register msk = temps.AcquireScratch();
  __ Sub_w(msk, rhs, Operand(1));
  __ and_(scratch, rhs, msk);
  __ MacroAssembler::Branch(&rhs_not_power_of_2, ne, scratch, Operand(zero_reg),
                            Label::kNear);
  // {rhs} is power of 2.
  __ and_(out, lhs, msk);
  __ MacroAssembler::Branch(*done, Label::kNear);

  __ bind(&rhs_not_power_of_2);
  __ mod_w(out, lhs, rhs);

  __ bind(*done);
}

#define DEF_BITWISE_BINOP(Instruction, opcode)                         \
  void Instruction::SetValueLocationConstraints() {                    \
    UseRegister(LeftInput());                                          \
    if (TryGetLogicalImmediateInt32ConstantInput(this, kRightIndex)) { \
      UseAny(RightInput());                                            \
    } else {                                                           \
      UseRegister(RightInput());                                       \
    }                                                                  \
    DefineAsRegister(this);                                            \
  }                                                                    \
                                                                       \
  void Instruction::GenerateCode(MaglevAssembler* masm,                \
                                 const ProcessingState& state) {       \
    Register left = ToRegister(LeftInput());                           \
    Register out = ToRegister(result());                               \
    if (!RightInput().operand().IsRegister()) {                        \
      auto right_const = TryGetInt32ConstantInput(kRightIndex);        \
      DCHECK(right_const);                                             \
      __ opcode(out, left, Operand(*right_const));                     \
    } else {                                                           \
      Register right = ToRegister(RightInput());                       \
      __ opcode(out, left, right);                                     \
    }                                                                  \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, And)
DEF_BITWISE_BINOP(Int32BitwiseOr, Or)
DEF_BITWISE_BINOP(Int32BitwiseXor, Xor)
#undef DEF_BITWISE_BINOP

#define DEF_SHIFT_BINOP(Instruction, opcode_reg, opcode_imm)        \
  void Instruction::SetValueLocationConstraints() {                 \
    UseRegister(LeftInput());                                       \
    if (TryGetInt32ConstantInput(kRightIndex)) {                    \
      UseAny(RightInput());                                         \
    } else {                                                        \
      UseRegister(RightInput());                                    \
    }                                                               \
    DefineAsRegister(this);                                         \
  }                                                                 \
                                                                    \
  void Instruction::GenerateCode(MaglevAssembler* masm,             \
                                 const ProcessingState& state) {    \
    Register out = ToRegister(result());                            \
    Register left = ToRegister(LeftInput());                        \
    if (auto right_const = TryGetInt32ConstantInput(kRightIndex)) { \
      int right = *right_const & 31;                                \
      __ opcode_imm(out, left, right);                              \
    } else {                                                        \
      Register right = ToRegister(RightInput());                    \
      __ opcode_reg(out, left, right);                              \
    }                                                               \
  }
DEF_SHIFT_BINOP(Int32ShiftLeft, sll_w, slli_w)
DEF_SHIFT_BINOP(Int32ShiftRight, sra_w, srai_w)
DEF_SHIFT_BINOP(Int32ShiftRightLogical, srl_w, srli_w)
#undef DEF_SHIFT_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ orn(out, zero_reg, value);
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
  __ fadd_d(out, left, right);
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
  __ fsub_d(out, left, right);
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
  __ fmul_d(out, left, right);
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
  __ fdiv_d(out, left, right);
}

int Float64Modulus::MaxCallStackArgs() const { return 0; }

void Float64Modulus::SetValueLocationConstraints() {
  UseFixed(LeftInput(), f0);
  UseFixed(RightInput(), f1);
  DefineSameAsFirst(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(0, 2);
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ fneg_d(out, value);
}

void Float64Abs::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ fabs_d(out, in);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());

  if (kind_ == Kind::kNearest) {
    // Round to the nearest representable value; if it is exactly halfway
    // between two representable values, round to +Infinity.
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireScratchDouble();
    DoubleRegister half_one = temps.AcquireScratchDouble();
    __ Move(half_one, 0.5);
    __ fadd_d(temp, in, half_one);
    __ Floor_d(temp, temp);
    // Reserve the sign bit when it is between -0.5 and -0.0.
    __ fcopysign_d(out, temp, in);
  } else if (kind_ == Kind::kCeil) {
    __ Ceil_d(out, in);
  } else if (kind_ == Kind::kFloor) {
    __ Floor_d(out, in);
  }
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(LeftInput(), f0);
  UseFixed(RightInput(), f1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(0, 2);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 0, 2);
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

  Label is_nan, done;
  __ Float64Min(out, left, right, &is_nan);
  __ MacroAssembler::Branch(&done);
  __ bind(&is_nan);
  __ fadd_d(out, left, right);
  __ bind(&done);
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

  Label is_nan, done;
  __ Float64Max(out, left, right, &is_nan);
  __ MacroAssembler::Branch(&done);
  __ bind(&is_nan);
  __ fadd_d(out, left, right);
  __ bind(&done);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(ValueInput(), f0);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(0, 1);
  __ CallCFunction(ieee_function_ref(), 0, 1);
}

int Float64Ieee754Binary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Binary::SetValueLocationConstraints() {
  UseFixed(LeftInput(), f0);
  UseFixed(RightInput(), f1);
  DefineSameAsFirst(this);
}
void Float64Ieee754Binary::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(0, 2);
  __ CallCFunction(ieee_function_ref(), 0, 2);
}

void Float64Sqrt::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void Float64Sqrt::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister result_register = ToDoubleRegister(result());
  __ fsqrt_d(result_register, value);
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
    __ srli_w(result_register, result_register, shift_size);
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
  Label fail, done;
  if (element_size > 1) {
    limit = temps.Acquire();
    __ Sub_w(limit, byte_length, Operand(element_size - 1));
    __ MacroAssembler::Branch(&fail, lt, limit, Operand(zero_reg),
                              Label::Distance::kNear);
  }
  __ MacroAssembler::Branch(&done, ult, index, Operand(limit),
                            Label::Distance::kNear);
  __ bind(&fail);
  __ EmitEagerDeopt(this, DeoptimizeReason::kOutOfBounds);

  __ bind(&done);
}

void ChangeFloat64ToHoleyFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void ChangeFloat64ToHoleyFloat64::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  // A Float64 value could contain a NaN with the bit pattern that has a special
  // interpretation in the HoleyFloat64 representation, so we need to canicalize
  // those before changing representation.
  __ FPUCanonicalizeNaN(ToDoubleRegister(result()),
                        ToDoubleRegister(ValueInput()));
}

void HoleyFloat64ToSilencedFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void HoleyFloat64ToSilencedFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  // The hole value is a signalling NaN, so just silence it to get the float64
  // value.
  __ FPUCanonicalizeNaN(ToDoubleRegister(result()),
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
  __ FPUCanonicalizeNaN(ToDoubleRegister(this->result()),
                        ToDoubleRegister(ValueInput()));
}

void UnsafeFloat64ToHoleyFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void UnsafeFloat64ToHoleyFloat64::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE
void HoleyFloat64ConvertHoleToUndefined::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
}
void HoleyFloat64ConvertHoleToUndefined::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  Label done;

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ JumpIfNotHoleNan(value, scratch, &done);
  __ Move(value, UndefinedNan());
  __ bind(&done);
}
#endif  // V8_ENABLE_UNDEFINED_DOUBLE

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
      __ LoadStackLimit(stack_limit, StackLimitKind::kInterruptStackLimit);
      __ MacroAssembler::Branch(&next, ugt, sp, Operand(stack_limit),
                                Label::Distance::kNear);
    }

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ Ld_d(function,
              MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ Push(function);
      // Move into kContextRegister after the load into scratch0, just in case
      // scratch0 happens to be kContextRegister.
      __ Move(kContextRegister, masm->native_context().object());
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ MacroAssembler::Branch(*done);  // All done, continue.
    __ bind(&next);
  }

  // No pending interrupts. Call into the TieringManager if needed.
  {
    SaveRegisterStateForCall save_register_state(masm,
                                                 node->register_snapshot());
    Register function = scratch0;
    __ Ld_d(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
    __ Push(function);
    // Move into kContextRegister after the load into scratch0, just in case
    // scratch0 happens to be kContextRegister.
    __ Move(kContextRegister, masm->native_context().object());
    // Note: must not cause a lazy deopt!
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
    save_register_state.DefineSafepoint();
  }
  __ MacroAssembler::Branch(*done);
}

void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   Register feedback_cell,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register budget = temps.Acquire();
  __ Ld_w(budget,
          FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ Sub_w(budget, budget, Operand(amount));
  __ St_w(budget,
          FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));

  ZoneLabelRef done(masm);
  // Scratch register 'budget' is released before going into deferred_code.
  Register scratch = budget;
  Label* deferred_code = __ MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Node* node,
         ReduceInterruptBudgetType type, Register scratch) {
        HandleInterruptsAndTiering(masm, done, node, type, scratch);
      },
      done, node, type, scratch);
  __ MacroAssembler::Branch(deferred_code, lt, budget, Operand(zero_reg));

  __ bind(*done);
}

}  // namespace

int ReduceInterruptBudgetForLoop::MaxCallStackArgs() const { return 1; }

void ReduceInterruptBudgetForLoop::SetValueLocationConstraints() {
  UseRegister(FeedbackCellInput());
  set_temporaries_needed(1);
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
  // We cannot use scratch registers, since they're used in LeaveFrame and
  // DropArguments.
  Register actual_params_size = t1;
  Register params_size = t2;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to reuse the
  // incoming argc's register (if it's still valid).
  __ Ld_d(actual_params_size,
          MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ li(params_size, Operand(formal_params_size));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ MacroAssembler::Branch(&corrected_args_count, ge, params_size,
                            Operand(actual_params_size),
                            Label::Distance::kNear);
  __ Move(params_size, actual_params_size);
  __ bind(&corrected_args_count);

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(params_size);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
