// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_BASELINE_ASSEMBLER_H_
#define V8_BASELINE_BASELINE_ASSEMBLER_H_

#include "src/codegen/macro-assembler.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/tagged-index.h"

namespace v8 {
namespace internal {
namespace baseline {

class BaselineAssembler {
 public:
  class ScratchRegisterScope;

  explicit BaselineAssembler(MacroAssembler* masm) : masm_(masm) {}
  inline static MemOperand RegisterFrameOperand(
      interpreter::Register interpreter_register);
  inline void RegisterFrameAddress(interpreter::Register interpreter_register,
                                   V8_REG_OUT Register rscratch);
  inline MemOperand ContextOperand();
  inline MemOperand FunctionOperand();
  inline MemOperand FeedbackVectorOperand();
  inline MemOperand FeedbackCellOperand();

  inline void GetCode(LocalIsolate* isolate, CodeDesc* desc);
  inline int pc_offset() const;
  inline void CodeEntry() const;
  inline void ExceptionHandler() const;
  V8_INLINE void RecordComment(const char* string);
  V8_ASM_NO_FALLTHROUGH inline void Trap();
  inline void DebugBreak();
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  inline void AssertInSandboxedExecutionMode();
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  template <typename Field>
  inline void DecodeField(V8_REG_INOUT Register reg);

  inline void Bind(V8_ASM_BIND Label* label);
  // Marks the current position as a valid jump target on CFI enabled
  // architectures.
  inline void JumpTarget();
  V8_ASM_NO_FALLTHROUGH inline void Jump(
      V8_ASM_BRANCH Label* target, Label::Distance distance = Label::kFar);
  inline void JumpIfRoot(V8_REG_IN Register value, RootIndex index,
                         V8_ASM_BRANCH Label* target,
                         Label::Distance distance = Label::kFar);
  inline void JumpIfNotRoot(V8_REG_IN Register value, RootIndex index,
                            V8_ASM_BRANCH Label* target,
                            Label::Distance distance = Label::kFar);
  inline void JumpIfSmi(V8_REG_IN Register value, V8_ASM_BRANCH Label* target,
                        Label::Distance distance = Label::kFar);
  inline void JumpIfNotSmi(V8_REG_IN Register value,
                           V8_ASM_BRANCH Label* target,
                           Label::Distance distance = Label::kFar);

  inline void TestAndBranch(V8_REG_IN Register value, int mask, Condition cc,
                            V8_ASM_BRANCH Label* target,
                            Label::Distance distance = Label::kFar);

  inline void JumpIf(Condition cc, V8_REG_IN Register lhs, const Operand& rhs,
                     V8_ASM_BRANCH Label* target,
                     Label::Distance distance = Label::kFar);
#if V8_STATIC_ROOTS_BOOL
  // Fast JS_RECEIVER test which assumes to receive either a primitive object or
  // a js receiver.
  inline void JumpIfJSAnyIsPrimitive(V8_REG_IN Register heap_object,
                                     V8_ASM_BRANCH Label* target,
                                     Label::Distance distance = Label::kFar);
#endif
  inline void JumpIfObjectType(Condition cc, V8_REG_IN Register object,
                               InstanceType instance_type,
                               V8_REG_OUT Register map,
                               V8_ASM_BRANCH Label* target,
                               Label::Distance distance = Label::kFar);
  // Might not load the map into the scratch register.
  inline void JumpIfObjectTypeFast(Condition cc, V8_REG_IN Register object,
                                   InstanceType instance_type,
                                   V8_ASM_BRANCH Label* target,
                                   Label::Distance distance = Label::kFar);
  inline void JumpIfInstanceType(Condition cc, V8_REG_IN Register map,
                                 InstanceType instance_type,
                                 V8_ASM_BRANCH Label* target,
                                 Label::Distance distance = Label::kFar);
  inline void JumpIfPointer(Condition cc, V8_REG_IN Register value,
                            MemOperand operand, V8_ASM_BRANCH Label* target,
                            Label::Distance distance = Label::kFar);
  inline Condition CheckSmi(V8_REG_IN Register value);
  inline void JumpIfSmi(Condition cc, V8_REG_IN Register value, Tagged<Smi> smi,
                        V8_ASM_BRANCH Label* target,
                        Label::Distance distance = Label::kFar);
  inline void JumpIfSmi(Condition cc, V8_REG_IN Register lhs,
                        V8_REG_IN Register rhs, V8_ASM_BRANCH Label* target,
                        Label::Distance distance = Label::kFar);
  inline void JumpIfImmediate(Condition cc, V8_REG_IN Register left, int right,
                              V8_ASM_BRANCH Label* target,
                              Label::Distance distance = Label::kFar);
  inline void JumpIfTagged(Condition cc, V8_REG_IN Register value,
                           MemOperand operand, V8_ASM_BRANCH Label* target,
                           Label::Distance distance = Label::kFar);
  inline void JumpIfTagged(Condition cc, MemOperand operand,
                           V8_REG_IN Register value,
                           V8_ASM_BRANCH Label* target,
                           Label::Distance distance = Label::kFar);
#ifdef V8_STATIC_ROOTS
  // Jumps to the true or false target if the value is a static root known to by
  // truthy/falsy, fallthrough otherwise.
  inline void JumpIfStaticRootToBoolean(V8_REG_IN Register value,
                                        V8_ASM_BRANCH Label* true_target,
                                        Label::Distance true_distance,
                                        V8_ASM_BRANCH Label* false_target,
                                        Label::Distance false_distance);
#endif
  inline void JumpIfByte(Condition cc, V8_REG_IN Register value, int32_t byte,
                         V8_ASM_BRANCH Label* target,
                         Label::Distance distance = Label::kFar);

