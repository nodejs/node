// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-sce.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void HStackCheckEliminationPhase::Run() {
  // For each loop block walk the dominator tree from the backwards branch to
  // the loop header. If a call instruction is encountered the backwards branch
  // is dominated by a call and the stack check in the backwards branch can be
  // removed.
  for (int i = 0; i < graph()->blocks()->length(); i++) {
    HBasicBlock* block = graph()->blocks()->at(i);
    if (block->IsLoopHeader()) {
      HBasicBlock* back_edge = block->loop_information()->GetLastBackEdge();
      HBasicBlock* dominator = back_edge;
      while (true) {
        for (HInstructionIterator it(dominator); !it.Done(); it.Advance()) {
          if (it.Current()->HasStackCheck()) {
            block->loop_information()->stack_check()->Eliminate();
            break;
          }
        }

        // Done when the loop header is processed.
        if (dominator == block) break;

        // Move up the dominator tree.
        dominator = dominator->dominator();
      }
    }
  }
}

}  // namespace internal
}  // namespace v8
