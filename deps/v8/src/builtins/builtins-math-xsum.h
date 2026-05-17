// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_MATH_XSUM_H_
#define V8_BUILTINS_BUILTINS_MATH_XSUM_H_

#include <array>

#include "include/v8-internal.h"
#include "src/base/macros.h"
#include "src/numbers/conversions.h"

namespace v8 {
namespace internal {

class Xsum {
 public:
  V8_EXPORT Xsum();

  // Default xsum algo.
  template <typename T>
  inline void Add(T value) {
    if (adds_until_propagate_ == 0) {
      CarryPropagate();
    }
    Add1NoCarry(value);
    adds_until_propagate_--;
  }
  V8_EXPORT double Round();

  // Variant used in Math.sumPrecise.
  inline void AddForSumPrecise(double value) {
    // Math.sumPrecise treats all NaN the same.
    if (nan_ != 0) return;
    // Empty lists or lists with only -0 result in -0.
    if (!IsMinusZero(value)) [[unlikely]] {
      minus_zero_ = false;
    }
    Add(value);
  }
  inline void AddForSumPrecise(int value) {
    minus_zero_ = false;
    Add(value);
  }

  enum class Result {
    kMinusZero,
    kFinite,
    kPlusInfinity,
    kMinusInfinity,
    kNaN
  };

  inline std::tuple<Result, double> GetSumPrecise() {
    if (minus_zero_) {
      return {Result::kMinusZero, 0};
    }
    if (nan_ != 0 || inf_sign_change_) {
      return {Result::kNaN, 0};
    }
    if (inf_ != 0) {
      return {base::bit_cast<double>(inf_) > 0 ? Result::kPlusInfinity
                                               : Result::kMinusInfinity,
              0};
    }
    return {Result::kFinite, Round()};
  }

 private:
  static constexpr int kMantissaBits = 52;
  static constexpr int kExpBits = 11;
  static constexpr int64_t kMantissaMask = (int64_t{1} << kMantissaBits) - 1;
  static constexpr int64_t kExpMask = (1 << kExpBits) - 1;
  static constexpr int64_t kExpBias = (1 << (kExpBits - 1)) - 1;
  static constexpr int kSignBit = kMantissaBits + kExpBits;
  static constexpr uint64_t kSignMask = uint64_t{1} << kSignBit;

  static constexpr int kSchunkBits = 64;
  static constexpr int kLowExpBits = 5;
  static constexpr int kLowExpMask = (1 << kLowExpBits) - 1;
  static constexpr int kHighExpBits = kExpBits - kLowExpBits;
  static constexpr int kSchunks = (1 << kHighExpBits) + 3;  // 67

  static constexpr int kLowMantissaBits = 1 << kLowExpBits;  // 32
  static constexpr int64_t kLowMantissaMask =
      (int64_t{1} << kLowMantissaBits) - 1;

  static constexpr int kSmallCarryBits =
      (kSchunkBits - 1) - kMantissaBits;                               // 11
  static constexpr int kSmallCarryTerms = (1 << kSmallCarryBits) - 1;  // 2047

  V8_EXPORT void AddInfNan(int64_t ivalue);
  // Returns the index of the uppermost non-zero chunk.
  V8_EXPORT int CarryPropagate();

  template <typename T>
  inline void Add1NoCarry(T value) {
    int64_t ivalue = base::bit_cast<int64_t>(static_cast<double>(value));

    int64_t exp = (ivalue >> kMantissaBits) & kExpMask;
    int64_t mantissa = ivalue & kMantissaMask;
    int64_t high_exp = exp >> kLowExpBits;
    int64_t low_exp = exp & kLowExpMask;

    if (exp == 0) {
      // Zero or denormalized.
      if (mantissa == 0) return;
      exp = low_exp = 1;
    } else if (exp == kExpMask) {
      // Inf or NaN.
      AddInfNan(ivalue);
      return;
    } else {
      // Normalized.
      mantissa |= int64_t{1} << kMantissaBits;
    }

    auto chunk_it = chunk_.begin() + high_exp;

    int64_t split_mantissa[2];
    split_mantissa[0] =
        (static_cast<uint64_t>(mantissa) << low_exp) & kLowMantissaMask;
    split_mantissa[1] = mantissa >> (kLowMantissaBits - low_exp);

    if (ivalue < 0) {
      *chunk_it -= split_mantissa[0];
      *(chunk_it + 1) -= split_mantissa[1];
    } else {
      *chunk_it += split_mantissa[0];
      *(chunk_it + 1) += split_mantissa[1];
    }
  }

  std::array<int64_t, kSchunks> chunk_;
  int64_t inf_ = 0;
  int64_t nan_ = 0;
  int adds_until_propagate_ = kSmallCarryTerms;
  bool minus_zero_ = true;
  bool inf_sign_change_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_MATH_XSUM_H_
