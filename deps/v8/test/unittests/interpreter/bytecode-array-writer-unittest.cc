// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"

#include "src/api/api.h"
#include "src/codegen/source-position-table.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/interpreter/bytecode-array-writer.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-node.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecode-source-info.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/utils/utils.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {
namespace bytecode_array_writer_unittest {

#define B(Name) static_cast<uint8_t>(Bytecode::k##Name)
#define R(i) static_cast<uint32_t>(Register(i).ToOperand())

class BytecodeArrayWriterUnittest : public TestWithIsolateAndZone {
 public:
  BytecodeArrayWriterUnittest()
      : constant_array_builder_(zone()),
        bytecode_array_writer_(
            zone(), &constant_array_builder_,
            SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS) {}
  ~BytecodeArrayWriterUnittest() override = default;

  void Write(Bytecode bytecode, BytecodeSourceInfo info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0,
             BytecodeSourceInfo info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             BytecodeSourceInfo info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             uint32_t operand2, BytecodeSourceInfo info = BytecodeSourceInfo());
  void Write(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
             uint32_t operand2, uint32_t operand3,
             BytecodeSourceInfo info = BytecodeSourceInfo());

  void WriteJump(Bytecode bytecode, BytecodeLabel* label,
                 BytecodeSourceInfo info = BytecodeSourceInfo());
  void WriteJumpLoop(Bytecode bytecode, BytecodeLoopHeader* loop_header,
                     int depth, BytecodeSourceInfo info = BytecodeSourceInfo());

  BytecodeArrayWriter* writer() { return &bytecode_array_writer_; }
  ZoneVector<unsigned char>* bytecodes() { return writer()->bytecodes(); }
  SourcePositionTableBuilder* source_position_table_builder() {
    return writer()->source_position_table_builder();
  }

 private:
  ConstantArrayBuilder constant_array_builder_;
  BytecodeArrayWriter bytecode_array_writer_;
};

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode,
                                        BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, info);
  writer()->Write(&node);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, operand0, info);
  writer()->Write(&node);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1,
                                        BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, operand0, operand1, info);
  writer()->Write(&node);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1, uint32_t operand2,
                                        BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, operand0, operand1, operand2, info);
  writer()->Write(&node);
}

void BytecodeArrayWriterUnittest::Write(Bytecode bytecode, uint32_t operand0,
                                        uint32_t operand1, uint32_t operand2,
                                        uint32_t operand3,
                                        BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, operand0, operand1, operand2, operand3, info);
  writer()->Write(&node);
}

void BytecodeArrayWriterUnittest::WriteJump(Bytecode bytecode,
                                            BytecodeLabel* label,
                                            BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, 0, info);
  writer()->WriteJump(&node, label);
}

void BytecodeArrayWriterUnittest::WriteJumpLoop(Bytecode bytecode,
                                                BytecodeLoopHeader* loop_header,
                                                int depth,
                                                BytecodeSourceInfo info) {
  BytecodeNode node(bytecode, 0, depth, info);
  writer()->WriteJumpLoop(&node, loop_header);
}

