// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-assembler.h"

#include <limits>
#include <ostream>

#include "src/code-factory.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/machine-type.h"
#include "src/macro-assembler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;

InterpreterAssembler::InterpreterAssembler(Isolate* isolate, Zone* zone,
                                           Bytecode bytecode,
                                           OperandScale operand_scale)
    : CodeStubAssembler(isolate, zone, InterpreterDispatchDescriptor(isolate),
                        Code::ComputeFlags(Code::BYTECODE_HANDLER),
                        Bytecodes::ToString(bytecode),
                        Bytecodes::ReturnCount(bytecode)),
      bytecode_(bytecode),
      operand_scale_(operand_scale),
      accumulator_(this, MachineRepresentation::kTagged),
      accumulator_use_(AccumulatorUse::kNone),
      made_call_(false),
      disable_stack_check_across_call_(false),
      stack_pointer_before_call_(nullptr) {
  accumulator_.Bind(
      Parameter(InterpreterDispatchDescriptor::kAccumulatorParameter));
  if (FLAG_trace_ignition) {
    TraceBytecode(Runtime::kInterpreterTraceBytecodeEntry);
  }
}

InterpreterAssembler::~InterpreterAssembler() {
  // If the following check fails the handler does not use the
  // accumulator in the way described in the bytecode definitions in
  // bytecodes.h.
  DCHECK_EQ(accumulator_use_, Bytecodes::GetAccumulatorUse(bytecode_));
}

Node* InterpreterAssembler::GetAccumulatorUnchecked() {
  return accumulator_.value();
}

Node* InterpreterAssembler::GetAccumulator() {
  DCHECK(Bytecodes::ReadsAccumulator(bytecode_));
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kRead;
  return GetAccumulatorUnchecked();
}

void InterpreterAssembler::SetAccumulator(Node* value) {
  DCHECK(Bytecodes::WritesAccumulator(bytecode_));
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kWrite;
  accumulator_.Bind(value);
}

Node* InterpreterAssembler::GetContext() {
  return LoadRegister(Register::current_context());
}

void InterpreterAssembler::SetContext(Node* value) {
  StoreRegister(value, Register::current_context());
}

Node* InterpreterAssembler::BytecodeOffset() {
  return Parameter(InterpreterDispatchDescriptor::kBytecodeOffsetParameter);
}

Node* InterpreterAssembler::BytecodeArrayTaggedPointer() {
  if (made_call_) {
    // If we have made a call, restore bytecode array from stack frame in case
    // the debugger has swapped us to the patched debugger bytecode array.
    return LoadRegister(Register::bytecode_array());
  } else {
    return Parameter(InterpreterDispatchDescriptor::kBytecodeArrayParameter);
  }
}

Node* InterpreterAssembler::DispatchTableRawPointer() {
  return Parameter(InterpreterDispatchDescriptor::kDispatchTableParameter);
}

Node* InterpreterAssembler::RegisterLocation(Node* reg_index) {
  return IntPtrAdd(LoadParentFramePointer(), RegisterFrameOffset(reg_index));
}

Node* InterpreterAssembler::RegisterFrameOffset(Node* index) {
  return WordShl(index, kPointerSizeLog2);
}

Node* InterpreterAssembler::LoadRegister(Register reg) {
  return Load(MachineType::AnyTagged(), LoadParentFramePointer(),
              IntPtrConstant(reg.ToOperand() << kPointerSizeLog2));
}

Node* InterpreterAssembler::LoadRegister(Node* reg_index) {
  return Load(MachineType::AnyTagged(), LoadParentFramePointer(),
              RegisterFrameOffset(reg_index));
}

Node* InterpreterAssembler::StoreRegister(Node* value, Register reg) {
  return StoreNoWriteBarrier(
      MachineRepresentation::kTagged, LoadParentFramePointer(),
      IntPtrConstant(reg.ToOperand() << kPointerSizeLog2), value);
}

Node* InterpreterAssembler::StoreRegister(Node* value, Node* reg_index) {
  return StoreNoWriteBarrier(MachineRepresentation::kTagged,
                             LoadParentFramePointer(),
                             RegisterFrameOffset(reg_index), value);
}

Node* InterpreterAssembler::NextRegister(Node* reg_index) {
  // Register indexes are negative, so the next index is minus one.
  return IntPtrAdd(reg_index, IntPtrConstant(-1));
}

Node* InterpreterAssembler::OperandOffset(int operand_index) {
  return IntPtrConstant(
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale()));
}

