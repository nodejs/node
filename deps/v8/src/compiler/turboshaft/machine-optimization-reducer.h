// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_MACHINE_OPTIMIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_MACHINE_OPTIMIZATION_REDUCER_H_

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/functional.h"
#include "src/base/ieee754.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/overflowing-math.h"
#include "src/base/template-utils.h"
#include "src/base/vector.h"
#include "src/builtins/builtins.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/numbers/conversions.h"

namespace v8::internal::compiler::turboshaft {

template <bool signalling_nan_possible, class Next>
class MachineOptimizationReducer;

template <class Next>
using MachineOptimizationReducerSignallingNanPossible =
    MachineOptimizationReducer<true, Next>;
template <class Next>
using MachineOptimizationReducerSignallingNanImpossible =
    MachineOptimizationReducer<false, Next>;

// The MachineOptimizationAssembler performs basic optimizations on low-level
// operations that can be performed on-the-fly, without requiring type analysis
// or analyzing uses. It largely corresponds to MachineOperatorReducer in
// sea-of-nodes Turbofan.
//
// Additional optimizations include some of the control-flow reductions that
// were previously done in CommonOperatorReducer, including:
//    1- Reducing Phis, whose all inputs are the same, replace
//      them with their input.

template <bool signalling_nan_possible, class Next>
class MachineOptimizationReducer : public Next {
 public:
  using Next::Asm;

  template <class... Args>
  explicit MachineOptimizationReducer(const std::tuple<Args...>& args)
      : Next(args) {}

  // TODO(mslekova): Implement ReduceSelect and ReducePhi,
  // by reducing `(f > 0) ? f : -f` to `fabs(f)`.

  OpIndex ReduceChange(OpIndex input, ChangeOp::Kind kind,
                       ChangeOp::Assumption assumption,
                       RegisterRepresentation from, RegisterRepresentation to) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceChange(input, kind, assumption, from, to);
    }
    using Kind = ChangeOp::Kind;
    if (from == WordRepresentation::Word32()) {
      input = TryRemoveWord32ToWord64Conversion(input);
    }
    if (uint64_t value;
        from.IsWord() &&
        Asm().MatchWordConstant(input, WordRepresentation(from), &value)) {
      if (kind == Kind::kSignExtend && from == WordRepresentation::Word32() &&
          to == WordRepresentation::Word64()) {
        return Asm().Word64Constant(int64_t{static_cast<int32_t>(value)});
      }
      if (kind == any_of(Kind::kZeroExtend, Kind::kBitcast) &&
          from == WordRepresentation::Word32() &&
          to == WordRepresentation::Word64()) {
        return Asm().Word64Constant(uint64_t{static_cast<uint32_t>(value)});
      }
      if (kind == Kind::kBitcast && from == WordRepresentation::Word32() &&
          to == RegisterRepresentation::Float32()) {
        return Asm().Float32Constant(
            base::bit_cast<float>(static_cast<uint32_t>(value)));
      }
      if (kind == Kind::kBitcast && from == WordRepresentation::Word64() &&
          to == RegisterRepresentation::Float64()) {
        return Asm().Float64Constant(base::bit_cast<double>(value));
      }
      if (kind == Kind::kSignedToFloat &&
          from == WordRepresentation::Word32() &&
          to == RegisterRepresentation::Float64()) {
        return Asm().Float64Constant(
            static_cast<double>(static_cast<int32_t>(value)));
      }
      if (kind == Kind::kSignedToFloat &&
          from == WordRepresentation::Word64() &&
          to == RegisterRepresentation::Float64()) {
        return Asm().Float64Constant(
            static_cast<double>(static_cast<int64_t>(value)));
      }
      if (kind == Kind::kUnsignedToFloat &&
          from == WordRepresentation::Word32() &&
          to == RegisterRepresentation::Float64()) {
        return Asm().Float64Constant(
            static_cast<double>(static_cast<uint32_t>(value)));
      }
    }
    if (float value; from == RegisterRepresentation::Float32() &&
                     Asm().MatchFloat32Constant(input, &value)) {
      if (kind == Kind::kFloatConversion &&
          to == RegisterRepresentation::Float64()) {
        return Asm().Float64Constant(value);
      }
      if (kind == Kind::kBitcast && to == WordRepresentation::Word32()) {
        return Asm().Word32Constant(base::bit_cast<uint32_t>(value));
      }
    }
    if (double value; from == RegisterRepresentation::Float64() &&
                      Asm().MatchFloat64Constant(input, &value)) {
      if (kind == Kind::kFloatConversion &&
          to == RegisterRepresentation::Float32()) {
        return Asm().Float32Constant(DoubleToFloat32_NoInline(value));
      }
      if (kind == Kind::kBitcast && to == WordRepresentation::Word64()) {
        return Asm().Word64Constant(base::bit_cast<uint64_t>(value));
      }
      if (kind == Kind::kSignedFloatTruncateOverflowToMin) {
        double truncated = std::trunc(value);
        if (to == WordRepresentation::Word64()) {
          int64_t result = std::numeric_limits<int64_t>::min();
          if (truncated >= std::numeric_limits<int64_t>::min() &&
              truncated <= kMaxDoubleRepresentableInt64) {
            result = static_cast<int64_t>(truncated);
          }
          return Asm().Word64Constant(result);
        }
        if (to == WordRepresentation::Word32()) {
          int32_t result = std::numeric_limits<int32_t>::min();
          if (truncated >= std::numeric_limits<int32_t>::min() &&
              truncated <= std::numeric_limits<int32_t>::max()) {
            result = static_cast<int32_t>(truncated);
          }
          return Asm().Word32Constant(result);
        }
      }
      if (kind == Kind::kJSFloatTruncate &&
          to == WordRepresentation::Word32()) {
        return Asm().Word32Constant(DoubleToInt32_NoInline(value));
      }
    }
    if (float value; from == RegisterRepresentation::Float32() &&
                     Asm().MatchFloat32Constant(input, &value)) {
      if (kind == Kind::kFloatConversion &&
          to == RegisterRepresentation::Float64()) {
        return Asm().Float64Constant(value);
      }
    }

