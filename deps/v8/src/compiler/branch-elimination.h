// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BRANCH_ELIMINATION_H_
#define V8_COMPILER_BRANCH_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/control-path-state.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class SourcePositionTable;

// Represents a condition along with its value in the current control path.
// Also stores the node that branched on this condition.
struct BranchCondition {
  BranchCondition() : node(nullptr), branch(nullptr), is_true(false) {}
  BranchCondition(Node* condition, Node* branch, bool is_true)
      : node(condition), branch(branch), is_true(is_true) {}
  Node* node;
  Node* branch;
  bool is_true;

  bool operator==(const BranchCondition& other) const {
    return node == other.node && branch == other.branch &&
           is_true == other.is_true;
  }
  bool operator!=(const BranchCondition& other) const {
    return !(*this == other);
  }

  bool IsSet() { return node != nullptr; }
};

class V8_EXPORT_PRIVATE BranchElimination final
    : public NON_EXPORTED_BASE(AdvancedReducerWithControlPathState)<
          BranchCondition, kUniqueInstance> {
 public:
  // TODO(nicohartmann@): Remove {Phase} once all Branch operators have
  // specified semantics.
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
  using ControlPathConditions =
      ControlPathState<BranchCondition, kUniqueInstance>;

  Reduction ReduceBranch(Node* node);
  Reduction ReduceDeoptimizeConditional(Node* node);
  Reduction ReduceIf(Node* node, bool is_true_branch);
  Reduction ReduceTrapConditional(Node* node);
  Reduction ReduceLoop(Node* node);
  Reduction ReduceMerge(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherControl(Node* node);
  void SimplifyBranchCondition(Node* branch);
  bool TryEliminateBranchWithPhiCondition(Node* branch, Node* phi, Node* merge);
  Reduction UpdateStatesHelper(Node* node,
                               ControlPathConditions prev_conditions,
                               Node* current_condition, Node* current_branch,
                               bool is_true_branch, bool in_new_block) {
    return UpdateStates(
        node, prev_conditions, current_condition,
        BranchCondition(current_condition, current_branch, is_true_branch),
        in_new_block);
  }

  Node* dead() const { return dead_; }
  TFGraph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;

  JSGraph* const jsgraph_;

  Node* dead_;
  Phase phase_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BRANCH_ELIMINATION_H_
