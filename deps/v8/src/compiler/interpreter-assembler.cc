// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/interpreter-assembler.h"

#include <ostream>

#include "src/code-factory.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-type.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
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
      end_nodes_(zone),
      accumulator_(
          raw_assembler_->Parameter(Linkage::kInterpreterAccumulatorParameter)),
      code_generated_(false) {}


InterpreterAssembler::~InterpreterAssembler() {}


Handle<Code> InterpreterAssembler::GenerateCode() {
  DCHECK(!code_generated_);

  End();

  const char* bytecode_name = interpreter::Bytecodes::ToString(bytecode_);
  Schedule* schedule = raw_assembler_->Export();
  // TODO(rmcilroy): use a non-testing code generator.
  Handle<Code> code = Pipeline::GenerateCodeForInterpreter(
      isolate(), raw_assembler_->call_descriptor(), graph(), schedule,
      bytecode_name);

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    OFStream os(stdout);
    code->Disassemble(bytecode_name, os);
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


Node* InterpreterAssembler::ContextTaggedPointer() {
  return raw_assembler_->Parameter(Linkage::kInterpreterContextParameter);
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
  return WordShl(index, kPointerSizeLog2);
}


Node* InterpreterAssembler::RegisterLocation(Node* reg_index) {
  return IntPtrAdd(RegisterFileRawPointer(), RegisterFrameOffset(reg_index));
}


Node* InterpreterAssembler::LoadRegister(Node* reg_index) {
  return raw_assembler_->Load(kMachAnyTagged, RegisterFileRawPointer(),
                              RegisterFrameOffset(reg_index));
}


Node* InterpreterAssembler::StoreRegister(Node* value, Node* reg_index) {
  return raw_assembler_->Store(kMachAnyTagged, RegisterFileRawPointer(),
                               RegisterFrameOffset(reg_index), value);
}


Node* InterpreterAssembler::BytecodeOperand(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  return raw_assembler_->Load(
      kMachUint8, BytecodeArrayTaggedPointer(),
      IntPtrAdd(BytecodeOffset(), Int32Constant(1 + operand_index)));
}


Node* InterpreterAssembler::BytecodeOperandSignExtended(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  Node* load = raw_assembler_->Load(
      kMachInt8, BytecodeArrayTaggedPointer(),
      IntPtrAdd(BytecodeOffset(), Int32Constant(1 + operand_index)));
  // Ensure that we sign extend to full pointer size
  if (kPointerSize == 8) {
    load = raw_assembler_->ChangeInt32ToInt64(load);
  }
  return load;
}


Node* InterpreterAssembler::BytecodeOperandCount(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kCount,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperand(operand_index);
}


Node* InterpreterAssembler::BytecodeOperandImm8(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kImm8,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperandSignExtended(operand_index);
}


Node* InterpreterAssembler::BytecodeOperandIdx(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kIdx,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperand(operand_index);
}


Node* InterpreterAssembler::BytecodeOperandReg(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kReg,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperandSignExtended(operand_index);
}


Node* InterpreterAssembler::Int32Constant(int value) {
  return raw_assembler_->Int32Constant(value);
}


Node* InterpreterAssembler::IntPtrConstant(intptr_t value) {
  return raw_assembler_->IntPtrConstant(value);
}


Node* InterpreterAssembler::NumberConstant(double value) {
  return raw_assembler_->NumberConstant(value);
}


Node* InterpreterAssembler::HeapConstant(Handle<HeapObject> object) {
  return raw_assembler_->HeapConstant(object);
}


Node* InterpreterAssembler::BooleanConstant(bool value) {
  return raw_assembler_->BooleanConstant(value);
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


Node* InterpreterAssembler::IntPtrAdd(Node* a, Node* b) {
  return raw_assembler_->IntPtrAdd(a, b);
}


Node* InterpreterAssembler::IntPtrSub(Node* a, Node* b) {
  return raw_assembler_->IntPtrSub(a, b);
}


Node* InterpreterAssembler::WordShl(Node* value, int shift) {
  return raw_assembler_->WordShl(value, Int32Constant(shift));
}


Node* InterpreterAssembler::LoadConstantPoolEntry(Node* index) {
  Node* constant_pool = LoadObjectField(BytecodeArrayTaggedPointer(),
                                        BytecodeArray::kConstantPoolOffset);
  Node* entry_offset =
      IntPtrAdd(IntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag),
                WordShl(index, kPointerSizeLog2));
  return raw_assembler_->Load(kMachAnyTagged, constant_pool, entry_offset);
}


Node* InterpreterAssembler::LoadObjectField(Node* object, int offset) {
  return raw_assembler_->Load(kMachAnyTagged, object,
                              IntPtrConstant(offset - kHeapObjectTag));
}


Node* InterpreterAssembler::LoadContextSlot(Node* context, int slot_index) {
  return raw_assembler_->Load(kMachAnyTagged, context,
                              IntPtrConstant(Context::SlotOffset(slot_index)));
}


Node* InterpreterAssembler::LoadContextSlot(int slot_index) {
  return LoadContextSlot(ContextTaggedPointer(), slot_index);
}


Node* InterpreterAssembler::LoadTypeFeedbackVector() {
  Node* function = raw_assembler_->Load(
      kMachAnyTagged, RegisterFileRawPointer(),
      IntPtrConstant(InterpreterFrameConstants::kFunctionFromRegisterPointer));
  Node* shared_info =
      LoadObjectField(function, JSFunction::kSharedFunctionInfoOffset);
  Node* vector =
      LoadObjectField(shared_info, SharedFunctionInfo::kFeedbackVectorOffset);
  return vector;
}


Node* InterpreterAssembler::CallJS(Node* function, Node* first_arg,
                                   Node* arg_count) {
  Callable builtin = CodeFactory::PushArgsAndCall(isolate());
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), builtin.descriptor(), 0, CallDescriptor::kNoFlags);

  Node* code_target = HeapConstant(builtin.code());

  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg_count;
  args[1] = first_arg;
  args[2] = function;
  args[3] = ContextTaggedPointer();

  return raw_assembler_->CallN(descriptor, code_target, args);
}