    const Operation& input_op = Asm().Get(input);
    if (const ChangeOp* change_op = input_op.TryCast<ChangeOp>()) {
      if (change_op->from == to && change_op->to == from &&
          change_op->IsReversibleBy(kind, signalling_nan_possible)) {
        return change_op->input();
      }
    }
    return Next::ReduceChange(input, kind, assumption, from, to);
  }

  OpIndex ReduceFloat64InsertWord32(OpIndex float64, OpIndex word32,
                                    Float64InsertWord32Op::Kind kind) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceFloat64InsertWord32(float64, word32, kind);
    }
    double f;
    uint32_t w;
    if (Asm().MatchFloat64Constant(float64, &f) &&
        Asm().MatchWord32Constant(word32, &w)) {
      uint64_t float_as_word = base::bit_cast<uint64_t>(f);
      switch (kind) {
        case Float64InsertWord32Op::Kind::kLowHalf:
          return Asm().Float64Constant(base::bit_cast<double>(
              (float_as_word & uint64_t{0xFFFFFFFF00000000}) | w));
        case Float64InsertWord32Op::Kind::kHighHalf:
          return Asm().Float64Constant(base::bit_cast<double>(
              (float_as_word & uint64_t{0xFFFFFFFF}) | (uint64_t{w} << 32)));
      }
    }
    return Next::ReduceFloat64InsertWord32(float64, word32, kind);
  }

  OpIndex ReduceTaggedBitcast(OpIndex input, RegisterRepresentation from,
                              RegisterRepresentation to) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceTaggedBitcast(input, from, to);
    }
    // A Tagged -> Untagged -> Tagged sequence can be short-cut.
    // An Untagged -> Tagged -> Untagged sequence however cannot be removed,
    // because the GC might have modified the pointer.
    if (auto* input_bitcast = Asm().template TryCast<TaggedBitcastOp>(input)) {
      if (all_of(input_bitcast->to, from) ==
              RegisterRepresentation::PointerSized() &&
          all_of(input_bitcast->from, to) == RegisterRepresentation::Tagged()) {
        return input_bitcast->input();
      }
    }
    return Next::ReduceTaggedBitcast(input, from, to);
  }

  OpIndex ReduceFloatUnary(OpIndex input, FloatUnaryOp::Kind kind,
                           FloatRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceFloatUnary(input, kind, rep);
    }
    if (float k; rep == FloatRepresentation::Float32() &&
                 Asm().MatchFloat32Constant(input, &k)) {
      if (std::isnan(k)) {
        return Asm().Float32Constant(std::numeric_limits<float>::quiet_NaN());
      }
      switch (kind) {
        case FloatUnaryOp::Kind::kAbs:
          return Asm().Float32Constant(std::abs(k));
        case FloatUnaryOp::Kind::kNegate:
          return Asm().Float32Constant(-k);
        case FloatUnaryOp::Kind::kSilenceNaN:
          DCHECK(!std::isnan(k));
          return Asm().Float32Constant(k);
        case FloatUnaryOp::Kind::kRoundDown:
          return Asm().Float32Constant(std::floor(k));
        case FloatUnaryOp::Kind::kRoundUp:
          return Asm().Float32Constant(std::ceil(k));
        case FloatUnaryOp::Kind::kRoundToZero:
          return Asm().Float32Constant(std::trunc(k));
        case FloatUnaryOp::Kind::kRoundTiesEven:
          DCHECK_EQ(std::nearbyint(1.5), 2);
          DCHECK_EQ(std::nearbyint(2.5), 2);
          return Asm().Float32Constant(std::nearbyint(k));
        case FloatUnaryOp::Kind::kLog:
          return Asm().Float32Constant(base::ieee754::log(k));
        case FloatUnaryOp::Kind::kSqrt:
          return Asm().Float32Constant(std::sqrt(k));
        case FloatUnaryOp::Kind::kExp:
          return Asm().Float32Constant(base::ieee754::exp(k));
        case FloatUnaryOp::Kind::kExpm1:
          return Asm().Float32Constant(base::ieee754::expm1(k));
        case FloatUnaryOp::Kind::kSin:
          return Asm().Float32Constant(SIN_IMPL(k));
        case FloatUnaryOp::Kind::kCos:
          return Asm().Float32Constant(COS_IMPL(k));
        case FloatUnaryOp::Kind::kSinh:
          return Asm().Float32Constant(base::ieee754::sinh(k));
        case FloatUnaryOp::Kind::kCosh:
          return Asm().Float32Constant(base::ieee754::cosh(k));
        case FloatUnaryOp::Kind::kAcos:
          return Asm().Float32Constant(base::ieee754::acos(k));
        case FloatUnaryOp::Kind::kAsin:
          return Asm().Float32Constant(base::ieee754::asin(k));
        case FloatUnaryOp::Kind::kAsinh:
          return Asm().Float32Constant(base::ieee754::asinh(k));
        case FloatUnaryOp::Kind::kAcosh:
          return Asm().Float32Constant(base::ieee754::acosh(k));
        case FloatUnaryOp::Kind::kTan:
          return Asm().Float32Constant(base::ieee754::tan(k));
        case FloatUnaryOp::Kind::kTanh:
          return Asm().Float32Constant(base::ieee754::tanh(k));
        case FloatUnaryOp::Kind::kLog2:
          return Asm().Float32Constant(base::ieee754::log2(k));
        case FloatUnaryOp::Kind::kLog10:
          return Asm().Float32Constant(base::ieee754::log10(k));
        case FloatUnaryOp::Kind::kLog1p:
          return Asm().Float32Constant(base::ieee754::log1p(k));
        case FloatUnaryOp::Kind::kCbrt:
          return Asm().Float32Constant(base::ieee754::cbrt(k));
        case FloatUnaryOp::Kind::kAtan:
          return Asm().Float32Constant(base::ieee754::atan(k));
        case FloatUnaryOp::Kind::kAtanh:
          return Asm().Float32Constant(base::ieee754::atanh(k));
      }
    } else if (double k; rep == FloatRepresentation::Float64() &&
                         Asm().MatchFloat64Constant(input, &k)) {
      if (std::isnan(k)) {
        return Asm().Float64Constant(std::numeric_limits<double>::quiet_NaN());
      }
      switch (kind) {
        case FloatUnaryOp::Kind::kAbs:
          return Asm().Float64Constant(std::abs(k));
        case FloatUnaryOp::Kind::kNegate:
          return Asm().Float64Constant(-k);
        case FloatUnaryOp::Kind::kSilenceNaN:
          DCHECK(!std::isnan(k));
          return Asm().Float64Constant(k);
        case FloatUnaryOp::Kind::kRoundDown:
          return Asm().Float64Constant(std::floor(k));
        case FloatUnaryOp::Kind::kRoundUp:
          return Asm().Float64Constant(std::ceil(k));
        case FloatUnaryOp::Kind::kRoundToZero:
          return Asm().Float64Constant(std::trunc(k));
        case FloatUnaryOp::Kind::kRoundTiesEven:
          DCHECK_EQ(std::nearbyint(1.5), 2);
          DCHECK_EQ(std::nearbyint(2.5), 2);
          return Asm().Float64Constant(std::nearbyint(k));
        case FloatUnaryOp::Kind::kLog:
          return Asm().Float64Constant(base::ieee754::log(k));
        case FloatUnaryOp::Kind::kSqrt:
          return Asm().Float64Constant(std::sqrt(k));
        case FloatUnaryOp::Kind::kExp:
          return Asm().Float64Constant(base::ieee754::exp(k));
        case FloatUnaryOp::Kind::kExpm1:
          return Asm().Float64Constant(base::ieee754::expm1(k));
        case FloatUnaryOp::Kind::kSin:
          return Asm().Float64Constant(SIN_IMPL(k));
        case FloatUnaryOp::Kind::kCos:
          return Asm().Float64Constant(COS_IMPL(k));
        case FloatUnaryOp::Kind::kSinh:
          return Asm().Float64Constant(base::ieee754::sinh(k));
        case FloatUnaryOp::Kind::kCosh:
          return Asm().Float64Constant(base::ieee754::cosh(k));
        case FloatUnaryOp::Kind::kAcos:
          return Asm().Float64Constant(base::ieee754::acos(k));
        case FloatUnaryOp::Kind::kAsin:
          return Asm().Float64Constant(base::ieee754::asin(k));
        case FloatUnaryOp::Kind::kAsinh:
          return Asm().Float64Constant(base::ieee754::asinh(k));
        case FloatUnaryOp::Kind::kAcosh:
          return Asm().Float64Constant(base::ieee754::acosh(k));
        case FloatUnaryOp::Kind::kTan:
          return Asm().Float64Constant(base::ieee754::tan(k));
        case FloatUnaryOp::Kind::kTanh:
          return Asm().Float64Constant(base::ieee754::tanh(k));
        case FloatUnaryOp::Kind::kLog2:
          return Asm().Float64Constant(base::ieee754::log2(k));
        case FloatUnaryOp::Kind::kLog10:
          return Asm().Float64Constant(base::ieee754::log10(k));
        case FloatUnaryOp::Kind::kLog1p:
          return Asm().Float64Constant(base::ieee754::log1p(k));
        case FloatUnaryOp::Kind::kCbrt:
          return Asm().Float64Constant(base::ieee754::cbrt(k));
        case FloatUnaryOp::Kind::kAtan:
          return Asm().Float64Constant(base::ieee754::atan(k));
        case FloatUnaryOp::Kind::kAtanh:
          return Asm().Float64Constant(base::ieee754::atanh(k));
      }
    }
    return Next::ReduceFloatUnary(input, kind, rep);
  }

  OpIndex ReduceWordUnary(OpIndex input, WordUnaryOp::Kind kind,
                          WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceWordUnary(input, kind, rep);
    }
    if (rep == WordRepresentation::Word32()) {
      input = TryRemoveWord32ToWord64Conversion(input);
    }
    if (uint32_t k; rep == WordRepresentation::Word32() &&
                    Asm().MatchWord32Constant(input, &k)) {
      switch (kind) {
        case WordUnaryOp::Kind::kReverseBytes:
          return Asm().Word32Constant(base::bits::ReverseBytes(k));
        case WordUnaryOp::Kind::kCountLeadingZeros:
          return Asm().Word32Constant(base::bits::CountLeadingZeros(k));
        case WordUnaryOp::Kind::kCountTrailingZeros:
          return Asm().Word32Constant(base::bits::CountTrailingZeros(k));
        case WordUnaryOp::Kind::kPopCount:
          return Asm().Word32Constant(base::bits::CountPopulation(k));
        case WordUnaryOp::Kind::kSignExtend8:
          return Asm().Word32Constant(int32_t{static_cast<int8_t>(k)});
        case WordUnaryOp::Kind::kSignExtend16:
          return Asm().Word32Constant(int32_t{static_cast<int16_t>(k)});
      }
    } else if (uint64_t k; rep == WordRepresentation::Word64() &&
                           Asm().MatchWord64Constant(input, &k)) {
      switch (kind) {
        case WordUnaryOp::Kind::kReverseBytes:
          return Asm().Word64Constant(base::bits::ReverseBytes(k));
        case WordUnaryOp::Kind::kCountLeadingZeros:
          return Asm().Word64Constant(
              uint64_t{base::bits::CountLeadingZeros(k)});
        case WordUnaryOp::Kind::kCountTrailingZeros:
          return Asm().Word64Constant(
              uint64_t{base::bits::CountTrailingZeros(k)});
        case WordUnaryOp::Kind::kPopCount:
          return Asm().Word64Constant(uint64_t{base::bits::CountPopulation(k)});
        case WordUnaryOp::Kind::kSignExtend8:
          return Asm().Word64Constant(int64_t{static_cast<int8_t>(k)});
        case WordUnaryOp::Kind::kSignExtend16:
          return Asm().Word64Constant(int64_t{static_cast<int16_t>(k)});
      }
    }
    return Next::ReduceWordUnary(input, kind, rep);
  }

  OpIndex ReduceFloatBinop(OpIndex lhs, OpIndex rhs, FloatBinopOp::Kind kind,
                           FloatRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceFloatBinop(lhs, rhs, kind, rep);
    }

    using Kind = FloatBinopOp::Kind;

    // Place constant on the right for commutative operators.
    if (FloatBinopOp::IsCommutative(kind) &&
        Asm().template Is<ConstantOp>(lhs) &&
        !Asm().template Is<ConstantOp>(rhs)) {
      return ReduceFloatBinop(rhs, lhs, kind, rep);
    }

    // constant folding
    if (float k1, k2; rep == FloatRepresentation::Float32() &&
                      Asm().MatchFloat32Constant(lhs, &k1) &&
                      Asm().MatchFloat32Constant(rhs, &k2)) {
      switch (kind) {
        case Kind::kAdd:
          return Asm().Float32Constant(k1 + k2);
        case Kind::kMul:
          return Asm().Float32Constant(k1 * k2);
        case Kind::kSub:
          return Asm().Float32Constant(k1 - k2);
        case Kind::kMin:
          return Asm().Float32Constant(JSMin(k1, k2));
        case Kind::kMax:
          return Asm().Float32Constant(JSMax(k1, k2));
        case Kind::kDiv:
          return Asm().Float32Constant(k1 / k2);
        case Kind::kPower:
          return Asm().Float32Constant(base::ieee754::pow(k1, k2));
        case Kind::kAtan2:
          return Asm().Float32Constant(base::ieee754::atan2(k1, k2));
        case Kind::kMod:
          UNREACHABLE();
      }
    }
    if (double k1, k2; rep == FloatRepresentation::Float64() &&
                       Asm().MatchFloat64Constant(lhs, &k1) &&
                       Asm().MatchFloat64Constant(rhs, &k2)) {
      switch (kind) {
        case Kind::kAdd:
          return Asm().Float64Constant(k1 + k2);
        case Kind::kMul:
          return Asm().Float64Constant(k1 * k2);
        case Kind::kSub:
          return Asm().Float64Constant(k1 - k2);
        case Kind::kMin:
          return Asm().Float64Constant(JSMin(k1, k2));
        case Kind::kMax:
          return Asm().Float64Constant(JSMax(k1, k2));
        case Kind::kDiv:
          return Asm().Float64Constant(k1 / k2);
        case Kind::kMod:
          return Asm().Float64Constant(Modulo(k1, k2));
        case Kind::kPower:
          return Asm().Float64Constant(base::ieee754::pow(k1, k2));
        case Kind::kAtan2:
          return Asm().Float64Constant(base::ieee754::atan2(k1, k2));
      }
    }

    // lhs <op> NaN  =>  NaN
    if (Asm().MatchNaN(rhs) || (Asm().MatchNaN(lhs) && kind != Kind::kPower)) {
      // Return a quiet NaN since Wasm operations could have signalling NaN as
      // input but not as output.
      return Asm().FloatConstant(std::numeric_limits<double>::quiet_NaN(), rep);
    }

    if (Asm().template Is<ConstantOp>(rhs)) {
      if (kind == Kind::kMul) {
        // lhs * 1  =>  lhs
        if (!signalling_nan_possible && Asm().MatchFloat(rhs, 1.0)) {
          return lhs;
        }
        // lhs * 2  =>  lhs + lhs
        if (Asm().MatchFloat(rhs, 2.0)) {
          return Asm().FloatAdd(lhs, lhs, rep);
        }
        // lhs * -1  =>  -lhs
        if (Asm().MatchFloat(rhs, -1.0)) {
          return Asm().FloatNegate(lhs, rep);
        }
      }

      if (kind == Kind::kDiv) {
        // lhs / 1  =>  lhs
        if (!signalling_nan_possible && Asm().MatchFloat(rhs, 1.0)) {
          return lhs;
        }
        // lhs / -1  =>  -lhs
        if (Asm().MatchFloat(rhs, -1.0)) {
          return Asm().FloatNegate(lhs, rep);
        }
        // All reciprocals of non-denormal powers of two can be represented
        // exactly, so division by power of two can be reduced to
        // multiplication by reciprocal, with the same result.
        // x / k  =>  x * (1 / k)
        if (rep == FloatRepresentation::Float32()) {
          if (float k;
              Asm().MatchFloat32Constant(rhs, &k) && std::isnormal(k) &&
              k != 0 && std::isfinite(k) &&
              base::bits::IsPowerOfTwo(base::Double(k).Significand())) {
            return Asm().FloatMul(lhs, Asm().FloatConstant(1.0 / k, rep), rep);
          }
        } else {
          DCHECK_EQ(rep, FloatRepresentation::Float64());
          if (double k;
              Asm().MatchFloat64Constant(rhs, &k) && std::isnormal(k) &&
              k != 0 && std::isfinite(k) &&
              base::bits::IsPowerOfTwo(base::Double(k).Significand())) {
            return Asm().FloatMul(lhs, Asm().FloatConstant(1.0 / k, rep), rep);
          }
        }
      }

      if (kind == Kind::kMod) {
        // x % 0  =>  NaN
        if (Asm().MatchFloat(rhs, 0.0)) {
          return Asm().FloatConstant(std::numeric_limits<double>::quiet_NaN(),
                                     rep);
        }
      }

      if (kind == Kind::kSub) {
        // lhs - +0.0  =>  lhs
        if (!signalling_nan_possible && Asm().MatchFloat(rhs, +0.0)) {
          return lhs;
        }
      }

      if (kind == Kind::kPower) {
        if (Asm().MatchFloat(rhs, 0.0) || Asm().MatchFloat(rhs, -0.0)) {
          // lhs ** 0  ==>  1
          return Asm().FloatConstant(1.0, rep);
        }
        if (Asm().MatchFloat(rhs, 2.0)) {
          // lhs ** 2  ==>  lhs * lhs
          return Asm().FloatMul(lhs, lhs, rep);
        }
        if (Asm().MatchFloat(rhs, 0.5)) {
          // lhs ** 0.5  ==>  sqrt(lhs)
          // (unless if lhs is -infinity)
          Block* if_neg_infinity = Asm().NewBlock();
          Block* otherwise = Asm().NewBlock();
          Block* merge = Asm().NewBlock();
          Asm().Branch(Asm().FloatLessThanOrEqual(
                           lhs, Asm().FloatConstant(-V8_INFINITY, rep), rep),
                       if_neg_infinity, otherwise, BranchHint::kFalse);

          // TODO(dmercadier,tebbi): once the VariableAssembler has landed, and
          // use only one AutoVariable both both {infty} and {sqrt} to avoid the
          // "if (infty.valid() && sqrt.valid()) { return Phi ... } else ...".
          OpIndex infty = OpIndex::Invalid();
          OpIndex sqrt = OpIndex::Invalid();
          if (Asm().Bind(if_neg_infinity)) {
            infty = Asm().FloatConstant(V8_INFINITY, rep);
            Asm().Goto(merge);
          }

          if (Asm().Bind(otherwise)) {
            sqrt = Asm().FloatSqrt(lhs, rep);
            Asm().Goto(merge);
          }

          Asm().BindReachable(merge);
          if (infty.valid() && sqrt.valid()) {
            return Asm().Phi(base::VectorOf({infty, sqrt}), rep);
          } else if (infty.valid()) {
            return infty;
          } else {
            DCHECK(sqrt.valid());
            return sqrt;
          }
        }
      }
    }

    if (!signalling_nan_possible && kind == Kind::kSub &&
        Asm().MatchFloat(lhs, -0.0)) {
      // -0.0 - round_down(-0.0 - y) => round_up(y)
      if (OpIndex a, b, c;
          FloatUnaryOp::IsSupported(FloatUnaryOp::Kind::kRoundUp, rep) &&
          Asm().MatchFloatRoundDown(rhs, &a, rep) &&
          Asm().MatchFloatSub(a, &b, &c, rep) && Asm().MatchFloat(b, -0.0)) {
        return Asm().FloatRoundUp(c, rep);
      }
      // -0.0 - rhs  =>  -rhs
      return Asm().FloatNegate(rhs, rep);
    }

    return Next::ReduceFloatBinop(lhs, rhs, kind, rep);
  }

  OpIndex ReduceWordBinop(OpIndex left, OpIndex right, WordBinopOp::Kind kind,
                          WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceWordBinop(left, right, kind, rep);
    }

    using Kind = WordBinopOp::Kind;

    DCHECK_EQ(rep, any_of(WordRepresentation::Word32(),
                          WordRepresentation::Word64()));
    bool is_64 = rep == WordRepresentation::Word64();

    if (!is_64) {
      left = TryRemoveWord32ToWord64Conversion(left);
      right = TryRemoveWord32ToWord64Conversion(right);
    }

    // Place constant on the right for commutative operators.
    if (WordBinopOp::IsCommutative(kind) &&
        Asm().template Is<ConstantOp>(left) &&
        !Asm().template Is<ConstantOp>(right)) {
      return ReduceWordBinop(right, left, kind, rep);
    }
    // constant folding
    if (uint64_t k1, k2; Asm().MatchWordConstant(left, rep, &k1) &&
                         Asm().MatchWordConstant(right, rep, &k2)) {
      switch (kind) {
        case Kind::kAdd:
          return Asm().WordConstant(k1 + k2, rep);
        case Kind::kMul:
          return Asm().WordConstant(k1 * k2, rep);
        case Kind::kBitwiseAnd:
          return Asm().WordConstant(k1 & k2, rep);
        case Kind::kBitwiseOr:
          return Asm().WordConstant(k1 | k2, rep);
        case Kind::kBitwiseXor:
          return Asm().WordConstant(k1 ^ k2, rep);
        case Kind::kSub:
          return Asm().WordConstant(k1 - k2, rep);
        case Kind::kSignedMulOverflownBits:
          return Asm().WordConstant(
              is_64 ? base::bits::SignedMulHigh64(static_cast<int64_t>(k1),
                                                  static_cast<int64_t>(k2))
                    : base::bits::SignedMulHigh32(static_cast<int32_t>(k1),
                                                  static_cast<int32_t>(k2)),
              rep);
        case Kind::kUnsignedMulOverflownBits:
          return Asm().WordConstant(
              is_64 ? base::bits::UnsignedMulHigh64(k1, k2)
                    : base::bits::UnsignedMulHigh32(static_cast<uint32_t>(k1),
                                                    static_cast<uint32_t>(k2)),
              rep);
        case Kind::kSignedDiv:
          return Asm().WordConstant(
              is_64 ? base::bits::SignedDiv64(k1, k2)
                    : base::bits::SignedDiv32(static_cast<int32_t>(k1),
                                              static_cast<int32_t>(k2)),
              rep);
        case Kind::kUnsignedDiv:
          return Asm().WordConstant(
              is_64 ? base::bits::UnsignedDiv64(k1, k2)
                    : base::bits::UnsignedDiv32(static_cast<uint32_t>(k1),
                                                static_cast<uint32_t>(k2)),
              rep);
        case Kind::kSignedMod:
          return Asm().WordConstant(
              is_64 ? base::bits::SignedMod64(k1, k2)
                    : base::bits::SignedMod32(static_cast<int32_t>(k1),
                                              static_cast<int32_t>(k2)),
              rep);
        case Kind::kUnsignedMod:
          return Asm().WordConstant(
              is_64 ? base::bits::UnsignedMod64(k1, k2)
                    : base::bits::UnsignedMod32(static_cast<uint32_t>(k1),
                                                static_cast<uint32_t>(k2)),
              rep);
      }
    }

    // TODO(tebbi): Detect and merge multiple bitfield checks for CSA/Torque
    // code.

    if (uint64_t right_value;
        Asm().MatchWordConstant(right, rep, &right_value)) {
      int64_t right_value_signed =
          is_64 ? static_cast<int64_t>(right_value)
                : int64_t{static_cast<int32_t>(right_value)};
      // (a <op> k1) <op> k2  =>  a <op> (k1 <op> k2)
      if (OpIndex a, k1; WordBinopOp::IsAssociative(kind) &&
                         Asm().MatchWordBinop(left, &a, &k1, kind, rep) &&
                         Asm().template Is<ConstantOp>(k1)) {
        OpIndex k2 = right;
        return ReduceWordBinop(a, ReduceWordBinop(k1, k2, kind, rep), kind,
                               rep);
      }
      switch (kind) {
        case Kind::kSub:
          // left - k  => left + -k
          return ReduceWordBinop(left, Asm().WordConstant(-right_value, rep),
                                 Kind::kAdd, rep);
        case Kind::kAdd:
          // left + 0  =>  left
          if (right_value == 0) {
            return left;
          }
          break;
        case Kind::kBitwiseXor:
          // left ^ 0  =>  left
          if (right_value == 0) {
            return left;
          }
          // left ^ 1  =>  left == 0  if left is 0 or 1
          if (right_value == 1 && IsBit(left)) {
            return Asm().Word32Equal(left, Asm().Word32Constant(0));
          }
          // (x ^ -1) ^ -1  =>  x
          {
            OpIndex x, y;
            int64_t k;
            if (right_value_signed == -1 &&
                Asm().MatchBitwiseAnd(left, &x, &y, rep) &&
                Asm().MatchWordConstant(y, rep, &k) && k == -1) {
              return x;
            }
          }
          break;
        case Kind::kBitwiseOr:
          // left | 0  =>  left
          if (right_value == 0) {
            return left;
          }
          // left | -1  =>  -1
          if (right_value_signed == -1) {
            return right;
          }
          // (x & K1) | K2 => x | K2 if K2 has ones for every zero bit in K1.
          // This case can be constructed by UpdateWord and UpdateWord32 in CSA.
          {
            OpIndex x, y;
            uint64_t k1;
            uint64_t k2 = right_value;
            if (Asm().MatchBitwiseAnd(left, &x, &y, rep) &&
                Asm().MatchWordConstant(y, rep, &k1) &&
                (k1 | k2) == rep.MaxUnsignedValue()) {
              return Asm().WordBitwiseOr(x, right, rep);
            }
          }
          break;
        case Kind::kMul:
          // left * 0  =>  0
          if (right_value == 0) {
            return Asm().WordConstant(0, rep);
          }
          // left * 1  =>  left
          if (right_value == 1) {
            return left;
          }
          // left * -1 => 0 - left
          if (right_value_signed == -1) {
            return Asm().WordSub(Asm().WordConstant(0, rep), left, rep);
          }
          // left * 2^k  =>  left << k
          if (base::bits::IsPowerOfTwo(right_value)) {
            OpIndex shift_amount = Asm().WordConstant(
                base::bits::WhichPowerOfTwo(right_value), rep);
            return Asm().ShiftLeft(left, shift_amount, rep);
          }
          break;
        case Kind::kBitwiseAnd:
          // left & -1 => left
          if (right_value_signed == -1) {
            return left;
          }
          // x & 0  =>  0
          if (right_value == 0) {
            return right;
          }

          if (right_value == 1) {
            // (x + x) & 1  =>  0
            OpIndex left_ignore_extensions =
                IsWord32ConvertedToWord64(left)
                    ? UndoWord32ToWord64Conversion(left)
                    : left;
            if (OpIndex a, b;
                Asm().MatchWordAdd(left_ignore_extensions, &a, &b,
                                   WordRepresentation::Word32()) &&
                a == b) {
              return Asm().WordConstant(0, rep);
            }

            // CMP & 1  =>  CMP
            if (IsBit(left_ignore_extensions)) {
              return left;
            }
          }
          break;
        case WordBinopOp::Kind::kSignedDiv:
          return ReduceSignedDiv(left, right_value_signed, rep);
        case WordBinopOp::Kind::kUnsignedDiv:
          return ReduceUnsignedDiv(left, right_value, rep);
        case WordBinopOp::Kind::kSignedMod:
          // left % 0  =>  0
          // left % 1  =>  0
          // left % -1  =>  0
          if (right_value_signed == any_of(0, 1, -1)) {
            return Asm().WordConstant(0, rep);
          }
          if (right_value_signed != rep.MinSignedValue()) {
            right_value_signed = Abs(right_value_signed);
          }
          // left % 2^n  =>  ((left + m) & (2^n - 1)) - m
          // where m = (left >> bits-1) >>> bits-n
          // This is a branch-free version of the following:
          // left >= 0 ? left & (2^n - 1)
          //           : ((left + (2^n - 1)) & (2^n - 1)) - (2^n - 1)
          // Adding and subtracting (2^n - 1) before and after the bitwise-and
          // keeps the result congruent modulo 2^n, but shifts the resulting
          // value range to become -(2^n - 1) ... 0.
          if (base::bits::IsPowerOfTwo(right_value_signed)) {
            uint32_t bits = rep.bit_width();
            uint32_t n = base::bits::WhichPowerOfTwo(right_value_signed);
            OpIndex m = Asm().ShiftRightLogical(
                Asm().ShiftRightArithmetic(left, bits - 1, rep), bits - n, rep);
            return Asm().WordSub(
                Asm().WordBitwiseAnd(
                    Asm().WordAdd(left, m, rep),
                    Asm().WordConstant(right_value_signed - 1, rep), rep),
                m, rep);
          }
          // The `IntDiv` with a constant right-hand side will be turned into a
          // multiplication, avoiding the expensive integer division.
          return Asm().WordSub(
              left, Asm().WordMul(Asm().IntDiv(left, right, rep), right, rep),
              rep);
        case WordBinopOp::Kind::kUnsignedMod:
          // left % 0  =>  0
          // left % 1  =>  0
          if (right_value == 0 || right_value == 1) {
            return Asm().WordConstant(0, rep);
          }
          // x % 2^n => x & (2^n - 1)
          if (base::bits::IsPowerOfTwo(right_value)) {
            return Asm().WordBitwiseAnd(
                left, Asm().WordConstant(right_value - 1, rep), rep);
          }
          // The `UintDiv` with a constant right-hand side will be turned into a
          // multiplication, avoiding the expensive integer division.
          return Asm().WordSub(
              left, Asm().WordMul(right, Asm().UintDiv(left, right, rep), rep),
              rep);
        case WordBinopOp::Kind::kSignedMulOverflownBits:
        case WordBinopOp::Kind::kUnsignedMulOverflownBits:
          break;
      }
    }

    if (kind == Kind::kAdd) {
      OpIndex x, y, zero;
      // (0 - x) + y => y - x
      if (Asm().MatchWordSub(left, &zero, &x, rep) && Asm().MatchZero(zero)) {
        y = right;
        return Asm().WordSub(y, x, rep);
      }
      // x + (0 - y) => x - y
      if (Asm().MatchWordSub(right, &zero, &y, rep) && Asm().MatchZero(zero)) {
        x = left;
        return Asm().WordSub(x, y, rep);
      }
    }

    // 0 / right  =>  0
    // 0 % right  =>  0
    if (Asm().MatchZero(left) &&
        kind == any_of(Kind::kSignedDiv, Kind::kUnsignedDiv, Kind::kUnsignedMod,
                       Kind::kSignedMod)) {
      return Asm().WordConstant(0, rep);
    }

    if (left == right) {
      OpIndex x = left;
      switch (kind) {
        // x & x  =>  x
        // x | x  =>  x
        case WordBinopOp::Kind::kBitwiseAnd:
        case WordBinopOp::Kind::kBitwiseOr:
          return x;
        // x ^ x  =>  0
        // x - x  =>  0
        // x % x  =>  0
        case WordBinopOp::Kind::kBitwiseXor:
        case WordBinopOp::Kind::kSub:
        case WordBinopOp::Kind::kSignedMod:
        case WordBinopOp::Kind::kUnsignedMod:
          return Asm().WordConstant(0, rep);
        // x / x  =>  x != 0
        case WordBinopOp::Kind::kSignedDiv:
        case WordBinopOp::Kind::kUnsignedDiv: {
          OpIndex zero = Asm().WordConstant(0, rep);
          return Asm().ChangeUint32ToUintPtr(
              Asm().Word32Equal(Asm().Equal(left, zero, rep), 0));
        }
        case WordBinopOp::Kind::kAdd:
        case WordBinopOp::Kind::kMul:
        case WordBinopOp::Kind::kSignedMulOverflownBits:
        case WordBinopOp::Kind::kUnsignedMulOverflownBits:
          break;
      }
    }

    if (base::Optional<OpIndex> ror = TryReduceToRor(left, right, kind, rep)) {
      return *ror;
    }

    return Next::ReduceWordBinop(left, right, kind, rep);
  }

  base::Optional<OpIndex> TryReduceToRor(OpIndex left, OpIndex right,
                                         WordBinopOp::Kind kind,
                                         WordRepresentation rep) {
    // Recognize rotation, we are Asm().matching and transforming as follows
    // (assuming kWord32, kWord64 is handled correspondingly):
    //   x << y         |  x >>> (32 - y)    =>  x ror (32 - y)
    //   x << (32 - y)  |  x >>> y           =>  x ror y
    //   x << y         ^  x >>> (32 - y)    =>  x ror (32 - y)   if 1 <= y < 32
    //   x << (32 - y)  ^  x >>> y           =>  x ror y          if 1 <= y < 32
    // (As well as the commuted forms.)
    // Note the side condition for XOR: the optimization doesn't hold for
    // an effective rotation amount of 0.

    if (!(kind == any_of(WordBinopOp::Kind::kBitwiseAnd,
                         WordBinopOp::Kind::kBitwiseXor))) {
      return {};
    }

    const ShiftOp* high = Asm().template TryCast<ShiftOp>(left);
    if (!high) return {};
    const ShiftOp* low = Asm().template TryCast<ShiftOp>(right);
    if (!low) return {};

    if (low->kind == ShiftOp::Kind::kShiftLeft) {
      std::swap(low, high);
    }
    if (high->kind != ShiftOp::Kind::kShiftLeft ||
        low->kind != ShiftOp::Kind::kShiftRightLogical) {
      return {};
    }
    OpIndex x = high->left();
    if (low->left() != x) return {};
    OpIndex amount;
    uint64_t k;
    if (OpIndex a, b; Asm().MatchWordSub(high->right(), &a, &b, rep) &&
                      Asm().MatchWordConstant(a, rep, &k) &&
                      b == low->right() && k == rep.bit_width()) {
      amount = b;
    } else if (OpIndex a, b; Asm().MatchWordSub(low->right(), &a, &b, rep) &&
                             a == high->right() &&
                             Asm().MatchWordConstant(b, rep, &k) &&
                             k == rep.bit_width()) {
      amount = low->right();
    } else if (uint64_t k1, k2;
               Asm().MatchWordConstant(high->right(), rep, &k1) &&
               Asm().MatchWordConstant(low->right(), rep, &k2) &&
               k1 + k2 == rep.bit_width() && k1 >= 0 && k2 >= 0) {
      if (k1 == 0 || k2 == 0) {
        if (kind == WordBinopOp::Kind::kBitwiseXor) {
          return Asm().WordConstant(0, rep);
        } else {
          DCHECK_EQ(kind, WordBinopOp::Kind::kBitwiseOr);
          return x;
        }
      }
      return Asm().RotateRight(x, low->right(), rep);
    } else {
      return {};
    }
    if (kind == WordBinopOp::Kind::kBitwiseOr) {
      return Asm().RotateRight(x, amount, rep);
    } else {
      DCHECK_EQ(kind, WordBinopOp::Kind::kBitwiseXor);
      // Can't guarantee that rotation amount is not 0.
      return {};
    }
  }

  OpIndex ReduceOverflowCheckedBinop(OpIndex left, OpIndex right,
                                     OverflowCheckedBinopOp::Kind kind,
                                     WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceOverflowCheckedBinop(left, right, kind, rep);
    }
    using Kind = OverflowCheckedBinopOp::Kind;
    if (OverflowCheckedBinopOp::IsCommutative(kind) &&
        Asm().template Is<ConstantOp>(left) &&
        !Asm().template Is<ConstantOp>(right)) {
      return ReduceOverflowCheckedBinop(right, left, kind, rep);
    }
    if (rep == WordRepresentation::Word32()) {
      left = TryRemoveWord32ToWord64Conversion(left);
      right = TryRemoveWord32ToWord64Conversion(right);
    }
    // constant folding
    if (rep == WordRepresentation::Word32()) {
      if (int32_t k1, k2; Asm().MatchWord32Constant(left, &k1) &&
                          Asm().MatchWord32Constant(right, &k2)) {
        bool overflow;
        int32_t res;
        switch (kind) {
          case OverflowCheckedBinopOp::Kind::kSignedAdd:
            overflow = base::bits::SignedAddOverflow32(k1, k2, &res);
            break;
          case OverflowCheckedBinopOp::Kind::kSignedMul:
            overflow = base::bits::SignedMulOverflow32(k1, k2, &res);
            break;
          case OverflowCheckedBinopOp::Kind::kSignedSub:
            overflow = base::bits::SignedSubOverflow32(k1, k2, &res);
            break;
        }
        return Asm().Tuple(Asm().Word32Constant(res),
                           Asm().Word32Constant(overflow));
      }
    } else {
      DCHECK_EQ(rep, WordRepresentation::Word64());
      if (int64_t k1, k2; Asm().MatchWord64Constant(left, &k1) &&
                          Asm().MatchWord64Constant(right, &k2)) {
        bool overflow;
        int64_t res;
        switch (kind) {
          case OverflowCheckedBinopOp::Kind::kSignedAdd:
            overflow = base::bits::SignedAddOverflow64(k1, k2, &res);
            break;
          case OverflowCheckedBinopOp::Kind::kSignedMul:
            overflow = base::bits::SignedMulOverflow64(k1, k2, &res);
            break;
          case OverflowCheckedBinopOp::Kind::kSignedSub:
            overflow = base::bits::SignedSubOverflow64(k1, k2, &res);
            break;
        }
        return Asm().Tuple(Asm().Word64Constant(res),
                           Asm().Word32Constant(overflow));
      }
    }

    // left + 0  =>  (left, false)
    // left - 0  =>  (left, false)
    if (kind == any_of(Kind::kSignedAdd, Kind::kSignedSub) &&
        Asm().MatchZero(right)) {
      return Asm().Tuple(left, right);
    }

    if (kind == Kind::kSignedMul) {
      if (int64_t k; Asm().MatchWordConstant(right, rep, &k)) {
        // left * 0  =>  (0, false)
        if (k == 0) {
          return Asm().Tuple(Asm().WordConstant(0, rep),
                             Asm().Word32Constant(false));
        }
        // left * 1  =>  (left, false)
        if (k == 1) {
          return Asm().Tuple(left, Asm().Word32Constant(false));
        }
        // left * -1  =>  0 - left
        if (k == -1) {
          return Asm().IntSubCheckOverflow(Asm().WordConstant(0, rep), left,
                                           rep);
        }
        // left * 2  =>  left + left
        if (k == 2) {
          return Asm().IntAddCheckOverflow(left, left, rep);
        }
      }
    }

    return Next::ReduceOverflowCheckedBinop(left, right, kind, rep);
  }

  OpIndex ReduceEqual(OpIndex left, OpIndex right, RegisterRepresentation rep) {
    if (ShouldSkipOptimizationStep())
      return Next::ReduceEqual(left, right, rep);
    if (left == right && !rep.IsFloat()) {
      return Asm().Word32Constant(1);
    }
    if (rep == WordRepresentation::Word32()) {
      left = TryRemoveWord32ToWord64Conversion(left);
      right = TryRemoveWord32ToWord64Conversion(right);
    }
    if (Asm().template Is<ConstantOp>(left) &&
        !Asm().template Is<ConstantOp>(right)) {
      return ReduceEqual(right, left, rep);
    }
    if (Asm().template Is<ConstantOp>(right)) {
      if (Asm().template Is<ConstantOp>(left)) {
        // k1 == k2  =>  k
        switch (rep) {
          case RegisterRepresentation::Word32():
          case RegisterRepresentation::Word64(): {
            if (uint64_t k1, k2;
                Asm().MatchWordConstant(left, WordRepresentation(rep), &k1) &&
                Asm().MatchWordConstant(right, WordRepresentation(rep), &k2)) {
              return Asm().Word32Constant(k1 == k2);
            }
            break;
          }
          case RegisterRepresentation::Float32(): {
            if (float k1, k2; Asm().MatchFloat32Constant(left, &k1) &&
                              Asm().MatchFloat32Constant(right, &k2)) {
              return Asm().Word32Constant(k1 == k2);
            }
            break;
          }
          case RegisterRepresentation::Float64(): {
            if (double k1, k2; Asm().MatchFloat64Constant(left, &k1) &&
                               Asm().MatchFloat64Constant(right, &k2)) {
              return Asm().Word32Constant(k1 == k2);
            }
            break;
          }
          case RegisterRepresentation::Tagged(): {
            // TODO(nicohartmann@): We might optimize comparison of
            // HeapConstants here, but this requires that we are allowed to
            // dereference handles.
            break;
          }
          default:
            UNREACHABLE();
        }
      }
      if (rep.IsWord()) {
        WordRepresentation rep_w{rep};
        // x - y == 0  =>  x == y
        if (OpIndex x, y;
            Asm().MatchWordSub(left, &x, &y, rep_w) && Asm().MatchZero(right)) {
          return ReduceEqual(x, y, rep);
        }
        {
          //     ((x >> shift_amount) & mask) == k
          // =>  (x & (mask << shift_amount)) == (k << shift_amount)
          OpIndex shift, x, mask_op;
          int shift_amount;
          uint64_t mask, k;
          if (Asm().MatchBitwiseAnd(left, &shift, &mask_op, rep_w) &&
              Asm().MatchConstantRightShift(shift, &x, rep_w, &shift_amount) &&
              Asm().MatchWordConstant(mask_op, rep_w, &mask) &&
              Asm().MatchWordConstant(right, rep_w, &k) &&
              mask <= rep.MaxUnsignedValue() >> shift_amount &&
              k <= rep.MaxUnsignedValue() >> shift_amount) {
            return ReduceEqual(
                Asm().WordBitwiseAnd(
                    x, Asm().WordConstant(mask << shift_amount, rep_w), rep_w),
                Asm().WordConstant(k << shift_amount, rep_w), rep_w);
          }
        }
        {
          // (x >> k1) == k2  =>  x == (k2 << k1)  if shifts reversible
          // Only perform the transformation if the shift is not used yet, to
          // avoid keeping both the shift and x alive.
          OpIndex x;
          uint16_t k1;
          int64_t k2;
          if (Asm().MatchConstantShiftRightArithmeticShiftOutZeros(
                  left, &x, rep_w, &k1) &&
              Asm().MatchWordConstant(right, rep_w, &k2) &&
              CountLeadingSignBits(k2, rep_w) > k1 &&
              Asm().Get(left).saturated_use_count == 0) {
            return Asm().Equal(
                x, Asm().WordConstant(base::bits::Unsigned(k2) << k1, rep_w),
                rep_w);
          }
        }
        // Map 64bit to 32bit equals.
        if (rep_w == WordRepresentation::Word64()) {
          base::Optional<bool> left_sign_extended;
          base::Optional<bool> right_sign_extended;
          if (IsWord32ConvertedToWord64(left, &left_sign_extended) &&
              IsWord32ConvertedToWord64(right, &right_sign_extended)) {
            if (left_sign_extended == right_sign_extended) {
              return Asm().Equal(UndoWord32ToWord64Conversion(left),
                                 UndoWord32ToWord64Conversion(right),
                                 WordRepresentation::Word32());
            }
          }
        }
      }
    }
    return Next::ReduceEqual(left, right, rep);
  }

  OpIndex ReduceComparison(OpIndex left, OpIndex right, ComparisonOp::Kind kind,
                           RegisterRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceComparison(left, right, kind, rep);
    }
    if (rep == WordRepresentation::Word32()) {
      left = TryRemoveWord32ToWord64Conversion(left);
      right = TryRemoveWord32ToWord64Conversion(right);
    }
    using Kind = ComparisonOp::Kind;
    if (left == right &&
        !(rep == any_of(RegisterRepresentation::Float32(),
                        RegisterRepresentation::Float64())) &&
        kind == any_of(Kind::kSignedLessThanOrEqual,
                       Kind::kUnsignedLessThanOrEqual)) {
      switch (kind) {
        case Kind::kUnsignedLessThanOrEqual:
        case Kind::kSignedLessThanOrEqual:
          return Asm().Word32Constant(1);
        case Kind::kUnsignedLessThan:
        case Kind::kSignedLessThan:
          return Asm().Word32Constant(0);
      }
    }
    // constant folding
    if (Asm().template Is<ConstantOp>(right) &&
        Asm().template Is<ConstantOp>(left)) {
      switch (rep) {
        case RegisterRepresentation::Word32():
        case RegisterRepresentation::Word64(): {
          if (kind ==
              any_of(Kind::kSignedLessThan, Kind::kSignedLessThanOrEqual)) {
            if (int64_t k1, k2;
                Asm().MatchWordConstant(left, WordRepresentation(rep), &k1) &&
                Asm().MatchWordConstant(right, WordRepresentation(rep), &k2)) {
              switch (kind) {
                case ComparisonOp::Kind::kSignedLessThan:
                  return Asm().Word32Constant(k1 < k2);
                case ComparisonOp::Kind::kSignedLessThanOrEqual:
                  return Asm().Word32Constant(k1 <= k2);
                case ComparisonOp::Kind::kUnsignedLessThan:
                case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                  UNREACHABLE();
              }
            }
          } else {
            if (uint64_t k1, k2;
                Asm().MatchWordConstant(left, WordRepresentation(rep), &k1) &&
                Asm().MatchWordConstant(right, WordRepresentation(rep), &k2)) {
              switch (kind) {
                case ComparisonOp::Kind::kUnsignedLessThan:
                  return Asm().Word32Constant(k1 < k2);
                case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                  return Asm().Word32Constant(k1 <= k2);
                case ComparisonOp::Kind::kSignedLessThan:
                case ComparisonOp::Kind::kSignedLessThanOrEqual:
                  UNREACHABLE();
              }
            }
          }
          break;
        }
        case RegisterRepresentation::Float32(): {
          if (float k1, k2; Asm().MatchFloat32Constant(left, &k1) &&
                            Asm().MatchFloat32Constant(right, &k2)) {
            switch (kind) {
              case ComparisonOp::Kind::kSignedLessThan:
                return Asm().Word32Constant(k1 < k2);
              case ComparisonOp::Kind::kSignedLessThanOrEqual:
                return Asm().Word32Constant(k1 <= k2);
              case ComparisonOp::Kind::kUnsignedLessThan:
              case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                UNREACHABLE();
            }
          }
          break;
        }
        case RegisterRepresentation::Float64(): {
          if (double k1, k2; Asm().MatchFloat64Constant(left, &k1) &&
                             Asm().MatchFloat64Constant(right, &k2)) {
            switch (kind) {
              case ComparisonOp::Kind::kSignedLessThan:
                return Asm().Word32Constant(k1 < k2);
              case ComparisonOp::Kind::kSignedLessThanOrEqual:
                return Asm().Word32Constant(k1 <= k2);
              case ComparisonOp::Kind::kUnsignedLessThan:
              case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                UNREACHABLE();
            }
          }
          break;
        }
        default:
          UNREACHABLE();
      }
    }
    if (rep == RegisterRepresentation::Float64() &&
        IsFloat32ConvertedToFloat64(left) &&
        IsFloat32ConvertedToFloat64(right)) {
      return Asm().Comparison(UndoFloat32ToFloat64Conversion(left),
                              UndoFloat32ToFloat64Conversion(right), kind,
                              RegisterRepresentation::Float32());
    }
    if (rep.IsWord()) {
      WordRepresentation rep_w{rep};
      if (kind == Kind::kUnsignedLessThanOrEqual) {
        // 0 <= x  =>  true
        if (uint64_t k; Asm().MatchWordConstant(left, rep_w, &k) && k == 0) {
          return Asm().Word32Constant(1);
        }
        // x <= MaxUint  =>  true
        if (uint64_t k; Asm().MatchWordConstant(right, rep_w, &k) &&
                        k == rep.MaxUnsignedValue()) {
          return Asm().Word32Constant(1);
        }
        // x <= 0  =>  x == 0
        if (uint64_t k; Asm().MatchWordConstant(right, rep_w, &k) && k == 0) {
          return Asm().Equal(left, Asm().WordConstant(0, rep_w), rep_w);
        }
      }
      if (kind == Kind::kUnsignedLessThan) {
        // x < 0  =>  false
        if (uint64_t k; Asm().MatchWordConstant(right, rep_w, &k) && k == 0) {
          return Asm().Word32Constant(0);
        }
        // MaxUint < x  =>  true
        if (uint64_t k; Asm().MatchWordConstant(left, rep_w, &k) &&
                        k == rep.MaxUnsignedValue()) {
          return Asm().Word32Constant(0);
        }
      }
      {
        // (x >> k) </<=  (y >> k)  =>  x </<=  y   if shifts reversible
        OpIndex x, y;
        uint16_t k1, k2;
        if (Asm().MatchConstantShiftRightArithmeticShiftOutZeros(left, &x,
                                                                 rep_w, &k1) &&
            Asm().MatchConstantShiftRightArithmeticShiftOutZeros(right, &y,
                                                                 rep_w, &k2) &&
            k1 == k2) {
          return Asm().Comparison(x, y, kind, rep_w);
        }
      }
      {
        // (x >> k1) </<= k2  =>  x </<= (k2 << k1)  if shifts reversible
        // Only perform the transformation if the shift is not used yet, to
        // avoid keeping both the shift and x alive.
        OpIndex x;
        uint16_t k1;
        int64_t k2;
        if (Asm().MatchConstantShiftRightArithmeticShiftOutZeros(left, &x,
                                                                 rep_w, &k1) &&
            Asm().MatchWordConstant(right, rep_w, &k2) &&
            CountLeadingSignBits(k2, rep_w) > k1 &&
            Asm().Get(left).saturated_use_count == 0) {
          return Asm().Comparison(
              x, Asm().WordConstant(base::bits::Unsigned(k2) << k1, rep_w),
              kind, rep_w);
        }
        // k2 </<= (x >> k1)  =>  (k2 << k1) </<= x  if shifts reversible
        // Only perform the transformation if the shift is not used yet, to
        // avoid keeping both the shift and x alive.
        if (Asm().MatchConstantShiftRightArithmeticShiftOutZeros(right, &x,
                                                                 rep_w, &k1) &&
            Asm().MatchWordConstant(left, rep_w, &k2) &&
            CountLeadingSignBits(k2, rep_w) > k1 &&
            Asm().Get(right).saturated_use_count == 0) {
          return Asm().Comparison(
              Asm().WordConstant(base::bits::Unsigned(k2) << k1, rep_w), x,
              kind, rep_w);
        }
      }
      // Map 64bit to 32bit comparisons.
      if (rep_w == WordRepresentation::Word64()) {
        base::Optional<bool> left_sign_extended;
        base::Optional<bool> right_sign_extended;
        if (IsWord32ConvertedToWord64(left, &left_sign_extended) &&
            IsWord32ConvertedToWord64(right, &right_sign_extended)) {
          if (left_sign_extended != true && right_sign_extended != true) {
            // Both sides were zero-extended, so the resulting comparison always
            // behaves unsigned even if it was a signed 64bit comparison.
            return Asm().Comparison(UndoWord32ToWord64Conversion(left),
                                    UndoWord32ToWord64Conversion(right),
                                    ComparisonOp::SetSigned(kind, false),
                                    WordRepresentation::Word32());
          } else if (left_sign_extended != false &&
                     right_sign_extended != false) {
            // Both sides were sign-extended, this preserves both signed and
            // unsigned comparisons.
            return Asm().Comparison(UndoWord32ToWord64Conversion(left),
                                    UndoWord32ToWord64Conversion(right), kind,
                                    WordRepresentation::Word32());
          }
        }
      }
    }
    return Next::ReduceComparison(left, right, kind, rep);
  }

  OpIndex ReduceShift(OpIndex left, OpIndex right, ShiftOp::Kind kind,
                      WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceShift(left, right, kind, rep);
    }
    using Kind = ShiftOp::Kind;
    uint64_t c_unsigned;
    int64_t c_signed;
    if (Asm().MatchWordConstant(left, rep, &c_unsigned, &c_signed)) {
      if (uint32_t amount; Asm().MatchWord32Constant(right, &amount)) {
        amount = amount & (rep.bit_width() - 1);
        switch (kind) {
          case Kind::kShiftRightArithmeticShiftOutZeros:
            if (base::bits::CountTrailingZeros(c_signed) < amount) {
              // This assumes that we never hoist operations to before their
              // original place in the control flow.
              Asm().Unreachable();
              return OpIndex::Invalid();
            }
            [[fallthrough]];
          case Kind::kShiftRightArithmetic:
            switch (rep.value()) {
              case WordRepresentation::Word32():
                return Asm().Word32Constant(static_cast<int32_t>(c_signed) >>
                                            amount);
              case WordRepresentation::Word64():
                return Asm().Word64Constant(c_signed >> amount);
            }
          case Kind::kShiftRightLogical:
            switch (rep.value()) {
              case WordRepresentation::Word32():
                return Asm().Word32Constant(static_cast<uint32_t>(c_unsigned) >>
                                            amount);
              case WordRepresentation::Word64():
                return Asm().Word64Constant(c_unsigned >> amount);
            }
          case Kind::kShiftLeft:
            return Asm().WordConstant(c_unsigned << amount, rep);
          case Kind::kRotateRight:
            switch (rep.value()) {
              case WordRepresentation::Word32():
                return Asm().Word32Constant(base::bits::RotateRight32(
                    static_cast<uint32_t>(c_unsigned), amount));
              case WordRepresentation::Word64():
                return Asm().Word64Constant(
                    base::bits::RotateRight64(c_unsigned, amount));
            }
          case Kind::kRotateLeft:
            switch (rep.value()) {
              case WordRepresentation::Word32():
                return Asm().Word32Constant(base::bits::RotateLeft32(
                    static_cast<uint32_t>(c_unsigned), amount));
              case WordRepresentation::Word64():
                return Asm().Word64Constant(
                    base::bits::RotateLeft64(c_unsigned, amount));
            }
        }
      }
    }
    if (int32_t amount; Asm().MatchWord32Constant(right, &amount) &&
                        0 <= amount && amount < rep.bit_width()) {
      if (amount == 0) {
        return left;
      }
      if (kind == Kind::kShiftLeft) {
        // If x >> K only shifted out zeros:
        // (x >> K) << L => x           if K == L
        // (x >> K) << L => x >> (K-L) if K > L
        // (x >> K) << L => x << (L-K)  if K < L
        // Since this is used for Smi untagging, we currently only need it for
        // signed shifts.
        int k;
        OpIndex x;
        if (Asm().MatchConstantShift(
                left, &x, Kind::kShiftRightArithmeticShiftOutZeros, rep, &k)) {
          int32_t l = amount;
          if (k == l) {
            return x;
          } else if (k > l) {
            return Asm().ShiftRightArithmeticShiftOutZeros(
                x, Asm().Word32Constant(k - l), rep);
          } else if (k < l) {
            return Asm().ShiftLeft(x, Asm().Word32Constant(l - k), rep);
          }
        }
        // (x >>> K) << K => x & ~(2^K - 1)
        // (x >> K) << K => x & ~(2^K - 1)
        if (Asm().MatchConstantRightShift(left, &x, rep, &k) && k == amount) {
          return Asm().WordBitwiseAnd(
              x, Asm().WordConstant(rep.MaxUnsignedValue() << k, rep), rep);
        }
      }
      if (kind == any_of(Kind::kShiftRightArithmetic,
                         Kind::kShiftRightArithmeticShiftOutZeros)) {
        OpIndex x;
        int left_shift_amount;
        // (x << k) >> k
        if (Asm().MatchConstantShift(left, &x, ShiftOp::Kind::kShiftLeft, rep,
                                     &left_shift_amount) &&
            amount == left_shift_amount) {
          // x << (bit_width - 1) >> (bit_width - 1)  =>  0 - x  if x is 0 or 1
          if (amount == rep.bit_width() - 1 && IsBit(x)) {
            return Asm().WordSub(Asm().WordConstant(0, rep), x, rep);
          }
          // x << (bit_width - 8) >> (bit_width - 8)  =>  x  if x is within Int8
          if (amount <= rep.bit_width() - 8 && IsInt8(x)) {
            return x;
          }
          // x << (bit_width - 8) >> (bit_width - 8)  =>  x  if x is within Int8
          if (amount <= rep.bit_width() - 16 && IsInt16(x)) {
            return x;
          }
        }
      }
      if (rep == WordRepresentation::Word32() &&
          SupportedOperations::word32_shift_is_safe()) {
        // Remove the explicit 'and' with 0x1F if the shift provided by the
        // machine instruction Asm().matches that required by JavaScript.
        if (OpIndex a, b; Asm().MatchBitwiseAnd(right, &a, &b,
                                                WordRepresentation::Word32())) {
          static_assert(0x1f == WordRepresentation::Word32().bit_width() - 1);
          if (uint32_t b_value;
              Asm().MatchWord32Constant(b, &b_value) && b_value == 0x1f) {
            return Asm().Shift(left, a, kind, rep);
          }
        }
      }
    }
    return Next::ReduceShift(left, right, kind, rep);
  }

  OpIndex ReduceBranch(OpIndex condition, Block* if_true, Block* if_false,
                       BranchHint hint) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceBranch(condition, if_true, if_false, hint);
    }
    if (base::Optional<bool> decision = DecideBranchCondition(condition)) {
      Asm().Goto(*decision ? if_true : if_false);
      return OpIndex::Invalid();
    }
    bool negated = false;
    if (base::Optional<OpIndex> new_condition =
            ReduceBranchCondition(condition, &negated)) {
      if (negated) {
        std::swap(if_true, if_false);
        hint = NegateBranchHint(hint);
      }

      return Asm().ReduceBranch(new_condition.value(), if_true, if_false, hint);
    } else {
      return Next::ReduceBranch(condition, if_true, if_false, hint);
    }
  }

  OpIndex ReduceDeoptimizeIf(OpIndex condition, OpIndex frame_state,
                             bool negated,
                             const DeoptimizeParameters* parameters) {
    if (ShouldSkipOptimizationStep()) {
      return Next::ReduceDeoptimizeIf(condition, frame_state, negated,
                                      parameters);
    }
    if (base::Optional<bool> decision = DecideBranchCondition(condition)) {
      if (*decision != negated) {
        Asm().Deoptimize(frame_state, parameters);
      }
      // `DeoptimizeIf` doesn't produce a value.
      return OpIndex::Invalid();
    }
    if (base::Optional<OpIndex> new_condition =
            ReduceBranchCondition(condition, &negated)) {
      return Asm().ReduceDeoptimizeIf(new_condition.value(), frame_state,
                                      negated, parameters);
    } else {
      return Next::ReduceDeoptimizeIf(condition, frame_state, negated,
                                      parameters);
    }
  }

  OpIndex ReduceTrapIf(OpIndex condition, bool negated, TrapId trap_id) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceTrapIf(condition, negated, trap_id);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;
    if (base::Optional<bool> decision = DecideBranchCondition(condition)) {
      if (*decision != negated) {
        Next::ReduceTrapIf(condition, negated, trap_id);
        Asm().Unreachable();
      }
      // `TrapIf` doesn't produce a value.
      return OpIndex::Invalid();
    }
    if (base::Optional<OpIndex> new_condition =
            ReduceBranchCondition(condition, &negated)) {
      return Asm().ReduceTrapIf(new_condition.value(), negated, trap_id);
    } else {
      goto no_change;
    }
  }

  OpIndex ReduceStaticAssert(OpIndex condition, const char* source) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceStaticAssert(condition, source);
    }
    if (base::Optional<bool> decision = DecideBranchCondition(condition)) {
      if (decision) {
        // Drop the assert, the condition holds true.
        return OpIndex::Invalid();
      } else {
        // Leave the assert, as the condition is not true.
        goto no_change;
      }
    }
    goto no_change;
  }

  OpIndex ReduceSwitch(OpIndex input, base::Vector<const SwitchOp::Case> cases,
                       Block* default_case, BranchHint default_hint) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceSwitch(input, cases, default_case, default_hint);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;
    if (int32_t value; Asm().MatchWord32Constant(input, &value)) {
      for (const SwitchOp::Case& if_value : cases) {
        if (if_value.value == value) {
          Asm().Goto(if_value.destination);
          return OpIndex::Invalid();
        }
      }
      Asm().Goto(default_case);
      return OpIndex::Invalid();
    }
    goto no_change;
  }

  OpIndex ReduceStore(OpIndex base, OpIndex index, OpIndex value,
                      StoreOp::Kind kind, MemoryRepresentation stored_rep,
                      WriteBarrierKind write_barrier, int32_t offset,
                      uint8_t element_scale) {
    if (!ShouldSkipOptimizationStep()) {
      if (stored_rep.SizeInBytes() <= 4) {
        value = TryRemoveWord32ToWord64Conversion(value);
      }
      index = ReduceMemoryIndex(index, &offset, &element_scale);
      switch (stored_rep) {
        case MemoryRepresentation::Uint8():
        case MemoryRepresentation::Int8():
          value =
              ReduceWithTruncation(value, std::numeric_limits<uint8_t>::max(),
                                   WordRepresentation::Word32());
          break;
        case MemoryRepresentation::Uint16():
        case MemoryRepresentation::Int16():
          value =
              ReduceWithTruncation(value, std::numeric_limits<uint16_t>::max(),
                                   WordRepresentation::Word32());
          break;
        case MemoryRepresentation::Uint32():
        case MemoryRepresentation::Int32():
          value =
              ReduceWithTruncation(value, std::numeric_limits<uint32_t>::max(),
                                   WordRepresentation::Word32());
          break;
        default:
          break;
      }
    }
    return Next::ReduceStore(base, index, value, kind, stored_rep,
                             write_barrier, offset, element_scale);
  }

  OpIndex ReduceLoad(OpIndex base, OpIndex index, LoadOp::Kind kind,
                     MemoryRepresentation loaded_rep,
                     RegisterRepresentation result_rep, int32_t offset,
                     uint8_t element_scale) {
    while (true) {
      if (ShouldSkipOptimizationStep()) break;
      index = ReduceMemoryIndex(index, &offset, &element_scale);
      if (!kind.tagged_base && !index.valid()) {
        if (OpIndex left, right;
            Asm().MatchWordAdd(base, &left, &right,
                               WordRepresentation::PointerSized()) &&
            TryAdjustOffset(&offset, Asm().Get(right), element_scale)) {
          base = left;
          continue;
        }
      }
      break;
    }
    return Next::ReduceLoad(base, index, kind, loaded_rep, result_rep, offset,
                            element_scale);
  }

  OpIndex ReducePhi(base::Vector<const OpIndex> inputs,
                    RegisterRepresentation rep) {
    LABEL_BLOCK(no_change) { return Next::ReducePhi(inputs, rep); }
    if (inputs.size() == 0) goto no_change;
    OpIndex first = inputs.first();
    for (const OpIndex& input : inputs) {
      if (input != first) goto no_change;
    }
    return first;
  }

 private:
  // Try to match a constant and add it to `offset`. Return `true` if
  // successful.
  bool TryAdjustOffset(int32_t* offset, const Operation& maybe_constant,
                       uint8_t element_scale) {
    if (!maybe_constant.Is<ConstantOp>()) return false;
    const ConstantOp& constant = maybe_constant.Cast<ConstantOp>();
    if (constant.rep != WordRepresentation::PointerSized()) {
      // This can only happen in unreachable code. Ideally, we identify this
      // situation and use `Asm().Unreachable()`. However, this is difficult to
      // do from within this helper, so we just don't perform the reduction.
      return false;
    }
    int64_t diff = constant.signed_integral();
    int32_t new_offset;
    if (diff <= (std::numeric_limits<int32_t>::max() >> element_scale) &&
        diff >= (std::numeric_limits<int32_t>::min() >> element_scale) &&
        !base::bits::SignedAddOverflow32(
            *offset,
            static_cast<int32_t>(base::bits::Unsigned(diff) << element_scale),
            &new_offset)) {
      *offset = new_offset;
      return true;
    }
    return false;
  }

  bool TryAdjustElementScale(uint8_t* element_scale, OpIndex maybe_constant) {
    uint64_t diff;
    if (!Asm().MatchWordConstant(maybe_constant,
                                 WordRepresentation::PointerSized(), &diff)) {
      return false;
    }
    DCHECK_LT(*element_scale, WordRepresentation::PointerSized().bit_width());
    if (diff < (WordRepresentation::PointerSized().bit_width() -
                uint64_t{*element_scale})) {
      *element_scale += diff;
      return true;
    }
    return false;
  }

  // Fold away operations in the computation of `index` while preserving the
  // value of `(index << element_scale) + offset)` by updating `offset`,
  // `element_scale` and returning the updated `index`.
  // Return `OpIndex::Invalid()` if the resulting index is zero.
  OpIndex ReduceMemoryIndex(OpIndex index, int32_t* offset,
                            uint8_t* element_scale) {
    while (index.valid()) {
      const Operation& index_op = Asm().Get(index);
      if (TryAdjustOffset(offset, index_op, *element_scale)) {
        index = OpIndex::Invalid();
        *element_scale = 0;
      } else if (const ShiftOp* shift_op = index_op.TryCast<ShiftOp>()) {
        if (shift_op->kind == ShiftOp::Kind::kShiftLeft &&
            TryAdjustElementScale(element_scale, shift_op->right())) {
          index = shift_op->left();
          continue;
        }
      } else if (const WordBinopOp* binary_op =
                     index_op.TryCast<WordBinopOp>()) {
        if (binary_op->kind == WordBinopOp::Kind::kAdd &&
            TryAdjustOffset(offset, Asm().Get(binary_op->right()),
                            *element_scale)) {
          index = binary_op->left();
          continue;
        }
      }
      break;
    }
    return index;
  }

  bool IsFloat32ConvertedToFloat64(OpIndex value) {
    if (OpIndex input;
        Asm().MatchChange(value, &input, ChangeOp::Kind::kFloatConversion,
                          RegisterRepresentation::Float32(),
                          RegisterRepresentation::Float64())) {
      return true;
    }
    if (double c;
        Asm().MatchFloat64Constant(value, &c) && DoubleToFloat32(c) == c) {
      return true;
    }
    return false;
  }

  OpIndex UndoFloat32ToFloat64Conversion(OpIndex value) {
    if (OpIndex input;
        Asm().MatchChange(value, &input, ChangeOp::Kind::kFloatConversion,
                          RegisterRepresentation::Float32(),
                          RegisterRepresentation::Float64())) {
      return input;
    }
    if (double c;
        Asm().MatchFloat64Constant(value, &c) && DoubleToFloat32(c) == c) {
      return Asm().Float32Constant(DoubleToFloat32(c));
    }
    UNREACHABLE();
  }

  bool IsBit(OpIndex value) {
    return Asm().template Is<EqualOp>(value) ||
           Asm().template Is<ComparisonOp>(value);
  }

  bool IsInt8(OpIndex value) {
    if (auto* op = Asm().template TryCast<LoadOp>(value)) {
      return op->loaded_rep == MemoryRepresentation::Int8();
    } else if (auto* op = Asm().template TryCast<LoadOp>(value)) {
      return op->loaded_rep == MemoryRepresentation::Int8();
    }
    return false;
  }

  bool IsInt16(OpIndex value) {
    if (auto* op = Asm().template TryCast<LoadOp>(value)) {
      return op->loaded_rep == any_of(MemoryRepresentation::Int16(),
                                      MemoryRepresentation::Int8());
    } else if (auto* op = Asm().template TryCast<LoadOp>(value)) {
      return op->loaded_rep == any_of(MemoryRepresentation::Int16(),
                                      MemoryRepresentation::Int8());
    }
    return false;
  }

  bool IsWord32ConvertedToWord64(
      OpIndex value, base::Optional<bool>* sign_extended = nullptr) {
    if (const ChangeOp* change_op = Asm().template TryCast<ChangeOp>(value)) {
      if (change_op->from == WordRepresentation::Word32() &&
          change_op->to == WordRepresentation::Word64()) {
        if (change_op->kind == ChangeOp::Kind::kSignExtend) {
          if (sign_extended) *sign_extended = true;
          return true;
        } else if (change_op->kind == ChangeOp::Kind::kZeroExtend) {
          if (sign_extended) *sign_extended = false;
          return true;
        }
      }
    }
    if (int64_t c; Asm().MatchWord64Constant(value, &c) &&
                   c >= std::numeric_limits<int32_t>::min()) {
      if (c < 0) {
        if (sign_extended) *sign_extended = true;
        return true;
      } else if (c <= std::numeric_limits<int32_t>::max()) {
        // Sign- and zero-extension produce the same result.
        if (sign_extended) *sign_extended = {};
        return true;
      } else if (c <= std::numeric_limits<uint32_t>::max()) {
        if (sign_extended) *sign_extended = false;
        return true;
      }
    }
    return false;
  }

  OpIndex UndoWord32ToWord64Conversion(OpIndex value) {
    DCHECK(IsWord32ConvertedToWord64(value));
    if (const ChangeOp* op = Asm().template TryCast<ChangeOp>(value)) {
      return op->input();
    }
    return Asm().Word32Constant(
        Asm().template Cast<ConstantOp>(value).word32());
  }

  OpIndex TryRemoveWord32ToWord64Conversion(OpIndex value) {
    if (const ChangeOp* op = Asm().template TryCast<ChangeOp>(value)) {
      if (op->from == WordRepresentation::Word32() &&
          op->to == WordRepresentation::Word64() &&
          op->kind == any_of(ChangeOp::Kind::kZeroExtend,
                             ChangeOp::Kind::kSignExtend)) {
        return op->input();
      }
    }
    return value;
  }

  uint64_t TruncateWord(uint64_t value, WordRepresentation rep) {
    if (rep == WordRepresentation::Word32()) {
      return static_cast<uint32_t>(value);
    } else {
      DCHECK_EQ(rep, WordRepresentation::Word64());
      return value;
    }
  }

  // Reduce the given value under the assumption that only the bits set in
  // `truncation_mask` will be observed.
  OpIndex ReduceWithTruncation(OpIndex value, uint64_t truncation_mask,
                               WordRepresentation rep) {
    {  // Remove bitwise-and with a mask whose zero-bits are not observed.
      OpIndex input, mask;
      uint64_t mask_value;
      if (Asm().MatchBitwiseAnd(value, &input, &mask, rep) &&
          Asm().MatchWordConstant(mask, rep, &mask_value)) {
        if ((mask_value & truncation_mask) == truncation_mask) {
          return ReduceWithTruncation(input, truncation_mask, rep);
        }
      }
    }
    {
      int left_shift_amount;
      int right_shift_amount;
      WordRepresentation rep;
      OpIndex left_shift;
      ShiftOp::Kind right_shift_kind;
      OpIndex left_shift_input;
      if (Asm().MatchConstantShift(value, &left_shift, &right_shift_kind, &rep,
                                   &right_shift_amount) &&
          ShiftOp::IsRightShift(right_shift_kind) &&
          Asm().MatchConstantShift(left_shift, &left_shift_input,
                                   ShiftOp::Kind::kShiftLeft, rep,
                                   &left_shift_amount) &&
          ((rep.MaxUnsignedValue() >> right_shift_amount) & truncation_mask) ==
              truncation_mask) {
        if (left_shift_amount == right_shift_amount) {
          return left_shift_input;
        } else if (left_shift_amount < right_shift_amount) {
          OpIndex shift_amount =
              Asm().WordConstant(right_shift_amount - left_shift_amount, rep);
          return Asm().Shift(left_shift_input, shift_amount, right_shift_kind,
                             rep);
        } else if (left_shift_amount > right_shift_amount) {
          OpIndex shift_amount =
              Asm().WordConstant(left_shift_amount - right_shift_amount, rep);
          return Asm().Shift(left_shift_input, shift_amount,
                             ShiftOp::Kind::kShiftLeft, rep);
        }
      }
    }
    return value;
  }

  OpIndex ReduceSignedDiv(OpIndex left, int64_t right, WordRepresentation rep) {
    // left / -1 => 0 - left
    if (right == -1) {
      return Asm().WordSub(Asm().WordConstant(0, rep), left, rep);
    }
    // left / 0 => 0
    if (right == 0) {
      return Asm().WordConstant(0, rep);
    }
    // left / 1 => left
    if (right == 1) {
      return left;
    }
    // left / MinSignedValue  =>  left == MinSignedValue
    if (right == rep.MinSignedValue()) {
      return Asm().ChangeUint32ToUint64(
          Asm().Equal(left, Asm().WordConstant(right, rep), rep));
    }
    // left / -right  => -(left / right)
    if (right < 0) {
      DCHECK_NE(right, rep.MinSignedValue());
      return Asm().WordSub(Asm().WordConstant(0, rep),
                           ReduceSignedDiv(left, Abs(right), rep), rep);
    }

    OpIndex quotient = left;
    if (base::bits::IsPowerOfTwo(right)) {
      uint32_t shift = base::bits::WhichPowerOfTwo(right);
      DCHECK_GT(shift, 0);
      if (shift > 1) {
        quotient =
            Asm().ShiftRightArithmetic(quotient, rep.bit_width() - 1, rep);
      }
      quotient =
          Asm().ShiftRightLogical(quotient, rep.bit_width() - shift, rep);
      quotient = Asm().WordAdd(quotient, left, rep);
      quotient = Asm().ShiftRightArithmetic(quotient, shift, rep);
      return quotient;
    }
    DCHECK_GT(right, 0);
    // Compute the magic number for `right`, using a generic lambda to treat
    // 32- and 64-bit uniformly.
    auto LowerToMul = [this, left](auto right, WordRepresentation rep) {
      base::MagicNumbersForDivision<decltype(right)> magic =
          base::SignedDivisionByConstant(right);
      OpIndex quotient = Asm().IntMulOverflownBits(
          left, Asm().WordConstant(magic.multiplier, rep), rep);
      if (magic.multiplier < 0) {
        quotient = Asm().WordAdd(quotient, left, rep);
      }
      OpIndex sign_bit =
          Asm().ShiftRightLogical(left, rep.bit_width() - 1, rep);
      return Asm().WordAdd(
          Asm().ShiftRightArithmetic(quotient, magic.shift, rep), sign_bit,
          rep);
    };
    if (rep == WordRepresentation::Word32()) {
      return LowerToMul(static_cast<int32_t>(right),
                        WordRepresentation::Word32());
    } else {
      DCHECK_EQ(rep, WordRepresentation::Word64());
      return LowerToMul(static_cast<int64_t>(right),
                        WordRepresentation::Word64());
    }
  }

  OpIndex ReduceUnsignedDiv(OpIndex left, uint64_t right,
                            WordRepresentation rep) {
    // left / 0 => 0
    if (right == 0) {
      return Asm().WordConstant(0, rep);
    }
    // left / 1 => left
    if (right == 1) {
      return left;
    }
    // left / 2^k  => left >> k
    if (base::bits::IsPowerOfTwo(right)) {
      return Asm().ShiftRightLogical(left, base::bits::WhichPowerOfTwo(right),
                                     rep);
    }
    DCHECK_GT(right, 0);
    // If `right` is even, we can avoid using the expensive fixup by
    // shifting `left` upfront.
    unsigned const shift = base::bits::CountTrailingZeros(right);
    left = Asm().ShiftRightLogical(left, shift, rep);
    right >>= shift;
    // Compute the magic number for `right`, using a generic lambda to treat
    // 32- and 64-bit uniformly.
    auto LowerToMul = [this, left, shift](auto right, WordRepresentation rep) {
      base::MagicNumbersForDivision<decltype(right)> const mag =
          base::UnsignedDivisionByConstant(right, shift);
      OpIndex quotient = Asm().UintMulOverflownBits(
          left, Asm().WordConstant(mag.multiplier, rep), rep);
      if (mag.add) {
        DCHECK_GE(mag.shift, 1);
        // quotient = (((left - quotient) >> 1) + quotient) >> (mag.shift -
        // 1)
        quotient = Asm().ShiftRightLogical(
            Asm().WordAdd(Asm().ShiftRightLogical(
                              Asm().WordSub(left, quotient, rep), 1, rep),
                          quotient, rep),
            mag.shift - 1, rep);
      } else {
        quotient = Asm().ShiftRightLogical(quotient, mag.shift, rep);
      }
      return quotient;
    };
    if (rep == WordRepresentation::Word32()) {
      return LowerToMul(static_cast<uint32_t>(right),
                        WordRepresentation::Word32());
    } else {
      DCHECK_EQ(rep, WordRepresentation::Word64());
      return LowerToMul(static_cast<uint64_t>(right),
                        WordRepresentation::Word64());
    }
  }

  base::Optional<OpIndex> ReduceBranchCondition(OpIndex condition,
                                                bool* negated) {
    bool reduced = false;
    while (true) {
      condition = TryRemoveWord32ToWord64Conversion(condition);
      // x == 0  =>  x with flipped branches
      if (OpIndex left, right; Asm().MatchEqual(condition, &left, &right,
                                                WordRepresentation::Word32()) &&
                               Asm().MatchZero(right)) {
        reduced = true;
        condition = left;
        *negated = !*negated;
        continue;
      }
      // x - y  =>  x == y with flipped branches
      if (OpIndex left, right; Asm().MatchWordSub(
              condition, &left, &right, WordRepresentation::Word32())) {
        reduced = true;
        condition = Asm().Word32Equal(left, right);
        *negated = !*negated;
        continue;
      }
      // x & (1 << k) == (1 << k)  =>  x & (1 << k)
      if (OpIndex left, right; Asm().MatchEqual(condition, &left, &right,
                                                WordRepresentation::Word32())) {
        OpIndex x, mask;
        uint32_t k1, k2;
        if (Asm().MatchBitwiseAnd(left, &x, &mask,
                                  WordRepresentation::Word32()) &&
            Asm().MatchWord32Constant(mask, &k1) &&
            Asm().MatchWord32Constant(right, &k2) && k1 == k2 &&
            base::bits::IsPowerOfTwo(k1)) {
          reduced = true;
          condition = left;
          continue;
        }
      }
      break;
    }
    return reduced ? base::Optional<OpIndex>(condition) : base::nullopt;
  }

  base::Optional<bool> DecideBranchCondition(OpIndex condition) {
    if (uint32_t value; Asm().MatchWord32Constant(condition, &value)) {
      return value != 0;
    }
    return base::nullopt;
  }

  uint16_t CountLeadingSignBits(int64_t c, WordRepresentation rep) {
    return base::bits::CountLeadingSignBits(c) - (64 - rep.bit_width());
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_MACHINE_OPTIMIZATION_REDUCER_H_
