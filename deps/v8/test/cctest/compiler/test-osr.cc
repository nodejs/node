// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/operator.h"
#include "src/compiler/osr.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(titzer): move this method to a common testing place.

static int CheckInputs(Node* node, Node* i0 = NULL, Node* i1 = NULL,
                       Node* i2 = NULL, Node* i3 = NULL) {
  int count = 4;
  if (i3 == NULL) count = 3;
  if (i2 == NULL) count = 2;
  if (i1 == NULL) count = 1;
  if (i0 == NULL) count = 0;
  CHECK_EQ(count, node->InputCount());
  if (i0 != NULL) CHECK_EQ(i0, node->InputAt(0));
  if (i1 != NULL) CHECK_EQ(i1, node->InputAt(1));
  if (i2 != NULL) CHECK_EQ(i2, node->InputAt(2));
  if (i3 != NULL) CHECK_EQ(i3, node->InputAt(3));
  return count;
}


static Operator kIntLt(IrOpcode::kInt32LessThan, Operator::kPure,
                       "Int32LessThan", 2, 0, 0, 1, 0, 0);
static Operator kIntAdd(IrOpcode::kInt32Add, Operator::kPure, "Int32Add", 2, 0,
                        0, 1, 0, 0);


static const int kMaxOsrValues = 10;

class OsrDeconstructorTester : public HandleAndZoneScope {
 public:
  explicit OsrDeconstructorTester(int num_values)
      : isolate(main_isolate()),
        common(main_zone()),
        graph(main_zone()),
        jsgraph(main_isolate(), &graph, &common, nullptr, nullptr, nullptr),
        start(graph.NewNode(common.Start(1))),
        p0(graph.NewNode(common.Parameter(0), start)),
        end(graph.NewNode(common.End(1), start)),
        osr_normal_entry(graph.NewNode(common.OsrNormalEntry(), start, start)),
        osr_loop_entry(graph.NewNode(common.OsrLoopEntry(), start, start)),
        self(graph.NewNode(common.Int32Constant(0xaabbccdd))) {
    CHECK(num_values <= kMaxOsrValues);
    graph.SetStart(start);
    for (int i = 0; i < num_values; i++) {
      osr_values[i] = graph.NewNode(common.OsrValue(i), osr_loop_entry);
    }
  }

  Isolate* isolate;
  CommonOperatorBuilder common;
  Graph graph;
  JSGraph jsgraph;
  Node* start;
  Node* p0;
  Node* end;
  Node* osr_normal_entry;
  Node* osr_loop_entry;
  Node* self;
  Node* osr_values[kMaxOsrValues];

  Node* NewOsrPhi(Node* loop, Node* incoming, int osr_value, Node* back1 = NULL,
                  Node* back2 = NULL, Node* back3 = NULL) {
    int count = 5;
    if (back3 == NULL) count = 4;
    if (back2 == NULL) count = 3;
    if (back1 == NULL) count = 2;
    CHECK_EQ(loop->InputCount(), count);
    CHECK_EQ(osr_loop_entry, loop->InputAt(1));

    Node* inputs[6];
    inputs[0] = incoming;
    inputs[1] = osr_values[osr_value];
    if (count > 2) inputs[2] = back1;
    if (count > 3) inputs[3] = back2;
    if (count > 4) inputs[4] = back3;
    inputs[count] = loop;
    return graph.NewNode(common.Phi(MachineRepresentation::kTagged, count),
                         count + 1, inputs);
  }

  Node* NewLoop(bool is_osr, int num_backedges, Node* entry = nullptr) {
    if (entry == nullptr) entry = osr_normal_entry;
    Node* loop = graph.NewNode(common.Loop(1), entry);
    if (is_osr) {
      loop->AppendInput(graph.zone(), osr_loop_entry);
    }
    for (int i = 0; i < num_backedges; i++) {
      loop->AppendInput(graph.zone(), loop);
    }
    NodeProperties::ChangeOp(loop, common.Loop(loop->InputCount()));
    return loop;
  }