  inline void LoadMap(V8_REG_OUT Register output, V8_REG_IN Register value);
  inline void LoadRoot(V8_REG_OUT Register output, RootIndex index);
  inline void LoadNativeContextSlot(V8_REG_OUT Register output, uint32_t index);

  inline void Move(V8_REG_OUT Register output, V8_REG_IN Register source);
  inline void Move(V8_REG_OUT Register output, MemOperand operand);
  inline void Move(V8_REG_OUT Register output, Tagged<Smi> value);
  inline void Move(V8_REG_OUT Register output, Tagged<TaggedIndex> value);
  inline void Move(V8_REG_OUT Register output, interpreter::Register source);
  inline void Move(interpreter::Register output, V8_REG_IN Register source);
  inline void Move(V8_REG_OUT Register output, RootIndex source);
  inline void Move(MemOperand output, V8_REG_IN Register source);
  inline void Move(V8_REG_OUT Register output, ExternalReference reference);
  inline void Move(V8_REG_OUT Register output, Handle<HeapObject> value);
  inline void Move(V8_REG_OUT Register output, int32_t immediate);
  inline void MoveMaybeSmi(V8_REG_OUT Register output,
                           V8_REG_IN Register source);
  inline void MoveSmi(V8_REG_OUT Register output, V8_REG_IN Register source);

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
  V8_ASM_PUSH inline int Push(V8_REG_IN T... vals);

  // Like Push(vals...), but pushes in reverse order, to support our reversed
  // order argument JS calling convention. Doesn't return the number of
  // arguments pushed though.
  //
  // Note that padding is still inserted before the first pushed value (i.e. the
  // last value).
  template <typename... T>
  inline void PushReverse(V8_REG_IN T... vals);

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
  V8_ASM_POP inline void Pop(V8_REG_OUT T... registers);

  V8_REG_CLOBBERS_CALLER_SAVED inline void CallBuiltin(Builtin builtin);
  V8_ASM_NO_FALLTHROUGH inline void TailCallBuiltin(Builtin builtin);
  V8_REG_CLOBBERS_CALLER_SAVED inline void CallRuntime(
      Runtime::FunctionId function, int nargs);

