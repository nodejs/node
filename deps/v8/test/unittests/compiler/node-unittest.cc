// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node.h"

#include <functional>
#include <set>

#include "src/compiler/operator.h"
#include "src/compiler/turbofan-graph.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Contains;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::UnorderedElementsAre;

namespace v8 {
namespace internal {
namespace compiler {
namespace node_unittest {

class NodeTest : public TestWithZone {
 public:
  NodeTest() = default;
};

const IrOpcode::Value kOpcode0 = static_cast<IrOpcode::Value>(0);
const IrOpcode::Value kOpcode1 = static_cast<IrOpcode::Value>(1);
const IrOpcode::Value kOpcode2 = static_cast<IrOpcode::Value>(2);
const IrOpcode::Value kOpcode3 = static_cast<IrOpcode::Value>(3);

const Operator kOp0(kOpcode0, Operator::kNoProperties, "Op0", 0, 0, 0, 1, 0, 0);
const Operator kOp1(kOpcode1, Operator::kNoProperties, "Op1", 1, 0, 0, 1, 0, 0);
const Operator kOp2(kOpcode2, Operator::kNoProperties, "Op2", 2, 0, 0, 1, 0, 0);
const Operator kOp3(kOpcode3, Operator::kNoProperties, "Op3", 3, 0, 0, 1, 0, 0);

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

#define NONE reinterpret_cast<Node*>(1)

#define CHECK_USES(node, ...)                                          \
  do {                                                                 \
    Node* __array[] = {__VA_ARGS__};                                   \
    int __size =                                                       \
        __array[0] != NONE ? static_cast<int>(arraysize(__array)) : 0; \
    CheckUseChain(node, __array, __size);                              \
  } while (false)

namespace {

using NodeMSet = std::multiset<Node*, std::less<Node*>>;

void CheckUseChain(Node* node, Node** uses, int use_count) {
  // Check ownership.
  if (use_count == 1) CHECK(node->OwnedBy(uses[0]));
  if (use_count > 1) {
    Node* first_use = uses[0];
    bool different_uses = false;
    for (int i = 0; i < use_count; i++) {
      if (uses[i] != first_use) {
        different_uses = true;
        break;
      }
    }
    if (different_uses) {
      // If there are different uses, check that node is not owned by any use.
      for (int i = 0; i < use_count; i++) {
        CHECK(!node->OwnedBy(uses[i]));
      }
    } else {
      // If all uses are the same, check that node is owned by that use.
      CHECK(node->OwnedBy(first_use));
    }
  }

  // Check the self-reported use count.
  CHECK_EQ(use_count, node->UseCount());

  // Build the expectation set.
  NodeMSet expect_set;
  for (int i = 0; i < use_count; i++) {
    expect_set.insert(uses[i]);
  }

  {
    // Check that iterating over the uses gives the right counts.
    NodeMSet use_set;
    for (auto use : node->uses()) {
      use_set.insert(use);
    }
    CHECK(expect_set == use_set);
  }

  {
    // Check that iterating over the use edges gives the right counts,
    // input indices, from(), and to() pointers.
    NodeMSet use_set;
    for (auto edge : node->use_edges()) {
      CHECK_EQ(node, edge.to());
      CHECK_EQ(node, edge.from()->InputAt(edge.index()));
      use_set.insert(edge.from());
    }
    CHECK(expect_set == use_set);
  }

  {
    // Check the use nodes actually have the node as inputs.
    for (Node* use : node->uses()) {
      size_t count = 0;
      for (Node* input : use->inputs()) {
        if (input == node) count++;
      }
      CHECK_EQ(count, expect_set.count(use));
    }
  }
}

void CheckInputs(Node* node, Node** inputs, int input_count) {
  CHECK_EQ(input_count, node->InputCount());
  // Check InputAt().
  for (int i = 0; i < static_cast<int>(input_count); i++) {
    CHECK_EQ(inputs[i], node->InputAt(i));
  }

  // Check input iterator.
  int index = 0;
  for (Node* input : node->inputs()) {
    CHECK_EQ(inputs[index], input);
    index++;
  }

  // Check use lists of inputs.
  for (int i = 0; i < static_cast<int>(input_count); i++) {
    Node* input = inputs[i];
    if (!input) continue;  // skip null inputs
    bool found = false;
    // Check regular use list.
    for (Node* use : input->uses()) {
      if (use == node) {
        found = true;
        break;
      }
    }
    CHECK(found);
    int count = 0;
    // Check use edge list.
    for (auto edge : input->use_edges()) {
      if (edge.from() == node && edge.to() == input && edge.index() == i) {
        count++;
      }
    }
    CHECK_EQ(1, count);
  }
}

}  // namespace

#define CHECK_INPUTS(node, ...)                                        \
  do {                                                                 \
    Node* __array[] = {__VA_ARGS__};                                   \
    int __size =                                                       \
        __array[0] != NONE ? static_cast<int>(arraysize(__array)) : 0; \
    CheckInputs(node, __array, __size);                                \
  } while (false)

TEST_F(NodeTest, NodeUseIteratorReplaceUses) {
  TFGraph graph(zone());
  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp1, n0);
  Node* n3 = graph.NewNode(&kOp0);

