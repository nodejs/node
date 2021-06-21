// Use of this source code is governed by a BSD-style license that can be
// Copyright 2021 the V8 project authors. All rights reserved.
// found in the LICENSE file.

#ifndef V8_BASELINE_IA32_BASELINE_ASSEMBLER_IA32_INL_H_
#define V8_BASELINE_IA32_BASELINE_ASSEMBLER_IA32_INL_H_

#include "src/baseline/baseline-assembler.h"
#include "src/codegen/ia32/register-ia32.h"
#include "src/codegen/interface-descriptors.h"

namespace v8 {
namespace internal {
namespace baseline {

namespace detail {

static constexpr Register kScratchRegisters[] = {ecx, edx, esi, edi};
static constexpr int kNumScratchRegisters = arraysize(kScratchRegisters);

}  // namespace detail

class BaselineAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(BaselineAssembler* assembler)
      : assembler_(assembler),
        prev_scope_(assembler->scratch_register_scope_),
        registers_used_(prev_scope_ == nullptr ? 0
                                               : prev_scope_->registers_used_) {
    assembler_->scratch_register_scope_ = this;
  }
  ~ScratchRegisterScope() { assembler_->scratch_register_scope_ = prev_scope_; }

  Register AcquireScratch() {
    DCHECK_LT(registers_used_, detail::kNumScratchRegisters);
    return detail::kScratchRegisters[registers_used_++];
  }

 private:
  BaselineAssembler* assembler_;
  ScratchRegisterScope* prev_scope_;
  int registers_used_;
};

// TODO(v8:11461): Unify condition names in the MacroAssembler.
enum class Condition : uint32_t {
  kEqual = equal,
  kNotEqual = not_equal,

  kLessThan = less,
  kGreaterThan = greater,
  kLessThanEqual = less_equal,
  kGreaterThanEqual = greater_equal,

  kUnsignedLessThan = below,
  kUnsignedGreaterThan = above,
  kUnsignedLessThanEqual = below_equal,
  kUnsignedGreaterThanEqual = above_equal,

  kOverflow = overflow,
  kNoOverflow = no_overflow,

