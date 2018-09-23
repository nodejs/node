// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/interpreter-assembler.h"

#include <ostream>

#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-type.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/macro-assembler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {


InterpreterAssembler::InterpreterAssembler(Isolate* isolate, Zone* zone,
                                           interpreter::Bytecode bytecode)
    : bytecode_(bytecode),
      raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone),
          Linkage::GetInterpreterDispatchDescriptor(zone), kMachPtr,
          InstructionSelector::SupportedMachineOperatorFlags())),
      end_node_(nullptr),
      accumulator_(
          raw_assembler_->Parameter(Linkage::kInterpreterAccumulatorParameter)),
      code_generated_(false) {}


InterpreterAssembler::~InterpreterAssembler() {}


Handle<Code> InterpreterAssembler::GenerateCode() {
  DCHECK(!code_generated_);

  End();

  Schedule* schedule = raw_assembler_->Export();
  // TODO(rmcilroy): use a non-testing code generator.
  Handle<Code> code = Pipeline::GenerateCodeForTesting(
      isolate(), raw_assembler_->call_descriptor(), graph(), schedule);

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    OFStream os(stdout);
    code->Disassemble(interpreter::Bytecodes::ToString(bytecode_), os);
    os << std::flush;
  }
#endif

  code_generated_ = true;
  return code;
}


Node* InterpreterAssembler::GetAccumulator() {
  return accumulator_;
}


void InterpreterAssembler::SetAccumulator(Node* value) {
  accumulator_ = value;
}


Node* InterpreterAssembler::RegisterFileRawPointer() {
  return raw_assembler_->Parameter(Linkage::kInterpreterRegisterFileParameter);
}


Node* InterpreterAssembler::BytecodeArrayTaggedPointer() {
  return raw_assembler_->Parameter(Linkage::kInterpreterBytecodeArrayParameter);
}


Node* InterpreterAssembler::BytecodeOffset() {
  return raw_assembler_->Parameter(
      Linkage::kInterpreterBytecodeOffsetParameter);
}


Node* InterpreterAssembler::DispatchTableRawPointer() {
  return raw_assembler_->Parameter(Linkage::kInterpreterDispatchTableParameter);
}


Node* InterpreterAssembler::RegisterFrameOffset(Node* index) {
  return raw_assembler_->WordShl(index, Int32Constant(kPointerSizeLog2));
}


Node* InterpreterAssembler::LoadRegister(Node* reg_index) {
  return raw_assembler_->Load(kMachPtr, RegisterFileRawPointer(),
                              RegisterFrameOffset(reg_index));
}


Node* InterpreterAssembler::StoreRegister(Node* value, Node* reg_index) {
  return raw_assembler_->Store(kMachPtr, RegisterFileRawPointer(),
                               RegisterFrameOffset(reg_index), value);
}


Node* InterpreterAssembler::BytecodeOperand(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  return raw_assembler_->Load(
      kMachUint8, BytecodeArrayTaggedPointer(),
      raw_assembler_->IntPtrAdd(BytecodeOffset(),
                                Int32Constant(1 + operand_index)));
}


Node* InterpreterAssembler::BytecodeOperandSignExtended(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  Node* load = raw_assembler_->Load(
      kMachInt8, BytecodeArrayTaggedPointer(),
      raw_assembler_->IntPtrAdd(BytecodeOffset(),
                                Int32Constant(1 + operand_index)));
  // Ensure that we sign extend to full pointer size
  if (kPointerSize == 8) {
    load = raw_assembler_->ChangeInt32ToInt64(load);
  }
  return load;
}


Node* InterpreterAssembler::BytecodeOperandImm8(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kImm8,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperandSignExtended(operand_index);
}


Node* InterpreterAssembler::BytecodeOperandReg(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kReg,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperandSignExtended(operand_index);
}


Node* InterpreterAssembler::Int32Constant(int value) {
  return raw_assembler_->Int32Constant(value);
}


Node* InterpreterAssembler::NumberConstant(double value) {
  return raw_assembler_->NumberConstant(value);
}


Node* InterpreterAssembler::HeapConstant(Unique<HeapObject> object) {
  return raw_assembler_->HeapConstant(object);
}


