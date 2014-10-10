// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-operator.h"

#include "src/compiler/operator-properties-inl.h"
#include "src/test/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(bmeurer): Drop once we use std::ostream instead of our OStream.
inline std::ostream& operator<<(std::ostream& os, const ElementAccess& access) {
  OStringStream ost;
  ost << access;
  return os << ost.c_str();
}


// -----------------------------------------------------------------------------
// Pure operators.


namespace {

struct PureOperator {
  const Operator* (SimplifiedOperatorBuilder::*constructor)();
  IrOpcode::Value opcode;
  Operator::Properties properties;
  int value_input_count;
};


std::ostream& operator<<(std::ostream& os, const PureOperator& pop) {
  return os << IrOpcode::Mnemonic(pop.opcode);
}


const PureOperator kPureOperators[] = {
#define PURE(Name, properties, input_count)              \
  {                                                      \
    &SimplifiedOperatorBuilder::Name, IrOpcode::k##Name, \
        Operator::kPure | properties, input_count        \
  }
    PURE(BooleanNot, Operator::kNoProperties, 1),
    PURE(NumberEqual, Operator::kCommutative, 2),
    PURE(NumberLessThan, Operator::kNoProperties, 2),
    PURE(NumberLessThanOrEqual, Operator::kNoProperties, 2),
    PURE(NumberAdd, Operator::kCommutative, 2),
    PURE(NumberSubtract, Operator::kNoProperties, 2),
    PURE(NumberMultiply, Operator::kCommutative, 2),
    PURE(NumberDivide, Operator::kNoProperties, 2),
    PURE(NumberModulus, Operator::kNoProperties, 2),
    PURE(NumberToInt32, Operator::kNoProperties, 1),
    PURE(NumberToUint32, Operator::kNoProperties, 1),
    PURE(StringEqual, Operator::kCommutative, 2),
    PURE(StringLessThan, Operator::kNoProperties, 2),
    PURE(StringLessThanOrEqual, Operator::kNoProperties, 2),
    PURE(StringAdd, Operator::kNoProperties, 2),
    PURE(ChangeTaggedToInt32, Operator::kNoProperties, 1),
    PURE(ChangeTaggedToUint32, Operator::kNoProperties, 1),
    PURE(ChangeTaggedToFloat64, Operator::kNoProperties, 1),
    PURE(ChangeInt32ToTagged, Operator::kNoProperties, 1),
    PURE(ChangeUint32ToTagged, Operator::kNoProperties, 1),
    PURE(ChangeFloat64ToTagged, Operator::kNoProperties, 1),
    PURE(ChangeBoolToBit, Operator::kNoProperties, 1),
    PURE(ChangeBitToBool, Operator::kNoProperties, 1)
#undef PURE
};

}  // namespace


class SimplifiedPureOperatorTest
    : public TestWithZone,
      public ::testing::WithParamInterface<PureOperator> {};


TEST_P(SimplifiedPureOperatorTest, InstancesAreGloballyShared) {
  const PureOperator& pop = GetParam();
  SimplifiedOperatorBuilder simplified1(zone());
  SimplifiedOperatorBuilder simplified2(zone());
  EXPECT_EQ((simplified1.*pop.constructor)(), (simplified2.*pop.constructor)());
}


