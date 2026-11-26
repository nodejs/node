// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_

#include <array>
#include <optional>
#include <tuple>
#include <utility>

#include "src/base/bits.h"
#include "src/codegen/cpu-features.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/utils.h"

namespace v8::internal::compiler::turboshaft {

// InstructionSelectionNormalizationReducer performs some normalization of the
// graph in order to simplify Instruction Selection. It should run only once,
// right before Instruction Selection. The normalizations currently performed
// are:
//
//  * Making sure that Constants are on the right-hand side of commutative
//    binary operations.
//
//  * Replacing multiplications by small powers of 2 with shifts.
//
//  * Folding additions and subtractions of 0.
//
//  * Splitting widening SIMD loads into 64-bit zero-extending loads and
//    low-half extensions on ARM64.
//
//  * Decomposing SIMD dot products of widening extensions into ExtMul
//    operations and pairwise adds.
//
//  * Combining SIMD multiply + pairwise add patterns into ARM64 dot products.
//
//  * Canonicalizing SIMD multiplications of matching widening extensions into
//    ExtMul operations.

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <typename Next>
class InstructionSelectionNormalizationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(InstructionSelectionNormalization)

  V<Word> REDUCE(WordBinop)(V<Word> left, V<Word> right, WordBinopOp::Kind kind,
                            WordRepresentation rep) {
    // Putting constant on the right side.
    if (WordBinopOp::IsCommutative(kind)) {
      if (!IsSimpleConstant(right) && IsSimpleConstant(left)) {
        std::swap(left, right);
      } else if (!IsComplexConstant(right) && IsComplexConstant(left)) {
        std::swap(left, right);
      }
    }

    if (kind == any_of(WordBinopOp::Kind::kAdd, WordBinopOp::Kind::kSub) &&
        __ matcher().MatchIntegralZero(right)) {
      return left;
    }

    // Transforming multiplications by power of two constants into shifts
    if (kind == WordBinopOp::Kind::kMul) {
      int64_t cst;
      if (__ matcher().MatchPowerOfTwoWordConstant(right, &cst, rep) &&
          cst < rep.bit_width()) {
        return __ ShiftLeft(left, base::bits::WhichPowerOfTwo(cst), rep);
      }
    }

    return Next::ReduceWordBinop(left, right, kind, rep);
  }

  V<Word32> REDUCE(Comparison)(V<Any> left, V<Any> right,
                               ComparisonOp::Kind kind,
                               RegisterRepresentation rep) {
    if (ComparisonOp::IsCommutative(kind)) {
      if (!IsSimpleConstant(right) && IsSimpleConstant(left)) {
        std::swap(left, right);
      } else if (!IsComplexConstant(right) && IsComplexConstant(left)) {
        std::swap(left, right);
      }
    }
    return Next::ReduceComparison(left, right, kind, rep);
  }

#if V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64
  V<Simd128> REDUCE(Simd128LoadTransform)(
      V<WordPtr> base, V<WordPtr> index,
      Simd128LoadTransformOp::LoadKind load_kind,
      Simd128LoadTransformOp::TransformKind transform_kind, int offset) {
    using TransformKind = Simd128LoadTransformOp::TransformKind;
    using UnaryKind = Simd128UnaryOp::Kind;

    UnaryKind extension_kind;
    switch (transform_kind) {
      case TransformKind::k8x8S:
        extension_kind = UnaryKind::kI16x8SConvertI8x16Low;
        break;
      case TransformKind::k8x8U:
        extension_kind = UnaryKind::kI16x8UConvertI8x16Low;
        break;
      case TransformKind::k16x4S:
        extension_kind = UnaryKind::kI32x4SConvertI16x8Low;
        break;
      case TransformKind::k16x4U:
        extension_kind = UnaryKind::kI32x4UConvertI16x8Low;
        break;
      case TransformKind::k32x2S:
        extension_kind = UnaryKind::kI64x2SConvertI32x4Low;
        break;
      case TransformKind::k32x2U:
        extension_kind = UnaryKind::kI64x2UConvertI32x4Low;
        break;
      default:
        return Next::ReduceSimd128LoadTransform(base, index, load_kind,
                                                transform_kind, offset);
    }

    V<Simd128> load = __ Simd128LoadTransform(base, index, load_kind,
                                              TransformKind::k64Zero, offset);
    return __ Simd128Unary(load, extension_kind);
  }
#endif  // V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64

#if V8_ENABLE_SIMD128
  V<Simd128> REDUCE(Simd128Binop)(V<Simd128> left, V<Simd128> right,
                                  Simd128BinopOp::Kind kind) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceSimd128Binop(left, right, kind);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

#if V8_TARGET_ARCH_ARM64
    if (std::optional<WideningMulMatch> match =
            TryMatchMulOfExtends(left, right, kind)) {
      return __ Simd128Binop(match->left, match->right, match->extmul_kind);
    }