Node* InterpreterAssembler::CallIC(CallInterfaceDescriptor descriptor,
                                   Node* target, Node** args) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, 0, CallDescriptor::kNoFlags);
  return raw_assembler_->CallN(call_descriptor, target, args);
}


Node* InterpreterAssembler::CallIC(CallInterfaceDescriptor descriptor,
                                   Node* target, Node* arg1, Node* arg2,
                                   Node* arg3, Node* arg4) {
  Node** args = zone()->NewArray<Node*>(5);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = ContextTaggedPointer();
  return CallIC(descriptor, target, args);
}


Node* InterpreterAssembler::CallIC(CallInterfaceDescriptor descriptor,
                                   Node* target, Node* arg1, Node* arg2,
                                   Node* arg3, Node* arg4, Node* arg5) {
  Node** args = zone()->NewArray<Node*>(6);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = arg5;
  args[5] = ContextTaggedPointer();
  return CallIC(descriptor, target, args);
}


Node* InterpreterAssembler::CallRuntime(Runtime::FunctionId function_id,
                                        Node* arg1) {
  return raw_assembler_->CallRuntime1(function_id, arg1,
                                      ContextTaggedPointer());
}


Node* InterpreterAssembler::CallRuntime(Runtime::FunctionId function_id,
                                        Node* arg1, Node* arg2) {
  return raw_assembler_->CallRuntime2(function_id, arg1, arg2,
                                      ContextTaggedPointer());
}


void InterpreterAssembler::Return() {
  Node* exit_trampoline_code_object =
      HeapConstant(isolate()->builtins()->InterpreterExitTrampoline());
  // If the order of the parameters you need to change the call signature below.
  STATIC_ASSERT(0 == Linkage::kInterpreterAccumulatorParameter);
  STATIC_ASSERT(1 == Linkage::kInterpreterRegisterFileParameter);
  STATIC_ASSERT(2 == Linkage::kInterpreterBytecodeOffsetParameter);
  STATIC_ASSERT(3 == Linkage::kInterpreterBytecodeArrayParameter);
  STATIC_ASSERT(4 == Linkage::kInterpreterDispatchTableParameter);
  STATIC_ASSERT(5 == Linkage::kInterpreterContextParameter);
  Node* args[] = { GetAccumulator(),
                   RegisterFileRawPointer(),
                   BytecodeOffset(),
                   BytecodeArrayTaggedPointer(),
                   DispatchTableRawPointer(),
                   ContextTaggedPointer() };
  Node* tail_call = raw_assembler_->TailCallN(
      call_descriptor(), exit_trampoline_code_object, args);
  // This should always be the end node.
  AddEndInput(tail_call);
}


Node* InterpreterAssembler::Advance(int delta) {
  return IntPtrAdd(BytecodeOffset(), Int32Constant(delta));
}


Node* InterpreterAssembler::Advance(Node* delta) {
  return raw_assembler_->IntPtrAdd(BytecodeOffset(), delta);
}


void InterpreterAssembler::Jump(Node* delta) { DispatchTo(Advance(delta)); }


void InterpreterAssembler::JumpIfWordEqual(Node* lhs, Node* rhs, Node* delta) {
  RawMachineAssembler::Label match, no_match;
  Node* condition = raw_assembler_->WordEqual(lhs, rhs);
  raw_assembler_->Branch(condition, &match, &no_match);
  raw_assembler_->Bind(&match);
  DispatchTo(Advance(delta));
  raw_assembler_->Bind(&no_match);
  Dispatch();
}


void InterpreterAssembler::Dispatch() {
  DispatchTo(Advance(interpreter::Bytecodes::Size(bytecode_)));
}


void InterpreterAssembler::DispatchTo(Node* new_bytecode_offset) {
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
  STATIC_ASSERT(5 == Linkage::kInterpreterContextParameter);
  Node* args[] = { GetAccumulator(),
                   RegisterFileRawPointer(),
                   new_bytecode_offset,
                   BytecodeArrayTaggedPointer(),
                   DispatchTableRawPointer(),
                   ContextTaggedPointer() };
  Node* tail_call =
      raw_assembler_->TailCallN(call_descriptor(), target_code_object, args);
  // This should always be the end node.
  AddEndInput(tail_call);
}


void InterpreterAssembler::AddEndInput(Node* input) {
  DCHECK_NOT_NULL(input);
  end_nodes_.push_back(input);
}


void InterpreterAssembler::End() {
  DCHECK(!end_nodes_.empty());
  int end_count = static_cast<int>(end_nodes_.size());
  Node* end = graph()->NewNode(raw_assembler_->common()->End(end_count),
                               end_count, &end_nodes_[0]);
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


Zone* InterpreterAssembler::zone() { return raw_assembler_->zone(); }


}  // namespace interpreter
}  // namespace internal
}  // namespace v8
