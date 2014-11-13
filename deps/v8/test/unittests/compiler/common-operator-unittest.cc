// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"

#include <limits>

#include "src/compiler/operator-properties-inl.h"
#include "test/unittests/test-utils.h"

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
    SHARED(IfTrue, Operator::kFoldable, 0, 0, 1, 0, 1),
    SHARED(IfFalse, Operator::kFoldable, 0, 0, 1, 0, 1),
    SHARED(Throw, Operator::kFoldable, 1, 1, 1, 0, 1),
    SHARED(Return, Operator::kNoProperties, 1, 1, 1, 0, 1)
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

  EXPECT_EQ(sop.value_input_count, op->ValueInputCount());
  EXPECT_EQ(sop.effect_input_count, op->EffectInputCount());
  EXPECT_EQ(sop.control_input_count, op->ControlInputCount());
  EXPECT_EQ(
      sop.value_input_count + sop.effect_input_count + sop.control_input_count,
      OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(0, op->ValueOutputCount());
  EXPECT_EQ(sop.effect_output_count, op->EffectOutputCount());
  EXPECT_EQ(sop.control_output_count, op->ControlOutputCount());
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


const int kArguments[] = {1, 5, 6, 42, 100, 10000, 65000};


const float kFloatValues[] = {-std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::min(),
                              -1.0f,
                              -0.0f,
                              0.0f,
                              1.0f,
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN(),
                              std::numeric_limits<float>::signaling_NaN()};


const double kDoubleValues[] = {-std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::min(),
                                -1.0,
                                -0.0,
                                0.0,
                                1.0,
                                std::numeric_limits<double>::max(),
                                std::numeric_limits<double>::infinity(),
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::signaling_NaN()};


const BranchHint kHints[] = {BranchHint::kNone, BranchHint::kTrue,
                             BranchHint::kFalse};

}  // namespace


TEST_F(CommonOperatorTest, Branch) {
  TRACED_FOREACH(BranchHint, hint, kHints) {
    const Operator* const op = common()->Branch(hint);
    EXPECT_EQ(IrOpcode::kBranch, op->opcode());
    EXPECT_EQ(Operator::kFoldable, op->properties());
    EXPECT_EQ(hint, BranchHintOf(op));
    EXPECT_EQ(1, op->ValueInputCount());
    EXPECT_EQ(0, op->EffectInputCount());
    EXPECT_EQ(1, op->ControlInputCount());
    EXPECT_EQ(2, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ValueOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(2, op->ControlOutputCount());
  }
}


TEST_F(CommonOperatorTest, Select) {
  static const MachineType kTypes[] = {
      kMachInt8,    kMachUint8,   kMachInt16,    kMachUint16,
      kMachInt32,   kMachUint32,  kMachInt64,    kMachUint64,
      kMachFloat32, kMachFloat64, kMachAnyTagged};
  TRACED_FOREACH(MachineType, type, kTypes) {
    TRACED_FOREACH(BranchHint, hint, kHints) {
      const Operator* const op = common()->Select(type, hint);
      EXPECT_EQ(IrOpcode::kSelect, op->opcode());
      EXPECT_EQ(Operator::kPure, op->properties());
      EXPECT_EQ(type, SelectParametersOf(op).type());
      EXPECT_EQ(hint, SelectParametersOf(op).hint());
      EXPECT_EQ(3, op->ValueInputCount());
      EXPECT_EQ(0, op->EffectInputCount());
      EXPECT_EQ(0, op->ControlInputCount());
      EXPECT_EQ(3, OperatorProperties::GetTotalInputCount(op));
      EXPECT_EQ(1, op->ValueOutputCount());
      EXPECT_EQ(0, op->EffectOutputCount());
      EXPECT_EQ(0, op->ControlOutputCount());
    }
  }
}


TEST_F(CommonOperatorTest, Float32Constant) {
  TRACED_FOREACH(float, value, kFloatValues) {
    const Operator* op = common()->Float32Constant(value);
    EXPECT_PRED2(base::bit_equal_to<float>(), value, OpParameter<float>(op));
    EXPECT_EQ(0, op->ValueInputCount());
    EXPECT_EQ(0, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ValueOutputCount());
  }
  TRACED_FOREACH(float, v1, kFloatValues) {
    TRACED_FOREACH(float, v2, kFloatValues) {
      const Operator* op1 = common()->Float32Constant(v1);
      const Operator* op2 = common()->Float32Constant(v2);
      EXPECT_EQ(bit_cast<uint32_t>(v1) == bit_cast<uint32_t>(v2),
                op1->Equals(op2));
    }
  }
}


TEST_F(CommonOperatorTest, Float64Constant) {
  TRACED_FOREACH(double, value, kFloatValues) {
    const Operator* op = common()->Float64Constant(value);
    EXPECT_PRED2(base::bit_equal_to<double>(), value, OpParameter<double>(op));
    EXPECT_EQ(0, op->ValueInputCount());
    EXPECT_EQ(0, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ValueOutputCount());
  }
  TRACED_FOREACH(double, v1, kFloatValues) {
    TRACED_FOREACH(double, v2, kFloatValues) {
      const Operator* op1 = common()->Float64Constant(v1);
      const Operator* op2 = common()->Float64Constant(v2);
      EXPECT_EQ(bit_cast<uint64_t>(v1) == bit_cast<uint64_t>(v2),
                op1->Equals(op2));
    }
  }
}


TEST_F(CommonOperatorTest, NumberConstant) {
  TRACED_FOREACH(double, value, kFloatValues) {
    const Operator* op = common()->NumberConstant(value);
    EXPECT_PRED2(base::bit_equal_to<double>(), value, OpParameter<double>(op));
    EXPECT_EQ(0, op->ValueInputCount());
    EXPECT_EQ(0, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ValueOutputCount());
  }
  TRACED_FOREACH(double, v1, kFloatValues) {
    TRACED_FOREACH(double, v2, kFloatValues) {
      const Operator* op1 = common()->NumberConstant(v1);
      const Operator* op2 = common()->NumberConstant(v2);
      EXPECT_EQ(bit_cast<uint64_t>(v1) == bit_cast<uint64_t>(v2),
                op1->Equals(op2));
    }
  }
}


TEST_F(CommonOperatorTest, ValueEffect) {
  TRACED_FOREACH(int, arguments, kArguments) {
    const Operator* op = common()->ValueEffect(arguments);
    EXPECT_EQ(arguments, op->ValueInputCount());
    EXPECT_EQ(arguments, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(1, op->EffectOutputCount());
    EXPECT_EQ(0, op->ValueOutputCount());
  }
}


TEST_F(CommonOperatorTest, Finish) {
  TRACED_FOREACH(int, arguments, kArguments) {
    const Operator* op = common()->Finish(arguments);
    EXPECT_EQ(1, op->ValueInputCount());
    EXPECT_EQ(arguments, op->EffectInputCount());
    EXPECT_EQ(arguments + 1, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ValueOutputCount());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
