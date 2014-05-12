// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hydrogen-canonicalize.h"
#include "hydrogen-redundant-phi.h"

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
  HRedundantPhiEliminationPhase redundant_phi_eliminator(graph());
  for (int i = 0; i < blocks->length(); ++i) {
    // Eliminate redundant phis in the block first; changes to their inputs
    // might have made them redundant, and eliminating them creates more
    // opportunities for constant folding and strength reduction.
    redundant_phi_eliminator.ProcessBlock(blocks->at(i));
    // Now canonicalize each instruction.
    for (HInstructionIterator it(blocks->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      HValue* value = instr->Canonicalize();
      if (value != instr) instr->DeleteAndReplaceWith(value);
    }
  }
}

} }  // namespace v8::internal
