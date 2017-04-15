// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-accessor.h"

#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayAccessor::BytecodeArrayAccessor(
    Handle<BytecodeArray> bytecode_array, int initial_offset)
    : bytecode_array_(bytecode_array),
      bytecode_offset_(initial_offset),
      operand_scale_(OperandScale::kSingle),
      prefix_offset_(0) {
  UpdateOperandScale();
}

void BytecodeArrayAccessor::SetOffset(int offset) {
  bytecode_offset_ = offset;
  UpdateOperandScale();
}

void BytecodeArrayAccessor::UpdateOperandScale() {
  if (OffsetInBounds()) {
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

bool BytecodeArrayAccessor::OffsetInBounds() const {
  return bytecode_offset_ >= 0 && bytecode_offset_ < bytecode_array()->length();
}

Bytecode BytecodeArrayAccessor::current_bytecode() const {
  DCHECK(OffsetInBounds());
  uint8_t current_byte =
      bytecode_array()->get(bytecode_offset_ + current_prefix_offset());
  Bytecode current_bytecode = Bytecodes::FromByte(current_byte);
  DCHECK(!Bytecodes::IsPrefixScalingBytecode(current_bytecode));
  return current_bytecode;
}

int BytecodeArrayAccessor::current_bytecode_size() const {
  return current_prefix_offset() +
         Bytecodes::Size(current_bytecode(), current_operand_scale());
}

uint32_t BytecodeArrayAccessor::GetUnsignedOperand(
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

int32_t BytecodeArrayAccessor::GetSignedOperand(
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

uint32_t BytecodeArrayAccessor::GetFlagOperand(int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kFlag8);
  return GetUnsignedOperand(operand_index, OperandType::kFlag8);
}

uint32_t BytecodeArrayAccessor::GetUnsignedImmediateOperand(
    int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kUImm);
  return GetUnsignedOperand(operand_index, OperandType::kUImm);
}

int32_t BytecodeArrayAccessor::GetImmediateOperand(int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kImm);
  return GetSignedOperand(operand_index, OperandType::kImm);
}

uint32_t BytecodeArrayAccessor::GetRegisterCountOperand(
    int operand_index) const {
  DCHECK_EQ(Bytecodes::GetOperandType(current_bytecode(), operand_index),
            OperandType::kRegCount);
  return GetUnsignedOperand(operand_index, OperandType::kRegCount);
}

uint32_t BytecodeArrayAccessor::GetIndexOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK_EQ(operand_type, OperandType::kIdx);
  return GetUnsignedOperand(operand_index, operand_type);
}

Register BytecodeArrayAccessor::GetRegisterOperand(int operand_index) const {
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

int BytecodeArrayAccessor::GetRegisterOperandRange(int operand_index) const {
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

Runtime::FunctionId BytecodeArrayAccessor::GetRuntimeIdOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK(operand_type == OperandType::kRuntimeId);
  uint32_t raw_id = GetUnsignedOperand(operand_index, operand_type);
  return static_cast<Runtime::FunctionId>(raw_id);
}

Runtime::FunctionId BytecodeArrayAccessor::GetIntrinsicIdOperand(
    int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK(operand_type == OperandType::kIntrinsicId);
  uint32_t raw_id = GetUnsignedOperand(operand_index, operand_type);
  return IntrinsicsHelper::ToRuntimeId(
      static_cast<IntrinsicsHelper::IntrinsicId>(raw_id));
}

Handle<Object> BytecodeArrayAccessor::GetConstantForIndexOperand(
    int operand_index) const {
  return FixedArray::get(bytecode_array()->constant_pool(),
                         GetIndexOperand(operand_index),
                         bytecode_array()->GetIsolate());
}

int BytecodeArrayAccessor::GetJumpTargetOffset() const {
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

bool BytecodeArrayAccessor::OffsetWithinBytecode(int offset) const {
  return current_offset() <= offset &&
         offset < current_offset() + current_bytecode_size();
}

std::ostream& BytecodeArrayAccessor::PrintTo(std::ostream& os) const {
  return BytecodeDecoder::Decode(
      os, bytecode_array()->GetFirstBytecodeAddress() + bytecode_offset_,
      bytecode_array()->parameter_count());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
