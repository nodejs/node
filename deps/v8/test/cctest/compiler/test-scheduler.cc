// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

struct TestLoop {
  int count;
  BasicBlock** nodes;
  BasicBlock* header() { return nodes[0]; }
  BasicBlock* last() { return nodes[count - 1]; }
  ~TestLoop() { delete[] nodes; }
};


static TestLoop* CreateLoop(Schedule* schedule, int count) {
  TestLoop* loop = new TestLoop();
  loop->count = count;
  loop->nodes = new BasicBlock* [count];
  for (int i = 0; i < count; i++) {
    loop->nodes[i] = schedule->NewBasicBlock();
    if (i > 0) schedule->AddSuccessor(loop->nodes[i - 1], loop->nodes[i]);
  }
  schedule->AddSuccessor(loop->nodes[count - 1], loop->nodes[0]);
  return loop;
}


static void CheckRPONumbers(BasicBlockVector* order, int expected,
                            bool loops_allowed) {
  CHECK_EQ(expected, static_cast<int>(order->size()));
  for (int i = 0; i < static_cast<int>(order->size()); i++) {
    CHECK(order->at(i)->rpo_number_ == i);
    if (!loops_allowed) CHECK_LT(order->at(i)->loop_end_, 0);
  }
}


static void CheckLoopContains(BasicBlock** blocks, int body_size) {
  BasicBlock* header = blocks[0];
  CHECK_GT(header->loop_end_, 0);
  CHECK_EQ(body_size, (header->loop_end_ - header->rpo_number_));
  for (int i = 0; i < body_size; i++) {
    int num = blocks[i]->rpo_number_;
    CHECK(num >= header->rpo_number_ && num < header->loop_end_);
    CHECK(header->LoopContains(blocks[i]));
    CHECK(header->IsLoopHeader() || blocks[i]->loop_header_ == header);
  }
}


TEST(RPODegenerate1) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 1, false);
  CHECK_EQ(schedule.entry(), order->at(0));
}


TEST(RPODegenerate2) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  schedule.AddGoto(schedule.entry(), schedule.exit());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 2, false);
  CHECK_EQ(schedule.entry(), order->at(0));
  CHECK_EQ(schedule.exit(), order->at(1));
}


TEST(RPOLine) {
  HandleAndZoneScope scope;

  for (int i = 0; i < 10; i++) {
    Schedule schedule(scope.main_zone());

    BasicBlock* last = schedule.entry();
    for (int j = 0; j < i; j++) {
      BasicBlock* block = schedule.NewBasicBlock();
      schedule.AddGoto(last, block);
      last = block;
    }
    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
    CheckRPONumbers(order, 1 + i, false);

    Schedule::BasicBlocks blocks(schedule.all_blocks());
    for (Schedule::BasicBlocks::iterator iter = blocks.begin();
         iter != blocks.end(); ++iter) {
      BasicBlock* block = *iter;
      if (block->rpo_number_ >= 0 && block->SuccessorCount() == 1) {
        CHECK(block->rpo_number_ + 1 == block->SuccessorAt(0)->rpo_number_);
      }
    }
  }
}


TEST(RPOSelfLoop) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  schedule.AddSuccessor(schedule.entry(), schedule.entry());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 1, true);
  BasicBlock* loop[] = {schedule.entry()};
  CheckLoopContains(loop, 1);
}


TEST(RPOEntryLoop) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  schedule.AddSuccessor(schedule.entry(), schedule.exit());
  schedule.AddSuccessor(schedule.exit(), schedule.entry());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 2, true);
  BasicBlock* loop[] = {schedule.entry(), schedule.exit()};
  CheckLoopContains(loop, 2);
}


TEST(RPOEndLoop) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 2));
  schedule.AddSuccessor(schedule.entry(), loop1->header());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 3, true);
  CheckLoopContains(loop1->nodes, loop1->count);
}


TEST(RPOEndLoopNested) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 2));
  schedule.AddSuccessor(schedule.entry(), loop1->header());
  schedule.AddSuccessor(loop1->last(), schedule.entry());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 3, true);
  CheckLoopContains(loop1->nodes, loop1->count);
}


TEST(RPODiamond) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.exit();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(A, C);
  schedule.AddSuccessor(B, D);
  schedule.AddSuccessor(C, D);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 4, false);

  CHECK_EQ(0, A->rpo_number_);
  CHECK((B->rpo_number_ == 1 && C->rpo_number_ == 2) ||
        (B->rpo_number_ == 2 && C->rpo_number_ == 1));
  CHECK_EQ(3, D->rpo_number_);
}


TEST(RPOLoop1) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.exit();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(B, C);
  schedule.AddSuccessor(C, B);
  schedule.AddSuccessor(C, D);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 4, true);
  BasicBlock* loop[] = {B, C};
  CheckLoopContains(loop, 2);
}


TEST(RPOLoop2) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.exit();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(B, C);
  schedule.AddSuccessor(C, B);
  schedule.AddSuccessor(B, D);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 4, true);
  BasicBlock* loop[] = {B, C};
  CheckLoopContains(loop, 2);
}


TEST(RPOLoopN) {
  HandleAndZoneScope scope;

  for (int i = 0; i < 11; i++) {
    Schedule schedule(scope.main_zone());
    BasicBlock* A = schedule.entry();
    BasicBlock* B = schedule.NewBasicBlock();
    BasicBlock* C = schedule.NewBasicBlock();
    BasicBlock* D = schedule.NewBasicBlock();
    BasicBlock* E = schedule.NewBasicBlock();
    BasicBlock* F = schedule.NewBasicBlock();
    BasicBlock* G = schedule.exit();

    schedule.AddSuccessor(A, B);
    schedule.AddSuccessor(B, C);
    schedule.AddSuccessor(C, D);
    schedule.AddSuccessor(D, E);
    schedule.AddSuccessor(E, F);
    schedule.AddSuccessor(F, B);
    schedule.AddSuccessor(B, G);

    // Throw in extra backedges from time to time.
    if (i == 1) schedule.AddSuccessor(B, B);
    if (i == 2) schedule.AddSuccessor(C, B);
    if (i == 3) schedule.AddSuccessor(D, B);
    if (i == 4) schedule.AddSuccessor(E, B);
    if (i == 5) schedule.AddSuccessor(F, B);

    // Throw in extra loop exits from time to time.
    if (i == 6) schedule.AddSuccessor(B, G);
    if (i == 7) schedule.AddSuccessor(C, G);
    if (i == 8) schedule.AddSuccessor(D, G);
    if (i == 9) schedule.AddSuccessor(E, G);
    if (i == 10) schedule.AddSuccessor(F, G);

    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
    CheckRPONumbers(order, 7, true);
    BasicBlock* loop[] = {B, C, D, E, F};
    CheckLoopContains(loop, 5);
  }
}


TEST(RPOLoopNest1) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.NewBasicBlock();
  BasicBlock* E = schedule.NewBasicBlock();
  BasicBlock* F = schedule.exit();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(B, C);
  schedule.AddSuccessor(C, D);
  schedule.AddSuccessor(D, C);
  schedule.AddSuccessor(D, E);
  schedule.AddSuccessor(E, B);
  schedule.AddSuccessor(E, F);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 6, true);
  BasicBlock* loop1[] = {B, C, D, E};
  CheckLoopContains(loop1, 4);

  BasicBlock* loop2[] = {C, D};
  CheckLoopContains(loop2, 2);
}


