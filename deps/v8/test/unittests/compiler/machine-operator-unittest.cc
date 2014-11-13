// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator.h"
#include "src/compiler/operator-properties-inl.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {
namespace compiler {

#if GTEST_HAS_COMBINE

template <typename T>
class MachineOperatorTestWithParam
    : public ::testing::TestWithParam< ::testing::tuple<MachineType, T> > {
 protected:
  MachineType type() const { return ::testing::get<0>(B::GetParam()); }
  const T& GetParam() const { return ::testing::get<1>(B::GetParam()); }

 private:
  typedef ::testing::TestWithParam< ::testing::tuple<MachineType, T> > B;
};


namespace {

const MachineType kMachineReps[] = {kRepWord32, kRepWord64};


const MachineType kMachineTypes[] = {
    kMachFloat32, kMachFloat64,   kMachInt8,   kMachUint8,  kMachInt16,
    kMachUint16,  kMachInt32,     kMachUint32, kMachInt64,  kMachUint64,
    kMachPtr,     kMachAnyTagged, kRepBit,     kRepWord8,   kRepWord16,
    kRepWord32,   kRepWord64,     kRepFloat32, kRepFloat64, kRepTagged};

}  // namespace


// -----------------------------------------------------------------------------
// Load operator.


typedef MachineOperatorTestWithParam<LoadRepresentation>
    MachineLoadOperatorTest;


TEST_P(MachineLoadOperatorTest, InstancesAreGloballyShared) {
  MachineOperatorBuilder machine1(type());
  MachineOperatorBuilder machine2(type());
  EXPECT_EQ(machine1.Load(GetParam()), machine2.Load(GetParam()));
}


TEST_P(MachineLoadOperatorTest, NumberOfInputsAndOutputs) {
  MachineOperatorBuilder machine(type());
  const Operator* op = machine.Load(GetParam());

  EXPECT_EQ(2, op->ValueInputCount());
  EXPECT_EQ(1, op->EffectInputCount());
  EXPECT_EQ(1, op->ControlInputCount());
  EXPECT_EQ(4, OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(1, op->ValueOutputCount());
  EXPECT_EQ(1, op->EffectOutputCount());
  EXPECT_EQ(0, op->ControlOutputCount());
}


TEST_P(MachineLoadOperatorTest, OpcodeIsCorrect) {
  MachineOperatorBuilder machine(type());
  EXPECT_EQ(IrOpcode::kLoad, machine.Load(GetParam())->opcode());
}


TEST_P(MachineLoadOperatorTest, ParameterIsCorrect) {
  MachineOperatorBuilder machine(type());
  EXPECT_EQ(GetParam(),
            OpParameter<LoadRepresentation>(machine.Load(GetParam())));
}


INSTANTIATE_TEST_CASE_P(MachineOperatorTest, MachineLoadOperatorTest,
                        ::testing::Combine(::testing::ValuesIn(kMachineReps),
                                           ::testing::ValuesIn(kMachineTypes)));


// -----------------------------------------------------------------------------
// Store operator.


class MachineStoreOperatorTest
    : public MachineOperatorTestWithParam<
          ::testing::tuple<MachineType, WriteBarrierKind> > {
 protected:
  StoreRepresentation GetParam() const {
    return StoreRepresentation(
        ::testing::get<0>(MachineOperatorTestWithParam<
            ::testing::tuple<MachineType, WriteBarrierKind> >::GetParam()),
        ::testing::get<1>(MachineOperatorTestWithParam<
            ::testing::tuple<MachineType, WriteBarrierKind> >::GetParam()));
  }
};


TEST_P(MachineStoreOperatorTest, InstancesAreGloballyShared) {
  MachineOperatorBuilder machine1(type());
  MachineOperatorBuilder machine2(type());
  EXPECT_EQ(machine1.Store(GetParam()), machine2.Store(GetParam()));
}


TEST_P(MachineStoreOperatorTest, NumberOfInputsAndOutputs) {
  MachineOperatorBuilder machine(type());
  const Operator* op = machine.Store(GetParam());

  EXPECT_EQ(3, op->ValueInputCount());
  EXPECT_EQ(1, op->EffectInputCount());
  EXPECT_EQ(1, op->ControlInputCount());
  EXPECT_EQ(5, OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(0, op->ValueOutputCount());
  EXPECT_EQ(1, op->EffectOutputCount());
  EXPECT_EQ(0, op->ControlOutputCount());
}


TEST_P(MachineStoreOperatorTest, OpcodeIsCorrect) {
  MachineOperatorBuilder machine(type());
  EXPECT_EQ(IrOpcode::kStore, machine.Store(GetParam())->opcode());
}


TEST_P(MachineStoreOperatorTest, ParameterIsCorrect) {
  MachineOperatorBuilder machine(type());
  EXPECT_EQ(GetParam(),
            OpParameter<StoreRepresentation>(machine.Store(GetParam())));
}


INSTANTIATE_TEST_CASE_P(
    MachineOperatorTest, MachineStoreOperatorTest,
    ::testing::Combine(
        ::testing::ValuesIn(kMachineReps),
        ::testing::Combine(::testing::ValuesIn(kMachineTypes),
                           ::testing::Values(kNoWriteBarrier,
                                             kFullWriteBarrier))));


// -----------------------------------------------------------------------------
// Pure operators.


namespace {

struct PureOperator {
  const Operator* (MachineOperatorBuilder::*constructor)();
  IrOpcode::Value opcode;
  int value_input_count;
  int control_input_count;
  int value_output_count;
};


std::ostream& operator<<(std::ostream& os, const PureOperator& pop) {
  return os << IrOpcode::Mnemonic(pop.opcode);
}


const PureOperator kPureOperators[] = {
#define PURE(Name, value_input_count, control_input_count, value_output_count) \
  {                                                                            \
    &MachineOperatorBuilder::Name, IrOpcode::k##Name, value_input_count,       \
        control_input_count, value_output_count                                \
  }
    PURE(Word32And, 2, 0, 1), PURE(Word32Or, 2, 0, 1), PURE(Word32Xor, 2, 0, 1),
    PURE(Word32Shl, 2, 0, 1), PURE(Word32Shr, 2, 0, 1),
    PURE(Word32Sar, 2, 0, 1), PURE(Word32Ror, 2, 0, 1),
    PURE(Word32Equal, 2, 0, 1), PURE(Word64And, 2, 0, 1),
    PURE(Word64Or, 2, 0, 1), PURE(Word64Xor, 2, 0, 1), PURE(Word64Shl, 2, 0, 1),
    PURE(Word64Shr, 2, 0, 1), PURE(Word64Sar, 2, 0, 1),
    PURE(Word64Ror, 2, 0, 1), PURE(Word64Equal, 2, 0, 1),
    PURE(Int32Add, 2, 0, 1), PURE(Int32AddWithOverflow, 2, 0, 2),
    PURE(Int32Sub, 2, 0, 1), PURE(Int32SubWithOverflow, 2, 0, 2),
    PURE(Int32Mul, 2, 0, 1), PURE(Int32MulHigh, 2, 0, 1),
    PURE(Int32Div, 2, 1, 1), PURE(Uint32Div, 2, 1, 1), PURE(Int32Mod, 2, 1, 1),
    PURE(Uint32Mod, 2, 1, 1), PURE(Int32LessThan, 2, 0, 1),
    PURE(Int32LessThanOrEqual, 2, 0, 1), PURE(Uint32LessThan, 2, 0, 1),
    PURE(Uint32LessThanOrEqual, 2, 0, 1), PURE(Int64Add, 2, 0, 1),
    PURE(Int64Sub, 2, 0, 1), PURE(Int64Mul, 2, 0, 1), PURE(Int64Div, 2, 0, 1),
    PURE(Uint64Div, 2, 0, 1), PURE(Int64Mod, 2, 0, 1), PURE(Uint64Mod, 2, 0, 1),
    PURE(Int64LessThan, 2, 0, 1), PURE(Int64LessThanOrEqual, 2, 0, 1),
    PURE(Uint64LessThan, 2, 0, 1), PURE(ChangeFloat32ToFloat64, 1, 0, 1),
    PURE(ChangeFloat64ToInt32, 1, 0, 1), PURE(ChangeFloat64ToUint32, 1, 0, 1),
    PURE(ChangeInt32ToInt64, 1, 0, 1), PURE(ChangeUint32ToFloat64, 1, 0, 1),
    PURE(ChangeUint32ToUint64, 1, 0, 1),
    PURE(TruncateFloat64ToFloat32, 1, 0, 1),
    PURE(TruncateFloat64ToInt32, 1, 0, 1), PURE(TruncateInt64ToInt32, 1, 0, 1),
    PURE(Float64Add, 2, 0, 1), PURE(Float64Sub, 2, 0, 1),
    PURE(Float64Mul, 2, 0, 1), PURE(Float64Div, 2, 0, 1),
    PURE(Float64Mod, 2, 0, 1), PURE(Float64Sqrt, 1, 0, 1),
    PURE(Float64Equal, 2, 0, 1), PURE(Float64LessThan, 2, 0, 1),
    PURE(Float64LessThanOrEqual, 2, 0, 1), PURE(LoadStackPointer, 0, 0, 1),
    PURE(Float64Floor, 1, 0, 1), PURE(Float64Ceil, 1, 0, 1),
    PURE(Float64RoundTruncate, 1, 0, 1), PURE(Float64RoundTiesAway, 1, 0, 1)
#undef PURE
};


typedef MachineOperatorTestWithParam<PureOperator> MachinePureOperatorTest;

}  // namespace


TEST_P(MachinePureOperatorTest, InstancesAreGloballyShared) {
  const PureOperator& pop = GetParam();
  MachineOperatorBuilder machine1(type());
  MachineOperatorBuilder machine2(type());
  EXPECT_EQ((machine1.*pop.constructor)(), (machine2.*pop.constructor)());
}


TEST_P(MachinePureOperatorTest, NumberOfInputsAndOutputs) {
  MachineOperatorBuilder machine(type());
  const PureOperator& pop = GetParam();
  const Operator* op = (machine.*pop.constructor)();

  EXPECT_EQ(pop.value_input_count, op->ValueInputCount());
  EXPECT_EQ(0, op->EffectInputCount());
  EXPECT_EQ(pop.control_input_count, op->ControlInputCount());
  EXPECT_EQ(pop.value_input_count + pop.control_input_count,
            OperatorProperties::GetTotalInputCount(op));

  EXPECT_EQ(pop.value_output_count, op->ValueOutputCount());
  EXPECT_EQ(0, op->EffectOutputCount());
  EXPECT_EQ(0, op->ControlOutputCount());
}


TEST_P(MachinePureOperatorTest, MarkedAsPure) {
  MachineOperatorBuilder machine(type());
  const PureOperator& pop = GetParam();
  const Operator* op = (machine.*pop.constructor)();
  EXPECT_TRUE(op->HasProperty(Operator::kPure));
}


TEST_P(MachinePureOperatorTest, OpcodeIsCorrect) {
  MachineOperatorBuilder machine(type());
  const PureOperator& pop = GetParam();
  const Operator* op = (machine.*pop.constructor)();
  EXPECT_EQ(pop.opcode, op->opcode());
}


INSTANTIATE_TEST_CASE_P(
    MachineOperatorTest, MachinePureOperatorTest,
    ::testing::Combine(::testing::ValuesIn(kMachineReps),
                       ::testing::ValuesIn(kPureOperators)));

#endif  // GTEST_HAS_COMBINE


// -----------------------------------------------------------------------------
// Pseudo operators.


TEST(MachineOperatorTest, PseudoOperatorsWhenWordSizeIs32Bit) {
  MachineOperatorBuilder machine(kRepWord32);
  EXPECT_EQ(machine.Word32And(), machine.WordAnd());
  EXPECT_EQ(machine.Word32Or(), machine.WordOr());
  EXPECT_EQ(machine.Word32Xor(), machine.WordXor());
  EXPECT_EQ(machine.Word32Shl(), machine.WordShl());
  EXPECT_EQ(machine.Word32Shr(), machine.WordShr());
  EXPECT_EQ(machine.Word32Sar(), machine.WordSar());
  EXPECT_EQ(machine.Word32Ror(), machine.WordRor());
  EXPECT_EQ(machine.Word32Equal(), machine.WordEqual());
  EXPECT_EQ(machine.Int32Add(), machine.IntAdd());
  EXPECT_EQ(machine.Int32Sub(), machine.IntSub());
  EXPECT_EQ(machine.Int32Mul(), machine.IntMul());
  EXPECT_EQ(machine.Int32Div(), machine.IntDiv());
  EXPECT_EQ(machine.Uint32Div(), machine.UintDiv());
  EXPECT_EQ(machine.Int32Mod(), machine.IntMod());
  EXPECT_EQ(machine.Uint32Mod(), machine.UintMod());
  EXPECT_EQ(machine.Int32LessThan(), machine.IntLessThan());
  EXPECT_EQ(machine.Int32LessThanOrEqual(), machine.IntLessThanOrEqual());
}


TEST(MachineOperatorTest, PseudoOperatorsWhenWordSizeIs64Bit) {
  MachineOperatorBuilder machine(kRepWord64);
  EXPECT_EQ(machine.Word64And(), machine.WordAnd());
  EXPECT_EQ(machine.Word64Or(), machine.WordOr());
  EXPECT_EQ(machine.Word64Xor(), machine.WordXor());
  EXPECT_EQ(machine.Word64Shl(), machine.WordShl());
  EXPECT_EQ(machine.Word64Shr(), machine.WordShr());
  EXPECT_EQ(machine.Word64Sar(), machine.WordSar());
  EXPECT_EQ(machine.Word64Ror(), machine.WordRor());
  EXPECT_EQ(machine.Word64Equal(), machine.WordEqual());
  EXPECT_EQ(machine.Int64Add(), machine.IntAdd());
  EXPECT_EQ(machine.Int64Sub(), machine.IntSub());
  EXPECT_EQ(machine.Int64Mul(), machine.IntMul());
  EXPECT_EQ(machine.Int64Div(), machine.IntDiv());
  EXPECT_EQ(machine.Uint64Div(), machine.UintDiv());
  EXPECT_EQ(machine.Int64Mod(), machine.IntMod());
  EXPECT_EQ(machine.Uint64Mod(), machine.UintMod());
  EXPECT_EQ(machine.Int64LessThan(), machine.IntLessThan());
  EXPECT_EQ(machine.Int64LessThanOrEqual(), machine.IntLessThanOrEqual());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