    if (CpuFeatures::IsSupported(DOTPROD) &&
        kind == Simd128BinopOp::Kind::kI32x4Add) {
      // The following almost looks like a pairwise dot-product, except for the
      // root addition:
      // I32x4Add(I32xExtAddPairwise(I16x8ExtMulLowI8x16S(x, y)),
      //          I32x4ExtAddPairwise(I16x8ExtMulHighI8x16S(x, y)))
      // To use the dot-product instruction, the root needs to be converted to
      // I32x4AddPairwise.
      //
      // So, for input A and B:
      // A = A0, A1, A2, A3, ...
      // B = B0, B1, B2, B3, ...
      //
      // Two ExtMuls create two vector products:
      // ExtMulLow(A, B)  = AB0, AB1, AB2, AB3, ...
      // ExtMulHigh(A, B) = AB8, AB9, AB10, AB11, ...
      //
      // Which are then partially reduced:
      // ExtAddPairwise(ExtMulLow)  = AB0 + AB1, AB2 + AB3, AB4 + AB5
      // ExtAddPairwise(ExtMulHigh) = AB8 + AB9, AB10 + AB11, AB12 + AB13
      //
      // And then those two partial sums are combined:
      // I32x4Add = AB0 + AB1 + AB8 + AB9, AB2 + AB3 + AB10 + AB11
      //
      // But to use a pairwise add we need to ensure the correct elements are
      // adjacent to each other:
      // I32x4Add = AB0 + AB1 + AB8 + AB9, AB2 + AB3 + AB10 + AB11
      //                ^           ^
      //            keep 0 and 1 adjacent, but 8 and 9 need to be moved.
      // We can shuffle the inputs to give the correct order using this pattern:
      // 0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15,
      // Which, with 16-bit lanes, is equivalent to: 0, 4, 1, 5, 2, 6, 3, 7

      auto TryCastExtMul = [this](V<Simd128> node) -> const Simd128BinopOp* {
        if (auto* unop = __ Get(node).template TryCast<Simd128UnaryOp>()) {
          if (unop->kind == Simd128UnaryOp::Kind::kI32x4ExtAddPairwiseI16x8S) {
            if (auto* binop =
                    __ Get(unop->input()).template TryCast<Simd128BinopOp>()) {
              if (binop->kind == Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S ||
                  binop->kind == Simd128BinopOp::Kind::kI16x8ExtMulHighI8x16S) {
                return binop;
              }
            }
          }
        }
        return nullptr;
      };

      const Simd128BinopOp* mul_left = TryCastExtMul(left);
      const Simd128BinopOp* mul_right = TryCastExtMul(right);
      if (mul_left && mul_right && mul_left->kind != mul_right->kind &&
          mul_left->left() == mul_right->left() &&
          mul_left->right() == mul_right->right()) {
        // Using 16-bit elements, the top half is interleaved into the odd
        // lanes and the bottom half is placed into the even lanes. So, when
        // we perform the second pairwise addition it is equivalent to the
        // original lane-wise addition.
        uint8_t shuffle[16] = {
            0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15,
        };
        auto shuffle_kind = Simd128ShuffleOp::Kind::kI8x16;
        V<Simd128> shuffle_left = __ Simd128Shuffle(
            mul_left->left(), mul_left->left(), shuffle_kind, shuffle);
        V<Simd128> shuffle_right = __ Simd128Shuffle(
            mul_right->right(), mul_right->right(), shuffle_kind, shuffle);
        return __ Simd128Binop(shuffle_left, shuffle_right,
                               Simd128BinopOp::Kind::kI32x4DotI8x16S);
      }
    }