TEST(RPOLoopNest2) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.NewBasicBlock();
  BasicBlock* E = schedule.NewBasicBlock();
  BasicBlock* F = schedule.NewBasicBlock();
  BasicBlock* G = schedule.NewBasicBlock();
  BasicBlock* H = schedule.exit();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(B, C);
  schedule.AddSuccessor(C, D);
  schedule.AddSuccessor(D, E);
  schedule.AddSuccessor(E, F);
  schedule.AddSuccessor(F, G);
  schedule.AddSuccessor(G, H);

  schedule.AddSuccessor(E, D);
  schedule.AddSuccessor(F, C);
  schedule.AddSuccessor(G, B);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 8, true);
  BasicBlock* loop1[] = {B, C, D, E, F, G};
  CheckLoopContains(loop1, 6);

  BasicBlock* loop2[] = {C, D, E, F};
  CheckLoopContains(loop2, 4);

  BasicBlock* loop3[] = {D, E};
  CheckLoopContains(loop3, 2);
}


TEST(RPOLoopFollow1) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 1));
  SmartPointer<TestLoop> loop2(CreateLoop(&schedule, 1));

  BasicBlock* A = schedule.entry();
  BasicBlock* E = schedule.exit();

  schedule.AddSuccessor(A, loop1->header());
  schedule.AddSuccessor(loop1->header(), loop2->header());
  schedule.AddSuccessor(loop2->last(), E);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);

  CheckLoopContains(loop1->nodes, loop1->count);

  CHECK_EQ(schedule.BasicBlockCount(), static_cast<int>(order->size()));
  CheckLoopContains(loop1->nodes, loop1->count);
  CheckLoopContains(loop2->nodes, loop2->count);
}


TEST(RPOLoopFollow2) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 1));
  SmartPointer<TestLoop> loop2(CreateLoop(&schedule, 1));

  BasicBlock* A = schedule.entry();
  BasicBlock* S = schedule.NewBasicBlock();
  BasicBlock* E = schedule.exit();

  schedule.AddSuccessor(A, loop1->header());
  schedule.AddSuccessor(loop1->header(), S);
  schedule.AddSuccessor(S, loop2->header());
  schedule.AddSuccessor(loop2->last(), E);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);

  CheckLoopContains(loop1->nodes, loop1->count);

  CHECK_EQ(schedule.BasicBlockCount(), static_cast<int>(order->size()));
  CheckLoopContains(loop1->nodes, loop1->count);
  CheckLoopContains(loop2->nodes, loop2->count);
}


TEST(RPOLoopFollowN) {
  HandleAndZoneScope scope;

  for (int size = 1; size < 5; size++) {
    for (int exit = 0; exit < size; exit++) {
      Schedule schedule(scope.main_zone());
      SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
      SmartPointer<TestLoop> loop2(CreateLoop(&schedule, size));
      BasicBlock* A = schedule.entry();
      BasicBlock* E = schedule.exit();

      schedule.AddSuccessor(A, loop1->header());
      schedule.AddSuccessor(loop1->nodes[exit], loop2->header());
      schedule.AddSuccessor(loop2->nodes[exit], E);
      BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
      CheckLoopContains(loop1->nodes, loop1->count);

      CHECK_EQ(schedule.BasicBlockCount(), static_cast<int>(order->size()));
      CheckLoopContains(loop1->nodes, loop1->count);
      CheckLoopContains(loop2->nodes, loop2->count);
    }
  }
}


TEST(RPONestedLoopFollow1) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 1));
  SmartPointer<TestLoop> loop2(CreateLoop(&schedule, 1));

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* E = schedule.exit();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(B, loop1->header());
  schedule.AddSuccessor(loop1->header(), loop2->header());
  schedule.AddSuccessor(loop2->last(), C);
  schedule.AddSuccessor(C, E);
  schedule.AddSuccessor(C, B);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);

  CheckLoopContains(loop1->nodes, loop1->count);

  CHECK_EQ(schedule.BasicBlockCount(), static_cast<int>(order->size()));
  CheckLoopContains(loop1->nodes, loop1->count);
  CheckLoopContains(loop2->nodes, loop2->count);

  BasicBlock* loop3[] = {B, loop1->nodes[0], loop2->nodes[0], C};
  CheckLoopContains(loop3, 4);
}


TEST(RPOLoopBackedges1) {
  HandleAndZoneScope scope;

  int size = 8;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      Schedule schedule(scope.main_zone());
      BasicBlock* A = schedule.entry();
      BasicBlock* E = schedule.exit();

      SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
      schedule.AddSuccessor(A, loop1->header());
      schedule.AddSuccessor(loop1->last(), E);

      schedule.AddSuccessor(loop1->nodes[i], loop1->header());
      schedule.AddSuccessor(loop1->nodes[j], E);

      BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
      CheckRPONumbers(order, schedule.BasicBlockCount(), true);
      CheckLoopContains(loop1->nodes, loop1->count);
    }
  }
}


TEST(RPOLoopOutedges1) {
  HandleAndZoneScope scope;

  int size = 8;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      Schedule schedule(scope.main_zone());
      BasicBlock* A = schedule.entry();
      BasicBlock* D = schedule.NewBasicBlock();
      BasicBlock* E = schedule.exit();

      SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
      schedule.AddSuccessor(A, loop1->header());
      schedule.AddSuccessor(loop1->last(), E);

      schedule.AddSuccessor(loop1->nodes[i], loop1->header());
      schedule.AddSuccessor(loop1->nodes[j], D);
      schedule.AddSuccessor(D, E);

      BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
      CheckRPONumbers(order, schedule.BasicBlockCount(), true);
      CheckLoopContains(loop1->nodes, loop1->count);
    }
  }
}


TEST(RPOLoopOutedges2) {
  HandleAndZoneScope scope;

  int size = 8;
  for (int i = 0; i < size; i++) {
    Schedule schedule(scope.main_zone());
    BasicBlock* A = schedule.entry();
    BasicBlock* E = schedule.exit();

    SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
    schedule.AddSuccessor(A, loop1->header());
    schedule.AddSuccessor(loop1->last(), E);

    for (int j = 0; j < size; j++) {
      BasicBlock* O = schedule.NewBasicBlock();
      schedule.AddSuccessor(loop1->nodes[j], O);
      schedule.AddSuccessor(O, E);
    }

    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
    CheckRPONumbers(order, schedule.BasicBlockCount(), true);
    CheckLoopContains(loop1->nodes, loop1->count);
  }
}


TEST(RPOLoopOutloops1) {
  HandleAndZoneScope scope;

  int size = 8;
  for (int i = 0; i < size; i++) {
    Schedule schedule(scope.main_zone());
    BasicBlock* A = schedule.entry();
    BasicBlock* E = schedule.exit();
    SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
    schedule.AddSuccessor(A, loop1->header());
    schedule.AddSuccessor(loop1->last(), E);

    TestLoop** loopN = new TestLoop* [size];
    for (int j = 0; j < size; j++) {
      loopN[j] = CreateLoop(&schedule, 2);
      schedule.AddSuccessor(loop1->nodes[j], loopN[j]->header());
      schedule.AddSuccessor(loopN[j]->last(), E);
    }

    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
    CheckRPONumbers(order, schedule.BasicBlockCount(), true);
    CheckLoopContains(loop1->nodes, loop1->count);

    for (int j = 0; j < size; j++) {
      CheckLoopContains(loopN[j]->nodes, loopN[j]->count);
      delete loopN[j];
    }
    delete[] loopN;
  }
}


