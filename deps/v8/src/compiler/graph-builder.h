// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_BUILDER_H_
#define V8_COMPILER_GRAPH_BUILDER_H_

#include "src/v8.h"

#include "src/allocation.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/unique.h"

namespace v8 {
namespace internal {

class BitVector;

namespace compiler {

class Node;

// A common base class for anything that creates nodes in a graph.
class GraphBuilder {
 public:
  explicit GraphBuilder(Graph* graph) : graph_(graph) {}
  virtual ~GraphBuilder() {}

  Node* NewNode(const Operator* op, bool incomplete = false) {
    return MakeNode(op, 0, static_cast<Node**>(NULL), incomplete);
  }

  Node* NewNode(const Operator* op, Node* n1) {
    return MakeNode(op, 1, &n1, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* buffer[] = {n1, n2};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* buffer[] = {n1, n2, n3};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* buffer[] = {n1, n2, n3, n4};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5) {
    Node* buffer[] = {n1, n2, n3, n4, n5};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6};
    return MakeNode(op, arraysize(nodes), nodes, false);
  }

  Node* NewNode(const Operator* op, int value_input_count, Node** value_inputs,
                bool incomplete = false) {
    return MakeNode(op, value_input_count, value_inputs, incomplete);
  }

  Graph* graph() const { return graph_; }

 protected:
  // Base implementation used by all factory methods.
  virtual Node* MakeNode(const Operator* op, int value_input_count,
                         Node** value_inputs, bool incomplete) = 0;

 private:
  Graph* graph_;
};


// The StructuredGraphBuilder produces a high-level IR graph. It is used as the
// base class for concrete implementations (e.g the AstGraphBuilder or the
// StubGraphBuilder).
class StructuredGraphBuilder : public GraphBuilder {
 public:
  StructuredGraphBuilder(Zone* zone, Graph* graph,
                         CommonOperatorBuilder* common);
  virtual ~StructuredGraphBuilder() {}

  // Creates a new Phi node having {count} input values.
  Node* NewPhi(int count, Node* input, Node* control);
  Node* NewEffectPhi(int count, Node* input, Node* control);

  // Helpers for merging control, effect or value dependencies.
  Node* MergeControl(Node* control, Node* other);
  Node* MergeEffect(Node* value, Node* other, Node* control);
  Node* MergeValue(Node* value, Node* other, Node* control);

  // Helpers to create new control nodes.
  Node* NewIfTrue() { return NewNode(common()->IfTrue()); }
  Node* NewIfFalse() { return NewNode(common()->IfFalse()); }
  Node* NewMerge() { return NewNode(common()->Merge(1), true); }
  Node* NewLoop() { return NewNode(common()->Loop(1), true); }
  Node* NewBranch(Node* condition, BranchHint hint = BranchHint::kNone) {
    return NewNode(common()->Branch(hint), condition);
  }

 protected:
  class Environment;
  friend class Environment;
  friend class ControlBuilder;

  // The following method creates a new node having the specified operator and
  // ensures effect and control dependencies are wired up. The dependencies
  // tracked by the environment might be mutated.
  virtual Node* MakeNode(const Operator* op, int value_input_count,
                         Node** value_inputs, bool incomplete) FINAL;

  Environment* environment() const { return environment_; }
  void set_environment(Environment* env) { environment_ = env; }

  Node* current_context() const { return current_context_; }
  void set_current_context(Node* context) { current_context_ = context; }

  Node* exit_control() const { return exit_control_; }
  void set_exit_control(Node* node) { exit_control_ = node; }

  Node* dead_control();

  Zone* graph_zone() const { return graph()->zone(); }
  Zone* local_zone() const { return local_zone_; }
  Isolate* isolate() const { return graph_zone()->isolate(); }
  CommonOperatorBuilder* common() const { return common_; }

  // Helper to wrap a Handle<T> into a Unique<T>.
  template <class T>
  Unique<T> MakeUnique(Handle<T> object) {
    return Unique<T>::CreateUninitialized(object);
  }

  // Support for control flow builders. The concrete type of the environment
  // depends on the graph builder, but environments themselves are not virtual.
  virtual Environment* CopyEnvironment(Environment* env);

  // Helper to indicate a node exits the function body.
  void UpdateControlDependencyToLeaveFunction(Node* exit);

 private:
  CommonOperatorBuilder* common_;
  Environment* environment_;

  // Zone local to the builder for data not leaking into the graph.
  Zone* local_zone_;

  // Temporary storage for building node input lists.
  int input_buffer_size_;
  Node** input_buffer_;

  // Node representing the control dependency for dead code.
  SetOncePointer<Node> dead_control_;

  // Node representing the current context within the function body.
  Node* current_context_;

  // Merge of all control nodes that exit the function body.
  Node* exit_control_;

  // Growth increment for the temporary buffer used to construct input lists to
  // new nodes.
  static const int kInputBufferSizeIncrement = 64;

  Node** EnsureInputBufferSize(int size);

  DISALLOW_COPY_AND_ASSIGN(StructuredGraphBuilder);
};


// The abstract execution environment contains static knowledge about
// execution state at arbitrary control-flow points. It allows for
// simulation of the control-flow at compile time.
class StructuredGraphBuilder::Environment : public ZoneObject {
 public:
  Environment(StructuredGraphBuilder* builder, Node* control_dependency);
  Environment(const Environment& copy);

  // Control dependency tracked by this environment.
  Node* GetControlDependency() { return control_dependency_; }
  void UpdateControlDependency(Node* dependency) {
    control_dependency_ = dependency;
  }

  // Effect dependency tracked by this environment.
  Node* GetEffectDependency() { return effect_dependency_; }
  void UpdateEffectDependency(Node* dependency) {
    effect_dependency_ = dependency;
  }

  // Mark this environment as being unreachable.
  void MarkAsUnreachable() {
    UpdateControlDependency(builder()->dead_control());
  }
  bool IsMarkedAsUnreachable() {
    return GetControlDependency()->opcode() == IrOpcode::kDead;
  }

  // Merge another environment into this one.
  void Merge(Environment* other);

  // Copies this environment at a control-flow split point.
  Environment* CopyForConditional() { return builder()->CopyEnvironment(this); }

  // Copies this environment to a potentially unreachable control-flow point.
  Environment* CopyAsUnreachable() {
    Environment* env = builder()->CopyEnvironment(this);
    env->MarkAsUnreachable();
    return env;
  }

  // Copies this environment at a loop header control-flow point.
  Environment* CopyForLoop(BitVector* assigned) {
    PrepareForLoop(assigned);
    return builder()->CopyEnvironment(this);
  }

  Node* GetContext() { return builder_->current_context(); }

 protected:
  Zone* zone() const { return builder_->local_zone(); }
  Graph* graph() const { return builder_->graph(); }
  StructuredGraphBuilder* builder() const { return builder_; }
  CommonOperatorBuilder* common() { return builder_->common(); }
  NodeVector* values() { return &values_; }

  // Prepare environment to be used as loop header.
  void PrepareForLoop(BitVector* assigned);

 private:
  StructuredGraphBuilder* builder_;
  Node* control_dependency_;
  Node* effect_dependency_;
  NodeVector values_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GRAPH_BUILDER_H__
