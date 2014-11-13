// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/bits.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties-inl.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static const size_t kNumLeafs = 4;

// TODO(titzer): convert this whole file into unit tests.

static int CheckInputs(Node* node, Node* i0 = NULL, Node* i1 = NULL,
                       Node* i2 = NULL) {
  int count = 3;
  if (i2 == NULL) count = 2;
  if (i1 == NULL) count = 1;
  if (i0 == NULL) count = 0;
  CHECK_EQ(count, node->InputCount());
  if (i0 != NULL) CHECK_EQ(i0, node->InputAt(0));
  if (i1 != NULL) CHECK_EQ(i1, node->InputAt(1));
  if (i2 != NULL) CHECK_EQ(i2, node->InputAt(2));
  return count;
}


static int CheckMerge(Node* node, Node* i0 = NULL, Node* i1 = NULL,
                      Node* i2 = NULL) {
  CHECK_EQ(IrOpcode::kMerge, node->opcode());
  int count = CheckInputs(node, i0, i1, i2);
  CHECK_EQ(count, node->op()->ControlInputCount());
  return count;
}


static int CheckLoop(Node* node, Node* i0 = NULL, Node* i1 = NULL,
                     Node* i2 = NULL) {
  CHECK_EQ(IrOpcode::kLoop, node->opcode());
  int count = CheckInputs(node, i0, i1, i2);
  CHECK_EQ(count, node->op()->ControlInputCount());
  return count;
}


bool IsUsedBy(Node* a, Node* b) {
  for (UseIter i = a->uses().begin(); i != a->uses().end(); ++i) {
    if (b == *i) return true;
  }
  return false;
}


// A helper for all tests dealing with ControlTester.
class ControlReducerTester : HandleAndZoneScope {
 public:
  ControlReducerTester()
      : isolate(main_isolate()),
        common(main_zone()),
        graph(main_zone()),
        jsgraph(&graph, &common, NULL, NULL),
        start(graph.NewNode(common.Start(1))),
        end(graph.NewNode(common.End(), start)),
        p0(graph.NewNode(common.Parameter(0), start)),
        zero(jsgraph.Int32Constant(0)),
        one(jsgraph.OneConstant()),
        half(jsgraph.Constant(0.5)),
        self(graph.NewNode(common.Int32Constant(0xaabbccdd))),
        dead(graph.NewNode(common.Dead())) {
    graph.SetEnd(end);
    graph.SetStart(start);
    leaf[0] = zero;
    leaf[1] = one;
    leaf[2] = half;
    leaf[3] = p0;
  }

  Isolate* isolate;
  CommonOperatorBuilder common;
  Graph graph;
  JSGraph jsgraph;
  Node* start;
  Node* end;
  Node* p0;
  Node* zero;
  Node* one;
  Node* half;
  Node* self;
  Node* dead;
  Node* leaf[kNumLeafs];

  Node* Phi(Node* a) {
    return SetSelfReferences(graph.NewNode(op(1, false), a, start));
  }

  Node* Phi(Node* a, Node* b) {
    return SetSelfReferences(graph.NewNode(op(2, false), a, b, start));
  }

  Node* Phi(Node* a, Node* b, Node* c) {
    return SetSelfReferences(graph.NewNode(op(3, false), a, b, c, start));
  }

  Node* Phi(Node* a, Node* b, Node* c, Node* d) {
    return SetSelfReferences(graph.NewNode(op(4, false), a, b, c, d, start));
  }

  Node* EffectPhi(Node* a) {
    return SetSelfReferences(graph.NewNode(op(1, true), a, start));
  }

  Node* EffectPhi(Node* a, Node* b) {
    return SetSelfReferences(graph.NewNode(op(2, true), a, b, start));
  }

  Node* EffectPhi(Node* a, Node* b, Node* c) {
    return SetSelfReferences(graph.NewNode(op(3, true), a, b, c, start));
  }

