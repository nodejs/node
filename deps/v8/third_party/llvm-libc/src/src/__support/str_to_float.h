//===-- String to float conversion utils ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This file is shared with libc++. You should also be careful when adding
// dependencies to this file, since it needs to build for all libc++ targets.
// -----------------------------------------------------------------------------

#ifndef LLVM_LIBC_SRC___SUPPORT_STR_TO_FLOAT_H
#define LLVM_LIBC_SRC___SUPPORT_STR_TO_FLOAT_H

#include "hdr/errno_macros.h" // For ERANGE
#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/detailed_powers_of_ten.h"
#include "src/__support/high_precision_decimal.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/str_to_integer.h"
#include "src/__support/str_to_num_result.h"
#include "src/__support/uint128.h"
#include "src/__support/wctype_utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This interface is shared with libc++, if you change this interface you need
// to update it in both libc and libc++.
// -----------------------------------------------------------------------------
template <class T> struct ExpandedFloat {
  typename fputil::FPBits<T>::StorageType mantissa;
  int32_t exponent;
};

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This interface is shared with libc++, if you change this interface you need
// to update it in both libc and libc++.
// -----------------------------------------------------------------------------
template <class T> struct FloatConvertReturn {
  ExpandedFloat<T> num = {0, 0};
  int error = 0;
};

LIBC_INLINE uint64_t low64(const UInt128 &num) {
  return static_cast<uint64_t>(num & 0xffffffffffffffff);
}

LIBC_INLINE uint64_t high64(const UInt128 &num) {
  return static_cast<uint64_t>(num >> 64);
}

template <class T> LIBC_INLINE void set_implicit_bit(fputil::FPBits<T> &) {
  return;
}

#if defined(LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80)
template <>
LIBC_INLINE void
set_implicit_bit<long double>(fputil::FPBits<long double> &result) {
  result.set_implicit_bit(result.get_biased_exponent() != 0);
}
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

// This Eisel-Lemire implementation is based on the algorithm described in the
// paper Number Parsing at a Gigabyte per Second, Software: Practice and
// Experience 51 (8), 2021 (https://arxiv.org/abs/2101.11408), as well as the
// description by Nigel Tao
// (https://nigeltao.github.io/blog/2020/eisel-lemire.html) and the golang
// implementation, also by Nigel Tao
// (https://github.com/golang/go/blob/release-branch.go1.16/src/strconv/eisel_lemire.go#L25)
// for some optimizations as well as handling 32 bit floats.
template <class T>
LIBC_INLINE cpp::optional<ExpandedFloat<T>>
eisel_lemire(ExpandedFloat<T> init_num,
             RoundDirection round = RoundDirection::Nearest) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  StorageType mantissa = init_num.mantissa;
  int32_t exp10 = init_num.exponent;

  if (sizeof(T) > 8) { // This algorithm cannot handle anything longer than a
                       // double, so we skip straight to the fallback.
    return cpp::nullopt;
  }

  // Exp10 Range
  if (exp10 < DETAILED_POWERS_OF_TEN_MIN_EXP_10 ||
      exp10 > DETAILED_POWERS_OF_TEN_MAX_EXP_10) {
    return cpp::nullopt;
  }

  // Normalization
  uint32_t clz = static_cast<uint32_t>(cpp::countl_zero<StorageType>(mantissa));
  mantissa <<= clz;

  int32_t exp2 = exp10_to_exp2(exp10) + FPBits::STORAGE_LEN + FPBits::EXP_BIAS -
                 static_cast<int32_t>(clz);

  // Multiplication
  const uint64_t *power_of_ten =
      DETAILED_POWERS_OF_TEN[exp10 - DETAILED_POWERS_OF_TEN_MIN_EXP_10];

  UInt128 first_approx =
      static_cast<UInt128>(mantissa) * static_cast<UInt128>(power_of_ten[1]);

  // Wider Approximation
  UInt128 final_approx;
  // The halfway constant is used to check if the bits that will be shifted away
  // initially are all 1. For doubles this is 64 (bitstype size) - 52 (final
  // mantissa size) - 3 (we shift away the last two bits separately for
  // accuracy, and the most significant bit is ignored.) = 9 bits. Similarly,
  // it's 6 bits for floats in this case.
  const uint64_t halfway_constant =
      (uint64_t(1) << (FPBits::STORAGE_LEN - (FPBits::FRACTION_LEN + 3))) - 1;
  if ((high64(first_approx) & halfway_constant) == halfway_constant &&
      low64(first_approx) + mantissa < mantissa) {
    UInt128 low_bits =
        static_cast<UInt128>(mantissa) * static_cast<UInt128>(power_of_ten[0]);
    UInt128 second_approx =
        first_approx + static_cast<UInt128>(high64(low_bits));

    if ((high64(second_approx) & halfway_constant) == halfway_constant &&
        low64(second_approx) + 1 == 0 &&
        low64(low_bits) + mantissa < mantissa) {
      return cpp::nullopt;
    }
    final_approx = second_approx;
  } else {
    final_approx = first_approx;
  }

  // Shifting to 54 bits for doubles and 25 bits for floats
  StorageType msb = static_cast<StorageType>(high64(final_approx) >>
                                             (FPBits::STORAGE_LEN - 1));
  StorageType final_mantissa = static_cast<StorageType>(
      high64(final_approx) >>
      (msb + FPBits::STORAGE_LEN - (FPBits::FRACTION_LEN + 3)));
  exp2 -= static_cast<uint32_t>(1 ^ msb); // same as !msb

  if (round == RoundDirection::Nearest) {
    // Half-way ambiguity
    if (low64(final_approx) == 0 &&
        (high64(final_approx) & halfway_constant) == 0 &&
        (final_mantissa & 3) == 1) {
      return cpp::nullopt;
    }

    // Round to even.
    final_mantissa += final_mantissa & 1;

  } else if (round == RoundDirection::Up) {
    // If any of the bits being rounded away are non-zero, then round up.
    if (low64(final_approx) > 0 ||
        (high64(final_approx) & halfway_constant) > 0) {
      // Add two since the last current lowest bit is about to be shifted away.
      final_mantissa += 2;
    }
  }
  // else round down, which has no effect.

  // From 54 to 53 bits for doubles and 25 to 24 bits for floats
  final_mantissa >>= 1;
  if ((final_mantissa >> (FPBits::FRACTION_LEN + 1)) > 0) {
    final_mantissa >>= 1;
    ++exp2;
  }

  // The if block is equivalent to (but has fewer branches than):
  //   if exp2 <= 0 || exp2 >= 0x7FF { etc }
  if (static_cast<uint32_t>(exp2) - 1 >= (1 << FPBits::EXP_LEN) - 2) {
    return cpp::nullopt;
  }

  ExpandedFloat<T> output;
  output.mantissa = final_mantissa;
  output.exponent = exp2;
  return output;
}

