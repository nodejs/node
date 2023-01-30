// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/arm64/assembler-arm64-inl.h"
#include "src/codegen/arm64/register-arm64.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/maglev/arm64/maglev-assembler-arm64-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

// TODO(v8:7700): Remove this logic when all nodes are implemented.
class MaglevUnimplementedIRNode {
 public:
  MaglevUnimplementedIRNode() {}
  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block) {}
  template <typename NodeT>
  void Process(NodeT* node, const ProcessingState& state);
  bool has_unimplemented_node() const { return has_unimplemented_node_; }

 private:
  bool has_unimplemented_node_ = false;
};

#define UNIMPLEMENTED_NODE(Node, ...)                                     \
  void Node::SetValueLocationConstraints() {}                             \
                                                                          \
  void Node::GenerateCode(MaglevAssembler* masm,                          \
                          const ProcessingState& state) {                 \
    USE(__VA_ARGS__);                                                     \
  }                                                                       \
  template <>                                                             \
  void MaglevUnimplementedIRNode::Process(Node* node,                     \
                                          const ProcessingState& state) { \
    std::cerr << "Unimplemented Maglev IR Node: " #Node << std::endl;     \
    has_unimplemented_node_ = true;                                       \
  }

#define UNIMPLEMENTED_NODE_WITH_CALL(Node, ...)    \
  int Node::MaxCallStackArgs() const { return 0; } \
  UNIMPLEMENTED_NODE(Node, __VA_ARGS__)

// If we don't have a specialization, it means we have implemented the node.
template <typename NodeT>
void MaglevUnimplementedIRNode::Process(NodeT* node,
                                        const ProcessingState& state) {}

bool MaglevGraphHasUnimplementedNode(Graph* graph) {
  GraphProcessor<MaglevUnimplementedIRNode> processor;
  processor.ProcessGraph(graph);
  return processor.node_processor().has_unimplemented_node();
}

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();
  __ negs(out, value);
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
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, value, Immediate(1));
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
  Register out = ToRegister(result()).W();
  __ Subs(out, value, Immediate(1));
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

