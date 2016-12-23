// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-peephole-optimizer.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodePeepholeOptimizerTest : public BytecodePipelineStage,
                                      public TestWithIsolateAndZone {
 public:
  BytecodePeepholeOptimizerTest()
      : peephole_optimizer_(this), last_written_(Bytecode::kIllegal) {}
  ~BytecodePeepholeOptimizerTest() override {}

  void Reset() {
    last_written_.set_bytecode(Bytecode::kIllegal);
    write_count_ = 0;
  }

  void Write(BytecodeNode* node) override {
    write_count_++;
    last_written_.Clone(node);
  }

  void WriteJump(BytecodeNode* node, BytecodeLabel* label) override {
    write_count_++;
    last_written_.Clone(node);
  }

  void BindLabel(BytecodeLabel* label) override {}
  void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) override {}
  Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int fixed_register_count, int parameter_count,
      Handle<FixedArray> handle_table) override {
    return Handle<BytecodeArray>();
  }

  void Flush() {
    optimizer()->ToBytecodeArray(isolate(), 0, 0,
                                 factory()->empty_fixed_array());
  }

  BytecodePeepholeOptimizer* optimizer() { return &peephole_optimizer_; }

  int write_count() const { return write_count_; }
  const BytecodeNode& last_written() const { return last_written_; }

 private:
  BytecodePeepholeOptimizer peephole_optimizer_;

  int write_count_ = 0;
  BytecodeNode last_written_;
};

// Sanity tests.

TEST_F(BytecodePeepholeOptimizerTest, FlushOnJump) {
  CHECK_EQ(write_count(), 0);

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 0);

  BytecodeLabel target;
  BytecodeNode jump(Bytecode::kJump, 0, nullptr);
  optimizer()->WriteJump(&jump, &target);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(jump, last_written());
}

TEST_F(BytecodePeepholeOptimizerTest, FlushOnBind) {
  CHECK_EQ(write_count(), 0);

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 0);

  BytecodeLabel target;
  optimizer()->BindLabel(&target);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(add, last_written());
}

// Nop elimination tests.

TEST_F(BytecodePeepholeOptimizerTest, ElideEmptyNop) {
  BytecodeNode nop(Bytecode::kNop);
  optimizer()->Write(&nop);
  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  Flush();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(add, last_written());
}

TEST_F(BytecodePeepholeOptimizerTest, ElideExpressionNop) {
  BytecodeSourceInfo source_info(3, false);
  BytecodeNode nop(Bytecode::kNop, &source_info);
  optimizer()->Write(&nop);
  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  Flush();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(add, last_written());
}

TEST_F(BytecodePeepholeOptimizerTest, KeepStatementNop) {
  BytecodeSourceInfo source_info(3, true);
  BytecodeNode nop(Bytecode::kNop, &source_info);
  optimizer()->Write(&nop);
  source_info.MakeExpressionPosition(3);
  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1, &source_info);
  optimizer()->Write(&add);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(add, last_written());
}

// Tests covering BytecodePeepholeOptimizer::UpdateCurrentBytecode().

TEST_F(BytecodePeepholeOptimizerTest, KeepJumpIfToBooleanTrue) {
  BytecodeNode first(Bytecode::kLdaNull);
  BytecodeNode second(Bytecode::kJumpIfToBooleanTrue, 3);
  BytecodeLabel label;
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->WriteJump(&second, &label);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, ElideJumpIfToBooleanTrue) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kJumpIfToBooleanTrue, 3);
  BytecodeLabel label;
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->WriteJump(&second, &label);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, KeepToBooleanLogicalNot) {
  BytecodeNode first(Bytecode::kLdaNull);
  BytecodeNode second(Bytecode::kToBooleanLogicalNot);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, ElideToBooleanLogicalNot) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kToBooleanLogicalNot);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLogicalNot);
}

// Tests covering BytecodePeepholeOptimizer::CanElideCurrent().

TEST_F(BytecodePeepholeOptimizerTest, StarRxLdarRy) {
  BytecodeNode first(Bytecode::kStar, Register(0).ToOperand());
  BytecodeNode second(Bytecode::kLdar, Register(1).ToOperand());
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, StarRxLdarRx) {
  BytecodeLabel label;
  BytecodeNode first(Bytecode::kStar, Register(0).ToOperand());
  BytecodeNode second(Bytecode::kLdar, Register(0).ToOperand());
  optimizer()->Write(&first);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  Flush();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
}

