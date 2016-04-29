// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/loop-peeling.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::BitEq;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

struct While {
  Node* loop;
  Node* branch;
  Node* if_true;
  Node* exit;
};


// A helper for building branches.
struct Branch {
  Node* branch;
  Node* if_true;
  Node* if_false;
};


// A helper for building counters attached to loops.
struct Counter {
  Node* base;
  Node* inc;
  Node* phi;
  Node* add;
};


class LoopPeelingTest : public GraphTest {
 public:
  LoopPeelingTest() : GraphTest(1), machine_(zone()) {}
  ~LoopPeelingTest() override {}

 protected:
  MachineOperatorBuilder machine_;

  MachineOperatorBuilder* machine() { return &machine_; }

  LoopTree* GetLoopTree() {
    if (FLAG_trace_turbo_graph) {
      OFStream os(stdout);
      os << AsRPO(*graph());
    }
    Zone zone(isolate()->allocator());
    return LoopFinder::BuildLoopTree(graph(), &zone);
  }


  PeeledIteration* PeelOne() {
    LoopTree* loop_tree = GetLoopTree();
    LoopTree::Loop* loop = loop_tree->outer_loops()[0];
    EXPECT_TRUE(LoopPeeler::CanPeel(loop_tree, loop));
    return Peel(loop_tree, loop);
  }

  PeeledIteration* Peel(LoopTree* loop_tree, LoopTree::Loop* loop) {
    EXPECT_TRUE(LoopPeeler::CanPeel(loop_tree, loop));
    PeeledIteration* peeled =
        LoopPeeler::Peel(graph(), common(), loop_tree, loop, zone());
    if (FLAG_trace_turbo_graph) {
      OFStream os(stdout);
      os << AsRPO(*graph());
    }
    return peeled;
  }

  Node* InsertReturn(Node* val, Node* effect, Node* control) {
    Node* r = graph()->NewNode(common()->Return(), val, effect, control);
    graph()->SetEnd(r);
    return r;
  }

  Node* ExpectPeeled(Node* node, PeeledIteration* iter) {
    Node* p = iter->map(node);
    EXPECT_NE(node, p);
    return p;
  }

  void ExpectNotPeeled(Node* node, PeeledIteration* iter) {
    EXPECT_EQ(node, iter->map(node));
  }

  While NewWhile(Node* cond, Node* control = nullptr) {
    if (control == nullptr) control = start();
    Node* loop = graph()->NewNode(common()->Loop(2), control, control);
    Node* branch = graph()->NewNode(common()->Branch(), cond, loop);
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* exit = graph()->NewNode(common()->IfFalse(), branch);
    loop->ReplaceInput(1, if_true);
    return {loop, branch, if_true, exit};
  }

  void Chain(While* a, Node* control) { a->loop->ReplaceInput(0, control); }
  void Nest(While* a, While* b) {
    b->loop->ReplaceInput(1, a->exit);
    a->loop->ReplaceInput(0, b->if_true);
  }
  Node* NewPhi(While* w, Node* a, Node* b) {
    return graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2), a,
                            b, w->loop);
  }

  Branch NewBranch(Node* cond, Node* control = nullptr) {
    if (control == nullptr) control = start();
    Node* branch = graph()->NewNode(common()->Branch(), cond, control);
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    return {branch, if_true, if_false};
  }

  Counter NewCounter(While* w, int32_t b, int32_t k) {
    Node* base = Int32Constant(b);
    Node* inc = Int32Constant(k);
    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, 2), base, base, w->loop);
    Node* add = graph()->NewNode(machine()->Int32Add(), phi, inc);
    phi->ReplaceInput(1, add);
    return {base, inc, phi, add};
  }
};


