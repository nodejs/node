// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  if (value >= 1 << array_operation->MaxIndexOffsetBits() || value < 0) return;
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
