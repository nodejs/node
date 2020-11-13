// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ELEMENTS_KIND_H_
#define V8_OBJECTS_ELEMENTS_KIND_H_

#include "src/base/bits.h"
#include "src/base/bounds.h"
#include "src/base/macros.h"
#include "src/common/checks.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

// V has parameters (Type, type, TYPE, C type)
#define TYPED_ARRAYS(V)                                  \
  V(Uint8, uint8, UINT8, uint8_t)                        \
  V(Int8, int8, INT8, int8_t)                            \
  V(Uint16, uint16, UINT16, uint16_t)                    \
  V(Int16, int16, INT16, int16_t)                        \
  V(Uint32, uint32, UINT32, uint32_t)                    \
  V(Int32, int32, INT32, int32_t)                        \
  V(Float32, float32, FLOAT32, float)                    \
  V(Float64, float64, FLOAT64, double)                   \
  V(Uint8Clamped, uint8_clamped, UINT8_CLAMPED, uint8_t) \
  V(BigUint64, biguint64, BIGUINT64, uint64_t)           \
  V(BigInt64, bigint64, BIGINT64, int64_t)

enum ElementsKind : uint8_t {
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

  // The nonextensible kind for elements.
  PACKED_NONEXTENSIBLE_ELEMENTS,
  HOLEY_NONEXTENSIBLE_ELEMENTS,

  // The sealed kind for elements.
  PACKED_SEALED_ELEMENTS,
  HOLEY_SEALED_ELEMENTS,

  // The frozen kind for elements.
  PACKED_FROZEN_ELEMENTS,
  HOLEY_FROZEN_ELEMENTS,

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
#define TYPED_ARRAY_ELEMENTS_KIND(Type, type, TYPE, ctype) TYPE##_ELEMENTS,
  TYPED_ARRAYS(TYPED_ARRAY_ELEMENTS_KIND)
#undef TYPED_ARRAY_ELEMENTS_KIND

  // Sentinel ElementsKind for objects with no elements.
  NO_ELEMENTS,

  // Derived constants from ElementsKind.
  FIRST_ELEMENTS_KIND = PACKED_SMI_ELEMENTS,
  LAST_ELEMENTS_KIND = BIGINT64_ELEMENTS,
  FIRST_FAST_ELEMENTS_KIND = PACKED_SMI_ELEMENTS,
  LAST_FAST_ELEMENTS_KIND = HOLEY_DOUBLE_ELEMENTS,
  FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND = UINT8_ELEMENTS,
  LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND = BIGINT64_ELEMENTS,
  TERMINAL_FAST_ELEMENTS_KIND = HOLEY_ELEMENTS,
  FIRST_ANY_NONEXTENSIBLE_ELEMENTS_KIND = PACKED_NONEXTENSIBLE_ELEMENTS,
  LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND = HOLEY_FROZEN_ELEMENTS,

// Alias for kSystemPointerSize-sized elements
#ifdef V8_COMPRESS_POINTERS
  SYSTEM_POINTER_ELEMENTS = PACKED_DOUBLE_ELEMENTS,
#else
  SYSTEM_POINTER_ELEMENTS = PACKED_ELEMENTS,
#endif
};

constexpr int kElementsKindCount = LAST_ELEMENTS_KIND - FIRST_ELEMENTS_KIND + 1;
constexpr int kFastElementsKindCount =
    LAST_FAST_ELEMENTS_KIND - FIRST_FAST_ELEMENTS_KIND + 1;

// The number to add to a packed elements kind to reach a holey elements kind
constexpr int kFastElementsKindPackedToHoley =
    HOLEY_SMI_ELEMENTS - PACKED_SMI_ELEMENTS;

constexpr int kElementsKindBits = 5;
STATIC_ASSERT((1 << kElementsKindBits) > LAST_ELEMENTS_KIND);
STATIC_ASSERT((1 << (kElementsKindBits - 1)) <= LAST_ELEMENTS_KIND);

