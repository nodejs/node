// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
#define V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_

#include "src/allocation.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/frames.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {
namespace interpreter {

class V8_EXPORT_PRIVATE InterpreterAssembler : public CodeStubAssembler {
 public:
  InterpreterAssembler(compiler::CodeAssemblerState* state, Bytecode bytecode,
                       OperandScale operand_scale);
  ~InterpreterAssembler();

  // Returns the 32-bit unsigned count immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandCount(int operand_index);
  // Returns the 32-bit unsigned flag for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::Node* BytecodeOperandFlag(int operand_index);
  // Returns the 32-bit zero-extended index immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandIdxInt32(int operand_index);
  // Returns the word zero-extended index immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandIdx(int operand_index);
  // Returns the smi index immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::Node* BytecodeOperandIdxSmi(int operand_index);
  // Returns the 32-bit unsigned immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::Node* BytecodeOperandUImm(int operand_index);
  // Returns the word-size unsigned immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandUImmWord(int operand_index);
  // Returns the 32-bit signed immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::Node* BytecodeOperandImm(int operand_index);
  // Returns the word-size signed immediate for bytecode operand |operand_index|
  // in the current bytecode.
  compiler::Node* BytecodeOperandImmIntPtr(int operand_index);
  // Returns the smi immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandImmSmi(int operand_index);
  // Returns the word-size sign-extended register index for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandReg(int operand_index);
  // Returns the 32-bit unsigned runtime id immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandRuntimeId(int operand_index);
  // Returns the 32-bit unsigned intrinsic id immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandIntrinsicId(int operand_index);

  // Accumulator.
  compiler::Node* GetAccumulator();
  void SetAccumulator(compiler::Node* value);

  // Context.
  compiler::Node* GetContext();
  void SetContext(compiler::Node* value);

  // Context at |depth| in the context chain starting at |context|.
  compiler::Node* GetContextAtDepth(compiler::Node* context,
                                    compiler::Node* depth);

  // Goto the given |target| if the context chain starting at |context| has any
  // extensions up to the given |depth|.
  void GotoIfHasContextExtensionUpToDepth(compiler::Node* context,
                                          compiler::Node* depth, Label* target);

  // Number of registers.
  compiler::Node* RegisterCount();

  // Backup/restore register file to/from a fixed array of the correct length.
  compiler::Node* ExportRegisterFile(compiler::Node* array);
  compiler::Node* ImportRegisterFile(compiler::Node* array);

  // Loads from and stores to the interpreter register file.
  compiler::Node* LoadRegister(Register reg);
  compiler::Node* LoadRegister(compiler::Node* reg_index);
  compiler::Node* LoadAndUntagRegister(Register reg);
  compiler::Node* StoreRegister(compiler::Node* value, Register reg);
  compiler::Node* StoreRegister(compiler::Node* value,
                                compiler::Node* reg_index);
  compiler::Node* StoreAndTagRegister(compiler::Node* value, Register reg);

  // Returns the next consecutive register.
  compiler::Node* NextRegister(compiler::Node* reg_index);

  // Returns the location in memory of the register |reg_index| in the
  // interpreter register file.
  compiler::Node* RegisterLocation(compiler::Node* reg_index);

  // Load constant at |index| in the constant pool.
  compiler::Node* LoadConstantPoolEntry(compiler::Node* index);

  // Load and untag constant at |index| in the constant pool.
  compiler::Node* LoadAndUntagConstantPoolEntry(compiler::Node* index);

  // Load the FeedbackVector for the current function.
  compiler::Node* LoadFeedbackVector();

  // Increment the call count for a CALL_IC or construct call.
  // The call count is located at feedback_vector[slot_id + 1].
  compiler::Node* IncrementCallCount(compiler::Node* feedback_vector,
                                     compiler::Node* slot_id);

  // Call JSFunction or Callable |function| with |arg_count| arguments (not
  // including receiver) and the first argument located at |first_arg|. Type
  // feedback is collected in the slot at index |slot_id|.
  //
  // If the |receiver_mode| is kNullOrUndefined, then the receiver is implicitly
  // undefined and |first_arg| is the first parameter. Otherwise, |first_arg| is
  // the receiver and it is converted according to |receiver_mode|.
  compiler::Node* CallJSWithFeedback(
      compiler::Node* function, compiler::Node* context,
      compiler::Node* first_arg, compiler::Node* arg_count,
      compiler::Node* slot_id, compiler::Node* feedback_vector,
      ConvertReceiverMode receiver_mode, TailCallMode tail_call_mode);

  // Call JSFunction or Callable |function| with |arg_count| arguments (not
  // including receiver) and the first argument located at |first_arg|, possibly
  // including the receiver depending on |receiver_mode|.
  compiler::Node* CallJS(compiler::Node* function, compiler::Node* context,
                         compiler::Node* first_arg, compiler::Node* arg_count,
                         ConvertReceiverMode receiver_mode,
                         TailCallMode tail_call_mode);