  Node* EffectPhi(Node* a, Node* b, Node* c, Node* d) {
    return SetSelfReferences(graph.NewNode(op(4, true), a, b, c, d, start));
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

  const Operator* op(int count, bool effect) {
    return effect ? common.EffectPhi(count) : common.Phi(kMachAnyTagged, count);
  }

  void Trim() { ControlReducer::TrimGraph(main_zone(), &jsgraph); }

  void ReduceGraph() {
    ControlReducer::ReduceGraph(main_zone(), &jsgraph, &common);
  }

  // Checks one-step reduction of a phi.
  void ReducePhi(Node* expect, Node* phi) {
    Node* result = ControlReducer::ReducePhiForTesting(&jsgraph, &common, phi);
    CHECK_EQ(expect, result);
    ReducePhiIterative(expect, phi);  // iterative should give the same result.
  }

  void ReducePhiIterative(Node* expect, Node* phi) {
    p0->ReplaceInput(0, start);  // hack: parameters may be trimmed.
    Node* ret = graph.NewNode(common.Return(), phi, start, start);
    Node* end = graph.NewNode(common.End(), ret);
    graph.SetEnd(end);
    ControlReducer::ReduceGraph(main_zone(), &jsgraph, &common);
    CheckInputs(end, ret);
    CheckInputs(ret, expect, start, start);
  }

  void ReduceMerge(Node* expect, Node* merge) {
    Node* result =
        ControlReducer::ReduceMergeForTesting(&jsgraph, &common, merge);
    CHECK_EQ(expect, result);
  }

  void ReduceMergeIterative(Node* expect, Node* merge) {
    p0->ReplaceInput(0, start);  // hack: parameters may be trimmed.
    Node* end = graph.NewNode(common.End(), merge);
    graph.SetEnd(end);
    ReduceGraph();
    CheckInputs(end, expect);
  }

  void ReduceBranch(Node* expect, Node* branch) {
    Node* result =
        ControlReducer::ReduceBranchForTesting(&jsgraph, &common, branch);
    CHECK_EQ(expect, result);
  }

  Node* Return(Node* val, Node* effect, Node* control) {
    Node* ret = graph.NewNode(common.Return(), val, effect, control);
    end->ReplaceInput(0, ret);
    return ret;
  }
};


TEST(Trim1_live) {
  ControlReducerTester T;
  CHECK(IsUsedBy(T.start, T.p0));
  T.graph.SetEnd(T.p0);
  T.Trim();
  CHECK(IsUsedBy(T.start, T.p0));
  CheckInputs(T.p0, T.start);
}


TEST(Trim1_dead) {
  ControlReducerTester T;
  CHECK(IsUsedBy(T.start, T.p0));
  T.Trim();
  CHECK(!IsUsedBy(T.start, T.p0));
  CHECK_EQ(NULL, T.p0->InputAt(0));
}


TEST(Trim2_live) {
  ControlReducerTester T;
  Node* phi =
      T.graph.NewNode(T.common.Phi(kMachAnyTagged, 2), T.one, T.half, T.start);
  CHECK(IsUsedBy(T.one, phi));
  CHECK(IsUsedBy(T.half, phi));
  CHECK(IsUsedBy(T.start, phi));
  T.graph.SetEnd(phi);
  T.Trim();
  CHECK(IsUsedBy(T.one, phi));
  CHECK(IsUsedBy(T.half, phi));
  CHECK(IsUsedBy(T.start, phi));
  CheckInputs(phi, T.one, T.half, T.start);
}


TEST(Trim2_dead) {
  ControlReducerTester T;
  Node* phi =
      T.graph.NewNode(T.common.Phi(kMachAnyTagged, 2), T.one, T.half, T.start);
  CHECK(IsUsedBy(T.one, phi));
  CHECK(IsUsedBy(T.half, phi));
  CHECK(IsUsedBy(T.start, phi));
  T.Trim();
  CHECK(!IsUsedBy(T.one, phi));
  CHECK(!IsUsedBy(T.half, phi));
  CHECK(!IsUsedBy(T.start, phi));
  CHECK_EQ(NULL, phi->InputAt(0));
  CHECK_EQ(NULL, phi->InputAt(1));
  CHECK_EQ(NULL, phi->InputAt(2));
}


TEST(Trim_chain1) {
  ControlReducerTester T;
  const int kDepth = 15;
  Node* live[kDepth];
  Node* dead[kDepth];
  Node* end = T.start;
  for (int i = 0; i < kDepth; i++) {
    live[i] = end = T.graph.NewNode(T.common.Merge(1), end);
    dead[i] = T.graph.NewNode(T.common.Merge(1), end);
  }
  // end         -> live[last] ->  live[last-1] -> ... -> start
  //     dead[last] ^ dead[last-1] ^ ...                  ^
  T.graph.SetEnd(end);
  T.Trim();
  for (int i = 0; i < kDepth; i++) {
    CHECK(!IsUsedBy(live[i], dead[i]));
    CHECK_EQ(NULL, dead[i]->InputAt(0));
    CHECK_EQ(i == 0 ? T.start : live[i - 1], live[i]->InputAt(0));
  }
}


TEST(Trim_chain2) {
  ControlReducerTester T;
  const int kDepth = 15;
  Node* live[kDepth];
  Node* dead[kDepth];
  Node* l = T.start;
  Node* d = T.start;
  for (int i = 0; i < kDepth; i++) {
    live[i] = l = T.graph.NewNode(T.common.Merge(1), l);
    dead[i] = d = T.graph.NewNode(T.common.Merge(1), d);
  }
  // end -> live[last] -> live[last-1] -> ... -> start
  //        dead[last] -> dead[last-1] -> ... -> start
  T.graph.SetEnd(l);
  T.Trim();
  CHECK(!IsUsedBy(T.start, dead[0]));
  for (int i = 0; i < kDepth; i++) {
    CHECK_EQ(i == 0 ? NULL : dead[i - 1], dead[i]->InputAt(0));
    CHECK_EQ(i == 0 ? T.start : live[i - 1], live[i]->InputAt(0));
  }
}


TEST(Trim_cycle1) {
  ControlReducerTester T;
  Node* loop = T.graph.NewNode(T.common.Loop(1), T.start, T.start);
  loop->ReplaceInput(1, loop);
  Node* end = T.graph.NewNode(T.common.End(), loop);
  T.graph.SetEnd(end);

  CHECK(IsUsedBy(T.start, loop));
  CHECK(IsUsedBy(loop, end));
  CHECK(IsUsedBy(loop, loop));

  T.Trim();

  // nothing should have happened to the loop itself.
  CHECK(IsUsedBy(T.start, loop));
  CHECK(IsUsedBy(loop, end));
  CHECK(IsUsedBy(loop, loop));
  CheckInputs(loop, T.start, loop);
  CheckInputs(end, loop);
}


TEST(Trim_cycle2) {
  ControlReducerTester T;
  Node* loop = T.graph.NewNode(T.common.Loop(2), T.start, T.start);
  loop->ReplaceInput(1, loop);
  Node* end = T.graph.NewNode(T.common.End(), loop);
  Node* phi =
      T.graph.NewNode(T.common.Phi(kMachAnyTagged, 2), T.one, T.half, loop);
  T.graph.SetEnd(end);

  CHECK(IsUsedBy(T.start, loop));
  CHECK(IsUsedBy(loop, end));
  CHECK(IsUsedBy(loop, loop));
  CHECK(IsUsedBy(loop, phi));
  CHECK(IsUsedBy(T.one, phi));
  CHECK(IsUsedBy(T.half, phi));

  T.Trim();

  // nothing should have happened to the loop itself.
  CHECK(IsUsedBy(T.start, loop));
  CHECK(IsUsedBy(loop, end));
  CHECK(IsUsedBy(loop, loop));
  CheckInputs(loop, T.start, loop);
  CheckInputs(end, loop);

  // phi should have been trimmed away.
  CHECK(!IsUsedBy(loop, phi));
  CHECK(!IsUsedBy(T.one, phi));
  CHECK(!IsUsedBy(T.half, phi));
  CHECK_EQ(NULL, phi->InputAt(0));
  CHECK_EQ(NULL, phi->InputAt(1));
  CHECK_EQ(NULL, phi->InputAt(2));
}


void CheckTrimConstant(ControlReducerTester* T, Node* k) {
  Node* phi = T->graph.NewNode(T->common.Phi(kMachInt32, 1), k, T->start);
  CHECK(IsUsedBy(k, phi));
  T->Trim();
  CHECK(!IsUsedBy(k, phi));
  CHECK_EQ(NULL, phi->InputAt(0));
  CHECK_EQ(NULL, phi->InputAt(1));
}


TEST(Trim_constants) {
  ControlReducerTester T;
  int32_t int32_constants[] = {
      0, -1,  -2,  2,  2,  3,  3,  4,  4,  5,  5,  4,  5,  6, 6, 7, 8, 7, 8, 9,
      0, -11, -12, 12, 12, 13, 13, 14, 14, 15, 15, 14, 15, 6, 6, 7, 8, 7, 8, 9};

  for (size_t i = 0; i < arraysize(int32_constants); i++) {
    CheckTrimConstant(&T, T.jsgraph.Int32Constant(int32_constants[i]));
    CheckTrimConstant(&T, T.jsgraph.Float64Constant(int32_constants[i]));
    CheckTrimConstant(&T, T.jsgraph.Constant(int32_constants[i]));
  }

  Node* other_constants[] = {
      T.jsgraph.UndefinedConstant(), T.jsgraph.TheHoleConstant(),
      T.jsgraph.TrueConstant(),      T.jsgraph.FalseConstant(),
      T.jsgraph.NullConstant(),      T.jsgraph.ZeroConstant(),
      T.jsgraph.OneConstant(),       T.jsgraph.NaNConstant(),
      T.jsgraph.Constant(21),        T.jsgraph.Constant(22.2)};

  for (size_t i = 0; i < arraysize(other_constants); i++) {
    CheckTrimConstant(&T, other_constants[i]);
  }
}


TEST(CReducePhi1) {
  ControlReducerTester R;

  R.ReducePhi(R.leaf[0], R.Phi(R.leaf[0]));
  R.ReducePhi(R.leaf[1], R.Phi(R.leaf[1]));
  R.ReducePhi(R.leaf[2], R.Phi(R.leaf[2]));
  R.ReducePhi(R.leaf[3], R.Phi(R.leaf[3]));
}


TEST(CReducePhi1_dead) {
  ControlReducerTester R;

  R.ReducePhi(R.leaf[0], R.Phi(R.leaf[0], R.dead));
  R.ReducePhi(R.leaf[1], R.Phi(R.leaf[1], R.dead));
  R.ReducePhi(R.leaf[2], R.Phi(R.leaf[2], R.dead));
  R.ReducePhi(R.leaf[3], R.Phi(R.leaf[3], R.dead));

  R.ReducePhi(R.leaf[0], R.Phi(R.dead, R.leaf[0]));
  R.ReducePhi(R.leaf[1], R.Phi(R.dead, R.leaf[1]));
  R.ReducePhi(R.leaf[2], R.Phi(R.dead, R.leaf[2]));
  R.ReducePhi(R.leaf[3], R.Phi(R.dead, R.leaf[3]));
}


TEST(CReducePhi1_dead2) {
  ControlReducerTester R;

  R.ReducePhi(R.leaf[0], R.Phi(R.leaf[0], R.dead, R.dead));
  R.ReducePhi(R.leaf[0], R.Phi(R.dead, R.leaf[0], R.dead));
  R.ReducePhi(R.leaf[0], R.Phi(R.dead, R.dead, R.leaf[0]));
}


TEST(CReducePhi2a) {
  ControlReducerTester R;

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(a, a));
  }
}


