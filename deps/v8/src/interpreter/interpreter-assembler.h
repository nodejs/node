// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
#define V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_

#include "src/allocation.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/frames.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {
namespace interpreter {

class InterpreterAssembler : public CodeStubAssembler {
 public:
  InterpreterAssembler(Isolate* isolate, Zone* zone, Bytecode bytecode,
                       OperandScale operand_scale);
  virtual ~InterpreterAssembler();

  // Returns the count immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandCount(int operand_index);
  // Returns the 8-bit flag for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandFlag(int operand_index);
  // Returns the index immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandIdx(int operand_index);
  // Returns the UImm8 immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandUImm(int operand_index);
  // Returns the Imm8 immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandImm(int operand_index);
  // Returns the register index for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandReg(int operand_index);
  // Returns the runtime id immediate for bytecode operand
  // |operand_index| in the current bytecode.
  compiler::Node* BytecodeOperandRuntimeId(int operand_index);
  // Returns the intrinsic id immediate for bytecode operand
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
  compiler::Node* StoreRegister(compiler::Node* value, Register reg);
  compiler::Node* StoreRegister(compiler::Node* value,
                                compiler::Node* reg_index);

  // Returns the next consecutive register.
  compiler::Node* NextRegister(compiler::Node* reg_index);

  // Returns the location in memory of the register |reg_index| in the
  // interpreter register file.
  compiler::Node* RegisterLocation(compiler::Node* reg_index);

  // Load constant at |index| in the constant pool.
  compiler::Node* LoadConstantPoolEntry(compiler::Node* index);

  // Load and untag constant at |index| in the constant pool.
  compiler::Node* LoadAndUntagConstantPoolEntry(compiler::Node* index);

  // Load |slot_index| from |context|.
  compiler::Node* LoadContextSlot(compiler::Node* context, int slot_index);
  compiler::Node* LoadContextSlot(compiler::Node* context,
                                  compiler::Node* slot_index);
  // Stores |value| into |slot_index| of |context|.
  compiler::Node* StoreContextSlot(compiler::Node* context,
                                   compiler::Node* slot_index,
                                   compiler::Node* value);

  // Load the TypeFeedbackVector for the current function.
  compiler::Node* LoadTypeFeedbackVector();

  // Increment the call count for a CALL_IC or construct call.
  // The call count is located at feedback_vector[slot_id + 1].
  compiler::Node* IncrementCallCount(compiler::Node* type_feedback_vector,
                                     compiler::Node* slot_id);

  // Call JSFunction or Callable |function| with |arg_count|
  // arguments (not including receiver) and the first argument
  // located at |first_arg|. Type feedback is collected in the
  // slot at index |slot_id|.
  compiler::Node* CallJSWithFeedback(compiler::Node* function,
                                     compiler::Node* context,
                                     compiler::Node* first_arg,
                                     compiler::Node* arg_count,
                                     compiler::Node* slot_id,
                                     compiler::Node* type_feedback_vector,
                                     TailCallMode tail_call_mode);

  // Call JSFunction or Callable |function| with |arg_count|
  // arguments (not including receiver) and the first argument
  // located at |first_arg|.
  compiler::Node* CallJS(compiler::Node* function, compiler::Node* context,
                         compiler::Node* first_arg, compiler::Node* arg_count,
                         TailCallMode tail_call_mode);

  // Call constructor |constructor| with |arg_count| arguments (not
  // including receiver) and the first argument located at
  // |first_arg|. The |new_target| is the same as the
  // |constructor| for the new keyword, but differs for the super
  // keyword.
  compiler::Node* CallConstruct(compiler::Node* constructor,
                                compiler::Node* context,
                                compiler::Node* new_target,
                                compiler::Node* first_arg,
                                compiler::Node* arg_count,
                                compiler::Node* slot_id,
                                compiler::Node* type_feedback_vector);

  // Call runtime function with |arg_count| arguments and the first argument
  // located at |first_arg|.
  compiler::Node* CallRuntimeN(compiler::Node* function_id,
                               compiler::Node* context,
                               compiler::Node* first_arg,
                               compiler::Node* arg_count, int return_size = 1);

  // Jump relative to the current bytecode by |jump_offset|.
  compiler::Node* Jump(compiler::Node* jump_offset);

  // Jump relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are equal.
  void JumpIfWordEqual(compiler::Node* lhs, compiler::Node* rhs,
                       compiler::Node* jump_offset);

  // Jump relative to the current bytecode by |jump_offset| if the
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
  void CallPrologue() override;
  void CallEpilogue() override;

  // Increment the dispatch counter for the (current, next) bytecode pair.
  void TraceBytecodeDispatch(compiler::Node* target_index);

  // Traces the current bytecode by calling |function_id|.
  void TraceBytecode(Runtime::FunctionId function_id);

  // Updates the bytecode array's interrupt budget by |weight| and calls
  // Runtime::kInterrupt if counter reaches zero.
  void UpdateInterruptBudget(compiler::Node* weight);

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

  compiler::Node* BytecodeOperandUnsignedByte(int operand_index);
  compiler::Node* BytecodeOperandSignedByte(int operand_index);
  compiler::Node* BytecodeOperandUnsignedShort(int operand_index);
  compiler::Node* BytecodeOperandSignedShort(int operand_index);
  compiler::Node* BytecodeOperandUnsignedQuad(int operand_index);
  compiler::Node* BytecodeOperandSignedQuad(int operand_index);

  compiler::Node* BytecodeSignedOperand(int operand_index,
                                        OperandSize operand_size);
  compiler::Node* BytecodeUnsignedOperand(int operand_index,
                                          OperandSize operand_size);

  // Jump relative to the current bytecode by |jump_offset| if the
  // |condition| is true. Helper function for JumpIfWordEqual and
  // JumpIfWordNotEqual.
  void JumpConditional(compiler::Node* condition, compiler::Node* jump_offset);

  // Updates and returns BytecodeOffset() advanced by the current bytecode's
  // size. Traces the exit of the current bytecode.
  compiler::Node* Advance();

  // Updates and returns BytecodeOffset() advanced by delta bytecodes.
  // Traces the exit of the current bytecode.
  compiler::Node* Advance(int delta);
  compiler::Node* Advance(compiler::Node* delta);

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

  OperandScale operand_scale() const { return operand_scale_; }

  Bytecode bytecode_;
  OperandScale operand_scale_;
  CodeStubAssembler::Variable bytecode_offset_;
  CodeStubAssembler::Variable interpreted_frame_pointer_;
  CodeStubAssembler::Variable accumulator_;
  AccumulatorUse accumulator_use_;
  bool made_call_;

  bool disable_stack_check_across_call_;
  compiler::Node* stack_pointer_before_call_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterAssembler);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