    if (kind != Simd128BinopOp::Kind::kI32x4DotI16x8S) goto no_change;

    auto GetExtMul =
        [this](OpIndex input) -> std::optional<Simd128BinopOp::Kind> {
      using unop_extmul_pair =
          std::pair<Simd128UnaryOp::Kind, Simd128BinopOp::Kind>;
      std::array extend_muls = std::to_array<unop_extmul_pair>({
          {Simd128UnaryOp::Kind::kI16x8SConvertI8x16Low,
           Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S},
          {Simd128UnaryOp::Kind::kI16x8SConvertI8x16High,
           Simd128BinopOp::Kind::kI16x8ExtMulHighI8x16S},
          {Simd128UnaryOp::Kind::kI16x8UConvertI8x16Low,
           Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16U},
          {Simd128UnaryOp::Kind::kI16x8UConvertI8x16High,
           Simd128BinopOp::Kind::kI16x8ExtMulHighI8x16U},
      });

      if (const Simd128UnaryOp* unop =
              __ Get(input).template TryCast<Simd128UnaryOp>()) {
        for (const auto [unop_kind, extmul_kind] : extend_muls) {
          if (unop->kind == unop_kind) {
            return extmul_kind;
          }
        }
      }
      return {};
    };

    if (std::optional<Simd128BinopOp::Kind> left_extmul = GetExtMul(left)) {
      if (std::optional<Simd128BinopOp::Kind> right_extmul = GetExtMul(right)) {
        OpIndex left_conv = __ Get(left).input(0);
        OpIndex right_conv = __ Get(right).input(0);
        if (left_extmul.value() == right_extmul.value()) {
          Simd128BinopOp::Kind extmul_kind = left_extmul.value();
          Simd128UnaryOp::Kind extadd_kind =
              extmul_kind == Simd128BinopOp::Kind::kI16x8ExtMulLowI8x16S ||
                      extmul_kind ==
                          Simd128BinopOp::Kind::kI16x8ExtMulHighI8x16S
                  ? Simd128UnaryOp::Kind::kI32x4ExtAddPairwiseI16x8S
                  : Simd128UnaryOp::Kind::kI32x4ExtAddPairwiseI16x8U;
          return __ Simd128Unary(
              __ Simd128Binop(left_conv, right_conv, extmul_kind), extadd_kind);
        }
        OpIndex mul_low = __ Simd128Binop(
            left, right, Simd128BinopOp::Kind::kI32x4ExtMulLowI16x8S);
        OpIndex mul_high = __ Simd128Binop(
            left, right, Simd128BinopOp::Kind::kI32x4ExtMulHighI16x8S);
        return __ Simd128Binop(mul_low, mul_high,
                               Simd128BinopOp::Kind::kI32x4AddPairwise);
      }
    }
#endif  // V8_TARGET_ARCH_ARM64

    goto no_change;
  }
#endif  // V8_ENABLE_SIMD128

 private:
