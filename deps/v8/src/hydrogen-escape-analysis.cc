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

#include "hydrogen-escape-analysis.h"

namespace v8 {
namespace internal {


void HEscapeAnalysisPhase::CollectIfNoEscapingUses(HInstruction* instr) {
  for (HUseIterator it(instr->uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (use->HasEscapingOperandAt(it.index())) {
      if (FLAG_trace_escape_analysis) {
        PrintF("#%d (%s) escapes through #%d (%s) @%d\n", instr->id(),
               instr->Mnemonic(), use->id(), use->Mnemonic(), it.index());
      }
      return;
    }
  }
  if (FLAG_trace_escape_analysis) {
    PrintF("#%d (%s) is being captured\n", instr->id(), instr->Mnemonic());
  }
  captured_.Add(instr, zone());
}


void HEscapeAnalysisPhase::CollectCapturedValues() {
  int block_count = graph()->blocks()->length();
  for (int i = 0; i < block_count; ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->IsAllocate() || instr->IsAllocateObject()) {
        CollectIfNoEscapingUses(instr);
      }
    }
  }
}


} }  // namespace v8::internal