TEST_F(LoopPeelingTest, SimpleLoop) {
  Node* p0 = Parameter(0);
  While w = NewWhile(p0);
  Node* r = InsertReturn(p0, start(), w.exit);

  PeeledIteration* peeled = PeelOne();

  Node* br1 = ExpectPeeled(w.branch, peeled);
  Node* if_true1 = ExpectPeeled(w.if_true, peeled);
  Node* if_false1 = ExpectPeeled(w.exit, peeled);

  EXPECT_THAT(br1, IsBranch(p0, start()));
  EXPECT_THAT(if_true1, IsIfTrue(br1));
  EXPECT_THAT(if_false1, IsIfFalse(br1));

  EXPECT_THAT(w.loop, IsLoop(if_true1, w.if_true));
  EXPECT_THAT(r, IsReturn(p0, start(), IsMerge(w.exit, if_false1)));
}


TEST_F(LoopPeelingTest, SimpleLoopWithCounter) {
  Node* p0 = Parameter(0);
  While w = NewWhile(p0);
  Counter c = NewCounter(&w, 0, 1);
  Node* r = InsertReturn(c.phi, start(), w.exit);

  PeeledIteration* peeled = PeelOne();

  Node* br1 = ExpectPeeled(w.branch, peeled);
  Node* if_true1 = ExpectPeeled(w.if_true, peeled);
  Node* if_false1 = ExpectPeeled(w.exit, peeled);

  EXPECT_THAT(br1, IsBranch(p0, start()));
  EXPECT_THAT(if_true1, IsIfTrue(br1));
  EXPECT_THAT(if_false1, IsIfFalse(br1));
  EXPECT_THAT(w.loop, IsLoop(if_true1, w.if_true));

  EXPECT_THAT(peeled->map(c.add), IsInt32Add(c.base, c.inc));

  Capture<Node*> merge;
  EXPECT_THAT(
      r, IsReturn(IsPhi(MachineRepresentation::kTagged, c.phi, c.base,
                        AllOf(CaptureEq(&merge), IsMerge(w.exit, if_false1))),
                  start(), CaptureEq(&merge)));
}


TEST_F(LoopPeelingTest, SimpleNestedLoopWithCounter_peel_outer) {
  Node* p0 = Parameter(0);
  While outer = NewWhile(p0);
  While inner = NewWhile(p0);
  Nest(&inner, &outer);

  Counter c = NewCounter(&outer, 0, 1);
  Node* r = InsertReturn(c.phi, start(), outer.exit);

  PeeledIteration* peeled = PeelOne();

  Node* bro = ExpectPeeled(outer.branch, peeled);
  Node* if_trueo = ExpectPeeled(outer.if_true, peeled);
  Node* if_falseo = ExpectPeeled(outer.exit, peeled);

  EXPECT_THAT(bro, IsBranch(p0, start()));
  EXPECT_THAT(if_trueo, IsIfTrue(bro));
  EXPECT_THAT(if_falseo, IsIfFalse(bro));

  Node* bri = ExpectPeeled(inner.branch, peeled);
  Node* if_truei = ExpectPeeled(inner.if_true, peeled);
  Node* if_falsei = ExpectPeeled(inner.exit, peeled);

  EXPECT_THAT(bri, IsBranch(p0, ExpectPeeled(inner.loop, peeled)));
  EXPECT_THAT(if_truei, IsIfTrue(bri));
  EXPECT_THAT(if_falsei, IsIfFalse(bri));

  EXPECT_THAT(outer.loop, IsLoop(if_falsei, inner.exit));
  EXPECT_THAT(peeled->map(c.add), IsInt32Add(c.base, c.inc));

  Capture<Node*> merge;
  EXPECT_THAT(
      r,
      IsReturn(IsPhi(MachineRepresentation::kTagged, c.phi, c.base,
                     AllOf(CaptureEq(&merge), IsMerge(outer.exit, if_falseo))),
               start(), CaptureEq(&merge)));
}


