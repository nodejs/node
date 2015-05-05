// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-flow-optimizer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class ControlFlowOptimizerTest : public GraphTest {
 public:
  explicit ControlFlowOptimizerTest(int num_parameters = 3)
      : GraphTest(num_parameters),
        machine_(zone()),
        javascript_(zone()),
        jsgraph_(isolate(), graph(), common(), javascript(), machine()) {}
  ~ControlFlowOptimizerTest() OVERRIDE {}

 protected:
  void Optimize() {
    ControlFlowOptimizer optimizer(jsgraph(), zone());
    optimizer.Optimize();
  }

  Node* EmptyFrameState() { return jsgraph()->EmptyFrameState(); }

  JSGraph* jsgraph() { return &jsgraph_; }
  JSOperatorBuilder* javascript() { return &javascript_; }
  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
  JSGraph jsgraph_;
};


TEST_F(ControlFlowOptimizerTest, BuildSwitch1) {
  Node* index = Parameter(0);
  Node* branch0 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(0)),
      start());
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* branch1 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(1)),
      if_false0);
  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
  Node* merge =
      graph()->NewNode(common()->Merge(3), if_true0, if_true1, if_false1);
  graph()->SetEnd(graph()->NewNode(common()->End(), merge));
  Optimize();
  Capture<Node*> switch_capture;
  EXPECT_THAT(end(),
              IsEnd(IsMerge(IsIfValue(0, CaptureEq(&switch_capture)),
                            IsIfValue(1, CaptureEq(&switch_capture)),
                            IsIfDefault(AllOf(CaptureEq(&switch_capture),
                                              IsSwitch(index, start()))))));
}


TEST_F(ControlFlowOptimizerTest, BuildSwitch2) {
  Node* input = Parameter(0);
  Node* context = Parameter(1);
  Node* index = FLAG_turbo_deoptimization
                    ? graph()->NewNode(javascript()->ToNumber(), input, context,
                                       EmptyFrameState(), start(), start())
                    : graph()->NewNode(javascript()->ToNumber(), input, context,
                                       start(), start());
  Node* if_success = graph()->NewNode(common()->IfSuccess(), index);
  Node* branch0 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(0)),
      if_success);
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* branch1 = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32Equal(), index, Int32Constant(1)),
      if_false0);
  Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
  Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
  Node* merge =
      graph()->NewNode(common()->Merge(3), if_true0, if_true1, if_false1);
  graph()->SetEnd(graph()->NewNode(common()->End(), merge));
  Optimize();
  Capture<Node*> switch_capture;
  EXPECT_THAT(
      end(),
      IsEnd(IsMerge(IsIfValue(0, CaptureEq(&switch_capture)),
                    IsIfValue(1, CaptureEq(&switch_capture)),
                    IsIfDefault(AllOf(CaptureEq(&switch_capture),
                                      IsSwitch(index, IsIfSuccess(index)))))));
}


TEST_F(ControlFlowOptimizerTest, CloneBranch) {
  Node* cond0 = Parameter(0);
  Node* cond1 = Parameter(1);
  Node* cond2 = Parameter(2);
  Node* branch0 = graph()->NewNode(common()->Branch(), cond0, start());
  Node* control1 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* control2 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* merge0 = graph()->NewNode(common()->Merge(2), control1, control2);
  Node* phi0 =
      graph()->NewNode(common()->Phi(kRepBit, 2), cond1, cond2, merge0);
  Node* branch = graph()->NewNode(common()->Branch(), phi0, merge0);
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  graph()->SetEnd(graph()->NewNode(common()->End(), merge));
  Optimize();
  Capture<Node*> branch1_capture, branch2_capture;
  EXPECT_THAT(
      end(),
      IsEnd(IsMerge(IsMerge(IsIfTrue(CaptureEq(&branch1_capture)),
                            IsIfTrue(CaptureEq(&branch2_capture))),
                    IsMerge(IsIfFalse(AllOf(CaptureEq(&branch1_capture),
                                            IsBranch(cond1, control1))),
                            IsIfFalse(AllOf(CaptureEq(&branch2_capture),
                                            IsBranch(cond2, control2)))))));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
