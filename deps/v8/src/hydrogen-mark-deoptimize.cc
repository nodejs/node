// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen-mark-deoptimize.h"

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
  DCHECK(phi->CheckFlag(HValue::kAllowUndefinedAsNaN));
  DCHECK(worklist_.is_empty());

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


}  // namespace internal
}  // namespace v8
