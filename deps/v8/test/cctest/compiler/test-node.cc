// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "src/v8.h"

#include "graph-tester.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 1, 0, 0);

TEST(NodeAllocation) {
  GraphTester graph;
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator);
  CHECK(n2->id() != n1->id());
}


TEST(NodeWithOpcode) {
  GraphTester graph;
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator);
  CHECK(n1->op() == &dummy_operator);
  CHECK(n2->op() == &dummy_operator);
}


TEST(NodeInputs1) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  CHECK_EQ(1, n2->InputCount());
  CHECK(n0 == n2->InputAt(0));
}


TEST(NodeInputs2) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator, n0, n1);
  CHECK_EQ(2, n2->InputCount());
  CHECK(n0 == n2->InputAt(0));
  CHECK(n1 == n2->InputAt(1));
}


TEST(NodeInputs3) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator, n0, n1, n1);
  CHECK_EQ(3, n2->InputCount());
  CHECK(n0 == n2->InputAt(0));
  CHECK(n1 == n2->InputAt(1));
  CHECK(n1 == n2->InputAt(2));
}


TEST(NodeInputIteratorEmpty) {
  GraphTester graph;
  Node* n1 = graph.NewNode(&dummy_operator);
  Node::Inputs::iterator i(n1->inputs().begin());
  int input_count = 0;
  for (; i != n1->inputs().end(); ++i) {
    input_count++;
  }
  CHECK_EQ(0, input_count);
}


TEST(NodeInputIteratorOne) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node::Inputs::iterator i(n1->inputs().begin());
  CHECK_EQ(1, n1->InputCount());
  CHECK_EQ(n0, *i);
  ++i;
  CHECK(n1->inputs().end() == i);
}


TEST(NodeUseIteratorEmpty) {
  GraphTester graph;
  Node* n1 = graph.NewNode(&dummy_operator);
  Node::Uses::iterator i(n1->uses().begin());
  int use_count = 0;
  for (; i != n1->uses().end(); ++i) {
    Node::Edge edge(i.edge());
    USE(edge);
    use_count++;
  }
  CHECK_EQ(0, use_count);
}


TEST(NodeUseIteratorOne) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node::Uses::iterator i(n0->uses().begin());
  CHECK_EQ(n1, *i);
  ++i;
  CHECK(n0->uses().end() == i);
}


TEST(NodeUseIteratorReplaceNoUses) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator);
  Node* n3 = graph.NewNode(&dummy_operator, n2);
  n0->ReplaceUses(n1);
  CHECK(n0->uses().begin() == n0->uses().end());
  n0->ReplaceUses(n2);
  CHECK(n0->uses().begin() == n0->uses().end());
  USE(n3);
}


TEST(NodeUseIteratorReplaceUses) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator);
  Node::Uses::iterator i1(n0->uses().begin());
  CHECK_EQ(n1, *i1);
  ++i1;
  CHECK_EQ(n2, *i1);
  n0->ReplaceUses(n3);
  Node::Uses::iterator i2(n3->uses().begin());
  CHECK_EQ(n1, *i2);
  ++i2;
  CHECK_EQ(n2, *i2);
  Node::Inputs::iterator i3(n1->inputs().begin());
  CHECK_EQ(n3, *i3);
  ++i3;
  CHECK(n1->inputs().end() == i3);
  Node::Inputs::iterator i4(n2->inputs().begin());
  CHECK_EQ(n3, *i4);
  ++i4;
  CHECK(n2->inputs().end() == i4);
}


TEST(NodeUseIteratorReplaceUsesSelf) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator);

  n1->ReplaceInput(0, n1);  // Create self-reference.

  Node::Uses::iterator i1(n1->uses().begin());
  CHECK_EQ(n1, *i1);

  n1->ReplaceUses(n3);

  CHECK(n1->uses().begin() == n1->uses().end());

  Node::Uses::iterator i2(n3->uses().begin());
  CHECK_EQ(n1, *i2);
  ++i2;
  CHECK(n1->uses().end() == i2);
}


