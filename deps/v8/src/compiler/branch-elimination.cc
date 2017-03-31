// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

BranchElimination::BranchElimination(Editor* editor, JSGraph* js_graph,
                                     Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(js_graph),
      node_conditions_(zone, js_graph->graph()->NodeCount()),
      zone_(zone),
      dead_(js_graph->graph()->NewNode(js_graph->common()->Dead())) {
  NodeProperties::SetType(dead_, Type::None());
}

BranchElimination::~BranchElimination() {}


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


Reduction BranchElimination::ReduceBranch(Node* node) {
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  const ControlPathConditions* from_input = node_conditions_.Get(control_input);
  if (from_input != nullptr) {
    Maybe<bool> condition_value = from_input->LookupCondition(condition);
    // If we know the condition we can discard the branch.
    if (condition_value.IsJust()) {
      bool known_value = condition_value.FromJust();
      for (Node* const use : node->uses()) {
        switch (use->opcode()) {
          case IrOpcode::kIfTrue:
            Replace(use, known_value ? control_input : dead());
            break;
          case IrOpcode::kIfFalse:
            Replace(use, known_value ? dead() : control_input);
            break;
          default:
            UNREACHABLE();
        }
      }
      return Replace(dead());
    }
  }
  return TakeConditionsFromFirstControl(node);
}

Reduction BranchElimination::ReduceDeoptimizeConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kDeoptimizeIf ||
         node->opcode() == IrOpcode::kDeoptimizeUnless);
  bool condition_is_true = node->opcode() == IrOpcode::kDeoptimizeUnless;
  DeoptimizeReason reason = DeoptimizeReasonOf(node->op());
  Node* condition = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  ControlPathConditions const* conditions = node_conditions_.Get(control);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (conditions == nullptr) {
    return UpdateConditions(node, conditions);
  }
  Maybe<bool> condition_value = conditions->LookupCondition(condition);
  if (condition_value.IsJust()) {
    // If we know the condition we can discard the branch.
    if (condition_is_true == condition_value.FromJust()) {
      // We don't update the conditions here, because we're replacing {node}
      // with the {control} node that already contains the right information.
      ReplaceWithValue(node, dead(), effect, control);
    } else {
      control =
          graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kEager, reason),
                           frame_state, effect, control);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
    }
    return Replace(dead());
  }
  return UpdateConditions(
      node, conditions->AddCondition(zone_, condition, condition_is_true));
}

Reduction BranchElimination::ReduceIf(Node* node, bool is_true_branch) {
  // Add the condition to the list arriving from the input branch.
  Node* branch = NodeProperties::GetControlInput(node, 0);
  const ControlPathConditions* from_branch = node_conditions_.Get(branch);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (from_branch == nullptr) {
    return UpdateConditions(node, nullptr);
  }
  Node* condition = branch->InputAt(0);
  return UpdateConditions(
      node, from_branch->AddCondition(zone_, condition, is_true_branch));
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
    if (node_conditions_.Get(input) == nullptr) {
      return UpdateConditions(node, nullptr);
    }
  }

  auto input_it = inputs.begin();

  DCHECK_GT(inputs.count(), 0);

  const ControlPathConditions* first = node_conditions_.Get(*input_it);
  ++input_it;
  // Make a copy of the first input's conditions and merge with the conditions
  // from other inputs.
  ControlPathConditions* conditions =
      new (zone_->New(sizeof(ControlPathConditions)))
          ControlPathConditions(*first);
  auto input_end = inputs.end();
  for (; input_it != input_end; ++input_it) {
    conditions->Merge(*(node_conditions_.Get(*input_it)));
  }

  return UpdateConditions(node, conditions);
}


Reduction BranchElimination::ReduceStart(Node* node) {
  return UpdateConditions(node, ControlPathConditions::Empty(zone_));
}


const BranchElimination::ControlPathConditions*
BranchElimination::PathConditionsForControlNodes::Get(Node* node) {
  if (static_cast<size_t>(node->id()) < info_for_node_.size()) {
    return info_for_node_[node->id()];
  }
  return nullptr;
}


