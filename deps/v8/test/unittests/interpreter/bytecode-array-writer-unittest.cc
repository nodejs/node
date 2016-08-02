// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-array-writer.h"
#include "src/interpreter/source-position-table.h"
#include "src/isolate.h"
#include "src/utils.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayWriterUnittest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayWriterUnittest()
      : source_position_builder_(isolate(), zone()),
        bytecode_array_writer_(zone(), &source_position_builder_) {}
  ~BytecodeArrayWriterUnittest() override {}

  void Write(BytecodeNode* node, const BytecodeSourceInfo& info);
  void Write(Bytecode bytecode,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, OperandScale operand_scale,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             OperandScale operand_scale,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             uint32_t operand2, OperandScale operand_scale,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             uint32_t operand2, uint32_t operand3, OperandScale operand_scale,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());

  SourcePositionTableBuilder* source_position_builder() {
    return &source_position_builder_;
  }
  BytecodeArrayWriter* writer() { return &bytecode_array_writer_; }

 private:
  SourcePositionTableBuilder source_position_builder_;
  BytecodeArrayWriter bytecode_array_writer_;
};

void BytecodeArrayWriterUnittest::Write(BytecodeNode* node,
                                        const BytecodeSourceInfo& info) {
  if (info.is_valid()) {
    node->source_info().Update(info);
  }
  writer()->Write(node);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        OperandScale operand_scale,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand_scale);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1,
                                        OperandScale operand_scale,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand1, operand_scale);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1, uint32_t operand2,
                                        OperandScale operand_scale,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand1, operand2, operand_scale);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1, uint32_t operand2,
                                        uint32_t operand3,
                                        OperandScale operand_scale,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand1, operand2, operand3,
                    operand_scale);
  Write(&node, info);
}

TEST_F(BytecodeArrayWriterUnittest, SimpleExample) {
  CHECK_EQ(writer()->bytecodes()->size(), 0);

  Write(Bytecode::kStackCheck, {10, false});
  CHECK_EQ(writer()->bytecodes()->size(), 1);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 0);

  Write(Bytecode::kLdaSmi, 0xff, OperandScale::kSingle, {55, true});
  CHECK_EQ(writer()->bytecodes()->size(), 3);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 0);

  Write(Bytecode::kLdar, Register(1).ToOperand(), OperandScale::kDouble);
  CHECK_EQ(writer()->bytecodes()->size(), 7);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 2 * kPointerSize);

  Write(Bytecode::kReturn, {70, true});
  CHECK_EQ(writer()->bytecodes()->size(), 8);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 2 * kPointerSize);

  static const uint8_t bytes[] = {B(StackCheck), B(LdaSmi), U8(0xff), B(Wide),
                                  B(Ldar),       R16(1),    B(Return)};
  CHECK_EQ(writer()->bytecodes()->size(), arraysize(bytes));
  for (size_t i = 0; i < arraysize(bytes); ++i) {
    CHECK_EQ(writer()->bytecodes()->at(i), bytes[i]);
  }

  CHECK_EQ(writer()->FlushForOffset(), arraysize(bytes));
  writer()->FlushBasicBlock();
  CHECK_EQ(writer()->bytecodes()->size(), arraysize(bytes));

  PositionTableEntry expected_positions[] = {
      {0, 10, false}, {1, 55, true}, {7, 70, true}};
  Handle<ByteArray> source_positions =
      source_position_builder()->ToSourcePositionTable();
  SourcePositionTableIterator source_iterator(*source_positions);
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.bytecode_offset(), expected.bytecode_offset);
    CHECK_EQ(source_iterator.source_position(), expected.source_position);
    CHECK_EQ(source_iterator.is_statement(), expected.is_statement);
    source_iterator.Advance();
  }
  CHECK(source_iterator.done());
}

