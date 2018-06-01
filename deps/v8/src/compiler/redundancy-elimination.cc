// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/redundancy-elimination.h"

#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

RedundancyElimination::RedundancyElimination(Editor* editor, Zone* zone)
    : AdvancedReducer(editor), node_checks_(zone), zone_(zone) {}

RedundancyElimination::~RedundancyElimination() {}

Reduction RedundancyElimination::Reduce(Node* node) {
  if (node_checks_.Get(node)) return NoChange();
  switch (node->opcode()) {
    case IrOpcode::kCheckBounds:
    case IrOpcode::kCheckEqualsInternalizedString:
    case IrOpcode::kCheckEqualsSymbol:
    case IrOpcode::kCheckFloat64Hole:
    case IrOpcode::kCheckHeapObject:
    case IrOpcode::kCheckIf:
    case IrOpcode::kCheckInternalizedString:
    case IrOpcode::kCheckNotTaggedHole:
    case IrOpcode::kCheckNumber:
    case IrOpcode::kCheckReceiver:
    case IrOpcode::kCheckSmi:
    case IrOpcode::kCheckString:
    case IrOpcode::kCheckSymbol:
    case IrOpcode::kCheckedFloat64ToInt32:
    case IrOpcode::kCheckedInt32Add:
    case IrOpcode::kCheckedInt32Div:
    case IrOpcode::kCheckedInt32Mod:
    case IrOpcode::kCheckedInt32Mul:
    case IrOpcode::kCheckedInt32Sub:
    case IrOpcode::kCheckedInt32ToTaggedSigned:
    case IrOpcode::kCheckedTaggedSignedToInt32:
    case IrOpcode::kCheckedTaggedToFloat64:
    case IrOpcode::kCheckedTaggedToInt32:
    case IrOpcode::kCheckedTaggedToTaggedPointer:
    case IrOpcode::kCheckedTaggedToTaggedSigned:
    case IrOpcode::kCheckedTruncateTaggedToWord32:
    case IrOpcode::kCheckedUint32Div:
    case IrOpcode::kCheckedUint32Mod:
    case IrOpcode::kCheckedUint32ToInt32:
    case IrOpcode::kCheckedUint32ToTaggedSigned:
      return ReduceCheckNode(node);
    case IrOpcode::kSpeculativeNumberAdd:
    case IrOpcode::kSpeculativeNumberSubtract:
    case IrOpcode::kSpeculativeSafeIntegerAdd:
    case IrOpcode::kSpeculativeSafeIntegerSubtract:
      // For increments and decrements by a constant, try to learn from the last
      // bounds check.
      return TryReuseBoundsCheckForFirstInput(node);
    case IrOpcode::kEffectPhi:
      return ReduceEffectPhi(node);
    case IrOpcode::kDead:
      break;
    case IrOpcode::kStart:
      return ReduceStart(node);
    default:
      return ReduceOtherNode(node);
  }
  return NoChange();
}

// static
RedundancyElimination::EffectPathChecks*
RedundancyElimination::EffectPathChecks::Copy(Zone* zone,
                                              EffectPathChecks const* checks) {
  return new (zone->New(sizeof(EffectPathChecks))) EffectPathChecks(*checks);
}

// static
RedundancyElimination::EffectPathChecks const*
RedundancyElimination::EffectPathChecks::Empty(Zone* zone) {
  return new (zone->New(sizeof(EffectPathChecks))) EffectPathChecks(nullptr, 0);
}

bool RedundancyElimination::EffectPathChecks::Equals(
    EffectPathChecks const* that) const {
  if (this->size_ != that->size_) return false;
  Check* this_head = this->head_;
  Check* that_head = that->head_;
  while (this_head != that_head) {
    if (this_head->node != that_head->node) return false;
    this_head = this_head->next;
    that_head = that_head->next;
  }
  return true;
}

void RedundancyElimination::EffectPathChecks::Merge(
    EffectPathChecks const* that) {
  // Change the current check list to a longest common tail of this check
  // list and the other list.

  // First, we throw away the prefix of the longer list, so that
  // we have lists of the same length.
  Check* that_head = that->head_;
  size_t that_size = that->size_;
  while (that_size > size_) {
    that_head = that_head->next;
    that_size--;
  }
  while (size_ > that_size) {
    head_ = head_->next;
    size_--;
  }

  // Then we go through both lists in lock-step until we find
  // the common tail.
  while (head_ != that_head) {
    DCHECK_LT(0u, size_);
    DCHECK_NOT_NULL(head_);
    size_--;
    head_ = head_->next;
    that_head = that_head->next;
  }
}