// TODO: Re-enable eisel-lemire for long double is double double once it's
// properly supported.
#if !defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64) &&                             \
    !defined(LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE)
template <>
LIBC_INLINE cpp::optional<ExpandedFloat<long double>>
eisel_lemire<long double>(ExpandedFloat<long double> init_num,
                          RoundDirection round) {
  using FPBits = typename fputil::FPBits<long double>;
  using StorageType = typename FPBits::StorageType;

  UInt128 mantissa = init_num.mantissa;
  int32_t exp10 = init_num.exponent;

  // Exp10 Range
  // This doesn't reach very far into the range for long doubles, since it's
  // sized for doubles and their 11 exponent bits, and not for long doubles and
  // their 15 exponent bits (max exponent of ~300 for double vs ~5000 for long
  // double). This is a known tradeoff, and was made because a proper long
  // double table would be approximately 16 times larger. This would have
  // significant memory and storage costs all the time to speed up a relatively
  // uncommon path. In addition the exp10_to_exp2 function only approximates
  // multiplying by log(10)/log(2), and that approximation may not be accurate
  // out to the full long double range.
  if (exp10 < DETAILED_POWERS_OF_TEN_MIN_EXP_10 ||
      exp10 > DETAILED_POWERS_OF_TEN_MAX_EXP_10) {
    return cpp::nullopt;
  }

  // Normalization
  int32_t clz = static_cast<int32_t>(cpp::countl_zero(mantissa)) -
                ((sizeof(UInt128) - sizeof(StorageType)) * CHAR_BIT);
  mantissa <<= clz;

  int32_t exp2 =
      exp10_to_exp2(exp10) + FPBits::STORAGE_LEN + FPBits::EXP_BIAS - clz;

  // Multiplication
  const uint64_t *power_of_ten =
      DETAILED_POWERS_OF_TEN[exp10 - DETAILED_POWERS_OF_TEN_MIN_EXP_10];

  // Since the input mantissa is more than 64 bits, we have to multiply with the
  // full 128 bits of the power of ten to get an approximation with the same
  // number of significant bits. This means that we only get the one
  // approximation, and that approximation is 256 bits long.
  UInt128 approx_upper = static_cast<UInt128>(high64(mantissa)) *
                         static_cast<UInt128>(power_of_ten[1]);

  UInt128 approx_middle_a = static_cast<UInt128>(high64(mantissa)) *
                            static_cast<UInt128>(power_of_ten[0]);
  UInt128 approx_middle_b = static_cast<UInt128>(low64(mantissa)) *
                            static_cast<UInt128>(power_of_ten[1]);

  UInt128 approx_middle = approx_middle_a + approx_middle_b;

  // Handle overflow in the middle
  approx_upper += (approx_middle < approx_middle_a) ? UInt128(1) << 64 : 0;

  UInt128 approx_lower = static_cast<UInt128>(low64(mantissa)) *
                         static_cast<UInt128>(power_of_ten[0]);

  UInt128 final_approx_lower =
      approx_lower + (static_cast<UInt128>(low64(approx_middle)) << 64);
  UInt128 final_approx_upper = approx_upper + high64(approx_middle) +
                               (final_approx_lower < approx_lower ? 1 : 0);

  // The halfway constant is used to check if the bits that will be shifted away
  // initially are all 1. For 80 bit floats this is 128 (bitstype size) - 64
  // (final mantissa size) - 3 (we shift away the last two bits separately for
  // accuracy, and the most significant bit is ignored.) = 61 bits. Similarly,
  // it's 12 bits for 128 bit floats in this case.
  constexpr UInt128 HALFWAY_CONSTANT =
      (UInt128(1) << (FPBits::STORAGE_LEN - (FPBits::FRACTION_LEN + 3))) - 1;

  if ((final_approx_upper & HALFWAY_CONSTANT) == HALFWAY_CONSTANT &&
      final_approx_lower + mantissa < mantissa) {
    return cpp::nullopt;
  }

  // Shifting to 65 bits for 80 bit floats and 113 bits for 128 bit floats
  uint32_t msb =
      static_cast<uint32_t>(final_approx_upper >> (FPBits::STORAGE_LEN - 1));
  UInt128 final_mantissa = final_approx_upper >> (msb + FPBits::STORAGE_LEN -
                                                  (FPBits::FRACTION_LEN + 3));
  exp2 -= static_cast<uint32_t>(1 ^ msb); // same as !msb

  if (round == RoundDirection::Nearest) {
    // Half-way ambiguity
    if (final_approx_lower == 0 &&
        (final_approx_upper & HALFWAY_CONSTANT) == 0 &&
        (final_mantissa & 3) == 1) {
      return cpp::nullopt;
    }
    // Round to even.
    final_mantissa += final_mantissa & 1;

  } else if (round == RoundDirection::Up) {
    // If any of the bits being rounded away are non-zero, then round up.
    if (final_approx_lower > 0 || (final_approx_upper & HALFWAY_CONSTANT) > 0) {
      // Add two since the last current lowest bit is about to be shifted away.
      final_mantissa += 2;
    }
  }
  // else round down, which has no effect.

  // From 65 to 64 bits for 80 bit floats and 113  to 112 bits for 128 bit
  // floats
  final_mantissa >>= 1;
  if ((final_mantissa >> (FPBits::FRACTION_LEN + 1)) > 0) {
    final_mantissa >>= 1;
    ++exp2;
  }

  // The if block is equivalent to (but has fewer branches than):
  //   if exp2 <= 0 || exp2 >= MANTISSA_MAX { etc }
  if (exp2 - 1 >= (1 << FPBits::EXP_LEN) - 2) {
    return cpp::nullopt;
  }

  ExpandedFloat<long double> output;
  output.mantissa = static_cast<StorageType>(final_mantissa);
  output.exponent = exp2;
  return output;
}
#endif // !defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64) &&
       // !defined(LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE)

