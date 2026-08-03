// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"

#include "src/base/small-vector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"

namespace v8 {
namespace internal {
namespace compiler {

BranchElimination::BranchElimination(Editor* editor, JSGraph* js_graph,
                                     Zone* zone, Phase phase)
    : AdvancedReducerWithControlPathState(editor, zone, js_graph->graph()),
      jsgraph_(js_graph),
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
      } else {
        return NoChange();
      }
  }
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

  auto SemanticsOf = [phase = this->phase_](Node* branch) {
    BranchSemantics semantics = BranchSemantics::kUnspecified;
    if (branch->opcode() == IrOpcode::kBranch) {
      semantics = BranchParametersOf(branch->op()).semantics();
    }
    if (semantics == BranchSemantics::kUnspecified) {
      semantics =
          (phase == kEARLY ? BranchSemantics::kJS : BranchSemantics::kMachine);
    }
    return semantics;
  };

  DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
  Node* merge = NodeProperties::GetControlInput(branch);
  if (merge->opcode() != IrOpcode::kMerge) return;

  Node* condition = branch->InputAt(0);
  BranchSemantics semantics = SemanticsOf(branch);
  TFGraph* graph = jsgraph()->graph();
  base::SmallVector<Node*, 2> phi_inputs;

  Node::Inputs inputs = merge->inputs();
  int input_count = inputs.count();
  for (int i = 0; i != input_count; ++i) {
    Node* input = inputs[i];
    ControlPathConditions from_input = GetState(input);

    BranchCondition branch_condition = from_input.LookupState(condition);
    if (!branch_condition.IsSet()) return;
    if (SemanticsOf(branch_condition.branch) != semantics) return;
    bool condition_value = branch_condition.is_true;

    if (semantics == BranchSemantics::kJS) {
      phi_inputs.emplace_back(jsgraph()->BooleanConstant(condition_value));
    } else {
      DCHECK_EQ(semantics, BranchSemantics::kMachine);
      phi_inputs.emplace_back(
          condition_value
              ? graph->NewNode(jsgraph()->common()->Int32Constant(1))
              : graph->NewNode(jsgraph()->common()->Int32Constant(0)));
    }
  }
  phi_inputs.emplace_back(merge);
  Node* new_phi =
      graph->NewNode(common()->Phi(semantics == BranchSemantics::kJS
                                       ? MachineRepresentation::kTagged
                                       : MachineRepresentation::kWord32,
                                   input_count),
                     input_count + 1, &phi_inputs.at(0));

  // Replace the branch condition with the new phi.
  NodeProperties::ReplaceValueInput(branch, new_phi, 0);
}

bool BranchElimination::TryEliminateBranchWithPhiCondition(Node* branch,
                                                           Node* phi,
                                                           Node* merge) {
  // If the condition of the branch comes from two constant values,
  // then try to merge the branches successors into its predecessors,
  // and eliminate the (branch, phi, merge) nodes.
  //
  //  pred0   pred1
  //     \    /
  //      merge             0   1
  //       |  \___________  |  /
  //       |              \ | /              pred0     pred1
  //       |               phi                 |         |
  //       |   _____________/        =>        |         |
  //       |  /                                |         |
  //      branch                             succ0     succ1
  //      /    \
  //   false   true
  //     |      |
  //   succ0  succ1
  //

  DCHECK_EQ(branch->opcode(), IrOpcode::kBranch);
  DCHECK_EQ(phi->opcode(), IrOpcode::kPhi);
  DCHECK_EQ(merge->opcode(), IrOpcode::kMerge);
  DCHECK_EQ(NodeProperties::GetControlInput(branch, 0), merge);
  if (!phi->OwnedBy(branch)) return false;
  if (phi->InputCount() != 3) return false;
  if (phi->InputAt(2) != merge) return false;
  if (merge->UseCount() != 2) return false;

  Node::Inputs phi_inputs = phi->inputs();
  Node* first_value = phi_inputs[0];
  Node* second_value = phi_inputs[1];
  if (first_value->opcode() != IrOpcode::kInt32Constant ||
      second_value->opcode() != IrOpcode::kInt32Constant) {
    return false;
  }
  Node::Inputs merge_inputs = merge->inputs();
  Node* predecessor0 = merge_inputs[0];
  Node* predecessor1 = merge_inputs[1];
  DCHECK_EQ(branch->op()->ControlOutputCount(), 2);
  Node** projections = zone()->AllocateArray<Node*>(2);
  NodeProperties::CollectControlProjections(branch, projections, 2);
  Node* branch_true = projections[0];
  Node* branch_false = projections[1];
  DCHECK_EQ(branch_true->opcode(), IrOpcode::kIfTrue);
  DCHECK_EQ(branch_false->opcode(), IrOpcode::kIfFalse);

  // The input values of phi should be true(1) and false(0).
  Int32Matcher mfirst_value(first_value);
  Int32Matcher msecond_value(second_value);
  Node* predecessor_true = nullptr;
  Node* predecessor_false = nullptr;
  if (mfirst_value.Is(1) && msecond_value.Is(0)) {
    predecessor_true = predecessor0;
    predecessor_false = predecessor1;
  } else if (mfirst_value.Is(0) && msecond_value.Is(1)) {
    predecessor_true = predecessor1;
    predecessor_false = predecessor0;
  } else {
    return false;
  }

  // Merge the branches successors into its predecessors.
  for (Edge edge : branch_true->use_edges()) {
    edge.UpdateTo(predecessor_true);
  }
  for (Edge edge : branch_false->use_edges()) {
    edge.UpdateTo(predecessor_false);
  }

  branch_true->Kill();
  branch_false->Kill();
  branch->Kill();
  phi->Kill();
  merge->Kill();
  return true;
}