TEST_F(LoopPeelingTest, SimpleNestedLoopWithCounter_peel_inner) {
  Node* p0 = Parameter(0);
  While outer = NewWhile(p0);
  While inner = NewWhile(p0);
  Nest(&inner, &outer);

  Counter c = NewCounter(&outer, 0, 1);
  Node* r = InsertReturn(c.phi, start(), outer.exit);

  LoopTree* loop_tree = GetLoopTree();
  LoopTree::Loop* loop = loop_tree->ContainingLoop(inner.loop);
  EXPECT_NE(nullptr, loop);
  EXPECT_EQ(1u, loop->depth());

  PeeledIteration* peeled = Peel(loop_tree, loop);

  ExpectNotPeeled(outer.loop, peeled);
  ExpectNotPeeled(outer.branch, peeled);
  ExpectNotPeeled(outer.if_true, peeled);
  ExpectNotPeeled(outer.exit, peeled);

  Node* bri = ExpectPeeled(inner.branch, peeled);
  Node* if_truei = ExpectPeeled(inner.if_true, peeled);
  Node* if_falsei = ExpectPeeled(inner.exit, peeled);

  EXPECT_THAT(bri, IsBranch(p0, ExpectPeeled(inner.loop, peeled)));
  EXPECT_THAT(if_truei, IsIfTrue(bri));
  EXPECT_THAT(if_falsei, IsIfFalse(bri));

  EXPECT_THAT(outer.loop, IsLoop(start(), IsMerge(inner.exit, if_falsei)));
  ExpectNotPeeled(c.add, peeled);

  EXPECT_THAT(r, IsReturn(c.phi, start(), outer.exit));
}


TEST_F(LoopPeelingTest, SimpleInnerCounter_peel_inner) {
  Node* p0 = Parameter(0);
  While outer = NewWhile(p0);
  While inner = NewWhile(p0);
  Nest(&inner, &outer);
  Counter c = NewCounter(&inner, 0, 1);
  Node* phi = NewPhi(&outer, Int32Constant(11), c.phi);

  Node* r = InsertReturn(phi, start(), outer.exit);

  LoopTree* loop_tree = GetLoopTree();
  LoopTree::Loop* loop = loop_tree->ContainingLoop(inner.loop);
  EXPECT_NE(nullptr, loop);
  EXPECT_EQ(1u, loop->depth());

  PeeledIteration* peeled = Peel(loop_tree, loop);

  ExpectNotPeeled(outer.loop, peeled);
  ExpectNotPeeled(outer.branch, peeled);
  ExpectNotPeeled(outer.if_true, peeled);
  ExpectNotPeeled(outer.exit, peeled);

  Node* bri = ExpectPeeled(inner.branch, peeled);
  Node* if_truei = ExpectPeeled(inner.if_true, peeled);
  Node* if_falsei = ExpectPeeled(inner.exit, peeled);

  EXPECT_THAT(bri, IsBranch(p0, ExpectPeeled(inner.loop, peeled)));
  EXPECT_THAT(if_truei, IsIfTrue(bri));
  EXPECT_THAT(if_falsei, IsIfFalse(bri));

  EXPECT_THAT(outer.loop, IsLoop(start(), IsMerge(inner.exit, if_falsei)));
  EXPECT_THAT(peeled->map(c.add), IsInt32Add(c.base, c.inc));

  Node* back = phi->InputAt(1);
  EXPECT_THAT(back, IsPhi(MachineRepresentation::kTagged, c.phi, c.base,
                          IsMerge(inner.exit, if_falsei)));

  EXPECT_THAT(phi, IsPhi(MachineRepresentation::kTagged, IsInt32Constant(11),
                         back, outer.loop));

  EXPECT_THAT(r, IsReturn(phi, start(), outer.exit));
}