  Node* NewOsrLoop(int num_backedges, Node* entry = NULL) {
    return NewLoop(true, num_backedges, entry);
  }

  void DeconstructOsr() {
    OsrHelper helper(0, 0);
    helper.Deconstruct(&jsgraph, &common, main_zone());
    AllNodes nodes(main_zone(), &graph);
    // Should be edited out.
    CHECK(!nodes.IsLive(osr_normal_entry));
    CHECK(!nodes.IsLive(osr_loop_entry));
    // No dangling nodes should be left over.
    for (Node* const node : nodes.reachable) {
      for (Node* const use : node->uses()) {
        CHECK(std::find(nodes.reachable.begin(), nodes.reachable.end(), use) !=
              nodes.reachable.end());
      }
    }
  }
};


TEST(Deconstruct_osr0) {
  OsrDeconstructorTester T(0);

  Node* loop = T.NewOsrLoop(1);

  T.graph.SetEnd(loop);

  T.DeconstructOsr();

  CheckInputs(loop, T.start, loop);
}


TEST(Deconstruct_osr1) {
  OsrDeconstructorTester T(1);

  Node* loop = T.NewOsrLoop(1);
  Node* osr_phi =
      T.NewOsrPhi(loop, T.jsgraph.OneConstant(), 0, T.jsgraph.ZeroConstant());

  Node* ret = T.graph.NewNode(T.common.Return(), osr_phi, T.start, loop);
  T.graph.SetEnd(ret);

  T.DeconstructOsr();

  CheckInputs(loop, T.start, loop);
  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.ZeroConstant(), loop);
  CheckInputs(ret, osr_phi, T.start, loop);
}


TEST(Deconstruct_osr_remove_prologue) {
  OsrDeconstructorTester T(1);
  Diamond d(&T.graph, &T.common, T.p0);
  d.Chain(T.osr_normal_entry);

  Node* loop = T.NewOsrLoop(1, d.merge);
  Node* osr_phi =
      T.NewOsrPhi(loop, T.jsgraph.OneConstant(), 0, T.jsgraph.ZeroConstant());

  Node* ret = T.graph.NewNode(T.common.Return(), osr_phi, T.start, loop);
  T.graph.SetEnd(ret);

  T.DeconstructOsr();

  CheckInputs(loop, T.start, loop);
  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.ZeroConstant(), loop);
  CheckInputs(ret, osr_phi, T.start, loop);

  // The control before the loop should have been removed.
  AllNodes nodes(T.main_zone(), &T.graph);
  CHECK(!nodes.IsLive(d.branch));
  CHECK(!nodes.IsLive(d.if_true));
  CHECK(!nodes.IsLive(d.if_false));
  CHECK(!nodes.IsLive(d.merge));
}


TEST(Deconstruct_osr_with_body1) {
  OsrDeconstructorTester T(1);

  Node* loop = T.NewOsrLoop(1);

  Node* branch = T.graph.NewNode(T.common.Branch(), T.p0, loop);
  Node* if_true = T.graph.NewNode(T.common.IfTrue(), branch);
  Node* if_false = T.graph.NewNode(T.common.IfFalse(), branch);
  loop->ReplaceInput(2, if_true);

  Node* osr_phi =
      T.NewOsrPhi(loop, T.jsgraph.OneConstant(), 0, T.jsgraph.ZeroConstant());

  Node* ret = T.graph.NewNode(T.common.Return(), osr_phi, T.start, if_false);
  T.graph.SetEnd(ret);

  T.DeconstructOsr();

  CheckInputs(loop, T.start, if_true);
  CheckInputs(branch, T.p0, loop);
  CheckInputs(if_true, branch);
  CheckInputs(if_false, branch);
  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.ZeroConstant(), loop);
  CheckInputs(ret, osr_phi, T.start, if_false);
}


