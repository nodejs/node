// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_

#include "src/compiler/bytecode-analysis.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

// The GraphProcessor takes a NodeProcessor, and applies it to each Node in the
// Graph by calling NodeProcessor::Process on each Node.
//
// The GraphProcessor also keeps track of the current ProcessingState, including
// the inferred corresponding InterpreterFrameState and (optionally) the state
// at the most recent Checkpoint, and passes this to the Process method.
//
// It expects a NodeProcessor class with:
//
//   // True if the GraphProcessor should snapshot Checkpoint states for
//   // deopting nodes.
//   static constexpr bool kNeedsCheckpointStates;
//
//   // A function that processes the graph before the nodes are walked.
//   void PreProcessGraph(MaglevCompilationUnit*, Graph* graph);
//
//   // A function that processes the graph after the nodes are walked.
//   void PostProcessGraph(MaglevCompilationUnit*, Graph* graph);
//
//   // A function that processes each basic block before its nodes are walked.
//   void PreProcessBasicBlock(MaglevCompilationUnit*, BasicBlock* block);
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

class ProcessingState {
 public:
  explicit ProcessingState(MaglevCompilationUnit* compilation_unit,
                           BlockConstIterator block_it,
                           const InterpreterFrameState* interpreter_frame_state,
                           const Checkpoint* checkpoint,
                           const InterpreterFrameState* checkpoint_frame_state)
      : compilation_unit_(compilation_unit),
        block_it_(block_it),
        interpreter_frame_state_(interpreter_frame_state),
        checkpoint_(checkpoint),
        checkpoint_frame_state_(checkpoint_frame_state) {}

  // Disallow copies, since the underlying frame states stay mutable.
  ProcessingState(const ProcessingState&) = delete;
  ProcessingState& operator=(const ProcessingState&) = delete;

  BasicBlock* block() const { return *block_it_; }
  BasicBlock* next_block() const { return *(block_it_ + 1); }

  const InterpreterFrameState* interpreter_frame_state() const {
    DCHECK_NOT_NULL(interpreter_frame_state_);
    return interpreter_frame_state_;
  }

  const Checkpoint* checkpoint() const {
    DCHECK_NOT_NULL(checkpoint_);
    return checkpoint_;
  }

  const InterpreterFrameState* checkpoint_frame_state() const {
    DCHECK_NOT_NULL(checkpoint_frame_state_);
    return checkpoint_frame_state_;
  }

  int register_count() const { return compilation_unit_->register_count(); }
  int parameter_count() const { return compilation_unit_->parameter_count(); }

  MaglevGraphLabeller* graph_labeller() const {
    return compilation_unit_->graph_labeller();
  }

 private:
  MaglevCompilationUnit* compilation_unit_;
  BlockConstIterator block_it_;
  const InterpreterFrameState* interpreter_frame_state_;
  const Checkpoint* checkpoint_;
  const InterpreterFrameState* checkpoint_frame_state_;
};

template <typename NodeProcessor>
class GraphProcessor {
 public:
  static constexpr bool kNeedsCheckpointStates =
      NodeProcessor::kNeedsCheckpointStates;

  template <typename... Args>
  explicit GraphProcessor(MaglevCompilationUnit* compilation_unit,
                          Args&&... args)
      : compilation_unit_(compilation_unit),
        node_processor_(std::forward<Args>(args)...),
        current_frame_state_(*compilation_unit_) {
    if (kNeedsCheckpointStates) {
      checkpoint_state_.emplace(*compilation_unit_);
    }
  }

  void ProcessGraph(Graph* graph) {
    graph_ = graph;

    node_processor_.PreProcessGraph(compilation_unit_, graph);

    for (block_it_ = graph->begin(); block_it_ != graph->end(); ++block_it_) {
      BasicBlock* block = *block_it_;

      node_processor_.PreProcessBasicBlock(compilation_unit_, block);

      if (block->has_state()) {
        current_frame_state_.CopyFrom(*compilation_unit_, *block->state());
        if (kNeedsCheckpointStates) {
          checkpoint_state_->last_checkpoint_block_it = block_it_;
          checkpoint_state_->last_checkpoint_node_it = NodeConstIterator();
        }
      }

      if (block->has_phi()) {
        for (Phi* phi : *block->phis()) {
          node_processor_.Process(phi, GetCurrentState());
        }
      }

      for (node_it_ = block->nodes().begin(); node_it_ != block->nodes().end();
           ++node_it_) {
        Node* node = *node_it_;
        ProcessNodeBase(node, GetCurrentState());
      }

      ProcessNodeBase(block->control_node(), GetCurrentState());
    }

    node_processor_.PostProcessGraph(compilation_unit_, graph);
  }

  NodeProcessor& node_processor() { return node_processor_; }
  const NodeProcessor& node_processor() const { return node_processor_; }

 private:
  ProcessingState GetCurrentState() {
    return ProcessingState(
        compilation_unit_, block_it_, &current_frame_state_,
        kNeedsCheckpointStates ? checkpoint_state_->latest_checkpoint : nullptr,
        kNeedsCheckpointStates ? &checkpoint_state_->checkpoint_frame_state
                               : nullptr);
  }