Node* InterpreterAssembler::BytecodeOperandUnsignedByte(int operand_index) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kByte, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  Node* operand_offset = OperandOffset(operand_index);
  return Load(MachineType::Uint8(), BytecodeArrayTaggedPointer(),
              IntPtrAdd(BytecodeOffset(), operand_offset));
}

Node* InterpreterAssembler::BytecodeOperandSignedByte(int operand_index) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kByte, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  Node* operand_offset = OperandOffset(operand_index);
  Node* load = Load(MachineType::Int8(), BytecodeArrayTaggedPointer(),
                    IntPtrAdd(BytecodeOffset(), operand_offset));

  // Ensure that we sign extend to full pointer size
  if (kPointerSize == 8) {
    load = ChangeInt32ToInt64(load);
  }
  return load;
}

compiler::Node* InterpreterAssembler::BytecodeOperandReadUnaligned(
    int relative_offset, MachineType result_type) {
  static const int kMaxCount = 4;
  DCHECK(!TargetSupportsUnalignedAccess());

  int count;
  switch (result_type.representation()) {
    case MachineRepresentation::kWord16:
      count = 2;
      break;
    case MachineRepresentation::kWord32:
      count = 4;
      break;
    default:
      UNREACHABLE();
      break;
  }
  MachineType msb_type =
      result_type.IsSigned() ? MachineType::Int8() : MachineType::Uint8();

#if V8_TARGET_LITTLE_ENDIAN
  const int kStep = -1;
  int msb_offset = count - 1;
#elif V8_TARGET_BIG_ENDIAN
  const int kStep = 1;
  int msb_offset = 0;
#else
#error "Unknown Architecture"
#endif

  // Read the most signicant bytecode into bytes[0] and then in order
  // down to least significant in bytes[count - 1].
  DCHECK(count <= kMaxCount);
  compiler::Node* bytes[kMaxCount];
  for (int i = 0; i < count; i++) {
    MachineType machine_type = (i == 0) ? msb_type : MachineType::Uint8();
    Node* offset = IntPtrConstant(relative_offset + msb_offset + i * kStep);
    Node* array_offset = IntPtrAdd(BytecodeOffset(), offset);
    bytes[i] = Load(machine_type, BytecodeArrayTaggedPointer(), array_offset);
  }

  // Pack LSB to MSB.
  Node* result = bytes[--count];
  for (int i = 1; --count >= 0; i++) {
    Node* shift = Int32Constant(i * kBitsPerByte);
    Node* value = Word32Shl(bytes[count], shift);
    result = Word32Or(value, result);
  }
  return result;
}

Node* InterpreterAssembler::BytecodeOperandUnsignedShort(int operand_index) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(
      OperandSize::kShort,
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  if (TargetSupportsUnalignedAccess()) {
    return Load(MachineType::Uint16(), BytecodeArrayTaggedPointer(),
                IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)));
  } else {
    return BytecodeOperandReadUnaligned(operand_offset, MachineType::Uint16());
  }
}

Node* InterpreterAssembler::BytecodeOperandSignedShort(int operand_index) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(
      OperandSize::kShort,
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  Node* load;
  if (TargetSupportsUnalignedAccess()) {
    load = Load(MachineType::Int16(), BytecodeArrayTaggedPointer(),
                IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)));
  } else {
    load = BytecodeOperandReadUnaligned(operand_offset, MachineType::Int16());
  }

  // Ensure that we sign extend to full pointer size
  if (kPointerSize == 8) {
    load = ChangeInt32ToInt64(load);
  }
  return load;
}

Node* InterpreterAssembler::BytecodeOperandUnsignedQuad(int operand_index) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kQuad, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  if (TargetSupportsUnalignedAccess()) {
    return Load(MachineType::Uint32(), BytecodeArrayTaggedPointer(),
                IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)));
  } else {
    return BytecodeOperandReadUnaligned(operand_offset, MachineType::Uint32());
  }
}

Node* InterpreterAssembler::BytecodeOperandSignedQuad(int operand_index) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kQuad, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  Node* load;
  if (TargetSupportsUnalignedAccess()) {
    load = Load(MachineType::Int32(), BytecodeArrayTaggedPointer(),
                IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)));
  } else {
    load = BytecodeOperandReadUnaligned(operand_offset, MachineType::Int32());
  }

  // Ensure that we sign extend to full pointer size
  if (kPointerSize == 8) {
    load = ChangeInt32ToInt64(load);
  }
  return load;
}

