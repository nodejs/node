// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-operator.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/operator-properties.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace machine_operator_unittest {

template <typename T>
class MachineOperatorTestWithParam
    : public TestWithZone,
      public ::testing::WithParamInterface<
          ::testing::tuple<MachineRepresentation, T> > {
 protected:
  MachineRepresentation representation() const {
    return ::testing::get<0>(B::GetParam());
  }
  const T& GetParam() const { return ::testing::get<1>(B::GetParam()); }

 private:
  using B = ::testing::WithParamInterface<
      ::testing::tuple<MachineRepresentation, T> >;
};


const MachineRepresentation kMachineReps[] = {MachineRepresentation::kWord32,
                                              MachineRepresentation::kWord64};


const MachineType kMachineTypesForAccess[] = {
    MachineType::Float32(), MachineType::Float64(),  MachineType::Int8(),
    MachineType::Uint8(),   MachineType::Int16(),    MachineType::Uint16(),
    MachineType::Int32(),   MachineType::Uint32(),   MachineType::Int64(),
    MachineType::Uint64(),  MachineType::AnyTagged()};


const MachineRepresentation kRepresentationsForStore[] = {
    MachineRepresentation::kFloat32, MachineRepresentation::kFloat64,
    MachineRepresentation::kWord8,   MachineRepresentation::kWord16,
    MachineRepresentation::kWord32,  MachineRepresentation::kWord64,
    MachineRepresentation::kTagged};


// -----------------------------------------------------------------------------
// Load operator.

using MachineLoadOperatorTest =
    MachineOperatorTestWithParam<LoadRepresentation>;

TEST_P(MachineLoadOperatorTest, InstancesAreGloballyShared) {
  MachineOperatorBuilder machine1(zone(), representation());
  MachineOperatorBuilder machine2(zone(), representation());
  EXPECT_EQ(machine1.Load(GetParam()), machine2.Load(GetParam()));
}


TEST_P(MachineLoadOperatorTest, NumberOfInputsAndOutputs) {
  MachineOperatorBuilder machine(zone(), representation());
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
  MachineOperatorBuilder machine(zone(), representation());
  EXPECT_EQ(IrOpcode::kLoad, machine.Load(GetParam())->opcode());
}


