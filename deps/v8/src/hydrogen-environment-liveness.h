// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HYDROGEN_ENVIRONMENT_LIVENESS_H_
#define V8_HYDROGEN_ENVIRONMENT_LIVENESS_H_


#include "hydrogen.h"

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
class EnvironmentSlotLivenessAnalyzer {
 public:
  explicit EnvironmentSlotLivenessAnalyzer(HGraph* graph);

  void AnalyzeAndTrim();

 private:
  void ZapEnvironmentSlot(int index, HSimulate* simulate);
  void ZapEnvironmentSlotsInSuccessors(HBasicBlock* block, BitVector* live);
  void ZapEnvironmentSlotsForInstruction(HEnvironmentMarker* marker);
  void UpdateLivenessAtBlockEnd(HBasicBlock* block, BitVector* live);
  void UpdateLivenessAtInstruction(HInstruction* instr, BitVector* live);

  Zone* zone() { return &zone_; }

  HGraph* graph_;
  // Use a dedicated Zone for this phase, with a ZoneScope to ensure it
  // gets freed.
  Zone zone_;
  ZoneScope zone_scope_;

  int block_count_;

  // Largest number of local variables in any environment in the graph
  // (including inlined environments).
  int maximum_environment_size_;

  // Per-block data. All these lists are indexed by block_id.
  ZoneList<BitVector*>* live_at_block_start_;
  ZoneList<HSimulate*>* first_simulate_;
  ZoneList<BitVector*>* first_simulate_invalid_for_index_;

  // List of all HEnvironmentMarker instructions for quick iteration/deletion.
  // It is populated during the first pass over the graph, controlled by
  // |collect_markers_|.
  ZoneList<HEnvironmentMarker*>* markers_;
  bool collect_markers_;

  // Keeps track of the last simulate seen, as well as the environment slots
  // for which a new live range has started since (so they must not be zapped
  // in that simulate when the end of another live range of theirs is found).
  HSimulate* last_simulate_;
  BitVector* went_live_since_last_simulate_;
};


} }  // namespace v8::internal

#endif /* V8_HYDROGEN_ENVIRONMENT_LIVENESS_H_ */
