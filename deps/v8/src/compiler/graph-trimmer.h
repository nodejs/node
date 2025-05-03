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
class TFGraph;

// Trims dead nodes from the node graph.
class V8_EXPORT_PRIVATE GraphTrimmer final {
 public:
  GraphTrimmer(Zone* zone, TFGraph* graph);
  ~GraphTrimmer();
  GraphTrimmer(const GraphTrimmer&) = delete;
  GraphTrimmer& operator=(const GraphTrimmer&) = delete;

  // Trim nodes in the {graph} that are not reachable from {graph->end()}.
  void TrimGraph();

  // Trim nodes in the {graph} that are not reachable from either {graph->end()}
  // or any of the roots in the sequence [{begin},{end}[.
  template <typename ForwardIterator>
  void TrimGraph(ForwardIterator begin, ForwardIterator end) {
    while (begin != end) {
      Node* const node = *begin++;
      if (!node->IsDead()) MarkAsLive(node);
    }
    TrimGraph();
  }

 private:
  V8_INLINE bool IsLive(Node* const node) { return is_live_.Get(node); }
  V8_INLINE void MarkAsLive(Node* const node) {
    DCHECK(!node->IsDead());
    if (!IsLive(node)) {
      is_live_.Set(node, true);
      live_.push_back(node);
    }
  }

  TFGraph* graph() const { return graph_; }

  TFGraph* const graph_;
  NodeMarker<bool> is_live_;
  NodeVector live_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_TRIMMER_H_
