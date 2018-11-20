// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/hydrogen-environment-liveness.h"


namespace v8 {
namespace internal {


HEnvironmentLivenessAnalysisPhase::HEnvironmentLivenessAnalysisPhase(
    HGraph* graph)
    : HPhase("H_Environment liveness analysis", graph),
      block_count_(graph->blocks()->length()),
      maximum_environment_size_(graph->maximum_environment_size()),
      live_at_block_start_(block_count_, zone()),
      first_simulate_(block_count_, zone()),
      first_simulate_invalid_for_index_(block_count_, zone()),
      markers_(maximum_environment_size_, zone()),
      collect_markers_(true),
      last_simulate_(NULL),
      went_live_since_last_simulate_(maximum_environment_size_, zone()) {
  DCHECK(maximum_environment_size_ > 0);
  for (int i = 0; i < block_count_; ++i) {
    live_at_block_start_.Add(
        new(zone()) BitVector(maximum_environment_size_, zone()), zone());
    first_simulate_.Add(NULL, zone());
    first_simulate_invalid_for_index_.Add(
        new(zone()) BitVector(maximum_environment_size_, zone()), zone());
  }
}


void HEnvironmentLivenessAnalysisPhase::ZapEnvironmentSlot(
    int index, HSimulate* simulate) {
  int operand_index = simulate->ToOperandIndex(index);
  if (operand_index == -1) {
    simulate->AddAssignedValue(index, graph()->GetConstantUndefined());
  } else {
    simulate->SetOperandAt(operand_index, graph()->GetConstantUndefined());
  }
}


void HEnvironmentLivenessAnalysisPhase::ZapEnvironmentSlotsInSuccessors(
    HBasicBlock* block, BitVector* live) {
  // When a value is live in successor A but dead in B, we must
  // explicitly zap it in B.
  for (HSuccessorIterator it(block->end()); !it.Done(); it.Advance()) {
    HBasicBlock* successor = it.Current();
    int successor_id = successor->block_id();
    BitVector* live_in_successor = live_at_block_start_[successor_id];
    if (live_in_successor->Equals(*live)) continue;
    for (int i = 0; i < live->length(); ++i) {
      if (!live->Contains(i)) continue;
      if (live_in_successor->Contains(i)) continue;
      if (first_simulate_invalid_for_index_.at(successor_id)->Contains(i)) {
        continue;
      }
      HSimulate* simulate = first_simulate_.at(successor_id);
      if (simulate == NULL) continue;
      DCHECK(VerifyClosures(simulate->closure(),
          block->last_environment()->closure()));
      ZapEnvironmentSlot(i, simulate);
    }
  }
}


void HEnvironmentLivenessAnalysisPhase::ZapEnvironmentSlotsForInstruction(
    HEnvironmentMarker* marker) {
  if (!marker->CheckFlag(HValue::kEndsLiveRange)) return;
  HSimulate* simulate = marker->next_simulate();
  if (simulate != NULL) {
    DCHECK(VerifyClosures(simulate->closure(), marker->closure()));
    ZapEnvironmentSlot(marker->index(), simulate);
  }
}


void HEnvironmentLivenessAnalysisPhase::UpdateLivenessAtBlockEnd(
    HBasicBlock* block,
    BitVector* live) {
  // Liveness at the end of each block: union of liveness in successors.
  live->Clear();
  for (HSuccessorIterator it(block->end()); !it.Done(); it.Advance()) {
    live->Union(*live_at_block_start_[it.Current()->block_id()]);
  }
}


void HEnvironmentLivenessAnalysisPhase::UpdateLivenessAtInstruction(
    HInstruction* instr,
    BitVector* live) {
  switch (instr->opcode()) {
    case HValue::kEnvironmentMarker: {
      HEnvironmentMarker* marker = HEnvironmentMarker::cast(instr);
      int index = marker->index();
      if (!live->Contains(index)) {
        marker->SetFlag(HValue::kEndsLiveRange);
      } else {
        marker->ClearFlag(HValue::kEndsLiveRange);
      }
      if (!went_live_since_last_simulate_.Contains(index)) {
        marker->set_next_simulate(last_simulate_);
      }
      if (marker->kind() == HEnvironmentMarker::LOOKUP) {
        live->Add(index);
      } else {
        DCHECK(marker->kind() == HEnvironmentMarker::BIND);
        live->Remove(index);
        went_live_since_last_simulate_.Add(index);
      }
      if (collect_markers_) {
        // Populate |markers_| list during the first pass.
        markers_.Add(marker, zone());
      }
      break;
    }
    case HValue::kLeaveInlined:
      // No environment values are live at the end of an inlined section.
      live->Clear();
      last_simulate_ = NULL;

      // The following DCHECKs guard the assumption used in case
      // kEnterInlined below:
      DCHECK(instr->next()->IsSimulate());
      DCHECK(instr->next()->next()->IsGoto());

      break;
    case HValue::kEnterInlined: {
      // Those environment values are live that are live at any return
      // target block. Here we make use of the fact that the end of an
      // inline sequence always looks like this: HLeaveInlined, HSimulate,
      // HGoto (to return_target block), with no environment lookups in
      // between (see DCHECKs above).
      HEnterInlined* enter = HEnterInlined::cast(instr);
      live->Clear();
      for (int i = 0; i < enter->return_targets()->length(); ++i) {
        int return_id = enter->return_targets()->at(i)->block_id();
        live->Union(*live_at_block_start_[return_id]);
      }
      last_simulate_ = NULL;
      break;
    }
    case HValue::kSimulate:
      last_simulate_ = HSimulate::cast(instr);
      went_live_since_last_simulate_.Clear();
      break;
    default:
      break;
  }
}


void HEnvironmentLivenessAnalysisPhase::Run() {
  DCHECK(maximum_environment_size_ > 0);

  // Main iteration. Compute liveness of environment slots, and store it
  // for each block until it doesn't change any more. For efficiency, visit
  // blocks in reverse order and walk backwards through each block. We
  // need several iterations to propagate liveness through nested loops.
  BitVector live(maximum_environment_size_, zone());
  BitVector worklist(block_count_, zone());
  for (int i = 0; i < block_count_; ++i) {
    worklist.Add(i);
  }
  while (!worklist.IsEmpty()) {
    for (int block_id = block_count_ - 1; block_id >= 0; --block_id) {
      if (!worklist.Contains(block_id)) {
        continue;
      }
      worklist.Remove(block_id);
      last_simulate_ = NULL;

      HBasicBlock* block = graph()->blocks()->at(block_id);
      UpdateLivenessAtBlockEnd(block, &live);

      for (HInstruction* instr = block->end(); instr != NULL;
           instr = instr->previous()) {
        UpdateLivenessAtInstruction(instr, &live);
      }

      // Reached the start of the block, do necessary bookkeeping:
      // store computed information for this block and add predecessors
      // to the work list as necessary.
      first_simulate_.Set(block_id, last_simulate_);
      first_simulate_invalid_for_index_[block_id]->CopyFrom(
          went_live_since_last_simulate_);
      if (live_at_block_start_[block_id]->UnionIsChanged(live)) {
        for (int i = 0; i < block->predecessors()->length(); ++i) {
          worklist.Add(block->predecessors()->at(i)->block_id());
        }
        if (block->IsInlineReturnTarget()) {
          worklist.Add(block->inlined_entry_block()->block_id());
        }
      }
    }
    // Only collect bind/lookup instructions during the first pass.
    collect_markers_ = false;
  }

  // Analysis finished. Zap dead environment slots.
  for (int i = 0; i < markers_.length(); ++i) {
    ZapEnvironmentSlotsForInstruction(markers_[i]);
  }
  for (int block_id = block_count_ - 1; block_id >= 0; --block_id) {
    HBasicBlock* block = graph()->blocks()->at(block_id);
    UpdateLivenessAtBlockEnd(block, &live);
    ZapEnvironmentSlotsInSuccessors(block, &live);
  }

  // Finally, remove the HEnvironment{Bind,Lookup} markers.
  for (int i = 0; i < markers_.length(); ++i) {
    markers_[i]->DeleteAndReplaceWith(NULL);
  }
}


#ifdef DEBUG
bool HEnvironmentLivenessAnalysisPhase::VerifyClosures(
    Handle<JSFunction> a, Handle<JSFunction> b) {
  Heap::RelocationLock for_heap_access(isolate()->heap());
  AllowHandleDereference for_verification;
  return a.is_identical_to(b);
}
#endif

} }  // namespace v8::internal
