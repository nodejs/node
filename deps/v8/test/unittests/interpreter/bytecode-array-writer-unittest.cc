// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/api.h"
#include "src/factory.h"
#include "src/interpreter/bytecode-array-writer.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/isolate.h"
#include "src/source-position-table.h"
#include "src/utils.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayWriterUnittest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayWriterUnittest()
      : constant_array_builder_(zone(), isolate()->factory()->the_hole_value()),
        bytecode_array_writer_(
            zone(), &constant_array_builder_,
            SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS) {}
  ~BytecodeArrayWriterUnittest() override {}

  void Write(BytecodeNode* node, const BytecodeSourceInfo& info);
  void Write(Bytecode bytecode,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,

             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             uint32_t operand2,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             uint32_t operand2, uint32_t operand3,
             const BytecodeSourceInfo& info = BytecodeSourceInfo());

  void WriteJump(Bytecode bytecode, BytecodeLabel* label,

                 const BytecodeSourceInfo& info = BytecodeSourceInfo());

  BytecodeArrayWriter* writer() { return &bytecode_array_writer_; }
  ZoneVector<unsigned char>* bytecodes() { return writer()->bytecodes(); }
  SourcePositionTableBuilder* source_position_table_builder() {
    return writer()->source_position_table_builder();
  }
  int max_register_count() { return writer()->max_register_count(); }

 private:
  ConstantArrayBuilder constant_array_builder_;
  BytecodeArrayWriter bytecode_array_writer_;
};

void BytecodeArrayWriterUnittest::Write(BytecodeNode* node,
                                        const BytecodeSourceInfo& info) {
  if (info.is_valid()) {
    node->source_info().Clone(info);
  }
  writer()->Write(node);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand1);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1, uint32_t operand2,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand1, operand2);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1, uint32_t operand2,
                                        uint32_t operand3,
                                        const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, operand0, operand1, operand2, operand3);
  Write(&node, info);
}

void BytecodeArrayWriterUnittest::WriteJump(Bytecode bytecode,
                                            BytecodeLabel* label,
                                            const BytecodeSourceInfo& info) {
  BytecodeNode node(bytecode, 0);
  if (info.is_valid()) {
    node.source_info().Clone(info);
  }
  writer()->WriteJump(&node, label);
}

TEST_F(BytecodeArrayWriterUnittest, SimpleExample) {
  CHECK_EQ(bytecodes()->size(), 0);

  Write(Bytecode::kStackCheck, {10, false});
  CHECK_EQ(bytecodes()->size(), 1);
  CHECK_EQ(max_register_count(), 0);

  Write(Bytecode::kLdaSmi, 127, {55, true});
  CHECK_EQ(bytecodes()->size(), 3);
  CHECK_EQ(max_register_count(), 0);

  Write(Bytecode::kLdar, Register(200).ToOperand());
  CHECK_EQ(bytecodes()->size(), 7);
  CHECK_EQ(max_register_count(), 201);

  Write(Bytecode::kReturn, {70, true});
  CHECK_EQ(bytecodes()->size(), 8);
  CHECK_EQ(max_register_count(), 201);

  static const uint8_t bytes[] = {B(StackCheck), B(LdaSmi), U8(127),  B(Wide),
                                  B(Ldar),       R16(200),  B(Return)};
  CHECK_EQ(bytecodes()->size(), arraysize(bytes));
  for (size_t i = 0; i < arraysize(bytes); ++i) {
    CHECK_EQ(bytecodes()->at(i), bytes[i]);
  }

  Handle<BytecodeArray> bytecode_array = writer()->ToBytecodeArray(
      isolate(), 0, 0, factory()->empty_fixed_array());
  CHECK_EQ(bytecodes()->size(), arraysize(bytes));

  PositionTableEntry expected_positions[] = {
      {0, 10, false}, {1, 55, true}, {7, 70, true}};
  SourcePositionTableIterator source_iterator(
      bytecode_array->source_position_table());
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.code_offset(), expected.code_offset);
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
      /*  9       */ B(ToObject), R8(3),
      /* 11       */ B(ForInPrepare), R8(3), R8(4),
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

  BytecodeLabel back_jump, jump_for_in, jump_end_1, jump_end_2, jump_end_3;

#define R(i) static_cast<uint32_t>(Register(i).ToOperand())
  Write(Bytecode::kStackCheck, {30, false});
  Write(Bytecode::kLdaConstant, U8(0), {42, true});
  CHECK_EQ(max_register_count(), 0);
  Write(Bytecode::kStar, R(1), {42, false});
  CHECK_EQ(max_register_count(), 2);
  WriteJump(Bytecode::kJumpIfUndefined, &jump_end_1, {68, true});
  WriteJump(Bytecode::kJumpIfNull, &jump_end_2);
  Write(Bytecode::kToObject, R(3));
  CHECK_EQ(max_register_count(), 4);
  Write(Bytecode::kForInPrepare, R(3), R(4));
  CHECK_EQ(max_register_count(), 7);
  Write(Bytecode::kLdaZero);
  CHECK_EQ(max_register_count(), 7);
  Write(Bytecode::kStar, R(7));
  CHECK_EQ(max_register_count(), 8);
  writer()->BindLabel(&back_jump);
  Write(Bytecode::kForInDone, R(7), R(6), {63, true});
  CHECK_EQ(max_register_count(), 8);
  WriteJump(Bytecode::kJumpIfTrue, &jump_end_3);
  Write(Bytecode::kForInNext, R(3), R(7), R(4), U8(1));
  WriteJump(Bytecode::kJumpIfUndefined, &jump_for_in);
  Write(Bytecode::kStar, R(0));
  Write(Bytecode::kStackCheck, {54, false});
  Write(Bytecode::kLdar, R(0));
  Write(Bytecode::kStar, R(2));
  Write(Bytecode::kReturn, {85, true});
  writer()->BindLabel(&jump_for_in);
  Write(Bytecode::kForInStep, R(7));
  Write(Bytecode::kStar, R(7));
  WriteJump(Bytecode::kJump, &back_jump);
  writer()->BindLabel(&jump_end_1);
  writer()->BindLabel(&jump_end_2);
  writer()->BindLabel(&jump_end_3);
  Write(Bytecode::kLdaUndefined);
  Write(Bytecode::kReturn, {85, true});
  CHECK_EQ(max_register_count(), 8);
#undef R

  CHECK_EQ(bytecodes()->size(), arraysize(expected_bytes));
  for (size_t i = 0; i < arraysize(expected_bytes); ++i) {
    CHECK_EQ(static_cast<int>(bytecodes()->at(i)),
             static_cast<int>(expected_bytes[i]));
  }

  Handle<BytecodeArray> bytecode_array = writer()->ToBytecodeArray(
      isolate(), 0, 0, factory()->empty_fixed_array());
  SourcePositionTableIterator source_iterator(
      bytecode_array->source_position_table());
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.code_offset(), expected.code_offset);
    CHECK_EQ(source_iterator.source_position(), expected.source_position);
    CHECK_EQ(source_iterator.is_statement(), expected.is_statement);
    source_iterator.Advance();
  }
  CHECK(source_iterator.done());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
