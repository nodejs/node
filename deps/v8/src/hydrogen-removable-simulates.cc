// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      if (current->IsEnterInlined()) {
        // Ensure there's a non-foldable HSimulate before an HEnterInlined to
        // avoid folding across HEnterInlined.
        ASSERT(!HSimulate::cast(current->previous())->
                   is_candidate_for_removal());
      }
      if (current->IsLeaveInlined()) {
        // Never fold simulates from inlined environments into simulates in the
        // outer environment. Simply remove all accumulated simulates without
        // merging. This is safe because simulates after instructions with side
        // effects are never added to the merge list.
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
      if (!current_simulate->is_candidate_for_removal()) {
        current_simulate->MergeWith(&mergelist);
      } else if (current_simulate->ast_id().IsNone()) {
        ASSERT(current_simulate->next()->IsEnterInlined());
        if (!mergelist.is_empty()) {
          HSimulate* last = mergelist.RemoveLast();
          last->MergeWith(&mergelist);
        }
      } else if (current_simulate->previous()->HasObservableSideEffects()) {
        while (current_simulate->next()->IsSimulate()) {
          it.Advance();
          HSimulate* next_simulate = HSimulate::cast(it.Current());
          if (next_simulate->ast_id().IsNone()) break;
          mergelist.Add(current_simulate, zone());
          current_simulate = next_simulate;
          if (!current_simulate->is_candidate_for_removal()) break;
        }
        current_simulate->MergeWith(&mergelist);
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
