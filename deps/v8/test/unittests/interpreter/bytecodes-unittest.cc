// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/v8.h"

#include "src/interpreter/bytecodes.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

TEST(OperandConversion, Registers) {
  int register_count = Register::MaxRegisterIndex() + 1;
  int step = register_count / 7;
  for (int i = 0; i < register_count; i += step) {
    if (i <= kMaxInt8) {
      uint8_t operand0 = Register(i).ToOperand();
      Register reg0 = Register::FromOperand(operand0);
      CHECK_EQ(i, reg0.index());
    }

    uint16_t operand1 = Register(i).ToWideOperand();
    Register reg1 = Register::FromWideOperand(operand1);
    CHECK_EQ(i, reg1.index());

    uint32_t operand2 = Register(i).ToRawOperand();
    Register reg2 = Register::FromRawOperand(operand2);
    CHECK_EQ(i, reg2.index());
  }

  for (int i = 0; i <= kMaxUInt8; i++) {
    uint8_t operand = static_cast<uint8_t>(i);
    Register reg = Register::FromOperand(operand);
    if (i > 0 && i < -kMinInt8) {
      CHECK(reg.is_parameter());
    } else {
      CHECK(!reg.is_parameter());
    }
  }
}

TEST(OperandConversion, Parameters) {
  int parameter_counts[] = {7, 13, 99};

  size_t count = sizeof(parameter_counts) / sizeof(parameter_counts[0]);
  for (size_t p = 0; p < count; p++) {
    int parameter_count = parameter_counts[p];
    for (int i = 0; i < parameter_count; i++) {
      Register r = Register::FromParameterIndex(i, parameter_count);
      uint8_t operand_value = r.ToOperand();
      Register s = Register::FromOperand(operand_value);
      CHECK_EQ(i, s.ToParameterIndex(parameter_count));
    }
  }
}

TEST(OperandConversion, RegistersParametersNoOverlap) {
  int register_count = Register::MaxRegisterIndex() + 1;
  int parameter_count = Register::MaxParameterIndex() + 1;
  int32_t register_space_size = base::bits::RoundUpToPowerOfTwo32(
      static_cast<uint32_t>(register_count + parameter_count));
  uint32_t range = static_cast<uint32_t>(register_space_size);
  std::vector<uint8_t> operand_count(range);

  for (int i = 0; i < register_count; i += 1) {
    Register r = Register(i);
    uint32_t operand = r.ToWideOperand();
    CHECK_LT(operand, operand_count.size());
    operand_count[operand] += 1;
    CHECK_EQ(operand_count[operand], 1);
  }

  for (int i = 0; i < parameter_count; i += 1) {
    Register r = Register::FromParameterIndex(i, parameter_count);
    uint32_t operand = r.ToWideOperand();
    CHECK_LT(operand, operand_count.size());
    operand_count[operand] += 1;
    CHECK_EQ(operand_count[operand], 1);
  }
}

TEST(Bytecodes, HasAnyRegisterOperands) {
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kAdd), 1);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kCall), 2);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kCallRuntime), 1);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kCallRuntimeWide), 1);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kCallRuntimeForPair),
           2);
  CHECK_EQ(
      Bytecodes::NumberOfRegisterOperands(Bytecode::kCallRuntimeForPairWide),
      2);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kDeletePropertyStrict),
           1);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kForInPrepare), 1);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kForInPrepareWide), 1);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kInc), 0);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kJumpIfTrue), 0);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kNew), 2);
  CHECK_EQ(Bytecodes::NumberOfRegisterOperands(Bytecode::kToName), 0);
}

TEST(Bytecodes, RegisterOperandBitmaps) {
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kAdd), 1);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kCallRuntimeForPair),
           10);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kStar), 1);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kMov), 3);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kTestIn), 1);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kForInPrepare), 1);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kForInDone), 3);
  CHECK_EQ(Bytecodes::GetRegisterOperandBitmap(Bytecode::kForInNext), 7);
}

TEST(Bytecodes, RegisterOperands) {
  CHECK(Bytecodes::IsRegisterOperandType(OperandType::kReg8));
  CHECK(Bytecodes::IsRegisterInputOperandType(OperandType::kReg8));
  CHECK(!Bytecodes::IsRegisterOutputOperandType(OperandType::kReg8));
  CHECK(!Bytecodes::IsRegisterInputOperandType(OperandType::kRegOut8));
  CHECK(Bytecodes::IsRegisterOutputOperandType(OperandType::kRegOut8));

#define IS_REGISTER_OPERAND_TYPE(Name, _) \
  CHECK(Bytecodes::IsRegisterOperandType(OperandType::k##Name));
  REGISTER_OPERAND_TYPE_LIST(IS_REGISTER_OPERAND_TYPE)
#undef IS_REGISTER_OPERAND_TYPE

#define IS_NOT_REGISTER_OPERAND_TYPE(Name, _) \
  CHECK(!Bytecodes::IsRegisterOperandType(OperandType::k##Name));
  NON_REGISTER_OPERAND_TYPE_LIST(IS_NOT_REGISTER_OPERAND_TYPE)
#undef IS_NOT_REGISTER_OPERAND_TYPE

#define IS_REGISTER_INPUT_OPERAND_TYPE(Name, _) \
  CHECK(Bytecodes::IsRegisterInputOperandType(OperandType::k##Name));
  REGISTER_INPUT_OPERAND_TYPE_LIST(IS_REGISTER_INPUT_OPERAND_TYPE)
#undef IS_REGISTER_INPUT_OPERAND_TYPE

#define IS_NOT_REGISTER_INPUT_OPERAND_TYPE(Name, _) \
  CHECK(!Bytecodes::IsRegisterInputOperandType(OperandType::k##Name));
  NON_REGISTER_OPERAND_TYPE_LIST(IS_NOT_REGISTER_INPUT_OPERAND_TYPE);
  REGISTER_OUTPUT_OPERAND_TYPE_LIST(IS_NOT_REGISTER_INPUT_OPERAND_TYPE)
#undef IS_NOT_REGISTER_INPUT_OPERAND_TYPE

#define IS_REGISTER_OUTPUT_OPERAND_TYPE(Name, _) \
  CHECK(Bytecodes::IsRegisterOutputOperandType(OperandType::k##Name));
  REGISTER_OUTPUT_OPERAND_TYPE_LIST(IS_REGISTER_OUTPUT_OPERAND_TYPE)
#undef IS_REGISTER_OUTPUT_OPERAND_TYPE

#define IS_NOT_REGISTER_OUTPUT_OPERAND_TYPE(Name, _) \
  CHECK(!Bytecodes::IsRegisterOutputOperandType(OperandType::k##Name));
  NON_REGISTER_OPERAND_TYPE_LIST(IS_NOT_REGISTER_OUTPUT_OPERAND_TYPE)
  REGISTER_INPUT_OPERAND_TYPE_LIST(IS_NOT_REGISTER_OUTPUT_OPERAND_TYPE)
#undef IS_NOT_REGISTER_INPUT_OPERAND_TYPE
}

TEST(Bytecodes, DebugBreak) {
  for (uint32_t i = 0; i < Bytecodes::ToByte(Bytecode::kLast); i++) {
    Bytecode bytecode = Bytecodes::FromByte(i);
    Bytecode debugbreak = Bytecodes::GetDebugBreak(bytecode);
    if (!Bytecodes::IsDebugBreak(debugbreak)) {
      PrintF("Bytecode %s has no matching debug break with length %d\n",
             Bytecodes::ToString(bytecode), Bytecodes::Size(bytecode));
      CHECK(false);
    }
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