UNIMPLEMENTED_NODE_WITH_CALL(Float64Ieee754Unary)
UNIMPLEMENTED_NODE_WITH_CALL(BuiltinStringFromCharCode)
UNIMPLEMENTED_NODE_WITH_CALL(CallBuiltin)
UNIMPLEMENTED_NODE_WITH_CALL(CallRuntime)
UNIMPLEMENTED_NODE_WITH_CALL(CallWithArrayLike)
UNIMPLEMENTED_NODE_WITH_CALL(CallWithSpread)
UNIMPLEMENTED_NODE_WITH_CALL(Construct)
UNIMPLEMENTED_NODE_WITH_CALL(ConstructWithSpread)
UNIMPLEMENTED_NODE_WITH_CALL(ConvertReceiver, mode_)
UNIMPLEMENTED_NODE(GeneratorRestoreRegister)
UNIMPLEMENTED_NODE(LoadSignedIntDataViewElement, type_)
UNIMPLEMENTED_NODE(LoadDoubleDataViewElement)
UNIMPLEMENTED_NODE(LoadSignedIntTypedArrayElement, elements_kind_)
UNIMPLEMENTED_NODE(LoadUnsignedIntTypedArrayElement, elements_kind_)
UNIMPLEMENTED_NODE(LoadDoubleTypedArrayElement, elements_kind_)
UNIMPLEMENTED_NODE(CheckedSmiTagUint32)
UNIMPLEMENTED_NODE_WITH_CALL(CheckedObjectToIndex)
UNIMPLEMENTED_NODE(CheckedTruncateNumberToInt32)
UNIMPLEMENTED_NODE(CheckedInt32ToUint32)
UNIMPLEMENTED_NODE(CheckedUint32ToInt32)
UNIMPLEMENTED_NODE(ChangeInt32ToFloat64)
UNIMPLEMENTED_NODE(ChangeUint32ToFloat64)
UNIMPLEMENTED_NODE(CheckedTruncateFloat64ToInt32)
UNIMPLEMENTED_NODE(CheckedTruncateFloat64ToUint32)
UNIMPLEMENTED_NODE(TruncateUint32ToInt32)
UNIMPLEMENTED_NODE(TruncateFloat64ToInt32)
UNIMPLEMENTED_NODE(HoleyFloat64Box)
UNIMPLEMENTED_NODE(LogicalNot)
UNIMPLEMENTED_NODE(SetPendingMessage)
UNIMPLEMENTED_NODE_WITH_CALL(StringAt)
UNIMPLEMENTED_NODE(TestUndetectable)
UNIMPLEMENTED_NODE(TestTypeOf, literal_)
UNIMPLEMENTED_NODE_WITH_CALL(ToObject)
UNIMPLEMENTED_NODE_WITH_CALL(ToString)
UNIMPLEMENTED_NODE(AssertInt32, condition_, reason_)
UNIMPLEMENTED_NODE(CheckDynamicValue)
UNIMPLEMENTED_NODE(CheckUint32IsSmi)
UNIMPLEMENTED_NODE(CheckHeapObject)
UNIMPLEMENTED_NODE(CheckJSArrayBounds)
UNIMPLEMENTED_NODE(CheckJSDataViewBounds, element_type_)
UNIMPLEMENTED_NODE(CheckJSObjectElementsBounds)
UNIMPLEMENTED_NODE(CheckJSTypedArrayBounds, elements_kind_)
UNIMPLEMENTED_NODE(CheckMaps, check_type_)
UNIMPLEMENTED_NODE_WITH_CALL(CheckMapsWithMigration, check_type_)
UNIMPLEMENTED_NODE(CheckNumber)
UNIMPLEMENTED_NODE(CheckSmi)
UNIMPLEMENTED_NODE(CheckString, check_type_)
UNIMPLEMENTED_NODE(CheckValue)
UNIMPLEMENTED_NODE(CheckInstanceType, check_type_)
UNIMPLEMENTED_NODE_WITH_CALL(GeneratorStore)
UNIMPLEMENTED_NODE_WITH_CALL(JumpLoopPrologue, loop_depth_, unit_)
UNIMPLEMENTED_NODE_WITH_CALL(StoreMap)
UNIMPLEMENTED_NODE(StoreDoubleField)
UNIMPLEMENTED_NODE(StoreSignedIntDataViewElement, type_)
UNIMPLEMENTED_NODE(StoreDoubleDataViewElement)
UNIMPLEMENTED_NODE(StoreTaggedFieldNoWriteBarrier)
UNIMPLEMENTED_NODE_WITH_CALL(StoreTaggedFieldWithWriteBarrier)
UNIMPLEMENTED_NODE_WITH_CALL(ThrowReferenceErrorIfHole)
UNIMPLEMENTED_NODE_WITH_CALL(ThrowSuperNotCalledIfHole)
UNIMPLEMENTED_NODE_WITH_CALL(ThrowSuperAlreadyCalledIfNotHole)
UNIMPLEMENTED_NODE_WITH_CALL(ThrowIfNotSuperConstructor)
UNIMPLEMENTED_NODE(BranchIfUndefinedOrNull)
UNIMPLEMENTED_NODE(BranchIfJSReceiver)
UNIMPLEMENTED_NODE(Switch)

int BuiltinStringPrototypeCharCodeAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return 2;
}
void BuiltinStringPrototypeCharCodeAt::SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
}
void BuiltinStringPrototypeCharCodeAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Label done;
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeAt(save_registers, ToRegister(result()),
                      ToRegister(string_input()), ToRegister(index_input()),
                      Register::no_reg(), &done);
  __ bind(&done);
}