TEST(RPOLoopMultibackedge) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* A = schedule.entry();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.exit();
  BasicBlock* E = schedule.NewBasicBlock();

  schedule.AddSuccessor(A, B);
  schedule.AddSuccessor(B, C);
  schedule.AddSuccessor(B, D);
  schedule.AddSuccessor(B, E);
  schedule.AddSuccessor(C, B);
  schedule.AddSuccessor(D, B);
  schedule.AddSuccessor(E, B);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(&schedule);
  CheckRPONumbers(order, 5, true);

  BasicBlock* loop1[] = {B, C, D, E};
  CheckLoopContains(loop1, 4);
}


TEST(BuildScheduleEmpty) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder builder(scope.main_zone());
  graph.SetStart(graph.NewNode(builder.Start(0)));
  graph.SetEnd(graph.NewNode(builder.End(), graph.start()));

  USE(Scheduler::ComputeSchedule(&graph));
}


TEST(BuildScheduleOneParameter) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder builder(scope.main_zone());
  graph.SetStart(graph.NewNode(builder.Start(0)));

  Node* p1 = graph.NewNode(builder.Parameter(0), graph.start());
  Node* ret = graph.NewNode(builder.Return(), p1, graph.start(), graph.start());

  graph.SetEnd(graph.NewNode(builder.End(), ret));

  USE(Scheduler::ComputeSchedule(&graph));
}


static int GetScheduledNodeCount(Schedule* schedule) {
  int node_count = 0;
  for (BasicBlockVectorIter i = schedule->rpo_order()->begin();
       i != schedule->rpo_order()->end(); ++i) {
    BasicBlock* block = *i;
    for (BasicBlock::const_iterator j = block->begin(); j != block->end();
         ++j) {
      ++node_count;
    }
    BasicBlock::Control control = block->control_;
    if (control != BasicBlock::kNone) {
      ++node_count;
    }
  }
  return node_count;
}


static void PrintGraph(Graph* graph) {
  OFStream os(stdout);
  os << AsDOT(*graph);
}


static void PrintSchedule(Schedule* schedule) {
  OFStream os(stdout);
  os << *schedule << endl;
}


TEST(BuildScheduleIfSplit) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder builder(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());
  graph.SetStart(graph.NewNode(builder.Start(3)));

  Node* p1 = graph.NewNode(builder.Parameter(0), graph.start());
  Node* p2 = graph.NewNode(builder.Parameter(1), graph.start());
  Node* p3 = graph.NewNode(builder.Parameter(2), graph.start());
  Node* p4 = graph.NewNode(builder.Parameter(3), graph.start());
  Node* p5 = graph.NewNode(builder.Parameter(4), graph.start());
  Node* cmp = graph.NewNode(js_builder.LessThanOrEqual(), p1, p2, p3,
                            graph.start(), graph.start());
  Node* branch = graph.NewNode(builder.Branch(), cmp, graph.start());
  Node* true_branch = graph.NewNode(builder.IfTrue(), branch);
  Node* false_branch = graph.NewNode(builder.IfFalse(), branch);

  Node* ret1 = graph.NewNode(builder.Return(), p4, graph.start(), true_branch);
  Node* ret2 = graph.NewNode(builder.Return(), p5, graph.start(), false_branch);
  Node* merge = graph.NewNode(builder.Merge(2), ret1, ret2);
  graph.SetEnd(graph.NewNode(builder.End(), merge));

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);


  CHECK_EQ(13, GetScheduledNodeCount(schedule));
}


TEST(BuildScheduleIfSplitWithEffects) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common_builder(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());
  Operator* op;

  Handle<Object> object =
      Handle<Object>(isolate->heap()->undefined_value(), isolate);
  PrintableUnique<Object> unique_constant =
      PrintableUnique<Object>::CreateUninitialized(scope.main_zone(), object);

  // Manually transcripted code for:
  // function turbo_fan_test(a, b, c, y) {
  //   if (a < b) {
  //     return a + b - c * c - a + y;
  //   } else {
  //     return c * c - a;
  //   }
  // }
  op = common_builder.Start(0);
  Node* n0 = graph.NewNode(op);
  USE(n0);
  Node* nil = graph.NewNode(common_builder.Dead());
  op = common_builder.End();
  Node* n23 = graph.NewNode(op, nil);
  USE(n23);
  op = common_builder.Merge(2);
  Node* n22 = graph.NewNode(op, nil, nil);
  USE(n22);
  op = common_builder.Return();
  Node* n16 = graph.NewNode(op, nil, nil, nil);
  USE(n16);
  op = js_builder.Add();
  Node* n15 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n15);
  op = js_builder.Subtract();
  Node* n14 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n14);
  op = js_builder.Subtract();
  Node* n13 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n13);
  op = js_builder.Add();
  Node* n11 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n11);
  op = common_builder.Parameter(0);
  Node* n2 = graph.NewNode(op, n0);
  USE(n2);
  n11->ReplaceInput(0, n2);
  op = common_builder.Parameter(0);
  Node* n3 = graph.NewNode(op, n0);
  USE(n3);
  n11->ReplaceInput(1, n3);
  op = common_builder.HeapConstant(unique_constant);
  Node* n7 = graph.NewNode(op);
  USE(n7);
  n11->ReplaceInput(2, n7);
  op = js_builder.LessThan();
  Node* n8 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n8);
  n8->ReplaceInput(0, n2);
  n8->ReplaceInput(1, n3);
  n8->ReplaceInput(2, n7);
  n8->ReplaceInput(3, n0);
  n8->ReplaceInput(4, n0);
  n11->ReplaceInput(3, n8);
  op = common_builder.IfTrue();
  Node* n10 = graph.NewNode(op, nil);
  USE(n10);
  op = common_builder.Branch();
  Node* n9 = graph.NewNode(op, nil, nil);
  USE(n9);
  n9->ReplaceInput(0, n8);
  n9->ReplaceInput(1, n0);
  n10->ReplaceInput(0, n9);
  n11->ReplaceInput(4, n10);
  n13->ReplaceInput(0, n11);
  op = js_builder.Multiply();
  Node* n12 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n12);
  op = common_builder.Parameter(0);
  Node* n4 = graph.NewNode(op, n0);
  USE(n4);
  n12->ReplaceInput(0, n4);
  n12->ReplaceInput(1, n4);
  n12->ReplaceInput(2, n7);
  n12->ReplaceInput(3, n11);
  n12->ReplaceInput(4, n10);
  n13->ReplaceInput(1, n12);
  n13->ReplaceInput(2, n7);
  n13->ReplaceInput(3, n12);
  n13->ReplaceInput(4, n10);
  n14->ReplaceInput(0, n13);
  n14->ReplaceInput(1, n2);
  n14->ReplaceInput(2, n7);
  n14->ReplaceInput(3, n13);
  n14->ReplaceInput(4, n10);
  n15->ReplaceInput(0, n14);
  op = common_builder.Parameter(0);
  Node* n5 = graph.NewNode(op, n0);
  USE(n5);
  n15->ReplaceInput(1, n5);
  n15->ReplaceInput(2, n7);
  n15->ReplaceInput(3, n14);
  n15->ReplaceInput(4, n10);
  n16->ReplaceInput(0, n15);
  n16->ReplaceInput(1, n15);
  n16->ReplaceInput(2, n10);
  n22->ReplaceInput(0, n16);
  op = common_builder.Return();
  Node* n21 = graph.NewNode(op, nil, nil, nil);
  USE(n21);
  op = js_builder.Subtract();
  Node* n20 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n20);
  op = js_builder.Multiply();
  Node* n19 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n19);
  n19->ReplaceInput(0, n4);
  n19->ReplaceInput(1, n4);
  n19->ReplaceInput(2, n7);
  n19->ReplaceInput(3, n8);
  op = common_builder.IfFalse();
  Node* n18 = graph.NewNode(op, nil);
  USE(n18);
  n18->ReplaceInput(0, n9);
  n19->ReplaceInput(4, n18);
  n20->ReplaceInput(0, n19);
  n20->ReplaceInput(1, n2);
  n20->ReplaceInput(2, n7);
  n20->ReplaceInput(3, n19);
  n20->ReplaceInput(4, n18);
  n21->ReplaceInput(0, n20);
  n21->ReplaceInput(1, n20);
  n21->ReplaceInput(2, n18);
  n22->ReplaceInput(1, n21);
  n23->ReplaceInput(0, n22);

  graph.SetStart(n0);
  graph.SetEnd(n23);

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);

  CHECK_EQ(20, GetScheduledNodeCount(schedule));
}


