// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/source-position.h"
#include "src/compiler/verifier.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

static Operator dummy_operator(IrOpcode::kParameter, Operator::kNoWrite,
                               "dummy", 0, 0, 0, 1, 0, 0);


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
  SourcePositionTable table(&graph);
  os << AsJSON(graph, &table);
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
  SourcePositionTable table(&graph);
  os << AsJSON(graph, &table);
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
  SourcePositionTable table(&graph);
  os << AsJSON(graph, &table);
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
  SourcePositionTable table(&graph);
  os << AsJSON(graph, &table);
}


TEST(NodeNetworkOfDummiesReachableFromEnd) {
  HandleAndZoneScope scope;
  Graph graph(scope.main_zone());
  CommonOperatorBuilder common(scope.main_zone());

  Node* start = graph.NewNode(common.Start(0));
  graph.SetStart(start);
  Node* n2 = graph.NewNode(&dummy_operator, graph.start());
  Node* n3 = graph.NewNode(&dummy_operator, graph.start());
  Node* n4 = graph.NewNode(&dummy_operator, n2);
  Node* n5 = graph.NewNode(&dummy_operator, n2);
  Node* n6 = graph.NewNode(&dummy_operator, n3);
  Node* n7 = graph.NewNode(&dummy_operator, n3);
  Node* n8 = graph.NewNode(&dummy_operator, n5);
  Node* n9 = graph.NewNode(&dummy_operator, n5);
  Node* n10 = graph.NewNode(&dummy_operator, n9);
  Node* n11 = graph.NewNode(&dummy_operator, n9);
  Node* end_dependencies[6] = {n4, n8, n10, n11, n6, n7};
  Node* end = graph.NewNode(&dummy_operator, 6, end_dependencies);
  graph.SetEnd(end);

  OFStream os(stdout);
  os << AsDOT(graph);
  SourcePositionTable table(&graph);
  os << AsJSON(graph, &table);
}
