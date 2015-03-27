// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class ControlReducerTest : public GraphTest {
 protected:
  void ReduceGraph() {
    JSOperatorBuilder javascript(zone());
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, &machine);
    ControlReducer::ReduceGraph(zone(), &jsgraph, common());
  }
};


TEST_F(ControlReducerTest, NonTerminatingLoop) {
  Node* loop = graph()->NewNode(common()->Loop(2), graph()->start());
  loop->AppendInput(graph()->zone(), loop);
  ReduceGraph();
  Capture<Node*> branch;
  EXPECT_THAT(
      graph()->end(),
      IsEnd(IsMerge(
          graph()->start(),
          IsReturn(IsUndefinedConstant(), graph()->start(),
                   IsIfFalse(
                       AllOf(CaptureEq(&branch),
                             IsBranch(IsAlways(),
                                      AllOf(loop, IsLoop(graph()->start(),
                                                         IsIfTrue(CaptureEq(
                                                             &branch)))))))))));
}


TEST_F(ControlReducerTest, NonTerminatingLoopWithEffectPhi) {
  Node* loop = graph()->NewNode(common()->Loop(2), graph()->start());
  loop->AppendInput(graph()->zone(), loop);
  Node* ephi = graph()->NewNode(common()->EffectPhi(2), graph()->start());
  ephi->AppendInput(graph()->zone(), ephi);
  ephi->AppendInput(graph()->zone(), loop);
  ReduceGraph();
  Capture<Node*> branch;
  EXPECT_THAT(
      graph()->end(),
      IsEnd(IsMerge(
          graph()->start(),
          IsReturn(IsUndefinedConstant(),
                   AllOf(ephi, IsEffectPhi(graph()->start(), ephi, loop)),
                   IsIfFalse(
                       AllOf(CaptureEq(&branch),
                             IsBranch(IsAlways(),
                                      AllOf(loop, IsLoop(graph()->start(),
                                                         IsIfTrue(CaptureEq(
                                                             &branch)))))))))));
}


TEST_F(ControlReducerTest, NonTerminatingLoopWithTwoEffectPhis) {
  Node* loop = graph()->NewNode(common()->Loop(2), graph()->start());
  loop->AppendInput(graph()->zone(), loop);
  Node* ephi1 = graph()->NewNode(common()->EffectPhi(2), graph()->start());
  ephi1->AppendInput(graph()->zone(), ephi1);
  ephi1->AppendInput(graph()->zone(), loop);
  Node* ephi2 = graph()->NewNode(common()->EffectPhi(2), graph()->start());
  ephi2->AppendInput(graph()->zone(), ephi2);
  ephi2->AppendInput(graph()->zone(), loop);
  ReduceGraph();
  Capture<Node*> branch;
  EXPECT_THAT(
      graph()->end(),
      IsEnd(IsMerge(
          graph()->start(),
          IsReturn(
              IsUndefinedConstant(),
              IsEffectSet(
                  AllOf(ephi1, IsEffectPhi(graph()->start(), ephi1, loop)),
                  AllOf(ephi2, IsEffectPhi(graph()->start(), ephi2, loop))),
              IsIfFalse(AllOf(
                  CaptureEq(&branch),
                  IsBranch(
                      IsAlways(),
                      AllOf(loop, IsLoop(graph()->start(),
                                         IsIfTrue(CaptureEq(&branch)))))))))));
}


TEST_F(ControlReducerTest, NonTerminatingLoopWithDeadEnd) {
  Node* loop = graph()->NewNode(common()->Loop(2), graph()->start());
  loop->AppendInput(graph()->zone(), loop);
  graph()->end()->ReplaceInput(0, graph()->NewNode(common()->Dead()));
  ReduceGraph();
  Capture<Node*> branch;
  EXPECT_THAT(
      graph()->end(),
      IsEnd(IsReturn(
          IsUndefinedConstant(), graph()->start(),
          IsIfFalse(AllOf(
              CaptureEq(&branch),
              IsBranch(IsAlways(),
                       AllOf(loop, IsLoop(graph()->start(),
                                          IsIfTrue(CaptureEq(&branch))))))))));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