TEST_P(SimplifiedPureOperatorTest, NumberOfInputsAndOutputs) {
  SimplifiedOperatorBuilder simplified(zone());
  const PureOperator& pop = GetParam();
  const Operator* op = (simplified.*pop.constructor)();

  EXPECT_EQ(pop.value_input_count, OperatorProperties::GetValueInputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetEffectInputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetControlInputCount(op));
  EXPECT_EQ(pop.value_input_count, OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(1, OperatorProperties::GetValueOutputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetEffectOutputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetControlOutputCount(op));
}


TEST_P(SimplifiedPureOperatorTest, OpcodeIsCorrect) {
  SimplifiedOperatorBuilder simplified(zone());
  const PureOperator& pop = GetParam();
  const Operator* op = (simplified.*pop.constructor)();
  EXPECT_EQ(pop.opcode, op->opcode());
}


TEST_P(SimplifiedPureOperatorTest, Properties) {
  SimplifiedOperatorBuilder simplified(zone());
  const PureOperator& pop = GetParam();
  const Operator* op = (simplified.*pop.constructor)();
  EXPECT_EQ(pop.properties, op->properties() & pop.properties);
}

INSTANTIATE_TEST_CASE_P(SimplifiedOperatorTest, SimplifiedPureOperatorTest,
                        ::testing::ValuesIn(kPureOperators));


// -----------------------------------------------------------------------------
// Element access operators.

namespace {

const ElementAccess kElementAccesses[] = {
    {kTaggedBase, FixedArray::kHeaderSize, Type::Any(), kMachAnyTagged},
    {kUntaggedBase, kNonHeapObjectHeaderSize - kHeapObjectTag, Type::Any(),
     kMachInt8},
    {kUntaggedBase, kNonHeapObjectHeaderSize - kHeapObjectTag, Type::Any(),
     kMachInt16},
    {kUntaggedBase, kNonHeapObjectHeaderSize - kHeapObjectTag, Type::Any(),
     kMachInt32},
    {kUntaggedBase, kNonHeapObjectHeaderSize - kHeapObjectTag, Type::Any(),
     kMachUint8},
    {kUntaggedBase, kNonHeapObjectHeaderSize - kHeapObjectTag, Type::Any(),
     kMachUint16},
    {kUntaggedBase, kNonHeapObjectHeaderSize - kHeapObjectTag, Type::Any(),
     kMachUint32},
    {kUntaggedBase, 0, Type::Signed32(), kMachInt8},
    {kUntaggedBase, 0, Type::Unsigned32(), kMachUint8},
    {kUntaggedBase, 0, Type::Signed32(), kMachInt16},
    {kUntaggedBase, 0, Type::Unsigned32(), kMachUint16},
    {kUntaggedBase, 0, Type::Signed32(), kMachInt32},
    {kUntaggedBase, 0, Type::Unsigned32(), kMachUint32},
    {kUntaggedBase, 0, Type::Number(), kRepFloat32},
    {kUntaggedBase, 0, Type::Number(), kRepFloat64},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Signed32(),
     kMachInt8},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Unsigned32(),
     kMachUint8},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Signed32(),
     kMachInt16},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Unsigned32(),
     kMachUint16},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Signed32(),
     kMachInt32},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Unsigned32(),
     kMachUint32},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Number(),
     kRepFloat32},
    {kTaggedBase, FixedTypedArrayBase::kDataOffset, Type::Number(),
     kRepFloat64}};

}  // namespace


class SimplifiedElementAccessOperatorTest
    : public TestWithZone,
      public ::testing::WithParamInterface<ElementAccess> {};


TEST_P(SimplifiedElementAccessOperatorTest, LoadElement) {
  SimplifiedOperatorBuilder simplified(zone());
  const ElementAccess& access = GetParam();
  const Operator* op = simplified.LoadElement(access);

  EXPECT_EQ(IrOpcode::kLoadElement, op->opcode());
  EXPECT_EQ(Operator::kNoThrow | Operator::kNoWrite, op->properties());
  EXPECT_EQ(access, ElementAccessOf(op));

  EXPECT_EQ(3, OperatorProperties::GetValueInputCount(op));
  EXPECT_EQ(1, OperatorProperties::GetEffectInputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetControlInputCount(op));
  EXPECT_EQ(4, OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(1, OperatorProperties::GetValueOutputCount(op));
  EXPECT_EQ(1, OperatorProperties::GetEffectOutputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetControlOutputCount(op));
}


TEST_P(SimplifiedElementAccessOperatorTest, StoreElement) {
  SimplifiedOperatorBuilder simplified(zone());
  const ElementAccess& access = GetParam();
  const Operator* op = simplified.StoreElement(access);

  EXPECT_EQ(IrOpcode::kStoreElement, op->opcode());
  EXPECT_EQ(Operator::kNoRead | Operator::kNoThrow, op->properties());
  EXPECT_EQ(access, ElementAccessOf(op));

  EXPECT_EQ(4, OperatorProperties::GetValueInputCount(op));
  EXPECT_EQ(1, OperatorProperties::GetEffectInputCount(op));
  EXPECT_EQ(1, OperatorProperties::GetControlInputCount(op));
  EXPECT_EQ(6, OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(0, OperatorProperties::GetValueOutputCount(op));
  EXPECT_EQ(1, OperatorProperties::GetEffectOutputCount(op));
  EXPECT_EQ(0, OperatorProperties::GetControlOutputCount(op));
}


INSTANTIATE_TEST_CASE_P(SimplifiedOperatorTest,
                        SimplifiedElementAccessOperatorTest,
                        ::testing::ValuesIn(kElementAccesses));

}  // namespace compiler
}  // namespace internal
}  // namespace v8
