// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/arm/assembler-arm.h"
#include "src/codegen/arm/register-arm.h"
#include "src/maglev/arm/maglev-assembler-arm-inl.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"

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
  __ cmp(value, Operand(0));
  __ EmitEagerDeoptIf(eq, DeoptimizeReason::kOverflow, this);

  __ rsb(out, value, Operand(0), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32AbsWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register out = ToRegister(result());
  Label done;
  __ cmp(out, Operand(0));
  __ JumpIf(ge, &done);
  __ rsb(out, out, Operand(0), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
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
  __ add(out, value, Operand(1));
}

void Int32Decrement::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Decrement::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ sub(out, value, Operand(1));
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ add(out, value, Operand(1), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ sub(out, value, Operand(1), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
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
      DCHECK_NE(scratch, result_string);
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ Move(scratch, char_code);
      __ strh(scratch, FieldMemOperand(result_string,
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
    __ add(ToRegister(result()), ToRegister(AllocationBlockInput()),
           Operand(offset()));
  }
}

void ArgumentsLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void ArgumentsLength::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register argc = ToRegister(result());
  __ ldr(argc, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ sub(argc, argc, Operand(1));  // Remove receiver.
}

void RestLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void RestLength::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register length = ToRegister(result());
  Label done;
  __ ldr(length, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ sub(length, length, Operand(formal_parameter_count() + 1), SetCC);
  __ b(kGreaterThanEqual, &done);
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
  // On 32-bit platforms, IntPtr is the same as Int32.
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
      __ VmovHigh(scratch, target);
      __ cmp(scratch, Operand(0));
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
  __ add(out, left, right, LeaveCC);
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
  __ sub(out, left, right, LeaveCC);
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
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();

  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ smull(out, scratch, left, right);
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
  __ smull(scratch, out, left, right);
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

  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm, SUDIV);
    // On ARM, integer division does not trap on overflow or division by zero.
    // - Division by zero returns 0.
    // - Signed overflow (INT_MIN / -1) returns INT_MIN.
    __ sdiv(out, left, right);
  } else {
    ZoneLabelRef do_division(masm), done(masm);
    __ cmp(right, Operand(0));
    __ JumpToDeferredIf(
        le,
        [](MaglevAssembler* masm, ZoneLabelRef do_division, ZoneLabelRef done,
           Register left, Register right, Register out) {
          Label right_is_neg;
          // Truncated value of anything divided by 0 is 0.
          __ b(ne, &right_is_neg);
          __ Move(out, 0);
          __ b(*done);

          // Return -left if right = -1.
          // This avoids a hardware exception if left = INT32_MIN.
          // Int32Divide returns a truncated value and according to
          // ecma262#sec-toint32, the truncated value of INT32_MIN
          // is INT32_MIN.
          __ bind(&right_is_neg);
          __ cmp(right, Operand(-1));
          __ b(ne, *do_division);
          __ rsb(out, left, Operand(0));
          __ b(*done);
        },
        do_division, done, left, right, out);

    UseScratchRegisterScope temps(masm);
    LowDwVfpRegister double_right = temps.AcquireLowD();
    SwVfpRegister tmp = double_right.low();
    DwVfpRegister double_left = temps.AcquireD();
    DwVfpRegister double_res = double_left;
    __ bind(*do_division);
    __ vmov(tmp, left);
    __ vcvt_f64_s32(double_left, tmp);
    __ vmov(tmp, right);
    __ vcvt_f64_s32(double_right, tmp);
    __ vdiv(double_res, double_left, double_right);
    __ vcvt_s32_f64(tmp, double_res);
    __ vmov(out, tmp);

    __ bind(*done);
  }
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
  __ add(out, left, right, SetCC);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
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
  __ sub(out, left, right, SetCC);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  Register out = ToRegister(result());

  // TODO(leszeks): peephole optimise multiplication by a constant.

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res_low = out;
  if (out_alias_input) {
    res_low = temps.AcquireScratch();
  }
  Register res_high = temps.AcquireScratch();
  __ smull(res_low, res_high, left, right);

  // ARM doesn't set the overflow flag for multiplication, so we need to
  // test on kNotEqual.
  __ cmp(res_high, Operand(res_low, ASR, 31));
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ tst(res_low, res_low);
  __ b(ne, &end);
  Register temp = res_high;
  __ orr(temp, left, right, SetCC);
  // If one of them is negative, we must have a -0 result, which is non-int32,
  // so deopt.
  __ EmitEagerDeoptIf(mi, DeoptimizeReason::kOverflow, this);

  __ bind(&end);
  if (out_alias_input) {
    __ Move(out, res_low);
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

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ cmp(right, Operand(0));
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
        __ tst(left, left);
        __ JumpIf(eq, deopt);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ cmp(left, Operand(kMinInt));
        __ JumpIf(ne, *done);
        __ cmp(right, Operand(-1));
        __ JumpIf(ne, *done);
        __ JumpToDeopt(deopt);
      },
      done, left, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.AcquireScratch();
  }
  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm, SUDIV);
    __ sdiv(res, left, right);
  } else {
    UseScratchRegisterScope temps(masm);
    LowDwVfpRegister double_right = temps.AcquireLowD();
    SwVfpRegister tmp = double_right.low();
    DwVfpRegister double_left = temps.AcquireD();
    DwVfpRegister double_res = double_left;
    __ vmov(tmp, left);
    __ vcvt_f64_s32(double_left, tmp);
    __ vmov(tmp, right);
    __ vcvt_f64_s32(double_right, tmp);
    __ vdiv(double_res, double_left, double_right);
    __ vcvt_s32_f64(tmp, double_res);
    __ vmov(res, tmp);
  }

  // Check that the remainder is zero.
  Register temp = temps.AcquireScratch();
  __ mul(temp, res, right);
  __ cmp(temp, left);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, this);

  __ Move(out, res);
}

