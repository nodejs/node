//===-- Add and subtract IEEE 754 floating-point numbers --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_ADD_SUB_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_ADD_SUB_H

#include "hdr/fenv_macros.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil::generic {

template <bool IsSub, typename OutType, typename InType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                           cpp::is_floating_point_v<InType> &&
                                           sizeof(OutType) <= sizeof(InType),
                                       OutType>
add_or_sub(InType x, InType y) {
  using OutFPBits = FPBits<OutType>;
  using OutStorageType = typename OutFPBits::StorageType;
  using InFPBits = FPBits<InType>;
  using InStorageType = typename InFPBits::StorageType;

  constexpr int GUARD_BITS_LEN = 3;
  constexpr int RESULT_FRACTION_LEN = InFPBits::FRACTION_LEN + GUARD_BITS_LEN;
  constexpr int RESULT_MANTISSA_LEN = RESULT_FRACTION_LEN + 1;

  using DyadicFloat =
      DyadicFloat<cpp::bit_ceil(static_cast<size_t>(RESULT_MANTISSA_LEN))>;

  InFPBits x_bits(x);
  InFPBits y_bits(y);

  bool is_effectively_add = (x_bits.sign() == y_bits.sign()) != IsSub;

  if (LIBC_UNLIKELY(x_bits.is_inf_or_nan() || y_bits.is_inf_or_nan() ||
                    x_bits.is_zero() || y_bits.is_zero())) {
    if (x_bits.is_nan() || y_bits.is_nan()) {
      if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan())
        raise_except_if_required(FE_INVALID);

      if (x_bits.is_quiet_nan()) {
        InStorageType x_payload = x_bits.get_mantissa();
        x_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(x_bits.sign(),
                                    static_cast<OutStorageType>(x_payload))
            .get_val();
      }

      if (y_bits.is_quiet_nan()) {
        InStorageType y_payload = y_bits.get_mantissa();
        y_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(y_bits.sign(),
                                    static_cast<OutStorageType>(y_payload))
            .get_val();
      }

      return OutFPBits::quiet_nan().get_val();
    }

    if (x_bits.is_inf()) {
      if (y_bits.is_inf()) {
        if (!is_effectively_add) {
          raise_except_if_required(FE_INVALID);
          return OutFPBits::quiet_nan().get_val();
        }

        return OutFPBits::inf(x_bits.sign()).get_val();
      }

      return OutFPBits::inf(x_bits.sign()).get_val();
    }

    if (y_bits.is_inf()) {
      if constexpr (IsSub)
        return OutFPBits::inf(y_bits.sign().negate()).get_val();
      else
        return OutFPBits::inf(y_bits.sign()).get_val();
    }

    if (x_bits.is_zero()) {
      if (y_bits.is_zero()) {
        if (is_effectively_add)
          return OutFPBits::zero(x_bits.sign()).get_val();
        switch (fputil::quick_get_round()) {
        case FE_DOWNWARD:
          return OutFPBits::zero(Sign::NEG).get_val();
        default:
          return OutFPBits::zero(Sign::POS).get_val();
        }
      }

      if constexpr (cpp::is_same_v<InType, bfloat16> &&
                    cpp::is_same_v<OutType, bfloat16>) {
        OutFPBits out_y_bits(y);
        if constexpr (IsSub)
          out_y_bits.set_sign(out_y_bits.sign().negate());
        return out_y_bits.get_val();
      } else {

#ifdef LIBC_HAS_CONSTANT_EVALUATION
        InType tmp = y;
#else
        // volatile prevents Clang from converting tmp to OutType and then
        // immediately back to InType before negating it, resulting in double
        // rounding.
        volatile InType tmp = y;
#endif // LIBC_HAS_CONSTANT_EVALUATION
        if constexpr (IsSub)
          tmp = -tmp;
        return cast<OutType>(tmp);
      }
    }

    if (y_bits.is_zero())
      return cast<OutType>(x);
  }

  InType x_abs = x_bits.abs().get_val();
  InType y_abs = y_bits.abs().get_val();

  if (x_abs == y_abs && !is_effectively_add) {
    switch (fputil::quick_get_round()) {
    case FE_DOWNWARD:
      return OutFPBits::zero(Sign::NEG).get_val();
    default:
      return OutFPBits::zero(Sign::POS).get_val();
    }
  }

  Sign result_sign = Sign::POS;

  if (x_abs > y_abs) {
    result_sign = x_bits.sign();
  } else if (x_abs < y_abs) {
    result_sign = y_bits.sign();
    if constexpr (IsSub)
      result_sign = result_sign.negate();
  } else if (is_effectively_add) {
    result_sign = x_bits.sign();
  }

  InFPBits max_bits(cpp::max(x_abs, y_abs));
  InFPBits min_bits(cpp::min(x_abs, y_abs));

  InStorageType result_mant{};

  if (max_bits.is_subnormal()) {
    // min_bits must be subnormal too.

    if (is_effectively_add)
      result_mant = max_bits.get_mantissa() + min_bits.get_mantissa();
    else
      result_mant = max_bits.get_mantissa() - min_bits.get_mantissa();

    result_mant <<= GUARD_BITS_LEN;
  } else {
    InStorageType max_mant = static_cast<InStorageType>(
        max_bits.get_explicit_mantissa() << GUARD_BITS_LEN);
    InStorageType min_mant = static_cast<InStorageType>(
        min_bits.get_explicit_mantissa() << GUARD_BITS_LEN);

    int alignment = (max_bits.get_biased_exponent() - max_bits.is_normal()) -
                    (min_bits.get_biased_exponent() - min_bits.is_normal());

    InStorageType aligned_min_mant = static_cast<InStorageType>(
        min_mant >> cpp::min(alignment, RESULT_MANTISSA_LEN));
    bool aligned_min_mant_sticky{};

    if (alignment <= GUARD_BITS_LEN)
      aligned_min_mant_sticky = false;
    else if (alignment > InFPBits::FRACTION_LEN + GUARD_BITS_LEN)
      aligned_min_mant_sticky = true;
    else
      aligned_min_mant_sticky =
          (static_cast<InStorageType>(
              min_mant << (InFPBits::STORAGE_LEN - alignment))) != 0;

    InStorageType min_mant_sticky =
        static_cast<InStorageType>(static_cast<int>(aligned_min_mant_sticky));

    if (is_effectively_add)
      result_mant = max_mant + (aligned_min_mant | min_mant_sticky);
    else
      result_mant = max_mant - (aligned_min_mant | min_mant_sticky);
  }

  int result_exp = max_bits.get_explicit_exponent() - RESULT_FRACTION_LEN;
  DyadicFloat result(result_sign, result_exp, result_mant);
  return result.template as<OutType, /*ShouldSignalExceptions=*/true>();
}

template <typename OutType, typename InType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                           cpp::is_floating_point_v<InType> &&
                                           sizeof(OutType) <= sizeof(InType),
                                       OutType>
add(InType x, InType y) {
  return add_or_sub</*IsSub=*/false, OutType>(x, y);
}

template <typename OutType, typename InType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                           cpp::is_floating_point_v<InType> &&
                                           sizeof(OutType) <= sizeof(InType),
                                       OutType>
sub(InType x, InType y) {
  return add_or_sub</*IsSub=*/true, OutType>(x, y);
}

} // namespace fputil::generic
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_ADD_SUB_H
