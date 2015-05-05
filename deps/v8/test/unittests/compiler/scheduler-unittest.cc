// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/verifier.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyOf;

namespace v8 {
namespace internal {
namespace compiler {

class SchedulerTest : public TestWithZone {
 public:
  SchedulerTest()
      : graph_(zone()), common_(zone()), simplified_(zone()), js_(zone()) {}

  Schedule* ComputeAndVerifySchedule(size_t expected) {
    if (FLAG_trace_turbo) {
      OFStream os(stdout);
      os << AsDOT(*graph());
    }

    Schedule* schedule =
        Scheduler::ComputeSchedule(zone(), graph(), Scheduler::kSplitNodes);

    if (FLAG_trace_turbo_scheduler) {
      OFStream os(stdout);
      os << *schedule << std::endl;
    }
    ScheduleVerifier::Run(schedule);
    EXPECT_EQ(expected, GetScheduledNodeCount(schedule));
    return schedule;
  }

  size_t GetScheduledNodeCount(const Schedule* schedule) {
    size_t node_count = 0;
    for (auto block : *schedule->rpo_order()) {
      node_count += block->NodeCount();
      if (block->control() != BasicBlock::kNone) ++node_count;
    }
    return node_count;
  }

  Graph* graph() { return &graph_; }
  CommonOperatorBuilder* common() { return &common_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  JSOperatorBuilder* js() { return &js_; }

 private:
  Graph graph_;
  CommonOperatorBuilder common_;
  SimplifiedOperatorBuilder simplified_;
  JSOperatorBuilder js_;
};


class SchedulerRPOTest : public SchedulerTest {
 public:
  SchedulerRPOTest() {}

  // TODO(titzer): pull RPO tests out to their own file.
  void CheckRPONumbers(BasicBlockVector* order, size_t expected,
                       bool loops_allowed) {
    CHECK(expected == order->size());
    for (int i = 0; i < static_cast<int>(order->size()); i++) {
      CHECK(order->at(i)->rpo_number() == i);
      if (!loops_allowed) {
        CHECK(!order->at(i)->loop_end());
        CHECK(!order->at(i)->loop_header());
      }
    }
  }

  void CheckLoop(BasicBlockVector* order, BasicBlock** blocks, int body_size) {
    BasicBlock* header = blocks[0];
    BasicBlock* end = header->loop_end();
    CHECK(end);
    CHECK_GT(end->rpo_number(), 0);
    CHECK_EQ(body_size, end->rpo_number() - header->rpo_number());
    for (int i = 0; i < body_size; i++) {
      CHECK_GE(blocks[i]->rpo_number(), header->rpo_number());
      CHECK_LT(blocks[i]->rpo_number(), end->rpo_number());
      CHECK(header->LoopContains(blocks[i]));
      CHECK(header->IsLoopHeader() || blocks[i]->loop_header() == header);
    }
    if (header->rpo_number() > 0) {
      CHECK_NE(order->at(header->rpo_number() - 1)->loop_header(), header);
    }
    if (end->rpo_number() < static_cast<int>(order->size())) {
      CHECK_NE(order->at(end->rpo_number())->loop_header(), header);
    }
  }

  struct TestLoop {
    int count;
    BasicBlock** nodes;
    BasicBlock* header() { return nodes[0]; }
    BasicBlock* last() { return nodes[count - 1]; }
    ~TestLoop() { delete[] nodes; }
  };

  TestLoop* CreateLoop(Schedule* schedule, int count) {
    TestLoop* loop = new TestLoop();
    loop->count = count;
    loop->nodes = new BasicBlock* [count];
    for (int i = 0; i < count; i++) {
      loop->nodes[i] = schedule->NewBasicBlock();
      if (i > 0) {
        schedule->AddSuccessorForTesting(loop->nodes[i - 1], loop->nodes[i]);
      }
    }
    schedule->AddSuccessorForTesting(loop->nodes[count - 1], loop->nodes[0]);
    return loop;
  }
};


namespace {

const Operator kHeapConstant(IrOpcode::kHeapConstant, Operator::kPure,
                             "HeapConstant", 0, 0, 0, 1, 0, 0);
const Operator kIntAdd(IrOpcode::kInt32Add, Operator::kPure, "Int32Add", 2, 0,
                       0, 1, 0, 0);
const Operator kMockCall(IrOpcode::kCall, Operator::kNoProperties, "MockCall",
                         0, 0, 1, 1, 0, 2);

}  // namespace


// -----------------------------------------------------------------------------
// Special reverse-post-order block ordering.


TEST_F(SchedulerRPOTest, Degenerate1) {
  Schedule schedule(zone());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 1, false);
  EXPECT_EQ(schedule.start(), order->at(0));
}


TEST_F(SchedulerRPOTest, Degenerate2) {
  Schedule schedule(zone());

  schedule.AddGoto(schedule.start(), schedule.end());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 2, false);
  EXPECT_EQ(schedule.start(), order->at(0));
  EXPECT_EQ(schedule.end(), order->at(1));
}


TEST_F(SchedulerRPOTest, Line) {
  for (int i = 0; i < 10; i++) {
    Schedule schedule(zone());

    BasicBlock* last = schedule.start();
    for (int j = 0; j < i; j++) {
      BasicBlock* block = schedule.NewBasicBlock();
      block->set_deferred(i & 1);
      schedule.AddGoto(last, block);
      last = block;
    }
    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
    CheckRPONumbers(order, 1 + i, false);

    for (size_t i = 0; i < schedule.BasicBlockCount(); i++) {
      BasicBlock* block = schedule.GetBlockById(BasicBlock::Id::FromSize(i));
      if (block->rpo_number() >= 0 && block->SuccessorCount() == 1) {
        EXPECT_EQ(block->rpo_number() + 1, block->SuccessorAt(0)->rpo_number());
      }
    }
  }
}


TEST_F(SchedulerRPOTest, SelfLoop) {
  Schedule schedule(zone());
  schedule.AddSuccessorForTesting(schedule.start(), schedule.start());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 1, true);
  BasicBlock* loop[] = {schedule.start()};
  CheckLoop(order, loop, 1);
}


TEST_F(SchedulerRPOTest, EntryLoop) {
  Schedule schedule(zone());
  BasicBlock* body = schedule.NewBasicBlock();
  schedule.AddSuccessorForTesting(schedule.start(), body);
  schedule.AddSuccessorForTesting(body, schedule.start());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 2, true);
  BasicBlock* loop[] = {schedule.start(), body};
  CheckLoop(order, loop, 2);
}


TEST_F(SchedulerRPOTest, EndLoop) {
  Schedule schedule(zone());
  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 2));
  schedule.AddSuccessorForTesting(schedule.start(), loop1->header());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 3, true);
  CheckLoop(order, loop1->nodes, loop1->count);
}


TEST_F(SchedulerRPOTest, EndLoopNested) {
  Schedule schedule(zone());
  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 2));
  schedule.AddSuccessorForTesting(schedule.start(), loop1->header());
  schedule.AddSuccessorForTesting(loop1->last(), schedule.start());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 3, true);
  CheckLoop(order, loop1->nodes, loop1->count);
}


TEST_F(SchedulerRPOTest, Diamond) {
  Schedule schedule(zone());

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.end();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(A, C);
  schedule.AddSuccessorForTesting(B, D);
  schedule.AddSuccessorForTesting(C, D);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 4, false);

  EXPECT_EQ(0, A->rpo_number());
  EXPECT_THAT(B->rpo_number(), AnyOf(1, 2));
  EXPECT_THAT(C->rpo_number(), AnyOf(1, 2));
  EXPECT_EQ(3, D->rpo_number());
}