  // Call JSFunction or Callable |function| with |arg_count|
  // arguments (not including receiver) and the first argument
  // located at |first_arg|.
  compiler::Node* CallJSWithSpread(compiler::Node* function,
                                   compiler::Node* context,
                                   compiler::Node* first_arg,
                                   compiler::Node* arg_count);

  // Call constructor |constructor| with |arg_count| arguments (not
  // including receiver) and the first argument located at
  // |first_arg|. The |new_target| is the same as the
  // |constructor| for the new keyword, but differs for the super
  // keyword.
  compiler::Node* Construct(compiler::Node* constructor,
                            compiler::Node* context, compiler::Node* new_target,
                            compiler::Node* first_arg,
                            compiler::Node* arg_count, compiler::Node* slot_id,
                            compiler::Node* feedback_vector);

  // Call constructor |constructor| with |arg_count| arguments (not including
  // receiver) and the first argument located at |first_arg|. The last argument
  // is always a spread. The |new_target| is the same as the |constructor| for
  // the new keyword, but differs for the super keyword.
  compiler::Node* ConstructWithSpread(compiler::Node* constructor,
                                      compiler::Node* context,
                                      compiler::Node* new_target,
                                      compiler::Node* first_arg,
                                      compiler::Node* arg_count);

  // Call runtime function with |arg_count| arguments and the first argument
  // located at |first_arg|.
  compiler::Node* CallRuntimeN(compiler::Node* function_id,
                               compiler::Node* context,
                               compiler::Node* first_arg,
                               compiler::Node* arg_count, int return_size = 1);

  // Jump forward relative to the current bytecode by the |jump_offset|.
  compiler::Node* Jump(compiler::Node* jump_offset);

  // Jump backward relative to the current bytecode by the |jump_offset|.
  compiler::Node* JumpBackward(compiler::Node* jump_offset);

  // Jump forward relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are equal.
  void JumpIfWordEqual(compiler::Node* lhs, compiler::Node* rhs,
                       compiler::Node* jump_offset);

  // Jump forward relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are not equal.
  void JumpIfWordNotEqual(compiler::Node* lhs, compiler::Node* rhs,
                          compiler::Node* jump_offset);

  // Returns true if the stack guard check triggers an interrupt.
  compiler::Node* StackCheckTriggeredInterrupt();

  // Updates the profiler interrupt budget for a return.
  void UpdateInterruptBudgetOnReturn();

  // Returns the OSR nesting level from the bytecode header.
  compiler::Node* LoadOSRNestingLevel();

  // Dispatch to the bytecode.
  compiler::Node* Dispatch();

  // Dispatch to bytecode handler.
  compiler::Node* DispatchToBytecodeHandler(compiler::Node* handler) {
    return DispatchToBytecodeHandler(handler, BytecodeOffset());
  }

  // Dispatch bytecode as wide operand variant.
  void DispatchWide(OperandScale operand_scale);

  // Truncate tagged |value| to word32 and store the type feedback in
  // |var_type_feedback|.
  compiler::Node* TruncateTaggedToWord32WithFeedback(
      compiler::Node* context, compiler::Node* value,
      Variable* var_type_feedback);

  // Abort with the given bailout reason.
  void Abort(BailoutReason bailout_reason);
  void AbortIfWordNotEqual(compiler::Node* lhs, compiler::Node* rhs,
                           BailoutReason bailout_reason);

  // Dispatch to frame dropper trampoline if necessary.
  void MaybeDropFrames(compiler::Node* context);

  // Returns the offset from the BytecodeArrayPointer of the current bytecode.
  compiler::Node* BytecodeOffset();

 protected:
  Bytecode bytecode() const { return bytecode_; }
  static bool TargetSupportsUnalignedAccess();

 private:
  // Returns a tagged pointer to the current function's BytecodeArray object.
  compiler::Node* BytecodeArrayTaggedPointer();

  // Returns a raw pointer to first entry in the interpreter dispatch table.
  compiler::Node* DispatchTableRawPointer();

  // Returns the accumulator value without checking whether bytecode
  // uses it. This is intended to be used only in dispatch and in
  // tracing as these need to bypass accumulator use validity checks.
  compiler::Node* GetAccumulatorUnchecked();

  // Returns the frame pointer for the interpreted frame of the function being
  // interpreted.
  compiler::Node* GetInterpretedFramePointer();

  // Saves and restores interpreter bytecode offset to the interpreter stack
  // frame when performing a call.
  void CallPrologue();
  void CallEpilogue();

  // Increment the dispatch counter for the (current, next) bytecode pair.
  void TraceBytecodeDispatch(compiler::Node* target_index);