  CHECK_USES(n0, n1, n2);

  CHECK_INPUTS(n1, n0);
  CHECK_INPUTS(n2, n0);

  n0->ReplaceUses(n3);

  CHECK_USES(n0, NONE);
  CHECK_USES(n1, NONE);
  CHECK_USES(n2, NONE);
  CHECK_USES(n3, n1, n2);

  CHECK_INPUTS(n1, n3);
  CHECK_INPUTS(n2, n3);
}

TEST_F(NodeTest, NodeUseIteratorReplaceUsesSelf) {
  TFGraph graph(zone());
  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);

  CHECK_USES(n0, n1);
  CHECK_USES(n1, NONE);

  n1->ReplaceInput(0, n1);  // Create self-reference.

  CHECK_USES(n0, NONE);
  CHECK_USES(n1, n1);

  Node* n2 = graph.NewNode(&kOp0);

  n1->ReplaceUses(n2);

  CHECK_USES(n0, NONE);
  CHECK_USES(n1, NONE);
  CHECK_USES(n2, n1);
}

TEST_F(NodeTest, ReplaceInput) {
  TFGraph graph(zone());
  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp0);
  Node* n2 = graph.NewNode(&kOp0);
  Node* n3 = graph.NewNode(&kOp3, n0, n1, n2);
  Node* n4 = graph.NewNode(&kOp0);

  CHECK_USES(n0, n3);
  CHECK_USES(n1, n3);
  CHECK_USES(n2, n3);
  CHECK_USES(n3, NONE);
  CHECK_USES(n4, NONE);

  CHECK_INPUTS(n3, n0, n1, n2);

  n3->ReplaceInput(1, n4);

  CHECK_USES(n1, NONE);
  CHECK_USES(n4, n3);

  CHECK_INPUTS(n3, n0, n4, n2);
}

TEST_F(NodeTest, OwnedByGraph) {
  TFGraph graph(zone());

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);

    CHECK(!n0->OwnedBy(n1));
    CHECK(!n1->OwnedBy(n0));

    Node* n2 = graph.NewNode(&kOp1, n0);
    CHECK(n0->OwnedBy(n2));
    CHECK(!n2->OwnedBy(n0));

    Node* n3 = graph.NewNode(&kOp1, n0);
    CHECK(!n0->OwnedBy(n2));
    CHECK(!n0->OwnedBy(n3));
    CHECK(!n2->OwnedBy(n0));
    CHECK(!n3->OwnedBy(n0));
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp1, n0);
    CHECK(n0->OwnedBy(n1));
    CHECK(!n1->OwnedBy(n0));
    Node* n2 = graph.NewNode(&kOp1, n0);
    CHECK(!n0->OwnedBy(n1));
    CHECK(!n0->OwnedBy(n2));
    CHECK(!n1->OwnedBy(n0));
    CHECK(!n1->OwnedBy(n2));
    CHECK(!n2->OwnedBy(n0));
    CHECK(!n2->OwnedBy(n1));

    Node* n3 = graph.NewNode(&kOp0);
    n2->ReplaceInput(0, n3);

    CHECK(n0->OwnedBy(n1));
    CHECK(!n1->OwnedBy(n0));
    CHECK(!n1->OwnedBy(n0));
    CHECK(!n1->OwnedBy(n2));
    CHECK(!n2->OwnedBy(n0));
    CHECK(!n2->OwnedBy(n1));
    CHECK(n3->OwnedBy(n2));
    CHECK(!n2->OwnedBy(n3));
  }
}

TEST_F(NodeTest, Uses) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);

  CHECK_USES(n0, n1);
  CHECK_USES(n1, NONE);

  Node* n2 = graph.NewNode(&kOp1, n0);

  CHECK_USES(n0, n1, n2);
  CHECK_USES(n2, NONE);

  Node* n3 = graph.NewNode(&kOp1, n0);

  CHECK_USES(n0, n1, n2, n3);
  CHECK_USES(n3, NONE);
}