TEST(CReducePhi2b) {
  ControlReducerTester R;

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(R.self, a));
    R.ReducePhi(a, R.Phi(a, R.self));
  }
}


TEST(CReducePhi2c) {
  ControlReducerTester R;

  for (size_t i = 1; i < kNumLeafs; i++) {
    Node* a = R.leaf[i], *b = R.leaf[0];
    Node* phi1 = R.Phi(b, a);
    R.ReducePhi(phi1, phi1);

    Node* phi2 = R.Phi(a, b);
    R.ReducePhi(phi2, phi2);
  }
}


TEST(CReducePhi2_dead) {
  ControlReducerTester R;

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(a, a, R.dead));
    R.ReducePhi(a, R.Phi(a, R.dead, a));
    R.ReducePhi(a, R.Phi(R.dead, a, a));
  }

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(R.self, a));
    R.ReducePhi(a, R.Phi(a, R.self));
    R.ReducePhi(a, R.Phi(R.self, a, R.dead));
    R.ReducePhi(a, R.Phi(a, R.self, R.dead));
  }

  for (size_t i = 1; i < kNumLeafs; i++) {
    Node* a = R.leaf[i], *b = R.leaf[0];
    Node* phi1 = R.Phi(b, a, R.dead);
    R.ReducePhi(phi1, phi1);

    Node* phi2 = R.Phi(a, b, R.dead);
    R.ReducePhi(phi2, phi2);
  }
}


TEST(CReducePhi3) {
  ControlReducerTester R;

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(a, a, a));
  }

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(R.self, a, a));
    R.ReducePhi(a, R.Phi(a, R.self, a));
    R.ReducePhi(a, R.Phi(a, a, R.self));
  }

  for (size_t i = 1; i < kNumLeafs; i++) {
    Node* a = R.leaf[i], *b = R.leaf[0];
    Node* phi1 = R.Phi(b, a, a);
    R.ReducePhi(phi1, phi1);

    Node* phi2 = R.Phi(a, b, a);
    R.ReducePhi(phi2, phi2);

    Node* phi3 = R.Phi(a, a, b);
    R.ReducePhi(phi3, phi3);
  }
}


TEST(CReducePhi4) {
  ControlReducerTester R;

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(a, a, a, a));
  }

  for (size_t i = 0; i < kNumLeafs; i++) {
    Node* a = R.leaf[i];
    R.ReducePhi(a, R.Phi(R.self, a, a, a));
    R.ReducePhi(a, R.Phi(a, R.self, a, a));
    R.ReducePhi(a, R.Phi(a, a, R.self, a));
    R.ReducePhi(a, R.Phi(a, a, a, R.self));

    R.ReducePhi(a, R.Phi(R.self, R.self, a, a));
    R.ReducePhi(a, R.Phi(a, R.self, R.self, a));
    R.ReducePhi(a, R.Phi(a, a, R.self, R.self));
    R.ReducePhi(a, R.Phi(R.self, a, a, R.self));
  }

  for (size_t i = 1; i < kNumLeafs; i++) {
    Node* a = R.leaf[i], *b = R.leaf[0];
    Node* phi1 = R.Phi(b, a, a, a);
    R.ReducePhi(phi1, phi1);

    Node* phi2 = R.Phi(a, b, a, a);
    R.ReducePhi(phi2, phi2);

    Node* phi3 = R.Phi(a, a, b, a);
    R.ReducePhi(phi3, phi3);

    Node* phi4 = R.Phi(a, a, a, b);
    R.ReducePhi(phi4, phi4);
  }
}


TEST(CReducePhi_iterative1) {
  ControlReducerTester R;

  R.ReducePhiIterative(R.leaf[0], R.Phi(R.leaf[0], R.Phi(R.leaf[0])));
  R.ReducePhiIterative(R.leaf[0], R.Phi(R.Phi(R.leaf[0]), R.leaf[0]));
}


TEST(CReducePhi_iterative2) {
  ControlReducerTester R;

  R.ReducePhiIterative(R.leaf[0], R.Phi(R.Phi(R.leaf[0]), R.Phi(R.leaf[0])));
}


TEST(CReducePhi_iterative3) {
  ControlReducerTester R;

  R.ReducePhiIterative(R.leaf[0],
                       R.Phi(R.leaf[0], R.Phi(R.leaf[0], R.leaf[0])));
  R.ReducePhiIterative(R.leaf[0],
                       R.Phi(R.Phi(R.leaf[0], R.leaf[0]), R.leaf[0]));
}


TEST(CReducePhi_iterative4) {
  ControlReducerTester R;

  R.ReducePhiIterative(R.leaf[0], R.Phi(R.Phi(R.leaf[0], R.leaf[0]),
                                        R.Phi(R.leaf[0], R.leaf[0])));

  Node* p1 = R.Phi(R.leaf[0], R.leaf[0]);
  R.ReducePhiIterative(R.leaf[0], R.Phi(p1, p1));

  Node* p2 = R.Phi(R.leaf[0], R.leaf[0], R.leaf[0]);
  R.ReducePhiIterative(R.leaf[0], R.Phi(p2, p2, p2));

  Node* p3 = R.Phi(R.leaf[0], R.leaf[0], R.leaf[0]);
  R.ReducePhiIterative(R.leaf[0], R.Phi(p3, p3, R.leaf[0]));
}


TEST(CReducePhi_iterative_self1) {
  ControlReducerTester R;

  R.ReducePhiIterative(R.leaf[0], R.Phi(R.leaf[0], R.Phi(R.leaf[0], R.self)));
  R.ReducePhiIterative(R.leaf[0], R.Phi(R.Phi(R.leaf[0], R.self), R.leaf[0]));
}


TEST(CReducePhi_iterative_self2) {
  ControlReducerTester R;

  R.ReducePhiIterative(
      R.leaf[0], R.Phi(R.Phi(R.leaf[0], R.self), R.Phi(R.leaf[0], R.self)));
  R.ReducePhiIterative(
      R.leaf[0], R.Phi(R.Phi(R.self, R.leaf[0]), R.Phi(R.self, R.leaf[0])));

  Node* p1 = R.Phi(R.leaf[0], R.self);
  R.ReducePhiIterative(R.leaf[0], R.Phi(p1, p1));

  Node* p2 = R.Phi(R.self, R.leaf[0]);
  R.ReducePhiIterative(R.leaf[0], R.Phi(p2, p2));
}


TEST(EReducePhi1) {
  ControlReducerTester R;

  R.ReducePhi(R.leaf[0], R.EffectPhi(R.leaf[0]));
  R.ReducePhi(R.leaf[1], R.EffectPhi(R.leaf[1]));
  R.ReducePhi(R.leaf[2], R.EffectPhi(R.leaf[2]));
  R.ReducePhi(R.leaf[3], R.EffectPhi(R.leaf[3]));
}