Node* InterpreterAssembler::BytecodeSignedOperand(int operand_index,
                                                  OperandSize operand_size) {
  DCHECK(!Bytecodes::IsUnsignedOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  switch (operand_size) {
    case OperandSize::kByte:
      return BytecodeOperandSignedByte(operand_index);
    case OperandSize::kShort:
      return BytecodeOperandSignedShort(operand_index);
    case OperandSize::kQuad:
      return BytecodeOperandSignedQuad(operand_index);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return nullptr;
}

Node* InterpreterAssembler::BytecodeUnsignedOperand(int operand_index,
                                                    OperandSize operand_size) {
  DCHECK(Bytecodes::IsUnsignedOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  switch (operand_size) {
    case OperandSize::kByte:
      return BytecodeOperandUnsignedByte(operand_index);
    case OperandSize::kShort:
      return BytecodeOperandUnsignedShort(operand_index);
    case OperandSize::kQuad:
      return BytecodeOperandUnsignedQuad(operand_index);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return nullptr;
}

Node* InterpreterAssembler::BytecodeOperandCount(int operand_index) {
  DCHECK_EQ(OperandType::kRegCount,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandFlag(int operand_index) {
  DCHECK_EQ(OperandType::kFlag8,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  DCHECK_EQ(operand_size, OperandSize::kByte);
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandImm(int operand_index) {
  DCHECK_EQ(OperandType::kImm,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeSignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandIdx(int operand_index) {
  DCHECK(OperandType::kIdx ==
         Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandReg(int operand_index) {
  DCHECK(Bytecodes::IsRegisterOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeSignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandRuntimeId(int operand_index) {
  DCHECK(OperandType::kRuntimeId ==
         Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  DCHECK_EQ(operand_size, OperandSize::kShort);
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::LoadConstantPoolEntry(Node* index) {
  Node* constant_pool = LoadObjectField(BytecodeArrayTaggedPointer(),
                                        BytecodeArray::kConstantPoolOffset);
  Node* entry_offset =
      IntPtrAdd(IntPtrConstant(FixedArray::kHeaderSize - kHeapObjectTag),
                WordShl(index, kPointerSizeLog2));
  return Load(MachineType::AnyTagged(), constant_pool, entry_offset);
}

Node* InterpreterAssembler::LoadContextSlot(Node* context, int slot_index) {
  return Load(MachineType::AnyTagged(), context,
              IntPtrConstant(Context::SlotOffset(slot_index)));
}

Node* InterpreterAssembler::LoadContextSlot(Node* context, Node* slot_index) {
  Node* offset =
      IntPtrAdd(WordShl(slot_index, kPointerSizeLog2),
                IntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
  return Load(MachineType::AnyTagged(), context, offset);
}

Node* InterpreterAssembler::StoreContextSlot(Node* context, Node* slot_index,
                                             Node* value) {
  Node* offset =
      IntPtrAdd(WordShl(slot_index, kPointerSizeLog2),
                IntPtrConstant(Context::kHeaderSize - kHeapObjectTag));
  return Store(MachineRepresentation::kTagged, context, offset, value);
}

Node* InterpreterAssembler::LoadTypeFeedbackVector() {
  Node* function = LoadRegister(Register::function_closure());
  Node* shared_info =
      LoadObjectField(function, JSFunction::kSharedFunctionInfoOffset);
  Node* vector =
      LoadObjectField(shared_info, SharedFunctionInfo::kFeedbackVectorOffset);
  return vector;
}

void InterpreterAssembler::CallPrologue() {
  StoreRegister(SmiTag(BytecodeOffset()), Register::bytecode_offset());

  if (FLAG_debug_code && !disable_stack_check_across_call_) {
    DCHECK(stack_pointer_before_call_ == nullptr);
    stack_pointer_before_call_ = LoadStackPointer();
  }
  made_call_ = true;
}

void InterpreterAssembler::CallEpilogue() {
  if (FLAG_debug_code && !disable_stack_check_across_call_) {
    Node* stack_pointer_after_call = LoadStackPointer();
    Node* stack_pointer_before_call = stack_pointer_before_call_;
    stack_pointer_before_call_ = nullptr;
    AbortIfWordNotEqual(stack_pointer_before_call, stack_pointer_after_call,
                        kUnexpectedStackPointer);
  }
}

Node* InterpreterAssembler::CallJS(Node* function, Node* context,
                                   Node* first_arg, Node* arg_count,
                                   TailCallMode tail_call_mode) {
  Callable callable =
      CodeFactory::InterpreterPushArgsAndCall(isolate(), tail_call_mode);
  Node* code_target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), code_target, context, arg_count,
                  first_arg, function);
}

Node* InterpreterAssembler::CallConstruct(Node* constructor, Node* context,
                                          Node* new_target, Node* first_arg,
                                          Node* arg_count) {
  Callable callable = CodeFactory::InterpreterPushArgsAndConstruct(isolate());
  Node* code_target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), code_target, context, arg_count,
                  new_target, constructor, first_arg);
}

Node* InterpreterAssembler::CallRuntimeN(Node* function_id, Node* context,
                                         Node* first_arg, Node* arg_count,
                                         int result_size) {
  Callable callable = CodeFactory::InterpreterCEntry(isolate(), result_size);
  Node* code_target = HeapConstant(callable.code());

  // Get the function entry from the function id.
  Node* function_table = ExternalConstant(
      ExternalReference::runtime_function_table_address(isolate()));
  Node* function_offset =
      Int32Mul(function_id, Int32Constant(sizeof(Runtime::Function)));
  Node* function = IntPtrAdd(function_table, function_offset);
  Node* function_entry =
      Load(MachineType::Pointer(), function,
           IntPtrConstant(offsetof(Runtime::Function, entry)));

  return CallStub(callable.descriptor(), code_target, context, arg_count,
                  first_arg, function_entry, result_size);
}

void InterpreterAssembler::UpdateInterruptBudget(Node* weight) {
  Label ok(this), interrupt_check(this, Label::kDeferred), end(this);
  Node* budget_offset =
      IntPtrConstant(BytecodeArray::kInterruptBudgetOffset - kHeapObjectTag);

  // Update budget by |weight| and check if it reaches zero.
  Variable new_budget(this, MachineRepresentation::kWord32);
  Node* old_budget =
      Load(MachineType::Int32(), BytecodeArrayTaggedPointer(), budget_offset);
  new_budget.Bind(Int32Add(old_budget, weight));
  Node* condition =
      Int32GreaterThanOrEqual(new_budget.value(), Int32Constant(0));
  Branch(condition, &ok, &interrupt_check);

  // Perform interrupt and reset budget.
  Bind(&interrupt_check);
  {
    CallRuntime(Runtime::kInterrupt, GetContext());
    new_budget.Bind(Int32Constant(Interpreter::InterruptBudget()));
    Goto(&ok);
  }

  // Update budget.
  Bind(&ok);
  StoreNoWriteBarrier(MachineRepresentation::kWord32,
                      BytecodeArrayTaggedPointer(), budget_offset,
                      new_budget.value());
}

Node* InterpreterAssembler::Advance(int delta) {
  return IntPtrAdd(BytecodeOffset(), IntPtrConstant(delta));
}

Node* InterpreterAssembler::Advance(Node* delta) {
  return IntPtrAdd(BytecodeOffset(), delta);
}

Node* InterpreterAssembler::Jump(Node* delta) {
  UpdateInterruptBudget(delta);
  return DispatchTo(Advance(delta));
}

void InterpreterAssembler::JumpConditional(Node* condition, Node* delta) {
  Label match(this), no_match(this);

  BranchIf(condition, &match, &no_match);
  Bind(&match);
  Jump(delta);
  Bind(&no_match);
  Dispatch();
}

void InterpreterAssembler::JumpIfWordEqual(Node* lhs, Node* rhs, Node* delta) {
  JumpConditional(WordEqual(lhs, rhs), delta);
}

void InterpreterAssembler::JumpIfWordNotEqual(Node* lhs, Node* rhs,
                                              Node* delta) {
  JumpConditional(WordNotEqual(lhs, rhs), delta);
}

Node* InterpreterAssembler::Dispatch() {
  return DispatchTo(Advance(Bytecodes::Size(bytecode_, operand_scale_)));
}

Node* InterpreterAssembler::DispatchTo(Node* new_bytecode_offset) {
  Node* target_bytecode = Load(
      MachineType::Uint8(), BytecodeArrayTaggedPointer(), new_bytecode_offset);
  if (kPointerSize == 8) {
    target_bytecode = ChangeUint32ToUint64(target_bytecode);
  }

  if (FLAG_trace_ignition_dispatches) {
    TraceBytecodeDispatch(target_bytecode);
  }

  Node* target_code_entry =
      Load(MachineType::Pointer(), DispatchTableRawPointer(),
           WordShl(target_bytecode, IntPtrConstant(kPointerSizeLog2)));

  return DispatchToBytecodeHandlerEntry(target_code_entry, new_bytecode_offset);
}

Node* InterpreterAssembler::DispatchToBytecodeHandler(Node* handler,
                                                      Node* bytecode_offset) {
  Node* handler_entry =
      IntPtrAdd(handler, IntPtrConstant(Code::kHeaderSize - kHeapObjectTag));
  return DispatchToBytecodeHandlerEntry(handler_entry, bytecode_offset);
}

Node* InterpreterAssembler::DispatchToBytecodeHandlerEntry(
    Node* handler_entry, Node* bytecode_offset) {
  if (FLAG_trace_ignition) {
    TraceBytecode(Runtime::kInterpreterTraceBytecodeExit);
  }

  InterpreterDispatchDescriptor descriptor(isolate());
  Node* args[] = {GetAccumulatorUnchecked(), bytecode_offset,
                  BytecodeArrayTaggedPointer(), DispatchTableRawPointer()};
  return TailCallBytecodeDispatch(descriptor, handler_entry, args);
}

void InterpreterAssembler::DispatchWide(OperandScale operand_scale) {
  // Dispatching a wide bytecode requires treating the prefix
  // bytecode a base pointer into the dispatch table and dispatching
  // the bytecode that follows relative to this base.
  //
  //   Indices 0-255 correspond to bytecodes with operand_scale == 0
  //   Indices 256-511 correspond to bytecodes with operand_scale == 1
  //   Indices 512-767 correspond to bytecodes with operand_scale == 2
  Node* next_bytecode_offset = Advance(1);
  Node* next_bytecode = Load(MachineType::Uint8(), BytecodeArrayTaggedPointer(),
                             next_bytecode_offset);
  if (kPointerSize == 8) {
    next_bytecode = ChangeUint32ToUint64(next_bytecode);
  }

  if (FLAG_trace_ignition_dispatches) {
    TraceBytecodeDispatch(next_bytecode);
  }

  Node* base_index;
  switch (operand_scale) {
    case OperandScale::kDouble:
      base_index = IntPtrConstant(1 << kBitsPerByte);
      break;
    case OperandScale::kQuadruple:
      base_index = IntPtrConstant(2 << kBitsPerByte);
      break;
    default:
      UNREACHABLE();
      base_index = nullptr;
  }
  Node* target_index = IntPtrAdd(base_index, next_bytecode);
  Node* target_code_entry =
      Load(MachineType::Pointer(), DispatchTableRawPointer(),
           WordShl(target_index, kPointerSizeLog2));

  DispatchToBytecodeHandlerEntry(target_code_entry, next_bytecode_offset);
}

void InterpreterAssembler::UpdateInterruptBudgetOnReturn() {
  // TODO(rmcilroy): Investigate whether it is worth supporting self
  // optimization of primitive functions like FullCodegen.

  // Update profiling count by -BytecodeOffset to simulate backedge to start of
  // function.
  Node* profiling_weight =
      Int32Sub(Int32Constant(kHeapObjectTag + BytecodeArray::kHeaderSize),
               BytecodeOffset());
  UpdateInterruptBudget(profiling_weight);
}

Node* InterpreterAssembler::StackCheckTriggeredInterrupt() {
  Node* sp = LoadStackPointer();
  Node* stack_limit = Load(
      MachineType::Pointer(),
      ExternalConstant(ExternalReference::address_of_stack_limit(isolate())));
  return UintPtrLessThan(sp, stack_limit);
}

void InterpreterAssembler::Abort(BailoutReason bailout_reason) {
  disable_stack_check_across_call_ = true;
  Node* abort_id = SmiTag(Int32Constant(bailout_reason));
  CallRuntime(Runtime::kAbort, GetContext(), abort_id);
  disable_stack_check_across_call_ = false;
}

void InterpreterAssembler::AbortIfWordNotEqual(Node* lhs, Node* rhs,
                                               BailoutReason bailout_reason) {
  Label ok(this), abort(this, Label::kDeferred);
  BranchIfWordEqual(lhs, rhs, &ok, &abort);

  Bind(&abort);
  Abort(bailout_reason);
  Goto(&ok);

  Bind(&ok);
}

void InterpreterAssembler::TraceBytecode(Runtime::FunctionId function_id) {
  CallRuntime(function_id, GetContext(), BytecodeArrayTaggedPointer(),
              SmiTag(BytecodeOffset()), GetAccumulatorUnchecked());
}

void InterpreterAssembler::TraceBytecodeDispatch(Node* target_bytecode) {
  Node* counters_table = ExternalConstant(
      ExternalReference::interpreter_dispatch_counters(isolate()));
  Node* source_bytecode_table_index = IntPtrConstant(
      static_cast<int>(bytecode_) * (static_cast<int>(Bytecode::kLast) + 1));

  Node* counter_offset =
      WordShl(IntPtrAdd(source_bytecode_table_index, target_bytecode),
              IntPtrConstant(kPointerSizeLog2));
  Node* old_counter =
      Load(MachineType::IntPtr(), counters_table, counter_offset);

  Label counter_ok(this), counter_saturated(this, Label::kDeferred);

  Node* counter_reached_max = WordEqual(
      old_counter, IntPtrConstant(std::numeric_limits<uintptr_t>::max()));
  BranchIf(counter_reached_max, &counter_saturated, &counter_ok);

  Bind(&counter_ok);
  {
    Node* new_counter = IntPtrAdd(old_counter, IntPtrConstant(1));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), counters_table,
                        counter_offset, new_counter);
    Goto(&counter_saturated);
  }

  Bind(&counter_saturated);
}

// static
bool InterpreterAssembler::TargetSupportsUnalignedAccess() {
#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
  return false;
#elif V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_PPC
  return CpuFeatures::IsSupported(UNALIGNED_ACCESSES);
#elif V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_X87 || \
    V8_TARGET_ARCH_S390
  return true;
#else
#error "Unknown Architecture"
#endif
}

Node* InterpreterAssembler::RegisterCount() {
  Node* bytecode_array = LoadRegister(Register::bytecode_array());
  Node* frame_size = LoadObjectField(
      bytecode_array, BytecodeArray::kFrameSizeOffset, MachineType::Int32());
  return Word32Sar(frame_size, Int32Constant(kPointerSizeLog2));
}

Node* InterpreterAssembler::ExportRegisterFile(Node* array) {
  if (FLAG_debug_code) {
    Node* array_size = SmiUntag(LoadFixedArrayBaseLength(array));
    AbortIfWordNotEqual(
        array_size, RegisterCount(), kInvalidRegisterFileInGenerator);
  }

  Variable var_index(this, MachineRepresentation::kWord32);
  var_index.Bind(Int32Constant(0));

  // Iterate over register file and write values into array.
  // The mapping of register to array index must match that used in
  // BytecodeGraphBuilder::VisitResumeGenerator.
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  Bind(&loop);
  {
    Node* index = var_index.value();
    Node* condition = Int32LessThan(index, RegisterCount());
    GotoUnless(condition, &done_loop);

    Node* reg_index =
        Int32Sub(Int32Constant(Register(0).ToOperand()), index);
    Node* value = LoadRegister(ChangeInt32ToIntPtr(reg_index));

    StoreFixedArrayElement(array, index, value);

    var_index.Bind(Int32Add(index, Int32Constant(1)));
    Goto(&loop);
  }
  Bind(&done_loop);

  return array;
}

Node* InterpreterAssembler::ImportRegisterFile(Node* array) {
  if (FLAG_debug_code) {
    Node* array_size = SmiUntag(LoadFixedArrayBaseLength(array));
    AbortIfWordNotEqual(
        array_size, RegisterCount(), kInvalidRegisterFileInGenerator);
  }

  Variable var_index(this, MachineRepresentation::kWord32);
  var_index.Bind(Int32Constant(0));

  // Iterate over array and write values into register file.  Also erase the
  // array contents to not keep them alive artificially.
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  Bind(&loop);
  {
    Node* index = var_index.value();
    Node* condition = Int32LessThan(index, RegisterCount());
    GotoUnless(condition, &done_loop);

    Node* value = LoadFixedArrayElement(array, index);

    Node* reg_index =
        Int32Sub(Int32Constant(Register(0).ToOperand()), index);
    StoreRegister(value, ChangeInt32ToIntPtr(reg_index));

    StoreFixedArrayElement(array, index, StaleRegisterConstant());

    var_index.Bind(Int32Add(index, Int32Constant(1)));
    Goto(&loop);
  }
  Bind(&done_loop);

  return array;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