RedundancyElimination::EffectPathChecks const*
RedundancyElimination::EffectPathChecks::AddCheck(Zone* zone,
                                                  Node* node) const {
  Check* head = new (zone->New(sizeof(Check))) Check(node, head_);
  return new (zone->New(sizeof(EffectPathChecks)))
      EffectPathChecks(head, size_ + 1);
}

namespace {

// Does check {a} subsume check {b}?
bool CheckSubsumes(Node const* a, Node const* b) {
  if (a->op() != b->op()) {
    if (a->opcode() == IrOpcode::kCheckInternalizedString &&
        b->opcode() == IrOpcode::kCheckString) {
      // CheckInternalizedString(node) implies CheckString(node)
    } else if (a->opcode() != b->opcode()) {
      return false;
    } else {
      switch (a->opcode()) {
        case IrOpcode::kCheckBounds:
        case IrOpcode::kCheckSmi:
        case IrOpcode::kCheckString:
        case IrOpcode::kCheckNumber:
          break;
        case IrOpcode::kCheckedInt32ToTaggedSigned:
        case IrOpcode::kCheckedTaggedSignedToInt32:
        case IrOpcode::kCheckedTaggedToTaggedPointer:
        case IrOpcode::kCheckedTaggedToTaggedSigned:
        case IrOpcode::kCheckedUint32ToInt32:
        case IrOpcode::kCheckedUint32ToTaggedSigned:
          break;
        case IrOpcode::kCheckedFloat64ToInt32:
        case IrOpcode::kCheckedTaggedToInt32: {
          const CheckMinusZeroParameters& ap =
              CheckMinusZeroParametersOf(a->op());
          const CheckMinusZeroParameters& bp =
              CheckMinusZeroParametersOf(b->op());
          if (ap.mode() != bp.mode()) {
            return false;
          }
          break;
        }
        default:
          DCHECK(!IsCheckedWithFeedback(a->op()));
          return false;
      }
    }
  }
  for (int i = a->op()->ValueInputCount(); --i >= 0;) {
    if (a->InputAt(i) != b->InputAt(i)) return false;
  }
  return true;
}

}  // namespace

Node* RedundancyElimination::EffectPathChecks::LookupCheck(Node* node) const {
  for (Check const* check = head_; check != nullptr; check = check->next) {
    if (CheckSubsumes(check->node, node)) {
      DCHECK(!check->node->IsDead());
      return check->node;
    }
  }
  return nullptr;
}

Node* RedundancyElimination::EffectPathChecks::LookupBoundsCheckFor(
    Node* node) const {
  for (Check const* check = head_; check != nullptr; check = check->next) {
    if (check->node->opcode() == IrOpcode::kCheckBounds &&
        check->node->InputAt(0) == node) {
      return check->node;
    }
  }
  return nullptr;
}

RedundancyElimination::EffectPathChecks const*
RedundancyElimination::PathChecksForEffectNodes::Get(Node* node) const {
  size_t const id = node->id();
  if (id < info_for_node_.size()) return info_for_node_[id];
  return nullptr;
}

void RedundancyElimination::PathChecksForEffectNodes::Set(
    Node* node, EffectPathChecks const* checks) {
  size_t const id = node->id();
  if (id >= info_for_node_.size()) info_for_node_.resize(id + 1, nullptr);
  info_for_node_[id] = checks;
}

Reduction RedundancyElimination::ReduceCheckNode(Node* node) {
  Node* const effect = NodeProperties::GetEffectInput(node);
  EffectPathChecks const* checks = node_checks_.Get(effect);
  // If we do not know anything about the predecessor, do not propagate just yet
  // because we will have to recompute anyway once we compute the predecessor.
  if (checks == nullptr) return NoChange();
  // See if we have another check that dominates us.
  if (Node* check = checks->LookupCheck(node)) {
    ReplaceWithValue(node, check);
    return Replace(check);
  }

  // Learn from this check.
  return UpdateChecks(node, checks->AddCheck(zone(), node));
}