TEST(ReplaceInput) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator);
  Node* n3 = graph.NewNode(&dummy_operator, n0, n1, n2);
  Node::Inputs::iterator i1(n3->inputs().begin());
  CHECK(n0 == *i1);
  CHECK_EQ(n0, n3->InputAt(0));
  ++i1;
  CHECK_EQ(n1, *i1);
  CHECK_EQ(n1, n3->InputAt(1));
  ++i1;
  CHECK_EQ(n2, *i1);
  CHECK_EQ(n2, n3->InputAt(2));
  ++i1;
  CHECK(i1 == n3->inputs().end());

  Node::Uses::iterator i2(n1->uses().begin());
  CHECK_EQ(n3, *i2);
  ++i2;
  CHECK(i2 == n1->uses().end());

  Node* n4 = graph.NewNode(&dummy_operator);
  Node::Uses::iterator i3(n4->uses().begin());
  CHECK(i3 == n4->uses().end());

  n3->ReplaceInput(1, n4);

  Node::Uses::iterator i4(n1->uses().begin());
  CHECK(i4 == n1->uses().end());

  Node::Uses::iterator i5(n4->uses().begin());
  CHECK_EQ(n3, *i5);
  ++i5;
  CHECK(i5 == n4->uses().end());

  Node::Inputs::iterator i6(n3->inputs().begin());
  CHECK(n0 == *i6);
  CHECK_EQ(n0, n3->InputAt(0));
  ++i6;
  CHECK_EQ(n4, *i6);
  CHECK_EQ(n4, n3->InputAt(1));
  ++i6;
  CHECK_EQ(n2, *i6);
  CHECK_EQ(n2, n3->InputAt(2));
  ++i6;
  CHECK(i6 == n3->inputs().end());
}


TEST(OwnedBy) {
  GraphTester graph;

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);

    CHECK(!n0->OwnedBy(n1));
    CHECK(!n1->OwnedBy(n0));

    Node* n2 = graph.NewNode(&dummy_operator, n0);
    CHECK(n0->OwnedBy(n2));
    CHECK(!n2->OwnedBy(n0));

    Node* n3 = graph.NewNode(&dummy_operator, n0);
    CHECK(!n0->OwnedBy(n2));
    CHECK(!n0->OwnedBy(n3));
    CHECK(!n2->OwnedBy(n0));
    CHECK(!n3->OwnedBy(n0));
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator, n0);
    CHECK(n0->OwnedBy(n1));
    CHECK(!n1->OwnedBy(n0));
    Node* n2 = graph.NewNode(&dummy_operator, n0);
    CHECK(!n0->OwnedBy(n1));
    CHECK(!n0->OwnedBy(n2));
    CHECK(!n1->OwnedBy(n0));
    CHECK(!n1->OwnedBy(n2));
    CHECK(!n2->OwnedBy(n0));
    CHECK(!n2->OwnedBy(n1));

    Node* n3 = graph.NewNode(&dummy_operator);
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


TEST(Uses) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  CHECK_EQ(1, n0->UseCount());
  printf("A: %d vs %d\n", n0->UseAt(0)->id(), n1->id());
  CHECK(n0->UseAt(0) == n1);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  CHECK_EQ(2, n0->UseCount());
  printf("B: %d vs %d\n", n0->UseAt(1)->id(), n2->id());
  CHECK(n0->UseAt(1) == n2);
  Node* n3 = graph.NewNode(&dummy_operator, n0);
  CHECK_EQ(3, n0->UseCount());
  CHECK(n0->UseAt(2) == n3);
}


