// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-assembler.h"

#include <limits>
#include <ostream>

#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/machine-type.h"
#include "src/execution/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/objects-inl.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::CodeAssemblerState;
using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;

InterpreterAssembler::InterpreterAssembler(CodeAssemblerState* state,
                                           Bytecode bytecode,
                                           OperandScale operand_scale)
    : CodeStubAssembler(state),
      bytecode_(bytecode),
      operand_scale_(operand_scale),
      TVARIABLE_CONSTRUCTOR(interpreted_frame_pointer_),
      VARIABLE_CONSTRUCTOR(
          bytecode_array_, MachineRepresentation::kTagged,
          Parameter(InterpreterDispatchDescriptor::kBytecodeArray)),
      TVARIABLE_CONSTRUCTOR(
          bytecode_offset_,
          UncheckedCast<IntPtrT>(
              Parameter(InterpreterDispatchDescriptor::kBytecodeOffset))),
      VARIABLE_CONSTRUCTOR(
          dispatch_table_, MachineType::PointerRepresentation(),
          Parameter(InterpreterDispatchDescriptor::kDispatchTable)),
      VARIABLE_CONSTRUCTOR(
          accumulator_, MachineRepresentation::kTagged,
          Parameter(InterpreterDispatchDescriptor::kAccumulator)),
      accumulator_use_(AccumulatorUse::kNone),
      made_call_(false),
      reloaded_frame_ptr_(false),
      bytecode_array_valid_(true) {
#ifdef V8_TRACE_IGNITION
  TraceBytecode(Runtime::kInterpreterTraceBytecodeEntry);
#endif
  RegisterCallGenerationCallbacks([this] { CallPrologue(); },
                                  [this] { CallEpilogue(); });

  // Save the bytecode offset immediately if bytecode will make a call along
  // the critical path, or it is a return bytecode.
  if (Bytecodes::MakesCallAlongCriticalPath(bytecode) ||
      Bytecodes::Returns(bytecode)) {
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

TNode<RawPtrT> InterpreterAssembler::GetInterpretedFramePointer() {
  if (!interpreted_frame_pointer_.IsBound()) {
    interpreted_frame_pointer_ = LoadParentFramePointer();
  } else if (Bytecodes::MakesCallAlongCriticalPath(bytecode_) && made_call_ &&
             !reloaded_frame_ptr_) {
    interpreted_frame_pointer_ = LoadParentFramePointer();
    reloaded_frame_ptr_ = true;
  }
  return interpreted_frame_pointer_.value();
}

TNode<IntPtrT> InterpreterAssembler::BytecodeOffset() {
  if (Bytecodes::MakesCallAlongCriticalPath(bytecode_) && made_call_ &&
      (bytecode_offset_.value() ==
       Parameter(InterpreterDispatchDescriptor::kBytecodeOffset))) {
    bytecode_offset_ = ReloadBytecodeOffset();
  }
  return bytecode_offset_.value();
}

TNode<IntPtrT> InterpreterAssembler::ReloadBytecodeOffset() {
  TNode<IntPtrT> offset = LoadAndUntagRegister(Register::bytecode_offset());
  if (operand_scale() != OperandScale::kSingle) {
    // Add one to the offset such that it points to the actual bytecode rather
    // than the Wide / ExtraWide prefix bytecode.
    offset = IntPtrAdd(offset, IntPtrConstant(1));
  }
  return offset;
}

void InterpreterAssembler::SaveBytecodeOffset() {
  TNode<IntPtrT> bytecode_offset = BytecodeOffset();
  if (operand_scale() != OperandScale::kSingle) {
    // Subtract one from the bytecode_offset such that it points to the Wide /
    // ExtraWide prefix bytecode.
    bytecode_offset = IntPtrSub(BytecodeOffset(), IntPtrConstant(1));
  }
  int store_offset =
      Register::bytecode_offset().ToOperand() * kSystemPointerSize;
  TNode<RawPtrT> base = GetInterpretedFramePointer();

  if (SmiValuesAre32Bits()) {
    int zero_offset = store_offset + 4;
    int payload_offset = store_offset;
#if V8_TARGET_LITTLE_ENDIAN
    std::swap(zero_offset, payload_offset);
#endif
    StoreNoWriteBarrier(MachineRepresentation::kWord32, base,
                        IntPtrConstant(zero_offset), Int32Constant(0));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, base,
                        IntPtrConstant(payload_offset),
                        TruncateIntPtrToInt32(bytecode_offset));
  } else {
    StoreNoWriteBarrier(MachineRepresentation::kTaggedSigned, base,
                        IntPtrConstant(store_offset), SmiTag(bytecode_offset));
  }
}

Node* InterpreterAssembler::BytecodeArrayTaggedPointer() {
  // Force a re-load of the bytecode array after every call in case the debugger
  // has been activated.
  if (!bytecode_array_valid_) {
    bytecode_array_.Bind(LoadRegister(Register::bytecode_array()));
    bytecode_array_valid_ = true;
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

Node* InterpreterAssembler::GetAccumulatorUnchecked() {
  return accumulator_.value();
}

TNode<Object> InterpreterAssembler::GetAccumulator() {
  DCHECK(Bytecodes::ReadsAccumulator(bytecode_));
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kRead;
  return TaggedPoisonOnSpeculation(GetAccumulatorUnchecked());
}

void InterpreterAssembler::SetAccumulator(Node* value) {
  DCHECK(Bytecodes::WritesAccumulator(bytecode_));
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kWrite;
  accumulator_.Bind(value);
}

TNode<Context> InterpreterAssembler::GetContext() {
  return CAST(LoadRegister(Register::current_context()));
}

void InterpreterAssembler::SetContext(TNode<Context> value) {
  StoreRegister(value, Register::current_context());
}

Node* InterpreterAssembler::GetContextAtDepth(TNode<Context> context,
                                              TNode<Uint32T> depth) {
  TVARIABLE(Context, cur_context, context);
  TVARIABLE(Uint32T, cur_depth, depth);

  Label context_found(this);

  Variable* context_search_loop_variables[2] = {&cur_depth, &cur_context};
  Label context_search(this, 2, context_search_loop_variables);

  // Fast path if the depth is 0.
  Branch(Word32Equal(depth, Int32Constant(0)), &context_found, &context_search);

  // Loop until the depth is 0.
  BIND(&context_search);
  {
    cur_depth = Unsigned(Int32Sub(cur_depth.value(), Int32Constant(1)));
    cur_context =
        CAST(LoadContextElement(cur_context.value(), Context::PREVIOUS_INDEX));

    Branch(Word32Equal(cur_depth.value(), Int32Constant(0)), &context_found,
           &context_search);
  }

  BIND(&context_found);
  return cur_context.value();
}

void InterpreterAssembler::GotoIfHasContextExtensionUpToDepth(
    TNode<Context> context, TNode<Uint32T> depth, Label* target) {
  TVARIABLE(Context, cur_context, context);
  TVARIABLE(Uint32T, cur_depth, depth);

  Variable* context_search_loop_variables[2] = {&cur_depth, &cur_context};
  Label context_search(this, 2, context_search_loop_variables);

  // Loop until the depth is 0.
  Goto(&context_search);
  BIND(&context_search);
  {
    // TODO(leszeks): We only need to do this check if the context had a sloppy
    // eval, we could pass in a context chain bitmask to figure out which
    // contexts actually need to be checked.

    TNode<Object> extension_slot =
        LoadContextElement(cur_context.value(), Context::EXTENSION_INDEX);

    // Jump to the target if the extension slot is not a hole.
    GotoIf(TaggedNotEqual(extension_slot, TheHoleConstant()), target);

    cur_depth = Unsigned(Int32Sub(cur_depth.value(), Int32Constant(1)));
    cur_context =
        CAST(LoadContextElement(cur_context.value(), Context::PREVIOUS_INDEX));

    GotoIf(Word32NotEqual(cur_depth.value(), Int32Constant(0)),
           &context_search);
  }
}

TNode<IntPtrT> InterpreterAssembler::RegisterLocation(Node* reg_index) {
  return Signed(WordPoisonOnSpeculation(
      IntPtrAdd(GetInterpretedFramePointer(), RegisterFrameOffset(reg_index))));
}

TNode<IntPtrT> InterpreterAssembler::RegisterLocation(Register reg) {
  return RegisterLocation(IntPtrConstant(reg.ToOperand()));
}

TNode<IntPtrT> InterpreterAssembler::RegisterFrameOffset(Node* index) {
  return Signed(TimesSystemPointerSize(index));
}

TNode<Object> InterpreterAssembler::LoadRegister(Node* reg_index) {
  return LoadFullTagged(GetInterpretedFramePointer(),
                        RegisterFrameOffset(reg_index),
                        LoadSensitivity::kCritical);
}

TNode<Object> InterpreterAssembler::LoadRegister(Register reg) {
  return LoadFullTagged(GetInterpretedFramePointer(),
                        IntPtrConstant(reg.ToOperand() * kSystemPointerSize));
}

TNode<IntPtrT> InterpreterAssembler::LoadAndUntagRegister(Register reg) {
  TNode<RawPtrT> base = GetInterpretedFramePointer();
  int index = reg.ToOperand() * kSystemPointerSize;
  if (SmiValuesAre32Bits()) {
#if V8_TARGET_LITTLE_ENDIAN
    index += 4;
#endif
    return ChangeInt32ToIntPtr(
        Load(MachineType::Int32(), base, IntPtrConstant(index)));
  } else {
    return SmiToIntPtr(
        Load(MachineType::TaggedSigned(), base, IntPtrConstant(index)));
  }
}

TNode<Object> InterpreterAssembler::LoadRegisterAtOperandIndex(
    int operand_index) {
  return LoadRegister(
      BytecodeOperandReg(operand_index, LoadSensitivity::kSafe));
}

std::pair<TNode<Object>, TNode<Object>>
InterpreterAssembler::LoadRegisterPairAtOperandIndex(int operand_index) {
  DCHECK_EQ(OperandType::kRegPair,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  Node* first_reg_index =
      BytecodeOperandReg(operand_index, LoadSensitivity::kSafe);
  TNode<IntPtrT> second_reg_index = NextRegister(first_reg_index);
  return std::make_pair(LoadRegister(first_reg_index),
                        LoadRegister(second_reg_index));
}

InterpreterAssembler::RegListNodePair
InterpreterAssembler::GetRegisterListAtOperandIndex(int operand_index) {
  DCHECK(Bytecodes::IsRegisterListOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  DCHECK_EQ(OperandType::kRegCount,
            Bytecodes::GetOperandType(bytecode_, operand_index + 1));
  TNode<IntPtrT> base_reg = RegisterLocation(
      BytecodeOperandReg(operand_index, LoadSensitivity::kSafe));
  TNode<Uint32T> reg_count = BytecodeOperandCount(operand_index + 1);
  return RegListNodePair(base_reg, reg_count);
}

Node* InterpreterAssembler::LoadRegisterFromRegisterList(
    const RegListNodePair& reg_list, int index) {
  TNode<IntPtrT> location = RegisterLocationInRegisterList(reg_list, index);
  // Location is already poisoned on speculation, so no need to poison here.
  return LoadFullTagged(location);
}

TNode<IntPtrT> InterpreterAssembler::RegisterLocationInRegisterList(
    const RegListNodePair& reg_list, int index) {
  CSA_ASSERT(this,
             Uint32GreaterThan(reg_list.reg_count(), Int32Constant(index)));
  TNode<IntPtrT> offset = RegisterFrameOffset(IntPtrConstant(index));
  // Register indexes are negative, so subtract index from base location to get
  // location.
  return Signed(IntPtrSub(reg_list.base_reg_location(), offset));
}

void InterpreterAssembler::StoreRegister(Node* value, Register reg) {
  StoreFullTaggedNoWriteBarrier(
      GetInterpretedFramePointer(),
      IntPtrConstant(reg.ToOperand() * kSystemPointerSize), value);
}

void InterpreterAssembler::StoreRegister(Node* value, Node* reg_index) {
  StoreFullTaggedNoWriteBarrier(GetInterpretedFramePointer(),
                                RegisterFrameOffset(reg_index), value);
}

void InterpreterAssembler::StoreRegisterAtOperandIndex(Node* value,
                                                       int operand_index) {
  StoreRegister(value,
                BytecodeOperandReg(operand_index, LoadSensitivity::kSafe));
}

void InterpreterAssembler::StoreRegisterPairAtOperandIndex(Node* value1,
                                                           Node* value2,
                                                           int operand_index) {
  DCHECK_EQ(OperandType::kRegOutPair,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  Node* first_reg_index =
      BytecodeOperandReg(operand_index, LoadSensitivity::kSafe);
  StoreRegister(value1, first_reg_index);
  TNode<IntPtrT> second_reg_index = NextRegister(first_reg_index);
  StoreRegister(value2, second_reg_index);
}

void InterpreterAssembler::StoreRegisterTripleAtOperandIndex(
    Node* value1, Node* value2, Node* value3, int operand_index) {
  DCHECK_EQ(OperandType::kRegOutTriple,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  Node* first_reg_index =
      BytecodeOperandReg(operand_index, LoadSensitivity::kSafe);
  StoreRegister(value1, first_reg_index);
  TNode<IntPtrT> second_reg_index = NextRegister(first_reg_index);
  StoreRegister(value2, second_reg_index);
  TNode<IntPtrT> third_reg_index = NextRegister(second_reg_index);
  StoreRegister(value3, third_reg_index);
}

TNode<IntPtrT> InterpreterAssembler::NextRegister(Node* reg_index) {
  // Register indexes are negative, so the next index is minus one.
  return Signed(IntPtrAdd(reg_index, IntPtrConstant(-1)));
}

Node* InterpreterAssembler::OperandOffset(int operand_index) {
  return IntPtrConstant(
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale()));
}

TNode<Uint8T> InterpreterAssembler::BytecodeOperandUnsignedByte(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kByte, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  Node* operand_offset = OperandOffset(operand_index);
  return Load<Uint8T>(BytecodeArrayTaggedPointer(),
                      IntPtrAdd(BytecodeOffset(), operand_offset),
                      needs_poisoning);
}

TNode<Int8T> InterpreterAssembler::BytecodeOperandSignedByte(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kByte, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  Node* operand_offset = OperandOffset(operand_index);
  return Load<Int8T>(BytecodeArrayTaggedPointer(),
                     IntPtrAdd(BytecodeOffset(), operand_offset),
                     needs_poisoning);
}

TNode<Word32T> InterpreterAssembler::BytecodeOperandReadUnaligned(
    int relative_offset, MachineType result_type,
    LoadSensitivity needs_poisoning) {
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
  DCHECK_LE(count, kMaxCount);
  TNode<Word32T> bytes[kMaxCount];
  for (int i = 0; i < count; i++) {
    MachineType machine_type = (i == 0) ? msb_type : MachineType::Uint8();
    TNode<IntPtrT> offset =
        IntPtrConstant(relative_offset + msb_offset + i * kStep);
    TNode<WordT> array_offset = IntPtrAdd(BytecodeOffset(), offset);
    bytes[i] =
        UncheckedCast<Word32T>(Load(machine_type, BytecodeArrayTaggedPointer(),
                                    array_offset, needs_poisoning));
  }

  // Pack LSB to MSB.
  TNode<Word32T> result = bytes[--count];
  for (int i = 1; --count >= 0; i++) {
    TNode<Int32T> shift = Int32Constant(i * kBitsPerByte);
    TNode<Word32T> value = Word32Shl(bytes[count], shift);
    result = Word32Or(value, result);
  }
  return result;
}

TNode<Uint16T> InterpreterAssembler::BytecodeOperandUnsignedShort(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(
      OperandSize::kShort,
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  if (TargetSupportsUnalignedAccess()) {
    return Load<Uint16T>(
        BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)),
        needs_poisoning);
  } else {
    return UncheckedCast<Uint16T>(BytecodeOperandReadUnaligned(
        operand_offset, MachineType::Uint16(), needs_poisoning));
  }
}

TNode<Int16T> InterpreterAssembler::BytecodeOperandSignedShort(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(
      OperandSize::kShort,
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  if (TargetSupportsUnalignedAccess()) {
    return Load<Int16T>(
        BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)),
        needs_poisoning);
  } else {
    return UncheckedCast<Int16T>(BytecodeOperandReadUnaligned(
        operand_offset, MachineType::Int16(), needs_poisoning));
  }
}

TNode<Uint32T> InterpreterAssembler::BytecodeOperandUnsignedQuad(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kQuad, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  if (TargetSupportsUnalignedAccess()) {
    return Load<Uint32T>(
        BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)),
        needs_poisoning);
  } else {
    return UncheckedCast<Uint32T>(BytecodeOperandReadUnaligned(
        operand_offset, MachineType::Uint32(), needs_poisoning));
  }
}

TNode<Int32T> InterpreterAssembler::BytecodeOperandSignedQuad(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(bytecode_));
  DCHECK_EQ(OperandSize::kQuad, Bytecodes::GetOperandSize(
                                    bytecode_, operand_index, operand_scale()));
  int operand_offset =
      Bytecodes::GetOperandOffset(bytecode_, operand_index, operand_scale());
  if (TargetSupportsUnalignedAccess()) {
    return Load<Int32T>(
        BytecodeArrayTaggedPointer(),
        IntPtrAdd(BytecodeOffset(), IntPtrConstant(operand_offset)),
        needs_poisoning);
  } else {
    return UncheckedCast<Int32T>(BytecodeOperandReadUnaligned(
        operand_offset, MachineType::Int32(), needs_poisoning));
  }
}

TNode<Int32T> InterpreterAssembler::BytecodeSignedOperand(
    int operand_index, OperandSize operand_size,
    LoadSensitivity needs_poisoning) {
  DCHECK(!Bytecodes::IsUnsignedOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  switch (operand_size) {
    case OperandSize::kByte:
      return BytecodeOperandSignedByte(operand_index, needs_poisoning);
    case OperandSize::kShort:
      return BytecodeOperandSignedShort(operand_index, needs_poisoning);
    case OperandSize::kQuad:
      return BytecodeOperandSignedQuad(operand_index, needs_poisoning);
    case OperandSize::kNone:
      UNREACHABLE();
  }
}

TNode<Uint32T> InterpreterAssembler::BytecodeUnsignedOperand(
    int operand_index, OperandSize operand_size,
    LoadSensitivity needs_poisoning) {
  DCHECK(Bytecodes::IsUnsignedOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  switch (operand_size) {
    case OperandSize::kByte:
      return BytecodeOperandUnsignedByte(operand_index, needs_poisoning);
    case OperandSize::kShort:
      return BytecodeOperandUnsignedShort(operand_index, needs_poisoning);
    case OperandSize::kQuad:
      return BytecodeOperandUnsignedQuad(operand_index, needs_poisoning);
    case OperandSize::kNone:
      UNREACHABLE();
  }
}

TNode<Uint32T> InterpreterAssembler::BytecodeOperandCount(int operand_index) {
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

TNode<Uint32T> InterpreterAssembler::BytecodeOperandUImm(int operand_index) {
  DCHECK_EQ(OperandType::kUImm,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandUImmWord(int operand_index) {
  return ChangeUint32ToWord(BytecodeOperandUImm(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandUImmSmi(int operand_index) {
  return SmiFromInt32(Signed(BytecodeOperandUImm(operand_index)));
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
  return SmiFromInt32(BytecodeOperandImm(operand_index));
}

Node* InterpreterAssembler::BytecodeOperandIdxInt32(int operand_index) {
  DCHECK_EQ(OperandType::kIdx,
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

Node* InterpreterAssembler::BytecodeOperandConstantPoolIdx(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK_EQ(OperandType::kIdx,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return ChangeUint32ToWord(
      BytecodeUnsignedOperand(operand_index, operand_size, needs_poisoning));
}

Node* InterpreterAssembler::BytecodeOperandReg(
    int operand_index, LoadSensitivity needs_poisoning) {
  DCHECK(Bytecodes::IsRegisterOperandType(
      Bytecodes::GetOperandType(bytecode_, operand_index)));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return ChangeInt32ToIntPtr(
      BytecodeSignedOperand(operand_index, operand_size, needs_poisoning));
}

Node* InterpreterAssembler::BytecodeOperandRuntimeId(int operand_index) {
  DCHECK_EQ(OperandType::kRuntimeId,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  DCHECK_EQ(operand_size, OperandSize::kShort);
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::BytecodeOperandNativeContextIndex(
    int operand_index) {
  DCHECK_EQ(OperandType::kNativeContextIndex,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  return ChangeUint32ToWord(
      BytecodeUnsignedOperand(operand_index, operand_size));
}

Node* InterpreterAssembler::BytecodeOperandIntrinsicId(int operand_index) {
  DCHECK_EQ(OperandType::kIntrinsicId,
            Bytecodes::GetOperandType(bytecode_, operand_index));
  OperandSize operand_size =
      Bytecodes::GetOperandSize(bytecode_, operand_index, operand_scale());
  DCHECK_EQ(operand_size, OperandSize::kByte);
  return BytecodeUnsignedOperand(operand_index, operand_size);
}

Node* InterpreterAssembler::LoadConstantPoolEntry(Node* index) {
  TNode<FixedArray> constant_pool = CAST(LoadObjectField(
      BytecodeArrayTaggedPointer(), BytecodeArray::kConstantPoolOffset));
  return UnsafeLoadFixedArrayElement(
      constant_pool, UncheckedCast<IntPtrT>(index), LoadSensitivity::kCritical);
}

TNode<IntPtrT> InterpreterAssembler::LoadAndUntagConstantPoolEntry(
    Node* index) {
  return SmiUntag(LoadConstantPoolEntry(index));
}

Node* InterpreterAssembler::LoadConstantPoolEntryAtOperandIndex(
    int operand_index) {
  Node* index =
      BytecodeOperandConstantPoolIdx(operand_index, LoadSensitivity::kSafe);
  return LoadConstantPoolEntry(index);
}

TNode<IntPtrT>
InterpreterAssembler::LoadAndUntagConstantPoolEntryAtOperandIndex(
    int operand_index) {
  return SmiUntag(LoadConstantPoolEntryAtOperandIndex(operand_index));
}

TNode<HeapObject> InterpreterAssembler::LoadFeedbackVector() {
  TNode<JSFunction> function = CAST(LoadRegister(Register::function_closure()));
  return CodeStubAssembler::LoadFeedbackVector(function);
}

void InterpreterAssembler::CallPrologue() {
  if (!Bytecodes::MakesCallAlongCriticalPath(bytecode_)) {
    // Bytecodes that make a call along the critical path save the bytecode
    // offset in the bytecode handler's prologue. For other bytecodes, if
    // there are multiple calls in the bytecode handler, you need to spill
    // before each of them, unless SaveBytecodeOffset has explicitly been called
    // in a path that dominates _all_ of those calls (which we don't track).
    SaveBytecodeOffset();
  }

  bytecode_array_valid_ = false;
  made_call_ = true;
}

void InterpreterAssembler::CallEpilogue() {
}

void InterpreterAssembler::IncrementCallCount(Node* feedback_vector,
                                              Node* slot_id) {
  Comment("increment call count");
  TNode<Smi> call_count =
      CAST(LoadFeedbackVectorSlot(feedback_vector, slot_id, kTaggedSize));
  // The lowest {FeedbackNexus::CallCountField::kShift} bits of the call
  // count are used as flags. To increment the call count by 1 we hence
  // have to increment by 1 << {FeedbackNexus::CallCountField::kShift}.
  TNode<Smi> new_count = SmiAdd(
      call_count, SmiConstant(1 << FeedbackNexus::CallCountField::kShift));
  // Count is Smi, so we don't need a write barrier.
  StoreFeedbackVectorSlot(feedback_vector, slot_id, new_count,
                          SKIP_WRITE_BARRIER, kTaggedSize);
}

void InterpreterAssembler::CollectCallableFeedback(Node* target, Node* context,
                                                   Node* feedback_vector,
                                                   Node* slot_id) {
  Label extra_checks(this, Label::kDeferred), done(this);

  // Check if we have monomorphic {target} feedback already.
  TNode<MaybeObject> feedback =
      LoadFeedbackVectorSlot(feedback_vector, slot_id);
  Comment("check if monomorphic");
  TNode<BoolT> is_monomorphic = IsWeakReferenceTo(feedback, CAST(target));
  GotoIf(is_monomorphic, &done);

  // Check if it is a megamorphic {target}.
  Comment("check if megamorphic");
  TNode<BoolT> is_megamorphic = TaggedEqual(
      feedback, HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())));
  Branch(is_megamorphic, &done, &extra_checks);

  BIND(&extra_checks);
  {
    Label initialize(this), mark_megamorphic(this);

    Comment("check if weak reference");
    TNode<BoolT> is_uninitialized = TaggedEqual(
        feedback,
        HeapConstant(FeedbackVector::UninitializedSentinel(isolate())));
    GotoIf(is_uninitialized, &initialize);
    CSA_ASSERT(this, IsWeakOrCleared(feedback));

    // If the weak reference is cleared, we have a new chance to become
    // monomorphic.
    Comment("check if weak reference is cleared");
    Branch(IsCleared(feedback), &initialize, &mark_megamorphic);

    BIND(&initialize);
    {
      // Check if {target} is a JSFunction in the current native context.
      Comment("check if function in same native context");
      GotoIf(TaggedIsSmi(target), &mark_megamorphic);
      // Check if the {target} is a JSFunction or JSBoundFunction
      // in the current native context.
      VARIABLE(var_current, MachineRepresentation::kTagged, target);
      Label loop(this, &var_current), done_loop(this);
      Goto(&loop);
      BIND(&loop);
      {
        Label if_boundfunction(this), if_function(this);
        Node* current = var_current.value();
        CSA_ASSERT(this, TaggedIsNotSmi(current));
        TNode<Uint16T> current_instance_type = LoadInstanceType(current);
        GotoIf(InstanceTypeEqual(current_instance_type, JS_BOUND_FUNCTION_TYPE),
               &if_boundfunction);
        Branch(InstanceTypeEqual(current_instance_type, JS_FUNCTION_TYPE),
               &if_function, &mark_megamorphic);

        BIND(&if_function);
        {
          // Check that the JSFunction {current} is in the current native
          // context.
          TNode<Context> current_context =
              CAST(LoadObjectField(current, JSFunction::kContextOffset));
          TNode<Context> current_native_context =
              LoadNativeContext(current_context);
          Branch(
              TaggedEqual(LoadNativeContext(context), current_native_context),
              &done_loop, &mark_megamorphic);
        }

        BIND(&if_boundfunction);
        {
          // Continue with the [[BoundTargetFunction]] of {target}.
          var_current.Bind(LoadObjectField(
              current, JSBoundFunction::kBoundTargetFunctionOffset));
          Goto(&loop);
        }
      }
      BIND(&done_loop);
      StoreWeakReferenceInFeedbackVector(feedback_vector, slot_id,
                                         CAST(target));
      ReportFeedbackUpdate(feedback_vector, slot_id, "Call:Initialize");
      Goto(&done);
    }

    BIND(&mark_megamorphic);
    {
      // MegamorphicSentinel is an immortal immovable object so
      // write-barrier is not needed.
      Comment("transition to megamorphic");
      DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kmegamorphic_symbol));
      StoreFeedbackVectorSlot(
          feedback_vector, slot_id,
          HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())),
          SKIP_WRITE_BARRIER);
      ReportFeedbackUpdate(feedback_vector, slot_id,
                           "Call:TransitionMegamorphic");
      Goto(&done);
    }
  }

  BIND(&done);
}

void InterpreterAssembler::CollectCallFeedback(Node* target, Node* context,
                                               Node* maybe_feedback_vector,
                                               Node* slot_id) {
  Label feedback_done(this);
  // If feedback_vector is not valid, then nothing to do.
  GotoIf(IsUndefined(maybe_feedback_vector), &feedback_done);

  CSA_SLOW_ASSERT(this, IsFeedbackVector(maybe_feedback_vector));

  // Increment the call count.
  IncrementCallCount(maybe_feedback_vector, slot_id);

  // Collect the callable {target} feedback.
  CollectCallableFeedback(target, context, maybe_feedback_vector, slot_id);
  Goto(&feedback_done);

  BIND(&feedback_done);
}

void InterpreterAssembler::CallJSAndDispatch(
    Node* function, Node* context, const RegListNodePair& args,
    ConvertReceiverMode receiver_mode) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK(Bytecodes::IsCallOrConstruct(bytecode_) ||
         bytecode_ == Bytecode::kInvokeIntrinsic);
  DCHECK_EQ(Bytecodes::GetReceiverMode(bytecode_), receiver_mode);

  Node* args_count;
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // The receiver is implied, so it is not in the argument list.
    args_count = args.reg_count();
  } else {
    // Subtract the receiver from the argument count.
    TNode<Int32T> receiver_count = Int32Constant(1);
    args_count = Int32Sub(args.reg_count(), receiver_count);
  }

  Callable callable = CodeFactory::InterpreterPushArgsThenCall(
      isolate(), receiver_mode, InterpreterPushArgsMode::kOther);
  TNode<Code> code_target = HeapConstant(callable.code());

  TailCallStubThenBytecodeDispatch(callable.descriptor(), code_target, context,
                                   args_count, args.base_reg_location(),
                                   function);
  // TailCallStubThenDispatch updates accumulator with result.
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kWrite;
}

template <class... TArgs>
void InterpreterAssembler::CallJSAndDispatch(Node* function, Node* context,
                                             Node* arg_count,
                                             ConvertReceiverMode receiver_mode,
                                             TArgs... args) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK(Bytecodes::IsCallOrConstruct(bytecode_) ||
         bytecode_ == Bytecode::kInvokeIntrinsic);
  DCHECK_EQ(Bytecodes::GetReceiverMode(bytecode_), receiver_mode);
  Callable callable = CodeFactory::Call(isolate());
  TNode<Code> code_target = HeapConstant(callable.code());

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    // The first argument parameter (the receiver) is implied to be undefined.
    TailCallStubThenBytecodeDispatch(
        callable.descriptor(), code_target, context, function, arg_count,
        static_cast<Node*>(UndefinedConstant()), args...);
  } else {
    TailCallStubThenBytecodeDispatch(callable.descriptor(), code_target,
                                     context, function, arg_count, args...);
  }
  // TailCallStubThenDispatch updates accumulator with result.
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kWrite;
}

// Instantiate CallJSAndDispatch() for argument counts used by interpreter
// generator.
template V8_EXPORT_PRIVATE void InterpreterAssembler::CallJSAndDispatch(
    Node* function, Node* context, Node* arg_count,
    ConvertReceiverMode receiver_mode);
template V8_EXPORT_PRIVATE void InterpreterAssembler::CallJSAndDispatch(
    Node* function, Node* context, Node* arg_count,
    ConvertReceiverMode receiver_mode, Node*);
template V8_EXPORT_PRIVATE void InterpreterAssembler::CallJSAndDispatch(
    Node* function, Node* context, Node* arg_count,
    ConvertReceiverMode receiver_mode, Node*, Node*);
template V8_EXPORT_PRIVATE void InterpreterAssembler::CallJSAndDispatch(
    Node* function, Node* context, Node* arg_count,
    ConvertReceiverMode receiver_mode, Node*, Node*, Node*);

void InterpreterAssembler::CallJSWithSpreadAndDispatch(
    Node* function, Node* context, const RegListNodePair& args, Node* slot_id,
    Node* maybe_feedback_vector) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK_EQ(Bytecodes::GetReceiverMode(bytecode_), ConvertReceiverMode::kAny);
  CollectCallFeedback(function, context, maybe_feedback_vector, slot_id);
  Comment("call using CallWithSpread builtin");
  Callable callable = CodeFactory::InterpreterPushArgsThenCall(
      isolate(), ConvertReceiverMode::kAny,
      InterpreterPushArgsMode::kWithFinalSpread);
  TNode<Code> code_target = HeapConstant(callable.code());

  TNode<Int32T> receiver_count = Int32Constant(1);
  TNode<Word32T> args_count = Int32Sub(args.reg_count(), receiver_count);
  TailCallStubThenBytecodeDispatch(callable.descriptor(), code_target, context,
                                   args_count, args.base_reg_location(),
                                   function);
  // TailCallStubThenDispatch updates accumulator with result.
  accumulator_use_ = accumulator_use_ | AccumulatorUse::kWrite;
}

Node* InterpreterAssembler::Construct(SloppyTNode<Object> target, Node* context,
                                      SloppyTNode<Object> new_target,
                                      const RegListNodePair& args,
                                      Node* slot_id, Node* feedback_vector) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  VARIABLE(var_result, MachineRepresentation::kTagged);
  VARIABLE(var_site, MachineRepresentation::kTagged);
  Label extra_checks(this, Label::kDeferred), return_result(this, &var_result),
      construct(this), construct_array(this, &var_site);
  GotoIf(IsUndefined(feedback_vector), &construct);

  // Increment the call count.
  IncrementCallCount(feedback_vector, slot_id);

  // Check if we have monomorphic {new_target} feedback already.
  TNode<MaybeObject> feedback =
      LoadFeedbackVectorSlot(feedback_vector, slot_id);
  Branch(IsWeakReferenceTo(feedback, new_target), &construct, &extra_checks);

  BIND(&extra_checks);
  {
    Label check_allocation_site(this), check_initialized(this),
        initialize(this), mark_megamorphic(this);

    // Check if it is a megamorphic {new_target}..
    Comment("check if megamorphic");
    TNode<BoolT> is_megamorphic = TaggedEqual(
        feedback, HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())));
    GotoIf(is_megamorphic, &construct);

    Comment("check if weak reference");
    GotoIfNot(IsWeakOrCleared(feedback), &check_allocation_site);

    // If the weak reference is cleared, we have a new chance to become
    // monomorphic.
    Comment("check if weak reference is cleared");
    Branch(IsCleared(feedback), &initialize, &mark_megamorphic);

    BIND(&check_allocation_site);
    {
      // Check if it is an AllocationSite.
      Comment("check if allocation site");
      TNode<HeapObject> strong_feedback = CAST(feedback);
      GotoIfNot(IsAllocationSite(strong_feedback), &check_initialized);

      // Make sure that {target} and {new_target} are the Array constructor.
      TNode<Object> array_function = LoadContextElement(
          LoadNativeContext(context), Context::ARRAY_FUNCTION_INDEX);
      GotoIfNot(TaggedEqual(target, array_function), &mark_megamorphic);
      GotoIfNot(TaggedEqual(new_target, array_function), &mark_megamorphic);
      var_site.Bind(strong_feedback);
      Goto(&construct_array);
    }

    BIND(&check_initialized);
    {
      // Check if it is uninitialized.
      Comment("check if uninitialized");
      TNode<BoolT> is_uninitialized =
          TaggedEqual(feedback, UninitializedSymbolConstant());
      Branch(is_uninitialized, &initialize, &mark_megamorphic);
    }

    BIND(&initialize);
    {
      Comment("check if function in same native context");
      GotoIf(TaggedIsSmi(new_target), &mark_megamorphic);
      // Check if the {new_target} is a JSFunction or JSBoundFunction
      // in the current native context.
      VARIABLE(var_current, MachineRepresentation::kTagged, new_target);
      Label loop(this, &var_current), done_loop(this);
      Goto(&loop);
      BIND(&loop);
      {
        Label if_boundfunction(this), if_function(this);
        Node* current = var_current.value();
        CSA_ASSERT(this, TaggedIsNotSmi(current));
        TNode<Uint16T> current_instance_type = LoadInstanceType(current);
        GotoIf(InstanceTypeEqual(current_instance_type, JS_BOUND_FUNCTION_TYPE),
               &if_boundfunction);
        Branch(InstanceTypeEqual(current_instance_type, JS_FUNCTION_TYPE),
               &if_function, &mark_megamorphic);

        BIND(&if_function);
        {
          // Check that the JSFunction {current} is in the current native
          // context.
          TNode<Context> current_context =
              CAST(LoadObjectField(current, JSFunction::kContextOffset));
          TNode<Context> current_native_context =
              LoadNativeContext(current_context);
          Branch(
              TaggedEqual(LoadNativeContext(context), current_native_context),
              &done_loop, &mark_megamorphic);
        }

        BIND(&if_boundfunction);
        {
          // Continue with the [[BoundTargetFunction]] of {current}.
          var_current.Bind(LoadObjectField(
              current, JSBoundFunction::kBoundTargetFunctionOffset));
          Goto(&loop);
        }
      }
      BIND(&done_loop);

      // Create an AllocationSite if {target} and {new_target} refer
      // to the current native context's Array constructor.
      Label create_allocation_site(this), store_weak_reference(this);
      GotoIfNot(TaggedEqual(target, new_target), &store_weak_reference);
      TNode<Object> array_function = LoadContextElement(
          LoadNativeContext(context), Context::ARRAY_FUNCTION_INDEX);
      Branch(TaggedEqual(target, array_function), &create_allocation_site,
             &store_weak_reference);

      BIND(&create_allocation_site);
      {
        var_site.Bind(CreateAllocationSiteInFeedbackVector(feedback_vector,
                                                           SmiTag(slot_id)));
        ReportFeedbackUpdate(feedback_vector, slot_id,
                             "Construct:CreateAllocationSite");
        Goto(&construct_array);
      }

      BIND(&store_weak_reference);
      {
        StoreWeakReferenceInFeedbackVector(feedback_vector, slot_id,
                                           CAST(new_target));
        ReportFeedbackUpdate(feedback_vector, slot_id,
                             "Construct:StoreWeakReference");
        Goto(&construct);
      }
    }

    BIND(&mark_megamorphic);
    {
      // MegamorphicSentinel is an immortal immovable object so
      // write-barrier is not needed.
      Comment("transition to megamorphic");
      DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kmegamorphic_symbol));
      StoreFeedbackVectorSlot(
          feedback_vector, slot_id,
          HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())),
          SKIP_WRITE_BARRIER);
      ReportFeedbackUpdate(feedback_vector, slot_id,
                           "Construct:TransitionMegamorphic");
      Goto(&construct);
    }
  }

  BIND(&construct_array);
  {
    // TODO(bmeurer): Introduce a dedicated builtin to deal with the Array
    // constructor feedback collection inside of Ignition.
    Comment("call using ConstructArray builtin");
    Callable callable = CodeFactory::InterpreterPushArgsThenConstruct(
        isolate(), InterpreterPushArgsMode::kArrayFunction);
    TNode<Code> code_target = HeapConstant(callable.code());
    var_result.Bind(CallStub(callable.descriptor(), code_target, context,
                             args.reg_count(), args.base_reg_location(), target,
                             new_target, var_site.value()));
    Goto(&return_result);
  }

  BIND(&construct);
  {
    // TODO(bmeurer): Remove the generic type_info parameter from the Construct.
    Comment("call using Construct builtin");
    Callable callable = CodeFactory::InterpreterPushArgsThenConstruct(
        isolate(), InterpreterPushArgsMode::kOther);
    TNode<Code> code_target = HeapConstant(callable.code());
    var_result.Bind(CallStub(callable.descriptor(), code_target, context,
                             args.reg_count(), args.base_reg_location(), target,
                             new_target, UndefinedConstant()));
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

Node* InterpreterAssembler::ConstructWithSpread(Node* target, Node* context,
                                                Node* new_target,
                                                const RegListNodePair& args,
                                                Node* slot_id,
                                                Node* feedback_vector) {
  // TODO(bmeurer): Unify this with the Construct bytecode feedback
  // above once we have a way to pass the AllocationSite to the Array
  // constructor _and_ spread the last argument at the same time.
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  Label extra_checks(this, Label::kDeferred), construct(this);
  GotoIf(IsUndefined(feedback_vector), &construct);

  // Increment the call count.
  IncrementCallCount(feedback_vector, slot_id);

  // Check if we have monomorphic {new_target} feedback already.
  TNode<MaybeObject> feedback =
      LoadFeedbackVectorSlot(feedback_vector, slot_id);
  Branch(IsWeakReferenceTo(feedback, CAST(new_target)), &construct,
         &extra_checks);

  BIND(&extra_checks);
  {
    Label check_initialized(this), initialize(this), mark_megamorphic(this);

    // Check if it is a megamorphic {new_target}.
    Comment("check if megamorphic");
    TNode<BoolT> is_megamorphic = TaggedEqual(
        feedback, HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())));
    GotoIf(is_megamorphic, &construct);

    Comment("check if weak reference");
    GotoIfNot(IsWeakOrCleared(feedback), &check_initialized);

    // If the weak reference is cleared, we have a new chance to become
    // monomorphic.
    Comment("check if weak reference is cleared");
    Branch(IsCleared(feedback), &initialize, &mark_megamorphic);

    BIND(&check_initialized);
    {
      // Check if it is uninitialized.
      Comment("check if uninitialized");
      TNode<BoolT> is_uninitialized =
          TaggedEqual(feedback, UninitializedSymbolConstant());
      Branch(is_uninitialized, &initialize, &mark_megamorphic);
    }

    BIND(&initialize);
    {
      Comment("check if function in same native context");
      GotoIf(TaggedIsSmi(new_target), &mark_megamorphic);
      // Check if the {new_target} is a JSFunction or JSBoundFunction
      // in the current native context.
      VARIABLE(var_current, MachineRepresentation::kTagged, new_target);
      Label loop(this, &var_current), done_loop(this);
      Goto(&loop);
      BIND(&loop);
      {
        Label if_boundfunction(this), if_function(this);
        Node* current = var_current.value();
        CSA_ASSERT(this, TaggedIsNotSmi(current));
        TNode<Uint16T> current_instance_type = LoadInstanceType(current);
        GotoIf(InstanceTypeEqual(current_instance_type, JS_BOUND_FUNCTION_TYPE),
               &if_boundfunction);
        Branch(InstanceTypeEqual(current_instance_type, JS_FUNCTION_TYPE),
               &if_function, &mark_megamorphic);

        BIND(&if_function);
        {
          // Check that the JSFunction {current} is in the current native
          // context.
          TNode<Context> current_context =
              CAST(LoadObjectField(current, JSFunction::kContextOffset));
          TNode<Context> current_native_context =
              LoadNativeContext(current_context);
          Branch(
              TaggedEqual(LoadNativeContext(context), current_native_context),
              &done_loop, &mark_megamorphic);
        }

        BIND(&if_boundfunction);
        {
          // Continue with the [[BoundTargetFunction]] of {current}.
          var_current.Bind(LoadObjectField(
              current, JSBoundFunction::kBoundTargetFunctionOffset));
          Goto(&loop);
        }
      }
      BIND(&done_loop);
      StoreWeakReferenceInFeedbackVector(feedback_vector, slot_id,
                                         CAST(new_target));
      ReportFeedbackUpdate(feedback_vector, slot_id,
                           "ConstructWithSpread:Initialize");
      Goto(&construct);
    }

    BIND(&mark_megamorphic);
    {
      // MegamorphicSentinel is an immortal immovable object so
      // write-barrier is not needed.
      Comment("transition to megamorphic");
      DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kmegamorphic_symbol));
      StoreFeedbackVectorSlot(
          feedback_vector, slot_id,
          HeapConstant(FeedbackVector::MegamorphicSentinel(isolate())),
          SKIP_WRITE_BARRIER);
      ReportFeedbackUpdate(feedback_vector, slot_id,
                           "ConstructWithSpread:TransitionMegamorphic");
      Goto(&construct);
    }
  }

  BIND(&construct);
  Comment("call using ConstructWithSpread builtin");
  Callable callable = CodeFactory::InterpreterPushArgsThenConstruct(
      isolate(), InterpreterPushArgsMode::kWithFinalSpread);
  TNode<Code> code_target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), code_target, context, args.reg_count(),
                  args.base_reg_location(), target, new_target,
                  UndefinedConstant());
}

