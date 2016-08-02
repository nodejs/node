// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-pipeline.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

using BytecodeNodeTest = TestWithIsolateAndZone;

TEST(BytecodeSourceInfo, Operations) {
  BytecodeSourceInfo x(0, true);
  CHECK_EQ(x.source_position(), 0);
  CHECK_EQ(x.is_statement(), true);
  CHECK_EQ(x.is_valid(), true);
  x.set_invalid();
  CHECK_EQ(x.is_statement(), false);
  CHECK_EQ(x.is_valid(), false);

  x.Update({1, true});
  BytecodeSourceInfo y(1, true);
  CHECK(x == y);
  CHECK(!(x != y));

  x.set_invalid();
  CHECK(!(x == y));
  CHECK(x != y);

  y.Update({2, false});
  CHECK_EQ(y.source_position(), 1);
  CHECK_EQ(y.is_statement(), true);

  y.Update({2, true});
  CHECK_EQ(y.source_position(), 2);
  CHECK_EQ(y.is_statement(), true);

  y.set_invalid();
  y.Update({3, false});
  CHECK_EQ(y.source_position(), 3);
  CHECK_EQ(y.is_statement(), false);

  y.Update({3, true});
  CHECK_EQ(y.source_position(), 3);
  CHECK_EQ(y.is_statement(), true);
}

TEST_F(BytecodeNodeTest, Constructor0) {
  BytecodeNode node;
  CHECK_EQ(node.bytecode(), Bytecode::kIllegal);
  CHECK(!node.source_info().is_valid());
}

TEST_F(BytecodeNodeTest, Constructor1) {
  BytecodeNode node(Bytecode::kLdaZero);
  CHECK_EQ(node.bytecode(), Bytecode::kLdaZero);
  CHECK_EQ(node.operand_count(), 0);
  CHECK_EQ(node.operand_scale(), OperandScale::kSingle);
  CHECK(!node.source_info().is_valid());
  CHECK_EQ(node.Size(), 1);
}

TEST_F(BytecodeNodeTest, Constructor2) {
  uint32_t operands[] = {0x11};
  BytecodeNode node(Bytecode::kJumpIfTrue, operands[0], OperandScale::kDouble);
  CHECK_EQ(node.bytecode(), Bytecode::kJumpIfTrue);
  CHECK_EQ(node.operand_count(), 1);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand_scale(), OperandScale::kDouble);
  CHECK(!node.source_info().is_valid());
  CHECK_EQ(node.Size(), 4);
}

TEST_F(BytecodeNodeTest, Constructor3) {
  uint32_t operands[] = {0x11, 0x22};
  BytecodeNode node(Bytecode::kLdaGlobal, operands[0], operands[1],
                    OperandScale::kQuadruple);
  CHECK_EQ(node.bytecode(), Bytecode::kLdaGlobal);
  CHECK_EQ(node.operand_count(), 2);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand(1), operands[1]);
  CHECK_EQ(node.operand_scale(), OperandScale::kQuadruple);
  CHECK(!node.source_info().is_valid());
  CHECK_EQ(node.Size(), 10);
}

TEST_F(BytecodeNodeTest, Constructor4) {
  uint32_t operands[] = {0x11, 0x22, 0x33};
  BytecodeNode node(Bytecode::kLoadIC, operands[0], operands[1], operands[2],
                    OperandScale::kSingle);
  CHECK_EQ(node.operand_count(), 3);
  CHECK_EQ(node.bytecode(), Bytecode::kLoadIC);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand(1), operands[1]);
  CHECK_EQ(node.operand(2), operands[2]);
  CHECK_EQ(node.operand_scale(), OperandScale::kSingle);
  CHECK(!node.source_info().is_valid());
  CHECK_EQ(node.Size(), 4);
}

TEST_F(BytecodeNodeTest, Constructor5) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  CHECK_EQ(node.operand_count(), 4);
  CHECK_EQ(node.bytecode(), Bytecode::kForInNext);
  CHECK_EQ(node.operand(0), operands[0]);
  CHECK_EQ(node.operand(1), operands[1]);
  CHECK_EQ(node.operand(2), operands[2]);
  CHECK_EQ(node.operand(3), operands[3]);
  CHECK_EQ(node.operand_scale(), OperandScale::kDouble);
  CHECK(!node.source_info().is_valid());
  CHECK_EQ(node.Size(), 10);
}

TEST_F(BytecodeNodeTest, Equality) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  CHECK_EQ(node, node);
  BytecodeNode other(Bytecode::kForInNext, operands[0], operands[1],
                     operands[2], operands[3], OperandScale::kDouble);
  CHECK_EQ(node, other);
}

TEST_F(BytecodeNodeTest, EqualityWithSourceInfo) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  node.source_info().Update({3, true});
  CHECK_EQ(node, node);
  BytecodeNode other(Bytecode::kForInNext, operands[0], operands[1],
                     operands[2], operands[3], OperandScale::kDouble);
  other.source_info().Update({3, true});
  CHECK_EQ(node, other);
}

TEST_F(BytecodeNodeTest, NoEqualityWithDifferentSourceInfo) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  node.source_info().Update({3, true});
  BytecodeNode other(Bytecode::kForInNext, operands[0], operands[1],
                     operands[2], operands[3], OperandScale::kDouble);
  CHECK_NE(node, other);
}

TEST_F(BytecodeNodeTest, Clone) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  BytecodeNode clone;
  clone.Clone(&node);
  CHECK_EQ(clone, node);
}

TEST_F(BytecodeNodeTest, SetBytecode0) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  BytecodeSourceInfo source_info(77, false);
  node.source_info().Update(source_info);

  BytecodeNode clone;
  clone.Clone(&node);
  clone.set_bytecode(Bytecode::kNop);
  CHECK_EQ(clone.bytecode(), Bytecode::kNop);
  CHECK_EQ(clone.operand_count(), 0);
  CHECK_EQ(clone.operand_scale(), OperandScale::kSingle);
  CHECK_EQ(clone.source_info(), source_info);
}

TEST_F(BytecodeNodeTest, SetBytecode1) {
  uint32_t operands[] = {0x71, 0xa5, 0x5a, 0xfc};
  BytecodeNode node(Bytecode::kForInNext, operands[0], operands[1], operands[2],
                    operands[3], OperandScale::kDouble);
  BytecodeSourceInfo source_info(77, false);
  node.source_info().Update(source_info);

  BytecodeNode clone;
  clone.Clone(&node);
  clone.set_bytecode(Bytecode::kJump, 0x01aabbcc, OperandScale::kQuadruple);
  CHECK_EQ(clone.bytecode(), Bytecode::kJump);
  CHECK_EQ(clone.operand_count(), 1);
  CHECK_EQ(clone.operand(0), 0x01aabbcc);
  CHECK_EQ(clone.operand_scale(), OperandScale::kQuadruple);
  CHECK_EQ(clone.source_info(), source_info);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