TEST(Deconstruct_osr_with_body2) {
  OsrDeconstructorTester T(1);

  Node* loop = T.NewOsrLoop(1);

  // Two chained branches in the the body of the loop.
  Node* branch1 = T.graph.NewNode(T.common.Branch(), T.p0, loop);
  Node* if_true1 = T.graph.NewNode(T.common.IfTrue(), branch1);
  Node* if_false1 = T.graph.NewNode(T.common.IfFalse(), branch1);

  Node* branch2 = T.graph.NewNode(T.common.Branch(), T.p0, if_true1);
  Node* if_true2 = T.graph.NewNode(T.common.IfTrue(), branch2);
  Node* if_false2 = T.graph.NewNode(T.common.IfFalse(), branch2);
  loop->ReplaceInput(2, if_true2);

  Node* osr_phi =
      T.NewOsrPhi(loop, T.jsgraph.OneConstant(), 0, T.jsgraph.ZeroConstant());

  Node* merge = T.graph.NewNode(T.common.Merge(2), if_false1, if_false2);
  Node* ret = T.graph.NewNode(T.common.Return(), osr_phi, T.start, merge);
  T.graph.SetEnd(ret);

  T.DeconstructOsr();

  CheckInputs(loop, T.start, if_true2);
  CheckInputs(branch1, T.p0, loop);
  CheckInputs(branch2, T.p0, if_true1);
  CheckInputs(if_true1, branch1);
  CheckInputs(if_false1, branch1);
  CheckInputs(if_true2, branch2);
  CheckInputs(if_false2, branch2);

  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.ZeroConstant(), loop);
  CheckInputs(ret, osr_phi, T.start, merge);
  CheckInputs(merge, if_false1, if_false2);
}


TEST(Deconstruct_osr_with_body3) {
  OsrDeconstructorTester T(1);

  Node* loop = T.NewOsrLoop(2);

  // Two branches that create two different backedges.
  Node* branch1 = T.graph.NewNode(T.common.Branch(), T.p0, loop);
  Node* if_true1 = T.graph.NewNode(T.common.IfTrue(), branch1);
  Node* if_false1 = T.graph.NewNode(T.common.IfFalse(), branch1);

  Node* branch2 = T.graph.NewNode(T.common.Branch(), T.p0, if_true1);
  Node* if_true2 = T.graph.NewNode(T.common.IfTrue(), branch2);
  Node* if_false2 = T.graph.NewNode(T.common.IfFalse(), branch2);
  loop->ReplaceInput(2, if_false1);
  loop->ReplaceInput(3, if_true2);

  Node* osr_phi =
      T.NewOsrPhi(loop, T.jsgraph.OneConstant(), 0, T.jsgraph.ZeroConstant(),
                  T.jsgraph.ZeroConstant());

  Node* ret = T.graph.NewNode(T.common.Return(), osr_phi, T.start, if_false2);
  T.graph.SetEnd(ret);

  T.DeconstructOsr();

  CheckInputs(loop, T.start, if_false1, if_true2);
  CheckInputs(branch1, T.p0, loop);
  CheckInputs(branch2, T.p0, if_true1);
  CheckInputs(if_true1, branch1);
  CheckInputs(if_false1, branch1);
  CheckInputs(if_true2, branch2);
  CheckInputs(if_false2, branch2);

  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.ZeroConstant(),
              T.jsgraph.ZeroConstant(), loop);
  CheckInputs(ret, osr_phi, T.start, if_false2);
}


struct While {
  OsrDeconstructorTester& t;
  Node* branch;
  Node* if_true;
  Node* exit;
  Node* loop;

  While(OsrDeconstructorTester& R, Node* cond, bool is_osr, int backedges = 1)
      : t(R) {
    loop = t.NewLoop(is_osr, backedges);
    branch = t.graph.NewNode(t.common.Branch(), cond, loop);
    if_true = t.graph.NewNode(t.common.IfTrue(), branch);
    exit = t.graph.NewNode(t.common.IfFalse(), branch);
    loop->ReplaceInput(loop->InputCount() - 1, if_true);
  }

  void Nest(While& that) {
    that.loop->ReplaceInput(that.loop->InputCount() - 1, exit);
    this->loop->ReplaceInput(0, that.if_true);
  }

