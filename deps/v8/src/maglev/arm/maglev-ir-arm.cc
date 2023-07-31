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

#define MAGLEV_NODE_NOT_IMPLEMENTED(Node)                    \
  do {                                                       \
    PrintF("Maglev: Node not yet implemented'" #Node "'\n"); \
    masm->set_failed(true);                                  \
  } while (false)

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(value_input());
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

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  Register out = ToRegister(result());
  __ add(out, value, Operand(1), SetCC);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
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
  if (code_input().node()->Is<Int32Constant>()) {
    UseAny(code_input());
  } else {
    UseRegister(code_input());
  }
  set_temporaries_needed(2);
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(BuiltinStringFromCharCode);
}

int BuiltinStringPrototypeCharCodeOrCodePointAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return 2;
}
void BuiltinStringPrototypeCharCodeOrCodePointAt::
    SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
}
void BuiltinStringPrototypeCharCodeOrCodePointAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Label done;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeOrCodePointAt(mode_, save_registers, ToRegister(result()),
                                 ToRegister(string_input()),
                                 ToRegister(index_input()), scratch, &done);
  __ bind(&done);
}

void FoldedAllocation::SetValueLocationConstraints() {
  UseRegister(raw_allocation());
  DefineAsRegister(this);
}

void FoldedAllocation::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  __ add(ToRegister(result()), ToRegister(raw_allocation()), Operand(offset()));
}

void CheckedTruncateFloat64ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToUint32::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckedTruncateFloat64ToUint32);
}

void CheckNumber::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckNumber::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckNumber);
}

int CheckedObjectToIndex::MaxCallStackArgs() const { return 0; }

void Int32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Int32ToNumber::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register object = ToRegister(result());
  Register value = ToRegister(input());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ add(scratch, value, value, SetCC);
  __ JumpToDeferredIf(
      vs,
      [](MaglevAssembler* masm, Register object, Register value,
         Register scratch, ZoneLabelRef done, Int32ToNumber* node) {
        MaglevAssembler::ScratchRegisterScope temps(masm);
        // We can include {scratch} back to the temporary set, since we jump
        // over its use to the label {done}.
        temps.Include(scratch);
        DoubleRegister double_value = temps.AcquireDouble();
        __ Int32ToDouble(double_value, value);
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ b(*done);
      },
      object, value, scratch, done, this);
  __ Move(object, scratch);
  __ bind(*done);
}

void Uint32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Uint32ToNumber::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Uint32ToNumber);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register out = ToRegister(result());
  __ add(out, left, right, SetCC);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Int32SubtractWithOverflow);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Int32MultiplyWithOverflow);
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Int32DivideWithOverflow);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseAndClobberRegister(left_input());
  UseAndClobberRegister(right_input());
  DefineAsRegister(this);
}
void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Int32ModulusWithOverflow);
}

#define DEF_BITWISE_BINOP(Instruction, opcode, right_modifier)   \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(left_input());                                   \
    UseRegister(right_input());                                  \
    DefineAsRegister(this);                                      \
  }                                                              \
                                                                 \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(left_input());                    \
    Register right = ToRegister(right_input());                  \
    Register out = ToRegister(result());                         \
    __ opcode(out, left, right_modifier(right));                 \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_, )
DEF_BITWISE_BINOP(Int32BitwiseOr, orr, )
DEF_BITWISE_BINOP(Int32BitwiseXor, eor, )
DEF_BITWISE_BINOP(Int32ShiftLeft, lsl, Operand)
DEF_BITWISE_BINOP(Int32ShiftRight, asr, Operand)
DEF_BITWISE_BINOP(Int32ShiftRightLogical, lsr, Operand)
#undef DEF_BITWISE_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(value_input());
  Register out = ToRegister(result());
  __ mvn(out, Operand(value));
}

void Float64Add::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vadd(out, left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vsub(out, left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vmul(out, left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vdiv(out, left, right);
}

void Float64Modulus::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Float64Modulus);
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  __ vneg(out, value);
}

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(Float64Round);
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 2);
  __ MovToFloatParameters(left, right);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 0, 2);
  __ MovFromFloatResult(out);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  FrameScope scope(masm, StackFrame::MANUAL);
  __ PrepareCallCFunction(0, 1);
  __ MovToFloatParameter(value);
  __ CallCFunction(ieee_function_, 0, 1);
  __ MovFromFloatResult(out);
}

void CheckJSTypedArrayBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  if (ElementsKindSize(elements_kind_) == 1) {
    UseRegister(index_input());
  } else {
    UseAndClobberRegister(index_input());
  }
}
void CheckJSTypedArrayBounds::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckJSTypedArrayBounds);
}

int CheckJSDataViewBounds::MaxCallStackArgs() const { return 1; }
void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
  set_temporaries_needed(1);
}
void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  USE(element_type_);
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckJSDataViewBounds);
}

void CheckedInternalizedString::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
}
void CheckedInternalizedString::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(CheckedInternalizedString);
}

void HoleyFloat64ToMaybeNanFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void HoleyFloat64ToMaybeNanFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(HoleyFloat64ToMaybeNanFloat64);
}

namespace {

enum class ReduceInterruptBudgetType { kLoop, kReturn };

void HandleInterruptsAndTiering(MaglevAssembler* masm, ZoneLabelRef done,
                                Node* node, ReduceInterruptBudgetType type,
                                Register scratch0) {
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
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register feedback_cell = scratch;
  Register budget = temps.Acquire();
  __ ldr(feedback_cell,
         MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedField(
      feedback_cell,
      FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
  __ ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ sub(budget, budget, Operand(amount));
  __ str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(lt, HandleInterruptsAndTiering, done, node, type,
                      scratch);
  __ bind(*done);
}

}  // namespace

int ReduceInterruptBudgetForLoop::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForLoop::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void ReduceInterruptBudgetForLoop::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ReduceInterruptBudgetType::kLoop,
                                amount());
}

int ReduceInterruptBudgetForReturn::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForReturn::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void ReduceInterruptBudgetForReturn::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ReduceInterruptBudgetType::kReturn,
                                amount());
}

namespace {

template <bool check_detached, typename ResultReg, typename NodeT>
void GenerateTypedArrayLoad(MaglevAssembler* masm, NodeT* node, Register object,
                            Register index, ResultReg result_reg,
                            ElementsKind kind) {
  MAGLEV_NODE_NOT_IMPLEMENTED(GenerateTypedArrayLoad);
}

template <bool check_detached, typename ValueReg, typename NodeT>
void GenerateTypedArrayStore(MaglevAssembler* masm, NodeT* node,
                             Register object, Register index, ValueReg value,
                             ElementsKind kind) {
  MAGLEV_NODE_NOT_IMPLEMENTED(GenerateTypedArrayStore);
}

}  // namespace

#define DEF_LOAD_TYPED_ARRAY(Name, ResultReg, ToResultReg, check_detached) \
  void Name::SetValueLocationConstraints() {                               \
    UseRegister(object_input());                                           \
    UseRegister(index_input());                                            \
    DefineAsRegister(this);                                                \
  }                                                                        \
  void Name::GenerateCode(MaglevAssembler* masm,                           \
                          const ProcessingState& state) {                  \
    Register object = ToRegister(object_input());                          \
    Register index = ToRegister(index_input());                            \
    ResultReg result_reg = ToResultReg(result());                          \
                                                                           \
    GenerateTypedArrayLoad<check_detached>(masm, this, object, index,      \
                                           result_reg, elements_kind_);    \
  }

DEF_LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElement, Register, ToRegister,
                     /*check_detached*/ true)
DEF_LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElementNoDeopt, Register,
                     ToRegister,
                     /*check_detached*/ false)

DEF_LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElement, Register, ToRegister,
                     /*check_detached*/ true)
DEF_LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElementNoDeopt, Register,
                     ToRegister,
                     /*check_detached*/ false)

DEF_LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElement, DoubleRegister,
                     ToDoubleRegister,
                     /*check_detached*/ true)
DEF_LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElementNoDeopt, DoubleRegister,
                     ToDoubleRegister, /*check_detached*/ false)
#undef DEF_LOAD_TYPED_ARRAY

#define DEF_STORE_TYPED_ARRAY(Name, ValueReg, ToValueReg, check_detached)     \
  void Name::SetValueLocationConstraints() {                                  \
    UseRegister(object_input());                                              \
    UseRegister(index_input());                                               \
    UseRegister(value_input());                                               \
  }                                                                           \
  void Name::GenerateCode(MaglevAssembler* masm,                              \
                          const ProcessingState& state) {                     \
    Register object = ToRegister(object_input());                             \
    Register index = ToRegister(index_input());                               \
    ValueReg value = ToValueReg(value_input());                               \
                                                                              \
    GenerateTypedArrayStore<check_detached>(masm, this, object, index, value, \
                                            elements_kind_);                  \
  }

