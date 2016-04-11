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
  for (int i = 0; i < 128; i++) {
    uint8_t operand_value = Register(i).ToOperand();
    Register r = Register::FromOperand(operand_value);
    CHECK_EQ(i, r.index());
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
  std::vector<uint8_t> operand_count(256);

  for (int i = 0; i <= kMaxInt8; i++) {
    Register r = Register(i);
    uint8_t operand = r.ToOperand();
    operand_count[operand] += 1;
    CHECK_EQ(operand_count[operand], 1);
  }

  int parameter_count = Register::MaxParameterIndex() + 1;
  for (int i = 0; i < parameter_count; i++) {
    Register r = Register::FromParameterIndex(i, parameter_count);
    uint8_t operand = r.ToOperand();
    operand_count[operand] += 1;
    CHECK_EQ(operand_count[operand], 1);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
