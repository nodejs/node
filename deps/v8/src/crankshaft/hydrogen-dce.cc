// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-dce.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void HDeadCodeEliminationPhase::MarkLive(
    HValue* instr, ZoneList<HValue*>* worklist) {
  if (instr->CheckFlag(HValue::kIsLive)) return;  // Already live.

  if (FLAG_trace_dead_code_elimination) PrintLive(NULL, instr);

  // Transitively mark all inputs of live instructions live.
  worklist->Add(instr, zone());
  while (!worklist->is_empty()) {
    HValue* instr = worklist->RemoveLast();
    instr->SetFlag(HValue::kIsLive);
    for (int i = 0; i < instr->OperandCount(); ++i) {
      HValue* input = instr->OperandAt(i);
      if (!input->CheckFlag(HValue::kIsLive)) {
        input->SetFlag(HValue::kIsLive);
        worklist->Add(input, zone());
        if (FLAG_trace_dead_code_elimination) PrintLive(instr, input);
      }
    }
  }
}


void HDeadCodeEliminationPhase::PrintLive(HValue* ref, HValue* instr) {
  AllowHandleDereference allow_deref;
  OFStream os(stdout);
  os << "[MarkLive ";
  if (ref != NULL) {
    os << *ref;
  } else {
    os << "root";
  }
  os << " -> " << *instr << "]" << std::endl;
}


void HDeadCodeEliminationPhase::MarkLiveInstructions() {
  ZoneList<HValue*> worklist(10, zone());

  // Transitively mark all live instructions, starting from roots.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->CannotBeEliminated()) MarkLive(instr, &worklist);
    }
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (phi->CannotBeEliminated()) MarkLive(phi, &worklist);
    }
  }

  DCHECK(worklist.is_empty());  // Should have processed everything.
}


void HDeadCodeEliminationPhase::RemoveDeadInstructions() {
  ZoneList<HPhi*> worklist(graph()->blocks()->length(), zone());

  // Remove any instruction not marked kIsLive.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->CheckFlag(HValue::kIsLive)) {
        // Instruction has not been marked live, so remove it.
        instr->DeleteAndReplaceWith(NULL);
      } else {
        // Clear the liveness flag to leave the graph clean for the next DCE.
        instr->ClearFlag(HValue::kIsLive);
      }
    }
    // Collect phis that are dead and remove them in the next pass.
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (!phi->CheckFlag(HValue::kIsLive)) {
        worklist.Add(phi, zone());
      } else {
        phi->ClearFlag(HValue::kIsLive);
      }
    }
  }

  // Process phis separately to avoid simultaneously mutating the phi list.
  while (!worklist.is_empty()) {
    HPhi* phi = worklist.RemoveLast();
    HBasicBlock* block = phi->block();
    phi->DeleteAndReplaceWith(NULL);
    if (phi->HasMergedIndex()) {
      block->RecordDeletedPhi(phi->merged_index());
    }
  }
}

}  // namespace internal
}  // namespace v8