TEST_P(MachineLoadOperatorTest, ParameterIsCorrect) {
  MachineOperatorBuilder machine(zone(), representation());
  EXPECT_EQ(GetParam(), LoadRepresentationOf(machine.Load(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    MachineOperatorTest, MachineLoadOperatorTest,
    ::testing::Combine(::testing::ValuesIn(kMachineReps),
                       ::testing::ValuesIn(kMachineTypesForAccess)));

// -----------------------------------------------------------------------------
// Store operator.


class MachineStoreOperatorTest
    : public MachineOperatorTestWithParam<
          ::testing::tuple<MachineRepresentation, WriteBarrierKind> > {
 protected:
  StoreRepresentation GetParam() const {
    return StoreRepresentation(
        ::testing::get<0>(
            MachineOperatorTestWithParam< ::testing::tuple<
                MachineRepresentation, WriteBarrierKind> >::GetParam()),
        ::testing::get<1>(
            MachineOperatorTestWithParam< ::testing::tuple<
                MachineRepresentation, WriteBarrierKind> >::GetParam()));
  }
};


TEST_P(MachineStoreOperatorTest, InstancesAreGloballyShared) {
  MachineOperatorBuilder machine1(zone(), representation());
  MachineOperatorBuilder machine2(zone(), representation());
  EXPECT_EQ(machine1.Store(GetParam()), machine2.Store(GetParam()));
}


TEST_P(MachineStoreOperatorTest, NumberOfInputsAndOutputs) {
  MachineOperatorBuilder machine(zone(), representation());
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
  MachineOperatorBuilder machine(zone(), representation());
  EXPECT_EQ(IrOpcode::kStore, machine.Store(GetParam())->opcode());
}


TEST_P(MachineStoreOperatorTest, ParameterIsCorrect) {
  MachineOperatorBuilder machine(zone(), representation());
  EXPECT_EQ(GetParam(), StoreRepresentationOf(machine.Store(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(
    MachineOperatorTest, MachineStoreOperatorTest,
    ::testing::Combine(
        ::testing::ValuesIn(kMachineReps),
        ::testing::Combine(::testing::ValuesIn(kRepresentationsForStore),
                           ::testing::Values(kNoWriteBarrier,
                                             kFullWriteBarrier))));

// -----------------------------------------------------------------------------
// Pure operators.

struct PureOperator {
  const Operator* (MachineOperatorBuilder::*constructor)();
  char const* const constructor_name;
  int value_input_count;
  int control_input_count;
  int value_output_count;
};


std::ostream& operator<<(std::ostream& os, PureOperator const& pop) {
  return os << pop.constructor_name;
}

const PureOperator kPureOperators[] = {
#define PURE(Name, value_input_count, control_input_count, value_output_count) \
  {                                                                            \
    &MachineOperatorBuilder::Name, #Name, value_input_count,                   \
        control_input_count, value_output_count                                \
  }
    PURE(Word32And, 2, 0, 1),                 // --
    PURE(Word32Or, 2, 0, 1),                  // --
    PURE(Word32Xor, 2, 0, 1),                 // --
    PURE(Word32Shl, 2, 0, 1),                 // --
    PURE(Word32Shr, 2, 0, 1),                 // --
    PURE(Word32Sar, 2, 0, 1),                 // --
    PURE(Word32Ror, 2, 0, 1),                 // --
    PURE(Word32Equal, 2, 0, 1),               // --
    PURE(Word32Clz, 1, 0, 1),                 // --
    PURE(Word64And, 2, 0, 1),                 // --
    PURE(Word64Or, 2, 0, 1),                  // --
    PURE(Word64Xor, 2, 0, 1),                 // --
    PURE(Word64Shl, 2, 0, 1),                 // --
    PURE(Word64Shr, 2, 0, 1),                 // --
    PURE(Word64Sar, 2, 0, 1),                 // --
    PURE(Word64Ror, 2, 0, 1),                 // --
    PURE(Word64Equal, 2, 0, 1),               // --
    PURE(Int32Add, 2, 0, 1),                  // --
    PURE(Int32Sub, 2, 0, 1),                  // --
    PURE(Int32Mul, 2, 0, 1),                  // --
    PURE(Int32MulHigh, 2, 0, 1),              // --
    PURE(Int32Div, 2, 1, 1),                  // --
    PURE(Uint32Div, 2, 1, 1),                 // --
    PURE(Int32Mod, 2, 1, 1),                  // --
    PURE(Uint32Mod, 2, 1, 1),                 // --
    PURE(Int32LessThan, 2, 0, 1),             // --
    PURE(Int32LessThanOrEqual, 2, 0, 1),      // --
    PURE(Uint32LessThan, 2, 0, 1),            // --
    PURE(Uint32LessThanOrEqual, 2, 0, 1),     // --
    PURE(Int64Add, 2, 0, 1),                  // --
    PURE(Int64Sub, 2, 0, 1),                  // --
    PURE(Int64Mul, 2, 0, 1),                  // --
    PURE(Int64Div, 2, 1, 1),                  // --
    PURE(Uint64Div, 2, 1, 1),                 // --
    PURE(Int64Mod, 2, 1, 1),                  // --
    PURE(Uint64Mod, 2, 1, 1),                 // --
    PURE(Int64LessThan, 2, 0, 1),             // --
    PURE(Int64LessThanOrEqual, 2, 0, 1),      // --
    PURE(Uint64LessThan, 2, 0, 1),            // --
    PURE(Uint64LessThanOrEqual, 2, 0, 1),     // --
    PURE(ChangeFloat32ToFloat64, 1, 0, 1),    // --
    PURE(ChangeFloat64ToInt32, 1, 0, 1),      // --
    PURE(ChangeFloat64ToUint32, 1, 0, 1),     // --
    PURE(ChangeInt32ToInt64, 1, 0, 1),        // --
    PURE(ChangeUint32ToFloat64, 1, 0, 1),     // --
    PURE(ChangeUint32ToUint64, 1, 0, 1),      // --
    PURE(TruncateFloat64ToFloat32, 1, 0, 1),  // --
    PURE(TruncateInt64ToInt32, 1, 0, 1),      // --
    PURE(Float32Abs, 1, 0, 1),                // --
    PURE(Float32Add, 2, 0, 1),                // --
    PURE(Float32Sub, 2, 0, 1),                // --
    PURE(Float32Mul, 2, 0, 1),                // --
    PURE(Float32Div, 2, 0, 1),                // --
    PURE(Float32Sqrt, 1, 0, 1),               // --
    PURE(Float32Equal, 2, 0, 1),              // --
    PURE(Float32LessThan, 2, 0, 1),           // --
    PURE(Float32LessThanOrEqual, 2, 0, 1),    // --
    PURE(Float32Neg, 1, 0, 1),                // --
    PURE(Float64Abs, 1, 0, 1),                // --
    PURE(Float64Add, 2, 0, 1),                // --
    PURE(Float64Sub, 2, 0, 1),                // --
    PURE(Float64Mul, 2, 0, 1),                // --
    PURE(Float64Div, 2, 0, 1),                // --
    PURE(Float64Mod, 2, 0, 1),                // --
    PURE(Float64Sqrt, 1, 0, 1),               // --
    PURE(Float64Max, 2, 0, 1),                // --
    PURE(Float64Min, 2, 0, 1),                // --
    PURE(Float64Equal, 2, 0, 1),              // --
    PURE(Float64LessThan, 2, 0, 1),           // --
    PURE(Float64LessThanOrEqual, 2, 0, 1),    // --
    PURE(Float64ExtractLowWord32, 1, 0, 1),   // --
    PURE(Float64ExtractHighWord32, 1, 0, 1),  // --
    PURE(Float64InsertLowWord32, 2, 0, 1),    // --
    PURE(Float64InsertHighWord32, 2, 0, 1),   // --
    PURE(Float64Neg, 1, 0, 1),                // --
#undef PURE
};


class MachinePureOperatorTest : public TestWithZone {
 protected:
  MachineRepresentation word_type() {
    return MachineType::PointerRepresentation();
  }
};


TEST_F(MachinePureOperatorTest, PureOperators) {
  TRACED_FOREACH(MachineRepresentation, machine_rep1, kMachineReps) {
    MachineOperatorBuilder machine1(zone(), machine_rep1);
    TRACED_FOREACH(MachineRepresentation, machine_rep2, kMachineReps) {
      MachineOperatorBuilder machine2(zone(), machine_rep2);
      TRACED_FOREACH(PureOperator, pop, kPureOperators) {
        const Operator* op1 = (machine1.*pop.constructor)();
        const Operator* op2 = (machine2.*pop.constructor)();
        EXPECT_EQ(op1, op2);
        EXPECT_EQ(pop.value_input_count, op1->ValueInputCount());
        EXPECT_EQ(pop.control_input_count, op1->ControlInputCount());
        EXPECT_EQ(pop.value_output_count, op1->ValueOutputCount());
      }
    }
  }
}


// Optional operators.

struct OptionalOperatorEntry {
  const OptionalOperator (MachineOperatorBuilder::*constructor)();
  MachineOperatorBuilder::Flag enabling_flag;
  char const* const constructor_name;
  int value_input_count;
  int control_input_count;
  int value_output_count;
};


std::ostream& operator<<(std::ostream& os, OptionalOperatorEntry const& pop) {
  return os << pop.constructor_name;
}

const OptionalOperatorEntry kOptionalOperators[] = {
#define OPTIONAL_ENTRY(Name, value_input_count, control_input_count,       \
                       value_output_count)                                 \
  {                                                                        \
    &MachineOperatorBuilder::Name, MachineOperatorBuilder::k##Name, #Name, \
        value_input_count, control_input_count, value_output_count         \
  }
    OPTIONAL_ENTRY(Float64RoundDown, 1, 0, 1),      // --
    OPTIONAL_ENTRY(Float64RoundTruncate, 1, 0, 1),  // --
    OPTIONAL_ENTRY(Float64RoundTiesAway, 1, 0, 1),  // --
#undef OPTIONAL_ENTRY
};


class MachineOptionalOperatorTest : public TestWithZone {
 protected:
  MachineRepresentation word_rep() {
    return MachineType::PointerRepresentation();
  }
};


TEST_F(MachineOptionalOperatorTest, OptionalOperators) {
  TRACED_FOREACH(OptionalOperatorEntry, pop, kOptionalOperators) {
    TRACED_FOREACH(MachineRepresentation, machine_rep1, kMachineReps) {
      MachineOperatorBuilder machine1(zone(), machine_rep1, pop.enabling_flag);
      TRACED_FOREACH(MachineRepresentation, machine_rep2, kMachineReps) {
        MachineOperatorBuilder machine2(zone(), machine_rep2,
                                        pop.enabling_flag);
        const Operator* op1 = (machine1.*pop.constructor)().op();
        const Operator* op2 = (machine2.*pop.constructor)().op();
        EXPECT_EQ(op1, op2);
        EXPECT_EQ(pop.value_input_count, op1->ValueInputCount());
        EXPECT_EQ(pop.control_input_count, op1->ControlInputCount());
        EXPECT_EQ(pop.value_output_count, op1->ValueOutputCount());

        MachineOperatorBuilder machine3(zone(), word_rep());
        EXPECT_TRUE((machine1.*pop.constructor)().IsSupported());
        EXPECT_FALSE((machine3.*pop.constructor)().IsSupported());
      }
    }
  }
}


// -----------------------------------------------------------------------------
// Pseudo operators.

using MachineOperatorTest = TestWithZone;

TEST_F(MachineOperatorTest, PseudoOperatorsWhenWordSizeIs32Bit) {
  MachineOperatorBuilder machine(zone(), MachineRepresentation::kWord32);
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


TEST_F(MachineOperatorTest, PseudoOperatorsWhenWordSizeIs64Bit) {
  MachineOperatorBuilder machine(zone(), MachineRepresentation::kWord64);
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

}  // namespace machine_operator_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
