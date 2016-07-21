// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-writer.h"

#include <iomanip>
#include "src/interpreter/source-position-table.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayWriter::BytecodeArrayWriter(
    Zone* zone, SourcePositionTableBuilder* source_position_table_builder)
    : bytecodes_(zone),
      max_register_count_(0),
      source_position_table_builder_(source_position_table_builder) {}

// override
BytecodeArrayWriter::~BytecodeArrayWriter() {}

// override
size_t BytecodeArrayWriter::FlushForOffset() { return bytecodes()->size(); }

// override
void BytecodeArrayWriter::Write(BytecodeNode* node) {
  UpdateSourcePositionTable(node);
  EmitBytecode(node);
}

void BytecodeArrayWriter::UpdateSourcePositionTable(
    const BytecodeNode* const node) {
  int bytecode_offset = static_cast<int>(bytecodes()->size());
  const BytecodeSourceInfo& source_info = node->source_info();
  if (source_info.is_valid()) {
    source_position_table_builder_->AddPosition(bytecode_offset,
                                                source_info.source_position(),
                                                source_info.is_statement());
  }
}

void BytecodeArrayWriter::EmitBytecode(const BytecodeNode* const node) {
  DCHECK_NE(node->bytecode(), Bytecode::kIllegal);

  OperandScale operand_scale = node->operand_scale();
  if (operand_scale != OperandScale::kSingle) {
    Bytecode prefix = Bytecodes::OperandScaleToPrefixBytecode(operand_scale);
    bytecodes()->push_back(Bytecodes::ToByte(prefix));
  }

  Bytecode bytecode = node->bytecode();
  bytecodes()->push_back(Bytecodes::ToByte(bytecode));

  int register_operand_bitmap = Bytecodes::GetRegisterOperandBitmap(bytecode);
  const uint32_t* const operands = node->operands();
  const OperandType* operand_types = Bytecodes::GetOperandTypes(bytecode);
  for (int i = 0; operand_types[i] != OperandType::kNone; ++i) {
    OperandType operand_type = operand_types[i];
    switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
      case OperandSize::kNone:
        UNREACHABLE();
        break;
      case OperandSize::kByte:
        bytecodes()->push_back(static_cast<uint8_t>(operands[i]));
        break;
      case OperandSize::kShort: {
        uint8_t operand_bytes[2];
        WriteUnalignedUInt16(operand_bytes, operands[i]);
        bytecodes()->insert(bytecodes()->end(), operand_bytes,
                            operand_bytes + 2);
        break;
      }
      case OperandSize::kQuad: {
        uint8_t operand_bytes[4];
        WriteUnalignedUInt32(operand_bytes, operands[i]);
        bytecodes()->insert(bytecodes()->end(), operand_bytes,
                            operand_bytes + 4);
        break;
      }
    }

    if ((register_operand_bitmap >> i) & 1) {
      int count;
      if (operand_types[i + 1] == OperandType::kRegCount) {
        count = static_cast<int>(operands[i + 1]);
      } else {
        count = Bytecodes::GetNumberOfRegistersRepresentedBy(operand_type);
      }
      Register reg = Register::FromOperand(static_cast<int32_t>(operands[i]));
      max_register_count_ = std::max(max_register_count_, reg.index() + count);
    }
  }
}

// override
void BytecodeArrayWriter::FlushBasicBlock() {}

int BytecodeArrayWriter::GetMaximumFrameSizeUsed() {
  return max_register_count_ * kPointerSize;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