Node* InterpreterAssembler::CallRuntimeN(Node* function_id, Node* context,
                                         const RegListNodePair& args,
                                         int result_size) {
  DCHECK(Bytecodes::MakesCallAlongCriticalPath(bytecode_));
  DCHECK(Bytecodes::IsCallRuntime(bytecode_));
  Callable callable = CodeFactory::InterpreterCEntry(isolate(), result_size);
  TNode<Code> code_target = HeapConstant(callable.code());

  // Get the function entry from the function id.
  Node* function_table = ExternalConstant(
      ExternalReference::runtime_function_table_address(isolate()));
  TNode<Word32T> function_offset =
      Int32Mul(function_id, Int32Constant(sizeof(Runtime::Function)));
  TNode<WordT> function =
      IntPtrAdd(function_table, ChangeUint32ToWord(function_offset));
  Node* function_entry =
      Load(MachineType::Pointer(), function,
           IntPtrConstant(offsetof(Runtime::Function, entry)));

  return CallStubR(StubCallMode::kCallCodeObject, callable.descriptor(),
                   result_size, code_target, context, args.reg_count(),
                   args.base_reg_location(), function_entry);
}

void InterpreterAssembler::UpdateInterruptBudget(Node* weight, bool backward) {
  Comment("[ UpdateInterruptBudget");

  // Assert that the weight is positive (negative weights should be implemented
  // as backward updates).
  CSA_ASSERT(this, Int32GreaterThanOrEqual(weight, Int32Constant(0)));

  Label load_budget_from_bytecode(this), load_budget_done(this);
  TNode<JSFunction> function = CAST(LoadRegister(Register::function_closure()));
  TNode<FeedbackCell> feedback_cell =
      LoadObjectField<FeedbackCell>(function, JSFunction::kFeedbackCellOffset);
  TNode<Int32T> old_budget = LoadObjectField<Int32T>(
      feedback_cell, FeedbackCell::kInterruptBudgetOffset);

  // Make sure we include the current bytecode in the budget calculation.
  TNode<Int32T> budget_after_bytecode =
      Int32Sub(old_budget, Int32Constant(CurrentBytecodeSize()));

  Label done(this);
  TVARIABLE(Int32T, new_budget);
  if (backward) {
    // Update budget by |weight| and check if it reaches zero.
    new_budget = Signed(Int32Sub(budget_after_bytecode, weight));
    TNode<BoolT> condition =
        Int32GreaterThanOrEqual(new_budget.value(), Int32Constant(0));
    Label ok(this), interrupt_check(this, Label::kDeferred);
    Branch(condition, &ok, &interrupt_check);

    BIND(&interrupt_check);
    CallRuntime(Runtime::kBytecodeBudgetInterrupt, GetContext(), function);
    Goto(&done);

    BIND(&ok);
  } else {
    // For a forward jump, we know we only increase the interrupt budget, so
    // no need to check if it's below zero.
    new_budget = Signed(Int32Add(budget_after_bytecode, weight));
  }

  // Update budget.
  StoreObjectFieldNoWriteBarrier(
      feedback_cell, FeedbackCell::kInterruptBudgetOffset, new_budget.value(),
      MachineRepresentation::kWord32);
  Goto(&done);
  BIND(&done);
  Comment("] UpdateInterruptBudget");
}

