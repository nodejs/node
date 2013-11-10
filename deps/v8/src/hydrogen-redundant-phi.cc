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

#include "hydrogen-redundant-phi.h"

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
      ASSERT(blocks->at(i)->phis()->at(j)->GetRedundantReplacement() == NULL);
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


} }  // namespace v8::internal
