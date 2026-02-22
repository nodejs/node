// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/arm64/assembler-arm64-inl.h"
#include "src/codegen/arm64/constants-arm64.h"
#include "src/codegen/arm64/register-arm64.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/maglev/arm64/maglev-assembler-arm64-inl.h"
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
    if (MacroAssemblerBase::IsImmAddSub(*res)) {
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
    unsigned u1, u2, u3;
    if (MacroAssemblerBase::IsImmLogical(*res, 32, &u1, &u2, &u3)) {
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
  Register value = ToRegister(ValueInput()).W();
  Register out = ToRegister(result()).W();

  // Deopt when result would be -0.
  static_assert(Int32NegateWithOverflow::kProperties.can_eager_deopt());
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");
  __ Cbz(value, fail);

  __ Negs(out, value);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32AbsWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register out = ToRegister(result()).W();

  DCHECK(ToRegister(ValueInput()).W().Aliases(out));
  __ AbsWithOverflow(out, out);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32Increment::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Increment::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput()).W();
  Register out = ToRegister(result()).W();
  __ Add(out, value, Immediate(1));
}

void Int32Decrement::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}
void Int32Decrement::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register value = ToRegister(ValueInput()).W();
  Register out = ToRegister(result()).W();
  __ Sub(out, value, Immediate(1));
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, value, Immediate(1));
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
  Register value = ToRegister(ValueInput()).W();
  Register out = ToRegister(result()).W();
  __ Subs(out, value, Immediate(1));
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
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ Move(scratch, char_code);
      __ Strh(scratch.W(),
              FieldMemOperand(result_string,
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
    __ Add(ToRegister(result()), ToRegister(AllocationBlockInput()), offset());
  }
}

void ArgumentsLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void ArgumentsLength::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register argc = ToRegister(result());
  __ Ldr(argc, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Sub(argc, argc, 1);  // Remove receiver.
}

void RestLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void RestLength::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register length = ToRegister(result());
  Label done;
  __ Ldr(length, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Subs(length, length, formal_parameter_count() + 1);
  __ B(kGreaterThanEqual, &done);
  __ Move(length, 0);
  __ Bind(&done);
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
  __ CompareAndBranch(input_reg.X(),
                      Immediate(std::numeric_limits<int32_t>::max()), gt,
                      __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32));
  __ CompareAndBranch(input_reg.X(),
                      Immediate(std::numeric_limits<int32_t>::min()), lt,
                      __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32));
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
  if (value().is_nan()) {
    __ JumpIfNotNan(target, fail);
  } else if (value().get_scalar() == 0) {  // If value is +0.0 or -0.0.
    Register scratch = temps.AcquireScratch();
    __ Fcmp(target, value().get_scalar());
    __ JumpIf(kNotEqual, fail);
    __ Fmov(scratch, target);
    if (value().get_bits() == 0) {
      __ Tbnz(scratch, 63, fail);
    } else {
      __ Tbz(scratch, 63, fail);
    }
  } else {
    DoubleRegister double_scratch = temps.AcquireScratchDouble();
    __ Move(double_scratch, value());
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
  Register left = ToRegister(LeftInput()).W();
  Register out = ToRegister(result()).W();
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ Add(out, left, *right_const);
  } else {
    Register right = ToRegister(RightInput()).W();
    __ Add(out, left, right);
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
  Register left = ToRegister(LeftInput()).W();
  Register out = ToRegister(result()).W();
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ Sub(out, left, *right_const);
  } else {
    Register right = ToRegister(RightInput()).W();
    __ Sub(out, left, right);
  }
}

void Int32Multiply::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32Multiply::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register left = ToRegister(LeftInput()).W();
  Register right = ToRegister(RightInput()).W();
  Register out = ToRegister(result()).X();

  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ Smull(out, left, right);

  // Making sure that the 32-bit output is zero-extended.
  __ Mov(out.W(), out.W());
}

void Int32MultiplyOverflownBits::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}

void Int32MultiplyOverflownBits::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register left = ToRegister(LeftInput()).W();
  Register right = ToRegister(RightInput()).W();
  Register out = ToRegister(result()).X();

  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ Smull(out, left, right);

  // Note: this has to be a Lsr rather than a Asr to ensure that the 32-bit
  // output is zero-extended.
  __ Lsr(out, out, 32);
}

