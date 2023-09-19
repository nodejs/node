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
  NodeMarkerBase(const NodeMarkerBase&) = delete;
  NodeMarkerBase& operator=(const NodeMarkerBase&) = delete;

  V8_INLINE Mark Get(const Node* node) {
    Mark mark = node->mark();
    if (mark < mark_min_) {
      return 0;
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
};

// A NodeMarker assigns a local "state" to every node of a graph in constant
// memory. Only one NodeMarker per graph is valid at a given time, that is,
// after you create a NodeMarker you should no longer use NodeMarkers that
// were created earlier. Internally, the local state is stored in the Node
// structure.
//
// When you initialize a NodeMarker, all the local states are conceptually
// set to State(0) in constant time.
//
// In its current implementation, in debug mode NodeMarker will try to
// (efficiently) detect invalid use of an older NodeMarker. Namely, if you set a
// node with a NodeMarker, and then get or set that node with an older
// NodeMarker you will get a crash.
//
// GraphReducer uses a NodeMarker, so individual Reducers cannot use a
// NodeMarker.
template <typename State>
class NodeMarker : public NodeMarkerBase {
 public:
  V8_INLINE NodeMarker(Graph* graph, uint32_t num_states)
      : NodeMarkerBase(graph, num_states) {}

  V8_INLINE State Get(const Node* node) {
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
