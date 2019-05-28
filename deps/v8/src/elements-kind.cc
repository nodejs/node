// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/elements-kind.h"

#include "src/base/lazy-instance.h"
#include "src/elements.h"
#include "src/objects-inl.h"
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
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS:
    case FLOAT64_ELEMENTS:
    case BIGINT64_ELEMENTS:
    case BIGUINT64_ELEMENTS:
      return 3;
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      return kTaggedSizeLog2;
    case NO_ELEMENTS:
      UNREACHABLE();
  }
  UNREACHABLE();
}

int ElementsKindToByteSize(ElementsKind elements_kind) {
  return 1 << ElementsKindToShiftSize(elements_kind);
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

ElementsKind kFastElementsKindSequence[kFastElementsKindCount] = {
    PACKED_SMI_ELEMENTS,     // 0
    HOLEY_SMI_ELEMENTS,      // 1
    PACKED_DOUBLE_ELEMENTS,  // 2
    HOLEY_DOUBLE_ELEMENTS,   // 3
    PACKED_ELEMENTS,         // 4
    HOLEY_ELEMENTS           // 5
};
STATIC_ASSERT(PACKED_SMI_ELEMENTS == FIRST_FAST_ELEMENTS_KIND);
// Verify that kFastElementsKindPackedToHoley is correct.
STATIC_ASSERT(PACKED_SMI_ELEMENTS + kFastElementsKindPackedToHoley ==
              HOLEY_SMI_ELEMENTS);
STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS + kFastElementsKindPackedToHoley ==
              HOLEY_DOUBLE_ELEMENTS);
STATIC_ASSERT(PACKED_ELEMENTS + kFastElementsKindPackedToHoley ==
              HOLEY_ELEMENTS);

ElementsKind GetFastElementsKindFromSequenceIndex(int sequence_number) {
  DCHECK(sequence_number >= 0 &&
         sequence_number < kFastElementsKindCount);
  return kFastElementsKindSequence[sequence_number];
}

int GetSequenceIndexFromFastElementsKind(ElementsKind elements_kind) {
  for (int i = 0; i < kFastElementsKindCount; ++i) {
    if (kFastElementsKindSequence[i] == elements_kind) {
      return i;
    }
  }
  UNREACHABLE();
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
  if (!IsFastElementsKind(from_kind)) return false;
  if (!IsFastTransitionTarget(to_kind)) return false;
  DCHECK(!IsFixedTypedArrayElementsKind(from_kind));
  DCHECK(!IsFixedTypedArrayElementsKind(to_kind));
  switch (from_kind) {
    case PACKED_SMI_ELEMENTS:
      return to_kind != PACKED_SMI_ELEMENTS;
    case HOLEY_SMI_ELEMENTS:
      return to_kind != PACKED_SMI_ELEMENTS && to_kind != HOLEY_SMI_ELEMENTS;
    case PACKED_DOUBLE_ELEMENTS:
      return to_kind != PACKED_SMI_ELEMENTS && to_kind != HOLEY_SMI_ELEMENTS &&
             to_kind != PACKED_DOUBLE_ELEMENTS;
    case HOLEY_DOUBLE_ELEMENTS:
      return to_kind == PACKED_ELEMENTS || to_kind == HOLEY_ELEMENTS;
    case PACKED_ELEMENTS:
      return to_kind == HOLEY_ELEMENTS;
    case HOLEY_ELEMENTS:
      return false;
    default:
      return false;
  }
}

bool UnionElementsKindUptoSize(ElementsKind* a_out, ElementsKind b) {
  // Assert that the union of two ElementKinds can be computed via std::max.
  static_assert(PACKED_SMI_ELEMENTS < HOLEY_SMI_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  static_assert(HOLEY_SMI_ELEMENTS < PACKED_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  static_assert(PACKED_ELEMENTS < HOLEY_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  static_assert(PACKED_DOUBLE_ELEMENTS < HOLEY_DOUBLE_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  ElementsKind a = *a_out;
  switch (a) {
    case PACKED_SMI_ELEMENTS:
      switch (b) {
        case PACKED_SMI_ELEMENTS:
        case HOLEY_SMI_ELEMENTS:
        case PACKED_ELEMENTS:
        case HOLEY_ELEMENTS:
          *a_out = b;
          return true;
        default:
          return false;
      }
    case HOLEY_SMI_ELEMENTS:
      switch (b) {
        case PACKED_SMI_ELEMENTS:
        case HOLEY_SMI_ELEMENTS:
          *a_out = HOLEY_SMI_ELEMENTS;
          return true;
        case PACKED_ELEMENTS:
        case HOLEY_ELEMENTS:
          *a_out = HOLEY_ELEMENTS;
          return true;
        default:
          return false;
      }
    case PACKED_ELEMENTS:
      switch (b) {
        case PACKED_SMI_ELEMENTS:
        case PACKED_ELEMENTS:
          *a_out = PACKED_ELEMENTS;
          return true;
        case HOLEY_SMI_ELEMENTS:
        case HOLEY_ELEMENTS:
          *a_out = HOLEY_ELEMENTS;
          return true;
        default:
          return false;
      }
    case HOLEY_ELEMENTS:
      switch (b) {
        case PACKED_SMI_ELEMENTS:
        case HOLEY_SMI_ELEMENTS:
        case PACKED_ELEMENTS:
        case HOLEY_ELEMENTS:
          *a_out = HOLEY_ELEMENTS;
          return true;
        default:
          return false;
      }
      break;
    case PACKED_DOUBLE_ELEMENTS:
      switch (b) {
        case PACKED_DOUBLE_ELEMENTS:
        case HOLEY_DOUBLE_ELEMENTS:
          *a_out = b;
          return true;
        default:
          return false;
      }
    case HOLEY_DOUBLE_ELEMENTS:
      switch (b) {
        case PACKED_DOUBLE_ELEMENTS:
        case HOLEY_DOUBLE_ELEMENTS:
          *a_out = HOLEY_DOUBLE_ELEMENTS;
          return true;
        default:
          return false;
      }

      break;
    default:
      break;
  }
  return false;
}

}  // namespace internal
}  // namespace v8