TEST_F(NodeTest, Inputs) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp1, n0);
  Node* n3 = graph.NewNode(&kOp3, n0, n1, n2);

  CHECK_INPUTS(n3, n0, n1, n2);

  Node* n4 = graph.NewNode(&kOp3, n0, n1, n2);
  n3->AppendInput(graph.zone(), n4);

  CHECK_INPUTS(n3, n0, n1, n2, n4);
  CHECK_USES(n4, n3);

  n3->AppendInput(graph.zone(), n4);

  CHECK_INPUTS(n3, n0, n1, n2, n4, n4);
  CHECK_USES(n4, n3, n3);

  Node* n5 = graph.NewNode(&kOp1, n4);

  CHECK_USES(n4, n3, n3, n5);
}

TEST_F(NodeTest, InsertInputs) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp1, n0);

  {
    Node* node = graph.NewNode(&kOp1, n0);
    node->InsertInputs(graph.zone(), 0, 1);
    node->ReplaceInput(0, n1);
    CHECK_INPUTS(node, n1, n0);
  }
  {
    Node* node = graph.NewNode(&kOp1, n0);
    node->InsertInputs(graph.zone(), 0, 2);
    node->ReplaceInput(0, node);
    node->ReplaceInput(1, n2);
    CHECK_INPUTS(node, node, n2, n0);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 0, 1);
    node->ReplaceInput(0, node);
    CHECK_INPUTS(node, node, n0, n1, n2);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 1, 1);
    node->ReplaceInput(1, node);
    CHECK_INPUTS(node, n0, node, n1, n2);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 2, 1);
    node->ReplaceInput(2, node);
    CHECK_INPUTS(node, n0, n1, node, n2);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 2, 1);
    node->ReplaceInput(2, node);
    CHECK_INPUTS(node, n0, n1, node, n2);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 0, 4);
    node->ReplaceInput(0, node);
    node->ReplaceInput(1, node);
    node->ReplaceInput(2, node);
    node->ReplaceInput(3, node);
    CHECK_INPUTS(node, node, node, node, node, n0, n1, n2);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 1, 4);
    node->ReplaceInput(1, node);
    node->ReplaceInput(2, node);
    node->ReplaceInput(3, node);
    node->ReplaceInput(4, node);
    CHECK_INPUTS(node, n0, node, node, node, node, n1, n2);
  }
  {
    Node* node = graph.NewNode(&kOp3, n0, n1, n2);
    node->InsertInputs(graph.zone(), 2, 4);
    node->ReplaceInput(2, node);
    node->ReplaceInput(3, node);
    node->ReplaceInput(4, node);
    node->ReplaceInput(5, node);
    CHECK_INPUTS(node, n0, n1, node, node, node, node, n2);
  }
}

TEST_F(NodeTest, RemoveInput) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp2, n0, n1);

  CHECK_INPUTS(n0, NONE);
  CHECK_INPUTS(n1, n0);
  CHECK_INPUTS(n2, n0, n1);
  CHECK_USES(n0, n1, n2);

  n1->RemoveInput(0);
  CHECK_INPUTS(n1, NONE);
  CHECK_USES(n0, n2);

  n2->RemoveInput(0);
  CHECK_INPUTS(n2, n1);
  CHECK_USES(n0, NONE);
  CHECK_USES(n1, n2);

  n2->RemoveInput(0);
  CHECK_INPUTS(n2, NONE);
  CHECK_USES(n0, NONE);
  CHECK_USES(n1, NONE);
  CHECK_USES(n2, NONE);
}

TEST_F(NodeTest, AppendInputsAndIterator) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp2, n0, n1);

  CHECK_INPUTS(n0, NONE);
  CHECK_INPUTS(n1, n0);
  CHECK_INPUTS(n2, n0, n1);
  CHECK_USES(n0, n1, n2);

  Node* n3 = graph.NewNode(&kOp0);

  n2->AppendInput(graph.zone(), n3);

  CHECK_INPUTS(n2, n0, n1, n3);
  CHECK_USES(n3, n2);
}

TEST_F(NodeTest, NullInputsSimple) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp2, n0, n1);

  CHECK_INPUTS(n0, NONE);
  CHECK_INPUTS(n1, n0);
  CHECK_INPUTS(n2, n0, n1);
  CHECK_USES(n0, n1, n2);

  n2->ReplaceInput(0, nullptr);

  CHECK_INPUTS(n2, nullptr, n1);

  CHECK_USES(n0, n1);

  n2->ReplaceInput(1, nullptr);

  CHECK_INPUTS(n2, nullptr, nullptr);

  CHECK_USES(n1, NONE);
}