TEST(Inputs) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator, n0, n1, n2);
  CHECK_EQ(3, n3->InputCount());
  CHECK(n3->InputAt(0) == n0);
  CHECK(n3->InputAt(1) == n1);
  CHECK(n3->InputAt(2) == n2);
  Node* n4 = graph.NewNode(&dummy_operator, n0, n1, n2);
  n3->AppendInput(graph.zone(), n4);
  CHECK_EQ(4, n3->InputCount());
  CHECK(n3->InputAt(0) == n0);
  CHECK(n3->InputAt(1) == n1);
  CHECK(n3->InputAt(2) == n2);
  CHECK(n3->InputAt(3) == n4);
  Node* n5 = graph.NewNode(&dummy_operator, n4);
  n3->AppendInput(graph.zone(), n4);
  CHECK_EQ(5, n3->InputCount());
  CHECK(n3->InputAt(0) == n0);
  CHECK(n3->InputAt(1) == n1);
  CHECK(n3->InputAt(2) == n2);
  CHECK(n3->InputAt(3) == n4);
  CHECK(n3->InputAt(4) == n4);

  // Make sure uses have been hooked op correctly.
  Node::Uses uses(n4->uses());
  Node::Uses::iterator current = uses.begin();
  CHECK(current != uses.end());
  CHECK(*current == n3);
  ++current;
  CHECK(current != uses.end());
  CHECK(*current == n5);
  ++current;
  CHECK(current != uses.end());
  CHECK(*current == n3);
  ++current;
  CHECK(current == uses.end());
}


TEST(RemoveInput) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0, n1);

  n1->RemoveInput(0);
  CHECK_EQ(0, n1->InputCount());
  CHECK_EQ(1, n0->UseCount());

  n2->RemoveInput(0);
  CHECK_EQ(1, n2->InputCount());
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(1, n1->UseCount());

  n2->RemoveInput(0);
  CHECK_EQ(0, n2->InputCount());
}


TEST(AppendInputsAndIterator) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0, n1);

  Node::Inputs inputs(n2->inputs());
  Node::Inputs::iterator current = inputs.begin();
  CHECK(current != inputs.end());
  CHECK(*current == n0);
  ++current;
  CHECK(current != inputs.end());
  CHECK(*current == n1);
  ++current;
  CHECK(current == inputs.end());

  Node* n3 = graph.NewNode(&dummy_operator);
  n2->AppendInput(graph.zone(), n3);
  inputs = n2->inputs();
  current = inputs.begin();
  CHECK(current != inputs.end());
  CHECK(*current == n0);
  CHECK_EQ(0, current.index());
  ++current;
  CHECK(current != inputs.end());
  CHECK(*current == n1);
  CHECK_EQ(1, current.index());
  ++current;
  CHECK(current != inputs.end());
  CHECK(*current == n3);
  CHECK_EQ(2, current.index());
  ++current;
  CHECK(current == inputs.end());
}


TEST(NullInputsSimple) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0, n1);
  CHECK_EQ(2, n2->InputCount());

  CHECK(n0 == n2->InputAt(0));
  CHECK(n1 == n2->InputAt(1));
  CHECK_EQ(2, n0->UseCount());
  n2->ReplaceInput(0, NULL);
  CHECK(NULL == n2->InputAt(0));
  CHECK(n1 == n2->InputAt(1));
  CHECK_EQ(1, n0->UseCount());
}


TEST(NullInputsAppended) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator, n0);
  n3->AppendInput(graph.zone(), n1);
  n3->AppendInput(graph.zone(), n2);
  CHECK_EQ(3, n3->InputCount());

  CHECK(n0 == n3->InputAt(0));
  CHECK(n1 == n3->InputAt(1));
  CHECK(n2 == n3->InputAt(2));
  CHECK_EQ(1, n1->UseCount());
  n3->ReplaceInput(1, NULL);
  CHECK(n0 == n3->InputAt(0));
  CHECK(NULL == n3->InputAt(1));
  CHECK(n2 == n3->InputAt(2));
  CHECK_EQ(0, n1->UseCount());
}


