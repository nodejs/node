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

#include "hydrogen-deoptimizing-mark.h"

namespace v8 {
namespace internal {

void HPropagateDeoptimizingMarkPhase::MarkAsDeoptimizing() {
  HBasicBlock* block = graph()->entry_block();
  ZoneList<HBasicBlock*> stack(graph()->blocks()->length(), zone());
  while (block != NULL) {
    const ZoneList<HBasicBlock*>* dominated_blocks(block->dominated_blocks());
    if (!dominated_blocks->is_empty()) {
      if (block->IsDeoptimizing()) {
        for (int i = 0; i < dominated_blocks->length(); ++i) {
          dominated_blocks->at(i)->MarkAsDeoptimizing();
        }
      }
      for (int i = 1; i < dominated_blocks->length(); ++i) {
        stack.Add(dominated_blocks->at(i), zone());
      }
      block = dominated_blocks->at(0);
    } else if (!stack.is_empty()) {
      // Pop next block from stack.
      block = stack.RemoveLast();
    } else {
      // All blocks processed.
      block = NULL;
    }
  }
}


void HPropagateDeoptimizingMarkPhase::NullifyUnreachableInstructions() {
  if (!FLAG_unreachable_code_elimination) return;
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    bool nullify = false;
    const ZoneList<HBasicBlock*>* predecessors = block->predecessors();
    int predecessors_length = predecessors->length();
    bool all_predecessors_deoptimizing = (predecessors_length > 0);
    for (int j = 0; j < predecessors_length; ++j) {
      if (!predecessors->at(j)->IsDeoptimizing()) {
        all_predecessors_deoptimizing = false;
        break;
      }
    }
    if (all_predecessors_deoptimizing) nullify = true;
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      // Leave the basic structure of the graph intact.
      if (instr->IsBlockEntry()) continue;
      if (instr->IsControlInstruction()) continue;
      if (instr->IsSimulate()) continue;
      if (instr->IsEnterInlined()) continue;
      if (instr->IsLeaveInlined()) continue;
      if (nullify) {
        HInstruction* last_dummy = NULL;
        for (int j = 0; j < instr->OperandCount(); ++j) {
          HValue* operand = instr->OperandAt(j);
          // Insert an HDummyUse for each operand, unless the operand
          // is an HDummyUse itself. If it's even from the same block,
          // remember it as a potential replacement for the instruction.
          if (operand->IsDummyUse()) {
            if (operand->block() == instr->block() &&
                last_dummy == NULL) {
              last_dummy = HInstruction::cast(operand);
            }
            continue;
          }
          if (operand->IsControlInstruction()) {
            // Inserting a dummy use for a value that's not defined anywhere
            // will fail. Some instructions define fake inputs on such
            // values as control flow dependencies.
            continue;
          }
          HDummyUse* dummy = new(graph()->zone()) HDummyUse(operand);
          dummy->InsertBefore(instr);
          last_dummy = dummy;
        }
        if (last_dummy == NULL) last_dummy = graph()->GetConstant1();
        instr->DeleteAndReplaceWith(last_dummy);
        continue;
      }
      if (instr->IsDeoptimize()) {
        ASSERT(block->IsDeoptimizing());
        nullify = true;
      }
    }
  }
}


void HPropagateDeoptimizingMarkPhase::Run() {
  // Skip this phase if there is nothing to be done anyway.
  if (!graph()->has_soft_deoptimize()) return;
  MarkAsDeoptimizing();
  NullifyUnreachableInstructions();
}

} }  // namespace v8::internal
