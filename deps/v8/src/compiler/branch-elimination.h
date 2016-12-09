// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BRANCH_CONDITION_ELIMINATION_H_
#define V8_COMPILER_BRANCH_CONDITION_ELIMINATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;


class BranchElimination final : public AdvancedReducer {
 public:
  BranchElimination(Editor* editor, JSGraph* js_graph, Zone* zone);
  ~BranchElimination() final;

  Reduction Reduce(Node* node) final;

 private:
  struct BranchCondition {
    Node* condition;
    bool is_true;
    BranchCondition* next;

    BranchCondition(Node* condition, bool is_true, BranchCondition* next)
        : condition(condition), is_true(is_true), next(next) {}
  };

  // Class for tracking information about branch conditions.
  // At the moment it is a linked list of conditions and their values
  // (true or false).
  class ControlPathConditions {
   public:
    Maybe<bool> LookupCondition(Node* condition) const;

    const ControlPathConditions* AddCondition(Zone* zone, Node* condition,
                                              bool is_true) const;
    static const ControlPathConditions* Empty(Zone* zone);
    void Merge(const ControlPathConditions& other);

    bool operator==(const ControlPathConditions& other) const;
    bool operator!=(const ControlPathConditions& other) const {
      return !(*this == other);
    }

   private:
    ControlPathConditions(BranchCondition* head, size_t condition_count)
        : head_(head), condition_count_(condition_count) {}

    BranchCondition* head_;
    // We keep track of the list length so that we can find the longest
    // common tail easily.
    size_t condition_count_;
  };

  // Maps each control node to the condition information known about the node.
  // If the information is nullptr, then we have not calculated the information
  // yet.
  class PathConditionsForControlNodes {
   public:
    PathConditionsForControlNodes(Zone* zone, size_t size_hint)
        : info_for_node_(size_hint, nullptr, zone) {}
    const ControlPathConditions* Get(Node* node);
    void Set(Node* node, const ControlPathConditions* conditions);

   private:
    ZoneVector<const ControlPathConditions*> info_for_node_;
  };

  Reduction ReduceBranch(Node* node);
  Reduction ReduceDeoptimizeConditional(Node* node);
  Reduction ReduceIf(Node* node, bool is_true_branch);
  Reduction ReduceLoop(Node* node);
  Reduction ReduceMerge(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherControl(Node* node);

  Reduction TakeConditionsFromFirstControl(Node* node);
  Reduction UpdateConditions(Node* node,
                             const ControlPathConditions* conditions);

  Node* dead() const { return dead_; }
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;

  JSGraph* const jsgraph_;
  PathConditionsForControlNodes node_conditions_;
  Zone* zone_;
  Node* dead_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BRANCH_CONDITION_ELIMINATION_H_