TEST_F(BytecodePeepholeOptimizerTest, StarRxLdarRxStatement) {
  BytecodeNode first(Bytecode::kStar, Register(0).ToOperand());
  BytecodeSourceInfo source_info(3, true);
  BytecodeNode second(Bytecode::kLdar, Register(0).ToOperand(), &source_info);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kNop);
  CHECK_EQ(last_written().source_info(), second.source_info());
}

TEST_F(BytecodePeepholeOptimizerTest, StarRxLdarRxStatementStarRy) {
  BytecodeLabel label;
  BytecodeNode first(Bytecode::kStar, Register(0).ToOperand());
  BytecodeSourceInfo source_info(0, true);
  BytecodeNode second(Bytecode::kLdar, Register(0).ToOperand(), &source_info);
  BytecodeNode third(Bytecode::kStar, Register(3).ToOperand());
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 1);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), third);
}

TEST_F(BytecodePeepholeOptimizerTest, LdarToName) {
  BytecodeNode first(Bytecode::kLdar, Register(0).ToOperand());
  BytecodeNode second(Bytecode::kToName, Register(0).ToOperand());
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, TypeOfToName) {
  BytecodeNode first(Bytecode::kTypeOf);
  BytecodeNode second(Bytecode::kToName, Register(0).ToOperand());
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), first);
  Flush();
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written(), second);
  CHECK_EQ(last_written().bytecode(), Bytecode::kStar);
}

// Tests covering BytecodePeepholeOptimizer::CanElideLast().

TEST_F(BytecodePeepholeOptimizerTest, LdaTrueLdaFalse) {
  BytecodeNode first(Bytecode::kLdaTrue);
  BytecodeNode second(Bytecode::kLdaFalse);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  Flush();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, LdaTrueStatementLdaFalse) {
  BytecodeSourceInfo source_info(3, true);
  BytecodeNode first(Bytecode::kLdaTrue, &source_info);
  BytecodeNode second(Bytecode::kLdaFalse);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  Flush();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), second);
  CHECK(second.source_info().is_statement());
  CHECK_EQ(second.source_info().source_position(), 3);
}

TEST_F(BytecodePeepholeOptimizerTest, NopStackCheck) {
  BytecodeNode first(Bytecode::kNop);
  BytecodeNode second(Bytecode::kStackCheck, nullptr);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  Flush();
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written(), second);
}

TEST_F(BytecodePeepholeOptimizerTest, NopStatementStackCheck) {
  BytecodeSourceInfo source_info(3, true);
  BytecodeNode first(Bytecode::kNop, &source_info);
  BytecodeNode second(Bytecode::kStackCheck);
  optimizer()->Write(&first);
  CHECK_EQ(write_count(), 0);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 0);
  Flush();
  CHECK_EQ(write_count(), 1);
  BytecodeSourceInfo expected_source_info(3, true);
  BytecodeNode expected(Bytecode::kStackCheck, &expected_source_info);
  CHECK_EQ(last_written(), expected);
}

// Tests covering BytecodePeepholeOptimizer::UpdateLastAndCurrentBytecodes().

TEST_F(BytecodePeepholeOptimizerTest, MergeLoadICStar) {
  const uint32_t operands[] = {
      static_cast<uint32_t>(Register(31).ToOperand()), 32, 33,
      static_cast<uint32_t>(Register(256).ToOperand())};
  const int expected_operand_count = static_cast<int>(arraysize(operands));

  BytecodeNode first(Bytecode::kLdaNamedProperty, operands[0], operands[1],
                     operands[2]);
  BytecodeNode second(Bytecode::kStar, operands[3]);
  BytecodeNode third(Bytecode::kReturn);
  optimizer()->Write(&first);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdrNamedProperty);
  CHECK_EQ(last_written().operand_count(), expected_operand_count);
  for (int i = 0; i < expected_operand_count; ++i) {
    CHECK_EQ(last_written().operand(i), operands[i]);
  }
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdar);
  CHECK_EQ(last_written().operand(0), operands[expected_operand_count - 1]);
  Flush();
  CHECK_EQ(last_written().bytecode(), third.bytecode());
}