void BranchElimination::PathConditionsForControlNodes::Set(
    Node* node, const ControlPathConditions* conditions) {
  size_t index = static_cast<size_t>(node->id());
  if (index >= info_for_node_.size()) {
    info_for_node_.resize(index + 1, nullptr);
  }
  info_for_node_[index] = conditions;
}


Reduction BranchElimination::ReduceOtherControl(Node* node) {
  DCHECK_EQ(1, node->op()->ControlInputCount());
  return TakeConditionsFromFirstControl(node);
}


Reduction BranchElimination::TakeConditionsFromFirstControl(Node* node) {
  // We just propagate the information from the control input (ideally,
  // we would only revisit control uses if there is change).
  const ControlPathConditions* from_input =
      node_conditions_.Get(NodeProperties::GetControlInput(node, 0));
  return UpdateConditions(node, from_input);
}


Reduction BranchElimination::UpdateConditions(
    Node* node, const ControlPathConditions* conditions) {
  const ControlPathConditions* original = node_conditions_.Get(node);
  // Only signal that the node has Changed if the condition information has
  // changed.
  if (conditions != original) {
    if (conditions == nullptr || original == nullptr ||
        *conditions != *original) {
      node_conditions_.Set(node, conditions);
      return Changed(node);
    }
  }
  return NoChange();
}


// static
const BranchElimination::ControlPathConditions*
BranchElimination::ControlPathConditions::Empty(Zone* zone) {
  return new (zone->New(sizeof(ControlPathConditions)))
      ControlPathConditions(nullptr, 0);
}


void BranchElimination::ControlPathConditions::Merge(
    const ControlPathConditions& other) {
  // Change the current condition list to a longest common tail
  // of this condition list and the other list. (The common tail
  // should correspond to the list from the common dominator.)

  // First, we throw away the prefix of the longer list, so that
  // we have lists of the same length.
  size_t other_size = other.condition_count_;
  BranchCondition* other_condition = other.head_;
  while (other_size > condition_count_) {
    other_condition = other_condition->next;
    other_size--;
  }
  while (condition_count_ > other_size) {
    head_ = head_->next;
    condition_count_--;
  }

  // Then we go through both lists in lock-step until we find
  // the common tail.
  while (head_ != other_condition) {
    DCHECK(condition_count_ > 0);
    condition_count_--;
    other_condition = other_condition->next;
    head_ = head_->next;
  }
}


const BranchElimination::ControlPathConditions*
BranchElimination::ControlPathConditions::AddCondition(Zone* zone,
                                                       Node* condition,
                                                       bool is_true) const {
  DCHECK(LookupCondition(condition).IsNothing());

  BranchCondition* new_head = new (zone->New(sizeof(BranchCondition)))
      BranchCondition(condition, is_true, head_);

  ControlPathConditions* conditions =
      new (zone->New(sizeof(ControlPathConditions)))
          ControlPathConditions(new_head, condition_count_ + 1);
  return conditions;
}


Maybe<bool> BranchElimination::ControlPathConditions::LookupCondition(
    Node* condition) const {
  for (BranchCondition* current = head_; current != nullptr;
       current = current->next) {
    if (current->condition == condition) {
      return Just<bool>(current->is_true);
    }
  }
  return Nothing<bool>();
}


bool BranchElimination::ControlPathConditions::operator==(
    const ControlPathConditions& other) const {
  if (condition_count_ != other.condition_count_) return false;
  BranchCondition* this_condition = head_;
  BranchCondition* other_condition = other.head_;
  while (true) {
    if (this_condition == other_condition) return true;
    if (this_condition->condition != other_condition->condition ||
        this_condition->is_true != other_condition->is_true) {
      return false;
    }
    this_condition = this_condition->next;
    other_condition = other_condition->next;
  }
  UNREACHABLE();
  return false;
}

Graph* BranchElimination::graph() const { return jsgraph()->graph(); }

CommonOperatorBuilder* BranchElimination::common() const {
  return jsgraph()->common();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