TEST(EReducePhi1_dead) {
  ControlReducerTester R;

  R.ReducePhi(R.leaf[0], R.EffectPhi(R.leaf[0], R.dead));
  R.ReducePhi(R.leaf[1], R.EffectPhi(R.leaf[1], R.dead));
  R.ReducePhi(R.leaf[2], R.EffectPhi(R.leaf[2], R.dead));
  R.ReducePhi(R.leaf[3], R.EffectPhi(R.leaf[3], R.dead));

  R.ReducePhi(R.leaf[0], R.EffectPhi(R.dead, R.leaf[0]));
  R.ReducePhi(R.leaf[1], R.EffectPhi(R.dead, R.leaf[1]));
  R.ReducePhi(R.leaf[2], R.EffectPhi(R.dead, R.leaf[2]));
  R.ReducePhi(R.leaf[3], R.EffectPhi(R.dead, R.leaf[3]));
}


TEST(EReducePhi1_dead2) {
  ControlReducerTester R;

  R.ReducePhi(R.leaf[0], R.EffectPhi(R.leaf[0], R.dead, R.dead));
  R.ReducePhi(R.leaf[0], R.EffectPhi(R.dead, R.leaf[0], R.dead));
  R.ReducePhi(R.leaf[0], R.EffectPhi(R.dead, R.dead, R.leaf[0]));
}


TEST(CMergeReduce_simple1) {
  ControlReducerTester R;

  Node* merge = R.graph.NewNode(R.common.Merge(1), R.start);
  R.ReduceMerge(R.start, merge);
}


TEST(CMergeReduce_simple2) {
  ControlReducerTester R;

  Node* merge1 = R.graph.NewNode(R.common.Merge(1), R.start);
  Node* merge2 = R.graph.NewNode(R.common.Merge(1), merge1);
  R.ReduceMerge(merge1, merge2);
  R.ReduceMergeIterative(R.start, merge2);
}


TEST(CMergeReduce_none1) {
  ControlReducerTester R;

  Node* merge = R.graph.NewNode(R.common.Merge(2), R.start, R.start);
  R.ReduceMerge(merge, merge);
}


TEST(CMergeReduce_none2) {
  ControlReducerTester R;

  Node* t = R.graph.NewNode(R.common.IfTrue(), R.start);
  Node* f = R.graph.NewNode(R.common.IfFalse(), R.start);
  Node* merge = R.graph.NewNode(R.common.Merge(2), t, f);
  R.ReduceMerge(merge, merge);
}


TEST(CMergeReduce_self3) {
  ControlReducerTester R;

  Node* merge =
      R.SetSelfReferences(R.graph.NewNode(R.common.Merge(2), R.start, R.self));
  R.ReduceMerge(merge, merge);
}


TEST(CMergeReduce_dead1) {
  ControlReducerTester R;

  Node* merge = R.graph.NewNode(R.common.Merge(2), R.start, R.dead);
  R.ReduceMerge(R.start, merge);
}


TEST(CMergeReduce_dead2) {
  ControlReducerTester R;

  Node* merge1 = R.graph.NewNode(R.common.Merge(1), R.start);
  Node* merge2 = R.graph.NewNode(R.common.Merge(2), merge1, R.dead);
  R.ReduceMerge(merge1, merge2);
  R.ReduceMergeIterative(R.start, merge2);
}


TEST(CMergeReduce_dead_rm1a) {
  ControlReducerTester R;

  for (int i = 0; i < 3; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(3), R.start, R.start, R.start);
    merge->ReplaceInput(i, R.dead);
    R.ReduceMerge(merge, merge);
    CheckMerge(merge, R.start, R.start);
  }
}


TEST(CMergeReduce_dead_rm1b) {
  ControlReducerTester R;

  Node* t = R.graph.NewNode(R.common.IfTrue(), R.start);
  Node* f = R.graph.NewNode(R.common.IfFalse(), R.start);
  for (int i = 0; i < 2; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(3), R.dead, R.dead, R.dead);
    for (int j = i + 1; j < 3; j++) {
      merge->ReplaceInput(i, t);
      merge->ReplaceInput(j, f);
      R.ReduceMerge(merge, merge);
      CheckMerge(merge, t, f);
    }
  }
}


TEST(CMergeReduce_dead_rm2) {
  ControlReducerTester R;

  for (int i = 0; i < 3; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(3), R.dead, R.dead, R.dead);
    merge->ReplaceInput(i, R.start);
    R.ReduceMerge(R.start, merge);
  }
}


TEST(CLoopReduce_dead_rm1) {
  ControlReducerTester R;

  for (int i = 0; i < 3; i++) {
    Node* loop = R.graph.NewNode(R.common.Loop(3), R.dead, R.start, R.start);
    R.ReduceMerge(loop, loop);
    CheckLoop(loop, R.start, R.start);
  }
}


TEST(CMergeReduce_edit_phi1) {
  ControlReducerTester R;

  for (int i = 0; i < 3; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(3), R.start, R.start, R.start);
    merge->ReplaceInput(i, R.dead);
    Node* phi = R.graph.NewNode(R.common.Phi(kMachAnyTagged, 3), R.leaf[0],
                                R.leaf[1], R.leaf[2], merge);
    R.ReduceMerge(merge, merge);
    CHECK_EQ(IrOpcode::kPhi, phi->opcode());
    CHECK_EQ(2, phi->op()->InputCount());
    CHECK_EQ(3, phi->InputCount());
    CHECK_EQ(R.leaf[i < 1 ? 1 : 0], phi->InputAt(0));
    CHECK_EQ(R.leaf[i < 2 ? 2 : 1], phi->InputAt(1));
    CHECK_EQ(merge, phi->InputAt(2));
  }
}


TEST(CMergeReduce_edit_effect_phi1) {
  ControlReducerTester R;

  for (int i = 0; i < 3; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(3), R.start, R.start, R.start);
    merge->ReplaceInput(i, R.dead);
    Node* phi = R.graph.NewNode(R.common.EffectPhi(3), R.leaf[0], R.leaf[1],
                                R.leaf[2], merge);
    R.ReduceMerge(merge, merge);
    CHECK_EQ(IrOpcode::kEffectPhi, phi->opcode());
    CHECK_EQ(0, phi->op()->InputCount());
    CHECK_EQ(2, phi->op()->EffectInputCount());
    CHECK_EQ(3, phi->InputCount());
    CHECK_EQ(R.leaf[i < 1 ? 1 : 0], phi->InputAt(0));
    CHECK_EQ(R.leaf[i < 2 ? 2 : 1], phi->InputAt(1));
    CHECK_EQ(merge, phi->InputAt(2));
  }
}


static const int kSelectorSize = 4;

// Helper to select K of N nodes according to a mask, useful for the test below.
struct Selector {
  int mask;
  int count;
  explicit Selector(int m) {
    mask = m;
    count = v8::base::bits::CountPopulation32(m);
  }
  bool is_selected(int i) { return (mask & (1 << i)) != 0; }
  void CheckNode(Node* node, IrOpcode::Value opcode, Node** inputs,
                 Node* control) {
    CHECK_EQ(opcode, node->opcode());
    CHECK_EQ(count + (control != NULL ? 1 : 0), node->InputCount());
    int index = 0;
    for (int i = 0; i < kSelectorSize; i++) {
      if (mask & (1 << i)) {
        CHECK_EQ(inputs[i], node->InputAt(index++));
      }
    }
    CHECK_EQ(count, index);
    if (control != NULL) CHECK_EQ(control, node->InputAt(index++));
  }
  int single_index() {
    CHECK_EQ(1, count);
    return WhichPowerOf2(mask);
  }
};