int CreateEmptyObjectLiteral::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void CreateEmptyObjectLiteral::SetValueLocationConstraints() {
  DefineAsRegister(this);
}
void CreateEmptyObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register object = ToRegister(result());
  RegisterSnapshot save_registers = register_snapshot();
  __ Allocate(save_registers, object, map().instance_size());
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();
  __ Move(scratch, map().object());
  __ StoreTaggedField(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  __ LoadRoot(scratch, RootIndex::kEmptyFixedArray);
  __ StoreTaggedField(
      scratch, FieldMemOperand(object, JSObject::kPropertiesOrHashOffset));
  __ StoreTaggedField(scratch,
                      FieldMemOperand(object, JSObject::kElementsOffset));
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  for (int i = 0; i < map().GetInObjectProperties(); i++) {
    int offset = map().GetInObjectPropertyOffset(i);
    __ StoreTaggedField(scratch, FieldMemOperand(object, offset));
  }
}

void CheckSymbol::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckSymbol::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotASymbol, this);
  }
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();
  __ CmpObjectType(object, SYMBOL_TYPE, scratch);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotASymbol, this);
}

void Int32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Int32ToNumber::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register object = ToRegister(result());
  Register value = ToRegister(input());
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireW();
  __ Adds(scratch, value.W(), value.W());
  __ JumpToDeferredIf(
      vs,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Int32ToNumber* node) {
        DoubleRegister double_value = kScratchDoubleReg;
        __ Scvtf(double_value, value.W());
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ B(*done);
      },
      object, value, done, this);
  __ Mov(object.W(), scratch);
  __ bind(*done);
}

void Uint32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Uint32ToNumber::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register value = ToRegister(input());
  Register object = ToRegister(result());
  __ Cmp(value.W(), Immediate(Smi::kMaxValue));
  __ JumpToDeferredIf(
      hi,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Uint32ToNumber* node) {
        DoubleRegister double_value = kScratchDoubleReg;
        __ Ucvtf(double_value, value.W());
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ B(*done);
      },
      object, value, done, this);
  __ Add(object, value, value);
  __ bind(*done);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, left, right);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

// UNIMPLEMENTED_NODE(Int32SubtractWithOverflow)
void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();
  __ Subs(out, left, right);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();

  // TODO(leszeks): peephole optimise multiplication by a constant.

  UseScratchRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out.X();
  if (out_alias_input) {
    res = temps.AcquireX();
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
    UseScratchRegisterScope temps(masm);
    Register temp = temps.AcquireW();
    __ orr(temp, left, right);
    __ cmp(temp, Immediate(0));
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ EmitEagerDeoptIf(lt, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
  if (out_alias_input) {
    __ Move(out, res.W());
  }
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
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

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        // TODO(leszeks): Using kNotInt32 here, but kDivisionByZero would be
        // better. Right now all eager deopts in a node have to be the same --
        // we should allow a node to emit multiple eager deopts with different
        // reasons.
        __ EmitEagerDeoptIf(eq, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is zero, as that would produce minus zero.
        __ Cmp(left, Immediate(0));
        // TODO(leszeks): Better DeoptimizeReason = kMinusZero.
        __ EmitEagerDeoptIf(eq, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ Cmp(left, Immediate(kMinInt));
        __ JumpIf(ne, *done);
        __ Cmp(right, Immediate(-1));
        __ JumpIf(ne, *done);
        // TODO(leszeks): Better DeoptimizeReason = kOverflow, but
        // eager_deopt_info is already configured as kNotInt32.
        __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
      },
      done, left, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  UseScratchRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.AcquireW();
  }
  __ sdiv(res, left, right);

  // Check that the remainder is zero.
  Register temp = temps.AcquireW();
  __ Msub(temp, res, right, left);
  __ Cmp(temp, Immediate(0));
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, this);

  __ Move(out, res);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
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

  Register left = ToRegister(left_input()).W();
  Register right_maybe_neg = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);

  UseScratchRegisterScope temps(masm);
  Register right = temps.AcquireW();
  __ Move(right, right_maybe_neg);

  __ Cmp(right, Immediate(0));
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register right,
         Int32ModulusWithOverflow* node) {
        __ Negs(right, right);
        __ EmitEagerDeoptIf(eq, DeoptimizeReason::kDivisionByZero, node);
        __ Jump(*rhs_checked);
      },
      rhs_checked, right, this);
  __ bind(*rhs_checked);

  __ Cmp(left, Immediate(0));
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireW();
    __ JumpToDeferredIf(
        lt,
        [](MaglevAssembler* masm, ZoneLabelRef done, Register left_neg,
           Register right, Register out, Register scratch,
           Int32ModulusWithOverflow* node) {
          Register left = scratch;
          Register res = node->general_temporaries().first().W();
          __ neg(left, left_neg);
          __ udiv(res, left, right);
          __ msub(out, res, right, left);
          __ cmp(out, Immediate(0));
          // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
          // only allows one deopt reason per IR.
          __ EmitEagerDeoptIf(eq, DeoptimizeReason::kDivisionByZero, node);
          __ neg(out, out);
          __ b(*done);
        },
        done, left, right, out, scratch, this);
  }

  Label right_not_power_of_2;
  Register mask = temps.AcquireW();
  __ Add(mask, right, Immediate(-1));
  __ Tst(mask, right);
  __ JumpIf(ne, &right_not_power_of_2);

  // {right} is power of 2.
  __ And(out, mask, left);
  __ Jump(*done);

  __ bind(&right_not_power_of_2);

  // We store the result of the Udiv in a temporary register in case {out} is
  // the same as {left} or {right}: we'll still need those 2 registers intact to
  // get the remainder.
  Register res = mask;
  __ Udiv(res, left, right);
  __ Msub(out, res, right, left);

  __ bind(*done);
}

