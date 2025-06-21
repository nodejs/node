// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-trimmer.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "testing/gmock-support.h"

using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace v8 {
namespace internal {
namespace compiler {

class GraphTrimmerTest : public GraphTest {
 public:
  GraphTrimmerTest() : GraphTest(1) {}

 protected:
  void TrimGraph(Node* root) {
    Node* const roots[1] = {root};
    GraphTrimmer trimmer(zone(), graph());
    trimmer.TrimGraph(&roots[0], &roots[arraysize(roots)]);
  }
  void TrimGraph() {
    GraphTrimmer trimmer(zone(), graph());
    trimmer.TrimGraph();
  }
};


namespace {

const Operator kDead0(IrOpcode::kDead, Operator::kNoProperties, "Dead0", 0, 0,
                      1, 0, 0, 0);
const Operator kLive0(IrOpcode::kDead, Operator::kNoProperties, "Live0", 0, 0,
                      1, 0, 0, 1);

}  // namespace


TEST_F(GraphTrimmerTest, Empty) {
  Node* const start = graph()->NewNode(common()->Start(0));
  Node* const end = graph()->NewNode(common()->End(1), start);
  graph()->SetStart(start);
  graph()->SetEnd(end);
  TrimGraph();
  EXPECT_EQ(end, graph()->end());
  EXPECT_EQ(start, graph()->start());
  EXPECT_EQ(start, end->InputAt(0));
}


TEST_F(GraphTrimmerTest, DeadUseOfStart) {
  Node* const dead0 = graph()->NewNode(&kDead0, graph()->start());
  graph()->SetEnd(graph()->NewNode(common()->End(1), graph()->start()));
  TrimGraph();
  EXPECT_THAT(dead0->inputs(), ElementsAre(nullptr));
  EXPECT_THAT(graph()->start()->uses(), ElementsAre(graph()->end()));
}


TEST_F(GraphTrimmerTest, DeadAndLiveUsesOfStart) {
  Node* const dead0 = graph()->NewNode(&kDead0, graph()->start());
  Node* const live0 = graph()->NewNode(&kLive0, graph()->start());
  graph()->SetEnd(graph()->NewNode(common()->End(1), live0));
  TrimGraph();
  EXPECT_THAT(dead0->inputs(), ElementsAre(nullptr));
  EXPECT_THAT(graph()->start()->uses(), ElementsAre(live0));
  EXPECT_THAT(live0->uses(), ElementsAre(graph()->end()));
}


TEST_F(GraphTrimmerTest, Roots) {
  Node* const live0 = graph()->NewNode(&kLive0, graph()->start());
  Node* const live1 = graph()->NewNode(&kLive0, graph()->start());
  graph()->SetEnd(graph()->NewNode(common()->End(1), live0));
  TrimGraph(live1);
  EXPECT_THAT(graph()->start()->uses(), UnorderedElementsAre(live0, live1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
