// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-infer-representation.h"

namespace v8 {
namespace internal {

void HInferRepresentationPhase::AddToWorklist(HValue* current) {
  if (current->representation().IsTagged()) return;
  if (!current->CheckFlag(HValue::kFlexibleRepresentation)) return;
  if (in_worklist_.Contains(current->id())) return;
  worklist_.Add(current, zone());
  in_worklist_.Add(current->id());
}


void HInferRepresentationPhase::Run() {
  // (1) Initialize bit vectors and count real uses. Each phi gets a
  // bit-vector of length <number of phis>.
  const ZoneList<HPhi*>* phi_list = graph()->phi_list();
  int phi_count = phi_list->length();
  ZoneList<BitVector*> connected_phis(phi_count, zone());
  for (int i = 0; i < phi_count; ++i) {
    phi_list->at(i)->InitRealUses(i);
    BitVector* connected_set = new(zone()) BitVector(phi_count, zone());
    connected_set->Add(i);
    connected_phis.Add(connected_set, zone());
  }

  // (2) Do a fixed point iteration to find the set of connected phis.  A
  // phi is connected to another phi if its value is used either directly or
  // indirectly through a transitive closure of the def-use relation.
  bool change = true;
  while (change) {
    change = false;
    // We normally have far more "forward edges" than "backward edges",
    // so we terminate faster when we walk backwards.
    for (int i = phi_count - 1; i >= 0; --i) {
      HPhi* phi = phi_list->at(i);
      for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
        HValue* use = it.value();
        if (use->IsPhi()) {
          int id = HPhi::cast(use)->phi_id();
          if (connected_phis[i]->UnionIsChanged(*connected_phis[id]))
            change = true;
        }
      }
    }
  }

  // Set truncation flags for groups of connected phis. This is a conservative
  // approximation; the flag will be properly re-computed after representations
  // have been determined.
  if (phi_count > 0) {
    BitVector done(phi_count, zone());
    for (int i = 0; i < phi_count; ++i) {
      if (done.Contains(i)) continue;

      // Check if all uses of all connected phis in this group are truncating.
      bool all_uses_everywhere_truncating_int32 = true;
      bool all_uses_everywhere_truncating_smi = true;
      for (BitVector::Iterator it(connected_phis[i]);
           !it.Done();
           it.Advance()) {
        int index = it.Current();
        all_uses_everywhere_truncating_int32 &=
            phi_list->at(index)->CheckFlag(HInstruction::kTruncatingToInt32);
        all_uses_everywhere_truncating_smi &=
            phi_list->at(index)->CheckFlag(HInstruction::kTruncatingToSmi);
        done.Add(index);
      }

      if (!all_uses_everywhere_truncating_int32) {
        // Clear truncation flag of this group of connected phis.
        for (BitVector::Iterator it(connected_phis[i]);
             !it.Done();
             it.Advance()) {
          int index = it.Current();
          phi_list->at(index)->ClearFlag(HInstruction::kTruncatingToInt32);
        }
      }
      if (!all_uses_everywhere_truncating_smi) {
        // Clear truncation flag of this group of connected phis.
        for (BitVector::Iterator it(connected_phis[i]);
             !it.Done();
             it.Advance()) {
          int index = it.Current();
          phi_list->at(index)->ClearFlag(HInstruction::kTruncatingToSmi);
        }
      }
    }
  }

  // Simplify constant phi inputs where possible.
  // This step uses kTruncatingToInt32 flags of phis.
  for (int i = 0; i < phi_count; ++i) {
    phi_list->at(i)->SimplifyConstantInputs();
  }

  // Use the phi reachability information from step 2 to
  // sum up the non-phi use counts of all connected phis.
  for (int i = 0; i < phi_count; ++i) {
    HPhi* phi = phi_list->at(i);
    for (BitVector::Iterator it(connected_phis[i]);
         !it.Done();
         it.Advance()) {
      int index = it.Current();
      HPhi* it_use = phi_list->at(index);
      if (index != i) phi->AddNonPhiUsesFrom(it_use);  // Don't count twice.
    }
  }

  // Initialize work list
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    const ZoneList<HPhi*>* phis = block->phis();
    for (int j = 0; j < phis->length(); ++j) {
      AddToWorklist(phis->at(j));
    }

    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      AddToWorklist(current);
    }
  }

  // Do a fixed point iteration, trying to improve representations
  while (!worklist_.is_empty()) {
    HValue* current = worklist_.RemoveLast();
    current->InferRepresentation(this);
    in_worklist_.Remove(current->id());
  }

  // Lastly: any instruction that we don't have representation information
  // for defaults to Tagged.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    const ZoneList<HPhi*>* phis = block->phis();
    for (int j = 0; j < phis->length(); ++j) {
      HPhi* phi = phis->at(j);
      if (phi->representation().IsNone()) {
        phi->ChangeRepresentation(Representation::Tagged());
      }
    }
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (current->representation().IsNone() &&
          current->CheckFlag(HInstruction::kFlexibleRepresentation)) {
        if (current->CheckFlag(HInstruction::kCannotBeTagged)) {
          current->ChangeRepresentation(Representation::Double());
        } else {
          current->ChangeRepresentation(Representation::Tagged());
        }
      }
    }
  }
}

}  // namespace internal
}  // namespace v8