TEST_F(BytecodeArrayWriterUnittest, SimpleExample) {
  CHECK_EQ(bytecodes()->size(), 0u);

  Write(Bytecode::kStackCheck, {10, false});
  CHECK_EQ(bytecodes()->size(), 1u);

  Write(Bytecode::kLdaSmi, 127, {55, true});
  CHECK_EQ(bytecodes()->size(), 3u);

  Write(Bytecode::kStar, Register(20).ToOperand());
  CHECK_EQ(bytecodes()->size(), 5u);

  Write(Bytecode::kLdar, Register(200).ToOperand());
  CHECK_EQ(bytecodes()->size(), 9u);

  Write(Bytecode::kReturn, {70, true});
  CHECK_EQ(bytecodes()->size(), 10u);

  static const uint8_t expected_bytes[] = {
      // clang-format off
      /*  0 10 E> */ B(StackCheck),
      /*  1 55 S> */ B(LdaSmi), U8(127),
      /*  3       */ B(Star), R8(20),
      /*  5       */ B(Wide), B(Ldar), R16(200),
      /*  9 70 S> */ B(Return),
      // clang-format on
  };
  CHECK_EQ(bytecodes()->size(), arraysize(expected_bytes));
  for (size_t i = 0; i < arraysize(expected_bytes); ++i) {
    CHECK_EQ(bytecodes()->at(i), expected_bytes[i]);
  }

  Handle<BytecodeArray> bytecode_array =
      writer()->ToBytecodeArray(isolate(), 0, 0, factory()->empty_byte_array());
  CHECK_EQ(bytecodes()->size(), arraysize(expected_bytes));

  PositionTableEntry expected_positions[] = {
      {0, 10, false}, {1, 55, true}, {9, 70, true}};
  SourcePositionTableIterator source_iterator(
      bytecode_array->SourcePositionTable());
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.code_offset(), expected.code_offset);
    CHECK_EQ(source_iterator.source_position().ScriptOffset(),
             expected.source_position);
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
      /*  3 42 E> */ B(Add), R8(1), U8(1),
      /*  5 68 S> */ B(JumpIfUndefined), U8(39),
      /*  7       */ B(JumpIfNull), U8(37),
      /*  9       */ B(ToObject), R8(3),
      /* 11       */ B(ForInPrepare), R8(3), U8(4),
      /* 14       */ B(LdaZero),
      /* 15       */ B(Star), R8(7),
      /* 17 63 S> */ B(ForInContinue), R8(7), R8(6),
      /* 20       */ B(JumpIfFalse), U8(24),
      /* 22       */ B(ForInNext), R8(3), R8(7), R8(4), U8(1),
      /* 27       */ B(JumpIfUndefined), U8(10),
      /* 29       */ B(Star), R8(0),
      /* 31 54 E> */ B(StackCheck),
      /* 32       */ B(Ldar), R8(0),
      /* 34       */ B(Star), R8(2),
      /* 36 85 S> */ B(Return),
      /* 37       */ B(ForInStep), R8(7),
      /* 39       */ B(Star), R8(7),
      /* 41       */ B(JumpLoop), U8(24), U8(0),
      /* 44       */ B(LdaUndefined),
      /* 45 85 S> */ B(Return),
      // clang-format on
  };

  static const PositionTableEntry expected_positions[] = {
      {0, 30, false}, {1, 42, true},   {3, 42, false}, {6, 68, true},
      {18, 63, true}, {32, 54, false}, {37, 85, true}, {46, 85, true}};

  BytecodeLoopHeader loop_header;
  BytecodeLabel jump_for_in, jump_end_1, jump_end_2, jump_end_3;

  Write(Bytecode::kStackCheck, {30, false});
  Write(Bytecode::kLdaConstant, U8(0), {42, true});
  Write(Bytecode::kAdd, R(1), U8(1), {42, false});
  WriteJump(Bytecode::kJumpIfUndefined, &jump_end_1, {68, true});
  WriteJump(Bytecode::kJumpIfNull, &jump_end_2);
  Write(Bytecode::kToObject, R(3));
  Write(Bytecode::kForInPrepare, R(3), U8(4));
  Write(Bytecode::kLdaZero);
  Write(Bytecode::kStar, R(7));
  writer()->BindLoopHeader(&loop_header);
  Write(Bytecode::kForInContinue, R(7), R(6), {63, true});
  WriteJump(Bytecode::kJumpIfFalse, &jump_end_3);
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
  WriteJumpLoop(Bytecode::kJumpLoop, &loop_header, 0);
  writer()->BindLabel(&jump_end_1);
  writer()->BindLabel(&jump_end_2);
  writer()->BindLabel(&jump_end_3);
  Write(Bytecode::kLdaUndefined);
  Write(Bytecode::kReturn, {85, true});

  CHECK_EQ(bytecodes()->size(), arraysize(expected_bytes));
  for (size_t i = 0; i < arraysize(expected_bytes); ++i) {
    CHECK_EQ(static_cast<int>(bytecodes()->at(i)),
             static_cast<int>(expected_bytes[i]));
  }

  Handle<BytecodeArray> bytecode_array =
      writer()->ToBytecodeArray(isolate(), 0, 0, factory()->empty_byte_array());
  SourcePositionTableIterator source_iterator(
      bytecode_array->SourcePositionTable());
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.code_offset(), expected.code_offset);
    CHECK_EQ(source_iterator.source_position().ScriptOffset(),
             expected.source_position);
    CHECK_EQ(source_iterator.is_statement(), expected.is_statement);
    source_iterator.Advance();
  }
  CHECK(source_iterator.done());
}