TEST(CMergeReduce_exhaustive_4) {
  ControlReducerTester R;
  Node* controls[] = {
      R.graph.NewNode(R.common.Start(1)), R.graph.NewNode(R.common.Start(2)),
      R.graph.NewNode(R.common.Start(3)), R.graph.NewNode(R.common.Start(4))};
  Node* values[] = {R.jsgraph.Int32Constant(11), R.jsgraph.Int32Constant(22),
                    R.jsgraph.Int32Constant(33), R.jsgraph.Int32Constant(44)};
  Node* effects[] = {
      R.jsgraph.Float64Constant(123.4), R.jsgraph.Float64Constant(223.4),
      R.jsgraph.Float64Constant(323.4), R.jsgraph.Float64Constant(423.4)};

  for (int mask = 0; mask < (1 << (kSelectorSize - 1)); mask++) {
    // Reduce a single merge with a given mask.
    Node* merge = R.graph.NewNode(R.common.Merge(4), controls[0], controls[1],
                                  controls[2], controls[3]);
    Node* phi = R.graph.NewNode(R.common.Phi(kMachAnyTagged, 4), values[0],
                                values[1], values[2], values[3], merge);
    Node* ephi = R.graph.NewNode(R.common.EffectPhi(4), effects[0], effects[1],
                                 effects[2], effects[3], merge);

    Node* phi_use =
        R.graph.NewNode(R.common.Phi(kMachAnyTagged, 1), phi, R.start);
    Node* ephi_use = R.graph.NewNode(R.common.EffectPhi(1), ephi, R.start);

    Selector selector(mask);

    for (int i = 0; i < kSelectorSize; i++) {  // set up dead merge inputs.
      if (!selector.is_selected(i)) merge->ReplaceInput(i, R.dead);
    }

    Node* result =
        ControlReducer::ReduceMergeForTesting(&R.jsgraph, &R.common, merge);

    int count = selector.count;
    if (count == 0) {
      // result should be dead.
      CHECK_EQ(IrOpcode::kDead, result->opcode());
    } else if (count == 1) {
      // merge should be replaced with one of the controls.
      CHECK_EQ(controls[selector.single_index()], result);
      // Phis should have been directly replaced.
      CHECK_EQ(values[selector.single_index()], phi_use->InputAt(0));
      CHECK_EQ(effects[selector.single_index()], ephi_use->InputAt(0));
    } else {
      // Otherwise, nodes should be edited in place.
      CHECK_EQ(merge, result);
      selector.CheckNode(merge, IrOpcode::kMerge, controls, NULL);
      selector.CheckNode(phi, IrOpcode::kPhi, values, merge);
      selector.CheckNode(ephi, IrOpcode::kEffectPhi, effects, merge);
      CHECK_EQ(phi, phi_use->InputAt(0));
      CHECK_EQ(ephi, ephi_use->InputAt(0));
      CHECK_EQ(count, phi->op()->InputCount());
      CHECK_EQ(count + 1, phi->InputCount());
      CHECK_EQ(count, ephi->op()->EffectInputCount());
      CHECK_EQ(count + 1, ephi->InputCount());
    }
  }
}


TEST(CMergeReduce_edit_many_phis1) {
  ControlReducerTester R;

  const int kPhiCount = 10;
  Node* phis[kPhiCount];

  for (int i = 0; i < 3; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(3), R.start, R.start, R.start);
    merge->ReplaceInput(i, R.dead);
    for (int j = 0; j < kPhiCount; j++) {
      phis[j] = R.graph.NewNode(R.common.Phi(kMachAnyTagged, 3), R.leaf[0],
                                R.leaf[1], R.leaf[2], merge);
    }
    R.ReduceMerge(merge, merge);
    for (int j = 0; j < kPhiCount; j++) {
      Node* phi = phis[j];
      CHECK_EQ(IrOpcode::kPhi, phi->opcode());
      CHECK_EQ(2, phi->op()->InputCount());
      CHECK_EQ(3, phi->InputCount());
      CHECK_EQ(R.leaf[i < 1 ? 1 : 0], phi->InputAt(0));
      CHECK_EQ(R.leaf[i < 2 ? 2 : 1], phi->InputAt(1));
      CHECK_EQ(merge, phi->InputAt(2));
    }
  }
}


TEST(CMergeReduce_simple_chain1) {
  ControlReducerTester R;
  for (int i = 0; i < 5; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(1), R.start);
    for (int j = 0; j < i; j++) {
      merge = R.graph.NewNode(R.common.Merge(1), merge);
    }
    R.ReduceMergeIterative(R.start, merge);
  }
}


TEST(CMergeReduce_dead_chain1) {
  ControlReducerTester R;
  for (int i = 0; i < 5; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(1), R.dead);
    for (int j = 0; j < i; j++) {
      merge = R.graph.NewNode(R.common.Merge(1), merge);
    }
    Node* end = R.graph.NewNode(R.common.End(), merge);
    R.graph.SetEnd(end);
    R.ReduceGraph();
    CHECK(merge->IsDead());
    CHECK_EQ(NULL, end->InputAt(0));  // end dies.
  }
}


TEST(CMergeReduce_dead_chain2) {
  ControlReducerTester R;
  for (int i = 0; i < 5; i++) {
    Node* merge = R.graph.NewNode(R.common.Merge(1), R.start);
    for (int j = 0; j < i; j++) {
      merge = R.graph.NewNode(R.common.Merge(2), merge, R.dead);
    }
    R.ReduceMergeIterative(R.start, merge);
  }
}


struct Branch {
  Node* branch;
  Node* if_true;
  Node* if_false;

  Branch(ControlReducerTester& R, Node* cond, Node* control = NULL) {
    if (control == NULL) control = R.start;
    branch = R.graph.NewNode(R.common.Branch(), cond, control);
    if_true = R.graph.NewNode(R.common.IfTrue(), branch);
    if_false = R.graph.NewNode(R.common.IfFalse(), branch);
  }
};


// TODO(titzer): use the diamonds from src/compiler/diamond.h here.
struct Diamond {
  Node* branch;
  Node* if_true;
  Node* if_false;
  Node* merge;
  Node* phi;

  Diamond(ControlReducerTester& R, Node* cond) {
    branch = R.graph.NewNode(R.common.Branch(), cond, R.start);
    if_true = R.graph.NewNode(R.common.IfTrue(), branch);
    if_false = R.graph.NewNode(R.common.IfFalse(), branch);
    merge = R.graph.NewNode(R.common.Merge(2), if_true, if_false);
    phi = NULL;
  }

  Diamond(ControlReducerTester& R, Node* cond, Node* tv, Node* fv) {
    branch = R.graph.NewNode(R.common.Branch(), cond, R.start);
    if_true = R.graph.NewNode(R.common.IfTrue(), branch);
    if_false = R.graph.NewNode(R.common.IfFalse(), branch);
    merge = R.graph.NewNode(R.common.Merge(2), if_true, if_false);
    phi = R.graph.NewNode(R.common.Phi(kMachAnyTagged, 2), tv, fv, merge);
  }

  void chain(Diamond& that) { branch->ReplaceInput(1, that.merge); }

  // Nest {this} into either the if_true or if_false branch of {that}.
  void nest(Diamond& that, bool if_true) {
    if (if_true) {
      branch->ReplaceInput(1, that.if_true);
      that.merge->ReplaceInput(0, merge);
    } else {
      branch->ReplaceInput(1, that.if_false);
      that.merge->ReplaceInput(1, merge);
    }
  }
};


