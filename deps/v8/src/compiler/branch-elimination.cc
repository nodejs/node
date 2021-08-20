// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"

#include "src/base/small-vector.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

BranchElimination::BranchElimination(Editor* editor, JSGraph* js_graph,
                                     Zone* zone, Phase phase)
    : AdvancedReducer(editor),
      jsgraph_(js_graph),
      node_conditions_(js_graph->graph()->NodeCount(), zone),
      reduced_(js_graph->graph()->NodeCount(), zone),
      zone_(zone),
      dead_(js_graph->Dead()),
      phase_(phase) {}

BranchElimination::~BranchElimination() = default;

Reduction BranchElimination::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kDead:
      return NoChange();
    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless:
      return ReduceDeoptimizeConditional(node);
    case IrOpcode::kMerge:
      return ReduceMerge(node);
    case IrOpcode::kLoop:
      return ReduceLoop(node);
    case IrOpcode::kBranch:
      return ReduceBranch(node);
    case IrOpcode::kIfFalse:
      return ReduceIf(node, false);
    case IrOpcode::kIfTrue:
      return ReduceIf(node, true);
    case IrOpcode::kTrapIf:
    case IrOpcode::kTrapUnless:
      return ReduceTrapConditional(node);
    case IrOpcode::kStart:
      return ReduceStart(node);
    default:
      if (node->op()->ControlOutputCount() > 0) {
        return ReduceOtherControl(node);
      }
      break;
  }
  return NoChange();
}

void BranchElimination::SimplifyBranchCondition(Node* branch) {
  // Try to use a phi as a branch condition if the control flow from the branch
  // is known from previous branches. For example, in the graph below, the
  // control flow of the second_branch is predictable because the first_branch
  // use the same branch condition. In such case, create a new phi with constant
  // inputs and let the second branch use the phi as its branch condition. From
  // this transformation, more branch folding opportunities would be exposed to
  // later passes through branch cloning in effect-control-linearizer.
  //
  // condition                             condition
  //    |   \                                   |
  //    |  first_branch                        first_branch
  //    |   /          \                       /          \
  //    |  /            \                     /            \
  //    |first_true  first_false           first_true  first_false
  //    |  \           /                      \           /
  //    |   \         /                        \         /
  //    |  first_merge           ==>          first_merge
  //    |       |                              /    |
  //   second_branch                    1  0  /     |
  //    /          \                     \ | /      |
  //   /            \                     phi       |
  // second_true  second_false              \       |
  //                                      second_branch
  //                                      /          \
  //                                     /            \
  //                                   second_true  second_false
  //

  DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
  Node* merge = NodeProperties::GetControlInput(branch);
  if (merge->opcode() != IrOpcode::kMerge) return;

  Node* branch_condition = branch->InputAt(0);
  Node* previous_branch;
  bool condition_value;
  Graph* graph = jsgraph()->graph();
  base::SmallVector<Node*, 2> phi_inputs;

  Node::Inputs inputs = merge->inputs();
  int input_count = inputs.count();
  for (int i = 0; i != input_count; ++i) {
    Node* input = inputs[i];
    ControlPathConditions from_input = node_conditions_.Get(input);
    if (!from_input.LookupCondition(branch_condition, &previous_branch,
                                    &condition_value))
      return;

    if (phase_ == kEARLY) {
      phi_inputs.emplace_back(condition_value ? jsgraph()->TrueConstant()
                                              : jsgraph()->FalseConstant());
    } else {
      phi_inputs.emplace_back(
          condition_value
              ? graph->NewNode(jsgraph()->common()->Int32Constant(1))
              : graph->NewNode(jsgraph()->common()->Int32Constant(0)));
    }
  }
  phi_inputs.emplace_back(merge);
  Node* new_phi = graph->NewNode(
      common()->Phi(phase_ == kEARLY ? MachineRepresentation::kTagged
                                     : MachineRepresentation::kWord32,
                    input_count),
      input_count + 1, &phi_inputs.at(0));

  // Replace the branch condition with the new phi.
  NodeProperties::ReplaceValueInput(branch, new_phi, 0);
}