// The nth item in POWERS_OF_TWO represents the greatest power of two less than
// 10^n. This tells us how much we can safely shift without overshooting.
constexpr uint8_t POWERS_OF_TWO[19] = {
    0, 3, 6, 9, 13, 16, 19, 23, 26, 29, 33, 36, 39, 43, 46, 49, 53, 56, 59,
};
constexpr int32_t NUM_POWERS_OF_TWO =
    sizeof(POWERS_OF_TWO) / sizeof(POWERS_OF_TWO[0]);

// Takes a mantissa and base 10 exponent and converts it into its closest
// floating point type T equivalent. This is the fallback algorithm used when
// the Eisel-Lemire algorithm fails, it's slower but more accurate. It's based
// on the Simple Decimal Conversion algorithm by Nigel Tao, described at this
// link: https://nigeltao.github.io/blog/2020/parse-number-f64-simple.html
template <typename T, typename CharType>
LIBC_INLINE FloatConvertReturn<T> simple_decimal_conversion(
    const CharType *__restrict numStart,
    const size_t num_len = cpp::numeric_limits<size_t>::max(),
    RoundDirection round = RoundDirection::Nearest) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  int32_t exp2 = 0;
  HighPrecisionDecimal hpd = HighPrecisionDecimal(numStart, num_len);

  FloatConvertReturn<T> output;

  if (hpd.get_num_digits() == 0) {
    output.num = {0, 0};
    return output;
  }

  // If the exponent is too large and can't be represented in this size of
  // float, return inf.
  if (hpd.get_decimal_point() > 0 &&
      exp10_to_exp2(hpd.get_decimal_point() - 1) > FPBits::EXP_BIAS) {
    output.num = {0, fputil::FPBits<T>::MAX_BIASED_EXPONENT};
    output.error = ERANGE;
    return output;
  }
  // If the exponent is too small even for a subnormal, return 0.
  if (hpd.get_decimal_point() < 0 &&
      exp10_to_exp2(-hpd.get_decimal_point()) >
          (FPBits::EXP_BIAS + static_cast<int32_t>(FPBits::FRACTION_LEN))) {
    output.num = {0, 0};
    output.error = ERANGE;
    return output;
  }

  // Right shift until the number is smaller than 1.
  while (hpd.get_decimal_point() > 0) {
    int32_t shift_amount = 0;
    if (hpd.get_decimal_point() >= NUM_POWERS_OF_TWO) {
      shift_amount = 60;
    } else {
      shift_amount = POWERS_OF_TWO[hpd.get_decimal_point()];
    }
    exp2 += shift_amount;
    hpd.shift(-shift_amount);
  }

  // Left shift until the number is between 1/2 and 1
  while (hpd.get_decimal_point() < 0 ||
         (hpd.get_decimal_point() == 0 && hpd.get_digits()[0] < 5)) {
    int32_t shift_amount = 0;

    if (-hpd.get_decimal_point() >= NUM_POWERS_OF_TWO) {
      shift_amount = 60;
    } else if (hpd.get_decimal_point() != 0) {
      shift_amount = POWERS_OF_TWO[-hpd.get_decimal_point()];
    } else { // This handles the case of the number being between .1 and .5
      shift_amount = 1;
    }
    exp2 -= shift_amount;
    hpd.shift(shift_amount);
  }

  // Left shift once so that the number is between 1 and 2
  --exp2;
  hpd.shift(1);

  // Get the biased exponent
  exp2 += FPBits::EXP_BIAS;

  // Handle the exponent being too large (and return inf).
  if (exp2 >= FPBits::MAX_BIASED_EXPONENT) {
    output.num = {0, FPBits::MAX_BIASED_EXPONENT};
    output.error = ERANGE;
    return output;
  }

  // Shift left to fill the mantissa
  hpd.shift(FPBits::FRACTION_LEN);
  StorageType final_mantissa = hpd.round_to_integer_type<StorageType>();

  // Handle subnormals
  if (exp2 <= 0) {
    // Shift right until there is a valid exponent
    while (exp2 < 0) {
      hpd.shift(-1);
      ++exp2;
    }
    // Shift right one more time to compensate for the left shift to get it
    // between 1 and 2.
    hpd.shift(-1);
    final_mantissa = hpd.round_to_integer_type<StorageType>(round);

    // Check if by shifting right we've caused this to round to a normal number.
    if ((final_mantissa >> FPBits::FRACTION_LEN) != 0) {
      ++exp2;
    }
  }

  // Check if rounding added a bit, and shift down if that's the case.
  if (final_mantissa == StorageType(2) << FPBits::FRACTION_LEN) {
    final_mantissa >>= 1;
    ++exp2;

    // Check if this rounding causes exp2 to go out of range and make the result
    // INF. If this is the case, then finalMantissa and exp2 are already the
    // correct values for an INF result.
    if (exp2 >= FPBits::MAX_BIASED_EXPONENT) {
      output.error = ERANGE;
    }
  }

  if (exp2 == 0) {
    output.error = ERANGE;
  }

  output.num = {final_mantissa, exp2};
  return output;
}

