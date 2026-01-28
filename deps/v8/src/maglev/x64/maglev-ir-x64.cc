// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/x64/assembler-x64-inl.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/common/globals.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

// ---
// Nodes
// ---

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
    __ leaq(ToRegister(result()),
            Operand(ToRegister(AllocationBlockInput()), offset()));
  }
}

void ArgumentsLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void ArgumentsLength::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  __ movq(ToRegister(result()),
          Operand(rbp, StandardFrameConstants::kArgCOffset));
  __ decl(ToRegister(result()));  // Remove receiver.
}

void RestLength::SetValueLocationConstraints() { DefineAsRegister(this); }

void RestLength::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  Register length = ToRegister(result());
  Label done;
  __ movq(length, Operand(rbp, StandardFrameConstants::kArgCOffset));
  __ subl(length, Immediate(formal_parameter_count() + 1));
  __ j(greater_equal, &done, Label::Distance::kNear);
  __ Move(length, 0);
  __ bind(&done);
  __ UncheckedSmiTagInt32(length);
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
    __ AssertNotSmi(object);
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ LoadBoundedSizeFromObject(result_register, object,
                               JSTypedArray::kRawByteLengthOffset);
  int shift_size = ElementsKindToShiftSize(elements_kind_);
  if (shift_size > 0) {
    // TODO(leszeks): Merge this shift with the one in LoadBoundedSize.
    DCHECK(shift_size == 1 || shift_size == 2 || shift_size == 3);
    __ shrq(result_register, Immediate(shift_size));
  }
}

void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(IndexInput());
  int element_size = compiler::ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    UseAndClobberRegister(ByteLengthInput());
  } else {
    UseRegister(ByteLengthInput());
  }
}

void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register index = ToRegister(IndexInput());
  Register byte_length = ToRegister(ByteLengthInput());

  int element_size = compiler::ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    __ subq(byte_length, Immediate(element_size - 1));
    __ EmitEagerDeoptIf(negative, DeoptimizeReason::kOutOfBounds, this);
  }
  __ cmpl(index, byte_length);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

int CheckedObjectToIndex::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(1);
}

void CheckedIntPtrToInt32::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void CheckedIntPtrToInt32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(ValueInput());

  // Copy input(32 bit) to scratch. Is input equal(64 bit) to scratch?
  __ movl(kScratchRegister, input_reg);
  __ cmpq(kScratchRegister, input_reg);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);
}

void CheckFloat64SameValue::SetValueLocationConstraints() {
  UseRegister(ValueInput());
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
      __ movq(scratch, target);
      __ testq(scratch, scratch);
      __ JumpIf(value().get_bits() == 0 ? kNotEqual : kEqual, fail);
    }
  }
}

int BuiltinStringFromCharCode::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void BuiltinStringFromCharCode::SetValueLocationConstraints() {
  if (CharCodeInput().node()->Is<Int32Constant>()) {
    UseAny(CharCodeInput());
  } else {
    UseAndClobberRegister(CharCodeInput());
    set_temporaries_needed(1);
  }
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result_string = ToRegister(result());
  if (Int32Constant* constant =
          CharCodeInput().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value() & 0xFFFF;
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ movw(
          FieldOperand(result_string, OFFSET_OF_DATA_START(SeqTwoByteString)),
          Immediate(char_code));
    }
  } else {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    Register char_code = ToRegister(CharCodeInput());
    __ StringFromCharCode(register_snapshot(), nullptr, result_string,
                          char_code, scratch,
                          MaglevAssembler::CharCodeMaskMode::kMustApplyMask);
  }
}

void Int32Add::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineSameAsFirst(this);
}

void Int32Add::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ addl(left, Immediate(*right_const));
  } else {
    Register right = ToRegister(RightInput());
    __ addl(left, right);
  }
}

void Int32Subtract::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineSameAsFirst(this);
}

void Int32Subtract::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ subl(left, Immediate(*right_const));
  } else {
    Register right = ToRegister(RightInput());
    __ subl(left, right);
  }
}

void Int32Multiply::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
    DefineAsRegister(this);
  } else {
    UseRegister(RightInput());
    DefineSameAsFirst(this);
  }
}

void Int32Multiply::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register result = ToRegister(this->result());
  if (auto right_const = TryGetInt32ConstantInput(kRightIndex)) {
    __ imull(result, left, Immediate(*right_const));
  } else {
    Register right = ToRegister(RightInput());
    DCHECK_EQ(result, left);
    __ imull(left, right);
  }
}