  kZero = zero,
  kNotZero = not_zero,
};

inline internal::Condition AsMasmCondition(Condition cond) {
  return static_cast<internal::Condition>(cond);
}

namespace detail {

#define __ masm_->

#ifdef DEBUG
inline bool Clobbers(Register target, MemOperand op) {
  return op.is_reg(target);
}
#endif

}  // namespace detail

MemOperand BaselineAssembler::RegisterFrameOperand(
    interpreter::Register interpreter_register) {
  return MemOperand(ebp, interpreter_register.ToOperand() * kSystemPointerSize);
}
MemOperand BaselineAssembler::FeedbackVectorOperand() {
  return MemOperand(ebp, BaselineFrameConstants::kFeedbackVectorFromFp);
}

void BaselineAssembler::Bind(Label* label) { __ bind(label); }
void BaselineAssembler::BindWithoutJumpTarget(Label* label) { __ bind(label); }

void BaselineAssembler::JumpTarget() {
  // NOP on ia32.
}

void BaselineAssembler::Jump(Label* target, Label::Distance distance) {
  __ jmp(target, distance);
}
void BaselineAssembler::JumpIf(Condition cc, Label* target,
                               Label::Distance distance) {
  __ j(AsMasmCondition(cc), target, distance);
}
void BaselineAssembler::JumpIfRoot(Register value, RootIndex index,
                                   Label* target, Label::Distance distance) {
  __ JumpIfRoot(value, index, target, distance);
}
void BaselineAssembler::JumpIfNotRoot(Register value, RootIndex index,
                                      Label* target, Label::Distance distance) {
  __ JumpIfNotRoot(value, index, target, distance);
}
void BaselineAssembler::JumpIfSmi(Register value, Label* target,
                                  Label::Distance distance) {
  __ JumpIfSmi(value, target, distance);
}
void BaselineAssembler::JumpIfNotSmi(Register value, Label* target,
                                     Label::Distance distance) {
  __ JumpIfNotSmi(value, target, distance);
}

void BaselineAssembler::CallBuiltin(Builtins::Name builtin) {
  __ RecordCommentForOffHeapTrampoline(builtin);
  __ Call(__ EntryFromBuiltinIndexAsOperand(builtin));
  if (FLAG_code_comments) __ RecordComment("]");
}

void BaselineAssembler::TailCallBuiltin(Builtins::Name builtin) {
  __ RecordCommentForOffHeapTrampoline(builtin);
  __ jmp(__ EntryFromBuiltinIndexAsOperand(builtin));
  if (FLAG_code_comments) __ RecordComment("]");
}

void BaselineAssembler::Test(Register value, int mask) {
  if ((mask & 0xff) == mask) {
    __ test_b(value, Immediate(mask));
  } else {
    __ test(value, Immediate(mask));
  }
}

void BaselineAssembler::CmpObjectType(Register object,
                                      InstanceType instance_type,
                                      Register map) {
  __ AssertNotSmi(object);
  __ CmpObjectType(object, instance_type, map);
}
void BaselineAssembler::CmpInstanceType(Register map,
                                        InstanceType instance_type) {
  if (emit_debug_code()) {
    __ movd(xmm0, eax);
    __ AssertNotSmi(map);
    __ CmpObjectType(map, MAP_TYPE, eax);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    __ movd(eax, xmm0);
  }
  __ CmpInstanceType(map, instance_type);
}
void BaselineAssembler::Cmp(Register value, Smi smi) {
  if (smi.value() == 0) {
    __ test(value, value);
  } else {
    __ cmp(value, Immediate(smi));
  }
}
void BaselineAssembler::ComparePointer(Register value, MemOperand operand) {
  __ cmp(value, operand);
}
void BaselineAssembler::SmiCompare(Register lhs, Register rhs) {
  __ AssertSmi(lhs);
  __ AssertSmi(rhs);
  __ cmp(lhs, rhs);
}
void BaselineAssembler::CompareTagged(Register value, MemOperand operand) {
  __ cmp(value, operand);
}
void BaselineAssembler::CompareTagged(MemOperand operand, Register value) {
  __ cmp(operand, value);
}
void BaselineAssembler::CompareByte(Register value, int32_t byte) {
  __ cmpb(value, Immediate(byte));
}
void BaselineAssembler::Move(interpreter::Register output, Register source) {
  return __ mov(RegisterFrameOperand(output), source);
}
void BaselineAssembler::Move(Register output, TaggedIndex value) {
  __ Move(output, Immediate(value.ptr()));
}
void BaselineAssembler::Move(MemOperand output, Register source) {
  __ mov(output, source);
}
void BaselineAssembler::Move(Register output, ExternalReference reference) {
  __ Move(output, Immediate(reference));
}
void BaselineAssembler::Move(Register output, Handle<HeapObject> value) {
  __ Move(output, value);
}
void BaselineAssembler::Move(Register output, int32_t value) {
  __ Move(output, Immediate(value));
}
void BaselineAssembler::MoveMaybeSmi(Register output, Register source) {
  __ mov(output, source);
}
void BaselineAssembler::MoveSmi(Register output, Register source) {
  __ mov(output, source);
}

namespace detail {
inline void PushSingle(MacroAssembler* masm, RootIndex source) {
  masm->PushRoot(source);
}
inline void PushSingle(MacroAssembler* masm, Register reg) { masm->Push(reg); }
inline void PushSingle(MacroAssembler* masm, TaggedIndex value) {
  masm->Push(Immediate(value.ptr()));
}
inline void PushSingle(MacroAssembler* masm, Smi value) { masm->Push(value); }
inline void PushSingle(MacroAssembler* masm, Handle<HeapObject> object) {
  masm->Push(object);
}
inline void PushSingle(MacroAssembler* masm, int32_t immediate) {
  masm->Push(Immediate(immediate));
}
inline void PushSingle(MacroAssembler* masm, MemOperand operand) {
  masm->Push(operand);
}
inline void PushSingle(MacroAssembler* masm, interpreter::Register source) {
  return PushSingle(masm, BaselineAssembler::RegisterFrameOperand(source));
}

template <typename Arg>
struct PushHelper {
  static int Push(BaselineAssembler* basm, Arg arg) {
    PushSingle(basm->masm(), arg);
    return 1;
  }
  static int PushReverse(BaselineAssembler* basm, Arg arg) {
    return Push(basm, arg);
  }
};

template <>
struct PushHelper<interpreter::RegisterList> {
  static int Push(BaselineAssembler* basm, interpreter::RegisterList list) {
    for (int reg_index = 0; reg_index < list.register_count(); ++reg_index) {
      PushSingle(basm->masm(), list[reg_index]);
    }
    return list.register_count();
  }
  static int PushReverse(BaselineAssembler* basm,
                         interpreter::RegisterList list) {
    for (int reg_index = list.register_count() - 1; reg_index >= 0;
         --reg_index) {
      PushSingle(basm->masm(), list[reg_index]);
    }
    return list.register_count();
  }
};

template <typename... Args>
struct PushAllHelper;
template <>
struct PushAllHelper<> {
  static int Push(BaselineAssembler* masm) { return 0; }
  static int PushReverse(BaselineAssembler* masm) { return 0; }
};
template <typename Arg, typename... Args>
struct PushAllHelper<Arg, Args...> {
  static int Push(BaselineAssembler* masm, Arg arg, Args... args) {
    int nargs = PushHelper<Arg>::Push(masm, arg);
    return nargs + PushAllHelper<Args...>::Push(masm, args...);
  }
  static int PushReverse(BaselineAssembler* masm, Arg arg, Args... args) {
    int nargs = PushAllHelper<Args...>::PushReverse(masm, args...);
    return nargs + PushHelper<Arg>::PushReverse(masm, arg);
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
  ITERATE_PACK(__ Pop(registers));
}

void BaselineAssembler::LoadTaggedPointerField(Register output, Register source,
                                               int offset) {
  __ mov(output, FieldOperand(source, offset));
}
void BaselineAssembler::LoadTaggedSignedField(Register output, Register source,
                                              int offset) {
  __ mov(output, FieldOperand(source, offset));
}
void BaselineAssembler::LoadTaggedAnyField(Register output, Register source,
                                           int offset) {
  __ mov(output, FieldOperand(source, offset));
}
void BaselineAssembler::LoadByteField(Register output, Register source,
                                      int offset) {
  __ mov_b(output, FieldOperand(source, offset));
}
void BaselineAssembler::StoreTaggedSignedField(Register target, int offset,
                                               Smi value) {
  __ mov(FieldOperand(target, offset), Immediate(value));
}
void BaselineAssembler::StoreTaggedFieldWithWriteBarrier(Register target,
                                                         int offset,

                                                         Register value) {
  BaselineAssembler::ScratchRegisterScope scratch_scope(this);
  Register scratch = scratch_scope.AcquireScratch();
  DCHECK(!AreAliased(scratch, target, value));
  __ mov(FieldOperand(target, offset), value);
  __ RecordWriteField(target, offset, value, scratch, kDontSaveFPRegs);
}
void BaselineAssembler::StoreTaggedFieldNoWriteBarrier(Register target,
                                                       int offset,
                                                       Register value) {
  DCHECK(!AreAliased(target, value));
  __ mov(FieldOperand(target, offset), value);
}

void BaselineAssembler::AddToInterruptBudget(int32_t weight) {
  ScratchRegisterScope scratch_scope(this);
  Register feedback_cell = scratch_scope.AcquireScratch();
  LoadFunction(feedback_cell);
  LoadTaggedPointerField(feedback_cell, feedback_cell,
                         JSFunction::kFeedbackCellOffset);
  __ add(FieldOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset),
         Immediate(weight));
}

void BaselineAssembler::AddToInterruptBudget(Register weight) {
  ScratchRegisterScope scratch_scope(this);
  Register feedback_cell = scratch_scope.AcquireScratch();
  DCHECK(!AreAliased(feedback_cell, weight));
  LoadFunction(feedback_cell);
  LoadTaggedPointerField(feedback_cell, feedback_cell,
                         JSFunction::kFeedbackCellOffset);
  __ add(FieldOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset),
         weight);
}

void BaselineAssembler::AddSmi(Register lhs, Smi rhs) {
  if (rhs.value() == 0) return;
  __ add(lhs, Immediate(rhs));
}

void BaselineAssembler::Switch(Register reg, int case_value_base,
                               Label** labels, int num_labels) {
  ScratchRegisterScope scope(this);
  Register table = scope.AcquireScratch();
  DCHECK(!AreAliased(reg, table));
  Label fallthrough, jump_table;
  if (case_value_base > 0) {
    __ sub(reg, Immediate(case_value_base));
  }
  __ cmp(reg, Immediate(num_labels));
  __ j(above_equal, &fallthrough);
  __ lea(table, MemOperand(&jump_table));
  __ jmp(Operand(table, reg, times_system_pointer_size, 0));
  // Emit the jump table inline, under the assumption that it's not too big.
  __ Align(kSystemPointerSize);
  __ bind(&jump_table);
  for (int i = 0; i < num_labels; ++i) {
    __ dd(labels[i]);
  }
  __ bind(&fallthrough);
}

#undef __
#define __ basm.

void BaselineAssembler::EmitReturn(MacroAssembler* masm) {
  BaselineAssembler basm(masm);

  Register weight = BaselineLeaveFrameDescriptor::WeightRegister();
  Register params_size = BaselineLeaveFrameDescriptor::ParamsSizeRegister();

  __ RecordComment("[ Update Interrupt Budget");
  __ AddToInterruptBudget(weight);

  // Use compare flags set by AddToInterruptBudget
  Label skip_interrupt_label;
  __ JumpIf(Condition::kGreaterThanEqual, &skip_interrupt_label);
  {
    __ masm()->SmiTag(params_size);
    __ Push(params_size, kInterpreterAccumulatorRegister);

    __ LoadContext(kContextRegister);
    __ Push(MemOperand(ebp, InterpreterFrameConstants::kFunctionOffset));
    __ CallRuntime(Runtime::kBytecodeBudgetInterruptFromBytecode, 1);

    __ Pop(kInterpreterAccumulatorRegister, params_size);
    __ masm()->SmiUntag(params_size);
  }
  __ RecordComment("]");

  __ Bind(&skip_interrupt_label);

  BaselineAssembler::ScratchRegisterScope scope(&basm);
  Register scratch = scope.AcquireScratch();
  DCHECK(!AreAliased(weight, params_size, scratch));

  Register actual_params_size = scratch;
  // Compute the size of the actual parameters + receiver (in bytes).
  __ masm()->mov(actual_params_size,
                 MemOperand(ebp, StandardFrameConstants::kArgCOffset));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ masm()->cmp(params_size, actual_params_size);
  __ JumpIf(Condition::kGreaterThanEqual, &corrected_args_count, Label::kNear);
  __ masm()->mov(params_size, actual_params_size);
  __ Bind(&corrected_args_count);

  // Leave the frame (also dropping the register file).
  __ masm()->LeaveFrame(StackFrame::BASELINE);

  // Drop receiver + arguments.
  Register return_pc = scratch;
  __ masm()->PopReturnAddressTo(return_pc);
  __ masm()->lea(esp, MemOperand(esp, params_size, times_system_pointer_size,
                                 kSystemPointerSize));
  __ masm()->PushReturnAddressFrom(return_pc);
  __ masm()->Ret();
}

#undef __

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_IA32_BASELINE_ASSEMBLER_IA32_INL_H_