struct While {
  Node* branch;
  Node* if_true;
  Node* exit;
  Node* loop;

  While(ControlReducerTester& R, Node* cond) {
    loop = R.graph.NewNode(R.common.Loop(2), R.start, R.start);
    branch = R.graph.NewNode(R.common.Branch(), cond, loop);
    if_true = R.graph.NewNode(R.common.IfTrue(), branch);
    exit = R.graph.NewNode(R.common.IfFalse(), branch);
    loop->ReplaceInput(1, if_true);
  }

  void chain(Node* control) { loop->ReplaceInput(0, control); }
};


TEST(CBranchReduce_none1) {
  ControlReducerTester R;
  Diamond d(R, R.p0);
  R.ReduceBranch(d.branch, d.branch);
}


TEST(CBranchReduce_none2) {
  ControlReducerTester R;
  Diamond d1(R, R.p0);
  Diamond d2(R, R.p0);
  d2.chain(d1);
  R.ReduceBranch(d2.branch, d2.branch);
}


TEST(CBranchReduce_true) {
  ControlReducerTester R;
  Node* true_values[] = {
      R.one,                               R.jsgraph.Int32Constant(2),
      R.jsgraph.Int32Constant(0x7fffffff), R.jsgraph.Constant(1.0),
      R.jsgraph.Constant(22.1),            R.jsgraph.TrueConstant()};

  for (size_t i = 0; i < arraysize(true_values); i++) {
    Diamond d(R, true_values[i]);
    Node* true_use = R.graph.NewNode(R.common.Merge(1), d.if_true);
    Node* false_use = R.graph.NewNode(R.common.Merge(1), d.if_false);
    R.ReduceBranch(R.start, d.branch);
    CHECK_EQ(R.start, true_use->InputAt(0));
    CHECK_EQ(IrOpcode::kDead, false_use->InputAt(0)->opcode());
    CHECK(d.if_true->IsDead());   // replaced
    CHECK(d.if_false->IsDead());  // replaced
  }
}


TEST(CBranchReduce_false) {
  ControlReducerTester R;
  Node* false_values[] = {R.zero, R.jsgraph.Constant(0.0),
                          R.jsgraph.Constant(-0.0), R.jsgraph.FalseConstant()};

  for (size_t i = 0; i < arraysize(false_values); i++) {
    Diamond d(R, false_values[i]);
    Node* true_use = R.graph.NewNode(R.common.Merge(1), d.if_true);
    Node* false_use = R.graph.NewNode(R.common.Merge(1), d.if_false);
    R.ReduceBranch(R.start, d.branch);
    CHECK_EQ(R.start, false_use->InputAt(0));
    CHECK_EQ(IrOpcode::kDead, true_use->InputAt(0)->opcode());
    CHECK(d.if_true->IsDead());   // replaced
    CHECK(d.if_false->IsDead());  // replaced
  }
}


TEST(CDiamondReduce_true) {
  ControlReducerTester R;
  Diamond d1(R, R.one);
  R.ReduceMergeIterative(R.start, d1.merge);
}


TEST(CDiamondReduce_false) {
  ControlReducerTester R;
  Diamond d2(R, R.zero);
  R.ReduceMergeIterative(R.start, d2.merge);
}


TEST(CChainedDiamondsReduce_true_false) {
  ControlReducerTester R;
  Diamond d1(R, R.one);
  Diamond d2(R, R.zero);
  d2.chain(d1);

  R.ReduceMergeIterative(R.start, d2.merge);
}


TEST(CChainedDiamondsReduce_x_false) {
  ControlReducerTester R;
  Diamond d1(R, R.p0);
  Diamond d2(R, R.zero);
  d2.chain(d1);

  R.ReduceMergeIterative(d1.merge, d2.merge);
}


TEST(CChainedDiamondsReduce_false_x) {
  ControlReducerTester R;
  Diamond d1(R, R.zero);
  Diamond d2(R, R.p0);
  d2.chain(d1);

  R.ReduceMergeIterative(d2.merge, d2.merge);
  CheckInputs(d2.branch, R.p0, R.start);
}


TEST(CChainedDiamondsReduce_phi1) {
  ControlReducerTester R;
  Diamond d1(R, R.zero, R.one, R.zero);  // foldable branch, phi.
  Diamond d2(R, d1.phi);
  d2.chain(d1);

  R.ReduceMergeIterative(R.start, d2.merge);
}


TEST(CChainedDiamondsReduce_phi2) {
  ControlReducerTester R;
  Diamond d1(R, R.p0, R.one, R.one);  // redundant phi.
  Diamond d2(R, d1.phi);
  d2.chain(d1);

  R.ReduceMergeIterative(d1.merge, d2.merge);
}


TEST(CNestedDiamondsReduce_true_true_false) {
  ControlReducerTester R;
  Diamond d1(R, R.one);
  Diamond d2(R, R.zero);
  d2.nest(d1, true);

  R.ReduceMergeIterative(R.start, d1.merge);
}


TEST(CNestedDiamondsReduce_false_true_false) {
  ControlReducerTester R;
  Diamond d1(R, R.one);
  Diamond d2(R, R.zero);
  d2.nest(d1, false);

  R.ReduceMergeIterative(R.start, d1.merge);
}


TEST(CNestedDiamonds_xyz) {
  ControlReducerTester R;

  for (int a = 0; a < 2; a++) {
    for (int b = 0; b < 2; b++) {
      for (int c = 0; c < 2; c++) {
        Diamond d1(R, R.jsgraph.Int32Constant(a));
        Diamond d2(R, R.jsgraph.Int32Constant(b));
        d2.nest(d1, c);

        R.ReduceMergeIterative(R.start, d1.merge);
      }
    }
  }
}


TEST(CDeadLoop1) {
  ControlReducerTester R;

  Node* loop = R.graph.NewNode(R.common.Loop(1), R.start);
  Branch b(R, R.p0, loop);
  loop->ReplaceInput(0, b.if_true);  // loop is not connected to start.
  Node* merge = R.graph.NewNode(R.common.Merge(2), R.start, b.if_false);
  R.ReduceMergeIterative(R.start, merge);
  CHECK(b.if_true->IsDead());
  CHECK(b.if_false->IsDead());
}


TEST(CDeadLoop2) {
  ControlReducerTester R;

  While w(R, R.p0);
  Diamond d(R, R.zero);
  // if (0) { while (p0) ; } else { }
  w.branch->ReplaceInput(1, d.if_true);
  d.merge->ReplaceInput(0, w.exit);

  R.ReduceMergeIterative(R.start, d.merge);
  CHECK(d.if_true->IsDead());
  CHECK(d.if_false->IsDead());
}


TEST(CNonTermLoop1) {
  ControlReducerTester R;
  Node* loop =
      R.SetSelfReferences(R.graph.NewNode(R.common.Loop(2), R.start, R.self));
  R.ReduceGraph();
  Node* end = R.graph.end();
  CheckLoop(loop, R.start, loop);
  Node* merge = end->InputAt(0);
  CheckMerge(merge, R.start, loop);
}


TEST(CNonTermLoop2) {
  ControlReducerTester R;
  Diamond d(R, R.p0);
  Node* loop = R.SetSelfReferences(
      R.graph.NewNode(R.common.Loop(2), d.if_false, R.self));
  d.merge->ReplaceInput(1, R.dead);
  Node* end = R.graph.end();
  end->ReplaceInput(0, d.merge);
  R.ReduceGraph();
  CHECK_EQ(end, R.graph.end());
  CheckLoop(loop, d.if_false, loop);
  Node* merge = end->InputAt(0);
  CheckMerge(merge, d.if_true, loop);
}


