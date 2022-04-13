// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ALL_NODES_H_
#define V8_COMPILER_ALL_NODES_H_

#include "src/compiler/node.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper utility that traverses the graph and gathers all nodes reachable
// from end.
class AllNodes {
 public:
  // Constructor. Traverses the graph and builds the {reachable} set of nodes
  // reachable from {end}. When {only_inputs} is true, find the nodes
  // reachable through input edges; these are all live nodes.
  AllNodes(Zone* local_zone, Node* end, const Graph* graph,
           bool only_inputs = true);
  // Constructor. Traverses the graph and builds the {reachable} set of nodes
  // reachable from the End node.
  AllNodes(Zone* local_zone, const Graph* graph, bool only_inputs = true);

  bool IsLive(const Node* node) const {
    CHECK(only_inputs_);
    return IsReachable(node);
  }

  bool IsReachable(const Node* node) const {
    if (!node) return false;
    size_t id = node->id();
    return id < is_reachable_.size() && is_reachable_[id];
  }

  NodeVector reachable;  // Nodes reachable from end.

 private:
  void Mark(Zone* local_zone, Node* end, const Graph* graph);

  BoolVector is_reachable_;
  const bool only_inputs_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ALL_NODES_H_