TEST_F(SchedulerRPOTest, Loop1) {
  Schedule schedule(zone());

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.end();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(B, C);
  schedule.AddSuccessorForTesting(C, B);
  schedule.AddSuccessorForTesting(C, D);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 4, true);
  BasicBlock* loop[] = {B, C};
  CheckLoop(order, loop, 2);
}


TEST_F(SchedulerRPOTest, Loop2) {
  Schedule schedule(zone());

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.end();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(B, C);
  schedule.AddSuccessorForTesting(C, B);
  schedule.AddSuccessorForTesting(B, D);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 4, true);
  BasicBlock* loop[] = {B, C};
  CheckLoop(order, loop, 2);
}


TEST_F(SchedulerRPOTest, LoopN) {
  for (int i = 0; i < 11; i++) {
    Schedule schedule(zone());
    BasicBlock* A = schedule.start();
    BasicBlock* B = schedule.NewBasicBlock();
    BasicBlock* C = schedule.NewBasicBlock();
    BasicBlock* D = schedule.NewBasicBlock();
    BasicBlock* E = schedule.NewBasicBlock();
    BasicBlock* F = schedule.NewBasicBlock();
    BasicBlock* G = schedule.end();

    schedule.AddSuccessorForTesting(A, B);
    schedule.AddSuccessorForTesting(B, C);
    schedule.AddSuccessorForTesting(C, D);
    schedule.AddSuccessorForTesting(D, E);
    schedule.AddSuccessorForTesting(E, F);
    schedule.AddSuccessorForTesting(F, B);
    schedule.AddSuccessorForTesting(B, G);

    // Throw in extra backedges from time to time.
    if (i == 1) schedule.AddSuccessorForTesting(B, B);
    if (i == 2) schedule.AddSuccessorForTesting(C, B);
    if (i == 3) schedule.AddSuccessorForTesting(D, B);
    if (i == 4) schedule.AddSuccessorForTesting(E, B);
    if (i == 5) schedule.AddSuccessorForTesting(F, B);

    // Throw in extra loop exits from time to time.
    if (i == 6) schedule.AddSuccessorForTesting(B, G);
    if (i == 7) schedule.AddSuccessorForTesting(C, G);
    if (i == 8) schedule.AddSuccessorForTesting(D, G);
    if (i == 9) schedule.AddSuccessorForTesting(E, G);
    if (i == 10) schedule.AddSuccessorForTesting(F, G);

    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
    CheckRPONumbers(order, 7, true);
    BasicBlock* loop[] = {B, C, D, E, F};
    CheckLoop(order, loop, 5);
  }
}


TEST_F(SchedulerRPOTest, LoopNest1) {
  Schedule schedule(zone());

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.NewBasicBlock();
  BasicBlock* E = schedule.NewBasicBlock();
  BasicBlock* F = schedule.end();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(B, C);
  schedule.AddSuccessorForTesting(C, D);
  schedule.AddSuccessorForTesting(D, C);
  schedule.AddSuccessorForTesting(D, E);
  schedule.AddSuccessorForTesting(E, B);
  schedule.AddSuccessorForTesting(E, F);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 6, true);
  BasicBlock* loop1[] = {B, C, D, E};
  CheckLoop(order, loop1, 4);

  BasicBlock* loop2[] = {C, D};
  CheckLoop(order, loop2, 2);
}


TEST_F(SchedulerRPOTest, LoopNest2) {
  Schedule schedule(zone());

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.NewBasicBlock();
  BasicBlock* E = schedule.NewBasicBlock();
  BasicBlock* F = schedule.NewBasicBlock();
  BasicBlock* G = schedule.NewBasicBlock();
  BasicBlock* H = schedule.end();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(B, C);
  schedule.AddSuccessorForTesting(C, D);
  schedule.AddSuccessorForTesting(D, E);
  schedule.AddSuccessorForTesting(E, F);
  schedule.AddSuccessorForTesting(F, G);
  schedule.AddSuccessorForTesting(G, H);

  schedule.AddSuccessorForTesting(E, D);
  schedule.AddSuccessorForTesting(F, C);
  schedule.AddSuccessorForTesting(G, B);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 8, true);
  BasicBlock* loop1[] = {B, C, D, E, F, G};
  CheckLoop(order, loop1, 6);

  BasicBlock* loop2[] = {C, D, E, F};
  CheckLoop(order, loop2, 4);

  BasicBlock* loop3[] = {D, E};
  CheckLoop(order, loop3, 2);
}


TEST_F(SchedulerRPOTest, LoopFollow1) {
  Schedule schedule(zone());

  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 1));
  SmartPointer<TestLoop> loop2(CreateLoop(&schedule, 1));

  BasicBlock* A = schedule.start();
  BasicBlock* E = schedule.end();

  schedule.AddSuccessorForTesting(A, loop1->header());
  schedule.AddSuccessorForTesting(loop1->header(), loop2->header());
  schedule.AddSuccessorForTesting(loop2->last(), E);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);

  EXPECT_EQ(schedule.BasicBlockCount(), order->size());
  CheckLoop(order, loop1->nodes, loop1->count);
  CheckLoop(order, loop2->nodes, loop2->count);
}


TEST_F(SchedulerRPOTest, LoopFollow2) {
  Schedule schedule(zone());

  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 1));
  SmartPointer<TestLoop> loop2(CreateLoop(&schedule, 1));

  BasicBlock* A = schedule.start();
  BasicBlock* S = schedule.NewBasicBlock();
  BasicBlock* E = schedule.end();

  schedule.AddSuccessorForTesting(A, loop1->header());
  schedule.AddSuccessorForTesting(loop1->header(), S);
  schedule.AddSuccessorForTesting(S, loop2->header());
  schedule.AddSuccessorForTesting(loop2->last(), E);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);

  EXPECT_EQ(schedule.BasicBlockCount(), order->size());
  CheckLoop(order, loop1->nodes, loop1->count);
  CheckLoop(order, loop2->nodes, loop2->count);
}


TEST_F(SchedulerRPOTest, LoopFollowN) {
  for (int size = 1; size < 5; size++) {
    for (int exit = 0; exit < size; exit++) {
      Schedule schedule(zone());
      SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
      SmartPointer<TestLoop> loop2(CreateLoop(&schedule, size));
      BasicBlock* A = schedule.start();
      BasicBlock* E = schedule.end();

      schedule.AddSuccessorForTesting(A, loop1->header());
      schedule.AddSuccessorForTesting(loop1->nodes[exit], loop2->header());
      schedule.AddSuccessorForTesting(loop2->nodes[exit], E);
      BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);

      EXPECT_EQ(schedule.BasicBlockCount(), order->size());
      CheckLoop(order, loop1->nodes, loop1->count);
      CheckLoop(order, loop2->nodes, loop2->count);
    }
  }
}


TEST_F(SchedulerRPOTest, NestedLoopFollow1) {
  Schedule schedule(zone());

  SmartPointer<TestLoop> loop1(CreateLoop(&schedule, 1));
  SmartPointer<TestLoop> loop2(CreateLoop(&schedule, 1));

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* E = schedule.end();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(B, loop1->header());
  schedule.AddSuccessorForTesting(loop1->header(), loop2->header());
  schedule.AddSuccessorForTesting(loop2->last(), C);
  schedule.AddSuccessorForTesting(C, E);
  schedule.AddSuccessorForTesting(C, B);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);

  EXPECT_EQ(schedule.BasicBlockCount(), order->size());
  CheckLoop(order, loop1->nodes, loop1->count);
  CheckLoop(order, loop2->nodes, loop2->count);

  BasicBlock* loop3[] = {B, loop1->nodes[0], loop2->nodes[0], C};
  CheckLoop(order, loop3, 4);
}