TEST(NonTermLoop3) {
  ControlReducerTester R;
  Node* loop = R.graph.NewNode(R.common.Loop(2), R.start, R.start);
  Branch b(R, R.one, loop);
  loop->ReplaceInput(1, b.if_true);
  Node* end = R.graph.end();
  end->ReplaceInput(0, b.if_false);

  R.ReduceGraph();

  CHECK_EQ(end, R.graph.end());
  CheckInputs(end, loop);
  CheckInputs(loop, R.start, loop);
}


TEST(CNonTermLoop_terminate1) {
  ControlReducerTester R;
  Node* loop = R.graph.NewNode(R.common.Loop(2), R.start, R.start);
  Node* effect = R.SetSelfReferences(
      R.graph.NewNode(R.common.EffectPhi(2), R.start, R.self, loop));
  Branch b(R, R.one, loop);
  loop->ReplaceInput(1, b.if_true);
  Node* end = R.graph.end();
  end->ReplaceInput(0, b.if_false);

  R.ReduceGraph();

  CHECK_EQ(end, R.graph.end());
  CheckLoop(loop, R.start, loop);
  Node* terminate = end->InputAt(0);
  CHECK_EQ(IrOpcode::kTerminate, terminate->opcode());
  CHECK_EQ(2, terminate->InputCount());
  CHECK_EQ(1, terminate->op()->EffectInputCount());
  CHECK_EQ(1, terminate->op()->ControlInputCount());
  CheckInputs(terminate, effect, loop);
}


TEST(CNonTermLoop_terminate2) {
  ControlReducerTester R;
  Node* loop = R.graph.NewNode(R.common.Loop(2), R.start, R.start);
  Node* effect1 = R.SetSelfReferences(
      R.graph.NewNode(R.common.EffectPhi(2), R.start, R.self, loop));
  Node* effect2 = R.SetSelfReferences(
      R.graph.NewNode(R.common.EffectPhi(2), R.start, R.self, loop));
  Branch b(R, R.one, loop);
  loop->ReplaceInput(1, b.if_true);
  Node* end = R.graph.end();
  end->ReplaceInput(0, b.if_false);

  R.ReduceGraph();

  CheckLoop(loop, R.start, loop);
  CHECK_EQ(end, R.graph.end());
  Node* terminate = end->InputAt(0);
  CHECK_EQ(IrOpcode::kTerminate, terminate->opcode());
  CHECK_EQ(3, terminate->InputCount());
  CHECK_EQ(2, terminate->op()->EffectInputCount());
  CHECK_EQ(1, terminate->op()->ControlInputCount());
  Node* e0 = terminate->InputAt(0);
  Node* e1 = terminate->InputAt(1);
  CHECK(e0 == effect1 || e1 == effect1);
  CHECK(e0 == effect2 || e1 == effect2);
  CHECK_EQ(loop, terminate->InputAt(2));
}


TEST(CNonTermLoop_terminate_m1) {
  ControlReducerTester R;
  Node* loop =
      R.SetSelfReferences(R.graph.NewNode(R.common.Loop(2), R.start, R.self));
  Node* effect = R.SetSelfReferences(
      R.graph.NewNode(R.common.EffectPhi(2), R.start, R.self, loop));
  R.ReduceGraph();
  Node* end = R.graph.end();
  CHECK_EQ(R.start, loop->InputAt(0));
  CHECK_EQ(loop, loop->InputAt(1));
  Node* merge = end->InputAt(0);
  CHECK_EQ(IrOpcode::kMerge, merge->opcode());
  CHECK_EQ(2, merge->InputCount());
  CHECK_EQ(2, merge->op()->ControlInputCount());
  CHECK_EQ(R.start, merge->InputAt(0));

  Node* terminate = merge->InputAt(1);
  CHECK_EQ(IrOpcode::kTerminate, terminate->opcode());
  CHECK_EQ(2, terminate->InputCount());
  CHECK_EQ(1, terminate->op()->EffectInputCount());
  CHECK_EQ(1, terminate->op()->ControlInputCount());
  CHECK_EQ(effect, terminate->InputAt(0));
  CHECK_EQ(loop, terminate->InputAt(1));
}


TEST(CNonTermLoop_big1) {
  ControlReducerTester R;
  Branch b1(R, R.p0);
  Node* rt = R.graph.NewNode(R.common.Return(), R.one, R.start, b1.if_true);

  Branch b2(R, R.p0, b1.if_false);
  Node* rf = R.graph.NewNode(R.common.Return(), R.zero, R.start, b2.if_true);
  Node* loop = R.SetSelfReferences(
      R.graph.NewNode(R.common.Loop(2), b2.if_false, R.self));
  Node* merge = R.graph.NewNode(R.common.Merge(2), rt, rf);
  R.end->ReplaceInput(0, merge);

  R.ReduceGraph();

  CheckInputs(R.end, merge);
  CheckInputs(merge, rt, rf, loop);
  CheckInputs(loop, b2.if_false, loop);
}


TEST(CNonTermLoop_big2) {
  ControlReducerTester R;
  Branch b1(R, R.p0);
  Node* rt = R.graph.NewNode(R.common.Return(), R.one, R.start, b1.if_true);

  Branch b2(R, R.zero, b1.if_false);
  Node* rf = R.graph.NewNode(R.common.Return(), R.zero, R.start, b2.if_true);
  Node* loop = R.SetSelfReferences(
      R.graph.NewNode(R.common.Loop(2), b2.if_false, R.self));
  Node* merge = R.graph.NewNode(R.common.Merge(2), rt, rf);
  R.end->ReplaceInput(0, merge);

  R.ReduceGraph();

  Node* new_merge = R.end->InputAt(0);  // old merge was reduced.
  CHECK_NE(merge, new_merge);
  CheckInputs(new_merge, rt, loop);
  CheckInputs(loop, b1.if_false, loop);
  CHECK(merge->IsDead());
  CHECK(rf->IsDead());
  CHECK(b2.if_true->IsDead());
}


TEST(Return1) {
  ControlReducerTester R;
  Node* ret = R.Return(R.one, R.start, R.start);
  R.ReduceGraph();
  CheckInputs(R.graph.end(), ret);
  CheckInputs(ret, R.one, R.start, R.start);
}


TEST(Return2) {
  ControlReducerTester R;
  Diamond d(R, R.one);
  Node* ret = R.Return(R.half, R.start, d.merge);
  R.ReduceGraph();
  CHECK(d.branch->IsDead());
  CHECK(d.if_true->IsDead());
  CHECK(d.if_false->IsDead());
  CHECK(d.merge->IsDead());

  CheckInputs(R.graph.end(), ret);
  CheckInputs(ret, R.half, R.start, R.start);
}


TEST(Return_true1) {
  ControlReducerTester R;
  Diamond d(R, R.one, R.half, R.zero);
  Node* ret = R.Return(d.phi, R.start, d.merge);
  R.ReduceGraph();
  CHECK(d.branch->IsDead());
  CHECK(d.if_true->IsDead());
  CHECK(d.if_false->IsDead());
  CHECK(d.merge->IsDead());
  CHECK(d.phi->IsDead());

  CheckInputs(R.graph.end(), ret);
  CheckInputs(ret, R.half, R.start, R.start);
}


