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
#include "src/objects-inl.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::CodeAssemblerState;
using compiler::Node;

InterpreterAssembler::InterpreterAssembler(CodeAssemblerState* state,
                                           Bytecode bytecode,
                                           OperandScale operand_scale)
    : CodeStubAssembler(state),
      bytecode_(bytecode),
      operand_scale_(operand_scale),
      bytecode_offset_(this, MachineType::PointerRepresentation()),
      interpreted_frame_pointer_(this, MachineType::PointerRepresentation()),
      bytecode_array_(this, MachineRepresentation::kTagged),
      dispatch_table_(this, MachineType::PointerRepresentation()),
      accumulator_(this, MachineRepresentation::kTagged),
      accumulator_use_(AccumulatorUse::kNone),
      made_call_(false),
      reloaded_frame_ptr_(false),
      saved_bytecode_offset_(false),
      disable_stack_check_across_call_(false),
      stack_pointer_before_call_(nullptr) {
  accumulator_.Bind(Parameter(InterpreterDispatchDescriptor::kAccumulator));
  bytecode_offset_.Bind(
      Parameter(InterpreterDispatchDescriptor::kBytecodeOffset));
  bytecode_array_.Bind(
      Parameter(InterpreterDispatchDescriptor::kBytecodeArray));
  dispatch_table_.Bind(
      Parameter(InterpreterDispatchDescriptor::kDispatchTable));

#ifdef V8_TRACE_IGNITION
  TraceBytecode(Runtime::kInterpreterTraceBytecodeEntry);
#endif
  RegisterCallGenerationCallbacks([this] { CallPrologue(); },
                                  [this] { CallEpilogue(); });

  if (Bytecodes::MakesCallAlongCriticalPath(bytecode)) {
    SaveBytecodeOffset();
  }
}

InterpreterAssembler::~InterpreterAssembler() {
  // If the following check fails the handler does not use the
  // accumulator in the way described in the bytecode definitions in
  // bytecodes.h.
  DCHECK_EQ(accumulator_use_, Bytecodes::GetAccumulatorUse(bytecode_));
  UnregisterCallGenerationCallbacks();
}

Node* InterpreterAssembler::GetInterpretedFramePointer() {
  if (!interpreted_frame_pointer_.IsBound()) {
    interpreted_frame_pointer_.Bind(LoadParentFramePointer());
  } else if (Bytecodes::MakesCallAlongCriticalPath(bytecode_) && made_call_ &&
             !reloaded_frame_ptr_) {
    interpreted_frame_pointer_.Bind(LoadParentFramePointer());
    reloaded_frame_ptr_ = true;
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
  Branch(Word32Equal(depth, Int32Constant(0)), &context_found, &context_search);

  // Loop until the depth is 0.
  BIND(&context_search);
  {
    cur_depth.Bind(Int32Sub(cur_depth.value(), Int32Constant(1)));
    cur_context.Bind(
        LoadContextElement(cur_context.value(), Context::PREVIOUS_INDEX));

    Branch(Word32Equal(cur_depth.value(), Int32Constant(0)), &context_found,
           &context_search);
  }

  BIND(&context_found);
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
  BIND(&context_search);
  {
    // TODO(leszeks): We only need to do this check if the context had a sloppy
    // eval, we could pass in a context chain bitmask to figure out which
    // contexts actually need to be checked.

    Node* extension_slot =
        LoadContextElement(cur_context.value(), Context::EXTENSION_INDEX);

    // Jump to the target if the extension slot is not a hole.
    GotoIf(WordNotEqual(extension_slot, TheHoleConstant()), target);

    cur_depth.Bind(Int32Sub(cur_depth.value(), Int32Constant(1)));
    cur_context.Bind(
        LoadContextElement(cur_context.value(), Context::PREVIOUS_INDEX));

    GotoIf(Word32NotEqual(cur_depth.value(), Int32Constant(0)),
           &context_search);
  }
}

Node* InterpreterAssembler::BytecodeOffset() {
  if (Bytecodes::MakesCallAlongCriticalPath(bytecode_) && made_call_ &&
      (bytecode_offset_.value() ==
       Parameter(InterpreterDispatchDescriptor::kBytecodeOffset))) {
    bytecode_offset_.Bind(LoadAndUntagRegister(Register::bytecode_offset()));
  }
  return bytecode_offset_.value();
}

Node* InterpreterAssembler::BytecodeArrayTaggedPointer() {
  // Force a re-load of the bytecode array after every call in case the debugger
  // has been activated.
  if (made_call_ &&
      (bytecode_array_.value() ==
       Parameter(InterpreterDispatchDescriptor::kBytecodeArray))) {
    bytecode_array_.Bind(LoadRegister(Register::bytecode_array()));
  }
  return bytecode_array_.value();
}

Node* InterpreterAssembler::DispatchTableRawPointer() {
  if (Bytecodes::MakesCallAlongCriticalPath(bytecode_) && made_call_ &&
      (dispatch_table_.value() ==
       Parameter(InterpreterDispatchDescriptor::kDispatchTable))) {
    dispatch_table_.Bind(ExternalConstant(
        ExternalReference::interpreter_dispatch_table_address(isolate())));
  }
  return dispatch_table_.value();
}

Node* InterpreterAssembler::RegisterLocation(Node* reg_index) {
  return IntPtrAdd(GetInterpretedFramePointer(),
                   RegisterFrameOffset(reg_index));
}

Node* InterpreterAssembler::RegisterFrameOffset(Node* index) {
  return TimesPointerSize(index);
}

Node* InterpreterAssembler::LoadRegister(Register reg) {
  return Load(MachineType::AnyTagged(), GetInterpretedFramePointer(),
              IntPtrConstant(reg.ToOperand() << kPointerSizeLog2));
}

Node* InterpreterAssembler::LoadRegister(Node* reg_index) {
  return Load(MachineType::AnyTagged(), GetInterpretedFramePointer(),
              RegisterFrameOffset(reg_index));
}

Node* InterpreterAssembler::LoadAndUntagRegister(Register reg) {
  return LoadAndUntagSmi(GetInterpretedFramePointer(), reg.ToOperand()
                                                           << kPointerSizeLog2);
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

Node* InterpreterAssembler::StoreAndTagRegister(compiler::Node* value,
                                                Register reg) {
  int offset = reg.ToOperand() << kPointerSizeLog2;
  return StoreAndTagSmi(GetInterpretedFramePointer(), offset, value);
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
  return Load(MachineType::Int8(), BytecodeArrayTaggedPointer(),
              IntPtrAdd(BytecodeOffset(), operand_offset));
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
  if (TargetSupportsUnalignedAccess()) {
    return Load(MachineType::Int16(), BytecodeArrayTaggedPointer(),
                IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)));
  } else {
    return BytecodeOperandReadUnaligned(operand_offset, MachineType::Int16());
  }
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
  if (TargetSupportsUnalignedAccess()) {
    return Load(MachineType::Int32(), BytecodeArrayTaggedPointer(),
                IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)));
  } else {
    return BytecodeOperandReadUnaligned(operand_offset, MachineType::Int32());
  }
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

