// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BRANCH_ELIMINATION_H_
#define V8_COMPILER_BRANCH_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/functional-list.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/node-aux-data.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;

class V8_EXPORT_PRIVATE BranchElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  BranchElimination(Editor* editor, JSGraph* js_graph, Zone* zone);
  ~BranchElimination() final;

  const char* reducer_name() const override { return "BranchElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  struct BranchCondition {
    Node* condition;
    Node* branch;
    bool is_true;

    bool operator==(BranchCondition other) const {
      return condition == other.condition && branch == other.branch &&
             is_true == other.is_true;
    }
    bool operator!=(BranchCondition other) const { return !(*this == other); }
  };

  // Class for tracking information about branch conditions.
  // At the moment it is a linked list of conditions and their values
  // (true or false).
  class ControlPathConditions : public FunctionalList<BranchCondition> {
   public:
    bool LookupCondition(Node* condition, Node** branch, bool* is_true) const;
    void AddCondition(Zone* zone, Node* condition, Node* branch, bool is_true,
                      ControlPathConditions hint);

   private:
    using FunctionalList<BranchCondition>::PushFront;
  };

  Reduction ReduceBranch(Node* node);
  Reduction ReduceDeoptimizeConditional(Node* node);
  Reduction ReduceIf(Node* node, bool is_true_branch);
  Reduction ReduceLoop(Node* node);
  Reduction ReduceMerge(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherControl(Node* node);

  Reduction TakeConditionsFromFirstControl(Node* node);
  Reduction UpdateConditions(Node* node, ControlPathConditions conditions);
  Reduction UpdateConditions(Node* node, ControlPathConditions prev_conditions,
                             Node* current_condition, Node* current_branch,
                             bool is_true_branch);

  Node* dead() const { return dead_; }
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;

  JSGraph* const jsgraph_;

  // Maps each control node to the condition information known about the node.
  // If the information is nullptr, then we have not calculated the information
  // yet.
  NodeAuxData<ControlPathConditions> node_conditions_;
  NodeAuxData<bool> reduced_;
  Zone* zone_;
  Node* dead_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BRANCH_ELIMINATION_H_