  Node* Phi(Node* i1, Node* i2, Node* i3) {
    if (loop->InputCount() == 2) {
      return t.graph.NewNode(t.common.Phi(MachineRepresentation::kTagged, 2),
                             i1, i2, loop);
    } else {
      return t.graph.NewNode(t.common.Phi(MachineRepresentation::kTagged, 3),
                             i1, i2, i3, loop);
    }
  }
};


static Node* FindSuccessor(Node* node, IrOpcode::Value opcode) {
  for (Node* use : node->uses()) {
    if (use->opcode() == opcode) return use;
  }
  UNREACHABLE();  // should have been found.
  return nullptr;
}


TEST(Deconstruct_osr_nested1) {
  OsrDeconstructorTester T(1);

  While outer(T, T.p0, false);
  While inner(T, T.p0, true);
  inner.Nest(outer);

  Node* outer_phi = outer.Phi(T.p0, T.p0, nullptr);
  outer.branch->ReplaceInput(0, outer_phi);

  Node* osr_phi = inner.Phi(T.jsgraph.TrueConstant(), T.osr_values[0],
                            T.jsgraph.FalseConstant());
  inner.branch->ReplaceInput(0, osr_phi);
  outer_phi->ReplaceInput(1, osr_phi);

  Node* ret =
      T.graph.NewNode(T.common.Return(), outer_phi, T.start, outer.exit);
  Node* end = T.graph.NewNode(T.common.End(1), ret);
  T.graph.SetEnd(end);

  T.DeconstructOsr();

  // Check structure of deconstructed graph.
  // Check inner OSR loop is directly connected to start.
  CheckInputs(inner.loop, T.start, inner.if_true);
  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.FalseConstant(), inner.loop);

  // Check control transfer to copy of outer loop.
  Node* new_outer_loop = FindSuccessor(inner.exit, IrOpcode::kLoop);
  Node* new_outer_phi = FindSuccessor(new_outer_loop, IrOpcode::kPhi);
  CHECK_NE(new_outer_loop, outer.loop);
  CHECK_NE(new_outer_phi, outer_phi);

  CheckInputs(new_outer_loop, inner.exit, new_outer_loop->InputAt(1));

  // Check structure of outer loop.
  Node* new_outer_branch = FindSuccessor(new_outer_loop, IrOpcode::kBranch);
  CHECK_NE(new_outer_branch, outer.branch);
  CheckInputs(new_outer_branch, new_outer_phi, new_outer_loop);
  Node* new_outer_exit = FindSuccessor(new_outer_branch, IrOpcode::kIfFalse);
  Node* new_outer_if_true = FindSuccessor(new_outer_branch, IrOpcode::kIfTrue);

  // Check structure of return.
  end = T.graph.end();
  Node* new_ret = end->InputAt(0);
  CHECK_EQ(IrOpcode::kReturn, new_ret->opcode());
  CheckInputs(new_ret, new_outer_phi, T.start, new_outer_exit);

  // Check structure of inner loop.
  Node* new_inner_loop = FindSuccessor(new_outer_if_true, IrOpcode::kLoop);
  Node* new_inner_phi = FindSuccessor(new_inner_loop, IrOpcode::kPhi);

  CheckInputs(new_inner_phi, T.jsgraph.TrueConstant(),
              T.jsgraph.FalseConstant(), new_inner_loop);
  CheckInputs(new_outer_phi, osr_phi, new_inner_phi, new_outer_loop);
}


