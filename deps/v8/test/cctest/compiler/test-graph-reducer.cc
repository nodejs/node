// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "graph-tester.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph-reducer.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

const uint8_t OPCODE_A0 = 10;
const uint8_t OPCODE_A1 = 11;
const uint8_t OPCODE_A2 = 12;
const uint8_t OPCODE_B0 = 20;
const uint8_t OPCODE_B1 = 21;
const uint8_t OPCODE_B2 = 22;
const uint8_t OPCODE_C0 = 30;
const uint8_t OPCODE_C1 = 31;
const uint8_t OPCODE_C2 = 32;

static SimpleOperator OPA0(OPCODE_A0, Operator::kNoWrite, 0, 0, "opa0");
static SimpleOperator OPA1(OPCODE_A1, Operator::kNoWrite, 1, 0, "opa1");
static SimpleOperator OPA2(OPCODE_A2, Operator::kNoWrite, 2, 0, "opa2");
static SimpleOperator OPB0(OPCODE_B0, Operator::kNoWrite, 0, 0, "opa0");
static SimpleOperator OPB1(OPCODE_B1, Operator::kNoWrite, 1, 0, "opa1");
static SimpleOperator OPB2(OPCODE_B2, Operator::kNoWrite, 2, 0, "opa2");
static SimpleOperator OPC0(OPCODE_C0, Operator::kNoWrite, 0, 0, "opc0");
static SimpleOperator OPC1(OPCODE_C1, Operator::kNoWrite, 1, 0, "opc1");
static SimpleOperator OPC2(OPCODE_C2, Operator::kNoWrite, 2, 0, "opc2");


// Replaces all "A" operators with "B" operators without creating new nodes.
class InPlaceABReducer : public Reducer {
 public:
  virtual Reduction Reduce(Node* node) {
    switch (node->op()->opcode()) {
      case OPCODE_A0:
        CHECK_EQ(0, node->InputCount());
        node->set_op(&OPB0);
        return Replace(node);
      case OPCODE_A1:
        CHECK_EQ(1, node->InputCount());
        node->set_op(&OPB1);
        return Replace(node);
      case OPCODE_A2:
        CHECK_EQ(2, node->InputCount());
        node->set_op(&OPB2);
        return Replace(node);
    }
    return NoChange();
  }
};


// Replaces all "A" operators with "B" operators by allocating new nodes.
class NewABReducer : public Reducer {
 public:
  explicit NewABReducer(Graph* graph) : graph_(graph) {}
  virtual Reduction Reduce(Node* node) {
    switch (node->op()->opcode()) {
      case OPCODE_A0:
        CHECK_EQ(0, node->InputCount());
        return Replace(graph_->NewNode(&OPB0));
      case OPCODE_A1:
        CHECK_EQ(1, node->InputCount());
        return Replace(graph_->NewNode(&OPB1, node->InputAt(0)));
      case OPCODE_A2:
        CHECK_EQ(2, node->InputCount());
        return Replace(
            graph_->NewNode(&OPB2, node->InputAt(0), node->InputAt(1)));
    }
    return NoChange();
  }
  Graph* graph_;
};


// Replaces all "B" operators with "C" operators without creating new nodes.
class InPlaceBCReducer : public Reducer {
 public:
  virtual Reduction Reduce(Node* node) {
    switch (node->op()->opcode()) {
      case OPCODE_B0:
        CHECK_EQ(0, node->InputCount());
        node->set_op(&OPC0);
        return Replace(node);
      case OPCODE_B1:
        CHECK_EQ(1, node->InputCount());
        node->set_op(&OPC1);
        return Replace(node);
      case OPCODE_B2:
        CHECK_EQ(2, node->InputCount());
        node->set_op(&OPC2);
        return Replace(node);
    }
    return NoChange();
  }
};