TEST_F(BytecodePeepholeOptimizerTest, MergeLdaKeyedPropertyStar) {
  const uint32_t operands[] = {static_cast<uint32_t>(Register(31).ToOperand()),
                               9999997,
                               static_cast<uint32_t>(Register(1).ToOperand())};
  const int expected_operand_count = static_cast<int>(arraysize(operands));

  BytecodeNode first(Bytecode::kLdaKeyedProperty, operands[0], operands[1],
                     nullptr);
  BytecodeNode second(Bytecode::kStar, operands[2]);
  BytecodeNode third(Bytecode::kReturn);
  optimizer()->Write(&first);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdrKeyedProperty);
  CHECK_EQ(last_written().operand_count(), expected_operand_count);
  for (int i = 0; i < expected_operand_count; ++i) {
    CHECK_EQ(last_written().operand(i), operands[i]);
  }
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdar);
  CHECK_EQ(last_written().operand(0), operands[expected_operand_count - 1]);
  Flush();
  CHECK_EQ(last_written().bytecode(), third.bytecode());
}

TEST_F(BytecodePeepholeOptimizerTest, MergeLdaGlobalStar) {
  const uint32_t operands[] = {19191,
                               static_cast<uint32_t>(Register(1).ToOperand())};
  const int expected_operand_count = static_cast<int>(arraysize(operands));

  BytecodeNode first(Bytecode::kLdaGlobal, operands[0]);
  BytecodeNode second(Bytecode::kStar, operands[1]);
  BytecodeNode third(Bytecode::kReturn);
  optimizer()->Write(&first);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdrGlobal);
  CHECK_EQ(last_written().operand_count(), expected_operand_count);
  for (int i = 0; i < expected_operand_count; ++i) {
    CHECK_EQ(last_written().operand(i), operands[i]);
  }
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdar);
  CHECK_EQ(last_written().operand(0), operands[expected_operand_count - 1]);
  Flush();
  CHECK_EQ(last_written().bytecode(), third.bytecode());
}

TEST_F(BytecodePeepholeOptimizerTest, MergeLdaContextSlotStar) {
  const uint32_t operands[] = {
      static_cast<uint32_t>(Register(200000).ToOperand()), 55005500,
      static_cast<uint32_t>(Register(0).ToOperand()),
      static_cast<uint32_t>(Register(1).ToOperand())};
  const int expected_operand_count = static_cast<int>(arraysize(operands));

  BytecodeNode first(Bytecode::kLdaContextSlot, operands[0], operands[1],
                     operands[2]);
  BytecodeNode second(Bytecode::kStar, operands[3]);
  BytecodeNode third(Bytecode::kReturn);
  optimizer()->Write(&first);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdrContextSlot);
  CHECK_EQ(last_written().operand_count(), expected_operand_count);
  for (int i = 0; i < expected_operand_count; ++i) {
    CHECK_EQ(last_written().operand(i), operands[i]);
  }
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdar);
  CHECK_EQ(last_written().operand(0), operands[expected_operand_count - 1]);
  Flush();
  CHECK_EQ(last_written().bytecode(), third.bytecode());
}

TEST_F(BytecodePeepholeOptimizerTest, MergeLdaUndefinedStar) {
  const uint32_t operands[] = {
      static_cast<uint32_t>(Register(100000).ToOperand())};
  const int expected_operand_count = static_cast<int>(arraysize(operands));

  BytecodeNode first(Bytecode::kLdaUndefined);
  BytecodeNode second(Bytecode::kStar, operands[0]);
  BytecodeNode third(Bytecode::kReturn);
  optimizer()->Write(&first);
  optimizer()->Write(&second);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdrUndefined);
  CHECK_EQ(last_written().operand_count(), expected_operand_count);
  for (int i = 0; i < expected_operand_count; ++i) {
    CHECK_EQ(last_written().operand(i), operands[i]);
  }
  optimizer()->Write(&third);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(last_written().bytecode(), Bytecode::kLdar);
  CHECK_EQ(last_written().operand(0), operands[expected_operand_count - 1]);
  Flush();
  CHECK_EQ(last_written().bytecode(), third.bytecode());
}

