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


// NodeIds are identifying numbers for nodes that can be used to index auxiliary
// out-of-line data associated with each node.
typedef uint32_t NodeId;


// Represents the result of trying to reduce a node in the graph.
class Reduction final {
 public:
  explicit Reduction(Node* replacement = nullptr) : replacement_(replacement) {}

  Node* replacement() const { return replacement_; }
  bool Changed() const { return replacement() != nullptr; }

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

  // Invoked by the {GraphReducer} when all nodes are done.  Can be used to
  // do additional reductions at the end, which in turn can cause a new round
  // of reductions.
  virtual void Finalize();

  // Helper functions for subclasses to produce reductions for a node.
  static Reduction NoChange() { return Reduction(); }
  static Reduction Replace(Node* node) { return Reduction(node); }
  static Reduction Changed(Node* node) { return Reduction(node); }
};


// An advanced reducer can also edit the graphs by changing and replacing nodes
// other than the one currently being reduced.
class AdvancedReducer : public Reducer {
 public:
  // Observe the actions of this reducer.
  class Editor {
   public:
    virtual ~Editor() {}

    // Replace {node} with {replacement}.
    virtual void Replace(Node* node, Node* replacement) = 0;
    // Revisit the {node} again later.
    virtual void Revisit(Node* node) = 0;
    // Replace value uses of {node} with {value} and effect uses of {node} with
    // {effect}. If {effect == nullptr}, then use the effect input to {node}.
    // All control uses will be relaxed assuming {node} cannot throw.
    virtual void ReplaceWithValue(Node* node, Node* value, Node* effect,
                                  Node* control) = 0;
  };

  explicit AdvancedReducer(Editor* editor) : editor_(editor) {}

 protected:
  // Helper functions for subclasses to produce reductions for a node.
  static Reduction Replace(Node* node) { return Reducer::Replace(node); }

  // Helper functions for subclasses to edit the graph.
  void Replace(Node* node, Node* replacement) {
    DCHECK_NOT_NULL(editor_);
    editor_->Replace(node, replacement);
  }
  void Revisit(Node* node) {
    DCHECK_NOT_NULL(editor_);
    editor_->Revisit(node);
  }
  void ReplaceWithValue(Node* node, Node* value, Node* effect = nullptr,
                        Node* control = nullptr) {
    DCHECK_NOT_NULL(editor_);
    editor_->ReplaceWithValue(node, value, effect, control);
  }

  // Relax the effects of {node} by immediately replacing effect and control
  // uses of {node} with the effect and control input to {node}.
  // TODO(turbofan): replace the effect input to {node} with {graph->start()}.
  void RelaxEffectsAndControls(Node* node) {
    ReplaceWithValue(node, node, nullptr, nullptr);
  }

  // Relax the control uses of {node} by immediately replacing them with the
  // control input to {node}.
  void RelaxControls(Node* node) {
    ReplaceWithValue(node, node, node, nullptr);
  }

 private:
  Editor* const editor_;
};


// Performs an iterative reduction of a node graph.
class GraphReducer : public AdvancedReducer::Editor {
 public:
  GraphReducer(Zone* zone, Graph* graph, Node* dead = nullptr);
  ~GraphReducer();

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

  // Replace {node} with {replacement}.
  void Replace(Node* node, Node* replacement) final;

  // Replace value uses of {node} with {value} and effect uses of {node} with
  // {effect}. If {effect == nullptr}, then use the effect input to {node}. All
  // control uses will be relaxed assuming {node} cannot throw.
  void ReplaceWithValue(Node* node, Node* value, Node* effect,
                        Node* control) final;

  // Replace all uses of {node} with {replacement} if the id of {replacement} is
  // less than or equal to {max_id}. Otherwise, replace all uses of {node} whose
  // id is less than or equal to {max_id} with the {replacement}.
  void Replace(Node* node, Node* replacement, NodeId max_id);

  // Node stack operations.
  void Pop();
  void Push(Node* node);

  // Revisit queue operations.
  bool Recurse(Node* node);
  void Revisit(Node* node) final;

  Graph* const graph_;
  Node* const dead_;
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