TEST(BuildScheduleSimpleLoop) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common_builder(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());
  Operator* op;

  Handle<Object> object =
      Handle<Object>(isolate->heap()->undefined_value(), isolate);
  PrintableUnique<Object> unique_constant =
      PrintableUnique<Object>::CreateUninitialized(scope.main_zone(), object);

  // Manually transcripted code for:
  // function turbo_fan_test(a, b) {
  //   while (a < b) {
  //     a++;
  //   }
  //   return a;
  // }
  op = common_builder.Start(0);
  Node* n0 = graph.NewNode(op);
  USE(n0);
  Node* nil = graph.NewNode(common_builder.Dead());
  op = common_builder.End();
  Node* n20 = graph.NewNode(op, nil);
  USE(n20);
  op = common_builder.Return();
  Node* n19 = graph.NewNode(op, nil, nil, nil);
  USE(n19);
  op = common_builder.Phi(2);
  Node* n8 = graph.NewNode(op, nil, nil, nil);
  USE(n8);
  op = common_builder.Parameter(0);
  Node* n2 = graph.NewNode(op, n0);
  USE(n2);
  n8->ReplaceInput(0, n2);
  op = js_builder.Add();
  Node* n18 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n18);
  op = js_builder.ToNumber();
  Node* n16 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n16);
  n16->ReplaceInput(0, n8);
  op = common_builder.HeapConstant(unique_constant);
  Node* n5 = graph.NewNode(op);
  USE(n5);
  n16->ReplaceInput(1, n5);
  op = js_builder.LessThan();
  Node* n12 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n12);
  n12->ReplaceInput(0, n8);
  op = common_builder.Phi(2);
  Node* n9 = graph.NewNode(op, nil, nil, nil);
  USE(n9);
  op = common_builder.Parameter(0);
  Node* n3 = graph.NewNode(op, n0);
  USE(n3);
  n9->ReplaceInput(0, n3);
  n9->ReplaceInput(1, n9);
  op = common_builder.Loop(2);
  Node* n6 = graph.NewNode(op, nil, nil);
  USE(n6);
  n6->ReplaceInput(0, n0);
  op = common_builder.IfTrue();
  Node* n14 = graph.NewNode(op, nil);
  USE(n14);
  op = common_builder.Branch();
  Node* n13 = graph.NewNode(op, nil, nil);
  USE(n13);
  n13->ReplaceInput(0, n12);
  n13->ReplaceInput(1, n6);
  n14->ReplaceInput(0, n13);
  n6->ReplaceInput(1, n14);
  n9->ReplaceInput(2, n6);
  n12->ReplaceInput(1, n9);
  n12->ReplaceInput(2, n5);
  op = common_builder.Phi(2);
  Node* n10 = graph.NewNode(op, nil, nil, nil);
  USE(n10);
  n10->ReplaceInput(0, n0);
  n10->ReplaceInput(1, n18);
  n10->ReplaceInput(2, n6);
  n12->ReplaceInput(3, n10);
  n12->ReplaceInput(4, n6);
  n16->ReplaceInput(2, n12);
  n16->ReplaceInput(3, n14);
  n18->ReplaceInput(0, n16);
  op = common_builder.NumberConstant(0);
  Node* n17 = graph.NewNode(op);
  USE(n17);
  n18->ReplaceInput(1, n17);
  n18->ReplaceInput(2, n5);
  n18->ReplaceInput(3, n16);
  n18->ReplaceInput(4, n14);
  n8->ReplaceInput(1, n18);
  n8->ReplaceInput(2, n6);
  n19->ReplaceInput(0, n8);
  n19->ReplaceInput(1, n12);
  op = common_builder.IfFalse();
  Node* n15 = graph.NewNode(op, nil);
  USE(n15);
  n15->ReplaceInput(0, n13);
  n19->ReplaceInput(2, n15);
  n20->ReplaceInput(0, n19);

  graph.SetStart(n0);
  graph.SetEnd(n20);

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);

  CHECK_EQ(19, GetScheduledNodeCount(schedule));
}