// This class is used for templating the constants for Clinger's Fast Path,
// described as a method of approximation in
// Clinger WD. How to Read Floating Point Numbers Accurately. SIGPLAN Not 1990
// Jun;25(6):92–101. https://doi.org/10.1145/93548.93557.
// As well as the additions by Gay that extend the useful range by the number of
// exact digits stored by the float type, described in
// Gay DM, Correctly rounded binary-decimal and decimal-binary conversions;
// 1990. AT&T Bell Laboratories Numerical Analysis Manuscript 90-10.
template <class T> class ClingerConsts;

template <> class ClingerConsts<float> {
public:
  static constexpr float POWERS_OF_TEN_ARRAY[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5,
                                                  1e6, 1e7, 1e8, 1e9, 1e10};
  static constexpr int32_t EXACT_POWERS_OF_TEN = 10;
  static constexpr int32_t DIGITS_IN_MANTISSA = 7;
  static constexpr float MAX_EXACT_INT = 16777215.0;
};

template <> class ClingerConsts<double> {
public:
  static constexpr double POWERS_OF_TEN_ARRAY[] = {
      1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
      1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
  static constexpr int32_t EXACT_POWERS_OF_TEN = 22;
  static constexpr int32_t DIGITS_IN_MANTISSA = 15;
  static constexpr double MAX_EXACT_INT = 9007199254740991.0;
};

#if defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64)
template <> class ClingerConsts<long double> {
public:
  static constexpr long double POWERS_OF_TEN_ARRAY[] = {
      1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
      1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
  static constexpr int32_t EXACT_POWERS_OF_TEN =
      ClingerConsts<double>::EXACT_POWERS_OF_TEN;
  static constexpr int32_t DIGITS_IN_MANTISSA =
      ClingerConsts<double>::DIGITS_IN_MANTISSA;
  static constexpr long double MAX_EXACT_INT =
      ClingerConsts<double>::MAX_EXACT_INT;
};
#elif defined(LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80)
template <> class ClingerConsts<long double> {
public:
  static constexpr long double POWERS_OF_TEN_ARRAY[] = {
      1e0L,  1e1L,  1e2L,  1e3L,  1e4L,  1e5L,  1e6L,  1e7L,  1e8L,  1e9L,
      1e10L, 1e11L, 1e12L, 1e13L, 1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L,
      1e20L, 1e21L, 1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L};
  static constexpr int32_t EXACT_POWERS_OF_TEN = 27;
  static constexpr int32_t DIGITS_IN_MANTISSA = 21;
  static constexpr long double MAX_EXACT_INT = 18446744073709551615.0L;
};
#elif defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT128)
template <> class ClingerConsts<long double> {
public:
  static constexpr long double POWERS_OF_TEN_ARRAY[] = {
      1e0L,  1e1L,  1e2L,  1e3L,  1e4L,  1e5L,  1e6L,  1e7L,  1e8L,  1e9L,
      1e10L, 1e11L, 1e12L, 1e13L, 1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L,
      1e20L, 1e21L, 1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L, 1e28L, 1e29L,
      1e30L, 1e31L, 1e32L, 1e33L, 1e34L, 1e35L, 1e36L, 1e37L, 1e38L, 1e39L,
      1e40L, 1e41L, 1e42L, 1e43L, 1e44L, 1e45L, 1e46L, 1e47L, 1e48L};
  static constexpr int32_t EXACT_POWERS_OF_TEN = 48;
  static constexpr int32_t DIGITS_IN_MANTISSA = 33;
  static constexpr long double MAX_EXACT_INT =
      10384593717069655257060992658440191.0L;
};
#elif defined(LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE)
// TODO: Add proper double double type support here, currently using constants
// for double since it should be safe.
template <> class ClingerConsts<long double> {
public:
  static constexpr double POWERS_OF_TEN_ARRAY[] = {
      1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
      1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
  static constexpr int32_t EXACT_POWERS_OF_TEN = 22;
  static constexpr int32_t DIGITS_IN_MANTISSA = 15;
  static constexpr double MAX_EXACT_INT = 9007199254740991.0;
};
#else
#error "Unknown long double type"
#endif

// Take an exact mantissa and exponent and attempt to convert it using only
// exact floating point arithmetic. This only handles numbers with low
// exponents, but handles them quickly. This is an implementation of Clinger's
// Fast Path, as described above.
template <class T>
LIBC_INLINE cpp::optional<ExpandedFloat<T>>
clinger_fast_path(ExpandedFloat<T> init_num,
                  RoundDirection round = RoundDirection::Nearest) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  StorageType mantissa = init_num.mantissa;
  int32_t exp10 = init_num.exponent;

  if ((mantissa >> FPBits::FRACTION_LEN) > 0) {
    return cpp::nullopt;
  }

  FPBits result;
  T float_mantissa;
  if constexpr (is_big_int_v<StorageType> || sizeof(T) > sizeof(uint64_t)) {
    float_mantissa =
        (static_cast<T>(uint64_t(mantissa >> 64)) * static_cast<T>(0x1.0p64)) +
        static_cast<T>(uint64_t(mantissa));
  } else {
    float_mantissa = static_cast<T>(mantissa);
  }

  if (exp10 == 0) {
    result = FPBits(float_mantissa);
  }
  if (exp10 > 0) {
    if (exp10 > ClingerConsts<T>::EXACT_POWERS_OF_TEN +
                    ClingerConsts<T>::DIGITS_IN_MANTISSA) {
      return cpp::nullopt;
    }
    if (exp10 > ClingerConsts<T>::EXACT_POWERS_OF_TEN) {
      float_mantissa = float_mantissa *
                       ClingerConsts<T>::POWERS_OF_TEN_ARRAY
                           [exp10 - ClingerConsts<T>::EXACT_POWERS_OF_TEN];
      exp10 = ClingerConsts<T>::EXACT_POWERS_OF_TEN;
    }
    if (float_mantissa > ClingerConsts<T>::MAX_EXACT_INT) {
      return cpp::nullopt;
    }
    result =
        FPBits(float_mantissa * ClingerConsts<T>::POWERS_OF_TEN_ARRAY[exp10]);
  } else if (exp10 < 0) {
    if (-exp10 > ClingerConsts<T>::EXACT_POWERS_OF_TEN) {
      return cpp::nullopt;
    }
    result =
        FPBits(float_mantissa / ClingerConsts<T>::POWERS_OF_TEN_ARRAY[-exp10]);
  }

  // If the rounding mode is not nearest, then the sign of the number may affect
  // the result. To make sure the rounding mode is respected properly, the
  // calculation is redone with a negative result, and the rounding mode is used
  // to select the correct result.
  if (round != RoundDirection::Nearest) {
    FPBits negative_result;
    // I'm 99% sure this will break under fast math optimizations.
    negative_result = FPBits((-float_mantissa) *
                             ClingerConsts<T>::POWERS_OF_TEN_ARRAY[exp10]);

    // If the results are equal, then we don't need to use the rounding mode.
    if (result.get_val() != -negative_result.get_val()) {
      FPBits lower_result;
      FPBits higher_result;

      if (result.get_val() < -negative_result.get_val()) {
        lower_result = result;
        higher_result = negative_result;
      } else {
        lower_result = negative_result;
        higher_result = result;
      }

      if (round == RoundDirection::Up) {
        result = higher_result;
      } else {
        result = lower_result;
      }
    }
  }

  ExpandedFloat<T> output;
  output.mantissa = result.get_explicit_mantissa();
  output.exponent = result.get_biased_exponent();
  return output;
}

// The upper bound is the highest base-10 exponent that could possibly give a
// non-inf result for this size of float. The value is
// log10(2^(exponent bias)).
// The generic approximation uses the fact that log10(2^x) ~= x/3
template <typename T> LIBC_INLINE constexpr int32_t get_upper_bound() {
  return fputil::FPBits<T>::EXP_BIAS / 3;
}

template <> LIBC_INLINE constexpr int32_t get_upper_bound<float>() {
  return 39;
}

template <> LIBC_INLINE constexpr int32_t get_upper_bound<double>() {
  return 309;
}

// The lower bound is the largest negative base-10 exponent that could possibly
// give a non-zero result for this size of float. The value is
// log10(2^(exponent bias + final mantissa width + intermediate mantissa width))
// The intermediate mantissa is the integer that's been parsed from the string,
// and the final mantissa is the fractional part of the output number. A very
// low base 10 exponent with a very high intermediate mantissa can cancel each
// other out, and subnormal numbers allow for the result to be at the very low
// end of the final mantissa.
template <typename T> LIBC_INLINE constexpr int32_t get_lower_bound() {
  using FPBits = typename fputil::FPBits<T>;
  return -((FPBits::EXP_BIAS +
            static_cast<int32_t>(FPBits::FRACTION_LEN + FPBits::STORAGE_LEN)) /
           3);
}

template <> LIBC_INLINE constexpr int32_t get_lower_bound<float>() {
  return -(39 + 6 + 10);
}

template <> LIBC_INLINE constexpr int32_t get_lower_bound<double>() {
  return -(309 + 15 + 20);
}

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This interface is shared with libc++, if you change this interface you need
// to update it in both libc and libc++.
// -----------------------------------------------------------------------------
// Takes a mantissa and base 10 exponent and converts it into its closest
// floating point type T equivalient. First we try the Eisel-Lemire algorithm,
// then if that fails then we fall back to a more accurate algorithm for
// accuracy.
template <typename T, typename CharType>
LIBC_INLINE FloatConvertReturn<T> decimal_exp_to_float(
    ExpandedFloat<T> init_num, [[maybe_unused]] bool truncated,
    RoundDirection round, const CharType *__restrict numStart,
    const size_t num_len = cpp::numeric_limits<size_t>::max()) {
  using FPBits = typename fputil::FPBits<T>;

  int32_t exp10 = init_num.exponent;

  FloatConvertReturn<T> output;
  [[maybe_unused]] cpp::optional<ExpandedFloat<T>> opt_output;

  // If the exponent is too large and can't be represented in this size of
  // float, return inf. These bounds are relatively loose, but are mostly
  // serving as a first pass. Some close numbers getting through is okay.
  if (exp10 > get_upper_bound<T>()) {
    output.num = {0, FPBits::MAX_BIASED_EXPONENT};
    output.error = ERANGE;
    return output;
  }
  // If the exponent is too small even for a subnormal, return 0.
  if (exp10 < get_lower_bound<T>()) {
    output.num = {0, 0};
    output.error = ERANGE;
    return output;
  }

  // Clinger's Fast Path and Eisel-Lemire can't set errno, but they can fail.
  // For this reason the "error" field in their return values is used to
  // represent whether they've failed as opposed to the errno value. Any
  // non-zero value represents a failure.

#ifndef LIBC_COPT_STRTOFLOAT_DISABLE_CLINGER_FAST_PATH
  if (!truncated) {
    opt_output = clinger_fast_path<T>(init_num, round);
    // If the algorithm succeeded the error will be 0, else it will be a
    // non-zero number.
    if (opt_output.has_value()) {
      return {opt_output.value(), 0};
    }
  }
#endif // LIBC_COPT_STRTOFLOAT_DISABLE_CLINGER_FAST_PATH

#ifndef LIBC_COPT_STRTOFLOAT_DISABLE_EISEL_LEMIRE
  // Try Eisel-Lemire
  using StorageType = typename FPBits::StorageType;
  StorageType mantissa = init_num.mantissa;
  opt_output = eisel_lemire<T>(init_num, round);
  if (opt_output.has_value()) {
    if (!truncated) {
      return {opt_output.value(), 0};
    }
    // If the mantissa is truncated, then the result may be off by the LSB, so
    // check if rounding the mantissa up changes the result. If not, then it's
    // safe, else use the fallback.
    auto second_output = eisel_lemire<T>({mantissa + 1, exp10}, round);
    if (second_output.has_value()) {
      if (opt_output->mantissa == second_output->mantissa &&
          opt_output->exponent == second_output->exponent) {
        return {opt_output.value(), 0};
      }
    }
  }
#endif // LIBC_COPT_STRTOFLOAT_DISABLE_EISEL_LEMIRE

#ifndef LIBC_COPT_STRTOFLOAT_DISABLE_SIMPLE_DECIMAL_CONVERSION
  output = simple_decimal_conversion<T>(numStart, num_len, round);
#else
#warning "Simple decimal conversion is disabled, result may not be correct."
#endif // LIBC_COPT_STRTOFLOAT_DISABLE_SIMPLE_DECIMAL_CONVERSION

  return output;
}

// -----------------------------------------------------------------------------
//                               **** WARNING ****
// This interface is shared with libc++, if you change this interface you need
// to update it in both libc and libc++.
// -----------------------------------------------------------------------------
// Takes a mantissa and base 2 exponent and converts it into its closest
// floating point type T equivalient. Since the exponent is already in the right
// form, this is mostly just shifting and rounding. This is used for hexadecimal
// numbers since a base 16 exponent multiplied by 4 is the base 2 exponent.
template <class T>
LIBC_INLINE FloatConvertReturn<T> binary_exp_to_float(ExpandedFloat<T> init_num,
                                                      bool truncated,
                                                      RoundDirection round) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  StorageType mantissa = init_num.mantissa;
  int32_t exp2 = init_num.exponent;

  FloatConvertReturn<T> output;

  // This is the number of leading zeroes a properly normalized float of type T
  // should have.
  constexpr int32_t INF_EXP = (1 << FPBits::EXP_LEN) - 1;

  // Normalization step 1: Bring the leading bit to the highest bit of
  // StorageType.
  uint32_t amount_to_shift_left = cpp::countl_zero<StorageType>(mantissa);
  mantissa <<= amount_to_shift_left;

  // Keep exp2 representing the exponent of the lowest bit of StorageType.
  exp2 -= amount_to_shift_left;

  // biased_exponent represents the biased exponent of the most significant bit.
  int32_t biased_exponent = exp2 + FPBits::STORAGE_LEN + FPBits::EXP_BIAS - 1;

  // Handle numbers that're too large and get squashed to inf
  if (biased_exponent >= INF_EXP) {
    // This indicates an overflow, so we make the result INF and set errno.
    output.num = {0, (1 << FPBits::EXP_LEN) - 1};
    output.error = ERANGE;
    return output;
  }

  uint32_t amount_to_shift_right =
      FPBits::STORAGE_LEN - FPBits::FRACTION_LEN - 1;

  // Handle subnormals.
  if (biased_exponent <= 0) {
    amount_to_shift_right += static_cast<uint32_t>(1 - biased_exponent);
    biased_exponent = 0;

    if (amount_to_shift_right > FPBits::STORAGE_LEN) {
      // Return 0 if the exponent is too small.
      output.num = {0, 0};
      output.error = ERANGE;
      return output;
    }
  }

  StorageType round_bit_mask = StorageType(1) << (amount_to_shift_right - 1);
  StorageType sticky_mask = round_bit_mask - 1;
  bool round_bit = static_cast<bool>(mantissa & round_bit_mask);
  bool sticky_bit = static_cast<bool>(mantissa & sticky_mask) || truncated;

  if (amount_to_shift_right < FPBits::STORAGE_LEN) {
    // Shift the mantissa and clear the implicit bit.
    mantissa >>= amount_to_shift_right;
    mantissa &= FPBits::FRACTION_MASK;
  } else {
    mantissa = 0;
  }
  bool least_significant_bit = static_cast<bool>(mantissa & StorageType(1));

  // TODO: check that this rounding behavior is correct.

  if (round == RoundDirection::Nearest) {
    // Perform rounding-to-nearest, tie-to-even.
    if (round_bit && (least_significant_bit || sticky_bit)) {
      ++mantissa;
    }
  } else if (round == RoundDirection::Up) {
    if (round_bit || sticky_bit) {
      ++mantissa;
    }
  } else /* (round == RoundDirection::Down)*/ {
    if (round_bit && sticky_bit) {
      ++mantissa;
    }
  }

  if (mantissa > FPBits::FRACTION_MASK) {
    // Rounding causes the exponent to increase.
    ++biased_exponent;

    if (biased_exponent == INF_EXP) {
      output.error = ERANGE;
    }
  }

  if (biased_exponent == 0) {
    output.error = ERANGE;
  }

  output.num = {mantissa & FPBits::FRACTION_MASK, biased_exponent};
  return output;
}

// Checks if the first characters of the string pointer are the start of a
// hexadecimal floating point number. Does not advance the string pointer.
template <typename CharType>
LIBC_INLINE static bool is_float_hex_start(const CharType *__restrict src) {
  if (!is_char_or_wchar(src[0], '0', L'0') ||
      !is_char_or_wchar(tolower(src[1]), 'x', L'x')) {
    return false;
  }
  size_t first_digit = 2;
  if (src[2] == constants<CharType>::DECIMAL_POINT) {
    ++first_digit;
  }
  return isalnum(src[first_digit]) && b36_char_to_int(src[first_digit]) < 16;
}

// Verifies that first prefix_len characters of str, when lowercased, match the
// specified prefix.
template <typename CharType>
LIBC_INLINE static bool tolower_starts_with(const CharType *str,
                                            size_t prefix_len,
                                            const CharType *prefix) {
  for (size_t i = 0; i < prefix_len; ++i) {
    if (tolower(str[i]) != prefix[i])
      return false;
  }
  return true;
}

// Attempts parsing a decimal floating point number at the start of the string.
template <typename T, typename CharType>
LIBC_INLINE static StrToNumResult<ExpandedFloat<T>>
decimal_string_to_float(const CharType *__restrict src, RoundDirection round) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  constexpr uint32_t BASE = 10;
  bool truncated = false;
  bool seen_digit = false;
  bool after_decimal = false;
  StorageType mantissa = 0;
  int32_t exponent = 0;

  size_t index = 0;

  StrToNumResult<ExpandedFloat<T>> output({0, 0});

  // The goal for the first step of parsing is to convert the number in src to
  // the format mantissa * (base ^ exponent)

  // The loop fills the mantissa with as many digits as it can hold
  const StorageType bitstype_max_div_by_base =
      cpp::numeric_limits<StorageType>::max() / BASE;
  while (true) {
    if (isdigit(src[index])) {
      uint32_t digit = static_cast<uint32_t>(b36_char_to_int(src[index]));
      seen_digit = true;

      if (mantissa < bitstype_max_div_by_base) {
        mantissa = (mantissa * BASE) + digit;
        if (after_decimal) {
          --exponent;
        }
      } else {
        if (digit > 0)
          truncated = true;
        if (!after_decimal)
          ++exponent;
      }

      ++index;
      continue;
    }
    if (src[index] == constants<CharType>::DECIMAL_POINT) {
      if (after_decimal) {
        break; // this means that src[index] points to a second decimal point,
               // ending the number.
      }
      after_decimal = true;
      ++index;
      continue;
    }
    // The character is neither a digit nor a decimal point.
    break;
  }

  if (!seen_digit)
    return output;

  // TODO: When adding max length argument, handle the case of a trailing
  // exponent marker, see scanf for more details.
  if (tolower(src[index]) == constants<CharType>::DECIMAL_EXPONENT_MARKER) {
    int sign = get_sign(src + index + 1);
    if (isdigit(src[index + 1 + static_cast<size_t>(sign != 0)])) {
      ++index;
      auto result = strtointeger<int32_t>(src + index, 10);
      if (result.has_error())
        output.error = result.error;
      int32_t add_to_exponent = result.value;
      index += static_cast<size_t>(result.parsed_len);

      // Here we do this operation as int64 to avoid overflow.
      int64_t temp_exponent = static_cast<int64_t>(exponent) +
                              static_cast<int64_t>(add_to_exponent);

      // If the result is in the valid range, then we use it. The valid range is
      // also within the int32 range, so this prevents overflow issues.
      if (temp_exponent > FPBits::MAX_BIASED_EXPONENT) {
        exponent = FPBits::MAX_BIASED_EXPONENT;
      } else if (temp_exponent < -FPBits::MAX_BIASED_EXPONENT) {
        exponent = -FPBits::MAX_BIASED_EXPONENT;
      } else {
        exponent = static_cast<int32_t>(temp_exponent);
      }
    }
  }

  output.parsed_len = index;
  if (mantissa == 0) { // if we have a 0, then also 0 the exponent.
    output.value = {0, 0};
  } else {
    auto temp =
        decimal_exp_to_float<T>({mantissa, exponent}, truncated, round, src);
    output.value = temp.num;
    output.error = temp.error;
  }
  return output;
}