TEST_F(NodeTest, NullInputsAppended) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp1, n0);
  Node* n3 = graph.NewNode(&kOp1, n0);
  n3->AppendInput(graph.zone(), n1);
  n3->AppendInput(graph.zone(), n2);

  CHECK_INPUTS(n3, n0, n1, n2);
  CHECK_USES(n0, n1, n2, n3);
  CHECK_USES(n1, n3);
  CHECK_USES(n2, n3);

  n3->ReplaceInput(1, nullptr);
  CHECK_USES(n1, NONE);

  CHECK_INPUTS(n3, n0, nullptr, n2);
}

TEST_F(NodeTest, ReplaceUsesFromAppendedInputs) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp1, n0);
  Node* n2 = graph.NewNode(&kOp1, n0);
  Node* n3 = graph.NewNode(&kOp0);

  CHECK_INPUTS(n2, n0);

  n2->AppendInput(graph.zone(), n1);
  CHECK_INPUTS(n2, n0, n1);
  CHECK_USES(n1, n2);

  n2->AppendInput(graph.zone(), n0);
  CHECK_INPUTS(n2, n0, n1, n0);
  CHECK_USES(n1, n2);
  CHECK_USES(n0, n2, n1, n2);

  n0->ReplaceUses(n3);

  CHECK_USES(n0, NONE);
  CHECK_INPUTS(n2, n3, n1, n3);
  CHECK_USES(n3, n2, n1, n2);
}

TEST_F(NodeTest, ReplaceInputMultipleUses) {
  TFGraph graph(zone());

  Node* n0 = graph.NewNode(&kOp0);
  Node* n1 = graph.NewNode(&kOp0);
  Node* n2 = graph.NewNode(&kOp1, n0);
  n2->ReplaceInput(0, n1);
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(1, n1->UseCount());

  Node* n3 = graph.NewNode(&kOp1, n0);
  n3->ReplaceInput(0, n1);
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(2, n1->UseCount());
}

TEST_F(NodeTest, TrimInputCountInline) {
  TFGraph graph(zone());

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp1, n0);
    n1->TrimInputCount(1);
    CHECK_INPUTS(n1, n0);
    CHECK_USES(n0, n1);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp1, n0);
    n1->TrimInputCount(0);
    CHECK_INPUTS(n1, NONE);
    CHECK_USES(n0, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp2, n0, n1);
    n2->TrimInputCount(2);
    CHECK_INPUTS(n2, n0, n1);
    CHECK_USES(n0, n2);
    CHECK_USES(n1, n2);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp2, n0, n1);
    n2->TrimInputCount(1);
    CHECK_INPUTS(n2, n0);
    CHECK_USES(n0, n2);
    CHECK_USES(n1, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp2, n0, n1);
    n2->TrimInputCount(0);
    CHECK_INPUTS(n2, NONE);
    CHECK_USES(n0, NONE);
    CHECK_USES(n1, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp2, n0, n0);
    n2->TrimInputCount(1);
    CHECK_INPUTS(n2, n0);
    CHECK_USES(n0, n2);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp2, n0, n0);
    n2->TrimInputCount(0);
    CHECK_INPUTS(n2, NONE);
    CHECK_USES(n0, NONE);
  }
}

TEST_F(NodeTest, TrimInputCountOutOfLine1) {
  TFGraph graph(zone());

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    n1->AppendInput(graph.zone(), n0);
    CHECK_INPUTS(n1, n0);
    CHECK_USES(n0, n1);

    n1->TrimInputCount(1);
    CHECK_INPUTS(n1, n0);
    CHECK_USES(n0, n1);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    n1->AppendInput(graph.zone(), n0);
    CHECK_EQ(1, n1->InputCount());
    n1->TrimInputCount(0);
    CHECK_EQ(0, n1->InputCount());
    CHECK_EQ(0, n0->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp0);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_INPUTS(n2, n0, n1);
    n2->TrimInputCount(2);
    CHECK_INPUTS(n2, n0, n1);
    CHECK_USES(n0, n2);
    CHECK_USES(n1, n2);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp0);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_INPUTS(n2, n0, n1);
    n2->TrimInputCount(1);
    CHECK_INPUTS(n2, n0);
    CHECK_USES(n0, n2);
    CHECK_USES(n1, NONE);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp0);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_INPUTS(n2, n0, n1);
    n2->TrimInputCount(0);
    CHECK_INPUTS(n2, NONE);
    CHECK_USES(n0, NONE);
    CHECK_USES(n1, NONE);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp0);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_INPUTS(n2, n0, n0);
    CHECK_USES(n0, n2, n2);
    n2->TrimInputCount(1);
    CHECK_INPUTS(n2, n0);
    CHECK_USES(n0, n2);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp0);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_INPUTS(n2, n0, n0);
    CHECK_USES(n0, n2, n2);
    n2->TrimInputCount(0);
    CHECK_INPUTS(n2, NONE);
    CHECK_USES(n0, NONE);
  }
}