Reduction BranchElimination::ReduceBranch(Node* node) {
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  ControlPathConditions from_input = node_conditions_.Get(control_input);
  Node* branch;
  bool condition_value;
  // If we know the condition we can discard the branch.
  if (from_input.LookupCondition(condition, &branch, &condition_value)) {
    MarkAsSafetyCheckIfNeeded(branch, node);
    for (Node* const use : node->uses()) {
      switch (use->opcode()) {
        case IrOpcode::kIfTrue:
          Replace(use, condition_value ? control_input : dead());
          break;
        case IrOpcode::kIfFalse:
          Replace(use, condition_value ? dead() : control_input);
          break;
        default:
          UNREACHABLE();
      }
    }
    return Replace(dead());
  }
  SimplifyBranchCondition(node);
  // Trigger revisits of the IfTrue/IfFalse projections, since they depend on
  // the branch condition.
  for (Node* const use : node->uses()) {
    Revisit(use);
  }
  return TakeConditionsFromFirstControl(node);
}

Reduction BranchElimination::ReduceTrapConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kTrapIf ||
         node->opcode() == IrOpcode::kTrapUnless);
  bool trapping_condition = node->opcode() == IrOpcode::kTrapIf;
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!reduced_.Get(control_input)) {
    return NoChange();
  }
  ControlPathConditions from_input = node_conditions_.Get(control_input);

  Node* branch;
  bool condition_value;

  if (from_input.LookupCondition(condition, &branch, &condition_value)) {
    if (condition_value == trapping_condition) {
      // This will always trap. Mark its outputs as dead and connect it to
      // graph()->end().
      ReplaceWithValue(node, dead(), dead(), dead());
      Node* effect = NodeProperties::GetEffectInput(node);
      Node* control = graph()->NewNode(common()->Throw(), effect, node);
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
      return Changed(node);
    } else {
      // This will not trap, remove it.
      return Replace(control_input);
    }
  }
  return UpdateConditions(node, from_input, condition, node,
                          !trapping_condition);
}

Reduction BranchElimination::ReduceDeoptimizeConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kDeoptimizeIf ||
         node->opcode() == IrOpcode::kDeoptimizeUnless);
  bool condition_is_true = node->opcode() == IrOpcode::kDeoptimizeUnless;
  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  Node* condition = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!reduced_.Get(control)) {
    return NoChange();
  }

  ControlPathConditions conditions = node_conditions_.Get(control);
  bool condition_value;
  Node* branch;
  // If we know the condition we can discard the branch.
  if (conditions.LookupCondition(condition, &branch, &condition_value)) {
    MarkAsSafetyCheckIfNeeded(branch, node);
    if (condition_is_true == condition_value) {
      // We don't update the conditions here, because we're replacing {node}
      // with the {control} node that already contains the right information.
      ReplaceWithValue(node, dead(), effect, control);
    } else {
      control = graph()->NewNode(
          common()->Deoptimize(p.kind(), p.reason(), p.feedback()), frame_state,
          effect, control);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
    }
    return Replace(dead());
  }
  return UpdateConditions(node, conditions, condition, node, condition_is_true);
}

Reduction BranchElimination::ReduceIf(Node* node, bool is_true_branch) {
  // Add the condition to the list arriving from the input branch.
  Node* branch = NodeProperties::GetControlInput(node, 0);
  ControlPathConditions from_branch = node_conditions_.Get(branch);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!reduced_.Get(branch)) {
    return NoChange();
  }
  Node* condition = branch->InputAt(0);
  return UpdateConditions(node, from_branch, condition, branch, is_true_branch);
}

Reduction BranchElimination::ReduceLoop(Node* node) {
  // Here we rely on having only reducible loops:
  // The loop entry edge always dominates the header, so we can just use
  // the information from the loop entry edge.
  return TakeConditionsFromFirstControl(node);
}

