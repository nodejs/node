// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ALL_NODES_H_
#define V8_COMPILER_ALL_NODES_H_

#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper utility that traverses the graph and gathers all nodes reachable
// from end.
class AllNodes {
 public:
  // Constructor. Traverses the graph and builds the {live} sets.
  AllNodes(Zone* local_zone, const Graph* graph);

  bool IsLive(Node* node) {
    if (!node) return false;
    size_t id = node->id();
    return id < is_live.size() && is_live[id];
  }

  NodeVector live;  // Nodes reachable from end.

 private:
  BoolVector is_live;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ALL_NODES_H_