void Int32MultiplyOverflownBits::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  // TODO(victorgomes): Ideally we would like to have UseFixedAndClobber(rax),
  // but we don't support that yet.
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineAsFixed(this, rdx);  // imull returns high bits in rdx.
  RequireSpecificTemporary(rax);
}

void Int32MultiplyOverflownBits::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  DCHECK_EQ(ToRegister(result()), rdx);
  if (auto right_const = TryGetInt32ConstantInput(kRightIndex)) {
    __ movl(rax, Immediate(*right_const));
  } else {
    __ movl(rax, ToRegister(RightInput()));
  }
  __ imull(left);
}

void Int32Divide::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsFixed(this, rax);
  // rax,rdx are clobbered by idiv.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32Divide::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  ZoneLabelRef do_division(masm), done(masm);

  // TODO(leszeks): peephole optimise division by a constant.

  __ movl(rax, left);
  __ testl(right, right);
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef do_division, ZoneLabelRef done,
         Register right) {
        Label right_is_neg;
        // Truncated value of anything divided by 0 is 0.
        __ j(not_equal, &right_is_neg);
        __ xorl(rax, rax);
        __ jmp(*done);

        // Return -left if right = -1.
        // This avoids a hardware exception if left = INT32_MIN.
        // Int32Divide returns a truncated value and according to
        // ecma262#sec-toint32, the truncated value of INT32_MIN
        // is INT32_MIN.
        __ bind(&right_is_neg);
        __ cmpl(right, Immediate(-1));
        __ j(not_equal, *do_division);
        __ negl(rax);
        __ jmp(*done);
      },
      do_division, done, right);

  __ bind(*do_division);
  __ cdq();  // Sign extend eax into edx.
  __ idivl(right);

  __ bind(*done);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineSameAsFirst(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ addl(left, Immediate(*right_const));
  } else {
    Register right = ToRegister(RightInput());
    __ addl(left, right);
  }
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineSameAsFirst(this);
}

void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ subl(left, Immediate(*right_const));
  } else {
    Register right = ToRegister(RightInput());
    __ subl(left, right);
  }
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
}

void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result = ToRegister(this->result());
  Register right = ToRegister(RightInput());
  DCHECK_EQ(result, ToRegister(LeftInput()));

  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register saved_left = temps.Acquire();
  __ movl(saved_left, result);
  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ imull(result, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{saved_left, result} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ cmpl(result, Immediate(0));
  __ j(not_zero, &end);
  {
    __ orl(saved_left, right);
    __ cmpl(saved_left, Immediate(0));
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ EmitEagerDeoptIf(less, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseAndClobberRegister(RightInput());
  DefineAsFixed(this, rdx);
  // rax,rdx are clobbered by div.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // If AreAliased(lhs, rhs):
  //   deopt if lhs < 0  // Minus zero.
  //   0
  //
  // Otherwise, use the same algorithm as in EffectControlLinearizer:
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

  static constexpr DeoptimizeReason deopt_reason =
      DeoptimizeReason::kDivisionByZero;

  if (lhs == rhs) {
    // For the modulus algorithm described above, lhs and rhs must not alias
    // each other.
    __ testl(lhs, lhs);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(negative, deopt_reason, this);
    __ Move(ToRegister(result()), 0);
    return;
  }

  DCHECK(!AreAliased(lhs, rhs, rax, rdx));

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);

  __ cmpl(rhs, Immediate(0));
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ negl(rhs);
        __ j(not_zero, *rhs_checked);
        __ EmitEagerDeopt(node, deopt_reason);
      },
      rhs_checked, rhs, this);
  __ bind(*rhs_checked);

  __ cmpl(lhs, Immediate(0));
  __ JumpToDeferredIf(
      less,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Int32ModulusWithOverflow* node) {
        // `divl(divisor)` divides rdx:rax by the divisor and stores the
        // quotient in rax, the remainder in rdx.
        __ movl(rax, lhs);
        __ negl(rax);
        __ xorl(rdx, rdx);
        __ divl(rhs);
        __ negl(rdx);
        __ j(not_zero, *done);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
        // allows one deopt reason per IR.
        __ EmitEagerDeopt(node, deopt_reason);
      },
      done, lhs, rhs, this);

  Label rhs_not_power_of_2;
  Register mask = rax;
  __ leal(mask, Operand(rhs, -1));
  __ testl(rhs, mask);
  __ j(not_zero, &rhs_not_power_of_2, Label::kNear);

  // {rhs} is power of 2.
  __ andl(mask, lhs);
  __ movl(ToRegister(result()), mask);
  __ jmp(*done, Label::kNear);

  __ bind(&rhs_not_power_of_2);
  // `divl(divisor)` divides rdx:rax by the divisor and stores the
  // quotient in rax, the remainder in rdx.
  __ movl(rax, lhs);
  __ xorl(rdx, rdx);
  __ divl(rhs);
  // Result is implicitly written to rdx.
  DCHECK_EQ(ToRegister(result()), rdx);

  __ bind(*done);
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineAsFixed(this, rax);
  // rax,rdx are clobbered by idiv.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  Register right = ToRegister(RightInput());
  __ movl(rax, left);

  // TODO(leszeks): peephole optimise division by a constant.

  // Sign extend eax into edx.
  __ cdq();

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ cmpl(right, Immediate(0));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register right,
         Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        // TODO(leszeks): Using kNotInt32 here, but kDivisionByZero would be
        // better. Right now all eager deopts in a node have to be the same --
        // we should allow a node to emit multiple eager deopts with different
        // reasons.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is zero, as that would produce minus zero. Left is in
        // rax already.
        __ cmpl(rax, Immediate(0));
        // TODO(leszeks): Better DeoptimizeReason = kMinusZero.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ cmpl(rax, Immediate(kMinInt));
        __ j(not_equal, *done);
        __ cmpl(right, Immediate(-1));
        __ j(not_equal, *done);
        // TODO(leszeks): Better DeoptimizeReason = kOverflow, but
        // eager_deopt_info is already configured as kNotInt32.
        __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
      },
      done, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  __ idivl(right);

  // Check that the remainder is zero.
  __ cmpl(rdx, Immediate(0));
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{rax, rdx} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);
  DCHECK_EQ(ToRegister(result()), rax);
}

