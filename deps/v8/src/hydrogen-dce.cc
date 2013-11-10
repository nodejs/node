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

void HDeadCodeEliminationPhase::MarkLive(
    HValue* instr, ZoneList<HValue*>* worklist) {
  if (instr->CheckFlag(HValue::kIsLive)) return;  // Already live.

  if (FLAG_trace_dead_code_elimination) PrintLive(NULL, instr);

  // Transitively mark all inputs of live instructions live.
  worklist->Add(instr, zone());
  while (!worklist->is_empty()) {
    HValue* instr = worklist->RemoveLast();
    instr->SetFlag(HValue::kIsLive);
    for (int i = 0; i < instr->OperandCount(); ++i) {
      HValue* input = instr->OperandAt(i);
      if (!input->CheckFlag(HValue::kIsLive)) {
        input->SetFlag(HValue::kIsLive);
        worklist->Add(input, zone());
        if (FLAG_trace_dead_code_elimination) PrintLive(instr, input);
      }
    }
  }
}


void HDeadCodeEliminationPhase::PrintLive(HValue* ref, HValue* instr) {
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


void HDeadCodeEliminationPhase::MarkLiveInstructions() {
  ZoneList<HValue*> worklist(10, zone());

  // Transitively mark all live instructions, starting from roots.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->CannotBeEliminated()) MarkLive(instr, &worklist);
    }
    for (int j = 0; j < block->phis()->length(); j++) {
      HPhi* phi = block->phis()->at(j);
      if (phi->CannotBeEliminated()) MarkLive(phi, &worklist);
    }
  }

  ASSERT(worklist.is_empty());  // Should have processed everything.
}


void HDeadCodeEliminationPhase::RemoveDeadInstructions() {
  ZoneList<HPhi*> worklist(graph()->blocks()->length(), zone());

  // Remove any instruction not marked kIsLive.
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->CheckFlag(HValue::kIsLive)) {
        // Instruction has not been marked live, so remove it.
        instr->DeleteAndReplaceWith(NULL);
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