// Attempts parsing a hexadecimal floating point number at the start of the
// string.
template <typename T, typename CharType>
LIBC_INLINE static StrToNumResult<ExpandedFloat<T>>
hexadecimal_string_to_float(const CharType *__restrict src,
                            RoundDirection round) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  constexpr uint32_t BASE = 16;
  bool truncated = false;
  bool seen_digit = false;
  bool after_decimal = false;
  StorageType mantissa = 0;
  int32_t exponent = 0;

  size_t index = 0;

  StrToNumResult<ExpandedFloat<T>> output({0, 0});

  // The goal for the first step of parsing is to convert the number in src to
  // the format mantissa * (base ^ exponent)

  // The loop fills the mantissa with as many digits as it can hold
  const StorageType bitstype_max_div_by_base =
      cpp::numeric_limits<StorageType>::max() / BASE;
  while (true) {
    if (isalnum(src[index])) {
      uint32_t digit = static_cast<uint32_t>(b36_char_to_int(src[index]));
      if (digit < BASE)
        seen_digit = true;
      else
        break;

      if (mantissa < bitstype_max_div_by_base) {
        mantissa = (mantissa * BASE) + digit;
        if (after_decimal)
          --exponent;
      } else {
        if (digit > 0)
          truncated = true;
        if (!after_decimal)
          ++exponent;
      }
      ++index;
      continue;
    }
    if (src[index] == constants<CharType>::DECIMAL_POINT) {
      if (after_decimal) {
        break; // this means that src[index] points to a second decimal point,
               // ending the number.
      }
      after_decimal = true;
      ++index;
      continue;
    }
    // The character is neither a hexadecimal digit nor a decimal point.
    break;
  }

  if (!seen_digit)
    return output;

  // Convert the exponent from having a base of 16 to having a base of 2.
  exponent *= 4;

  if (tolower(src[index]) == constants<CharType>::HEX_EXPONENT_MARKER) {
    int sign = get_sign(src + index + 1);
    if (isdigit(src[index + 1 + static_cast<size_t>(sign != 0)])) {
      ++index;
      auto result = strtointeger<int32_t>(src + index, 10);
      if (result.has_error())
        output.error = result.error;

      int32_t add_to_exponent = result.value;
      index += static_cast<size_t>(result.parsed_len);

      // Here we do this operation as int64 to avoid overflow.
      int64_t temp_exponent = static_cast<int64_t>(exponent) +
                              static_cast<int64_t>(add_to_exponent);

      // If the result is in the valid range, then we use it. The valid range is
      // also within the int32 range, so this prevents overflow issues.
      if (temp_exponent > FPBits::MAX_BIASED_EXPONENT) {
        exponent = FPBits::MAX_BIASED_EXPONENT;
      } else if (temp_exponent < -FPBits::MAX_BIASED_EXPONENT) {
        exponent = -FPBits::MAX_BIASED_EXPONENT;
      } else {
        exponent = static_cast<int32_t>(temp_exponent);
      }
    }
  }
  output.parsed_len = index;
  if (mantissa == 0) { // if we have a 0, then also 0 the exponent.
    output.value.exponent = 0;
    output.value.mantissa = 0;
  } else {
    auto temp = binary_exp_to_float<T>({mantissa, exponent}, truncated, round);
    output.error = temp.error;
    output.value = temp.num;
  }
  return output;
}