TEST(BuildScheduleComplexLoops) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common_builder(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());
  Operator* op;

  Handle<Object> object =
      Handle<Object>(isolate->heap()->undefined_value(), isolate);
  PrintableUnique<Object> unique_constant =
      PrintableUnique<Object>::CreateUninitialized(scope.main_zone(), object);

  // Manually transcripted code for:
  // function turbo_fan_test(a, b, c) {
  //   while (a < b) {
  //     a++;
  //     while (c < b) {
  //       c++;
  //     }
  //   }
  //   while (a < b) {
  //     a += 2;
  //   }
  //   return a;
  // }
  op = common_builder.Start(0);
  Node* n0 = graph.NewNode(op);
  USE(n0);
  Node* nil = graph.NewNode(common_builder.Dead());
  op = common_builder.End();
  Node* n46 = graph.NewNode(op, nil);
  USE(n46);
  op = common_builder.Return();
  Node* n45 = graph.NewNode(op, nil, nil, nil);
  USE(n45);
  op = common_builder.Phi(2);
  Node* n35 = graph.NewNode(op, nil, nil, nil);
  USE(n35);
  op = common_builder.Phi(2);
  Node* n9 = graph.NewNode(op, nil, nil, nil);
  USE(n9);
  op = common_builder.Parameter(0);
  Node* n2 = graph.NewNode(op, n0);
  USE(n2);
  n9->ReplaceInput(0, n2);
  op = common_builder.Phi(2);
  Node* n23 = graph.NewNode(op, nil, nil, nil);
  USE(n23);
  op = js_builder.Add();
  Node* n20 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n20);
  op = js_builder.ToNumber();
  Node* n18 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n18);
  n18->ReplaceInput(0, n9);
  op = common_builder.HeapConstant(unique_constant);
  Node* n6 = graph.NewNode(op);
  USE(n6);
  n18->ReplaceInput(1, n6);
  op = js_builder.LessThan();
  Node* n14 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n14);
  n14->ReplaceInput(0, n9);
  op = common_builder.Phi(2);
  Node* n10 = graph.NewNode(op, nil, nil, nil);
  USE(n10);
  op = common_builder.Parameter(0);
  Node* n3 = graph.NewNode(op, n0);
  USE(n3);
  n10->ReplaceInput(0, n3);
  op = common_builder.Phi(2);
  Node* n24 = graph.NewNode(op, nil, nil, nil);
  USE(n24);
  n24->ReplaceInput(0, n10);
  n24->ReplaceInput(1, n24);
  op = common_builder.Loop(2);
  Node* n21 = graph.NewNode(op, nil, nil);
  USE(n21);
  op = common_builder.IfTrue();
  Node* n16 = graph.NewNode(op, nil);
  USE(n16);
  op = common_builder.Branch();
  Node* n15 = graph.NewNode(op, nil, nil);
  USE(n15);
  n15->ReplaceInput(0, n14);
  op = common_builder.Loop(2);
  Node* n7 = graph.NewNode(op, nil, nil);
  USE(n7);
  n7->ReplaceInput(0, n0);
  op = common_builder.IfFalse();
  Node* n30 = graph.NewNode(op, nil);
  USE(n30);
  op = common_builder.Branch();
  Node* n28 = graph.NewNode(op, nil, nil);
  USE(n28);
  op = js_builder.LessThan();
  Node* n27 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n27);
  op = common_builder.Phi(2);
  Node* n25 = graph.NewNode(op, nil, nil, nil);
  USE(n25);
  op = common_builder.Phi(2);
  Node* n11 = graph.NewNode(op, nil, nil, nil);
  USE(n11);
  op = common_builder.Parameter(0);
  Node* n4 = graph.NewNode(op, n0);
  USE(n4);
  n11->ReplaceInput(0, n4);
  n11->ReplaceInput(1, n25);
  n11->ReplaceInput(2, n7);
  n25->ReplaceInput(0, n11);
  op = js_builder.Add();
  Node* n32 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n32);
  op = js_builder.ToNumber();
  Node* n31 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n31);
  n31->ReplaceInput(0, n25);
  n31->ReplaceInput(1, n6);
  n31->ReplaceInput(2, n27);
  op = common_builder.IfTrue();
  Node* n29 = graph.NewNode(op, nil);
  USE(n29);
  n29->ReplaceInput(0, n28);
  n31->ReplaceInput(3, n29);
  n32->ReplaceInput(0, n31);
  op = common_builder.NumberConstant(0);
  Node* n19 = graph.NewNode(op);
  USE(n19);
  n32->ReplaceInput(1, n19);
  n32->ReplaceInput(2, n6);
  n32->ReplaceInput(3, n31);
  n32->ReplaceInput(4, n29);
  n25->ReplaceInput(1, n32);
  n25->ReplaceInput(2, n21);
  n27->ReplaceInput(0, n25);
  n27->ReplaceInput(1, n24);
  n27->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n26 = graph.NewNode(op, nil, nil, nil);
  USE(n26);
  n26->ReplaceInput(0, n20);
  n26->ReplaceInput(1, n32);
  n26->ReplaceInput(2, n21);
  n27->ReplaceInput(3, n26);
  n27->ReplaceInput(4, n21);
  n28->ReplaceInput(0, n27);
  n28->ReplaceInput(1, n21);
  n30->ReplaceInput(0, n28);
  n7->ReplaceInput(1, n30);
  n15->ReplaceInput(1, n7);
  n16->ReplaceInput(0, n15);
  n21->ReplaceInput(0, n16);
  n21->ReplaceInput(1, n29);
  n24->ReplaceInput(2, n21);
  n10->ReplaceInput(1, n24);
  n10->ReplaceInput(2, n7);
  n14->ReplaceInput(1, n10);
  n14->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n12 = graph.NewNode(op, nil, nil, nil);
  USE(n12);
  n12->ReplaceInput(0, n0);
  n12->ReplaceInput(1, n27);
  n12->ReplaceInput(2, n7);
  n14->ReplaceInput(3, n12);
  n14->ReplaceInput(4, n7);
  n18->ReplaceInput(2, n14);
  n18->ReplaceInput(3, n16);
  n20->ReplaceInput(0, n18);
  n20->ReplaceInput(1, n19);
  n20->ReplaceInput(2, n6);
  n20->ReplaceInput(3, n18);
  n20->ReplaceInput(4, n16);
  n23->ReplaceInput(0, n20);
  n23->ReplaceInput(1, n23);
  n23->ReplaceInput(2, n21);
  n9->ReplaceInput(1, n23);
  n9->ReplaceInput(2, n7);
  n35->ReplaceInput(0, n9);
  op = js_builder.Add();
  Node* n44 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n44);
  n44->ReplaceInput(0, n35);
  op = common_builder.NumberConstant(0);
  Node* n43 = graph.NewNode(op);
  USE(n43);
  n44->ReplaceInput(1, n43);
  n44->ReplaceInput(2, n6);
  op = js_builder.LessThan();
  Node* n39 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n39);
  n39->ReplaceInput(0, n35);
  op = common_builder.Phi(2);
  Node* n36 = graph.NewNode(op, nil, nil, nil);
  USE(n36);
  n36->ReplaceInput(0, n10);
  n36->ReplaceInput(1, n36);
  op = common_builder.Loop(2);
  Node* n33 = graph.NewNode(op, nil, nil);
  USE(n33);
  op = common_builder.IfFalse();
  Node* n17 = graph.NewNode(op, nil);
  USE(n17);
  n17->ReplaceInput(0, n15);
  n33->ReplaceInput(0, n17);
  op = common_builder.IfTrue();
  Node* n41 = graph.NewNode(op, nil);
  USE(n41);
  op = common_builder.Branch();
  Node* n40 = graph.NewNode(op, nil, nil);
  USE(n40);
  n40->ReplaceInput(0, n39);
  n40->ReplaceInput(1, n33);
  n41->ReplaceInput(0, n40);
  n33->ReplaceInput(1, n41);
  n36->ReplaceInput(2, n33);
  n39->ReplaceInput(1, n36);
  n39->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n38 = graph.NewNode(op, nil, nil, nil);
  USE(n38);
  n38->ReplaceInput(0, n14);
  n38->ReplaceInput(1, n44);
  n38->ReplaceInput(2, n33);
  n39->ReplaceInput(3, n38);
  n39->ReplaceInput(4, n33);
  n44->ReplaceInput(3, n39);
  n44->ReplaceInput(4, n41);
  n35->ReplaceInput(1, n44);
  n35->ReplaceInput(2, n33);
  n45->ReplaceInput(0, n35);
  n45->ReplaceInput(1, n39);
  op = common_builder.IfFalse();
  Node* n42 = graph.NewNode(op, nil);
  USE(n42);
  n42->ReplaceInput(0, n40);
  n45->ReplaceInput(2, n42);
  n46->ReplaceInput(0, n45);

  graph.SetStart(n0);
  graph.SetEnd(n46);

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);

  CHECK_EQ(46, GetScheduledNodeCount(schedule));
}