// Wraps all "OPA0" nodes in "OPB1" operators by allocating new nodes.
class A0Wrapper FINAL : public Reducer {
 public:
  explicit A0Wrapper(Graph* graph) : graph_(graph) {}
  virtual Reduction Reduce(Node* node) OVERRIDE {
    switch (node->op()->opcode()) {
      case OPCODE_A0:
        CHECK_EQ(0, node->InputCount());
        return Replace(graph_->NewNode(&OPB1, node));
    }
    return NoChange();
  }
  Graph* graph_;
};


// Wraps all "OPB0" nodes in two "OPC1" operators by allocating new nodes.
class B0Wrapper FINAL : public Reducer {
 public:
  explicit B0Wrapper(Graph* graph) : graph_(graph) {}
  virtual Reduction Reduce(Node* node) OVERRIDE {
    switch (node->op()->opcode()) {
      case OPCODE_B0:
        CHECK_EQ(0, node->InputCount());
        return Replace(graph_->NewNode(&OPC1, graph_->NewNode(&OPC1, node)));
    }
    return NoChange();
  }
  Graph* graph_;
};


// Replaces all "OPA1" nodes with the first input.
class A1Forwarder : public Reducer {
  virtual Reduction Reduce(Node* node) {
    switch (node->op()->opcode()) {
      case OPCODE_A1:
        CHECK_EQ(1, node->InputCount());
        return Replace(node->InputAt(0));
    }
    return NoChange();
  }
};


// Replaces all "OPB1" nodes with the first input.
class B1Forwarder : public Reducer {
  virtual Reduction Reduce(Node* node) {
    switch (node->op()->opcode()) {
      case OPCODE_B1:
        CHECK_EQ(1, node->InputCount());
        return Replace(node->InputAt(0));
    }
    return NoChange();
  }
};


// Swaps the inputs to "OP2A" and "OP2B" nodes based on ids.
class AB2Sorter : public Reducer {
  virtual Reduction Reduce(Node* node) {
    switch (node->op()->opcode()) {
      case OPCODE_A2:
      case OPCODE_B2:
        CHECK_EQ(2, node->InputCount());
        Node* x = node->InputAt(0);
        Node* y = node->InputAt(1);
        if (x->id() > y->id()) {
          node->ReplaceInput(0, y);
          node->ReplaceInput(1, x);
          return Replace(node);
        }
    }
    return NoChange();
  }
};


// Simply records the nodes visited.
class ReducerRecorder : public Reducer {
 public:
  explicit ReducerRecorder(Zone* zone)
      : set(NodeSet::key_compare(), NodeSet::allocator_type(zone)) {}
  virtual Reduction Reduce(Node* node) {
    set.insert(node);
    return NoChange();
  }
  void CheckContains(Node* node) {
    CHECK_EQ(1, static_cast<int>(set.count(node)));
  }
  NodeSet set;
};


TEST(ReduceGraphFromEnd1) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* end = graph.NewNode(&OPA1, n1);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  ReducerRecorder recorder(graph.zone());
  reducer.AddReducer(&recorder);
  reducer.ReduceGraph();
  recorder.CheckContains(n1);
  recorder.CheckContains(end);
}


TEST(ReduceGraphFromEnd2) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* n2 = graph.NewNode(&OPA1, n1);
  Node* n3 = graph.NewNode(&OPA1, n1);
  Node* end = graph.NewNode(&OPA2, n2, n3);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  ReducerRecorder recorder(graph.zone());
  reducer.AddReducer(&recorder);
  reducer.ReduceGraph();
  recorder.CheckContains(n1);
  recorder.CheckContains(n2);
  recorder.CheckContains(n3);
  recorder.CheckContains(end);
}


TEST(ReduceInPlace1) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* end = graph.NewNode(&OPA1, n1);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  InPlaceABReducer r;
  reducer.AddReducer(&r);

  // Tests A* => B* with in-place updates.
  for (int i = 0; i < 3; i++) {
    int before = graph.NodeCount();
    reducer.ReduceGraph();
    CHECK_EQ(before, graph.NodeCount());
    CHECK_EQ(&OPB0, n1->op());
    CHECK_EQ(&OPB1, end->op());
    CHECK_EQ(n1, end->InputAt(0));
  }
}


