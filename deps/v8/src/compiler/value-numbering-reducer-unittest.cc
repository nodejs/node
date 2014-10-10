// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/compiler/graph.h"
#include "src/compiler/value-numbering-reducer.h"
#include "src/test/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

const SimpleOperator kOp0(0, Operator::kNoProperties, 0, 1, "op0");
const SimpleOperator kOp1(1, Operator::kNoProperties, 1, 1, "op1");

}  // namespace


class ValueNumberingReducerTest : public TestWithZone {
 public:
  ValueNumberingReducerTest() : graph_(zone()), reducer_(zone()) {}

 protected:
  Reduction Reduce(Node* node) { return reducer_.Reduce(node); }

  Graph* graph() { return &graph_; }

 private:
  Graph graph_;
  ValueNumberingReducer reducer_;
};


TEST_F(ValueNumberingReducerTest, AllInputsAreChecked) {
  Node* na = graph()->NewNode(&kOp0);
  Node* nb = graph()->NewNode(&kOp0);
  Node* n1 = graph()->NewNode(&kOp0, na);
  Node* n2 = graph()->NewNode(&kOp0, nb);
  EXPECT_FALSE(Reduce(n1).Changed());
  EXPECT_FALSE(Reduce(n2).Changed());
}


TEST_F(ValueNumberingReducerTest, DeadNodesAreNeverReturned) {
  Node* n0 = graph()->NewNode(&kOp0);
  Node* n1 = graph()->NewNode(&kOp1, n0);
  EXPECT_FALSE(Reduce(n1).Changed());
  n1->Kill();
  EXPECT_FALSE(Reduce(graph()->NewNode(&kOp1, n0)).Changed());
}


TEST_F(ValueNumberingReducerTest, OperatorEqualityNotIdentity) {
  static const size_t kMaxInputCount = 16;
  Node* inputs[kMaxInputCount];
  for (size_t i = 0; i < arraysize(inputs); ++i) {
    Operator::Opcode opcode = static_cast<Operator::Opcode>(
        std::numeric_limits<Operator::Opcode>::max() - i);
    inputs[i] = graph()->NewNode(new (zone()) SimpleOperator(
        opcode, Operator::kNoProperties, 0, 1, "Operator"));
  }
  TRACED_FORRANGE(size_t, input_count, 0, arraysize(inputs)) {
    const SimpleOperator op1(static_cast<Operator::Opcode>(input_count),
                             Operator::kNoProperties,
                             static_cast<int>(input_count), 1, "op");
    Node* n1 = graph()->NewNode(&op1, static_cast<int>(input_count), inputs);
    Reduction r1 = Reduce(n1);
    EXPECT_FALSE(r1.Changed());

    const SimpleOperator op2(static_cast<Operator::Opcode>(input_count),
                             Operator::kNoProperties,
                             static_cast<int>(input_count), 1, "op");
    Node* n2 = graph()->NewNode(&op2, static_cast<int>(input_count), inputs);
    Reduction r2 = Reduce(n2);
    EXPECT_TRUE(r2.Changed());
    EXPECT_EQ(n1, r2.replacement());
  }
}


TEST_F(ValueNumberingReducerTest, SubsequentReductionsYieldTheSameNode) {
  static const size_t kMaxInputCount = 16;
  Node* inputs[kMaxInputCount];
  for (size_t i = 0; i < arraysize(inputs); ++i) {
    Operator::Opcode opcode = static_cast<Operator::Opcode>(
        std::numeric_limits<Operator::Opcode>::max() - i);
    inputs[i] = graph()->NewNode(new (zone()) SimpleOperator(
        opcode, Operator::kNoProperties, 0, 1, "Operator"));
  }
  TRACED_FORRANGE(size_t, input_count, 0, arraysize(inputs)) {
    const SimpleOperator op1(1, Operator::kNoProperties,
                             static_cast<int>(input_count), 1, "op1");
    Node* n = graph()->NewNode(&op1, static_cast<int>(input_count), inputs);
    Reduction r = Reduce(n);
    EXPECT_FALSE(r.Changed());

    r = Reduce(graph()->NewNode(&op1, static_cast<int>(input_count), inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(n, r.replacement());

    r = Reduce(graph()->NewNode(&op1, static_cast<int>(input_count), inputs));
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(n, r.replacement());
  }
}


TEST_F(ValueNumberingReducerTest, WontReplaceNodeWithItself) {
  Node* n = graph()->NewNode(&kOp0);
  EXPECT_FALSE(Reduce(n).Changed());
  EXPECT_FALSE(Reduce(n).Changed());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
