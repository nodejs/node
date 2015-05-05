// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/v8.h"

#include "graph-tester.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 1, 0, 0);

TEST(TestPrintNodeGraphToNodeGraphviz) {
  GraphWithStartNodeTester graph;
  Node* n2 = graph.NewNode(&dummy_operator, graph.start());
  Node* n3 = graph.NewNode(&dummy_operator, graph.start());
  Node* n4 = graph.NewNode(&dummy_operator, n2);
  Node* n5 = graph.NewNode(&dummy_operator, n2);
  Node* n6 = graph.NewNode(&dummy_operator, n3);
  Node* n7 = graph.NewNode(&dummy_operator, n3);
  Node* n8 = graph.NewNode(&dummy_operator, n5);
  Node* n9 = graph.NewNode(&dummy_operator, n5);
  Node* n10 = graph.NewNode(&dummy_operator, n9);
  Node* n11 = graph.NewNode(&dummy_operator, n9);
  Node* end_dependencies[6] = {n4, n8, n10, n11, n6, n7};
  Node* n12 = graph.NewNode(&dummy_operator, 6, end_dependencies);
  graph.SetEnd(n12);

  OFStream os(stdout);
  os << AsDOT(graph);
}
