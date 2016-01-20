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
      context_(
          raw_assembler_->Parameter(Linkage::kInterpreterContextParameter)),
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


Node* InterpreterAssembler::GetAccumulator() { return accumulator_; }


void InterpreterAssembler::SetAccumulator(Node* value) { accumulator_ = value; }


Node* InterpreterAssembler::GetContext() { return context_; }


void InterpreterAssembler::SetContext(Node* value) { context_ = value; }


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


Node* InterpreterAssembler::LoadRegister(interpreter::Register reg) {
  return raw_assembler_->Load(
      kMachAnyTagged, RegisterFileRawPointer(),
      RegisterFrameOffset(Int32Constant(reg.ToOperand())));
}


Node* InterpreterAssembler::LoadRegister(Node* reg_index) {
  return raw_assembler_->Load(kMachAnyTagged, RegisterFileRawPointer(),
                              RegisterFrameOffset(reg_index));
}


Node* InterpreterAssembler::StoreRegister(Node* value, Node* reg_index) {
  return raw_assembler_->Store(kMachAnyTagged, RegisterFileRawPointer(),
                               RegisterFrameOffset(reg_index), value,
                               kNoWriteBarrier);
}


Node* InterpreterAssembler::BytecodeOperand(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(interpreter::OperandSize::kByte,
            interpreter::Bytecodes::GetOperandSize(bytecode_, operand_index));
  return raw_assembler_->Load(
      kMachUint8, BytecodeArrayTaggedPointer(),
      IntPtrAdd(BytecodeOffset(),
                Int32Constant(interpreter::Bytecodes::GetOperandOffset(
                    bytecode_, operand_index))));
}


Node* InterpreterAssembler::BytecodeOperandSignExtended(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(interpreter::OperandSize::kByte,
            interpreter::Bytecodes::GetOperandSize(bytecode_, operand_index));
  Node* load = raw_assembler_->Load(
      kMachInt8, BytecodeArrayTaggedPointer(),
      IntPtrAdd(BytecodeOffset(),
                Int32Constant(interpreter::Bytecodes::GetOperandOffset(
                    bytecode_, operand_index))));
  // Ensure that we sign extend to full pointer size
  if (kPointerSize == 8) {
    load = raw_assembler_->ChangeInt32ToInt64(load);
  }
  return load;
}


Node* InterpreterAssembler::BytecodeOperandShort(int operand_index) {
  DCHECK_LT(operand_index, interpreter::Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(interpreter::OperandSize::kShort,
            interpreter::Bytecodes::GetOperandSize(bytecode_, operand_index));
  if (TargetSupportsUnalignedAccess()) {
    return raw_assembler_->Load(
        kMachUint16, BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(),
                  Int32Constant(interpreter::Bytecodes::GetOperandOffset(
                      bytecode_, operand_index))));
  } else {
    int offset =
        interpreter::Bytecodes::GetOperandOffset(bytecode_, operand_index);
    Node* first_byte = raw_assembler_->Load(
        kMachUint8, BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(), Int32Constant(offset)));
    Node* second_byte = raw_assembler_->Load(
        kMachUint8, BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(), Int32Constant(offset + 1)));
#if V8_TARGET_LITTLE_ENDIAN
    return raw_assembler_->WordOr(WordShl(second_byte, kBitsPerByte),
                                  first_byte);
#elif V8_TARGET_BIG_ENDIAN
    return raw_assembler_->WordOr(WordShl(first_byte, kBitsPerByte),
                                  second_byte);
#else
#error "Unknown Architecture"
#endif
  }
}


Node* InterpreterAssembler::BytecodeOperandCount(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kCount8,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperand(operand_index);
}


