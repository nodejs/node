// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_MACHINE_OPTIMIZATION_ASSEMBLER_H_
#define V8_COMPILER_TURBOSHAFT_MACHINE_OPTIMIZATION_ASSEMBLER_H_

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
#include "src/codegen/machine-type.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/numbers/conversions.h"

namespace v8 ::internal::compiler::turboshaft {

// The MachineOptimizationAssembler performs basic optimizations on low-level
// operations that can be performed on-the-fly, without requiring type analysis
// or analyzing uses. It largely corresponds to MachineOperatorReducer in
// sea-of-nodes Turbofan.
template <class Base, bool signalling_nan_possible>
class MachineOptimizationAssembler
    : public AssemblerInterface<
          MachineOptimizationAssembler<Base, signalling_nan_possible>, Base> {
 public:
  template <class T>
  bool Is(OpIndex op) {
    return Base::template Is<T>(op);
  }
  template <class T>
  const T& Cast(OpIndex op) {
    return Base::template Cast<T>(op);
  }
  template <class T>
  const T* TryCast(OpIndex op) {
    return Base::template TryCast<T>(op);
  }
  using Base::Get;
  using Base::graph;

  MachineOptimizationAssembler(Graph* graph, Zone* phase_zone)
      : AssemblerInterface<MachineOptimizationAssembler, Base>(graph,
                                                               phase_zone) {}

  OpIndex Change(OpIndex input, ChangeOp::Kind kind,
                 ChangeOp::Assumption assumption, RegisterRepresentation from,
                 RegisterRepresentation to) {
    if (ShouldSkipOptimizationStep()) {
      return Base::Change(input, kind, assumption, from, to);
    }
    using Kind = ChangeOp::Kind;
    if (from == WordRepresentation::Word32()) {
      input = TryRemoveWord32ToWord64Conversion(input);
    }
    if (uint64_t value;
        from.IsWord() &&
        this->MatchWordConstant(input, WordRepresentation(from), &value)) {
      if (kind == Kind::kSignExtend && from == WordRepresentation::Word32() &&
          to == WordRepresentation::Word64()) {
        return this->Word64Constant(int64_t{static_cast<int32_t>(value)});
      }
      if (kind == any_of(Kind::kZeroExtend, Kind::kBitcast) &&
          from == WordRepresentation::Word32() &&
          to == WordRepresentation::Word64()) {
        return this->Word64Constant(uint64_t{static_cast<uint32_t>(value)});
      }
      if (kind == Kind::kBitcast && from == WordRepresentation::Word32() &&
          to == RegisterRepresentation::Float32()) {
        return this->Float32Constant(
            base::bit_cast<float>(static_cast<uint32_t>(value)));
      }
      if (kind == Kind::kBitcast && from == WordRepresentation::Word64() &&
          to == RegisterRepresentation::Float64()) {
        return this->Float64Constant(base::bit_cast<double>(value));
      }
      if (kind == Kind::kSignedToFloat &&
          from == WordRepresentation::Word32() &&
          to == RegisterRepresentation::Float64()) {
        return this->Float64Constant(
            static_cast<double>(static_cast<int32_t>(value)));
      }
      if (kind == Kind::kSignedToFloat &&
          from == WordRepresentation::Word64() &&
          to == RegisterRepresentation::Float64()) {
        return this->Float64Constant(
            static_cast<double>(static_cast<int64_t>(value)));
      }
      if (kind == Kind::kUnsignedToFloat &&
          from == WordRepresentation::Word32() &&
          to == RegisterRepresentation::Float64()) {
        return this->Float64Constant(
            static_cast<double>(static_cast<uint32_t>(value)));
      }
    }
    if (float value; from == RegisterRepresentation::Float32() &&
                     this->MatchFloat32Constant(input, &value)) {
      if (kind == Kind::kFloatConversion &&
          to == RegisterRepresentation::Float64()) {
        return this->Float64Constant(value);
      }
      if (kind == Kind::kBitcast && to == WordRepresentation::Word32()) {
        return this->Word32Constant(base::bit_cast<uint32_t>(value));
      }
    }
    if (double value; from == RegisterRepresentation::Float64() &&
                      this->MatchFloat64Constant(input, &value)) {
      if (kind == Kind::kFloatConversion &&
          to == RegisterRepresentation::Float32()) {
        return this->Float32Constant(DoubleToFloat32_NoInline(value));
      }
      if (kind == Kind::kBitcast && to == WordRepresentation::Word64()) {
        return this->Word64Constant(base::bit_cast<uint64_t>(value));
      }
      if (kind == Kind::kSignedFloatTruncateOverflowToMin) {
        double truncated = std::trunc(value);
        if (to == WordRepresentation::Word64()) {
          int64_t result = std::numeric_limits<int64_t>::min();
          if (truncated >= std::numeric_limits<int64_t>::min() &&
              truncated <= kMaxDoubleRepresentableInt64) {
            result = static_cast<int64_t>(truncated);
          }
          return this->Word64Constant(result);
        }
        if (to == WordRepresentation::Word32()) {
          int32_t result = std::numeric_limits<int32_t>::min();
          if (truncated >= std::numeric_limits<int32_t>::min() &&
              truncated <= std::numeric_limits<int32_t>::max()) {
            result = static_cast<int32_t>(truncated);
          }
          return this->Word32Constant(result);
        }
      }
      if (kind == Kind::kJSFloatTruncate &&
          to == WordRepresentation::Word32()) {
        return this->Word32Constant(DoubleToInt32_NoInline(value));
      }
    }
    if (float value; from == RegisterRepresentation::Float32() &&
                     this->MatchFloat32Constant(input, &value)) {
      if (kind == Kind::kFloatConversion &&
          to == RegisterRepresentation::Float64()) {
        return this->Float64Constant(value);
      }
    }

    const Operation& input_op = Get(input);
    if (const ChangeOp* change_op = input_op.TryCast<ChangeOp>()) {
      if (change_op->from == to && change_op->to == from &&
          change_op->IsReversibleBy(kind, signalling_nan_possible)) {
        return change_op->input();
      }
    }
    return Base::Change(input, kind, assumption, from, to);
  }

  OpIndex Float64InsertWord32(OpIndex float64, OpIndex word32,
                              Float64InsertWord32Op::Kind kind) {
    if (ShouldSkipOptimizationStep()) {
      return Base::Float64InsertWord32(float64, word32, kind);
    }
    double f;
    uint32_t w;
    if (this->MatchFloat64Constant(float64, &f) &&
        this->MatchWord32Constant(word32, &w)) {
      uint64_t float_as_word = base::bit_cast<uint64_t>(f);
      switch (kind) {
        case Float64InsertWord32Op::Kind::kLowHalf:
          return this->Float64Constant(base::bit_cast<double>(
              (float_as_word & uint64_t{0xFFFFFFFF00000000}) | w));
        case Float64InsertWord32Op::Kind::kHighHalf:
          return this->Float64Constant(base::bit_cast<double>(
              (float_as_word & uint64_t{0xFFFFFFFF}) | (uint64_t{w} << 32)));
      }
    }
    return Base::Float64InsertWord32(float64, word32, kind);
  }

  OpIndex TaggedBitcast(OpIndex input, RegisterRepresentation from,
                        RegisterRepresentation to) {
    if (ShouldSkipOptimizationStep()) {
      return Base::TaggedBitcast(input, from, to);
    }
    // A Tagged -> Untagged -> Tagged sequence can be short-cut.
    // An Untagged -> Tagged -> Untagged sequence however cannot be removed,
    // because the GC might have modified the pointer.
    if (auto* input_bitcast = TryCast<TaggedBitcastOp>(input)) {
      if (all_of(input_bitcast->to, from) ==
              RegisterRepresentation::PointerSized() &&
          all_of(input_bitcast->from, to) == RegisterRepresentation::Tagged()) {
        return input_bitcast->input();
      }
    }
    return Base::TaggedBitcast(input, from, to);
  }

  OpIndex FloatUnary(OpIndex input, FloatUnaryOp::Kind kind,
                     FloatRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::FloatUnary(input, kind, rep);
    }
    if (float k; rep == FloatRepresentation::Float32() &&
                 this->MatchFloat32Constant(input, &k)) {
      if (std::isnan(k)) {
        return this->Float32Constant(std::numeric_limits<float>::quiet_NaN());
      }
      switch (kind) {
        case FloatUnaryOp::Kind::kAbs:
          return this->Float32Constant(std::abs(k));
        case FloatUnaryOp::Kind::kNegate:
          return this->Float32Constant(-k);
        case FloatUnaryOp::Kind::kSilenceNaN:
          DCHECK(!std::isnan(k));
          return this->Float32Constant(k);
        case FloatUnaryOp::Kind::kRoundDown:
          return this->Float32Constant(std::floor(k));
        case FloatUnaryOp::Kind::kRoundUp:
          return this->Float32Constant(std::ceil(k));
        case FloatUnaryOp::Kind::kRoundToZero:
          return this->Float32Constant(std::trunc(k));
        case FloatUnaryOp::Kind::kRoundTiesEven:
          DCHECK_EQ(std::nearbyint(1.5), 2);
          DCHECK_EQ(std::nearbyint(2.5), 2);
          return this->Float32Constant(std::nearbyint(k));
        case FloatUnaryOp::Kind::kLog:
          return this->Float32Constant(base::ieee754::log(k));
        case FloatUnaryOp::Kind::kSqrt:
          return this->Float32Constant(std::sqrt(k));
        case FloatUnaryOp::Kind::kExp:
          return this->Float32Constant(base::ieee754::exp(k));
        case FloatUnaryOp::Kind::kExpm1:
          return this->Float32Constant(base::ieee754::expm1(k));
        case FloatUnaryOp::Kind::kSin:
          return this->Float32Constant(base::ieee754::sin(k));
        case FloatUnaryOp::Kind::kCos:
          return this->Float32Constant(base::ieee754::cos(k));
        case FloatUnaryOp::Kind::kSinh:
          return this->Float32Constant(base::ieee754::sinh(k));
        case FloatUnaryOp::Kind::kCosh:
          return this->Float32Constant(base::ieee754::cosh(k));
        case FloatUnaryOp::Kind::kAcos:
          return this->Float32Constant(base::ieee754::acos(k));
        case FloatUnaryOp::Kind::kAsin:
          return this->Float32Constant(base::ieee754::asin(k));
        case FloatUnaryOp::Kind::kAsinh:
          return this->Float32Constant(base::ieee754::asinh(k));
        case FloatUnaryOp::Kind::kAcosh:
          return this->Float32Constant(base::ieee754::acosh(k));
        case FloatUnaryOp::Kind::kTan:
          return this->Float32Constant(base::ieee754::tan(k));
        case FloatUnaryOp::Kind::kTanh:
          return this->Float32Constant(base::ieee754::tanh(k));
        case FloatUnaryOp::Kind::kLog2:
          return this->Float32Constant(base::ieee754::log2(k));
        case FloatUnaryOp::Kind::kLog10:
          return this->Float32Constant(base::ieee754::log10(k));
        case FloatUnaryOp::Kind::kLog1p:
          return this->Float32Constant(base::ieee754::log1p(k));
        case FloatUnaryOp::Kind::kCbrt:
          return this->Float32Constant(base::ieee754::cbrt(k));
        case FloatUnaryOp::Kind::kAtan:
          return this->Float32Constant(base::ieee754::atan(k));
        case FloatUnaryOp::Kind::kAtanh:
          return this->Float32Constant(base::ieee754::atanh(k));
      }
    } else if (double k; rep == FloatRepresentation::Float64() &&
                         this->MatchFloat64Constant(input, &k)) {
      if (std::isnan(k)) {
        return this->Float64Constant(std::numeric_limits<double>::quiet_NaN());
      }
      switch (kind) {
        case FloatUnaryOp::Kind::kAbs:
          return this->Float64Constant(std::abs(k));
        case FloatUnaryOp::Kind::kNegate:
          return this->Float64Constant(-k);
        case FloatUnaryOp::Kind::kSilenceNaN:
          DCHECK(!std::isnan(k));
          return this->Float64Constant(k);
        case FloatUnaryOp::Kind::kRoundDown:
          return this->Float64Constant(std::floor(k));
        case FloatUnaryOp::Kind::kRoundUp:
          return this->Float64Constant(std::ceil(k));
        case FloatUnaryOp::Kind::kRoundToZero:
          return this->Float64Constant(std::trunc(k));
        case FloatUnaryOp::Kind::kRoundTiesEven:
          DCHECK_EQ(std::nearbyint(1.5), 2);
          DCHECK_EQ(std::nearbyint(2.5), 2);
          return this->Float64Constant(std::nearbyint(k));
        case FloatUnaryOp::Kind::kLog:
          return this->Float64Constant(base::ieee754::log(k));
        case FloatUnaryOp::Kind::kSqrt:
          return this->Float64Constant(std::sqrt(k));
        case FloatUnaryOp::Kind::kExp:
          return this->Float64Constant(base::ieee754::exp(k));
        case FloatUnaryOp::Kind::kExpm1:
          return this->Float64Constant(base::ieee754::expm1(k));
        case FloatUnaryOp::Kind::kSin:
          return this->Float64Constant(base::ieee754::sin(k));
        case FloatUnaryOp::Kind::kCos:
          return this->Float64Constant(base::ieee754::cos(k));
        case FloatUnaryOp::Kind::kSinh:
          return this->Float64Constant(base::ieee754::sinh(k));
        case FloatUnaryOp::Kind::kCosh:
          return this->Float64Constant(base::ieee754::cosh(k));
        case FloatUnaryOp::Kind::kAcos:
          return this->Float64Constant(base::ieee754::acos(k));
        case FloatUnaryOp::Kind::kAsin:
          return this->Float64Constant(base::ieee754::asin(k));
        case FloatUnaryOp::Kind::kAsinh:
          return this->Float64Constant(base::ieee754::asinh(k));
        case FloatUnaryOp::Kind::kAcosh:
          return this->Float64Constant(base::ieee754::acosh(k));
        case FloatUnaryOp::Kind::kTan:
          return this->Float64Constant(base::ieee754::tan(k));
        case FloatUnaryOp::Kind::kTanh:
          return this->Float64Constant(base::ieee754::tanh(k));
        case FloatUnaryOp::Kind::kLog2:
          return this->Float64Constant(base::ieee754::log2(k));
        case FloatUnaryOp::Kind::kLog10:
          return this->Float64Constant(base::ieee754::log10(k));
        case FloatUnaryOp::Kind::kLog1p:
          return this->Float64Constant(base::ieee754::log1p(k));
        case FloatUnaryOp::Kind::kCbrt:
          return this->Float64Constant(base::ieee754::cbrt(k));
        case FloatUnaryOp::Kind::kAtan:
          return this->Float64Constant(base::ieee754::atan(k));
        case FloatUnaryOp::Kind::kAtanh:
          return this->Float64Constant(base::ieee754::atanh(k));
      }
    }
    return Base::FloatUnary(input, kind, rep);
  }

