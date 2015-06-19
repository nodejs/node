// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_MARKER_H_
#define V8_COMPILER_NODE_MARKER_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;
class Node;


// Marks are used during traversal of the graph to distinguish states of nodes.
// Each node has a mark which is a monotonically increasing integer, and a
// {NodeMarker} has a range of values that indicate states of a node.
typedef uint32_t Mark;


// Base class for templatized NodeMarkers.
class NodeMarkerBase {
 public:
  NodeMarkerBase(Graph* graph, uint32_t num_states);

  Mark Get(Node* node);
  void Set(Node* node, Mark mark);
  void Reset(Graph* graph);

 private:
  Mark mark_min_;
  Mark mark_max_;

  DISALLOW_COPY_AND_ASSIGN(NodeMarkerBase);
};


// A NodeMarker uses monotonically increasing marks to assign local "states"
// to nodes. Only one NodeMarker per graph is valid at a given time.
template <typename State>
class NodeMarker : public NodeMarkerBase {
 public:
  NodeMarker(Graph* graph, uint32_t num_states)
      : NodeMarkerBase(graph, num_states) {}

  State Get(Node* node) {
    return static_cast<State>(NodeMarkerBase::Get(node));
  }

  void Set(Node* node, State state) {
    NodeMarkerBase::Set(node, static_cast<Mark>(state));
  }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_MARKER_H_