Node* InterpreterAssembler::BytecodeOperandImm(int operand_index) {
  DCHECK_EQ(interpreter::OperandType::kImm8,
            interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
  return BytecodeOperandSignExtended(operand_index);
}


Node* InterpreterAssembler::BytecodeOperandIdx(int operand_index) {
  switch (interpreter::Bytecodes::GetOperandSize(bytecode_, operand_index)) {
    case interpreter::OperandSize::kByte:
      DCHECK_EQ(
          interpreter::OperandType::kIdx8,
          interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
      return BytecodeOperand(operand_index);
    case interpreter::OperandSize::kShort:
      DCHECK_EQ(
          interpreter::OperandType::kIdx16,
          interpreter::Bytecodes::GetOperandType(bytecode_, operand_index));
      return BytecodeOperandShort(operand_index);
    default:
      UNREACHABLE();
      return nullptr;
  }
}


Node* InterpreterAssembler::BytecodeOperandReg(int operand_index) {
#ifdef DEBUG
  interpreter::OperandType operand_type =
      interpreter::Bytecodes::GetOperandType(bytecode_, operand_index);
  DCHECK(operand_type == interpreter::OperandType::kReg8 ||
         operand_type == interpreter::OperandType::kMaybeReg8);
#endif
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


Node* InterpreterAssembler::LoadFixedArrayElement(Node* fixed_array,
                                                  int index) {
  Node* entry_offset =
      IntPtrAdd(IntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag),
                WordShl(Int32Constant(index), kPointerSizeLog2));
  return raw_assembler_->Load(kMachAnyTagged, fixed_array, entry_offset);
}


Node* InterpreterAssembler::LoadObjectField(Node* object, int offset) {
  return raw_assembler_->Load(kMachAnyTagged, object,
                              IntPtrConstant(offset - kHeapObjectTag));
}


Node* InterpreterAssembler::LoadContextSlot(Node* context, int slot_index) {
  return raw_assembler_->Load(kMachAnyTagged, context,
                              IntPtrConstant(Context::SlotOffset(slot_index)));
}


Node* InterpreterAssembler::LoadContextSlot(Node* context, Node* slot_index) {
  Node* offset =
      IntPtrAdd(WordShl(slot_index, kPointerSizeLog2),
                Int32Constant(Context::kHeaderSize - kHeapObjectTag));
  return raw_assembler_->Load(kMachAnyTagged, context, offset);
}


Node* InterpreterAssembler::StoreContextSlot(Node* context, Node* slot_index,
                                             Node* value) {
  Node* offset =
      IntPtrAdd(WordShl(slot_index, kPointerSizeLog2),
                Int32Constant(Context::kHeaderSize - kHeapObjectTag));
  return raw_assembler_->Store(kMachAnyTagged, context, offset, value,
                               kFullWriteBarrier);
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


Node* InterpreterAssembler::CallConstruct(Node* original_constructor,
                                          Node* constructor, Node* first_arg,
                                          Node* arg_count) {
  Callable callable = CodeFactory::InterpreterPushArgsAndConstruct(isolate());
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, CallDescriptor::kNoFlags);

  Node* code_target = HeapConstant(callable.code());

  Node** args = zone()->NewArray<Node*>(5);
  args[0] = arg_count;
  args[1] = original_constructor;
  args[2] = constructor;
  args[3] = first_arg;
  args[4] = GetContext();

  return CallN(descriptor, code_target, args);
}


Node* InterpreterAssembler::CallN(CallDescriptor* descriptor, Node* code_target,
                                  Node** args) {
  Node* stack_pointer_before_call = nullptr;
  if (FLAG_debug_code) {
    stack_pointer_before_call = raw_assembler_->LoadStackPointer();
  }
  Node* return_val = raw_assembler_->CallN(descriptor, code_target, args);
  if (FLAG_debug_code) {
    Node* stack_pointer_after_call = raw_assembler_->LoadStackPointer();
    AbortIfWordNotEqual(stack_pointer_before_call, stack_pointer_after_call,
                        kUnexpectedStackPointer);
  }
  return return_val;
}


Node* InterpreterAssembler::CallJS(Node* function, Node* first_arg,
                                   Node* arg_count) {
  Callable callable = CodeFactory::InterpreterPushArgsAndCall(isolate());
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, CallDescriptor::kNoFlags);

  Node* code_target = HeapConstant(callable.code());

  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg_count;
  args[1] = first_arg;
  args[2] = function;
  args[3] = GetContext();

  return CallN(descriptor, code_target, args);
}


Node* InterpreterAssembler::CallIC(CallInterfaceDescriptor descriptor,
                                   Node* target, Node** args) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, 0, CallDescriptor::kNoFlags);
  return CallN(call_descriptor, target, args);
}