TEST(ReduceInPlace2) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* n2 = graph.NewNode(&OPA1, n1);
  Node* n3 = graph.NewNode(&OPA1, n1);
  Node* end = graph.NewNode(&OPA2, n2, n3);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  InPlaceABReducer r;
  reducer.AddReducer(&r);

  // Tests A* => B* with in-place updates.
  for (int i = 0; i < 3; i++) {
    int before = graph.NodeCount();
    reducer.ReduceGraph();
    CHECK_EQ(before, graph.NodeCount());
    CHECK_EQ(&OPB0, n1->op());
    CHECK_EQ(&OPB1, n2->op());
    CHECK_EQ(n1, n2->InputAt(0));
    CHECK_EQ(&OPB1, n3->op());
    CHECK_EQ(n1, n3->InputAt(0));
    CHECK_EQ(&OPB2, end->op());
    CHECK_EQ(n2, end->InputAt(0));
    CHECK_EQ(n3, end->InputAt(1));
  }
}


TEST(ReduceNew1) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* n2 = graph.NewNode(&OPA1, n1);
  Node* n3 = graph.NewNode(&OPA1, n1);
  Node* end = graph.NewNode(&OPA2, n2, n3);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  NewABReducer r(&graph);
  reducer.AddReducer(&r);

  // Tests A* => B* while creating new nodes.
  for (int i = 0; i < 3; i++) {
    int before = graph.NodeCount();
    reducer.ReduceGraph();
    if (i == 0) {
      CHECK_NE(before, graph.NodeCount());
    } else {
      CHECK_EQ(before, graph.NodeCount());
    }
    Node* nend = graph.end();
    CHECK_NE(end, nend);  // end() should be updated too.

    Node* nn2 = nend->InputAt(0);
    Node* nn3 = nend->InputAt(1);
    Node* nn1 = nn2->InputAt(0);

    CHECK_EQ(nn1, nn3->InputAt(0));

    CHECK_EQ(&OPB0, nn1->op());
    CHECK_EQ(&OPB1, nn2->op());
    CHECK_EQ(&OPB1, nn3->op());
    CHECK_EQ(&OPB2, nend->op());
  }
}


TEST(Wrapping1) {
  GraphTester graph;

  Node* end = graph.NewNode(&OPA0);
  graph.SetEnd(end);
  CHECK_EQ(1, graph.NodeCount());

  GraphReducer reducer(&graph);
  A0Wrapper r(&graph);
  reducer.AddReducer(&r);

  reducer.ReduceGraph();
  CHECK_EQ(2, graph.NodeCount());

  Node* nend = graph.end();
  CHECK_NE(end, nend);
  CHECK_EQ(&OPB1, nend->op());
  CHECK_EQ(1, nend->InputCount());
  CHECK_EQ(end, nend->InputAt(0));
}


TEST(Wrapping2) {
  GraphTester graph;

  Node* end = graph.NewNode(&OPB0);
  graph.SetEnd(end);
  CHECK_EQ(1, graph.NodeCount());

  GraphReducer reducer(&graph);
  B0Wrapper r(&graph);
  reducer.AddReducer(&r);

  reducer.ReduceGraph();
  CHECK_EQ(3, graph.NodeCount());

  Node* nend = graph.end();
  CHECK_NE(end, nend);
  CHECK_EQ(&OPC1, nend->op());
  CHECK_EQ(1, nend->InputCount());

  Node* n1 = nend->InputAt(0);
  CHECK_NE(end, n1);
  CHECK_EQ(&OPC1, n1->op());
  CHECK_EQ(1, n1->InputCount());
  CHECK_EQ(end, n1->InputAt(0));
}


TEST(Forwarding1) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* end = graph.NewNode(&OPA1, n1);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  A1Forwarder r;
  reducer.AddReducer(&r);

  // Tests A1(x) => x
  for (int i = 0; i < 3; i++) {
    int before = graph.NodeCount();
    reducer.ReduceGraph();
    CHECK_EQ(before, graph.NodeCount());
    CHECK_EQ(&OPA0, n1->op());
    CHECK_EQ(n1, graph.end());
  }
}