TEST_F(BytecodePeepholeOptimizerTest, MergeLdaSmiWithBinaryOp) {
  Bytecode operator_replacement_pairs[][2] = {
      {Bytecode::kAdd, Bytecode::kAddSmi},
      {Bytecode::kSub, Bytecode::kSubSmi},
      {Bytecode::kBitwiseAnd, Bytecode::kBitwiseAndSmi},
      {Bytecode::kBitwiseOr, Bytecode::kBitwiseOrSmi},
      {Bytecode::kShiftLeft, Bytecode::kShiftLeftSmi},
      {Bytecode::kShiftRight, Bytecode::kShiftRightSmi}};

  for (auto operator_replacement : operator_replacement_pairs) {
    uint32_t imm_operand = 17;
    BytecodeSourceInfo source_info(3, true);
    BytecodeNode first(Bytecode::kLdaSmi, imm_operand, &source_info);
    uint32_t reg_operand = Register(0).ToOperand();
    uint32_t idx_operand = 1;
    BytecodeNode second(operator_replacement[0], reg_operand, idx_operand);
    optimizer()->Write(&first);
    optimizer()->Write(&second);
    Flush();
    CHECK_EQ(write_count(), 1);
    CHECK_EQ(last_written().bytecode(), operator_replacement[1]);
    CHECK_EQ(last_written().operand_count(), 3);
    CHECK_EQ(last_written().operand(0), imm_operand);
    CHECK_EQ(last_written().operand(1), reg_operand);
    CHECK_EQ(last_written().operand(2), idx_operand);
    CHECK_EQ(last_written().source_info(), first.source_info());
    Reset();
  }
}

TEST_F(BytecodePeepholeOptimizerTest, NotMergingLdaSmiWithBinaryOp) {
  Bytecode operator_replacement_pairs[][2] = {
      {Bytecode::kAdd, Bytecode::kAddSmi},
      {Bytecode::kSub, Bytecode::kSubSmi},
      {Bytecode::kBitwiseAnd, Bytecode::kBitwiseAndSmi},
      {Bytecode::kBitwiseOr, Bytecode::kBitwiseOrSmi},
      {Bytecode::kShiftLeft, Bytecode::kShiftLeftSmi},
      {Bytecode::kShiftRight, Bytecode::kShiftRightSmi}};

  for (auto operator_replacement : operator_replacement_pairs) {
    uint32_t imm_operand = 17;
    BytecodeSourceInfo source_info(3, true);
    BytecodeNode first(Bytecode::kLdaSmi, imm_operand, &source_info);
    uint32_t reg_operand = Register(0).ToOperand();
    source_info.MakeStatementPosition(4);
    BytecodeNode second(operator_replacement[0], reg_operand, 1, &source_info);
    optimizer()->Write(&first);
    optimizer()->Write(&second);
    CHECK_EQ(last_written(), first);
    Flush();
    CHECK_EQ(last_written(), second);
    Reset();
  }
}

TEST_F(BytecodePeepholeOptimizerTest, MergeLdaZeroWithBinaryOp) {
  Bytecode operator_replacement_pairs[][2] = {
      {Bytecode::kAdd, Bytecode::kAddSmi},
      {Bytecode::kSub, Bytecode::kSubSmi},
      {Bytecode::kBitwiseAnd, Bytecode::kBitwiseAndSmi},
      {Bytecode::kBitwiseOr, Bytecode::kBitwiseOrSmi},
      {Bytecode::kShiftLeft, Bytecode::kShiftLeftSmi},
      {Bytecode::kShiftRight, Bytecode::kShiftRightSmi}};

  for (auto operator_replacement : operator_replacement_pairs) {
    BytecodeNode first(Bytecode::kLdaZero);
    uint32_t reg_operand = Register(0).ToOperand();
    uint32_t idx_operand = 1;
    BytecodeNode second(operator_replacement[0], reg_operand, idx_operand);
    optimizer()->Write(&first);
    optimizer()->Write(&second);
    Flush();
    CHECK_EQ(write_count(), 1);
    CHECK_EQ(last_written().bytecode(), operator_replacement[1]);
    CHECK_EQ(last_written().operand_count(), 3);
    CHECK_EQ(last_written().operand(0), 0);
    CHECK_EQ(last_written().operand(1), reg_operand);
    CHECK_EQ(last_written().operand(2), idx_operand);
    Reset();
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
