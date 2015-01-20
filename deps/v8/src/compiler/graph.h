// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_H_
#define V8_COMPILER_GRAPH_H_

#include <map>
#include <set>

#include "src/compiler/node.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/source-position.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class GraphDecorator;


class Graph : public ZoneObject {
 public:
  explicit Graph(Zone* zone);

  // Base implementation used by all factory methods.
  Node* NewNode(const Operator* op, int input_count, Node** inputs,
                bool incomplete = false);

  // Factories for nodes with static input counts.
  Node* NewNode(const Operator* op) {
    return NewNode(op, 0, static_cast<Node**>(nullptr));
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

  template <class Visitor>
  inline void VisitNodeInputsFromEnd(Visitor* visitor);

  Zone* zone() const { return zone_; }
  Node* start() const { return start_; }
  Node* end() const { return end_; }

  void SetStart(Node* start) { start_ = start; }
  void SetEnd(Node* end) { end_ = end; }

  NodeId NextNodeID() { return next_node_id_++; }
  NodeId NodeCount() const { return next_node_id_; }

  void Decorate(Node* node);

  void AddDecorator(GraphDecorator* decorator) {
    decorators_.push_back(decorator);
  }

  void RemoveDecorator(GraphDecorator* decorator) {
    ZoneVector<GraphDecorator*>::iterator it =
        std::find(decorators_.begin(), decorators_.end(), decorator);
    DCHECK(it != decorators_.end());
    decorators_.erase(it, it + 1);
  }

 private:
  template <typename State>
  friend class NodeMarker;

  Zone* zone_;
  Node* start_;
  Node* end_;
  Mark mark_max_;
  NodeId next_node_id_;
  ZoneVector<GraphDecorator*> decorators_;

  DISALLOW_COPY_AND_ASSIGN(Graph);
};


// A NodeMarker uses monotonically increasing marks to assign local "states"
// to nodes. Only one NodeMarker per graph is valid at a given time.
template <typename State>
class NodeMarker BASE_EMBEDDED {
 public:
  NodeMarker(Graph* graph, uint32_t num_states)
      : mark_min_(graph->mark_max_), mark_max_(graph->mark_max_ += num_states) {
    DCHECK(num_states > 0);         // user error!
    DCHECK(mark_max_ > mark_min_);  // check for wraparound.
  }

  State Get(Node* node) {
    Mark mark = node->mark();
    if (mark < mark_min_) {
      mark = mark_min_;
      node->set_mark(mark_min_);
    }
    DCHECK_LT(mark, mark_max_);
    return static_cast<State>(mark - mark_min_);
  }

  void Set(Node* node, State state) {
    Mark local = static_cast<Mark>(state);
    DCHECK(local < (mark_max_ - mark_min_));
    DCHECK_LT(node->mark(), mark_max_);
    node->set_mark(local + mark_min_);
  }

 private:
  Mark mark_min_;
  Mark mark_max_;
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