  OpIndex WordUnary(OpIndex input, WordUnaryOp::Kind kind,
                    WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::WordUnary(input, kind, rep);
    }
    if (rep == WordRepresentation::Word32()) {
      input = TryRemoveWord32ToWord64Conversion(input);
    }
    if (uint32_t k; rep == WordRepresentation::Word32() &&
                    this->MatchWord32Constant(input, &k)) {
      switch (kind) {
        case WordUnaryOp::Kind::kReverseBytes:
          return this->Word32Constant(base::bits::ReverseBytes(k));
        case WordUnaryOp::Kind::kCountLeadingZeros:
          return this->Word32Constant(base::bits::CountLeadingZeros(k));
        case WordUnaryOp::Kind::kCountTrailingZeros:
          return this->Word32Constant(base::bits::CountTrailingZeros(k));
        case WordUnaryOp::Kind::kPopCount:
          return this->Word32Constant(base::bits::CountPopulation(k));
        case WordUnaryOp::Kind::kSignExtend8:
          return this->Word32Constant(int32_t{static_cast<int8_t>(k)});
        case WordUnaryOp::Kind::kSignExtend16:
          return this->Word32Constant(int32_t{static_cast<int16_t>(k)});
      }
    } else if (uint64_t k; rep == WordRepresentation::Word64() &&
                           this->MatchWord64Constant(input, &k)) {
      switch (kind) {
        case WordUnaryOp::Kind::kReverseBytes:
          return this->Word64Constant(base::bits::ReverseBytes(k));
        case WordUnaryOp::Kind::kCountLeadingZeros:
          return this->Word64Constant(
              uint64_t{base::bits::CountLeadingZeros(k)});
        case WordUnaryOp::Kind::kCountTrailingZeros:
          return this->Word64Constant(
              uint64_t{base::bits::CountTrailingZeros(k)});
        case WordUnaryOp::Kind::kPopCount:
          return this->Word64Constant(uint64_t{base::bits::CountPopulation(k)});
        case WordUnaryOp::Kind::kSignExtend8:
          return this->Word64Constant(int64_t{static_cast<int8_t>(k)});
        case WordUnaryOp::Kind::kSignExtend16:
          return this->Word64Constant(int64_t{static_cast<int16_t>(k)});
      }
    }
    return Base::WordUnary(input, kind, rep);
  }

  OpIndex FloatBinop(OpIndex lhs, OpIndex rhs, FloatBinopOp::Kind kind,
                     FloatRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::FloatBinop(lhs, rhs, kind, rep);
    }

    using Kind = FloatBinopOp::Kind;

    // Place constant on the right for commutative operators.
    if (FloatBinopOp::IsCommutative(kind) && Is<ConstantOp>(lhs) &&
        !Is<ConstantOp>(rhs)) {
      return FloatBinop(rhs, lhs, kind, rep);
    }

    // constant folding
    if (float k1, k2; rep == FloatRepresentation::Float32() &&
                      this->MatchFloat32Constant(lhs, &k1) &&
                      this->MatchFloat32Constant(rhs, &k2)) {
      switch (kind) {
        case Kind::kAdd:
          return this->Float32Constant(k1 + k2);
        case Kind::kMul:
          return this->Float32Constant(k1 * k2);
        case Kind::kSub:
          return this->Float32Constant(k1 - k2);
        case Kind::kMin:
          return this->Float32Constant(JSMin(k1, k2));
        case Kind::kMax:
          return this->Float32Constant(JSMax(k1, k2));
        case Kind::kDiv:
          return this->Float32Constant(k1 / k2);
        case Kind::kPower:
          return this->Float32Constant(base::ieee754::pow(k1, k2));
        case Kind::kAtan2:
          return this->Float32Constant(base::ieee754::atan2(k1, k2));
        case Kind::kMod:
          UNREACHABLE();
      }
    }
    if (double k1, k2; rep == FloatRepresentation::Float64() &&
                       this->MatchFloat64Constant(lhs, &k1) &&
                       this->MatchFloat64Constant(rhs, &k2)) {
      switch (kind) {
        case Kind::kAdd:
          return this->Float64Constant(k1 + k2);
        case Kind::kMul:
          return this->Float64Constant(k1 * k2);
        case Kind::kSub:
          return this->Float64Constant(k1 - k2);
        case Kind::kMin:
          return this->Float64Constant(JSMin(k1, k2));
        case Kind::kMax:
          return this->Float64Constant(JSMax(k1, k2));
        case Kind::kDiv:
          return this->Float64Constant(k1 / k2);
        case Kind::kMod:
          return this->Float64Constant(Modulo(k1, k2));
        case Kind::kPower:
          return this->Float64Constant(base::ieee754::pow(k1, k2));
        case Kind::kAtan2:
          return this->Float64Constant(base::ieee754::atan2(k1, k2));
      }
    }

    // lhs <op> NaN  =>  NaN
    if (this->MatchNaN(rhs) || (this->MatchNaN(lhs) && kind != Kind::kPower)) {
      // Return a quiet NaN since Wasm operations could have signalling NaN as
      // input but not as output.
      return this->FloatConstant(std::numeric_limits<double>::quiet_NaN(), rep);
    }

    if (Is<ConstantOp>(rhs)) {
      if (kind == Kind::kMul) {
        // lhs * 1  =>  lhs
        if (!signalling_nan_possible && this->MatchFloat(rhs, 1.0)) {
          return lhs;
        }
        // lhs * 2  =>  lhs + lhs
        if (this->MatchFloat(rhs, 2.0)) {
          return this->FloatAdd(lhs, lhs, rep);
        }
        // lhs * -1  =>  -lhs
        if (this->MatchFloat(rhs, -1.0)) {
          return this->FloatNegate(lhs, rep);
        }
      }

      if (kind == Kind::kDiv) {
        // lhs / 1  =>  lhs
        if (!signalling_nan_possible && this->MatchFloat(rhs, 1.0)) {
          return lhs;
        }
        // lhs / -1  =>  -lhs
        if (this->MatchFloat(rhs, -1.0)) {
          return this->FloatNegate(lhs, rep);
        }
        // All reciprocals of non-denormal powers of two can be represented
        // exactly, so division by power of two can be reduced to
        // multiplication by reciprocal, with the same result.
        // x / k  =>  x * (1 / k)
        if (rep == FloatRepresentation::Float32()) {
          if (float k;
              this->MatchFloat32Constant(rhs, &k) && std::isnormal(k) &&
              k != 0 && std::isfinite(k) &&
              base::bits::IsPowerOfTwo(base::Double(k).Significand())) {
            return this->FloatMul(lhs, this->FloatConstant(1.0 / k, rep), rep);
          }
        } else {
          DCHECK_EQ(rep, FloatRepresentation::Float64());
          if (double k;
              this->MatchFloat64Constant(rhs, &k) && std::isnormal(k) &&
              k != 0 && std::isfinite(k) &&
              base::bits::IsPowerOfTwo(base::Double(k).Significand())) {
            return this->FloatMul(lhs, this->FloatConstant(1.0 / k, rep), rep);
          }
        }
      }

      if (kind == Kind::kMod) {
        // x % 0  =>  NaN
        if (this->MatchFloat(rhs, 0.0)) {
          return this->FloatConstant(std::numeric_limits<double>::quiet_NaN(),
                                     rep);
        }
      }

      if (kind == Kind::kSub) {
        // lhs - +0.0  =>  lhs
        if (!signalling_nan_possible && this->MatchFloat(rhs, +0.0)) {
          return lhs;
        }
      }

      if (kind == Kind::kPower) {
        if (this->MatchFloat(rhs, 0.0) || this->MatchFloat(rhs, -0.0)) {
          return this->FloatConstant(1.0, rep);
        }
        if (this->MatchFloat(rhs, 2.0)) {
          return this->FloatMul(lhs, lhs, rep);
        }
        if (this->MatchFloat(rhs, 0.5)) {
          Block* if_neg_infinity = this->NewBlock(Block::Kind::kBranchTarget);
          if_neg_infinity->SetDeferred(true);
          Block* otherwise = this->NewBlock(Block::Kind::kBranchTarget);
          Block* merge = this->NewBlock(Block::Kind::kMerge);
          this->Branch(this->FloatLessThanOrEqual(
                           lhs, this->FloatConstant(-V8_INFINITY, rep), rep),
                       if_neg_infinity, otherwise);

          this->Bind(if_neg_infinity);
          OpIndex infty = this->FloatConstant(V8_INFINITY, rep);
          this->Goto(merge);

          this->Bind(otherwise);
          OpIndex sqrt = this->FloatSqrt(lhs, rep);
          this->Goto(merge);

          this->Bind(merge);
          return this->Phi(base::VectorOf({infty, sqrt}), rep);
        }
      }
    }

    if (!signalling_nan_possible && kind == Kind::kSub &&
        this->MatchFloat(lhs, -0.0)) {
      // -0.0 - round_down(-0.0 - y) => round_up(y)
      if (OpIndex a, b, c;
          FloatUnaryOp::IsSupported(FloatUnaryOp::Kind::kRoundUp, rep) &&
          this->MatchFloatRoundDown(rhs, &a, rep) &&
          this->MatchFloatSub(a, &b, &c, rep) && this->MatchFloat(b, -0.0)) {
        return this->FloatRoundUp(c, rep);
      }
      // -0.0 - rhs  =>  -rhs
      return this->FloatNegate(rhs, rep);
    }

    return Base::FloatBinop(lhs, rhs, kind, rep);
  }

  OpIndex WordBinop(OpIndex left, OpIndex right, WordBinopOp::Kind kind,
                    WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::WordBinop(left, right, kind, rep);
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
    if (WordBinopOp::IsCommutative(kind) && Is<ConstantOp>(left) &&
        !Is<ConstantOp>(right)) {
      return WordBinop(right, left, kind, rep);
    }
    // constant folding
    if (uint64_t k1, k2; this->MatchWordConstant(left, rep, &k1) &&
                         this->MatchWordConstant(right, rep, &k2)) {
      switch (kind) {
        case Kind::kAdd:
          return this->WordConstant(k1 + k2, rep);
        case Kind::kMul:
          return this->WordConstant(k1 * k2, rep);
        case Kind::kBitwiseAnd:
          return this->WordConstant(k1 & k2, rep);
        case Kind::kBitwiseOr:
          return this->WordConstant(k1 | k2, rep);
        case Kind::kBitwiseXor:
          return this->WordConstant(k1 ^ k2, rep);
        case Kind::kSub:
          return this->WordConstant(k1 - k2, rep);
        case Kind::kSignedMulOverflownBits:
          return this->WordConstant(
              is_64 ? base::bits::SignedMulHigh64(static_cast<int64_t>(k1),
                                                  static_cast<int64_t>(k2))
                    : base::bits::SignedMulHigh32(static_cast<int32_t>(k1),
                                                  static_cast<int32_t>(k2)),
              rep);
        case Kind::kUnsignedMulOverflownBits:
          return this->WordConstant(
              is_64 ? base::bits::UnsignedMulHigh64(k1, k2)
                    : base::bits::UnsignedMulHigh32(static_cast<uint32_t>(k1),
                                                    static_cast<uint32_t>(k2)),
              rep);
        case Kind::kSignedDiv:
          return this->WordConstant(
              is_64 ? base::bits::SignedDiv64(k1, k2)
                    : base::bits::SignedDiv32(static_cast<int32_t>(k1),
                                              static_cast<int32_t>(k2)),
              rep);
        case Kind::kUnsignedDiv:
          return this->WordConstant(
              is_64 ? base::bits::UnsignedDiv64(k1, k2)
                    : base::bits::UnsignedDiv32(static_cast<uint32_t>(k1),
                                                static_cast<uint32_t>(k2)),
              rep);
        case Kind::kSignedMod:
          return this->WordConstant(
              is_64 ? base::bits::SignedMod64(k1, k2)
                    : base::bits::SignedMod32(static_cast<int32_t>(k1),
                                              static_cast<int32_t>(k2)),
              rep);
        case Kind::kUnsignedMod:
          return this->WordConstant(
              is_64 ? base::bits::UnsignedMod64(k1, k2)
                    : base::bits::UnsignedMod32(static_cast<uint32_t>(k1),
                                                static_cast<uint32_t>(k2)),
              rep);
      }
    }

    // TODO(tebbi): Detect and merge multiple bitfield checks for CSA/Torque
    // code.

    if (uint64_t right_value;
        this->MatchWordConstant(right, rep, &right_value)) {
      int64_t right_value_signed =
          is_64 ? static_cast<int64_t>(right_value)
                : int64_t{static_cast<int32_t>(right_value)};
      // (a <op> k1) <op> k2  =>  a <op> (k1 <op> k2)
      if (OpIndex a, k1; WordBinopOp::IsAssociative(kind) &&
                         this->MatchWordBinop(left, &a, &k1, kind, rep) &&
                         Is<ConstantOp>(k1)) {
        OpIndex k2 = right;
        return WordBinop(a, WordBinop(k1, k2, kind, rep), kind, rep);
      }
      switch (kind) {
        case Kind::kSub:
          // left - k  => left + -k
          return WordBinop(left, this->WordConstant(-right_value, rep),
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
            return this->Word32Equal(left, this->Word32Constant(0));
          }
          // (x ^ -1) ^ -1  =>  x
          {
            OpIndex x, y;
            int64_t k;
            if (right_value_signed == -1 &&
                this->MatchBitwiseAnd(left, &x, &y, rep) &&
                this->MatchWordConstant(y, rep, &k) && k == -1) {
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
            if (this->MatchBitwiseAnd(left, &x, &y, rep) &&
                this->MatchWordConstant(y, rep, &k1) &&
                (k1 | k2) == rep.MaxUnsignedValue()) {
              return this->WordBitwiseOr(x, right, rep);
            }
          }
          break;
        case Kind::kMul:
          // left * 0  =>  0
          if (right_value == 0) {
            return this->WordConstant(0, rep);
          }
          // left * 1  =>  left
          if (right_value == 1) {
            return left;
          }
          // left * -1 => 0 - left
          if (right_value_signed == -1) {
            return this->WordSub(this->WordConstant(0, rep), left, rep);
          }
          // left * 2^k  =>  left << k
          if (base::bits::IsPowerOfTwo(right_value)) {
            OpIndex shift_amount = this->WordConstant(
                base::bits::WhichPowerOfTwo(right_value), rep);
            return this->ShiftLeft(left, shift_amount, rep);
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
                this->MatchWordAdd(left_ignore_extensions, &a, &b,
                                   WordRepresentation::Word32()) &&
                a == b) {
              return this->WordConstant(0, rep);
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
            return this->WordConstant(0, rep);
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
            OpIndex m = this->ShiftRightLogical(
                this->ShiftRightArithmetic(left, bits - 1, rep), bits - n, rep);
            return this->WordSub(
                this->WordBitwiseAnd(
                    this->WordAdd(left, m, rep),
                    this->WordConstant(right_value_signed - 1, rep), rep),
                m, rep);
          }
          // The `IntDiv` with a constant right-hand side will be turned into a
          // multiplication, avoiding the expensive integer division.
          return this->WordSub(
              left, this->WordMul(this->IntDiv(left, right, rep), right, rep),
              rep);
        case WordBinopOp::Kind::kUnsignedMod:
          // left % 0  =>  0
          // left % 1  =>  0
          if (right_value == 0 || right_value == 1) {
            return this->WordConstant(0, rep);
          }
          // x % 2^n => x & (2^n - 1)
          if (base::bits::IsPowerOfTwo(right_value)) {
            return this->WordBitwiseAnd(
                left, this->WordConstant(right_value - 1, rep), rep);
          }
          // The `UintDiv` with a constant right-hand side will be turned into a
          // multiplication, avoiding the expensive integer division.
          return this->WordSub(
              left, this->WordMul(right, this->UintDiv(left, right, rep), rep),
              rep);
        case WordBinopOp::Kind::kSignedMulOverflownBits:
        case WordBinopOp::Kind::kUnsignedMulOverflownBits:
          break;
      }
    }

    if (kind == Kind::kAdd) {
      OpIndex x, y, zero;
      // (0 - x) + y => y - x
      if (this->MatchWordSub(left, &zero, &x, rep) && this->MatchZero(zero)) {
        y = right;
        return this->WordSub(y, x, rep);
      }
      // x + (0 - y) => x - y
      if (this->MatchWordSub(right, &zero, &y, rep) && this->MatchZero(zero)) {
        x = left;
        return this->WordSub(x, y, rep);
      }
    }

    // 0 / right  =>  0
    // 0 % right  =>  0
    if (this->MatchZero(left) &&
        kind == any_of(Kind::kSignedDiv, Kind::kUnsignedDiv, Kind::kUnsignedMod,
                       Kind::kSignedMod)) {
      return this->WordConstant(0, rep);
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
          return this->WordConstant(0, rep);
        // x / x  =>  x != 0
        case WordBinopOp::Kind::kSignedDiv:
        case WordBinopOp::Kind::kUnsignedDiv: {
          OpIndex zero = this->WordConstant(0, rep);
          return this->Equal(this->Equal(left, zero, rep), zero, rep);
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

    return Base::WordBinop(left, right, kind, rep);
  }

  base::Optional<OpIndex> TryReduceToRor(OpIndex left, OpIndex right,
                                         WordBinopOp::Kind kind,
                                         WordRepresentation rep) {
    // Recognize rotation, we are this->matching and transforming as follows
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

    const ShiftOp* high = TryCast<ShiftOp>(left);
    if (!high) return {};
    const ShiftOp* low = TryCast<ShiftOp>(right);
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
    if (OpIndex a, b; this->MatchWordSub(high->right(), &a, &b, rep) &&
                      this->MatchWordConstant(a, rep, &k) &&
                      b == low->right() && k == rep.bit_width()) {
      amount = b;
    } else if (OpIndex a, b; this->MatchWordSub(low->right(), &a, &b, rep) &&
                             a == high->right() &&
                             this->MatchWordConstant(b, rep, &k) &&
                             k == rep.bit_width()) {
      amount = low->right();
    } else if (uint64_t k1, k2;
               this->MatchWordConstant(high->right(), rep, &k1) &&
               this->MatchWordConstant(low->right(), rep, &k2) &&
               k1 + k2 == rep.bit_width() && k1 >= 0 && k2 >= 0) {
      if (k1 == 0 || k2 == 0) {
        if (kind == WordBinopOp::Kind::kBitwiseXor) {
          return this->WordConstant(0, rep);
        } else {
          DCHECK_EQ(kind, WordBinopOp::Kind::kBitwiseOr);
          return x;
        }
      }
      return this->RotateRight(x, low->right(), rep);
    } else {
      return {};
    }
    if (kind == WordBinopOp::Kind::kBitwiseOr) {
      return this->RotateRight(x, amount, rep);
    } else {
      DCHECK_EQ(kind, WordBinopOp::Kind::kBitwiseXor);
      // Can't guarantee that rotation amount is not 0.
      return {};
    }
  }

  OpIndex Projection(OpIndex tuple, uint16_t index) {
    if (auto* tuple_op = TryCast<TupleOp>(tuple)) {
      return tuple_op->input(index);
    }
    return Base::Projection(tuple, index);
  }

  OpIndex OverflowCheckedBinop(OpIndex left, OpIndex right,
                               OverflowCheckedBinopOp::Kind kind,
                               WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::OverflowCheckedBinop(left, right, kind, rep);
    }
    using Kind = OverflowCheckedBinopOp::Kind;
    if (OverflowCheckedBinopOp::IsCommutative(kind) && Is<ConstantOp>(left) &&
        !Is<ConstantOp>(right)) {
      return OverflowCheckedBinop(right, left, kind, rep);
    }
    if (rep == WordRepresentation::Word32()) {
      left = TryRemoveWord32ToWord64Conversion(left);
      right = TryRemoveWord32ToWord64Conversion(right);
    }
    // constant folding
    if (rep == WordRepresentation::Word32()) {
      if (int32_t k1, k2; this->MatchWord32Constant(left, &k1) &&
                          this->MatchWord32Constant(right, &k2)) {
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
        return this->Tuple(this->Word32Constant(res),
                           this->Word32Constant(overflow));
      }
    } else {
      DCHECK_EQ(rep, WordRepresentation::Word64());
      if (int64_t k1, k2; this->MatchWord64Constant(left, &k1) &&
                          this->MatchWord64Constant(right, &k2)) {
        bool overflow;
        int64_t res;
        switch (kind) {
          case OverflowCheckedBinopOp::Kind::kSignedAdd:
            overflow = base::bits::SignedAddOverflow64(k1, k2, &res);
            break;
          case OverflowCheckedBinopOp::Kind::kSignedMul:
            UNREACHABLE();
          case OverflowCheckedBinopOp::Kind::kSignedSub:
            overflow = base::bits::SignedSubOverflow64(k1, k2, &res);
            break;
        }
        return this->Tuple(this->Word64Constant(res),
                           this->Word32Constant(overflow));
      }
    }

    // left + 0  =>  (left, false)
    // left - 0  =>  (left, false)
    if (kind == any_of(Kind::kSignedAdd, Kind::kSignedSub) &&
        this->MatchZero(right)) {
      return this->Tuple(left, right);
    }

    if (kind == Kind::kSignedMul) {
      if (int64_t k; this->MatchWordConstant(right, rep, &k)) {
        // left * 0  =>  (0, false)
        if (k == 0) {
          return this->Tuple(this->WordConstant(0, rep),
                             this->Word32Constant(false));
        }
        // left * 1  =>  (left, false)
        if (k == 1) {
          return this->Tuple(left, this->Word32Constant(false));
        }
        // left * -1  =>  0 - left
        if (k == -1) {
          return this->IntSubCheckOverflow(this->WordConstant(0, rep), left,
                                           rep);
        }
        // left * 2  =>  left + left
        if (k == 2) {
          return this->IntAddCheckOverflow(left, left, rep);
        }
      }
    }

    return Base::OverflowCheckedBinop(left, right, kind, rep);
  }

  OpIndex Equal(OpIndex left, OpIndex right, RegisterRepresentation rep) {
    if (ShouldSkipOptimizationStep()) return Base::Equal(left, right, rep);
    if (left == right && !rep.IsFloat()) {
      return this->Word32Constant(1);
    }
    if (rep == WordRepresentation::Word32()) {
      left = TryRemoveWord32ToWord64Conversion(left);
      right = TryRemoveWord32ToWord64Conversion(right);
    }
    if (Is<ConstantOp>(left) && !Is<ConstantOp>(right)) {
      return Equal(right, left, rep);
    }
    if (Is<ConstantOp>(right)) {
      if (Is<ConstantOp>(left)) {
        // k1 == k2  =>  k
        switch (rep) {
          case RegisterRepresentation::Word32():
          case RegisterRepresentation::Word64(): {
            if (uint64_t k1, k2;
                this->MatchWordConstant(left, WordRepresentation(rep), &k1) &&
                this->MatchWordConstant(right, WordRepresentation(rep), &k2)) {
              return this->Word32Constant(k1 == k2);
            }
            break;
          }
          case RegisterRepresentation::Float32(): {
            if (float k1, k2; this->MatchFloat32Constant(left, &k1) &&
                              this->MatchFloat32Constant(right, &k2)) {
              return this->Word32Constant(k1 == k2);
            }
            break;
          }
          case RegisterRepresentation::Float64(): {
            if (double k1, k2; this->MatchFloat64Constant(left, &k1) &&
                               this->MatchFloat64Constant(right, &k2)) {
              return this->Word32Constant(k1 == k2);
            }
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
            this->MatchWordSub(left, &x, &y, rep_w) && this->MatchZero(right)) {
          return Equal(x, y, rep);
        }
        {
          //     ((x >> shift_amount) & mask) == k
          // =>  (x & (mask << shift_amount)) == (k << shift_amount)
          OpIndex shift, x, mask_op;
          int shift_amount;
          uint64_t mask, k;
          if (this->MatchBitwiseAnd(left, &shift, &mask_op, rep_w) &&
              this->MatchConstantRightShift(shift, &x, rep_w, &shift_amount) &&
              this->MatchWordConstant(mask_op, rep_w, &mask) &&
              this->MatchWordConstant(right, rep_w, &k) &&
              mask <= rep.MaxUnsignedValue() >> shift_amount &&
              k <= rep.MaxUnsignedValue() >> shift_amount) {
            return Equal(
                this->WordBitwiseAnd(
                    x, this->Word64Constant(mask << shift_amount), rep_w),
                this->Word64Constant(k << shift_amount), rep_w);
          }
        }
        {
          // (x >> k1) == k2  =>  x == (k2 << k1)  if shifts reversible
          // Only perform the transformation if the shift is not used yet, to
          // avoid keeping both the shift and x alive.
          OpIndex x;
          uint16_t k1;
          int64_t k2;
          if (this->MatchConstantShiftRightArithmeticShiftOutZeros(
                  left, &x, rep_w, &k1) &&
              this->MatchWordConstant(right, rep_w, &k2) &&
              CountLeadingSignBits(k2, rep_w) > k1 &&
              Get(left).saturated_use_count == 0) {
            return this->Equal(
                x, this->WordConstant(base::bits::Unsigned(k2) << k1, rep_w),
                rep_w);
          }
        }
      }
    }
    return Base::Equal(left, right, rep);
  }

  OpIndex Comparison(OpIndex left, OpIndex right, ComparisonOp::Kind kind,
                     RegisterRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::Comparison(left, right, kind, rep);
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
          return this->Word32Constant(1);
        case Kind::kUnsignedLessThan:
        case Kind::kSignedLessThan:
          return this->Word32Constant(0);
      }
    }
    // constant folding
    if (Is<ConstantOp>(right) && Is<ConstantOp>(left)) {
      switch (rep) {
        case RegisterRepresentation::Word32():
        case RegisterRepresentation::Word64(): {
          if (kind ==
              any_of(Kind::kSignedLessThan, Kind::kSignedLessThanOrEqual)) {
            if (int64_t k1, k2;
                this->MatchWordConstant(left, WordRepresentation(rep), &k1) &&
                this->MatchWordConstant(right, WordRepresentation(rep), &k2)) {
              switch (kind) {
                case ComparisonOp::Kind::kSignedLessThan:
                  return this->Word32Constant(k1 < k2);
                case ComparisonOp::Kind::kSignedLessThanOrEqual:
                  return this->Word32Constant(k1 <= k2);
                case ComparisonOp::Kind::kUnsignedLessThan:
                case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                  UNREACHABLE();
              }
            }
          } else {
            if (uint64_t k1, k2;
                this->MatchWordConstant(left, WordRepresentation(rep), &k1) &&
                this->MatchWordConstant(right, WordRepresentation(rep), &k2)) {
              switch (kind) {
                case ComparisonOp::Kind::kUnsignedLessThan:
                  return this->Word32Constant(k1 < k2);
                case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                  return this->Word32Constant(k1 <= k2);
                case ComparisonOp::Kind::kSignedLessThan:
                case ComparisonOp::Kind::kSignedLessThanOrEqual:
                  UNREACHABLE();
              }
            }
          }
          break;
        }
        case RegisterRepresentation::Float32(): {
          if (float k1, k2; this->MatchFloat32Constant(left, &k1) &&
                            this->MatchFloat32Constant(right, &k2)) {
            switch (kind) {
              case ComparisonOp::Kind::kSignedLessThan:
                return this->Word32Constant(k1 < k2);
              case ComparisonOp::Kind::kSignedLessThanOrEqual:
                return this->Word32Constant(k1 <= k2);
              case ComparisonOp::Kind::kUnsignedLessThan:
              case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
                UNREACHABLE();
            }
          }
          break;
        }
        case RegisterRepresentation::Float64(): {
          if (double k1, k2; this->MatchFloat64Constant(left, &k1) &&
                             this->MatchFloat64Constant(right, &k2)) {
            switch (kind) {
              case ComparisonOp::Kind::kSignedLessThan:
                return this->Word32Constant(k1 < k2);
              case ComparisonOp::Kind::kSignedLessThanOrEqual:
                return this->Word32Constant(k1 <= k2);
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
      return this->Comparison(UndoFloat32ToFloat64Conversion(left),
                              UndoFloat32ToFloat64Conversion(right), kind,
                              RegisterRepresentation::Float32());
    }
    if (rep.IsWord()) {
      WordRepresentation rep_w{rep};
      if (kind == Kind::kUnsignedLessThanOrEqual) {
        // 0 <= x  =>  true
        if (uint64_t k; this->MatchWordConstant(left, rep_w, &k) && k == 0) {
          return this->Word32Constant(1);
        }
        // x <= MaxUint  =>  true
        if (uint64_t k; this->MatchWordConstant(right, rep_w, &k) &&
                        k == rep.MaxUnsignedValue()) {
          return this->Word32Constant(1);
        }
      }
      if (kind == Kind::kUnsignedLessThan) {
        // x < 0  =>  false
        if (uint64_t k; this->MatchWordConstant(right, rep_w, &k) && k == 0) {
          return this->Word32Constant(0);
        }
        // MaxUint < x  =>  true
        if (uint64_t k; this->MatchWordConstant(left, rep_w, &k) &&
                        k == rep.MaxUnsignedValue()) {
          return this->Word32Constant(0);
        }
      }
      {
        // (x >> k) </<=  (y >> k)  =>  x </<=  y   if shifts reversible
        OpIndex x, y;
        uint16_t k1, k2;
        if (this->MatchConstantShiftRightArithmeticShiftOutZeros(left, &x,
                                                                 rep_w, &k1) &&
            this->MatchConstantShiftRightArithmeticShiftOutZeros(right, &y,
                                                                 rep_w, &k2) &&
            k1 == k2) {
          return this->Comparison(x, y, kind, rep_w);
        }
      }
      {
        // (x >> k1) </<= k2  =>  x </<= (k2 << k1)  if shifts reversible
        // Only perform the transformation if the shift is not used yet, to
        // avoid keeping both the shift and x alive.
        OpIndex x;
        uint16_t k1;
        int64_t k2;
        if (this->MatchConstantShiftRightArithmeticShiftOutZeros(left, &x,
                                                                 rep_w, &k1) &&
            this->MatchWordConstant(right, rep_w, &k2) &&
            CountLeadingSignBits(k2, rep_w) > k1 &&
            Get(left).saturated_use_count == 0) {
          return this->Comparison(
              x, this->WordConstant(base::bits::Unsigned(k2) << k1, rep_w),
              kind, rep_w);
        }
        // k2 </<= (x >> k1)  =>  (k2 << k1) </<= x  if shifts reversible
        // Only perform the transformation if the shift is not used yet, to
        // avoid keeping both the shift and x alive.
        if (this->MatchConstantShiftRightArithmeticShiftOutZeros(right, &x,
                                                                 rep_w, &k1) &&
            this->MatchWordConstant(left, rep_w, &k2) &&
            CountLeadingSignBits(k2, rep_w) > k1 &&
            Get(right).saturated_use_count == 0) {
          return this->Comparison(
              this->WordConstant(base::bits::Unsigned(k2) << k1, rep_w), x,
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
            return this->Comparison(UndoWord32ToWord64Conversion(left),
                                    UndoWord32ToWord64Conversion(right),
                                    ComparisonOp::SetSigned(kind, false),
                                    WordRepresentation::Word32());
          } else if (left_sign_extended != false &&
                     right_sign_extended != false) {
            // Both sides were sign-extended, this preserves both signed and
            // unsigned comparisons.
            return this->Comparison(UndoWord32ToWord64Conversion(left),
                                    UndoWord32ToWord64Conversion(right), kind,
                                    WordRepresentation::Word32());
          }
        }
      }
    }
    return Base::Comparison(left, right, kind, rep);
  }

  OpIndex Shift(OpIndex left, OpIndex right, ShiftOp::Kind kind,
                WordRepresentation rep) {
    if (ShouldSkipOptimizationStep()) {
      return Base::Shift(left, right, kind, rep);
    }
    using Kind = ShiftOp::Kind;
    uint64_t c_unsigned;
    int64_t c_signed;
    if (this->MatchWordConstant(left, rep, &c_unsigned, &c_signed)) {
      if (uint32_t amount; this->MatchWord32Constant(right, &amount)) {
        amount = amount & (rep.bit_width() - 1);
        switch (kind) {
          case Kind::kShiftRightArithmeticShiftOutZeros:
            if (base::bits::CountTrailingZeros(c_signed) < amount) {
              // This assumes that we never hoist operations to before their
              // original place in the control flow.
              return this->Unreachable();
            }
            [[fallthrough]];
          case Kind::kShiftRightArithmetic:
            return this->WordConstant(c_signed >> amount, rep);
          case Kind::kShiftRightLogical:
            return this->WordConstant(c_unsigned >> amount, rep);
          case Kind::kShiftLeft:
            return this->WordConstant(c_unsigned << amount, rep);
          case Kind::kRotateRight:
            if (rep == WordRepresentation::Word32()) {
              return this->WordConstant(
                  base::bits::RotateRight32(static_cast<uint32_t>(c_unsigned),
                                            amount),
                  rep);
            } else {
              return this->WordConstant(
                  base::bits::RotateRight64(c_unsigned, amount), rep);
            }
          case Kind::kRotateLeft:
            if (rep == WordRepresentation::Word32()) {
              return this->WordConstant(
                  base::bits::RotateLeft32(static_cast<uint32_t>(c_unsigned),
                                           amount),
                  rep);
            } else {
              return this->WordConstant(
                  base::bits::RotateLeft64(c_unsigned, amount), rep);
            }
        }
      }
    }
    if (int32_t amount; this->MatchWord32Constant(right, &amount) &&
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
        if (this->MatchConstantShift(
                left, &x, Kind::kShiftRightArithmeticShiftOutZeros, rep, &k)) {
          int32_t l = amount;
          if (k == l) {
            return x;
          } else if (k > l) {
            return this->ShiftRightArithmeticShiftOutZeros(
                x, this->Word32Constant(k - l), rep);
          } else if (k < l) {
            return this->ShiftLeft(x, this->Word32Constant(l - k), rep);
          }
        }
        // (x >>> K) << K => x & ~(2^K - 1)
        // (x >> K) << K => x & ~(2^K - 1)
        if (this->MatchConstantRightShift(left, &x, rep, &k) && k == amount) {
          return this->WordBitwiseAnd(
              x, this->WordConstant(rep.MaxUnsignedValue() << k, rep), rep);
        }
      }
      if (kind == any_of(Kind::kShiftRightArithmetic,
                         Kind::kShiftRightArithmeticShiftOutZeros)) {
        OpIndex x;
        int left_shift_amount;
        // (x << k) >> k
        if (this->MatchConstantShift(left, &x, ShiftOp::Kind::kShiftLeft, rep,
                                     &left_shift_amount) &&
            amount == left_shift_amount) {
          // x << (bit_width - 1) >> (bit_width - 1)  =>  0 - x  if x is 0 or 1
          if (amount == rep.bit_width() - 1 && IsBit(x)) {
            return this->WordSub(this->WordConstant(0, rep), x, rep);
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
        // machine instruction this->matches that required by JavaScript.
        if (OpIndex a, b; this->MatchBitwiseAnd(right, &a, &b,
                                                WordRepresentation::Word32())) {
          static_assert(0x1f == WordRepresentation::Word32().bit_width() - 1);
          if (uint32_t b_value;
              this->MatchWord32Constant(b, &b_value) && b_value == 0x1f) {
            return this->Shift(left, a, kind, rep);
          }
        }
      }
    }
    return Base::Shift(left, right, kind, rep);
  }

  OpIndex Branch(OpIndex condition, Block* if_true, Block* if_false) {
    if (ShouldSkipOptimizationStep()) {
      return Base::Branch(condition, if_true, if_false);
    }
    if (base::Optional<bool> decision = DecideBranchCondition(condition)) {
      return this->Goto(*decision ? if_true : if_false);
    }
    bool negated = false;
    condition = ReduceBranchCondition(condition, &negated);
    if (negated) std::swap(if_true, if_false);
    return Base::Branch(condition, if_true, if_false);
  }

  OpIndex DeoptimizeIf(OpIndex condition, OpIndex frame_state, bool negated,
                       const DeoptimizeParameters* parameters) {
    if (ShouldSkipOptimizationStep()) {
      return Base::DeoptimizeIf(condition, frame_state, negated, parameters);
    }
    if (base::Optional<bool> decision = DecideBranchCondition(condition)) {
      if (*decision != negated) {
        this->Deoptimize(frame_state, parameters);
      }
      // `DeoptimizeIf` doesn't produce a value.
      return OpIndex::Invalid();
    }
    condition = ReduceBranchCondition(condition, &negated);
    return Base::DeoptimizeIf(condition, frame_state, negated, parameters);
  }

  OpIndex Store(OpIndex base, OpIndex value, StoreOp::Kind kind,
                MemoryRepresentation stored_rep, WriteBarrierKind write_barrier,
                int32_t offset) {
    if (ShouldSkipOptimizationStep()) {
      return Base::Store(base, value, kind, stored_rep, write_barrier, offset);
    }
    return IndexedStore(base, OpIndex::Invalid(), value, kind, stored_rep,
                        write_barrier, offset, 0);
  }

  OpIndex IndexedStore(OpIndex base, OpIndex index, OpIndex value,
                       IndexedStoreOp::Kind kind,
                       MemoryRepresentation stored_rep,
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
    if (index.valid()) {
      return Base::IndexedStore(base, index, value, kind, stored_rep,
                                write_barrier, offset, element_scale);
    } else {
      return Base::Store(base, value, kind, stored_rep, write_barrier, offset);
    }
  }

  OpIndex Load(OpIndex base, LoadOp::Kind kind, MemoryRepresentation loaded_rep,
               RegisterRepresentation result_rep, int32_t offset) {
    if (ShouldSkipOptimizationStep())
      return Base::Load(base, kind, loaded_rep, result_rep, offset);
    return IndexedLoad(base, OpIndex::Invalid(), kind, loaded_rep, result_rep,
                       offset, 0);
  }

  OpIndex IndexedLoad(OpIndex base, OpIndex index, IndexedLoadOp::Kind kind,
                      MemoryRepresentation loaded_rep,
                      RegisterRepresentation result_rep, int32_t offset,
                      uint8_t element_scale) {
    while (true) {
      if (ShouldSkipOptimizationStep()) break;
      index = ReduceMemoryIndex(index, &offset, &element_scale);
      if (kind != IndexedLoadOp::Kind::kTaggedBase && !index.valid()) {
        if (OpIndex left, right;
            this->MatchWordAdd(base, &left, &right,
                               WordRepresentation::PointerSized()) &&
            TryAdjustOffset(&offset, Get(right), element_scale)) {
          base = left;
          continue;
        }
      }
      break;
    }
    if (index.valid()) {
      return Base::IndexedLoad(base, index, kind, loaded_rep, result_rep,
                               offset, element_scale);
    } else {
      return Base::Load(base, kind, loaded_rep, result_rep, offset);
    }
  }

 private:
  bool TryAdjustOffset(int32_t* offset, const Operation& maybe_constant,
                       uint8_t element_scale) {
    if (!maybe_constant.Is<ConstantOp>()) return false;
    const ConstantOp& constant = maybe_constant.Cast<ConstantOp>();
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
    if (!this->MatchWordConstant(maybe_constant,
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
      const Operation& index_op = Get(index);
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
            TryAdjustOffset(offset, Get(binary_op->right()), *element_scale)) {
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
        this->MatchChange(value, &input, ChangeOp::Kind::kFloatConversion,
                          RegisterRepresentation::Float32(),
                          RegisterRepresentation::Float64())) {
      return true;
    }
    if (double c;
        this->MatchFloat64Constant(value, &c) && DoubleToFloat32(c) == c) {
      return true;
    }
    return false;
  }

  OpIndex UndoFloat32ToFloat64Conversion(OpIndex value) {
    if (OpIndex input;
        this->MatchChange(value, &input, ChangeOp::Kind::kFloatConversion,
                          RegisterRepresentation::Float32(),
                          RegisterRepresentation::Float64())) {
      return input;
    }
    if (double c;
        this->MatchFloat64Constant(value, &c) && DoubleToFloat32(c) == c) {
      return this->Float32Constant(DoubleToFloat32(c));
    }
    UNREACHABLE();
  }

  bool IsBit(OpIndex value) {
    return Is<EqualOp>(value) || Is<ComparisonOp>(value);
  }

  bool IsInt8(OpIndex value) {
    if (auto* op = TryCast<LoadOp>(value)) {
      return op->loaded_rep == MemoryRepresentation::Int8();
    } else if (auto* op = TryCast<IndexedLoadOp>(value)) {
      return op->loaded_rep == MemoryRepresentation::Int8();
    }
    return false;
  }

  bool IsInt16(OpIndex value) {
    if (auto* op = TryCast<LoadOp>(value)) {
      return op->loaded_rep == any_of(MemoryRepresentation::Int16(),
                                      MemoryRepresentation::Int8());
    } else if (auto* op = TryCast<IndexedLoadOp>(value)) {
      return op->loaded_rep == any_of(MemoryRepresentation::Int16(),
                                      MemoryRepresentation::Int8());
    }
    return false;
  }

  bool IsWord32ConvertedToWord64(
      OpIndex value, base::Optional<bool>* sign_extended = nullptr) {
    if (const ChangeOp* change_op = TryCast<ChangeOp>(value)) {
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
    if (int64_t c; this->MatchWord64Constant(value, &c) &&
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
    if (const ChangeOp* op = TryCast<ChangeOp>(value)) {
      return op->input();
    }
    return this->Word32Constant(this->Cast<ConstantOp>(value).word32());
  }

  OpIndex TryRemoveWord32ToWord64Conversion(OpIndex value) {
    if (const ChangeOp* op = TryCast<ChangeOp>(value)) {
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
      if (this->MatchBitwiseAnd(value, &input, &mask, rep) &&
          this->MatchWordConstant(mask, rep, &mask_value)) {
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
      if (this->MatchConstantShift(value, &left_shift, &right_shift_kind, &rep,
                                   &right_shift_amount) &&
          ShiftOp::IsRightShift(right_shift_kind) &&
          this->MatchConstantShift(left_shift, &left_shift_input,
                                   ShiftOp::Kind::kShiftLeft, rep,
                                   &left_shift_amount) &&
          ((rep.MaxUnsignedValue() >> right_shift_amount) & truncation_mask) ==
              truncation_mask) {
        if (left_shift_amount == right_shift_amount) {
          return left_shift_input;
        } else if (left_shift_amount < right_shift_amount) {
          OpIndex shift_amount =
              this->WordConstant(right_shift_amount - left_shift_amount, rep);
          return this->Shift(left_shift_input, shift_amount, right_shift_kind,
                             rep);
        } else if (left_shift_amount > right_shift_amount) {
          OpIndex shift_amount =
              this->WordConstant(left_shift_amount - right_shift_amount, rep);
          return this->Shift(left_shift_input, shift_amount,
                             ShiftOp::Kind::kShiftLeft, rep);
        }
      }
    }
    return value;
  }

  OpIndex ReduceSignedDiv(OpIndex left, int64_t right, WordRepresentation rep) {
    // left / -1 => 0 - left
    if (right == -1) {
      return this->WordSub(this->WordConstant(0, rep), left, rep);
    }
    // left / 0 => 0
    if (right == 0) {
      return this->WordConstant(0, rep);
    }
    // left / 1 => left
    if (right == 1) {
      return left;
    }
    // left / MinSignedValue  =>  left == MinSignedValue
    if (right == rep.MinSignedValue()) {
      return this->ChangeUint32ToUint64(
          this->Equal(left, this->WordConstant(right, rep), rep));
    }
    // left / -right  => -(left / right)
    if (right < 0) {
      DCHECK_NE(right, rep.MinSignedValue());
      return this->WordSub(this->WordConstant(0, rep),
                           ReduceSignedDiv(left, Abs(right), rep), rep);
    }

    OpIndex quotient = left;
    if (base::bits::IsPowerOfTwo(right)) {
      uint32_t shift = base::bits::WhichPowerOfTwo(right);
      DCHECK_GT(shift, 0);
      if (shift > 1) {
        quotient =
            this->ShiftRightArithmetic(quotient, rep.bit_width() - 1, rep);
      }
      quotient =
          this->ShiftRightArithmetic(quotient, rep.bit_width() - shift, rep);
      quotient = this->WordAdd(quotient, left, rep);
      quotient = this->ShiftRightArithmetic(quotient, shift, rep);
      return quotient;
    }
    DCHECK_GT(right, 0);
    // Compute the magic number for `right`, using a generic lambda to treat
    // 32- and 64-bit uniformly.
    auto LowerToMul = [this, left](auto right, WordRepresentation rep) {
      base::MagicNumbersForDivision<decltype(right)> magic =
          base::SignedDivisionByConstant(right);
      OpIndex quotient = this->IntMulOverflownBits(
          left, this->WordConstant(magic.multiplier, rep), rep);
      if (magic.multiplier < 0) {
        quotient = this->WordAdd(quotient, left, rep);
      }
      OpIndex sign_bit =
          this->ShiftRightLogical(left, rep.bit_width() - 1, rep);
      return this->WordAdd(
          this->ShiftRightArithmetic(quotient, magic.shift, rep), sign_bit,
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
      return this->WordConstant(0, rep);
    }
    // left / 1 => left
    if (right == 1) {
      return left;
    }
    // left / 2^k  => left >> k
    if (base::bits::IsPowerOfTwo(right)) {
      return this->ShiftRightLogical(left, base::bits::WhichPowerOfTwo(right),
                                     rep);
    }
    DCHECK_GT(right, 0);
    // If `right` is even, we can avoid using the expensive fixup by
    // shifting `left` upfront.
    unsigned const shift = base::bits::CountTrailingZeros(right);
    left = this->ShiftRightLogical(left, shift, rep);
    right >>= shift;
    // Compute the magic number for `right`, using a generic lambda to treat
    // 32- and 64-bit uniformly.
    auto LowerToMul = [this, left, shift](auto right, WordRepresentation rep) {
      base::MagicNumbersForDivision<decltype(right)> const mag =
          base::UnsignedDivisionByConstant(right, shift);
      OpIndex quotient = this->UintMulOverflownBits(
          left, this->WordConstant(mag.multiplier, rep), rep);
      if (mag.add) {
        DCHECK_GE(mag.shift, 1);
        // quotient = (((left - quotient) >> 1) + quotient) >> (mag.shift -
        // 1)
        quotient = this->ShiftRightLogical(
            this->WordAdd(this->ShiftRightLogical(
                              this->WordSub(left, quotient, rep), 1, rep),
                          quotient, rep),
            mag.shift - 1, rep);
      } else {
        quotient = this->ShiftRightLogical(quotient, mag.shift, rep);
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

  OpIndex ReduceBranchCondition(OpIndex condition, bool* negated) {
    while (true) {
      condition = TryRemoveWord32ToWord64Conversion(condition);
      // x == 0  =>  x with flipped branches
      if (OpIndex left, right; this->MatchEqual(condition, &left, &right,
                                                WordRepresentation::Word32()) &&
                               this->MatchZero(right)) {
        condition = left;
        *negated = !*negated;
        continue;
      }
      // x - y  =>  x == y with flipped branches
      if (OpIndex left, right; this->MatchWordSub(
              condition, &left, &right, WordRepresentation::Word32())) {
        condition = this->Word32Equal(left, right);
        *negated = !*negated;
        continue;
      }
      // x & (1 << k) == (1 << k)  =>  x & (1 << k)
      if (OpIndex left, right; this->MatchEqual(condition, &left, &right,
                                                WordRepresentation::Word32())) {
        OpIndex x, mask;
        uint32_t k1, k2;
        if (this->MatchBitwiseAnd(left, &x, &mask,
                                  WordRepresentation::Word32()) &&
            this->MatchWord32Constant(mask, &k1) &&
            this->MatchWord32Constant(right, &k2) && k1 == k2 &&
            base::bits::IsPowerOfTwo(k1)) {
          condition = left;
          continue;
        }
      }
      break;
    }
    return condition;
  }

  base::Optional<bool> DecideBranchCondition(OpIndex condition) {
    if (uint32_t value; this->MatchWord32Constant(condition, &value)) {
      return value != 0;
    }
    return base::nullopt;
  }

  uint16_t CountLeadingSignBits(int64_t c, WordRepresentation rep) {
    return base::bits::CountLeadingSignBits(c) - (64 - rep.bit_width());
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_MACHINE_OPTIMIZATION_ASSEMBLER_H_