Node* InterpreterAssembler::CallIC(CallInterfaceDescriptor descriptor,
                                   Node* target, Node* arg1, Node* arg2,
                                   Node* arg3) {
  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = GetContext();
  return CallIC(descriptor, target, args);
}


Node* InterpreterAssembler::CallIC(CallInterfaceDescriptor descriptor,
                                   Node* target, Node* arg1, Node* arg2,
                                   Node* arg3, Node* arg4) {
  Node** args = zone()->NewArray<Node*>(5);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = GetContext();
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
  args[5] = GetContext();
  return CallIC(descriptor, target, args);
}


Node* InterpreterAssembler::CallRuntime(Node* function_id, Node* first_arg,
                                        Node* arg_count) {
  Callable callable = CodeFactory::InterpreterCEntry(isolate());
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), 0, CallDescriptor::kNoFlags);

  Node* code_target = HeapConstant(callable.code());

  // Get the function entry from the function id.
  Node* function_table = raw_assembler_->ExternalConstant(
      ExternalReference::runtime_function_table_address(isolate()));
  Node* function_offset = raw_assembler_->Int32Mul(
      function_id, Int32Constant(sizeof(Runtime::Function)));
  Node* function = IntPtrAdd(function_table, function_offset);
  Node* function_entry = raw_assembler_->Load(
      kMachPtr, function, Int32Constant(offsetof(Runtime::Function, entry)));

  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg_count;
  args[1] = first_arg;
  args[2] = function_entry;
  args[3] = GetContext();

  return CallN(descriptor, code_target, args);
}


Node* InterpreterAssembler::CallRuntime(Runtime::FunctionId function_id,
                                        Node* arg1) {
  return raw_assembler_->CallRuntime1(function_id, arg1, GetContext());
}


Node* InterpreterAssembler::CallRuntime(Runtime::FunctionId function_id,
                                        Node* arg1, Node* arg2) {
  return raw_assembler_->CallRuntime2(function_id, arg1, arg2, GetContext());
}


Node* InterpreterAssembler::CallRuntime(Runtime::FunctionId function_id,
                                        Node* arg1, Node* arg2, Node* arg3,
                                        Node* arg4) {
  return raw_assembler_->CallRuntime4(function_id, arg1, arg2, arg3, arg4,
                                      GetContext());
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
                   GetContext() };
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
                   GetContext() };
  Node* tail_call =
      raw_assembler_->TailCallN(call_descriptor(), target_code_object, args);
  // This should always be the end node.
  AddEndInput(tail_call);
}


void InterpreterAssembler::Abort(BailoutReason bailout_reason) {
  Node* abort_id = SmiTag(Int32Constant(bailout_reason));
  CallRuntime(Runtime::kAbort, abort_id);
  Return();
}


void InterpreterAssembler::AbortIfWordNotEqual(Node* lhs, Node* rhs,
                                               BailoutReason bailout_reason) {
  RawMachineAssembler::Label match, no_match;
  Node* condition = raw_assembler_->WordEqual(lhs, rhs);
  raw_assembler_->Branch(condition, &match, &no_match);
  raw_assembler_->Bind(&no_match);
  Abort(bailout_reason);
  raw_assembler_->Bind(&match);
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


// static
bool InterpreterAssembler::TargetSupportsUnalignedAccess() {
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
  return false;
#elif V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_PPC
  return CpuFeatures::IsSupported(UNALIGNED_ACCESSES);
#elif V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_X87
  return true;
#else
#error "Unknown Architecture"
#endif
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


}  // namespace compiler
}  // namespace internal
}  // namespace v8
