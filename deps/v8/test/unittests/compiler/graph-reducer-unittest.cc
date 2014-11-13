// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/operator.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DefaultValue;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {

struct TestOperator : public Operator {
  TestOperator(Operator::Opcode opcode, Operator::Properties properties,
               size_t value_in, size_t value_out)
      : Operator(opcode, properties, "TestOp", value_in, 0, 0, value_out, 0,
                 0) {}
};


namespace {

TestOperator OP0(0, Operator::kNoWrite, 0, 1);
TestOperator OP1(1, Operator::kNoProperties, 1, 1);


struct MockReducer : public Reducer {
  MOCK_METHOD1(Reduce, Reduction(Node*));
};

}  // namespace


class GraphReducerTest : public TestWithZone {
 public:
  GraphReducerTest() : graph_(zone()) {}

  static void SetUpTestCase() {
    TestWithZone::SetUpTestCase();
    DefaultValue<Reduction>::Set(Reducer::NoChange());
  }

  static void TearDownTestCase() {
    DefaultValue<Reduction>::Clear();
    TestWithZone::TearDownTestCase();
  }

 protected:
  void ReduceNode(Node* node, Reducer* r) {
    GraphReducer reducer(graph());
    reducer.AddReducer(r);
    reducer.ReduceNode(node);
  }

  void ReduceNode(Node* node, Reducer* r1, Reducer* r2) {
    GraphReducer reducer(graph());
    reducer.AddReducer(r1);
    reducer.AddReducer(r2);
    reducer.ReduceNode(node);
  }

  void ReduceNode(Node* node, Reducer* r1, Reducer* r2, Reducer* r3) {
    GraphReducer reducer(graph());
    reducer.AddReducer(r1);
    reducer.AddReducer(r2);
    reducer.AddReducer(r3);
    reducer.ReduceNode(node);
  }

  Graph* graph() { return &graph_; }

 private:
  Graph graph_;
};


TEST_F(GraphReducerTest, NodeIsDeadAfterReplace) {
  StrictMock<MockReducer> r;
  Node* node0 = graph()->NewNode(&OP0);
  Node* node1 = graph()->NewNode(&OP1, node0);
  Node* node2 = graph()->NewNode(&OP1, node0);
  EXPECT_CALL(r, Reduce(node1)).WillOnce(Return(Reducer::Replace(node2)));
  ReduceNode(node1, &r);
  EXPECT_FALSE(node0->IsDead());
  EXPECT_TRUE(node1->IsDead());
  EXPECT_FALSE(node2->IsDead());
}


TEST_F(GraphReducerTest, ReduceOnceForEveryReducer) {
  StrictMock<MockReducer> r1, r2;
  Node* node0 = graph()->NewNode(&OP0);
  EXPECT_CALL(r1, Reduce(node0));
  EXPECT_CALL(r2, Reduce(node0));
  ReduceNode(node0, &r1, &r2);
}


TEST_F(GraphReducerTest, ReduceAgainAfterChanged) {
  Sequence s1, s2, s3;
  StrictMock<MockReducer> r1, r2, r3;
  Node* node0 = graph()->NewNode(&OP0);
  EXPECT_CALL(r1, Reduce(node0));
  EXPECT_CALL(r2, Reduce(node0));
  EXPECT_CALL(r3, Reduce(node0)).InSequence(s1, s2, s3).WillOnce(
      Return(Reducer::Changed(node0)));
  EXPECT_CALL(r1, Reduce(node0)).InSequence(s1);
  EXPECT_CALL(r2, Reduce(node0)).InSequence(s2);
  EXPECT_CALL(r3, Reduce(node0)).InSequence(s3);
  ReduceNode(node0, &r1, &r2, &r3);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
