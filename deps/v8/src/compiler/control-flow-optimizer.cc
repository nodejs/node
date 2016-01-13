// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-flow-optimizer.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

ControlFlowOptimizer::ControlFlowOptimizer(Graph* graph,
                                           CommonOperatorBuilder* common,
                                           MachineOperatorBuilder* machine,
                                           Zone* zone)
    : graph_(graph),
      common_(common),
      machine_(machine),
      queue_(zone),
      queued_(graph, 2),
      zone_(zone) {}


void ControlFlowOptimizer::Optimize() {
  Enqueue(graph()->start());
  while (!queue_.empty()) {
    Node* node = queue_.front();
    queue_.pop();
    if (node->IsDead()) continue;
    switch (node->opcode()) {
      case IrOpcode::kBranch:
        VisitBranch(node);
        break;
      default:
        VisitNode(node);
        break;
    }
  }
}


void ControlFlowOptimizer::Enqueue(Node* node) {
  DCHECK_NOT_NULL(node);
  if (node->IsDead() || queued_.Get(node)) return;
  queued_.Set(node, true);
  queue_.push(node);
}


void ControlFlowOptimizer::VisitNode(Node* node) {
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      Enqueue(edge.from());
    }
  }
}


void ControlFlowOptimizer::VisitBranch(Node* node) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());
  if (TryBuildSwitch(node)) return;
  if (TryCloneBranch(node)) return;
  VisitNode(node);
}


bool ControlFlowOptimizer::TryCloneBranch(Node* node) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());

  // This optimization is a special case of (super)block cloning. It takes an
  // input graph as shown below and clones the Branch node for every predecessor
  // to the Merge, essentially removing the Merge completely. This avoids
  // materializing the bit for the Phi and may offer potential for further
  // branch folding optimizations (i.e. because one or more inputs to the Phi is
  // a constant). Note that there may be more Phi nodes hanging off the Merge,
  // but we can only a certain subset of them currently (actually only Phi and
  // EffectPhi nodes whose uses have either the IfTrue or IfFalse as control
  // input).

  //   Control1 ... ControlN
  //      ^            ^
  //      |            |   Cond1 ... CondN
  //      +----+  +----+     ^         ^
  //           |  |          |         |
  //           |  |     +----+         |
  //          Merge<--+ | +------------+
  //            ^      \|/
  //            |      Phi
  //            |       |
  //          Branch----+
  //            ^
  //            |
  //      +-----+-----+
  //      |           |
  //    IfTrue     IfFalse
  //      ^           ^
  //      |           |

  // The resulting graph (modulo the Phi and EffectPhi nodes) looks like this:

  // Control1 Cond1 ... ControlN CondN
  //    ^      ^           ^      ^
  //    \      /           \      /
  //     Branch     ...     Branch
  //       ^                  ^
  //       |                  |
  //   +---+---+          +---+----+
  //   |       |          |        |
  // IfTrue IfFalse ... IfTrue  IfFalse
  //   ^       ^          ^        ^
  //   |       |          |        |
  //   +--+ +-------------+        |
  //      | |  +--------------+ +--+
  //      | |                 | |
  //     Merge               Merge
  //       ^                   ^
  //       |                   |

  Node* branch = node;
  Node* cond = NodeProperties::GetValueInput(branch, 0);
  if (!cond->OwnedBy(branch) || cond->opcode() != IrOpcode::kPhi) return false;
  Node* merge = NodeProperties::GetControlInput(branch);
  if (merge->opcode() != IrOpcode::kMerge ||
      NodeProperties::GetControlInput(cond) != merge) {
    return false;
  }
  // Grab the IfTrue/IfFalse projections of the Branch.
  BranchMatcher matcher(branch);
  // Check/collect other Phi/EffectPhi nodes hanging off the Merge.
  NodeVector phis(zone());
  for (Node* const use : merge->uses()) {
    if (use == branch || use == cond) continue;
    // We cannot currently deal with non-Phi/EffectPhi nodes hanging off the
    // Merge. Ideally, we would just clone the nodes (and everything that
    // depends on it to some distant join point), but that requires knowledge
    // about dominance/post-dominance.
    if (!NodeProperties::IsPhi(use)) return false;
    for (Edge edge : use->use_edges()) {
      // Right now we can only handle Phi/EffectPhi nodes whose uses are
      // directly control-dependend on either the IfTrue or the IfFalse
      // successor, because we know exactly how to update those uses.
      // TODO(turbofan): Generalize this to all Phi/EffectPhi nodes using
      // dominance/post-dominance on the sea of nodes.
      if (edge.from()->op()->ControlInputCount() != 1) return false;
      Node* control = NodeProperties::GetControlInput(edge.from());
      if (NodeProperties::IsPhi(edge.from())) {
        control = NodeProperties::GetControlInput(control, edge.index());
      }
      if (control != matcher.IfTrue() && control != matcher.IfFalse())
        return false;
    }
    phis.push_back(use);
  }
  BranchHint const hint = BranchHintOf(branch->op());
  int const input_count = merge->op()->ControlInputCount();
  DCHECK_LE(1, input_count);
  Node** const inputs = zone()->NewArray<Node*>(2 * input_count);
  Node** const merge_true_inputs = &inputs[0];
  Node** const merge_false_inputs = &inputs[input_count];
  for (int index = 0; index < input_count; ++index) {
    Node* cond1 = NodeProperties::GetValueInput(cond, index);
    Node* control1 = NodeProperties::GetControlInput(merge, index);
    Node* branch1 = graph()->NewNode(common()->Branch(hint), cond1, control1);
    merge_true_inputs[index] = graph()->NewNode(common()->IfTrue(), branch1);
    merge_false_inputs[index] = graph()->NewNode(common()->IfFalse(), branch1);
    Enqueue(branch1);
  }
  Node* const merge_true = graph()->NewNode(common()->Merge(input_count),
                                            input_count, merge_true_inputs);
  Node* const merge_false = graph()->NewNode(common()->Merge(input_count),
                                             input_count, merge_false_inputs);
  for (Node* const phi : phis) {
    for (int index = 0; index < input_count; ++index) {
      inputs[index] = phi->InputAt(index);
    }
    inputs[input_count] = merge_true;
    Node* phi_true = graph()->NewNode(phi->op(), input_count + 1, inputs);
    inputs[input_count] = merge_false;
    Node* phi_false = graph()->NewNode(phi->op(), input_count + 1, inputs);
    for (Edge edge : phi->use_edges()) {
      Node* control = NodeProperties::GetControlInput(edge.from());
      if (NodeProperties::IsPhi(edge.from())) {
        control = NodeProperties::GetControlInput(control, edge.index());
      }
      DCHECK(control == matcher.IfTrue() || control == matcher.IfFalse());
      edge.UpdateTo((control == matcher.IfTrue()) ? phi_true : phi_false);
    }
    phi->Kill();
  }
  // Fix up IfTrue and IfFalse and kill all dead nodes.
  matcher.IfFalse()->ReplaceUses(merge_false);
  matcher.IfTrue()->ReplaceUses(merge_true);
  matcher.IfFalse()->Kill();
  matcher.IfTrue()->Kill();
  branch->Kill();
  cond->Kill();
  merge->Kill();
  return true;
}


