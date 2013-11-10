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

#include "hydrogen-mark-unreachable.h"

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

} }  // namespace v8::internal
