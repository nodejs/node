// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-post-hoc-optimizations-processors.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;

// TODO(victorgomes): Add more verification.
class MaglevGraphVerifier {
 public:
  explicit MaglevGraphVerifier(MaglevCompilationInfo* compilation_info,
                               bool verify_sweepable_dead_nodes = true)
      : seen_(compilation_info->zone()),
        defined_(compilation_info->zone()),
        def_block_(compilation_info->zone()),
        idom_(compilation_info->zone()),
        rpo_number_(compilation_info->zone()),
        verify_sweepable_dead_nodes_(verify_sweepable_dead_nodes) {
    if (compilation_info->has_graph_labeller()) {
      graph_labeller_ = compilation_info->graph_labeller();
    }
  }

  void PreProcessGraph(Graph* graph) {
    seen_.resize(graph->max_block_id());
    if (v8_flags.maglev_verify_dominance) {
      rpo_number_.assign(graph->max_block_id(), -1);
      idom_.assign(graph->max_block_id(), nullptr);
      ComputeDominatorsAndDefBlocks(graph);
    }
  }
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    // Check Ids are unique.
    CHECK(!seen_[block->id()]);
    seen_[block->id()] = true;
    current_block_ = block;
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}

  ProcessResult Process(Dead* node, const ProcessingState& state) {
    node->VerifyInputs();
    if (v8_flags.maglev_verify_dominance) {
      MarkDefined(node);
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if (!verify_sweepable_dead_nodes_ &&
        DeadNodeSweepingProcessor::IsSweepableDeadNode(node)) {
      return ProcessResult::kContinue;
    }
    for (Input input : node->inputs()) {
      Opcode op = input.node()->opcode();
      CHECK_GE(op, kFirstOpcode);
      CHECK_LE(op, kLastOpcode);
    }
    node->VerifyInputs();
    if (v8_flags.maglev_verify_dominance) {
      VerifyInputDominance(node);
      VerifyEagerDeoptFrameDominance(node);
      VerifyLazyDeoptFrameDominance(node);
      MarkDefined(node);
    }
    return ProcessResult::kContinue;
  }

 private:
  // Immediate dominators (Cooper-Harvey-Kennedy) and each value's defining
  // block, so the dominance checks below can ask real dominance questions.
  // TODO(victorgomes): Consider merging this with Turboshaft's dominator-tree
  // computation (DominatorForwardTreeNode in compiler/turboshaft/graph.h).
  void ComputeDominatorsAndDefBlocks(Graph* graph) {
    auto& blocks = graph->blocks();
    const int block_count = static_cast<int>(blocks.size());
    for (int i = 0; i < block_count; ++i) {
      BasicBlock* block = blocks[i];
      rpo_number_[block->id()] = i;
      if (block->has_phi()) {
        for (Phi* phi : *block->phis()) def_block_[phi] = block;
      }
      for (Node* node : block->nodes()) {
        if (node == nullptr) continue;
        if (ValueNode* value = node->TryCast<ValueNode>()) {
          def_block_[value] = block;
        }
      }
    }
    if (block_count == 0) return;
    BasicBlock* entry = blocks[0];
    // The entry is its own immediate dominator: a self-reference marks the root
    // of the dominator tree, while nullptr marks an unreachable block.
    idom_[entry->id()] = entry;
    bool changed = true;
    while (changed) {
      changed = false;
      for (int i = 1; i < block_count; ++i) {
        BasicBlock* block = blocks[i];
        BasicBlock* new_idom = nullptr;
        block->ForEachPredecessor([&](BasicBlock* pred) {
          if (pred == nullptr) return;
          if (idom_[pred->id()] == nullptr) return;  // not yet processed
          new_idom = new_idom == nullptr ? pred : Intersect(pred, new_idom);
        });
        if (new_idom == nullptr) continue;
        if (idom_[block->id()] != new_idom) {
          idom_[block->id()] = new_idom;
          changed = true;
        }
      }
    }
  }

  BasicBlock* Intersect(BasicBlock* a, BasicBlock* b) {
    while (a != b) {
      while (rpo_number_[a->id()] > rpo_number_[b->id()]) a = idom_[a->id()];
      while (rpo_number_[b->id()] > rpo_number_[a->id()]) b = idom_[b->id()];
    }
    return a;
  }

  // Whether `def_block` dominates `use_block` (a block dominates itself).
  bool BlockDominates(BasicBlock* def_block, BasicBlock* use_block) {
    if (def_block == use_block) return true;
    // A use in an unreachable block never executes, so don't flag it. Reachable
    // blocks only have reachable dominators, so the walk never reads a null.
    if (idom_[use_block->id()] == nullptr) return true;
    return Intersect(def_block, use_block) == def_block;
  }

  // Some nodes don't belong to a given block (eg, Constants, Parameters...) and
  // so it's always valid to use them even though they aren't defined. All other
  // nodes should be defined before they are used.
  bool RequiresDefinition(ValueNode* node) {
    Opcode opcode = node->opcode();
    if (IsConstantNode(opcode)) return false;
    if (opcode == Opcode::kInitialValue) return false;
    if (opcode == Opcode::kDeadValue) return false;
    return true;
  }

  // Whether `value` dominates a use in `use_block`.
  bool ValueDominatesUse(ValueNode* value, BasicBlock* use_block) {
    auto def_it = def_block_.find(value);
    if (def_it == def_block_.end()) {
      return !RequiresDefinition(value);
    }
    BasicBlock* def_block = def_it->second;
    // Same block: the definition must come before the use (tracked as we go).
    if (def_block == use_block) return defined_.contains(value);
    return BlockDominates(def_block, use_block);
  }

  // Block-level dominance for phi inputs, which read the live-out value of a
  // predecessor and so ignore intra-block ordering.
  bool ValueBlockDominatesUse(ValueNode* value, BasicBlock* use_block) {
    auto def_it = def_block_.find(value);
    if (def_it == def_block_.end()) {
      return !RequiresDefinition(value);
    }
    return BlockDominates(def_it->second, use_block);
  }

  // Every value input must dominate its use: a phi input must dominate its
  // predecessor block, every other input must dominate the node.
  void VerifyInputDominance(NodeBase* node) {
    if (Phi* phi = node->TryCast<Phi>()) {
      for (int i = 0; i < phi->input_count(); ++i) {
        ValueNode* value = phi->input(i).node();
        if (ValueBlockDominatesUse(value, phi->predecessor_at(i))) continue;
        FATAL(
            "Maglev verification: input %d (a %s) of a Phi does not dominate "
            "its predecessor (dominance violation)",
            i, OpcodeToString(value->opcode()));
      }
      return;
    }
    for (Input input : node->inputs()) {
      ValueNode* value = input.node();
      if (ValueDominatesUse(value, current_block_)) continue;
      FATAL(
          "Maglev verification: a %s input of a %s does not dominate its use "
          "(dominance violation)",
          OpcodeToString(value->opcode()), OpcodeToString(node->opcode()));
    }
  }

  // Every value in a node's eager deopt frame must dominate the node.
  void VerifyEagerDeoptFrameDominance(NodeBase* node) {
    if (!node->properties().has_eager_deopt_info()) return;
    node->eager_deopt_info()->ForEachInput([&](ValueNode*& slot) {
      if (slot == nullptr) return;
      if (ValueDominatesUse(slot, current_block_)) return;
      FATAL(
          "Maglev verification: a %s in the eager deopt frame of a %s does not "
          "dominate its use (deopt-frame dominance violation)",
          OpcodeToString(slot->opcode()), OpcodeToString(node->opcode()));
    });
  }

  // Every value in a node's lazy deopt frame (except the node's own result)
  // must dominate the node.
  void VerifyLazyDeoptFrameDominance(NodeBase* node) {
    if (!node->properties().can_lazy_deopt()) return;
    ValueNode* value_node = node->TryCast<ValueNode>();
    node->lazy_deopt_info()->ForEachInput([&](ValueNode*& slot) {
      if (slot == nullptr) return;
      // Skip the node's own result value in the lazy deopt frame.
      if (value_node != nullptr && slot == value_node) return;
      if (ValueDominatesUse(slot, current_block_)) return;
      FATAL(
          "Maglev verification: a %s in the lazy deopt frame of a %s does not "
          "dominate its use (deopt-frame dominance violation)",
          OpcodeToString(slot->opcode()), OpcodeToString(node->opcode()));
    });
  }

  void MarkDefined(NodeBase* node) {
    if (ValueNode* value = node->TryCast<ValueNode>()) {
      defined_.insert(value);
    }
  }

  MaglevGraphLabeller* graph_labeller_ = nullptr;
  ZoneVector<bool> seen_;
  ZoneUnorderedSet<ValueNode*> defined_;
  ZoneUnorderedMap<ValueNode*, BasicBlock*> def_block_;
  ZoneVector<BasicBlock*> idom_;
  ZoneVector<int> rpo_number_;
  BasicBlock* current_block_ = nullptr;
  // AnyUseMarkingProcessor can leave the graph in an inconsistent state where
  // some dead nodes are still in the graph but their dead inputs aren't. We
  // just relax the "domination checks" part of the verification in that case.
  bool verify_sweepable_dead_nodes_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