namespace {
void Uint32Mod(MaglevAssembler* masm, Register out, Register left,
               Register right) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register res = temps.AcquireScratch();
  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm, SUDIV);
    __ udiv(res, left, right);
  } else {
    UseScratchRegisterScope temps(masm);
    LowDwVfpRegister double_right = temps.AcquireLowD();
    SwVfpRegister tmp = double_right.low();
    DwVfpRegister double_left = temps.AcquireD();
    DwVfpRegister double_res = double_left;
    __ vmov(tmp, left);
    __ vcvt_f64_s32(double_left, tmp);
    __ vmov(tmp, right);
    __ vcvt_f64_s32(double_right, tmp);
    __ vdiv(double_res, double_left, double_right);
    __ vcvt_s32_f64(tmp, double_res);
    __ vmov(res, tmp);
  }
  if (CpuFeatures::IsSupported(ARMv7)) {
    __ mls(out, res, right, left);
  } else {
    __ mul(res, res, right);
    __ sub(out, left, res);
  }
}
}  // namespace

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
    __ tst(lhs, lhs);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(mi, deopt_reason, this);
    __ Move(ToRegister(result()), 0);
    return;
  }

  DCHECK_NE(lhs, rhs);

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);
  __ cmp(rhs, Operand(0));
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ rsb(rhs, rhs, Operand(0), SetCC);
        __ b(ne, *rhs_checked);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      rhs_checked, rhs, this);
  __ bind(*rhs_checked);

  __ cmp(lhs, Operand(0));
  __ JumpToDeferredIf(
      lt,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Register out, Int32ModulusWithOverflow* node) {
        __ rsb(lhs, lhs, Operand(0));
        Uint32Mod(masm, out, lhs, rhs);
        __ rsb(out, out, Operand(0), SetCC);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
        // only allows one deopt reason per IR.
        __ b(ne, *done);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      done, lhs, rhs, out, this);

  Label rhs_not_power_of_2;
  {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    Register mask = temps.AcquireScratch();
    __ add(mask, rhs, Operand(-1));
    __ tst(mask, rhs);
    __ JumpIf(ne, &rhs_not_power_of_2);

    // {rhs} is power of 2.
    __ and_(out, mask, lhs);
    __ Jump(*done);
    // {mask} can be reused from now on.
  }

  __ bind(&rhs_not_power_of_2);
  Uint32Mod(masm, out, lhs, rhs);
  __ bind(*done);
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
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_)
DEF_BITWISE_BINOP(Int32BitwiseOr, orr)
DEF_BITWISE_BINOP(Int32BitwiseXor, eor)
#undef DEF_BITWISE_BINOP

#define DEF_SHIFT_BINOP(Instruction, opcode)                                   \
  void Instruction::SetValueLocationConstraints() {                            \
    UseRegister(LeftInput());                                                  \
    if (RightInput().node()->Is<Int32Constant>()) {                            \
      UseAny(RightInput());                                                    \
    } else {                                                                   \
      UseRegister(RightInput());                                               \
    }                                                                          \
    DefineAsRegister(this);                                                    \
  }                                                                            \
  void Instruction::GenerateCode(MaglevAssembler* masm,                        \
                                 const ProcessingState& state) {               \
    Register left = ToRegister(LeftInput());                                   \
    Register out = ToRegister(result());                                       \
    if (Int32Constant* constant =                                              \
            RightInput().node()->TryCast<Int32Constant>()) {                   \
      uint32_t shift = constant->value() & 31;                                 \
      if (shift == 0) {                                                        \
        /* TODO(victorgomes): Arm will do a shift of 32 if right == 0. Ideally \
         * we should not even emit the shift in the first place. We do a move  \
         * here for the moment. */                                             \
        __ Move(out, left);                                                    \
      } else {                                                                 \
        __ opcode(out, left, Operand(shift));                                  \
      }                                                                        \
    } else {                                                                   \
      MaglevAssembler::TemporaryRegisterScope temps(masm);                     \
      Register scratch = temps.AcquireScratch();                               \
      Register right = ToRegister(RightInput());                               \
      __ and_(scratch, right, Operand(31));                                    \
      __ opcode(out, left, Operand(scratch));                                  \
    }                                                                          \
  }