TEST(BuildScheduleBreakAndContinue) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common_builder(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());
  Operator* op;

  Handle<Object> object =
      Handle<Object>(isolate->heap()->undefined_value(), isolate);
  PrintableUnique<Object> unique_constant =
      PrintableUnique<Object>::CreateUninitialized(scope.main_zone(), object);

  // Manually transcripted code for:
  // function turbo_fan_test(a, b, c) {
  //   var d = 0;
  //   while (a < b) {
  //     a++;
  //     while (c < b) {
  //       c++;
  //       if (d == 0) break;
  //       a++;
  //     }
  //     if (a == 1) continue;
  //     d++;
  //   }
  //   return a + d;
  // }
  op = common_builder.Start(0);
  Node* n0 = graph.NewNode(op);
  USE(n0);
  Node* nil = graph.NewNode(common_builder.Dead());
  op = common_builder.End();
  Node* n58 = graph.NewNode(op, nil);
  USE(n58);
  op = common_builder.Return();
  Node* n57 = graph.NewNode(op, nil, nil, nil);
  USE(n57);
  op = js_builder.Add();
  Node* n56 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n56);
  op = common_builder.Phi(2);
  Node* n10 = graph.NewNode(op, nil, nil, nil);
  USE(n10);
  op = common_builder.Parameter(0);
  Node* n2 = graph.NewNode(op, n0);
  USE(n2);
  n10->ReplaceInput(0, n2);
  op = common_builder.Phi(2);
  Node* n25 = graph.NewNode(op, nil, nil, nil);
  USE(n25);
  op = js_builder.Add();
  Node* n22 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n22);
  op = js_builder.ToNumber();
  Node* n20 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n20);
  n20->ReplaceInput(0, n10);
  op = common_builder.HeapConstant(unique_constant);
  Node* n6 = graph.NewNode(op);
  USE(n6);
  n20->ReplaceInput(1, n6);
  op = js_builder.LessThan();
  Node* n16 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n16);
  n16->ReplaceInput(0, n10);
  op = common_builder.Phi(2);
  Node* n11 = graph.NewNode(op, nil, nil, nil);
  USE(n11);
  op = common_builder.Parameter(0);
  Node* n3 = graph.NewNode(op, n0);
  USE(n3);
  n11->ReplaceInput(0, n3);
  op = common_builder.Phi(2);
  Node* n26 = graph.NewNode(op, nil, nil, nil);
  USE(n26);
  n26->ReplaceInput(0, n11);
  n26->ReplaceInput(1, n26);
  op = common_builder.Loop(2);
  Node* n23 = graph.NewNode(op, nil, nil);
  USE(n23);
  op = common_builder.IfTrue();
  Node* n18 = graph.NewNode(op, nil);
  USE(n18);
  op = common_builder.Branch();
  Node* n17 = graph.NewNode(op, nil, nil);
  USE(n17);
  n17->ReplaceInput(0, n16);
  op = common_builder.Loop(2);
  Node* n8 = graph.NewNode(op, nil, nil);
  USE(n8);
  n8->ReplaceInput(0, n0);
  op = common_builder.Merge(2);
  Node* n53 = graph.NewNode(op, nil, nil);
  USE(n53);
  op = common_builder.IfTrue();
  Node* n49 = graph.NewNode(op, nil);
  USE(n49);
  op = common_builder.Branch();
  Node* n48 = graph.NewNode(op, nil, nil);
  USE(n48);
  op = js_builder.Equal();
  Node* n47 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n47);
  n47->ReplaceInput(0, n25);
  op = common_builder.NumberConstant(0);
  Node* n46 = graph.NewNode(op);
  USE(n46);
  n47->ReplaceInput(1, n46);
  n47->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n42 = graph.NewNode(op, nil, nil, nil);
  USE(n42);
  op = js_builder.LessThan();
  Node* n30 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n30);
  op = common_builder.Phi(2);
  Node* n27 = graph.NewNode(op, nil, nil, nil);
  USE(n27);
  op = common_builder.Phi(2);
  Node* n12 = graph.NewNode(op, nil, nil, nil);
  USE(n12);
  op = common_builder.Parameter(0);
  Node* n4 = graph.NewNode(op, n0);
  USE(n4);
  n12->ReplaceInput(0, n4);
  op = common_builder.Phi(2);
  Node* n41 = graph.NewNode(op, nil, nil, nil);
  USE(n41);
  n41->ReplaceInput(0, n27);
  op = js_builder.Add();
  Node* n35 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n35);
  op = js_builder.ToNumber();
  Node* n34 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n34);
  n34->ReplaceInput(0, n27);
  n34->ReplaceInput(1, n6);
  n34->ReplaceInput(2, n30);
  op = common_builder.IfTrue();
  Node* n32 = graph.NewNode(op, nil);
  USE(n32);
  op = common_builder.Branch();
  Node* n31 = graph.NewNode(op, nil, nil);
  USE(n31);
  n31->ReplaceInput(0, n30);
  n31->ReplaceInput(1, n23);
  n32->ReplaceInput(0, n31);
  n34->ReplaceInput(3, n32);
  n35->ReplaceInput(0, n34);
  op = common_builder.NumberConstant(0);
  Node* n21 = graph.NewNode(op);
  USE(n21);
  n35->ReplaceInput(1, n21);
  n35->ReplaceInput(2, n6);
  n35->ReplaceInput(3, n34);
  n35->ReplaceInput(4, n32);
  n41->ReplaceInput(1, n35);
  op = common_builder.Merge(2);
  Node* n40 = graph.NewNode(op, nil, nil);
  USE(n40);
  op = common_builder.IfFalse();
  Node* n33 = graph.NewNode(op, nil);
  USE(n33);
  n33->ReplaceInput(0, n31);
  n40->ReplaceInput(0, n33);
  op = common_builder.IfTrue();
  Node* n39 = graph.NewNode(op, nil);
  USE(n39);
  op = common_builder.Branch();
  Node* n38 = graph.NewNode(op, nil, nil);
  USE(n38);
  op = js_builder.Equal();
  Node* n37 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n37);
  op = common_builder.Phi(2);
  Node* n28 = graph.NewNode(op, nil, nil, nil);
  USE(n28);
  op = common_builder.Phi(2);
  Node* n13 = graph.NewNode(op, nil, nil, nil);
  USE(n13);
  op = common_builder.NumberConstant(0);
  Node* n7 = graph.NewNode(op);
  USE(n7);
  n13->ReplaceInput(0, n7);
  op = common_builder.Phi(2);
  Node* n54 = graph.NewNode(op, nil, nil, nil);
  USE(n54);
  n54->ReplaceInput(0, n28);
  op = js_builder.Add();
  Node* n52 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n52);
  op = js_builder.ToNumber();
  Node* n51 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n51);
  n51->ReplaceInput(0, n28);
  n51->ReplaceInput(1, n6);
  n51->ReplaceInput(2, n47);
  op = common_builder.IfFalse();
  Node* n50 = graph.NewNode(op, nil);
  USE(n50);
  n50->ReplaceInput(0, n48);
  n51->ReplaceInput(3, n50);
  n52->ReplaceInput(0, n51);
  n52->ReplaceInput(1, n21);
  n52->ReplaceInput(2, n6);
  n52->ReplaceInput(3, n51);
  n52->ReplaceInput(4, n50);
  n54->ReplaceInput(1, n52);
  n54->ReplaceInput(2, n53);
  n13->ReplaceInput(1, n54);
  n13->ReplaceInput(2, n8);
  n28->ReplaceInput(0, n13);
  n28->ReplaceInput(1, n28);
  n28->ReplaceInput(2, n23);
  n37->ReplaceInput(0, n28);
  op = common_builder.NumberConstant(0);
  Node* n36 = graph.NewNode(op);
  USE(n36);
  n37->ReplaceInput(1, n36);
  n37->ReplaceInput(2, n6);
  n37->ReplaceInput(3, n35);
  n37->ReplaceInput(4, n32);
  n38->ReplaceInput(0, n37);
  n38->ReplaceInput(1, n32);
  n39->ReplaceInput(0, n38);
  n40->ReplaceInput(1, n39);
  n41->ReplaceInput(2, n40);
  n12->ReplaceInput(1, n41);
  n12->ReplaceInput(2, n8);
  n27->ReplaceInput(0, n12);
  n27->ReplaceInput(1, n35);
  n27->ReplaceInput(2, n23);
  n30->ReplaceInput(0, n27);
  n30->ReplaceInput(1, n26);
  n30->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n29 = graph.NewNode(op, nil, nil, nil);
  USE(n29);
  n29->ReplaceInput(0, n22);
  op = js_builder.Add();
  Node* n45 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n45);
  op = js_builder.ToNumber();
  Node* n44 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n44);
  n44->ReplaceInput(0, n25);
  n44->ReplaceInput(1, n6);
  n44->ReplaceInput(2, n37);
  op = common_builder.IfFalse();
  Node* n43 = graph.NewNode(op, nil);
  USE(n43);
  n43->ReplaceInput(0, n38);
  n44->ReplaceInput(3, n43);
  n45->ReplaceInput(0, n44);
  n45->ReplaceInput(1, n21);
  n45->ReplaceInput(2, n6);
  n45->ReplaceInput(3, n44);
  n45->ReplaceInput(4, n43);
  n29->ReplaceInput(1, n45);
  n29->ReplaceInput(2, n23);
  n30->ReplaceInput(3, n29);
  n30->ReplaceInput(4, n23);
  n42->ReplaceInput(0, n30);
  n42->ReplaceInput(1, n37);
  n42->ReplaceInput(2, n40);
  n47->ReplaceInput(3, n42);
  n47->ReplaceInput(4, n40);
  n48->ReplaceInput(0, n47);
  n48->ReplaceInput(1, n40);
  n49->ReplaceInput(0, n48);
  n53->ReplaceInput(0, n49);
  n53->ReplaceInput(1, n50);
  n8->ReplaceInput(1, n53);
  n17->ReplaceInput(1, n8);
  n18->ReplaceInput(0, n17);
  n23->ReplaceInput(0, n18);
  n23->ReplaceInput(1, n43);
  n26->ReplaceInput(2, n23);
  n11->ReplaceInput(1, n26);
  n11->ReplaceInput(2, n8);
  n16->ReplaceInput(1, n11);
  n16->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n14 = graph.NewNode(op, nil, nil, nil);
  USE(n14);
  n14->ReplaceInput(0, n0);
  op = common_builder.Phi(2);
  Node* n55 = graph.NewNode(op, nil, nil, nil);
  USE(n55);
  n55->ReplaceInput(0, n47);
  n55->ReplaceInput(1, n52);
  n55->ReplaceInput(2, n53);
  n14->ReplaceInput(1, n55);
  n14->ReplaceInput(2, n8);
  n16->ReplaceInput(3, n14);
  n16->ReplaceInput(4, n8);
  n20->ReplaceInput(2, n16);
  n20->ReplaceInput(3, n18);
  n22->ReplaceInput(0, n20);
  n22->ReplaceInput(1, n21);
  n22->ReplaceInput(2, n6);
  n22->ReplaceInput(3, n20);
  n22->ReplaceInput(4, n18);
  n25->ReplaceInput(0, n22);
  n25->ReplaceInput(1, n45);
  n25->ReplaceInput(2, n23);
  n10->ReplaceInput(1, n25);
  n10->ReplaceInput(2, n8);
  n56->ReplaceInput(0, n10);
  n56->ReplaceInput(1, n13);
  n56->ReplaceInput(2, n6);
  n56->ReplaceInput(3, n16);
  op = common_builder.IfFalse();
  Node* n19 = graph.NewNode(op, nil);
  USE(n19);
  n19->ReplaceInput(0, n17);
  n56->ReplaceInput(4, n19);
  n57->ReplaceInput(0, n56);
  n57->ReplaceInput(1, n56);
  n57->ReplaceInput(2, n19);
  n58->ReplaceInput(0, n57);

  graph.SetStart(n0);
  graph.SetEnd(n58);

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);

  CHECK_EQ(62, GetScheduledNodeCount(schedule));
}


