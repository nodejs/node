// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"

#include <limits>

#include "src/compiler/operator-properties-inl.h"
#include "src/test/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {


// -----------------------------------------------------------------------------
// Shared operators.


namespace {

struct SharedOperator {
  const Operator* (CommonOperatorBuilder::*constructor)();
  IrOpcode::Value opcode;
  Operator::Properties properties;
  int value_input_count;
  int effect_input_count;
  int control_input_count;
  int effect_output_count;
  int control_output_count;
};


std::ostream& operator<<(std::ostream& os, const SharedOperator& fop) {
  return os << IrOpcode::Mnemonic(fop.opcode);
}


const SharedOperator kSharedOperators[] = {
#define SHARED(Name, properties, value_input_count, effect_input_count,        \
               control_input_count, effect_output_count, control_output_count) \
  {                                                                            \
    &CommonOperatorBuilder::Name, IrOpcode::k##Name, properties,               \
        value_input_count, effect_input_count, control_input_count,            \
        effect_output_count, control_output_count                              \
  }
    SHARED(Dead, Operator::kFoldable, 0, 0, 0, 0, 1),
    SHARED(End, Operator::kFoldable, 0, 0, 1, 0, 0),
    SHARED(Branch, Operator::kFoldable, 1, 0, 1, 0, 2),
    SHARED(IfTrue, Operator::kFoldable, 0, 0, 1, 0, 1),
    SHARED(IfFalse, Operator::kFoldable, 0, 0, 1, 0, 1),
    SHARED(Throw, Operator::kFoldable, 1, 0, 1, 0, 1),
    SHARED(Return, Operator::kNoProperties, 1, 1, 1, 1, 1),
    SHARED(ControlEffect, Operator::kPure, 0, 0, 1, 1, 0)
#undef SHARED
};


class CommonSharedOperatorTest
    : public TestWithZone,
      public ::testing::WithParamInterface<SharedOperator> {};

}  // namespace


TEST_P(CommonSharedOperatorTest, InstancesAreGloballyShared) {
  const SharedOperator& sop = GetParam();
  CommonOperatorBuilder common1(zone());
  CommonOperatorBuilder common2(zone());
  EXPECT_EQ((common1.*sop.constructor)(), (common2.*sop.constructor)());
}


TEST_P(CommonSharedOperatorTest, NumberOfInputsAndOutputs) {
  CommonOperatorBuilder common(zone());
  const SharedOperator& sop = GetParam();
  const Operator* op = (common.*sop.constructor)();

  EXPECT_EQ(sop.value_input_count, OperatorProperties::GetValueInputCount(op));
  EXPECT_EQ(sop.effect_input_count,
            OperatorProperties::GetEffectInputCount(op));
  EXPECT_EQ(sop.control_input_count,
            OperatorProperties::GetControlInputCount(op));
  EXPECT_EQ(
      sop.value_input_count + sop.effect_input_count + sop.control_input_count,
      OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(0, OperatorProperties::GetValueOutputCount(op));
  EXPECT_EQ(sop.effect_output_count,
            OperatorProperties::GetEffectOutputCount(op));
  EXPECT_EQ(sop.control_output_count,
            OperatorProperties::GetControlOutputCount(op));
}


TEST_P(CommonSharedOperatorTest, OpcodeIsCorrect) {
  CommonOperatorBuilder common(zone());
  const SharedOperator& sop = GetParam();
  const Operator* op = (common.*sop.constructor)();
  EXPECT_EQ(sop.opcode, op->opcode());
}


TEST_P(CommonSharedOperatorTest, Properties) {
  CommonOperatorBuilder common(zone());
  const SharedOperator& sop = GetParam();
  const Operator* op = (common.*sop.constructor)();
  EXPECT_EQ(sop.properties, op->properties());
}


INSTANTIATE_TEST_CASE_P(CommonOperatorTest, CommonSharedOperatorTest,
                        ::testing::ValuesIn(kSharedOperators));


// -----------------------------------------------------------------------------
// Other operators.


namespace {

class CommonOperatorTest : public TestWithZone {
 public:
  CommonOperatorTest() : common_(zone()) {}
  virtual ~CommonOperatorTest() {}

  CommonOperatorBuilder* common() { return &common_; }

 private:
  CommonOperatorBuilder common_;
};


const int kArguments[] = {1, 5, 6, 42, 100, 10000, kMaxInt};

const float kFloat32Values[] = {
    std::numeric_limits<float>::min(), -1.0f, -0.0f, 0.0f, 1.0f,
    std::numeric_limits<float>::max()};

}  // namespace


TEST_F(CommonOperatorTest, Float32Constant) {
  TRACED_FOREACH(float, value, kFloat32Values) {
    const Operator* op = common()->Float32Constant(value);
    EXPECT_FLOAT_EQ(value, OpParameter<float>(op));
    EXPECT_EQ(0, OperatorProperties::GetValueInputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetControlOutputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetEffectOutputCount(op));
    EXPECT_EQ(1, OperatorProperties::GetValueOutputCount(op));
  }
}


TEST_F(CommonOperatorTest, ValueEffect) {
  TRACED_FOREACH(int, arguments, kArguments) {
    const Operator* op = common()->ValueEffect(arguments);
    EXPECT_EQ(arguments, OperatorProperties::GetValueInputCount(op));
    EXPECT_EQ(arguments, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetControlOutputCount(op));
    EXPECT_EQ(1, OperatorProperties::GetEffectOutputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetValueOutputCount(op));
  }
}


TEST_F(CommonOperatorTest, Finish) {
  TRACED_FOREACH(int, arguments, kArguments) {
    const Operator* op = common()->Finish(arguments);
    EXPECT_EQ(1, OperatorProperties::GetValueInputCount(op));
    EXPECT_EQ(arguments, OperatorProperties::GetEffectInputCount(op));
    EXPECT_EQ(arguments + 1, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetControlOutputCount(op));
    EXPECT_EQ(0, OperatorProperties::GetEffectOutputCount(op));
    EXPECT_EQ(1, OperatorProperties::GetValueOutputCount(op));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