TEST_F(BytecodeArrayWriterUnittest, ElideNoneffectfulBytecodes) {
  if (!i::FLAG_ignition_elide_noneffectful_bytecodes) return;

  static const uint8_t expected_bytes[] = {
      // clang-format off
      /*  0  10 E> */ B(StackCheck),
      /*  1  55 S> */ B(Ldar), R8(20),
      /*  3        */ B(Star), R8(20),
      /*  5        */ B(CreateMappedArguments),
      /*  6  60 S> */ B(LdaSmi), U8(127),
      /*  8  70 S> */ B(Ldar), R8(20),
      /*  10 75 S> */ B(Return),
      // clang-format on
  };

  static const PositionTableEntry expected_positions[] = {{0, 10, false},
                                                          {1, 55, true},
                                                          {6, 60, false},
                                                          {8, 70, true},
                                                          {10, 75, true}};

  Write(Bytecode::kStackCheck, {10, false});
  Write(Bytecode::kLdaSmi, 127, {55, true});  // Should be elided.
  Write(Bytecode::kLdar, Register(20).ToOperand());
  Write(Bytecode::kStar, Register(20).ToOperand());
  Write(Bytecode::kLdar, Register(20).ToOperand());  // Should be elided.
  Write(Bytecode::kCreateMappedArguments);
  Write(Bytecode::kLdaSmi, 127, {60, false});  // Not elided due to source info.
  Write(Bytecode::kLdar, Register(20).ToOperand(), {70, true});
  Write(Bytecode::kReturn, {75, true});

  CHECK_EQ(bytecodes()->size(), arraysize(expected_bytes));
  for (size_t i = 0; i < arraysize(expected_bytes); ++i) {
    CHECK_EQ(static_cast<int>(bytecodes()->at(i)),
             static_cast<int>(expected_bytes[i]));
  }

  Handle<BytecodeArray> bytecode_array =
      writer()->ToBytecodeArray(isolate(), 0, 0, factory()->empty_byte_array());
  SourcePositionTableIterator source_iterator(
      bytecode_array->SourcePositionTable());
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.code_offset(), expected.code_offset);
    CHECK_EQ(source_iterator.source_position().ScriptOffset(),
             expected.source_position);
    CHECK_EQ(source_iterator.is_statement(), expected.is_statement);
    source_iterator.Advance();
  }
  CHECK(source_iterator.done());
}

TEST_F(BytecodeArrayWriterUnittest, DeadcodeElimination) {
  static const uint8_t expected_bytes[] = {
      // clang-format off
      /*  0  10 E> */ B(StackCheck),
      /*  1  55 S> */ B(LdaSmi), U8(127),
      /*  3        */ B(Jump), U8(2),
      /*  5  65 S> */ B(LdaSmi), U8(127),
      /*  7        */ B(JumpIfFalse), U8(3),
      /*  9  75 S> */ B(Return),
      /*  10       */ B(JumpIfFalse), U8(3),
      /*  12       */ B(Throw),
      /*  13       */ B(JumpIfFalse), U8(3),
      /*  15       */ B(ReThrow),
      /*  16       */ B(Return),
      // clang-format on
  };

  static const PositionTableEntry expected_positions[] = {
      {0, 10, false}, {1, 55, true}, {5, 65, true}, {9, 75, true}};

  BytecodeLabel after_jump, after_conditional_jump, after_return, after_throw,
      after_rethrow;

  Write(Bytecode::kStackCheck, {10, false});
  Write(Bytecode::kLdaSmi, 127, {55, true});
  WriteJump(Bytecode::kJump, &after_jump);
  Write(Bytecode::kLdaSmi, 127);                               // Dead code.
  WriteJump(Bytecode::kJumpIfFalse, &after_conditional_jump);  // Dead code.
  writer()->BindLabel(&after_jump);
  // We would bind the after_conditional_jump label here, but the jump to it is
  // dead.
  CHECK(!after_conditional_jump.has_referrer_jump());
  Write(Bytecode::kLdaSmi, 127, {65, true});
  WriteJump(Bytecode::kJumpIfFalse, &after_return);
  Write(Bytecode::kReturn, {75, true});
  Write(Bytecode::kLdaSmi, 127, {100, true});  // Dead code.
  writer()->BindLabel(&after_return);
  WriteJump(Bytecode::kJumpIfFalse, &after_throw);
  Write(Bytecode::kThrow);
  Write(Bytecode::kLdaSmi, 127);  // Dead code.
  writer()->BindLabel(&after_throw);
  WriteJump(Bytecode::kJumpIfFalse, &after_rethrow);
  Write(Bytecode::kReThrow);
  Write(Bytecode::kLdaSmi, 127);  // Dead code.
  writer()->BindLabel(&after_rethrow);
  Write(Bytecode::kReturn);

  CHECK_EQ(bytecodes()->size(), arraysize(expected_bytes));
  for (size_t i = 0; i < arraysize(expected_bytes); ++i) {
    CHECK_EQ(static_cast<int>(bytecodes()->at(i)),
             static_cast<int>(expected_bytes[i]));
  }

  Handle<BytecodeArray> bytecode_array =
      writer()->ToBytecodeArray(isolate(), 0, 0, factory()->empty_byte_array());
  SourcePositionTableIterator source_iterator(
      bytecode_array->SourcePositionTable());
  for (size_t i = 0; i < arraysize(expected_positions); ++i) {
    const PositionTableEntry& expected = expected_positions[i];
    CHECK_EQ(source_iterator.code_offset(), expected.code_offset);
    CHECK_EQ(source_iterator.source_position().ScriptOffset(),
             expected.source_position);
    CHECK_EQ(source_iterator.is_statement(), expected.is_statement);
    source_iterator.Advance();
  }
  CHECK(source_iterator.done());
}

#undef B
#undef R

}  // namespace bytecode_array_writer_unittest
}  // namespace interpreter
}  // namespace internal
}  // namespace v8
