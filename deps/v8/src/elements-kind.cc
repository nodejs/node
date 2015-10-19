// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/elements-kind.h"

#include "src/api.h"
#include "src/base/lazy-instance.h"
#include "src/elements.h"
#include "src/objects.h"

namespace v8 {
namespace internal {


int ElementsKindToShiftSize(ElementsKind elements_kind) {
  switch (elements_kind) {
    case UINT8_ELEMENTS:
    case INT8_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
      return 0;
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
      return 1;
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
    case FLOAT32_ELEMENTS:
      return 2;
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FLOAT64_ELEMENTS:
      return 3;
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      return kPointerSizeLog2;
  }
  UNREACHABLE();
  return 0;
}


int GetDefaultHeaderSizeForElementsKind(ElementsKind elements_kind) {
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);

  if (IsFixedTypedArrayElementsKind(elements_kind)) {
    return 0;
  } else {
    return FixedArray::kHeaderSize - kHeapObjectTag;
  }
}


const char* ElementsKindToString(ElementsKind kind) {
  ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
  return accessor->name();
}


struct InitializeFastElementsKindSequence {
  static void Construct(
      ElementsKind** fast_elements_kind_sequence_ptr) {
    ElementsKind* fast_elements_kind_sequence =
        new ElementsKind[kFastElementsKindCount];
    *fast_elements_kind_sequence_ptr = fast_elements_kind_sequence;
    STATIC_ASSERT(FAST_SMI_ELEMENTS == FIRST_FAST_ELEMENTS_KIND);
    fast_elements_kind_sequence[0] = FAST_SMI_ELEMENTS;
    fast_elements_kind_sequence[1] = FAST_HOLEY_SMI_ELEMENTS;
    fast_elements_kind_sequence[2] = FAST_DOUBLE_ELEMENTS;
    fast_elements_kind_sequence[3] = FAST_HOLEY_DOUBLE_ELEMENTS;
    fast_elements_kind_sequence[4] = FAST_ELEMENTS;
    fast_elements_kind_sequence[5] = FAST_HOLEY_ELEMENTS;

    // Verify that kFastElementsKindPackedToHoley is correct.
    STATIC_ASSERT(FAST_SMI_ELEMENTS + kFastElementsKindPackedToHoley ==
                  FAST_HOLEY_SMI_ELEMENTS);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS + kFastElementsKindPackedToHoley ==
                  FAST_HOLEY_DOUBLE_ELEMENTS);
    STATIC_ASSERT(FAST_ELEMENTS + kFastElementsKindPackedToHoley ==
                  FAST_HOLEY_ELEMENTS);
  }
};


static base::LazyInstance<ElementsKind*,
                          InitializeFastElementsKindSequence>::type
    fast_elements_kind_sequence = LAZY_INSTANCE_INITIALIZER;


ElementsKind GetFastElementsKindFromSequenceIndex(int sequence_number) {
  DCHECK(sequence_number >= 0 &&
         sequence_number < kFastElementsKindCount);
  return fast_elements_kind_sequence.Get()[sequence_number];
}


int GetSequenceIndexFromFastElementsKind(ElementsKind elements_kind) {
  for (int i = 0; i < kFastElementsKindCount; ++i) {
    if (fast_elements_kind_sequence.Get()[i] == elements_kind) {
      return i;
    }
  }
  UNREACHABLE();
  return 0;
}


ElementsKind GetNextTransitionElementsKind(ElementsKind kind) {
  int index = GetSequenceIndexFromFastElementsKind(kind);
  return GetFastElementsKindFromSequenceIndex(index + 1);
}


static inline bool IsFastTransitionTarget(ElementsKind elements_kind) {
  return IsFastElementsKind(elements_kind) ||
      elements_kind == DICTIONARY_ELEMENTS;
}

bool IsMoreGeneralElementsKindTransition(ElementsKind from_kind,
                                         ElementsKind to_kind) {
  if (IsFixedTypedArrayElementsKind(from_kind) ||
      IsFixedTypedArrayElementsKind(to_kind)) {
    return false;
  }
  if (IsFastElementsKind(from_kind) && IsFastTransitionTarget(to_kind)) {
    switch (from_kind) {
      case FAST_SMI_ELEMENTS:
        return to_kind != FAST_SMI_ELEMENTS;
      case FAST_HOLEY_SMI_ELEMENTS:
        return to_kind != FAST_SMI_ELEMENTS &&
            to_kind != FAST_HOLEY_SMI_ELEMENTS;
      case FAST_DOUBLE_ELEMENTS:
        return to_kind != FAST_SMI_ELEMENTS &&
            to_kind != FAST_HOLEY_SMI_ELEMENTS &&
            to_kind != FAST_DOUBLE_ELEMENTS;
      case FAST_HOLEY_DOUBLE_ELEMENTS:
        return to_kind == FAST_ELEMENTS ||
            to_kind == FAST_HOLEY_ELEMENTS;
      case FAST_ELEMENTS:
        return to_kind == FAST_HOLEY_ELEMENTS;
      case FAST_HOLEY_ELEMENTS:
        return false;
      default:
        return false;
    }
  }
  return false;
}


}  // namespace internal
}  // namespace v8