TNode<IntPtrT> InterpreterAssembler::Advance() {
  return Advance(CurrentBytecodeSize());
}

TNode<IntPtrT> InterpreterAssembler::Advance(int delta) {
  return Advance(IntPtrConstant(delta));
}

TNode<IntPtrT> InterpreterAssembler::Advance(SloppyTNode<IntPtrT> delta,
                                             bool backward) {
#ifdef V8_TRACE_IGNITION
  TraceBytecode(Runtime::kInterpreterTraceBytecodeExit);
#endif
  TNode<IntPtrT> next_offset = backward ? IntPtrSub(BytecodeOffset(), delta)
                                        : IntPtrAdd(BytecodeOffset(), delta);
  bytecode_offset_ = next_offset;
  return next_offset;
}

Node* InterpreterAssembler::Jump(Node* delta, bool backward) {
  DCHECK(!Bytecodes::IsStarLookahead(bytecode_, operand_scale_));

  UpdateInterruptBudget(TruncateIntPtrToInt32(delta), backward);
  Node* new_bytecode_offset = Advance(delta, backward);
  TNode<WordT> target_bytecode = LoadBytecode(new_bytecode_offset);
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

void InterpreterAssembler::JumpIfTaggedEqual(TNode<Object> lhs,
                                             TNode<Object> rhs, Node* delta) {
  JumpConditional(TaggedEqual(lhs, rhs), delta);
}

void InterpreterAssembler::JumpIfTaggedNotEqual(TNode<Object> lhs,
                                                TNode<Object> rhs,
                                                Node* delta) {
  JumpConditional(TaggedNotEqual(lhs, rhs), delta);
}

TNode<WordT> InterpreterAssembler::LoadBytecode(Node* bytecode_offset) {
  Node* bytecode =
      Load(MachineType::Uint8(), BytecodeArrayTaggedPointer(), bytecode_offset);
  return ChangeUint32ToWord(bytecode);
}

TNode<WordT> InterpreterAssembler::StarDispatchLookahead(
    TNode<WordT> target_bytecode) {
  Label do_inline_star(this), done(this);

  TVARIABLE(WordT, var_bytecode, target_bytecode);

  TNode<Int32T> star_bytecode =
      Int32Constant(static_cast<int>(Bytecode::kStar));
  TNode<BoolT> is_star =
      Word32Equal(TruncateWordToInt32(target_bytecode), star_bytecode);
  Branch(is_star, &do_inline_star, &done);

  BIND(&do_inline_star);
  {
    InlineStar();
    var_bytecode = LoadBytecode(BytecodeOffset());
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
  StoreRegister(GetAccumulator(),
                BytecodeOperandReg(0, LoadSensitivity::kSafe));

  DCHECK_EQ(accumulator_use_, Bytecodes::GetAccumulatorUse(bytecode_));

  Advance();
  bytecode_ = previous_bytecode;
  accumulator_use_ = previous_acc_use;
}

Node* InterpreterAssembler::Dispatch() {
  Comment("========= Dispatch");
  DCHECK_IMPLIES(Bytecodes::MakesCallAlongCriticalPath(bytecode_), made_call_);
  Node* target_offset = Advance();
  TNode<WordT> target_bytecode = LoadBytecode(target_offset);

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
           TimesSystemPointerSize(target_bytecode));

  return DispatchToBytecodeHandlerEntry(target_code_entry, new_bytecode_offset,
                                        target_bytecode);
}

Node* InterpreterAssembler::DispatchToBytecodeHandler(Node* handler,
                                                      Node* bytecode_offset,
                                                      Node* target_bytecode) {
  // TODO(ishell): Add CSA::CodeEntryPoint(code).
  TNode<IntPtrT> handler_entry =
      IntPtrAdd(BitcastTaggedToWord(handler),
                IntPtrConstant(Code::kHeaderSize - kHeapObjectTag));
  return DispatchToBytecodeHandlerEntry(handler_entry, bytecode_offset,
                                        target_bytecode);
}

Node* InterpreterAssembler::DispatchToBytecodeHandlerEntry(
    Node* handler_entry, Node* bytecode_offset, Node* target_bytecode) {
  // Propagate speculation poisoning.
  TNode<WordT> poisoned_handler_entry = WordPoisonOnSpeculation(handler_entry);
  return TailCallBytecodeDispatch(
      InterpreterDispatchDescriptor{}, poisoned_handler_entry,
      GetAccumulatorUnchecked(), bytecode_offset, BytecodeArrayTaggedPointer(),
      DispatchTableRawPointer());
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
  TNode<WordT> next_bytecode = LoadBytecode(next_bytecode_offset);

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
  }
  TNode<WordT> target_index = IntPtrAdd(base_index, next_bytecode);
  Node* target_code_entry =
      Load(MachineType::Pointer(), DispatchTableRawPointer(),
           TimesSystemPointerSize(target_index));

  DispatchToBytecodeHandlerEntry(target_code_entry, next_bytecode_offset,
                                 next_bytecode);
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
  TNode<Int32T> profiling_weight =
      Int32Sub(TruncateIntPtrToInt32(BytecodeOffset()),
               Int32Constant(kFirstBytecodeOffset));
  UpdateInterruptBudget(profiling_weight, true);
}

