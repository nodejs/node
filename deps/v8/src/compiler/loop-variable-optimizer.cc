// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/loop-variable-optimizer.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-marker.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// Macro for outputting trace information from representation inference.
#define TRACE(...)                                  \
  do {                                              \
    if (FLAG_trace_turbo_loop) PrintF(__VA_ARGS__); \
  } while (false)

LoopVariableOptimizer::LoopVariableOptimizer(Graph* graph,
                                             CommonOperatorBuilder* common,
                                             Zone* zone)
    : graph_(graph),
      common_(common),
      zone_(zone),
      limits_(graph->NodeCount(), zone),
      induction_vars_(zone) {}

void LoopVariableOptimizer::Run() {
  ZoneQueue<Node*> queue(zone());
  queue.push(graph()->start());
  NodeMarker<bool> queued(graph(), 2);
  while (!queue.empty()) {
    Node* node = queue.front();
    queue.pop();
    queued.Set(node, false);

    DCHECK_NULL(limits_[node->id()]);
    bool all_inputs_visited = true;
    int inputs_end = (node->opcode() == IrOpcode::kLoop)
                         ? kFirstBackedge
                         : node->op()->ControlInputCount();
    for (int i = 0; i < inputs_end; i++) {
      if (limits_[NodeProperties::GetControlInput(node, i)->id()] == nullptr) {
        all_inputs_visited = false;
        break;
      }
    }
    if (!all_inputs_visited) continue;

    VisitNode(node);
    DCHECK_NOT_NULL(limits_[node->id()]);

    // Queue control outputs.
    for (Edge edge : node->use_edges()) {
      if (NodeProperties::IsControlEdge(edge) &&
          edge.from()->op()->ControlOutputCount() > 0) {
        Node* use = edge.from();
        if (use->opcode() == IrOpcode::kLoop &&
            edge.index() != kAssumedLoopEntryIndex) {
          VisitBackedge(node, use);
        } else if (!queued.Get(use)) {
          queue.push(use);
          queued.Set(use, true);
        }
      }
    }
  }
}

class LoopVariableOptimizer::Constraint : public ZoneObject {
 public:
  InductionVariable::ConstraintKind kind() const { return kind_; }
  Node* left() const { return left_; }
  Node* right() const { return right_; }

  const Constraint* next() const { return next_; }

  Constraint(Node* left, InductionVariable::ConstraintKind kind, Node* right,
             const Constraint* next)
      : left_(left), right_(right), kind_(kind), next_(next) {}

 private:
  Node* left_;
  Node* right_;
  InductionVariable::ConstraintKind kind_;
  const Constraint* next_;
};

class LoopVariableOptimizer::VariableLimits : public ZoneObject {
 public:
  static VariableLimits* Empty(Zone* zone) {
    return new (zone) VariableLimits();
  }

  VariableLimits* Copy(Zone* zone) const {
    return new (zone) VariableLimits(this);
  }

  void Add(Node* left, InductionVariable::ConstraintKind kind, Node* right,
           Zone* zone) {
    head_ = new (zone) Constraint(left, kind, right, head_);
    limit_count_++;
  }

  void Merge(const VariableLimits* other) {
    // Change the current condition list to a longest common tail
    // of this condition list and the other list. (The common tail
    // should correspond to the list from the common dominator.)

    // First, we throw away the prefix of the longer list, so that
    // we have lists of the same length.
    size_t other_size = other->limit_count_;
    const Constraint* other_limit = other->head_;
    while (other_size > limit_count_) {
      other_limit = other_limit->next();
      other_size--;
    }
    while (limit_count_ > other_size) {
      head_ = head_->next();
      limit_count_--;
    }

    // Then we go through both lists in lock-step until we find
    // the common tail.
    while (head_ != other_limit) {
      DCHECK_LT(0, limit_count_);
      limit_count_--;
      other_limit = other_limit->next();
      head_ = head_->next();
    }
  }

  const Constraint* head() const { return head_; }

 private:
  VariableLimits() {}
  explicit VariableLimits(const VariableLimits* other)
      : head_(other->head_), limit_count_(other->limit_count_) {}

  const Constraint* head_ = nullptr;
  size_t limit_count_ = 0;
};

void InductionVariable::AddUpperBound(Node* bound,
                                      InductionVariable::ConstraintKind kind) {
  if (FLAG_trace_turbo_loop) {
    OFStream os(stdout);
    os << "New upper bound for " << phi()->id() << " (loop "
       << NodeProperties::GetControlInput(phi())->id() << "): " << *bound
       << std::endl;
  }
  upper_bounds_.push_back(Bound(bound, kind));
}

void InductionVariable::AddLowerBound(Node* bound,
                                      InductionVariable::ConstraintKind kind) {
  if (FLAG_trace_turbo_loop) {
    OFStream os(stdout);
    os << "New lower bound for " << phi()->id() << " (loop "
       << NodeProperties::GetControlInput(phi())->id() << "): " << *bound;
  }
  lower_bounds_.push_back(Bound(bound, kind));
}

