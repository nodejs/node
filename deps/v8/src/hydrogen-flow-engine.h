// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_FLOW_ENGINE_H_
#define V8_HYDROGEN_FLOW_ENGINE_H_

#include "src/hydrogen.h"
#include "src/hydrogen-instructions.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// An example implementation of effects that doesn't collect anything.
class NoEffects : public ZoneObject {
 public:
  explicit NoEffects(Zone* zone) { }

  inline bool Disabled() {
    return true;  // Nothing to do.
  }
  template <class State>
  inline void Apply(State* state) {
    // do nothing.
  }
  inline void Process(HInstruction* value, Zone* zone) {
    // do nothing.
  }
  inline void Union(NoEffects* other, Zone* zone) {
    // do nothing.
  }
};


// An example implementation of state that doesn't track anything.
class NoState {
 public:
  inline NoState* Copy(HBasicBlock* succ, Zone* zone) {
    return this;
  }
  inline NoState* Process(HInstruction* value, Zone* zone) {
    return this;
  }
  inline NoState* Merge(HBasicBlock* succ, NoState* other, Zone* zone) {
    return this;
  }
};


// This class implements an engine that can drive flow-sensitive analyses
// over a graph of basic blocks, either one block at a time (local analysis)
// or over the entire graph (global analysis). The flow engine is parameterized
// by the type of the state and the effects collected while walking over the
// graph.
//
// The "State" collects which facts are known while passing over instructions
// in control flow order, and the "Effects" collect summary information about
// which facts could be invalidated on other control flow paths. The effects
// are necessary to correctly handle loops in the control flow graph without
// doing a fixed-point iteration. Thus the flow engine is guaranteed to visit
// each block at most twice; once for state, and optionally once for effects.
//
// The flow engine requires the State and Effects classes to implement methods
// like the example NoState and NoEffects above. It's not necessary to provide
// an effects implementation for local analysis.
template <class State, class Effects>
class HFlowEngine {
 public:
  HFlowEngine(HGraph* graph, Zone* zone)
    : graph_(graph),
      zone_(zone),
#if DEBUG
      pred_counts_(graph->blocks()->length(), zone),
#endif
      block_states_(graph->blocks()->length(), zone),
      loop_effects_(graph->blocks()->length(), zone) {
    loop_effects_.AddBlock(NULL, graph_->blocks()->length(), zone);
  }

  // Local analysis. Iterates over the instructions in the given block.
  State* AnalyzeOneBlock(HBasicBlock* block, State* state) {
    // Go through all instructions of the current block, updating the state.
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      state = state->Process(it.Current(), zone_);
    }
    return state;
  }

  // Global analysis. Iterates over all blocks that are dominated by the given
  // block, starting with the initial state. Computes effects for nested loops.
  void AnalyzeDominatedBlocks(HBasicBlock* root, State* initial) {
    InitializeStates();
    SetStateAt(root, initial);

    // Iterate all dominated blocks starting from the given start block.
    for (int i = root->block_id(); i < graph_->blocks()->length(); i++) {
      HBasicBlock* block = graph_->blocks()->at(i);

      // Skip blocks not dominated by the root node.
      if (SkipNonDominatedBlock(root, block)) continue;
      State* state = State::Finish(StateAt(block), block, zone_);

      if (block->IsReachable()) {
        DCHECK(state != NULL);
        if (block->IsLoopHeader()) {
          // Apply loop effects before analyzing loop body.
          ComputeLoopEffects(block)->Apply(state);
        } else {
          // Must have visited all predecessors before this block.
          CheckPredecessorCount(block);
        }

        // Go through all instructions of the current block, updating the state.
        for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
          state = state->Process(it.Current(), zone_);
        }
      }

      // Propagate the block state forward to all successor blocks.
      int max = block->end()->SuccessorCount();
      for (int i = 0; i < max; i++) {
        HBasicBlock* succ = block->end()->SuccessorAt(i);
        IncrementPredecessorCount(succ);

        if (max == 1 && succ->predecessors()->length() == 1) {
          // Optimization: successor can inherit this state.
          SetStateAt(succ, state);
        } else {
          // Merge the current state with the state already at the successor.
          SetStateAt(succ,
                     State::Merge(StateAt(succ), succ, state, block, zone_));
        }
      }
    }
  }

 private:
  // Computes and caches the loop effects for the loop which has the given
  // block as its loop header.
  Effects* ComputeLoopEffects(HBasicBlock* block) {
    DCHECK(block->IsLoopHeader());
    Effects* effects = loop_effects_[block->block_id()];
    if (effects != NULL) return effects;  // Already analyzed this loop.

    effects = new(zone_) Effects(zone_);
    loop_effects_[block->block_id()] = effects;
    if (effects->Disabled()) return effects;  // No effects for this analysis.

    HLoopInformation* loop = block->loop_information();
    int end = loop->GetLastBackEdge()->block_id();
    // Process the blocks between the header and the end.
    for (int i = block->block_id(); i <= end; i++) {
      HBasicBlock* member = graph_->blocks()->at(i);
      if (i != block->block_id() && member->IsLoopHeader()) {
        // Recursively compute and cache the effects of the nested loop.
        DCHECK(member->loop_information()->parent_loop() == loop);
        Effects* nested = ComputeLoopEffects(member);
        effects->Union(nested, zone_);
        // Skip the nested loop's blocks.
        i = member->loop_information()->GetLastBackEdge()->block_id();
      } else {
        // Process all the effects of the block.
        if (member->IsUnreachable()) continue;
        DCHECK(member->current_loop() == loop);
        for (HInstructionIterator it(member); !it.Done(); it.Advance()) {
          effects->Process(it.Current(), zone_);
        }
      }
    }
    return effects;
  }

  inline bool SkipNonDominatedBlock(HBasicBlock* root, HBasicBlock* other) {
    if (root->block_id() == 0) return false;  // Visit the whole graph.
    if (root == other) return false;          // Always visit the root.
    return !root->Dominates(other);           // Only visit dominated blocks.
  }

  inline State* StateAt(HBasicBlock* block) {
    return block_states_.at(block->block_id());
  }

  inline void SetStateAt(HBasicBlock* block, State* state) {
    block_states_.Set(block->block_id(), state);
  }

  inline void InitializeStates() {
#if DEBUG
    pred_counts_.Rewind(0);
    pred_counts_.AddBlock(0, graph_->blocks()->length(), zone_);
#endif
    block_states_.Rewind(0);
    block_states_.AddBlock(NULL, graph_->blocks()->length(), zone_);
  }

  inline void CheckPredecessorCount(HBasicBlock* block) {
    DCHECK(block->predecessors()->length() == pred_counts_[block->block_id()]);
  }

  inline void IncrementPredecessorCount(HBasicBlock* block) {
#if DEBUG
    pred_counts_[block->block_id()]++;
#endif
  }

  HGraph* graph_;                    // The hydrogen graph.
  Zone* zone_;                       // Temporary zone.
#if DEBUG
  ZoneList<int> pred_counts_;        // Finished predecessors (by block id).
#endif
  ZoneList<State*> block_states_;    // Block states (by block id).
  ZoneList<Effects*> loop_effects_;  // Loop effects (by block id).
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_FLOW_ENGINE_H_