Node* InterpreterAssembler::LoadOsrNestingLevel() {
  return LoadObjectField(BytecodeArrayTaggedPointer(),
                         BytecodeArray::kOsrNestingLevelOffset,
                         MachineType::Int8());
}

void InterpreterAssembler::Abort(AbortReason abort_reason) {
  TNode<Smi> abort_id = SmiConstant(abort_reason);
  CallRuntime(Runtime::kAbort, GetContext(), abort_id);
}

void InterpreterAssembler::AbortIfWordNotEqual(TNode<WordT> lhs,
                                               TNode<WordT> rhs,
                                               AbortReason abort_reason) {
  Label ok(this), abort(this, Label::kDeferred);
  Branch(WordEqual(lhs, rhs), &ok, &abort);

  BIND(&abort);
  Abort(abort_reason);
  Goto(&ok);

  BIND(&ok);
}

void InterpreterAssembler::MaybeDropFrames(Node* context) {
  TNode<ExternalReference> restart_fp_address =
      ExternalConstant(ExternalReference::debug_restart_fp_address(isolate()));

  TNode<IntPtrT> restart_fp = Load<IntPtrT>(restart_fp_address);
  TNode<IntPtrT> null = IntPtrConstant(0);

  Label ok(this), drop_frames(this);
  Branch(IntPtrEqual(restart_fp, null), &ok, &drop_frames);

  BIND(&drop_frames);
  // We don't expect this call to return since the frame dropper tears down
  // the stack and jumps into the function on the target frame to restart it.
  CallStub(CodeFactory::FrameDropperTrampoline(isolate()), context, restart_fp);
  Abort(AbortReason::kUnexpectedReturnFromFrameDropper);
  Goto(&ok);

  BIND(&ok);
}

