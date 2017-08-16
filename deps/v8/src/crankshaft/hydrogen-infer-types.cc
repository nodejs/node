// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-infer-types.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void HInferTypesPhase::InferTypes(int from_inclusive, int to_inclusive) {
  for (int i = from_inclusive; i <= to_inclusive; ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);

    const ZoneList<HPhi*>* phis = block->phis();
    for (int j = 0; j < phis->length(); j++) {
      phis->at(j)->UpdateInferredType();
    }

    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      it.Current()->UpdateInferredType();
    }

    if (block->IsLoopHeader()) {
      HBasicBlock* last_back_edge =
          block->loop_information()->GetLastBackEdge();
      InferTypes(i + 1, last_back_edge->block_id());
      // Skip all blocks already processed by the recursive call.
      i = last_back_edge->block_id();
      // Update phis of the loop header now after the whole loop body is
      // guaranteed to be processed.
      for (int j = 0; j < block->phis()->length(); ++j) {
        HPhi* phi = block->phis()->at(j);
        worklist_.Add(phi, zone());
        in_worklist_.Add(phi->id());
      }
      while (!worklist_.is_empty()) {
        HValue* current = worklist_.RemoveLast();
        in_worklist_.Remove(current->id());
        if (current->UpdateInferredType()) {
          for (HUseIterator it(current->uses()); !it.Done(); it.Advance()) {
            HValue* use = it.value();
            if (!in_worklist_.Contains(use->id())) {
              in_worklist_.Add(use->id());
              worklist_.Add(use, zone());
            }
          }
        }
      }
      DCHECK(in_worklist_.IsEmpty());
    }
  }
}

}  // namespace internal
}  // namespace v8