TEST_F(SchedulerRPOTest, LoopBackedges1) {
  int size = 8;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      Schedule schedule(zone());
      BasicBlock* A = schedule.start();
      BasicBlock* E = schedule.end();

      SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
      schedule.AddSuccessorForTesting(A, loop1->header());
      schedule.AddSuccessorForTesting(loop1->last(), E);

      schedule.AddSuccessorForTesting(loop1->nodes[i], loop1->header());
      schedule.AddSuccessorForTesting(loop1->nodes[j], E);

      BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
      CheckRPONumbers(order, schedule.BasicBlockCount(), true);
      CheckLoop(order, loop1->nodes, loop1->count);
    }
  }
}


TEST_F(SchedulerRPOTest, LoopOutedges1) {
  int size = 8;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      Schedule schedule(zone());
      BasicBlock* A = schedule.start();
      BasicBlock* D = schedule.NewBasicBlock();
      BasicBlock* E = schedule.end();

      SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
      schedule.AddSuccessorForTesting(A, loop1->header());
      schedule.AddSuccessorForTesting(loop1->last(), E);

      schedule.AddSuccessorForTesting(loop1->nodes[i], loop1->header());
      schedule.AddSuccessorForTesting(loop1->nodes[j], D);
      schedule.AddSuccessorForTesting(D, E);

      BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
      CheckRPONumbers(order, schedule.BasicBlockCount(), true);
      CheckLoop(order, loop1->nodes, loop1->count);
    }
  }
}


TEST_F(SchedulerRPOTest, LoopOutedges2) {
  int size = 8;
  for (int i = 0; i < size; i++) {
    Schedule schedule(zone());
    BasicBlock* A = schedule.start();
    BasicBlock* E = schedule.end();

    SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
    schedule.AddSuccessorForTesting(A, loop1->header());
    schedule.AddSuccessorForTesting(loop1->last(), E);

    for (int j = 0; j < size; j++) {
      BasicBlock* O = schedule.NewBasicBlock();
      schedule.AddSuccessorForTesting(loop1->nodes[j], O);
      schedule.AddSuccessorForTesting(O, E);
    }

    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
    CheckRPONumbers(order, schedule.BasicBlockCount(), true);
    CheckLoop(order, loop1->nodes, loop1->count);
  }
}


TEST_F(SchedulerRPOTest, LoopOutloops1) {
  int size = 8;
  for (int i = 0; i < size; i++) {
    Schedule schedule(zone());
    BasicBlock* A = schedule.start();
    BasicBlock* E = schedule.end();
    SmartPointer<TestLoop> loop1(CreateLoop(&schedule, size));
    schedule.AddSuccessorForTesting(A, loop1->header());
    schedule.AddSuccessorForTesting(loop1->last(), E);

    TestLoop** loopN = new TestLoop* [size];
    for (int j = 0; j < size; j++) {
      loopN[j] = CreateLoop(&schedule, 2);
      schedule.AddSuccessorForTesting(loop1->nodes[j], loopN[j]->header());
      schedule.AddSuccessorForTesting(loopN[j]->last(), E);
    }

    BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
    CheckRPONumbers(order, schedule.BasicBlockCount(), true);
    CheckLoop(order, loop1->nodes, loop1->count);

    for (int j = 0; j < size; j++) {
      CheckLoop(order, loopN[j]->nodes, loopN[j]->count);
      delete loopN[j];
    }
    delete[] loopN;
  }
}


TEST_F(SchedulerRPOTest, LoopMultibackedge) {
  Schedule schedule(zone());

  BasicBlock* A = schedule.start();
  BasicBlock* B = schedule.NewBasicBlock();
  BasicBlock* C = schedule.NewBasicBlock();
  BasicBlock* D = schedule.NewBasicBlock();
  BasicBlock* E = schedule.NewBasicBlock();

  schedule.AddSuccessorForTesting(A, B);
  schedule.AddSuccessorForTesting(B, C);
  schedule.AddSuccessorForTesting(B, D);
  schedule.AddSuccessorForTesting(B, E);
  schedule.AddSuccessorForTesting(C, B);
  schedule.AddSuccessorForTesting(D, B);
  schedule.AddSuccessorForTesting(E, B);

  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 5, true);

  BasicBlock* loop1[] = {B, C, D, E};
  CheckLoop(order, loop1, 4);
}


// -----------------------------------------------------------------------------
// Graph end-to-end scheduling.


TEST_F(SchedulerTest, BuildScheduleEmpty) {
  graph()->SetStart(graph()->NewNode(common()->Start(0)));
  graph()->SetEnd(graph()->NewNode(common()->End(), graph()->start()));
  USE(Scheduler::ComputeSchedule(zone(), graph(), Scheduler::kNoFlags));
}


TEST_F(SchedulerTest, BuildScheduleOneParameter) {
  graph()->SetStart(graph()->NewNode(common()->Start(0)));

  Node* p1 = graph()->NewNode(common()->Parameter(0), graph()->start());
  Node* ret = graph()->NewNode(common()->Return(), p1, graph()->start(),
                               graph()->start());

  graph()->SetEnd(graph()->NewNode(common()->End(), ret));

  USE(Scheduler::ComputeSchedule(zone(), graph(), Scheduler::kNoFlags));
}


TEST_F(SchedulerTest, BuildScheduleIfSplit) {
  graph()->SetStart(graph()->NewNode(common()->Start(3)));

  Node* p1 = graph()->NewNode(common()->Parameter(0), graph()->start());
  Node* p2 = graph()->NewNode(common()->Parameter(1), graph()->start());
  Node* p3 = graph()->NewNode(common()->Parameter(2), graph()->start());
  Node* p4 = graph()->NewNode(common()->Parameter(3), graph()->start());
  Node* p5 = graph()->NewNode(common()->Parameter(4), graph()->start());
  Node* cmp = graph()->NewNode(js()->LessThanOrEqual(), p1, p2, p3,
                               graph()->start(), graph()->start());
  Node* branch = graph()->NewNode(common()->Branch(), cmp, graph()->start());
  Node* true_branch = graph()->NewNode(common()->IfTrue(), branch);
  Node* false_branch = graph()->NewNode(common()->IfFalse(), branch);

  Node* ret1 =
      graph()->NewNode(common()->Return(), p4, graph()->start(), true_branch);
  Node* ret2 =
      graph()->NewNode(common()->Return(), p5, graph()->start(), false_branch);
  Node* merge = graph()->NewNode(common()->Merge(2), ret1, ret2);
  graph()->SetEnd(graph()->NewNode(common()->End(), merge));

  ComputeAndVerifySchedule(13);
}


