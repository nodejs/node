// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/v8.h"

#include "graph-tester.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/operator.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 1, 0, 0);

class PreNodeVisitor : public NullNodeVisitor {
 public:
  void Pre(Node* node) {
    printf("NODE ID: %d\n", node->id());
    nodes_.push_back(node);
  }
  std::vector<Node*> nodes_;
};


class PostNodeVisitor : public NullNodeVisitor {
 public:
  void Post(Node* node) {
    printf("NODE ID: %d\n", node->id());
    nodes_.push_back(node);
  }
  std::vector<Node*> nodes_;
};


TEST(TestUseNodeVisitEmpty) {
  GraphWithStartNodeTester graph;

  PreNodeVisitor node_visitor;
  graph.VisitNodeUsesFromStart(&node_visitor);

  CHECK_EQ(1, static_cast<int>(node_visitor.nodes_.size()));
}


TEST(TestUseNodePreOrderVisitSimple) {
  GraphWithStartNodeTester graph;
  Node* n2 = graph.NewNode(&dummy_operator, graph.start());
  Node* n3 = graph.NewNode(&dummy_operator, n2);
  Node* n4 = graph.NewNode(&dummy_operator, n2, n3);
  Node* n5 = graph.NewNode(&dummy_operator, n4, n2);
  graph.SetEnd(n5);

  PreNodeVisitor node_visitor;
  graph.VisitNodeUsesFromStart(&node_visitor);

  CHECK_EQ(5, static_cast<int>(node_visitor.nodes_.size()));
  CHECK(graph.start()->id() == node_visitor.nodes_[0]->id());
  CHECK(n2->id() == node_visitor.nodes_[1]->id());
  CHECK(n3->id() == node_visitor.nodes_[2]->id());
  CHECK(n4->id() == node_visitor.nodes_[3]->id());
  CHECK(n5->id() == node_visitor.nodes_[4]->id());
}


TEST(TestInputNodePreOrderVisitSimple) {
  GraphWithStartNodeTester graph;
  Node* n2 = graph.NewNode(&dummy_operator, graph.start());
  Node* n3 = graph.NewNode(&dummy_operator, n2);
  Node* n4 = graph.NewNode(&dummy_operator, n2, n3);
  Node* n5 = graph.NewNode(&dummy_operator, n4, n2);
  graph.SetEnd(n5);

  PreNodeVisitor node_visitor;
  graph.VisitNodeInputsFromEnd(&node_visitor);
  CHECK_EQ(5, static_cast<int>(node_visitor.nodes_.size()));
  CHECK(n5->id() == node_visitor.nodes_[0]->id());
  CHECK(n4->id() == node_visitor.nodes_[1]->id());
  CHECK(n2->id() == node_visitor.nodes_[2]->id());
  CHECK(graph.start()->id() == node_visitor.nodes_[3]->id());
  CHECK(n3->id() == node_visitor.nodes_[4]->id());
}


TEST(TestUseNodePostOrderVisitSimple) {
  GraphWithStartNodeTester graph;
  Node* n2 = graph.NewNode(&dummy_operator, graph.start());
  Node* n3 = graph.NewNode(&dummy_operator, graph.start());
  Node* n4 = graph.NewNode(&dummy_operator, n2);
  Node* n5 = graph.NewNode(&dummy_operator, n2);
  Node* n6 = graph.NewNode(&dummy_operator, n2);
  Node* n7 = graph.NewNode(&dummy_operator, n3);
  Node* end_dependencies[4] = {n4, n5, n6, n7};
  Node* n8 = graph.NewNode(&dummy_operator, 4, end_dependencies);
  graph.SetEnd(n8);

  PostNodeVisitor node_visitor;
  graph.VisitNodeUsesFromStart(&node_visitor);

  CHECK_EQ(8, static_cast<int>(node_visitor.nodes_.size()));
  CHECK(graph.end()->id() == node_visitor.nodes_[0]->id());
  CHECK(n4->id() == node_visitor.nodes_[1]->id());
  CHECK(n5->id() == node_visitor.nodes_[2]->id());
  CHECK(n6->id() == node_visitor.nodes_[3]->id());
  CHECK(n2->id() == node_visitor.nodes_[4]->id());
  CHECK(n7->id() == node_visitor.nodes_[5]->id());
  CHECK(n3->id() == node_visitor.nodes_[6]->id());
  CHECK(graph.start()->id() == node_visitor.nodes_[7]->id());
}


TEST(TestUseNodePostOrderVisitLong) {
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

  PostNodeVisitor node_visitor;
  graph.VisitNodeUsesFromStart(&node_visitor);

  CHECK_EQ(12, static_cast<int>(node_visitor.nodes_.size()));
  CHECK(graph.end()->id() == node_visitor.nodes_[0]->id());
  CHECK(n4->id() == node_visitor.nodes_[1]->id());
  CHECK(n8->id() == node_visitor.nodes_[2]->id());
  CHECK(n10->id() == node_visitor.nodes_[3]->id());
  CHECK(n11->id() == node_visitor.nodes_[4]->id());
  CHECK(n9->id() == node_visitor.nodes_[5]->id());
  CHECK(n5->id() == node_visitor.nodes_[6]->id());
  CHECK(n2->id() == node_visitor.nodes_[7]->id());
  CHECK(n6->id() == node_visitor.nodes_[8]->id());
  CHECK(n7->id() == node_visitor.nodes_[9]->id());
  CHECK(n3->id() == node_visitor.nodes_[10]->id());
  CHECK(graph.start()->id() == node_visitor.nodes_[11]->id());
}


TEST(TestUseNodePreOrderVisitCycle) {
  GraphWithStartNodeTester graph;
  Node* n0 = graph.start_node();
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n1);
  n0->AppendInput(graph.main_zone(), n2);
  graph.SetStart(n0);
  graph.SetEnd(n2);

  PreNodeVisitor node_visitor;
  graph.VisitNodeUsesFromStart(&node_visitor);

  CHECK_EQ(3, static_cast<int>(node_visitor.nodes_.size()));
  CHECK(n0->id() == node_visitor.nodes_[0]->id());
  CHECK(n1->id() == node_visitor.nodes_[1]->id());
  CHECK(n2->id() == node_visitor.nodes_[2]->id());
}


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