#define DEF_BITWISE_BINOP(Instruction, opcode)                   \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(left_input());                                   \
    UseRegister(right_input());                                  \
    DefineAsRegister(this);                                      \
  }                                                              \
                                                                 \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(left_input()).W();                \
    Register right = ToRegister(right_input()).W();              \
    Register out = ToRegister(result()).W();                     \
    __ opcode(out, left, right);                                 \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_)
DEF_BITWISE_BINOP(Int32BitwiseOr, orr)
DEF_BITWISE_BINOP(Int32BitwiseXor, eor)
DEF_BITWISE_BINOP(Int32ShiftLeft, lslv)
DEF_BITWISE_BINOP(Int32ShiftRight, asrv)
DEF_BITWISE_BINOP(Int32ShiftRightLogical, lsrv)
#undef DEF_BITWISE_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();
  __ mvn(out, value);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register result = ToRegister(this->result());
  Label is_true, end;
  // TODO(leszeks): Investigate using cmov here.
  __ CompareAndBranch(left, right, ConditionFor(kOperation), &is_true);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kFalseValue);
  __ Jump(&end);
  {
    __ bind(&is_true);
    __ LoadRoot(result, RootIndex::kTrueValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Int32Equal)
DEF_OPERATION(Int32StrictEqual)
DEF_OPERATION(Int32LessThan)
DEF_OPERATION(Int32LessThanOrEqual)
DEF_OPERATION(Int32GreaterThan)
DEF_OPERATION(Int32GreaterThanOrEqual)
#undef DEF_OPERATION

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
  __ Fadd(out, left, right);
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
  __ Fsub(out, left, right);
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
  __ Fmul(out, left, right);
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
  __ Fdiv(out, left, right);
}

