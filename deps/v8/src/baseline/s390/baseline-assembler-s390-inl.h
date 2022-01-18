// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_S390_BASELINE_ASSEMBLER_S390_INL_H_
#define V8_BASELINE_S390_BASELINE_ASSEMBLER_S390_INL_H_

#include "src/baseline/baseline-assembler.h"
#include "src/codegen/s390/assembler-s390-inl.h"
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
      DCHECK(wrapped_scope_.CanAcquire());
      wrapped_scope_.Include(r8, r9);
      wrapped_scope_.Include(kInterpreterBytecodeOffsetRegister);
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

// TODO(v8:11429,leszeks): Unify condition names in the MacroAssembler.
enum class Condition : uint32_t {
  kEqual = static_cast<uint32_t>(eq),
  kNotEqual = static_cast<uint32_t>(ne),

  kLessThan = static_cast<uint32_t>(lt),
  kGreaterThan = static_cast<uint32_t>(gt),
  kLessThanEqual = static_cast<uint32_t>(le),
  kGreaterThanEqual = static_cast<uint32_t>(ge),

  kUnsignedLessThan = static_cast<uint32_t>(lo),
  kUnsignedGreaterThan = static_cast<uint32_t>(hi),
  kUnsignedLessThanEqual = static_cast<uint32_t>(ls),
  kUnsignedGreaterThanEqual = static_cast<uint32_t>(hs),

  kOverflow = static_cast<uint32_t>(vs),
  kNoOverflow = static_cast<uint32_t>(vc),

  kZero = static_cast<uint32_t>(eq),
  kNotZero = static_cast<uint32_t>(ne),
};

inline internal::Condition AsMasmCondition(Condition cond) {
  UNIMPLEMENTED();
  return static_cast<internal::Condition>(cond);
}

namespace detail {

#ifdef DEBUG
inline bool Clobbers(Register target, MemOperand op) {
  UNIMPLEMENTED();
  return false;
}
#endif

}  // namespace detail

#define __ masm_->

MemOperand BaselineAssembler::RegisterFrameOperand(
    interpreter::Register interpreter_register) {
  UNIMPLEMENTED();
  return MemOperand(fp, interpreter_register.ToOperand() * kSystemPointerSize);
}
MemOperand BaselineAssembler::FeedbackVectorOperand() {
  UNIMPLEMENTED();
  return MemOperand(fp, BaselineFrameConstants::kFeedbackVectorFromFp);
}

void BaselineAssembler::Bind(Label* label) { __ bind(label); }
void BaselineAssembler::BindWithoutJumpTarget(Label* label) { __ bind(label); }

void BaselineAssembler::JumpTarget() {
  // NOP on arm.
  UNIMPLEMENTED();
}

