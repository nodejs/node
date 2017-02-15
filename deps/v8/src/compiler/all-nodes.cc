// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/all-nodes.h"

#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

AllNodes::AllNodes(Zone* local_zone, const Graph* graph, bool only_inputs)
    : reachable(local_zone),
      is_reachable_(graph->NodeCount(), false, local_zone),
      only_inputs_(only_inputs) {
  Mark(local_zone, graph->end(), graph);
}

AllNodes::AllNodes(Zone* local_zone, Node* end, const Graph* graph,
                   bool only_inputs)
    : reachable(local_zone),
      is_reachable_(graph->NodeCount(), false, local_zone),
      only_inputs_(only_inputs) {
  Mark(local_zone, end, graph);
}

void AllNodes::Mark(Zone* local_zone, Node* end, const Graph* graph) {
  DCHECK_LT(end->id(), graph->NodeCount());
  is_reachable_[end->id()] = true;
  reachable.push_back(end);
  // Find all nodes reachable from {end}.
  for (size_t i = 0; i < reachable.size(); i++) {
    for (Node* const input : reachable[i]->inputs()) {
      if (input == nullptr) {
        // TODO(titzer): print a warning.
        continue;
      }
      if (!is_reachable_[input->id()]) {
        is_reachable_[input->id()] = true;
        reachable.push_back(input);
      }
    }
    if (!only_inputs_) {
      for (Node* use : reachable[i]->uses()) {
        if (use == nullptr || use->id() >= graph->NodeCount()) {
          continue;
        }
        if (!is_reachable_[use->id()]) {
          is_reachable_[use->id()] = true;
          reachable.push_back(use);
        }
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