constexpr int kFastElementsKindBits = 3;
STATIC_ASSERT((1 << kFastElementsKindBits) > LAST_FAST_ELEMENTS_KIND);
STATIC_ASSERT((1 << (kFastElementsKindBits - 1)) <= LAST_FAST_ELEMENTS_KIND);

V8_EXPORT_PRIVATE int ElementsKindToShiftSize(ElementsKind elements_kind);
V8_EXPORT_PRIVATE int ElementsKindToByteSize(ElementsKind elements_kind);
int GetDefaultHeaderSizeForElementsKind(ElementsKind elements_kind);
const char* ElementsKindToString(ElementsKind kind);

inline ElementsKind GetInitialFastElementsKind() { return PACKED_SMI_ELEMENTS; }

ElementsKind GetFastElementsKindFromSequenceIndex(int sequence_number);
int GetSequenceIndexFromFastElementsKind(ElementsKind elements_kind);

ElementsKind GetNextTransitionElementsKind(ElementsKind elements_kind);

inline bool IsDictionaryElementsKind(ElementsKind kind) {
  return kind == DICTIONARY_ELEMENTS;
}

inline bool IsFastArgumentsElementsKind(ElementsKind kind) {
  return kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
}

inline bool IsSlowArgumentsElementsKind(ElementsKind kind) {
  return kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
}

inline bool IsSloppyArgumentsElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, FAST_SLOPPY_ARGUMENTS_ELEMENTS,
                         SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
}

inline bool IsStringWrapperElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, FAST_STRING_WRAPPER_ELEMENTS,
                         SLOW_STRING_WRAPPER_ELEMENTS);
}

inline bool IsTypedArrayElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND,
                         LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND);
}

inline bool IsTerminalElementsKind(ElementsKind kind) {
  return kind == TERMINAL_FAST_ELEMENTS_KIND || IsTypedArrayElementsKind(kind);
}

inline bool IsFastElementsKind(ElementsKind kind) {
  STATIC_ASSERT(FIRST_FAST_ELEMENTS_KIND == 0);
  return kind <= LAST_FAST_ELEMENTS_KIND;
}

inline bool IsTransitionElementsKind(ElementsKind kind) {
  return IsFastElementsKind(kind) || IsTypedArrayElementsKind(kind) ||
         kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS ||
         kind == FAST_STRING_WRAPPER_ELEMENTS;
}

inline bool IsDoubleElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, PACKED_DOUBLE_ELEMENTS, HOLEY_DOUBLE_ELEMENTS);
}

inline bool IsFixedFloatElementsKind(ElementsKind kind) {
  return kind == FLOAT32_ELEMENTS || kind == FLOAT64_ELEMENTS;
}

inline bool IsDoubleOrFloatElementsKind(ElementsKind kind) {
  return IsDoubleElementsKind(kind) || IsFixedFloatElementsKind(kind);
}

// This predicate is used for disabling respective functionality in builtins.
inline bool IsAnyNonextensibleElementsKindUnchecked(ElementsKind kind) {
  return base::IsInRange(kind, FIRST_ANY_NONEXTENSIBLE_ELEMENTS_KIND,
                         LAST_ANY_NONEXTENSIBLE_ELEMENTS_KIND);
}

inline bool IsAnyNonextensibleElementsKind(ElementsKind kind) {
  DCHECK_IMPLIES(IsAnyNonextensibleElementsKindUnchecked(kind),
                 FLAG_enable_sealed_frozen_elements_kind);
  return IsAnyNonextensibleElementsKindUnchecked(kind);
}

inline bool IsNonextensibleElementsKind(ElementsKind kind) {
  DCHECK_IMPLIES(base::IsInRange(kind, PACKED_NONEXTENSIBLE_ELEMENTS,
                                 HOLEY_NONEXTENSIBLE_ELEMENTS),
                 FLAG_enable_sealed_frozen_elements_kind);
  return base::IsInRange(kind, PACKED_NONEXTENSIBLE_ELEMENTS,
                         HOLEY_NONEXTENSIBLE_ELEMENTS);
}

