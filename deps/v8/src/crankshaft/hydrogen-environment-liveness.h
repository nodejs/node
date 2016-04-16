// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_ENVIRONMENT_LIVENESS_H_
#define V8_CRANKSHAFT_HYDROGEN_ENVIRONMENT_LIVENESS_H_

#include "src/crankshaft/hydrogen.h"

namespace v8 {
namespace internal {


// Trims live ranges of environment slots by doing explicit liveness analysis.
// Values in the environment are kept alive by every subsequent LInstruction
// that is assigned an LEnvironment, which creates register pressure and
// unnecessary spill slot moves. Therefore it is beneficial to trim the
// live ranges of environment slots by zapping them with a constant after
// the last lookup that refers to them.
// Slots are identified by their index and only affected if whitelisted in
// HOptimizedGraphBuilder::IsEligibleForEnvironmentLivenessAnalysis().
class HEnvironmentLivenessAnalysisPhase : public HPhase {
 public:
  explicit HEnvironmentLivenessAnalysisPhase(HGraph* graph);

  void Run();

 private:
  void ZapEnvironmentSlot(int index, HSimulate* simulate);
  void ZapEnvironmentSlotsInSuccessors(HBasicBlock* block, BitVector* live);
  void ZapEnvironmentSlotsForInstruction(HEnvironmentMarker* marker);
  void UpdateLivenessAtBlockEnd(HBasicBlock* block, BitVector* live);
  void UpdateLivenessAtInstruction(HInstruction* instr, BitVector* live);
#ifdef DEBUG
  bool VerifyClosures(Handle<JSFunction> a, Handle<JSFunction> b);
#endif

  int block_count_;

  // Largest number of local variables in any environment in the graph
  // (including inlined environments).
  int maximum_environment_size_;

  // Per-block data. All these lists are indexed by block_id.
  ZoneList<BitVector*> live_at_block_start_;
  ZoneList<HSimulate*> first_simulate_;
  ZoneList<BitVector*> first_simulate_invalid_for_index_;

  // List of all HEnvironmentMarker instructions for quick iteration/deletion.
  // It is populated during the first pass over the graph, controlled by
  // |collect_markers_|.
  ZoneList<HEnvironmentMarker*> markers_;
  bool collect_markers_;

  // Keeps track of the last simulate seen, as well as the environment slots
  // for which a new live range has started since (so they must not be zapped
  // in that simulate when the end of another live range of theirs is found).
  HSimulate* last_simulate_;
  BitVector went_live_since_last_simulate_;

  DISALLOW_COPY_AND_ASSIGN(HEnvironmentLivenessAnalysisPhase);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_ENVIRONMENT_LIVENESS_H_