Reduction BranchElimination::ReduceBranch(Node* node) {
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  if (!IsReduced(control_input)) return NoChange();
  ControlPathConditions from_input = GetState(control_input);
  // If we know the condition we can discard the branch.
  BranchCondition branch_condition = from_input.LookupState(condition);
  if (branch_condition.IsSet()) {
    bool condition_value = branch_condition.is_true;
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
  // Try to reduce the pattern that branch condition comes from phi node.
  if (condition->opcode() == IrOpcode::kPhi &&
      control_input->opcode() == IrOpcode::kMerge) {
    if (TryEliminateBranchWithPhiCondition(node, condition, control_input)) {
      return Replace(dead());
    }
  }
  // Trigger revisits of the IfTrue/IfFalse projections, since they depend on
  // the branch condition.
  for (Node* const use : node->uses()) {
    Revisit(use);
  }
  return TakeStatesFromFirstControl(node);
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
  if (!IsReduced(control_input)) return NoChange();

  ControlPathConditions from_input = GetState(control_input);

  BranchCondition branch_condition = from_input.LookupState(condition);
  if (branch_condition.IsSet()) {
    bool condition_value = branch_condition.is_true;
    if (condition_value == trapping_condition) {
      // This will always trap. Mark its outputs as dead and connect it to
      // graph()->end().
      ReplaceWithValue(node, dead(), dead(), dead());
      Node* control = graph()->NewNode(common()->Throw(), node, node);
      MergeControlToEnd(graph(), common(), control);
      return Changed(node);
    } else {
      // This will not trap, remove it by relaxing effect/control.
      RelaxEffectsAndControls(node);
      Node* control = NodeProperties::GetControlInput(node);
      node->Kill();
      return Replace(control);  // Irrelevant argument
    }
  }
  return UpdateStatesHelper(node, from_input, condition, node,
                            !trapping_condition, false);
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
  if (!IsReduced(control)) {
    return NoChange();
  }

  ControlPathConditions conditions = GetState(control);
  BranchCondition branch_condition = conditions.LookupState(condition);
  if (branch_condition.IsSet()) {
    // If we know the condition we can discard the branch.
    bool condition_value = branch_condition.is_true;
    if (condition_is_true == condition_value) {
      // We don't update the conditions here, because we're replacing {node}
      // with the {control} node that already contains the right information.
      ReplaceWithValue(node, dead(), effect, control);
    } else {
      control = graph()->NewNode(common()->Deoptimize(p.reason(), p.feedback()),
                                 frame_state, effect, control);
      MergeControlToEnd(graph(), common(), control);
    }
    return Replace(dead());
  }
  return UpdateStatesHelper(node, conditions, condition, node,
                            condition_is_true, false);
}

Reduction BranchElimination::ReduceIf(Node* node, bool is_true_branch) {
  // Add the condition to the list arriving from the input branch.
  Node* branch = NodeProperties::GetControlInput(node, 0);
  ControlPathConditions from_branch = GetState(branch);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!IsReduced(branch)) {
    return NoChange();
  }
  Node* condition = branch->InputAt(0);
  return UpdateStatesHelper(node, from_branch, condition, branch,
                            is_true_branch, true);
}

Reduction BranchElimination::ReduceLoop(Node* node) {
  // Here we rely on having only reducible loops:
  // The loop entry edge always dominates the header, so we can just use
  // the information from the loop entry edge.
  return TakeStatesFromFirstControl(node);
}

Reduction BranchElimination::ReduceMerge(Node* node) {
  // Shortcut for the case when we do not know anything about some
  // input.
  Node::Inputs inputs = node->inputs();
  for (Node* input : inputs) {
    if (!IsReduced(input)) {
      return NoChange();
    }
  }

  auto input_it = inputs.begin();

  DCHECK_GT(inputs.count(), 0);

  ControlPathConditions conditions = GetState(*input_it);
  ++input_it;
  // Merge the first input's conditions with the conditions from the other
  // inputs.
  auto input_end = inputs.end();
  for (; input_it != input_end; ++input_it) {
    // Change the current condition block list to a longest common tail of this
    // condition list and the other list. (The common tail should correspond to
    // the list from the common dominator.)
    conditions.ResetToCommonAncestor(GetState(*input_it));
  }
  return UpdateStates(node, conditions);
}

Reduction BranchElimination::ReduceStart(Node* node) {
  return UpdateStates(node, ControlPathConditions(zone()));
}

Reduction BranchElimination::ReduceOtherControl(Node* node) {
  DCHECK_EQ(1, node->op()->ControlInputCount());
  return TakeStatesFromFirstControl(node);
}

TFGraph* BranchElimination::graph() const { return jsgraph()->graph(); }

Isolate* BranchElimination::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* BranchElimination::common() const {
  return jsgraph()->common();
}

// Workaround a gcc bug causing link errors.
// Related issue: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105848
template bool DefaultConstruct<bool>(Zone* zone);

}  // namespace compiler
}  // namespace internal
}  // namespace v8