Node* InterpreterAssembler::BytecodeOperandUImmWord(int operand_index) {
  return ChangeUint32ToWord(BytecodeOperandUImm(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandImm(int operand_index) {
  DCHECK_EQ(OperandType::kImm,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeSignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandImmIntPtr(int operand_index) {
  return ChangeInt32ToIntPtr(BytecodeOperandImm(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandImmSmi(int operand_index) {
  return SmiFromWord32(BytecodeOperandImm(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandIdxInt32(int operand_index) {
  DCHECK(OperandType::kIdx ==
         Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandIdx(int operand_index) {
  return ChangeUint32ToWord(BytecodeOperandIdxInt32(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandIdxSmi(int operand_index) {
  return SmiTag(BytecodeOperandIdx(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandReg(int operand_index) {
  DCHECK(Bytecodes::IsRegisterOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return ChangeInt32ToIntPtr(
      BytecodeSignedOperand(operand_index, operand_size));
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
  return LoadFixedArrayElement(constant_pool, index);
}

Node* InterpreterAssembler::LoadAndUntagConstantPoolEntry(Node* index) {
  return SmiUntag(LoadConstantPoolEntry(index));
}

Node* InterpreterAssembler::LoadFeedbackVector() {
  Node* function = LoadRegister(Register::function_closure());
  Node* cell = LoadObjectField(function, JSFunction::kFeedbackVectorOffset);
  Node* vector = LoadObjectField(cell, Cell::kValueOffset);
  return vector;
}

void InterpreterAssembler::SaveBytecodeOffset() {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  StoreAndTagRegister(BytecodeOffset(), Register::bytecode_offset());
  saved_bytecode_offset_ = true;
}

void InterpreterAssembler::CallPrologue() {
  if (!saved_bytecode_offset_) {
    // If there are multiple calls in the bytecode handler, you need to spill
    // before each of them, unless SaveBytecodeOffset has explicitly been called
    // in a path that dominates _all_ of those calls. Therefore don't set
    // saved_bytecode_offset_ to true or call SaveBytecodeOffset.
    StoreAndTagRegister(BytecodeOffset(), Register::bytecode_offset());
  }

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

Node* InterpreterAssembler::IncrementCallCount(Node* feedback_vector,
                                               Node* slot_id) {
  Comment("increment call count");
  Node* call_count_slot = IntPtrAdd(slot_id, IntPtrConstant(1));
  Node* call_count = LoadFixedArrayElement(feedback_vector, call_count_slot);
  Node* new_count = SmiAdd(call_count, SmiConstant(1));
  // Count is Smi, so we don't need a write barrier.
  return StoreFixedArrayElement(feedback_vector, call_count_slot, new_count,
                                SKIP_WRITE_BARRIER);
}

Node* InterpreterAssembler::CallJSWithFeedback(
    compiler::Node* function, compiler::Node* context,
    compiler::Node* first_arg, compiler::Node* arg_count,
    compiler::Node* slot_id, compiler::Node* feedback_vector,
    ConvertReceiverMode receiver_mode, TailCallMode tail_call_mode) {
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
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK(Bytecodes::IsCallOrConstruct(bytecode_));
  DCHECK_EQ(Bytecodes::GetReceiverMode(bytecode_), receiver_mode);

  STATIC_ASSERT(WeakCell::kSize >= kPointerSize);
  STATIC_ASSERT(AllocationSite::kTransitionInfoOffset ==
                    WeakCell::kValueOffset &&
                WeakCell::kValueOffset == Symbol::kHashFieldSlot);

  Variable return_value(this, MachineRepresentation::kTagged);
  Label call_function(this), extra_checks(this, Label::kDeferred), call(this),
      end(this);

  // The checks. First, does function match the recorded monomorphic target?
  Node* feedback_element = LoadFixedArrayElement(feedback_vector, slot_id);
  Node* feedback_value = LoadWeakCellValueUnchecked(feedback_element);
  Node* is_monomorphic = WordEqual(function, feedback_value);
  GotoIfNot(is_monomorphic, &extra_checks);

  // The compare above could have been a SMI/SMI comparison. Guard against
  // this convincing us that we have a monomorphic JSFunction.
  Node* is_smi = TaggedIsSmi(function);
  Branch(is_smi, &extra_checks, &call_function);

  BIND(&call_function);
  {
    // Increment the call count.
    IncrementCallCount(feedback_vector, slot_id);

    // Call using call function builtin.
    Callable callable = CodeFactory::InterpreterPushArgsThenCall(
        isolate(), receiver_mode, tail_call_mode,
        InterpreterPushArgsMode::kJSFunction);
    Node* code_target = HeapConstant(callable.code());
    Node* ret_value = CallStub(callable.descriptor(), code_target, context,
                               arg_count, first_arg, function);
    return_value.Bind(ret_value);
    Goto(&end);
  }

  BIND(&extra_checks);
  {
    Label check_initialized(this), mark_megamorphic(this),
        create_allocation_site(this);

    Comment("check if megamorphic");
    // Check if it is a megamorphic target.
    Node* is_megamorphic =
        WordEqual(feedback_element,
                  HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())));
    GotoIf(is_megamorphic, &call);

    Comment("check if it is an allocation site");
    GotoIfNot(IsAllocationSiteMap(LoadMap(feedback_element)),
              &check_initialized);

    if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
      // For undefined receivers (mostly global calls), do an additional check
      // for the monomorphic Array function, which would otherwise appear
      // megamorphic.

      // If it is not the Array() function, mark megamorphic.
      Node* context_slot = LoadContextElement(LoadNativeContext(context),
                                              Context::ARRAY_FUNCTION_INDEX);
      Node* is_array_function = WordEqual(context_slot, function);
      GotoIfNot(is_array_function, &mark_megamorphic);

      // It is a monomorphic Array function. Increment the call count.
      IncrementCallCount(feedback_vector, slot_id);

      // Call ArrayConstructorStub.
      Callable callable_call =
          CodeFactory::InterpreterPushArgsThenConstructArray(isolate());
      Node* code_target_call = HeapConstant(callable_call.code());
      Node* ret_value =
          CallStub(callable_call.descriptor(), code_target_call, context,
                   arg_count, function, feedback_element, first_arg);
      return_value.Bind(ret_value);
      Goto(&end);

    } else {
      Goto(&mark_megamorphic);
    }

    BIND(&check_initialized);
    {
      Comment("check if uninitialized");
      // Check if it is uninitialized target first.
      Node* is_uninitialized = WordEqual(
          feedback_element,
          HeapConstant(FeedbackVector::UninitializedSentinel(isolate())));
      GotoIfNot(is_uninitialized, &mark_megamorphic);

      Comment("handle_uninitialized");
      // If it is not a JSFunction mark it as megamorphic.
      Node* is_smi = TaggedIsSmi(function);
      GotoIf(is_smi, &mark_megamorphic);

      // Check if function is an object of JSFunction type.
      Node* instance_type = LoadInstanceType(function);
      Node* is_js_function =
          Word32Equal(instance_type, Int32Constant(JS_FUNCTION_TYPE));
      GotoIfNot(is_js_function, &mark_megamorphic);

      // Check if it is the Array() function.
      Node* context_slot = LoadContextElement(LoadNativeContext(context),
                                              Context::ARRAY_FUNCTION_INDEX);
      Node* is_array_function = WordEqual(context_slot, function);
      GotoIf(is_array_function, &create_allocation_site);

      // Check if the function belongs to the same native context.
      Node* native_context = LoadNativeContext(
          LoadObjectField(function, JSFunction::kContextOffset));
      Node* is_same_native_context =
          WordEqual(native_context, LoadNativeContext(context));
      GotoIfNot(is_same_native_context, &mark_megamorphic);

      CreateWeakCellInFeedbackVector(feedback_vector, SmiTag(slot_id),
                                     function);

      // Call using call function builtin.
      Goto(&call_function);
    }

    BIND(&create_allocation_site);
    {
      CreateAllocationSiteInFeedbackVector(feedback_vector, SmiTag(slot_id));

      // Call using CallFunction builtin. CallICs have a PREMONOMORPHIC state.
      // They start collecting feedback only when a call is executed the second
      // time. So, do not pass any feedback here.
      Goto(&call_function);
    }

    BIND(&mark_megamorphic);
    {
      // Mark it as a megamorphic.
      // MegamorphicSentinel is created as a part of Heap::InitialObjects
      // and will not move during a GC. So it is safe to skip write barrier.
      DCHECK(Heap::RootIsImmortalImmovable(Heap::kmegamorphic_symbolRootIndex));
      StoreFixedArrayElement(
          feedback_vector, slot_id,
          HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())),
          SKIP_WRITE_BARRIER);
      Goto(&call);
    }
  }

  BIND(&call);
  {
    Comment("Increment call count and call using Call builtin");
    // Increment the call count.
    IncrementCallCount(feedback_vector, slot_id);

    // Call using call builtin.
    Callable callable_call = CodeFactory::InterpreterPushArgsThenCall(
        isolate(), receiver_mode, tail_call_mode,
        InterpreterPushArgsMode::kOther);
    Node* code_target_call = HeapConstant(callable_call.code());
    Node* ret_value = CallStub(callable_call.descriptor(), code_target_call,
                               context, arg_count, first_arg, function);
    return_value.Bind(ret_value);
    Goto(&end);
  }

  BIND(&end);
  return return_value.value();
}

Node* InterpreterAssembler::CallJS(Node* function, Node* context,
                                   Node* first_arg, Node* arg_count,
                                   ConvertReceiverMode receiver_mode,
                                   TailCallMode tail_call_mode) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK(Bytecodes::IsCallOrConstruct(bytecode_) ||
         bytecode_ == Bytecode::kInvokeIntrinsic);
  DCHECK_EQ(Bytecodes::GetReceiverMode(bytecode_), receiver_mode);
  Callable callable = CodeFactory::InterpreterPushArgsThenCall(
      isolate(), receiver_mode, tail_call_mode,
      InterpreterPushArgsMode::kOther);
  Node* code_target = HeapConstant(callable.code());

  return CallStub(callable.descriptor(), code_target, context, arg_count,
                  first_arg, function);
}

Node* InterpreterAssembler::CallJSWithSpread(Node* function, Node* context,
                                             Node* first_arg, Node* arg_count) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK_EQ(Bytecodes::GetReceiverMode(bytecode_), ConvertReceiverMode::kAny);
  Callable callable = CodeFactory::InterpreterPushArgsThenCall(
      isolate(), ConvertReceiverMode::kAny, TailCallMode::kDisallow,
      InterpreterPushArgsMode::kWithFinalSpread);
  Node* code_target = HeapConstant(callable.code());

  return CallStub(callable.descriptor(), code_target, context, arg_count,
                  first_arg, function);
}

Node* InterpreterAssembler::Construct(Node* constructor, Node* context,
                                      Node* new_target, Node* first_arg,
                                      Node* arg_count, Node* slot_id,
                                      Node* feedback_vector) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  Variable return_value(this, MachineRepresentation::kTagged);
  Variable allocation_feedback(this, MachineRepresentation::kTagged);
  Label call_construct_function(this, &allocation_feedback),
      extra_checks(this, Label::kDeferred), call_construct(this), end(this);

  // Slot id of 0 is used to indicate no type feedback is available.
  STATIC_ASSERT(FeedbackVector::kReservedIndexCount > 0);
  Node* is_feedback_unavailable = WordEqual(slot_id, IntPtrConstant(0));
  GotoIf(is_feedback_unavailable, &call_construct);

  // Check that the constructor is not a smi.
  Node* is_smi = TaggedIsSmi(constructor);
  GotoIf(is_smi, &call_construct);

  // Check that constructor is a JSFunction.
  Node* instance_type = LoadInstanceType(constructor);
  Node* is_js_function =
      Word32Equal(instance_type, Int32Constant(JS_FUNCTION_TYPE));
  GotoIfNot(is_js_function, &call_construct);

  // Check if it is a monomorphic constructor.
  Node* feedback_element = LoadFixedArrayElement(feedback_vector, slot_id);
  Node* feedback_value = LoadWeakCellValueUnchecked(feedback_element);
  Node* is_monomorphic = WordEqual(constructor, feedback_value);
  allocation_feedback.Bind(UndefinedConstant());
  Branch(is_monomorphic, &call_construct_function, &extra_checks);

  BIND(&call_construct_function);
  {
    Comment("call using ConstructFunction");
    IncrementCallCount(feedback_vector, slot_id);
    Callable callable_function = CodeFactory::InterpreterPushArgsThenConstruct(
        isolate(), InterpreterPushArgsMode::kJSFunction);
    return_value.Bind(CallStub(callable_function.descriptor(),
                               HeapConstant(callable_function.code()), context,
                               arg_count, new_target, constructor,
                               allocation_feedback.value(), first_arg));
    Goto(&end);
  }

  BIND(&extra_checks);
  {
    Label check_allocation_site(this), check_initialized(this),
        initialize(this), mark_megamorphic(this);

    // Check if it is a megamorphic target.
    Comment("check if megamorphic");
    Node* is_megamorphic =
        WordEqual(feedback_element,
                  HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())));
    GotoIf(is_megamorphic, &call_construct_function);

    Comment("check if weak cell");
    Node* is_weak_cell = WordEqual(LoadMap(feedback_element),
                                   LoadRoot(Heap::kWeakCellMapRootIndex));
    GotoIfNot(is_weak_cell, &check_allocation_site);

    // If the weak cell is cleared, we have a new chance to become
    // monomorphic.
    Comment("check if weak cell is cleared");
    Node* is_smi = TaggedIsSmi(feedback_value);
    Branch(is_smi, &initialize, &mark_megamorphic);

    BIND(&check_allocation_site);
    {
      Comment("check if it is an allocation site");
      Node* is_allocation_site =
          WordEqual(LoadObjectField(feedback_element, 0),
                    LoadRoot(Heap::kAllocationSiteMapRootIndex));
      GotoIfNot(is_allocation_site, &check_initialized);

      // Make sure the function is the Array() function.
      Node* context_slot = LoadContextElement(LoadNativeContext(context),
                                              Context::ARRAY_FUNCTION_INDEX);
      Node* is_array_function = WordEqual(context_slot, constructor);
      GotoIfNot(is_array_function, &mark_megamorphic);

      allocation_feedback.Bind(feedback_element);
      Goto(&call_construct_function);
    }

    BIND(&check_initialized);
    {
      // Check if it is uninitialized.
      Comment("check if uninitialized");
      Node* is_uninitialized = WordEqual(
          feedback_element, LoadRoot(Heap::kuninitialized_symbolRootIndex));
      Branch(is_uninitialized, &initialize, &mark_megamorphic);
    }

    BIND(&initialize);
    {
      Label create_allocation_site(this), create_weak_cell(this);
      Comment("initialize the feedback element");
      // Create an allocation site if the function is an array function,
      // otherwise create a weak cell.
      Node* context_slot = LoadContextElement(LoadNativeContext(context),
                                              Context::ARRAY_FUNCTION_INDEX);
      Node* is_array_function = WordEqual(context_slot, constructor);
      Branch(is_array_function, &create_allocation_site, &create_weak_cell);

      BIND(&create_allocation_site);
      {
        Node* site = CreateAllocationSiteInFeedbackVector(feedback_vector,
                                                          SmiTag(slot_id));
        allocation_feedback.Bind(site);
        Goto(&call_construct_function);
      }

      BIND(&create_weak_cell);
      {
        CreateWeakCellInFeedbackVector(feedback_vector, SmiTag(slot_id),
                                       constructor);
        Goto(&call_construct_function);
      }
    }

    BIND(&mark_megamorphic);
    {
      // MegamorphicSentinel is an immortal immovable object so
      // write-barrier is not needed.
      Comment("transition to megamorphic");
      DCHECK(Heap::RootIsImmortalImmovable(Heap::kmegamorphic_symbolRootIndex));
      StoreFixedArrayElement(
          feedback_vector, slot_id,
          HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())),
          SKIP_WRITE_BARRIER);
      Goto(&call_construct_function);
    }
  }

  BIND(&call_construct);
  {
    Comment("call using Construct builtin");
    Callable callable = CodeFactory::InterpreterPushArgsThenConstruct(
        isolate(), InterpreterPushArgsMode::kOther);
    Node* code_target = HeapConstant(callable.code());
    return_value.Bind(CallStub(callable.descriptor(), code_target, context,
                               arg_count, new_target, constructor,
                               UndefinedConstant(), first_arg));
    Goto(&end);
  }

  BIND(&end);
  return return_value.value();
}

Node* InterpreterAssembler::ConstructWithSpread(Node* constructor,
                                                Node* context, Node* new_target,
                                                Node* first_arg,
                                                Node* arg_count) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  Variable return_value(this, MachineRepresentation::kTagged);
  Comment("call using ConstructWithSpread");
  Callable callable = CodeFactory::InterpreterPushArgsThenConstruct(
      isolate(), InterpreterPushArgsMode::kWithFinalSpread);
  Node* code_target = HeapConstant(callable.code());
  return_value.Bind(CallStub(callable.descriptor(), code_target, context,
                             arg_count, new_target, constructor,
                             UndefinedConstant(), first_arg));

  return return_value.value();
}

Node* InterpreterAssembler::CallRuntimeN(Node* function_id, Node* context,
                                         Node* first_arg, Node* arg_count,
                                         int result_size) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK(Bytecodes::IsCallRuntime(bytecode_));
  Callable callable = CodeFactory::InterpreterCEntry(isolate(), result_size);
  Node* code_target = HeapConstant(callable.code());

  // Get the function entry from the function id.
  Node* function_table = ExternalConstant(
      ExternalReference::runtime_function_table_address(isolate()));
  Node* function_offset =
      Int32Mul(function_id, Int32Constant(sizeof(Runtime::Function)));
  Node* function =
      IntPtrAdd(function_table, ChangeUint32ToWord(function_offset));
  Node* function_entry =
      Load(MachineType::Pointer(), function,
           IntPtrConstant(offsetof(Runtime::Function, entry)));

  return CallStubR(callable.descriptor(), result_size, code_target, context,
                   arg_count, first_arg, function_entry);
}

void InterpreterAssembler::UpdateInterruptBudget(Node* weight, bool backward) {
  Label ok(this), interrupt_check(this, Label::kDeferred), end(this);
  Node* budget_offset =
      IntPtrConstant(BytecodeArray::kInterruptBudgetOffset - kHeapObjectTag);

  // Assert that the weight is positive (negative weights should be implemented
  // as backward updates).
  CSA_ASSERT(this, Int32GreaterThanOrEqual(weight, Int32Constant(0)));

  // Update budget by |weight| and check if it reaches zero.
  Variable new_budget(this, MachineRepresentation::kWord32);
  Node* old_budget =
      Load(MachineType::Int32(), BytecodeArrayTaggedPointer(), budget_offset);
  // Make sure we include the current bytecode in the budget calculation.
  Node* budget_after_bytecode =
      Int32Sub(old_budget, Int32Constant(CurrentBytecodeSize()));
  if (backward) {
    new_budget.Bind(Int32Sub(budget_after_bytecode, weight));
  } else {
    new_budget.Bind(Int32Add(budget_after_bytecode, weight));
  }
  Node* condition =
      Int32GreaterThanOrEqual(new_budget.value(), Int32Constant(0));
  Branch(condition, &ok, &interrupt_check);

  // Perform interrupt and reset budget.
  BIND(&interrupt_check);
  {
    CallRuntime(Runtime::kInterrupt, GetContext());
    new_budget.Bind(Int32Constant(Interpreter::InterruptBudget()));
    Goto(&ok);
  }

  // Update budget.
  BIND(&ok);
  StoreNoWriteBarrier(MachineRepresentation::kWord32,
                      BytecodeArrayTaggedPointer(), budget_offset,
                      new_budget.value());
}

Node* InterpreterAssembler::Advance() { return Advance(CurrentBytecodeSize()); }

Node* InterpreterAssembler::Advance(int delta) {
  return Advance(IntPtrConstant(delta));
}

Node* InterpreterAssembler::Advance(Node* delta, bool backward) {
#ifdef V8_TRACE_IGNITION
  TraceBytecode(Runtime::kInterpreterTraceBytecodeExit);
#endif
  Node* next_offset = backward ? IntPtrSub(BytecodeOffset(), delta)
                               : IntPtrAdd(BytecodeOffset(), delta);
  bytecode_offset_.Bind(next_offset);
  return next_offset;
}

Node* InterpreterAssembler::Jump(Node* delta, bool backward) {
  DCHECK(!Bytecodes::IsStarLookahead(bytecode_, operand_scale_));

  UpdateInterruptBudget(TruncateWordToWord32(delta), backward);
  Node* new_bytecode_offset = Advance(delta, backward);
  Node* target_bytecode = LoadBytecode(new_bytecode_offset);
  return DispatchToBytecode(target_bytecode, new_bytecode_offset);
}

Node* InterpreterAssembler::Jump(Node* delta) { return Jump(delta, false); }

Node* InterpreterAssembler::JumpBackward(Node* delta) {
  return Jump(delta, true);
}

void InterpreterAssembler::JumpConditional(Node* condition, Node* delta) {
  Label match(this), no_match(this);

  Branch(condition, &match, &no_match);
  BIND(&match);
  Jump(delta);
  BIND(&no_match);
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
  return ChangeUint32ToWord(bytecode);
}

Node* InterpreterAssembler::StarDispatchLookahead(Node* target_bytecode) {
  Label do_inline_star(this), done(this);

  Variable var_bytecode(this, MachineType::PointerRepresentation());
  var_bytecode.Bind(target_bytecode);

  Node* star_bytecode = IntPtrConstant(static_cast<int>(Bytecode::kStar));
  Node* is_star = WordEqual(target_bytecode, star_bytecode);
  Branch(is_star, &do_inline_star, &done);

  BIND(&do_inline_star);
  {
    InlineStar();
    var_bytecode.Bind(LoadBytecode(BytecodeOffset()));
    Goto(&done);
  }
  BIND(&done);
  return var_bytecode.value();
}

void InterpreterAssembler::InlineStar() {
  Bytecode previous_bytecode = bytecode_;
  AccumulatorUse previous_acc_use = accumulator_use_;

  bytecode_ = Bytecode::kStar;
  accumulator_use_ = AccumulatorUse::kNone;

#ifdef V8_TRACE_IGNITION
  TraceBytecode(Runtime::kInterpreterTraceBytecodeEntry);
#endif
  StoreRegister(GetAccumulator(), BytecodeOperandReg(0));

  DCHECK_EQ(accumulator_use_, Bytecodes::GetAccumulatorUse(bytecode_));

  Advance();
  bytecode_ = previous_bytecode;
  accumulator_use_ = previous_acc_use;
}

Node* InterpreterAssembler::Dispatch() {
  Comment("========= Dispatch");
  DCHECK_IMPLIES(Bytecodes::MakesCallAlongCriticalPath(bytecode_), made_call_);
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
           TimesPointerSize(target_bytecode));

  return DispatchToBytecodeHandlerEntry(target_code_entry, new_bytecode_offset);
}

Node* InterpreterAssembler::DispatchToBytecodeHandler(Node* handler,
                                                      Node* bytecode_offset) {
  // TODO(ishell): Add CSA::CodeEntryPoint(code).
  Node* handler_entry =
      IntPtrAdd(BitcastTaggedToWord(handler),
                IntPtrConstant(Code::kHeaderSize - kHeapObjectTag));
  return DispatchToBytecodeHandlerEntry(handler_entry, bytecode_offset);
}

Node* InterpreterAssembler::DispatchToBytecodeHandlerEntry(
    Node* handler_entry, Node* bytecode_offset) {
  InterpreterDispatchDescriptor descriptor(isolate());
  return TailCallBytecodeDispatch(
      descriptor, handler_entry, GetAccumulatorUnchecked(), bytecode_offset,
      BytecodeArrayTaggedPointer(), DispatchTableRawPointer());
}

void InterpreterAssembler::DispatchWide(OperandScale operand_scale) {
  // Dispatching a wide bytecode requires treating the prefix
  // bytecode a base pointer into the dispatch table and dispatching
  // the bytecode that follows relative to this base.
  //
  //   Indices 0-255 correspond to bytecodes with operand_scale == 0
  //   Indices 256-511 correspond to bytecodes with operand_scale == 1
  //   Indices 512-767 correspond to bytecodes with operand_scale == 2
  DCHECK_IMPLIES(Bytecodes::MakesCallAlongCriticalPath(bytecode_), made_call_);
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
           TimesPointerSize(target_index));

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
  var_type_feedback->Bind(SmiConstant(BinaryOperationFeedback::kNone));
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {value}.
    value = var_value.value();

    // Check if the {value} is a Smi or a HeapObject.
    Label if_valueissmi(this), if_valueisnotsmi(this);
    Branch(TaggedIsSmi(value), &if_valueissmi, &if_valueisnotsmi);

    BIND(&if_valueissmi);
    {
      // Convert the Smi {value}.
      var_result.Bind(SmiToWord32(value));
      var_type_feedback->Bind(
          SmiOr(var_type_feedback->value(),
                SmiConstant(BinaryOperationFeedback::kSignedSmall)));
      Goto(&done_loop);
    }

    BIND(&if_valueisnotsmi);
    {
      // Check if {value} is a HeapNumber.
      Label if_valueisheapnumber(this),
          if_valueisnotheapnumber(this, Label::kDeferred);
      Node* value_map = LoadMap(value);
      Branch(IsHeapNumberMap(value_map), &if_valueisheapnumber,
             &if_valueisnotheapnumber);

      BIND(&if_valueisheapnumber);
      {
        // Truncate the floating point value.
        var_result.Bind(TruncateHeapNumberValueToWord32(value));
        var_type_feedback->Bind(
            SmiOr(var_type_feedback->value(),
                  SmiConstant(BinaryOperationFeedback::kNumber)));
        Goto(&done_loop);
      }

      BIND(&if_valueisnotheapnumber);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_type_feedback->value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));

        Label if_valueisoddball(this),
            if_valueisnotoddball(this, Label::kDeferred);
        Node* is_oddball = Word32Equal(LoadMapInstanceType(value_map),
                                       Int32Constant(ODDBALL_TYPE));
        Branch(is_oddball, &if_valueisoddball, &if_valueisnotoddball);

        BIND(&if_valueisoddball);
        {
          // Convert Oddball to a Number and perform checks again.
          var_value.Bind(LoadObjectField(value, Oddball::kToNumberOffset));
          var_type_feedback->Bind(
              SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
          Goto(&loop);
        }

        BIND(&if_valueisnotoddball);
        {
          // Convert the {value} to a Number first.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_value.Bind(CallStub(callable, context, value));
          var_type_feedback->Bind(SmiConstant(BinaryOperationFeedback::kAny));
          Goto(&loop);
        }
      }
    }
  }
  BIND(&done_loop);
  return var_result.value();
}

void InterpreterAssembler::UpdateInterruptBudgetOnReturn() {
  // TODO(rmcilroy): Investigate whether it is worth supporting self
  // optimization of primitive functions like FullCodegen.

  // Update profiling count by the number of bytes between the end of the
  // current bytecode and the start of the first one, to simulate backedge to
  // start of function.
  //
  // With headers and current offset, the bytecode array layout looks like:
  //
  //           <---------- simulated backedge ----------
  // | header | first bytecode | .... | return bytecode |
  //  |<------ current offset ------->
  //  ^ tagged bytecode array pointer
  //
  // UpdateInterruptBudget already handles adding the bytecode size to the
  // length of the back-edge, so we just have to correct for the non-zero offset
  // of the first bytecode.

  const int kFirstBytecodeOffset = BytecodeArray::kHeaderSize - kHeapObjectTag;
  Node* profiling_weight = Int32Sub(TruncateWordToWord32(BytecodeOffset()),
                                    Int32Constant(kFirstBytecodeOffset));
  UpdateInterruptBudget(profiling_weight, true);
}

Node* InterpreterAssembler::StackCheckTriggeredInterrupt() {
  Node* sp = LoadStackPointer();
  Node* stack_limit = Load(
      MachineType::Pointer(),
      ExternalConstant(ExternalReference::address_of_stack_limit(isolate())));
  return UintPtrLessThan(sp, stack_limit);
}

Node* InterpreterAssembler::LoadOSRNestingLevel() {
  return LoadObjectField(BytecodeArrayTaggedPointer(),
                         BytecodeArray::kOSRNestingLevelOffset,
                         MachineType::Int8());
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
  Branch(WordEqual(lhs, rhs), &ok, &abort);

  BIND(&abort);
  Abort(bailout_reason);
  Goto(&ok);

  BIND(&ok);
}

void InterpreterAssembler::MaybeDropFrames(Node* context) {
  Node* restart_fp_address =
      ExternalConstant(ExternalReference::debug_restart_fp_address(isolate()));

  Node* restart_fp = Load(MachineType::Pointer(), restart_fp_address);
  Node* null = IntPtrConstant(0);

  Label ok(this), drop_frames(this);
  Branch(IntPtrEqual(restart_fp, null), &ok, &drop_frames);

  BIND(&drop_frames);
  // We don't expect this call to return since the frame dropper tears down
  // the stack and jumps into the function on the target frame to restart it.
  CallStub(CodeFactory::FrameDropperTrampoline(isolate()), context, restart_fp);
  Abort(kUnexpectedReturnFromFrameDropper);
  Goto(&ok);

  BIND(&ok);
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
      TimesPointerSize(IntPtrAdd(source_bytecode_table_index, target_bytecode));
  Node* old_counter =
      Load(MachineType::IntPtr(), counters_table, counter_offset);

  Label counter_ok(this), counter_saturated(this, Label::kDeferred);

  Node* counter_reached_max = WordEqual(
      old_counter, IntPtrConstant(std::numeric_limits<uintptr_t>::max()));
  Branch(counter_reached_max, &counter_saturated, &counter_ok);

  BIND(&counter_ok);
  {
    Node* new_counter = IntPtrAdd(old_counter, IntPtrConstant(1));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), counters_table,
                        counter_offset, new_counter);
    Goto(&counter_saturated);
  }

  BIND(&counter_saturated);
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
      bytecode_array, BytecodeArray::kFrameSizeOffset, MachineType::Uint32());
  return WordShr(ChangeUint32ToWord(frame_size),
                 IntPtrConstant(kPointerSizeLog2));
}

