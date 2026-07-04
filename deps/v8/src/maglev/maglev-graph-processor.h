// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_

#include <algorithm>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

// The GraphProcessor takes a NodeProcessor, and applies it to each Node in the
// Graph by calling NodeProcessor::Process on each Node.
//
// The GraphProcessor also keeps track of the current ProcessingState, and
// passes this to the Process method.
//
// It expects a NodeProcessor class with:
//
//   // A function that processes the graph before the nodes are walked.
//   void PreProcessGraph(Graph* graph);
//
//   // A function that processes the graph after the nodes are walked.
//   void PostProcessGraph(Graph* graph);
//
//   // A function that processes each basic block before its nodes are walked.
//   BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
//
//   // Process methods for each Node type. The GraphProcessor switches over
//   // the Node's opcode, casts it to the appropriate FooNode, and dispatches
//   // to NodeProcessor::Process. It's then up to the NodeProcessor to provide
//   // either distinct Process methods per Node type, or using templates or
//   // overloading as appropriate to group node processing.
//   void Process(FooNode* node, const ProcessingState& state) {}
//
template <typename NodeProcessor>
class GraphProcessor;
template <typename NodeProcessor>
class GraphBackwardProcessor;

enum class BlockProcessResult {
  kContinue,     // Process exited normally.
  kSkip,         // Skip processing this blockand and do not call the following
                 // processors. Only valid for PreProcessBasicBlock.
  kRevisitLoop,  // Revisit the current loop, assuming that the current block
                 // ends with a JumpLoop (no MultiProcessor support, and only
                 // valid for PostProcessBasicBlock)
};

enum class ProcessResult {
  kContinue,  // Process exited normally, and the following processors will be
              // called on the node.
  kRemove,    // Remove the current node from the graph (and do not call the
              // following processors). If the node processor supports splicing
              // and recorded a pending splice during Process(), the visited
              // node is rewritten to Identity-to-spliced-result, the subgraph
              // blocks are stitched in, and tail nodes + original control move
              // to the join block. Only the first node processor can record a
              // splice.
  kRevisit,   // Process this node again. Note that the node is allowed to have
              // changed.
  kTruncateBlock,  // Remove all nodes from this point from the basic block
                   // (including the current node) and do not call the following
                   // processors. If the node processor supports splicing and
                   // recorded a pending splice during Process(), the subgraph
                   // is stitched in before the truncation point (the visited
                   // node and the tail are dropped; the subgraph's exit takes
                   // the original control, which becomes a dead edge).
  kHoist,          // Hoist the current instruction to the parent basic block
                   // and reset the current instruction to the beginning of the
                   // block. Parent block must be dominating.
  kAbort,      // Stop processing now, do not process subsequent nodes/blocks.
               // Should not be used when processing Constants.
  kSkipBlock,  // Stop processing this block and skip the remaining nodes (no
               // MultiProcessor support).
};

class ProcessingState {
 public:
  static constexpr int kNoNodeIndex = -1;

  explicit ProcessingState(Graph* graph, int current_block_index,
                           int node_index = kNoNodeIndex)
      : graph_(graph),
        current_block_index_(current_block_index),
        node_index_(node_index) {
    DCHECK_IMPLIES(node_index != kNoNodeIndex, node_index >= 0);
  }

  // Disallow copies, since the underlying frame states stay mutable.
  ProcessingState(const ProcessingState&) = delete;
  ProcessingState& operator=(const ProcessingState&) = delete;

  BasicBlock* block() const {
    if (current_block_index_ == graph_->num_blocks()) {
      return nullptr;
    }
    return graph_->blocks()[current_block_index_];
  }

  BasicBlock* next_block() const {
    if (current_block_index_ + 1 >= static_cast<int>(graph_->blocks().size())) {
      return nullptr;
    }
    return graph_->blocks()[current_block_index_ + 1];
  }

  int node_index() const {
    DCHECK_GE(node_index_, 0);
    return node_index_;
  }

 private:
  Graph* graph_;
  int current_block_index_;
  const int node_index_;  // Index inside the basic block.
};

template <typename NodeProcessor>
class GraphProcessor {
 public:
  template <typename... Args>
  explicit GraphProcessor(Args&&... args)
      : node_processor_(std::forward<Args>(args)...) {}

