// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_RISCV64_BASELINE_ASSEMBLER_RISCV64_INL_H_
#define V8_BASELINE_RISCV64_BASELINE_ASSEMBLER_RISCV64_INL_H_

#include "src/baseline/baseline-assembler.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/interface-descriptors.h"
namespace v8 {
namespace internal {
namespace baseline {

class BaselineAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(BaselineAssembler* assembler)
      : assembler_(assembler),
        prev_scope_(assembler->scratch_register_scope_),
        wrapped_scope_(assembler->masm()) {
    if (!assembler_->scratch_register_scope_) {
      // If we haven't opened a scratch scope yet, for the first one add a
      // couple of extra registers.
      wrapped_scope_.Include(kScratchReg, kScratchReg2);
    }
    assembler_->scratch_register_scope_ = this;
  }
  ~ScratchRegisterScope() { assembler_->scratch_register_scope_ = prev_scope_; }

  Register AcquireScratch() { return wrapped_scope_.Acquire(); }

 private:
  BaselineAssembler* assembler_;
  ScratchRegisterScope* prev_scope_;
  UseScratchRegisterScope wrapped_scope_;
};

enum class Condition : uint32_t {
  kEqual = eq,
  kNotEqual = ne,

  kLessThan = lt,
  kGreaterThan = gt,
  kLessThanEqual = le,
  kGreaterThanEqual = ge,

  kUnsignedLessThan = Uless,
  kUnsignedGreaterThan = Ugreater,
  kUnsignedLessThanEqual = Uless_equal,
  kUnsignedGreaterThanEqual = Ugreater_equal,

  kOverflow = overflow,
  kNoOverflow = no_overflow,

  kZero = eq,
  kNotZero = ne,
};

inline internal::Condition AsMasmCondition(Condition cond) {
  return static_cast<internal::Condition>(cond);
}

namespace detail {

#ifdef DEBUG
inline bool Clobbers(Register target, MemOperand op) {
  return op.is_reg() && op.rm() == target;
}
#endif

}  // namespace detail

#define __ masm_->

MemOperand BaselineAssembler::RegisterFrameOperand(
    interpreter::Register interpreter_register) {
  return MemOperand(fp, interpreter_register.ToOperand() * kSystemPointerSize);
}
void BaselineAssembler::RegisterFrameAddress(
    interpreter::Register interpreter_register, Register rscratch) {
  return __ Add64(rscratch, fp,
                  interpreter_register.ToOperand() * kSystemPointerSize);
}
MemOperand BaselineAssembler::FeedbackVectorOperand() {
  return MemOperand(fp, BaselineFrameConstants::kFeedbackVectorFromFp);
}

void BaselineAssembler::Bind(Label* label) { __ bind(label); }

void BaselineAssembler::BindWithoutJumpTarget(Label* label) { __ bind(label); }

void BaselineAssembler::JumpTarget() {
  // Nop
}

void BaselineAssembler::Jump(Label* target, Label::Distance distance) {
  __ jmp(target);
}
void BaselineAssembler::JumpIfRoot(Register value, RootIndex index,
                                   Label* target, Label::Distance) {
  __ JumpIfRoot(value, index, target);
}
void BaselineAssembler::JumpIfNotRoot(Register value, RootIndex index,
                                      Label* target, Label::Distance) {
  __ JumpIfNotRoot(value, index, target);
}
void BaselineAssembler::JumpIfSmi(Register value, Label* target,
                                  Label::Distance) {
  __ JumpIfSmi(value, target);
}
void BaselineAssembler::JumpIfNotSmi(Register value, Label* target,
                                     Label::Distance) {
  __ JumpIfSmi(value, target);
}
void BaselineAssembler::JumpIfImmediate(Condition cc, Register left, int right,
                                        Label* target,
                                        Label::Distance distance) {
  JumpIf(cc, left, Operand(right), target, distance);
}
void BaselineAssembler::CallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(masm_,
                          __ CommentForOffHeapTrampoline("call", builtin));
  Register temp = t6;
  __ LoadEntryFromBuiltin(builtin, temp);
  __ Call(temp);
}

void BaselineAssembler::TailCallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(masm_,
                          __ CommentForOffHeapTrampoline("tail call", builtin));
  Register temp = t6;
  __ LoadEntryFromBuiltin(builtin, temp);
  __ Jump(temp);
}

