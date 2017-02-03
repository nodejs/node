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
#include "src/zone/zone.h"

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
      bytecode_offset_(this, MachineType::PointerRepresentation()),
      interpreted_frame_pointer_(this, MachineType::PointerRepresentation()),
      accumulator_(this, MachineRepresentation::kTagged),
      accumulator_use_(AccumulatorUse::kNone),
      made_call_(false),
      disable_stack_check_across_call_(false),
      stack_pointer_before_call_(nullptr) {
  accumulator_.Bind(Parameter(InterpreterDispatchDescriptor::kAccumulator));
  bytecode_offset_.Bind(
      Parameter(InterpreterDispatchDescriptor::kBytecodeOffset));
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

Node* InterpreterAssembler::GetInterpretedFramePointer() {
  if (!interpreted_frame_pointer_.IsBound()) {
    interpreted_frame_pointer_.Bind(LoadParentFramePointer());
  }
  return interpreted_frame_pointer_.value();
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

Node* InterpreterAssembler::GetContextAtDepth(Node* context, Node* depth) {
  Variable cur_context(this, MachineRepresentation::kTaggedPointer);
  cur_context.Bind(context);

  Variable cur_depth(this, MachineRepresentation::kWord32);
  cur_depth.Bind(depth);

  Label context_found(this);

  Variable* context_search_loop_variables[2] = {&cur_depth, &cur_context};
  Label context_search(this, 2, context_search_loop_variables);

  // Fast path if the depth is 0.
  BranchIfWord32Equal(depth, Int32Constant(0), &context_found, &context_search);

  // Loop until the depth is 0.
  Bind(&context_search);
  {
    cur_depth.Bind(Int32Sub(cur_depth.value(), Int32Constant(1)));
    cur_context.Bind(
        LoadContextSlot(cur_context.value(), Context::PREVIOUS_INDEX));

    BranchIfWord32Equal(cur_depth.value(), Int32Constant(0), &context_found,
                        &context_search);
  }

  Bind(&context_found);
  return cur_context.value();
}

void InterpreterAssembler::GotoIfHasContextExtensionUpToDepth(Node* context,
                                                              Node* depth,
                                                              Label* target) {
  Variable cur_context(this, MachineRepresentation::kTaggedPointer);
  cur_context.Bind(context);

  Variable cur_depth(this, MachineRepresentation::kWord32);
  cur_depth.Bind(depth);

  Variable* context_search_loop_variables[2] = {&cur_depth, &cur_context};
  Label context_search(this, 2, context_search_loop_variables);

  // Loop until the depth is 0.
  Goto(&context_search);
  Bind(&context_search);
  {
    // TODO(leszeks): We only need to do this check if the context had a sloppy
    // eval, we could pass in a context chain bitmask to figure out which
    // contexts actually need to be checked.

    Node* extension_slot =
        LoadContextSlot(cur_context.value(), Context::EXTENSION_INDEX);

    // Jump to the target if the extension slot is not a hole.
    GotoIf(WordNotEqual(extension_slot, TheHoleConstant()), target);

    cur_depth.Bind(Int32Sub(cur_depth.value(), Int32Constant(1)));
    cur_context.Bind(
        LoadContextSlot(cur_context.value(), Context::PREVIOUS_INDEX));

    GotoIf(Word32NotEqual(cur_depth.value(), Int32Constant(0)),
           &context_search);
  }
}

Node* InterpreterAssembler::BytecodeOffset() {
  return bytecode_offset_.value();
}

Node* InterpreterAssembler::BytecodeArrayTaggedPointer() {
  if (made_call_) {
    // If we have made a call, restore bytecode array from stack frame in case
    // the debugger has swapped us to the patched debugger bytecode array.
    return LoadRegister(Register::bytecode_array());
  } else {
    return Parameter(InterpreterDispatchDescriptor::kBytecodeArray);
  }
}

Node* InterpreterAssembler::DispatchTableRawPointer() {
  return Parameter(InterpreterDispatchDescriptor::kDispatchTable);
}

Node* InterpreterAssembler::RegisterLocation(Node* reg_index) {
  return IntPtrAdd(GetInterpretedFramePointer(),
                   RegisterFrameOffset(reg_index));
}

Node* InterpreterAssembler::RegisterFrameOffset(Node* index) {
  return WordShl(index, kPointerSizeLog2);
}

Node* InterpreterAssembler::LoadRegister(Register reg) {
  return Load(MachineType::AnyTagged(), GetInterpretedFramePointer(),
              IntPtrConstant(reg.ToOperand() << kPointerSizeLog2));
}

Node* InterpreterAssembler::LoadRegister(Node* reg_index) {
  return Load(MachineType::AnyTagged(), GetInterpretedFramePointer(),
              RegisterFrameOffset(reg_index));
}

Node* InterpreterAssembler::StoreRegister(Node* value, Register reg) {
  return StoreNoWriteBarrier(
      MachineRepresentation::kTagged, GetInterpretedFramePointer(),
      IntPtrConstant(reg.ToOperand() << kPointerSizeLog2), value);
}

Node* InterpreterAssembler::StoreRegister(Node* value, Node* reg_index) {
  return StoreNoWriteBarrier(MachineRepresentation::kTagged,
                             GetInterpretedFramePointer(),
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

Node* InterpreterAssembler::BytecodeOperandUImm(int operand_index) {
  DCHECK_EQ(OperandType::kUImm,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
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

Node* InterpreterAssembler::BytecodeOperandIntrinsicId(int operand_index) {
  DCHECK(OperandType::kIntrinsicId ==
         Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  DCHECK_EQ(operand_size, OperandSize::kByte);
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

Node* InterpreterAssembler::LoadAndUntagConstantPoolEntry(Node* index) {
  Node* constant_pool = LoadObjectField(BytecodeArrayTaggedPointer(),
                                        BytecodeArray::kConstantPoolOffset);
  int offset = FixedArray::kHeaderSize - kHeapObjectTag;
#if V8_TARGET_LITTLE_ENDIAN
  if (Is64()) {
    offset += kPointerSize / 2;
  }
#endif
  Node* entry_offset =
      IntPtrAdd(IntPtrConstant(offset), WordShl(index, kPointerSizeLog2));
  if (Is64()) {
    return ChangeInt32ToInt64(
        Load(MachineType::Int32(), constant_pool, entry_offset));
  } else {
    return SmiUntag(
        Load(MachineType::AnyTagged(), constant_pool, entry_offset));
  }
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
  Node* literals = LoadObjectField(function, JSFunction::kLiteralsOffset);
  Node* vector =
      LoadObjectField(literals, LiteralsArray::kFeedbackVectorOffset);
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

Node* InterpreterAssembler::IncrementCallCount(Node* type_feedback_vector,
                                               Node* slot_id) {
  Comment("increment call count");
  Node* call_count_slot = IntPtrAdd(slot_id, IntPtrConstant(1));
  Node* call_count =
      LoadFixedArrayElement(type_feedback_vector, call_count_slot);
  Node* new_count = SmiAdd(call_count, SmiTag(Int32Constant(1)));
  // Count is Smi, so we don't need a write barrier.
  return StoreFixedArrayElement(type_feedback_vector, call_count_slot,
                                new_count, SKIP_WRITE_BARRIER);
}

Node* InterpreterAssembler::CallJSWithFeedback(Node* function, Node* context,
                                               Node* first_arg, Node* arg_count,
                                               Node* slot_id,
                                               Node* type_feedback_vector,
                                               TailCallMode tail_call_mode) {
  // Static checks to assert it is safe to examine the type feedback element.
  // We don't know that we have a weak cell. We might have a private symbol
  // or an AllocationSite, but the memory is safe to examine.
  // AllocationSite::kTransitionInfoOffset - contains a Smi or pointer to
  // FixedArray.
  // WeakCell::kValueOffset - contains a JSFunction or Smi(0)
  // Symbol::kHashFieldSlot - if the low bit is 1, then the hash is not
  // computed, meaning that it can't appear to be a pointer. If the low bit is
  // 0, then hash is computed, but the 0 bit prevents the field from appearing
  // to be a pointer.
  STATIC_ASSERT(WeakCell::kSize >= kPointerSize);
  STATIC_ASSERT(AllocationSite::kTransitionInfoOffset ==
                    WeakCell::kValueOffset &&
                WeakCell::kValueOffset == Symbol::kHashFieldSlot);

  Variable return_value(this, MachineRepresentation::kTagged);
  Label handle_monomorphic(this), extra_checks(this), end(this), call(this),
      call_function(this), call_without_feedback(this);

  // Slot id of 0 is used to indicate no typefeedback is available. Call using
  // call builtin.
  STATIC_ASSERT(TypeFeedbackVector::kReservedIndexCount > 0);
  Node* is_feedback_unavailable = Word32Equal(slot_id, Int32Constant(0));
  GotoIf(is_feedback_unavailable, &call_without_feedback);

  // The checks. First, does function match the recorded monomorphic target?
  Node* feedback_element = LoadFixedArrayElement(type_feedback_vector, slot_id);
  Node* feedback_value = LoadWeakCellValue(feedback_element);
  Node* is_monomorphic = WordEqual(function, feedback_value);
  BranchIf(is_monomorphic, &handle_monomorphic, &extra_checks);

  Bind(&handle_monomorphic);
  {
    // The compare above could have been a SMI/SMI comparison. Guard against
    // this convincing us that we have a monomorphic JSFunction.
    Node* is_smi = WordIsSmi(function);
    GotoIf(is_smi, &extra_checks);

    // Increment the call count.
    IncrementCallCount(type_feedback_vector, slot_id);

    // Call using call function builtin.
    Callable callable = CodeFactory::InterpreterPushArgsAndCall(
        isolate(), tail_call_mode, CallableType::kJSFunction);
    Node* code_target = HeapConstant(callable.code());
    Node* ret_value = CallStub(callable.descriptor(), code_target, context,
                               arg_count, first_arg, function);
    return_value.Bind(ret_value);
    Goto(&end);
  }

  Bind(&extra_checks);
  {
    Label check_initialized(this, Label::kDeferred), mark_megamorphic(this),
        check_allocation_site(this),
        create_allocation_site(this, Label::kDeferred);
    // Check if it is a megamorphic target
    Node* is_megamorphic = WordEqual(
        feedback_element,
        HeapConstant(TypeFeedbackVector::MegamorphicSentinel(isolate())));
    BranchIf(is_megamorphic, &call, &check_allocation_site);

    Bind(&check_allocation_site);
    {
      Node* is_allocation_site =
          WordEqual(LoadMap(feedback_element),
                    LoadRoot(Heap::kAllocationSiteMapRootIndex));
      GotoUnless(is_allocation_site, &check_initialized);

      // If it is not the Array() function, mark megamorphic.
      Node* context_slot =
          LoadFixedArrayElement(LoadNativeContext(context),
                                Int32Constant(Context::ARRAY_FUNCTION_INDEX));
      Node* is_array_function = WordEqual(context_slot, function);
      GotoUnless(is_array_function, &mark_megamorphic);

      // It is a monomorphic Array function. Increment the call count.
      IncrementCallCount(type_feedback_vector, slot_id);

      // Call ArrayConstructorStub.
      Callable callable_call =
          CodeFactory::InterpreterPushArgsAndConstructArray(isolate());
      Node* code_target_call = HeapConstant(callable_call.code());
      Node* ret_value =
          CallStub(callable_call.descriptor(), code_target_call, context,
                   arg_count, function, feedback_element, first_arg);
      return_value.Bind(ret_value);
      Goto(&end);
    }

    Bind(&check_initialized);
    {
      Label possibly_monomorphic(this);
      // Check if it is uninitialized.
      Node* is_uninitialized = WordEqual(
          feedback_element,
          HeapConstant(TypeFeedbackVector::UninitializedSentinel(isolate())));
      GotoUnless(is_uninitialized, &mark_megamorphic);

      Node* is_smi = WordIsSmi(function);
      GotoIf(is_smi, &mark_megamorphic);

      // Check if function is an object of JSFunction type
      Node* instance_type = LoadInstanceType(function);
      Node* is_js_function =
          WordEqual(instance_type, Int32Constant(JS_FUNCTION_TYPE));
      GotoUnless(is_js_function, &mark_megamorphic);

      // Check if it is the Array() function.
      Node* context_slot =
          LoadFixedArrayElement(LoadNativeContext(context),
                                Int32Constant(Context::ARRAY_FUNCTION_INDEX));
      Node* is_array_function = WordEqual(context_slot, function);
      GotoIf(is_array_function, &create_allocation_site);

      // Check if the function belongs to the same native context
      Node* native_context = LoadNativeContext(
          LoadObjectField(function, JSFunction::kContextOffset));
      Node* is_same_native_context =
          WordEqual(native_context, LoadNativeContext(context));
      GotoUnless(is_same_native_context, &mark_megamorphic);

      CreateWeakCellInFeedbackVector(type_feedback_vector, SmiTag(slot_id),
                                     function);

      // Call using call function builtin.
      Goto(&call_function);
    }

    Bind(&create_allocation_site);
    {
      CreateAllocationSiteInFeedbackVector(type_feedback_vector,
                                           SmiTag(slot_id));

      // Call using CallFunction builtin. CallICs have a PREMONOMORPHIC state.
      // They start collecting feedback only when a call is executed the second
      // time. So, do not pass any feedback here.
      Goto(&call_function);
    }

    Bind(&mark_megamorphic);
    {
      // Mark it as a megamorphic.
      // MegamorphicSentinel is created as a part of Heap::InitialObjects
      // and will not move during a GC. So it is safe to skip write barrier.
      DCHECK(Heap::RootIsImmortalImmovable(Heap::kmegamorphic_symbolRootIndex));
      StoreFixedArrayElement(
          type_feedback_vector, slot_id,
          HeapConstant(TypeFeedbackVector::MegamorphicSentinel(isolate())),
          SKIP_WRITE_BARRIER);
      Goto(&call);
    }
  }

  Bind(&call_function);
  {
    // Increment the call count.
    IncrementCallCount(type_feedback_vector, slot_id);

    Callable callable_call = CodeFactory::InterpreterPushArgsAndCall(
        isolate(), tail_call_mode, CallableType::kJSFunction);
    Node* code_target_call = HeapConstant(callable_call.code());
    Node* ret_value = CallStub(callable_call.descriptor(), code_target_call,
                               context, arg_count, first_arg, function);
    return_value.Bind(ret_value);
    Goto(&end);
  }

  Bind(&call);
  {
    // Increment the call count.
    IncrementCallCount(type_feedback_vector, slot_id);

    // Call using call builtin.
    Callable callable_call = CodeFactory::InterpreterPushArgsAndCall(
        isolate(), tail_call_mode, CallableType::kAny);
    Node* code_target_call = HeapConstant(callable_call.code());
    Node* ret_value = CallStub(callable_call.descriptor(), code_target_call,
                               context, arg_count, first_arg, function);
    return_value.Bind(ret_value);
    Goto(&end);
  }

  Bind(&call_without_feedback);
  {
    // Call using call builtin.
    Callable callable_call = CodeFactory::InterpreterPushArgsAndCall(
        isolate(), tail_call_mode, CallableType::kAny);
    Node* code_target_call = HeapConstant(callable_call.code());
    Node* ret_value = CallStub(callable_call.descriptor(), code_target_call,
                               context, arg_count, first_arg, function);
    return_value.Bind(ret_value);
    Goto(&end);
  }

  Bind(&end);
  return return_value.value();
}

Node* InterpreterAssembler::CallJS(Node* function, Node* context,
                                   Node* first_arg, Node* arg_count,
                                   TailCallMode tail_call_mode) {
  Callable callable = CodeFactory::InterpreterPushArgsAndCall(
      isolate(), tail_call_mode, CallableType::kAny);
  Node* code_target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), code_target, context, arg_count,
                  first_arg, function);
}

Node* InterpreterAssembler::CallConstruct(Node* constructor, Node* context,
                                          Node* new_target, Node* first_arg,
                                          Node* arg_count, Node* slot_id,
                                          Node* type_feedback_vector) {
  Label call_construct(this), js_function(this), end(this);
  Variable return_value(this, MachineRepresentation::kTagged);
  Variable allocation_feedback(this, MachineRepresentation::kTagged);
  allocation_feedback.Bind(UndefinedConstant());

  // Slot id of 0 is used to indicate no type feedback is available.
  STATIC_ASSERT(TypeFeedbackVector::kReservedIndexCount > 0);
  Node* is_feedback_unavailable = Word32Equal(slot_id, Int32Constant(0));
  GotoIf(is_feedback_unavailable, &call_construct);

  // Check that the constructor is not a smi.
  Node* is_smi = WordIsSmi(constructor);
  GotoIf(is_smi, &call_construct);

  // Check that constructor is a JSFunction.
  Node* instance_type = LoadInstanceType(constructor);
  Node* is_js_function =
      WordEqual(instance_type, Int32Constant(JS_FUNCTION_TYPE));
  BranchIf(is_js_function, &js_function, &call_construct);

  Bind(&js_function);
  {
    // Cache the called function in a feedback vector slot. Cache states
    // are uninitialized, monomorphic (indicated by a JSFunction), and
    // megamorphic.
    // TODO(mythria/v8:5210): Check if it is better to mark extra_checks as a
    // deferred block so that call_construct_function will be scheduled.
    Label extra_checks(this), call_construct_function(this);

    Node* feedback_element =
        LoadFixedArrayElement(type_feedback_vector, slot_id);
    Node* feedback_value = LoadWeakCellValue(feedback_element);
    Node* is_monomorphic = WordEqual(constructor, feedback_value);
    BranchIf(is_monomorphic, &call_construct_function, &extra_checks);

    Bind(&extra_checks);
    {
      Label mark_megamorphic(this), initialize(this),
          check_allocation_site(this), check_initialized(this),
          set_alloc_feedback_and_call(this);
      {
        // Check if it is a megamorphic target
        Comment("check if megamorphic");
        Node* is_megamorphic = WordEqual(
            feedback_element,
            HeapConstant(TypeFeedbackVector::MegamorphicSentinel(isolate())));
        GotoIf(is_megamorphic, &call_construct_function);

        Comment("check if weak cell");
        Node* is_weak_cell = WordEqual(LoadMap(feedback_element),
                                       LoadRoot(Heap::kWeakCellMapRootIndex));
        GotoUnless(is_weak_cell, &check_allocation_site);
        // If the weak cell is cleared, we have a new chance to become
        // monomorphic.
        Comment("check if weak cell is cleared");
        Node* is_smi = WordIsSmi(feedback_value);
        BranchIf(is_smi, &initialize, &mark_megamorphic);
      }

      Bind(&check_allocation_site);
      {
        Comment("check if it is an allocation site");
        Node* is_allocation_site =
            WordEqual(LoadObjectField(feedback_element, 0),
                      LoadRoot(Heap::kAllocationSiteMapRootIndex));
        GotoUnless(is_allocation_site, &check_initialized);

        // Make sure the function is the Array() function
        Node* context_slot =
            LoadFixedArrayElement(LoadNativeContext(context),
                                  Int32Constant(Context::ARRAY_FUNCTION_INDEX));
        Node* is_array_function = WordEqual(context_slot, constructor);
        BranchIf(is_array_function, &set_alloc_feedback_and_call,
                 &mark_megamorphic);
      }

      Bind(&set_alloc_feedback_and_call);
      {
        allocation_feedback.Bind(feedback_element);
        Goto(&call_construct_function);
      }

      Bind(&check_initialized);
      {
        // Check if it is uninitialized.
        Comment("check if uninitialized");
        Node* is_uninitialized = WordEqual(
            feedback_element, LoadRoot(Heap::kuninitialized_symbolRootIndex));
        BranchIf(is_uninitialized, &initialize, &mark_megamorphic);
      }

      Bind(&initialize);
      {
        Label create_weak_cell(this), create_allocation_site(this);
        Comment("initialize the feedback element");
        // Check that it is the Array() function.
        Node* context_slot =
            LoadFixedArrayElement(LoadNativeContext(context),
                                  Int32Constant(Context::ARRAY_FUNCTION_INDEX));
        Node* is_array_function = WordEqual(context_slot, constructor);
        BranchIf(is_array_function, &create_allocation_site, &create_weak_cell);

        Bind(&create_allocation_site);
        {
          Node* site = CreateAllocationSiteInFeedbackVector(
              type_feedback_vector, SmiTag(slot_id));
          allocation_feedback.Bind(site);
          Goto(&call_construct_function);
        }

        Bind(&create_weak_cell);
        {
          CreateWeakCellInFeedbackVector(type_feedback_vector, SmiTag(slot_id),
                                         constructor);
          Goto(&call_construct_function);
        }
      }

      Bind(&mark_megamorphic);
      {
        // MegamorphicSentinel is an immortal immovable object so
        // write-barrier is not needed.
        Comment("transition to megamorphic");
        DCHECK(
            Heap::RootIsImmortalImmovable(Heap::kmegamorphic_symbolRootIndex));
        StoreFixedArrayElement(
            type_feedback_vector, slot_id,
            HeapConstant(TypeFeedbackVector::MegamorphicSentinel(isolate())),
            SKIP_WRITE_BARRIER);
        Goto(&call_construct_function);
      }
    }

    Bind(&call_construct_function);
    {
      Comment("call using callConstructFunction");
      IncrementCallCount(type_feedback_vector, slot_id);
      Callable callable_function = CodeFactory::InterpreterPushArgsAndConstruct(
          isolate(), CallableType::kJSFunction);
      return_value.Bind(CallStub(callable_function.descriptor(),
                                 HeapConstant(callable_function.code()),
                                 context, arg_count, new_target, constructor,
                                 allocation_feedback.value(), first_arg));
      Goto(&end);
    }
  }

  Bind(&call_construct);
  {
    Comment("call using callConstruct builtin");
    Callable callable = CodeFactory::InterpreterPushArgsAndConstruct(
        isolate(), CallableType::kAny);
    Node* code_target = HeapConstant(callable.code());
    return_value.Bind(CallStub(callable.descriptor(), code_target, context,
                               arg_count, new_target, constructor,
                               UndefinedConstant(), first_arg));
    Goto(&end);
  }

  Bind(&end);
  return return_value.value();
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
  // TODO(rmcilroy): It might be worthwhile to only update the budget for
  // backwards branches. Those are distinguishable by the {JumpLoop} bytecode.

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

Node* InterpreterAssembler::Advance() {
  return Advance(Bytecodes::Size(bytecode_, operand_scale_));
}

Node* InterpreterAssembler::Advance(int delta) {
  return Advance(IntPtrConstant(delta));
}

Node* InterpreterAssembler::Advance(Node* delta) {
  if (FLAG_trace_ignition) {
    TraceBytecode(Runtime::kInterpreterTraceBytecodeExit);
  }
  Node* next_offset = IntPtrAdd(BytecodeOffset(), delta);
  bytecode_offset_.Bind(next_offset);
  return next_offset;
}

Node* InterpreterAssembler::Jump(Node* delta) {
  DCHECK(!Bytecodes::IsStarLookahead(bytecode_, operand_scale_));

  UpdateInterruptBudget(delta);
  Node* new_bytecode_offset = Advance(delta);
  Node* target_bytecode = LoadBytecode(new_bytecode_offset);
  return DispatchToBytecode(target_bytecode, new_bytecode_offset);
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

Node* InterpreterAssembler::LoadBytecode(compiler::Node* bytecode_offset) {
  Node* bytecode =
      Load(MachineType::Uint8(), BytecodeArrayTaggedPointer(), bytecode_offset);
  if (kPointerSize == 8) {
    bytecode = ChangeUint32ToUint64(bytecode);
  }
  return bytecode;
}

Node* InterpreterAssembler::StarDispatchLookahead(Node* target_bytecode) {
  Label do_inline_star(this), done(this);

  Variable var_bytecode(this, MachineRepresentation::kWord8);
  var_bytecode.Bind(target_bytecode);

  Node* star_bytecode = IntPtrConstant(static_cast<int>(Bytecode::kStar));
  Node* is_star = WordEqual(target_bytecode, star_bytecode);
  BranchIf(is_star, &do_inline_star, &done);

  Bind(&do_inline_star);
  {
    InlineStar();
    var_bytecode.Bind(LoadBytecode(BytecodeOffset()));
    Goto(&done);
  }
  Bind(&done);
  return var_bytecode.value();
}

void InterpreterAssembler::InlineStar() {
  Bytecode previous_bytecode = bytecode_;
  AccumulatorUse previous_acc_use = accumulator_use_;

  bytecode_ = Bytecode::kStar;
  accumulator_use_ = AccumulatorUse::kNone;

  if (FLAG_trace_ignition) {
    TraceBytecode(Runtime::kInterpreterTraceBytecodeEntry);
  }
  StoreRegister(GetAccumulator(), BytecodeOperandReg(0));

  DCHECK_EQ(accumulator_use_, Bytecodes::GetAccumulatorUse(bytecode_));

  Advance();
  bytecode_ = previous_bytecode;
  accumulator_use_ = previous_acc_use;
}

Node* InterpreterAssembler::Dispatch() {
  Node* target_offset = Advance();
  Node* target_bytecode = LoadBytecode(target_offset);

  if (Bytecodes::IsStarLookahead(bytecode_, operand_scale_)) {
    target_bytecode = StarDispatchLookahead(target_bytecode);
  }
  return DispatchToBytecode(target_bytecode, BytecodeOffset());
}

Node* InterpreterAssembler::DispatchToBytecode(Node* target_bytecode,
                                               Node* new_bytecode_offset) {
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
  Node* next_bytecode = LoadBytecode(next_bytecode_offset);

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

Node* InterpreterAssembler::TruncateTaggedToWord32WithFeedback(
    Node* context, Node* value, Variable* var_type_feedback) {
  // We might need to loop once due to ToNumber conversion.
  Variable var_value(this, MachineRepresentation::kTagged),
      var_result(this, MachineRepresentation::kWord32);
  Variable* loop_vars[] = {&var_value, var_type_feedback};
  Label loop(this, 2, loop_vars), done_loop(this, &var_result);
  var_value.Bind(value);
  var_type_feedback->Bind(Int32Constant(BinaryOperationFeedback::kNone));
  Goto(&loop);
  Bind(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    Label if_valueissmi(this), if_valueisnotsmi(this);
    Branch(WordIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

    Bind(&if_valueissmi);
    {
      // Convert the Smi {value}.
      var_result.Bind(SmiToWord32(value));
      var_type_feedback->Bind(
          Word32Or(var_type_feedback->value(),
                   Int32Constant(BinaryOperationFeedback::kSignedSmall)));
      Goto(&done_loop);
    }

    Bind(&if_valueisnotsmi);
    {
      // Check if {value} is a HeapNumber.
      Label if_valueisheapnumber(this),
          if_valueisnotheapnumber(this, Label::kDeferred);
      Branch(WordEqual(LoadMap(value), HeapNumberMapConstant()),
             &if_valueisheapnumber, &if_valueisnotheapnumber);

      Bind(&if_valueisheapnumber);
      {
        // Truncate the floating point value.
        var_result.Bind(TruncateHeapNumberValueToWord32(value));
        var_type_feedback->Bind(
            Word32Or(var_type_feedback->value(),
                     Int32Constant(BinaryOperationFeedback::kNumber)));
        Goto(&done_loop);
      }

      Bind(&if_valueisnotheapnumber);
      {
        // Convert the {value} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_value.Bind(CallStub(callable, context, value));
        var_type_feedback->Bind(Int32Constant(BinaryOperationFeedback::kAny));
        Goto(&loop);
      }
    }
  }
  Bind(&done_loop);
  return var_result.value();
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

Node* InterpreterAssembler::LoadOSRNestingLevel() {
  Node* offset =
      IntPtrConstant(BytecodeArray::kOSRNestingLevelOffset - kHeapObjectTag);
  return Load(MachineType::Int8(), BytecodeArrayTaggedPointer(), offset);
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
#elif V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_X87 || \
    V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || \
    V8_TARGET_ARCH_PPC
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
    Node* array_size = LoadAndUntagFixedArrayBaseLength(array);
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
    Node* array_size = LoadAndUntagFixedArrayBaseLength(array);
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
