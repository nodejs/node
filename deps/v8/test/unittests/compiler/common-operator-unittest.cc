// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/compiler/common-operator.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/operator-properties.h"
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
  int value_output_count;
  int effect_output_count;
  int control_output_count;
};


std::ostream& operator<<(std::ostream& os, const SharedOperator& fop) {
  return os << IrOpcode::Mnemonic(fop.opcode);
}

const SharedOperator kSharedOperators[] = {
#define SHARED(Name, properties, value_input_count, effect_input_count,      \
               control_input_count, value_output_count, effect_output_count, \
               control_output_count)                                         \
  {                                                                          \
    &CommonOperatorBuilder::Name, IrOpcode::k##Name, properties,             \
        value_input_count, effect_input_count, control_input_count,          \
        value_output_count, effect_output_count, control_output_count        \
  }
    SHARED(Dead, Operator::kFoldable, 0, 0, 0, 1, 1, 1),
    SHARED(IfTrue, Operator::kKontrol, 0, 0, 1, 0, 0, 1),
    SHARED(IfFalse, Operator::kKontrol, 0, 0, 1, 0, 0, 1),
    SHARED(IfSuccess, Operator::kKontrol, 0, 0, 1, 0, 0, 1),
    SHARED(IfException, Operator::kKontrol, 0, 1, 1, 1, 1, 1),
    SHARED(Throw, Operator::kKontrol, 0, 1, 1, 0, 0, 1),
    SHARED(Terminate, Operator::kKontrol, 0, 1, 1, 0, 0, 1)
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

  EXPECT_EQ(sop.value_output_count, op->ValueOutputCount());
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
  ~CommonOperatorTest() override {}

  CommonOperatorBuilder* common() { return &common_; }

 private:
  CommonOperatorBuilder common_;
};


const int kArguments[] = {1, 5, 6, 42, 100, 10000, 65000};


const size_t kCases[] = {3, 4, 100, 255, 1024, 65000};


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


const size_t kInputCounts[] = {3, 4, 100, 255, 1024, 65000};


const int32_t kInt32Values[] = {
    std::numeric_limits<int32_t>::min(), -1914954528, -1698749618, -1578693386,
    -1577976073, -1573998034, -1529085059, -1499540537, -1299205097,
    -1090814845, -938186388, -806828902, -750927650, -520676892, -513661538,
    -453036354, -433622833, -282638793, -28375, -27788, -22770, -18806, -14173,
    -11956, -11200, -10212, -8160, -3751, -2758, -1522, -121, -120, -118, -117,
    -106, -84, -80, -74, -59, -52, -48, -39, -35, -17, -11, -10, -9, -7, -5, 0,
    9, 12, 17, 23, 29, 31, 33, 35, 40, 47, 55, 56, 62, 64, 67, 68, 69, 74, 79,
    84, 89, 90, 97, 104, 118, 124, 126, 127, 7278, 17787, 24136, 24202, 25570,
    26680, 30242, 32399, 420886487, 642166225, 821912648, 822577803, 851385718,
    1212241078, 1411419304, 1589626102, 1596437184, 1876245816, 1954730266,
    2008792749, 2045320228, std::numeric_limits<int32_t>::max()};


const BranchHint kBranchHints[] = {BranchHint::kNone, BranchHint::kTrue,
                                   BranchHint::kFalse};

}  // namespace