Node* InterpreterAssembler::ExportRegisterFile(Node* array) {
  Node* register_count = RegisterCount();
  if (FLAG_debug_code) {
    Node* array_size = LoadAndUntagFixedArrayBaseLength(array);
    AbortIfWordNotEqual(array_size, register_count,
                        kInvalidRegisterFileInGenerator);
  }

  Variable var_index(this, MachineType::PointerRepresentation());
  var_index.Bind(IntPtrConstant(0));

  // Iterate over register file and write values into array.
  // The mapping of register to array index must match that used in
  // BytecodeGraphBuilder::VisitResumeGenerator.
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    Node* index = var_index.value();
    GotoIfNot(UintPtrLessThan(index, register_count), &done_loop);

    Node* reg_index = IntPtrSub(IntPtrConstant(Register(0).ToOperand()), index);
    Node* value = LoadRegister(reg_index);

    StoreFixedArrayElement(array, index, value);

    var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
    Goto(&loop);
  }
  BIND(&done_loop);

  return array;
}

Node* InterpreterAssembler::ImportRegisterFile(Node* array) {
  Node* register_count = RegisterCount();
  if (FLAG_debug_code) {
    Node* array_size = LoadAndUntagFixedArrayBaseLength(array);
    AbortIfWordNotEqual(array_size, register_count,
                        kInvalidRegisterFileInGenerator);
  }

  Variable var_index(this, MachineType::PointerRepresentation());
  var_index.Bind(IntPtrConstant(0));

  // Iterate over array and write values into register file.  Also erase the
  // array contents to not keep them alive artificially.
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    Node* index = var_index.value();
    GotoIfNot(UintPtrLessThan(index, register_count), &done_loop);

    Node* value = LoadFixedArrayElement(array, index);

    Node* reg_index = IntPtrSub(IntPtrConstant(Register(0).ToOperand()), index);
    StoreRegister(value, reg_index);

    StoreFixedArrayElement(array, index, StaleRegisterConstant());

    var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
    Goto(&loop);
  }
  BIND(&done_loop);

  return array;
}

int InterpreterAssembler::CurrentBytecodeSize() const {
  return Bytecodes::Size(bytecode_, operand_scale_);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
