// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-representation-changes.h"
#include "src/objects-inl.h"

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
  bool is_truncating_to_number =
      use_value->CheckFlag(HValue::kTruncatingToNumber);
  if (value->IsConstant()) {
    HConstant* constant = HConstant::cast(value);
    // Try to create a new copy of the constant with the new representation.
    if (is_truncating_to_int && to.IsInteger32()) {
      Maybe<HConstant*> res = constant->CopyToTruncatedInt32(graph()->zone());
      if (res.IsJust()) new_value = res.FromJust();
    } else {
      new_value = constant->CopyToRepresentation(to, graph()->zone());
    }
  }

  if (new_value == NULL) {
    new_value = new (graph()->zone())
        HChange(value, to, is_truncating_to_smi, is_truncating_to_int,
                is_truncating_to_number);
  }

  new_value->InsertBefore(next);
  use_value->SetOperandAt(use_index, new_value);
}


static bool IsNonDeoptingIntToSmiChange(HChange* change) {
  Representation from_rep = change->from();
  Representation to_rep = change->to();
  // Flags indicating Uint32 operations are set in a later Hydrogen phase.
  DCHECK(!change->CheckFlag(HValue::kUint32));
  return from_rep.IsInteger32() && to_rep.IsSmi() && SmiValuesAre32Bits();
}


void HRepresentationChangesPhase::InsertRepresentationChangesForValue(
    HValue* value) {
  Representation r = value->representation();
  if (r.IsNone()) {
#ifdef DEBUG
    for (HUseIterator it(value->uses()); !it.Done(); it.Advance()) {
      HValue* use_value = it.value();
      int use_index = it.index();
      Representation req = use_value->RequiredInputRepresentation(use_index);
      DCHECK(req.IsNone());
    }
#endif
    return;
  }
  if (value->HasNoUses()) {
    if (value->IsForceRepresentation()) value->DeleteAndReplaceWith(NULL);
    return;
  }

  for (HUseIterator it(value->uses()); !it.Done(); it.Advance()) {
    HValue* use_value = it.value();
    int use_index = it.index();
    Representation req = use_value->RequiredInputRepresentation(use_index);
    if (req.IsNone() || req.Equals(r)) continue;

    // If this is an HForceRepresentation instruction, and an HChange has been
    // inserted above it, examine the input representation of the HChange. If
    // that's int32, and this HForceRepresentation use is int32, and int32 to
    // smi changes can't cause deoptimisation, set the input of the use to the
    // input of the HChange.
    if (value->IsForceRepresentation()) {
      HValue* input = HForceRepresentation::cast(value)->value();
      if (input->IsChange()) {
        HChange* change = HChange::cast(input);
        if (change->from().Equals(req) && IsNonDeoptingIntToSmiChange(change)) {
          use_value->SetOperandAt(use_index, change->value());
          continue;
        }
      }
    }
    InsertRepresentationChangeForUse(value, use_value, use_index, req);
  }
  if (value->HasNoUses()) {
    DCHECK(value->IsConstant() || value->IsForceRepresentation());
    value->DeleteAndReplaceWith(NULL);
  } else {
    // The only purpose of a HForceRepresentation is to represent the value
    // after the (possible) HChange instruction.  We make it disappear.
    if (value->IsForceRepresentation()) {
      value->DeleteAndReplaceWith(HForceRepresentation::cast(value)->value());
    }
  }
}


