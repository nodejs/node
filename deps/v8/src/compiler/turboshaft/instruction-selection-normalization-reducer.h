// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_

#include <array>
#include <optional>
#include <tuple>

#include "src/base/bits.h"
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
//  * Splitting widening SIMD loads into 64-bit zero-extending loads and
//    low-half extensions on ARM64.
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

#if V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64
  V<Simd128> REDUCE(Simd128Binop)(V<Simd128> left, V<Simd128> right,
                                  Simd128BinopOp::Kind kind) {
    if (!ShouldSkipOptimizationStep()) {
      if (std::optional<WideningMulMatch> match =
              TryMatchMulOfExtends(left, right, kind)) {
        return __ Simd128Binop(match->left, match->right, match->extmul_kind);
      }
    }
    return Next::ReduceSimd128Binop(left, right, kind);
  }
#endif  // V8_ENABLE_SIMD128 && V8_TARGET_ARCH_ARM64

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