TEST(Return_false1) {
  ControlReducerTester R;
  Diamond d(R, R.zero, R.one, R.half);
  Node* ret = R.Return(d.phi, R.start, d.merge);
  R.ReduceGraph();
  CHECK(d.branch->IsDead());
  CHECK(d.if_true->IsDead());
  CHECK(d.if_false->IsDead());
  CHECK(d.merge->IsDead());
  CHECK(d.phi->IsDead());

  CheckInputs(R.graph.end(), ret);
  CheckInputs(ret, R.half, R.start, R.start);
}


void CheckDeadDiamond(Diamond& d) {
  CHECK(d.branch->IsDead());
  CHECK(d.if_true->IsDead());
  CHECK(d.if_false->IsDead());
  CHECK(d.merge->IsDead());
  if (d.phi != NULL) CHECK(d.phi->IsDead());
}


void CheckLiveDiamond(Diamond& d, bool live_phi = true) {
  CheckInputs(d.merge, d.if_true, d.if_false);
  CheckInputs(d.if_true, d.branch);
  CheckInputs(d.if_false, d.branch);
  if (d.phi != NULL) {
    if (live_phi) {
      CHECK_EQ(3, d.phi->InputCount());
      CHECK_EQ(d.merge, d.phi->InputAt(2));
    } else {
      CHECK(d.phi->IsDead());
    }
  }
}


TEST(Return_effect1) {
  ControlReducerTester R;
  Diamond d(R, R.one);
  Node* e1 = R.jsgraph.Float64Constant(-100.1);
  Node* e2 = R.jsgraph.Float64Constant(+100.1);
  Node* effect = R.graph.NewNode(R.common.EffectPhi(2), e1, e2, d.merge);
  Node* ret = R.Return(R.p0, effect, d.merge);
  R.ReduceGraph();
  CheckDeadDiamond(d);
  CHECK(effect->IsDead());

  CheckInputs(R.graph.end(), ret);
  CheckInputs(ret, R.p0, e1, R.start);
}


TEST(Return_nested_diamonds1) {
  ControlReducerTester R;
  Diamond d1(R, R.p0, R.one, R.zero);
  Diamond d2(R, R.p0);
  Diamond d3(R, R.p0);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // nothing should happen.

  CheckInputs(ret, d1.phi, R.start, d1.merge);
  CheckInputs(d1.phi, R.one, R.zero, d1.merge);
  CheckInputs(d1.merge, d2.merge, d3.merge);
  CheckLiveDiamond(d2);
  CheckLiveDiamond(d3);
}


TEST(Return_nested_diamonds_true1) {
  ControlReducerTester R;
  Diamond d1(R, R.one, R.one, R.zero);
  Diamond d2(R, R.p0);
  Diamond d3(R, R.p0);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 gets folded true.

  CheckInputs(ret, R.one, R.start, d2.merge);
  CheckInputs(d2.branch, R.p0, R.start);
  CheckDeadDiamond(d1);
  CheckLiveDiamond(d2);
  CheckDeadDiamond(d3);
}


TEST(Return_nested_diamonds_false1) {
  ControlReducerTester R;
  Diamond d1(R, R.zero, R.one, R.zero);
  Diamond d2(R, R.p0);
  Diamond d3(R, R.p0);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 gets folded false.

  CheckInputs(ret, R.zero, R.start, d3.merge);
  CheckInputs(d3.branch, R.p0, R.start);
  CheckDeadDiamond(d1);
  CheckDeadDiamond(d2);
  CheckLiveDiamond(d3);
}


TEST(Return_nested_diamonds_true_true1) {
  ControlReducerTester R;
  Diamond d1(R, R.one, R.one, R.zero);
  Diamond d2(R, R.one);
  Diamond d3(R, R.p0);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 and d2 both get folded true.

  CheckInputs(ret, R.one, R.start, R.start);
  CheckDeadDiamond(d1);
  CheckDeadDiamond(d2);
  CheckDeadDiamond(d3);
}


TEST(Return_nested_diamonds_true_false1) {
  ControlReducerTester R;
  Diamond d1(R, R.one, R.one, R.zero);
  Diamond d2(R, R.zero);
  Diamond d3(R, R.p0);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 gets folded true and d2 gets folded false.

  CheckInputs(ret, R.one, R.start, R.start);
  CheckDeadDiamond(d1);
  CheckDeadDiamond(d2);
  CheckDeadDiamond(d3);
}


TEST(Return_nested_diamonds2) {
  ControlReducerTester R;
  Node* x2 = R.jsgraph.Float64Constant(11.1);
  Node* y2 = R.jsgraph.Float64Constant(22.2);
  Node* x3 = R.jsgraph.Float64Constant(33.3);
  Node* y3 = R.jsgraph.Float64Constant(44.4);

  Diamond d2(R, R.p0, x2, y2);
  Diamond d3(R, R.p0, x3, y3);
  Diamond d1(R, R.p0, d2.phi, d3.phi);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // nothing should happen.

  CheckInputs(ret, d1.phi, R.start, d1.merge);
  CheckInputs(d1.phi, d2.phi, d3.phi, d1.merge);
  CheckInputs(d1.merge, d2.merge, d3.merge);
  CheckLiveDiamond(d2);
  CheckLiveDiamond(d3);
}


TEST(Return_nested_diamonds_true2) {
  ControlReducerTester R;
  Node* x2 = R.jsgraph.Float64Constant(11.1);
  Node* y2 = R.jsgraph.Float64Constant(22.2);
  Node* x3 = R.jsgraph.Float64Constant(33.3);
  Node* y3 = R.jsgraph.Float64Constant(44.4);

  Diamond d2(R, R.p0, x2, y2);
  Diamond d3(R, R.p0, x3, y3);
  Diamond d1(R, R.one, d2.phi, d3.phi);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 gets folded true.

  CheckInputs(ret, d2.phi, R.start, d2.merge);
  CheckInputs(d2.branch, R.p0, R.start);
  CheckDeadDiamond(d1);
  CheckLiveDiamond(d2);
  CheckDeadDiamond(d3);
}


TEST(Return_nested_diamonds_true_true2) {
  ControlReducerTester R;
  Node* x2 = R.jsgraph.Float64Constant(11.1);
  Node* y2 = R.jsgraph.Float64Constant(22.2);
  Node* x3 = R.jsgraph.Float64Constant(33.3);
  Node* y3 = R.jsgraph.Float64Constant(44.4);

  Diamond d2(R, R.one, x2, y2);
  Diamond d3(R, R.p0, x3, y3);
  Diamond d1(R, R.one, d2.phi, d3.phi);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 gets folded true.

  CheckInputs(ret, x2, R.start, R.start);
  CheckDeadDiamond(d1);
  CheckDeadDiamond(d2);
  CheckDeadDiamond(d3);
}


TEST(Return_nested_diamonds_true_false2) {
  ControlReducerTester R;
  Node* x2 = R.jsgraph.Float64Constant(11.1);
  Node* y2 = R.jsgraph.Float64Constant(22.2);
  Node* x3 = R.jsgraph.Float64Constant(33.3);
  Node* y3 = R.jsgraph.Float64Constant(44.4);

  Diamond d2(R, R.zero, x2, y2);
  Diamond d3(R, R.p0, x3, y3);
  Diamond d1(R, R.one, d2.phi, d3.phi);

  d2.nest(d1, true);
  d3.nest(d1, false);

  Node* ret = R.Return(d1.phi, R.start, d1.merge);

  R.ReduceGraph();  // d1 gets folded true.

  CheckInputs(ret, y2, R.start, R.start);
  CheckDeadDiamond(d1);
  CheckDeadDiamond(d2);
  CheckDeadDiamond(d3);
}
