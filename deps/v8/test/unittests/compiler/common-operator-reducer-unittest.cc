// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/machine-type.h"
#include "src/compiler/operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class CommonOperatorReducerTest : public GraphTest {
 public:
  explicit CommonOperatorReducerTest(int num_parameters = 1)
      : GraphTest(num_parameters), machine_(zone()) {}
  ~CommonOperatorReducerTest() override {}

 protected:
  Reduction Reduce(Node* node, MachineOperatorBuilder::Flags flags =
                                   MachineOperatorBuilder::kNoFlags) {
    JSOperatorBuilder javascript(zone());
    MachineOperatorBuilder machine(zone(), kMachPtr, flags);
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, &machine);
    CommonOperatorReducer reducer(&jsgraph);
    return reducer.Reduce(node);
  }

  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
};


namespace {

const BranchHint kBranchHints[] = {BranchHint::kNone, BranchHint::kFalse,
                                   BranchHint::kTrue};


const MachineType kMachineTypes[] = {
    kMachFloat32, kMachFloat64,   kMachInt8,   kMachUint8,  kMachInt16,
    kMachUint16,  kMachInt32,     kMachUint32, kMachInt64,  kMachUint64,
    kMachPtr,     kMachAnyTagged, kRepBit,     kRepWord8,   kRepWord16,
    kRepWord32,   kRepWord64,     kRepFloat32, kRepFloat64, kRepTagged};


const Operator kOp0(0, Operator::kNoProperties, "Op0", 0, 0, 0, 1, 1, 0);

}  // namespace


// -----------------------------------------------------------------------------
// EffectPhi


TEST_F(CommonOperatorReducerTest, RedundantEffectPhi) {
  const int kMaxInputs = 64;
  Node* inputs[kMaxInputs];
  Node* const input = graph()->NewNode(&kOp0);
  TRACED_FORRANGE(int, input_count, 2, kMaxInputs - 1) {
    int const value_input_count = input_count - 1;
    for (int i = 0; i < value_input_count; ++i) {
      inputs[i] = input;
    }
    inputs[value_input_count] = graph()->start();
    Reduction r = Reduce(graph()->NewNode(
        common()->EffectPhi(value_input_count), input_count, inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(input, r.replacement());
  }
}


// -----------------------------------------------------------------------------
// Phi


TEST_F(CommonOperatorReducerTest, RedundantPhi) {
  const int kMaxInputs = 64;
  Node* inputs[kMaxInputs];
  Node* const input = graph()->NewNode(&kOp0);
  TRACED_FORRANGE(int, input_count, 2, kMaxInputs - 1) {
    int const value_input_count = input_count - 1;
    TRACED_FOREACH(MachineType, type, kMachineTypes) {
      for (int i = 0; i < value_input_count; ++i) {
        inputs[i] = graph()->start();
      }
      Node* merge = graph()->NewNode(common()->Merge(value_input_count),
                                     value_input_count, inputs);
      for (int i = 0; i < value_input_count; ++i) {
        inputs[i] = input;
      }
      inputs[value_input_count] = merge;
      Reduction r = Reduce(graph()->NewNode(
          common()->Phi(type, value_input_count), input_count, inputs));
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(input, r.replacement());
    }
  }
}


TEST_F(CommonOperatorReducerTest, PhiToFloat32Abs) {
  Node* p0 = Parameter(0);
  Node* c0 = Float32Constant(0.0);
  Node* check = graph()->NewNode(machine()->Float32LessThan(), c0, p0);
  Node* branch = graph()->NewNode(common()->Branch(), check, graph()->start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = p0;
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(machine()->Float32Sub(), c0, p0);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(kMachFloat32, 2), vtrue, vfalse, merge);
  Reduction r = Reduce(phi);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat32Abs(p0));
}


TEST_F(CommonOperatorReducerTest, PhiToFloat64Abs) {
  Node* p0 = Parameter(0);
  Node* c0 = Float64Constant(0.0);
  Node* check = graph()->NewNode(machine()->Float64LessThan(), c0, p0);
  Node* branch = graph()->NewNode(common()->Branch(), check, graph()->start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* vtrue = p0;
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(machine()->Float64Sub(), c0, p0);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(kMachFloat64, 2), vtrue, vfalse, merge);
  Reduction r = Reduce(phi);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64Abs(p0));
}


TEST_F(CommonOperatorReducerTest, PhiToFloat32Max) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float32LessThan(), p0, p1);
  Node* branch = graph()->NewNode(common()->Branch(), check, graph()->start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(kMachFloat32, 2), p1, p0, merge);
  Reduction r = Reduce(phi, MachineOperatorBuilder::kFloat32Max);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat32Max(p1, p0));
}