Reduction BranchElimination::ReduceMerge(Node* node) {
  // Shortcut for the case when we do not know anything about some
  // input.
  Node::Inputs inputs = node->inputs();
  for (Node* input : inputs) {
    if (!reduced_.Get(input)) {
      return NoChange();
    }
  }

  auto input_it = inputs.begin();

  DCHECK_GT(inputs.count(), 0);

  ControlPathConditions conditions = node_conditions_.Get(*input_it);
  ++input_it;
  // Merge the first input's conditions with the conditions from the other
  // inputs.
  auto input_end = inputs.end();
  for (; input_it != input_end; ++input_it) {
    // Change the current condition list to a longest common tail
    // of this condition list and the other list. (The common tail
    // should correspond to the list from the common dominator.)
    conditions.ResetToCommonAncestor(node_conditions_.Get(*input_it));
  }
  return UpdateConditions(node, conditions);
}

Reduction BranchElimination::ReduceStart(Node* node) {
  return UpdateConditions(node, {});
}

Reduction BranchElimination::ReduceOtherControl(Node* node) {
  DCHECK_EQ(1, node->op()->ControlInputCount());
  return TakeConditionsFromFirstControl(node);
}

Reduction BranchElimination::TakeConditionsFromFirstControl(Node* node) {
  // We just propagate the information from the control input (ideally,
  // we would only revisit control uses if there is change).
  Node* input = NodeProperties::GetControlInput(node, 0);
  if (!reduced_.Get(input)) return NoChange();
  return UpdateConditions(node, node_conditions_.Get(input));
}

Reduction BranchElimination::UpdateConditions(
    Node* node, ControlPathConditions conditions) {
  // Only signal that the node has Changed if the condition information has
  // changed.
  if (reduced_.Set(node, true) | node_conditions_.Set(node, conditions)) {
    return Changed(node);
  }
  return NoChange();
}

Reduction BranchElimination::UpdateConditions(
    Node* node, ControlPathConditions prev_conditions, Node* current_condition,
    Node* current_branch, bool is_true_branch) {
  ControlPathConditions original = node_conditions_.Get(node);
  // The control path for the node is the path obtained by appending the
  // current_condition to the prev_conditions. Use the original control path as
  // a hint to avoid allocations.
  prev_conditions.AddCondition(zone_, current_condition, current_branch,
                               is_true_branch, original);
  return UpdateConditions(node, prev_conditions);
}

void BranchElimination::ControlPathConditions::AddCondition(
    Zone* zone, Node* condition, Node* branch, bool is_true,
    ControlPathConditions hint) {
  if (!LookupCondition(condition)) {
    PushFront({condition, branch, is_true}, zone, hint);
  }
}

bool BranchElimination::ControlPathConditions::LookupCondition(
    Node* condition) const {
  for (BranchCondition element : *this) {
    if (element.condition == condition) return true;
  }
  return false;
}

bool BranchElimination::ControlPathConditions::LookupCondition(
    Node* condition, Node** branch, bool* is_true) const {
  for (BranchCondition element : *this) {
    if (element.condition == condition) {
      *is_true = element.is_true;
      *branch = element.branch;
      return true;
    }
  }
  return false;
}

void BranchElimination::MarkAsSafetyCheckIfNeeded(Node* branch, Node* node) {
  // Check if {branch} is dead because we might have a stale side-table entry.
  if (!branch->IsDead() && branch->opcode() != IrOpcode::kDead &&
      branch->opcode() != IrOpcode::kTrapIf &&
      branch->opcode() != IrOpcode::kTrapUnless) {
    IsSafetyCheck branch_safety = IsSafetyCheckOf(branch->op());
    IsSafetyCheck combined_safety =
        CombineSafetyChecks(branch_safety, IsSafetyCheckOf(node->op()));
    if (branch_safety != combined_safety) {
      NodeProperties::ChangeOp(
          branch, common()->MarkAsSafetyCheck(branch->op(), combined_safety));
    }
  }
}

Graph* BranchElimination::graph() const { return jsgraph()->graph(); }

Isolate* BranchElimination::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* BranchElimination::common() const {
  return jsgraph()->common();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