DEF_SHIFT_BINOP(Int32ShiftLeft, lsl)
DEF_SHIFT_BINOP(Int32ShiftRight, asr)
DEF_SHIFT_BINOP(Int32ShiftRightLogical, lsr)
#undef DEF_SHIFT_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  Register out = ToRegister(result());
  __ mvn(out, Operand(value));
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
  __ vadd(out, left, right);
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
  __ vsub(out, left, right);
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
  __ vmul(out, left, right);
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
  __ vdiv(out, left, right);
}

int Float64Modulus::MaxCallStackArgs() const { return 0; }
void Float64Modulus::SetValueLocationConstraints() {
  UseFixed(LeftInput(), d0);
  UseFixed(RightInput(), d1);
  DefineAsRegister(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 2);
  __ MovToFloatParameters(ToDoubleRegister(LeftInput()),
                          ToDoubleRegister(RightInput()));
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
  // Move the result in the double result register.
  __ MovFromFloatResult(ToDoubleRegister(result()));
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ vneg(out, value);
}

void Float64Abs::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ vabs(out, in);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  CpuFeatureScope scope(masm, ARMv8);
  if (kind_ == Kind::kNearest) {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireDouble();
    __ Move(temp, in);
    // vrintn rounds to even on tie, while JS expects it to round towards
    // +Infinity. Fix the difference by checking if we rounded down by exactly
    // 0.5, and if so, round to the other side.
    __ vrintn(out, in);
    __ vsub(temp, temp, out);
    DoubleRegister half_one = temps.AcquireScratchDouble();
    __ Move(half_one, 0.5);
    __ VFPCompareAndSetFlags(temp, half_one);
    Label done;
    __ JumpIf(ne, &done, Label::kNear);
    // Fix wrong tie-to-even by adding 0.5 twice.
    __ vadd(out, out, half_one);
    __ vadd(out, out, half_one);
    __ bind(&done);
  } else if (kind_ == Kind::kCeil) {
    __ vrintp(out, in);
  } else if (kind_ == Kind::kFloor) {
    __ vrintm(out, in);
  }
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(LeftInput(), d0);
  UseFixed(RightInput(), d1);
  DefineAsRegister(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 2);
  __ MovToFloatParameters(left, right);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 0, 2);
  __ MovFromFloatResult(out);
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

  Label has_nan, done;
  __ FloatMin(out, left, right, &has_nan);
  __ Jump(&done);

  __ bind(&has_nan);
  __ FloatMinOutOfLine(out, left, right);
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

  Label has_nan, done;
  __ FloatMax(out, left, right, &has_nan);
  __ Jump(&done);

  __ bind(&has_nan);
  __ FloatMaxOutOfLine(out, left, right);
  __ bind(&done);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(ValueInput(), d0);
  DefineAsRegister(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 1);
  __ MovToFloatParameter(value);
  __ CallCFunction(ieee_function_ref(), 0, 1);
  __ MovFromFloatResult(out);
}

int Float64Ieee754Binary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Binary::SetValueLocationConstraints() {
  UseFixed(LeftInput(), d0);
  UseFixed(RightInput(), d1);
  DefineAsRegister(this);
}
void Float64Ieee754Binary::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  DoubleRegister lhs = ToDoubleRegister(LeftInput());
  DoubleRegister rhs = ToDoubleRegister(RightInput());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 2);
  __ MovToFloatParameters(lhs, rhs);
  __ CallCFunction(ieee_function_ref(), 0, 2);
  __ MovFromFloatResult(out);
}

void Float64Sqrt::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Float64Sqrt::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ vsqrt(out, value);
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
    __ lsr(result_register, result_register, Operand(shift_size));
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
    __ sub(limit, byte_length, Operand(element_size - 1), SetCC);
    __ EmitEagerDeoptIf(mi, DeoptimizeReason::kOutOfBounds, this);
  }
  __ cmp(index, limit);
  __ EmitEagerDeoptIf(hs, DeoptimizeReason::kOutOfBounds, this);
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
  __ VFPCanonicalizeNaN(ToDoubleRegister(result()),
                        ToDoubleRegister(ValueInput()));
}

void HoleyFloat64ToSilencedFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void HoleyFloat64ToSilencedFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  // The hole value is a signalling NaN, so just silence it to get the
  // float64 value.
  __ VFPCanonicalizeNaN(ToDoubleRegister(this->result()),
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
  __ VFPCanonicalizeNaN(ToDoubleRegister(this->result()),
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
      __ cmp(sp, stack_limit);
      __ b(hi, &next);
    }

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
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
    __ ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
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
  Register budget = temps.Acquire();
  __ ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ sub(budget, budget, Operand(amount), SetCC);
  __ str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(lt, HandleInterruptsAndTiering, done, node, type, budget);
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
  Register actual_params_size = r4;
  Register params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to reuse the
  // incoming argc's register (if it's still valid).
  __ ldr(actual_params_size,
         MemOperand(fp, StandardFrameConstants::kArgCOffset));

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ Move(params_size, formal_params_size);
  __ cmp(params_size, actual_params_size);
  __ b(kGreaterThanEqual, &corrected_args_count);
  __ Move(params_size, actual_params_size);
  __ bind(&corrected_args_count);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(params_size);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
