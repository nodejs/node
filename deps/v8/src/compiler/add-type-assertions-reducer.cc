// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/add-type-assertions-reducer.h"

#include "src/compiler/node-properties.h"
#include "src/compiler/schedule.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
struct AddTypeAssertionsImpl {
  JSGraph* jsgraph;
  Schedule* schedule;
  Zone* phase_zone;

  SimplifiedOperatorBuilder* simplified = jsgraph->simplified();
  Graph* graph = jsgraph->graph();

  void Run();
  void ProcessBlock(BasicBlock* block);
  void InsertAssertion(Node* asserted, Node* effect_successor);
};

void AddTypeAssertionsImpl::Run() {
  for (BasicBlock* block : *(schedule->rpo_order())) {
    ProcessBlock(block);
  }
}

void AddTypeAssertionsImpl::ProcessBlock(BasicBlock* block) {
  // To keep things simple, this only inserts type assertions for nodes that are
  // followed by an effectful operation in the same basic block. We could build
  // a proper new effect chain like in the EffectControlLinearizer, but right
  // now, this doesn't quite seem worth the effort.
  std::vector<Node*> pending;
  bool inside_of_region = false;
  for (Node* node : *block) {
    if (node->opcode() == IrOpcode::kBeginRegion) {
      inside_of_region = true;
    } else if (inside_of_region) {
      if (node->opcode() == IrOpcode::kFinishRegion) {
        inside_of_region = false;
      }
      continue;
    }
    if (node->op()->EffectOutputCount() == 1 &&
        node->op()->EffectInputCount() == 1) {
      for (Node* pending_node : pending) {
        InsertAssertion(pending_node, node);
      }
      pending.clear();
    }
    if (node->opcode() == IrOpcode::kAssertType ||
        node->opcode() == IrOpcode::kAllocate ||
        node->opcode() == IrOpcode::kObjectState ||
        node->opcode() == IrOpcode::kObjectId ||
        node->opcode() == IrOpcode::kPhi || !NodeProperties::IsTyped(node) ||
        node->opcode() == IrOpcode::kUnreachable) {
      continue;
    }
    Type type = NodeProperties::GetType(node);
    if (type.CanBeAsserted()) {
      pending.push_back(node);
    }
  }
}

void AddTypeAssertionsImpl::InsertAssertion(Node* asserted,
                                            Node* effect_successor) {
  Node* assertion = graph->NewNode(
      simplified->AssertType(NodeProperties::GetType(asserted)), asserted,
      NodeProperties::GetEffectInput(effect_successor));
  NodeProperties::ReplaceEffectInput(effect_successor, assertion);
}

}  // namespace

void AddTypeAssertions(JSGraph* jsgraph, Schedule* schedule, Zone* phase_zone) {
  AddTypeAssertionsImpl{jsgraph, schedule, phase_zone}.Run();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
