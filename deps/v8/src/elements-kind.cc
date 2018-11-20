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
    case EXTERNAL_INT8_ELEMENTS:
    case EXTERNAL_UINT8_CLAMPED_ELEMENTS:
    case EXTERNAL_UINT8_ELEMENTS:
    case UINT8_ELEMENTS:
    case INT8_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
      return 0;
    case EXTERNAL_INT16_ELEMENTS:
    case EXTERNAL_UINT16_ELEMENTS:
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
      return 1;
    case EXTERNAL_INT32_ELEMENTS:
    case EXTERNAL_UINT32_ELEMENTS:
    case EXTERNAL_FLOAT32_ELEMENTS:
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
    case FLOAT32_ELEMENTS:
      return 2;
    case EXTERNAL_FLOAT64_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FLOAT64_ELEMENTS:
      return 3;
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case SLOPPY_ARGUMENTS_ELEMENTS:
      return kPointerSizeLog2;
  }
  UNREACHABLE();
  return 0;
}


int GetDefaultHeaderSizeForElementsKind(ElementsKind elements_kind) {
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  return IsExternalArrayElementsKind(elements_kind)
      ? 0 : (FixedArray::kHeaderSize - kHeapObjectTag);
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
  switch (kind) {
#define FIXED_TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
    case TYPE##_ELEMENTS: return EXTERNAL_##TYPE##_ELEMENTS;

    TYPED_ARRAYS(FIXED_TYPED_ARRAY_CASE)
#undef FIXED_TYPED_ARRAY_CASE
    default: {
      int index = GetSequenceIndexFromFastElementsKind(kind);
      return GetFastElementsKindFromSequenceIndex(index + 1);
    }
  }
}


ElementsKind GetNextMoreGeneralFastElementsKind(ElementsKind elements_kind,
                                                bool allow_only_packed) {
  DCHECK(IsFastElementsKind(elements_kind));
  DCHECK(elements_kind != TERMINAL_FAST_ELEMENTS_KIND);
  while (true) {
    elements_kind = GetNextTransitionElementsKind(elements_kind);
    if (!IsFastHoleyElementsKind(elements_kind) || !allow_only_packed) {
      return elements_kind;
    }
  }
  UNREACHABLE();
  return TERMINAL_FAST_ELEMENTS_KIND;
}


static bool IsTypedArrayElementsKind(ElementsKind elements_kind) {
  return IsFixedTypedArrayElementsKind(elements_kind) ||
      IsExternalArrayElementsKind(elements_kind);
}


static inline bool IsFastTransitionTarget(ElementsKind elements_kind) {
  return IsFastElementsKind(elements_kind) ||
      elements_kind == DICTIONARY_ELEMENTS;
}

bool IsMoreGeneralElementsKindTransition(ElementsKind from_kind,
                                         ElementsKind to_kind) {
  if (IsTypedArrayElementsKind(from_kind) ||
      IsTypedArrayElementsKind(to_kind)) {
    switch (from_kind) {
#define FIXED_TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
      case TYPE##_ELEMENTS:                                   \
        return to_kind == EXTERNAL_##TYPE##_ELEMENTS;

      TYPED_ARRAYS(FIXED_TYPED_ARRAY_CASE);
#undef FIXED_TYPED_ARRAY_CASE
      default:
        return false;
    }
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


} }  // namespace v8::internal
