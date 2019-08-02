// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

using testing::Contains;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::UnorderedElementsAre;

namespace v8 {
namespace internal {
namespace compiler {
namespace node_unittest {

using NodeTest = TestWithZone;

const IrOpcode::Value kOpcode0 = static_cast<IrOpcode::Value>(0);
const IrOpcode::Value kOpcode1 = static_cast<IrOpcode::Value>(1);
const IrOpcode::Value kOpcode2 = static_cast<IrOpcode::Value>(2);

const Operator kOp0(kOpcode0, Operator::kNoProperties, "Op0", 0, 0, 0, 1, 0, 0);
const Operator kOp1(kOpcode1, Operator::kNoProperties, "Op1", 1, 0, 0, 1, 0, 0);
const Operator kOp2(kOpcode2, Operator::kNoProperties, "Op2", 2, 0, 0, 1, 0, 0);


TEST_F(NodeTest, New) {
  Node* const node = Node::New(zone(), 1, &kOp0, 0, nullptr, false);
  EXPECT_EQ(1U, node->id());
  EXPECT_EQ(0, node->UseCount());
  EXPECT_TRUE(node->uses().empty());
  EXPECT_EQ(0, node->InputCount());
  EXPECT_TRUE(node->inputs().empty());
  EXPECT_EQ(&kOp0, node->op());
  EXPECT_EQ(kOpcode0, node->opcode());
}


TEST_F(NodeTest, NewWithInputs) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  EXPECT_EQ(0, n0->UseCount());
  EXPECT_EQ(0, n0->InputCount());
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  EXPECT_EQ(1, n0->UseCount());
  EXPECT_THAT(n0->uses(), UnorderedElementsAre(n1));
  EXPECT_EQ(0, n1->UseCount());
  EXPECT_EQ(1, n1->InputCount());
  EXPECT_EQ(n0, n1->InputAt(0));
  Node* n0_n1[] = {n0, n1};
  Node* n2 = Node::New(zone(), 2, &kOp2, 2, n0_n1, false);
  EXPECT_EQ(2, n0->UseCount());
  EXPECT_THAT(n0->uses(), UnorderedElementsAre(n1, n2));
  EXPECT_THAT(n1->uses(), UnorderedElementsAre(n2));
  EXPECT_EQ(2, n2->InputCount());
  EXPECT_EQ(n0, n2->InputAt(0));
  EXPECT_EQ(n1, n2->InputAt(1));
}


TEST_F(NodeTest, InputIteratorEmpty) {
  Node* node = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  EXPECT_EQ(node->inputs().begin(), node->inputs().end());
}


TEST_F(NodeTest, InputIteratorOne) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  EXPECT_THAT(n1->inputs(), ElementsAre(n0));
}


TEST_F(NodeTest, InputIteratorTwo) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  Node* n0_n1[] = {n0, n1};
  Node* n2 = Node::New(zone(), 2, &kOp2, 2, n0_n1, false);
  EXPECT_THAT(n2->inputs(), ElementsAre(n0, n1));
}


TEST_F(NodeTest, UseIteratorEmpty) {
  Node* node = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  EXPECT_EQ(node->uses().begin(), node->uses().end());
}


TEST_F(NodeTest, UseIteratorOne) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  EXPECT_THAT(n0->uses(), ElementsAre(n1));
}


TEST_F(NodeTest, UseIteratorTwo) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  Node* n0_n1[] = {n0, n1};
  Node* n2 = Node::New(zone(), 2, &kOp2, 2, n0_n1, false);
  EXPECT_THAT(n0->uses(), UnorderedElementsAre(n1, n2));
}


TEST_F(NodeTest, OwnedBy) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  EXPECT_FALSE(n0->OwnedBy(n0));
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  EXPECT_FALSE(n0->OwnedBy(n0));
  EXPECT_FALSE(n1->OwnedBy(n1));
  EXPECT_TRUE(n0->OwnedBy(n1));
  Node* n0_n1[] = {n0, n1};
  Node* n2 = Node::New(zone(), 2, &kOp2, 2, n0_n1, false);
  EXPECT_FALSE(n0->OwnedBy(n0));
  EXPECT_FALSE(n1->OwnedBy(n1));
  EXPECT_FALSE(n2->OwnedBy(n2));
  EXPECT_FALSE(n0->OwnedBy(n1));
  EXPECT_FALSE(n0->OwnedBy(n2));
  EXPECT_TRUE(n1->OwnedBy(n2));
  EXPECT_TRUE(n0->OwnedBy(n1, n2));
  n2->ReplaceInput(0, n2);
  EXPECT_TRUE(n0->OwnedBy(n1));
  EXPECT_TRUE(n1->OwnedBy(n2));
  n2->ReplaceInput(1, n0);
  EXPECT_FALSE(n0->OwnedBy(n1));
  EXPECT_FALSE(n1->OwnedBy(n2));
}


