// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_ELEMENTS_KIND_H_
#define V8_ELEMENTS_KIND_H_

#include "v8checks.h"

namespace v8 {
namespace internal {

enum ElementsKind {
  // The "fast" kind for elements that only contain SMI values. Must be first
  // to make it possible to efficiently check maps for this kind.
  FAST_SMI_ELEMENTS,
  FAST_HOLEY_SMI_ELEMENTS,

  // The "fast" kind for tagged values. Must be second to make it possible to
  // efficiently check maps for this and the FAST_SMI_ONLY_ELEMENTS kind
  // together at once.
  FAST_ELEMENTS,
  FAST_HOLEY_ELEMENTS,

  // The "fast" kind for unwrapped, non-tagged double values.
  FAST_DOUBLE_ELEMENTS,
  FAST_HOLEY_DOUBLE_ELEMENTS,

  // The "slow" kind.
  DICTIONARY_ELEMENTS,
  NON_STRICT_ARGUMENTS_ELEMENTS,
  // The "fast" kind for external arrays
  EXTERNAL_BYTE_ELEMENTS,
  EXTERNAL_UNSIGNED_BYTE_ELEMENTS,
  EXTERNAL_SHORT_ELEMENTS,
  EXTERNAL_UNSIGNED_SHORT_ELEMENTS,
  EXTERNAL_INT_ELEMENTS,
  EXTERNAL_UNSIGNED_INT_ELEMENTS,
  EXTERNAL_FLOAT_ELEMENTS,
  EXTERNAL_DOUBLE_ELEMENTS,
  EXTERNAL_PIXEL_ELEMENTS,