Node* InterpreterAssembler::SmiShiftBitsConstant() {
  return Int32Constant(kSmiShiftSize + kSmiTagSize);
}


Node* InterpreterAssembler::SmiTag(Node* value) {
  return raw_assembler_->WordShl(value, SmiShiftBitsConstant());
}


Node* InterpreterAssembler::SmiUntag(Node* value) {
  return raw_assembler_->WordSar(value, SmiShiftBitsConstant());
}


void InterpreterAssembler::Return() {
  Node* exit_trampoline_code_object =
      HeapConstant(Unique<HeapObject>::CreateImmovable(
          isolate()->builtins()->InterpreterExitTrampoline()));
  // If the order of the parameters you need to change the call signature below.
  STATIC_ASSERT(0 == Linkage::kInterpreterAccumulatorParameter);
  STATIC_ASSERT(1 == Linkage::kInterpreterRegisterFileParameter);
  STATIC_ASSERT(2 == Linkage::kInterpreterBytecodeOffsetParameter);
  STATIC_ASSERT(3 == Linkage::kInterpreterBytecodeArrayParameter);
  STATIC_ASSERT(4 == Linkage::kInterpreterDispatchTableParameter);
  Node* tail_call = raw_assembler_->TailCallInterpreterDispatch(
      call_descriptor(), exit_trampoline_code_object, GetAccumulator(),
      RegisterFileRawPointer(), BytecodeOffset(), BytecodeArrayTaggedPointer(),
      DispatchTableRawPointer());
  // This should always be the end node.
  SetEndInput(tail_call);
}


Node* InterpreterAssembler::Advance(int delta) {
  return raw_assembler_->IntPtrAdd(BytecodeOffset(), Int32Constant(delta));
}


void InterpreterAssembler::Dispatch() {
  Node* new_bytecode_offset = Advance(interpreter::Bytecodes::Size(bytecode_));
  Node* target_bytecode = raw_assembler_->Load(
      kMachUint8, BytecodeArrayTaggedPointer(), new_bytecode_offset);

  // TODO(rmcilroy): Create a code target dispatch table to avoid conversion
  // from code object on every dispatch.
  Node* target_code_object = raw_assembler_->Load(
      kMachPtr, DispatchTableRawPointer(),
      raw_assembler_->Word32Shl(target_bytecode,
                                Int32Constant(kPointerSizeLog2)));

  // If the order of the parameters you need to change the call signature below.
  STATIC_ASSERT(0 == Linkage::kInterpreterAccumulatorParameter);
  STATIC_ASSERT(1 == Linkage::kInterpreterRegisterFileParameter);
  STATIC_ASSERT(2 == Linkage::kInterpreterBytecodeOffsetParameter);
  STATIC_ASSERT(3 == Linkage::kInterpreterBytecodeArrayParameter);
  STATIC_ASSERT(4 == Linkage::kInterpreterDispatchTableParameter);
  Node* tail_call = raw_assembler_->TailCallInterpreterDispatch(
      call_descriptor(), target_code_object, GetAccumulator(),
      RegisterFileRawPointer(), new_bytecode_offset,
      BytecodeArrayTaggedPointer(), DispatchTableRawPointer());
  // This should always be the end node.
  SetEndInput(tail_call);
}


void InterpreterAssembler::SetEndInput(Node* input) {
  DCHECK(!end_node_);
  end_node_ = input;
}


void InterpreterAssembler::End() {
  DCHECK(end_node_);
  // TODO(rmcilroy): Support more than 1 end input.
  Node* end = graph()->NewNode(raw_assembler_->common()->End(1), end_node_);
  graph()->SetEnd(end);
}


// RawMachineAssembler delegate helpers:
Isolate* InterpreterAssembler::isolate() { return raw_assembler_->isolate(); }


Graph* InterpreterAssembler::graph() { return raw_assembler_->graph(); }


CallDescriptor* InterpreterAssembler::call_descriptor() const {
  return raw_assembler_->call_descriptor();
}


Schedule* InterpreterAssembler::schedule() {
  return raw_assembler_->schedule();
}



}  // namespace interpreter
}  // namespace internal
}  // namespace v8
