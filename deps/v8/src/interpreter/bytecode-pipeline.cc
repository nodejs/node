// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-pipeline.h"

#include <iomanip>
#include "src/interpreter/source-position-table.h"

namespace v8 {
namespace internal {
namespace interpreter {

void BytecodeSourceInfo::Update(const BytecodeSourceInfo& entry) {
  DCHECK(entry.is_valid());
  if (!is_valid() || (entry.is_statement() && !is_statement()) ||
      (entry.is_statement() && is_statement() &&
       entry.source_position() > source_position())) {
    // Position is updated if any of the following conditions are met:
    //   (1) there is no existing position.
    //   (2) the incoming position is a statement and the current position
    //       is an expression.
    //   (3) the existing position is a statement and the incoming
    //       statement has a later source position.
    // Condition 3 is needed for the first statement in a function which
    // may end up with later statement positions being added during bytecode
    // generation.
    source_position_ = entry.source_position_;
    is_statement_ = entry.is_statement_;
  }
}

BytecodeNode::BytecodeNode(Bytecode bytecode) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 0);
  bytecode_ = bytecode;
  operand_scale_ = OperandScale::kSingle;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           OperandScale operand_scale) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 1);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operand_scale_ = operand_scale;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           uint32_t operand1, OperandScale operand_scale) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 2);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operands_[1] = operand1;
  operand_scale_ = operand_scale;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           uint32_t operand1, uint32_t operand2,
                           OperandScale operand_scale) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 3);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operands_[1] = operand1;
  operands_[2] = operand2;
  operand_scale_ = operand_scale;
}

BytecodeNode::BytecodeNode(Bytecode bytecode, uint32_t operand0,
                           uint32_t operand1, uint32_t operand2,
                           uint32_t operand3, OperandScale operand_scale) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 4);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operands_[1] = operand1;
  operands_[2] = operand2;
  operands_[3] = operand3;
  operand_scale_ = operand_scale;
}

void BytecodeNode::set_bytecode(Bytecode bytecode) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 0);
  bytecode_ = bytecode;
  operand_scale_ = OperandScale::kSingle;
}

void BytecodeNode::set_bytecode(Bytecode bytecode, uint32_t operand0,
                                OperandScale operand_scale) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), 1);
  bytecode_ = bytecode;
  operands_[0] = operand0;
  operand_scale_ = operand_scale;
}

size_t BytecodeNode::Size() const {
  size_t size = Bytecodes::Size(bytecode_, operand_scale_);
  if (Bytecodes::OperandScaleRequiresPrefixBytecode(operand_scale_)) {
    size += 1;
  }
  return size;
}

void BytecodeNode::Print(std::ostream& os) const {
#ifdef DEBUG
  std::ios saved_state(nullptr);
  saved_state.copyfmt(os);

  os << Bytecodes::ToString(bytecode_);
  if (Bytecodes::OperandScaleRequiresPrefixBytecode(operand_scale_)) {
    Bytecode scale_prefix =
        Bytecodes::OperandScaleToPrefixBytecode(operand_scale_);
    os << '.' << Bytecodes::ToString(scale_prefix);
  }

  for (int i = 0; i < operand_count(); ++i) {
    os << ' ' << std::setw(8) << std::setfill('0') << std::hex << operands_[i];
  }
  os.copyfmt(saved_state);

  if (source_info_.is_valid()) {
    os << source_info_;
  }
  os << '\n';
#else
  os << static_cast<const void*>(this);
#endif  // DEBUG
}

void BytecodeNode::Clone(const BytecodeNode* const other) {
  memcpy(this, other, sizeof(*other));
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

std::ostream& operator<<(std::ostream& os, const BytecodeNode& node) {
  node.Print(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const BytecodeSourceInfo& info) {
  if (info.is_valid()) {
    char description = info.is_statement() ? 'S' : 'E';
    os << info.source_position() << ' ' << description << '>';
  }
  return os;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