void Int32Divide::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32Divide::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register left = ToRegister(LeftInput()).W();
  Register right = ToRegister(RightInput()).W();
  Register out = ToRegister(result()).W();

  // TODO(leszeks): peephole optimise division by a constant.

  // On ARM, integer division does not trap on overflow or division by zero.
  // - Division by zero returns 0.
  // - Signed overflow (INT_MIN / -1) returns INT_MIN.
  __ Sdiv(out, left, right);
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
  Register left = ToRegister(LeftInput()).W();
  Register out = ToRegister(result()).W();
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ Adds(out, left, *right_const);
  } else {
    Register right = ToRegister(RightInput()).W();
    __ Adds(out, left, right);
  }
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
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
  Register left = ToRegister(LeftInput()).W();
  Register out = ToRegister(result()).W();
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ Subs(out, left, *right_const);
  } else {
    Register right = ToRegister(RightInput()).W();
    __ Subs(out, left, right);
  }
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
  Register left = ToRegister(LeftInput()).W();
  Register right = ToRegister(RightInput()).W();
  Register out = ToRegister(result()).W();

  // TODO(leszeks): peephole optimise multiplication by a constant.

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out.X();
  if (out_alias_input) {
    res = temps.AcquireScratch();
  }

  __ Smull(res, left, right);

  // if res != (res[0:31] sign extended to 64 bits), then the multiplication
  // result is too large for 32 bits.
  __ Cmp(res, Operand(res.W(), SXTW));
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ CompareAndBranch(res, Immediate(0), ne, &end);
  {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    Register temp = temps.AcquireScratch().W();
    __ Orr(temp, left, right);
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ RecordComment("-- Jump to eager deopt if the result is negative zero");
    __ Tbnz(temp, temp.SizeInBits() - 1,
            __ GetDeoptLabel(this, DeoptimizeReason::kOverflow));
  }
  __ Bind(&end);

  // Making sure that the 32-bit output is zero-extended (and moving it to the
  // right register if {out_alias_input} is true).
  __ Mov(out, res.W());
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsRegister(this);
}
void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(LeftInput()).W();
  Register right = ToRegister(RightInput()).W();
  Register out = ToRegister(result()).W();

  // TODO(leszeks): peephole optimise division by a constant.

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ Cmp(right, Immediate(0));
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
        __ CompareAndBranch(left, Immediate(0), eq, deopt);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ Cmp(left, Immediate(kMinInt));
        __ JumpIf(ne, *done);
        __ Cmp(right, Immediate(-1));
        __ JumpIf(ne, *done);
        __ JumpToDeopt(deopt);
      },
      done, left, right, this);
  __ Bind(*done);

  // Perform the actual integer division.
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.AcquireScratch().W();
  }
  __ Sdiv(res, left, right);

  // Check that the remainder is zero.
  Register temp = temps.AcquireScratch().W();
  __ Msub(temp, res, right, left);
  __ CompareAndBranch(temp, Immediate(0), ne,
                      __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32));

  __ Mov(out, res);
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

  Register lhs = ToRegister(LeftInput()).W();
  Register rhs = ToRegister(RightInput()).W();
  Register out = ToRegister(result()).W();

  static constexpr DeoptimizeReason deopt_reason =
      DeoptimizeReason::kDivisionByZero;

  if (lhs == rhs) {
    // For the modulus algorithm described above, lhs and rhs must not alias
    // each other.
    __ Tst(lhs, lhs);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(mi, deopt_reason, this);
    __ Move(ToRegister(result()), 0);
    return;
  }

  DCHECK(!AreAliased(lhs, rhs));

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);
  __ Cmp(rhs, Immediate(0));
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ Negs(rhs, rhs);
        __ B(*rhs_checked, ne);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      rhs_checked, rhs, this);
  __ Bind(*rhs_checked);

  __ Cmp(lhs, Immediate(0));
  __ JumpToDeferredIf(
      lt,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Register out, Int32ModulusWithOverflow* node) {
        MaglevAssembler::TemporaryRegisterScope temps(masm);
        Register res = temps.AcquireScratch().W();
        __ Neg(lhs, lhs);
        __ Udiv(res, lhs, rhs);
        __ Msub(out, res, rhs, lhs);
        __ Negs(out, out);
        __ B(*done, ne);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
        // only allows one deopt reason per IR.
        __ EmitEagerDeopt(node, deopt_reason);
      },
      done, lhs, rhs, out, this);

  Label rhs_not_power_of_2;
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register mask = temps.AcquireScratch().W();
  __ Add(mask, rhs, Immediate(-1));
  __ Tst(mask, rhs);
  __ JumpIf(ne, &rhs_not_power_of_2);

  // {rhs} is power of 2.
  __ And(out, mask, lhs);
  __ Jump(*done);

  __ Bind(&rhs_not_power_of_2);

  // We store the result of the Udiv in a temporary register in case {out} is
  // the same as {lhs} or {rhs}: we'll still need those 2 registers intact to
  // get the remainder.
  Register res = mask;
  __ Udiv(res, lhs, rhs);
  __ Msub(out, res, rhs, lhs);

  __ Bind(*done);
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
    Register left = ToRegister(LeftInput()).W();                       \
    Register out = ToRegister(result()).W();                           \
    if (!RightInput().operand().IsRegister()) {                        \
      auto right_const = TryGetInt32ConstantInput(kRightIndex);        \
      DCHECK(right_const);                                             \
      __ opcode(out, left, *right_const);                              \
    } else {                                                           \
      Register right = ToRegister(RightInput()).W();                   \
      __ opcode(out, left, right);                                     \
    }                                                                  \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_)