void InterpreterAssembler::TraceBytecode(Runtime::FunctionId function_id) {
  CallRuntime(function_id, GetContext(), BytecodeArrayTaggedPointer(),
              SmiTag(BytecodeOffset()), GetAccumulatorUnchecked());
}

void InterpreterAssembler::TraceBytecodeDispatch(Node* target_bytecode) {
  TNode<ExternalReference> counters_table = ExternalConstant(
      ExternalReference::interpreter_dispatch_counters(isolate()));
  TNode<IntPtrT> source_bytecode_table_index = IntPtrConstant(
      static_cast<int>(bytecode_) * (static_cast<int>(Bytecode::kLast) + 1));

  TNode<WordT> counter_offset = TimesSystemPointerSize(
      IntPtrAdd(source_bytecode_table_index, target_bytecode));
  TNode<IntPtrT> old_counter = Load<IntPtrT>(counters_table, counter_offset);

  Label counter_ok(this), counter_saturated(this, Label::kDeferred);

  TNode<BoolT> counter_reached_max = WordEqual(
      old_counter, IntPtrConstant(std::numeric_limits<uintptr_t>::max()));
  Branch(counter_reached_max, &counter_saturated, &counter_ok);

  BIND(&counter_ok);
  {
    TNode<IntPtrT> new_counter = IntPtrAdd(old_counter, IntPtrConstant(1));
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
#elif V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390 || \
    V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_PPC
  return true;
#else
#error "Unknown Architecture"
#endif
}

void InterpreterAssembler::AbortIfRegisterCountInvalid(
    Node* parameters_and_registers, Node* formal_parameter_count,
    Node* register_count) {
  TNode<IntPtrT> array_size =
      LoadAndUntagFixedArrayBaseLength(parameters_and_registers);

  Label ok(this), abort(this, Label::kDeferred);
  Branch(UintPtrLessThanOrEqual(
             IntPtrAdd(formal_parameter_count, register_count), array_size),
         &ok, &abort);

  BIND(&abort);
  Abort(AbortReason::kInvalidParametersAndRegistersInGenerator);
  Goto(&ok);

  BIND(&ok);
}

Node* InterpreterAssembler::ExportParametersAndRegisterFile(
    TNode<FixedArray> array, const RegListNodePair& registers,
    TNode<Int32T> formal_parameter_count) {
  // Store the formal parameters (without receiver) followed by the
  // registers into the generator's internal parameters_and_registers field.
  TNode<IntPtrT> formal_parameter_count_intptr =
      ChangeInt32ToIntPtr(formal_parameter_count);
  TNode<UintPtrT> register_count = ChangeUint32ToWord(registers.reg_count());
  if (FLAG_debug_code) {
    CSA_ASSERT(this, IntPtrEqual(registers.base_reg_location(),
                                 RegisterLocation(Register(0))));
    AbortIfRegisterCountInvalid(array, formal_parameter_count_intptr,
                                register_count);
  }

  {
    Variable var_index(this, MachineType::PointerRepresentation());
    var_index.Bind(IntPtrConstant(0));

    // Iterate over parameters and write them into the array.
    Label loop(this, &var_index), done_loop(this);

    TNode<IntPtrT> reg_base = IntPtrAdd(
        IntPtrConstant(Register::FromParameterIndex(0, 1).ToOperand() - 1),
        formal_parameter_count_intptr);

    Goto(&loop);
    BIND(&loop);
    {
      Node* index = var_index.value();
      GotoIfNot(UintPtrLessThan(index, formal_parameter_count_intptr),
                &done_loop);

      TNode<WordT> reg_index = IntPtrSub(reg_base, index);
      TNode<Object> value = LoadRegister(reg_index);

      StoreFixedArrayElement(array, index, value);

      var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
      Goto(&loop);
    }
    BIND(&done_loop);
  }

  {
    // Iterate over register file and write values into array.
    // The mapping of register to array index must match that used in
    // BytecodeGraphBuilder::VisitResumeGenerator.
    Variable var_index(this, MachineType::PointerRepresentation());
    var_index.Bind(IntPtrConstant(0));

    Label loop(this, &var_index), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* index = var_index.value();
      GotoIfNot(UintPtrLessThan(index, register_count), &done_loop);

      TNode<WordT> reg_index =
          IntPtrSub(IntPtrConstant(Register(0).ToOperand()), index);
      TNode<Object> value = LoadRegister(reg_index);

      TNode<WordT> array_index =
          IntPtrAdd(formal_parameter_count_intptr, index);
      StoreFixedArrayElement(array, array_index, value);

      var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
      Goto(&loop);
    }
    BIND(&done_loop);
  }

  return array;
}