void HRepresentationChangesPhase::Run() {
  // Compute truncation flag for phis:
  //
  // - Initially assume that all phis allow truncation to number and iteratively
  //   remove the ones that are used in an operation that not do an implicit
  //   ToNumber conversion.
  // - Also assume that all Integer32 phis allow ToInt32 truncation and all
  //   Smi phis allow truncation to Smi.
  //
  ZoneList<HPhi*> number_worklist(8, zone());
  ZoneList<HPhi*> int_worklist(8, zone());
  ZoneList<HPhi*> smi_worklist(8, zone());

  const ZoneList<HPhi*>* phi_list(graph()->phi_list());
  for (int i = 0; i < phi_list->length(); i++) {
    HPhi* phi = phi_list->at(i);
    if (phi->representation().IsInteger32()) {
      phi->SetFlag(HValue::kTruncatingToInt32);
    } else if (phi->representation().IsSmi()) {
      phi->SetFlag(HValue::kTruncatingToSmi);
      phi->SetFlag(HValue::kTruncatingToInt32);
    }
    phi->SetFlag(HValue::kTruncatingToNumber);
  }

  for (int i = 0; i < phi_list->length(); i++) {
    HPhi* phi = phi_list->at(i);
    HValue* value = NULL;

    if (phi->CheckFlag(HValue::kTruncatingToNumber) &&
        !phi->CheckUsesForFlag(HValue::kTruncatingToNumber, &value)) {
      number_worklist.Add(phi, zone());
      phi->ClearFlag(HValue::kTruncatingToNumber);
      phi->ClearFlag(HValue::kTruncatingToInt32);
      phi->ClearFlag(HValue::kTruncatingToSmi);
      if (FLAG_trace_representation) {
        PrintF("#%d Phi is not truncating Number because of #%d %s\n",
               phi->id(), value->id(), value->Mnemonic());
      }
    } else if (phi->representation().IsSmiOrInteger32() &&
               !phi->CheckUsesForFlag(HValue::kTruncatingToInt32, &value)) {
      int_worklist.Add(phi, zone());
      phi->ClearFlag(HValue::kTruncatingToInt32);
      phi->ClearFlag(HValue::kTruncatingToSmi);
      if (FLAG_trace_representation) {
        PrintF("#%d Phi is not truncating Int32 because of #%d %s\n",
               phi->id(), value->id(), value->Mnemonic());
      }
    } else if (phi->representation().IsSmi() &&
               !phi->CheckUsesForFlag(HValue::kTruncatingToSmi, &value)) {
      smi_worklist.Add(phi, zone());
      phi->ClearFlag(HValue::kTruncatingToSmi);
      if (FLAG_trace_representation) {
        PrintF("#%d Phi is not truncating Smi because of #%d %s\n",
               phi->id(), value->id(), value->Mnemonic());
      }
    }
  }

  while (!number_worklist.is_empty()) {
    HPhi* current = number_worklist.RemoveLast();
    for (int i = current->OperandCount() - 1; i >= 0; --i) {
      HValue* input = current->OperandAt(i);
      if (input->IsPhi() && input->CheckFlag(HValue::kTruncatingToNumber)) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating Number because of #%d %s\n",
                 input->id(), current->id(), current->Mnemonic());
        }
        input->ClearFlag(HValue::kTruncatingToNumber);
        input->ClearFlag(HValue::kTruncatingToInt32);
        input->ClearFlag(HValue::kTruncatingToSmi);
        number_worklist.Add(HPhi::cast(input), zone());
      }
    }
  }

  while (!int_worklist.is_empty()) {
    HPhi* current = int_worklist.RemoveLast();
    for (int i = 0; i < current->OperandCount(); ++i) {
      HValue* input = current->OperandAt(i);
      if (input->IsPhi() &&
          input->representation().IsSmiOrInteger32() &&
          input->CheckFlag(HValue::kTruncatingToInt32)) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating Int32 because of #%d %s\n",
                 input->id(), current->id(), current->Mnemonic());
        }
        input->ClearFlag(HValue::kTruncatingToInt32);
        int_worklist.Add(HPhi::cast(input), zone());
      }
    }
  }

  while (!smi_worklist.is_empty()) {
    HPhi* current = smi_worklist.RemoveLast();
    for (int i = 0; i < current->OperandCount(); ++i) {
      HValue* input = current->OperandAt(i);
      if (input->IsPhi() &&
          input->representation().IsSmi() &&
          input->CheckFlag(HValue::kTruncatingToSmi)) {
        if (FLAG_trace_representation) {
          PrintF("#%d Phi is not truncating Smi because of #%d %s\n",
                 input->id(), current->id(), current->Mnemonic());
        }
        input->ClearFlag(HValue::kTruncatingToSmi);
        smi_worklist.Add(HPhi::cast(input), zone());
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

}  // namespace internal
}  // namespace v8
