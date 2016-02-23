// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/all-nodes.h"

#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

AllNodes::AllNodes(Zone* local_zone, const Graph* graph)
    : live(local_zone), is_live(graph->NodeCount(), false, local_zone) {
  Node* end = graph->end();
  is_live[end->id()] = true;
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
      if (!is_live[input->id()]) {
        is_live[input->id()] = true;
        live.push_back(input);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