  // Traces the current bytecode by calling |function_id|.
  void TraceBytecode(Runtime::FunctionId function_id);

  // Updates the bytecode array's interrupt budget by a 32-bit unsigned |weight|
  // and calls Runtime::kInterrupt if counter reaches zero. If |backward|, then
  // the interrupt budget is decremented, otherwise it is incremented.
  void UpdateInterruptBudget(compiler::Node* weight, bool backward);

  // Returns the offset of register |index| relative to RegisterFilePointer().
  compiler::Node* RegisterFrameOffset(compiler::Node* index);

  // Returns the offset of an operand relative to the current bytecode offset.
  compiler::Node* OperandOffset(int operand_index);

  // Returns a value built from an sequence of bytes in the bytecode
  // array starting at |relative_offset| from the current bytecode.
  // The |result_type| determines the size and signedness.  of the
  // value read. This method should only be used on architectures that
  // do not support unaligned memory accesses.
  compiler::Node* BytecodeOperandReadUnaligned(int relative_offset,
                                               MachineType result_type);

  // Returns zero- or sign-extended to word32 value of the operand.
  compiler::Node* BytecodeOperandUnsignedByte(int operand_index);
  compiler::Node* BytecodeOperandSignedByte(int operand_index);
  compiler::Node* BytecodeOperandUnsignedShort(int operand_index);
  compiler::Node* BytecodeOperandSignedShort(int operand_index);
  compiler::Node* BytecodeOperandUnsignedQuad(int operand_index);
  compiler::Node* BytecodeOperandSignedQuad(int operand_index);

  // Returns zero- or sign-extended to word32 value of the operand of
  // given size.
  compiler::Node* BytecodeSignedOperand(int operand_index,
                                        OperandSize operand_size);
  compiler::Node* BytecodeUnsignedOperand(int operand_index,
                                          OperandSize operand_size);

  // Jump relative to the current bytecode by the |jump_offset|. If |backward|,
  // then jump backward (subtract the offset), otherwise jump forward (add the
  // offset). Helper function for Jump and JumpBackward.
  compiler::Node* Jump(compiler::Node* jump_offset, bool backward);

  // Jump forward relative to the current bytecode by |jump_offset| if the
  // |condition| is true. Helper function for JumpIfWordEqual and
  // JumpIfWordNotEqual.
  void JumpConditional(compiler::Node* condition, compiler::Node* jump_offset);

  // Save the bytecode offset to the interpreter frame.
  void SaveBytecodeOffset();

  // Updates and returns BytecodeOffset() advanced by the current bytecode's
  // size. Traces the exit of the current bytecode.
  compiler::Node* Advance();

  // Updates and returns BytecodeOffset() advanced by delta bytecodes.
  // Traces the exit of the current bytecode.
  compiler::Node* Advance(int delta);
  compiler::Node* Advance(compiler::Node* delta, bool backward = false);

  // Load the bytecode at |bytecode_offset|.
  compiler::Node* LoadBytecode(compiler::Node* bytecode_offset);

  // Look ahead for Star and inline it in a branch. Returns a new target
  // bytecode node for dispatch.
  compiler::Node* StarDispatchLookahead(compiler::Node* target_bytecode);

  // Build code for Star at the current BytecodeOffset() and Advance() to the
  // next dispatch offset.
  void InlineStar();

  // Dispatch to |target_bytecode| at |new_bytecode_offset|.
  // |target_bytecode| should be equivalent to loading from the offset.
  compiler::Node* DispatchToBytecode(compiler::Node* target_bytecode,
                                     compiler::Node* new_bytecode_offset);

  // Dispatch to the bytecode handler with code offset |handler|.
  compiler::Node* DispatchToBytecodeHandler(compiler::Node* handler,
                                            compiler::Node* bytecode_offset);

  // Dispatch to the bytecode handler with code entry point |handler_entry|.
  compiler::Node* DispatchToBytecodeHandlerEntry(
      compiler::Node* handler_entry, compiler::Node* bytecode_offset);

  int CurrentBytecodeSize() const;

  OperandScale operand_scale() const { return operand_scale_; }

  Bytecode bytecode_;
  OperandScale operand_scale_;
  CodeStubAssembler::Variable bytecode_offset_;
  CodeStubAssembler::Variable interpreted_frame_pointer_;
  CodeStubAssembler::Variable bytecode_array_;
  CodeStubAssembler::Variable dispatch_table_;
  CodeStubAssembler::Variable accumulator_;
  AccumulatorUse accumulator_use_;
  bool made_call_;
  bool reloaded_frame_ptr_;
  bool saved_bytecode_offset_;

  bool disable_stack_check_across_call_;
  compiler::Node* stack_pointer_before_call_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterAssembler);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