  inline void LoadTaggedField(V8_REG_OUT Register output,
                              V8_REG_IN Register source, int offset);
  inline void LoadTaggedSignedField(V8_REG_OUT Register output,
                                    V8_REG_IN Register source, int offset);
  inline void LoadTaggedSignedFieldAndUntag(V8_REG_OUT Register output,
                                            V8_REG_IN Register source,
                                            int offset);
  inline void LoadWord16FieldZeroExtend(V8_REG_OUT Register output,
                                        V8_REG_IN Register source, int offset);
  inline void LoadWord8Field(V8_REG_OUT Register output,
                             V8_REG_IN Register source, int offset);
#if V8_TARGET_ARCH_X64
  V8_REG_CLOBBERS(kScratchRegister)
#endif
  inline void StoreTaggedSignedField(V8_REG_IN Register target, int offset,
                                     Tagged<Smi> value);
#if V8_TARGET_ARCH_X64
  V8_REG_CLOBBERS(rcx, kScratchRegister)
#elif V8_TARGET_ARCH_ARM64
  V8_REG_CLOBBERS(x5)
#endif
  inline void StoreTaggedFieldWithWriteBarrier(
      V8_REG_IN Register target, int offset, V8_REG_IN_CLOBBER Register value);
  inline void StoreTaggedFieldNoWriteBarrier(V8_REG_IN Register target,
                                             int offset,
                                             V8_REG_IN Register value);
  inline void LoadFixedArrayElement(V8_REG_OUT Register output,
                                    V8_REG_IN Register array, int32_t index);
  inline void LoadPrototype(V8_REG_OUT Register prototype,
                            V8_REG_IN Register object);

// Loads compressed pointer or loads from compressed pointer. This is because
// X64 supports complex addressing mode, pointer decompression can be done by
// [%compressed_base + %r1 + K].
#if V8_TARGET_ARCH_X64
  inline void LoadTaggedField(V8_REG_OUT TaggedRegister output,
                              V8_REG_IN Register source, int offset);
  inline void LoadTaggedField(V8_REG_OUT TaggedRegister output,
                              V8_REG_IN TaggedRegister source, int offset);
  inline void LoadTaggedField(V8_REG_OUT Register output,
                              V8_REG_IN TaggedRegister source, int offset);
  inline void LoadFixedArrayElement(V8_REG_OUT Register output,
                                    V8_REG_IN TaggedRegister array,
                                    int32_t index);
  inline void LoadFixedArrayElement(V8_REG_OUT TaggedRegister output,
                                    V8_REG_IN TaggedRegister array,
                                    int32_t index);

  // Pushes a single register slot without inserting 16-byte stack alignment
  // padding. Only for building fixed/local frame layouts (e.g.
  // PrologueFillFrame) where alignment is managed across the whole frame body.
  inline void PushUnpadded(Register reg);
#endif

  // Falls through and sets scratch_and_result to 0 on failure, jumps to
  // on_result on success.
#if V8_TARGET_ARCH_X64
  V8_REG_CLOBBERS(kScratchRegister)
#endif
  inline void TryLoadOptimizedOsrCode(V8_REG_OUT Register scratch_and_result,
                                      V8_REG_IN Register feedback_vector,
                                      FeedbackSlot slot,
                                      V8_ASM_BRANCH Label* on_result,
                                      Label::Distance distance);

  // Loads the feedback cell from the function, and sets flags on add so that
  // we can compare afterward.
  inline void AddToInterruptBudgetAndJumpIfNotExceeded(
      int32_t weight, V8_ASM_BRANCH Label* skip_interrupt_label);

  // By default, the output register may be compressed on 64-bit architectures
  // that support pointer compression.
  enum class CompressionMode {
    kDefault,
    kForceDecompression,
  };
  inline void LdaContextSlotNoCell(
      V8_REG_INOUT Register context, uint32_t index, uint32_t depth,
      CompressionMode compression_mode = CompressionMode::kDefault,
      V8_REG_OUT Register output = kInterpreterAccumulatorRegister);
#if V8_TARGET_ARCH_X64
  V8_REG_CLOBBERS(rcx, kScratchRegister)
#elif V8_TARGET_ARCH_ARM64
  V8_REG_CLOBBERS(x5)
#endif
  inline void StaContextSlotNoCell(V8_REG_IN_CLOBBER Register context,
                                   V8_REG_IN_CLOBBER Register value,
                                   uint32_t index, uint32_t depth);
  V8_REG_IMPLICIT_OUT(kInterpreterAccumulatorRegister)
  inline void LdaModuleVariable(V8_REG_IN_CLOBBER Register context,
                                int cell_index, uint32_t depth);
#if V8_TARGET_ARCH_X64
  V8_REG_CLOBBERS(rcx, kScratchRegister)
#elif V8_TARGET_ARCH_ARM64
  V8_REG_CLOBBERS(x5)
#endif
  inline void StaModuleVariable(V8_REG_IN_CLOBBER Register context,
                                V8_REG_IN_CLOBBER Register value,
                                int cell_index, uint32_t depth);