TEST_F(LoopPeelingTest, TwoBackedgeLoop) {
  Node* p0 = Parameter(0);
  Node* loop = graph()->NewNode(common()->Loop(3), start(), start(), start());
  Branch b1 = NewBranch(p0, loop);
  Branch b2 = NewBranch(p0, b1.if_true);

  loop->ReplaceInput(1, b2.if_true);
  loop->ReplaceInput(2, b2.if_false);

  Node* r = InsertReturn(p0, start(), b1.if_false);

  PeeledIteration* peeled = PeelOne();

  Node* b1b = ExpectPeeled(b1.branch, peeled);
  Node* b1t = ExpectPeeled(b1.if_true, peeled);
  Node* b1f = ExpectPeeled(b1.if_false, peeled);

  EXPECT_THAT(b1b, IsBranch(p0, start()));
  EXPECT_THAT(ExpectPeeled(b1.if_true, peeled), IsIfTrue(b1b));
  EXPECT_THAT(b1f, IsIfFalse(b1b));

  Node* b2b = ExpectPeeled(b2.branch, peeled);
  Node* b2t = ExpectPeeled(b2.if_true, peeled);
  Node* b2f = ExpectPeeled(b2.if_false, peeled);

  EXPECT_THAT(b2b, IsBranch(p0, b1t));
  EXPECT_THAT(b2t, IsIfTrue(b2b));
  EXPECT_THAT(b2f, IsIfFalse(b2b));

  EXPECT_THAT(loop, IsLoop(IsMerge(b2t, b2f), b2.if_true, b2.if_false));
  EXPECT_THAT(r, IsReturn(p0, start(), IsMerge(b1.if_false, b1f)));
}


TEST_F(LoopPeelingTest, TwoBackedgeLoopWithPhi) {
  Node* p0 = Parameter(0);
  Node* loop = graph()->NewNode(common()->Loop(3), start(), start(), start());
  Branch b1 = NewBranch(p0, loop);
  Branch b2 = NewBranch(p0, b1.if_true);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 3),
                               Int32Constant(0), Int32Constant(1),
                               Int32Constant(2), loop);

  loop->ReplaceInput(1, b2.if_true);
  loop->ReplaceInput(2, b2.if_false);

  Node* r = InsertReturn(phi, start(), b1.if_false);

  PeeledIteration* peeled = PeelOne();

  Node* b1b = ExpectPeeled(b1.branch, peeled);
  Node* b1t = ExpectPeeled(b1.if_true, peeled);
  Node* b1f = ExpectPeeled(b1.if_false, peeled);

  EXPECT_THAT(b1b, IsBranch(p0, start()));
  EXPECT_THAT(ExpectPeeled(b1.if_true, peeled), IsIfTrue(b1b));
  EXPECT_THAT(b1f, IsIfFalse(b1b));

  Node* b2b = ExpectPeeled(b2.branch, peeled);
  Node* b2t = ExpectPeeled(b2.if_true, peeled);
  Node* b2f = ExpectPeeled(b2.if_false, peeled);

  EXPECT_THAT(b2b, IsBranch(p0, b1t));
  EXPECT_THAT(b2t, IsIfTrue(b2b));
  EXPECT_THAT(b2f, IsIfFalse(b2b));

  EXPECT_THAT(loop, IsLoop(IsMerge(b2t, b2f), b2.if_true, b2.if_false));

  EXPECT_THAT(phi,
              IsPhi(MachineRepresentation::kTagged,
                    IsPhi(MachineRepresentation::kTagged, IsInt32Constant(1),
                          IsInt32Constant(2), IsMerge(b2t, b2f)),
                    IsInt32Constant(1), IsInt32Constant(2), loop));

  Capture<Node*> merge;
  EXPECT_THAT(
      r, IsReturn(IsPhi(MachineRepresentation::kTagged, phi, IsInt32Constant(0),
                        AllOf(CaptureEq(&merge), IsMerge(b1.if_false, b1f))),
                  start(), CaptureEq(&merge)));
}


