// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "src/v8.h"

#include "graph-tester.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 1, 0, 0);

TEST(NodeUseIteratorReplaceUses) {
  GraphTester graph;
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator);
  auto i1(n0->uses().begin());
  CHECK_EQ(n1, *i1);
  ++i1;
  CHECK_EQ(n2, *i1);
  n0->ReplaceUses(n3);
  auto i2(n3->uses().begin());
  CHECK_EQ(n1, *i2);
  ++i2;
  CHECK_EQ(n2, *i2);
  auto i3(n1->inputs().begin());
  CHECK_EQ(n3, *i3);
  ++i3;
  CHECK(n1->inputs().end() == i3);
  auto i4(n2->inputs().begin());
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

  auto i1(n1->uses().begin());
  CHECK_EQ(n1, *i1);

  n1->ReplaceUses(n3);

  CHECK(n1->uses().begin() == n1->uses().end());

  auto i2(n3->uses().begin());
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
  auto i1(n3->inputs().begin());
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

  auto i2(n1->uses().begin());
  CHECK_EQ(n3, *i2);
  ++i2;
  CHECK(i2 == n1->uses().end());

  Node* n4 = graph.NewNode(&dummy_operator);
  auto i3(n4->uses().begin());
  CHECK(i3 == n4->uses().end());

  n3->ReplaceInput(1, n4);

  auto i4(n1->uses().begin());
  CHECK(i4 == n1->uses().end());

  auto i5(n4->uses().begin());
  CHECK_EQ(n3, *i5);
  ++i5;
  CHECK(i5 == n4->uses().end());

  auto i6(n3->inputs().begin());
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
  auto current = uses.begin();
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

  Node::InputEdges inputs(n2->input_edges());
  Node::InputEdges::iterator current = inputs.begin();
  CHECK(current != inputs.end());
  CHECK((*current).to() == n0);
  ++current;
  CHECK(current != inputs.end());
  CHECK((*current).to() == n1);
  ++current;
  CHECK(current == inputs.end());

  Node* n3 = graph.NewNode(&dummy_operator);
  n2->AppendInput(graph.zone(), n3);
  inputs = n2->input_edges();
  current = inputs.begin();
  CHECK(current != inputs.end());
  CHECK((*current).to() == n0);
  CHECK_EQ(0, (*current).index());
  ++current;
  CHECK(current != inputs.end());
  CHECK((*current).to() == n1);
  CHECK_EQ(1, (*current).index());
  ++current;
  CHECK(current != inputs.end());
  CHECK((*current).to() == n3);
  CHECK_EQ(2, (*current).index());
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
  auto current = uses.begin();
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
    CHECK(!n1->InputAt(0));

    CHECK_EQ(1, n1->UseCount());
    n2->RemoveAllInputs();
    CHECK_EQ(2, n2->InputCount());
    CHECK_EQ(0, n0->UseCount());
    CHECK_EQ(0, n1->UseCount());
    CHECK(!n2->InputAt(0));
    CHECK(!n2->InputAt(1));
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
    CHECK(!n1->InputAt(0));
  }
}