void LoopVariableOptimizer::VisitBackedge(Node* from, Node* loop) {
  if (loop->op()->ControlInputCount() != 2) return;

  // Go through the constraints, and update the induction variables in
  // this loop if they are involved in the constraint.
  const VariableLimits* limits = limits_[from->id()];
  for (const Constraint* constraint = limits->head(); constraint != nullptr;
       constraint = constraint->next()) {
    if (constraint->left()->opcode() == IrOpcode::kPhi &&
        NodeProperties::GetControlInput(constraint->left()) == loop) {
      auto var = induction_vars_.find(constraint->left()->id());
      if (var != induction_vars_.end()) {
        var->second->AddUpperBound(constraint->right(), constraint->kind());
      }
    }
    if (constraint->right()->opcode() == IrOpcode::kPhi &&
        NodeProperties::GetControlInput(constraint->right()) == loop) {
      auto var = induction_vars_.find(constraint->right()->id());
      if (var != induction_vars_.end()) {
        var->second->AddLowerBound(constraint->left(), constraint->kind());
      }
    }
  }
}

void LoopVariableOptimizer::VisitNode(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kMerge:
      return VisitMerge(node);
    case IrOpcode::kLoop:
      return VisitLoop(node);
    case IrOpcode::kIfFalse:
      return VisitIf(node, false);
    case IrOpcode::kIfTrue:
      return VisitIf(node, true);
    case IrOpcode::kStart:
      return VisitStart(node);
    case IrOpcode::kLoopExit:
      return VisitLoopExit(node);
    default:
      return VisitOtherControl(node);
  }
}

void LoopVariableOptimizer::VisitMerge(Node* node) {
  // Merge the limits of all incoming edges.
  VariableLimits* merged = limits_[node->InputAt(0)->id()]->Copy(zone());
  for (int i = 1; i < node->InputCount(); i++) {
    merged->Merge(limits_[node->InputAt(i)->id()]);
  }
  limits_[node->id()] = merged;
}

void LoopVariableOptimizer::VisitLoop(Node* node) {
  DetectInductionVariables(node);
  // Conservatively take the limits from the loop entry here.
  return TakeConditionsFromFirstControl(node);
}

void LoopVariableOptimizer::VisitIf(Node* node, bool polarity) {
  Node* branch = node->InputAt(0);
  Node* cond = branch->InputAt(0);
  VariableLimits* limits = limits_[branch->id()]->Copy(zone());
  // Normalize to less than comparison.
  switch (cond->opcode()) {
    case IrOpcode::kJSLessThan:
    case IrOpcode::kSpeculativeNumberLessThan:
      AddCmpToLimits(limits, cond, InductionVariable::kStrict, polarity);
      break;
    case IrOpcode::kJSGreaterThan:
      AddCmpToLimits(limits, cond, InductionVariable::kNonStrict, !polarity);
      break;
    case IrOpcode::kJSLessThanOrEqual:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      AddCmpToLimits(limits, cond, InductionVariable::kNonStrict, polarity);
      break;
    case IrOpcode::kJSGreaterThanOrEqual:
      AddCmpToLimits(limits, cond, InductionVariable::kStrict, !polarity);
      break;
    default:
      break;
  }
  limits_[node->id()] = limits;
}

void LoopVariableOptimizer::AddCmpToLimits(
    VariableLimits* limits, Node* node, InductionVariable::ConstraintKind kind,
    bool polarity) {
  Node* left = node->InputAt(0);
  Node* right = node->InputAt(1);
  if (FindInductionVariable(left) || FindInductionVariable(right)) {
    if (polarity) {
      limits->Add(left, kind, right, zone());
    } else {
      kind = (kind == InductionVariable::kStrict)
                 ? InductionVariable::kNonStrict
                 : InductionVariable::kStrict;
      limits->Add(right, kind, left, zone());
    }
  }
}

void LoopVariableOptimizer::VisitStart(Node* node) {
  limits_[node->id()] = VariableLimits::Empty(zone());
}

void LoopVariableOptimizer::VisitLoopExit(Node* node) {
  return TakeConditionsFromFirstControl(node);
}

void LoopVariableOptimizer::VisitOtherControl(Node* node) {
  DCHECK_EQ(1, node->op()->ControlInputCount());
  return TakeConditionsFromFirstControl(node);
}

void LoopVariableOptimizer::TakeConditionsFromFirstControl(Node* node) {
  const VariableLimits* limits =
      limits_[NodeProperties::GetControlInput(node, 0)->id()];
  DCHECK_NOT_NULL(limits);
  limits_[node->id()] = limits;
}

const InductionVariable* LoopVariableOptimizer::FindInductionVariable(
    Node* node) {
  auto var = induction_vars_.find(node->id());
  if (var != induction_vars_.end()) {
    return var->second;
  }
  return nullptr;
}

