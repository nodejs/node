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

#include "hydrogen-mark-deoptimize.h"

namespace v8 {
namespace internal {

void HMarkDeoptimizeOnUndefinedPhase::Run() {
  const ZoneList<HPhi*>* phi_list = graph()->phi_list();
  for (int i = 0; i < phi_list->length(); i++) {
    HPhi* phi = phi_list->at(i);
    if (phi->CheckFlag(HValue::kAllowUndefinedAsNaN) &&
        !phi->CheckUsesForFlag(HValue::kAllowUndefinedAsNaN)) {
      ProcessPhi(phi);
    }
  }
}


void HMarkDeoptimizeOnUndefinedPhase::ProcessPhi(HPhi* phi) {
  ASSERT(phi->CheckFlag(HValue::kAllowUndefinedAsNaN));
  ASSERT(worklist_.is_empty());

  // Push the phi onto the worklist
  phi->ClearFlag(HValue::kAllowUndefinedAsNaN);
  worklist_.Add(phi, zone());

  // Process all phis that can reach this phi
  while (!worklist_.is_empty()) {
    phi = worklist_.RemoveLast();
    for (int i = phi->OperandCount() - 1; i >= 0; --i) {
      HValue* input = phi->OperandAt(i);
      if (input->IsPhi() && input->CheckFlag(HValue::kAllowUndefinedAsNaN)) {
        input->ClearFlag(HValue::kAllowUndefinedAsNaN);
        worklist_.Add(HPhi::cast(input), zone());
      }
    }
  }
}


void HComputeChangeUndefinedToNaN::Run() {
  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  for (int i = 0; i < blocks->length(); ++i) {
    const HBasicBlock* block(blocks->at(i));
    for (HInstruction* current = block->first(); current != NULL; ) {
      HInstruction* next = current->next();
      if (current->IsChange()) {
        if (HChange::cast(current)->can_convert_undefined_to_nan()) {
          current->SetFlag(HValue::kAllowUndefinedAsNaN);
        }
      }
      current = next;
    }
  }
}


} }  // namespace v8::internal
