// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ELEMENTS_KIND_H_
#define V8_ELEMENTS_KIND_H_

#include "src/base/macros.h"
#include "src/checks.h"

namespace v8 {
namespace internal {

enum ElementsKind {
  // The "fast" kind for elements that only contain SMI values. Must be first
  // to make it possible to efficiently check maps for this kind.
  PACKED_SMI_ELEMENTS,
  HOLEY_SMI_ELEMENTS,

  // The "fast" kind for tagged values. Must be second to make it possible to
  // efficiently check maps for this and the PACKED_SMI_ELEMENTS kind
  // together at once.
  PACKED_ELEMENTS,
  HOLEY_ELEMENTS,

  // The "fast" kind for unwrapped, non-tagged double values.
  PACKED_DOUBLE_ELEMENTS,
  HOLEY_DOUBLE_ELEMENTS,

  // The "slow" kind.
  DICTIONARY_ELEMENTS,

  // Elements kind of the "arguments" object (only in sloppy mode).
  FAST_SLOPPY_ARGUMENTS_ELEMENTS,
  SLOW_SLOPPY_ARGUMENTS_ELEMENTS,

  // For string wrapper objects ("new String('...')"), the string's characters
  // are overlaid onto a regular elements backing store.
  FAST_STRING_WRAPPER_ELEMENTS,
  SLOW_STRING_WRAPPER_ELEMENTS,

  // Fixed typed arrays.
  UINT8_ELEMENTS,
  INT8_ELEMENTS,
  UINT16_ELEMENTS,
  INT16_ELEMENTS,
  UINT32_ELEMENTS,
  INT32_ELEMENTS,
  FLOAT32_ELEMENTS,
  FLOAT64_ELEMENTS,
  UINT8_CLAMPED_ELEMENTS,
  BIGUINT64_ELEMENTS,
  BIGINT64_ELEMENTS,

  // Sentinel ElementsKind for objects with no elements.
  NO_ELEMENTS,