TEST_F(CommonOperatorReducerTest, PhiToFloat64Max) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float64LessThan(), p0, p1);
  Node* branch = graph()->NewNode(common()->Branch(), check, graph()->start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(kMachFloat64, 2), p1, p0, merge);
  Reduction r = Reduce(phi, MachineOperatorBuilder::kFloat64Max);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64Max(p1, p0));
}


TEST_F(CommonOperatorReducerTest, PhiToFloat32Min) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float32LessThan(), p0, p1);
  Node* branch = graph()->NewNode(common()->Branch(), check, graph()->start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(kMachFloat32, 2), p0, p1, merge);
  Reduction r = Reduce(phi, MachineOperatorBuilder::kFloat32Min);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat32Min(p0, p1));
}


TEST_F(CommonOperatorReducerTest, PhiToFloat64Min) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float64LessThan(), p0, p1);
  Node* branch = graph()->NewNode(common()->Branch(), check, graph()->start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(kMachFloat64, 2), p0, p1, merge);
  Reduction r = Reduce(phi, MachineOperatorBuilder::kFloat64Min);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64Min(p0, p1));
}


// -----------------------------------------------------------------------------
// Select


TEST_F(CommonOperatorReducerTest, RedundantSelect) {
  Node* const input = graph()->NewNode(&kOp0);
  TRACED_FOREACH(BranchHint, hint, kBranchHints) {
    TRACED_FOREACH(MachineType, type, kMachineTypes) {
      Reduction r = Reduce(
          graph()->NewNode(common()->Select(type, hint), input, input, input));
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(input, r.replacement());
    }
  }
}


TEST_F(CommonOperatorReducerTest, SelectWithFalseConstant) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* select = graph()->NewNode(common()->Select(kMachAnyTagged),
                                  FalseConstant(), p0, p1);
  Reduction r = Reduce(select);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(p1, r.replacement());
}


TEST_F(CommonOperatorReducerTest, SelectWithTrueConstant) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* select = graph()->NewNode(common()->Select(kMachAnyTagged),
                                  TrueConstant(), p0, p1);
  Reduction r = Reduce(select);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(p0, r.replacement());
}


TEST_F(CommonOperatorReducerTest, SelectToFloat32Abs) {
  Node* p0 = Parameter(0);
  Node* c0 = Float32Constant(0.0);
  Node* check = graph()->NewNode(machine()->Float32LessThan(), c0, p0);
  Node* select =
      graph()->NewNode(common()->Select(kMachFloat32), check, p0,
                       graph()->NewNode(machine()->Float32Sub(), c0, p0));
  Reduction r = Reduce(select);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat32Abs(p0));
}


TEST_F(CommonOperatorReducerTest, SelectToFloat64Abs) {
  Node* p0 = Parameter(0);
  Node* c0 = Float64Constant(0.0);
  Node* check = graph()->NewNode(machine()->Float64LessThan(), c0, p0);
  Node* select =
      graph()->NewNode(common()->Select(kMachFloat64), check, p0,
                       graph()->NewNode(machine()->Float64Sub(), c0, p0));
  Reduction r = Reduce(select);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64Abs(p0));
}


TEST_F(CommonOperatorReducerTest, SelectToFloat32Max) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float32LessThan(), p0, p1);
  Node* select =
      graph()->NewNode(common()->Select(kMachFloat32), check, p1, p0);
  Reduction r = Reduce(select, MachineOperatorBuilder::kFloat32Max);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat32Max(p1, p0));
}


TEST_F(CommonOperatorReducerTest, SelectToFloat64Max) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float64LessThan(), p0, p1);
  Node* select =
      graph()->NewNode(common()->Select(kMachFloat64), check, p1, p0);
  Reduction r = Reduce(select, MachineOperatorBuilder::kFloat64Max);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64Max(p1, p0));
}


TEST_F(CommonOperatorReducerTest, SelectToFloat32Min) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float32LessThan(), p0, p1);
  Node* select =
      graph()->NewNode(common()->Select(kMachFloat32), check, p0, p1);
  Reduction r = Reduce(select, MachineOperatorBuilder::kFloat32Min);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat32Min(p0, p1));
}


TEST_F(CommonOperatorReducerTest, SelectToFloat64Min) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* check = graph()->NewNode(machine()->Float64LessThan(), p0, p1);
  Node* select =
      graph()->NewNode(common()->Select(kMachFloat64), check, p0, p1);
  Reduction r = Reduce(select, MachineOperatorBuilder::kFloat64Min);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFloat64Min(p0, p1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