  inline void IncrementSmi(MemOperand lhs);
  inline void SmiUntag(V8_REG_INOUT Register value);
  inline void SmiUntag(V8_REG_OUT Register output, V8_REG_IN Register value);

  inline void Word32And(V8_REG_OUT Register output, V8_REG_IN Register lhs,
                        int rhs);

#ifdef V8_ENABLE_SPARKPLUG_PLUS
  // Jumps to |target| unless both |lhs| and |rhs| are Smis, using a single
  // merged tag test.
  inline void JumpIfNotBothSmi(V8_REG_IN Register lhs, V8_REG_IN Register rhs,
                               V8_ASM_BRANCH Label* target,
                               Label::Distance distance = Label::kFar);

  // Compares two tagged values without asserting that they are Smis.
  inline void JumpIfTagged(Condition cc, V8_REG_IN Register lhs,
                           V8_REG_IN Register rhs, V8_ASM_BRANCH Label* target,
                           Label::Distance distance = Label::kFar);

  // value := value + Smi(constant). Jumps to |overflow| when the result is not
  // a valid Smi.
  inline void SmiAddConstantAndJumpIfOverflow(
      V8_REG_INOUT Register value, int constant, V8_ASM_BRANCH Label* overflow,
      Label::Distance distance = Label::kFar);

  // Computes a tagged Smi Add/Sub into |output|. |output| may alias |lhs|.
  // Jumps to |overflow| without modifying |lhs| when the result is not a Smi.
  inline void SmiAddSubAndJumpIfOverflow(
      V8_REG_OUT Register output, V8_REG_IN Register lhs, Tagged<Smi> rhs,
      Operation operation, V8_ASM_BRANCH Label* overflow,
      Label::Distance distance = Label::kFar);

  // Computes BitwiseAnd/BitwiseOr/BitwiseXor/ShiftRight in place.
  // TODO(yuheng): Add support for ShiftLeft and ShiftRightLogical with overflow
  // checks. They are currently omitted because their results can exceed the Smi
  // range.
  inline void SmiBitwiseOp(V8_REG_INOUT Register lhs, Tagged<Smi> rhs,
                           Operation operation);
#endif  // V8_ENABLE_SPARKPLUG_PLUS

  inline void Switch(V8_REG_IN_CLOBBER Register reg, int case_value_base,
                     Label** labels, int num_labels);

  // Register operands.
  inline void LoadRegister(V8_REG_OUT Register output,
                           interpreter::Register source);
  inline void StoreRegister(interpreter::Register output,
                            V8_REG_IN Register value);

  // Frame values
  inline void LoadFunction(V8_REG_OUT Register output);
  inline void LoadContext(V8_REG_OUT Register output);
  inline void StoreContext(V8_REG_IN Register context);

  inline void LoadFeedbackCell(V8_REG_OUT Register output);
  inline void AssertFeedbackCell(V8_REG_IN Register object);

#ifdef V8_ENABLE_CET_SHADOW_STACK
  // If CET shadow stack is enabled, reserves a few bytes as NOP that can be
  // patched later.
  inline void MaybeEmitPlaceHolderForDeopt();
#endif  // V8_ENABLE_CET_SHADOW_STACK

  inline static void EmitReturn(MacroAssembler* masm);

  MacroAssembler* masm() { return masm_; }

 private:
  MacroAssembler* masm_;
  ScratchRegisterScope* scratch_register_scope_ = nullptr;
};

class EnsureAccumulatorPreservedScope final {
 public:
  inline explicit EnsureAccumulatorPreservedScope(BaselineAssembler* assembler);

  inline ~EnsureAccumulatorPreservedScope();

 private:
  inline void AssertEqualToAccumulator(Register reg);

  BaselineAssembler* assembler_;
#ifdef V8_CODE_COMMENTS
  Assembler::CodeComment comment_;
#endif
};

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_BASELINE_ASSEMBLER_H_