TEST_F(BytecodeArrayWriterUnittest, ComplexExample) {
  static const uint8_t expected_bytes[] = {
      // clang-format off
      /*  0 30 E> */ B(StackCheck),
      /*  1 42 S> */ B(LdaConstant), U8(0),
      /*  3 42 E> */ B(Star), R8(1),
      /*  5 68 S> */ B(JumpIfUndefined), U8(38),
      /*  7       */ B(JumpIfNull), U8(36),
      /*  9       */ B(ToObject),
      /* 10       */ B(Star), R8(3),
      /* 12       */ B(ForInPrepare), R8(4),
      /* 14       */ B(LdaZero),
      /* 15       */ B(Star), R8(7),
      /* 17 63 S> */ B(ForInDone), R8(7), R8(6),
      /* 20       */ B(JumpIfTrue), U8(23),
      /* 22       */ B(ForInNext), R8(3), R8(7), R8(4), U8(1),
      /* 27       */ B(JumpIfUndefined), U8(10),
      /* 29       */ B(Star), R8(0),
      /* 31 54 E> */ B(StackCheck),
      /* 32       */ B(Ldar), R8(0),
      /* 34       */ B(Star), R8(2),
      /* 36 85 S> */ B(Return),
      /* 37       */ B(ForInStep), R8(7),
      /* 39       */ B(Star), R8(7),
      /* 41       */ B(Jump), U8(-24),
      /* 43       */ B(LdaUndefined),
      /* 44 85 S> */ B(Return),
      // clang-format on
  };

  static const PositionTableEntry expected_positions[] = {
      {0, 30, false}, {1, 42, true},   {3, 42, false}, {5, 68, true},
      {17, 63, true}, {31, 54, false}, {36, 85, true}, {44, 85, true}};

#define R(i) static_cast<uint32_t>(Register(i).ToOperand())
  Write(Bytecode::kStackCheck, {30, false});
  Write(Bytecode::kLdaConstant, U8(0), OperandScale::kSingle, {42, true});
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 0 * kPointerSize);
  Write(Bytecode::kStar, R(1), OperandScale::kSingle, {42, false});
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 2 * kPointerSize);
  Write(Bytecode::kJumpIfUndefined, U8(38), OperandScale::kSingle, {68, true});
  Write(Bytecode::kJumpIfNull, U8(36), OperandScale::kSingle);
  Write(Bytecode::kToObject);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 2 * kPointerSize);
  Write(Bytecode::kStar, R(3), OperandScale::kSingle);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 4 * kPointerSize);
  Write(Bytecode::kForInPrepare, R(4), OperandScale::kSingle);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 7 * kPointerSize);
  Write(Bytecode::kLdaZero);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 7 * kPointerSize);
  Write(Bytecode::kStar, R(7), OperandScale::kSingle);
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 8 * kPointerSize);
  Write(Bytecode::kForInDone, R(7), R(6), OperandScale::kSingle, {63, true});
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 8 * kPointerSize);
  Write(Bytecode::kJumpIfTrue, U8(23), OperandScale::kSingle);
  Write(Bytecode::kForInNext, R(3), R(7), R(4), U8(1), OperandScale::kSingle);
  Write(Bytecode::kJumpIfUndefined, U8(10), OperandScale::kSingle);
  Write(Bytecode::kStar, R(0), OperandScale::kSingle);
  Write(Bytecode::kStackCheck, {54, false});
  Write(Bytecode::kLdar, R(0), OperandScale::kSingle);
  Write(Bytecode::kStar, R(2), OperandScale::kSingle);
  Write(Bytecode::kReturn, {85, true});
  Write(Bytecode::kForInStep, R(7), OperandScale::kSingle);
  Write(Bytecode::kStar, R(7), OperandScale::kSingle);
  Write(Bytecode::kJump, U8(-24), OperandScale::kSingle);
  Write(Bytecode::kLdaUndefined);
  Write(Bytecode::kReturn, {85, true});
  CHECK_EQ(writer()->GetMaximumFrameSizeUsed(), 8 * kPointerSize);
#undef R

  CHECK_EQ(writer()->bytecodes()->size(), arraysize(expected_bytes));
  for (size_t i = 0; i < arraysize(expected_bytes); ++i) {
    CHECK_EQ(static_cast<int>(writer()->bytecodes()->at(i)),
             static_cast<int>(expected_bytes[i]));
  }

  Handle<ByteArray> source_positions =
      source_position_builder()->ToSourcePositionTable();
  SourcePositionTableIterator source_iterator(*source_positions);
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.bytecode_offset(), expected.bytecode_offset);
    CHECK_EQ(source_iterator.source_position(), expected.source_position);
    CHECK_EQ(source_iterator.is_statement(), expected.is_statement);
    source_iterator.Advance();
  }
  CHECK(source_iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