#define DEF_BITWISE_BINOP(Instruction, opcode)                   \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(LeftInput());                                    \
    if (TryGetInt32ConstantInput(kRightIndex)) {                 \
      UseAny(RightInput());                                      \
    } else {                                                     \
      UseRegister(RightInput());                                 \
    }                                                            \
    DefineSameAsFirst(this);                                     \
  }                                                              \
                                                                 \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(LeftInput());                     \
    if (!RightInput().operand().IsRegister()) {                  \
      auto right_const = TryGetInt32ConstantInput(kRightIndex);  \
      DCHECK(right_const);                                       \
      __ opcode(left, Immediate(*right_const));                  \
    } else {                                                     \
      Register right = ToRegister(RightInput());                 \
      __ opcode(left, right);                                    \
    }                                                            \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, andl)
DEF_BITWISE_BINOP(Int32BitwiseOr, orl)
DEF_BITWISE_BINOP(Int32BitwiseXor, xorl)
#undef DEF_BITWISE_BINOP

#define DEF_SHIFT_BINOP(Instruction, opcode)                        \
  void Instruction::SetValueLocationConstraints() {                 \
    UseRegister(LeftInput());                                       \
    if (TryGetInt32ConstantInput(kRightIndex)) {                    \
      UseAny(RightInput());                                         \
    } else {                                                        \
      UseFixed(RightInput(), rcx);                                  \
    }                                                               \
    DefineSameAsFirst(this);                                        \
  }                                                                 \
                                                                    \
  void Instruction::GenerateCode(MaglevAssembler* masm,             \
                                 const ProcessingState& state) {    \
    Register left = ToRegister(LeftInput());                        \
    if (auto right_const = TryGetInt32ConstantInput(kRightIndex)) { \
      DCHECK(right_const);                                          \
      int right = *right_const & 31;                                \
      if (right != 0) {                                             \
        __ opcode(left, Immediate(right));                          \
      }                                                             \
    } else {                                                        \
      DCHECK_EQ(rcx, ToRegister(RightInput()));                     \
      __ opcode##_cl(left);                                         \
    }                                                               \
  }
DEF_SHIFT_BINOP(Int32ShiftLeft, shll)
DEF_SHIFT_BINOP(Int32ShiftRight, sarl)
DEF_SHIFT_BINOP(Int32ShiftRightLogical, shrl)
#undef DEF_SHIFT_BINOP

