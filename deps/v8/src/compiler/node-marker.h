// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_MARKER_H_
#define V8_COMPILER_NODE_MARKER_H_

#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;


// Base class for templatized NodeMarkers.
class NodeMarkerBase {
 public:
  NodeMarkerBase(Graph* graph, uint32_t num_states);

  V8_INLINE Mark Get(Node* node) {
    Mark mark = node->mark();
    if (mark < mark_min_) {
      mark = mark_min_;
      node->set_mark(mark_min_);
    }
    DCHECK_LT(mark, mark_max_);
    return mark - mark_min_;
  }
  V8_INLINE void Set(Node* node, Mark mark) {
    DCHECK_LT(mark, mark_max_ - mark_min_);
    DCHECK_LT(node->mark(), mark_max_);
    node->set_mark(mark + mark_min_);
  }

 private:
  Mark const mark_min_;
  Mark const mark_max_;

  DISALLOW_COPY_AND_ASSIGN(NodeMarkerBase);
};


// A NodeMarker uses monotonically increasing marks to assign local "states"
// to nodes. Only one NodeMarker per graph is valid at a given time.
template <typename State>
class NodeMarker : public NodeMarkerBase {
 public:
  V8_INLINE NodeMarker(Graph* graph, uint32_t num_states)
      : NodeMarkerBase(graph, num_states) {}

  V8_INLINE State Get(Node* node) {
    return static_cast<State>(NodeMarkerBase::Get(node));
  }

  V8_INLINE void Set(Node* node, State state) {
    NodeMarkerBase::Set(node, static_cast<Mark>(state));
  }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_MARKER_H_
