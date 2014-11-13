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
#include "src/compiler/verifier.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

TEST(NodeWithNullInputReachableFromEnd) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());

  Node* start = graph.NewNode(common.Start(0));
  graph.SetStart(start);
  Node* k = graph.NewNode(common.Int32Constant(0));
  Node* phi = graph.NewNode(common.Phi(kMachAnyTagged, 1), k, start);
  phi->ReplaceInput(0, NULL);
  graph.SetEnd(phi);

  OFStream os(stdout);
  os << AsDOT(graph);
  os << AsJSON(graph);
}


TEST(NodeWithNullControlReachableFromEnd) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());

  Node* start = graph.NewNode(common.Start(0));
  graph.SetStart(start);
  Node* k = graph.NewNode(common.Int32Constant(0));
  Node* phi = graph.NewNode(common.Phi(kMachAnyTagged, 1), k, start);
  phi->ReplaceInput(1, NULL);
  graph.SetEnd(phi);

  OFStream os(stdout);
  os << AsDOT(graph);
  os << AsJSON(graph);
}


TEST(NodeWithNullInputReachableFromStart) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());

  Node* start = graph.NewNode(common.Start(0));
  graph.SetStart(start);
  Node* k = graph.NewNode(common.Int32Constant(0));
  Node* phi = graph.NewNode(common.Phi(kMachAnyTagged, 1), k, start);
  phi->ReplaceInput(0, NULL);
  graph.SetEnd(start);

  OFStream os(stdout);
  os << AsDOT(graph);
  os << AsJSON(graph);
}


TEST(NodeWithNullControlReachableFromStart) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());

  Node* start = graph.NewNode(common.Start(0));
  graph.SetStart(start);
  Node* merge = graph.NewNode(common.Merge(2), start, start);
  merge->ReplaceInput(1, NULL);
  graph.SetEnd(merge);

  OFStream os(stdout);
  os << AsDOT(graph);
  os << AsJSON(graph);
}