Node* InterpreterAssembler::ImportRegisterFile(
    TNode<FixedArray> array, const RegListNodePair& registers,
    TNode<Int32T> formal_parameter_count) {
  TNode<IntPtrT> formal_parameter_count_intptr =
      ChangeInt32ToIntPtr(formal_parameter_count);
  TNode<UintPtrT> register_count = ChangeUint32ToWord(registers.reg_count());
  if (FLAG_debug_code) {
    CSA_ASSERT(this, IntPtrEqual(registers.base_reg_location(),
                                 RegisterLocation(Register(0))));
    AbortIfRegisterCountInvalid(array, formal_parameter_count_intptr,
                                register_count);
  }

  TVARIABLE(IntPtrT, var_index, IntPtrConstant(0));

  // Iterate over array and write values into register file.  Also erase the
  // array contents to not keep them alive artificially.
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> index = var_index.value();
    GotoIfNot(UintPtrLessThan(index, register_count), &done_loop);

    TNode<IntPtrT> array_index =
        IntPtrAdd(formal_parameter_count_intptr, index);
    TNode<Object> value = LoadFixedArrayElement(array, array_index);

    TNode<IntPtrT> reg_index =
        IntPtrSub(IntPtrConstant(Register(0).ToOperand()), index);
    StoreRegister(value, reg_index);

    StoreFixedArrayElement(array, array_index, StaleRegisterConstant());

    var_index = IntPtrAdd(index, IntPtrConstant(1));
    Goto(&loop);
  }
  BIND(&done_loop);

  return array;
}