  // Derived constants from ElementsKind.
  FIRST_ELEMENTS_KIND = PACKED_SMI_ELEMENTS,
  LAST_ELEMENTS_KIND = BIGINT64_ELEMENTS,
  FIRST_FAST_ELEMENTS_KIND = PACKED_SMI_ELEMENTS,
  LAST_FAST_ELEMENTS_KIND = HOLEY_DOUBLE_ELEMENTS,
  FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND = UINT8_ELEMENTS,
  LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND = BIGINT64_ELEMENTS,
  TERMINAL_FAST_ELEMENTS_KIND = HOLEY_ELEMENTS
};

const int kElementsKindCount = LAST_ELEMENTS_KIND - FIRST_ELEMENTS_KIND + 1;
const int kFastElementsKindCount =
    LAST_FAST_ELEMENTS_KIND - FIRST_FAST_ELEMENTS_KIND + 1;

// The number to add to a packed elements kind to reach a holey elements kind
const int kFastElementsKindPackedToHoley =
    HOLEY_SMI_ELEMENTS - PACKED_SMI_ELEMENTS;

int ElementsKindToShiftSize(ElementsKind elements_kind);
int GetDefaultHeaderSizeForElementsKind(ElementsKind elements_kind);
const char* ElementsKindToString(ElementsKind kind);

inline ElementsKind GetInitialFastElementsKind() { return PACKED_SMI_ELEMENTS; }

ElementsKind GetFastElementsKindFromSequenceIndex(int sequence_number);
int GetSequenceIndexFromFastElementsKind(ElementsKind elements_kind);

ElementsKind GetNextTransitionElementsKind(ElementsKind elements_kind);

inline bool IsDictionaryElementsKind(ElementsKind kind) {
  return kind == DICTIONARY_ELEMENTS;
}

inline bool IsSloppyArgumentsElementsKind(ElementsKind kind) {
  return kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS ||
         kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
}

inline bool IsStringWrapperElementsKind(ElementsKind kind) {
  return kind == FAST_STRING_WRAPPER_ELEMENTS ||
         kind == SLOW_STRING_WRAPPER_ELEMENTS;
}

inline bool IsFixedTypedArrayElementsKind(ElementsKind kind) {
  return kind >= FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND &&
         kind <= LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND;
}

inline bool IsTerminalElementsKind(ElementsKind kind) {
  return kind == TERMINAL_FAST_ELEMENTS_KIND ||
         IsFixedTypedArrayElementsKind(kind);
}

inline bool IsFastElementsKind(ElementsKind kind) {
  STATIC_ASSERT(FIRST_FAST_ELEMENTS_KIND == 0);
  return kind <= HOLEY_DOUBLE_ELEMENTS;
}

inline bool IsTransitionElementsKind(ElementsKind kind) {
  return IsFastElementsKind(kind) || IsFixedTypedArrayElementsKind(kind) ||
         kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS ||
         kind == FAST_STRING_WRAPPER_ELEMENTS;
}

inline bool IsDoubleElementsKind(ElementsKind kind) {
  return kind == PACKED_DOUBLE_ELEMENTS || kind == HOLEY_DOUBLE_ELEMENTS;
}


inline bool IsFixedFloatElementsKind(ElementsKind kind) {
  return kind == FLOAT32_ELEMENTS || kind == FLOAT64_ELEMENTS;
}


inline bool IsDoubleOrFloatElementsKind(ElementsKind kind) {
  return IsDoubleElementsKind(kind) || IsFixedFloatElementsKind(kind);
}

inline bool IsSmiOrObjectElementsKind(ElementsKind kind) {
  return kind == PACKED_SMI_ELEMENTS || kind == HOLEY_SMI_ELEMENTS ||
         kind == PACKED_ELEMENTS || kind == HOLEY_ELEMENTS;
}

inline bool IsSmiElementsKind(ElementsKind kind) {
  return kind == PACKED_SMI_ELEMENTS || kind == HOLEY_SMI_ELEMENTS;
}

inline bool IsFastNumberElementsKind(ElementsKind kind) {
  return IsSmiElementsKind(kind) || IsDoubleElementsKind(kind);
}

inline bool IsObjectElementsKind(ElementsKind kind) {
  return kind == PACKED_ELEMENTS || kind == HOLEY_ELEMENTS;
}

inline bool IsHoleyElementsKind(ElementsKind kind) {
  return kind == HOLEY_SMI_ELEMENTS || kind == HOLEY_DOUBLE_ELEMENTS ||
         kind == HOLEY_ELEMENTS;
}

inline bool IsHoleyOrDictionaryElementsKind(ElementsKind kind) {
  return IsHoleyElementsKind(kind) || kind == DICTIONARY_ELEMENTS;
}


inline bool IsFastPackedElementsKind(ElementsKind kind) {
  return kind == PACKED_SMI_ELEMENTS || kind == PACKED_DOUBLE_ELEMENTS ||
         kind == PACKED_ELEMENTS;
}


inline ElementsKind GetPackedElementsKind(ElementsKind holey_kind) {
  if (holey_kind == HOLEY_SMI_ELEMENTS) {
    return PACKED_SMI_ELEMENTS;
  }
  if (holey_kind == HOLEY_DOUBLE_ELEMENTS) {
    return PACKED_DOUBLE_ELEMENTS;
  }
  if (holey_kind == HOLEY_ELEMENTS) {
    return PACKED_ELEMENTS;
  }
  return holey_kind;
}


inline ElementsKind GetHoleyElementsKind(ElementsKind packed_kind) {
  if (packed_kind == PACKED_SMI_ELEMENTS) {
    return HOLEY_SMI_ELEMENTS;
  }
  if (packed_kind == PACKED_DOUBLE_ELEMENTS) {
    return HOLEY_DOUBLE_ELEMENTS;
  }
  if (packed_kind == PACKED_ELEMENTS) {
    return HOLEY_ELEMENTS;
  }
  return packed_kind;
}

inline bool UnionElementsKindUptoPackedness(ElementsKind* a_out,
                                            ElementsKind b) {
  // Assert that the union of two ElementKinds can be computed via std::max.
  static_assert(PACKED_SMI_ELEMENTS < HOLEY_SMI_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  static_assert(PACKED_ELEMENTS < HOLEY_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  static_assert(PACKED_DOUBLE_ELEMENTS < HOLEY_DOUBLE_ELEMENTS,
                "ElementsKind union not computable via std::max.");
  ElementsKind a = *a_out;
  switch (a) {
    case HOLEY_SMI_ELEMENTS:
    case PACKED_SMI_ELEMENTS:
      if (b == PACKED_SMI_ELEMENTS || b == HOLEY_SMI_ELEMENTS) {
        *a_out = std::max(a, b);
        return true;
      }
      break;
    case PACKED_ELEMENTS:
    case HOLEY_ELEMENTS:
      if (b == PACKED_ELEMENTS || b == HOLEY_ELEMENTS) {
        *a_out = std::max(a, b);
        return true;
      }
      break;
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS:
      if (b == PACKED_DOUBLE_ELEMENTS || b == HOLEY_DOUBLE_ELEMENTS) {
        *a_out = std::max(a, b);
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

bool UnionElementsKindUptoSize(ElementsKind* a_out, ElementsKind b);

inline ElementsKind FastSmiToObjectElementsKind(ElementsKind from_kind) {
  DCHECK(IsSmiElementsKind(from_kind));
  return (from_kind == PACKED_SMI_ELEMENTS) ? PACKED_ELEMENTS : HOLEY_ELEMENTS;
}


inline bool IsSimpleMapChangeTransition(ElementsKind from_kind,
                                        ElementsKind to_kind) {
  return (GetHoleyElementsKind(from_kind) == to_kind) ||
         (IsSmiElementsKind(from_kind) && IsObjectElementsKind(to_kind));
}


bool IsMoreGeneralElementsKindTransition(ElementsKind from_kind,
                                         ElementsKind to_kind);


inline ElementsKind GetMoreGeneralElementsKind(ElementsKind from_kind,
                                               ElementsKind to_kind) {
  if (IsMoreGeneralElementsKindTransition(from_kind, to_kind)) {
    return to_kind;
  }
  return from_kind;
}


inline bool IsTransitionableFastElementsKind(ElementsKind from_kind) {
  return IsFastElementsKind(from_kind) &&
         from_kind != TERMINAL_FAST_ELEMENTS_KIND;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_ELEMENTS_KIND_H_
