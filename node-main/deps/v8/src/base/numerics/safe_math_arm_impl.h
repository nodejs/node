// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_SAFE_MATH_ARM_IMPL_H_
#define V8_BASE_NUMERICS_SAFE_MATH_ARM_IMPL_H_

// IWYU pragma: private

#include <stdint.h>

#include <cassert>

#include "src/base/numerics/safe_conversions.h"

namespace v8::base::internal {

template <typename T, typename U>
struct CheckedMulFastAsmOp {
  static constexpr bool is_supported =
      kEnableAsmCode && kIsFastIntegerArithmeticPromotionContained<T, U>;

  // The following is not an assembler routine and is thus constexpr safe, it
  // just emits much more efficient code than the Clang and GCC builtins for
  // performing overflow-checked multiplication when a twice wider type is
  // available. The below compiles down to 2-3 instructions, depending on the
  // width of the types in use.
  // As an example, an int32_t multiply compiles to:
  //    smull   r0, r1, r0, r1
  //    cmp     r1, r1, asr #31
  // And an int16_t multiply compiles to:
  //    smulbb  r1, r1, r0
  //    asr     r2, r1, #16
  //    cmp     r2, r1, asr #15
  template <typename V>
  static constexpr bool Do(T x, U y, V* result) {
    using Promotion = FastIntegerArithmeticPromotion<T, U>;
    Promotion presult;

    presult = static_cast<Promotion>(x) * static_cast<Promotion>(y);
    if (!IsValueInRangeForNumericType<V>(presult)) {
      return false;
    }
    *result = static_cast<V>(presult);
    return true;
  }
};

template <typename T, typename U>
struct ClampedAddFastAsmOp {
  static constexpr bool is_supported =
      kEnableAsmCode && kIsBigEnoughPromotionContained<T, U> &&
      kIsTypeInRangeForNumericType<int32_t, BigEnoughPromotion<T, U>>;

  template <typename V>
  __attribute__((always_inline)) static V Do(T x, U y) {
    // This will get promoted to an int, so let the compiler do whatever is
    // clever and rely on the saturated cast to bounds check.
    if constexpr (kIsIntegerArithmeticSafe<int, T, U>) {
      return saturated_cast<V>(static_cast<int>(x) + static_cast<int>(y));
    } else {
      int32_t result;
      int32_t x_i32 = checked_cast<int32_t>(x);
      int32_t y_i32 = checked_cast<int32_t>(y);

      asm("qadd %[result], %[first], %[second]"
          : [result] "=r"(result)
          : [first] "r"(x_i32), [second] "r"(y_i32));
      return saturated_cast<V>(result);
    }
  }
};

template <typename T, typename U>
struct ClampedSubFastAsmOp {
  static constexpr bool is_supported =
      kEnableAsmCode && kIsBigEnoughPromotionContained<T, U> &&
      kIsTypeInRangeForNumericType<int32_t, BigEnoughPromotion<T, U>>;

  template <typename V>
  __attribute__((always_inline)) static V Do(T x, U y) {
    // This will get promoted to an int, so let the compiler do whatever is
    // clever and rely on the saturated cast to bounds check.
    if constexpr (kIsIntegerArithmeticSafe<int, T, U>) {
      return saturated_cast<V>(static_cast<int>(x) - static_cast<int>(y));
    } else {
      int32_t result;
      int32_t x_i32 = checked_cast<int32_t>(x);
      int32_t y_i32 = checked_cast<int32_t>(y);

      asm("qsub %[result], %[first], %[second]"
          : [result] "=r"(result)
          : [first] "r"(x_i32), [second] "r"(y_i32));
      return saturated_cast<V>(result);
    }
  }
};

template <typename T, typename U>
struct ClampedMulFastAsmOp {
  static constexpr bool is_supported =
      kEnableAsmCode && CheckedMulFastAsmOp<T, U>::is_supported;

  template <typename V>
  __attribute__((always_inline)) static V Do(T x, U y) {
    // Use the CheckedMulFastAsmOp for full-width 32-bit values, because
    // it's fewer instructions than promoting and then saturating.
    if constexpr (!kIsIntegerArithmeticSafe<int32_t, T, U> &&
                  !kIsIntegerArithmeticSafe<uint32_t, T, U>) {
      V result;
      return CheckedMulFastAsmOp<T, U>::Do(x, y, &result)
                 ? result
                 : CommonMaxOrMin<V>(IsValueNegative(x) ^ IsValueNegative(y));
    } else {
      static_assert(kIsFastIntegerArithmeticPromotionContained<T, U>);
      using Promotion = FastIntegerArithmeticPromotion<T, U>;
      return saturated_cast<V>(static_cast<Promotion>(x) *
                               static_cast<Promotion>(y));
    }
  }
};

}  // namespace v8::base::internal

#endif  // V8_BASE_NUMERICS_SAFE_MATH_ARM_IMPL_H_
