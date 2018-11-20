// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ALL_NODES_H_
#define V8_COMPILER_ALL_NODES_H_

#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper utility that traverses the graph and gathers all nodes reachable
// from end.
class AllNodes {
 public:
  // Constructor. Traverses the graph and builds the {live} and {gray} sets.
  AllNodes(Zone* local_zone, const Graph* graph);

  bool IsLive(Node* node) {
    return node != nullptr && node->id() < static_cast<int>(state.size()) &&
           state[node->id()] == kLive;
  }

  NodeVector live;  // Nodes reachable from end.
  NodeVector gray;  // Nodes themselves not reachable from end, but that
                    // appear in use lists of live nodes.

 private:
  enum State { kDead, kGray, kLive };

  ZoneVector<State> state;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_ALL_NODES_H_