int Float64Modulus::MaxCallStackArgs() const { return 0; }
void Float64Modulus::SetValueLocationConstraints() {
  UseFixed(left_input(), v0);
  UseFixed(right_input(), v1);
  DefineSameAsFirst(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fneg(out, value);
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(left_input(), v0);
  UseFixed(right_input(), v1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 2);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_false, end;
  __ Fcmp(left, right);
  // Check for NaN first.
  __ JumpIf(vs, &is_false);
  __ JumpIf(NegateCondition(ConditionFor(kOperation)), &is_false);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kTrueValue);
  __ Jump(&end);
  {
    __ bind(&is_false);
    __ LoadRoot(result, RootIndex::kFalseValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Float64Equal)
DEF_OPERATION(Float64StrictEqual)
DEF_OPERATION(Float64LessThan)
DEF_OPERATION(Float64LessThanOrEqual)
DEF_OPERATION(Float64GreaterThan)
DEF_OPERATION(Float64GreaterThanOrEqual)
#undef DEF_OPERATION

void CheckInt32IsSmi::SetValueLocationConstraints() { UseRegister(input()); }
void CheckInt32IsSmi::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  // TODO(leszeks): This basically does a SmiTag and throws the result away.
  // Don't throw the result away if we want to actually use it.
  Register reg = ToRegister(input()).W();
  __ Adds(wzr, reg, reg);
  DCHECK_REGLIST_EMPTY(RegList{reg} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kNotASmi, this);
}

void CheckedSmiTagInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedSmiTagInt32::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register reg = ToRegister(input()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, reg, reg);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void CheckedInternalizedString::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineSameAsFirst(this);
}
void CheckedInternalizedString::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();
  Register object = ToRegister(object_input());

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }

  __ LoadMap(scratch, object);
  __ RecordComment("Test IsInternalizedString");
  // Go to the slow path if this is a non-string, or a non-internalised string.
  __ Ldrh(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ Tst(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  static_assert((kStringTag | kInternalizedTag) == 0);
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      ne,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         CheckedInternalizedString* node, EagerDeoptInfo* deopt_info,
         Register instance_type) {
        __ RecordComment("Deferred Test IsThinString");
        static_assert(kThinStringTagBit > 0);
        // Deopt if this isn't a string.
        __ Tst(instance_type, Immediate(kIsNotStringMask));
        __ JumpIf(ne, deopt_info->deopt_entry_label());
        // Deopt if this isn't a thin string.
        __ Tst(instance_type, Immediate(kThinStringTagBit));
        __ JumpIf(eq, deopt_info->deopt_entry_label());
        __ LoadTaggedPointerField(
            object, FieldMemOperand(object, ThinString::kActualOffset));
        if (v8_flags.debug_code) {
          __ RecordComment("DCHECK IsInternalizedString");
          Register scratch = instance_type;
          __ LoadMap(scratch, object);
          __ Ldrh(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
          __ Tst(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
          static_assert((kStringTag | kInternalizedTag) == 0);
          __ Check(eq, AbortReason::kUnexpectedValue);
        }
        __ jmp(*done);
      },
      done, object, this, eager_deopt_info(), scratch);
  __ bind(*done);
}

void UnsafeSmiTag::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void UnsafeSmiTag::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register reg = ToRegister(input()).W();
  Register out = ToRegister(result()).W();
  if (v8_flags.debug_code) {
    if (input().node()->properties().value_representation() ==
        ValueRepresentation::kUint32) {
      __ cmp(reg, Immediate(Smi::kMaxValue));
      __ Check(ls, AbortReason::kInputDoesNotFitSmi);
    }
  }
  __ Adds(out, reg, reg);
  if (v8_flags.debug_code) {
    __ Check(vc, AbortReason::kInputDoesNotFitSmi);
  }
}

void Float64Box::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Box::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  Register object = ToRegister(result());
  __ AllocateHeapNumber(register_snapshot(), object, value);
}

void CheckedFloat64Unbox::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedFloat64Unbox::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register value = ToRegister(input());
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi);
  // If Smi, convert to Float64.
  UseScratchRegisterScope temps(masm);
  Register temp = temps.AcquireX();
  __ SmiToInt32(temp, value);
  __ sxtw(temp, temp.W());
  __ scvtf(ToDoubleRegister(result()), temp);
  __ Jump(&done);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ Move(temp, FieldMemOperand(value, HeapObject::kMapOffset));
  __ CompareRoot(temp, RootIndex::kHeapNumberMap);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotANumber, this);
  __ Move(temp, FieldMemOperand(value, HeapNumber::kValueOffset));
  __ fmov(ToDoubleRegister(result()), temp);
  __ bind(&done);
}

void IncreaseInterruptBudget::SetValueLocationConstraints() {}
void IncreaseInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  UseScratchRegisterScope temps(masm);
  Register feedback_cell = temps.AcquireX();
  Register budget = temps.AcquireW();
  __ Ldr(feedback_cell,
         MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      feedback_cell,
      FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
  __ Ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ Add(budget, budget, Immediate(amount()));
  __ Str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
}