DEF_STORE_TYPED_ARRAY(StoreIntTypedArrayElement, Register, ToRegister,
                      /*check_detached*/ true)
DEF_STORE_TYPED_ARRAY(StoreIntTypedArrayElementNoDeopt, Register, ToRegister,
                      /*check_detached*/ false)

DEF_STORE_TYPED_ARRAY(StoreDoubleTypedArrayElement, DoubleRegister,
                      ToDoubleRegister,
                      /*check_detached*/ true)
DEF_STORE_TYPED_ARRAY(StoreDoubleTypedArrayElementNoDeopt, DoubleRegister,
                      ToDoubleRegister, /*check_detached*/ false)

#undef DEF_STORE_TYPED_ARRAY

void StoreFixedDoubleArrayElement::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  UseRegister(value_input());
  set_temporaries_needed(1);
}
void StoreFixedDoubleArrayElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(StoreFixedDoubleArrayElement);
}

void StoreDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreDoubleField::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(StoreDoubleField);
}

void LoadSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  DefineAsRegister(this);
}
void LoadSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(LoadSignedIntDataViewElement);
}

void StoreSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (ExternalArrayElementSize(type_) > 1) {
    UseAndClobberRegister(value_input());
  } else {
    UseRegister(value_input());
  }
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
}
void StoreSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(StoreSignedIntDataViewElement);
}

void LoadDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void LoadDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(LoadDoubleDataViewElement);
}

void StoreDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  UseRegister(value_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
}
void StoreDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(StoreDoubleDataViewElement);
}

void SetPendingMessage::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}

void SetPendingMessage::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  MAGLEV_NODE_NOT_IMPLEMENTED(SetPendingMessage);
}

int FunctionEntryStackCheck::MaxCallStackArgs() const { return 1; }
void FunctionEntryStackCheck::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void FunctionEntryStackCheck::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  if (!masm->code_gen_state()->needs_stack_check()) return;
  // Stack check. This folds the checks for both the interrupt stack limit
  // check and the real stack limit into one by just checking for the
  // interrupt limit. The interrupt limit is either equal to the real
  // stack limit or tighter. By ensuring we have space until that limit
  // after building the frame we can quickly precheck both at once.
  MaglevAssembler::ScratchRegisterScope temps(masm);
  const int stack_check_offset = masm->code_gen_state()->stack_check_offset();
  Register stack_cmp_reg = sp;
  if (stack_check_offset > kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = temps.Acquire();
    __ sub(stack_cmp_reg, sp, Operand(stack_check_offset));
  }
  Register interrupt_stack_limit = temps.Acquire();
  __ LoadStackLimit(interrupt_stack_limit,
                    StackLimitKind::kInterruptStackLimit);
  __ cmp(stack_cmp_reg, interrupt_stack_limit);

  ZoneLabelRef deferred_call_stack_guard_return(masm);
  __ JumpToDeferredIf(
      lo,
      [](MaglevAssembler* masm, FunctionEntryStackCheck* node,
         ZoneLabelRef done, int stack_check_offset) {
        ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt call");
        {
          SaveRegisterStateForCall save_register_state(
              masm, node->register_snapshot());
          // Push the frame size
          __ Push(Smi::FromInt(stack_check_offset));
          __ CallRuntime(Runtime::kStackGuardWithGap, 1);
          save_register_state.DefineSafepointWithLazyDeopt(
              node->lazy_deopt_info());
        }
        __ b(*done);
      },
      this, deferred_call_stack_guard_return, stack_check_offset);
  __ bind(*deferred_call_stack_guard_return);
}

// ---
// Control nodes
// ---
void Return::SetValueLocationConstraints() {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size =
      masm->compilation_info()->toplevel_compilation_unit()->parameter_count();

  // We're not going to continue execution, so we can use an arbitrary register
  // here instead of relying on temporaries from the register allocator.
  Register actual_params_size = r4;
  Register params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
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
  __ DropArguments(params_size, MacroAssembler::kCountIsInteger,
                   MacroAssembler::kCountIncludesReceiver);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