  void ProcessGraph(Graph* graph) {
    graph_ = graph;
    // Initializing {current_block_index_} to `graph->num_blocks()` so that
    // the ProcessingState can return nullptr as the block of the constant
    // nodes.
    current_block_index_ = graph->num_blocks();
    node_processor_.PreProcessGraph(graph);

    auto process_constants = [&](auto& map) {
      for (auto it = map.begin(); it != map.end();) {
        ProcessResult result =
            node_processor_.Process(it->second, GetCurrentState(0));
        switch (result) {
          [[likely]] case ProcessResult::kContinue:
            ++it;
            break;
          case ProcessResult::kRevisit:
            break;
          case ProcessResult::kRemove:
            it = map.erase(it);
            break;
          case ProcessResult::kTruncateBlock:
          case ProcessResult::kHoist:
          case ProcessResult::kAbort:
          case ProcessResult::kSkipBlock:
            UNREACHABLE();
        }
      }
    };
    // LINT.IfChange(maglev_constant_nodes)
    process_constants(graph->heap_constants());
    process_constants(graph->root());
    process_constants(graph->smi());
    process_constants(graph->tagged_index());
    process_constants(graph->int32());
    process_constants(graph->uint32());
    process_constants(graph->intptr());
    process_constants(graph->float64());
    process_constants(graph->holey_float64());
    process_constants(graph->heap_number());
    process_constants(graph->trusted_constants());
    // LINT.ThenChange()

    for (current_block_index_ = 0; current_block_index_ < graph->num_blocks();
         current_block_index_++) {
      BasicBlock* block = graph->blocks()[current_block_index_];
      if (V8_UNLIKELY(block->is_dead())) continue;
      bool process_control_block = true;

      BlockProcessResult preprocess_result =
          node_processor_.PreProcessBasicBlock(block);
      switch (preprocess_result) {
        [[likely]] case BlockProcessResult::kContinue:
          break;
        case BlockProcessResult::kSkip:
          continue;
        case BlockProcessResult::kRevisitLoop:
          UNREACHABLE();
      }

      if (block->has_phi()) {
        auto& phis = *block->phis();
        for (auto it = phis.begin(); it != phis.end();) {
          Phi* phi = *it;
          ProcessResult result =
              node_processor_.Process(phi, GetCurrentState());
          switch (result) {
            [[likely]] case ProcessResult::kContinue:
              ++it;
              break;
            case ProcessResult::kRevisit:
              break;
            case ProcessResult::kRemove:
              it = phis.RemoveAt(it);
              break;
            case ProcessResult::kAbort:
              return;
            case ProcessResult::kSkipBlock:
              goto skip_block;
            case ProcessResult::kTruncateBlock:
            case ProcessResult::kHoist:
              UNREACHABLE();
          }
        }
      }

      node_processor_.PostPhiProcessing();

      for (node_it_ = block->nodes().begin();
           node_it_ != block->nodes().end();) {
        Node* node = *node_it_;
        if (node == nullptr) {
          ++node_it_;
          continue;
        }
        ProcessResult result = ProcessNodeBase(
            node, GetCurrentState(node_it_ - block->nodes().begin()));
        if constexpr (SupportsSplice<NodeProcessor>) {
          // A pending splice is only consumed by kRemove / kTruncateBlock;
          // any other result with a pending splice would leak it into the
          // next node and apply it at the wrong site.
          DCHECK_IMPLIES(node_processor_.HasPendingSplice(),
                         result == ProcessResult::kRemove ||
                             result == ProcessResult::kTruncateBlock);
        }
        switch (result) {
          [[likely]] case ProcessResult::kContinue:
            ++node_it_;
            break;
          case ProcessResult::kRevisit:
            break;
          case ProcessResult::kRemove:
            if constexpr (SupportsSplice<NodeProcessor>) {
              if (node_processor_.HasPendingSplice()) {
                ApplyPendingSplice(block, node, /*truncate=*/false);
                break;
              }
            }
            *node_it_ = nullptr;
            ++node_it_;
            break;
          case ProcessResult::kTruncateBlock:
            if constexpr (SupportsSplice<NodeProcessor>) {
              if (node_processor_.HasPendingSplice()) {
                ApplyPendingSplice(block, node, /*truncate=*/true);
                break;
              }
            }
            block->nodes().resize(node_it_ - block->nodes().begin());
            node_it_ = block->nodes().end();
            graph_->set_may_have_unreachable_blocks(true);
            break;
          case ProcessResult::kHoist: {
            DCHECK(block->predecessor_count() == 1 ||
                   (block->predecessor_count() == 2 && block->is_loop()));
            BasicBlock* target = block->predecessor_at(0);
            DCHECK_EQ(target->successors().size(), 1);
            Node* cur = *node_it_;
            cur->set_owner(target);
            *node_it_ = nullptr;
            target->nodes().push_back(cur);
            node_it_ = block->nodes().begin();
            break;
          }
          case ProcessResult::kAbort:
            return;
          case ProcessResult::kSkipBlock:
            goto skip_block;
        }
      }

      while (process_control_block) {
        ProcessResult control_result =
            ProcessNodeBase(block->control_node(), GetCurrentState());
        process_control_block = false;
        switch (control_result) {
          [[likely]] case ProcessResult::kContinue:
            break;
          case ProcessResult::kRevisit:
            process_control_block = true;
            break;
          case ProcessResult::kSkipBlock:
            break;
          case ProcessResult::kAbort:
            return;
          case ProcessResult::kRemove:
          case ProcessResult::kTruncateBlock:
          case ProcessResult::kHoist:
            UNREACHABLE();
        }
      }
    skip_block:
      BlockProcessResult post_action =
          node_processor_.PostProcessBasicBlock(block);
      switch (post_action) {
        case BlockProcessResult::kContinue:
          break;
        case BlockProcessResult::kRevisitLoop: {
          JumpLoop* jump = block->control_node()->Cast<JumpLoop>();
          current_block_index_ = graph->block_offset(jump->target()) - 1;
          break;
        }
        case BlockProcessResult::kSkip:
          UNREACHABLE();
      }
      continue;
    }

    node_processor_.PostProcessGraph(graph);
  }

