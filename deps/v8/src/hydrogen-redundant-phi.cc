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
  // We do a simple fixed point iteration without any work list, because
  // machine-generated JavaScript can lead to a very dense Hydrogen graph with
  // an enormous work list and will consequently result in OOM. Experiments
  // showed that this simple algorithm is good enough, and even e.g. tracking
  // the set or range of blocks to consider is not a real improvement.
  bool need_another_iteration;
  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  ZoneList<HPhi*> redundant_phis(blocks->length(), zone());
  do {
    need_another_iteration = false;
    for (int i = 0; i < blocks->length(); ++i) {
      HBasicBlock* block = blocks->at(i);
      for (int j = 0; j < block->phis()->length(); j++) {
        HPhi* phi = block->phis()->at(j);
        HValue* replacement = phi->GetRedundantReplacement();
        if (replacement != NULL) {
          // Remember phi to avoid concurrent modification of the block's phis.
          redundant_phis.Add(phi, zone());
          for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
            HValue* value = it.value();
            value->SetOperandAt(it.index(), replacement);
            need_another_iteration |= value->IsPhi();
          }
        }
      }
      for (int i = 0; i < redundant_phis.length(); i++) {
        block->RemovePhi(redundant_phis[i]);
      }
      redundant_phis.Clear();
    }
  } while (need_another_iteration);

#if DEBUG
  // Make sure that we *really* removed all redundant phis.
  for (int i = 0; i < blocks->length(); ++i) {
    for (int j = 0; j < blocks->at(i)->phis()->length(); j++) {
      ASSERT(blocks->at(i)->phis()->at(j)->GetRedundantReplacement() == NULL);
    }
  }
#endif
}

} }  // namespace v8::internal
