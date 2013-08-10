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

#include "hydrogen-dehoist.h"

namespace v8 {
namespace internal {

static void DehoistArrayIndex(ArrayInstructionInterface* array_operation) {
  HValue* index = array_operation->GetKey()->ActualValue();
  if (!index->representation().IsSmiOrInteger32()) return;
  if (!index->IsAdd() && !index->IsSub()) return;

  HConstant* constant;
  HValue* subexpression;
  HBinaryOperation* binary_operation = HBinaryOperation::cast(index);
  if (binary_operation->left()->IsConstant() && index->IsAdd()) {
    subexpression = binary_operation->right();
    constant = HConstant::cast(binary_operation->left());
  } else if (binary_operation->right()->IsConstant()) {
    subexpression = binary_operation->left();
    constant = HConstant::cast(binary_operation->right());
  } else {
    return;
  }

  if (!constant->HasInteger32Value()) return;
  int32_t sign = binary_operation->IsSub() ? -1 : 1;
  int32_t value = constant->Integer32Value() * sign;
  // We limit offset values to 30 bits because we want to avoid the risk of
  // overflows when the offset is added to the object header size.
  if (value >= 1 << 30 || value < 0) return;
  array_operation->SetKey(subexpression);
  if (binary_operation->HasNoUses()) {
    binary_operation->DeleteAndReplaceWith(NULL);
  }
  array_operation->SetIndexOffset(static_cast<uint32_t>(value));
  array_operation->SetDehoisted(true);
}


void HDehoistIndexComputationsPhase::Run() {
  const ZoneList<HBasicBlock*>* blocks(graph()->blocks());
  for (int i = 0; i < blocks->length(); ++i) {
    for (HInstructionIterator it(blocks->at(i)); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->IsLoadKeyed()) {
        DehoistArrayIndex(HLoadKeyed::cast(instr));
      } else if (instr->IsStoreKeyed()) {
        DehoistArrayIndex(HStoreKeyed::cast(instr));
      }
    }
  }
}

} }  // namespace v8::internal