bool ControlFlowOptimizer::TryBuildSwitch(Node* node) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());

  Node* branch = node;
  if (BranchHintOf(branch->op()) != BranchHint::kNone) return false;
  Node* cond = NodeProperties::GetValueInput(branch, 0);
  if (cond->opcode() != IrOpcode::kWord32Equal) return false;
  Int32BinopMatcher m(cond);
  Node* index = m.left().node();
  if (!m.right().HasValue()) return false;
  int32_t value = m.right().Value();
  ZoneSet<int32_t> values(zone());
  values.insert(value);

  Node* if_false;
  Node* if_true;
  while (true) {
    BranchMatcher matcher(branch);
    DCHECK(matcher.Matched());

    if_true = matcher.IfTrue();
    if_false = matcher.IfFalse();

    auto it = if_false->uses().begin();
    if (it == if_false->uses().end()) break;
    Node* branch1 = *it++;
    if (branch1->opcode() != IrOpcode::kBranch) break;
    if (BranchHintOf(branch1->op()) != BranchHint::kNone) break;
    if (it != if_false->uses().end()) break;
    Node* cond1 = branch1->InputAt(0);
    if (cond1->opcode() != IrOpcode::kWord32Equal) break;
    Int32BinopMatcher m1(cond1);
    if (m1.left().node() != index) break;
    if (!m1.right().HasValue()) break;
    int32_t value1 = m1.right().Value();
    if (values.find(value1) != values.end()) break;
    DCHECK_NE(value, value1);

    if (branch != node) {
      branch->NullAllInputs();
      if_true->ReplaceInput(0, node);
    }
    NodeProperties::ChangeOp(if_true, common()->IfValue(value));
    if_false->NullAllInputs();
    Enqueue(if_true);

    branch = branch1;
    value = value1;
    values.insert(value);
  }

  DCHECK_EQ(IrOpcode::kBranch, node->opcode());
  DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
  if (branch == node) {
    DCHECK_EQ(1u, values.size());
    return false;
  }
  DCHECK_LT(1u, values.size());
  node->ReplaceInput(0, index);
  NodeProperties::ChangeOp(node, common()->Switch(values.size() + 1));
  if_true->ReplaceInput(0, node);
  NodeProperties::ChangeOp(if_true, common()->IfValue(value));
  Enqueue(if_true);
  if_false->ReplaceInput(0, node);
  NodeProperties::ChangeOp(if_false, common()->IfDefault());
  Enqueue(if_false);
  branch->NullAllInputs();
  return true;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