  NodeProcessor& node_processor() { return node_processor_; }
  const NodeProcessor& node_processor() const { return node_processor_; }

 private:
  ProcessingState GetCurrentState(
      size_t node_index = ProcessingState::kNoNodeIndex) {
    return ProcessingState(graph_, current_block_index_,
                           static_cast<int>(node_index));
  }

  ProcessResult ProcessNodeBase(NodeBase* node, const ProcessingState& state) {
    switch (node->opcode()) {
#define CASE(OPCODE)                                        \
  case Opcode::k##OPCODE:                                   \
    PreProcess(node->Cast<OPCODE>(), state);                \
    return node_processor_.Process(node->Cast<OPCODE>(), state);

      NODE_BASE_LIST(CASE)
#undef CASE
    }
    UNREACHABLE();
  }

  void PreProcess(NodeBase* node, const ProcessingState& state) {}

  // The NodeProcessor supports the SpliceAndContinue protocol.
  template <typename NP>
  static constexpr bool SupportsSplice = requires(NP* np) {
    np->TakePendingSplice();
    np->HasPendingSplice();
  };

  // Splices the subgraph recorded by the processor's reducer into the
  // graph at the visited node. If `truncate` is false, the tail (after the
  // visited node) moves to splice.exit; the visited node itself is expected
  // to have already been rewritten by the processor's visitor (typically
  // to Identity-to-spliced-result). If `truncate` is true, the visited
  // node and tail are dropped (the reduction aborted with partial subgraph
  // nodes already emitted) and the graph is marked as potentially having
  // unreachable blocks.
  void ApplyPendingSplice(BasicBlock* block, Node* node, bool truncate)
    requires SupportsSplice<NodeProcessor>
  {
    auto splice = node_processor_.TakePendingSplice();

    ZoneVector<Node*> tail_nodes(graph_->zone());
    if (truncate) {
      // Drop the visited node and everything after it.
      auto& nodes = block->nodes();
      auto it = std::find(nodes.begin(), nodes.end(), node);
      DCHECK_NE(it, nodes.end());
      nodes.resize(it - nodes.begin());
    } else {
      // The visitor must have rewritten the visited node to an Identity
      // (typically to the spliced result Phi), so uses on the join-block
      // side observe the merged value.
      DCHECK(node->Is<Identity>());
      tail_nodes = block->Split(node, graph_->zone());
    }

    // Redirect `block`'s control flow to the subgraph entry. The original
    // control becomes the exit block's control.
    // TODO(victorgomes): We can avoid creating the new Jump node by appending
    // the splice block entry.
    ControlNode* original_control = block->reset_control_node();
    BasicBlockRef* jump_ref = graph_->zone()->New<BasicBlockRef>();
    Jump* jump_to_entry =
        NodeBase::New<Jump>(graph_->zone(), /*input_count=*/0, jump_ref);
    jump_ref->Bind(splice.entry);
    jump_to_entry->set_owner(block);
    block->set_control_node(jump_to_entry);
    splice.entry->set_predecessor(block);

    if (splice.exit == nullptr) {
      // The subgraph deopts/throws on every path, so there is no join block to
      // reconnect: the original control (and the already-truncated tail) is
      // dead. Unwire its successors from `block`.
      DCHECK(truncate);
      block->RemovePredecessorFollowing(original_control);
    } else {
      // Move tail (if any) + original control to the join block.
      for (Node* n : tail_nodes) {
        if (n == nullptr) continue;
        n->set_owner(splice.exit);
        splice.exit->nodes().push_back(n);
      }
      original_control->set_owner(splice.exit);
      splice.exit->set_control_node(original_control);

      // Successors of the original control had `block` as predecessor;
      // they now have the join block.
      splice.exit->ForEachSuccessor(
          [block, exit = splice.exit](BasicBlock* succ) {
            if (!succ->has_state()) {
              DCHECK_EQ(succ->predecessor(), block);
              succ->set_predecessor(exit);
              return;
            }
            for (int i = 0; i < succ->predecessor_count(); i++) {
              if (succ->predecessor_at(i) == block) {
                succ->state()->set_predecessor_at(i, exit);
                break;
              }
            }
          });
    }

    graph_->AddBlocksAt(splice.all_blocks, current_block_index_);
    node_it_ = block->nodes().end();

    if (truncate) {
      graph_->set_may_have_unreachable_blocks(true);
    }
  }

