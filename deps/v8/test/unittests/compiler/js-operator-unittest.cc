// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/operator-properties.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

const LanguageMode kLanguageModes[] = {SLOPPY, STRICT, STRONG};


#if GTEST_HAS_COMBINE

template <typename T>
class JSOperatorTestWithLanguageModeAndParam
    : public TestWithZone,
      public ::testing::WithParamInterface<::testing::tuple<LanguageMode, T>> {
 protected:
  LanguageMode language_mode() const {
    return ::testing::get<0>(B::GetParam());
  }
  const T& GetParam() const { return ::testing::get<1>(B::GetParam()); }

 private:
  typedef ::testing::WithParamInterface<::testing::tuple<LanguageMode, T>> B;
};

#endif  // GTEST_HAS_COMBINE

}  // namespace


// -----------------------------------------------------------------------------
// Shared operators without language mode.

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
  int control_output_count;
};


const SharedOperator kSharedOperators[] = {
#define SHARED(Name, properties, value_input_count, frame_state_input_count, \
               effect_input_count, control_input_count, value_output_count,  \
               effect_output_count, control_output_count)                    \
  {                                                                          \
    &JSOperatorBuilder::Name, IrOpcode::kJS##Name, properties,               \
        value_input_count, frame_state_input_count, effect_input_count,      \
        control_input_count, value_output_count, effect_output_count,        \
        control_output_count                                                 \
  }
    SHARED(Equal, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(NotEqual, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(StrictEqual, Operator::kPure, 2, 0, 0, 0, 1, 0, 0),
    SHARED(StrictNotEqual, Operator::kPure, 2, 0, 0, 0, 1, 0, 0),
    SHARED(UnaryNot, Operator::kPure, 1, 0, 0, 0, 1, 0, 0),
    SHARED(ToBoolean, Operator::kPure, 1, 0, 0, 0, 1, 0, 0),
    SHARED(ToNumber, Operator::kNoProperties, 1, 1, 1, 1, 1, 1, 2),
    SHARED(ToString, Operator::kNoProperties, 1, 0, 1, 1, 1, 1, 2),
    SHARED(ToName, Operator::kNoProperties, 1, 1, 1, 1, 1, 1, 2),
    SHARED(ToObject, Operator::kNoProperties, 1, 1, 1, 1, 1, 1, 2),
    SHARED(Yield, Operator::kNoProperties, 1, 0, 1, 1, 1, 1, 2),
    SHARED(Create, Operator::kEliminatable, 0, 0, 1, 0, 1, 1, 0),
    SHARED(HasProperty, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(TypeOf, Operator::kPure, 1, 0, 0, 0, 1, 0, 0),
    SHARED(InstanceOf, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(CreateFunctionContext, Operator::kNoProperties, 1, 0, 1, 1, 1, 1, 2),
    SHARED(CreateWithContext, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(CreateBlockContext, Operator::kNoProperties, 2, 0, 1, 1, 1, 1, 2),
    SHARED(CreateModuleContext, Operator::kNoProperties, 2, 0, 1, 1, 1, 1, 2),
    SHARED(CreateScriptContext, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2)
#undef SHARED
};


std::ostream& operator<<(std::ostream& os, const SharedOperator& sop) {
  return os << IrOpcode::Mnemonic(sop.opcode);
}

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
  EXPECT_EQ(sop.value_input_count, op->ValueInputCount());
  EXPECT_EQ(context_input_count, OperatorProperties::GetContextInputCount(op));
  EXPECT_EQ(sop.frame_state_input_count,
            OperatorProperties::GetFrameStateInputCount(op));
  EXPECT_EQ(sop.effect_input_count, op->EffectInputCount());
  EXPECT_EQ(sop.control_input_count, op->ControlInputCount());
  EXPECT_EQ(sop.value_input_count + context_input_count +
                sop.frame_state_input_count + sop.effect_input_count +
                sop.control_input_count,
            OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(sop.value_output_count, op->ValueOutputCount());
  EXPECT_EQ(sop.effect_output_count, op->EffectOutputCount());
  EXPECT_EQ(sop.control_output_count, op->ControlOutputCount());
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


// -----------------------------------------------------------------------------
// Shared operators with language mode.


#if GTEST_HAS_COMBINE

namespace {

struct SharedOperatorWithLanguageMode {
  const Operator* (JSOperatorBuilder::*constructor)(LanguageMode);
  IrOpcode::Value opcode;
  Operator::Properties properties;
  int value_input_count;
  int frame_state_input_count;
  int effect_input_count;
  int control_input_count;
  int value_output_count;
  int effect_output_count;
  int control_output_count;
};


const SharedOperatorWithLanguageMode kSharedOperatorsWithLanguageMode[] = {
#define SHARED(Name, properties, value_input_count, frame_state_input_count, \
               effect_input_count, control_input_count, value_output_count,  \
               effect_output_count, control_output_count)                    \
  {                                                                          \
    &JSOperatorBuilder::Name, IrOpcode::kJS##Name, properties,               \
        value_input_count, frame_state_input_count, effect_input_count,      \
        control_input_count, value_output_count, effect_output_count,        \
        control_output_count                                                 \
  }
    SHARED(LessThan, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(GreaterThan, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(LessThanOrEqual, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(GreaterThanOrEqual, Operator::kNoProperties, 2, 1, 1, 1, 1, 1, 2),
    SHARED(BitwiseOr, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(BitwiseXor, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(BitwiseAnd, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(ShiftLeft, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(ShiftRight, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(ShiftRightLogical, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(Add, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(Subtract, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(Multiply, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(Divide, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(Modulus, Operator::kNoProperties, 2, 2, 1, 1, 1, 1, 2),
    SHARED(StoreProperty, Operator::kNoProperties, 3, 2, 1, 1, 0, 1, 2),
#undef SHARED
};


std::ostream& operator<<(std::ostream& os,
                         const SharedOperatorWithLanguageMode& sop) {
  return os << IrOpcode::Mnemonic(sop.opcode);
}

}  // namespace


class JSSharedOperatorWithLanguageModeTest
    : public JSOperatorTestWithLanguageModeAndParam<
          SharedOperatorWithLanguageMode> {};


TEST_P(JSSharedOperatorWithLanguageModeTest, InstancesAreGloballyShared) {
  const SharedOperatorWithLanguageMode& sop = GetParam();
  JSOperatorBuilder javascript1(zone());
  JSOperatorBuilder javascript2(zone());
  EXPECT_EQ((javascript1.*sop.constructor)(language_mode()),
            (javascript2.*sop.constructor)(language_mode()));
}


TEST_P(JSSharedOperatorWithLanguageModeTest, NumberOfInputsAndOutputs) {
  JSOperatorBuilder javascript(zone());
  const SharedOperatorWithLanguageMode& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)(language_mode());

  const int context_input_count = 1;
  EXPECT_EQ(sop.value_input_count, op->ValueInputCount());
  EXPECT_EQ(context_input_count, OperatorProperties::GetContextInputCount(op));
  EXPECT_EQ(sop.frame_state_input_count,
            OperatorProperties::GetFrameStateInputCount(op));
  EXPECT_EQ(sop.effect_input_count, op->EffectInputCount());
  EXPECT_EQ(sop.control_input_count, op->ControlInputCount());
  EXPECT_EQ(sop.value_input_count + context_input_count +
                sop.frame_state_input_count + sop.effect_input_count +
                sop.control_input_count,
            OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(sop.value_output_count, op->ValueOutputCount());
  EXPECT_EQ(sop.effect_output_count, op->EffectOutputCount());
  EXPECT_EQ(sop.control_output_count, op->ControlOutputCount());
}


TEST_P(JSSharedOperatorWithLanguageModeTest, OpcodeIsCorrect) {
  JSOperatorBuilder javascript(zone());
  const SharedOperatorWithLanguageMode& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)(language_mode());
  EXPECT_EQ(sop.opcode, op->opcode());
}


TEST_P(JSSharedOperatorWithLanguageModeTest, Parameter) {
  JSOperatorBuilder javascript(zone());
  const SharedOperatorWithLanguageMode& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)(language_mode());
  EXPECT_EQ(language_mode(), OpParameter<LanguageMode>(op));
}


TEST_P(JSSharedOperatorWithLanguageModeTest, Properties) {
  JSOperatorBuilder javascript(zone());
  const SharedOperatorWithLanguageMode& sop = GetParam();
  const Operator* op = (javascript.*sop.constructor)(language_mode());
  EXPECT_EQ(sop.properties, op->properties());
}


INSTANTIATE_TEST_CASE_P(
    JSOperatorTest, JSSharedOperatorWithLanguageModeTest,
    ::testing::Combine(::testing::ValuesIn(kLanguageModes),
                       ::testing::ValuesIn(kSharedOperatorsWithLanguageMode)));

#endif  // GTEST_HAS_COMBINE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
