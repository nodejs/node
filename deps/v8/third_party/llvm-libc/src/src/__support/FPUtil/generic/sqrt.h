//===-- Square root of IEEE 754 floating point numbers ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_SQRT_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_SQRT_H

#include "src/__support/CPP/bit.h" // countl_zero
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/uint128.h"

#include "hdr/fenv_macros.h"

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
#include "sqrt_80_bit_long_double.h"
#endif // !LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

namespace internal {

template <typename T> struct SpecialLongDouble {
  static constexpr bool VALUE = false;
};

#if defined(LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80)
template <> struct SpecialLongDouble<long double> {
  static constexpr bool VALUE = true;
};
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

template <typename T>
LIBC_INLINE void normalize(int &exponent,
                           typename FPBits<T>::StorageType &mantissa) {
  const int shift =
      cpp::countl_zero(mantissa) -
      (8 * static_cast<int>(sizeof(mantissa)) - 1 - FPBits<T>::FRACTION_LEN);
  exponent -= shift;
  mantissa <<= shift;
}

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64
template <>
LIBC_INLINE void normalize<long double>(int &exponent, uint64_t &mantissa) {
  normalize<double>(exponent, mantissa);
}
#elif defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT128)
template <>
LIBC_INLINE void normalize<long double>(int &exponent, UInt128 &mantissa) {
  const uint64_t hi_bits = static_cast<uint64_t>(mantissa >> 64);
  const int shift =
      hi_bits ? (cpp::countl_zero(hi_bits) - 15)
              : (cpp::countl_zero(static_cast<uint64_t>(mantissa)) + 49);
  exponent -= shift;
  mantissa <<= shift;
}
#endif

} // namespace internal

// Correctly rounded IEEE 754 SQRT for all rounding modes.
// Shift-and-add algorithm.
template <typename OutType, typename InType>
LIBC_INLINE static constexpr cpp::enable_if_t<
    cpp::is_floating_point_v<OutType> && cpp::is_floating_point_v<InType> &&
        sizeof(OutType) <= sizeof(InType),
    OutType>
sqrt(InType x) {
  if constexpr (internal::SpecialLongDouble<OutType>::VALUE &&
                internal::SpecialLongDouble<InType>::VALUE) {
#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
    // Special 80-bit long double.
    return x86::sqrt(x);
#endif // !LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
  } else {
    // IEEE floating points formats.
    using OutFPBits = FPBits<OutType>;
    using InFPBits = FPBits<InType>;
    using InStorageType = typename InFPBits::StorageType;
    using DyadicFloat =
        DyadicFloat<cpp::bit_ceil(static_cast<size_t>(InFPBits::STORAGE_LEN))>;

    constexpr InStorageType ONE = InStorageType(1) << InFPBits::FRACTION_LEN;
    constexpr auto FLT_NAN = OutFPBits::quiet_nan().get_val();

    InFPBits bits(x);

    if (bits == InFPBits::inf(Sign::POS) || bits.is_zero() || bits.is_nan()) {
      // sqrt(+Inf) = +Inf
      // sqrt(+0) = +0
      // sqrt(-0) = -0
      // sqrt(NaN) = NaN
      // sqrt(-NaN) = -NaN
      return cast<OutType>(x);
    } else if (bits.is_neg()) {
      // sqrt(-Inf) = NaN
      // sqrt(-x) = NaN
      return FLT_NAN;
    } else {
      int x_exp = bits.get_exponent();
      InStorageType x_mant = bits.get_mantissa();

      // Step 1a: Normalize denormal input and append hidden bit to the mantissa
      if (bits.is_subnormal()) {
        ++x_exp; // let x_exp be the correct exponent of ONE bit.
        internal::normalize<InType>(x_exp, x_mant);
      } else {
        x_mant |= ONE;
      }

      // Step 1b: Make sure the exponent is even.
      if (x_exp & 1) {
        --x_exp;
        x_mant <<= 1;
      }

      // After step 1b, x = 2^(x_exp) * x_mant, where x_exp is even, and
      // 1 <= x_mant < 4.  So sqrt(x) = 2^(x_exp / 2) * y, with 1 <= y < 2.
      // Notice that the output of sqrt is always in the normal range.
      // To perform shift-and-add algorithm to find y, let denote:
      //   y(n) = 1.y_1 y_2 ... y_n, we can define the nth residue to be:
      //   r(n) = 2^n ( x_mant - y(n)^2 ).
      // That leads to the following recurrence formula:
      //   r(n) = 2*r(n-1) - y_n*[ 2*y(n-1) + 2^(-n-1) ]
      // with the initial conditions: y(0) = 1, and r(0) = x - 1.
      // So the nth digit y_n of the mantissa of sqrt(x) can be found by:
      //   y_n = 1 if 2*r(n-1) >= 2*y(n - 1) + 2^(-n-1)
      //         0 otherwise.
      InStorageType y = ONE;
      InStorageType r = x_mant - ONE;

      // TODO: Reduce iteration count to OutFPBits::FRACTION_LEN + 2 or + 3.
      for (InStorageType current_bit = ONE >> 1; current_bit;
           current_bit >>= 1) {
        r <<= 1;
        // 2*y(n - 1) + 2^(-n-1)
        InStorageType tmp = static_cast<InStorageType>((y << 1) + current_bit);
        if (r >= tmp) {
          r -= tmp;
          y += current_bit;
        }
      }

      // We compute one more iteration in order to round correctly.
      r <<= 2;
      y <<= 2;
      InStorageType tmp = y + 1;
      if (r >= tmp) {
        r -= tmp;
        // Rounding bit.
        y |= 2;
      }
      // Sticky bit.
      y |= static_cast<unsigned int>(r != 0);

      DyadicFloat yd(Sign::POS, (x_exp >> 1) - 2 - InFPBits::FRACTION_LEN, y);
      return yd.template as<OutType, /*ShouldSignalExceptions=*/true>();
    }
  }
}

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_SQRT_H
