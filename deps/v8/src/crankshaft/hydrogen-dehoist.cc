// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-dehoist.h"

#include "src/base/safe_math.h"

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
  v8::base::internal::CheckedNumeric<int32_t> checked_value =
      constant->Integer32Value();
  int32_t sign = binary_operation->IsSub() ? -1 : 1;
  checked_value = checked_value * sign;

  // Multiply value by elements size, bailing out on overflow.
  int32_t elements_kind_size =
      1 << ElementsKindToShiftSize(array_operation->elements_kind());
  checked_value = checked_value * elements_kind_size;
  if (!checked_value.IsValid()) return;
  int32_t value = checked_value.ValueOrDie();
  if (value < 0) return;

  // Ensure that the array operation can add value to existing base offset
  // without overflowing.
  if (!array_operation->TryIncreaseBaseOffset(value)) return;

  array_operation->SetKey(subexpression);
  if (binary_operation->HasNoUses()) {
    binary_operation->DeleteAndReplaceWith(NULL);
  }

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

}  // namespace internal
}  // namespace v8
