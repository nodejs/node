// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/phi-reducer.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

class PhiReducerTester : HandleAndZoneScope {
 public:
  explicit PhiReducerTester(int num_parameters = 0)
      : isolate(main_isolate()),
        common(main_zone()),
        graph(main_zone()),
        self(graph.NewNode(common.Start(num_parameters))),
        dead(graph.NewNode(common.Dead())) {
    graph.SetStart(self);
  }

  Isolate* isolate;
  CommonOperatorBuilder common;
  Graph graph;
  Node* self;
  Node* dead;

  void CheckReduce(Node* expect, Node* phi) {
    PhiReducer reducer;
    Reduction reduction = reducer.Reduce(phi);
    if (expect == phi) {
      CHECK(!reduction.Changed());
    } else {
      CHECK(reduction.Changed());
      CHECK_EQ(expect, reduction.replacement());
    }
  }

  Node* Int32Constant(int32_t val) {
    return graph.NewNode(common.Int32Constant(val));
  }

  Node* Float64Constant(double val) {
    return graph.NewNode(common.Float64Constant(val));
  }

  Node* Parameter(int32_t index = 0) {
    return graph.NewNode(common.Parameter(index), graph.start());
  }

  Node* Phi(Node* a) {
    return SetSelfReferences(graph.NewNode(common.Phi(1), a));
  }

  Node* Phi(Node* a, Node* b) {
    return SetSelfReferences(graph.NewNode(common.Phi(2), a, b));
  }

  Node* Phi(Node* a, Node* b, Node* c) {
    return SetSelfReferences(graph.NewNode(common.Phi(3), a, b, c));
  }

  Node* Phi(Node* a, Node* b, Node* c, Node* d) {
    return SetSelfReferences(graph.NewNode(common.Phi(4), a, b, c, d));
  }

  Node* PhiWithControl(Node* a, Node* control) {
    return SetSelfReferences(graph.NewNode(common.Phi(1), a, control));
  }

  Node* PhiWithControl(Node* a, Node* b, Node* control) {
    return SetSelfReferences(graph.NewNode(common.Phi(2), a, b, control));
  }

  Node* SetSelfReferences(Node* node) {
    Node::Inputs inputs = node->inputs();
    for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
         ++iter) {
      Node* input = *iter;
      if (input == self) node->ReplaceInput(iter.index(), node);
    }
    return node;
  }
};


TEST(PhiReduce1) {
  PhiReducerTester R;
  Node* zero = R.Int32Constant(0);
  Node* one = R.Int32Constant(1);
  Node* oneish = R.Float64Constant(1.1);
  Node* param = R.Parameter();

  Node* singles[] = {zero, one, oneish, param};
  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    R.CheckReduce(singles[i], R.Phi(singles[i]));
  }
}


TEST(PhiReduce2) {
  PhiReducerTester R;
  Node* zero = R.Int32Constant(0);
  Node* one = R.Int32Constant(1);
  Node* oneish = R.Float64Constant(1.1);
  Node* param = R.Parameter();

  Node* singles[] = {zero, one, oneish, param};
  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i];
    R.CheckReduce(a, R.Phi(a, a));
  }

  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i];
    R.CheckReduce(a, R.Phi(R.self, a));
    R.CheckReduce(a, R.Phi(a, R.self));
  }

  for (size_t i = 1; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i], *b = singles[0];
    Node* phi1 = R.Phi(b, a);
    R.CheckReduce(phi1, phi1);

    Node* phi2 = R.Phi(a, b);
    R.CheckReduce(phi2, phi2);
  }
}


TEST(PhiReduce3) {
  PhiReducerTester R;
  Node* zero = R.Int32Constant(0);
  Node* one = R.Int32Constant(1);
  Node* oneish = R.Float64Constant(1.1);
  Node* param = R.Parameter();

  Node* singles[] = {zero, one, oneish, param};
  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i];
    R.CheckReduce(a, R.Phi(a, a, a));
  }

  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i];
    R.CheckReduce(a, R.Phi(R.self, a, a));
    R.CheckReduce(a, R.Phi(a, R.self, a));
    R.CheckReduce(a, R.Phi(a, a, R.self));
  }

  for (size_t i = 1; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i], *b = singles[0];
    Node* phi1 = R.Phi(b, a, a);
    R.CheckReduce(phi1, phi1);

    Node* phi2 = R.Phi(a, b, a);
    R.CheckReduce(phi2, phi2);

    Node* phi3 = R.Phi(a, a, b);
    R.CheckReduce(phi3, phi3);
  }
}


TEST(PhiReduce4) {
  PhiReducerTester R;
  Node* zero = R.Int32Constant(0);
  Node* one = R.Int32Constant(1);
  Node* oneish = R.Float64Constant(1.1);
  Node* param = R.Parameter();

  Node* singles[] = {zero, one, oneish, param};
  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i];
    R.CheckReduce(a, R.Phi(a, a, a, a));
  }

  for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i];
    R.CheckReduce(a, R.Phi(R.self, a, a, a));
    R.CheckReduce(a, R.Phi(a, R.self, a, a));
    R.CheckReduce(a, R.Phi(a, a, R.self, a));
    R.CheckReduce(a, R.Phi(a, a, a, R.self));

    R.CheckReduce(a, R.Phi(R.self, R.self, a, a));
    R.CheckReduce(a, R.Phi(a, R.self, R.self, a));
    R.CheckReduce(a, R.Phi(a, a, R.self, R.self));
    R.CheckReduce(a, R.Phi(R.self, a, a, R.self));
  }

  for (size_t i = 1; i < ARRAY_SIZE(singles); i++) {
    Node* a = singles[i], *b = singles[0];
    Node* phi1 = R.Phi(b, a, a, a);
    R.CheckReduce(phi1, phi1);

    Node* phi2 = R.Phi(a, b, a, a);
    R.CheckReduce(phi2, phi2);

    Node* phi3 = R.Phi(a, a, b, a);
    R.CheckReduce(phi3, phi3);

    Node* phi4 = R.Phi(a, a, a, b);
    R.CheckReduce(phi4, phi4);
  }
}


TEST(PhiReduceShouldIgnoreControlNodes) {
  PhiReducerTester R;
  Node* zero = R.Int32Constant(0);
  Node* one = R.Int32Constant(1);
  Node* oneish = R.Float64Constant(1.1);
  Node* param = R.Parameter();

  Node* singles[] = {zero, one, oneish, param};
  for (size_t i = 0; i < ARRAY_SIZE(singles); ++i) {
    R.CheckReduce(singles[i], R.PhiWithControl(singles[i], R.dead));
    R.CheckReduce(singles[i], R.PhiWithControl(R.self, singles[i], R.dead));
    R.CheckReduce(singles[i], R.PhiWithControl(singles[i], R.self, R.dead));
  }
}