  // Derived constants from ElementsKind
  FIRST_ELEMENTS_KIND = FAST_SMI_ELEMENTS,
  LAST_ELEMENTS_KIND = EXTERNAL_PIXEL_ELEMENTS,
  FIRST_FAST_ELEMENTS_KIND = FAST_SMI_ELEMENTS,
  LAST_FAST_ELEMENTS_KIND = FAST_HOLEY_DOUBLE_ELEMENTS,
  FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND = EXTERNAL_BYTE_ELEMENTS,
  LAST_EXTERNAL_ARRAY_ELEMENTS_KIND = EXTERNAL_PIXEL_ELEMENTS,
  TERMINAL_FAST_ELEMENTS_KIND = FAST_HOLEY_ELEMENTS
};

const int kElementsKindCount = LAST_ELEMENTS_KIND - FIRST_ELEMENTS_KIND + 1;
const int kFastElementsKindCount = LAST_FAST_ELEMENTS_KIND -
    FIRST_FAST_ELEMENTS_KIND + 1;

const char* ElementsKindToString(ElementsKind kind);
void PrintElementsKind(FILE* out, ElementsKind kind);

ElementsKind GetInitialFastElementsKind();

ElementsKind GetFastElementsKindFromSequenceIndex(int sequence_index);

int GetSequenceIndexFromFastElementsKind(ElementsKind elements_kind);


inline bool IsDictionaryElementsKind(ElementsKind kind) {
  return kind == DICTIONARY_ELEMENTS;
}


inline bool IsExternalArrayElementsKind(ElementsKind kind) {
  return kind >= FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND &&
      kind <= LAST_EXTERNAL_ARRAY_ELEMENTS_KIND;
}


inline bool IsFastElementsKind(ElementsKind kind) {
  ASSERT(FIRST_FAST_ELEMENTS_KIND == 0);
  return kind <= FAST_HOLEY_DOUBLE_ELEMENTS;
}


inline bool IsFastDoubleElementsKind(ElementsKind kind) {
  return kind == FAST_DOUBLE_ELEMENTS ||
      kind == FAST_HOLEY_DOUBLE_ELEMENTS;
}


inline bool IsExternalFloatOrDoubleElementsKind(ElementsKind kind) {
  return kind == EXTERNAL_DOUBLE_ELEMENTS ||
      kind == EXTERNAL_FLOAT_ELEMENTS;
}


inline bool IsDoubleOrFloatElementsKind(ElementsKind kind) {
  return IsFastDoubleElementsKind(kind) ||
      IsExternalFloatOrDoubleElementsKind(kind);
}


inline bool IsFastSmiOrObjectElementsKind(ElementsKind kind) {
  return kind == FAST_SMI_ELEMENTS ||
      kind == FAST_HOLEY_SMI_ELEMENTS ||
      kind == FAST_ELEMENTS ||
      kind == FAST_HOLEY_ELEMENTS;
}


inline bool IsFastSmiElementsKind(ElementsKind kind) {
  return kind == FAST_SMI_ELEMENTS ||
      kind == FAST_HOLEY_SMI_ELEMENTS;
}


inline bool IsFastObjectElementsKind(ElementsKind kind) {
  return kind == FAST_ELEMENTS ||
      kind == FAST_HOLEY_ELEMENTS;
}


inline bool IsFastHoleyElementsKind(ElementsKind kind) {
  return kind == FAST_HOLEY_SMI_ELEMENTS ||
      kind == FAST_HOLEY_DOUBLE_ELEMENTS ||
      kind == FAST_HOLEY_ELEMENTS;
}


inline bool IsHoleyElementsKind(ElementsKind kind) {
  return IsFastHoleyElementsKind(kind) ||
      kind == DICTIONARY_ELEMENTS;
}


inline bool IsFastPackedElementsKind(ElementsKind kind) {
  return kind == FAST_SMI_ELEMENTS ||
      kind == FAST_DOUBLE_ELEMENTS ||
      kind == FAST_ELEMENTS;
}


inline ElementsKind GetPackedElementsKind(ElementsKind holey_kind) {
  if (holey_kind == FAST_HOLEY_SMI_ELEMENTS) {
    return FAST_SMI_ELEMENTS;
  }
  if (holey_kind == FAST_HOLEY_DOUBLE_ELEMENTS) {
    return FAST_DOUBLE_ELEMENTS;
  }
  if (holey_kind == FAST_HOLEY_ELEMENTS) {
    return FAST_ELEMENTS;
  }
  return holey_kind;
}


inline ElementsKind GetHoleyElementsKind(ElementsKind packed_kind) {
  if (packed_kind == FAST_SMI_ELEMENTS) {
    return FAST_HOLEY_SMI_ELEMENTS;
  }
  if (packed_kind == FAST_DOUBLE_ELEMENTS) {
    return FAST_HOLEY_DOUBLE_ELEMENTS;
  }
  if (packed_kind == FAST_ELEMENTS) {
    return FAST_HOLEY_ELEMENTS;
  }
  return packed_kind;
}


inline ElementsKind FastSmiToObjectElementsKind(ElementsKind from_kind) {
  ASSERT(IsFastSmiElementsKind(from_kind));
  return (from_kind == FAST_SMI_ELEMENTS)
      ? FAST_ELEMENTS
      : FAST_HOLEY_ELEMENTS;
}


inline bool IsSimpleMapChangeTransition(ElementsKind from_kind,
                                        ElementsKind to_kind) {
  return (GetHoleyElementsKind(from_kind) == to_kind) ||
      (IsFastSmiElementsKind(from_kind) &&
       IsFastObjectElementsKind(to_kind));
}


bool IsMoreGeneralElementsKindTransition(ElementsKind from_kind,
                                         ElementsKind to_kind);


inline bool IsTransitionableFastElementsKind(ElementsKind from_kind) {
  return IsFastElementsKind(from_kind) &&
      from_kind != TERMINAL_FAST_ELEMENTS_KIND;
}


ElementsKind GetNextMoreGeneralFastElementsKind(ElementsKind elements_kind,
                                                bool allow_only_packed);


inline bool CanTransitionToMoreGeneralFastElementsKind(
    ElementsKind elements_kind,
    bool allow_only_packed) {
  return IsFastElementsKind(elements_kind) &&
      (elements_kind != TERMINAL_FAST_ELEMENTS_KIND &&
       (!allow_only_packed || elements_kind != FAST_ELEMENTS));
}


} }  // namespace v8::internal

#endif  // V8_ELEMENTS_KIND_H_
