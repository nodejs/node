/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_HEAP_GRAPH_WALKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_HEAP_GRAPH_WALKER_H_

#include <inttypes.h>
#include <map>
#include <set>
#include <vector>

// Implements two algorithms that walk a HeapGraph.
// a) Traverse all references from roots and mark the nodes as reachable.
// b) For each node, calculate two numbers:
//    1. retained: The number of bytes that are directly and indirectly
//       referenced by the node.
//    2. uniquely retained: The number of bytes that are only retained through
//       this object. If this object were destroyed, this many bytes would be
//       freed up.
//
// The algorithm for b) is a modified Tarjan's algorithm. We use Tarjan's
// algorithm to find connected components. This is such that we break cycles
// that can exist in the retention graphs. All nodes within the cycle get
// assigned the same component. Then, most of the graph algorithm operates on
// these components.
//
// For instance, the below graph, which for simplicity does not contain any
// loops.
// Apart from nodes retaining / uniquely retaining themselves:
// a retains nothing
// a uniquely retains nothing
//
// b retains a
// b uniquely retains nothing
//
// c retains a
// c uniquely retains nothing
//
// d retains a, b, c
// d uniquely retains a, b, c
//
//     a      |
//    ^^      |
//   /  \     |
//   b   c    |
//   ^   ^    |
//    \ /     |
//     d      |
//
// The basic idea of the algorithm is to keep track of the number of unvisited
// nodes that retain the node. Nodes that have multiple parents get double
// counted; this is so we can always reduce the number of unvisited nodes by
// the number of edges from a node that retain a node.
//
// In the same graph:
// visiting a: 2 unvisited nodes retain a {b, c}
// visiting b: 2 unvisited nodes retain a {d, c}
// visiting c: 2 unvisited nodes retain a {d, d}
// visiting d: 0 unvisited nodes retain a
//
//
// A more complete example
//
//     a       |
//    ^^       |
//   /  \      |
//   b   c     |
//   ^   ^     |
//    \ / \    |
//     d  e    |
//     ^  ^    |
//     \  /    |
//      f      |
//
// visiting a: 2 unvisited nodes retain a ({b, c})
// visiting b: 2 unvisited nodes retain a ({d, c})
// visiting c: 3 unvisited nodes retain a ({d, d, e})
// visiting d: 2 unvisited nodes retain a ({f, e})
// visiting e: 2 unvisited nodes retain a ({f, f})
// visiting f: 0 unvisited nodes retain a

namespace perfetto {
namespace trace_processor {

class HeapGraphWalker {
 public:
  using ClassNameId = int64_t;

  struct PathFromRoot {
    static constexpr size_t kRoot = 0;
    struct Node {
      uint32_t depth = 0;
      // Invariant: parent_id < id of this node.
      size_t parent_id = 0;
      uint64_t size = 0;
      uint64_t count = 0;
      ClassNameId class_name = 0;
      std::map<ClassNameId, size_t> children;
    };
    std::vector<Node> nodes{Node{}};
  };

  class Delegate {
   public:
    virtual ~Delegate();
    virtual void MarkReachable(int64_t row) = 0;
    virtual void SetRetained(int64_t row,
                             int64_t retained,
                             int64_t unique_retained) = 0;
  };

  HeapGraphWalker(Delegate* delegate) : delegate_(delegate) {}

  void AddEdge(int64_t owner_row, int64_t owned_row);
  void AddNode(int64_t row, uint64_t size) { AddNode(row, size, 0); }
  void AddNode(int64_t row, uint64_t size, ClassNameId class_name);

  // Mark a a node as root. This marks all the nodes reachable from it as
  // reachable.
  void MarkRoot(int64_t row);
  // Calculate the retained and unique retained size for each node. This
  // includes nodes not reachable from roots.
  void CalculateRetained();

  PathFromRoot FindPathsFromRoot();

 private:
  struct Node {
    std::vector<Node*> children;
    std::vector<Node*> parents;
    uint64_t self_size = 0;
    uint64_t retained_size = 0;

    int64_t row = 0;
    uint64_t node_index = 0;
    uint64_t lowlink = 0;
    int64_t component = -1;

    ClassNameId class_name = 0;
    int32_t distance_to_root = -1;

    bool on_stack = false;
    bool find_paths_from_root_visited = false;

    bool root() { return distance_to_root == 0; }
    bool reachable() { return distance_to_root >= 0; }
  };

  struct Component {
    uint64_t self_size = 0;
    uint64_t unique_retained_size = 0;
    uint64_t unique_retained_root_size = 0;
    size_t incoming_edges = 0;
    size_t orig_incoming_edges = 0;
    size_t pending_nodes = 0;
    std::set<int64_t> children_components;
    uint64_t lowlink = 0;

    bool root = false;
  };

  Node& GetNode(int64_t id) { return nodes_[static_cast<size_t>(id)]; }

  void FindSCC(Node*);
  void FoundSCC(Node*);
  int64_t RetainedSize(const Component&);

  void FindPathFromRoot(Node* n, PathFromRoot* path);

  std::vector<Component> components_;
  std::vector<Node*> node_stack_;
  uint64_t next_node_index_ = 1;
  std::vector<Node> nodes_;

  std::vector<Node*> roots_;

  Delegate* delegate_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_HEAP_GRAPH_WALKER_H_