TEST_F(SchedulerTest, BuildScheduleIfSplitWithEffects) {
  FLAG_turbo_deoptimization = false;

  const Operator* op;

  // Manually transcripted code for:
  // function turbo_fan_test(a, b, c, y) {
  //   if (a < b) {
  //     return a + b - c * c - a + y;
  //   } else {
  //     return c * c - a;
  //   }
  // }
  op = common()->Start(0);
  Node* n0 = graph()->NewNode(op);
  USE(n0);
  Node* nil = graph()->NewNode(common()->Dead());
  op = common()->End();
  Node* n23 = graph()->NewNode(op, nil);
  USE(n23);
  op = common()->Merge(2);
  Node* n22 = graph()->NewNode(op, nil, nil);
  USE(n22);
  op = common()->Return();
  Node* n16 = graph()->NewNode(op, nil, nil, nil);
  USE(n16);
  op = js()->Add();
  Node* n15 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n15);
  op = js()->Subtract();
  Node* n14 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n14);
  op = js()->Subtract();
  Node* n13 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n13);
  op = js()->Add();
  Node* n11 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n11);
  op = common()->Parameter(0);
  Node* n2 = graph()->NewNode(op, n0);
  USE(n2);
  n11->ReplaceInput(0, n2);
  op = common()->Parameter(0);
  Node* n3 = graph()->NewNode(op, n0);
  USE(n3);
  n11->ReplaceInput(1, n3);
  op = &kHeapConstant;
  Node* n7 = graph()->NewNode(op);
  USE(n7);
  n11->ReplaceInput(2, n7);
  op = js()->LessThan();
  Node* n8 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n8);
  n8->ReplaceInput(0, n2);
  n8->ReplaceInput(1, n3);
  n8->ReplaceInput(2, n7);
  n8->ReplaceInput(3, n0);
  n8->ReplaceInput(4, n0);
  n11->ReplaceInput(3, n8);
  op = common()->IfTrue();
  Node* n10 = graph()->NewNode(op, nil);
  USE(n10);
  op = common()->Branch();
  Node* n9 = graph()->NewNode(op, nil, nil);
  USE(n9);
  n9->ReplaceInput(0, n8);
  n9->ReplaceInput(1, n0);
  n10->ReplaceInput(0, n9);
  n11->ReplaceInput(4, n10);
  n13->ReplaceInput(0, n11);
  op = js()->Multiply();
  Node* n12 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n12);
  op = common()->Parameter(0);
  Node* n4 = graph()->NewNode(op, n0);
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
  op = common()->Parameter(0);
  Node* n5 = graph()->NewNode(op, n0);
  USE(n5);
  n15->ReplaceInput(1, n5);
  n15->ReplaceInput(2, n7);
  n15->ReplaceInput(3, n14);
  n15->ReplaceInput(4, n10);
  n16->ReplaceInput(0, n15);
  n16->ReplaceInput(1, n15);
  n16->ReplaceInput(2, n10);
  n22->ReplaceInput(0, n16);
  op = common()->Return();
  Node* n21 = graph()->NewNode(op, nil, nil, nil);
  USE(n21);
  op = js()->Subtract();
  Node* n20 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n20);
  op = js()->Multiply();
  Node* n19 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n19);
  n19->ReplaceInput(0, n4);
  n19->ReplaceInput(1, n4);
  n19->ReplaceInput(2, n7);
  n19->ReplaceInput(3, n8);
  op = common()->IfFalse();
  Node* n18 = graph()->NewNode(op, nil);
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

  graph()->SetStart(n0);
  graph()->SetEnd(n23);

  ComputeAndVerifySchedule(20);
}


TEST_F(SchedulerTest, BuildScheduleSimpleLoop) {
  FLAG_turbo_deoptimization = false;

  const Operator* op;

  // Manually transcripted code for:
  // function turbo_fan_test(a, b) {
  //   while (a < b) {
  //     a++;
  //   }
  //   return a;
  // }
  op = common()->Start(0);
  Node* n0 = graph()->NewNode(op);
  USE(n0);
  Node* nil = graph()->NewNode(common()->Dead());
  op = common()->End();
  Node* n20 = graph()->NewNode(op, nil);
  USE(n20);
  op = common()->Return();
  Node* n19 = graph()->NewNode(op, nil, nil, nil);
  USE(n19);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n8 = graph()->NewNode(op, nil, nil, nil);
  USE(n8);
  op = common()->Parameter(0);
  Node* n2 = graph()->NewNode(op, n0);
  USE(n2);
  n8->ReplaceInput(0, n2);
  op = js()->Add();
  Node* n18 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n18);
  op = js()->ToNumber();
  Node* n16 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n16);
  n16->ReplaceInput(0, n8);
  op = &kHeapConstant;
  Node* n5 = graph()->NewNode(op);
  USE(n5);
  n16->ReplaceInput(1, n5);
  op = js()->LessThan();
  Node* n12 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n12);
  n12->ReplaceInput(0, n8);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n9 = graph()->NewNode(op, nil, nil, nil);
  USE(n9);
  op = common()->Parameter(0);
  Node* n3 = graph()->NewNode(op, n0);
  USE(n3);
  n9->ReplaceInput(0, n3);
  n9->ReplaceInput(1, n9);
  op = common()->Loop(2);
  Node* n6 = graph()->NewNode(op, nil, nil);
  USE(n6);
  n6->ReplaceInput(0, n0);
  op = common()->IfTrue();
  Node* n14 = graph()->NewNode(op, nil);
  USE(n14);
  op = common()->Branch();
  Node* n13 = graph()->NewNode(op, nil, nil);
  USE(n13);
  n13->ReplaceInput(0, n12);
  n13->ReplaceInput(1, n6);
  n14->ReplaceInput(0, n13);
  n6->ReplaceInput(1, n14);
  n9->ReplaceInput(2, n6);
  n12->ReplaceInput(1, n9);
  n12->ReplaceInput(2, n5);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n10 = graph()->NewNode(op, nil, nil, nil);
  USE(n10);
  n10->ReplaceInput(0, n0);
  n10->ReplaceInput(1, n18);
  n10->ReplaceInput(2, n6);
  n12->ReplaceInput(3, n10);
  n12->ReplaceInput(4, n6);
  n16->ReplaceInput(2, n12);
  n16->ReplaceInput(3, n14);
  n18->ReplaceInput(0, n16);
  op = common()->NumberConstant(0);
  Node* n17 = graph()->NewNode(op);
  USE(n17);
  n18->ReplaceInput(1, n17);
  n18->ReplaceInput(2, n5);
  n18->ReplaceInput(3, n16);
  n18->ReplaceInput(4, n14);
  n8->ReplaceInput(1, n18);
  n8->ReplaceInput(2, n6);
  n19->ReplaceInput(0, n8);
  n19->ReplaceInput(1, n12);
  op = common()->IfFalse();
  Node* n15 = graph()->NewNode(op, nil);
  USE(n15);
  n15->ReplaceInput(0, n13);
  n19->ReplaceInput(2, n15);
  n20->ReplaceInput(0, n19);

  graph()->SetStart(n0);
  graph()->SetEnd(n20);

  ComputeAndVerifySchedule(19);
}