Reduction RedundancyElimination::TryReuseBoundsCheckForFirstInput(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kSpeculativeNumberAdd ||
         node->opcode() == IrOpcode::kSpeculativeNumberSubtract ||
         node->opcode() == IrOpcode::kSpeculativeSafeIntegerAdd ||
         node->opcode() == IrOpcode::kSpeculativeSafeIntegerSubtract);

  DCHECK_EQ(1, node->op()->EffectInputCount());
  DCHECK_EQ(1, node->op()->EffectOutputCount());

  Node* const effect = NodeProperties::GetEffectInput(node);
  EffectPathChecks const* checks = node_checks_.Get(effect);

  // If we do not know anything about the predecessor, do not propagate just yet
  // because we will have to recompute anyway once we compute the predecessor.
  if (checks == nullptr) return NoChange();

  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  // Only use bounds checks for increments/decrements by a constant.
  if (right->opcode() == IrOpcode::kNumberConstant) {
    if (Node* bounds_check = checks->LookupBoundsCheckFor(left)) {
      // Only use the bounds checked type if it is better.
      if (NodeProperties::GetType(bounds_check)
              ->Is(NodeProperties::GetType(left))) {
        node->ReplaceInput(0, bounds_check);
      }
    }
  }

  return UpdateChecks(node, checks);
}

Reduction RedundancyElimination::ReduceEffectPhi(Node* node) {
  Node* const control = NodeProperties::GetControlInput(node);
  if (control->opcode() == IrOpcode::kLoop) {
    // Here we rely on having only reducible loops:
    // The loop entry edge always dominates the header, so we can just use
    // the information from the loop entry edge.
    return TakeChecksFromFirstEffect(node);
  }
  DCHECK_EQ(IrOpcode::kMerge, control->opcode());

  // Shortcut for the case when we do not know anything about some input.
  int const input_count = node->op()->EffectInputCount();
  for (int i = 0; i < input_count; ++i) {
    Node* const effect = NodeProperties::GetEffectInput(node, i);
    if (node_checks_.Get(effect) == nullptr) return NoChange();
  }

  // Make a copy of the first input's checks and merge with the checks
  // from other inputs.
  EffectPathChecks* checks = EffectPathChecks::Copy(
      zone(), node_checks_.Get(NodeProperties::GetEffectInput(node, 0)));
  for (int i = 1; i < input_count; ++i) {
    Node* const input = NodeProperties::GetEffectInput(node, i);
    checks->Merge(node_checks_.Get(input));
  }
  return UpdateChecks(node, checks);
}

Reduction RedundancyElimination::ReduceStart(Node* node) {
  return UpdateChecks(node, EffectPathChecks::Empty(zone()));
}

Reduction RedundancyElimination::ReduceOtherNode(Node* node) {
  if (node->op()->EffectInputCount() == 1) {
    if (node->op()->EffectOutputCount() == 1) {
      return TakeChecksFromFirstEffect(node);
    } else {
      // Effect terminators should be handled specially.
      return NoChange();
    }
  }
  DCHECK_EQ(0, node->op()->EffectInputCount());
  DCHECK_EQ(0, node->op()->EffectOutputCount());
  return NoChange();
}

Reduction RedundancyElimination::TakeChecksFromFirstEffect(Node* node) {
  DCHECK_EQ(1, node->op()->EffectOutputCount());
  Node* const effect = NodeProperties::GetEffectInput(node);
  EffectPathChecks const* checks = node_checks_.Get(effect);
  // If we do not know anything about the predecessor, do not propagate just yet
  // because we will have to recompute anyway once we compute the predecessor.
  if (checks == nullptr) return NoChange();
  // We just propagate the information from the effect input (ideally,
  // we would only revisit effect uses if there is change).
  return UpdateChecks(node, checks);
}

Reduction RedundancyElimination::UpdateChecks(Node* node,
                                              EffectPathChecks const* checks) {
  EffectPathChecks const* original = node_checks_.Get(node);
  // Only signal that the {node} has Changed, if the information about {checks}
  // has changed wrt. the {original}.
  if (checks != original) {
    if (original == nullptr || !checks->Equals(original)) {
      node_checks_.Set(node, checks);
      return Changed(node);
    }
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