  void ProcessNodeBase(NodeBase* node, const ProcessingState& state) {
    switch (node->opcode()) {
#define CASE(OPCODE)                                      \
  case Opcode::k##OPCODE:                                 \
    PreProcess(node->Cast<OPCODE>(), state);              \
    node_processor_.Process(node->Cast<OPCODE>(), state); \
    break;
      NODE_BASE_LIST(CASE)
#undef CASE
    }
  }

  void PreProcess(NodeBase* node, const ProcessingState& state) {}

  void PreProcess(Checkpoint* checkpoint, const ProcessingState& state) {
    current_frame_state_.set_accumulator(checkpoint->accumulator());
    if (kNeedsCheckpointStates) {
      checkpoint_state_->latest_checkpoint = checkpoint;
      if (checkpoint->is_used()) {
        checkpoint_state_->checkpoint_frame_state.CopyFrom(
            *compilation_unit_, current_frame_state_);
        checkpoint_state_->last_checkpoint_block_it = block_it_;
        checkpoint_state_->last_checkpoint_node_it = node_it_;
        ClearDeadCheckpointNodes();
      }
    }
  }

  void PreProcess(StoreToFrame* store_to_frame, const ProcessingState& state) {
    current_frame_state_.set(store_to_frame->target(), store_to_frame->value());
  }

  void PreProcess(SoftDeopt* node, const ProcessingState& state) {
    PreProcessDeoptingNode();
  }

  void PreProcess(CheckMaps* node, const ProcessingState& state) {
    PreProcessDeoptingNode();
  }

  void PreProcessDeoptingNode() {
    if (!kNeedsCheckpointStates) return;

    Checkpoint* checkpoint = checkpoint_state_->latest_checkpoint;
    if (checkpoint->is_used()) {
      DCHECK(!checkpoint_state_->last_checkpoint_node_it.is_null());
      DCHECK_EQ(checkpoint, *checkpoint_state_->last_checkpoint_node_it);
      return;
    }
    DCHECK_IMPLIES(!checkpoint_state_->last_checkpoint_node_it.is_null(),
                   checkpoint != *checkpoint_state_->last_checkpoint_node_it);

    // TODO(leszeks): The following code is _ugly_, should figure out how to
    // clean it up.

    // Go to the previous state checkpoint (either on the Checkpoint that
    // provided the current checkpoint snapshot, or on a BasicBlock).
    BlockConstIterator block_it = checkpoint_state_->last_checkpoint_block_it;
    NodeConstIterator node_it = checkpoint_state_->last_checkpoint_node_it;
    if (node_it.is_null()) {
      // There was no recent enough Checkpoint node, and the block iterator
      // points at a basic block with a state snapshot. Copy that snapshot and
      // start iterating from there.
      BasicBlock* block = *block_it;
      DCHECK(block->has_state());
      checkpoint_state_->checkpoint_frame_state.CopyFrom(*compilation_unit_,
                                                         *block->state());

      // Start iterating from the first node in the block.
      node_it = block->nodes().begin();
    } else {
      // The node iterator should point at the previous Checkpoint node. We
      // don't need that Checkpoint state snapshot anymore, we're making a new
      // one, so we can just reuse the snapshot as-is without copying it.
      DCHECK_NE(*node_it, checkpoint);
      DCHECK((*node_it)->Is<Checkpoint>());
      DCHECK((*node_it)->Cast<Checkpoint>()->is_used());

      // Advance it by one since we don't need to check this node anymore.
      ++node_it;
    }

    // Now walk forward to the checkpoint, and apply any StoreToFrame operations
    // along the way into the snapshotted checkpoint state.
    BasicBlock* block = *block_it;
    while (true) {
      // Check if we've run out of nodes in this block, and advance to the
      // next block if so.
      while (node_it == block->nodes().end()) {
        DCHECK_NE(block_it, graph_->end());

        // We should only end up visiting blocks with fallthrough to the next
        // block -- otherwise, the block should have had a frame state snapshot,
        // as either a merge block or a non-fallthrough jump target.
        if ((*block_it)->control_node()->Is<Jump>()) {
          DCHECK_EQ((*block_it)->control_node()->Cast<Jump>()->target(),
                    *(block_it + 1));
        } else {
          DCHECK_IMPLIES((*block_it)
                                 ->control_node()
                                 ->Cast<ConditionalControlNode>()
                                 ->if_true() != *(block_it + 1),
                         (*block_it)
                                 ->control_node()
                                 ->Cast<ConditionalControlNode>()
                                 ->if_false() != *(block_it + 1));
        }

        // Advance to the next block (which the above DCHECKs confirm is the
        // unconditional fallthrough from the previous block), and update the
        // cached block pointer.
        block_it++;
        block = *block_it;

        // We should never visit a block with state (aside from the very first
        // block we visit), since then that should have been our start point
        // to start with.
        DCHECK(!(*block_it)->has_state());
        node_it = (*block_it)->nodes().begin();
      }

      // We should never reach the current node, the "until" checkpoint node
      // should be before it.
      DCHECK_NE(node_it, node_it_);

      Node* node = *node_it;

      // Break once we hit the given Checkpoint node. This could be right at
      // the start of the iteration, if the BasicBlock held the snapshot and the
      // Checkpoint was the first node in it.
      if (node == checkpoint) break;

      // Update the state from the current node, if it's a state update.
      if (node->Is<StoreToFrame>()) {
        StoreToFrame* store_to_frame = node->Cast<StoreToFrame>();
        checkpoint_state_->checkpoint_frame_state.set(store_to_frame->target(),
                                                      store_to_frame->value());
      } else {
        // Any checkpoints we meet along the way should be unused, otherwise
        // they should have provided the most recent state snapshot.
        DCHECK_IMPLIES(node->Is<Checkpoint>(),
                       !node->Cast<Checkpoint>()->is_used());
      }

      // Continue to the next node.
      ++node_it;
    }

    checkpoint_state_->last_checkpoint_block_it = block_it;
    checkpoint_state_->last_checkpoint_node_it = node_it;
    checkpoint_state_->checkpoint_frame_state.set_accumulator(
        checkpoint->accumulator());
    ClearDeadCheckpointNodes();
    checkpoint->SetUsed();
  }