InductionVariable* LoopVariableOptimizer::TryGetInductionVariable(Node* phi) {
  DCHECK_EQ(2, phi->op()->ValueInputCount());
  Node* loop = NodeProperties::GetControlInput(phi);
  DCHECK_EQ(IrOpcode::kLoop, loop->opcode());
  Node* initial = phi->InputAt(0);
  Node* arith = phi->InputAt(1);
  InductionVariable::ArithmeticType arithmeticType;
  if (arith->opcode() == IrOpcode::kJSAdd ||
      arith->opcode() == IrOpcode::kSpeculativeNumberAdd ||
      arith->opcode() == IrOpcode::kSpeculativeSafeIntegerAdd) {
    arithmeticType = InductionVariable::ArithmeticType::kAddition;
  } else if (arith->opcode() == IrOpcode::kJSSubtract ||
             arith->opcode() == IrOpcode::kSpeculativeNumberSubtract ||
             arith->opcode() == IrOpcode::kSpeculativeSafeIntegerSubtract) {
    arithmeticType = InductionVariable::ArithmeticType::kSubtraction;
  } else {
    return nullptr;
  }

  // TODO(jarin) Support both sides.
  if (arith->InputAt(0) != phi) return nullptr;

  Node* effect_phi = nullptr;
  for (Node* use : loop->uses()) {
    if (use->opcode() == IrOpcode::kEffectPhi) {
      DCHECK_NULL(effect_phi);
      effect_phi = use;
    }
  }
  if (!effect_phi) return nullptr;

  Node* incr = arith->InputAt(1);
  return new (zone()) InductionVariable(phi, effect_phi, arith, incr, initial,
                                        zone(), arithmeticType);
}

void LoopVariableOptimizer::DetectInductionVariables(Node* loop) {
  if (loop->op()->ControlInputCount() != 2) return;
  TRACE("Loop variables for loop %i:", loop->id());
  for (Edge edge : loop->use_edges()) {
    if (NodeProperties::IsControlEdge(edge) &&
        edge.from()->opcode() == IrOpcode::kPhi) {
      Node* phi = edge.from();
      InductionVariable* induction_var = TryGetInductionVariable(phi);
      if (induction_var) {
        induction_vars_[phi->id()] = induction_var;
        TRACE(" %i", induction_var->phi()->id());
      }
    }
  }
  TRACE("\n");
}

void LoopVariableOptimizer::ChangeToInductionVariablePhis() {
  for (auto entry : induction_vars_) {
    // It only make sense to analyze the induction variables if
    // there is a bound.
    InductionVariable* induction_var = entry.second;
    DCHECK_EQ(MachineRepresentation::kTagged,
              PhiRepresentationOf(induction_var->phi()->op()));
    if (induction_var->upper_bounds().size() == 0 &&
        induction_var->lower_bounds().size() == 0) {
      continue;
    }
    // Insert the increment value to the value inputs.
    induction_var->phi()->InsertInput(graph()->zone(),
                                      induction_var->phi()->InputCount() - 1,
                                      induction_var->increment());
    // Insert the bound inputs to the value inputs.
    for (auto bound : induction_var->lower_bounds()) {
      induction_var->phi()->InsertInput(
          graph()->zone(), induction_var->phi()->InputCount() - 1, bound.bound);
    }
    for (auto bound : induction_var->upper_bounds()) {
      induction_var->phi()->InsertInput(
          graph()->zone(), induction_var->phi()->InputCount() - 1, bound.bound);
    }
    NodeProperties::ChangeOp(
        induction_var->phi(),
        common()->InductionVariablePhi(induction_var->phi()->InputCount() - 1));
  }
}

void LoopVariableOptimizer::ChangeToPhisAndInsertGuards() {
  for (auto entry : induction_vars_) {
    InductionVariable* induction_var = entry.second;
    if (induction_var->phi()->opcode() == IrOpcode::kInductionVariablePhi) {
      // Turn the induction variable phi back to normal phi.
      int value_count = 2;
      Node* control = NodeProperties::GetControlInput(induction_var->phi());
      DCHECK_EQ(value_count, control->op()->ControlInputCount());
      induction_var->phi()->TrimInputCount(value_count + 1);
      induction_var->phi()->ReplaceInput(value_count, control);
      NodeProperties::ChangeOp(
          induction_var->phi(),
          common()->Phi(MachineRepresentation::kTagged, value_count));

      // If the backedge is not a subtype of the phi's type, we insert a sigma
      // to get the typing right.
      Node* backedge_value = induction_var->phi()->InputAt(1);
      Type* backedge_type = NodeProperties::GetType(backedge_value);
      Type* phi_type = NodeProperties::GetType(induction_var->phi());
      if (!backedge_type->Is(phi_type)) {
        Node* loop = NodeProperties::GetControlInput(induction_var->phi());
        Node* backedge_control = loop->InputAt(1);
        Node* backedge_effect =
            NodeProperties::GetEffectInput(induction_var->effect_phi(), 1);
        Node* rename =
            graph()->NewNode(common()->TypeGuard(phi_type), backedge_value,
                             backedge_effect, backedge_control);
        induction_var->effect_phi()->ReplaceInput(1, rename);
        induction_var->phi()->ReplaceInput(1, rename);
      }
    }
  }
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
