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

#include "hydrogen-canonicalize.h"

namespace v8 {
namespace internal {

void HCanonicalizePhase::Run() {
  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  // Before removing no-op instructions, save their semantic value.
  // We must be careful not to set the flag unnecessarily, because GVN
  // cannot identify two instructions when their flag value differs.
  for (int i = 0; i < blocks->length(); ++i) {
    for (HInstructionIterator it(blocks->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->IsArithmeticBinaryOperation()) {
        if (instr->representation().IsInteger32()) {
          if (instr->HasAtLeastOneUseWithFlagAndNoneWithout(
                  HInstruction::kTruncatingToInt32)) {
            instr->SetFlag(HInstruction::kAllUsesTruncatingToInt32);
          }
        } else if (instr->representation().IsSmi()) {
          if (instr->HasAtLeastOneUseWithFlagAndNoneWithout(
                  HInstruction::kTruncatingToSmi)) {
            instr->SetFlag(HInstruction::kAllUsesTruncatingToSmi);
          } else if (instr->HasAtLeastOneUseWithFlagAndNoneWithout(
                         HInstruction::kTruncatingToInt32)) {
            // Avoid redundant minus zero check
            instr->SetFlag(HInstruction::kAllUsesTruncatingToInt32);
          }
        }
      }
    }
  }
  // Perform actual Canonicalization pass.
  for (int i = 0; i < blocks->length(); ++i) {
    for (HInstructionIterator it(blocks->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      HValue* value = instr->Canonicalize();
      if (value != instr) instr->DeleteAndReplaceWith(value);
    }
  }
}

} }  // namespace v8::internal