TEST(Forwarding2) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* n2 = graph.NewNode(&OPA1, n1);
  Node* n3 = graph.NewNode(&OPA1, n1);
  Node* end = graph.NewNode(&OPA2, n2, n3);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  A1Forwarder r;
  reducer.AddReducer(&r);

  // Tests reducing A2(A1(x), A1(y)) => A2(x, y).
  for (int i = 0; i < 3; i++) {
    int before = graph.NodeCount();
    reducer.ReduceGraph();
    CHECK_EQ(before, graph.NodeCount());
    CHECK_EQ(&OPA0, n1->op());
    CHECK_EQ(n1, end->InputAt(0));
    CHECK_EQ(n1, end->InputAt(1));
    CHECK_EQ(&OPA2, end->op());
    CHECK_EQ(0, n2->UseCount());
    CHECK_EQ(0, n3->UseCount());
  }
}


TEST(Forwarding3) {
  // Tests reducing a chain of A1(A1(A1(A1(x)))) => x.
  for (int i = 0; i < 8; i++) {
    GraphTester graph;

    Node* n1 = graph.NewNode(&OPA0);
    Node* end = n1;
    for (int j = 0; j < i; j++) {
      end = graph.NewNode(&OPA1, end);
    }
    graph.SetEnd(end);

    GraphReducer reducer(&graph);
    A1Forwarder r;
    reducer.AddReducer(&r);

    for (int i = 0; i < 3; i++) {
      int before = graph.NodeCount();
      reducer.ReduceGraph();
      CHECK_EQ(before, graph.NodeCount());
      CHECK_EQ(&OPA0, n1->op());
      CHECK_EQ(n1, graph.end());
    }
  }
}


TEST(ReduceForward1) {
  GraphTester graph;

  Node* n1 = graph.NewNode(&OPA0);
  Node* n2 = graph.NewNode(&OPA1, n1);
  Node* n3 = graph.NewNode(&OPA1, n1);
  Node* end = graph.NewNode(&OPA2, n2, n3);
  graph.SetEnd(end);

  GraphReducer reducer(&graph);
  InPlaceABReducer r;
  B1Forwarder f;
  reducer.AddReducer(&r);
  reducer.AddReducer(&f);

  // Tests first reducing A => B, then B1(x) => x.
  for (int i = 0; i < 3; i++) {
    int before = graph.NodeCount();
    reducer.ReduceGraph();
    CHECK_EQ(before, graph.NodeCount());
    CHECK_EQ(&OPB0, n1->op());
    CHECK(n2->IsDead());
    CHECK_EQ(n1, end->InputAt(0));
    CHECK(n3->IsDead());
    CHECK_EQ(n1, end->InputAt(0));
    CHECK_EQ(&OPB2, end->op());
    CHECK_EQ(0, n2->UseCount());
    CHECK_EQ(0, n3->UseCount());
  }
}


TEST(Sorter1) {
  HandleAndZoneScope scope;
  AB2Sorter r;
  for (int i = 0; i < 6; i++) {
    GraphTester graph;

    Node* n1 = graph.NewNode(&OPA0);
    Node* n2 = graph.NewNode(&OPA1, n1);
    Node* n3 = graph.NewNode(&OPA1, n1);
    Node* end = NULL;  // Initialize to please the compiler.

    if (i == 0) end = graph.NewNode(&OPA2, n2, n3);
    if (i == 1) end = graph.NewNode(&OPA2, n3, n2);
    if (i == 2) end = graph.NewNode(&OPA2, n2, n1);
    if (i == 3) end = graph.NewNode(&OPA2, n1, n2);
    if (i == 4) end = graph.NewNode(&OPA2, n3, n1);
    if (i == 5) end = graph.NewNode(&OPA2, n1, n3);

    graph.SetEnd(end);

    GraphReducer reducer(&graph);
    reducer.AddReducer(&r);

    int before = graph.NodeCount();
    reducer.ReduceGraph();
    CHECK_EQ(before, graph.NodeCount());
    CHECK_EQ(&OPA0, n1->op());
    CHECK_EQ(&OPA1, n2->op());
    CHECK_EQ(&OPA1, n3->op());
    CHECK_EQ(&OPA2, end->op());
    CHECK_EQ(end, graph.end());
    CHECK(end->InputAt(0)->id() <= end->InputAt(1)->id());
  }
}