void Int32Increment::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void Int32Increment::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  __ incl(ToRegister(ValueInput()));
}

void Int32Decrement::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void Int32Decrement::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  __ decl(ToRegister(ValueInput()));
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  __ incl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  __ decl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  // Deopt when the result would be -0.
  __ testl(value, value);
  __ EmitEagerDeoptIf(zero, DeoptimizeReason::kOverflow, this);

  __ negl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32AbsWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register value = ToRegister(result());
  Label done;
  __ cmpl(value, Immediate(0));
  __ j(greater_equal, &done);
  __ negl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
  __ bind(&done);
}

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(ValueInput());
  __ notl(value);
}

void Float64Add::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  __ Addsd(left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  __ Subsd(left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  __ Mulsd(left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  __ Divsd(left, right);
}

void Float64Modulus::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  RequireSpecificTemporary(rax);
  DefineAsRegister(this);
}

void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  // Approach copied from code-generator-x64.cc
  // Allocate space to use fld to move the value to the FPU stack.
  __ AllocateStackSpace(kDoubleSize);
  Operand scratch_stack_space = Operand(rsp, 0);
  __ Movsd(scratch_stack_space, ToDoubleRegister(RightInput()));
  __ fld_d(scratch_stack_space);
  __ Movsd(scratch_stack_space, ToDoubleRegister(LeftInput()));
  __ fld_d(scratch_stack_space);
  // Loop while fprem isn't done.
  Label mod_loop;
  __ bind(&mod_loop);
  // This instructions traps on all kinds inputs, but we are assuming the
  // floating point control word is set to ignore them all.
  __ fprem();
  // The following 2 instruction implicitly use rax.
  __ fnstsw_ax();
  if (CpuFeatures::IsSupported(SAHF)) {
    CpuFeatureScope sahf_scope(masm, SAHF);
    __ sahf();
  } else {
    __ shrl(rax, Immediate(8));
    __ andl(rax, Immediate(0xFF));
    __ pushq(rax);
    __ popfq();
  }
  __ j(parity_even, &mod_loop);
  // Move output to stack and clean up.
  __ fstp(1);
  __ fstp_d(scratch_stack_space);
  __ Movsd(ToDoubleRegister(result()), scratch_stack_space);
  __ addq(rsp, Immediate(kDoubleSize));
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}

void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  __ Negpd(value, value, kScratchRegister);
}

void Float64Abs::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister out = ToDoubleRegister(result());
  __ Abspd(out, out, kScratchRegister);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(ValueInput());
  DoubleRegister out = ToDoubleRegister(result());

  if (kind_ == Kind::kNearest) {
    MaglevAssembler::TemporaryRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireDouble();
    __ Move(temp, in);
    __ Roundsd(out, in, kRoundToNearest);
    // RoundToNearest rounds to even on tie, while JS expects it to round
    // towards +Infinity. Fix the difference by checking if we rounded down by
    // exactly 0.5, and if so, round to the other side.
    __ Subsd(temp, out);
    __ Move(kScratchDoubleReg, 0.5);
    Label done;
    __ Ucomisd(temp, kScratchDoubleReg);
    __ JumpIf(not_equal, &done, Label::kNear);
    // Fix wrong tie-to-even by adding 0.5 twice.
    __ Addsd(out, kScratchDoubleReg);
    __ Addsd(out, kScratchDoubleReg);
    __ bind(&done);
  } else if (kind_ == Kind::kFloor) {
    __ Roundsd(out, in, kRoundDown);
  } else if (kind_ == Kind::kCeil) {
    __ Roundsd(out, in, kRoundUp);
  }
}

int Float64Exponentiate::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(2);
}
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(LeftInput(), xmm0);
  UseFixed(RightInput(), xmm1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(2);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 2);
}

