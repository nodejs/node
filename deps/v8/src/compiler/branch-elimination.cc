// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"

#include "src/base/small-vector.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

BranchElimination::BranchElimination(Editor* editor, JSGraph* js_graph,
                                     Zone* zone,
                                     SourcePositionTable* source_positions,
                                     Phase phase)
    : AdvancedReducer(editor),
      jsgraph_(js_graph),
      node_conditions_(js_graph->graph()->NodeCount(), zone),
      reduced_(js_graph->graph()->NodeCount(), zone),
      zone_(zone),
      source_positions_(source_positions),
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
                                    &condition_value)) {
      return;
    }

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
  if (!reduced_.Get(control_input)) return NoChange();
  ControlPathConditions from_input = node_conditions_.Get(control_input);
  Node* branch;
  bool condition_value;
  // If we know the condition we can discard the branch.
  if (from_input.LookupCondition(condition, &branch, &condition_value)) {
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

// Simplify a trap following a merge.
// Assuming condition is in control1's path conditions, and !condition is in
// control2's path condtions, the following transformation takes place:
//
//            control1 control2     condition  effect1
//                  \   /                   \ /  |
//                  Merge                    X   |   control1
//                    |                     / \  |   /
//   effect1  effect2 |                    |   TrapIf     control2
//         \     |   /|         ==>        |         \    /
//          EffectPhi |                    | effect2  Merge
//              |    /                     |    |     /
//   condition  |   /                       \   |    /
//          \   |  /                        EffectPhi
//           TrapIf
// TODO(manoskouk): We require that the trap's effect input is the Merge's
// EffectPhi, so we can ensure that the new traps' effect inputs are not
// dominated by the Merge. Can we relax this?
bool BranchElimination::TryPullTrapIntoMerge(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kTrapIf ||
         node->opcode() == IrOpcode::kTrapUnless);
  Node* merge = NodeProperties::GetControlInput(node);
  DCHECK_EQ(merge->opcode(), IrOpcode::kMerge);
  Node* condition = NodeProperties::GetValueInput(node, 0);
  Node* effect_input = NodeProperties::GetEffectInput(node);
  if (!(effect_input->opcode() == IrOpcode::kEffectPhi &&
        NodeProperties::GetControlInput(effect_input) == merge)) {
    return false;
  }

  bool trapping_condition = node->opcode() == IrOpcode::kTrapIf;
  base::SmallVector<Node*, 8> new_merge_inputs;
  for (Edge edge : merge->input_edges()) {
    Node* input = edge.to();
    ControlPathConditions from_input = node_conditions_.Get(input);
    Node* previous_branch;
    bool condition_value;
    if (!from_input.LookupCondition(condition, &previous_branch,
                                    &condition_value)) {
      return false;
    }
    if (condition_value == trapping_condition) {
      Node* inputs[] = {
          condition, NodeProperties::GetEffectInput(effect_input, edge.index()),
          input};
      Node* trap_clone = graph()->NewNode(node->op(), 3, inputs);
      if (source_positions_) {
        source_positions_->SetSourcePosition(
            trap_clone, source_positions_->GetSourcePosition(node));
      }
      new_merge_inputs.emplace_back(trap_clone);
    } else {
      new_merge_inputs.emplace_back(input);
    }
  }

  for (int i = 0; i < merge->InputCount(); i++) {
    merge->ReplaceInput(i, new_merge_inputs[i]);
  }
  ReplaceWithValue(node, dead(), dead(), merge);
  node->Kill();
  Revisit(merge);

  return true;
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
  if (!reduced_.Get(control_input)) return NoChange();

  // If the trap comes directly after a merge, pull it into the merge. This will
  // unlock other optimizations later.
  if (control_input->opcode() == IrOpcode::kMerge &&
      TryPullTrapIntoMerge(node)) {
    return Replace(dead());
  }

  ControlPathConditions from_input = node_conditions_.Get(control_input);
  Node* previous_branch;
  bool condition_value;

  if (from_input.LookupCondition(condition, &previous_branch,
                                 &condition_value)) {
    if (condition_value == trapping_condition) {
      // Special case: Trap directly inside a branch without sibling nodes.
      // Replace the branch with the trap.
      //    condition  control              condition  control
      //     |   \      /                        \      /
      //     |    Branch                          TrapIf
      //     |    /    \            ==>             |
      //     |  IfTrue IfFalse                 <subgraph2>
      //     |  /        |
      //    TrapIf   <subraph2>            Dead
      //      |                              |
      //   <subgraph1>                     <subgraph1>
      // (and symmetrically for TrapUnless.)
      if (((trapping_condition &&
            control_input->opcode() == IrOpcode::kIfTrue) ||
           (!trapping_condition &&
            control_input->opcode() == IrOpcode::kIfFalse)) &&
          control_input->UseCount() == 1) {
        Node* branch = NodeProperties::GetControlInput(control_input);
        DCHECK_EQ(branch->opcode(), IrOpcode::kBranch);
        if (condition == NodeProperties::GetValueInput(branch, 0)) {
          Node* other_if_branch = nullptr;
          for (Node* use : branch->uses()) {
            if (use != control_input) other_if_branch = use;
          }
          DCHECK_NOT_NULL(other_if_branch);

          node->ReplaceInput(NodeProperties::FirstControlIndex(node),
                             NodeProperties::GetControlInput(branch));
          ReplaceWithValue(node, dead(), dead(), dead());
          ReplaceWithValue(other_if_branch, node, node, node);
          other_if_branch->Kill();
          control_input->Kill();
          branch->Kill();
          return Changed(node);
        }
      }

      // General case: This will always trap. Mark its outputs as dead and
      // connect it to graph()->end().
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
  if (!reduced_.Get(control)) {
    return NoChange();
  }

  ControlPathConditions conditions = node_conditions_.Get(control);
  bool condition_value;
  Node* branch;
  // If we know the condition we can discard the branch.
  if (conditions.LookupCondition(condition, &branch, &condition_value)) {
    if (condition_is_true == condition_value) {
      // We don't update the conditions here, because we're replacing {node}
      // with the {control} node that already contains the right information.
      ReplaceWithValue(node, dead(), effect, control);
    } else {
      control = graph()->NewNode(common()->Deoptimize(p.reason(), p.feedback()),
                                 frame_state, effect, control);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
    }
    return Replace(dead());
  }
  return UpdateConditions(node, conditions, condition, node, condition_is_true,
                          false);
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
  return UpdateConditions(node, from_branch, condition, branch, is_true_branch,
                          true);
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
    // Change the current condition block list to a longest common tail of this
    // condition list and the other list. (The common tail should correspond to
    // the list from the common dominator.)
    conditions.ResetToCommonAncestor(node_conditions_.Get(*input_it));
  }
  return UpdateConditions(node, conditions);
}

Reduction BranchElimination::ReduceStart(Node* node) {
  return UpdateConditions(node, ControlPathConditions(zone_));
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
  bool reduced_changed = reduced_.Set(node, true);
  bool node_conditions_changed = node_conditions_.Set(node, conditions);
  if (reduced_changed || node_conditions_changed) {
    return Changed(node);
  }
  return NoChange();
}

Reduction BranchElimination::UpdateConditions(
    Node* node, ControlPathConditions prev_conditions, Node* current_condition,
    Node* current_branch, bool is_true_branch, bool in_new_block) {
  // The control path for the node is the path obtained by appending the
  // current_condition to the prev_conditions. Use the original control path as
  // a hint to avoid allocations.
  if (in_new_block || prev_conditions.blocks_.Size() == 0) {
    prev_conditions.AddConditionInNewBlock(zone_, current_condition,
                                           current_branch, is_true_branch);
  } else {
    ControlPathConditions original = node_conditions_.Get(node);
    prev_conditions.AddCondition(zone_, current_condition, current_branch,
                                 is_true_branch, original);
  }
  return UpdateConditions(node, prev_conditions);
}

void BranchElimination::ControlPathConditions::AddCondition(
    Zone* zone, Node* condition, Node* branch, bool is_true,
    ControlPathConditions hint) {
  if (!LookupCondition(condition)) {
    BranchCondition branch_condition(condition, branch, is_true);
    FunctionalList<BranchCondition> prev_front = blocks_.Front();
    if (hint.blocks_.Size() > 0) {
      prev_front.PushFront(branch_condition, zone, hint.blocks_.Front());
    } else {
      prev_front.PushFront(branch_condition, zone);
    }
    blocks_.DropFront();
    blocks_.PushFront(prev_front, zone);
    conditions_.Set(condition, branch_condition);
    SLOW_DCHECK(BlocksAndConditionsInvariant());
  }
}

void BranchElimination::ControlPathConditions::AddConditionInNewBlock(
    Zone* zone, Node* condition, Node* branch, bool is_true) {
  FunctionalList<BranchCondition> new_block;
  if (!LookupCondition(condition)) {
    BranchCondition branch_condition(condition, branch, is_true);
    new_block.PushFront(branch_condition, zone);
    conditions_.Set(condition, branch_condition);
  }
  blocks_.PushFront(new_block, zone);
  SLOW_DCHECK(BlocksAndConditionsInvariant());
}

bool BranchElimination::ControlPathConditions::LookupCondition(
    Node* condition) const {
  return conditions_.Get(condition).IsSet();
}

bool BranchElimination::ControlPathConditions::LookupCondition(
    Node* condition, Node** branch, bool* is_true) const {
  const BranchCondition& element = conditions_.Get(condition);
  if (element.IsSet()) {
    *is_true = element.is_true;
    *branch = element.branch;
    return true;
  }
  return false;
}

void BranchElimination::ControlPathConditions::ResetToCommonAncestor(
    ControlPathConditions other) {
  while (other.blocks_.Size() > blocks_.Size()) other.blocks_.DropFront();
  while (blocks_.Size() > other.blocks_.Size()) {
    for (BranchCondition branch_condition : blocks_.Front()) {
      conditions_.Set(branch_condition.condition, {});
    }
    blocks_.DropFront();
  }
  while (blocks_ != other.blocks_) {
    for (BranchCondition branch_condition : blocks_.Front()) {
      conditions_.Set(branch_condition.condition, {});
    }
    blocks_.DropFront();
    other.blocks_.DropFront();
  }
  SLOW_DCHECK(BlocksAndConditionsInvariant());
}

#if DEBUG
bool BranchElimination::ControlPathConditions::BlocksAndConditionsInvariant() {
  PersistentMap<Node*, BranchCondition> conditions_copy(conditions_);
  for (auto block : blocks_) {
    for (BranchCondition condition : block) {
      // Every element of blocks_ has to be in conditions_.
      if (conditions_copy.Get(condition.condition) != condition) return false;
      conditions_copy.Set(condition.condition, {});
    }
  }
  // Every element of {conditions_} has to be in {blocks_}. We removed all
  // elements of blocks_ from condition_copy, so if it is not empty, the
  // invariant fails.
  return conditions_copy.begin() == conditions_copy.end();
}
#endif

Graph* BranchElimination::graph() const { return jsgraph()->graph(); }

Isolate* BranchElimination::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* BranchElimination::common() const {
  return jsgraph()->common();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