TEST(BuildScheduleSimpleLoopWithCodeMotion) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common_builder(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());
  MachineOperatorBuilder machine_builder(scope.main_zone(), kMachineWord32);
  Operator* op;

  Handle<Object> object =
      Handle<Object>(isolate->heap()->undefined_value(), isolate);
  PrintableUnique<Object> unique_constant =
      PrintableUnique<Object>::CreateUninitialized(scope.main_zone(), object);

  // Manually transcripted code for:
  // function turbo_fan_test(a, b, c) {
  //   while (a < b) {
  //     a += b + c;
  //   }
  //   return a;
  // }
  op = common_builder.Start(0);
  Node* n0 = graph.NewNode(op);
  USE(n0);
  Node* nil = graph.NewNode(common_builder.Dead());
  op = common_builder.End();
  Node* n22 = graph.NewNode(op, nil);
  USE(n22);
  op = common_builder.Return();
  Node* n21 = graph.NewNode(op, nil, nil, nil);
  USE(n21);
  op = common_builder.Phi(2);
  Node* n9 = graph.NewNode(op, nil, nil, nil);
  USE(n9);
  op = common_builder.Parameter(0);
  Node* n2 = graph.NewNode(op, n0);
  USE(n2);
  n9->ReplaceInput(0, n2);
  op = js_builder.Add();
  Node* n20 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n20);
  n20->ReplaceInput(0, n9);
  op = machine_builder.Int32Add();
  Node* n19 = graph.NewNode(op, nil, nil);
  USE(n19);
  op = common_builder.Phi(2);
  Node* n10 = graph.NewNode(op, nil, nil, nil);
  USE(n10);
  op = common_builder.Parameter(0);
  Node* n3 = graph.NewNode(op, n0);
  USE(n3);
  n10->ReplaceInput(0, n3);
  n10->ReplaceInput(1, n10);
  op = common_builder.Loop(2);
  Node* n7 = graph.NewNode(op, nil, nil);
  USE(n7);
  n7->ReplaceInput(0, n0);
  op = common_builder.IfTrue();
  Node* n17 = graph.NewNode(op, nil);
  USE(n17);
  op = common_builder.Branch();
  Node* n16 = graph.NewNode(op, nil, nil);
  USE(n16);
  op = js_builder.ToBoolean();
  Node* n15 = graph.NewNode(op, nil, nil, nil, nil);
  USE(n15);
  op = js_builder.LessThan();
  Node* n14 = graph.NewNode(op, nil, nil, nil, nil, nil);
  USE(n14);
  n14->ReplaceInput(0, n9);
  n14->ReplaceInput(1, n10);
  op = common_builder.HeapConstant(unique_constant);
  Node* n6 = graph.NewNode(op);
  USE(n6);
  n14->ReplaceInput(2, n6);
  op = common_builder.Phi(2);
  Node* n12 = graph.NewNode(op, nil, nil, nil);
  USE(n12);
  n12->ReplaceInput(0, n0);
  n12->ReplaceInput(1, n20);
  n12->ReplaceInput(2, n7);
  n14->ReplaceInput(3, n12);
  n14->ReplaceInput(4, n7);
  n15->ReplaceInput(0, n14);
  n15->ReplaceInput(1, n6);
  n15->ReplaceInput(2, n14);
  n15->ReplaceInput(3, n7);
  n16->ReplaceInput(0, n15);
  n16->ReplaceInput(1, n7);
  n17->ReplaceInput(0, n16);
  n7->ReplaceInput(1, n17);
  n10->ReplaceInput(2, n7);
  n19->ReplaceInput(0, n2);
  op = common_builder.Phi(2);
  Node* n11 = graph.NewNode(op, nil, nil, nil);
  USE(n11);
  op = common_builder.Parameter(0);
  Node* n4 = graph.NewNode(op, n0);
  USE(n4);
  n11->ReplaceInput(0, n4);
  n11->ReplaceInput(1, n11);
  n11->ReplaceInput(2, n7);
  n19->ReplaceInput(1, n3);
  n20->ReplaceInput(1, n19);
  n20->ReplaceInput(2, n6);
  n20->ReplaceInput(3, n19);
  n20->ReplaceInput(4, n17);
  n9->ReplaceInput(1, n20);
  n9->ReplaceInput(2, n7);
  n21->ReplaceInput(0, n9);
  n21->ReplaceInput(1, n15);
  op = common_builder.IfFalse();
  Node* n18 = graph.NewNode(op, nil);
  USE(n18);
  n18->ReplaceInput(0, n16);
  n21->ReplaceInput(2, n18);
  n22->ReplaceInput(0, n21);

  graph.SetStart(n0);
  graph.SetEnd(n22);

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);

  CHECK_EQ(19, GetScheduledNodeCount(schedule));

  // Make sure the integer-only add gets hoisted to a different block that the
  // JSAdd.
  CHECK(schedule->block(n19) != schedule->block(n20));
}


