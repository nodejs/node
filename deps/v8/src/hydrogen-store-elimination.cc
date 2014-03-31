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

#include "hydrogen-store-elimination.h"
#include "hydrogen-instructions.h"

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
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();

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
        store->access().Equals(prev->access())) {
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
    TRACE(("-- Observed stores at I%d (might deoptimize)\n", instr->id()));
    unobserved_.Rewind(0);
    return;
  }
  if (instr->CheckChangesFlag(kNewSpacePromotion)) {
    TRACE(("-- Observed stores at I%d (might GC)\n", instr->id()));
    unobserved_.Rewind(0);
    return;
  }
  if (instr->ChangesFlags().ContainsAnyOf(flags)) {
    TRACE(("-- Observed stores at I%d (GVN flags)\n", instr->id()));
    unobserved_.Rewind(0);
    return;
  }
}

} }  // namespace v8::internal