void BaselineAssembler::TestAndBranch(Register value, int mask, Condition cc,
                                      Label* target, Label::Distance) {
  ScratchRegisterScope temps(this);
  Register tmp = temps.AcquireScratch();
  __ And(tmp, value, Operand(mask));
  __ Branch(target, AsMasmCondition(cc), tmp, Operand(zero_reg));
}

void BaselineAssembler::JumpIf(Condition cc, Register lhs, const Operand& rhs,
                               Label* target, Label::Distance) {
  __ Branch(target, AsMasmCondition(cc), lhs, Operand(rhs));
}
void BaselineAssembler::JumpIfObjectType(Condition cc, Register object,
                                         InstanceType instance_type,
                                         Register map, Label* target,
                                         Label::Distance) {
  ScratchRegisterScope temps(this);
  Register type = temps.AcquireScratch();
  __ GetObjectType(object, map, type);
  __ Branch(target, AsMasmCondition(cc), type, Operand(instance_type));
}
void BaselineAssembler::JumpIfInstanceType(Condition cc, Register map,
                                           InstanceType instance_type,
                                           Label* target, Label::Distance) {
  ScratchRegisterScope temps(this);
  Register type = temps.AcquireScratch();
  if (FLAG_debug_code) {
    __ AssertNotSmi(map);
    __ GetObjectType(map, type, type);
    __ Assert(eq, AbortReason::kUnexpectedValue, type, Operand(MAP_TYPE));
  }
  __ Ld(type, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ Branch(target, AsMasmCondition(cc), type, Operand(instance_type));
}
void BaselineAssembler::JumpIfPointer(Condition cc, Register value,
                                      MemOperand operand, Label* target,
                                      Label::Distance) {
  ScratchRegisterScope temps(this);
  Register temp = temps.AcquireScratch();
  __ Ld(temp, operand);
  __ Branch(target, AsMasmCondition(cc), value, Operand(temp));
}
void BaselineAssembler::JumpIfSmi(Condition cc, Register value, Smi smi,
                                  Label* target, Label::Distance) {
  ScratchRegisterScope temps(this);
  Register temp = temps.AcquireScratch();
  __ li(temp, Operand(smi));
  __ SmiUntag(temp);
  __ Branch(target, AsMasmCondition(cc), value, Operand(temp));
}
void BaselineAssembler::JumpIfSmi(Condition cc, Register lhs, Register rhs,
                                  Label* target, Label::Distance) {
  // todo: compress pointer
  __ AssertSmi(lhs);
  __ AssertSmi(rhs);
  __ Branch(target, AsMasmCondition(cc), lhs, Operand(rhs));
}
void BaselineAssembler::JumpIfTagged(Condition cc, Register value,
                                     MemOperand operand, Label* target,
                                     Label::Distance) {
  // todo: compress pointer
  ScratchRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  __ Ld(scratch, operand);
  __ Branch(target, AsMasmCondition(cc), value, Operand(scratch));
}
void BaselineAssembler::JumpIfTagged(Condition cc, MemOperand operand,
                                     Register value, Label* target,
                                     Label::Distance) {
  // todo: compress pointer
  ScratchRegisterScope temps(this);
  Register scratch = temps.AcquireScratch();
  __ Ld(scratch, operand);
  __ Branch(target, AsMasmCondition(cc), scratch, Operand(value));
}
void BaselineAssembler::JumpIfByte(Condition cc, Register value, int32_t byte,
                                   Label* target, Label::Distance) {
  __ Branch(target, AsMasmCondition(cc), value, Operand(byte));
}

void BaselineAssembler::Move(interpreter::Register output, Register source) {
  Move(RegisterFrameOperand(output), source);
}
void BaselineAssembler::Move(Register output, TaggedIndex value) {
  __ li(output, Operand(value.ptr()));
}
void BaselineAssembler::Move(MemOperand output, Register source) {
  __ Sd(source, output);
}
void BaselineAssembler::Move(Register output, ExternalReference reference) {
  __ li(output, Operand(reference));
}
void BaselineAssembler::Move(Register output, Handle<HeapObject> value) {
  __ li(output, Operand(value));
}
void BaselineAssembler::Move(Register output, int32_t value) {
  __ li(output, Operand(value));
}
void BaselineAssembler::MoveMaybeSmi(Register output, Register source) {
  __ Move(output, source);
}
void BaselineAssembler::MoveSmi(Register output, Register source) {
  __ Move(output, source);
}

namespace detail {

template <typename Arg>
inline Register ToRegister(BaselineAssembler* basm,
                           BaselineAssembler::ScratchRegisterScope* scope,
                           Arg arg) {
  Register reg = scope->AcquireScratch();
  basm->Move(reg, arg);
  return reg;
}
inline Register ToRegister(BaselineAssembler* basm,
                           BaselineAssembler::ScratchRegisterScope* scope,
                           Register reg) {
  return reg;
}

template <typename... Args>
struct PushAllHelper;
template <>
struct PushAllHelper<> {
  static int Push(BaselineAssembler* basm) { return 0; }
  static int PushReverse(BaselineAssembler* basm) { return 0; }
};
template <typename Arg>
struct PushAllHelper<Arg> {
  static int Push(BaselineAssembler* basm, Arg arg) {
    BaselineAssembler::ScratchRegisterScope scope(basm);
    basm->masm()->Push(ToRegister(basm, &scope, arg));
    return 1;
  }
  static int PushReverse(BaselineAssembler* basm, Arg arg) {
    return Push(basm, arg);
  }
};
template <typename Arg, typename... Args>
struct PushAllHelper<Arg, Args...> {
  static int Push(BaselineAssembler* basm, Arg arg, Args... args) {
    PushAllHelper<Arg>::Push(basm, arg);
    return 1 + PushAllHelper<Args...>::Push(basm, args...);
  }
  static int PushReverse(BaselineAssembler* basm, Arg arg, Args... args) {
    int nargs = PushAllHelper<Args...>::PushReverse(basm, args...);
    PushAllHelper<Arg>::Push(basm, arg);
    return nargs + 1;
  }
};
template <>
struct PushAllHelper<interpreter::RegisterList> {
  static int Push(BaselineAssembler* basm, interpreter::RegisterList list) {
    for (int reg_index = 0; reg_index < list.register_count(); ++reg_index) {
      PushAllHelper<interpreter::Register>::Push(basm, list[reg_index]);
    }
    return list.register_count();
  }
  static int PushReverse(BaselineAssembler* basm,
                         interpreter::RegisterList list) {
    for (int reg_index = list.register_count() - 1; reg_index >= 0;
         --reg_index) {
      PushAllHelper<interpreter::Register>::Push(basm, list[reg_index]);
    }
    return list.register_count();
  }
};

template <typename... T>
struct PopAllHelper;
template <>
struct PopAllHelper<> {
  static void Pop(BaselineAssembler* basm) {}
};
template <>
struct PopAllHelper<Register> {
  static void Pop(BaselineAssembler* basm, Register reg) {
    basm->masm()->Pop(reg);
  }
};
template <typename... T>
struct PopAllHelper<Register, T...> {
  static void Pop(BaselineAssembler* basm, Register reg, T... tail) {
    PopAllHelper<Register>::Pop(basm, reg);
    PopAllHelper<T...>::Pop(basm, tail...);
  }
};

}  // namespace detail

template <typename... T>
int BaselineAssembler::Push(T... vals) {
  return detail::PushAllHelper<T...>::Push(this, vals...);
}

template <typename... T>
void BaselineAssembler::PushReverse(T... vals) {
  detail::PushAllHelper<T...>::PushReverse(this, vals...);
}

template <typename... T>
void BaselineAssembler::Pop(T... registers) {
  detail::PopAllHelper<T...>::Pop(this, registers...);
}

void BaselineAssembler::LoadTaggedPointerField(Register output, Register source,
                                               int offset) {
  __ LoadTaggedPointerField(output, FieldMemOperand(source, offset));
}
void BaselineAssembler::LoadTaggedSignedField(Register output, Register source,
                                              int offset) {
  __ LoadTaggedSignedField(output, FieldMemOperand(source, offset));
}
void BaselineAssembler::LoadTaggedAnyField(Register output, Register source,
                                           int offset) {
  __ LoadAnyTaggedField(output, FieldMemOperand(source, offset));
}
void BaselineAssembler::LoadWord16FieldZeroExtend(Register output,
                                                  Register source, int offset) {
  __ Lhu(output, FieldMemOperand(source, offset));
}
void BaselineAssembler::LoadWord8Field(Register output, Register source,
                                       int offset) {
  __ Lb(output, FieldMemOperand(source, offset));
}
void BaselineAssembler::StoreTaggedSignedField(Register target, int offset,
                                               Smi value) {
  ASM_CODE_COMMENT(masm_);
  ScratchRegisterScope temps(this);
  Register tmp = temps.AcquireScratch();
  __ li(tmp, Operand(value));
  __ StoreTaggedField(tmp, FieldMemOperand(target, offset));
}
void BaselineAssembler::StoreTaggedFieldWithWriteBarrier(Register target,
                                                         int offset,
                                                         Register value) {
  ASM_CODE_COMMENT(masm_);
  __ StoreTaggedField(value, FieldMemOperand(target, offset));
  __ RecordWriteField(target, offset, value, kRAHasNotBeenSaved,
                      SaveFPRegsMode::kIgnore);
}
void BaselineAssembler::StoreTaggedFieldNoWriteBarrier(Register target,
                                                       int offset,
                                                       Register value) {
  __ StoreTaggedField(value, FieldMemOperand(target, offset));
}

void BaselineAssembler::AddToInterruptBudgetAndJumpIfNotExceeded(
    int32_t weight, Label* skip_interrupt_label) {
  ASM_CODE_COMMENT(masm_);
  ScratchRegisterScope scratch_scope(this);
  Register feedback_cell = scratch_scope.AcquireScratch();
  LoadFunction(feedback_cell);
  LoadTaggedPointerField(feedback_cell, feedback_cell,
                         JSFunction::kFeedbackCellOffset);

  Register interrupt_budget = scratch_scope.AcquireScratch();
  __ Lw(interrupt_budget,
        FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  // Remember to set flags as part of the add!
  __ Add32(interrupt_budget, interrupt_budget, weight);
  __ Sw(interrupt_budget,
        FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  if (skip_interrupt_label) {
    DCHECK_LT(weight, 0);
    __ Branch(skip_interrupt_label, ge, interrupt_budget, Operand(weight));
  }
}

void BaselineAssembler::AddToInterruptBudgetAndJumpIfNotExceeded(
    Register weight, Label* skip_interrupt_label) {
  ASM_CODE_COMMENT(masm_);
  ScratchRegisterScope scratch_scope(this);
  Register feedback_cell = scratch_scope.AcquireScratch();
  LoadFunction(feedback_cell);
  LoadTaggedPointerField(feedback_cell, feedback_cell,
                         JSFunction::kFeedbackCellOffset);

  Register interrupt_budget = scratch_scope.AcquireScratch();
  __ Lw(interrupt_budget,
        FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  // Remember to set flags as part of the add!
  __ Add32(interrupt_budget, interrupt_budget, weight);
  __ Sw(interrupt_budget,
        FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  if (skip_interrupt_label)
    __ Branch(skip_interrupt_label, ge, interrupt_budget, Operand(weight));
}

void BaselineAssembler::AddSmi(Register lhs, Smi rhs) {
  ASM_CODE_COMMENT(masm_);
  if (SmiValuesAre31Bits()) {
    __ Add32(lhs, lhs, Operand(rhs));
  } else {
    __ Add64(lhs, lhs, Operand(rhs));
  }
}

void BaselineAssembler::Word32And(Register output, Register lhs, int rhs) {
  __ And(output, lhs, Operand(rhs));
}

void BaselineAssembler::Switch(Register reg, int case_value_base,
                               Label** labels, int num_labels) {
  ASM_CODE_COMMENT(masm_);
  Label fallthrough;
  if (case_value_base != 0) {
    __ Sub64(reg, reg, Operand(case_value_base));
  }

  // Mostly copied from code-generator-riscv64.cc
  ScratchRegisterScope scope(this);
  Label table;
  __ Branch(&fallthrough, AsMasmCondition(Condition::kUnsignedGreaterThanEqual),
            reg, Operand(int64_t(num_labels)));
  int64_t imm64;
  imm64 = __ branch_long_offset(&table);
  CHECK(is_int32(imm64 + 0x800));
  int32_t Hi20 = (((int32_t)imm64 + 0x800) >> 12);
  int32_t Lo12 = (int32_t)imm64 << 20 >> 20;
  __ BlockTrampolinePoolFor(2);
  __ auipc(t6, Hi20);  // Read PC + Hi20 into t6
  __ addi(t6, t6, Lo12);  // jump PC + Hi20 + Lo12

  int entry_size_log2 = 3;
  __ CalcScaledAddress(t6, t6, reg, entry_size_log2);
  __ Jump(t6);
  {
    TurboAssembler::BlockTrampolinePoolScope(masm());
    __ BlockTrampolinePoolFor(num_labels * kInstrSize * 2);
    __ bind(&table);
    for (int i = 0; i < num_labels; ++i) {
      __ BranchLong(labels[i]);
    }
    DCHECK_EQ(num_labels * 2, __ InstructionsGeneratedSince(&table));
    __ bind(&fallthrough);
  }
}

#undef __

#define __ basm.

void BaselineAssembler::EmitReturn(MacroAssembler* masm) {
  ASM_CODE_COMMENT(masm);
  BaselineAssembler basm(masm);

  Register weight = BaselineLeaveFrameDescriptor::WeightRegister();
  Register params_size = BaselineLeaveFrameDescriptor::ParamsSizeRegister();

  {
    ASM_CODE_COMMENT_STRING(masm, "Update Interrupt Budget");

    Label skip_interrupt_label;
    __ AddToInterruptBudgetAndJumpIfNotExceeded(weight, &skip_interrupt_label);
    __ masm()->SmiTag(params_size);
    __ masm()->Push(params_size, kInterpreterAccumulatorRegister);

    __ LoadContext(kContextRegister);
    __ LoadFunction(kJSFunctionRegister);
    __ masm()->Push(kJSFunctionRegister);
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt, 1);

    __ masm()->Pop(params_size, kInterpreterAccumulatorRegister);
    __ masm()->SmiUntag(params_size);

  __ Bind(&skip_interrupt_label);
  }

  BaselineAssembler::ScratchRegisterScope temps(&basm);
  Register actual_params_size = temps.AcquireScratch();
  // Compute the size of the actual parameters + receiver (in bytes).
  __ Move(actual_params_size,
          MemOperand(fp, StandardFrameConstants::kArgCOffset));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ masm()->Branch(&corrected_args_count, ge, params_size,
                    Operand(actual_params_size), Label::Distance::kNear);
  __ masm()->Move(params_size, actual_params_size);
  __ Bind(&corrected_args_count);

  // Leave the frame (also dropping the register file).
  __ masm()->LeaveFrame(StackFrame::BASELINE);

  // Drop receiver + arguments.
  __ masm()->DropArguments(params_size, MacroAssembler::kCountIsInteger,
                           MacroAssembler::kCountIncludesReceiver);
  __ masm()->Ret();
}

#undef __

inline void EnsureAccumulatorPreservedScope::AssertEqualToAccumulator(
    Register reg) {
  assembler_->masm()->Assert(eq, AbortReason::kUnexpectedValue, reg,
                             Operand(kInterpreterAccumulatorRegister));
}
}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_RISCV64_BASELINE_ASSEMBLER_RISCV64_INL_H_
