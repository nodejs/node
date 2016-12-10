// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyOf;

namespace v8 {
namespace internal {
namespace compiler {

class SchedulerRPOTest : public TestWithZone {
 public:
  SchedulerRPOTest() {}

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
    loop->nodes = new BasicBlock*[count];
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
  std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, 2));
  schedule.AddSuccessorForTesting(schedule.start(), loop1->header());
  BasicBlockVector* order = Scheduler::ComputeSpecialRPO(zone(), &schedule);
  CheckRPONumbers(order, 3, true);
  CheckLoop(order, loop1->nodes, loop1->count);
}

TEST_F(SchedulerRPOTest, EndLoopNested) {
  Schedule schedule(zone());
  std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, 2));
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

  std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, 1));
  std::unique_ptr<TestLoop> loop2(CreateLoop(&schedule, 1));

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

  std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, 1));
  std::unique_ptr<TestLoop> loop2(CreateLoop(&schedule, 1));

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
      std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, size));
      std::unique_ptr<TestLoop> loop2(CreateLoop(&schedule, size));
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

  std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, 1));
  std::unique_ptr<TestLoop> loop2(CreateLoop(&schedule, 1));

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

      std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, size));
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

      std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, size));
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

    std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, size));
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
    std::unique_ptr<TestLoop> loop1(CreateLoop(&schedule, size));
    schedule.AddSuccessorForTesting(A, loop1->header());
    schedule.AddSuccessorForTesting(loop1->last(), E);

    TestLoop** loopN = new TestLoop*[size];
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
