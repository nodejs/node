// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_REDUCER_H_
#define V8_COMPILER_GRAPH_REDUCER_H_

#include <list>

#include "src/zone-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Graph;
class Node;


// Represents the result of trying to reduce a node in the graph.
class Reduction V8_FINAL {
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
  virtual ~Reducer() {}

  // Try to reduce a node if possible.
  virtual Reduction Reduce(Node* node) = 0;

  // Helper functions for subclasses to produce reductions for a node.
  static Reduction NoChange() { return Reduction(); }
  static Reduction Replace(Node* node) { return Reduction(node); }
  static Reduction Changed(Node* node) { return Reduction(node); }
};


// Performs an iterative reduction of a node graph.
class GraphReducer V8_FINAL {
 public:
  explicit GraphReducer(Graph* graph);

  Graph* graph() const { return graph_; }

  void AddReducer(Reducer* reducer) { reducers_.push_back(reducer); }

  // Reduce a single node.
  void ReduceNode(Node* node);
  // Reduce the whole graph.
  void ReduceGraph();

 private:
  typedef std::list<Reducer*, zone_allocator<Reducer*> > Reducers;

  Graph* graph_;
  Reducers reducers_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GRAPH_REDUCER_H_
