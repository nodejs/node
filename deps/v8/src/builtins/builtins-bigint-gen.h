// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_BIGINT_GEN_H_
#define V8_BUILTINS_BUILTINS_BIGINT_GEN_H_

#include "src/codegen/code-stub-assembler.h"
#include "src/objects/bigint.h"

namespace v8 {
namespace internal {

class BigIntBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit BigIntBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<IntPtrT> ReadBigIntLength(TNode<BigInt> value) {
    TNode<Word32T> bitfield = LoadBigIntBitfield(value);
    return ChangeInt32ToIntPtr(
        Signed(DecodeWord32<BigIntBase::LengthBits>(bitfield)));
  }

  TNode<Uint32T> ReadBigIntSign(TNode<BigInt> value) {
    TNode<Word32T> bitfield = LoadBigIntBitfield(value);
    return DecodeWord32<BigIntBase::SignBits>(bitfield);
  }

  void WriteBigIntSignAndLength(TNode<BigInt> bigint, TNode<Uint32T> sign,
                                TNode<IntPtrT> length) {
    static_assert(BigIntBase::SignBits::kShift == 0);
    TNode<Uint32T> bitfield = Unsigned(
        Word32Or(Word32Shl(TruncateIntPtrToInt32(length),
                           Int32Constant(BigIntBase::LengthBits::kShift)),
                 Word32And(sign, Int32Constant(BigIntBase::SignBits::kMask))));
    StoreBigIntBitfield(bigint, bitfield);
  }

  void CppAbsoluteAddAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                     TNode<BigInt> y) {
    TNode<ExternalReference> mutable_big_int_absolute_add_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_absolute_add_and_canonicalize_function());
    CallCFunction(mutable_big_int_absolute_add_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppAbsoluteSubAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                     TNode<BigInt> y) {
    TNode<ExternalReference> mutable_big_int_absolute_sub_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_absolute_sub_and_canonicalize_function());
    CallCFunction(mutable_big_int_absolute_sub_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  TNode<Int32T> CppAbsoluteMulAndCanonicalize(TNode<BigInt> result,
                                              TNode<BigInt> x,
                                              TNode<BigInt> y) {
    TNode<ExternalReference> mutable_big_int_absolute_mul_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_absolute_mul_and_canonicalize_function());
    TNode<Int32T> return_code = UncheckedCast<Int32T>(CallCFunction(
        mutable_big_int_absolute_mul_and_canonicalize, MachineType::Int32(),
        std::make_pair(MachineType::AnyTagged(), result),
        std::make_pair(MachineType::AnyTagged(), x),
        std::make_pair(MachineType::AnyTagged(), y)));
    return return_code;
  }

  TNode<Int32T> CppAbsoluteDivAndCanonicalize(TNode<BigInt> result,
                                              TNode<BigInt> x,
                                              TNode<BigInt> y) {
    TNode<ExternalReference> mutable_big_int_absolute_div_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_absolute_div_and_canonicalize_function());
    TNode<Int32T> return_code = UncheckedCast<Int32T>(CallCFunction(
        mutable_big_int_absolute_div_and_canonicalize, MachineType::Int32(),
        std::make_pair(MachineType::AnyTagged(), result),
        std::make_pair(MachineType::AnyTagged(), x),
        std::make_pair(MachineType::AnyTagged(), y)));
    return return_code;
  }

  TNode<Int32T> CppAbsoluteModAndCanonicalize(TNode<BigInt> result,
                                              TNode<BigInt> x,
                                              TNode<BigInt> y) {
    TNode<ExternalReference> mutable_big_int_absolute_mod_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_absolute_mod_and_canonicalize_function());
    TNode<Int32T> return_code = UncheckedCast<Int32T>(CallCFunction(
        mutable_big_int_absolute_mod_and_canonicalize, MachineType::Int32(),
        std::make_pair(MachineType::AnyTagged(), result),
        std::make_pair(MachineType::AnyTagged(), x),
        std::make_pair(MachineType::AnyTagged(), y)));
    return return_code;
  }

  void CppBitwiseAndPosPosAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                          TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_and_pos_pos_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_and_pp_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_and_pos_pos_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseAndNegNegAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                          TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_and_neg_neg_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_and_nn_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_and_neg_neg_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseAndPosNegAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                          TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_and_pos_neg_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_and_pn_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_and_pos_neg_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseOrPosPosAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                         TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_or_pos_pos_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_or_pp_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_or_pos_pos_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseOrNegNegAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                         TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_or_neg_neg_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_or_nn_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_or_neg_neg_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseOrPosNegAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                         TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_or_pos_neg_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_or_pn_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_or_pos_neg_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseXorPosPosAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                          TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_xor_pos_pos_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_xor_pp_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_xor_pos_pos_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseXorNegNegAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                          TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_xor_neg_neg_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_xor_nn_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_xor_neg_neg_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppBitwiseXorPosNegAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                          TNode<BigInt> y) {
    TNode<ExternalReference>
        mutable_big_int_bitwise_xor_pos_neg_and_canonicalize = ExternalConstant(
            ExternalReference::
                mutable_big_int_bitwise_xor_pn_and_canonicalize_function());
    CallCFunction(mutable_big_int_bitwise_xor_pos_neg_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::AnyTagged(), y));
  }

  void CppLeftShiftAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                   TNode<IntPtrT> shift) {
    TNode<ExternalReference> mutable_big_int_left_shift_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_left_shift_and_canonicalize_function());
    CallCFunction(mutable_big_int_left_shift_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::IntPtr(), shift));
  }

  TNode<Uint32T> CppRightShiftResultLength(TNode<BigInt> x,
                                           TNode<Uint32T> x_sign,
                                           TNode<IntPtrT> shift) {
    TNode<ExternalReference> big_int_right_shift_result_length =
        ExternalConstant(
            ExternalReference::big_int_right_shift_result_length_function());
    return UncheckedCast<Uint32T>(
        CallCFunction(big_int_right_shift_result_length, MachineType::Uint32(),
                      std::make_pair(MachineType::AnyTagged(), x),
                      std::make_pair(MachineType::Uint32(), x_sign),
                      std::make_pair(MachineType::IntPtr(), shift)));
  }

  void CppRightShiftAndCanonicalize(TNode<BigInt> result, TNode<BigInt> x,
                                    TNode<IntPtrT> shift,
                                    TNode<Uint32T> must_round_down) {
    TNode<ExternalReference> mutable_big_int_right_shift_and_canonicalize =
        ExternalConstant(
            ExternalReference::
                mutable_big_int_right_shift_and_canonicalize_function());
    CallCFunction(mutable_big_int_right_shift_and_canonicalize,
                  MachineType::AnyTagged(),
                  std::make_pair(MachineType::AnyTagged(), result),
                  std::make_pair(MachineType::AnyTagged(), x),
                  std::make_pair(MachineType::IntPtr(), shift),
                  std::make_pair(MachineType::Uint32(), must_round_down));
  }

  TNode<Int32T> CppAbsoluteCompare(TNode<BigInt> x, TNode<BigInt> y) {
    TNode<ExternalReference> mutable_big_int_absolute_compare =
        ExternalConstant(
            ExternalReference::mutable_big_int_absolute_compare_function());
    TNode<Int32T> result = UncheckedCast<Int32T>(
        CallCFunction(mutable_big_int_absolute_compare, MachineType::Int32(),
                      std::make_pair(MachineType::AnyTagged(), x),
                      std::make_pair(MachineType::AnyTagged(), y)));
    return result;
  }
};

}  // namespace internal
}  // namespace v8
#endif  // V8_BUILTINS_BUILTINS_BIGINT_GEN_H_
