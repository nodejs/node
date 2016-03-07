// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stack>

#include "src/v8.h"

#include "src/interpreter/register-translator.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class RegisterTranslatorTest : public TestWithIsolateAndZone,
                               private RegisterMover {
 public:
  RegisterTranslatorTest() : translator_(this), move_count_(0) {
    window_start_ =
        RegisterTranslator::DistanceToTranslationWindow(Register(0));
    window_width_ =
        Register::MaxRegisterIndexForByteOperand() - window_start_ + 1;
  }

  ~RegisterTranslatorTest() override {}

  bool PopMoveAndMatch(Register from, Register to) {
    if (!moves_.empty()) {
      CHECK(from.is_valid() && to.is_valid());
      const std::pair<Register, Register> top = moves_.top();
      moves_.pop();
      return top.first == from && top.second == to;
    } else {
      return false;
    }
  }

  int move_count() const { return move_count_; }
  RegisterTranslator* translator() { return &translator_; }

  int window_start() const { return window_start_; }
  int window_width() const { return window_width_; }
  int window_limit() const { return window_start_ + window_width_; }

 protected:
  static const char* const kBadOperandRegex;

 private:
  void MoveRegisterUntranslated(Register from, Register to) override {
    moves_.push(std::make_pair(from, to));
    move_count_++;
  }

  RegisterTranslator translator_;
  std::stack<std::pair<Register, Register>> moves_;
  int move_count_;
  int window_start_;
  int window_width_;
};

const char* const RegisterTranslatorTest::kBadOperandRegex =
    ".*OperandType::kReg8 \\|\\| .*OperandType::kRegOut8\\) && "
    "RegisterIsMovableToWindow.*";

TEST_F(RegisterTranslatorTest, TestFrameSizeAdjustmentsForTranslationWindow) {
  EXPECT_EQ(0, RegisterTranslator::RegisterCountAdjustment(0, 0));
  EXPECT_EQ(0, RegisterTranslator::RegisterCountAdjustment(10, 10));
  EXPECT_EQ(window_width(),
            RegisterTranslator::RegisterCountAdjustment(173, 0));
  EXPECT_EQ(window_width(),
            RegisterTranslator::RegisterCountAdjustment(173, 137));
  EXPECT_EQ(window_width(),
            RegisterTranslator::RegisterCountAdjustment(173, 137));
  // TODO(oth): Add a kMaxParameters8 that derives this info from the frame.
  int param_limit = FLAG_enable_embedded_constant_pool ? 119 : 120;
  EXPECT_EQ(0, RegisterTranslator::RegisterCountAdjustment(0, param_limit));
  EXPECT_EQ(window_limit(),
            RegisterTranslator::RegisterCountAdjustment(0, 128));
  EXPECT_EQ(window_limit(),
            RegisterTranslator::RegisterCountAdjustment(0, 129));
  EXPECT_EQ(window_limit() - 32,
            RegisterTranslator::RegisterCountAdjustment(32, 129));
}

TEST_F(RegisterTranslatorTest, TestInTranslationWindow) {
  EXPECT_GE(window_start(), 0);
  EXPECT_FALSE(
      RegisterTranslator::InTranslationWindow(Register(window_start() - 1)));
  EXPECT_TRUE(RegisterTranslator::InTranslationWindow(
      Register(Register::MaxRegisterIndexForByteOperand())));
  EXPECT_FALSE(RegisterTranslator::InTranslationWindow(
      Register(Register::MaxRegisterIndexForByteOperand() + 1)));
  for (int index = window_start(); index < window_limit(); index += 1) {
    EXPECT_TRUE(RegisterTranslator::InTranslationWindow(Register(index)));
  }
}

