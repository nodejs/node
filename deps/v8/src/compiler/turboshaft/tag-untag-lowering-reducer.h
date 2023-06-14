// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_TAG_UNTAG_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_TAG_UNTAG_LOWERING_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class TagUntagLoweringReducer : public Next {
  static constexpr int kSmiShiftBits = kSmiShiftSize + kSmiTagSize;

 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  V<Object> REDUCE(Tag)(V<Word32> input, TagKind kind) {
    DCHECK_EQ(kind, TagKind::kSmiTag);
    // Do shift on 32bit values if Smis are stored in the lower word.
    if constexpr (Is64() && SmiValuesAre31Bits()) {
      return ChangeTaggedInt32ToSmi(__ Word32ShiftLeft(input, kSmiShiftBits));
    } else {
      return V<Object>::Cast(
          __ WordPtrShiftLeft(__ ChangeInt32ToIntPtr(input), kSmiShiftBits));
    }
  }

  V<Word32> REDUCE(Untag)(V<Object> input, TagKind kind,
                          RegisterRepresentation rep) {
    DCHECK_EQ(kind, TagKind::kSmiTag);
    DCHECK_EQ(rep, RegisterRepresentation::Word32());
    if constexpr (Is64() && SmiValuesAre31Bits()) {
      return __ Word32ShiftRightArithmeticShiftOutZeros(V<Word32>::Cast(input),
                                                        kSmiShiftBits);
    }
    return V<Word32>::Cast(__ WordPtrShiftRightArithmeticShiftOutZeros(
        V<WordPtr>::Cast(input), kSmiShiftBits));
  }

 private:
  V<Smi> ChangeTaggedInt32ToSmi(V<Word32> input) {
    DCHECK(SmiValuesAre31Bits());
    // In pointer compression, we smi-corrupt. Then, the upper bits are not
    // important.
    return COMPRESS_POINTERS_BOOL
               ? V<Smi>::Cast(__ BitcastWord32ToWord64(input))
               : V<Smi>::Cast(__ ChangeInt32ToIntPtr(input));
  }
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_TAG_UNTAG_LOWERING_REDUCER_H_
