// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static SimpleOperator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                                     0, 0, "dummy");

TEST(TestScheduleAllocation) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  CHECK_NE(NULL, schedule.start());
  CHECK_EQ(schedule.start(), *(schedule.all_blocks().begin()));
}


TEST(TestScheduleAddNode) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* n1 = graph.NewNode(&dummy_operator);

  Schedule schedule(scope.main_zone());

  BasicBlock* entry = schedule.start();
  schedule.AddNode(entry, n0);
  schedule.AddNode(entry, n1);

  CHECK_EQ(entry, schedule.block(n0));
  CHECK_EQ(entry, schedule.block(n1));
  CHECK(schedule.SameBasicBlock(n0, n1));

  Node* n2 = graph.NewNode(&dummy_operator);
  CHECK_EQ(NULL, schedule.block(n2));
}


TEST(TestScheduleAddGoto) {
  HandleAndZoneScope scope;

  Schedule schedule(scope.main_zone());
  BasicBlock* entry = schedule.start();
  BasicBlock* next = schedule.NewBasicBlock();

  schedule.AddGoto(entry, next);

  CHECK_EQ(0, entry->PredecessorCount());
  CHECK_EQ(1, entry->SuccessorCount());
  CHECK_EQ(next, entry->SuccessorAt(0));

  CHECK_EQ(1, next->PredecessorCount());
  CHECK_EQ(entry, next->PredecessorAt(0));
  CHECK_EQ(0, next->SuccessorCount());
}


TEST(TestScheduleAddBranch) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());

  BasicBlock* entry = schedule.start();
  BasicBlock* tblock = schedule.NewBasicBlock();
  BasicBlock* fblock = schedule.NewBasicBlock();

  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());
  Node* n0 = graph.NewNode(&dummy_operator);
  Node* b = graph.NewNode(common.Branch(), n0);

  schedule.AddBranch(entry, b, tblock, fblock);

  CHECK_EQ(0, entry->PredecessorCount());
  CHECK_EQ(2, entry->SuccessorCount());
  CHECK_EQ(tblock, entry->SuccessorAt(0));
  CHECK_EQ(fblock, entry->SuccessorAt(1));

  CHECK_EQ(1, tblock->PredecessorCount());
  CHECK_EQ(entry, tblock->PredecessorAt(0));
  CHECK_EQ(0, tblock->SuccessorCount());

  CHECK_EQ(1, fblock->PredecessorCount());
  CHECK_EQ(entry, fblock->PredecessorAt(0));
  CHECK_EQ(0, fblock->SuccessorCount());
}


TEST(TestScheduleAddReturn) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  Graph graph(scope.main_zone());
  Node* n0 = graph.NewNode(&dummy_operator);
  BasicBlock* entry = schedule.start();
  schedule.AddReturn(entry, n0);

  CHECK_EQ(0, entry->PredecessorCount());
  CHECK_EQ(1, entry->SuccessorCount());
  CHECK_EQ(schedule.end(), entry->SuccessorAt(0));
}


TEST(TestScheduleAddThrow) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  Graph graph(scope.main_zone());
  Node* n0 = graph.NewNode(&dummy_operator);
  BasicBlock* entry = schedule.start();
  schedule.AddThrow(entry, n0);

  CHECK_EQ(0, entry->PredecessorCount());
  CHECK_EQ(1, entry->SuccessorCount());
  CHECK_EQ(schedule.end(), entry->SuccessorAt(0));
}


TEST(BuildMulNodeGraph) {
  HandleAndZoneScope scope;
  Schedule schedule(scope.main_zone());
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());
  MachineOperatorBuilder machine;

  Node* start = graph.NewNode(common.Start(0));
  graph.SetStart(start);
  Node* param0 = graph.NewNode(common.Parameter(0), graph.start());
  Node* param1 = graph.NewNode(common.Parameter(1), graph.start());

  Node* mul = graph.NewNode(machine.Int32Mul(), param0, param1);
  Node* ret = graph.NewNode(common.Return(), mul, start);

  USE(ret);
}
