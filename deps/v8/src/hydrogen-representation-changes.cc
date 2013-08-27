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

#include "hydrogen-representation-changes.h"

namespace v8 {
namespace internal {

void HRepresentationChangesPhase::InsertRepresentationChangeForUse(
    HValue* value, HValue* use_value, int use_index, Representation to) {
  // Insert the representation change right before its use. For phi-uses we
  // insert at the end of the corresponding predecessor.
  HInstruction* next = NULL;
  if (use_value->IsPhi()) {
    next = use_value->block()->predecessors()->at(use_index)->end();
  } else {
    next = HInstruction::cast(use_value);
  }
  // For constants we try to make the representation change at compile
  // time. When a representation change is not possible without loss of
  // information we treat constants like normal instructions and insert the
  // change instructions for them.
  HInstruction* new_value = NULL;
  bool is_truncating_to_smi = use_value->CheckFlag(HValue::kTruncatingToSmi);
  bool is_truncating_to_int = use_value->CheckFlag(HValue::kTruncatingToInt32);
  if (value->IsConstant()) {
    HConstant* constant = HConstant::cast(value);
    // Try to create a new copy of the constant with the new representation.
    if (is_truncating_to_int && to.IsInteger32()) {
      Maybe<HConstant*> res = constant->CopyToTruncatedInt32(graph()->zone());
      if (res.has_value) new_value = res.value;
    } else {
      new_value = constant->CopyToRepresentation(to, graph()->zone());
    }
  }

  if (new_value == NULL) {
    new_value = new(graph()->zone()) HChange(
        value, to, is_truncating_to_smi, is_truncating_to_int);
  }

  new_value->InsertBefore(next);
  use_value->SetOperandAt(use_index, new_value);
}


void HRepresentationChangesPhase::InsertRepresentationChangesForValue(
    HValue* value) {
  Representation r = value->representation();
  if (r.IsNone()) return;
  if (value->HasNoUses()) return;

  for (HUseIterator it(value->uses()); !it.Done(); it.Advance()) {
    HValue* use_value = it.value();
    int use_index = it.index();
    Representation req = use_value->RequiredInputRepresentation(use_index);
    if (req.IsNone() || req.Equals(r)) continue;
    InsertRepresentationChangeForUse(value, use_value, use_index, req);
  }
  if (value->HasNoUses()) {
    ASSERT(value->IsConstant());
    value->DeleteAndReplaceWith(NULL);
  }

  // The only purpose of a HForceRepresentation is to represent the value
  // after the (possible) HChange instruction.  We make it disappear.
  if (value->IsForceRepresentation()) {
    value->DeleteAndReplaceWith(HForceRepresentation::cast(value)->value());
  }
}


void HRepresentationChangesPhase::Run() {
  // Compute truncation flag for phis: Initially assume that all
  // int32-phis allow truncation and iteratively remove the ones that
  // are used in an operation that does not allow a truncating
  // conversion.
  ZoneList<HPhi*> worklist(8, zone());

  const ZoneList<HPhi*>* phi_list(graph()->phi_list());
  for (int i = 0; i < phi_list->length(); i++) {
    HPhi* phi = phi_list->at(i);
    if (phi->representation().IsInteger32()) {
      phi->SetFlag(HValue::kTruncatingToInt32);
    } else if (phi->representation().IsSmi()) {
      phi->SetFlag(HValue::kTruncatingToSmi);
    }
  }

  for (int i = 0; i < phi_list->length(); i++) {
    HPhi* phi = phi_list->at(i);
    for (HUseIterator it(phi->uses()); !it.Done(); it.Advance()) {
      // If a Phi is used as a non-truncating int32 or as a double,
      // clear its "truncating" flag.
      HValue* use = it.value();
      Representation input_representation =
          use->RequiredInputRepresentation(it.index());
      if ((phi->representation().IsInteger32() &&
           !(input_representation.IsInteger32() &&
             use->CheckFlag(HValue::kTruncatingToInt32))) ||
          (phi->representation().IsSmi() &&
           !(input_representation.IsSmi() &&
             use->CheckFlag(HValue::kTruncatingToSmi)))) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating because of #%d %s\n",
                 phi->id(), it.value()->id(), it.value()->Mnemonic());
        }
        phi->ClearFlag(HValue::kTruncatingToInt32);
        phi->ClearFlag(HValue::kTruncatingToSmi);
        worklist.Add(phi, zone());
        break;
      }
    }
  }

  while (!worklist.is_empty()) {
    HPhi* current = worklist.RemoveLast();
    for (int i = 0; i < current->OperandCount(); ++i) {
      HValue* input = current->OperandAt(i);
      if (input->IsPhi() &&
          ((input->representation().IsInteger32() &&
            input->CheckFlag(HValue::kTruncatingToInt32)) ||
           (input->representation().IsSmi() &&
            input->CheckFlag(HValue::kTruncatingToSmi)))) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating because of #%d %s\n",
                 input->id(), current->id(), current->Mnemonic());
        }
        input->ClearFlag(HValue::kTruncatingToInt32);
        input->ClearFlag(HValue::kTruncatingToSmi);
        worklist.Add(HPhi::cast(input), zone());
      }
    }
  }

  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  for (int i = 0; i < blocks->length(); ++i) {
    // Process phi instructions first.
    const HBasicBlock* block(blocks->at(i));
    const ZoneList<HPhi*>* phis = block->phis();
    for (int j = 0; j < phis->length(); j++) {
      InsertRepresentationChangesForValue(phis->at(j));
    }

    // Process normal instructions.
    for (HInstruction* current = block->first(); current != NULL; ) {
      HInstruction* next = current->next();
      InsertRepresentationChangesForValue(current);
      current = next;
    }
  }
}

} }  // namespace v8::internal
