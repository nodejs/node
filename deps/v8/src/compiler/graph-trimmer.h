// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_TRIMMER_H_
#define V8_COMPILER_GRAPH_TRIMMER_H_

#include "src/compiler/node-marker.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;


// Trims dead nodes from the node graph.
class GraphTrimmer final {
 public:
  GraphTrimmer(Zone* zone, Graph* graph);
  ~GraphTrimmer();

  // Trim nodes in the {graph} that are not reachable from {graph->end()}.
  void TrimGraph();

  // Trim nodes in the {graph} that are not reachable from either {graph->end()}
  // or any of the roots in the sequence [{begin},{end}[.
  template <typename ForwardIterator>
  void TrimGraph(ForwardIterator begin, ForwardIterator end) {
    while (begin != end) MarkAsLive(*begin++);
    TrimGraph();
  }

 private:
  V8_INLINE bool IsLive(Node* const node) { return is_live_.Get(node); }
  V8_INLINE void MarkAsLive(Node* const node) {
    if (!node->IsDead() && !IsLive(node)) {
      is_live_.Set(node, true);
      live_.push_back(node);
    }
  }

  Graph* graph() const { return graph_; }

  Graph* const graph_;
  NodeMarker<bool> is_live_;
  NodeVector live_;

  DISALLOW_COPY_AND_ASSIGN(GraphTrimmer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_TRIMMER_H_
