// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/all-nodes.h"

namespace v8 {
namespace internal {
namespace compiler {

AllNodes::AllNodes(Zone* local_zone, const Graph* graph)
    : live(local_zone),
      gray(local_zone),
      state(graph->NodeCount(), AllNodes::kDead, local_zone) {
  Node* end = graph->end();
  state[end->id()] = AllNodes::kLive;
  live.push_back(end);
  // Find all live nodes reachable from end.
  for (size_t i = 0; i < live.size(); i++) {
    for (Node* const input : live[i]->inputs()) {
      if (input == nullptr) {
        // TODO(titzer): print a warning.
        continue;
      }
      if (input->id() >= graph->NodeCount()) {
        // TODO(titzer): print a warning.
        continue;
      }
      if (state[input->id()] != AllNodes::kLive) {
        live.push_back(input);
        state[input->id()] = AllNodes::kLive;
      }
    }
  }

  // Find all nodes that are not reachable from end that use live nodes.
  for (size_t i = 0; i < live.size(); i++) {
    for (Node* const use : live[i]->uses()) {
      if (state[use->id()] == AllNodes::kDead) {
        gray.push_back(use);
        state[use->id()] = AllNodes::kGray;
      }
    }
  }
}
}
}
}  // namespace v8::internal::compiler
