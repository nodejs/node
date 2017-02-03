// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-iterator.h"

#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayIterator::BytecodeArrayIterator(
    Handle<BytecodeArray> bytecode_array)
    : bytecode_array_(bytecode_array),
      bytecode_offset_(0),
      operand_scale_(OperandScale::kSingle),
      prefix_offset_(0) {
  UpdateOperandScale();
}

void BytecodeArrayIterator::Advance() {
  bytecode_offset_ += current_bytecode_size();
  UpdateOperandScale();
}

void BytecodeArrayIterator::UpdateOperandScale() {
  if (!done()) {
    uint8_t current_byte = bytecode_array()->get(bytecode_offset_);
    Bytecode current_bytecode = Bytecodes::FromByte(current_byte);
    if (Bytecodes::IsPrefixScalingBytecode(current_bytecode)) {
      operand_scale_ =
          Bytecodes::PrefixBytecodeToOperandScale(current_bytecode);
      prefix_offset_ = 1;
    } else {
      operand_scale_ = OperandScale::kSingle;
      prefix_offset_ = 0;
    }
  }
}

bool BytecodeArrayIterator::done() const {
  return bytecode_offset_ >= bytecode_array()->length();
}

Bytecode BytecodeArrayIterator::current_bytecode() const {
  DCHECK(!done());
  uint8_t current_byte =
      bytecode_array()->get(bytecode_offset_ + current_prefix_offset());
  Bytecode current_bytecode = Bytecodes::FromByte(current_byte);
  DCHECK(!Bytecodes::IsPrefixScalingBytecode(current_bytecode));
  return current_bytecode;
}

int BytecodeArrayIterator::current_bytecode_size() const {
  return current_prefix_offset() +
         Bytecodes::Size(current_bytecode(), current_operand_scale());
}

uint32_t BytecodeArrayIterator::GetUnsignedOperand(
    int operand_index, OperandType operand_type) const {
  DCHECK_GE(operand_index, 0);
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  DCHECK_EQ(operand_type,
            Bytecodes::GetOperandType(current_bytecode(), operand_index));
  DCHECK(Bytecodes::IsUnsignedOperandType(operand_type));
  const uint8_t* operand_start =
      bytecode_array()->GetFirstBytecodeAddress() + bytecode_offset_ +
      current_prefix_offset() +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index,
                                  current_operand_scale());
  return BytecodeDecoder::DecodeUnsignedOperand(operand_start, operand_type,
                                                current_operand_scale());
}

int32_t BytecodeArrayIterator::GetSignedOperand(
    int operand_index, OperandType operand_type) const {
  DCHECK_GE(operand_index, 0);
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  DCHECK_EQ(operand_type,
            Bytecodes::GetOperandType(current_bytecode(), operand_index));
  DCHECK(!Bytecodes::IsUnsignedOperandType(operand_type));
  const uint8_t* operand_start =
      bytecode_array()->GetFirstBytecodeAddress() + bytecode_offset_ +
      current_prefix_offset() +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index,
                                  current_operand_scale());
  return BytecodeDecoder::DecodeSignedOperand(operand_start, operand_type,
                                              current_operand_scale());
}

uint32_t BytecodeArrayIterator::GetFlagOperand(int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kFlag8);
  return GetUnsignedOperand(operand_index, OperandType::kFlag8);
}

uint32_t BytecodeArrayIterator::GetUnsignedImmediateOperand(
    int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kUImm);
  return GetUnsignedOperand(operand_index, OperandType::kUImm);
}

int32_t BytecodeArrayIterator::GetImmediateOperand(int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kImm);
  return GetSignedOperand(operand_index, OperandType::kImm);
}

uint32_t BytecodeArrayIterator::GetRegisterCountOperand(
    int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kRegCount);
  return GetUnsignedOperand(operand_index, OperandType::kRegCount);
}

uint32_t BytecodeArrayIterator::GetIndexOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK_EQ(operand_type, OperandType::kIdx);
  return GetUnsignedOperand(operand_index, operand_type);
}

Register BytecodeArrayIterator::GetRegisterOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  const uint8_t* operand_start =
      bytecode_array()->GetFirstBytecodeAddress() + bytecode_offset_ +
      current_prefix_offset() +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index,
                                  current_operand_scale());
  return BytecodeDecoder::DecodeRegisterOperand(operand_start, operand_type,
                                                current_operand_scale());
}

int BytecodeArrayIterator::GetRegisterOperandRange(int operand_index) const {
  DCHECK_LE(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  const OperandType* operand_types =
      Bytecodes::GetOperandTypes(current_bytecode());
  OperandType operand_type = operand_types[operand_index];
  DCHECK(Bytecodes::IsRegisterOperandType(operand_type));
  if (operand_type == OperandType::kRegList) {
    return GetRegisterCountOperand(operand_index + 1);
  } else {
    return Bytecodes::GetNumberOfRegistersRepresentedBy(operand_type);
  }
}

Runtime::FunctionId BytecodeArrayIterator::GetRuntimeIdOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK(operand_type == OperandType::kRuntimeId);
  uint32_t raw_id = GetUnsignedOperand(operand_index, operand_type);
  return static_cast<Runtime::FunctionId>(raw_id);
}

Runtime::FunctionId BytecodeArrayIterator::GetIntrinsicIdOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK(operand_type == OperandType::kIntrinsicId);
  uint32_t raw_id = GetUnsignedOperand(operand_index, operand_type);
  return IntrinsicsHelper::ToRuntimeId(
      static_cast<IntrinsicsHelper::IntrinsicId>(raw_id));
}

Handle<Object> BytecodeArrayIterator::GetConstantForIndexOperand(
    int operand_index) const {
  return FixedArray::get(bytecode_array()->constant_pool(),
                         GetIndexOperand(operand_index),
                         bytecode_array()->GetIsolate());
}


int BytecodeArrayIterator::GetJumpTargetOffset() const {
  Bytecode bytecode = current_bytecode();
  if (interpreter::Bytecodes::IsJumpImmediate(bytecode)) {
    int relative_offset = GetImmediateOperand(0);
    return current_offset() + relative_offset + current_prefix_offset();
  } else if (interpreter::Bytecodes::IsJumpConstant(bytecode)) {
    Smi* smi = Smi::cast(*GetConstantForIndexOperand(0));
    return current_offset() + smi->value() + current_prefix_offset();
  } else {
    UNREACHABLE();
    return kMinInt;
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