TEST_F(SchedulerTest, BuildScheduleComplexLoops) {
  FLAG_turbo_deoptimization = false;

  const Operator* op;

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
  op = common()->Start(0);
  Node* n0 = graph()->NewNode(op);
  USE(n0);
  Node* nil = graph()->NewNode(common()->Dead());
  op = common()->End();
  Node* n46 = graph()->NewNode(op, nil);
  USE(n46);
  op = common()->Return();
  Node* n45 = graph()->NewNode(op, nil, nil, nil);
  USE(n45);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n35 = graph()->NewNode(op, nil, nil, nil);
  USE(n35);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n9 = graph()->NewNode(op, nil, nil, nil);
  USE(n9);
  op = common()->Parameter(0);
  Node* n2 = graph()->NewNode(op, n0);
  USE(n2);
  n9->ReplaceInput(0, n2);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n23 = graph()->NewNode(op, nil, nil, nil);
  USE(n23);
  op = js()->Add();
  Node* n20 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n20);
  op = js()->ToNumber();
  Node* n18 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n18);
  n18->ReplaceInput(0, n9);
  op = &kHeapConstant;
  Node* n6 = graph()->NewNode(op);
  USE(n6);
  n18->ReplaceInput(1, n6);
  op = js()->LessThan();
  Node* n14 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n14);
  n14->ReplaceInput(0, n9);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n10 = graph()->NewNode(op, nil, nil, nil);
  USE(n10);
  op = common()->Parameter(0);
  Node* n3 = graph()->NewNode(op, n0);
  USE(n3);
  n10->ReplaceInput(0, n3);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n24 = graph()->NewNode(op, nil, nil, nil);
  USE(n24);
  n24->ReplaceInput(0, n10);
  n24->ReplaceInput(1, n24);
  op = common()->Loop(2);
  Node* n21 = graph()->NewNode(op, nil, nil);
  USE(n21);
  op = common()->IfTrue();
  Node* n16 = graph()->NewNode(op, nil);
  USE(n16);
  op = common()->Branch();
  Node* n15 = graph()->NewNode(op, nil, nil);
  USE(n15);
  n15->ReplaceInput(0, n14);
  op = common()->Loop(2);
  Node* n7 = graph()->NewNode(op, nil, nil);
  USE(n7);
  n7->ReplaceInput(0, n0);
  op = common()->IfFalse();
  Node* n30 = graph()->NewNode(op, nil);
  USE(n30);
  op = common()->Branch();
  Node* n28 = graph()->NewNode(op, nil, nil);
  USE(n28);
  op = js()->LessThan();
  Node* n27 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n27);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n25 = graph()->NewNode(op, nil, nil, nil);
  USE(n25);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n11 = graph()->NewNode(op, nil, nil, nil);
  USE(n11);
  op = common()->Parameter(0);
  Node* n4 = graph()->NewNode(op, n0);
  USE(n4);
  n11->ReplaceInput(0, n4);
  n11->ReplaceInput(1, n25);
  n11->ReplaceInput(2, n7);
  n25->ReplaceInput(0, n11);
  op = js()->Add();
  Node* n32 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n32);
  op = js()->ToNumber();
  Node* n31 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n31);
  n31->ReplaceInput(0, n25);
  n31->ReplaceInput(1, n6);
  n31->ReplaceInput(2, n27);
  op = common()->IfTrue();
  Node* n29 = graph()->NewNode(op, nil);
  USE(n29);
  n29->ReplaceInput(0, n28);
  n31->ReplaceInput(3, n29);
  n32->ReplaceInput(0, n31);
  op = common()->NumberConstant(0);
  Node* n19 = graph()->NewNode(op);
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
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n26 = graph()->NewNode(op, nil, nil, nil);
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
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n12 = graph()->NewNode(op, nil, nil, nil);
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
  op = js()->Add();
  Node* n44 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n44);
  n44->ReplaceInput(0, n35);
  op = common()->NumberConstant(0);
  Node* n43 = graph()->NewNode(op);
  USE(n43);
  n44->ReplaceInput(1, n43);
  n44->ReplaceInput(2, n6);
  op = js()->LessThan();
  Node* n39 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n39);
  n39->ReplaceInput(0, n35);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n36 = graph()->NewNode(op, nil, nil, nil);
  USE(n36);
  n36->ReplaceInput(0, n10);
  n36->ReplaceInput(1, n36);
  op = common()->Loop(2);
  Node* n33 = graph()->NewNode(op, nil, nil);
  USE(n33);
  op = common()->IfFalse();
  Node* n17 = graph()->NewNode(op, nil);
  USE(n17);
  n17->ReplaceInput(0, n15);
  n33->ReplaceInput(0, n17);
  op = common()->IfTrue();
  Node* n41 = graph()->NewNode(op, nil);
  USE(n41);
  op = common()->Branch();
  Node* n40 = graph()->NewNode(op, nil, nil);
  USE(n40);
  n40->ReplaceInput(0, n39);
  n40->ReplaceInput(1, n33);
  n41->ReplaceInput(0, n40);
  n33->ReplaceInput(1, n41);
  n36->ReplaceInput(2, n33);
  n39->ReplaceInput(1, n36);
  n39->ReplaceInput(2, n6);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n38 = graph()->NewNode(op, nil, nil, nil);
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
  op = common()->IfFalse();
  Node* n42 = graph()->NewNode(op, nil);
  USE(n42);
  n42->ReplaceInput(0, n40);
  n45->ReplaceInput(2, n42);
  n46->ReplaceInput(0, n45);

  graph()->SetStart(n0);
  graph()->SetEnd(n46);

  ComputeAndVerifySchedule(46);
}


