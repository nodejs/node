// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-store-elimination.h"

#include "src/crankshaft/hydrogen-instructions.h"

namespace v8 {
namespace internal {

#define TRACE(x) if (FLAG_trace_store_elimination) PrintF x

// Performs a block-by-block local analysis for removable stores.
void HStoreEliminationPhase::Run() {
  GVNFlagSet flags;  // Use GVN flags as an approximation for some instructions.
  flags.RemoveAll();

  flags.Add(kArrayElements);
  flags.Add(kArrayLengths);
  flags.Add(kStringLengths);
  flags.Add(kBackingStoreFields);
  flags.Add(kDoubleArrayElements);
  flags.Add(kDoubleFields);
  flags.Add(kElementsPointer);
  flags.Add(kInobjectFields);
  flags.Add(kExternalMemory);
  flags.Add(kStringChars);
  flags.Add(kTypedArrayElements);

  for (int i = 0; i < graph()->blocks()->length(); i++) {
    unobserved_.Rewind(0);
    HBasicBlock* block = graph()->blocks()->at(i);
    if (!block->IsReachable()) continue;
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->CheckFlag(HValue::kIsDead)) continue;

      // TODO(titzer): eliminate unobserved HStoreKeyed instructions too.
      switch (instr->opcode()) {
        case HValue::kStoreNamedField:
          // Remove any unobserved stores overwritten by this store.
          ProcessStore(HStoreNamedField::cast(instr));
          break;
        case HValue::kLoadNamedField:
          // Observe any unobserved stores on this object + field.
          ProcessLoad(HLoadNamedField::cast(instr));
          break;
        default:
          ProcessInstr(instr, flags);
          break;
      }
    }
  }
}


void HStoreEliminationPhase::ProcessStore(HStoreNamedField* store) {
  HValue* object = store->object()->ActualValue();
  int i = 0;
  while (i < unobserved_.length()) {
    HStoreNamedField* prev = unobserved_.at(i);
    if (aliasing_->MustAlias(object, prev->object()->ActualValue()) &&
        prev->CanBeReplacedWith(store)) {
      // This store is guaranteed to overwrite the previous store.
      prev->DeleteAndReplaceWith(NULL);
      TRACE(("++ Unobserved store S%d overwritten by S%d\n",
             prev->id(), store->id()));
      unobserved_.Remove(i);
    } else {
      // TODO(titzer): remove map word clearing from folded allocations.
      i++;
    }
  }
  // Only non-transitioning stores are removable.
  if (!store->has_transition()) {
    TRACE(("-- Might remove store S%d\n", store->id()));
    unobserved_.Add(store, zone());
  }
}


void HStoreEliminationPhase::ProcessLoad(HLoadNamedField* load) {
  HValue* object = load->object()->ActualValue();
  int i = 0;
  while (i < unobserved_.length()) {
    HStoreNamedField* prev = unobserved_.at(i);
    if (aliasing_->MayAlias(object, prev->object()->ActualValue()) &&
        load->access().Equals(prev->access())) {
      TRACE(("-- Observed store S%d by load L%d\n", prev->id(), load->id()));
      unobserved_.Remove(i);
    } else {
      i++;
    }
  }
}


void HStoreEliminationPhase::ProcessInstr(HInstruction* instr,
    GVNFlagSet flags) {
  if (unobserved_.length() == 0) return;  // Nothing to do.
  if (instr->CanDeoptimize()) {
    TRACE(("-- Observed stores at I%d (%s might deoptimize)\n",
           instr->id(), instr->Mnemonic()));
    unobserved_.Rewind(0);
    return;
  }
  if (instr->CheckChangesFlag(kNewSpacePromotion)) {
    TRACE(("-- Observed stores at I%d (%s might GC)\n",
           instr->id(), instr->Mnemonic()));
    unobserved_.Rewind(0);
    return;
  }
  if (instr->DependsOnFlags().ContainsAnyOf(flags)) {
    TRACE(("-- Observed stores at I%d (GVN flags of %s)\n",
           instr->id(), instr->Mnemonic()));
    unobserved_.Rewind(0);
    return;
  }
}

}  // namespace internal
}  // namespace v8
