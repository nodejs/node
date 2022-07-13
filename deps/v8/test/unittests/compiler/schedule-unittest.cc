// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/schedule.h"

#include "src/compiler/node.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;

namespace v8 {
namespace internal {
namespace compiler {
namespace schedule_unittest {

using BasicBlockTest = TestWithIsolateAndZone;

TEST_F(BasicBlockTest, Constructor) {
  int const id = random_number_generator()->NextInt();
  BasicBlock b(zone(), BasicBlock::Id::FromInt(id));
  EXPECT_FALSE(b.deferred());
  EXPECT_GT(0, b.dominator_depth());
  EXPECT_EQ(nullptr, b.dominator());
  EXPECT_EQ(nullptr, b.rpo_next());
  EXPECT_EQ(id, b.id().ToInt());
}


TEST_F(BasicBlockTest, GetCommonDominator1) {
  BasicBlock b(zone(), BasicBlock::Id::FromInt(0));
  EXPECT_EQ(&b, BasicBlock::GetCommonDominator(&b, &b));
}


TEST_F(BasicBlockTest, GetCommonDominator2) {
  BasicBlock b0(zone(), BasicBlock::Id::FromInt(0));
  BasicBlock b1(zone(), BasicBlock::Id::FromInt(1));
  BasicBlock b2(zone(), BasicBlock::Id::FromInt(2));
  b0.set_dominator_depth(0);
  b1.set_dominator(&b0);
  b1.set_dominator_depth(1);
  b2.set_dominator(&b1);
  b2.set_dominator_depth(2);
  EXPECT_EQ(&b0, BasicBlock::GetCommonDominator(&b0, &b1));
  EXPECT_EQ(&b0, BasicBlock::GetCommonDominator(&b0, &b2));
  EXPECT_EQ(&b0, BasicBlock::GetCommonDominator(&b1, &b0));
  EXPECT_EQ(&b0, BasicBlock::GetCommonDominator(&b2, &b0));
  EXPECT_EQ(&b1, BasicBlock::GetCommonDominator(&b1, &b2));
  EXPECT_EQ(&b1, BasicBlock::GetCommonDominator(&b2, &b1));
}


TEST_F(BasicBlockTest, GetCommonDominator3) {
  BasicBlock b0(zone(), BasicBlock::Id::FromInt(0));
  BasicBlock b1(zone(), BasicBlock::Id::FromInt(1));
  BasicBlock b2(zone(), BasicBlock::Id::FromInt(2));
  BasicBlock b3(zone(), BasicBlock::Id::FromInt(3));
  b0.set_dominator_depth(0);
  b1.set_dominator(&b0);
  b1.set_dominator_depth(1);
  b2.set_dominator(&b0);
  b2.set_dominator_depth(1);
  b3.set_dominator(&b2);
  b3.set_dominator_depth(2);
  EXPECT_EQ(&b0, BasicBlock::GetCommonDominator(&b1, &b3));
  EXPECT_EQ(&b0, BasicBlock::GetCommonDominator(&b3, &b1));
}

class ScheduleTest : public TestWithZone {
 public:
  ScheduleTest() : TestWithZone(kCompressGraphZone) {}
};

const Operator kCallOperator(IrOpcode::kCall, Operator::kNoProperties,
                             "MockCall", 0, 0, 0, 0, 0, 0);
const Operator kBranchOperator(IrOpcode::kBranch, Operator::kNoProperties,
                               "MockBranch", 0, 0, 0, 0, 0, 0);
const Operator kDummyOperator(IrOpcode::kParameter, Operator::kNoProperties,
                              "Dummy", 0, 0, 0, 0, 0, 0);


TEST_F(ScheduleTest, Constructor) {
  Schedule schedule(zone());
  EXPECT_NE(nullptr, schedule.start());
  EXPECT_EQ(schedule.start(),
            schedule.GetBlockById(BasicBlock::Id::FromInt(0)));
  EXPECT_NE(nullptr, schedule.end());
  EXPECT_EQ(schedule.end(), schedule.GetBlockById(BasicBlock::Id::FromInt(1)));
  EXPECT_NE(schedule.start(), schedule.end());
}


TEST_F(ScheduleTest, AddNode) {
  Schedule schedule(zone());
  BasicBlock* start = schedule.start();

  Node* node0 = Node::New(zone(), 0, &kDummyOperator, 0, nullptr, false);
  EXPECT_EQ(nullptr, schedule.block(node0));
  schedule.AddNode(start, node0);
  EXPECT_EQ(start, schedule.block(node0));
  EXPECT_THAT(*start, ElementsAre(node0));

  Node* node1 = Node::New(zone(), 1, &kDummyOperator, 0, nullptr, false);
  EXPECT_EQ(nullptr, schedule.block(node1));
  schedule.AddNode(start, node1);
  EXPECT_EQ(start, schedule.block(node1));
  EXPECT_THAT(*start, ElementsAre(node0, node1));

  EXPECT_TRUE(schedule.SameBasicBlock(node0, node1));
}


TEST_F(ScheduleTest, AddGoto) {
  Schedule schedule(zone());
  BasicBlock* start = schedule.start();
  BasicBlock* end = schedule.end();

  BasicBlock* block = schedule.NewBasicBlock();
  schedule.AddGoto(start, block);

  EXPECT_EQ(0u, start->PredecessorCount());
  EXPECT_EQ(1u, start->SuccessorCount());
  EXPECT_EQ(block, start->SuccessorAt(0));
  EXPECT_THAT(start->successors(), ElementsAre(block));

  EXPECT_EQ(1u, block->PredecessorCount());
  EXPECT_EQ(0u, block->SuccessorCount());
  EXPECT_EQ(start, block->PredecessorAt(0));
  EXPECT_THAT(block->predecessors(), ElementsAre(start));

  EXPECT_EQ(0u, end->PredecessorCount());
  EXPECT_EQ(0u, end->SuccessorCount());
}


TEST_F(ScheduleTest, AddCall) {
  Schedule schedule(zone());
  BasicBlock* start = schedule.start();

  Node* call = Node::New(zone(), 0, &kCallOperator, 0, nullptr, false);
  BasicBlock* sblock = schedule.NewBasicBlock();
  BasicBlock* eblock = schedule.NewBasicBlock();
  schedule.AddCall(start, call, sblock, eblock);

  EXPECT_EQ(start, schedule.block(call));

  EXPECT_EQ(0u, start->PredecessorCount());
  EXPECT_EQ(2u, start->SuccessorCount());
  EXPECT_EQ(sblock, start->SuccessorAt(0));
  EXPECT_EQ(eblock, start->SuccessorAt(1));
  EXPECT_THAT(start->successors(), ElementsAre(sblock, eblock));

  EXPECT_EQ(1u, sblock->PredecessorCount());
  EXPECT_EQ(0u, sblock->SuccessorCount());
  EXPECT_EQ(start, sblock->PredecessorAt(0));
  EXPECT_THAT(sblock->predecessors(), ElementsAre(start));

  EXPECT_EQ(1u, eblock->PredecessorCount());
  EXPECT_EQ(0u, eblock->SuccessorCount());
  EXPECT_EQ(start, eblock->PredecessorAt(0));
  EXPECT_THAT(eblock->predecessors(), ElementsAre(start));
}


TEST_F(ScheduleTest, AddBranch) {
  Schedule schedule(zone());
  BasicBlock* start = schedule.start();

  Node* branch = Node::New(zone(), 0, &kBranchOperator, 0, nullptr, false);
  BasicBlock* tblock = schedule.NewBasicBlock();
  BasicBlock* fblock = schedule.NewBasicBlock();
  schedule.AddBranch(start, branch, tblock, fblock);

  EXPECT_EQ(start, schedule.block(branch));

  EXPECT_EQ(0u, start->PredecessorCount());
  EXPECT_EQ(2u, start->SuccessorCount());
  EXPECT_EQ(tblock, start->SuccessorAt(0));
  EXPECT_EQ(fblock, start->SuccessorAt(1));
  EXPECT_THAT(start->successors(), ElementsAre(tblock, fblock));

  EXPECT_EQ(1u, tblock->PredecessorCount());
  EXPECT_EQ(0u, tblock->SuccessorCount());
  EXPECT_EQ(start, tblock->PredecessorAt(0));
  EXPECT_THAT(tblock->predecessors(), ElementsAre(start));

  EXPECT_EQ(1u, fblock->PredecessorCount());
  EXPECT_EQ(0u, fblock->SuccessorCount());
  EXPECT_EQ(start, fblock->PredecessorAt(0));
  EXPECT_THAT(fblock->predecessors(), ElementsAre(start));
}


TEST_F(ScheduleTest, AddReturn) {
  Schedule schedule(zone());
  BasicBlock* start = schedule.start();
  BasicBlock* end = schedule.end();

  Node* node = Node::New(zone(), 0, &kDummyOperator, 0, nullptr, false);
  schedule.AddReturn(start, node);

  EXPECT_EQ(0u, start->PredecessorCount());
  EXPECT_EQ(1u, start->SuccessorCount());
  EXPECT_EQ(end, start->SuccessorAt(0));
  EXPECT_THAT(start->successors(), ElementsAre(end));
}


TEST_F(ScheduleTest, InsertBranch) {
  Schedule schedule(zone());
  BasicBlock* start = schedule.start();
  BasicBlock* end = schedule.end();

  Node* node = Node::New(zone(), 0, &kDummyOperator, 0, nullptr, false);
  Node* branch = Node::New(zone(), 0, &kBranchOperator, 0, nullptr, false);
  BasicBlock* tblock = schedule.NewBasicBlock();
  BasicBlock* fblock = schedule.NewBasicBlock();
  BasicBlock* mblock = schedule.NewBasicBlock();

  schedule.AddReturn(start, node);
  schedule.AddGoto(tblock, mblock);
  schedule.AddGoto(fblock, mblock);
  schedule.InsertBranch(start, mblock, branch, tblock, fblock);

  EXPECT_EQ(0u, start->PredecessorCount());
  EXPECT_EQ(2u, start->SuccessorCount());
  EXPECT_EQ(tblock, start->SuccessorAt(0));
  EXPECT_EQ(fblock, start->SuccessorAt(1));
  EXPECT_THAT(start->successors(), ElementsAre(tblock, fblock));

  EXPECT_EQ(2u, mblock->PredecessorCount());
  EXPECT_EQ(1u, mblock->SuccessorCount());
  EXPECT_EQ(end, mblock->SuccessorAt(0));
  EXPECT_THAT(mblock->predecessors(), ElementsAre(tblock, fblock));
  EXPECT_THAT(mblock->successors(), ElementsAre(end));

  EXPECT_EQ(1u, end->PredecessorCount());
  EXPECT_EQ(0u, end->SuccessorCount());
  EXPECT_EQ(mblock, end->PredecessorAt(0));
  EXPECT_THAT(end->predecessors(), ElementsAre(mblock));
}

}  // namespace schedule_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
