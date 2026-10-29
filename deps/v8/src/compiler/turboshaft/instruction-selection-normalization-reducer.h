// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_INSTRUCTION_SELECTION_NORMALIZATION_REDUCER_H_

#include "src/base/bits.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"

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

 private:
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
