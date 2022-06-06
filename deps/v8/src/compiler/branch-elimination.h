// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BRANCH_ELIMINATION_H_
#define V8_COMPILER_BRANCH_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/functional-list.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/persistent-map.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;

class V8_EXPORT_PRIVATE BranchElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  enum Phase {
    kEARLY,
    kLATE,
  };
  BranchElimination(Editor* editor, JSGraph* js_graph, Zone* zone,
                    Phase phase = kLATE);
  ~BranchElimination() final;

  const char* reducer_name() const override { return "BranchElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  // Represents a condition along with its value in the current control path.
  // Also stores the node that branched on this condition.
  struct BranchCondition {
    BranchCondition() : condition(nullptr), branch(nullptr), is_true(false) {}
    BranchCondition(Node* condition, Node* branch, bool is_true)
        : condition(condition), branch(branch), is_true(is_true) {}
    Node* condition;
    Node* branch;
    bool is_true;

    bool operator==(BranchCondition other) const {
      return condition == other.condition && branch == other.branch &&
             is_true == other.is_true;
    }
    bool operator!=(BranchCondition other) const { return !(*this == other); }

    bool IsSet() const { return branch != nullptr; }
  };

  // Class for tracking information about branch conditions. It is represented
  // as a linked list of condition blocks, each of which corresponds to a block
  // of code bewteen an IfTrue/IfFalse and a Merge. Each block is in turn
  // represented as a linked list of {BranchCondition}s.
  class ControlPathConditions {
   public:
    explicit ControlPathConditions(Zone* zone) : conditions_(zone) {}
    // Checks if {condition} is present in this {ControlPathConditions}.
    bool LookupCondition(Node* condition) const;
    // Checks if {condition} is present in this {ControlPathConditions} and
    // copies its {branch} and {is_true} fields.
    bool LookupCondition(Node* condition, Node** branch, bool* is_true) const;
    // Adds a condition in the current code block, or a new block if the block
    // list is empty.
    void AddCondition(Zone* zone, Node* condition, Node* branch, bool is_true,
                      ControlPathConditions hint);
    // Adds a condition in a new block.
    void AddConditionInNewBlock(Zone* zone, Node* condition, Node* branch,
                                bool is_true);
    // Reset this {ControlPathConditions} to the longest prefix that is common
    // with {other}.
    void ResetToCommonAncestor(ControlPathConditions other);

    bool operator==(const ControlPathConditions& other) const {
      return blocks_ == other.blocks_;
    }
    bool operator!=(const ControlPathConditions& other) const {
      return blocks_ != other.blocks_;
    }

    friend class BranchElimination;

   private:
    FunctionalList<FunctionalList<BranchCondition>> blocks_;
    // This is an auxilliary data structure that provides fast lookups in the
    // set of conditions. It should hold at any point that the contents of
    // {blocks_} and {conditions_} is the same, which is implemented in
    // {BlocksAndConditionsInvariant}.
    PersistentMap<Node*, BranchCondition> conditions_;
#if DEBUG
    bool BlocksAndConditionsInvariant();
#endif
  };

  Reduction ReduceBranch(Node* node);
  Reduction ReduceDeoptimizeConditional(Node* node);
  Reduction ReduceIf(Node* node, bool is_true_branch);
  Reduction ReduceTrapConditional(Node* node);
  Reduction ReduceLoop(Node* node);
  Reduction ReduceMerge(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherControl(Node* node);
  void SimplifyBranchCondition(Node* branch);

  Reduction TakeConditionsFromFirstControl(Node* node);
  Reduction UpdateConditions(Node* node, ControlPathConditions conditions);
  Reduction UpdateConditions(Node* node, ControlPathConditions prev_conditions,
                             Node* current_condition, Node* current_branch,
                             bool is_true_branch, bool in_new_block);

  Node* dead() const { return dead_; }
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;

  JSGraph* const jsgraph_;

  // Maps each control node to the condition information known about the node.
  // If the information is nullptr, then we have not calculated the information
  // yet.

  NodeAuxData<ControlPathConditions, ZoneConstruct<ControlPathConditions>>
      node_conditions_;
  NodeAuxData<bool> reduced_;
  Zone* zone_;
  Node* dead_;
  Phase phase_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BRANCH_ELIMINATION_H_
