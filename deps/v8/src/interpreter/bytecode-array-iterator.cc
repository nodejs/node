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


uint8_t BytecodeArrayIterator::GetRawOperand(int operand_index,
                                             OperandType operand_type) const {
  DCHECK_GE(operand_index, 0);
  DCHECK_LT(operand_index, Bytecodes::NumberOfOperands(current_bytecode()));
  DCHECK_EQ(operand_type,
            Bytecodes::GetOperandType(current_bytecode(), operand_index));
  int operands_start = bytecode_offset_ + 1;
  return bytecode_array()->get(operands_start + operand_index);
}


int8_t BytecodeArrayIterator::GetSmi8Operand(int operand_index) const {
  uint8_t operand = GetRawOperand(operand_index, OperandType::kImm8);
  return static_cast<int8_t>(operand);
}


int BytecodeArrayIterator::GetIndexOperand(int operand_index) const {
  uint8_t operand = GetRawOperand(operand_index, OperandType::kIdx);
  return static_cast<int>(operand);
}


Register BytecodeArrayIterator::GetRegisterOperand(int operand_index) const {
  uint8_t operand = GetRawOperand(operand_index, OperandType::kReg);
  return Register::FromOperand(operand);
}


Handle<Object> BytecodeArrayIterator::GetConstantForIndexOperand(
    int operand_index) const {
  Handle<FixedArray> constants = handle(bytecode_array()->constant_pool());
  return FixedArray::get(constants, GetIndexOperand(operand_index));
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
