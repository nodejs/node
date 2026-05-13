//===-- A class to store a normalized floating point number -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_NORMALFLOAT_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_NORMALFLOAT_H

#include "FPBits.h"

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil {

// A class which stores the normalized form of a floating point value.
// The special IEEE-754 bits patterns of Zero, infinity and NaNs are
// are not handled by this class.
//
// A normalized floating point number is of this form:
//    (-1)*sign * 2^exponent * <mantissa>
// where <mantissa> is of the form 1.<...>.
template <typename T> struct NormalFloat {
  static_assert(
      cpp::is_floating_point_v<T>,
      "NormalFloat template parameter has to be a floating point type.");

  using StorageType = typename FPBits<T>::StorageType;
  static constexpr StorageType ONE =
      (StorageType(1) << FPBits<T>::FRACTION_LEN);

  // Unbiased exponent value.
  int32_t exponent{};

  StorageType mantissa{};
  // We want |StorageType| to have atleast one bit more than the actual mantissa
  // bit width to accommodate the implicit 1 value.
  static_assert(sizeof(StorageType) * 8 >= FPBits<T>::FRACTION_LEN + 1,
                "Bad type for mantissa in NormalFloat.");

  Sign sign = Sign::POS;

  LIBC_INLINE constexpr NormalFloat(Sign s, int32_t e, StorageType m)
      : exponent(e), mantissa(m), sign(s) {
    if (mantissa >= ONE)
      return;

    unsigned normalization_shift = evaluate_normalization_shift(mantissa);
    mantissa <<= normalization_shift;
    exponent -= normalization_shift;
  }

  LIBC_INLINE constexpr explicit NormalFloat(T x) {
    init_from_bits(FPBits<T>(x));
  }

  LIBC_INLINE constexpr explicit NormalFloat(FPBits<T> bits) {
    init_from_bits(bits);
  }

  // Compares this normalized number with another normalized number.
  // Returns -1 is this number is less than |other|, 0 if this number is equal
  // to |other|, and 1 if this number is greater than |other|.
  LIBC_INLINE constexpr int cmp(const NormalFloat<T> &other) const {
    const int result = sign.is_neg() ? -1 : 1;
    if (sign != other.sign)
      return result;

    if (exponent > other.exponent) {
      return result;
    } else if (exponent == other.exponent) {
      if (mantissa > other.mantissa)
        return result;
      else if (mantissa == other.mantissa)
        return 0;
      else
        return -result;
    } else {
      return -result;
    }
  }

  // Returns a new normalized floating point number which is equal in value
  // to this number multiplied by 2^e. That is:
  //     new = this *  2^e
  LIBC_INLINE constexpr NormalFloat<T> mul2(int e) const {
    NormalFloat<T> result = *this;
    result.exponent += e;
    return result;
  }

  LIBC_INLINE constexpr operator T() const {
    int biased_exponent = exponent + FPBits<T>::EXP_BIAS;
    // Max exponent is of the form 0xFF...E. That is why -2 and not -1.
    constexpr int MAX_EXPONENT_VALUE = (1 << FPBits<T>::EXP_LEN) - 2;
    if (biased_exponent > MAX_EXPONENT_VALUE) {
      return FPBits<T>::inf(sign).get_val();
    }

    FPBits<T> result(T(0.0));
    result.set_sign(sign);

    constexpr int SUBNORMAL_EXPONENT = -FPBits<T>::EXP_BIAS + 1;
    if (exponent < SUBNORMAL_EXPONENT) {
      unsigned shift = static_cast<unsigned>(SUBNORMAL_EXPONENT - exponent);
      // Since exponent > subnormalExponent, shift is strictly greater than
      // zero.
      if (shift <= FPBits<T>::FRACTION_LEN + 1) {
        // Generate a subnormal number. Might lead to loss of precision.
        // We round to nearest and round halfway cases to even.
        const StorageType shift_out_mask =
            static_cast<StorageType>(StorageType(1) << shift) - 1;
        const StorageType shift_out_value = mantissa & shift_out_mask;
        const StorageType halfway_value =
            static_cast<StorageType>(StorageType(1) << (shift - 1));
        result.set_biased_exponent(0);
        result.set_mantissa(mantissa >> shift);
        StorageType new_mantissa = result.get_mantissa();
        if (shift_out_value > halfway_value) {
          new_mantissa += 1;
        } else if (shift_out_value == halfway_value) {
          // Round to even.
          if (result.get_mantissa() & 0x1)
            new_mantissa += 1;
        }
        result.set_mantissa(new_mantissa);
        // Adding 1 to mantissa can lead to overflow. This can only happen if
        // mantissa was all ones (0b111..11). For such a case, we will carry
        // the overflow into the exponent.
        if (new_mantissa == ONE)
          result.set_biased_exponent(1);
        return result.get_val();
      } else {
        return result.get_val();
      }
    }

    result.set_biased_exponent(
        static_cast<StorageType>(exponent + FPBits<T>::EXP_BIAS));
    result.set_mantissa(mantissa);
    return result.get_val();
  }

private:
  LIBC_INLINE constexpr void init_from_bits(FPBits<T> bits) {
    sign = bits.sign();

    if (bits.is_inf_or_nan() || bits.is_zero()) {
      // Ignore special bit patterns. Implementations deal with them separately
      // anyway so this should not be a problem.
      exponent = 0;
      mantissa = 0;
      return;
    }

    // Normalize subnormal numbers.
    if (bits.is_subnormal()) {
      unsigned shift = evaluate_normalization_shift(bits.get_mantissa());
      mantissa = static_cast<StorageType>(bits.get_mantissa() << shift);
      exponent = 1 - FPBits<T>::EXP_BIAS - static_cast<int32_t>(shift);
    } else {
      exponent = bits.get_biased_exponent() - FPBits<T>::EXP_BIAS;
      mantissa = ONE | bits.get_mantissa();
    }
  }

  LIBC_INLINE constexpr unsigned evaluate_normalization_shift(StorageType m) {
    unsigned shift = 0;
    for (; (ONE & m) == 0 && (shift < FPBits<T>::FRACTION_LEN);
         m <<= 1, ++shift)
      ;
    return shift;
  }
};

#ifdef LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
template <>
LIBC_INLINE constexpr void
NormalFloat<long double>::init_from_bits(FPBits<long double> bits) {
  sign = bits.sign();

  if (bits.is_inf_or_nan() || bits.is_zero()) {
    // Ignore special bit patterns. Implementations deal with them separately
    // anyway so this should not be a problem.
    exponent = 0;
    mantissa = 0;
    return;
  }

  if (bits.is_subnormal()) {
    if (bits.get_implicit_bit() == 0) {
      // Since we ignore zero value, the mantissa in this case is non-zero.
      int normalization_shift =
          evaluate_normalization_shift(bits.get_mantissa());
      exponent = -16382 - normalization_shift;
      mantissa = (bits.get_mantissa() << normalization_shift);
    } else {
      exponent = -16382;
      mantissa = ONE | bits.get_mantissa();
    }
  } else {
    if (bits.get_implicit_bit() == 0) {
      // Invalid number so just store 0 similar to a NaN.
      exponent = 0;
      mantissa = 0;
    } else {
      exponent = bits.get_biased_exponent() - 16383;
      mantissa = ONE | bits.get_mantissa();
    }
  }
}

template <>
LIBC_INLINE constexpr NormalFloat<long double>::operator long double() const {
  using LDBits = FPBits<long double>;
  int biased_exponent = exponent + LDBits::EXP_BIAS;
  // Max exponent is of the form 0xFF...E. That is why -2 and not -1.
  constexpr int MAX_EXPONENT_VALUE = (1 << LDBits::EXP_LEN) - 2;
  if (biased_exponent > MAX_EXPONENT_VALUE) {
    return LDBits::inf(sign).get_val();
  }

  FPBits<long double> result(0.0l);
  result.set_sign(sign);

  constexpr int SUBNORMAL_EXPONENT = -LDBits::EXP_BIAS + 1;
  if (exponent < SUBNORMAL_EXPONENT) {
    unsigned shift = SUBNORMAL_EXPONENT - exponent;
    if (shift <= LDBits::FRACTION_LEN + 1) {
      // Generate a subnormal number. Might lead to loss of precision.
      // We round to nearest and round halfway cases to even.
      const StorageType shift_out_mask = (StorageType(1) << shift) - 1;
      const StorageType shift_out_value = mantissa & shift_out_mask;
      const StorageType halfway_value = StorageType(1) << (shift - 1);
      result.set_biased_exponent(0);
      result.set_mantissa(mantissa >> shift);
      StorageType new_mantissa = result.get_mantissa();
      if (shift_out_value > halfway_value) {
        new_mantissa += 1;
      } else if (shift_out_value == halfway_value) {
        // Round to even.
        if (result.get_mantissa() & 0x1)
          new_mantissa += 1;
      }
      result.set_mantissa(new_mantissa);
      // Adding 1 to mantissa can lead to overflow. This can only happen if
      // mantissa was all ones (0b111..11). For such a case, we will carry
      // the overflow into the exponent and set the implicit bit to 1.
      if (new_mantissa == ONE) {
        result.set_biased_exponent(1);
        result.set_implicit_bit(1);
      } else {
        result.set_implicit_bit(0);
      }
      return result.get_val();
    } else {
      return result.get_val();
    }
  }

  result.set_biased_exponent(biased_exponent);
  result.set_mantissa(mantissa);
  result.set_implicit_bit(1);
  return result.get_val();
}
#endif // LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80

} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_NORMALFLOAT_H
