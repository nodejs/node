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

#include "hydrogen-removable-simulates.h"

namespace v8 {
namespace internal {

void HMergeRemovableSimulatesPhase::Run() {
  ZoneList<HSimulate*> mergelist(2, zone());
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    // Make sure the merge list is empty at the start of a block.
    ASSERT(mergelist.is_empty());
    // Nasty heuristic: Never remove the first simulate in a block. This
    // just so happens to have a beneficial effect on register allocation.
    bool first = true;
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (current->IsLeaveInlined()) {
        // Never fold simulates from inlined environments into simulates
        // in the outer environment.
        // (Before each HEnterInlined, there is a non-foldable HSimulate
        // anyway, so we get the barrier in the other direction for free.)
        // Simply remove all accumulated simulates without merging.  This
        // is safe because simulates after instructions with side effects
        // are never added to the merge list.
        while (!mergelist.is_empty()) {
          mergelist.RemoveLast()->DeleteAndReplaceWith(NULL);
        }
        continue;
      }
      if (current->IsReturn()) {
        // Drop mergeable simulates in the list. This is safe because
        // simulates after instructions with side effects are never added
        // to the merge list.
        while (!mergelist.is_empty()) {
          mergelist.RemoveLast()->DeleteAndReplaceWith(NULL);
        }
        continue;
      }
      // Skip the non-simulates and the first simulate.
      if (!current->IsSimulate()) continue;
      if (first) {
        first = false;
        continue;
      }
      HSimulate* current_simulate = HSimulate::cast(current);
      if ((current_simulate->previous()->HasObservableSideEffects() &&
           !current_simulate->next()->IsSimulate()) ||
          !current_simulate->is_candidate_for_removal()) {
        // This simulate is not suitable for folding.
        // Fold the ones accumulated so far.
        current_simulate->MergeWith(&mergelist);
        continue;
      } else {
        // Accumulate this simulate for folding later on.
        mergelist.Add(current_simulate, zone());
      }
    }

    if (!mergelist.is_empty()) {
      // Merge the accumulated simulates at the end of the block.
      HSimulate* last = mergelist.RemoveLast();
      last->MergeWith(&mergelist);
    }
  }
}

} }  // namespace v8::internal