int InterpreterAssembler::CurrentBytecodeSize() const {
  return Bytecodes::Size(bytecode_, operand_scale_);
}

void InterpreterAssembler::ToNumberOrNumeric(Object::Conversion mode) {
  TNode<Object> object = GetAccumulator();
  TNode<Context> context = GetContext();

  Variable var_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Variable var_result(this, MachineRepresentation::kTagged);
  Label if_done(this), if_objectissmi(this), if_objectisheapnumber(this),
      if_objectisother(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(object), &if_objectissmi);
  Branch(IsHeapNumber(CAST(object)), &if_objectisheapnumber, &if_objectisother);

  BIND(&if_objectissmi);
  {
    var_result.Bind(object);
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kSignedSmall));
    Goto(&if_done);
  }

  BIND(&if_objectisheapnumber);
  {
    var_result.Bind(object);
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Goto(&if_done);
  }

  BIND(&if_objectisother);
  {
    auto builtin = Builtins::kNonNumberToNumber;
    if (mode == Object::Conversion::kToNumeric) {
      builtin = Builtins::kNonNumberToNumeric;
      // Special case for collecting BigInt feedback.
      Label not_bigint(this);
      GotoIfNot(IsBigInt(CAST(object)), &not_bigint);
      {
        var_result.Bind(object);
        var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kBigInt));
        Goto(&if_done);
      }
      BIND(&not_bigint);
    }

    // Convert {object} by calling out to the appropriate builtin.
    var_result.Bind(CallBuiltin(builtin, context, object));
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&if_done);
  }

  BIND(&if_done);

  // Record the type feedback collected for {object}.
  Node* slot_index = BytecodeOperandIdx(0);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

  UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector, slot_index);

  SetAccumulator(var_result.value());
  Dispatch();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
