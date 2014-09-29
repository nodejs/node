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

static SimpleOperator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                                     0, 0, "dummy");

class PreNodeVisitor : public NullNodeVisitor {
 public:
  GenericGraphVisit::Control Pre(Node* node) {
    printf("NODE ID: %d\n", node->id());
    nodes_.push_back(node);
    return GenericGraphVisit::CONTINUE;
  }
  std::vector<Node*> nodes_;
};


class PostNodeVisitor : public NullNodeVisitor {
 public:
  GenericGraphVisit::Control Post(Node* node) {
    printf("NODE ID: %d\n", node->id());
    nodes_.push_back(node);
    return GenericGraphVisit::CONTINUE;
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


struct ReenterNodeVisitor : NullNodeVisitor {
  GenericGraphVisit::Control Pre(Node* node) {
    printf("[%d] PRE NODE: %d\n", static_cast<int>(nodes_.size()), node->id());
    nodes_.push_back(node->id());
    int size = static_cast<int>(nodes_.size());
    switch (node->id()) {
      case 0:
        return size < 6 ? GenericGraphVisit::REENTER : GenericGraphVisit::SKIP;
      case 1:
        return size < 4 ? GenericGraphVisit::DEFER
                        : GenericGraphVisit::CONTINUE;
      default:
        return GenericGraphVisit::REENTER;
    }
  }

  GenericGraphVisit::Control Post(Node* node) {
    printf("[%d] POST NODE: %d\n", static_cast<int>(nodes_.size()), node->id());
    nodes_.push_back(-node->id());
    return node->id() == 4 ? GenericGraphVisit::REENTER
                           : GenericGraphVisit::CONTINUE;
  }

  void PreEdge(Node* from, int index, Node* to) {
    printf("[%d] PRE EDGE: %d-%d\n", static_cast<int>(edges_.size()),
           from->id(), to->id());
    edges_.push_back(std::make_pair(from->id(), to->id()));
  }

  void PostEdge(Node* from, int index, Node* to) {
    printf("[%d] POST EDGE: %d-%d\n", static_cast<int>(edges_.size()),
           from->id(), to->id());
    edges_.push_back(std::make_pair(-from->id(), -to->id()));
  }

  std::vector<int> nodes_;
  std::vector<std::pair<int, int> > edges_;
};


TEST(TestUseNodeReenterVisit) {
  GraphWithStartNodeTester graph;
  Node* n0 = graph.start_node();
  Node* n1 = graph.NewNode(&dummy_operator, n0);
  Node* n2 = graph.NewNode(&dummy_operator, n0);
  Node* n3 = graph.NewNode(&dummy_operator, n2);
  Node* n4 = graph.NewNode(&dummy_operator, n0);
  Node* n5 = graph.NewNode(&dummy_operator, n4);
  n0->AppendInput(graph.main_zone(), n3);
  graph.SetStart(n0);
  graph.SetEnd(n5);

  ReenterNodeVisitor visitor;
  graph.VisitNodeUsesFromStart(&visitor);

  CHECK_EQ(22, static_cast<int>(visitor.nodes_.size()));
  CHECK_EQ(24, static_cast<int>(visitor.edges_.size()));

  CHECK(n0->id() == visitor.nodes_[0]);
  CHECK(n0->id() == visitor.edges_[0].first);
  CHECK(n1->id() == visitor.edges_[0].second);
  CHECK(n1->id() == visitor.nodes_[1]);
  // N1 is deferred.
  CHECK(-n1->id() == visitor.edges_[1].second);
  CHECK(-n0->id() == visitor.edges_[1].first);
  CHECK(n0->id() == visitor.edges_[2].first);
  CHECK(n2->id() == visitor.edges_[2].second);
  CHECK(n2->id() == visitor.nodes_[2]);
  CHECK(n2->id() == visitor.edges_[3].first);
  CHECK(n3->id() == visitor.edges_[3].second);
  CHECK(n3->id() == visitor.nodes_[3]);
  // Circle back to N0, which we may reenter for now.
  CHECK(n3->id() == visitor.edges_[4].first);
  CHECK(n0->id() == visitor.edges_[4].second);
  CHECK(n0->id() == visitor.nodes_[4]);
  CHECK(n0->id() == visitor.edges_[5].first);
  CHECK(n1->id() == visitor.edges_[5].second);
  CHECK(n1->id() == visitor.nodes_[5]);
  // This time N1 is no longer deferred.
  CHECK(-n1->id() == visitor.nodes_[6]);
  CHECK(-n1->id() == visitor.edges_[6].second);
  CHECK(-n0->id() == visitor.edges_[6].first);
  CHECK(n0->id() == visitor.edges_[7].first);
  CHECK(n2->id() == visitor.edges_[7].second);
  CHECK(n2->id() == visitor.nodes_[7]);
  CHECK(n2->id() == visitor.edges_[8].first);
  CHECK(n3->id() == visitor.edges_[8].second);
  CHECK(n3->id() == visitor.nodes_[8]);
  CHECK(n3->id() == visitor.edges_[9].first);
  CHECK(n0->id() == visitor.edges_[9].second);
  CHECK(n0->id() == visitor.nodes_[9]);
  // This time we break at N0 and skip it.
  CHECK(-n0->id() == visitor.edges_[10].second);
  CHECK(-n3->id() == visitor.edges_[10].first);
  CHECK(-n3->id() == visitor.nodes_[10]);
  CHECK(-n3->id() == visitor.edges_[11].second);
  CHECK(-n2->id() == visitor.edges_[11].first);
  CHECK(-n2->id() == visitor.nodes_[11]);
  CHECK(-n2->id() == visitor.edges_[12].second);
  CHECK(-n0->id() == visitor.edges_[12].first);
  CHECK(n0->id() == visitor.edges_[13].first);
  CHECK(n4->id() == visitor.edges_[13].second);
  CHECK(n4->id() == visitor.nodes_[12]);
  CHECK(n4->id() == visitor.edges_[14].first);
  CHECK(n5->id() == visitor.edges_[14].second);
  CHECK(n5->id() == visitor.nodes_[13]);
  CHECK(-n5->id() == visitor.nodes_[14]);
  CHECK(-n5->id() == visitor.edges_[15].second);
  CHECK(-n4->id() == visitor.edges_[15].first);
  CHECK(-n4->id() == visitor.nodes_[15]);
  CHECK(-n4->id() == visitor.edges_[16].second);
  CHECK(-n0->id() == visitor.edges_[16].first);
  CHECK(-n0->id() == visitor.nodes_[16]);
  CHECK(-n0->id() == visitor.edges_[17].second);
  CHECK(-n3->id() == visitor.edges_[17].first);
  CHECK(-n3->id() == visitor.nodes_[17]);
  CHECK(-n3->id() == visitor.edges_[18].second);
  CHECK(-n2->id() == visitor.edges_[18].first);
  CHECK(-n2->id() == visitor.nodes_[18]);
  CHECK(-n2->id() == visitor.edges_[19].second);
  CHECK(-n0->id() == visitor.edges_[19].first);
  // N4 may be reentered.
  CHECK(n0->id() == visitor.edges_[20].first);
  CHECK(n4->id() == visitor.edges_[20].second);
  CHECK(n4->id() == visitor.nodes_[19]);
  CHECK(n4->id() == visitor.edges_[21].first);
  CHECK(n5->id() == visitor.edges_[21].second);
  CHECK(-n5->id() == visitor.edges_[22].second);
  CHECK(-n4->id() == visitor.edges_[22].first);
  CHECK(-n4->id() == visitor.nodes_[20]);
  CHECK(-n4->id() == visitor.edges_[23].second);
  CHECK(-n0->id() == visitor.edges_[23].first);
  CHECK(-n0->id() == visitor.nodes_[21]);
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