TEST(ReplaceUsesFromAppendedInputs) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator);
  n2->AppendInput(graph.zone(), n1);
  n2->AppendInput(graph.zone(), n0);
  CHECK_EQ(0, n3->UseCount());
  CHECK_EQ(3, n0->UseCount());
  n0->ReplaceUses(n3);
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(3, n3->UseCount());

  Node::Uses uses(n3->uses());
  Node::Uses::iterator current = uses.begin();
  CHECK(current != uses.end());
  CHECK(*current == n1);
  ++current;
  CHECK(current != uses.end());
  CHECK(*current == n2);
  ++current;
  CHECK(current != uses.end());
  CHECK(*current == n2);
  ++current;
  CHECK(current == uses.end());
}


template <bool result>
struct FixedPredicate {
  bool operator()(const Node* node) const { return result; }
};


TEST(ReplaceUsesIfWithFixedPredicate) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator);

  CHECK_EQ(0, n2->UseCount());
  n2->ReplaceUsesIf(FixedPredicate<true>(), n1);
  CHECK_EQ(0, n2->UseCount());
  n2->ReplaceUsesIf(FixedPredicate<false>(), n1);
  CHECK_EQ(0, n2->UseCount());

  CHECK_EQ(0, n3->UseCount());
  n3->ReplaceUsesIf(FixedPredicate<true>(), n1);
  CHECK_EQ(0, n3->UseCount());
  n3->ReplaceUsesIf(FixedPredicate<false>(), n1);
  CHECK_EQ(0, n3->UseCount());

  CHECK_EQ(2, n0->UseCount());
  CHECK_EQ(0, n1->UseCount());
  n0->ReplaceUsesIf(FixedPredicate<false>(), n1);
  CHECK_EQ(2, n0->UseCount());
  CHECK_EQ(0, n1->UseCount());
  n0->ReplaceUsesIf(FixedPredicate<true>(), n1);
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(2, n1->UseCount());

  n1->AppendInput(graph.zone(), n1);
  CHECK_EQ(3, n1->UseCount());
  n1->AppendInput(graph.zone(), n3);
  CHECK_EQ(1, n3->UseCount());
  n3->ReplaceUsesIf(FixedPredicate<true>(), n1);
  CHECK_EQ(4, n1->UseCount());
  CHECK_EQ(0, n3->UseCount());
  n1->ReplaceUsesIf(FixedPredicate<false>(), n3);
  CHECK_EQ(4, n1->UseCount());
  CHECK_EQ(0, n3->UseCount());
}


TEST(ReplaceUsesIfWithEqualTo) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0, n1);

  CHECK_EQ(0, n2->UseCount());
  n2->ReplaceUsesIf(std::bind1st(std::equal_to<Node*>(), n1), n0);
  CHECK_EQ(0, n2->UseCount());

  CHECK_EQ(2, n0->UseCount());
  CHECK_EQ(1, n1->UseCount());
  n1->ReplaceUsesIf(std::bind1st(std::equal_to<Node*>(), n0), n0);
  CHECK_EQ(2, n0->UseCount());
  CHECK_EQ(1, n1->UseCount());
  n0->ReplaceUsesIf(std::bind2nd(std::equal_to<Node*>(), n2), n1);
  CHECK_EQ(1, n0->UseCount());
  CHECK_EQ(2, n1->UseCount());
}


TEST(ReplaceInputMultipleUses) {
  GraphTester graph;

  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  n2->ReplaceInput(0, n1);
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(1, n1->UseCount());

  Node* n3 = graph.NewNode(&dummy_operator, n0);
  n3->ReplaceInput(0, n1);
  CHECK_EQ(0, n0->UseCount());
  CHECK_EQ(2, n1->UseCount());
}


