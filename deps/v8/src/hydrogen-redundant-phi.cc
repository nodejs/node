// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen-redundant-phi.h"

namespace v8 {
namespace internal {

void HRedundantPhiEliminationPhase::Run() {
  // Gather all phis from all blocks first.
  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  ZoneList<HPhi*> all_phis(blocks->length(), zone());
  for (int i = 0; i < blocks->length(); ++i) {
    HBasicBlock* block = blocks->at(i);
    for (int j = 0; j < block->phis()->length(); j++) {
      all_phis.Add(block->phis()->at(j), zone());
    }
  }

  // Iteratively reduce all phis in the list.
  ProcessPhis(&all_phis);

#if DEBUG
  // Make sure that we *really* removed all redundant phis.
  for (int i = 0; i < blocks->length(); ++i) {
    for (int j = 0; j < blocks->at(i)->phis()->length(); j++) {
      DCHECK(blocks->at(i)->phis()->at(j)->GetRedundantReplacement() == NULL);
    }
  }
#endif
}


void HRedundantPhiEliminationPhase::ProcessBlock(HBasicBlock* block) {
  ProcessPhis(block->phis());
}


void HRedundantPhiEliminationPhase::ProcessPhis(const ZoneList<HPhi*>* phis) {
  bool updated;
  do {
    // Iterately replace all redundant phis in the given list.
    updated = false;
    for (int i = 0; i < phis->length(); i++) {
      HPhi* phi = phis->at(i);
      if (phi->CheckFlag(HValue::kIsDead)) continue;  // Already replaced.

      HValue* replacement = phi->GetRedundantReplacement();
      if (replacement != NULL) {
        phi->SetFlag(HValue::kIsDead);
        for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
          HValue* value = it.value();
          value->SetOperandAt(it.index(), replacement);
          // Iterate again if used in another non-dead phi.
          updated |= value->IsPhi() && !value->CheckFlag(HValue::kIsDead);
        }
        phi->block()->RemovePhi(phi);
      }
    }
  } while (updated);
}


}  // namespace internal
}  // namespace v8