TEST(Deconstruct_osr_nested2) {
  OsrDeconstructorTester T(1);

  // Test multiple backedge outer loop.
  While outer(T, T.p0, false, 2);
  While inner(T, T.p0, true);
  inner.Nest(outer);

  Node* outer_phi = outer.Phi(T.p0, T.p0, T.p0);
  outer.branch->ReplaceInput(0, outer_phi);

  Node* osr_phi = inner.Phi(T.jsgraph.TrueConstant(), T.osr_values[0],
                            T.jsgraph.FalseConstant());
  inner.branch->ReplaceInput(0, osr_phi);
  outer_phi->ReplaceInput(1, osr_phi);
  outer_phi->ReplaceInput(2, T.jsgraph.FalseConstant());

  Node* x_branch = T.graph.NewNode(T.common.Branch(), osr_phi, inner.exit);
  Node* x_true = T.graph.NewNode(T.common.IfTrue(), x_branch);
  Node* x_false = T.graph.NewNode(T.common.IfFalse(), x_branch);

  outer.loop->ReplaceInput(1, x_true);
  outer.loop->ReplaceInput(2, x_false);

  Node* ret =
      T.graph.NewNode(T.common.Return(), outer_phi, T.start, outer.exit);
  Node* end = T.graph.NewNode(T.common.End(1), ret);
  T.graph.SetEnd(end);

  T.DeconstructOsr();

  // Check structure of deconstructed graph.
  // Check inner OSR loop is directly connected to start.
  CheckInputs(inner.loop, T.start, inner.if_true);
  CheckInputs(osr_phi, T.osr_values[0], T.jsgraph.FalseConstant(), inner.loop);

  // Check control transfer to copy of outer loop.
  Node* new_merge = FindSuccessor(x_true, IrOpcode::kMerge);
  CHECK_EQ(new_merge, FindSuccessor(x_false, IrOpcode::kMerge));
  CheckInputs(new_merge, x_true, x_false);

  Node* new_outer_loop = FindSuccessor(new_merge, IrOpcode::kLoop);
  Node* new_outer_phi = FindSuccessor(new_outer_loop, IrOpcode::kPhi);
  CHECK_NE(new_outer_loop, outer.loop);
  CHECK_NE(new_outer_phi, outer_phi);

  Node* new_entry_phi = FindSuccessor(new_merge, IrOpcode::kPhi);
  CheckInputs(new_entry_phi, osr_phi, T.jsgraph.FalseConstant(), new_merge);

  CHECK_EQ(new_merge, new_outer_loop->InputAt(0));

  // Check structure of outer loop.
  Node* new_outer_branch = FindSuccessor(new_outer_loop, IrOpcode::kBranch);
  CHECK_NE(new_outer_branch, outer.branch);
  CheckInputs(new_outer_branch, new_outer_phi, new_outer_loop);
  Node* new_outer_exit = FindSuccessor(new_outer_branch, IrOpcode::kIfFalse);
  Node* new_outer_if_true = FindSuccessor(new_outer_branch, IrOpcode::kIfTrue);

  // Check structure of return.
  end = T.graph.end();
  Node* new_ret = end->InputAt(0);
  CHECK_EQ(IrOpcode::kReturn, new_ret->opcode());
  CheckInputs(new_ret, new_outer_phi, T.start, new_outer_exit);

  // Check structure of inner loop.
  Node* new_inner_loop = FindSuccessor(new_outer_if_true, IrOpcode::kLoop);
  Node* new_inner_phi = FindSuccessor(new_inner_loop, IrOpcode::kPhi);

  CheckInputs(new_inner_phi, T.jsgraph.TrueConstant(),
              T.jsgraph.FalseConstant(), new_inner_loop);
  CheckInputs(new_outer_phi, new_entry_phi, new_inner_phi,
              T.jsgraph.FalseConstant(), new_outer_loop);
}


Node* MakeCounter(JSGraph* jsgraph, Node* start, Node* loop) {
  int count = loop->InputCount();
  NodeVector tmp_inputs(jsgraph->graph()->zone());
  for (int i = 0; i < count; i++) {
    tmp_inputs.push_back(start);
  }
  tmp_inputs.push_back(loop);

  Node* phi = jsgraph->graph()->NewNode(
      jsgraph->common()->Phi(MachineRepresentation::kWord32, count), count + 1,
      &tmp_inputs[0]);
  Node* inc = jsgraph->graph()->NewNode(&kIntAdd, phi, jsgraph->OneConstant());

  for (int i = 1; i < count; i++) {
    phi->ReplaceInput(i, inc);
  }
  return phi;
}