#if V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64
  struct WideningMulMatch {
    V<Simd128> left;
    V<Simd128> right;
    Simd128BinopOp::Kind extmul_kind;
  };

  std::optional<WideningMulMatch> TryMatchMulOfExtends(
      V<Simd128> left, V<Simd128> right, Simd128BinopOp::Kind kind) {
    using BinopKind = Simd128BinopOp::Kind;
    using UnaryKind = Simd128UnaryOp::Kind;
    using Mapping = std::tuple<BinopKind, UnaryKind, BinopKind>;

    if (kind != any_of(BinopKind::kI16x8Mul, BinopKind::kI32x4Mul,
                       BinopKind::kI64x2Mul)) {
      return std::nullopt;
    }

    static constexpr std::array mappings = {
        Mapping{BinopKind::kI16x8Mul, UnaryKind::kI16x8SConvertI8x16Low,
                BinopKind::kI16x8ExtMulLowI8x16S},
        Mapping{BinopKind::kI16x8Mul, UnaryKind::kI16x8SConvertI8x16High,
                BinopKind::kI16x8ExtMulHighI8x16S},
        Mapping{BinopKind::kI16x8Mul, UnaryKind::kI16x8UConvertI8x16Low,
                BinopKind::kI16x8ExtMulLowI8x16U},
        Mapping{BinopKind::kI16x8Mul, UnaryKind::kI16x8UConvertI8x16High,
                BinopKind::kI16x8ExtMulHighI8x16U},
        Mapping{BinopKind::kI32x4Mul, UnaryKind::kI32x4SConvertI16x8Low,
                BinopKind::kI32x4ExtMulLowI16x8S},
        Mapping{BinopKind::kI32x4Mul, UnaryKind::kI32x4SConvertI16x8High,
                BinopKind::kI32x4ExtMulHighI16x8S},
        Mapping{BinopKind::kI32x4Mul, UnaryKind::kI32x4UConvertI16x8Low,
                BinopKind::kI32x4ExtMulLowI16x8U},
        Mapping{BinopKind::kI32x4Mul, UnaryKind::kI32x4UConvertI16x8High,
                BinopKind::kI32x4ExtMulHighI16x8U},
        Mapping{BinopKind::kI64x2Mul, UnaryKind::kI64x2SConvertI32x4Low,
                BinopKind::kI64x2ExtMulLowI32x4S},
        Mapping{BinopKind::kI64x2Mul, UnaryKind::kI64x2SConvertI32x4High,
                BinopKind::kI64x2ExtMulHighI32x4S},
        Mapping{BinopKind::kI64x2Mul, UnaryKind::kI64x2UConvertI32x4Low,
                BinopKind::kI64x2ExtMulLowI32x4U},
        Mapping{BinopKind::kI64x2Mul, UnaryKind::kI64x2UConvertI32x4High,
                BinopKind::kI64x2ExtMulHighI32x4U},
    };

    const Simd128UnaryOp* left_extension =
        __ Get(left).template TryCast<Simd128UnaryOp>();
    const Simd128UnaryOp* right_extension =
        __ Get(right).template TryCast<Simd128UnaryOp>();
    if (left_extension == nullptr || right_extension == nullptr ||
        left_extension->kind != right_extension->kind) {
      return std::nullopt;
    }
    for (const auto& [mul, extension, extmul] : mappings) {
      if (mul == kind && extension == left_extension->kind) {
        return WideningMulMatch{left_extension->input(),
                                right_extension->input(), extmul};
      }
    }
    return std::nullopt;
  }
#endif  // V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64

  // Return true if {index} is a literal ConsantOp.
  bool IsSimpleConstant(V<Any> index) {
    return __ Get(index).template Is<ConstantOp>();
  }
  // Return true if {index} is a ConstantOp or a (chain of) Change/Cast/Bitcast
  // of a ConstantOp. Such an operation is succeptible to be recognized as a
  // constant by the instruction selector, and as such should rather be on the
  // right-hande side of commutative binops.
  bool IsComplexConstant(V<Any> index) {
    const Operation& op = __ Get(index);
    switch (op.opcode) {
      case Opcode::kConstant:
        return true;
      case Opcode::kChange:
        return IsComplexConstant(op.Cast<ChangeOp>().input());
      case Opcode::kTaggedBitcast:
        return IsComplexConstant(op.Cast<TaggedBitcastOp>().input());
      case Opcode::kTryChange:
        return IsComplexConstant(op.Cast<TryChangeOp>().input());
      default:
        return false;
    }
  }
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_