TEST_F(SchedulerTest, BuildScheduleBreakAndContinue) {
  FLAG_turbo_deoptimization = false;

  const Operator* op;

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
  op = common()->Start(0);
  Node* n0 = graph()->NewNode(op);
  USE(n0);
  Node* nil = graph()->NewNode(common()->Dead());
  op = common()->End();
  Node* n58 = graph()->NewNode(op, nil);
  USE(n58);
  op = common()->Return();
  Node* n57 = graph()->NewNode(op, nil, nil, nil);
  USE(n57);
  op = js()->Add();
  Node* n56 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n56);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n10 = graph()->NewNode(op, nil, nil, nil);
  USE(n10);
  op = common()->Parameter(0);
  Node* n2 = graph()->NewNode(op, n0);
  USE(n2);
  n10->ReplaceInput(0, n2);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n25 = graph()->NewNode(op, nil, nil, nil);
  USE(n25);
  op = js()->Add();
  Node* n22 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n22);
  op = js()->ToNumber();
  Node* n20 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n20);
  n20->ReplaceInput(0, n10);
  op = &kHeapConstant;
  Node* n6 = graph()->NewNode(op);
  USE(n6);
  n20->ReplaceInput(1, n6);
  op = js()->LessThan();
  Node* n16 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n16);
  n16->ReplaceInput(0, n10);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n11 = graph()->NewNode(op, nil, nil, nil);
  USE(n11);
  op = common()->Parameter(0);
  Node* n3 = graph()->NewNode(op, n0);
  USE(n3);
  n11->ReplaceInput(0, n3);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n26 = graph()->NewNode(op, nil, nil, nil);
  USE(n26);
  n26->ReplaceInput(0, n11);
  n26->ReplaceInput(1, n26);
  op = common()->Loop(2);
  Node* n23 = graph()->NewNode(op, nil, nil);
  USE(n23);
  op = common()->IfTrue();
  Node* n18 = graph()->NewNode(op, nil);
  USE(n18);
  op = common()->Branch();
  Node* n17 = graph()->NewNode(op, nil, nil);
  USE(n17);
  n17->ReplaceInput(0, n16);
  op = common()->Loop(2);
  Node* n8 = graph()->NewNode(op, nil, nil);
  USE(n8);
  n8->ReplaceInput(0, n0);
  op = common()->Merge(2);
  Node* n53 = graph()->NewNode(op, nil, nil);
  USE(n53);
  op = common()->IfTrue();
  Node* n49 = graph()->NewNode(op, nil);
  USE(n49);
  op = common()->Branch();
  Node* n48 = graph()->NewNode(op, nil, nil);
  USE(n48);
  op = js()->Equal();
  Node* n47 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n47);
  n47->ReplaceInput(0, n25);
  op = common()->NumberConstant(0);
  Node* n46 = graph()->NewNode(op);
  USE(n46);
  n47->ReplaceInput(1, n46);
  n47->ReplaceInput(2, n6);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n42 = graph()->NewNode(op, nil, nil, nil);
  USE(n42);
  op = js()->LessThan();
  Node* n30 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n30);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n27 = graph()->NewNode(op, nil, nil, nil);
  USE(n27);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n12 = graph()->NewNode(op, nil, nil, nil);
  USE(n12);
  op = common()->Parameter(0);
  Node* n4 = graph()->NewNode(op, n0);
  USE(n4);
  n12->ReplaceInput(0, n4);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n41 = graph()->NewNode(op, nil, nil, nil);
  USE(n41);
  n41->ReplaceInput(0, n27);
  op = js()->Add();
  Node* n35 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n35);
  op = js()->ToNumber();
  Node* n34 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n34);
  n34->ReplaceInput(0, n27);
  n34->ReplaceInput(1, n6);
  n34->ReplaceInput(2, n30);
  op = common()->IfTrue();
  Node* n32 = graph()->NewNode(op, nil);
  USE(n32);
  op = common()->Branch();
  Node* n31 = graph()->NewNode(op, nil, nil);
  USE(n31);
  n31->ReplaceInput(0, n30);
  n31->ReplaceInput(1, n23);
  n32->ReplaceInput(0, n31);
  n34->ReplaceInput(3, n32);
  n35->ReplaceInput(0, n34);
  op = common()->NumberConstant(0);
  Node* n21 = graph()->NewNode(op);
  USE(n21);
  n35->ReplaceInput(1, n21);
  n35->ReplaceInput(2, n6);
  n35->ReplaceInput(3, n34);
  n35->ReplaceInput(4, n32);
  n41->ReplaceInput(1, n35);
  op = common()->Merge(2);
  Node* n40 = graph()->NewNode(op, nil, nil);
  USE(n40);
  op = common()->IfFalse();
  Node* n33 = graph()->NewNode(op, nil);
  USE(n33);
  n33->ReplaceInput(0, n31);
  n40->ReplaceInput(0, n33);
  op = common()->IfTrue();
  Node* n39 = graph()->NewNode(op, nil);
  USE(n39);
  op = common()->Branch();
  Node* n38 = graph()->NewNode(op, nil, nil);
  USE(n38);
  op = js()->Equal();
  Node* n37 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n37);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n28 = graph()->NewNode(op, nil, nil, nil);
  USE(n28);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n13 = graph()->NewNode(op, nil, nil, nil);
  USE(n13);
  op = common()->NumberConstant(0);
  Node* n7 = graph()->NewNode(op);
  USE(n7);
  n13->ReplaceInput(0, n7);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n54 = graph()->NewNode(op, nil, nil, nil);
  USE(n54);
  n54->ReplaceInput(0, n28);
  op = js()->Add();
  Node* n52 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n52);
  op = js()->ToNumber();
  Node* n51 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n51);
  n51->ReplaceInput(0, n28);
  n51->ReplaceInput(1, n6);
  n51->ReplaceInput(2, n47);
  op = common()->IfFalse();
  Node* n50 = graph()->NewNode(op, nil);
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
  op = common()->NumberConstant(0);
  Node* n36 = graph()->NewNode(op);
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
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n29 = graph()->NewNode(op, nil, nil, nil);
  USE(n29);
  n29->ReplaceInput(0, n22);
  op = js()->Add();
  Node* n45 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n45);
  op = js()->ToNumber();
  Node* n44 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n44);
  n44->ReplaceInput(0, n25);
  n44->ReplaceInput(1, n6);
  n44->ReplaceInput(2, n37);
  op = common()->IfFalse();
  Node* n43 = graph()->NewNode(op, nil);
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
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n14 = graph()->NewNode(op, nil, nil, nil);
  USE(n14);
  n14->ReplaceInput(0, n0);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n55 = graph()->NewNode(op, nil, nil, nil);
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
  op = common()->IfFalse();
  Node* n19 = graph()->NewNode(op, nil);
  USE(n19);
  n19->ReplaceInput(0, n17);
  n56->ReplaceInput(4, n19);
  n57->ReplaceInput(0, n56);
  n57->ReplaceInput(1, n56);
  n57->ReplaceInput(2, n19);
  n58->ReplaceInput(0, n57);

  graph()->SetStart(n0);
  graph()->SetEnd(n58);

  ComputeAndVerifySchedule(62);
}


TEST_F(SchedulerTest, BuildScheduleSimpleLoopWithCodeMotion) {
  FLAG_turbo_deoptimization = false;

  const Operator* op;

  // Manually transcripted code for:
  // function turbo_fan_test(a, b, c) {
  //   while (a < b) {
  //     a += b + c;
  //   }
  //   return a;
  // }
  op = common()->Start(0);
  Node* n0 = graph()->NewNode(op);
  USE(n0);
  Node* nil = graph()->NewNode(common()->Dead());
  op = common()->End();
  Node* n22 = graph()->NewNode(op, nil);
  USE(n22);
  op = common()->Return();
  Node* n21 = graph()->NewNode(op, nil, nil, nil);
  USE(n21);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n9 = graph()->NewNode(op, nil, nil, nil);
  USE(n9);
  op = common()->Parameter(0);
  Node* n2 = graph()->NewNode(op, n0);
  USE(n2);
  n9->ReplaceInput(0, n2);
  op = js()->Add();
  Node* n20 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n20);
  n20->ReplaceInput(0, n9);
  op = &kIntAdd;
  Node* n19 = graph()->NewNode(op, nil, nil);
  USE(n19);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n10 = graph()->NewNode(op, nil, nil, nil);
  USE(n10);
  op = common()->Parameter(0);
  Node* n3 = graph()->NewNode(op, n0);
  USE(n3);
  n10->ReplaceInput(0, n3);
  n10->ReplaceInput(1, n10);
  op = common()->Loop(2);
  Node* n7 = graph()->NewNode(op, nil, nil);
  USE(n7);
  n7->ReplaceInput(0, n0);
  op = common()->IfTrue();
  Node* n17 = graph()->NewNode(op, nil);
  USE(n17);
  op = common()->Branch();
  Node* n16 = graph()->NewNode(op, nil, nil);
  USE(n16);
  op = js()->ToBoolean();
  Node* n15 = graph()->NewNode(op, nil, nil, nil, nil);
  USE(n15);
  op = js()->LessThan();
  Node* n14 = graph()->NewNode(op, nil, nil, nil, nil, nil);
  USE(n14);
  n14->ReplaceInput(0, n9);
  n14->ReplaceInput(1, n10);
  op = &kHeapConstant;
  Node* n6 = graph()->NewNode(op);
  USE(n6);
  n14->ReplaceInput(2, n6);
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n12 = graph()->NewNode(op, nil, nil, nil);
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
  op = common()->Phi(kMachAnyTagged, 2);
  Node* n11 = graph()->NewNode(op, nil, nil, nil);
  USE(n11);
  op = common()->Parameter(0);
  Node* n4 = graph()->NewNode(op, n0);
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
  op = common()->IfFalse();
  Node* n18 = graph()->NewNode(op, nil);
  USE(n18);
  n18->ReplaceInput(0, n16);
  n21->ReplaceInput(2, n18);
  n22->ReplaceInput(0, n21);

  graph()->SetStart(n0);
  graph()->SetEnd(n22);

  ComputeAndVerifySchedule(19);
}


namespace {

Node* CreateDiamond(Graph* graph, CommonOperatorBuilder* common, Node* cond) {
  Node* tv = graph->NewNode(common->Int32Constant(6));
  Node* fv = graph->NewNode(common->Int32Constant(7));
  Node* br = graph->NewNode(common->Branch(), cond, graph->start());
  Node* t = graph->NewNode(common->IfTrue(), br);
  Node* f = graph->NewNode(common->IfFalse(), br);
  Node* m = graph->NewNode(common->Merge(2), t, f);
  Node* phi = graph->NewNode(common->Phi(kMachAnyTagged, 2), tv, fv, m);
  return phi;
}

}  // namespace


