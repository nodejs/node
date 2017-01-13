// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_H_
#define V8_COMPILER_GRAPH_H_

#include "src/zone.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class GraphDecorator;
class Node;
class Operator;


// Marks are used during traversal of the graph to distinguish states of nodes.
// Each node has a mark which is a monotonically increasing integer, and a
// {NodeMarker} has a range of values that indicate states of a node.
typedef uint32_t Mark;


// NodeIds are identifying numbers for nodes that can be used to index auxiliary
// out-of-line data associated with each node.
typedef uint32_t NodeId;

class Graph final : public ZoneObject {
 public:
  explicit Graph(Zone* zone);

  // Scope used when creating a subgraph for inlining. Automatically preserves
  // the original start and end nodes of the graph, and resets them when you
  // leave the scope.
  class SubgraphScope final {
   public:
    explicit SubgraphScope(Graph* graph)
        : graph_(graph), start_(graph->start()), end_(graph->end()) {}
    ~SubgraphScope() {
      graph_->SetStart(start_);
      graph_->SetEnd(end_);
    }

   private:
    Graph* const graph_;
    Node* const start_;
    Node* const end_;

    DISALLOW_COPY_AND_ASSIGN(SubgraphScope);
  };

  // Base implementation used by all factory methods.
  Node* NewNodeUnchecked(const Operator* op, int input_count,
                         Node* const* inputs, bool incomplete = false);

  // Factory that checks the input count.
  Node* NewNode(const Operator* op, int input_count, Node* const* inputs,
                bool incomplete = false);

  // Factories for nodes with static input counts.
  Node* NewNode(const Operator* op) {
    return NewNode(op, 0, static_cast<Node* const*>(nullptr));
  }
  Node* NewNode(const Operator* op, Node* n1) { return NewNode(op, 1, &n1); }
  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* nodes[] = {n1, n2};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* nodes[] = {n1, n2, n3};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* nodes[] = {n1, n2, n3, n4};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5) {
    Node* nodes[] = {n1, n2, n3, n4, n5};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6, Node* n7) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6, n7};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6, Node* n7, Node* n8) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6, n7, n8};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6, Node* n7, Node* n8, Node* n9) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6, n7, n8, n9};
    return NewNode(op, arraysize(nodes), nodes);
  }

  // Clone the {node}, and assign a new node id to the copy.
  Node* CloneNode(const Node* node);

  Zone* zone() const { return zone_; }
  Node* start() const { return start_; }
  Node* end() const { return end_; }

  void SetStart(Node* start) { start_ = start; }
  void SetEnd(Node* end) { end_ = end; }

  size_t NodeCount() const { return next_node_id_; }

  void Decorate(Node* node);
  void AddDecorator(GraphDecorator* decorator);
  void RemoveDecorator(GraphDecorator* decorator);

 private:
  friend class NodeMarkerBase;

  inline NodeId NextNodeId();

  Zone* const zone_;
  Node* start_;
  Node* end_;
  Mark mark_max_;
  NodeId next_node_id_;
  ZoneVector<GraphDecorator*> decorators_;

  DISALLOW_COPY_AND_ASSIGN(Graph);
};


// A graph decorator can be used to add behavior to the creation of nodes
// in a graph.
class GraphDecorator : public ZoneObject {
 public:
  virtual ~GraphDecorator() {}
  virtual void Decorate(Node* node) = 0;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_H_
