// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-flow-optimizer.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

ControlFlowOptimizer::ControlFlowOptimizer(JSGraph* jsgraph, Zone* zone)
    : jsgraph_(jsgraph),
      queue_(zone),
      queued_(jsgraph->graph(), 2),
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
  for (Node* use : node->uses()) {
    if (NodeProperties::IsControl(use)) Enqueue(use);
  }
}


void ControlFlowOptimizer::VisitBranch(Node* node) {
  DCHECK_EQ(IrOpcode::kBranch, node->opcode());

  Node* branch = node;
  Node* cond = NodeProperties::GetValueInput(branch, 0);
  if (cond->opcode() != IrOpcode::kWord32Equal) return VisitNode(node);
  Int32BinopMatcher m(cond);
  Node* index = m.left().node();
  if (!m.right().HasValue()) return VisitNode(node);
  int32_t value = m.right().Value();
  ZoneSet<int32_t> values(zone());
  values.insert(value);

  Node* if_false;
  Node* if_true;
  while (true) {
    Node* control_projections[2];
    NodeProperties::CollectControlProjections(branch, control_projections, 2);
    if_true = control_projections[0];
    if_false = control_projections[1];
    DCHECK_EQ(IrOpcode::kIfTrue, if_true->opcode());
    DCHECK_EQ(IrOpcode::kIfFalse, if_false->opcode());

    auto it = if_false->uses().begin();
    if (it == if_false->uses().end()) break;
    Node* branch1 = *it++;
    if (branch1->opcode() != IrOpcode::kBranch) break;
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
      branch->RemoveAllInputs();
      if_true->ReplaceInput(0, node);
    }
    if_true->set_op(common()->IfValue(value));
    if_false->RemoveAllInputs();
    Enqueue(if_true);

    branch = branch1;
    value = value1;
    values.insert(value);
  }

  DCHECK_EQ(IrOpcode::kBranch, node->opcode());
  DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
  DCHECK_EQ(IrOpcode::kIfTrue, if_true->opcode());
  DCHECK_EQ(IrOpcode::kIfFalse, if_false->opcode());
  if (branch == node) {
    DCHECK_EQ(1u, values.size());
    Enqueue(if_true);
    Enqueue(if_false);
  } else {
    DCHECK_LT(1u, values.size());
    node->set_op(common()->Switch(values.size() + 1));
    node->ReplaceInput(0, index);
    if_true->set_op(common()->IfValue(value));
    if_true->ReplaceInput(0, node);
    Enqueue(if_true);
    if_false->set_op(common()->IfDefault());
    if_false->ReplaceInput(0, node);
    Enqueue(if_false);
    branch->RemoveAllInputs();
  }
}


CommonOperatorBuilder* ControlFlowOptimizer::common() const {
  return jsgraph()->common();
}


Graph* ControlFlowOptimizer::graph() const { return jsgraph()->graph(); }


MachineOperatorBuilder* ControlFlowOptimizer::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
