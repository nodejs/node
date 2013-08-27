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

#include "hydrogen-dce.h"
#include "v8.h"

namespace v8 {
namespace internal {

bool HDeadCodeEliminationPhase::MarkLive(HValue* ref, HValue* instr) {
  if (instr->CheckFlag(HValue::kIsLive)) return false;
  instr->SetFlag(HValue::kIsLive);

  if (FLAG_trace_dead_code_elimination) {
    HeapStringAllocator allocator;
    StringStream stream(&allocator);
    if (ref != NULL) {
      ref->PrintTo(&stream);
    } else {
      stream.Add("root ");
    }
    stream.Add(" -> ");
    instr->PrintTo(&stream);
    PrintF("[MarkLive %s]\n", *stream.ToCString());
  }

  return true;
}


void HDeadCodeEliminationPhase::MarkLiveInstructions() {
  ZoneList<HValue*> worklist(graph()->blocks()->length(), zone());

  // Mark initial root instructions for dead code elimination.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->CannotBeEliminated() && MarkLive(NULL, instr)) {
        worklist.Add(instr, zone());
      }
    }
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (phi->CannotBeEliminated() && MarkLive(NULL, phi)) {
        worklist.Add(phi, zone());
      }
    }
  }

  // Transitively mark all inputs of live instructions live.
  while (!worklist.is_empty()) {
    HValue* instr = worklist.RemoveLast();
    for (int i = 0; i < instr->OperandCount(); ++i) {
      if (MarkLive(instr, instr->OperandAt(i))) {
        worklist.Add(instr->OperandAt(i), zone());
      }
    }
  }
}


void HDeadCodeEliminationPhase::RemoveDeadInstructions() {
  ZoneList<HPhi*> worklist(graph()->blocks()->length(), zone());

  // Remove any instruction not marked kIsLive.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->CheckFlag(HValue::kIsLive)) {
        // Instruction has not been marked live; assume it is dead and remove.
        // TODO(titzer): we don't remove constants because some special ones
        // might be used by later phases and are assumed to be in the graph
        if (!instr->IsConstant()) instr->DeleteAndReplaceWith(NULL);
      } else {
        // Clear the liveness flag to leave the graph clean for the next DCE.
        instr->ClearFlag(HValue::kIsLive);
      }
    }
    // Collect phis that are dead and remove them in the next pass.
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (!phi->CheckFlag(HValue::kIsLive)) {
        worklist.Add(phi, zone());
      } else {
        phi->ClearFlag(HValue::kIsLive);
      }
    }
  }

  // Process phis separately to avoid simultaneously mutating the phi list.
  while (!worklist.is_empty()) {
    HPhi* phi = worklist.RemoveLast();
    HBasicBlock* block = phi->block();
    phi->DeleteAndReplaceWith(NULL);
    if (phi->HasMergedIndex()) {
      block->RecordDeletedPhi(phi->merged_index());
    }
  }
}

} }  // namespace v8::internal
