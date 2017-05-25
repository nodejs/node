// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-mark-unreachable.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


void HMarkUnreachableBlocksPhase::MarkUnreachableBlocks() {
  // If there is unreachable code in the graph, propagate the unreachable marks
  // using a fixed-point iteration.
  bool changed = true;
  const ZoneList<HBasicBlock*>* blocks = graph()->blocks();
  while (changed) {
    changed = false;
    for (int i = 0; i < blocks->length(); i++) {
      HBasicBlock* block = blocks->at(i);
      if (!block->IsReachable()) continue;
      bool is_reachable = blocks->at(0) == block;
      for (HPredecessorIterator it(block); !it.Done(); it.Advance()) {
        HBasicBlock* predecessor = it.Current();
        // A block is reachable if one of its predecessors is reachable,
        // doesn't deoptimize and either is known to transfer control to the
        // block or has a control flow instruction for which the next block
        // cannot be determined.
        if (predecessor->IsReachable() && !predecessor->IsDeoptimizing()) {
          HBasicBlock* pred_succ;
          bool known_pred_succ =
              predecessor->end()->KnownSuccessorBlock(&pred_succ);
          if (!known_pred_succ || pred_succ == block) {
            is_reachable = true;
            break;
          }
        }
        if (block->is_osr_entry()) {
          is_reachable = true;
        }
      }
      if (!is_reachable) {
        block->MarkUnreachable();
        changed = true;
      }
    }
  }
}


void HMarkUnreachableBlocksPhase::Run() {
  MarkUnreachableBlocks();
}

}  // namespace internal
}  // namespace v8