TEST_F(LoopPeelingTest, TwoBackedgeLoopWithCounter) {
  Node* p0 = Parameter(0);
  Node* loop = graph()->NewNode(common()->Loop(3), start(), start(), start());
  Branch b1 = NewBranch(p0, loop);
  Branch b2 = NewBranch(p0, b1.if_true);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 3),
                               Int32Constant(0), Int32Constant(1),
                               Int32Constant(2), loop);

  phi->ReplaceInput(
      1, graph()->NewNode(machine()->Int32Add(), phi, Int32Constant(1)));
  phi->ReplaceInput(
      2, graph()->NewNode(machine()->Int32Add(), phi, Int32Constant(2)));

  loop->ReplaceInput(1, b2.if_true);
  loop->ReplaceInput(2, b2.if_false);

  Node* r = InsertReturn(phi, start(), b1.if_false);

  PeeledIteration* peeled = PeelOne();

  Node* b1b = ExpectPeeled(b1.branch, peeled);
  Node* b1t = ExpectPeeled(b1.if_true, peeled);
  Node* b1f = ExpectPeeled(b1.if_false, peeled);

  EXPECT_THAT(b1b, IsBranch(p0, start()));
  EXPECT_THAT(ExpectPeeled(b1.if_true, peeled), IsIfTrue(b1b));
  EXPECT_THAT(b1f, IsIfFalse(b1b));

  Node* b2b = ExpectPeeled(b2.branch, peeled);
  Node* b2t = ExpectPeeled(b2.if_true, peeled);
  Node* b2f = ExpectPeeled(b2.if_false, peeled);

  EXPECT_THAT(b2b, IsBranch(p0, b1t));
  EXPECT_THAT(b2t, IsIfTrue(b2b));
  EXPECT_THAT(b2f, IsIfFalse(b2b));

  Capture<Node*> entry;
  EXPECT_THAT(loop, IsLoop(AllOf(CaptureEq(&entry), IsMerge(b2t, b2f)),
                           b2.if_true, b2.if_false));

  Node* eval = phi->InputAt(0);

  EXPECT_THAT(eval, IsPhi(MachineRepresentation::kTagged,
                          IsInt32Add(IsInt32Constant(0), IsInt32Constant(1)),
                          IsInt32Add(IsInt32Constant(0), IsInt32Constant(2)),
                          CaptureEq(&entry)));

  EXPECT_THAT(phi, IsPhi(MachineRepresentation::kTagged, eval,
                         IsInt32Add(phi, IsInt32Constant(1)),
                         IsInt32Add(phi, IsInt32Constant(2)), loop));

  Capture<Node*> merge;
  EXPECT_THAT(
      r, IsReturn(IsPhi(MachineRepresentation::kTagged, phi, IsInt32Constant(0),
                        AllOf(CaptureEq(&merge), IsMerge(b1.if_false, b1f))),
                  start(), CaptureEq(&merge)));
}


TEST_F(LoopPeelingTest, TwoExitLoop_nope) {
  Node* p0 = Parameter(0);
  Node* loop = graph()->NewNode(common()->Loop(2), start(), start());
  Branch b1 = NewBranch(p0, loop);
  Branch b2 = NewBranch(p0, b1.if_true);

  loop->ReplaceInput(1, b2.if_true);
  Node* merge = graph()->NewNode(common()->Merge(2), b1.if_false, b2.if_false);
  InsertReturn(p0, start(), merge);

  {
    LoopTree* loop_tree = GetLoopTree();
    LoopTree::Loop* loop = loop_tree->outer_loops()[0];
    EXPECT_FALSE(LoopPeeler::CanPeel(loop_tree, loop));
  }
}


const Operator kMockCall(IrOpcode::kCall, Operator::kNoProperties, "MockCall",
                         0, 0, 1, 1, 1, 2);


TEST_F(LoopPeelingTest, TwoExitLoopWithCall_nope) {
  Node* p0 = Parameter(0);
  Node* loop = graph()->NewNode(common()->Loop(2), start(), start());
  Branch b1 = NewBranch(p0, loop);

  Node* call = graph()->NewNode(&kMockCall, b1.if_true);
  Node* if_success = graph()->NewNode(common()->IfSuccess(), call);
  Node* if_exception = graph()->NewNode(
      common()->IfException(IfExceptionHint::kLocallyUncaught), call, call);

  loop->ReplaceInput(1, if_success);
  Node* merge = graph()->NewNode(common()->Merge(2), b1.if_false, if_exception);
  InsertReturn(p0, start(), merge);

  {
    LoopTree* loop_tree = GetLoopTree();
    LoopTree::Loop* loop = loop_tree->outer_loops()[0];
    EXPECT_FALSE(LoopPeeler::CanPeel(loop_tree, loop));
  }
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
