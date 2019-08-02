// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_
#define V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_

#include "src/codegen/code-stub-assembler.h"
#include "src/objects/bigint.h"
#include "src/objects/elements-kind.h"

namespace v8 {
namespace internal {

class DataViewBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit DataViewBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

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

  TNode<Uint32T> DataViewEncodeBigIntBits(bool sign, int32_t digits) {
    return Unsigned(Int32Constant(BigInt::SignBits::encode(sign) |
                                  BigInt::LengthBits::encode(digits)));
  }

  TNode<Uint32T> DataViewDecodeBigIntLength(TNode<BigInt> value) {
    TNode<Word32T> bitfield = LoadBigIntBitfield(value);
    return DecodeWord32<BigIntBase::LengthBits>(bitfield);
  }

  TNode<Uint32T> DataViewDecodeBigIntSign(TNode<BigInt> value) {
    TNode<Word32T> bitfield = LoadBigIntBitfield(value);
    return DecodeWord32<BigIntBase::SignBits>(bitfield);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DATA_VIEW_GEN_H_