void BaselineAssembler::Jump(Label* target, Label::Distance distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfRoot(Register value, RootIndex index,
                                   Label* target, Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfNotRoot(Register value, RootIndex index,
                                      Label* target, Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfSmi(Register value, Label* target,
                                  Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfNotSmi(Register value, Label* target,
                                     Label::Distance) {
  UNIMPLEMENTED();
}

void BaselineAssembler::CallBuiltin(Builtin builtin) { UNIMPLEMENTED(); }

void BaselineAssembler::TailCallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(masm_,
                          __ CommentForOffHeapTrampoline("tail call", builtin));
  UNIMPLEMENTED();
}

void BaselineAssembler::TestAndBranch(Register value, int mask, Condition cc,
                                      Label* target, Label::Distance) {
  UNIMPLEMENTED();
}

void BaselineAssembler::JumpIf(Condition cc, Register lhs, const Operand& rhs,
                               Label* target, Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfObjectType(Condition cc, Register object,
                                         InstanceType instance_type,
                                         Register map, Label* target,
                                         Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfInstanceType(Condition cc, Register map,
                                           InstanceType instance_type,
                                           Label* target, Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfPointer(Condition cc, Register value,
                                      MemOperand operand, Label* target,
                                      Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfSmi(Condition cc, Register value, Smi smi,
                                  Label* target, Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfSmi(Condition cc, Register lhs, Register rhs,
                                  Label* target, Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfTagged(Condition cc, Register value,
                                     MemOperand operand, Label* target,
                                     Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfTagged(Condition cc, MemOperand operand,
                                     Register value, Label* target,
                                     Label::Distance) {
  UNIMPLEMENTED();
}
void BaselineAssembler::JumpIfByte(Condition cc, Register value, int32_t byte,
                                   Label* target, Label::Distance) {
  UNIMPLEMENTED();
}

void BaselineAssembler::Move(interpreter::Register output, Register source) {
  UNIMPLEMENTED();
}
void BaselineAssembler::Move(Register output, TaggedIndex value) {
  UNIMPLEMENTED();
}
void BaselineAssembler::Move(MemOperand output, Register source) {
  UNIMPLEMENTED();
}
void BaselineAssembler::Move(Register output, ExternalReference reference) {
  UNIMPLEMENTED();
}
void BaselineAssembler::Move(Register output, Handle<HeapObject> value) {
  UNIMPLEMENTED();
}
void BaselineAssembler::Move(Register output, int32_t value) {
  UNIMPLEMENTED();
}
void BaselineAssembler::MoveMaybeSmi(Register output, Register source) {
  UNIMPLEMENTED();
}
void BaselineAssembler::MoveSmi(Register output, Register source) {
  UNIMPLEMENTED();
}

namespace detail {

template <typename Arg>
inline Register ToRegister(BaselineAssembler* basm,
                           BaselineAssembler::ScratchRegisterScope* scope,
                           Arg arg) {
  UNIMPLEMENTED();
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
// TODO(ishell): try to pack sequence of pushes into one instruction by
// looking at regiser codes. For example, Push(r1, r2, r5, r0, r3, r4)
// could be generated as two pushes: Push(r1, r2, r5) and Push(r0, r3, r4).
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
// TODO(ishell): try to pack sequence of pushes into one instruction by
// looking at regiser codes. For example, Push(r1, r2, r5, r0, r3, r4)
// could be generated as two pushes: Push(r1, r2, r5) and Push(r0, r3, r4).
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
// TODO(ishell): try to pack sequence of pops into one instruction by
// looking at regiser codes. For example, Pop(r1, r2, r5, r0, r3, r4)
// could be generated as two pops: Pop(r1, r2, r5) and Pop(r0, r3, r4).
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
  UNIMPLEMENTED();
}
void BaselineAssembler::LoadTaggedSignedField(Register output, Register source,
                                              int offset) {
  UNIMPLEMENTED();
}
void BaselineAssembler::LoadTaggedAnyField(Register output, Register source,
                                           int offset) {
  UNIMPLEMENTED();
}
void BaselineAssembler::LoadByteField(Register output, Register source,
                                      int offset) {
  UNIMPLEMENTED();
}
void BaselineAssembler::StoreTaggedSignedField(Register target, int offset,
                                               Smi value) {
  UNIMPLEMENTED();
}
void BaselineAssembler::StoreTaggedFieldWithWriteBarrier(Register target,
                                                         int offset,
                                                         Register value) {
  UNIMPLEMENTED();
}
void BaselineAssembler::StoreTaggedFieldNoWriteBarrier(Register target,
                                                       int offset,
                                                       Register value) {
  UNIMPLEMENTED();
}

void BaselineAssembler::AddToInterruptBudgetAndJumpIfNotExceeded(
    int32_t weight, Label* skip_interrupt_label) {
  UNIMPLEMENTED();
}

void BaselineAssembler::AddToInterruptBudgetAndJumpIfNotExceeded(
    Register weight, Label* skip_interrupt_label) {
  UNIMPLEMENTED();
}

void BaselineAssembler::AddSmi(Register lhs, Smi rhs) { UNIMPLEMENTED(); }

void BaselineAssembler::Switch(Register reg, int case_value_base,
                               Label** labels, int num_labels) {
  UNIMPLEMENTED();
}

#undef __

#define __ basm.

void BaselineAssembler::EmitReturn(MacroAssembler* masm) { UNIMPLEMENTED(); }

#undef __

inline void EnsureAccumulatorPreservedScope::AssertEqualToAccumulator(
    Register reg) {
  UNIMPLEMENTED();
}

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_S390_BASELINE_ASSEMBLER_S390_INL_H_