TEST_F(RegisterTranslatorTest, FitsInReg8Operand) {
  EXPECT_GT(window_start(), 0);
  EXPECT_TRUE(RegisterTranslator::FitsInReg8Operand(
      Register::FromParameterIndex(0, 3)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg8Operand(
      Register::FromParameterIndex(2, 3)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg8Operand(Register(0)));
  EXPECT_TRUE(
      RegisterTranslator::FitsInReg8Operand(Register(window_start() - 1)));
  EXPECT_FALSE(RegisterTranslator::FitsInReg8Operand(Register(kMaxInt8)));
  EXPECT_FALSE(RegisterTranslator::FitsInReg8Operand(Register(kMaxInt8 + 1)));
  for (int index = window_start(); index < window_limit(); index += 1) {
    EXPECT_FALSE(RegisterTranslator::FitsInReg8Operand(Register(index)));
  }
}

TEST_F(RegisterTranslatorTest, FitsInReg16Operand) {
  EXPECT_GT(window_start(), 0);
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(
      Register::FromParameterIndex(0, 3)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(
      Register::FromParameterIndex(2, 3)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(
      Register::FromParameterIndex(0, 999)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(
      Register::FromParameterIndex(0, Register::MaxParameterIndex() + 1)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(Register(0)));
  EXPECT_TRUE(
      RegisterTranslator::FitsInReg16Operand(Register(window_start() - 1)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(Register(kMaxInt8 + 1)));
  EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(Register(kMaxInt8 + 2)));
  for (int index = 0; index <= kMaxInt16 - window_width(); index += 1) {
    EXPECT_TRUE(RegisterTranslator::FitsInReg16Operand(Register(index)));
  }
  for (int index = Register::MaxRegisterIndex() - window_width() + 1;
       index < Register::MaxRegisterIndex() + 2; index += 1) {
    EXPECT_FALSE(RegisterTranslator::FitsInReg16Operand(Register(index)));
  }
}

TEST_F(RegisterTranslatorTest, NoTranslationRequired) {
  Register window_reg(window_start());
  Register local_reg(57);
  uint32_t operands[] = {local_reg.ToRawOperand()};
  translator()->TranslateInputRegisters(Bytecode::kLdar, operands, 1);
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(0, move_count());

  Register param_reg = Register::FromParameterIndex(129, 130);
  operands[0] = param_reg.ToRawOperand();
  translator()->TranslateInputRegisters(Bytecode::kAdd, operands, 1);
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(0, move_count());
}

TEST_F(RegisterTranslatorTest, TranslationRequired) {
  Register window_reg(window_start());
  Register local_reg(137);
  Register local_reg_translated(local_reg.index() + window_width());

  uint32_t operands[] = {local_reg.ToRawOperand()};
  translator()->TranslateInputRegisters(Bytecode::kLdar, operands, 1);
  EXPECT_EQ(1, move_count());
  EXPECT_TRUE(PopMoveAndMatch(local_reg_translated, window_reg));
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(1, move_count());
  EXPECT_FALSE(PopMoveAndMatch(window_reg, local_reg_translated));

  operands[0] = local_reg.ToRawOperand();
  translator()->TranslateInputRegisters(Bytecode::kStar, operands, 1);
  EXPECT_EQ(1, move_count());
  EXPECT_FALSE(PopMoveAndMatch(local_reg_translated, window_reg));
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(2, move_count());
  EXPECT_TRUE(PopMoveAndMatch(window_reg, local_reg_translated));

  Register param_reg = Register::FromParameterIndex(0, 130);
  operands[0] = {param_reg.ToRawOperand()};
  translator()->TranslateInputRegisters(Bytecode::kLdar, operands, 1);
  EXPECT_EQ(3, move_count());
  EXPECT_TRUE(PopMoveAndMatch(param_reg, window_reg));
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(3, move_count());
  EXPECT_FALSE(PopMoveAndMatch(window_reg, param_reg));

  operands[0] = {param_reg.ToRawOperand()};
  translator()->TranslateInputRegisters(Bytecode::kStar, operands, 1);
  EXPECT_EQ(3, move_count());
  EXPECT_FALSE(PopMoveAndMatch(local_reg_translated, window_reg));
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(4, move_count());
  EXPECT_TRUE(PopMoveAndMatch(window_reg, param_reg));
}

TEST_F(RegisterTranslatorTest, RangeTranslation) {
  Register window0(window_start());
  Register window1(window_start() + 1);
  Register window2(window_start() + 2);
  uint32_t operands[3];

  // Bytecode::kNew with valid range operand.
  Register constructor0(0);
  Register args0(1);
  operands[0] = constructor0.ToRawOperand();
  operands[1] = args0.ToRawOperand();
  operands[2] = 1;
  translator()->TranslateInputRegisters(Bytecode::kNew, operands, 3);
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(0, move_count());

  // Bytecode::kNewWide with valid range operand.
  Register constructor1(128);
  Register constructor1_translated(constructor1.index() + window_width());
  Register args1(129);
  Register args1_translated(args1.index() + window_width());
  operands[0] = constructor1.ToRawOperand();
  operands[1] = args1.ToRawOperand();
  operands[2] = 3;
  translator()->TranslateInputRegisters(Bytecode::kNewWide, operands, 3);
  translator()->TranslateOutputRegisters();
  EXPECT_EQ(0, move_count());
}

TEST_F(RegisterTranslatorTest, BadRange0) {
  // Bytecode::kNew with invalid range operand (kMaybeReg8).
  Register constructor1(128);
  Register args1(129);
  uint32_t operands[] = {constructor1.ToRawOperand(), args1.ToRawOperand(), 3};
  ASSERT_DEATH_IF_SUPPORTED(
      translator()->TranslateInputRegisters(Bytecode::kNew, operands, 3),
      kBadOperandRegex);
}

TEST_F(RegisterTranslatorTest, BadRange1) {
  // Bytecode::kForInPrepare with invalid range operand (kRegTriple8)
  Register for_in_state(160);
  Register for_in_state_translated(for_in_state.index() + window_width());
  uint32_t operands[] = {for_in_state.ToRawOperand()};
  ASSERT_DEATH_IF_SUPPORTED(translator()->TranslateInputRegisters(
                                Bytecode::kForInPrepare, operands, 1),
                            kBadOperandRegex);
}

TEST_F(RegisterTranslatorTest, BadRange2) {
  // Bytecode::kForInNext with invalid range operand (kRegPair8)
  Register receiver(192);
  Register receiver_translated(receiver.index() + window_width());
  Register index(193);
  Register index_translated(index.index() + window_width());
  Register cache_info_pair(194);
  Register cache_info_pair_translated(cache_info_pair.index() + window_width());
  uint32_t operands[] = {receiver.ToRawOperand(), index.ToRawOperand(),
                         cache_info_pair.ToRawOperand()};
  ASSERT_DEATH_IF_SUPPORTED(
      translator()->TranslateInputRegisters(Bytecode::kForInNext, operands, 3),
      kBadOperandRegex);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