  NodeProcessor node_processor_;
  Graph* graph_;
  int current_block_index_;
  NodeIterator node_it_;
};

template <typename NodeProcessor>
class GraphBackwardProcessor {
 public:
  template <typename... Args>
  explicit GraphBackwardProcessor(Args&&... args)
      : node_processor_(std::forward<Args>(args)...) {}

  void ProcessGraph(Graph* graph) {
    node_processor_.PreProcessGraph(graph);

    for (BasicBlock* block : base::Reversed(graph->blocks())) {
      {
        ProcessResult control_result = ProcessNodeBase(block->control_node());
        switch (control_result) {
          [[likely]] case ProcessResult::kContinue:
            break;
          case ProcessResult::kAbort:
            return;
          case ProcessResult::kRevisit:
          case ProcessResult::kRemove:
          case ProcessResult::kTruncateBlock:
          case ProcessResult::kHoist:
          case ProcessResult::kSkipBlock:
            UNREACHABLE();
        }
      }

      for (Node* node : base::Reversed(block->nodes())) {
        if (node == nullptr) continue;
        ProcessResult result = ProcessNodeBase(node);
        switch (result) {
          [[likely]] case ProcessResult::kContinue:
            break;
          case ProcessResult::kAbort:
            return;
          case ProcessResult::kRevisit:
          case ProcessResult::kRemove:
          case ProcessResult::kTruncateBlock:
          case ProcessResult::kHoist:
          case ProcessResult::kSkipBlock:
            UNREACHABLE();
        }
      }

      if (block->has_phi()) {
        auto& phis = *block->phis();
        for (auto it = phis.begin(); it != phis.end();) {
          Phi* phi = *it;
          ProcessResult result = node_processor_.Process(phi);
          switch (result) {
            [[likely]] case ProcessResult::kContinue:
              ++it;
              break;
            case ProcessResult::kRemove:
              it = phis.RemoveAt(it);
              break;
            case ProcessResult::kAbort:
              return;
            case ProcessResult::kTruncateBlock:
            case ProcessResult::kRevisit:
            case ProcessResult::kSkipBlock:
            case ProcessResult::kHoist:
              UNREACHABLE();
          }
        }
      }

      node_processor_.PostProcessBasicBlock(block);
    }

    auto process_constants = [&](auto& map) {
      for (auto it = map.begin(); it != map.end();) {
        ProcessResult result = node_processor_.Process(it->second);
        switch (result) {
          [[likely]] case ProcessResult::kContinue:
            ++it;
            break;
          case ProcessResult::kRemove:
            it = map.erase(it);
            break;
          case ProcessResult::kRevisit:
          case ProcessResult::kHoist:
          case ProcessResult::kAbort:
          case ProcessResult::kTruncateBlock:
          case ProcessResult::kSkipBlock:
            UNREACHABLE();
        }
      }
    };
    process_constants(graph->heap_constants());
    process_constants(graph->root());
    process_constants(graph->smi());
    process_constants(graph->tagged_index());
    process_constants(graph->int32());
    process_constants(graph->uint32());
    process_constants(graph->intptr());
    process_constants(graph->float64());
    process_constants(graph->heap_number());
    process_constants(graph->trusted_constants());

    node_processor_.PostProcessGraph(graph);
  }