template <typename T, typename CharType>
LIBC_INLINE constexpr typename fputil::FPBits<T>::StorageType
nan_mantissa_from_ncharseq(const CharType *str, size_t len) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  StorageType nan_mantissa = 0;

  if (len > 0 && isdigit(str[0])) {
    StrToNumResult<StorageType> strtoint_result =
        strtointeger<StorageType>(str, 0, len);
    if (!strtoint_result.has_error())
      nan_mantissa = strtoint_result.value;

    if (strtoint_result.parsed_len != static_cast<ptrdiff_t>(len))
      nan_mantissa = 0;
  }

  return nan_mantissa;
}

// Takes a pointer to a string and a pointer to a string pointer. This function
// is used as the backend for all of the string to float functions.
// TODO: Add src_len member to match strtointeger.
// TODO: Next, move from char* and length to string_view
template <typename T, typename CharType>
LIBC_INLINE StrToNumResult<T>
strtofloatingpoint(const CharType *__restrict src) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  FPBits result = FPBits();
  bool seen_digit = false;
  int error = 0;

  size_t index = first_non_whitespace(src);
  int sign = get_sign(src + index);
  bool is_positive = (sign >= 0);
  index += (sign != 0);

  if (sign < 0) {
    result.set_sign(Sign::NEG);
  }

  if (isdigit(src[index]) ||
      src[index] == constants<CharType>::DECIMAL_POINT) { // regular number
    int base = 10;
    if (is_float_hex_start(src + index)) {
      base = 16;
      index += 2;
      seen_digit = true;
    }

    RoundDirection round_direction = RoundDirection::Nearest;
    switch (fputil::quick_get_round()) {
    case FE_TONEAREST:
      round_direction = RoundDirection::Nearest;
      break;
    case FE_UPWARD:
      round_direction = is_positive ? RoundDirection::Up : RoundDirection::Down;
      break;
    case FE_DOWNWARD:
      round_direction = is_positive ? RoundDirection::Down : RoundDirection::Up;
      break;
    case FE_TOWARDZERO:
      round_direction = RoundDirection::Down;
      break;
    }

    StrToNumResult<ExpandedFloat<T>> parse_result({0, 0});
    if (base == 16) {
      parse_result =
          hexadecimal_string_to_float<T>(src + index, round_direction);
    } else { // base is 10
      parse_result = decimal_string_to_float<T>(src + index, round_direction);
    }
    seen_digit = parse_result.parsed_len != 0;
    result.set_mantissa(parse_result.value.mantissa);
    result.set_biased_exponent(parse_result.value.exponent);
    index += parse_result.parsed_len;
    error = parse_result.error;
  } else if (tolower_starts_with(src + index, 3,
                                 constants<CharType>::NAN_STRING)) {
    // NAN
    seen_digit = true;
    index += 3;
    StorageType nan_mantissa = 0;
    // this handles the case of `NaN(n-character-sequence)`, where the
    // n-character-sequence is made of 0 or more letters, numbers, or
    // underscore characters in any order.
    if (is_char_or_wchar(src[index], '(', L'(')) {
      size_t left_paren = index;
      ++index;
      while (isalnum(src[index]) || is_char_or_wchar(src[index], '_', L'_'))
        ++index;
      if (is_char_or_wchar(src[index], ')', L')')) {
        ++index;
        nan_mantissa = nan_mantissa_from_ncharseq<T>(src + (left_paren + 1),
                                                     index - left_paren - 2);
      } else {
        index = left_paren;
      }
    }
    result = FPBits(result.quiet_nan(result.sign(), nan_mantissa));
  } else if (tolower_starts_with(src + index, 8,
                                 constants<CharType>::INF_STRING)) {
    // INFINITY
    seen_digit = true;
    result = FPBits(result.inf(result.sign()));
    index += 8;
  } else if (tolower_starts_with(src + index, 3,
                                 constants<CharType>::INF_STRING)) {
    // INF
    seen_digit = true;
    result = FPBits(result.inf(result.sign()));
    index += 3;
  }

  if (!seen_digit) { // If there is nothing to actually parse, then return 0.
    return {T(0), 0, error};
  }

  // This function only does something if T is long double and the platform uses
  // special 80 bit long doubles. Otherwise it should be inlined out.
  set_implicit_bit<T>(result);

  return {result.get_val(), static_cast<ptrdiff_t>(index), error};
}

template <class T>
LIBC_INLINE constexpr StrToNumResult<T> strtonan(const char *arg) {
  using FPBits = typename fputil::FPBits<T>;
  using StorageType = typename FPBits::StorageType;

  LIBC_CRASH_ON_NULLPTR(arg);

  FPBits result;
  int error = 0;
  StorageType nan_mantissa = 0;

  ptrdiff_t index = 0;
  while (isalnum(arg[index]) || arg[index] == '_')
    ++index;

  if (arg[index] == '\0')
    nan_mantissa = nan_mantissa_from_ncharseq<T>(arg, index);

  result = FPBits::quiet_nan(Sign::POS, nan_mantissa);
  return {result.get_val(), 0, error};
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_STR_TO_FLOAT_H
