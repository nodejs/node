// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/machine-type.h"
#include "test/unittests/compiler/graph-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class CommonOperatorReducerTest : public GraphTest {
 public:
  explicit CommonOperatorReducerTest(int num_parameters = 1)
      : GraphTest(num_parameters) {}
  ~CommonOperatorReducerTest() OVERRIDE {}

 protected:
  Reduction Reduce(Node* node) {
    CommonOperatorReducer reducer;
    return reducer.Reduce(node);
  }
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
        inputs[i] = input;
      }
      inputs[value_input_count] = graph()->start();
      Reduction r = Reduce(graph()->NewNode(
          common()->Phi(type, value_input_count), input_count, inputs));
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(input, r.replacement());
    }
  }
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
