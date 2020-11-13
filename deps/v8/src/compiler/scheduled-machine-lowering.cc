// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/scheduled-machine-lowering.h"

#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/schedule.h"

namespace v8 {
namespace internal {
namespace compiler {

ScheduledMachineLowering::ScheduledMachineLowering(
    JSGraph* js_graph, Schedule* schedule, Zone* temp_zone,
    SourcePositionTable* source_positions, NodeOriginTable* node_origins,
    PoisoningMitigationLevel poison_level)
    : schedule_(schedule),
      graph_assembler_(js_graph, temp_zone, base::nullopt, schedule),
      select_lowering_(&graph_assembler_, js_graph->graph()),
      memory_lowering_(js_graph, temp_zone, &graph_assembler_, poison_level),
      reducers_({&select_lowering_, &memory_lowering_}, temp_zone),
      source_positions_(source_positions),
      node_origins_(node_origins) {}

void ScheduledMachineLowering::Run() {
  // TODO(rmcilroy) We should not depend on having rpo_order on schedule, and
  // instead just do our own RPO walk here.
  for (BasicBlock* block : *(schedule()->rpo_order())) {
    BasicBlock::iterator instr = block->begin();
    BasicBlock::iterator end_instr = block->end();
    gasm()->Reset(block);

    for (; instr != end_instr; instr++) {
      Node* node = *instr;
      Reduction reduction;
      for (auto reducer : reducers_) {
        reduction = reducer->Reduce(node);
        if (reduction.Changed()) break;
      }
      if (reduction.Changed()) {
        Node* replacement = reduction.replacement();
        if (replacement != node) {
          // Replace all uses of node and kill the node to make sure we don't
          // leave dangling dead uses.
          NodeProperties::ReplaceUses(node, replacement, gasm()->effect(),
                                      gasm()->control());
          node->Kill();
        } else {
          gasm()->AddNode(replacement);
        }
      } else {
        gasm()->AddNode(node);
      }
    }

    gasm()->FinalizeCurrentBlock(block);
  }

  schedule_->rpo_order()->clear();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