inline bool IsSealedElementsKind(ElementsKind kind) {
  DCHECK_IMPLIES(
      base::IsInRange(kind, PACKED_SEALED_ELEMENTS, HOLEY_SEALED_ELEMENTS),
      FLAG_enable_sealed_frozen_elements_kind);
  return base::IsInRange(kind, PACKED_SEALED_ELEMENTS, HOLEY_SEALED_ELEMENTS);
}

inline bool IsFrozenElementsKind(ElementsKind kind) {
  DCHECK_IMPLIES(
      base::IsInRange(kind, PACKED_FROZEN_ELEMENTS, HOLEY_FROZEN_ELEMENTS),
      FLAG_enable_sealed_frozen_elements_kind);
  return base::IsInRange(kind, PACKED_FROZEN_ELEMENTS, HOLEY_FROZEN_ELEMENTS);
}

inline bool IsSmiOrObjectElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, PACKED_SMI_ELEMENTS, HOLEY_ELEMENTS);
}

inline bool IsSmiElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS);
}

inline bool IsFastNumberElementsKind(ElementsKind kind) {
  return IsSmiElementsKind(kind) || IsDoubleElementsKind(kind);
}

inline bool IsObjectElementsKind(ElementsKind kind) {
  return base::IsInRange(kind, PACKED_ELEMENTS, HOLEY_ELEMENTS);
}

inline bool IsAnyHoleyNonextensibleElementsKind(ElementsKind kind) {
  DCHECK_IMPLIES(kind == HOLEY_NONEXTENSIBLE_ELEMENTS ||
                     kind == HOLEY_SEALED_ELEMENTS ||
                     kind == HOLEY_FROZEN_ELEMENTS,
                 FLAG_enable_sealed_frozen_elements_kind);
  return kind == HOLEY_NONEXTENSIBLE_ELEMENTS ||
         kind == HOLEY_SEALED_ELEMENTS || kind == HOLEY_FROZEN_ELEMENTS;
}

inline bool IsHoleyElementsKind(ElementsKind kind) {
  return kind % 2 == 1 && kind <= HOLEY_DOUBLE_ELEMENTS;
}

inline bool IsHoleyElementsKindForRead(ElementsKind kind) {
  return kind % 2 == 1 && kind <= HOLEY_FROZEN_ELEMENTS;
}

inline bool IsHoleyOrDictionaryElementsKind(ElementsKind kind) {
  return IsHoleyElementsKindForRead(kind) || kind == DICTIONARY_ELEMENTS;
}

inline bool IsFastPackedElementsKind(ElementsKind kind) {
  return kind % 2 == 0 && kind <= PACKED_DOUBLE_ELEMENTS;
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
  if (packed_kind == PACKED_NONEXTENSIBLE_ELEMENTS) {
    return HOLEY_NONEXTENSIBLE_ELEMENTS;
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

inline bool ElementsKindEqual(ElementsKind a, ElementsKind b) { return a == b; }

enum class CompactElementsKind : uint8_t {
  PACKED_SMI_ELEMENTS = PACKED_SMI_ELEMENTS,
  HOLEY_SMI_ELEMENTS = HOLEY_SMI_ELEMENTS,

  PACKED_ELEMENTS = PACKED_ELEMENTS,
  HOLEY_ELEMENTS = HOLEY_ELEMENTS,

  PACKED_DOUBLE_ELEMENTS = PACKED_DOUBLE_ELEMENTS,
  HOLEY_DOUBLE_ELEMENTS = HOLEY_DOUBLE_ELEMENTS,

  NON_COMPACT_ELEMENTS_KIND
};

inline CompactElementsKind ToCompactElementsKind(ElementsKind kind) {
  if (base::IsInRange(kind, PACKED_SMI_ELEMENTS, HOLEY_DOUBLE_ELEMENTS)) {
    return static_cast<CompactElementsKind>(kind);
  }

  return CompactElementsKind::NON_COMPACT_ELEMENTS_KIND;
}

const char* CompactElementsKindToString(CompactElementsKind kind);

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_ELEMENTS_KIND_H_