  // Walk the checkpointed state, and null out any values that are dead at this
  // checkpoint.
  // TODO(leszeks): Consider doing this on checkpoint copy, not as a
  // post-process step.
  void ClearDeadCheckpointNodes() {
    const compiler::BytecodeLivenessState* liveness =
        bytecode_analysis().GetInLivenessFor(
            checkpoint_state_->latest_checkpoint->bytecode_position());
    for (int i = 0; i < register_count(); ++i) {
      if (!liveness->RegisterIsLive(i)) {
        checkpoint_state_->checkpoint_frame_state.set(interpreter::Register(i),
                                                      nullptr);
      }
    }

    // The accumulator is on the checkpoint node itself, and should have already
    // been nulled out during graph building if it's dead.
    DCHECK_EQ(
        !liveness->AccumulatorIsLive(),
        checkpoint_state_->checkpoint_frame_state.accumulator() == nullptr);
  }

  int register_count() const { return compilation_unit_->register_count(); }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return compilation_unit_->bytecode_analysis();
  }

  MaglevCompilationUnit* const compilation_unit_;
  NodeProcessor node_processor_;
  Graph* graph_;
  BlockConstIterator block_it_;
  NodeConstIterator node_it_;
  InterpreterFrameState current_frame_state_;

  // The CheckpointState field only exists if the node processor needs
  // checkpoint states.
  struct CheckpointState {
    explicit CheckpointState(const MaglevCompilationUnit& compilation_unit)
        : checkpoint_frame_state(compilation_unit) {}
    Checkpoint* latest_checkpoint = nullptr;
    BlockConstIterator last_checkpoint_block_it;
    NodeConstIterator last_checkpoint_node_it;
    InterpreterFrameState checkpoint_frame_state;
  };
  base::Optional<CheckpointState> checkpoint_state_;
};

// A NodeProcessor that wraps multiple NodeProcessors, and forwards to each of
// them iteratively.
template <typename... Processors>
class NodeMultiProcessor;

template <>
class NodeMultiProcessor<> {
 public:
  static constexpr bool kNeedsCheckpointStates = false;

  void PreProcessGraph(MaglevCompilationUnit*, Graph* graph) {}
  void PostProcessGraph(MaglevCompilationUnit*, Graph* graph) {}
  void PreProcessBasicBlock(MaglevCompilationUnit*, BasicBlock* block) {}
  void Process(NodeBase* node, const ProcessingState& state) {}
};

template <typename Processor, typename... Processors>
class NodeMultiProcessor<Processor, Processors...>
    : NodeMultiProcessor<Processors...> {
  using Base = NodeMultiProcessor<Processors...>;

 public:
  static constexpr bool kNeedsCheckpointStates =
      Processor::kNeedsCheckpointStates || Base::kNeedsCheckpointStates;

  template <typename Node>
  void Process(Node* node, const ProcessingState& state) {
    processor_.Process(node, state);
    Base::Process(node, state);
  }
  void PreProcessGraph(MaglevCompilationUnit* unit, Graph* graph) {
    processor_.PreProcessGraph(unit, graph);
    Base::PreProcessGraph(unit, graph);
  }
  void PostProcessGraph(MaglevCompilationUnit* unit, Graph* graph) {
    // Post process in reverse order because that kind of makes sense.
    Base::PostProcessGraph(unit, graph);
    processor_.PostProcessGraph(unit, graph);
  }
  void PreProcessBasicBlock(MaglevCompilationUnit* unit, BasicBlock* block) {
    processor_.PreProcessBasicBlock(unit, block);
    Base::PreProcessBasicBlock(unit, block);
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