TEST_F(CommonOperatorTest, End) {
  TRACED_FOREACH(size_t, input_count, kInputCounts) {
    const Operator* const op = common()->End(input_count);
    EXPECT_EQ(IrOpcode::kEnd, op->opcode());
    EXPECT_EQ(Operator::kKontrol, op->properties());
    EXPECT_EQ(0, op->ValueInputCount());
    EXPECT_EQ(0, op->EffectInputCount());
    EXPECT_EQ(input_count, static_cast<uint32_t>(op->ControlInputCount()));
    EXPECT_EQ(input_count, static_cast<uint32_t>(
                               OperatorProperties::GetTotalInputCount(op)));
    EXPECT_EQ(0, op->ValueOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(0, op->ControlOutputCount());
  }
}


TEST_F(CommonOperatorTest, Return) {
  TRACED_FOREACH(int, input_count, kArguments) {
    const Operator* const op = common()->Return(input_count);
    EXPECT_EQ(IrOpcode::kReturn, op->opcode());
    EXPECT_EQ(Operator::kNoThrow, op->properties());
    EXPECT_EQ(input_count + 1, op->ValueInputCount());
    EXPECT_EQ(1, op->EffectInputCount());
    EXPECT_EQ(1, op->ControlInputCount());
    EXPECT_EQ(3 + input_count, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ValueOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ControlOutputCount());
  }
}


TEST_F(CommonOperatorTest, Branch) {
  TRACED_FOREACH(BranchHint, hint, kBranchHints) {
    const Operator* const op = common()->Branch(hint);
    EXPECT_EQ(IrOpcode::kBranch, op->opcode());
    EXPECT_EQ(Operator::kKontrol, op->properties());
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


TEST_F(CommonOperatorTest, Switch) {
  TRACED_FOREACH(size_t, cases, kCases) {
    const Operator* const op = common()->Switch(cases);
    EXPECT_EQ(IrOpcode::kSwitch, op->opcode());
    EXPECT_EQ(Operator::kKontrol, op->properties());
    EXPECT_EQ(1, op->ValueInputCount());
    EXPECT_EQ(0, op->EffectInputCount());
    EXPECT_EQ(1, op->ControlInputCount());
    EXPECT_EQ(2, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ValueOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(static_cast<int>(cases), op->ControlOutputCount());
  }
}


TEST_F(CommonOperatorTest, IfValue) {
  TRACED_FOREACH(int32_t, value, kInt32Values) {
    const Operator* const op = common()->IfValue(value);
    EXPECT_EQ(IrOpcode::kIfValue, op->opcode());
    EXPECT_EQ(Operator::kKontrol, op->properties());
    EXPECT_EQ(value, OpParameter<int32_t>(op));
    EXPECT_EQ(0, op->ValueInputCount());
    EXPECT_EQ(0, op->EffectInputCount());
    EXPECT_EQ(1, op->ControlInputCount());
    EXPECT_EQ(1, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ValueOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ControlOutputCount());
  }
}


TEST_F(CommonOperatorTest, Select) {
  static const MachineRepresentation kMachineRepresentations[] = {
      MachineRepresentation::kBit,     MachineRepresentation::kWord8,
      MachineRepresentation::kWord16,  MachineRepresentation::kWord32,
      MachineRepresentation::kWord64,  MachineRepresentation::kFloat32,
      MachineRepresentation::kFloat64, MachineRepresentation::kTagged};


  TRACED_FOREACH(MachineRepresentation, rep, kMachineRepresentations) {
    TRACED_FOREACH(BranchHint, hint, kBranchHints) {
      const Operator* const op = common()->Select(rep, hint);
      EXPECT_EQ(IrOpcode::kSelect, op->opcode());
      EXPECT_EQ(Operator::kPure, op->properties());
      EXPECT_EQ(rep, SelectParametersOf(op).representation());
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


TEST_F(CommonOperatorTest, BeginRegion) {
  {
    const Operator* op =
        common()->BeginRegion(RegionObservability::kObservable);
    EXPECT_EQ(1, op->EffectInputCount());
    EXPECT_EQ(1, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(1, op->EffectOutputCount());
    EXPECT_EQ(0, op->ValueOutputCount());
  }
  {
    const Operator* op =
        common()->BeginRegion(RegionObservability::kNotObservable);
    EXPECT_EQ(1, op->EffectInputCount());
    EXPECT_EQ(1, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(1, op->EffectOutputCount());
    EXPECT_EQ(0, op->ValueOutputCount());
  }
}

TEST_F(CommonOperatorTest, FinishRegion) {
  const Operator* op = common()->FinishRegion();
  EXPECT_EQ(1, op->ValueInputCount());
  EXPECT_EQ(1, op->EffectInputCount());
  EXPECT_EQ(2, OperatorProperties::GetTotalInputCount(op));
  EXPECT_EQ(0, op->ControlOutputCount());
  EXPECT_EQ(1, op->EffectOutputCount());
  EXPECT_EQ(1, op->ValueOutputCount());
}

TEST_F(CommonOperatorTest, Projection) {
  TRACED_FORRANGE(size_t, index, 0, 3) {
    const Operator* op = common()->Projection(index);
    EXPECT_EQ(index, ProjectionIndexOf(op));
    EXPECT_EQ(1, op->ValueInputCount());
    EXPECT_EQ(1, op->ControlInputCount());
    EXPECT_EQ(2, OperatorProperties::GetTotalInputCount(op));
    EXPECT_EQ(0, op->ControlOutputCount());
    EXPECT_EQ(0, op->EffectOutputCount());
    EXPECT_EQ(1, op->ValueOutputCount());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
