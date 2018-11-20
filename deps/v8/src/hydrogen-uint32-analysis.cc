// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen-uint32-analysis.h"

namespace v8 {
namespace internal {


static bool IsUnsignedLoad(HLoadKeyed* instr) {
  switch (instr->elements_kind()) {
    case EXTERNAL_UINT8_ELEMENTS:
    case EXTERNAL_UINT16_ELEMENTS:
    case EXTERNAL_UINT32_ELEMENTS:
    case EXTERNAL_UINT8_CLAMPED_ELEMENTS:
    case UINT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case UINT32_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
      return true;
    default:
      return false;
  }
}


static bool IsUint32Operation(HValue* instr) {
  return instr->IsShr() ||
      (instr->IsLoadKeyed() && IsUnsignedLoad(HLoadKeyed::cast(instr))) ||
      (instr->IsInteger32Constant() && instr->GetInteger32Constant() >= 0);
}


bool HUint32AnalysisPhase::IsSafeUint32Use(HValue* val, HValue* use) {
  // Operations that operate on bits are safe.
  if (use->IsBitwise() || use->IsShl() || use->IsSar() || use->IsShr()) {
    return true;
  } else if (use->IsSimulate()) {
    // Deoptimization has special support for uint32.
    return true;
  } else if (use->IsChange()) {
    // Conversions have special support for uint32.
    // This DCHECK guards that the conversion in question is actually
    // implemented. Do not extend the whitelist without adding
    // support to LChunkBuilder::DoChange().
    DCHECK(HChange::cast(use)->to().IsDouble() ||
           HChange::cast(use)->to().IsSmi() ||
           HChange::cast(use)->to().IsTagged());
    return true;
  } else if (use->IsStoreKeyed()) {
    HStoreKeyed* store = HStoreKeyed::cast(use);
    if (store->is_external()) {
      // Storing a value into an external integer array is a bit level
      // operation.
      if (store->value() == val) {
        // Clamping or a conversion to double should have beed inserted.
        DCHECK(store->elements_kind() != EXTERNAL_UINT8_CLAMPED_ELEMENTS);
        DCHECK(store->elements_kind() != EXTERNAL_FLOAT32_ELEMENTS);
        DCHECK(store->elements_kind() != EXTERNAL_FLOAT64_ELEMENTS);
        return true;
      }
    }
  } else if (use->IsCompareNumericAndBranch()) {
    HCompareNumericAndBranch* c = HCompareNumericAndBranch::cast(use);
    return IsUint32Operation(c->left()) && IsUint32Operation(c->right());
  }

  return false;
}


// Iterate over all uses and verify that they are uint32 safe: either don't
// distinguish between int32 and uint32 due to their bitwise nature or
// have special support for uint32 values.
// Encountered phis are optimistically treated as safe uint32 uses,
// marked with kUint32 flag and collected in the phis_ list. A separate
// pass will be performed later by UnmarkUnsafePhis to clear kUint32 from
// phis that are not actually uint32-safe (it requires fix point iteration).
bool HUint32AnalysisPhase::Uint32UsesAreSafe(HValue* uint32val) {
  bool collect_phi_uses = false;
  for (HUseIterator it(uint32val->uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();

    if (use->IsPhi()) {
      if (!use->CheckFlag(HInstruction::kUint32)) {
        // There is a phi use of this value from a phi that is not yet
        // collected in phis_ array. Separate pass is required.
        collect_phi_uses = true;
      }

      // Optimistically treat phis as uint32 safe.
      continue;
    }

    if (!IsSafeUint32Use(uint32val, use)) {
      return false;
    }
  }

  if (collect_phi_uses) {
    for (HUseIterator it(uint32val->uses()); !it.Done(); it.Advance()) {
      HValue* use = it.value();

      // There is a phi use of this value from a phi that is not yet
      // collected in phis_ array. Separate pass is required.
      if (use->IsPhi() && !use->CheckFlag(HInstruction::kUint32)) {
        use->SetFlag(HInstruction::kUint32);
        phis_.Add(HPhi::cast(use), zone());
      }
    }
  }

  return true;
}


// Check if all operands to the given phi are marked with kUint32 flag.
bool HUint32AnalysisPhase::CheckPhiOperands(HPhi* phi) {
  if (!phi->CheckFlag(HInstruction::kUint32)) {
    // This phi is not uint32 safe. No need to check operands.
    return false;
  }

  for (int j = 0; j < phi->OperandCount(); j++) {
    HValue* operand = phi->OperandAt(j);
    if (!operand->CheckFlag(HInstruction::kUint32)) {
      // Lazily mark constants that fit into uint32 range with kUint32 flag.
      if (operand->IsInteger32Constant() &&
          operand->GetInteger32Constant() >= 0) {
        operand->SetFlag(HInstruction::kUint32);
        continue;
      }

      // This phi is not safe, some operands are not uint32 values.
      return false;
    }
  }

  return true;
}


// Remove kUint32 flag from the phi itself and its operands. If any operand
// was a phi marked with kUint32 place it into a worklist for
// transitive clearing of kUint32 flag.
void HUint32AnalysisPhase::UnmarkPhi(HPhi* phi, ZoneList<HPhi*>* worklist) {
  phi->ClearFlag(HInstruction::kUint32);
  for (int j = 0; j < phi->OperandCount(); j++) {
    HValue* operand = phi->OperandAt(j);
    if (operand->CheckFlag(HInstruction::kUint32)) {
      operand->ClearFlag(HInstruction::kUint32);
      if (operand->IsPhi()) {
        worklist->Add(HPhi::cast(operand), zone());
      }
    }
  }
}


void HUint32AnalysisPhase::UnmarkUnsafePhis() {
  // No phis were collected. Nothing to do.
  if (phis_.length() == 0) return;

  // Worklist used to transitively clear kUint32 from phis that
  // are used as arguments to other phis.
  ZoneList<HPhi*> worklist(phis_.length(), zone());

  // Phi can be used as a uint32 value if and only if
  // all its operands are uint32 values and all its
  // uses are uint32 safe.

  // Iterate over collected phis and unmark those that
  // are unsafe. When unmarking phi unmark its operands
  // and add it to the worklist if it is a phi as well.
  // Phis that are still marked as safe are shifted down
  // so that all safe phis form a prefix of the phis_ array.
  int phi_count = 0;
  for (int i = 0; i < phis_.length(); i++) {
    HPhi* phi = phis_[i];

    if (CheckPhiOperands(phi) && Uint32UsesAreSafe(phi)) {
      phis_[phi_count++] = phi;
    } else {
      UnmarkPhi(phi, &worklist);
    }
  }

  // Now phis array contains only those phis that have safe
  // non-phi uses. Start transitively clearing kUint32 flag
  // from phi operands of discovered non-safe phis until
  // only safe phis are left.
  while (!worklist.is_empty())  {
    while (!worklist.is_empty()) {
      HPhi* phi = worklist.RemoveLast();
      UnmarkPhi(phi, &worklist);
    }

    // Check if any operands to safe phis were unmarked
    // turning a safe phi into unsafe. The same value
    // can flow into several phis.
    int new_phi_count = 0;
    for (int i = 0; i < phi_count; i++) {
      HPhi* phi = phis_[i];

      if (CheckPhiOperands(phi)) {
        phis_[new_phi_count++] = phi;
      } else {
        UnmarkPhi(phi, &worklist);
      }
    }
    phi_count = new_phi_count;
  }
}


void HUint32AnalysisPhase::Run() {
  if (!graph()->has_uint32_instructions()) return;

  ZoneList<HInstruction*>* uint32_instructions = graph()->uint32_instructions();
  for (int i = 0; i < uint32_instructions->length(); ++i) {
    // Analyze instruction and mark it with kUint32 if all
    // its uses are uint32 safe.
    HInstruction* current = uint32_instructions->at(i);
    if (current->IsLinked() &&
        current->representation().IsInteger32() &&
        Uint32UsesAreSafe(current)) {
      current->SetFlag(HInstruction::kUint32);
    }
  }

  // Some phis might have been optimistically marked with kUint32 flag.
  // Remove this flag from those phis that are unsafe and propagate
  // this information transitively potentially clearing kUint32 flag
  // from some non-phi operations that are used as operands to unsafe phis.
  UnmarkUnsafePhis();
}


} }  // namespace v8::internal