 private:
  ProcessResult ProcessNodeBase(NodeBase* node) {
    switch (node->opcode()) {
#define CASE(OPCODE)      \
  case Opcode::k##OPCODE: \
    return node_processor_.Process(node->Cast<OPCODE>());
      NODE_BASE_LIST(CASE)
#undef CASE
    }
    UNREACHABLE();
  }

  NodeProcessor node_processor_;
};

// A NodeProcessor that wraps multiple NodeProcessors, and forwards to each of
// them iteratively.
template <typename... Processors>
class NodeMultiProcessor;

template <>
class NodeMultiProcessor<> {
 public:
  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  V8_INLINE ProcessResult Process(NodeBase* node,
                                  const ProcessingState& state) {
    return ProcessResult::kContinue;
  }
  void PostPhiProcessing() {}
};

template <typename Processor, typename... Processors>
class NodeMultiProcessor<Processor, Processors...>
    : NodeMultiProcessor<Processors...> {
  using Base = NodeMultiProcessor<Processors...>;

 public:
  template <typename... Args>
  explicit NodeMultiProcessor(Processor&& processor, Args&&... processors)
      : Base(std::forward<Args>(processors)...),
        processor_(std::forward<Processor>(processor)) {}
  template <typename... Args>
  explicit NodeMultiProcessor(Args&&... processors)
      : Base(std::forward<Args>(processors)...) {}

  template <typename Node>
  ProcessResult Process(Node* node, const ProcessingState& state) {
    auto res = processor_.Process(node, state);
    switch (res) {
      [[likely]] case ProcessResult::kContinue:
        return Base::Process(node, state);
      case ProcessResult::kRevisit:
      case ProcessResult::kAbort:
      case ProcessResult::kRemove:
      case ProcessResult::kTruncateBlock:
        return res;
      case ProcessResult::kHoist:
      case ProcessResult::kSkipBlock:
        // TODO(olivf): How to combine these with multiple processors depends on
        // the needs of the actual processors. Implement once needed.
        UNREACHABLE();
    }
    UNREACHABLE();
  }
  void PreProcessGraph(Graph* graph) {
    processor_.PreProcessGraph(graph);
    Base::PreProcessGraph(graph);
  }
  void PostProcessGraph(Graph* graph) {
    // Post process in reverse order because that kind of makes sense.
    Base::PostProcessGraph(graph);
    processor_.PostProcessGraph(graph);
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    BlockProcessResult post_action = Base::PostProcessBasicBlock(block);
    switch (post_action) {
      [[likely]] case BlockProcessResult::kContinue:
        return processor_.PostProcessBasicBlock(block);
      case BlockProcessResult::kRevisitLoop:
        // TODO(dmercadier): How to combine this with multiple processors
        // depends on the needs of the actual processors. Implement once needed.
        UNREACHABLE();
      case BlockProcessResult::kSkip:
        // The block has already been processed, there is nothing to skip.
        UNREACHABLE();
    }
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    BlockProcessResult res = processor_.PreProcessBasicBlock(block);
    switch (res) {
      [[likely]] case BlockProcessResult::kContinue:
        return Base::PreProcessBasicBlock(block);
      case BlockProcessResult::kSkip:
        return res;
      case BlockProcessResult::kRevisitLoop:
        UNREACHABLE();
    }
    UNREACHABLE();
  }

  // Forward splice plumbing to the head processor when it exposes the
  // take/clear interface. Only the first processor in the chain may record
  // a pending splice (the multi-processor short-circuits on its result).
  template <typename P = Processor>
  auto TakePendingSplice() const
      -> decltype(std::declval<const P&>().TakePendingSplice()) {
    return processor_.TakePendingSplice();
  }
  template <typename P = Processor>
  auto HasPendingSplice() const
      -> decltype(std::declval<const P&>().HasPendingSplice()) {
    return processor_.HasPendingSplice();
  }

  void PostPhiProcessing() {
    processor_.PostPhiProcessing();
    Base::PostPhiProcessing();
  }

 private:
  Processor processor_;
};

template <typename... Processors>
using GraphMultiProcessor = GraphProcessor<NodeMultiProcessor<Processors...>>;

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_
