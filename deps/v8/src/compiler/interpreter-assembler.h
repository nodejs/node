// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INTERPRETER_ASSEMBLER_H_
#define V8_COMPILER_INTERPRETER_ASSEMBLER_H_

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/allocation.h"
#include "src/base/smart-pointers.h"
#include "src/builtins.h"
#include "src/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class Isolate;
class Zone;

namespace compiler {

class CallDescriptor;
class Graph;
class Node;
class Operator;
class RawMachineAssembler;
class Schedule;

class InterpreterAssembler {
 public:
  InterpreterAssembler(Isolate* isolate, Zone* zone,
                       interpreter::Bytecode bytecode);
  virtual ~InterpreterAssembler();

  Handle<Code> GenerateCode();

  // Returns the count immediate for bytecode operand |operand_index| in the
  // current bytecode.
  Node* BytecodeOperandCount(int operand_index);
  // Returns the index immediate for bytecode operand |operand_index| in the
  // current bytecode.
  Node* BytecodeOperandIdx(int operand_index);
  // Returns the Imm8 immediate for bytecode operand |operand_index| in the
  // current bytecode.
  Node* BytecodeOperandImm(int operand_index);
  // Returns the register index for bytecode operand |operand_index| in the
  // current bytecode.
  Node* BytecodeOperandReg(int operand_index);

  // Accumulator.
  Node* GetAccumulator();
  void SetAccumulator(Node* value);

  // Context.
  Node* GetContext();
  void SetContext(Node* value);

  // Loads from and stores to the interpreter register file.
  Node* LoadRegister(int offset);
  Node* LoadRegister(interpreter::Register reg);
  Node* LoadRegister(Node* reg_index);
  Node* StoreRegister(Node* value, int offset);
  Node* StoreRegister(Node* value, interpreter::Register reg);
  Node* StoreRegister(Node* value, Node* reg_index);

  // Returns the next consecutive register.
  Node* NextRegister(Node* reg_index);

  // Returns the location in memory of the register |reg_index| in the
  // interpreter register file.
  Node* RegisterLocation(Node* reg_index);

  // Constants.
  Node* Int32Constant(int value);
  Node* IntPtrConstant(intptr_t value);
  Node* NumberConstant(double value);
  Node* HeapConstant(Handle<HeapObject> object);
  Node* BooleanConstant(bool value);

  // Tag and untag Smi values.
  Node* SmiTag(Node* value);
  Node* SmiUntag(Node* value);

  // Basic arithmetic operations.
  Node* IntPtrAdd(Node* a, Node* b);
  Node* IntPtrSub(Node* a, Node* b);
  Node* WordShl(Node* value, int shift);

  // Load constant at |index| in the constant pool.
  Node* LoadConstantPoolEntry(Node* index);

  // Load an element from a fixed array on the heap.
  Node* LoadFixedArrayElement(Node* fixed_array, int index);

  // Load a field from an object on the heap.
  Node* LoadObjectField(Node* object, int offset);

  // Load |slot_index| from |context|.
  Node* LoadContextSlot(Node* context, int slot_index);
  Node* LoadContextSlot(Node* context, Node* slot_index);
  // Stores |value| into |slot_index| of |context|.
  Node* StoreContextSlot(Node* context, Node* slot_index, Node* value);

  // Load the TypeFeedbackVector for the current function.
  Node* LoadTypeFeedbackVector();

  // Project the output value at index |index|
  Node* Projection(int index, Node* node);

  // Call constructor |constructor| with |arg_count| arguments (not
  // including receiver) and the first argument located at
  // |first_arg|. The |new_target| is the same as the
  // |constructor| for the new keyword, but differs for the super
  // keyword.
  Node* CallConstruct(Node* new_target, Node* constructor, Node* first_arg,
                      Node* arg_count);

  // Call JSFunction or Callable |function| with |arg_count|
  // arguments (not including receiver) and the first argument
  // located at |first_arg|.
  Node* CallJS(Node* function, Node* first_arg, Node* arg_count);

  // Call an IC code stub.
  Node* CallIC(CallInterfaceDescriptor descriptor, Node* target, Node* arg1,
               Node* arg2, Node* arg3);
  Node* CallIC(CallInterfaceDescriptor descriptor, Node* target, Node* arg1,
               Node* arg2, Node* arg3, Node* arg4);
  Node* CallIC(CallInterfaceDescriptor descriptor, Node* target, Node* arg1,
               Node* arg2, Node* arg3, Node* arg4, Node* arg5);

  // Call runtime function.
  Node* CallRuntime(Node* function_id, Node* first_arg, Node* arg_count,
                    int return_size = 1);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* arg1);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* arg1, Node* arg2);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* arg1, Node* arg2,
                    Node* arg3, Node* arg4);

  // Jump relative to the current bytecode by |jump_offset|.
  void Jump(Node* jump_offset);

  // Jump relative to the current bytecode by |jump_offset| if the
  // word values |lhs| and |rhs| are equal.
  void JumpIfWordEqual(Node* lhs, Node* rhs, Node* jump_offset);

  // Returns from the function.
  void Return();

  // Dispatch to the bytecode.
  void Dispatch();

  // Abort with the given bailout reason.
  void Abort(BailoutReason bailout_reason);

 protected:
  static bool TargetSupportsUnalignedAccess();

  // Protected helpers (for testing) which delegate to RawMachineAssembler.
  CallDescriptor* call_descriptor() const;
  Graph* graph();

 private:
  // Returns a raw pointer to start of the register file on the stack.
  Node* RegisterFileRawPointer();
  // Returns a tagged pointer to the current function's BytecodeArray object.
  Node* BytecodeArrayTaggedPointer();
  // Returns the offset from the BytecodeArrayPointer of the current bytecode.
  Node* BytecodeOffset();
  // Returns a raw pointer to first entry in the interpreter dispatch table.
  Node* DispatchTableRawPointer();

  // Saves and restores interpreter bytecode offset to the interpreter stack
  // frame when performing a call.
  void CallPrologue();
  void CallEpilogue();

  // Returns the offset of register |index| relative to RegisterFilePointer().
  Node* RegisterFrameOffset(Node* index);

  Node* SmiShiftBitsConstant();
  Node* BytecodeOperand(int operand_index);
  Node* BytecodeOperandSignExtended(int operand_index);
  Node* BytecodeOperandShort(int operand_index);
  Node* BytecodeOperandShortSignExtended(int operand_index);

  Node* CallN(CallDescriptor* descriptor, Node* code_target, Node** args);
  Node* CallIC(CallInterfaceDescriptor descriptor, Node* target, Node** args);

  // Returns BytecodeOffset() advanced by delta bytecodes. Note: this does not
  // update BytecodeOffset() itself.
  Node* Advance(int delta);
  Node* Advance(Node* delta);

  // Starts next instruction dispatch at |new_bytecode_offset|.
  void DispatchTo(Node* new_bytecode_offset);

  // Abort operations for debug code.
  void AbortIfWordNotEqual(Node* lhs, Node* rhs, BailoutReason bailout_reason);

  // Private helpers which delegate to RawMachineAssembler.
  Isolate* isolate();
  Zone* zone();

  interpreter::Bytecode bytecode_;
  base::SmartPointer<RawMachineAssembler> raw_assembler_;

  Node* accumulator_;
  Node* bytecode_offset_;
  Node* context_;

  bool code_generated_;

  DISALLOW_COPY_AND_ASSIGN(InterpreterAssembler);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INTERPRETER_ASSEMBLER_H_