TARGET_TEST_F(SchedulerTest, FloatingDiamond1) {
  Node* start = graph()->NewNode(common()->Start(1));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* d1 = CreateDiamond(graph(), common(), p0);
  Node* ret = graph()->NewNode(common()->Return(), d1, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(13);
}


TARGET_TEST_F(SchedulerTest, FloatingDiamond2) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* p1 = graph()->NewNode(common()->Parameter(1), start);
  Node* d1 = CreateDiamond(graph(), common(), p0);
  Node* d2 = CreateDiamond(graph(), common(), p1);
  Node* add = graph()->NewNode(&kIntAdd, d1, d2);
  Node* ret = graph()->NewNode(common()->Return(), add, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(24);
}


TARGET_TEST_F(SchedulerTest, FloatingDiamond3) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* p1 = graph()->NewNode(common()->Parameter(1), start);
  Node* d1 = CreateDiamond(graph(), common(), p0);
  Node* d2 = CreateDiamond(graph(), common(), p1);
  Node* add = graph()->NewNode(&kIntAdd, d1, d2);
  Node* d3 = CreateDiamond(graph(), common(), add);
  Node* ret = graph()->NewNode(common()->Return(), d3, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(33);
}


TARGET_TEST_F(SchedulerTest, NestedFloatingDiamonds) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);

  Node* fv = graph()->NewNode(common()->Int32Constant(7));
  Node* br = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);

  Node* map = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()), p0, p0,
      p0, start, f);
  Node* br1 = graph()->NewNode(common()->Branch(), map, graph()->start());
  Node* t1 = graph()->NewNode(common()->IfTrue(), br1);
  Node* f1 = graph()->NewNode(common()->IfFalse(), br1);
  Node* m1 = graph()->NewNode(common()->Merge(2), t1, f1);
  Node* ttrue = graph()->NewNode(common()->Int32Constant(1));
  Node* ffalse = graph()->NewNode(common()->Int32Constant(0));
  Node* phi1 =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), ttrue, ffalse, m1);


  Node* m = graph()->NewNode(common()->Merge(2), t, f);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), fv, phi1, m);
  Node* ephi1 = graph()->NewNode(common()->EffectPhi(2), start, map, m);

  Node* ret = graph()->NewNode(common()->Return(), phi, ephi1, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(23);
}


TARGET_TEST_F(SchedulerTest, NestedFloatingDiamondWithChain) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* p1 = graph()->NewNode(common()->Parameter(1), start);
  Node* c = graph()->NewNode(common()->Int32Constant(7));

  Node* brA1 = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* tA1 = graph()->NewNode(common()->IfTrue(), brA1);
  Node* fA1 = graph()->NewNode(common()->IfFalse(), brA1);
  Node* mA1 = graph()->NewNode(common()->Merge(2), tA1, fA1);
  Node* phiA1 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p1, mA1);

  Node* brB1 = graph()->NewNode(common()->Branch(), p1, graph()->start());
  Node* tB1 = graph()->NewNode(common()->IfTrue(), brB1);
  Node* fB1 = graph()->NewNode(common()->IfFalse(), brB1);
  Node* mB1 = graph()->NewNode(common()->Merge(2), tB1, fB1);
  Node* phiB1 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p1, mB1);

  Node* brA2 = graph()->NewNode(common()->Branch(), phiB1, mA1);
  Node* tA2 = graph()->NewNode(common()->IfTrue(), brA2);
  Node* fA2 = graph()->NewNode(common()->IfFalse(), brA2);
  Node* mA2 = graph()->NewNode(common()->Merge(2), tA2, fA2);
  Node* phiA2 =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), phiB1, c, mA2);

  Node* brB2 = graph()->NewNode(common()->Branch(), phiA1, mB1);
  Node* tB2 = graph()->NewNode(common()->IfTrue(), brB2);
  Node* fB2 = graph()->NewNode(common()->IfFalse(), brB2);
  Node* mB2 = graph()->NewNode(common()->Merge(2), tB2, fB2);
  Node* phiB2 =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), phiA1, c, mB2);

  Node* add = graph()->NewNode(&kIntAdd, phiA2, phiB2);
  Node* ret = graph()->NewNode(common()->Return(), add, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(36);
}


TARGET_TEST_F(SchedulerTest, NestedFloatingDiamondWithLoop) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);

  Node* fv = graph()->NewNode(common()->Int32Constant(7));
  Node* br = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);

  Node* loop = graph()->NewNode(common()->Loop(2), f, start);
  Node* ind = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p0, loop);

  Node* add = graph()->NewNode(&kIntAdd, ind, fv);
  Node* br1 = graph()->NewNode(common()->Branch(), add, loop);
  Node* t1 = graph()->NewNode(common()->IfTrue(), br1);
  Node* f1 = graph()->NewNode(common()->IfFalse(), br1);

  loop->ReplaceInput(1, t1);  // close loop.
  ind->ReplaceInput(1, ind);  // close induction variable.

  Node* m = graph()->NewNode(common()->Merge(2), t, f1);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), fv, ind, m);

  Node* ret = graph()->NewNode(common()->Return(), phi, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(20);
}


TARGET_TEST_F(SchedulerTest, LoopedFloatingDiamond1) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);

  Node* c = graph()->NewNode(common()->Int32Constant(7));
  Node* loop = graph()->NewNode(common()->Loop(2), start, start);
  Node* ind = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p0, loop);
  Node* add = graph()->NewNode(&kIntAdd, ind, c);

  Node* br = graph()->NewNode(common()->Branch(), add, loop);
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);

  Node* br1 = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* t1 = graph()->NewNode(common()->IfTrue(), br1);
  Node* f1 = graph()->NewNode(common()->IfFalse(), br1);
  Node* m1 = graph()->NewNode(common()->Merge(2), t1, f1);
  Node* phi1 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), add, p0, m1);

  loop->ReplaceInput(1, t);    // close loop.
  ind->ReplaceInput(1, phi1);  // close induction variable.

  Node* ret = graph()->NewNode(common()->Return(), ind, start, f);
  Node* end = graph()->NewNode(common()->End(), ret, f);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(20);
}


TARGET_TEST_F(SchedulerTest, LoopedFloatingDiamond2) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);

  Node* c = graph()->NewNode(common()->Int32Constant(7));
  Node* loop = graph()->NewNode(common()->Loop(2), start, start);
  Node* ind = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p0, loop);

  Node* br1 = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* t1 = graph()->NewNode(common()->IfTrue(), br1);
  Node* f1 = graph()->NewNode(common()->IfFalse(), br1);
  Node* m1 = graph()->NewNode(common()->Merge(2), t1, f1);
  Node* phi1 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), c, ind, m1);

  Node* add = graph()->NewNode(&kIntAdd, ind, phi1);

  Node* br = graph()->NewNode(common()->Branch(), add, loop);
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);

  loop->ReplaceInput(1, t);   // close loop.
  ind->ReplaceInput(1, add);  // close induction variable.

  Node* ret = graph()->NewNode(common()->Return(), ind, start, f);
  Node* end = graph()->NewNode(common()->End(), ret, f);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(20);
}


