// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_REDUCER_H_
#define V8_COMPILER_GRAPH_REDUCER_H_

#include "src/compiler/node-marker.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;
class Node;


// Represents the result of trying to reduce a node in the graph.
class Reduction FINAL {
 public:
  explicit Reduction(Node* replacement = NULL) : replacement_(replacement) {}

  Node* replacement() const { return replacement_; }
  bool Changed() const { return replacement() != NULL; }

 private:
  Node* replacement_;
};


// A reducer can reduce or simplify a given node based on its operator and
// inputs. This class functions as an extension point for the graph reducer for
// language-specific reductions (e.g. reduction based on types or constant
// folding of low-level operators) can be integrated into the graph reduction
// phase.
class Reducer {
 public:
  Reducer() {}
  virtual ~Reducer() {}

  // Try to reduce a node if possible.
  virtual Reduction Reduce(Node* node) = 0;

  // Helper functions for subclasses to produce reductions for a node.
  static Reduction NoChange() { return Reduction(); }
  static Reduction Replace(Node* node) { return Reduction(node); }
  static Reduction Changed(Node* node) { return Reduction(node); }

 private:
  DISALLOW_COPY_AND_ASSIGN(Reducer);
};


// Performs an iterative reduction of a node graph.
class GraphReducer FINAL {
 public:
  GraphReducer(Graph* graph, Zone* zone);

  Graph* graph() const { return graph_; }

  void AddReducer(Reducer* reducer);

  // Reduce a single node.
  void ReduceNode(Node* const);
  // Reduce the whole graph.
  void ReduceGraph();

 private:
  enum class State : uint8_t;
  struct NodeState {
    Node* node;
    int input_index;
  };

  // Reduce a single node.
  Reduction Reduce(Node* const);
  // Reduce the node on top of the stack.
  void ReduceTop();

  // Node stack operations.
  void Pop();
  void Push(Node* node);

  // Revisit queue operations.
  bool Recurse(Node* node);
  void Revisit(Node* node);

  Graph* graph_;
  NodeMarker<State> state_;
  ZoneVector<Reducer*> reducers_;
  ZoneStack<Node*> revisit_;
  ZoneStack<NodeState> stack_;

  DISALLOW_COPY_AND_ASSIGN(GraphReducer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_REDUCER_H_