// Generate a node graph with the given permutations.
void GenDAG(Graph* graph, int* p3, int* p2, int* p1) {
  Node* level4 = graph->NewNode(&OPA0);
  Node* level3[] = {graph->NewNode(&OPA1, level4),
                    graph->NewNode(&OPA1, level4)};

  Node* level2[] = {graph->NewNode(&OPA1, level3[p3[0]]),
                    graph->NewNode(&OPA1, level3[p3[1]]),
                    graph->NewNode(&OPA1, level3[p3[0]]),
                    graph->NewNode(&OPA1, level3[p3[1]])};

  Node* level1[] = {graph->NewNode(&OPA2, level2[p2[0]], level2[p2[1]]),
                    graph->NewNode(&OPA2, level2[p2[2]], level2[p2[3]])};

  Node* end = graph->NewNode(&OPA2, level1[p1[0]], level1[p1[1]]);
  graph->SetEnd(end);
}


TEST(SortForwardReduce) {
  GraphTester graph;

  // Tests combined reductions on a series of DAGs.
  for (int j = 0; j < 2; j++) {
    int p3[] = {j, 1 - j};
    for (int m = 0; m < 2; m++) {
      int p1[] = {m, 1 - m};
      for (int k = 0; k < 24; k++) {  // All permutations of 0, 1, 2, 3
        int p2[] = {-1, -1, -1, -1};
        int n = k;
        for (int d = 4; d >= 1; d--) {  // Construct permutation.
          int p = n % d;
          for (int z = 0; z < 4; z++) {
            if (p2[z] == -1) {
              if (p == 0) p2[z] = d - 1;
              p--;
            }
          }
          n = n / d;
        }

        GenDAG(&graph, p3, p2, p1);

        GraphReducer reducer(&graph);
        AB2Sorter r1;
        A1Forwarder r2;
        InPlaceABReducer r3;
        reducer.AddReducer(&r1);
        reducer.AddReducer(&r2);
        reducer.AddReducer(&r3);

        reducer.ReduceGraph();

        Node* end = graph.end();
        CHECK_EQ(&OPB2, end->op());
        Node* n1 = end->InputAt(0);
        Node* n2 = end->InputAt(1);
        CHECK_NE(n1, n2);
        CHECK(n1->id() < n2->id());
        CHECK_EQ(&OPB2, n1->op());
        CHECK_EQ(&OPB2, n2->op());
        Node* n4 = n1->InputAt(0);
        CHECK_EQ(&OPB0, n4->op());
        CHECK_EQ(n4, n1->InputAt(1));
        CHECK_EQ(n4, n2->InputAt(0));
        CHECK_EQ(n4, n2->InputAt(1));
      }
    }
  }
}


TEST(Order) {
  // Test that the order of reducers doesn't matter, as they should be
  // rerun for changed nodes.
  for (int i = 0; i < 2; i++) {
    GraphTester graph;

    Node* n1 = graph.NewNode(&OPA0);
    Node* end = graph.NewNode(&OPA1, n1);
    graph.SetEnd(end);

    GraphReducer reducer(&graph);
    InPlaceABReducer abr;
    InPlaceBCReducer bcr;
    if (i == 0) {
      reducer.AddReducer(&abr);
      reducer.AddReducer(&bcr);
    } else {
      reducer.AddReducer(&bcr);
      reducer.AddReducer(&abr);
    }

    // Tests A* => C* with in-place updates.
    for (int i = 0; i < 3; i++) {
      int before = graph.NodeCount();
      reducer.ReduceGraph();
      CHECK_EQ(before, graph.NodeCount());
      CHECK_EQ(&OPC0, n1->op());
      CHECK_EQ(&OPC1, end->op());
      CHECK_EQ(n1, end->InputAt(0));
    }
  }
}
