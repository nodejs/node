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

#include "elements-kind.h"

#include "api.h"
#include "elements.h"
#include "objects.h"

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


const char* ElementsKindToString(ElementsKind kind) {
  ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
  return accessor->name();
}


void PrintElementsKind(FILE* out, ElementsKind kind) {
  PrintF(out, "%s", ElementsKindToString(kind));
}


ElementsKind GetInitialFastElementsKind() {
  if (FLAG_packed_arrays) {
    return FAST_SMI_ELEMENTS;
  } else {
    return FAST_HOLEY_SMI_ELEMENTS;
  }
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


static LazyInstance<ElementsKind*,
                    InitializeFastElementsKindSequence>::type
    fast_elements_kind_sequence = LAZY_INSTANCE_INITIALIZER;


ElementsKind GetFastElementsKindFromSequenceIndex(int sequence_number) {
  ASSERT(sequence_number >= 0 &&
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
  ASSERT(IsFastElementsKind(elements_kind));
  ASSERT(elements_kind != TERMINAL_FAST_ELEMENTS_KIND);
  while (true) {
    elements_kind = GetNextTransitionElementsKind(elements_kind);
    if (!IsFastHoleyElementsKind(elements_kind) || !allow_only_packed) {
      return elements_kind;
    }
  }
  UNREACHABLE();
  return TERMINAL_FAST_ELEMENTS_KIND;
}


bool IsMoreGeneralElementsKindTransition(ElementsKind from_kind,
                                         ElementsKind to_kind) {
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


} }  // namespace v8::internal
