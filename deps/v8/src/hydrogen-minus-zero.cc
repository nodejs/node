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

#include "hydrogen-minus-zero.h"

namespace v8 {
namespace internal {

void HComputeMinusZeroChecksPhase::Run() {
  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  for (int i = 0; i < blocks->length(); ++i) {
    for (HInstructionIterator it(blocks->at(i)); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (current->IsChange()) {
        HChange* change = HChange::cast(current);
        // Propagate flags for negative zero checks upwards from conversions
        // int32-to-tagged and int32-to-double.
        Representation from = change->value()->representation();
        ASSERT(from.Equals(change->from()));
        if (from.IsSmiOrInteger32()) {
          ASSERT(change->to().IsTagged() ||
                 change->to().IsDouble() ||
                 change->to().IsSmiOrInteger32());
          ASSERT(visited_.IsEmpty());
          PropagateMinusZeroChecks(change->value());
          visited_.Clear();
        }
      }
    }
  }
}


void HComputeMinusZeroChecksPhase::PropagateMinusZeroChecks(HValue* value) {
  for (HValue* current = value;
       current != NULL && !visited_.Contains(current->id());
       current = current->EnsureAndPropagateNotMinusZero(&visited_)) {
    // For phis, we must propagate the check to all of its inputs.
    if (current->IsPhi()) {
      visited_.Add(current->id());
      HPhi* phi = HPhi::cast(current);
      for (int i = 0; i < phi->OperandCount(); ++i) {
        PropagateMinusZeroChecks(phi->OperandAt(i));
      }
      break;
    }

    // For multiplication, division, and Math.min/max(), we must propagate
    // to the left and the right side.
    if (current->IsMul() || current->IsDiv() || current->IsMathMinMax()) {
      HBinaryOperation* operation = HBinaryOperation::cast(current);
      operation->EnsureAndPropagateNotMinusZero(&visited_);
      PropagateMinusZeroChecks(operation->left());
      PropagateMinusZeroChecks(operation->right());
    }
  }
}

} }  // namespace v8::internal
