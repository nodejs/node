// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_BASELINE_ASSEMBLER_H_
#define V8_BASELINE_BASELINE_ASSEMBLER_H_

// TODO(v8:11421): Remove #if once baseline compiler is ported to other
// architectures.
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || \
    V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64

#include "src/codegen/macro-assembler.h"
#include "src/objects/tagged-index.h"

namespace v8 {
namespace internal {
namespace baseline {

enum class Condition : uint32_t;

class BaselineAssembler {
 public:
  class ScratchRegisterScope;

  explicit BaselineAssembler(MacroAssembler* masm) : masm_(masm) {}
  inline static MemOperand RegisterFrameOperand(
      interpreter::Register interpreter_register);
  inline MemOperand ContextOperand();
  inline MemOperand FunctionOperand();
  inline MemOperand FeedbackVectorOperand();

  inline void GetCode(Isolate* isolate, CodeDesc* desc);
  inline int pc_offset() const;
  inline void CodeEntry() const;
  inline void ExceptionHandler() const;
  V8_INLINE void RecordComment(const char* string);
  inline void Trap();
  inline void DebugBreak();

  inline void Bind(Label* label);
  // Binds the label without marking it as a valid jump target.
  // This is only useful, when the position is already marked as a valid jump
  // target (i.e. at the beginning of the bytecode).
  inline void BindWithoutJumpTarget(Label* label);
  // Marks the current position as a valid jump target on CFI enabled
  // architectures.
  inline void JumpTarget();
  inline void JumpIf(Condition cc, Label* target,
                     Label::Distance distance = Label::kFar);
  inline void Jump(Label* target, Label::Distance distance = Label::kFar);
  inline void JumpIfRoot(Register value, RootIndex index, Label* target,
                         Label::Distance distance = Label::kFar);
  inline void JumpIfNotRoot(Register value, RootIndex index, Label* target,
                            Label ::Distance distance = Label::kFar);
  inline void JumpIfSmi(Register value, Label* target,
                        Label::Distance distance = Label::kFar);
  inline void JumpIfNotSmi(Register value, Label* target,
                           Label::Distance distance = Label::kFar);

  inline void Test(Register value, int mask);

  inline void CmpObjectType(Register object, InstanceType instance_type,
                            Register map);
  inline void CmpInstanceType(Register map, InstanceType instance_type);
  inline void Cmp(Register value, Smi smi);
  inline void ComparePointer(Register value, MemOperand operand);
  inline Condition CheckSmi(Register value);
  inline void SmiCompare(Register lhs, Register rhs);
  inline void CompareTagged(Register value, MemOperand operand);
  inline void CompareTagged(MemOperand operand, Register value);
  inline void CompareByte(Register value, int32_t byte);

  inline void LoadMap(Register output, Register value);
  inline void LoadRoot(Register output, RootIndex index);
  inline void LoadNativeContextSlot(Register output, uint32_t index);

  inline void Move(Register output, Register source);
  inline void Move(Register output, MemOperand operand);
  inline void Move(Register output, Smi value);
  inline void Move(Register output, TaggedIndex value);
  inline void Move(Register output, interpreter::Register source);
  inline void Move(interpreter::Register output, Register source);
  inline void Move(Register output, RootIndex source);
  inline void Move(MemOperand output, Register source);
  inline void Move(Register output, ExternalReference reference);
  inline void Move(Register output, Handle<HeapObject> value);
  inline void Move(Register output, int32_t immediate);
  inline void MoveMaybeSmi(Register output, Register source);
  inline void MoveSmi(Register output, Register source);

  // Push the given values, in the given order. If the stack needs alignment
  // (looking at you Arm64), the stack is padded from the front (i.e. before the
  // first value is pushed).
  //
  // This supports pushing a RegisterList as the last value -- the list is
  // iterated and each interpreter Register is pushed.
  //
  // The total number of values pushed is returned. Note that this might be
  // different from sizeof(T...), specifically if there was a RegisterList.
  template <typename... T>
  inline int Push(T... vals);

  // Like Push(vals...), but pushes in reverse order, to support our reversed
  // order argument JS calling convention. Doesn't return the number of
  // arguments pushed though.
  //
  // Note that padding is still inserted before the first pushed value (i.e. the
  // last value).
  template <typename... T>
  inline void PushReverse(T... vals);

  // Pop values off the stack into the given registers.
  //
  // Note that this inserts into registers in the given order, i.e. in reverse
  // order if the registers were pushed. This means that to spill registers,
  // push and pop have to be in reverse order, e.g.
  //
  //     Push(r1, r2, ..., rN);
  //     ClobberRegisters();
  //     Pop(rN, ..., r2, r1);
  //
  // On stack-alignment architectures, any padding is popped off after the last
  // register. This the behaviour of Push, which means that the above code still
  // works even if the number of registers doesn't match stack alignment.
  template <typename... T>
  inline void Pop(T... registers);

  inline void CallBuiltin(Builtins::Name builtin);
  inline void TailCallBuiltin(Builtins::Name builtin);
  inline void CallRuntime(Runtime::FunctionId function, int nargs);

  inline void LoadTaggedPointerField(Register output, Register source,
                                     int offset);
  inline void LoadTaggedSignedField(Register output, Register source,
                                    int offset);
  inline void LoadTaggedAnyField(Register output, Register source, int offset);
  inline void LoadByteField(Register output, Register source, int offset);
  inline void StoreTaggedSignedField(Register target, int offset, Smi value);
  inline void StoreTaggedFieldWithWriteBarrier(Register target, int offset,
                                               Register value);
  inline void StoreTaggedFieldNoWriteBarrier(Register target, int offset,
                                             Register value);
  inline void LoadFixedArrayElement(Register output, Register array,
                                    int32_t index);
  inline void LoadPrototype(Register prototype, Register object);

  // Loads the feedback cell from the function, and sets flags on add so that
  // we can compare afterward.
  inline void AddToInterruptBudget(int32_t weight);
  inline void AddToInterruptBudget(Register weight);

  inline void AddSmi(Register lhs, Smi rhs);
  inline void SmiUntag(Register value);
  inline void SmiUntag(Register output, Register value);

  inline void Switch(Register reg, int case_value_base, Label** labels,
                     int num_labels);

  // Register operands.
  inline void LoadRegister(Register output, interpreter::Register source);
  inline void StoreRegister(interpreter::Register output, Register value);

  // Frame values
  inline void LoadFunction(Register output);
  inline void LoadContext(Register output);
  inline void StoreContext(Register context);

  inline static void EmitReturn(MacroAssembler* masm);

  MacroAssembler* masm() { return masm_; }

 private:
  MacroAssembler* masm_;
  ScratchRegisterScope* scratch_register_scope_ = nullptr;
};

class SaveAccumulatorScope final {
 public:
  inline explicit SaveAccumulatorScope(BaselineAssembler* assembler);

  inline ~SaveAccumulatorScope();

 private:
  BaselineAssembler* assembler_;
};

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif

#endif  // V8_BASELINE_BASELINE_ASSEMBLER_H_
