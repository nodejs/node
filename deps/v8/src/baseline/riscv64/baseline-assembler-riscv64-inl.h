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

void BaselineAssembler::CallBuiltin(Builtin builtin) {
  if (masm()->options().short_builtin_calls) {
    __ CallBuiltin(builtin);
  } else {
    ASM_CODE_COMMENT_STRING(masm_,
                            __ CommentForOffHeapTrampoline("call", builtin));
    Register temp = t6;
    __ LoadEntryFromBuiltin(builtin, temp);
    __ Call(temp);
  }
}

void BaselineAssembler::TailCallBuiltin(Builtin builtin) {
  if (masm()->options().short_builtin_calls) {
    // Generate pc-relative jump.
    __ TailCallBuiltin(builtin);
  } else {
    ASM_CODE_COMMENT_STRING(
        masm_, __ CommentForOffHeapTrampoline("tail call", builtin));
    // t6 be used for function call in RISCV64
    // For example 'jalr t6' or 'jal t6'
    Register temp = t6;
    __ LoadEntryFromBuiltin(builtin, temp);
    __ Jump(temp);
  }
}

void BaselineAssembler::TestAndBranch(Register value, int mask, Condition cc,
                                      Label* target, Label::Distance) {
  ScratchRegisterScope temps(this);
  Register tmp = temps.AcquireScratch();
  __ And(tmp, value, Operand(mask));
  __ Branch(target, AsMasmCondition(cc), tmp, Operand(mask));
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
  ScratchRegisterScope temps(this);
  Register temp = temps.AcquireScratch();
  __ AssertSmi(lhs);
  __ AssertSmi(rhs);
  if (COMPRESS_POINTERS_BOOL) {
    __ Sub32(temp, lhs, rhs);
  } else {
    __ Sub64(temp, lhs, rhs);
  }
  __ Branch(target, AsMasmCondition(cc), temp, Operand(zero_reg));
}
void BaselineAssembler::JumpIfTagged(Condition cc, Register value,
                                     MemOperand operand, Label* target,
                                     Label::Distance) {
  ScratchRegisterScope temps(this);
  Register tmp1 = temps.AcquireScratch();
  Register tmp2 = temps.AcquireScratch();
  __ Ld(tmp1, operand);
  if (COMPRESS_POINTERS_BOOL) {
    __ Sub32(tmp2, value, tmp1);
  } else {
    __ Sub64(tmp2, value, tmp1);
  }
  __ Branch(target, AsMasmCondition(cc), tmp2, Operand(zero_reg));
}
void BaselineAssembler::JumpIfTagged(Condition cc, MemOperand operand,
                                     Register value, Label* target,
                                     Label::Distance) {
  ScratchRegisterScope temps(this);
  Register tmp1 = temps.AcquireScratch();
  Register tmp2 = temps.AcquireScratch();
  __ Ld(tmp1, operand);
  if (COMPRESS_POINTERS_BOOL) {
    __ Sub32(tmp2, tmp1, value);
  } else {
    __ Sub64(tmp2, tmp1, value);
  }
  __ Branch(target, AsMasmCondition(cc), tmp2, Operand(zero_reg));
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
struct CountPushHelper;
template <>
struct CountPushHelper<> {
  static int Count() { return 0; }
};
template <typename Arg, typename... Args>
struct CountPushHelper<Arg, Args...> {
  static int Count(Arg arg, Args... args) {
    return 1 + CountPushHelper<Args...>::Count(args...);
  }
};
template <typename... Args>
struct CountPushHelper<interpreter::RegisterList, Args...> {
  static int Count(interpreter::RegisterList list, Args... args) {
    return list.register_count() + CountPushHelper<Args...>::Count(args...);
  }
};

template <typename... Args>
struct PushAllHelper;
template <typename... Args>
void PushAll(BaselineAssembler* basm, Args... args) {
  PushAllHelper<Args...>::Push(basm, args...);
}
template <typename... Args>
void PushAllReverse(BaselineAssembler* basm, Args... args) {
  PushAllHelper<Args...>::PushReverse(basm, args...);
}

template <>
struct PushAllHelper<> {
  static void Push(BaselineAssembler* basm) {}
  static void PushReverse(BaselineAssembler* basm) {}
};

inline void PushSingle(MacroAssembler* masm, RootIndex source) {
  masm->PushRoot(source);
}
inline void PushSingle(MacroAssembler* masm, Register reg) { masm->Push(reg); }

inline void PushSingle(MacroAssembler* masm, Smi value) { masm->Push(value); }
inline void PushSingle(MacroAssembler* masm, Handle<HeapObject> object) {
  masm->Push(object);
}
inline void PushSingle(MacroAssembler* masm, int32_t immediate) {
  masm->li(kScratchReg, (int64_t)(immediate));
  PushSingle(masm, kScratchReg);
}

inline void PushSingle(MacroAssembler* masm, TaggedIndex value) {
  masm->li(kScratchReg, static_cast<int64_t>(value.ptr()));
  PushSingle(masm, kScratchReg);
}
inline void PushSingle(MacroAssembler* masm, MemOperand operand) {
  masm->Ld(kScratchReg, operand);
  PushSingle(masm, kScratchReg);
}
inline void PushSingle(MacroAssembler* masm, interpreter::Register source) {
  return PushSingle(masm, BaselineAssembler::RegisterFrameOperand(source));
}

template <typename Arg>
struct PushAllHelper<Arg> {
  static void Push(BaselineAssembler* basm, Arg arg) {
    PushSingle(basm->masm(), arg);
  }
  static void PushReverse(BaselineAssembler* basm, Arg arg) {
    // Push the padding register to round up the amount of values pushed.
    return Push(basm, arg);
  }
};
template <typename Arg1, typename Arg2, typename... Args>
struct PushAllHelper<Arg1, Arg2, Args...> {
  static void Push(BaselineAssembler* basm, Arg1 arg1, Arg2 arg2,
                   Args... args) {
    {
      BaselineAssembler::ScratchRegisterScope scope(basm);
      basm->masm()->Push(ToRegister(basm, &scope, arg1),
                         ToRegister(basm, &scope, arg2));
    }
    PushAll(basm, args...);
  }
  static void PushReverse(BaselineAssembler* basm, Arg1 arg1, Arg2 arg2,
                          Args... args) {
    PushAllReverse(basm, args...);
    {
      BaselineAssembler::ScratchRegisterScope scope(basm);
      basm->masm()->Push(ToRegister(basm, &scope, arg2),
                         ToRegister(basm, &scope, arg1));
    }
  }
};
// Currently RegisterLists are always be the last argument, so we don't
// specialize for the case where they're not. We do still specialise for the
// aligned and unaligned cases.
template <typename Arg>
struct PushAllHelper<Arg, interpreter::RegisterList> {
  static void Push(BaselineAssembler* basm, Arg arg,
                   interpreter::RegisterList list) {
    DCHECK_EQ(list.register_count() % 2, 1);
    PushAll(basm, arg, list[0], list.PopLeft());
  }
  static void PushReverse(BaselineAssembler* basm, Arg arg,
                          interpreter::RegisterList list) {
    if (list.register_count() == 0) {
      PushAllReverse(basm, arg);
    } else {
      PushAllReverse(basm, arg, list[0], list.PopLeft());
    }
  }
};
template <>
struct PushAllHelper<interpreter::RegisterList> {
  static void Push(BaselineAssembler* basm, interpreter::RegisterList list) {
    DCHECK_EQ(list.register_count() % 2, 0);
    for (int reg_index = 0; reg_index < list.register_count(); reg_index += 2) {
      PushAll(basm, list[reg_index], list[reg_index + 1]);
    }
  }
  static void PushReverse(BaselineAssembler* basm,
                          interpreter::RegisterList list) {
    int reg_index = list.register_count() - 1;
    if (reg_index % 2 == 0) {
      // Push the padding register to round up the amount of values pushed.
      PushAllReverse(basm, list[reg_index]);
      reg_index--;
    }
    for (; reg_index >= 1; reg_index -= 2) {
      PushAllReverse(basm, list[reg_index - 1], list[reg_index]);
    }
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
struct PopAllHelper<Register, Register, T...> {
  static void Pop(BaselineAssembler* basm, Register reg1, Register reg2,
                  T... tail) {
    basm->masm()->Pop(reg1, reg2);
    PopAllHelper<T...>::Pop(basm, tail...);
  }
};

}  // namespace detail

template <typename... T>
int BaselineAssembler::Push(T... vals) {
  // We have to count the pushes first, to decide whether to add padding before
  // the first push.
  int push_count = detail::CountPushHelper<T...>::Count(vals...);
  if (push_count % 2 == 0) {
    detail::PushAll(this, vals...);
  } else {
    detail::PushAll(this, vals...);
  }
  return push_count;
}

template <typename... T>
void BaselineAssembler::PushReverse(T... vals) {
  detail::PushAllReverse(this, vals...);
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
void BaselineAssembler::LoadByteField(Register output, Register source,
                                      int offset) {
  __ Ld(output, FieldMemOperand(source, offset));
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
  __ Ld(interrupt_budget,
        FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  // Remember to set flags as part of the add!
  __ Add64(interrupt_budget, interrupt_budget, weight);
  __ Sd(interrupt_budget,
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
  __ Ld(interrupt_budget,
        FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  // Remember to set flags as part of the add!
  __ Add64(interrupt_budget, interrupt_budget, weight);
  __ Sd(interrupt_budget,
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

void BaselineAssembler::Switch(Register reg, int case_value_base,
                               Label** labels, int num_labels) {
  ASM_CODE_COMMENT(masm_);
  Label fallthrough;
  if (case_value_base != 0) {
    __ Sub64(reg, reg, Operand(case_value_base));
  }

  // Mostly copied from code-generator-riscv64.cc
  ScratchRegisterScope scope(this);
  Register temp = scope.AcquireScratch();
  Label table;
  __ Branch(&fallthrough, AsMasmCondition(Condition::kUnsignedGreaterThanEqual),
            reg, Operand(int64_t(num_labels)));
  int64_t imm64;
  imm64 = __ branch_long_offset(&table);
  DCHECK(is_int32(imm64));
  int32_t Hi20 = (((int32_t)imm64 + 0x800) >> 12);
  int32_t Lo12 = (int32_t)imm64 << 20 >> 20;
  __ auipc(temp, Hi20);  // Read PC + Hi20 into t6
  __ lui(temp, Lo12);    // jump PC + Hi20 + Lo12

  int entry_size_log2 = 2;
  Register temp2 = scope.AcquireScratch();
  __ CalcScaledAddress(temp2, temp, reg, entry_size_log2);
  __ Jump(temp);
  {
    TurboAssembler::BlockTrampolinePoolScope(masm());
    __ BlockTrampolinePoolFor(num_labels * kInstrSize);
    __ bind(&table);
    for (int i = 0; i < num_labels; ++i) {
      __ Branch(labels[i]);
    }
    DCHECK_EQ(num_labels * kInstrSize, __ InstructionsGeneratedSince(&table));
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
    __ CallRuntime(Runtime::kBytecodeBudgetInterruptFromBytecode, 1);

    __ masm()->Pop(kInterpreterAccumulatorRegister, params_size);
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
  __ masm()->Add64(params_size, params_size, 1);  // Include the receiver.
  __ masm()->slli(params_size, params_size, kSystemPointerSizeLog2);
  __ masm()->Add64(sp, sp, params_size);
  __ masm()->Ret();
}

#undef __

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_RISCV64_BASELINE_ASSEMBLER_RISCV64_INL_H_