int ReduceInterruptBudget::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudget::SetValueLocationConstraints() {}
void ReduceInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  {
    UseScratchRegisterScope temps(masm);
    Register feedback_cell = temps.AcquireX();
    Register budget = temps.AcquireW();
    __ Ldr(feedback_cell,
           MemOperand(fp, StandardFrameConstants::kFunctionOffset));
    __ LoadTaggedPointerField(
        feedback_cell,
        FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
    __ Ldr(budget, FieldMemOperand(feedback_cell,
                                   FeedbackCell::kInterruptBudgetOffset));
    __ Sub(budget, budget, Immediate(amount()));
    __ Str(budget, FieldMemOperand(feedback_cell,
                                   FeedbackCell::kInterruptBudgetOffset));
  }

  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      lt,
      [](MaglevAssembler* masm, ZoneLabelRef done,
         ReduceInterruptBudget* node) {
        {
          SaveRegisterStateForCall save_register_state(
              masm, node->register_snapshot());
          UseScratchRegisterScope temps(masm);
          Register function = temps.AcquireX();
          __ Move(kContextRegister, static_cast<Handle<HeapObject>>(
                                        masm->native_context().object()));
          __ Ldr(function,
                 MemOperand(fp, StandardFrameConstants::kFunctionOffset));
          __ PushArgument(function);
          __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev,
                         1);
          save_register_state.DefineSafepointWithLazyDeopt(
              node->lazy_deopt_info());
        }
        __ B(*done);
      },
      done, this);
  __ bind(*done);
}

void LoadDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void LoadDoubleField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  UseScratchRegisterScope temps(masm);
  Register tmp = temps.AcquireX();
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldMemOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Ldr(ToDoubleRegister(result()),
         FieldMemOperand(tmp, HeapNumber::kValueOffset));
}

void LoadTaggedElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadTaggedElement::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();
  Register elements = temps.AcquireX();
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CompareObjectType(object, scratch, scratch, JS_OBJECT_TYPE);
    __ Assert(hs, AbortReason::kUnexpectedValue);
  }
  __ DecompressAnyTagged(elements,
                         FieldMemOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ CompareObjectType(elements, scratch, scratch, FIXED_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }

  __ Add(elements, elements, Operand(index, LSL, kTaggedSizeLog2));
  __ DecompressAnyTagged(ToRegister(result()),
                         FieldMemOperand(elements, FixedArray::kHeaderSize));
}

void LoadDoubleElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadDoubleElement::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.AcquireX();
  Register elements = temps.AcquireX();
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CompareObjectType(object, scratch, scratch, JS_OBJECT_TYPE);
    __ Assert(hs, AbortReason::kUnexpectedValue);
  }
  __ DecompressAnyTagged(elements,
                         FieldMemOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ CompareObjectType(elements, scratch, scratch, FIXED_DOUBLE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }
  __ Add(elements, elements, Operand(index, LSL, kDoubleSizeLog2));
  __ Ldr(ToDoubleRegister(result()),
         FieldMemOperand(elements, FixedArray::kHeaderSize));
}

void StringLength::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void StringLength::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  if (v8_flags.debug_code) {
    // Check if {object} is a string.
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.AcquireX();
    __ AssertNotSmi(object);
    __ LoadMap(scratch, object);
    __ CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                                LAST_STRING_TYPE);
    __ Check(ls, AbortReason::kUnexpectedValue);
  }
  __ Ldr(ToRegister(result()).W(),
         FieldMemOperand(object, String::kLengthOffset));
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
  // We cannot use scratch registers, since they're used in LeaveFrame and
  // DropArguments.
  Register actual_params_size = x9;
  Register params_size = x10;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
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
  __ bind(&corrected_args_count);

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(params_size, TurboAssembler::kCountIncludesReceiver);
  __ Ret();
}

void BranchIfFloat64Compare::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfFloat64Compare::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Fcmp(left, right);
  __ JumpIf(vs, if_false()->label());  // NaN check
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
