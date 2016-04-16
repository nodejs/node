// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
#define V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_

#include "src/allocation.h"
#include "src/base/smart-pointers.h"
#include "src/builtins.h"
#include "src/compiler/code-stub-assembler.h"
#include "src/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {
namespace interpreter {

class InterpreterAssembler : public compiler::CodeStubAssembler {
 public:
  InterpreterAssembler(Isolate* isolate, Zone* zone, Bytecode bytecode);
  virtual ~InterpreterAssembler();

  // Returns the count immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandCount(int operand_index);
  // Returns the index immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandIdx(int operand_index);
  // Returns the Imm8 immediate for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandImm(int operand_index);
  // Returns the register index for bytecode operand |operand_index| in the
  // current bytecode.
  compiler::Node* BytecodeOperandReg(int operand_index);

  // Accumulator.
  compiler::Node* GetAccumulator();
  void SetAccumulator(compiler::Node* value);

  // Context.
  compiler::Node* GetContext();
  void SetContext(compiler::Node* value);

  // Loads from and stores to the interpreter register file.
  compiler::Node* LoadRegister(int offset);
  compiler::Node* LoadRegister(Register reg);
  compiler::Node* LoadRegister(compiler::Node* reg_index);
  compiler::Node* StoreRegister(compiler::Node* value, int offset);
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

  // Load an element from a fixed array on the heap.
  compiler::Node* LoadFixedArrayElement(compiler::Node* fixed_array, int index);

  // Load a field from an object on the heap.
  compiler::Node* LoadObjectField(compiler::Node* object, int offset);

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
                                compiler::Node* arg_count);

  // Call runtime function with |arg_count| arguments and the first argument
  // located at |first_arg|.
  compiler::Node* CallRuntimeN(compiler::Node* function_id,
                               compiler::Node* context,
                               compiler::Node* first_arg,
                               compiler::Node* arg_count, int return_size = 1);

  // Jump relative to the current bytecode by |jump_offset|.
  void Jump(compiler::Node* jump_offset);

  // Jump relative to the current bytecode by |jump_offset| if the
  // |condition| is true. Helper function for JumpIfWordEqual and
  // JumpIfWordNotEqual.
  void JumpConditional(compiler::Node* condition, compiler::Node* jump_offset);

  // Jump relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are equal.
  void JumpIfWordEqual(compiler::Node* lhs, compiler::Node* rhs,
                       compiler::Node* jump_offset);

  // Jump relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are not equal.
  void JumpIfWordNotEqual(compiler::Node* lhs, compiler::Node* rhs,
                          compiler::Node* jump_offset);

  // Perform a stack guard check.
  void StackCheck();

  // Returns from the function.
  void InterpreterReturn();

  // Dispatch to the bytecode.
  void Dispatch();

  // Dispatch to bytecode handler.
  void DispatchToBytecodeHandler(compiler::Node* handler,
                                 compiler::Node* bytecode_offset);
  void DispatchToBytecodeHandler(compiler::Node* handler) {
    DispatchToBytecodeHandler(handler, BytecodeOffset());
  }

  // Abort with the given bailout reason.
  void Abort(BailoutReason bailout_reason);

 protected:
  static bool TargetSupportsUnalignedAccess();

 private:
  // Returns a raw pointer to start of the register file on the stack.
  compiler::Node* RegisterFileRawPointer();
  // Returns a tagged pointer to the current function's BytecodeArray object.
  compiler::Node* BytecodeArrayTaggedPointer();
  // Returns the offset from the BytecodeArrayPointer of the current bytecode.
  compiler::Node* BytecodeOffset();
  // Returns a raw pointer to first entry in the interpreter dispatch table.
  compiler::Node* DispatchTableRawPointer();

  // Saves and restores interpreter bytecode offset to the interpreter stack
  // frame when performing a call.
  void CallPrologue() override;
  void CallEpilogue() override;

  // Traces the current bytecode by calling |function_id|.
  void TraceBytecode(Runtime::FunctionId function_id);

  // Updates the bytecode array's interrupt budget by |weight| and calls
  // Runtime::kInterrupt if counter reaches zero.
  void UpdateInterruptBudget(compiler::Node* weight);

  // Returns the offset of register |index| relative to RegisterFilePointer().
  compiler::Node* RegisterFrameOffset(compiler::Node* index);

  compiler::Node* BytecodeOperand(int operand_index);
  compiler::Node* BytecodeOperandSignExtended(int operand_index);
  compiler::Node* BytecodeOperandShort(int operand_index);
  compiler::Node* BytecodeOperandShortSignExtended(int operand_index);

  // Returns BytecodeOffset() advanced by delta bytecodes. Note: this does not
  // update BytecodeOffset() itself.
  compiler::Node* Advance(int delta);
  compiler::Node* Advance(compiler::Node* delta);

  // Starts next instruction dispatch at |new_bytecode_offset|.
  void DispatchTo(compiler::Node* new_bytecode_offset);

  // Abort operations for debug code.
  void AbortIfWordNotEqual(compiler::Node* lhs, compiler::Node* rhs,
                           BailoutReason bailout_reason);

  Bytecode bytecode_;
  CodeStubAssembler::Variable accumulator_;
  CodeStubAssembler::Variable context_;
  CodeStubAssembler::Variable bytecode_array_;

  bool disable_stack_check_across_call_;
  compiler::Node* stack_pointer_before_call_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterAssembler);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_ASSEMBLER_H_
