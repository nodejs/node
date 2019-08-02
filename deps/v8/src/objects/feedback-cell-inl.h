// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FEEDBACK_CELL_INL_H_
#define V8_OBJECTS_FEEDBACK_CELL_INL_H_

#include "src/objects/feedback-cell.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(FeedbackCell, Struct)

CAST_ACCESSOR(FeedbackCell)

ACCESSORS(FeedbackCell, value, HeapObject, kValueOffset)
INT32_ACCESSORS(FeedbackCell, interrupt_budget, kInterruptBudgetOffset)

void FeedbackCell::clear_padding() {
  if (FeedbackCell::kAlignedSize == FeedbackCell::kUnalignedSize) return;
  DCHECK_GE(FeedbackCell::kAlignedSize, FeedbackCell::kUnalignedSize);
  memset(reinterpret_cast<byte*>(address() + FeedbackCell::kUnalignedSize), 0,
         FeedbackCell::kAlignedSize - FeedbackCell::kUnalignedSize);
}

void FeedbackCell::reset() {
  set_value(GetReadOnlyRoots().undefined_value());
  set_interrupt_budget(FeedbackCell::GetInitialInterruptBudget());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FEEDBACK_CELL_INL_H_