TEST_F(NodeTest, ReplaceUsesNone) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  Node* n0_n1[] = {n0, n1};
  Node* n2 = Node::New(zone(), 2, &kOp2, 2, n0_n1, false);
  Node* node = Node::New(zone(), 42, &kOp0, 0, nullptr, false);
  EXPECT_TRUE(node->uses().empty());
  node->ReplaceUses(n0);
  EXPECT_TRUE(node->uses().empty());
  node->ReplaceUses(n1);
  EXPECT_TRUE(node->uses().empty());
  node->ReplaceUses(n2);
  EXPECT_TRUE(node->uses().empty());
}


TEST_F(NodeTest, AppendInput) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);
  Node* node = Node::New(zone(), 12345, &kOp0, 0, nullptr, true);
  EXPECT_TRUE(node->inputs().empty());
  node->AppendInput(zone(), n0);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n0));
  node->AppendInput(zone(), n1);
  EXPECT_THAT(node->inputs(), ElementsAre(n0, n1));
  node->AppendInput(zone(), n0);
  EXPECT_THAT(node->inputs(), ElementsAre(n0, n1, n0));
  node->AppendInput(zone(), n0);
  EXPECT_THAT(node->inputs(), ElementsAre(n0, n1, n0, n0));
  node->AppendInput(zone(), n1);
  EXPECT_THAT(node->inputs(), ElementsAre(n0, n1, n0, n0, n1));
}


TEST_F(NodeTest, TrimThenAppend) {
  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp0, 0, nullptr, false);
  Node* n2 = Node::New(zone(), 2, &kOp0, 0, nullptr, false);
  Node* n3 = Node::New(zone(), 3, &kOp0, 0, nullptr, false);
  Node* n4 = Node::New(zone(), 4, &kOp0, 0, nullptr, false);
  Node* n5 = Node::New(zone(), 5, &kOp0, 0, nullptr, false);
  Node* n6 = Node::New(zone(), 6, &kOp0, 0, nullptr, false);
  Node* n7 = Node::New(zone(), 7, &kOp0, 0, nullptr, false);
  Node* n8 = Node::New(zone(), 8, &kOp0, 0, nullptr, false);
  Node* n9 = Node::New(zone(), 9, &kOp0, 0, nullptr, false);
  Node* node = Node::New(zone(), 12345, &kOp0, 0, nullptr, true);

  EXPECT_TRUE(node->inputs().empty());

  node->AppendInput(zone(), n0);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n0));

  node->TrimInputCount(0);
  EXPECT_TRUE(node->inputs().empty());

  node->AppendInput(zone(), n1);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1));

  node->AppendInput(zone(), n2);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n2));

  node->TrimInputCount(1);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1));

  node->AppendInput(zone(), n3);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3));

  node->AppendInput(zone(), n4);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4));

  node->AppendInput(zone(), n5);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4, n5));

  node->AppendInput(zone(), n6);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4, n5, n6));

  node->AppendInput(zone(), n7);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4, n5, n6, n7));

  node->TrimInputCount(4);
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4, n5));

  node->AppendInput(zone(), n8);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4, n5, n8));

  node->AppendInput(zone(), n9);
  EXPECT_FALSE(node->inputs().empty());
  EXPECT_THAT(node->inputs(), ElementsAre(n1, n3, n4, n5, n8, n9));
}


TEST_F(NodeTest, BigNodes) {
  static const int kMaxSize = 512;
  Node* inputs[kMaxSize];

  Node* n0 = Node::New(zone(), 0, &kOp0, 0, nullptr, false);
  Node* n1 = Node::New(zone(), 1, &kOp1, 1, &n0, false);

  for (int i = 0; i < kMaxSize; i++) {
    inputs[i] = i & 1 ? n0 : n1;
  }

  for (int size = 13; size <= kMaxSize; size += 9) {
    Node* node = Node::New(zone(), 12345, &kOp0, size, inputs, false);
    EXPECT_EQ(size, node->InputCount());

    for (int i = 0; i < size; i++) {
      EXPECT_EQ(inputs[i], node->InputAt(i));
    }

    EXPECT_THAT(n0->uses(), Contains(node));
    EXPECT_THAT(n1->uses(), Contains(node));
    EXPECT_THAT(node->inputs(), ElementsAreArray(inputs, size));
  }
}

}  // namespace node_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
