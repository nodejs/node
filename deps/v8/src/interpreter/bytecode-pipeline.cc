// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-pipeline.h"

#include <iomanip>
#include "src/source-position-table.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeNode::BytecodeNode(Bytecode bytecode) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 0);
  bytecode_ = bytecode;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 1);
  bytecode_ = bytecode;
  operands_[0] = operand0;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           uint32_t operand1) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 2);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operands_[1] = operand1;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           uint32_t operand1, uint32_t operand2) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 3);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operands_[1] = operand1;
  operands_[2] = operand2;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           uint32_t operand1, uint32_t operand2,
                           uint32_t operand3) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 4);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operands_[1] = operand1;
  operands_[2] = operand2;
  operands_[3] = operand3;
}

BytecodeNode::BytecodeNode(const BytecodeNode& other) {
  memcpy(this, &other, sizeof(other));
}

BytecodeNode& BytecodeNode::operator=(const BytecodeNode& other) {
  memcpy(this, &other, sizeof(other));
  return *this;
}

void BytecodeNode::Clone(const BytecodeNode* const other) {
  memcpy(this, other, sizeof(*other));
}

void BytecodeNode::Print(std::ostream& os) const {
#ifdef DEBUG
  std::ios saved_state(nullptr);
  saved_state.copyfmt(os);
  os << Bytecodes::ToString(bytecode_);

  for (int i = 0; i < operand_count(); ++i) {
    os << ' ' << std::setw(8) << std::setfill('0') << std::hex << operands_[i];
  }
  os.copyfmt(saved_state);

  if (source_info_.is_valid()) {
    os << ' ' << source_info_;
  }
  os << '\n';
#else
  os << static_cast<const void*>(this);
#endif  // DEBUG
}

void BytecodeNode::Transform(Bytecode new_bytecode, uint32_t extra_operand) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(new_bytecode),
            Bytecodes::NumberOfOperands(bytecode()) + 1);
  DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 1 ||
         Bytecodes::GetOperandType(new_bytecode, 0) ==
             Bytecodes::GetOperandType(bytecode(), 0));
  DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 2 ||
         Bytecodes::GetOperandType(new_bytecode, 1) ==
             Bytecodes::GetOperandType(bytecode(), 1));
  DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 3 ||
         Bytecodes::GetOperandType(new_bytecode, 2) ==
             Bytecodes::GetOperandType(bytecode(), 2));
  DCHECK(Bytecodes::NumberOfOperands(bytecode()) < 4);
  operands_[operand_count()] = extra_operand;
  bytecode_ = new_bytecode;
}

bool BytecodeNode::operator==(const BytecodeNode& other) const {
  if (this == &other) {
    return true;
  } else if (this->bytecode() != other.bytecode() ||
             this->source_info() != other.source_info()) {
    return false;
  } else {
    for (int i = 0; i < this->operand_count(); ++i) {
      if (this->operand(i) != other.operand(i)) {
        return false;
      }
    }
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const BytecodeSourceInfo& info) {
  if (info.is_valid()) {
    char description = info.is_statement() ? 'S' : 'E';
    os << info.source_position() << ' ' << description << '>';
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const BytecodeNode& node) {
  node.Print(os);
  return os;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