TEST(TrimInputCountInline) {
  GraphTester graph;

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator, n0);
    n1->TrimInputCount(1);
    CHECK_EQ(1, n1->InputCount());
    CHECK_EQ(n0, n1->InputAt(0));
    CHECK_EQ(1, n0->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator, n0);
    n1->TrimInputCount(0);
    CHECK_EQ(0, n1->InputCount());
    CHECK_EQ(0, n0->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0, n1);
    n2->TrimInputCount(2);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(1, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0, n1);
    n2->TrimInputCount(1);
    CHECK_EQ(1, n2->InputCount());
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0, n1);
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0, n0);
    n2->TrimInputCount(1);
    CHECK_EQ(1, n2->InputCount());
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0, n0);
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }
}


TEST(TrimInputCountOutOfLine1) {
  GraphTester graph;

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    n1->AppendInput(graph.zone(), n0);
    n1->TrimInputCount(1);
    CHECK_EQ(1, n1->InputCount());
    CHECK_EQ(n0, n1->InputAt(0));
    CHECK_EQ(1, n0->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    n1->AppendInput(graph.zone(), n0);
    CHECK_EQ(1, n1->InputCount());
    n1->TrimInputCount(0);
    CHECK_EQ(0, n1->InputCount());
    CHECK_EQ(0, n0->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_EQ(2, n2->InputCount());
    n2->TrimInputCount(2);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(n0, n2->InputAt(0));
    CHECK_EQ(n1, n2->InputAt(1));
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(1, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_EQ(2, n2->InputCount());
    n2->TrimInputCount(1);
    CHECK_EQ(1, n2->InputCount());
    CHECK_EQ(n0, n2->InputAt(0));
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_EQ(2, n2->InputCount());
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(2, n0->UseCount());
    n2->TrimInputCount(1);
    CHECK_EQ(1, n2->InputCount());
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator);
    n2->AppendInput(graph.zone(), n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(2, n0->UseCount());
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }
}


TEST(TrimInputCountOutOfLine2) {
  GraphTester graph;

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_EQ(2, n2->InputCount());
    n2->TrimInputCount(2);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(n0, n2->InputAt(0));
    CHECK_EQ(n1, n2->InputAt(1));
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(1, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_EQ(2, n2->InputCount());
    n2->TrimInputCount(1);
    CHECK_EQ(1, n2->InputCount());
    CHECK_EQ(n0, n2->InputAt(0));
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0);
    n2->AppendInput(graph.zone(), n1);
    CHECK_EQ(2, n2->InputCount());
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(2, n0->UseCount());
    n2->TrimInputCount(1);
    CHECK_EQ(1, n2->InputCount());
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n2 = graph.NewNode(&dummy_operator, n0);
    n2->AppendInput(graph.zone(), n0);
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(2, n0->UseCount());
    n2->TrimInputCount(0);
    CHECK_EQ(0, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n2->UseCount());
  }
}


TEST(RemoveAllInputs) {
  GraphTester graph;

  for (int i = 0; i < 2; i++) {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator, n0);
    Node* n2;
    if (i == 0) {
      n2 = graph.NewNode(&dummy_operator, n0, n1);
    } else {
      n2 = graph.NewNode(&dummy_operator, n0);
      n2->AppendInput(graph.zone(), n1);  // with out-of-line input.
    }

    n0->RemoveAllInputs();
    CHECK_EQ(0, n0->InputCount());

    CHECK_EQ(2, n0->UseCount());
    n1->RemoveAllInputs();
    CHECK_EQ(1, n1->InputCount());
    CHECK_EQ(1, n0->UseCount());
    CHECK_EQ(NULL, n1->InputAt(0));

    CHECK_EQ(1, n1->UseCount());
    n2->RemoveAllInputs();
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(NULL, n2->InputAt(0));
    CHECK_EQ(NULL, n2->InputAt(1));
  }

  {
    Node* n0 = graph.NewNode(&dummy_operator);
    Node* n1 = graph.NewNode(&dummy_operator, n0);
    n1->ReplaceInput(0, n1);  // self-reference.

    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(1, n1->UseCount());
    n1->RemoveAllInputs();
    CHECK_EQ(1, n1->InputCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK_EQ(NULL, n1->InputAt(0));
  }
}
