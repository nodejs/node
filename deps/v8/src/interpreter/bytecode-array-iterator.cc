// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-iterator.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayIterator::BytecodeArrayIterator(
    Handle<BytecodeArray> bytecode_array)
    : bytecode_array_(bytecode_array), bytecode_offset_(0) {}


void BytecodeArrayIterator::Advance() {
  bytecode_offset_ += Bytecodes::Size(current_bytecode());
}


bool BytecodeArrayIterator::done() const {
  return bytecode_offset_ >= bytecode_array()->length();
}


Bytecode BytecodeArrayIterator::current_bytecode() const {
  DCHECK(!done());
  uint8_t current_byte = bytecode_array()->get(bytecode_offset_);
  return interpreter::Bytecodes::FromByte(current_byte);
}


int BytecodeArrayIterator::current_bytecode_size() const {
  return Bytecodes::Size(current_bytecode());
}


uint32_t BytecodeArrayIterator::GetRawOperand(int operand_index,
                                              OperandType operand_type) const {
  DCHECK_GE(operand_index, 0);
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  DCHECK_EQ(operand_type,
            Bytecodes::GetOperandType(current_bytecode(), operand_index));
  uint8_t* operand_start =
      bytecode_array()->GetFirstBytecodeAddress() + bytecode_offset_ +
      Bytecodes::GetOperandOffset(current_bytecode(), operand_index);
  switch (Bytecodes::SizeOfOperand(operand_type)) {
    default:
    case OperandSize::kNone:
      UNREACHABLE();
    case OperandSize::kByte:
      return static_cast<uint32_t>(*operand_start);
    case OperandSize::kShort:
      return ReadUnalignedUInt16(operand_start);
  }
}


int8_t BytecodeArrayIterator::GetImmediateOperand(int operand_index) const {
  uint32_t operand = GetRawOperand(operand_index, OperandType::kImm8);
  return static_cast<int8_t>(operand);
}


int BytecodeArrayIterator::GetCountOperand(int operand_index) const {
  OperandSize size =
      Bytecodes::GetOperandSize(current_bytecode(), operand_index);
  OperandType type = (size == OperandSize::kByte) ? OperandType::kCount8
                                                  : OperandType::kCount16;
  uint32_t operand = GetRawOperand(operand_index, type);
  return static_cast<int>(operand);
}


int BytecodeArrayIterator::GetIndexOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK(operand_type == OperandType::kIdx8 ||
         operand_type == OperandType::kIdx16);
  uint32_t operand = GetRawOperand(operand_index, operand_type);
  return static_cast<int>(operand);
}


Register BytecodeArrayIterator::GetRegisterOperand(int operand_index) const {
  OperandType operand_type =
      Bytecodes::GetOperandType(current_bytecode(), operand_index);
  DCHECK(operand_type == OperandType::kReg8 ||
         operand_type == OperandType::kRegPair8 ||
         operand_type == OperandType::kMaybeReg8 ||
         operand_type == OperandType::kReg16);
  uint32_t operand = GetRawOperand(operand_index, operand_type);
  return Register::FromOperand(operand);
}


Handle<Object> BytecodeArrayIterator::GetConstantForIndexOperand(
    int operand_index) const {
  Handle<FixedArray> constants = handle(bytecode_array()->constant_pool());
  return FixedArray::get(constants, GetIndexOperand(operand_index));
}


int BytecodeArrayIterator::GetJumpTargetOffset() const {
  Bytecode bytecode = current_bytecode();
  if (interpreter::Bytecodes::IsJumpImmediate(bytecode)) {
    int relative_offset = GetImmediateOperand(0);
    return current_offset() + relative_offset;
  } else if (interpreter::Bytecodes::IsJumpConstant(bytecode) ||
             interpreter::Bytecodes::IsJumpConstantWide(bytecode)) {
    Smi* smi = Smi::cast(*GetConstantForIndexOperand(0));
    return current_offset() + smi->value();
  } else {
    UNREACHABLE();
    return kMinInt;
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