TARGET_TEST_F(SchedulerTest, LoopedFloatingDiamond3) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);

  Node* c = graph()->NewNode(common()->Int32Constant(7));
  Node* loop = graph()->NewNode(common()->Loop(2), start, start);
  Node* ind = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p0, loop);

  Node* br1 = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* t1 = graph()->NewNode(common()->IfTrue(), br1);
  Node* f1 = graph()->NewNode(common()->IfFalse(), br1);

  Node* loop1 = graph()->NewNode(common()->Loop(2), t1, start);
  Node* ind1 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p0, loop);

  Node* add1 = graph()->NewNode(&kIntAdd, ind1, c);
  Node* br2 = graph()->NewNode(common()->Branch(), add1, loop1);
  Node* t2 = graph()->NewNode(common()->IfTrue(), br2);
  Node* f2 = graph()->NewNode(common()->IfFalse(), br2);

  loop1->ReplaceInput(1, t2);   // close inner loop.
  ind1->ReplaceInput(1, ind1);  // close inner induction variable.

  Node* m1 = graph()->NewNode(common()->Merge(2), f1, f2);
  Node* phi1 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), c, ind1, m1);

  Node* add = graph()->NewNode(&kIntAdd, ind, phi1);

  Node* br = graph()->NewNode(common()->Branch(), add, loop);
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);

  loop->ReplaceInput(1, t);   // close loop.
  ind->ReplaceInput(1, add);  // close induction variable.

  Node* ret = graph()->NewNode(common()->Return(), ind, start, f);
  Node* end = graph()->NewNode(common()->End(), ret, f);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(28);
}


TARGET_TEST_F(SchedulerTest, PhisPushedDownToDifferentBranches) {
  Node* start = graph()->NewNode(common()->Start(2));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* p1 = graph()->NewNode(common()->Parameter(1), start);

  Node* v1 = graph()->NewNode(common()->Int32Constant(1));
  Node* v2 = graph()->NewNode(common()->Int32Constant(2));
  Node* v3 = graph()->NewNode(common()->Int32Constant(3));
  Node* v4 = graph()->NewNode(common()->Int32Constant(4));
  Node* br = graph()->NewNode(common()->Branch(), p0, graph()->start());
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);
  Node* m = graph()->NewNode(common()->Merge(2), t, f);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), v1, v2, m);
  Node* phi2 = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), v3, v4, m);

  Node* br2 = graph()->NewNode(common()->Branch(), p1, graph()->start());
  Node* t2 = graph()->NewNode(common()->IfTrue(), br2);
  Node* f2 = graph()->NewNode(common()->IfFalse(), br2);
  Node* m2 = graph()->NewNode(common()->Merge(2), t2, f2);
  Node* phi3 =
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), phi, phi2, m2);

  Node* ret = graph()->NewNode(common()->Return(), phi3, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(24);
}


TARGET_TEST_F(SchedulerTest, BranchHintTrue) {
  Node* start = graph()->NewNode(common()->Start(1));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* tv = graph()->NewNode(common()->Int32Constant(6));
  Node* fv = graph()->NewNode(common()->Int32Constant(7));
  Node* br = graph()->NewNode(common()->Branch(BranchHint::kTrue), p0, start);
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);
  Node* m = graph()->NewNode(common()->Merge(2), t, f);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), tv, fv, m);
  Node* ret = graph()->NewNode(common()->Return(), phi, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  Schedule* schedule = ComputeAndVerifySchedule(13);
  // Make sure the false block is marked as deferred.
  EXPECT_FALSE(schedule->block(t)->deferred());
  EXPECT_TRUE(schedule->block(f)->deferred());
}


TARGET_TEST_F(SchedulerTest, BranchHintFalse) {
  Node* start = graph()->NewNode(common()->Start(1));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* tv = graph()->NewNode(common()->Int32Constant(6));
  Node* fv = graph()->NewNode(common()->Int32Constant(7));
  Node* br = graph()->NewNode(common()->Branch(BranchHint::kFalse), p0, start);
  Node* t = graph()->NewNode(common()->IfTrue(), br);
  Node* f = graph()->NewNode(common()->IfFalse(), br);
  Node* m = graph()->NewNode(common()->Merge(2), t, f);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), tv, fv, m);
  Node* ret = graph()->NewNode(common()->Return(), phi, start, start);
  Node* end = graph()->NewNode(common()->End(), ret, start);

  graph()->SetEnd(end);

  Schedule* schedule = ComputeAndVerifySchedule(13);
  // Make sure the true block is marked as deferred.
  EXPECT_TRUE(schedule->block(t)->deferred());
  EXPECT_FALSE(schedule->block(f)->deferred());
}


TARGET_TEST_F(SchedulerTest, CallException) {
  Node* start = graph()->NewNode(common()->Start(1));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* c1 = graph()->NewNode(&kMockCall, start);
  Node* ok1 = graph()->NewNode(common()->IfSuccess(), c1);
  Node* ex1 = graph()->NewNode(common()->IfException(), c1);
  Node* c2 = graph()->NewNode(&kMockCall, ok1);
  Node* ok2 = graph()->NewNode(common()->IfSuccess(), c2);
  Node* ex2 = graph()->NewNode(common()->IfException(), c2);
  Node* hdl = graph()->NewNode(common()->Merge(2), ex1, ex2);
  Node* m = graph()->NewNode(common()->Merge(2), ok2, hdl);
  Node* phi = graph()->NewNode(common()->Phi(kMachAnyTagged, 2), c2, p0, m);
  Node* ret = graph()->NewNode(common()->Return(), phi, start, m);
  Node* end = graph()->NewNode(common()->End(), ret);

  graph()->SetEnd(end);

  Schedule* schedule = ComputeAndVerifySchedule(17);
  // Make sure the exception blocks as well as the handler are deferred.
  EXPECT_TRUE(schedule->block(ex1)->deferred());
  EXPECT_TRUE(schedule->block(ex2)->deferred());
  EXPECT_TRUE(schedule->block(hdl)->deferred());
  EXPECT_FALSE(schedule->block(m)->deferred());
}


TARGET_TEST_F(SchedulerTest, Switch) {
  Node* start = graph()->NewNode(common()->Start(1));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* sw = graph()->NewNode(common()->Switch(3), p0, start);
  Node* c0 = graph()->NewNode(common()->IfValue(0), sw);
  Node* v0 = graph()->NewNode(common()->Int32Constant(11));
  Node* c1 = graph()->NewNode(common()->IfValue(1), sw);
  Node* v1 = graph()->NewNode(common()->Int32Constant(22));
  Node* d = graph()->NewNode(common()->IfDefault(), sw);
  Node* vd = graph()->NewNode(common()->Int32Constant(33));
  Node* m = graph()->NewNode(common()->Merge(3), c0, c1, d);
  Node* phi = graph()->NewNode(common()->Phi(kMachInt32, 3), v0, v1, vd, m);
  Node* ret = graph()->NewNode(common()->Return(), phi, start, m);
  Node* end = graph()->NewNode(common()->End(), ret);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(16);
}


TARGET_TEST_F(SchedulerTest, FloatingSwitch) {
  Node* start = graph()->NewNode(common()->Start(1));
  graph()->SetStart(start);

  Node* p0 = graph()->NewNode(common()->Parameter(0), start);
  Node* sw = graph()->NewNode(common()->Switch(3), p0, start);
  Node* c0 = graph()->NewNode(common()->IfValue(0), sw);
  Node* v0 = graph()->NewNode(common()->Int32Constant(11));
  Node* c1 = graph()->NewNode(common()->IfValue(1), sw);
  Node* v1 = graph()->NewNode(common()->Int32Constant(22));
  Node* d = graph()->NewNode(common()->IfDefault(), sw);
  Node* vd = graph()->NewNode(common()->Int32Constant(33));
  Node* m = graph()->NewNode(common()->Merge(3), c0, c1, d);
  Node* phi = graph()->NewNode(common()->Phi(kMachInt32, 3), v0, v1, vd, m);
  Node* ret = graph()->NewNode(common()->Return(), phi, start, start);
  Node* end = graph()->NewNode(common()->End(), ret);

  graph()->SetEnd(end);

  ComputeAndVerifySchedule(16);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
