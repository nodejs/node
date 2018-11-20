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
  VisitNode(node);
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
  int32_t order = 1;
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
    NodeProperties::ChangeOp(if_true, common()->IfValue(value, order++));
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
  NodeProperties::ChangeOp(if_true, common()->IfValue(value, order++));
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
