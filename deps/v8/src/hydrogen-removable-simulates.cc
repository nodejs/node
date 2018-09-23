// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen-flow-engine.h"
#include "src/hydrogen-instructions.h"
#include "src/hydrogen-removable-simulates.h"

namespace v8 {
namespace internal {

class State : public ZoneObject {
 public:
  explicit State(Zone* zone)
      : zone_(zone), mergelist_(2, zone), first_(true), mode_(NORMAL) { }

  State* Process(HInstruction* instr, Zone* zone) {
    if (FLAG_trace_removable_simulates) {
      PrintF("[%s with state %p in B%d: #%d %s]\n",
             mode_ == NORMAL ? "processing" : "collecting",
             reinterpret_cast<void*>(this), instr->block()->block_id(),
             instr->id(), instr->Mnemonic());
    }
    // Forward-merge "trains" of simulates after an instruction with observable
    // side effects to keep live ranges short.
    if (mode_ == COLLECT_CONSECUTIVE_SIMULATES) {
      if (instr->IsSimulate()) {
        HSimulate* current_simulate = HSimulate::cast(instr);
        if (current_simulate->is_candidate_for_removal() &&
            !current_simulate->ast_id().IsNone()) {
          Remember(current_simulate);
          return this;
        }
      }
      FlushSimulates();
      mode_ = NORMAL;
    }
    // Ensure there's a non-foldable HSimulate before an HEnterInlined to avoid
    // folding across HEnterInlined.
    DCHECK(!(instr->IsEnterInlined() &&
             HSimulate::cast(instr->previous())->is_candidate_for_removal()));
    if (instr->IsLeaveInlined() || instr->IsReturn()) {
      // Never fold simulates from inlined environments into simulates in the
      // outer environment. Simply remove all accumulated simulates without
      // merging. This is safe because simulates after instructions with side
      // effects are never added to the merge list. The same reasoning holds for
      // return instructions.
      RemoveSimulates();
      return this;
    }
    if (instr->IsControlInstruction()) {
      // Merge the accumulated simulates at the end of the block.
      FlushSimulates();
      return this;
    }
    if (instr->IsCapturedObject()) {
      // Do not merge simulates across captured objects - captured objects
      // change environments during environment replay, and such changes
      // would not be reflected in the simulate.
      FlushSimulates();
      return this;
    }
    // Skip the non-simulates and the first simulate.
    if (!instr->IsSimulate()) return this;
    if (first_) {
      first_ = false;
      return this;
    }
    HSimulate* current_simulate = HSimulate::cast(instr);
    if (!current_simulate->is_candidate_for_removal()) {
      Remember(current_simulate);
      FlushSimulates();
    } else if (current_simulate->ast_id().IsNone()) {
      DCHECK(current_simulate->next()->IsEnterInlined());
      FlushSimulates();
    } else if (current_simulate->previous()->HasObservableSideEffects()) {
      Remember(current_simulate);
      mode_ = COLLECT_CONSECUTIVE_SIMULATES;
    } else {
      Remember(current_simulate);
    }

    return this;
  }

  static State* Merge(State* succ_state,
                      HBasicBlock* succ_block,
                      State* pred_state,
                      HBasicBlock* pred_block,
                      Zone* zone) {
    return (succ_state == NULL)
        ? pred_state->Copy(succ_block, pred_block, zone)
        : succ_state->Merge(succ_block, pred_state, pred_block, zone);
  }

  static State* Finish(State* state, HBasicBlock* block, Zone* zone) {
    if (FLAG_trace_removable_simulates) {
      PrintF("[preparing state %p for B%d]\n", reinterpret_cast<void*>(state),
             block->block_id());
    }
    // For our current local analysis, we should not remember simulates across
    // block boundaries.
    DCHECK(!state->HasRememberedSimulates());
    // Nasty heuristic: Never remove the first simulate in a block. This
    // just so happens to have a beneficial effect on register allocation.
    state->first_ = true;
    return state;
  }

 private:
  explicit State(const State& other)
      : zone_(other.zone_),
        mergelist_(other.mergelist_, other.zone_),
        first_(other.first_),
        mode_(other.mode_) { }

  enum Mode { NORMAL, COLLECT_CONSECUTIVE_SIMULATES };

  bool HasRememberedSimulates() const { return !mergelist_.is_empty(); }

  void Remember(HSimulate* sim) {
    mergelist_.Add(sim, zone_);
  }

  void FlushSimulates() {
    if (HasRememberedSimulates()) {
      mergelist_.RemoveLast()->MergeWith(&mergelist_);
    }
  }

  void RemoveSimulates() {
    while (HasRememberedSimulates()) {
      mergelist_.RemoveLast()->DeleteAndReplaceWith(NULL);
    }
  }

  State* Copy(HBasicBlock* succ_block, HBasicBlock* pred_block, Zone* zone) {
    State* copy = new(zone) State(*this);
    if (FLAG_trace_removable_simulates) {
      PrintF("[copy state %p from B%d to new state %p for B%d]\n",
             reinterpret_cast<void*>(this), pred_block->block_id(),
             reinterpret_cast<void*>(copy), succ_block->block_id());
    }
    return copy;
  }

  State* Merge(HBasicBlock* succ_block,
               State* pred_state,
               HBasicBlock* pred_block,
               Zone* zone) {
    // For our current local analysis, we should not remember simulates across
    // block boundaries.
    DCHECK(!pred_state->HasRememberedSimulates());
    DCHECK(!HasRememberedSimulates());
    if (FLAG_trace_removable_simulates) {
      PrintF("[merge state %p from B%d into %p for B%d]\n",
             reinterpret_cast<void*>(pred_state), pred_block->block_id(),
             reinterpret_cast<void*>(this), succ_block->block_id());
    }
    return this;
  }

  Zone* zone_;
  ZoneList<HSimulate*> mergelist_;
  bool first_;
  Mode mode_;
};


// We don't use effects here.
class Effects : public ZoneObject {
 public:
  explicit Effects(Zone* zone) { }
  bool Disabled() { return true; }
  void Process(HInstruction* instr, Zone* zone) { }
  void Apply(State* state) { }
  void Union(Effects* that, Zone* zone) { }
};


void HMergeRemovableSimulatesPhase::Run() {
  HFlowEngine<State, Effects> engine(graph(), zone());
  State* state = new(zone()) State(zone());
  engine.AnalyzeDominatedBlocks(graph()->blocks()->at(0), state);
}

}  // namespace internal
}  // namespace v8