namespace {

template <typename LeftIsMinFunction, typename RightIsMinFunction>
void Float64MinMaxHelper(MaglevAssembler* masm, DoubleRegister left_and_out,
                         DoubleRegister right,
                         LeftIsMinFunction left_is_min_code,
                         RightIsMinFunction right_is_min_code) {
  Label left_is_min, right_is_min, has_nan, done;
  __ Ucomisd(left_and_out, right);
  __ j(parity_even, &has_nan);
  __ j(below, &left_is_min, Label::kNear);
  __ j(above, &right_is_min, Label::kNear);

  // The values are equal. We still need to handle -0 vs 0.
  __ Movmskpd(kScratchRegister, left_and_out);
  __ testl(kScratchRegister, Immediate(1));  // Sign bit.
  // If left has sign bit 0, right might still have the sign bit set.
  __ j(zero, &right_is_min, Label::kNear);

  __ bind(&left_is_min);
  left_is_min_code();
  __ jmp(&done);

  __ bind(&right_is_min);
  right_is_min_code();
  __ jmp(&done);

  __ bind(&has_nan);
  __ Pcmpeqd(left_and_out, left_and_out);

  __ bind(&done);
}

}  // namespace

void Float64Min::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
}

void Float64Min::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left_and_out = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  Float64MinMaxHelper(
      masm, left_and_out, right,
      // Left is min
      [&]() {},
      // Right is min
      [&]() { __ Move(left_and_out, right); });
}

void Float64Max::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  UseRegister(RightInput());
  DefineSameAsFirst(this);
}

void Float64Max::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left_and_out = ToDoubleRegister(LeftInput());
  DoubleRegister right = ToDoubleRegister(RightInput());
  Float64MinMaxHelper(
      masm, left_and_out, right,
      // Left is min
      [&]() { __ Move(left_and_out, right); },
      // Right is min
      [&]() {});
}

int Float64Ieee754Unary::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(1);
}
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(ValueInput(), xmm0);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(1);
  __ CallCFunction(ieee_function_ref(), 1);
}

int Float64Ieee754Binary::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(2);
}
void Float64Ieee754Binary::SetValueLocationConstraints() {
  UseFixed(LeftInput(), xmm0);
  UseFixed(RightInput(), xmm1);
  DefineSameAsFirst(this);
}
void Float64Ieee754Binary::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(2);
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
  __ Sqrtsd(result_register, value);
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
  __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
  __ Subsd(value, kScratchDoubleReg);
}

void HoleyFloat64ToSilencedFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void HoleyFloat64ToSilencedFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  // The hole value is a signalling NaN, so just silence it to get the
  // float64 value.
  __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
  __ Subsd(value, kScratchDoubleReg);
}

void Float64ToSilencedFloat64::SetValueLocationConstraints() {
  UseRegister(ValueInput());
  DefineSameAsFirst(this);
}
void Float64ToSilencedFloat64::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  // The hole value is a signalling NaN, so just silence it to get the
  // float64 value.
  __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
  __ Subsd(value, kScratchDoubleReg);
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
}
void HoleyFloat64ConvertHoleToUndefined::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(ValueInput());
  Label done;
  __ JumpIfNotHoleNan(value, kScratchRegister, &done);
  __ Move(value, UndefinedNan());
  __ bind(&done);
}
#endif  // V8_ENABLE_UNDEFINED_DOUBLE

namespace {

enum class ReduceInterruptBudgetType { kLoop, kReturn };

void HandleInterruptsAndTiering(MaglevAssembler* masm, ZoneLabelRef done,
                                Node* node, ReduceInterruptBudgetType type) {
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
    __ cmpq(rsp, __ StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));
    __ j(above, &next);

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      __ Move(kContextRegister, masm->native_context().object());
      __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ jmp(*done);  // All done, continue.

    __ bind(&next);
  }

  // No pending interrupts. Call into the TieringManager if needed.
  {
    SaveRegisterStateForCall save_register_state(masm,
                                                 node->register_snapshot());
    __ Move(kContextRegister, masm->native_context().object());
    __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
    // Note: must not cause a lazy deopt!
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
    save_register_state.DefineSafepoint();
  }
  __ jmp(*done);
}

void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   Register feedback_cell,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  __ subl(FieldOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(less, HandleInterruptsAndTiering, done, node, type);
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
  Register actual_params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to reuse the
  // incoming argc's register (if it's still valid).
  __ movq(actual_params_size,
          MemOperand(rbp, StandardFrameConstants::kArgCOffset));

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label drop_dynamic_arg_size;
  __ cmpq(actual_params_size, Immediate(formal_params_size));
  __ j(greater, &drop_dynamic_arg_size);

  // Drop receiver + arguments according to static formal arguments size.
  __ Ret(formal_params_size * kSystemPointerSize, kScratchRegister);

  __ bind(&drop_dynamic_arg_size);
  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(actual_params_size, r9);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
