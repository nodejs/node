// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-node.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

using BytecodeNodeTest = TestWithIsolateAndZone;

TEST_F(BytecodeNodeTest, Constructor1) {
  BytecodeNode node(Bytecode::kLdaZero);
  CHECK_EQ(node.bytecode(), Bytecode::kLdaZero);
  CHECK_EQ(node.operand_count(), 0);
  CHECK(!node.source_info().is_valid());
}

TEST_F(BytecodeNodeTest, Constructor2) {
  uint32_t operands[] = {0x11};
  BytecodeNode node(Bytecode::kJumpIfTrue, operands[0]);
  CHECK_EQ(node.bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(node.operand_count(), 1);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK(!node.source_info().is_valid());
}

TEST_F(BytecodeNodeTest, Constructor3) {
  uint32_t operands[] = {0x11, 0x22};
  BytecodeNode node(Bytecode::kLdaGlobal, operands[0], operands[1]);
  CHECK_EQ(node.bytecode(), Bytecode::kLdaGlobal);
  CHECK_EQ(node.operand_count(), 2);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand(1), operands[1]);
  CHECK(!node.source_info().is_valid());
}

TEST_F(BytecodeNodeTest, Constructor4) {
  uint32_t operands[] = {0x11, 0x22, 0x33};
  BytecodeNode node(Bytecode::kLdaNamedProperty, operands[0], operands[1],
                    operands[2]);
  CHECK_EQ(node.operand_count(), 3);
  CHECK_EQ(node.bytecode(), Bytecode::kLdaNamedProperty);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand(1), operands[1]);
  CHECK_EQ(node.operand(2), operands[2]);
  CHECK(!node.source_info().is_valid());
}

TEST_F(BytecodeNodeTest, Constructor5) {
  uint32_t operands[] = {0x71, 0xA5, 0x5A, 0xFC};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3]);
  CHECK_EQ(node.operand_count(), 4);
  CHECK_EQ(node.bytecode(), Bytecode::kForInNext);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand(1), operands[1]);
  CHECK_EQ(node.operand(2), operands[2]);
  CHECK_EQ(node.operand(3), operands[3]);
  CHECK(!node.source_info().is_valid());
}

TEST_F(BytecodeNodeTest, Equality) {
  uint32_t operands[] = {0x71, 0xA5, 0x5A, 0xFC};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3]);
  CHECK_EQ(node, node);
  BytecodeNode other(Bytecode::kForInNext, operands[0], operands[1],
                     operands[2], operands[3]);
  CHECK_EQ(node, other);
}

TEST_F(BytecodeNodeTest, EqualityWithSourceInfo) {
  uint32_t operands[] = {0x71, 0xA5, 0x5A, 0xFC};
  BytecodeSourceInfo first_source_info(3, true);
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], first_source_info);
  CHECK_EQ(node, node);
  BytecodeSourceInfo second_source_info(3, true);
  BytecodeNode other(Bytecode::kForInNext, operands[0], operands[1],
                     operands[2], operands[3], second_source_info);
  CHECK_EQ(node, other);
}

TEST_F(BytecodeNodeTest, NoEqualityWithDifferentSourceInfo) {
  uint32_t operands[] = {0x71, 0xA5, 0x5A, 0xFC};
  BytecodeSourceInfo source_info(77, true);
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], source_info);
  BytecodeNode other(Bytecode::kForInNext, operands[0], operands[1],
                     operands[2], operands[3]);
  CHECK_NE(node, other);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