TEST(Deconstruct_osr_nested3) {
  OsrDeconstructorTester T(1);

  // outermost loop.
  While loop0(T, T.p0, false, 1);
  Node* loop0_cntr = MakeCounter(&T.jsgraph, T.p0, loop0.loop);
  loop0.branch->ReplaceInput(0, loop0_cntr);

  // middle loop.
  Node* loop1 = T.graph.NewNode(T.common.Loop(1), loop0.if_true);
  Node* loop1_phi =
      T.graph.NewNode(T.common.Phi(MachineRepresentation::kTagged, 2),
                      loop0_cntr, loop0_cntr, loop1);

  // innermost (OSR) loop.
  While loop2(T, T.p0, true, 1);
  loop2.loop->ReplaceInput(0, loop1);

  Node* loop2_cntr = MakeCounter(&T.jsgraph, loop1_phi, loop2.loop);
  loop2_cntr->ReplaceInput(1, T.osr_values[0]);
  Node* osr_phi = loop2_cntr;
  Node* loop2_inc = loop2_cntr->InputAt(2);
  loop2.branch->ReplaceInput(0, loop2_cntr);

  loop1_phi->ReplaceInput(1, loop2_cntr);
  loop0_cntr->ReplaceInput(1, loop2_cntr);

  // Branch to either the outer or middle loop.
  Node* branch = T.graph.NewNode(T.common.Branch(), loop2_cntr, loop2.exit);
  Node* if_true = T.graph.NewNode(T.common.IfTrue(), branch);
  Node* if_false = T.graph.NewNode(T.common.IfFalse(), branch);

  loop0.loop->ReplaceInput(1, if_true);
  loop1->AppendInput(T.graph.zone(), if_false);
  NodeProperties::ChangeOp(loop1, T.common.Loop(2));

  Node* ret =
      T.graph.NewNode(T.common.Return(), loop0_cntr, T.start, loop0.exit);
  Node* end = T.graph.NewNode(T.common.End(1), ret);
  T.graph.SetEnd(end);

  T.DeconstructOsr();

  // Check structure of deconstructed graph.
  // Check loop2 (OSR loop) is directly connected to start.
  CheckInputs(loop2.loop, T.start, loop2.if_true);
  CheckInputs(osr_phi, T.osr_values[0], loop2_inc, loop2.loop);
  CheckInputs(loop2.branch, osr_phi, loop2.loop);
  CheckInputs(loop2.if_true, loop2.branch);
  CheckInputs(loop2.exit, loop2.branch);
  CheckInputs(branch, osr_phi, loop2.exit);
  CheckInputs(if_true, branch);
  CheckInputs(if_false, branch);

  // Check structure of new_loop1.
  Node* new_loop1_loop = FindSuccessor(if_false, IrOpcode::kLoop);
  // TODO(titzer): check the internal copy of loop2.
  USE(new_loop1_loop);

  // Check structure of new_loop0.
  Node* new_loop0_loop_entry = FindSuccessor(if_true, IrOpcode::kMerge);
  Node* new_loop0_loop = FindSuccessor(new_loop0_loop_entry, IrOpcode::kLoop);
  // TODO(titzer): check the internal copies of loop1 and loop2.

  Node* new_loop0_branch = FindSuccessor(new_loop0_loop, IrOpcode::kBranch);
  Node* new_loop0_if_true = FindSuccessor(new_loop0_branch, IrOpcode::kIfTrue);
  Node* new_loop0_exit = FindSuccessor(new_loop0_branch, IrOpcode::kIfFalse);

  USE(new_loop0_if_true);

  Node* new_ret = T.graph.end()->InputAt(0);
  CHECK_EQ(IrOpcode::kReturn, new_ret->opcode());

  Node* new_loop0_phi = new_ret->InputAt(0);
  CHECK_EQ(IrOpcode::kPhi, new_loop0_phi->opcode());
  CHECK_EQ(new_loop0_loop, NodeProperties::GetControlInput(new_loop0_phi));
  CHECK_EQ(new_loop0_phi, FindSuccessor(new_loop0_loop, IrOpcode::kPhi));

  // Check that the return returns the phi from the OSR loop and control
  // depends on the copy of the outer loop0.
  CheckInputs(new_ret, new_loop0_phi, T.graph.start(), new_loop0_exit);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