DEF_BITWISE_BINOP(Int32BitwiseOr, orr)
DEF_BITWISE_BINOP(Int32BitwiseXor, eor)
#undef DEF_BITWISE_BINOP

#define DEF_SHIFT_BINOP(Instruction, opcode)                        \
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
    Register out = ToRegister(result()).W();                        \
    Register left = ToRegister(LeftInput()).W();                    \
    if (auto right_const = TryGetInt32ConstantInput(kRightIndex)) { \
      int right = *right_const & 31;                                \
      if (right == 0) {                                             \
        __ Move(out, left);                                         \
      } else {                                                      \
        __ opcode(out, left, right);                                \
      }                                                             \
    } else {                                                        \
      Register right = ToRegister(RightInput()).W();                \
      __ opcode##v(out, left, right);                               \
    }                                                               \
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
  Register value = ToRegister(ValueInput()).W();
  Register out = ToRegister(result()).W();
  __ Mvn(out, value);
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
  __ Fadd(out, left, right);
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
  __ Fsub(out, left, right);
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
  __ Fmul(out, left, right);
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
  __ Fdiv(out, left, right);
}

int Float64Modulus::MaxCallStackArgs() const { return 0; }
void Float64Modulus::SetValueLocationConstraints() {
  UseFixed(LeftInput(), v0);
  UseFixed(RightInput(), v1);
  DefineSameAsFirst(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
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
  __ Fneg(out, value);
}

void Float64Abs::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fabs(out, in);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());
  if (kind_ == Kind::kNearest) {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireScratchDouble();
    DoubleRegister half_one = temps.AcquireScratchDouble();
    __ Move(temp, in);
    // Frintn rounds to even on tie, while JS expects it to round towards
    // +Infinity. Fix the difference by checking if we rounded down by exactly
    // 0.5, and if so, round to the other side.
    __ Frintn(out, in);
    __ Fsub(temp, temp, out);
    __ Move(half_one, 0.5);
    __ Fcmp(temp, half_one);
    Label done;
    __ JumpIf(ne, &done, Label::kNear);
    // Fix wrong tie-to-even by adding 0.5 twice.
    __ Fadd(out, out, half_one);
    __ Fadd(out, out, half_one);
    __ bind(&done);
  } else if (kind_ == Kind::kCeil) {
    __ Frintp(out, in);
  } else if (kind_ == Kind::kFloor) {
    __ Frintm(out, in);
  }
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(LeftInput(), v0);
  UseFixed(RightInput(), v1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 2);
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

  __ fmin(out, left, right);
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

  __ fmax(out, left, right);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(ValueInput(), v0);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ieee_function_ref(), 1);
}

int Float64Ieee754Binary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Binary::SetValueLocationConstraints() {
  UseFixed(LeftInput(), v0);
  UseFixed(RightInput(), v1);
  DefineSameAsFirst(this);
}
void Float64Ieee754Binary::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ieee_function_ref(), 2);
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
    __ Lsr(result_register, result_register, shift_size);
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
    __ Subs(limit, byte_length, Immediate(element_size - 1));
    __ EmitEagerDeoptIf(mi, DeoptimizeReason::kOutOfBounds, this);
  }
  __ Cmp(index, limit);
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
  __ CanonicalizeNaN(ToDoubleRegister(result()),
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
  __ JumpIfNotHoleNan(value, scratch.W(), &done);
  __ Move(value, UndefinedNan());
  __ Bind(&done);
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
      __ Cmp(sp, stack_limit);
      __ B(&next, hi);
    }

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ Ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ Push(function);
      // Move into kContextRegister after the load into scratch0, just in case
      // scratch0 happens to be kContextRegister.
      __ Move(kContextRegister, masm->native_context().object());
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ B(*done);  // All done, continue.
    __ Bind(&next);
  }

  // No pending interrupts. Call into the TieringManager if needed.
  {
    SaveRegisterStateForCall save_register_state(masm,
                                                 node->register_snapshot());
    Register function = scratch0;
    __ Ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
    __ Push(function);
    // Move into kContextRegister after the load into scratch0, just in case
    // scratch0 happens to be kContextRegister.
    __ Move(kContextRegister, masm->native_context().object());
    // Note: must not cause a lazy deopt!
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
    save_register_state.DefineSafepoint();
  }
  __ B(*done);
}

void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   Register feedback_cell,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register budget = scratch.W();
  __ Ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ Subs(budget, budget, Immediate(amount));
  __ Str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(lt, HandleInterruptsAndTiering, done, node, type,
                      scratch);
  __ Bind(*done);
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
  Register actual_params_size = x9;
  Register params_size = x10;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to reuse the
  // incoming argc's register (if it's still valid).
  __ Ldr(actual_params_size,
         MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Mov(params_size, Immediate(formal_params_size));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ CompareAndBranch(params_size, actual_params_size, ge,
                      &corrected_args_count);
  __ Mov(params_size, actual_params_size);
  __ Bind(&corrected_args_count);

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(params_size);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
