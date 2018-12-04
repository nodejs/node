// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_
#define V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_

#include "src/elements-kind.h"
#include "src/objects/bigint.h"
#include "torque-generated/builtins-base-from-dsl-gen.h"

namespace v8 {
namespace internal {

class DataViewBuiltinsAssembler : public BaseBuiltinsFromDSLAssembler {
 public:
  explicit DataViewBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : BaseBuiltinsFromDSLAssembler(state) {}

  TNode<Int32T> LoadUint8(TNode<RawPtrT> data_pointer, TNode<UintPtrT> offset) {
    return UncheckedCast<Int32T>(
        Load(MachineType::Uint8(), data_pointer, offset));
  }

  TNode<Int32T> LoadInt8(TNode<RawPtrT> data_pointer, TNode<UintPtrT> offset) {
    return UncheckedCast<Int32T>(
        Load(MachineType::Int8(), data_pointer, offset));
  }

  void StoreWord8(TNode<RawPtrT> data_pointer, TNode<UintPtrT> offset,
                  TNode<Word32T> value) {
    StoreNoWriteBarrier(MachineRepresentation::kWord8, data_pointer, offset,
                        value);
  }

  int32_t DataViewElementSize(ElementsKind elements_kind) {
    return ElementsKindToByteSize(elements_kind);
  }

  TNode<IntPtrT> DataViewEncodeBigIntBits(bool sign, int32_t digits) {
    return IntPtrConstant(BigInt::SignBits::encode(sign) |
                          BigInt::LengthBits::encode(digits));
  }

  TNode<UintPtrT> DataViewDecodeBigIntLength(TNode<BigInt> value) {
    TNode<WordT> bitfield = LoadBigIntBitfield(value);
    return DecodeWord<BigIntBase::LengthBits>(bitfield);
  }

  TNode<UintPtrT> DataViewDecodeBigIntSign(TNode<BigInt> value) {
    TNode<WordT> bitfield = LoadBigIntBitfield(value);
    return DecodeWord<BigIntBase::SignBits>(bitfield);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_