#if V8_TURBOFAN_TARGET

// So we can get a real JS function.
static Handle<JSFunction> Compile(const char* source) {
  Isolate* isolate = CcTest::i_isolate();
  Handle<String> source_code = isolate->factory()
                                   ->NewStringFromUtf8(CStrVector(source))
                                   .ToHandleChecked();
  Handle<SharedFunctionInfo> shared_function = Compiler::CompileScript(
      source_code, Handle<String>(), 0, 0, false,
      Handle<Context>(isolate->native_context()), NULL, NULL,
      v8::ScriptCompiler::kNoCompileOptions, NOT_NATIVES_CODE);
  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared_function, isolate->native_context());
}


TEST(BuildScheduleTrivialLazyDeoptCall) {
  FLAG_turbo_deoptimization = true;

  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());
  JSOperatorBuilder js_builder(scope.main_zone());

  InitializedHandleScope handles;
  Handle<JSFunction> function = Compile("m()");
  CompilationInfoWithZone info(function);
  Linkage linkage(&info);

  // Manually transcribed code for:
  // function turbo_fan_test() {
  //   m();
  // }
  // where m can lazy deopt (so it has a deopt block associated with it).


  //                  Start                                    //
  //                    ^                                      //
  //                    | (EC)                                 //
  //                    |                                      //
  //         /------> Call <--------------\                    //
  //        /        ^    ^                \                   //
  //       /         |    |                 \        undef     //
  //      /          /    \                  \         ^       //
  //  (E) |     (C) /      \  (C)             \ (E)    |       //
  //      | Continuation  LazyDeoptimization  |        |       //
  //      \___    ^           ^               /        |       //
  //          \   |           |        ______/    Framestate   //
  //    undef  \  | (VC)      | (C)   /            ^           //
  //         \  \ |           |      /            /            //
  //          Return    Deoptimization ----------/             //
  //              ^           ^                                //
  //               \         /                                 //
  //            (C) \       / (C)                              //
  //                 \     /                                   //
  //                  Merge                                    //
  //                    ^                                      //
  //                    |                                      //
  //                   End                                     //

  Handle<Object> undef_object =
      Handle<Object>(isolate->heap()->undefined_value(), isolate);
  PrintableUnique<Object> undef_constant =
      PrintableUnique<Object>::CreateUninitialized(scope.main_zone(),
                                                   undef_object);

  Node* undef_node = graph.NewNode(common.HeapConstant(undef_constant));

  Node* start_node = graph.NewNode(common.Start(0));

  CallDescriptor* descriptor = linkage.GetJSCallDescriptor(0);
  Node* call_node = graph.NewNode(common.Call(descriptor),
                                  undef_node,   // function
                                  undef_node,   // context
                                  start_node,   // effect
                                  start_node);  // control

  Node* cont_node = graph.NewNode(common.Continuation(), call_node);
  Node* lazy_deopt_node = graph.NewNode(common.LazyDeoptimization(), call_node);

  Node* parameters = graph.NewNode(common.StateValues(1), undef_node);
  Node* locals = graph.NewNode(common.StateValues(0));
  Node* stack = graph.NewNode(common.StateValues(0));

  Node* state_node = graph.NewNode(common.FrameState(BailoutId(1234)),
                                   parameters, locals, stack);

  Node* return_node = graph.NewNode(common.Return(),
                                    undef_node,  // return value
                                    call_node,   // effect
                                    cont_node);  // control
  Node* deoptimization_node = graph.NewNode(common.Deoptimize(),
                                            state_node,  // deopt environment
                                            call_node,   // effect
                                            lazy_deopt_node);  // control

  Node* merge_node =
      graph.NewNode(common.Merge(2), return_node, deoptimization_node);

  Node* end_node = graph.NewNode(common.End(), merge_node);

  graph.SetStart(start_node);
  graph.SetEnd(end_node);

  PrintGraph(&graph);

  Schedule* schedule = Scheduler::ComputeSchedule(&graph);

  PrintSchedule(schedule);

  // Tests:
  // Continuation and deopt have basic blocks.
  BasicBlock* cont_block = schedule->block(cont_node);
  BasicBlock* deopt_block = schedule->block(lazy_deopt_node);
  BasicBlock* call_block = schedule->block(call_node);
  CHECK_NE(NULL, cont_block);
  CHECK_NE(NULL, deopt_block);
  CHECK_NE(NULL, call_block);
  // The basic blocks are different.
  CHECK_NE(cont_block, deopt_block);
  CHECK_NE(cont_block, call_block);
  CHECK_NE(deopt_block, call_block);
  // The call node finishes its own basic block.
  CHECK_EQ(BasicBlock::kCall, call_block->control_);
  CHECK_EQ(call_node, call_block->control_input_);
  // The lazy deopt block is deferred.
  CHECK(deopt_block->deferred_);
  CHECK(!call_block->deferred_);
  CHECK(!cont_block->deferred_);
  // The lazy deopt block contains framestate + bailout (and nothing else).
  CHECK_EQ(deoptimization_node, deopt_block->control_input_);
  CHECK_EQ(5, static_cast<int>(deopt_block->nodes_.size()));
  CHECK_EQ(lazy_deopt_node, deopt_block->nodes_[0]);
  CHECK_EQ(IrOpcode::kStateValues, deopt_block->nodes_[1]->op()->opcode());
  CHECK_EQ(IrOpcode::kStateValues, deopt_block->nodes_[2]->op()->opcode());
  CHECK_EQ(IrOpcode::kStateValues, deopt_block->nodes_[3]->op()->opcode());
  CHECK_EQ(state_node, deopt_block->nodes_[4]);
}

#endif
