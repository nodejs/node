// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"
#include "src/compiler/operator-properties-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

// -----------------------------------------------------------------------------
// Shared operators.

namespace {

struct SharedOperator {
  const Operator* (JSOperatorBuilder::*constructor)();
  IrOpcode::Value opcode;
  Operator::Properties properties;
  int value_input_count;
  int frame_state_input_count;
  int effect_input_count;
  int control_input_count;
  int value_output_count;
  int effect_output_count;
};


std::ostream& operator<<(std::ostream& os, const SharedOperator& sop) {
  return os << IrOpcode::Mnemonic(sop.opcode);
}


const SharedOperator kSharedOperators[] = {
#define SHARED(Name, properties, value_input_count, frame_state_input_count, \
               effect_input_count, control_input_count, value_output_count,  \
               effect_output_count)                                          \
  {                                                                          \
    &JSOperatorBuilder::Name, IrOpcode::kJS##Name, properties,               \
        value_input_count, frame_state_input_count, effect_input_count,      \
        control_input_count, value_output_count, effect_output_count         \
  }
    SHARED(Equal, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(NotEqual, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(StrictEqual, Operator::kPure, 2, 0, 0, 0, 1, 0),
    SHARED(StrictNotEqual, Operator::kPure, 2, 0, 0, 0, 1, 0),
    SHARED(LessThan, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(GreaterThan, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(LessThanOrEqual, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(GreaterThanOrEqual, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(BitwiseOr, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(BitwiseXor, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(BitwiseAnd, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(ShiftLeft, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(ShiftRight, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(ShiftRightLogical, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(Add, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(Subtract, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(Multiply, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(Divide, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(Modulus, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(UnaryNot, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(ToBoolean, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(ToNumber, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(ToString, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(ToName, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(ToObject, Operator::kNoProperties, 1, 1, 1, 1, 1, 1),
    SHARED(Yield, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(Create, Operator::kEliminatable, 0, 0, 1, 1, 1, 1),
    SHARED(HasProperty, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(TypeOf, Operator::kPure, 1, 0, 0, 0, 1, 0),
    SHARED(InstanceOf, Operator::kNoProperties, 2, 1, 1, 1, 1, 1),
    SHARED(Debugger, Operator::kNoProperties, 0, 0, 1, 1, 0, 1),
    SHARED(CreateFunctionContext, Operator::kNoProperties, 1, 0, 1, 1, 1, 1),
    SHARED(CreateWithContext, Operator::kNoProperties, 2, 0, 1, 1, 1, 1),
    SHARED(CreateBlockContext, Operator::kNoProperties, 2, 0, 1, 1, 1, 1),
    SHARED(CreateModuleContext, Operator::kNoProperties, 2, 0, 1, 1, 1, 1),
    SHARED(CreateGlobalContext, Operator::kNoProperties, 2, 0, 1, 1, 1, 1)
#undef SHARED
};

}  // namespace


class JSSharedOperatorTest
    : public TestWithZone,
      public ::testing::WithParamInterface<SharedOperator> {};


TEST_P(JSSharedOperatorTest, InstancesAreGloballyShared) {
  const SharedOperator& sop = GetParam();
  JSOperatorBuilder javascript1(zone());
  JSOperatorBuilder javascript2(zone());
  EXPECT_EQ((javascript1.*sop.constructor)(), (javascript2.*sop.constructor)());
}


TEST_P(JSSharedOperatorTest, NumberOfInputsAndOutputs) {
  JSOperatorBuilder javascript(zone());
  const SharedOperator& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)();

  const int context_input_count = 1;
  // TODO(jarin): Get rid of this hack.
  const int frame_state_input_count =
      FLAG_turbo_deoptimization ? sop.frame_state_input_count : 0;
  EXPECT_EQ(sop.value_input_count, op->ValueInputCount());
  EXPECT_EQ(context_input_count, OperatorProperties::GetContextInputCount(op));
  EXPECT_EQ(frame_state_input_count,
            OperatorProperties::GetFrameStateInputCount(op));
  EXPECT_EQ(sop.effect_input_count, op->EffectInputCount());
  EXPECT_EQ(sop.control_input_count, op->ControlInputCount());
  EXPECT_EQ(sop.value_input_count + context_input_count +
                frame_state_input_count + sop.effect_input_count +
                sop.control_input_count,
            OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(sop.value_output_count, op->ValueOutputCount());
  EXPECT_EQ(sop.effect_output_count, op->EffectOutputCount());
  EXPECT_EQ(0, op->ControlOutputCount());
}


TEST_P(JSSharedOperatorTest, OpcodeIsCorrect) {
  JSOperatorBuilder javascript(zone());
  const SharedOperator& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)();
  EXPECT_EQ(sop.opcode, op->opcode());
}


TEST_P(JSSharedOperatorTest, Properties) {
  JSOperatorBuilder javascript(zone());
  const SharedOperator& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)();
  EXPECT_EQ(sop.properties, op->properties());
}


INSTANTIATE_TEST_CASE_P(JSOperatorTest, JSSharedOperatorTest,
                        ::testing::ValuesIn(kSharedOperators));

}  // namespace compiler
}  // namespace internal
}  // namespace v8