TEST_F(NodeTest, TrimInputCountOutOfLine2) {
  TFGraph graph(zone());

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp1, n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_INPUTS(n2, n0, n1);
    n2->TrimInputCount(2);
    CHECK_INPUTS(n2, n0, n1);
    CHECK_USES(n0, n2);
    CHECK_USES(n1, n2);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp1, n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_INPUTS(n2, n0, n1);
    n2->TrimInputCount(1);
    CHECK_INPUTS(n2, n0);
    CHECK_USES(n0, n2);
    CHECK_USES(n1, NONE);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp1, n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_INPUTS(n2, n0, n1);
    n2->TrimInputCount(0);
    CHECK_INPUTS(n2, NONE);
    CHECK_USES(n0, NONE);
    CHECK_USES(n1, NONE);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp1, n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_INPUTS(n2, n0, n0);
    CHECK_USES(n0, n2, n2);
    n2->TrimInputCount(1);
    CHECK_INPUTS(n2, n0);
    CHECK_USES(n0, n2);
    CHECK_USES(n2, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n2 = graph.NewNode(&kOp1, n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(2, n0->UseCount());
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }
}

TEST_F(NodeTest, NullAllInputs) {
  TFGraph graph(zone());

  for (int i = 0; i < 2; i++) {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp1, n0);
    Node* n2;
    if (i == 0) {
      n2 = graph.NewNode(&kOp2, n0, n1);
      CHECK_INPUTS(n2, n0, n1);
    } else {
      n2 = graph.NewNode(&kOp1, n0);
      CHECK_INPUTS(n2, n0);
      n2->AppendInput(graph.zone(), n1);  // with out-of-line input.
      CHECK_INPUTS(n2, n0, n1);
    }

    n0->NullAllInputs();
    CHECK_INPUTS(n0, NONE);

    CHECK_USES(n0, n1, n2);
    n1->NullAllInputs();
    CHECK_INPUTS(n1, nullptr);
    CHECK_INPUTS(n2, n0, n1);
    CHECK_USES(n0, n2);

    n2->NullAllInputs();
    CHECK_INPUTS(n1, nullptr);
    CHECK_INPUTS(n2, nullptr, nullptr);
    CHECK_USES(n0, NONE);
  }

  {
    Node* n0 = graph.NewNode(&kOp0);
    Node* n1 = graph.NewNode(&kOp1, n0);
    n1->ReplaceInput(0, n1);  // self-reference.

    CHECK_INPUTS(n0, NONE);
    CHECK_INPUTS(n1, n1);
    CHECK_USES(n0, NONE);
    CHECK_USES(n1, n1);
    n1->NullAllInputs();

    CHECK_INPUTS(n0, NONE);
    CHECK_INPUTS(n1, nullptr);
    CHECK_USES(n0, NONE);
    CHECK_USES(n1, NONE);
  }
}

TEST_F(NodeTest, AppendAndTrim) {
  TFGraph graph(zone());

  Node* nodes[] = {graph.NewNode(&kOp0), graph.NewNode(&kOp0),
                   graph.NewNode(&kOp0), graph.NewNode(&kOp0),
                   graph.NewNode(&kOp0)};

  int max = static_cast<int>(arraysize(nodes));

  Node* last = graph.NewNode(&kOp0);

  for (int i = 0; i < max; i++) {
    last->AppendInput(graph.zone(), nodes[i]);
    CheckInputs(last, nodes, i + 1);

    for (int j = 0; j < max; j++) {
      if (j <= i) CHECK_USES(nodes[j], last);
      if (j > i) CHECK_USES(nodes[j], NONE);
    }

    CHECK_USES(last, NONE);
  }

  for (int i = max; i >= 0; i--) {
    last->TrimInputCount(i);
    CheckInputs(last, nodes, i);

    for (int j = 0; j < i; j++) {
      if (j < i) CHECK_USES(nodes[j], last);
      if (j >= i) CHECK_USES(nodes[j], NONE);
    }

    CHECK_USES(last, NONE);
  }
}

#undef NONE
#undef CHECK_USES
#undef CHECK_INPUTS

}  // namespace node_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
